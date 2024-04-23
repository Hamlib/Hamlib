// can run this using rigctl/rigctld and socat pty devices
// gcc -o simts890 -l hamlib simts890.c
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#if  0
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
#include <errno.h>
#include <hamlib/rig.h>

#define BUFSIZE 256

int mysleep = 20;

int filternum1 = 7;
int filternum2 = 8;
int datamode = 0;
int vfo, vfo_tx, ptt, ptt_data, ptt_mic, ptt_tune;
int operatingband;
int split;
int modeMain = 2;
int modeSub = 1;
int keyspd = 20;
int sl=3, sh=3;
int nr=0;
int pa=0;
int pc=25;
int sm = 35;
int nt = 0;
int ag = 100;
int ac = 0;
int nb1=0,nb2=0;
int sq=0;
int rg=0;
int mg=0;
int ra=0;
int rl=0;
int is=0;
int sp=0;


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

int
getmyline(int fd, char *buf)
{
    char c;
    int i = 0;
    memset(buf, 0, BUFSIZE);
    int retval;

    while ((retval = read(fd, &c, 1)) > 0)
    {
        buf[i++] = c;

        if (c == ';') { return strlen(buf); }
    }

    if (retval != 0)
    {
        perror("read failed:");
        close(fd);
        fd = openPort("");
    }

    if (strlen(buf) == 0) { hl_usleep(10 * 1000); }

    return strlen(buf);
}



