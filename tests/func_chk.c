/* this can change for balanced ENTERFUNC/RETURNFUNC statements */
/* RETURNFUNC2 is used when ENTERFUNC is not used in a function */
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    char buf[4096];
    char buf2[4096];
    FILE *fp = fopen(argv[1], "r");
    int linenum = 0;
    int enterfunc = 0;
    int infunc = 0;
    int retval = 0;
    retval = 0;

    while (fgets(buf, sizeof(buf), fp))
    {
        ++linenum;
        buf[16] = 0;

        if (strstr(buf, "ENTERFUNC;")) { enterfunc = 1; }

        if (enterfunc && strstr(buf, "RETURNFUNC2"))
        {
            printf("Line#%d need RETURNFUNC %s\n", linenum, argv[1]);
            retval = 1;
        }

        strcpy(buf2, buf);
        buf2[15] = 0; // truncate the string

        if (!enterfunc && strstr(buf2, "RETURNFUNC("))
        {
            printf("Line#%d need RETURNFUNC2 %s\n", linenum, argv[1]);
            retval = 1;
        }

        if (strstr(buf2, "RETURNFUNC("))
        {
            if (enterfunc == 0) { printf("Line#%d no matching ENTERFUNC %s\n", linenum, argv[1]); }

            enterfunc = 0;
        }
    }

    fclose(fp);
    return retval;
}
