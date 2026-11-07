#ifndef HAMLIB_TESTS_DAEMON_CLIENT_H
#define HAMLIB_TESTS_DAEMON_CLIENT_H 1

#include <pthread.h>
#include <stdio.h>

#define DAEMON_DEFAULT_MAX_CLIENTS 32U
#define DAEMON_CLIENT_TRACKER_INITIALIZER \
    { PTHREAD_MUTEX_INITIALIZER, 0U, DAEMON_DEFAULT_MAX_CLIENTS }

struct daemon_client_tracker
{
    pthread_mutex_t mutex;
    unsigned active;
    unsigned limit;
};

int daemon_client_limit_parse(const char *value, unsigned *limit);
int daemon_client_tracker_init(struct daemon_client_tracker *tracker,
                               unsigned limit);
void daemon_client_tracker_destroy(struct daemon_client_tracker *tracker);
int daemon_client_tracker_acquire(struct daemon_client_tracker *tracker);
void daemon_client_tracker_release(struct daemon_client_tracker *tracker);

#ifndef __MINGW32__
/*
 * Opens a stdio stream on a descriptor of its own, duplicated from a client
 * socket. Two streams on the accepted descriptor mean two fclose() calls closing
 * it, and the second lands on whatever number accept() has handed another client
 * meanwhile, taking that client's socket away from it. With a dup() behind each
 * stream, every fclose() closes a descriptor it owns, and the caller closes the
 * accepted one exactly once, at the end of the handler.
 *
 * Not on Windows, where an accepted socket is a SOCKET handle rather than a C
 * runtime descriptor: the daemons wrap it with _open_osfhandle() there, and
 * dup() does not apply.
 */
FILE *daemon_client_fdopen_dup(int sock, const char *mode);
#endif

#endif
