// can run this using rigctl/rigctld and socat pty devices
// socat -d -d pty,raw,echo=0 pty,raw,echo=0
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BUFSIZE 256

float freqA = 14074000;
float freqB = 14074500;

// ID 0310 == 310, Must drop leading zero
typedef enum nc_rigid_e
{
    NC_RIGID_NONE            = 0,
    NC_RIGID_FT450           = 241,
    NC_RIGID_FT450D          = 244,
    NC_RIGID_FT950           = 310,
    NC_RIGID_FT891           = 135,
    NC_RIGID_FT991           = 135,
    NC_RIGID_FT2000          = 251,
    NC_RIGID_FT2000D         = 252,
    NC_RIGID_FTDX1200        = 583,
    NC_RIGID_FTDX9000D       = 101,
    NC_RIGID_FTDX9000Contest = 102,
    NC_RIGID_FTDX9000MP      = 103,
    NC_RIGID_FTDX5000        = 362,
    NC_RIGID_FTDX3000        = 460,
    NC_RIGID_FTDX101D        = 681,
    NC_RIGID_FTDX101MP       = 682
} nc_rigid_t;

int
getmyline(FILE *fp, char *buf)
{
    int c;
    int i = 0;
    memset(buf, 0, BUFSIZE);

    while ((c = fgetc(fp)) != EOF)
    {
        buf[i++] = c;

        if (c == ';') { return strlen(buf); }
    }

    return strlen(buf);
}
int main()
{
    char buf[256];
    int n;
    FILE *fp = fopen("/dev/pts/3", "r+");

    //,while(getmyline(fp,buf))
    while (1)
    {
        getmyline(fp, buf);

        if (strcmp(buf, "RM5;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            n = fprintf(fp, "%s", "RM5100000;");
            printf("n=%d\n", n);

            if (n <= 0) { perror("RM5"); }

            fflush(fp);
        }

        if (strcmp(buf, "AN0;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            n = fprintf(fp, "%s", "AN030;");
            printf("n=%d\n", n);

            if (n <= 0) { perror("AN"); }

            fflush(fp);
        }
        else if (strcmp(buf, "IF;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            n = fprintf(fp, "%s", "IF059014200000+000000700000;");
            printf("n=%d\n", n);

            if (n <= 0) { perror("IF"); }

            fflush(fp);
        }
        else if (strcmp(buf, "ID;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
//            int id = NC_RIGID_FTDX101D;
            int id = NC_RIGID_FTDX3000;
            n = fprintf(fp, "ID%03d", id);
            printf("n=%d\n", n);

            if (n <= 0) { perror("ID"); }

            fflush(fp);
        }

#if 0
        else if (strncmp(buf, "AI", 2) == 0)
        {
            if (strcmp(buf, "AI;"))
            {
                printf("%s\n", buf);
                usleep(50 * 1000);
                n = fprintf(fp, "%s", "AI0;");
                printf("n=%d\n", n);

                if (n <= 0) { perror("AI"); }

                fflush(fp);
            }
        }

#endif
        else if (strcmp(buf, "VS;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            n = fprintf(fp, "%s", "VS0;");
            printf("n=%d\n", n);

            if (n < 0) { perror("VS"); }

            fflush(fp);
        }
        else if (strcmp(buf, "EX032;") == 0)
        {
            static int ant = 0;
            ant = (ant + 1) % 3;
            printf("%s\n", buf);
            usleep(50 * 1000);
            n = fprintf(fp, "EX032%1d;", ant);
            printf("n=%d\n", n);

            if (n < 0) { perror("EX032"); }

            fflush(fp);
        }

        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }

    }

    return 0;
}
