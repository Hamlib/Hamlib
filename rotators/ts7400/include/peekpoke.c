#include<unistd.h>
#include<sys/types.h>
#include<sys/mman.h>
#include<stdio.h>
#include<fcntl.h>

unsigned int parseBinary(char *str)
{
    unsigned int val = 0;

    if (*str == 'b')
    {
        str++;

        while (*str)
        {
            if (*str == '0')
            {
                val <<= 1;
            }
            else if (*str == '1')
            {
                val = (val << 1) + 1;
            }
            else
            {
                goto binaryError;
            }
        }
    }

    return val;
binaryError:
    fprintf(stderr, "Unrecognized numeric value: %s\n", str);
    exit(0);
}

unsigned int parseNumber(char *str)
{
    unsigned int addr = 0;

    if (!sscanf(str, "0x%x", &addr))
    {
        if (!sscanf(str, "%u", &addr))
        {
            addr = parseBinary(str);
        }
    }

    return addr;
}

/*
  Features that the old peekXX/pokeXX did not have:
  1. Support for 8/16/32 bit READ/WRITE in one function
  2. Support for decimal and binary values
  3. The value return is returned (to become the status code)
 */
int main(int argc, char **argv)
{
    off_t addr, page;
    int fd, bits, dowrite = 0, doread = 1;
    unsigned char *start;
    unsigned short *shortdat, shortval;
    unsigned int *intdat, intval;

    if (argc < 3 || argc > 5)
    {
        fprintf(stderr, "Usage: peekpoke BIT_WIDTH ADDRESS <VALUE <x>>\n");
        fprintf(stderr, "<x> can be anything; suppresses read-back on write\n");
        return 0;
    }

    sscanf(argv[1], "%d", &bits);

    if (bits != 8 && bits != 16 && bits != 32)
    {
        fprintf(stderr, "Error: BIT_WIDTH must be 8, 16, or 32\n");
        return 0;
    }

    addr = parseNumber(argv[2]);

    if (argc > 3)    // peekpoke BITS ADDRESS VALUE x
    {
        intval = parseNumber(argv[3]);

        if (argc > 4) { doread = 0; }

        dowrite = 1;
    }

    fd = open("/dev/mem", O_RDWR | O_SYNC);

    if (fd == -1)
    {
        perror("open(/dev/mem):");
        return 0;
    }

    page = addr & 0xfffff000;
    start = mmap(0, getpagesize(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, page);

    if (start == MAP_FAILED)
    {
        perror("mmap:");
        close(fd);
        return 0;
    }

    if (bits == 8)
    {
        unsigned char *chardat, charval;
        charval = (unsigned char)intval;
        chardat = start + (addr & 0xfff);

        if (dowrite)
        {
            *chardat = charval;
        }

        if (doread)
        {
            intval = (unsigned int) * chardat;
        }
    }
    else if (bits == 16)
    {
        shortval = (unsigned short)intval;
        shortdat = (unsigned short *)(start + (addr & 0xfff));

        if (dowrite)
        {
            *shortdat = shortval;
        }

        if (doread)
        {
            intval = (unsigned int) * shortdat;
        }
    }
    else     // bits == 32
    {
        intdat = (unsigned int *)(start + (addr & 0xfff));

        if (dowrite)
        {
            *intdat = intval;
        }

        if (doread)
        {
            intval = *intdat;
        }
    }

    if (doread)
    {
        printf("0x%X\n", intval);
    }

    close(fd);
    return intval;
}
