/* Per-client identity management for rigctld. */
/* Provides thread-safe client ID accessors for per-connection tracking. */

#ifndef RIGCTLD_CLIENT_H
#define RIGCTLD_CLIENT_H

#include <pthread.h>
#include <time.h>

/* Maximum concurrent TCP clients (reject connections beyond this). */
#define RIGCTLD_MAX_CLIENTS 128
#define RIGCTLD_AUTH_TIMEOUT_SECONDS 30
#define RIGCTLD_MAX_AUTH_FAILURES 5

struct rigctld_client_slot
{
    int socket_fd;
    int active;
    int authenticated;
    int auth_deadline_active;
    struct timespec auth_deadline;
};

struct rigctld_client_pool
{
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t watchdog_thread;
    struct rigctld_client_slot slots[RIGCTLD_MAX_CLIENTS];
    unsigned int active;
    unsigned int limit;
    unsigned int auth_timeout_ms;
    int closing;
    int stopping;
    int watchdog_started;
    int use_monotonic;
    int initialized;
};

typedef void (*rigctld_last_client_cb_t)(void *data);

int rigctld_client_pool_init(struct rigctld_client_pool *pool,
                             unsigned int maximum,
                             unsigned int auth_timeout_ms);
void rigctld_client_pool_stop(struct rigctld_client_pool *pool);
void rigctld_client_pool_destroy(struct rigctld_client_pool *pool);
int rigctld_client_reserve(struct rigctld_client_pool *pool, int socket_fd,
                           int auth_required);
unsigned int rigctld_client_release(struct rigctld_client_pool *pool,
                                    int client_slot);
unsigned int rigctld_client_release_last(struct rigctld_client_pool *pool,
        int client_slot,
        rigctld_last_client_cb_t callback, void *data);
unsigned int rigctld_client_count(struct rigctld_client_pool *pool);
void rigctld_client_set_authenticated(struct rigctld_client_pool *pool,
                                      int client_slot, int authenticated);
void rigctld_client_detach_socket(struct rigctld_client_pool *pool,
                                  int client_slot);
void rigctld_socket_close(int socket_fd);
int rigctld_socket_set_timeout(int socket_fd, unsigned int seconds);

/* Initialize client ID subsystem.  Call once at rigctld startup. */
void rigctld_client_id_init(void);

/* Set the calling thread's client ID.  Call once per client thread. */
void rigctld_client_id_set(int client_id);

/* Get the calling thread's client ID.  Returns 0 if not set. */
int  rigctld_client_id_get(void);

#endif /* RIGCTLD_CLIENT_H */
