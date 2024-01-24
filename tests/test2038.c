/* 2038 test
This is OK on 64-bit systems and mingw64
Does fail when compiled with gcc -m32 -o 2038 2038.c
*/
#include <stdio.h>
#include <time.h>
#include <string.h>

int main(void)
{
    time_t x;

    x = (time_t)((1U << 31) - 1);
    char *s = ctime(&x);

    //printf("%s", s);
    if (!strstr(s, "2038")) { return 1; }

    x += 1;
    s = ctime(&x);

    if (!strstr(s, "2038")) { return 1; }

    //printf("%s", s);
    x += 1;
    s = ctime(&x);

    if (!strstr(s, "2038")) { return 1; }

    //printf("%s", s);

    return 0;
}
