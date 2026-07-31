#include "hamlib/config.h"

#include <stdio.h>

#if defined(_WIN32) || defined(__MINGW32__)

int main(void)
{
    return 77;
}

#else

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define RETRY_COUNT 100
#define BURST_CLIENTS 16

struct daemon_case
{
    const char *name;
    const char *command;
};

static int failures;

static void fail(const char *daemon, const char *message)
{
    fprintf(stderr, "FAIL: %s: %s\n", daemon, message);
    ++failures;
}

static unsigned reserve_port(void)
{
    struct sockaddr_in address;
    socklen_t length = sizeof(address);
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        return 0;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0
            || getsockname(sock, (struct sockaddr *)&address, &length) < 0)
    {
        close(sock);
        return 0;
    }

    close(sock);
    return ntohs(address.sin_port);
}

static int connect_client(unsigned port)
{
    struct sockaddr_in address;
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        return -1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(sock, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        close(sock);
        return -1;
    }

    return sock;
}

static int readable_within(int sock, unsigned milliseconds)
{
    struct timeval timeout;
    fd_set set;

    FD_ZERO(&set);
    FD_SET(sock, &set);
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = (milliseconds % 1000) * 1000;
    return select(sock + 1, &set, NULL, NULL, &timeout) > 0;
}

static int closed_within(int sock, unsigned milliseconds)
{
    char byte;
    ssize_t count;

    if (!readable_within(sock, milliseconds))
    {
        return 0;
    }

    count = recv(sock, &byte, sizeof(byte), 0);
    return count <= 0;
}

static int wait_for_admission(unsigned port)
{
    unsigned attempt;

    for (attempt = 0; attempt < RETRY_COUNT; ++attempt)
    {
        int sock = connect_client(port);

        if (sock >= 0)
        {
            if (!closed_within(sock, 50))
            {
                return sock;
            }

            close(sock);
        }

        usleep(50000);
    }

    return -1;
}

static int run_command(unsigned port, const char *command)
{
    char response[256];
    int sock = wait_for_admission(port);
    size_t length = strlen(command);
    ssize_t count;

    if (sock < 0)
    {
        return 0;
    }

    if (send(sock, command, length, 0) != (ssize_t)length
            || !readable_within(sock, 2000))
    {
        close(sock);
        return 0;
    }

    count = recv(sock, response, sizeof(response) - 1, 0);
    close(sock);

    if (count <= 0)
    {
        return 0;
    }

    response[count] = '\0';
    return strstr(response, "RPRT -") == NULL;
}

static pid_t start_daemon(const char *name, unsigned port)
{
    char executable[128];
    char port_string[16];
    pid_t pid;

    snprintf(executable, sizeof(executable), "./%s", name);
    snprintf(port_string, sizeof(port_string), "%u", port);
    pid = fork();

    if (pid == 0)
    {
        execl(executable, name, "-m", "1", "-T", "127.0.0.1", "-t",
              port_string, "--max-clients=2", (char *)NULL);
        _exit(127);
    }

    return pid;
}

static int wait_for_exit(pid_t pid, unsigned milliseconds)
{
    unsigned elapsed;
    int status;

    for (elapsed = 0; elapsed < milliseconds; elapsed += 50000)
    {
        if (waitpid(pid, &status, WNOHANG) == pid)
        {
            return 1;
        }

        usleep(50000);
    }

    return 0;
}

static void stop_daemon(pid_t pid)
{
    kill(pid, SIGTERM);

    if (!wait_for_exit(pid, 7000000))
    {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }
}

static int option_exit_status(const char *name, const char *option,
                              const char *limit)
{
    char executable[128];
    pid_t pid;
    int status;

    snprintf(executable, sizeof(executable), "./%s", name);
    pid = fork();

    if (pid == 0)
    {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl(executable, name, option, limit, "--help", (char *)NULL);
        _exit(127);
    }

    if (pid < 0 || waitpid(pid, &status, 0) != pid || !WIFEXITED(status))
    {
        return -1;
    }

    return WEXITSTATUS(status);
}

