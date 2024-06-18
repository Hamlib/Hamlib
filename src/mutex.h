#include "hamlib/config.h"

#if defined(HAVE_PTHREAD)
#include <pthread.h>
#endif

#ifdef HAVE_PTHREAD
#define MUTEX(var) static pthread_mutex_t var = PTHREAD_MUTEX_INITIALIZER
#define MUTEX_LOCK(var) pthread_mutex_lock(&var)
#define MUTEX_UNLOCK(var)  pthread_mutex_unlock(&var)
#else
#warning NOT PTHREAD
#define MUTEX(var)
#define MUTEX_LOCK(var)
#define MUTEX_UNLOCK(var)
#endif

extern int MUTEX_CHECK(pthread_mutex_t *m);
