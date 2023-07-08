#define FIFO_SIZE 1024

typedef struct
{
    char data[FIFO_SIZE];
    int head;
    int tail;
} FIFO;

void initFIFO(FIFO *fifo);
int push(FIFO *fifo, const char *msg);
char pop(FIFO *fifo);
