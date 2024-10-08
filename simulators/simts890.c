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
#include <ctype.h>
#include <time.h>
#include <hamlib/rig.h>

#define BUFSIZE 256

/* Define a macro for sending response back to the app
 * This will allow us to reroute output to a buffering routine
 * Needed to handle multiple commands in a single message
 */
#define OUTPUT(s) write(fd, s, strlen(s))

int mysleep = 20;

int filternum1 = 7;
int filternum2 = 8;
int datamode = 0;
int vfo, vfo_tx, ptt, ptt_data, ptt_mic, ptt_tune;
int operatingband;
int split = 0;
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
// Clock data
int autoset = 1;
int tzs[2] = {36, 56}; // 0=primary(EST), 1=auxiliary(UTC)
char auxtzc = 'U';       // Auxiliary clock identifier (UTC)
// Antenna connections
char antnum = '1', recant = '0', driveout = '0', antout = '0';
// Multiple meter functions
struct meter_data {
  int enabled;
  int value;  // # of pips lit, range 0-70
    };
struct meter_data meter[6] = {
  { 0,  5},  // ALC
  { 0,  1},  // SWR
  { 0, 10},  // COMP
  { 0, 30},  // ID (amps)
  { 0, 60},  // Vd (Volts)
  { 0, 20}   // Temp (Unknown units)
};
int tfset = 0;
  

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
      "%1X"         // P9 Operating mode (See OM command)
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
            OUTPUT(err_txt[cmd_err - 1]);
            cmd_err = 0;
        }

        if (getmyline(fd, buf) > 0) 
        {
//             printf("Cmd:\"%s\"\n", buf);
        }


//        else { return 0; }

        if (strcmp(buf, "IF;") == 0)
        {
            char ifbuf[256];
            hl_usleep(mysleep * 1000);
            sprintf(ifbuf, IFformat, freqa,
		    (ptt + ptt_mic + ptt_data + ptt_tune) > 0 ? 1 : 0, modeA);
            OUTPUT(ifbuf);
        }
