void initFIFO(FIFO *fifo);
void resetFIFO(FIFO *fifo);
int push(FIFO *fifo, const char *msg);
char pop(FIFO *fifo);
