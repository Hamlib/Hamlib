/* This can check for balanced ENTERFUNC/RETURNFUNC statements */
/* RETURNFUNC2 is used when ENTERFUNC is not used in a function */
//
// Still may give false positives for /* */ comments or string literals
// Have to live with them until we can "borrow" a C parser/tokenizer
//  that doesn't bloat the build requirements.
//
// Note that this is very dependent on the Hamlib coding style
#include <stdio.h>
#include <string.h>

int main(int argc, const char *argv[])
{
    char buf[4096];
    const char *fname;
    char *s;
    FILE *fp;
    int idx, inafunc;
    int linenum;
    int enterfunc;
    int retval = 0;

    for (idx = 1; idx < argc; idx++)
    {
        fp = fopen(argv[idx], "r");

        if (fp == NULL) { fprintf(stderr, "Can't open %s\n", argv[idx]); continue;}

#if 0

        if (!(fname = strrchr(argv[idx], '/')))
        {
            fname = argv[idx];
        }
        else
        {
            fname++;
        }

#else
        fname = argv[idx];
#endif
        linenum = enterfunc = inafunc = 0;

        while (fgets(buf, sizeof(buf), fp))
        {
            ++linenum;

            if (buf[0] == '{')
            {
                //start of a function definition
                inafunc = 1;
                enterfunc = 0;
                continue;
            }
            else if (buf[0] == '}')
            {
                // end of function definition; back to comments
                inafunc = 0;
            }

            if (!inafunc)
            {
                // Not in a function, ignore everything
                continue;
            }

            // Take care of // comments
            if ((s = strstr(buf, "//")))
            {
                *s = '\0';
            }

            if (strstr(buf, "ENTERFUNC;")) { enterfunc = 1; }

            if (enterfunc && (strstr(buf, "RETURNFUNC2") || strstr(buf, "return ")
                              || strstr(buf, "return;")))
            {
                printf("%s:%d need RETURNFUNC\n", fname, linenum);
                retval = 1;
            }

            //TODO: Fix next to make a better guess at what's missing
            if (!enterfunc && strstr(buf, "RETURNFUNC("))
            {
                printf("%s:%d need RETURNFUNC2\n", fname, linenum);
                retval = 1;
            }

            if (strstr(buf, "RETURNFUNC("))
            {
                if (enterfunc == 0) { printf("%s:%d no matching ENTERFUNC\n", fname, linenum); }
            }
        }

        //printf("%s:%d Done\n", fname, linenum);
        fclose(fp);
    }

    return retval;
}
