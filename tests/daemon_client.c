#include "hamlib/config.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "daemon_client.h"

int daemon_client_limit_parse(const char *value, unsigned *limit)
{
    char *end;
    unsigned long parsed;
    const char *p;

    if (!value || !value[0] || !limit)
    {
        return -1;
    }

    for (p = value; *p; ++p)
    {
        if (*p < '0' || *p > '9')
        {
            return -1;
        }
    }

    errno = 0;
    parsed = strtoul(value, &end, 10);

    if (errno == ERANGE || *end || parsed == 0 || parsed > UINT_MAX)
    {
        return -1;
    }

    *limit = (unsigned)parsed;
    return 0;
}

int daemon_client_tracker_init(struct daemon_client_tracker *tracker,
                               unsigned limit)
{
    if (!tracker || limit == 0)
    {
        return EINVAL;
    }

    tracker->active = 0;
    tracker->limit = limit;
    return pthread_mutex_init(&tracker->mutex, NULL);
}

void daemon_client_tracker_destroy(struct daemon_client_tracker *tracker)
{
    pthread_mutex_destroy(&tracker->mutex);
}

int daemon_client_tracker_acquire(struct daemon_client_tracker *tracker)
{
    int acquired = 0;

    pthread_mutex_lock(&tracker->mutex);

    if (tracker->active < tracker->limit)
    {
        ++tracker->active;
        acquired = 1;
    }

    pthread_mutex_unlock(&tracker->mutex);
    return acquired;
}

void daemon_client_tracker_release(struct daemon_client_tracker *tracker)
{
    pthread_mutex_lock(&tracker->mutex);

    if (tracker->active > 0)
    {
        --tracker->active;
    }

    pthread_mutex_unlock(&tracker->mutex);
}
