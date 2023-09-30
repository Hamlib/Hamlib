#include <hamlib/rig.h>
#include <stdio.h>
#include <ctype.h>
#include "fifo.h"
#include "config.h"

void initFIFO(FIFO_RIG *fifo)
{
    fifo->head = 0;
    fifo->tail = 0;
#ifdef _PTHREAD_H
    static pthread_mutex_t t = PTHREAD_MUTEX_INITIALIZER;
    fifo->mutex = t;
#endif
}

void resetFIFO(FIFO_RIG *fifo)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: fifo flushed\n", __func__);
    fifo->head = fifo->tail;
    fifo->flush = 1;
}

// returns RIG_OK if added
// return -RIG error if overflow
int push(FIFO_RIG *fifo, const char *msg)
{
#ifdef _PTHREAD_H
    pthread_mutex_lock(&fifo->mutex);
#endif
    int len = strlen(msg);

    for (int i = 0; i < len; ++i)
    {
        // FIFO is meant for CW use only
        // So we skip some chars that don't work with CW
        if (msg[i] & 0x80) continue; // drop any chars that have high bit set
        switch(msg[i])
        {
            case 0x0d:
            case 0x0a:
            continue;
        }
        fifo->data[fifo->tail] = msg[i];
        if (isalnum(msg[i]))
        rig_debug(RIG_DEBUG_VERBOSE, "%s: push %c (%d,%d)\n", __func__, msg[i],
                  fifo->head, fifo->tail);
        else
        rig_debug(RIG_DEBUG_VERBOSE, "%s: push 0x%02x (%d,%d)\n", __func__, msg[i],
                  fifo->head, fifo->tail);

        if (fifo->tail + 1 == fifo->head) { return -RIG_EDOM; }

        fifo->tail = (fifo->tail + 1) % HAMLIB_FIFO_SIZE;
    }

#ifdef _PTHREAD_H
    pthread_mutex_unlock(&fifo->mutex);
#endif
    return RIG_OK;
}

int peek(FIFO_RIG *fifo)
{
    if (fifo == NULL) return -1;
    if (fifo->tail < 0 || fifo->head < 0) return -1;
    if (fifo->tail > 1023 || fifo->head > 1023) return -1;
    if (fifo->tail == fifo->head) { return -1; }
#ifdef _PTHREAD_H
    pthread_mutex_lock(&fifo->mutex);
#endif
    char c = fifo->data[fifo->head];

#if 0
    if (isalnum(c))
    rig_debug(RIG_DEBUG_VERBOSE, "%s: peek %c (%d,%d)\n", __func__, c, fifo->head,
              fifo->tail);
    else
    rig_debug(RIG_DEBUG_VERBOSE, "%s: peek 0x%02x (%d,%d)\n", __func__, c, fifo->head,
              fifo->tail);
#endif
#ifdef _PTHREAD_H
    pthread_mutex_unlock(&fifo->mutex);
#endif
    return c;
}

int pop(FIFO_RIG *fifo)
{
    if (fifo->tail == fifo->head) { return -1; }

#ifdef _PTHREAD_H
    pthread_mutex_lock(&fifo->mutex);
#endif
    char c = fifo->data[fifo->head];
#if 0
    if (isalnum(c))
    rig_debug(RIG_DEBUG_VERBOSE, "%s: pop %c (%d,%d)\n", __func__, c, fifo->head,
              fifo->tail);
    else
    rig_debug(RIG_DEBUG_VERBOSE, "%s: pop 0x%02x (%d,%d)\n", __func__, c, fifo->head,
              fifo->tail);
#endif
    fifo->head = (fifo->head + 1) % HAMLIB_FIFO_SIZE;
#ifdef _PTHREAD_H
    pthread_mutex_unlock(&fifo->mutex);
#endif
    return c;
}

#ifdef TEST
int main()
{
    FIFO_RIG fifo;
    initFIFO(&fifo);

    const char *str = "Hello, World!\n";

    // Pushing the string onto the FIFO
    push(&fifo, str);

    // Popping and printing one character at a time
    int c;

    while ((c = pop(&fifo)) != -1)
    {
        printf("%c", c);
    }

    return 0;
}
#endif
