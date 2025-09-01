#include "hamlib/config.h"

#include <pthread.h>

#define MUTEX(var) static pthread_mutex_t var = PTHREAD_MUTEX_INITIALIZER
#define MUTEX_LOCK(var) pthread_mutex_lock(&var)
#define MUTEX_UNLOCK(var)  pthread_mutex_unlock(&var)

extern int MUTEX_CHECK(pthread_mutex_t *m);
