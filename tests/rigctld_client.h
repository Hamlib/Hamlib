/* Per-client identity management for rigctld. */
/* Provides thread-safe client ID accessors for per-connection tracking. */

#ifndef RIGCTLD_CLIENT_H
#define RIGCTLD_CLIENT_H

/* Maximum concurrent TCP clients (reject connections beyond this). */
#define RIGCTLD_MAX_CLIENTS 128

/* Initialize client ID subsystem.  Call once at rigctld startup. */
void rigctld_client_id_init(void);

/* Set the calling thread's client ID.  Call once per client thread. */
void rigctld_client_id_set(int client_id);

/* Get the calling thread's client ID.  Returns 0 if not set. */
int  rigctld_client_id_get(void);

#endif /* RIGCTLD_CLIENT_H */
