// can run this using rigctl/rigctld and socat pty devices
// gcc -o simyaesu simyaesu.c
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#if 0
struct ip_mreq
{
    int dummy;
};
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "../include/hamlib/rig.h"

#define BUFSIZE 256

float freqA = 14074000;
float freqB = 14074500;
char tx_vfo = '0';
char rx_vfo = '0';
char modeA = '1';
char modeB = '1';
int ks = 20;
int bandselect = 5;
int  width = 21;
int narrow = 0;
int vd = 0;
int sm0 = 0;
int sm1 = 0;
int vs = 0;
int vx = 0;
int pa = 0;
int ra = 0;
int ag = 0;
int pc = 100;
int is = 0;
int bp_on = 0;
int bp_pos = 0;
int rl = 0;
int nb = 0;
int nr = 0;
int tx = 0;
int mg = 0;
int rg = 100;
int vg = 0;
int kr = 0;
int bi = 0;
int gt = 0;
int ex016 = 0;
int ex020 = 0;

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
getmyline(int fd, char *buf)
{
    char c;
    int i = 0;
    memset(buf, 0, BUFSIZE);

    while (read(fd, &c, 1) > 0)
    {
        buf[i++] = c;

        if (c == ';') { return strlen(buf); }
    }

    if (strlen(buf) == 0) { hl_usleep(10 * 1000); }

    return strlen(buf);
}

#if defined(WIN32) || defined(_WIN32)
int openPort(char *comport) // doesn't matter for using pts devices
{
    int fd;
    fd = open(comport, O_RDWR);

    if (fd < 0)
    {
        perror(comport);
    }

    return fd;
}

#else
int openPort(char *comport) // doesn't matter for using pts devices
{
    int fd = posix_openpt(O_RDWR);
    char *name = ptsname(fd);

    if (name == NULL)
    {
        perror("pstname");
        return -1;
    }

    printf("name=%s\n", name);

    if (fd == -1 || grantpt(fd) == -1 || unlockpt(fd) == -1)
    {
        perror("posix_openpt");
        return -1;
    }

    return fd;
}
#endif