#if 0
        else if (strncmp(buf, "RM2", 3) == 0)
        {
            pbuf = "RM20020;";
            OUTPUT(pbuf);
        }
        else if (strcmp(buf, "RM5;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "RM5100000;";
            OUTPUT(pbuf);
        }
#endif
        else if (strncmp(buf, "AN", 2) == 0)
        {  // Antenna connection handling
            hl_usleep(mysleep * 1000);
	    if (buf[2] == ';')
            {
		buf[2] = antnum;
		buf[3] = recant;
		buf[4] = driveout;
		buf[5] = antout;
		buf[6] = ';';
		buf[7] = '\0';
		OUTPUT(buf);
            }
            else
            {
		if (buf[2] != '9') antnum = buf[2];
		if (buf[3] != '9') recant = buf[3];
		if (buf[4] != '9') driveout = buf[4];
		if (buf[5] != '9') antout = buf[5];
            }
        }
        else if (strcmp(buf, "NB1;") == 0)
        {
            hl_usleep(mysleep * 20);
            sprintf(buf, "NB1%d;", nb1);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "NB1", 3) == 0)
        {
            puts(buf);
            sscanf(buf, "NB1%d", &nb1);
        }
        else if (strcmp(buf, "NB2;") == 0)
        {
            hl_usleep(mysleep * 20);
            sprintf(buf, "NB2%d;", nb2);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "NB2", 3) == 0)
        {
            puts(buf);
            sscanf(buf, "NB2%d", &nb2);
        }
        else if (strcmp(buf, "RA;") == 0)
        {
            hl_usleep(mysleep * 200);
            sprintf(buf, "RA%d;", ra);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "RA", 2) == 0)
        {
            sscanf(buf, "RA%d", &ra);
        }
        else if (strcmp(buf, "RG;") == 0)
        {
            hl_usleep(mysleep * 000);
            pbuf = "RG255;";
            OUTPUT(pbuf);
        }
        else if (strcmp(buf, "MG;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "MG050;";
            OUTPUT(pbuf);
        }
        else if (strcmp(buf, "AG;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "AG100;";
            OUTPUT(pbuf);
        }
        else if (strcmp(buf, "FV;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "FV1.05;";
            OUTPUT(pbuf);
        }
        else if (strncmp(buf, "IS;", 3) == 0)
        {
            snprintf(buf, sizeof(buf), "IS%+04d;", is);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "IS", 2) == 0)
        {
            sscanf(buf,"IS%d", &is);
        }
        else if (strncmp(buf, "SM;", 3) == 0)
        {
            pbuf = "SM0035;";
            OUTPUT(pbuf);
        }
        else if (strncmp(buf, "PC;", 3) == 0)
        {
            snprintf(buf, sizeof(buf), "PC%03d;", pc);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "PC", 2) == 0)
        {
            sscanf(buf,"PC%d", &pc);
        }
#if 0
        else if (strcmp(buf, "FW1;") == 0)
        {
            //usleep(mysleep * 1000);
            pbuf = "FW10;";
            OUTPUT(pbuf);
            hl_usleep(20 * 1000);
        }
        else if (strncmp(buf, "FW", 2) == 0)
        {
            continue;
        }
#endif
        else if (strcmp(buf, "ID;") == 0)
        {
            hl_usleep(mysleep * 1000);
            int id = 24;
            snprintf(buf, sizeof(buf), "ID%03d;", id);
            OUTPUT(buf);
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
            OUTPUT(pbuf);
        }
        else if (strncmp(buf, "EX", 2) == 0)
        {
            continue;
        }
        else if (strcmp(buf, "FA;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FA%011d;", freqa);
            OUTPUT(buf);
        }
        else if (strcmp(buf, "FB;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FB%011d;", freqb);
            OUTPUT(buf);
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
            OUTPUT(buf);
        }

        else if (strncmp(buf, "PS;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PS1;");
            OUTPUT(buf);
        }
        else if (buf[3] == ';' && strncmp(buf, "SF", 2) == 0)
        {
            int tmpvfo = buf[2] - '0';
            SNPRINTF(buf, sizeof(buf), SFformat, tmpvfo,
                     tmpvfo == 0 ? freqa : freqb,
                     tmpvfo == 0 ? modeA : modeB);
            //printf("SF buf=%s\n", buf);
            OUTPUT(buf);
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
#if 0
        else if (strncmp(buf, "MD;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "MD%d;",
                     modeA); // not worried about modeB yet for simulator
            OUTPUT(buf);
        }
        else if (strncmp(buf, "MD", 2) == 0)
        {
            sscanf(buf, "MD%d", &modeA); // not worried about modeB yet for simulator
        }
#endif
        else if (strncmp(buf, "FL", 2) == 0)
        {
            switch (buf[2]) {
            case '0': // Select the Receive Filter
            case '1': // Roofing Filter
            case '2': // IF Filter Shape
            case '3': // AF Filter Type
                continue; // For now
            default:
                cmd_err = 1;
            }
        }
        else if (strcmp(buf, "FR;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FR%d;", vfo);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "FR", 2) == 0)
        {
            sscanf(buf, "FR%d", &vfo);
        }
        else if (strcmp(buf, "FT;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FT%d;", vfo_tx);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "FT", 2) == 0)
        {
            sscanf(buf, "FT%d", &vfo_tx);
            if (vfo_tx != vfo)
            {
                split = 1;
            }
        }
#if 0
        else if (strncmp(buf, "DA;", 3) == 0)
        {
            snprintf(buf, sizeof(buf), "DA%d;", datamode);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "DA", 2) == 0)
        {
            sscanf(buf, "DA%d", &datamode);
        }
#endif
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
#if 0
        else if (strncmp(buf, "CB;", 3) == 0)
        {
            printf("No CB command!\n");
            sprintf(buf, "CB%d;", operatingband);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "CB", 2) == 0)
        {
            sscanf(buf, "CB%d", &operatingband);
        }
