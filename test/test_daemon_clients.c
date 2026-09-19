/*
 *  Hamlib Interface - client connection handling in rigctld, rotctld and
 *  ampctld
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*
 * Each daemon serves every client on a thread of its own, with the main thread
 * accepting the next connection while earlier clients are still tearing down.
 * Two defects lived in that teardown, identically in all three daemons:
 *
 * - The client socket carried two stdio streams on one descriptor, so it was
 *   closed more than once. POSIX hands out the lowest free descriptor, so
 *   between those closes accept() could give the same number to a new client
 *   -- and the stray close then took that client's socket away from it. Under
 *   rapid connect/disconnect this dropped dozens of sessions per thousand.
 *
 * - The handler ended with pthread_exit(), which on glibc must dlopen
 *   libgcc_s.so.1 to unwind and calls abort() if it cannot, so one client's
 *   thread finishing could kill the daemon and every other client with it.
 *
 * The tests check what a client sees rather than what the daemon logs: every
 * session opened during heavy churn must get a proper answer to its query, and
 * the daemon must still be running at the end. A stray close on any session
 * fails the first; an abort fails the second.
 *
 * The abort itself cannot be provoked portably -- it needs glibc's dlopen of
 * libgcc_s to fail, and macOS never loads that library -- so these tests
 * catch it only if it happens during the run. The fix does not depend on it:
 * returning from a thread's start routine never involves the unwinder.
 */

#ifdef HAVE_CONFIG_H
#  include "hamlib/config.h"
#endif

#include "acutest.h"
#include "test_debug.h"

#ifdef _WIN32

/* The daemons are started as child processes with fork()/execl(), which
 * Windows does not provide. The defects these tests guard were also
 * POSIX-side: the Windows builds open their streams through
 * _open_osfhandle() and close the socket differently. */
static void test_requires_posix_host(void)
{
    TEST_MSG("daemon client tests need fork(); skipped on this host");
    TEST_CHECK(1);
}

TEST_LIST =
{
    { "requires_posix_host", test_requires_posix_host },
    { NULL, NULL }
};

#else

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <hamlib/rig.h>

/* Sessions opened concurrently, and how many each thread opens in turn --
 * sized by measurement rather than guessed. With the double close put back,
 * sixteen threads of 150 dropped 8-25 sessions per daemon on every run; eight
 * threads dropped as few as one, close enough to zero that a reintroduced
 * bug could slip through. The whole suite still runs in under two seconds. */
#define CHURN_THREADS    16
#define CHURN_SESSIONS  150

/* Budgets, each for what it waits out:
 *
 * Reply: one short query to a dummy backend, which answers in well under a
 * millisecond; the budget covers a daemon a loaded runner has descheduled.
 *
 * Startup: the daemon has to exec, open its dummy backend and bind its port
 * before the first connect() can succeed. */
#define REPLY_TIMEOUT_MS    2000
#define STARTUP_TIMEOUT_MS  5000

struct daemon_spec
{
    const char *name;
    const char *query;      /* one line, answered with a number */
};

static const struct daemon_spec rigctld_spec = { "rigctld", "f\n" };
static const struct daemon_spec rotctld_spec = { "rotctld", "p\n" };
static const struct daemon_spec ampctld_spec = { "ampctld", "f\n" };

struct daemon_proc
{
    pid_t pid;
    int port;
    char log_path[64];
};


/* The daemons are built into tests/, next to this directory in both the
 * source build and make distcheck's build tree. */
static const char *daemon_path(const char *name)
{
    static char path[64];

    snprintf(path, sizeof(path), "../tests/%s", name);
    return access(path, X_OK) == 0 ? path : NULL;
}


static int pick_free_port(void)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int port = -1;

    if (sock < 0)
    {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0
            && getsockname(sock, (struct sockaddr *)&addr, &len) == 0)
    {
        port = ntohs(addr.sin_port);
    }

    close(sock);
    return port;
}


static int connect_to(int port)
{
    struct sockaddr_in addr;
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        close(sock);
        return -1;
    }

    return sock;
}


static int daemon_alive(struct daemon_proc *proc)
{
    int status;

    return proc->pid > 0 && waitpid(proc->pid, &status, WNOHANG) == 0;
}