int main(int argc, char *argv[])
{
    char buf[256];
    char *pbuf;
    int n;
    int fd = openPort(argv[1]);

    while (1)
    {
        if (getmyline(fd, buf))
        {
//            printf("Cmd:%s\n", buf);
        }
        else { continue; }

        if (strcmp(buf, ";") == 0)
        {
            pbuf = "?;";
            n = write(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "RM4;") == 0)
        {
            hl_usleep(50 * 1000);
            pbuf = "RM4100;";
            n = write(fd, pbuf, strlen(pbuf));

            if (n <= 0) { perror("RM4"); }
        }
        else if (strcmp(buf, "RM5;") == 0)
        {
            hl_usleep(50 * 1000);
            pbuf = "RM4100000;";
            n = write(fd, pbuf, strlen(pbuf));

            if (n <= 0) { perror("RM5"); }
        }
        else if (strcmp(buf, "RM6;") == 0)
        {
            hl_usleep(50 * 1000);
            pbuf = "AN030;";
            n = write(fd, pbuf, strlen(pbuf));

            if (n <= 0) { perror("AN"); }
        }
        else if (strcmp(buf, "IF;") == 0)
        {
            hl_usleep(50 * 1000);
            pbuf = "IF059014200000+000000700000;";
            n = write(fd, pbuf, strlen(pbuf));

            if (n <= 0) { perror("IF"); }
        }
        else if (strcmp(buf, "FA;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FA%08.0f;", freqA);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FA", 2) == 0)
        {
            sscanf(buf, "FA%f", &freqA);
        }
        else if (strcmp(buf, "FB;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FB%08.0f;", freqB);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FB", 2) == 0)
        {
            sscanf(buf, "FB%f", &freqB);
        }
        else if (strcmp(buf, "ID;") == 0)
        {
            hl_usleep(50 * 1000);
            int id = NC_RIGID_FTDX3000;
            SNPRINTF(buf, sizeof(buf), "ID%03d;", id);
            n = write(fd, buf, strlen(buf));

            if (n <= 0) { perror("ID"); }
        }
        else if (strcmp(buf, "PS;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PS1;");
            n = write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "AI;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "AI0;");
            n = write(fd, buf, strlen(buf));

            if (n <= 0) { perror("ID"); }
        }
        else if (strcmp(buf, "AI0;") == 0)
        {
            hl_usleep(50 * 1000);
        }
        else if (strcmp(buf, "FT;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "FT%c;", tx_vfo);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("FT"); }
        }
        else if (strncmp(buf, "FT", 2) == 0)
        {
            tx_vfo = buf[2];
        }
        else if (strcmp(buf, "MD0;") == 0)
        {
            printf("MD=%s\n", buf);
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "MD0%c;", modeA);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("MD0;"); }
        }
        else if (strncmp(buf, "MD0", 3) == 0)
        {
            modeA = buf[3];
        }
        else if (strcmp(buf, "MD1;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "MD1%c;", modeB);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("MD0;"); }
        }
        else if (strncmp(buf, "MD1", 3) == 0)
        {
            modeB = buf[3];
        }



#if 0
        else if (strncmp(buf, "AI", 2) == 0)
        {
            if (strcmp(buf, "AI;"))
            {
                hl_usleep(50 * 1000);
                n = fprintf(fp, "%s", "AI0;");

                if (n <= 0) { perror("AI"); }
            }
        }

#endif
        else if (strcmp(buf, "VS;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "VS%d;", vs);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("VS"); }
        }
        else if (strncmp(buf, "VS", 2) == 0)
        {
            sscanf(buf, "VS%d", &vs);
        }
        else if (strcmp(buf, "KR;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "KR%d;", kr);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("KR"); }
        }
        else if (strncmp(buf, "KR", 2) == 0)
        {
            sscanf(buf, "KR%d", &kr);
        }
        else if (strcmp(buf, "BI;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "BI%d;", bi);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("BI"); }
        }
        else if (strncmp(buf, "BI", 2) == 0)
        {
            sscanf(buf, "BI%d", &bi);
        }
        else if (strcmp(buf, "VX;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "VX%d;", vx);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("VX"); }
        }
        else if (strncmp(buf, "VX", 2) == 0)
        {
            sscanf(buf, "VX%d", &vx);
        }

        else if (strcmp(buf, "PA;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "PA%d;", pa);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("PA"); }
        }
        else if (strncmp(buf, "PA", 2) == 0)
        {
            sscanf(buf, "PA%d", &vs);
        }
        else if (strcmp(buf, "RA;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "RA%d;", ra);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("RA"); }
        }
        else if (strncmp(buf, "RA", 2) == 0)
        {
            sscanf(buf, "RA%d", &ra);
        }
        else if (strcmp(buf, "AG;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "AG%d;", ag);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("AG"); }
        }
        else if (strncmp(buf, "AG", 2) == 0)
        {
            sscanf(buf, "AG%d", &ag);
        }
        else if (strcmp(buf, "PC;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "PC%03d;", pc);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("PC"); }
        }
        else if (strncmp(buf, "PC", 2) == 0)
        {
            sscanf(buf, "PC%d", &pc);
        }
        else if (strcmp(buf, "VG;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "VG%03d;", vg);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("VG"); }
        }
        else if (strncmp(buf, "VG", 2) == 0)
        {
            sscanf(buf, "VG%d", &vg);
        }
        else if (strcmp(buf, "RG0;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "RG0%03d;", rg);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("RG"); }
        }
        else if (strncmp(buf, "RG", 2) == 0)
        {
            sscanf(buf, "RG0%d", &rg);
        }
        else if (strcmp(buf, "GT0;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "GT0%0d;", gt);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("GT"); }
        }
        else if (strncmp(buf, "GT", 2) == 0)
        {
            sscanf(buf, "GT0%d", &gt);
        }
        else if (strcmp(buf, "TX;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "TX+%04d;", tx);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("TX"); }
        }
        else if (strncmp(buf, "TX", 2) == 0)
        {
            sscanf(buf, "TX%d", &tx);
        }
        else if (strcmp(buf, "IS;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "IS+%04d;", is);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("IS"); }
        }
        else if (strncmp(buf, "IS", 2) == 0)
        {
            sscanf(buf, "IS%d", &is);
        }
        else if (strcmp(buf, "RL0;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "RL0%d;", rl);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("RL"); }
        }
        else if (strncmp(buf, "RL", 2) == 0)
        {
            sscanf(buf, "RL0%02d", &rl);
        }
        else if (strcmp(buf, "BP00;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "BP0%d;", bp_on);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("BP"); }
        }
        else if (strncmp(buf, "BP00", 4) == 0)
        {
            sscanf(buf, "BP00%d", &bp_on);
        }
        else if (strcmp(buf, "BP01;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "BP0%d;", bp_pos);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("BP"); }
        }
        else if (strncmp(buf, "BP01", 4) == 0)
        {
            sscanf(buf, "BP01%d", &bp_pos);
        }
        else if (strcmp(buf, "NB0;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "NB0%d;", nb);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("NB"); }
        }
        else if (strncmp(buf, "NB0", 3) == 0)
        {
            sscanf(buf, "NB0%d", &nb);
        }
        else if (strcmp(buf, "NR0;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "NR0%d;", nr);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("NR"); }
        }
        else if (strncmp(buf, "NR0", 3) == 0)
        {
            sscanf(buf, "NR0%d", &nr);
        }


        else if (strcmp(buf, "EX032;") == 0)
        {
            static int ant = 0;
            ant = (ant + 1) % 3;
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "EX032%1d;", ant);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("EX032"); }
        }
        else if (strcmp(buf, "EX016;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "EX016%04d;", ex016);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("EX016"); }
        }
        else if (strcmp(buf, "EX020;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "EX020%04d;", ex020);
            n = write(fd, buf, strlen(buf));

            if (n < 0) { perror("EX016"); }
        }
        else if (strncmp(buf, "EX020", 5) == 0)
        {
            sscanf(buf, "EX020%d\n", &ex020);
        }
        else if (strncmp(buf, "KS;", 3) == 0)
        {
            sprintf(buf, "KS%d;", ks);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "KS", 2) == 0)
        {
            sscanf(buf, "KS%03d", &ks);
        }
        else if (strncmp(buf, "MG;", 3) == 0)
        {
            sprintf(buf, "MG%03d;", mg);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "MG", 2) == 0)
        {
            sscanf(buf, "MG%03d", &mg);
        }
        else if (strncmp(buf, "BS;", 3) == 0) // cannot query BS
        {
            sprintf(buf, "BS%02d;", bandselect);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SH0;", 4) == 0)
        {
            sprintf(buf, "SH0%02d;", width);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SH0", 3) == 0)
        {
            sscanf(buf, "SH0%02d", &width);
        }
        else if (strncmp(buf, "NA0;", 4) == 0)
        {
            sprintf(buf, "NA0%d;", narrow);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "NA0", 3) == 0)
        {
            sscanf(buf, "NA0%d", &narrow);
        }
        else if (strncmp(buf, "VD;", 3) == 0)
        {
            sprintf(buf, "VD%d;", vd);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "VD", 2) == 0)
        {
            sscanf(buf, "VD%d", &vd);
        }
        else if (strncmp(buf, "SM0;", 4) == 0)
        {
            sprintf(buf, "SM0%d;", sm0);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SM0", 3) == 0)
        {
            sscanf(buf, "SM0%3d", &sm0);
        }
        else if (strncmp(buf, "SM1;", 4) == 0)
        {
            sprintf(buf, "SM1%d;", sm1);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SM1", 3) == 0)
        {
            sscanf(buf, "SM1%3d", &sm1);
        }

        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }

    }

    return 0;
}
