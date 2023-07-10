void initFIFO(FIFO_RIG *fifo);
void resetFIFO(FIFO_RIG *fifo);
int push(FIFO_RIG *fifo, const char *msg);
char pop(FIFO_RIG *fifo);
