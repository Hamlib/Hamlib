void initFIFO(FIFO_RIG *fifo);
void resetFIFO(FIFO_RIG *fifo);
int push(FIFO_RIG *fifo, const char *msg);
int pop(FIFO_RIG *fifo);
int peek(FIFO_RIG *fifo);