static void dump_log(const struct daemon_proc *proc)
{
    FILE *fp = fopen(proc->log_path, "r");
    char line[512];
    int lines = 0;

    if (fp == NULL)
    {
        return;
    }

    fprintf(stderr, "--- %s ---\n", proc->log_path);

    while (fgets(line, sizeof(line), fp) != NULL && lines++ < 60)
    {
        fputs(line, stderr);
    }

    fprintf(stderr, "--- end ---\n");
    fclose(fp);
}


static int start_daemon(const struct daemon_spec *spec,
                        struct daemon_proc *proc)
{
    const char *path = daemon_path(spec->name);
    char port_str[16];
    int waited;

    memset(proc, 0, sizeof(*proc));

    if (path == NULL)
    {
        TEST_MSG("%s is not built (expected at ../tests/%s)", spec->name,
                 spec->name);
        return -1;
    }

    proc->port = pick_free_port();

    if (proc->port < 0)
    {
        return -1;
    }

    snprintf(port_str, sizeof(port_str), "%d", proc->port);
    snprintf(proc->log_path, sizeof(proc->log_path), "%s-%d.log", spec->name,
             proc->port);

    proc->pid = fork();

    if (proc->pid < 0)
    {
        return -1;
    }

    if (proc->pid == 0)
    {
        /* -v keeps the daemon's own error reports, which is what explains a
         * failure here, without its per-command trace. */
        if (freopen(proc->log_path, "w", stderr) == NULL)
        {
            _exit(126);
        }

        execl(path, spec->name, "-m", "1", "-t", port_str, "-v", (char *)NULL);
        _exit(127);
    }

    for (waited = 0; waited < STARTUP_TIMEOUT_MS; waited += 20)
    {
        int sock;

        if (!daemon_alive(proc))
        {
            TEST_MSG("%s exited during startup", spec->name);
            dump_log(proc);
            proc->pid = 0;
            return -1;
        }

        sock = connect_to(proc->port);

        if (sock >= 0)
        {
            close(sock);
            return 0;
        }

        usleep(20 * 1000);
    }

    TEST_MSG("%s did not accept connections within %d ms", spec->name,
             STARTUP_TIMEOUT_MS);
    return -1;
}


static void stop_daemon(struct daemon_proc *proc)
{
    if (proc->pid > 0)
    {
        kill(proc->pid, SIGTERM);
        waitpid(proc->pid, NULL, 0);
        proc->pid = 0;
    }

    /* A test artefact: left behind, it fails distcleancheck. */
    if (proc->log_path[0] != '\0')
    {
        unlink(proc->log_path);
        proc->log_path[0] = '\0';
    }
}


enum session_result
{
    SESSION_ANSWERED,
    /* connect() itself failed. Says nothing about the daemon's teardown: a
     * client that has just closed thousands of connections holds their
     * ports in TIME_WAIT, and can run out of ephemeral ports to connect
     * from. */
    SESSION_NOT_CONNECTED,
    /* Connected, then no proper answer: dropped, reset, timed out, or an
     * error report. This is what a stray close does to a session. */
    SESSION_DROPPED
};

/* One session: connect, ask, read the first line of the answer, hang up.
 * Answered means the reply is a number -- the dummy backends' frequency and
 * position -- rather than an error report ("RPRT -n"). */
static enum session_result session(int port, const char *query)
{
    struct timeval tv;
    char reply[128];
    size_t got = 0;
    char *end;
    int sock = connect_to(port);
    int ok = 0;

    if (sock < 0)
    {
        return SESSION_NOT_CONNECTED;
    }

