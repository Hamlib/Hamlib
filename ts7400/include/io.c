// filename button.c
// connect a button to DIO pin 1 and ground
// blinks green and red led on the ts-7200 when button is pressed
//
// compile arm-linux-gcc -o button button.c
//

#include<unistd.h>
#include<sys/types.h>
#include<sys/mman.h>
#include<stdio.h>
#include<fcntl.h>
#include<string.h>

int main(int argc, char **argv)
{
    volatile unsigned int *PEDR, *PEDDR, *PBDR, *PBDDR, *GPIOBDB;
    int i;
    unsigned char state;
    unsigned char *start;
    int fd = open("/dev/mem", O_RDWR | O_SYNC);

    start = mmap(0, getpagesize(), PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                 0x80840000);
    PBDR = (unsigned int *)(start + 0x04);     // port b
    PBDDR = (unsigned int *)(start + 0x14);    // port b direction register
    PEDR = (unsigned int *)(start + 0x20);     // port e data
    PEDDR = (unsigned int *)(start + 0x24);    // port e direction register
    GPIOBDB = (unsigned int *)(start + 0xC4);  // debounce on port b

    *PBDDR = 0xf0;                 // upper nibble output, lower nibble input
    *PEDDR = 0xff;                             // all output (just 2 bits)
    *GPIOBDB = 0x01;               // enable debounce on bit 0

    state = *PBDR;                             // read initial state

    while (state & 0x01)                       // wait until button goes low
    {
        state = *PBDR;                          // remember bit 0 is pulled up with 4.7k ohm
    }


    // blink 5 times, sleep 1 second so it's visible
    for (i = 0; i < 5; i++)
    {
        *PEDR = 0xff;
        sleep(1);
        *PEDR = 0x00;
        sleep(1);
    }

    close(fd);
    return 0;
}
