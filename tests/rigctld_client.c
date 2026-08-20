/*
 * Per-client state shared by rigctld network daemons.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "hamlib/config.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#elif defined(HAVE_WS2TCPIP_H)
#  include <ws2tcpip.h>
#endif

#include "hamlib/rig.h"
#include "rigctld_client.h"

static pthread_key_t client_id_key;

static void client_pool_now(const struct rigctld_client_pool *pool,
                            struct timespec *now)
{
#ifdef CLOCK_MONOTONIC
    clock_gettime(pool->use_monotonic ? CLOCK_MONOTONIC : CLOCK_REALTIME, now);
#else
    clock_gettime(CLOCK_REALTIME, now);
#endif
}

static void client_pool_set_deadline(struct rigctld_client_pool *pool,
                                     struct rigctld_client_slot *slot)
{
    client_pool_now(pool, &slot->auth_deadline);
    slot->auth_deadline.tv_sec += pool->auth_timeout_ms / 1000;
    slot->auth_deadline.tv_nsec +=
        (long)(pool->auth_timeout_ms % 1000) * 1000000L;

    if (slot->auth_deadline.tv_nsec >= 1000000000L)
    {
        slot->auth_deadline.tv_sec++;
        slot->auth_deadline.tv_nsec -= 1000000000L;
    }

    slot->auth_deadline_active = pool->auth_timeout_ms != 0;
}

static int timespec_compare(const struct timespec *left,
                            const struct timespec *right)
{
    if (left->tv_sec != right->tv_sec)
    {
        return left->tv_sec < right->tv_sec ? -1 : 1;
    }

    if (left->tv_nsec != right->tv_nsec)
    {
        return left->tv_nsec < right->tv_nsec ? -1 : 1;
    }

    return 0;
}

static void rigctld_socket_shutdown(int socket_fd)
{
#ifdef _WIN32
    shutdown((SOCKET)socket_fd, SD_BOTH);
#else
    shutdown(socket_fd, SHUT_RDWR);
#endif
}

static void *client_pool_watchdog(void *data)
{
    struct rigctld_client_pool *pool = data;

    pthread_mutex_lock(&pool->mutex);

    while (!pool->stopping)
    {
        struct timespec earliest = { 0 };
        struct timespec now;
        int have_deadline = 0;

        client_pool_now(pool, &now);

        for (unsigned int i = 0; i < pool->limit; i++)
        {
            struct rigctld_client_slot *slot = &pool->slots[i];

            if (!slot->active || !slot->auth_deadline_active)
            {
                continue;
            }

            if (timespec_compare(&slot->auth_deadline, &now) <= 0)
            {
                slot->auth_deadline_active = 0;
                rigctld_socket_shutdown(slot->socket_fd);
                continue;
            }

            if (!have_deadline
                    || timespec_compare(&slot->auth_deadline, &earliest) < 0)
            {
                earliest = slot->auth_deadline;
                have_deadline = 1;
            }
        }

        if (pool->stopping)
        {
            break;
        }

        if (have_deadline)
        {
            pthread_cond_timedwait(&pool->condition, &pool->mutex, &earliest);
        }
        else
        {
            pthread_cond_wait(&pool->condition, &pool->mutex);
        }
    }

    pthread_mutex_unlock(&pool->mutex);
    return NULL;
}

int rigctld_client_pool_init(struct rigctld_client_pool *pool,
                             unsigned int maximum,
                             unsigned int auth_timeout_ms)
{
    pthread_condattr_t condition_attr;
    int retval;

    if (!pool || maximum == 0 || maximum > RIGCTLD_MAX_CLIENTS)
    {
        return -RIG_EINVAL;
    }

    memset(pool, 0, sizeof(*pool));
    pool->limit = maximum;
    pool->auth_timeout_ms = auth_timeout_ms;

    retval = pthread_mutex_init(&pool->mutex, NULL);

    if (retval != 0)
    {
        return -RIG_EIO;
    }

    retval = pthread_condattr_init(&condition_attr);

    if (retval != 0)
    {
        pthread_mutex_destroy(&pool->mutex);
        return -RIG_EIO;
    }

#if defined(_POSIX_CLOCK_SELECTION) && _POSIX_CLOCK_SELECTION >= 0
    pool->use_monotonic =
        pthread_condattr_setclock(&condition_attr, CLOCK_MONOTONIC) == 0;
#endif
    retval = pthread_cond_init(&pool->condition, &condition_attr);
    pthread_condattr_destroy(&condition_attr);

    if (retval != 0)
    {
        pthread_mutex_destroy(&pool->mutex);
        return -RIG_EIO;
    }

    for (unsigned int i = 0; i < pool->limit; i++)
    {
        pool->slots[i].socket_fd = -1;
    }

    pool->initialized = 1;

    if (auth_timeout_ms != 0)
    {
        retval = pthread_create(&pool->watchdog_thread, NULL,
                                client_pool_watchdog, pool);

        if (retval != 0)
        {
            rigctld_client_pool_destroy(pool);
            return -RIG_EIO;
        }

        pool->watchdog_started = 1;
    }

    return RIG_OK;
}

int rigctld_client_reserve(struct rigctld_client_pool *pool, int socket_fd,
                           int auth_required)
{
    int client_slot = 0;

    pthread_mutex_lock(&pool->mutex);

    while (pool->closing && !pool->stopping)
    {
        pthread_cond_wait(&pool->condition, &pool->mutex);
    }

    if (!pool->stopping && pool->active < pool->limit)
    {
        for (unsigned int i = 0; i < pool->limit; i++)
        {
            struct rigctld_client_slot *slot = &pool->slots[i];

            if (slot->active)
            {
                continue;
            }

            slot->active = 1;
            slot->socket_fd = socket_fd;
            slot->authenticated = !auth_required;
            slot->auth_deadline_active = 0;

            if (auth_required)
            {
                client_pool_set_deadline(pool, slot);
            }

            pool->active++;
            client_slot = (int)i + 1;
            pthread_cond_broadcast(&pool->condition);
            break;
        }
    }

    pthread_mutex_unlock(&pool->mutex);
    return client_slot;
}

unsigned int rigctld_client_release(struct rigctld_client_pool *pool,
                                    int client_slot)
{
    return rigctld_client_release_last(pool, client_slot, NULL, NULL);
}

unsigned int rigctld_client_release_last(struct rigctld_client_pool *pool,
        int client_slot,
        rigctld_last_client_cb_t callback, void *data)
{
    unsigned int active;
    int released = 0;

    pthread_mutex_lock(&pool->mutex);

    if (client_slot > 0 && (unsigned int)client_slot <= pool->limit
            && pool->slots[client_slot - 1].active)
    {
        struct rigctld_client_slot *slot = &pool->slots[client_slot - 1];

        slot->active = 0;
        slot->socket_fd = -1;
        slot->authenticated = 0;
        slot->auth_deadline_active = 0;
        pool->active--;
        released = 1;
    }

    active = pool->active;

    // Keep new reservations excluded while the daemon closes an idle rig.
    if (released && active == 0 && callback != NULL && !pool->stopping)
    {
        pool->closing = 1;
        pthread_mutex_unlock(&pool->mutex);
        callback(data);
        pthread_mutex_lock(&pool->mutex);
        pool->closing = 0;
        pthread_cond_broadcast(&pool->condition);
    }

    pthread_cond_broadcast(&pool->condition);

    pthread_mutex_unlock(&pool->mutex);
    return active;
}

unsigned int rigctld_client_count(struct rigctld_client_pool *pool)
{
    unsigned int active;

    pthread_mutex_lock(&pool->mutex);
    active = pool->active;
    pthread_mutex_unlock(&pool->mutex);
    return active;
}

void rigctld_client_set_authenticated(struct rigctld_client_pool *pool,
                                      int client_slot, int authenticated)
{
    if (!pool || client_slot <= 0 || (unsigned int)client_slot > pool->limit)
    {
        return;
    }

    pthread_mutex_lock(&pool->mutex);

    if (pool->slots[client_slot - 1].active && !pool->stopping)
    {
        if (authenticated)
        {
            pool->slots[client_slot - 1].authenticated = 1;
            pool->slots[client_slot - 1].auth_deadline_active = 0;
        }
        else if (pool->slots[client_slot - 1].authenticated)
        {
            pool->slots[client_slot - 1].authenticated = 0;
            client_pool_set_deadline(pool, &pool->slots[client_slot - 1]);
        }

        pthread_cond_broadcast(&pool->condition);
    }

    pthread_mutex_unlock(&pool->mutex);
}

void rigctld_client_detach_socket(struct rigctld_client_pool *pool,
                                  int client_slot)
{
    if (!pool || client_slot <= 0 || (unsigned int)client_slot > pool->limit)
    {
        return;
    }

    pthread_mutex_lock(&pool->mutex);

    if (pool->slots[client_slot - 1].active)
    {
        pool->slots[client_slot - 1].socket_fd = -1;
        pool->slots[client_slot - 1].auth_deadline_active = 0;
        pthread_cond_broadcast(&pool->condition);
    }

    pthread_mutex_unlock(&pool->mutex);
}

void rigctld_client_pool_stop(struct rigctld_client_pool *pool)
{
    if (!pool || !pool->initialized)
    {
        return;
    }

    pthread_mutex_lock(&pool->mutex);
    pool->stopping = 1;

    for (unsigned int i = 0; i < pool->limit; i++)
    {
        if (pool->slots[i].active && pool->slots[i].socket_fd >= 0)
        {
            pool->slots[i].auth_deadline_active = 0;
            rigctld_socket_shutdown(pool->slots[i].socket_fd);
        }
    }

    pthread_cond_broadcast(&pool->condition);
    pthread_mutex_unlock(&pool->mutex);

    if (pool->watchdog_started)
    {
        pthread_join(pool->watchdog_thread, NULL);
        pool->watchdog_started = 0;
    }

    pthread_mutex_lock(&pool->mutex);

    while (pool->active != 0 || pool->closing)
    {
        pthread_cond_wait(&pool->condition, &pool->mutex);
    }

    pthread_mutex_unlock(&pool->mutex);
}

void rigctld_client_pool_destroy(struct rigctld_client_pool *pool)
{
    if (!pool || !pool->initialized)
    {
        return;
    }

    rigctld_client_pool_stop(pool);
    pthread_cond_destroy(&pool->condition);
    pthread_mutex_destroy(&pool->mutex);
    pool->initialized = 0;
}

void rigctld_socket_close(int socket_fd)
{
#ifdef _WIN32
    closesocket((SOCKET)socket_fd);
#else
    close(socket_fd);
#endif
}

int rigctld_socket_set_timeout(int socket_fd, unsigned int seconds)
{
#ifdef _WIN32
    DWORD timeout = seconds * 1000;
    int retval = setsockopt((SOCKET)socket_fd, SOL_SOCKET, SO_RCVTIMEO,
                            (const char *)&timeout, sizeof(timeout));

    if (retval == 0)
    {
        retval = setsockopt((SOCKET)socket_fd, SOL_SOCKET, SO_SNDTIMEO,
                            (const char *)&timeout, sizeof(timeout));
    }

#else
    struct timeval timeout = { .tv_sec = seconds, .tv_usec = 0 };
    int retval = setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO,
                            &timeout, sizeof(timeout));

    if (retval == 0)
    {
        retval = setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO,
                            &timeout, sizeof(timeout));
    }

#endif

    if (retval != 0)
    {
#ifdef _WIN32
        rig_debug(RIG_DEBUG_ERR, "%s: setsockopt failed: %d\n", __func__,
                  WSAGetLastError());
#else
        rig_debug(RIG_DEBUG_ERR, "%s: setsockopt failed: %s\n", __func__,
                  strerror(errno));
#endif
        return -RIG_EIO;
    }

    return RIG_OK;
}

void rigctld_client_id_init(void)
{
    pthread_key_create(&client_id_key, NULL);
}

void rigctld_client_id_set(int client_id)
{
    pthread_setspecific(client_id_key, (void *)(intptr_t)client_id);
}

int rigctld_client_id_get(void)
{
    return (int)(intptr_t)pthread_getspecific(client_id_key);
}
