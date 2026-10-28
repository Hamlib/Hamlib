#ifndef HAMLIB_TESTS_DAEMON_CLIENT_H
#define HAMLIB_TESTS_DAEMON_CLIENT_H 1

#include <pthread.h>

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

#endif
