#include <hamlib/rig.h>
#include <stdio.h>
#include "fifo.h"


void initFIFO(FIFO *fifo)
{
    fifo->head = 0;
    fifo->tail = 0;
}

void resetFIFO(FIFO *fifo)
{
    fifo->head = fifo->tail;
}

// returns RIG_OK if added
// return -RIG error if overflow
int push(FIFO *fifo, const char *msg)
{
    int len = strlen(msg);

    for (int i = 0; i < len; ++i)
    {
        fifo->data[fifo->tail] = msg[i];
        rig_debug(RIG_DEBUG_VERBOSE, "%s: push %c (%d,%d)\n", __func__, msg[i],
                  fifo->head, fifo->tail);

        if (fifo->tail + 1 == fifo->head) { return -RIG_EDOM; }

        fifo->tail = (fifo->tail + 1) % HAMLIB_FIFO_SIZE;
    }

    return RIG_OK;
}

char pop(FIFO *fifo)
{
    if (fifo->tail == fifo->head) { return -1; }

    char c = fifo->data[fifo->head];
    rig_debug(RIG_DEBUG_VERBOSE, "%s: pop %c (%d,%d)\n", __func__, c, fifo->head,
              fifo->tail);
    fifo->head = (fifo->head + 1) % HAMLIB_FIFO_SIZE;
    return c;
}

#ifdef TEST
int main()
{
    FIFO fifo;
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
