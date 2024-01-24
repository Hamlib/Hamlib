/* 2038 test
This is OK on 64-bit systems and mingw64
Does fail when compiled with gcc -m32 -o 2038 2038.c
*/
#include <stdio.h>
#include <time.h>
#include <string.h>

int test2038(void)
{
    time_t x;

    //printf("sizeof(time_t)=%d\n", (int)sizeof(time_t));

    x = (time_t)((1U << 31) - 1);
    //printf("x=%lu\n", (unsigned long)x);
    char *s = ctime(&x);

    //printf("%s\n", s);
    //printf("x=%lu\n", (unsigned long)x);
    if (!strstr(s, "2038")) { return 1; }

    x += 1;
    s = ctime(&x);

    //printf("x=%lu\n", (unsigned long)x);
    //printf("%s\n", s);
    if (!strstr(s, "2038")) { return 1; }

    //printf("x=%lu\n", (unsigned long)x);
    //printf("%s\n", s);
    x += 1;
    s = ctime(&x);

    if (!strstr(s, "2038")) { return 1; }

    //printf("%s\n", s);

    return 0;
}

int main(void)
{
    return test2038();
}