int main(int argc, char *argv[])
{
    char buf[256];
    char *pbuf;
    int fd = openPort(argv[1]);
    int freqa = 14074000, freqb = 140735000;
    int modeA = 1, modeB = 2;
    int cmd_err = 0;
    char *err_txt[] = { "?;", "E;", "O;" };
    /* The IF command is not documented for the TS-890S, and is supposed
     *  to be supplanted by SF. However, it is still there for legacy S/W.
     *  This description is taken from the TS-590S/SG manual, with values
     *  reflecting a real TS-890S.
     */
    char IFformat[] = "IF" // Output only
      "%011d"       // P1 freq(Hz)
      "     "       // P2 ??
      " 0000"       // P3 RIT/XIT freq(Hz)
      "0"           // P4 RIT on/off
      "0"           // P5 XIT on/off
      "000"         // P6,P7 mem channel
      "%1d"         // P8 RX/TX
      "%1X"         // P9 Operating mode (See MD command)
      "0"           // P10 Function?
      "0"           // P11 Scan status?
      "0"           // P12 Simplex/Split
      "0"           // P13 Tone/CTCSS (not on TS-890S)
      "00"          // P14 Tone/CTCSS freq (not on TS-890S)
      "0;";         // P15 Always zero
    char SFformat[] = "SF" // Input/Output
      "%1d"         // P1 VFOA/VFOB
      "%011d"       // P2 Freq(Hz)
      "%1X;";       // P3 Mode

    while (1)
    {
        hl_usleep(10);
        buf[0] = 0;

        /* Clean up from last continue - pass along any errors found */
        if (cmd_err != 0)
        {
            write(fd, err_txt[cmd_err - 1], strlen(err_txt[cmd_err - 1]));
            cmd_err = 0;
        }

        if (getmyline(fd, buf) > 0) 
        {
//             printf("Cmd:\"%s\"\n", buf);
        }


//        else { return 0; }

        if (strncmp(buf, "RM2", 3) == 0)
        {
            pbuf = "RM20020;";
            write(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "RM5;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "RM5100000;";
            write(fd, pbuf, strlen(pbuf));
        }

        else if (strcmp(buf, "AN0;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "AN030;";
            write(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "IF;") == 0)
        {
            char ifbuf[256];
            hl_usleep(mysleep * 1000);
//            pbuf = "IF000503130001000+0000000000030000000;"
//            sprintf(ifbuf, "IF%011d1000+0000002000000000000;", freqa);
            sprintf(ifbuf, IFformat, freqa,
		    (ptt + ptt_mic + ptt_data + ptt_tune) > 0 ? 1 : 0, modeA);
            //pbuf = "IF00010138698     +00000000002000000 ;
            write(fd, ifbuf, strlen(ifbuf));
        }
        else if (strcmp(buf, "NB1;") == 0)
        {
            hl_usleep(mysleep * 20);
            sprintf(buf,"NB1%d;", nb1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "NB1",3) == 0)
        {
            puts(buf);
            sscanf(buf, "NB1%d", &nb1);
        }
        else if (strcmp(buf, "NB2;") == 0)
        {
            hl_usleep(mysleep * 20);
            sprintf(buf,"NB2%d;", nb2);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "NB2",3) == 0)
        {
            puts(buf);
            sscanf(buf, "NB2%d", &nb2);
        }
        else if (strcmp(buf, "RA;") == 0)
        {
            hl_usleep(mysleep * 200);
            sprintf(buf,"RA%d;", ra);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "RA", 2) == 0)
        {
            sscanf(buf, "RA%d", &ra);
        }
        else if (strcmp(buf, "RG;") == 0)
        {
            hl_usleep(mysleep * 000);
            pbuf = "RG255;";
            write(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "MG;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "MG050;";
            write(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "AG;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "AG100;";
            write(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "FV;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "FV1.04;";
            write(fd, pbuf, strlen(pbuf));
        }
        else if (strncmp(buf, "IS;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "IS+0000;");
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "IS", 2) == 0)
        {
            sscanf(buf,"S%d", &is);
        }
        else if (strncmp(buf, "SM;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "SM0035;");
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "PC;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PC%03d;", pc);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "PC", 2) == 0)
        {
            sscanf(buf,"PC%d", &pc);
        }
        else if (strcmp(buf, "FW1;") == 0)
        {
            //usleep(mysleep * 1000);
            pbuf = "FW10;";
            write(fd, pbuf, strlen(pbuf));
            hl_usleep(20 * 1000);
        }
        else if (strncmp(buf, "FW", 2) == 0)
        {
            continue;
        }
        else if (strcmp(buf, "ID;") == 0)
        {
            hl_usleep(mysleep * 1000);
            int id = 24;
            SNPRINTF(buf, sizeof(buf), "ID%03d;", id);
            write(fd, buf, strlen(buf));
        }

#if 0
        else if (strncmp(buf, "AI", 2) == 0)
        {
            if (strcmp(buf, "AI;"))
            {
                hl_usleep(mysleep * 1000);
                n = fprintf(fp, "%s", "AI0;");
            }
        }

#endif
        else if (strcmp(buf, "EX00011;") == 0)
        {
            pbuf = "EX00011 001;";
            write(fd, pbuf, strlen(pbuf));
        }
        else if (strncmp(buf, "EX", 2) == 0)
        {
            continue;
        }
        else if (strcmp(buf, "FA;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FA%011d;", freqa);
            write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "FB;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FB%011d;", freqb);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FA", 2) == 0)
        {
            sscanf(buf, "FA%d", &freqa);
        }
        else if (strncmp(buf, "FB", 2) == 0)
        {
            sscanf(buf, "FB%d", &freqb);
        }
        else if (strncmp(buf, "AI;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "AI0;");
            write(fd, buf, strlen(buf));
        }

        else if (strncmp(buf, "PS;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PS1;");
            write(fd, buf, strlen(buf));
        }
        else if (buf[3] == ';' && strncmp(buf, "SF", 2) == 0)
        {
            int tmpvfo = buf[2] - '0';
            SNPRINTF(buf, sizeof(buf), SFformat, tmpvfo,
                     tmpvfo == 0 ? freqa : freqb,
                     tmpvfo == 0 ? modeA : modeB);
            printf("SF buf=%s\n", buf);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SF", 2) == 0)
        {
            int tmpvfo, tmpfreq;
            mode_t tmpmode;

            if (sscanf(buf, SFformat, &tmpvfo, &tmpfreq, &tmpmode) != 3)
            {
                printf("Error decoding SF:%s\n", buf);
                cmd_err = 1;
                continue;
            }

            //printf("tmpvfo=%d, tmpfreq=%d, tmpmode=%d\n", tmpvfo, tmpfreq, tmpmode);
            if (tmpvfo == 0)
            {
                modeA = tmpmode;
                freqa = tmpfreq;
            }
	    else
            {
                modeB = tmpmode;
                freqb = tmpfreq;
            }

            printf("modeA=%X, modeB=%X\n", modeA, modeB);
        }
        else if (strncmp(buf, "MD;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "MD%d;",
                     modeA); // not worried about modeB yet for simulator
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "MD", 2) == 0)
        {
            sscanf(buf, "MD%d", &modeA); // not worried about modeB yet for simulator
        }
        else if (strncmp(buf, "FL;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FL%03d%03d;", filternum1, filternum2);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FL", 2) == 0)
        {
            sscanf(buf, "FL%3d%3d", &filternum1, &filternum2);
        }
        else if (strcmp(buf, "FR;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FR%d;", vfo);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FR", 2) == 0)
        {
            sscanf(buf, "FR%d", &vfo);
        }
        else if (strcmp(buf, "FT;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FR%d;", vfo_tx);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FT", 2) == 0)
        {
            sscanf(buf, "FT%d", &vfo_tx);
        }
        else if (strncmp(buf, "DA;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "DA%d;", datamode);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "DA", 2) == 0)
        {
            sscanf(buf, "DA%d", &datamode);
        }
        else if (strncmp(buf, "BD;", 3) == 0)
        {
            continue;
        }
        else if (strncmp(buf, "BU;", 3) == 0)
        {
            continue;
        }
        else if (strcmp(buf, "RX;") == 0)
        {
            ptt = ptt_mic = ptt_data = ptt_tune = 0;
        }
        else if (strncmp(buf, "TX", 2) == 0)
        {
            ptt = ptt_mic = ptt_data = ptt_tune = 0;

            switch (buf[2])
            {
            case ';':
            case '0': ptt = ptt_mic = 1;
	      break;
            case '1': ptt_data = 1;
	      break;
            case '2': ptt_tune = 1;
	      break;
            }

        }
        else if (strncmp(buf, "CB;", 3) == 0)
        {
            printf("No CB command!\n");
            sprintf(buf, "CB%d;", operatingband);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "CB", 2) == 0)
        {
            sscanf(buf, "CB%d", &operatingband);
        }
        else if (strncmp(buf, "TB;", 3) == 0)
        {
            sprintf(buf, "TB%d;", split);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "TB", 2) == 0)
        {
            sscanf(buf, "TB%d", &split);
        }
        else if (strncmp(buf, "KS;", 3) == 0)
        {
            sprintf(buf, "KS%03d;", keyspd);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "KS", 2) == 0)
        {
            sscanf(buf, "KS%03d", &keyspd);
        }
        else if (strncmp(buf, "OM0;", 4) == 0)
        {
            sprintf(buf, "OM0%d;", modeMain);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "OM0", 3) == 0)
        {
            sscanf(buf, "OM0%d", &modeMain);
        }
        else if (strncmp(buf, "OM1;", 4) == 0)
        {
            sprintf(buf, "OM1%d;", modeSub);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "OM1", 3) == 0)
        {
            sscanf(buf, "OM1%d", &modeSub);
        }
        else if (strcmp(buf, "RM;") == 0)
        {
            sprintf(buf, "RM2%04d;", 10);
            write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "SL0;") == 0)
        {
            sprintf(buf,"SL0%02d;", sl);
			printf("R: %s\n", buf);
            write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "SH0;") == 0)
        {
            sprintf(buf,"SH0%03d;", sh);
			printf("R: %s\n", buf);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SL0", 3) == 0)
        {
			printf("Cmd: %s\n", buf);
            sscanf(buf,"SL0%3d", &sl);
        }
        else if (strncmp(buf, "SH0", 3) == 0)
        {
			printf("Cmd: %s\n", buf);
            sscanf("SH0%3d","%d", &sh);
        }
        else if (strcmp(buf, "NR;") == 0)
        {
            sprintf(buf,"NR%d;", nr);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "NR", 2) == 0)
        {
            puts(buf);
            sscanf(buf,"NR%d", &nr);
        }
        else if (strcmp(buf, "PA;") == 0)
        {
            sprintf(buf,"PA%d;", pa);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "PA", 2) == 0)
        {
            sscanf(buf,"PA%d", &pa);
        }
        else if (strcmp(buf, "SM;") == 0)
        {
            sprintf(buf,"SM%04d;", sm);
            write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "PC;") == 0)
        {
            sprintf(buf,"PC%03d;", sm);
            write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "NT;") == 0)
        {
            sprintf(buf,"NT%d;", nt);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "NT", 2) == 0)
        {
            sscanf(buf,"NT%d", &nt);
        }
        else if (strcmp(buf, "AG;") == 0)
        {
            sprintf(buf,"AG%03d;", ag);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "AG", 2) == 0)
        {
            sscanf(buf,"AG%d", &ag);
        }
        else if (strcmp(buf, "AC;") == 0)
        {
            sprintf(buf,"AC%03d;", ac);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "AC", 2) == 0)
        {
            sscanf(buf,"AC%d", &ac);
        }
        else if (strcmp(buf, "SQ;") == 0)
        {
            sprintf(buf,"SQ%03d;", sq);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SQ", 2) == 0)
        {
            sscanf(buf,"SQ%d", &sq);
        }
        else if (strcmp(buf, "RG;") == 0)
        {
            sprintf(buf,"RG%03d;", rg);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "RG", 2) == 0)
        {
            sscanf(buf,"RG%d", &rg);
        }
        else if (strcmp(buf, "MG;") == 0)
        {
            sprintf(buf,"MG%03d;", mg);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "MG", 2) == 0)
        {
            sscanf(buf,"MG%d", &mg);
        }
        else if (strncmp(buf, "RL1;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "RL%02d;", rl);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "RL1", 2) == 0)
        {
            puts(buf);
            sscanf(buf,"RL1%d", &rl);
        }
        else if (strcmp(buf, "SP;") == 0)
        {
            sprintf(buf,"SP%d;", sp);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SP", 2) == 0)
        {
            sscanf(buf,"SP%d", &sp);
        }
        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }

    }

    return 0;
}