#endif
        else if (strncmp(buf, "TB;", 3) == 0)
        {
            sprintf(buf, "TB%d;", split);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "TB", 2) == 0)
        {
            sscanf(buf, "TB%d", &split);
        }
        else if (strncmp(buf, "KS;", 3) == 0)
        {
            sprintf(buf, "KS%03d;", keyspd);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "KS", 2) == 0)
        {
            sscanf(buf, "KS%03d", &keyspd);
        }
        else if (strncmp(buf, "OM", 2) == 0)
        {
          /* The TS-890S displays two frequencies and modes - left and right,
           * along with arrows that show which is VFO A and which is VFO B.
           * In almost all cases, the left VFO is the receive freq.  The right
           * VFO is only used in split operation, as the transmit frequency.
           */
          if (buf[3] == ';')
          {
	    int tmpvfo = buf[2] - '0';
	    if (tmpvfo < 0 || tmpvfo > 1)
            {
                cmd_err = 1;
            }
	    else
            {
                sprintf(buf, "OM%d%X;", tmpvfo, tmpvfo == 0 ? modeMain : modeSub);
                OUTPUT(buf);
            }
            continue;
          }
	  /* Setting - Only sets the active function (RX/TX)
	   * For now, just set the mode of the current vfo. Not
	   * always correct, but the best we can do until I turn
	   * VFOs into objects.
           */
	  sscanf(&buf[3], "%1X", &modeMain);
        }
        else if (strncmp(buf, "RM", 2) == 0)
        {  // Meter control/readout
	  if (buf[2] == ';')
          { // Read all enabled meters
	      char tbuf[8];
	      buf[0] = '\0';
	      pbuf = buf;
	      for (int i = 0; i < 6; i++)
		{
		  if (meter[i].enabled)
		    {
		      snprintf(tbuf, sizeof tbuf, "RM%d%03d;", i + 1, meter[i].value);
		      pbuf = stpcpy(pbuf, tbuf);
		    }
		}
	      if (buf[0] != '\0')
		{
		  OUTPUT(buf);
		}
          }
          else
          { // Enable/disable one meter
	      int target = buf[2] - '1';
	      int status = buf[3] - '0';
	      if (target < 0 || target > 5 || status < 0 || status > 1)
              {
		  cmd_err = 2;
		  continue;
              }
	      meter[target].enabled = status;
          }
        }
        else if (strcmp(buf, "SL0;") == 0)
        {
            sprintf(buf,"SL0%02d;", sl);
			printf("R: %s\n", buf);
            OUTPUT(buf);
        }
        else if (strcmp(buf, "SH0;") == 0)
        {
            sprintf(buf,"SH0%03d;", sh);
			printf("R: %s\n", buf);
            OUTPUT(buf);
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
            OUTPUT(buf);
        }
        else if (strncmp(buf, "NR", 2) == 0)
        {
            puts(buf);
            sscanf(buf,"NR%d", &nr);
        }
        else if (strcmp(buf, "PA;") == 0)
        {
            sprintf(buf,"PA%d;", pa);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "PA", 2) == 0)
        {
            sscanf(buf,"PA%d", &pa);
        }
        else if (strcmp(buf, "SM;") == 0)
        {
            sprintf(buf,"SM%04d;", sm);
            OUTPUT(buf);
        }
        else if (strcmp(buf, "PC;") == 0)
        {
            sprintf(buf,"PC%03d;", sm);
            OUTPUT(buf);
        }
        else if (strcmp(buf, "NT;") == 0)
        {
            sprintf(buf,"NT%d;", nt);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "NT", 2) == 0)
        {
            sscanf(buf,"NT%d", &nt);
        }
        else if (strcmp(buf, "AG;") == 0)
        {
            sprintf(buf,"AG%03d;", ag);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "AG", 2) == 0)
        {
            sscanf(buf,"AG%d", &ag);
        }
        else if (strcmp(buf, "AC;") == 0)
        {
            sprintf(buf,"AC%03d;", ac);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "AC", 2) == 0)
        {
            sscanf(buf,"AC%d", &ac);
        }
        else if (strcmp(buf, "SQ;") == 0)
        {
            sprintf(buf,"SQ%03d;", sq);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "SQ", 2) == 0)
        {
            sscanf(buf,"SQ%d", &sq);
        }
        else if (strcmp(buf, "RG;") == 0)
        {
            sprintf(buf,"RG%03d;", rg);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "RG", 2) == 0)
        {
            sscanf(buf,"RG%d", &rg);
        }
        else if (strcmp(buf, "MG;") == 0)
        {
            sprintf(buf,"MG%03d;", mg);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "MG", 2) == 0)
        {
            sscanf(buf,"MG%d", &mg);
        }
        else if (strncmp(buf, "RL1;", 3) == 0)
        {
            snprintf(buf, sizeof(buf), "RL%02d;", rl);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "RL1", 2) == 0)
        {
            puts(buf);
            sscanf(buf,"RL1%d", &rl);
        }
        else if (strcmp(buf, "SP;") == 0)
        {
            sprintf(buf,"SP%d;", sp);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "SP", 2) == 0)
        {
            sscanf(buf,"SP%d", &sp);
        }
        else if (strncmp(buf, "BS", 2) == 0)
        {  // All the Bandscope commands
            switch (toupper(buf[2])) {
            case '0': // Scope Display ON/OFF
            case '1': // Scope Display Type
            case '2': // Bandscpoe Operation Mode
            case '3': // Bandscope Span
            case '4': // Bandscope Span
            case '5': // Bandscope Scope Range (Fixed Mode)
            case '6': // Bandscope Dispaly Pause
            case '7': // Bandscope Marker
            case '8': // Bandscope Attenuator
            case '9': // Bandscope Max Hold
            case 'A': // Bandscope Averaging
            case 'B': // Bandscope Waterfall Display Speed
            case 'C': // Bandscope Reference Level
            case 'D': // Bandscope Waterfall Display Clear
            case 'E': // Bandscope Marker Shift / Marker Center
            case 'G': // Audio Scope Attenuator
            case 'H': // Audio Scope Span
            case 'I': // Oscilloscope Level
            case 'J': // Oscilloscpoe Sweep Time
            case 'K': // Bandscope Shift Position
            case 'L': // Bandscope Receive Circuit State
            case 'M': // Bandscope Scope Range Lower/Upper Frequency Limit
            case 'N': // Audio Scope Display Pause
            case 'O': // Expands Spectrum Analysis Range
              break;
            default: // Unknown
              cmd_err = 1;
            }
        }
        else if (strncmp(buf, "CK", 2) == 0)
        {  // All the clock functions
            switch (buf[2]) {
            case '0': // Get/Set Local clock
              {
                time_t t;
                struct tm *localtm;

                if (buf[3] == ';')
                  {
                    t = time(NULL);
		    localtm = localtime(&t);
                    strftime(&buf[3], BUFSIZ - 3, "%y%m%d%H%M%S;", localtm);
                    OUTPUT(buf);
                  }
                else
                  {
                    printf("Clock not set. cmd = %s\n", buf);
                  }
                break;
              }
            case '1': // Setting status
              buf[3] = '1';
              buf[4] = ';';
	      buf[5] = '\0';
              OUTPUT(buf);
              break;
            case '2': // Local clock time zone
            case '3': // Auxiliary clock time zone
              {
                int idx = buf[2] - '2';
                if (buf[3] == ';')
                  {
                    sprintf(&buf[3], "%03d;", tzs[idx]);
                    OUTPUT(buf);
                  }
                else
                  {
                    sscanf(&buf[3], "%3d;", &tzs[idx]);
                  }
                break;
              }
	    case '4': // ID character for auxiliary clock
              if (buf[3] == ';')
              {
                  buf[3] = auxtzc;
                  buf[4] = ';';
		  buf[5] = '\0';
                  OUTPUT(buf);
                }
              else
                {
                  auxtzc = buf[3];
                }
              break;
            case '5': // Date display format
              break;
            case '6': // Automatic date/time retrieval (NTP)
              //TODO: Fix this when we can set the clock
              if (buf[3] == ';')
                {
                  buf[3] = autoset + '0';
                  buf[4] = ';';
		  buf[5] = '\0';
                  OUTPUT(buf);
		}
	      else
		{
		  autoset = buf[3] - '0';
		}
	      break;
	    case '7': // NTP server address
	    case '8': // Force time update via NTP
	    case '9': // Clock display (primary/secondary/both)
	    default:
	      printf("Bad clock command - %s\n", buf);
	    }
	  }
        else if (strncmp(buf, "CM", 2) == 0)
        { // CW Message Memory
          switch (buf[2]) {
          case '0': // Registration of CW Message (Paddle Input)
          case '1': // Play/Stop the CW Message
          case '2': // Register State of CW Message (Paddle Input)
          case '3': // Clear the CW Message (Paddle Inut)
          case '4': // CW Message Memory Name (Paddle Input)
          case '5': // Registering the CW Message Memory (Text Input)
          case '6': // CW Message Channel Repeat
          case '7': // Contest Number
            break;
          default:
            cmd_err = 1;  // Unknown command
          }
	}
        else if (strncmp(buf, "MA", 2) == 0)
        { // Memory Channel Functions
          switch (buf[2]) {
          case '0': // Memory Channel Configuration
          case '1': // Memort Channel (Direct Write)
          case '2': // Memory Channel (Channel Name)
          case '3': // Memory Channel (Scan Lockout)
          case '4': // Memory Channel (Channel Copy)
          case '5': // Memory Channle (Channel Deletion)
          case '6': // Programmable VFO End Frequency
          case '7': // Memory Channel (Temporary Change Frequency)
            break;
          default:
            cmd_err = 1;
          }
        }
        else if (strncmp(buf, "SC", 2) == 0)
        { // Scan functions
	  switch (buf[2]) {
	  case '0': // Scan
	  case '1': // Scan Speed
	  case '2': // Tone Scan/CTCSS Scan
	  case '3': // Program Scan/VFO Scan Selection
	    break;
	  default:
	    cmd_err = 1;
          }
        }
        else if (strncmp(buf, "TS", 2) == 0)
        { // TF-SET
	    if (buf[2] == ';')
            {
                snprintf(buf, sizeof buf, "TS%d;", tfset);
                OUTPUT(buf);
                continue;
            }
	    int tmpset = buf[2] - '0';
	    if (tmpset < 0 || tmpset > 1)
            {
                cmd_err = 1;
                continue;
            }
            tfset = tmpset;
        }
        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }

    }

    return 0;
}