    tv.tv_sec = REPLY_TIMEOUT_MS / 1000;
    tv.tv_usec = (REPLY_TIMEOUT_MS % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (send(sock, query, strlen(query), 0) == (ssize_t)strlen(query))
    {
        while (got < sizeof(reply) - 1)
        {
            ssize_t n = recv(sock, reply + got, sizeof(reply) - 1 - got, 0);

            if (n <= 0)
            {
                break;
            }

            got += (size_t)n;

            if (memchr(reply, '\n', got) != NULL)
            {
                break;
            }
        }

        reply[got] = '\0';

        if (got > 0 && strncmp(reply, "RPRT", 4) != 0)
        {
            (void)strtod(reply, &end);
            ok = (end != reply);
        }
    }

    /* Hang up with a reset rather than an orderly close, so this end keeps
     * no TIME_WAIT: thousands of sessions a run, repeated, would otherwise
     * exhaust the ephemeral ports and fail connect() for reasons that have
     * nothing to do with the daemon. The daemon's session loop ends on the
     * reset exactly as it would on end-of-file, into the same teardown --
     * which is the code under test. */
    {
        struct linger lg = { 1, 0 };

        setsockopt(sock, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
    }

    close(sock);
    return ok ? SESSION_ANSWERED : SESSION_DROPPED;
}


struct churn_ctx
{
    int port;
    const char *query;
    int dropped;
    int not_connected;
};

static void *churn_thread(void *arg)
{
    struct churn_ctx *c = arg;
    int i;

    for (i = 0; i < CHURN_SESSIONS; i++)
    {
        switch (session(c->port, c->query))
        {
        case SESSION_ANSWERED:      break;

        case SESSION_NOT_CONNECTED: c->not_connected++; break;

        case SESSION_DROPPED:       c->dropped++; break;
        }
    }

    return NULL;
}


/* Many short sessions at once: each thread's hang-ups are torn down while
 * other threads' connections are being accepted, which is the window a
 * stray close needs. Every one of them must still be answered. */
static void check_sessions_survive_churn(const struct daemon_spec *spec)
{
    struct daemon_proc proc;
    struct churn_ctx ctx[CHURN_THREADS];
    pthread_t th[CHURN_THREADS];
    int i, started = 0, dropped = 0, not_connected = 0;

    if (start_daemon(spec, &proc) != 0)
    {
        stop_daemon(&proc);
        TEST_CHECK_(0, "could not start %s", spec->name);
        return;
    }

    for (i = 0; i < CHURN_THREADS; i++)
    {
        ctx[i].port = proc.port;
        ctx[i].query = spec->query;
        ctx[i].dropped = 0;
        ctx[i].not_connected = 0;

        if (pthread_create(&th[i], NULL, churn_thread, &ctx[i]) == 0)
        {
            started++;
        }
    }

    for (i = 0; i < started; i++)
    {
        pthread_join(th[i], NULL);
        dropped += ctx[i].dropped;
        not_connected += ctx[i].not_connected;
    }

    TEST_CHECK_(started == CHURN_THREADS, "only %d of %d client threads started",
                started, CHURN_THREADS);

    TEST_CHECK_(dropped == 0,
                "%d of %d %s sessions connected and then got no answer: a "
                "session's socket was closed from under it", dropped,
                started * CHURN_SESSIONS, spec->name);

    /* Not the bug this suite is about, but a run that could not connect has
     * not tested it either, so it must not pass quietly. */
    TEST_CHECK_(not_connected == 0,
                "%d of %d %s sessions could not connect at all -- a client-side "
                "or listen-queue problem, not the teardown", not_connected,
                started * CHURN_SESSIONS, spec->name);

    /* The daemon has to outlive its clients -- a client thread finishing must
     * never be able to take the process down. */
    TEST_CHECK_(daemon_alive(&proc), "%s exited during the churn", spec->name);

    if (dropped != 0 || not_connected != 0 || !daemon_alive(&proc))
    {
        dump_log(&proc);
    }

    stop_daemon(&proc);
}


void test_rigctld_sessions_survive_churn(void)
{
    check_sessions_survive_churn(&rigctld_spec);
}

void test_rotctld_sessions_survive_churn(void)
{
    check_sessions_survive_churn(&rotctld_spec);
}

void test_ampctld_sessions_survive_churn(void)
{
    check_sessions_survive_churn(&ampctld_spec);
}


TEST_LIST =
{
    { "rigctld_sessions_survive_churn", test_rigctld_sessions_survive_churn },
    { "rotctld_sessions_survive_churn", test_rotctld_sessions_survive_churn },
    { "ampctld_sessions_survive_churn", test_ampctld_sessions_survive_churn },
    { NULL, NULL }
};

#endif  /* _WIN32 */