static void test_cli_options(void)
{
    static const char *daemons[] =
    {
        "rigctld", "rigctltcp", "rotctld", "ampctld"
    };
    unsigned i;

    for (i = 0; i < sizeof(daemons) / sizeof(daemons[0]); ++i)
    {
        if (option_exit_status(daemons[i], "-N", "2") != 0)
        {
            fail(daemons[i], "valid --max-clients option was rejected");
        }

        if (option_exit_status(daemons[i], "--max-clients", "0") == 0)
        {
            fail(daemons[i], "zero --max-clients value was accepted");
        }
    }
}

static void test_daemon(const struct daemon_case *test)
{
    int held[2] = { -1, -1 };
    int burst[BURST_CLIENTS];
    unsigned port = 0;
    pid_t pid = -1;
    unsigned start_attempt;
    unsigned i;

    for (start_attempt = 0; start_attempt < 3; ++start_attempt)
    {
        port = reserve_port();

        if (port == 0)
        {
            continue;
        }

        pid = start_daemon(test->name, port);

        if (pid < 0)
        {
            continue;
        }

        held[0] = wait_for_admission(port);

        if (held[0] >= 0)
        {
            break;
        }

        stop_daemon(pid);
        pid = -1;
    }

    if (held[0] < 0)
    {
        fail(test->name, "could not start daemon on a loopback port");
        return;
    }

    held[1] = wait_for_admission(port);

    if (held[0] < 0 || held[1] < 0)
    {
        fail(test->name, "could not establish two admitted clients");
        goto cleanup;
    }

    burst[0] = connect_client(port);

    if (burst[0] < 0 || !closed_within(burst[0], 2000))
    {
        fail(test->name, "third client was not rejected at the limit");
    }

    if (burst[0] >= 0)
    {
        close(burst[0]);
    }

    if (kill(pid, 0) != 0)
    {
        fail(test->name, "daemon exited after rejecting a client");
        goto cleanup;
    }

    close(held[0]);
    held[0] = -1;

    if (!run_command(port, test->command))
    {
        fail(test->name, "released slot was not readmitted with a working command");
        goto cleanup;
    }

    for (i = 0; i < 20; ++i)
    {
        if (!run_command(port, test->command))
        {
            fail(test->name, "connect/disconnect cycle leaked an admission slot");
            goto cleanup;
        }
    }

    close(held[1]);
    held[1] = -1;
    held[0] = wait_for_admission(port);
    held[1] = wait_for_admission(port);

    if (held[0] < 0 || held[1] < 0)
    {
        fail(test->name, "could not refill client slots before saturation burst");
        goto cleanup;
    }

    for (i = 0; i < BURST_CLIENTS; ++i)
    {
        burst[i] = connect_client(port);

        if (burst[i] < 0 || !closed_within(burst[i], 2000))
        {
            fail(test->name, "saturation burst client was not rejected");
        }

        if (burst[i] >= 0)
        {
            close(burst[i]);
        }
    }

    close(held[0]);
    held[0] = -1;

    if (!run_command(port, test->command))
    {
        fail(test->name, "saturation burst leaked an admission slot");
    }

cleanup:
    if (held[0] >= 0)
    {
        close(held[0]);
    }

    if (held[1] >= 0)
    {
        close(held[1]);
    }

    stop_daemon(pid);
}

int main(void)
{
    static const struct daemon_case daemons[] =
    {
        { "rigctld", "f\n" },
        { "rotctld", "p\n" },
        { "ampctld", "f\n" }
    };
    unsigned i;

    test_cli_options();

    for (i = 0; i < sizeof(daemons) / sizeof(daemons[0]); ++i)
    {
        test_daemon(&daemons[i]);
    }

    return failures ? 1 : 0;
}

#endif
