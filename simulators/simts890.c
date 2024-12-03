//#define TRACE /* Full traffic trace if enabled */
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
//#include <hamlib/rig.h>

/* Definitions */
/* The TS-890S has some undocumented commands, left over from older
 *   Kenwood models. They have newer counterparts with more functionality,
 *   but are still around for legacy software. If you want to see if your
 *   app is only using the latest-and-greatest, comment out the next define.
 */
#define LEGACY
// Size of command buffer
#define BUFSIZE 256
// Number of selectable bands
#define NBANDS 11
/* Type we're emulating - K=The Americas(default), E=Europe */
#if !defined(TYPE)
#define TYPE K
#endif
/* Define a macro for sending response back to the app
 * This will allow us to reroute output to a buffering routine
 * Needed to handle multiple commands in a single message
 * Also makes it easy to trace
 */
#if defined(TRACE)
#define OUTPUT(s) {printf("Resp:\"%s\"\n", s); write(fd, s, strlen(s)); }
#else
#define OUTPUT(s) write(fd, s, strlen(s))
#endif

int mysleep = 20;

int filternum1 = 7;
int filternum2 = 8;
int ptt, ptt_data, ptt_mic, ptt_tune;
int keyspd = 20;
int sl = 3, sh = 3;
int nr = 0;
int pa = 0;
int pc = 25;
int sm = 35;
int nt = 0;
int ag = 100;
int ac = 0;
int nb[2] = {0, 0}; // NB1/NB2 OFF/ON
int sq = 0;
int rg = 0;
int mg = 0;
int ra = 0;
int rl = 0;
int is = 0;
int sp = 0;        // Split OFF/ON
int split_op = 0;  // Split frequency setting operation in progress
int rit = 0, xit = 0, rxit =
                       0; // RIT off/on, XIT off/on, Offset freq(-9999<=rxit<=+9999)
int fine = 0;      // Fine tuning - step size off=10hz, on=1hz
// Clock data
int autoset = 1;
int tzs[2] = {36, 56}; // 0=primary(EST), 1=auxiliary(UTC)
char auxtzc = 'U';       // Auxiliary clock identifier (UTC)
// Antenna connections
char antnum = '1', recant = '0', driveout = '0', antout = '0';
// Multiple meter functions
struct meter_data
{
    int enabled;
    int value;  // # of pips lit, range 0-70
};
struct meter_data meter[6] =
{
    { 0,  5},  // ALC
    { 0,  1},  // SWR
    { 0, 10},  // COMP
    { 0, 30},  // ID (amps)
    { 0, 60},  // Vd (Volts)
    { 0, 20}   // Temp (Unknown units)
};
int tfset = 0;

typedef struct kvfo
{
    int freq;
    int mode;
    short band, vfo;  // Redundant, but useful for relative movement
} *kvfop_t;

int nummems = 3;  // Default - values = 1, 3, 5
int bandslot[2][NBANDS];  // 0-based band memory: ((bandslot[i] + 1) % nummems) (+1 for display)

/* Storage and default data for band memories
 * vfoA freq and mode initialized here, vfoB and other items set at startup
 * 1, 3(default), or 5 memories per band can be used. One is always active on
 * each band. Manually they are selected by multiple band button pushes; CAT
 * selection is by BD/BU command
 */
struct kvfo band_mem[2][NBANDS][5] = { {
#if TYPE==K
        /* 160M */ { { 1800000, 3}, { 1810000, 3}, { 1820000, 3}, { 1830000, 3}, { 1840000, 3} },
        /*  80M */ { { 3500000, 1}, { 3600000, 1}, { 3700000, 1}, { 3800000, 1}, { 3900000, 1} },
        /*  40M */ { { 7000000, 1}, { 7050000, 1}, { 7100000, 1}, { 7150000, 1}, { 7200000, 1} },
        /*  30M */ { {10100000, 3}, {10110000, 3}, {10120000, 3}, {10130000, 3}, {10140000, 3} },
        /*  20M */ { {14000000, 2}, {14100000, 2}, {14150000, 2}, {14200000, 2}, {14250000, 2} },
        /*  17M */ { {18068000, 2}, {18100000, 2}, {18110000, 2}, {18150000, 2}, {18160000, 2} },
        /*  15M */ { {21000000, 2}, {21100000, 2}, {21150000, 2}, {21200000, 2}, {21300000, 2} },
        /*  12M */ { {24890000, 2}, {24920000, 2}, {24940000, 2}, {24960000, 2}, {24980000, 2} },
        /*  10M */ { {28000000, 2}, {28300000, 2}, {28500000, 2}, {29000000, 4}, {29300000, 4} },
        /*   6M */ { {50000000, 2}, {50125000, 2}, {50200000, 2}, {51000000, 4}, {52000000, 4} },
        /* GENE */ { {  135700, 3}, {  472000, 3}, { 1000000, 5}, { 5305500, 2}, { 5403500, 2} }
#else  // TYPE==E
        /* 160M */ { { 1830000, 3}, { 1840000, 3}, { 1850000, 3}, { 1810000, 3}, { 1820000, 3} },
        /*  80M */ { { 3500000, 1}, { 3550000, 1}, { 3600000, 1}, { 3650000, 1}, { 3700000, 1} },
        /*  40M */ { { 7000000, 1}, { 7050000, 1}, { 7100000, 1}, { 7150000, 1}, { 7200000, 1} },
        /*  30M */ { {10100000, 3}, {10110000, 3}, {10120000, 3}, {10130000, 3}, {10140000, 3} },
        /*  20M */ { {14000000, 2}, {14100000, 2}, {14150000, 2}, {14200000, 2}, {14250000, 2} },
        /*  17M */ { {18068000, 2}, {18100000, 2}, {18110000, 2}, {18150000, 2}, {18160000, 2} },
        /*  15M */ { {21000000, 2}, {21100000, 2}, {21150000, 2}, {21200000, 2}, {21300000, 2} },
        /*  12M */ { {24890000, 2}, {24920000, 2}, {24940000, 2}, {24960000, 2}, {24980000, 2} },
        /*  10M */ { {28000000, 2}, {28300000, 2}, {28500000, 2}, {29000000, 4}, {29300000, 4} },
        /*   6M */ { {50000000, 2}, {50125000, 2}, {50200000, 2}, {51000000, 4}, {52000000, 4} },
        /* GENE */ { {70100000, 2}, {  135700, 3}, {  472000, 5}, {  999000, 5}, { 5258500, 2} }
#endif
    }
};

/* Band definitions
 *  differ by model
 */
struct band_def
{
    int low;
    int high;
};
const struct band_def band_limits[NBANDS] =
{
#if TYPE == K
    { 1800000,  2000000}, { 3500000,  4000000}, { 7000000,  7300000},
    {10100000, 10150000}, {14000000, 14350000}, {18068000, 18168000},
    {21000000, 21450000}, {24890000, 24990000}, {28000000, 29700000},
    {50000000, 54000000}, {   30000, 60000000}
#else
    { 1810000,  2000000}, { 3500000,  3800000}, { 7000000,  7200000},
    {10100000, 10150000}, {14000000, 14350000}, {18068000, 18168000},
    {21000000, 21450000}, {24890000, 24990000}, {28000000, 29700000},
    {50000000, 52000000}, {   30000, 74800000}
#endif
};

/* Table for mode<->emission class conversion
 * Claas 0 = SSB
 *       1 = CW/FSK/PSK
 *       2 = FM
 *       3 = AM
 */
const int mode2classtab[16] = { -1, 0, 0, 1, 2, 3, 1, 1, -1, 1, 1, 1, 0, 0, 2, 3};
const int stepvalues[4][10] =   // Step sizes in Hz
{
    /* SSB        */ { 500, 1000, 2500, 5000, 10000, 0, 0, 0, 0, 0},
    /* CW/FSK/PSK */ { 500, 1000, 2500, 5000, 10000, 0, 0, 0, 0, 0},
    /* FM */ { 5000, 6250, 10000, 12500, 15000, 20000, 25000, 30000, 50000, 100000},
    /* AM */ { 5000, 6250, 10000, 12500, 15000, 20000, 25000, 30000, 50000, 100000}
};
int stepsize[4] = { 1000, 500, 10000, 5000}; // Defaults by modeclass

/* Function prototypes */
int freq2band(int freq);
kvfop_t newvfo(kvfop_t ovfo, int band);
void swapvfos(kvfop_t *vfoset[]);
// Extracted from rig.h
int hl_usleep(unsigned long usec);  // Until it's replaced

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
    int cmd_err = 0;
    char *err_txt[] = { "?;", "E;", "O;" };

    struct kvfo *vfoA = &band_mem[0][4][0], *vfoB = &band_mem[1][6][0];
    kvfop_t *const vfoAB[2] = {&vfoA, &vfoB};   // 0=A, 1=B, fixed
    kvfop_t *vfoLR[2] = {&vfoA, &vfoB};  // 0=Left, 1=Right, can change

#if defined(LEGACY)
    /* The IF command is not documented for the TS-890S, and is supposed
     *  to be supplanted by SF. However, it is still there for legacy S/W.
     *  This description is taken from the TS-590S/SG manual, with values
     *  reflecting a real TS-890S.
     */
    const char IFformat[] = "IF" // Output only
                            "%011d"       // P1 freq(Hz)
                            "     "       // P2 ??
                            "% 05d"       // P3 RIT/XIT freq(Hz)
                            "%1d"         // P4 RIT on/off
                            "%1d"         // P5 XIT on/off
                            "000"         // P6,P7 mem channel
                            "%1d"         // P8 RX/TX
                            "%1X"         // P9 Operating mode (See OM command)
                            "0"           // P10 Function?
                            "0"           // P11 Scan status?
                            "%1d"         // P12 Simplex=0/Split=1
                            "0"           // P13 Tone/CTCSS (not on TS-890S)
                            "00"          // P14 Tone/CTCSS freq (not on TS-890S)
                            "0;";         // P15 Always zero
#endif
    const char SFformat[] = "SF" // Input/Output
                            "%1d"         // P1 VFOA/VFOB
                            "%011d"       // P2 Freq(Hz)
                            "%1X;";       // P3 Mode

    /* Initialization */
    for (int i = 0; i < NBANDS; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            band_mem[1][i][j] = band_mem[0][i][j];
            band_mem[1][i][j].vfo = 1;
            band_mem[0][i][j].band = band_mem[1][i][j].band = i;
        }
    }

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
#if defined(TRACE)
            printf("Cmd:\"%s\"\n", buf);
#endif
        }

//        else { return 0; }

        buf[0] = toupper(buf[0]);
        buf[1] = toupper(buf[1]);

        if (strcmp(buf, "IF;") == 0)
        {
            // Reads the transceiver status
#if defined(LEGACY)
            char ifbuf[256];
            hl_usleep(mysleep * 1000);
            sprintf(ifbuf, IFformat, (*vfoLR[0])->freq, rxit, rit, xit,
                    (ptt + ptt_mic + ptt_data + ptt_tune) > 0 ? 1 : 0,
                    (*vfoLR[0])->mode, sp);
            OUTPUT(ifbuf);
#else
            cmd_err = 1;
#endif
        }
        else if (strncmp(buf, "AN", 2) == 0)
        {
            // Antenna connection handling
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
                if (buf[2] != '9') { antnum = buf[2]; }

                if (buf[3] != '9') { recant = buf[3]; }

                if (buf[4] != '9') { driveout = buf[4]; }

                if (buf[5] != '9') { antout = buf[5]; }
            }
        }
        else if (strncmp(buf, "NB", 2) == 0)
        {
            // Noise Blanker settings
            int idx;

            switch (toupper(buf[2]))
            {
            case '1': // Noise Blanker 1
            case '2': // Noise Blanker 2
                idx = buf[2] - '1';

                if (buf[3] == ';')
                {
                    // Read
                    hl_usleep(mysleep * 20);
                    sprintf(buf, "NB%d%d;", idx + 1, nb[idx]);
                    OUTPUT(buf);
                }
                else
                {
                    // Set
                    nb[idx] = buf[3] - '0';
                }

                break;

            case 'D': // Noise Blanker 2, type B Depth
            case 'T': // Noise Blanker 2 Type
            case 'W': // Noise Blanker 2, type B Width
                break;

            default:
                cmd_err = 1;
            }
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
            sscanf(buf, "IS%d", &is);
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
            sscanf(buf, "PC%d", &pc);
        }
        else if (strcmp(buf, "ID;") == 0)
        {
            hl_usleep(mysleep * 1000);
            int id = 24;
            snprintf(buf, sizeof(buf), "ID%03d;", id);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "EX", 2) == 0)
        {
            // Menu Setting
            if (strcmp(buf + 2, "00011;") == 0)
            {
                // S-Meter Scale
                pbuf = "EX00011 001;";
                OUTPUT(pbuf);
            }
            else if (strncmp(buf + 2, "00311", 5) == 0)
            {
                // Number of Band Memories
                if (buf[7] == ';')
                {
                    snprintf(buf, sizeof buf, "EX00311 %03d;", nummems / 2); // Rounds down
                    OUTPUT(buf);
                }
                else
                {
                    int temp = -1;
                    sscanf(buf + 8, "%3d", &temp);

                    if (temp <= 2 && temp >= 0)
                    { nummems = temp * 2 + 1; }
                    else
                    { cmd_err = 1; }
                }
            }
            else if (strncmp(buf + 2, "00301", 5) >= 0 && strncmp(buf + 2, "00304", 5) <= 0)
            {
                // [SSB|CW/FSK/PSK|FM|AM] Mode Frequency Step Size (Multi/Channel Control)
                int class = buf[6] - '1';
                int i, tmpstep = -1;

                if (buf[7] == ';')
                {
                    // Read
                    for (i = 0; i < 10 && stepvalues[class][i] != 0; i++)
                    {
                        if (stepsize[class] == stepvalues[class][i])
                        {
                            tmpstep = i;
                            break;
                        }
                    }

                    if (tmpstep < 0) {cmd_err = 3; continue;}  // Shouldn't happen

                    snprintf(buf + 7, sizeof(buf) - 7, " %03d;", tmpstep);
                    OUTPUT(buf);
                }
                else
                {
                    // Set
                    tmpstep = atoi(buf + 8);

                    if (tmpstep < 0 || tmpstep > 9 || stepvalues[class][tmpstep] == 0)
                    {cmd_err = 1; continue;}

                    stepsize[class] = stepvalues[class][tmpstep];
                }
            }
        }
        else if (buf[0] == 'F' && (buf[1] == 'A' || buf[1] == 'B'))  // FA/FB
        {
            // VFO {A|B} Frequency
            int idx = buf[1] - 'A';

            if (buf[2] == ';')
            {
                snprintf(buf + 2, sizeof(buf) - 2, "%011d;", (*vfoAB[idx])->freq);
                OUTPUT(buf);
            }
            else
            {
                int tmpfreq, newband;
                kvfop_t ovfo, nvfo;
                sscanf(buf + 2, "%d", &tmpfreq);
                newband = freq2band(tmpfreq);

                if (newband < 0) {cmd_err = 1; continue; }

                ovfo = *vfoAB[idx];
                nvfo = newvfo(ovfo, newband);
                nvfo->freq = tmpfreq;
                *vfoAB[idx] = nvfo;
            }
        }
        else if (strncmp(buf, "AI;", 3) == 0)
        {
            pbuf = "AI0;";
            OUTPUT(pbuf);
        }

        else if (strncmp(buf, "PS;", 3) == 0)
        {
            snprintf(buf, sizeof(buf), "PS1;");
            OUTPUT(buf);
        }
        else if (buf[3] == ';' && strncmp(buf, "SF", 2) == 0)
        {
            int tmpvfo = buf[2] - '0';

            if (tmpvfo < 0 || tmpvfo > 1)
            {
                cmd_err = 1;
                continue;
            }

            snprintf(buf, sizeof(buf), SFformat, tmpvfo,
                     (*vfoAB[tmpvfo])->freq, (*vfoAB[tmpvfo])->mode);
            //printf("SF buf=%s\n", buf);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "SF", 2) == 0)
        {
            // Sets and Reads the VFO (Frequency and Mode)
            int tmpvfo, tmpfreq, tmpmode, newband;
            kvfop_t ovfo, nvfo;

            if (sscanf(buf, SFformat, &tmpvfo, &tmpfreq, &tmpmode) != 3 || tmpvfo < 0
                    || tmpvfo > 1)
            {
                printf("Error decoding SF:%s\n", buf);
                cmd_err = 1;
                continue;
            }

            //printf("tmpvfo=%d, tmpfreq=%d, tmpmode=%d\n", tmpvfo, tmpfreq, tmpmode);
            ovfo = *vfoAB[tmpvfo];
            newband = freq2band(tmpfreq);

            if (newband < 0) {cmd_err = 1; continue; }

            nvfo = newvfo(ovfo, newband);
            nvfo->mode = tmpmode;
            nvfo->freq = tmpfreq;
            *vfoAB[tmpvfo] = nvfo;
            printf("modeA=%X, modeB=%X\n", vfoA->mode, vfoB->mode);
        }
        else if (strncmp(buf, "FR", 2) == 0)
        {
            // Receiver Function (VFO A / VFO B / Memory channel)
            int idx;

            if (buf[2] == ';')
            {
                // Read
                idx = sp && tfset;
                snprintf(buf, sizeof(buf), "FR%d;", (*vfoLR[idx])->vfo);
                OUTPUT(buf);
            }
            else
            {
                // Set
                idx = buf[2] - '0';

                if (idx == 3)
                {
                    //TODO: Memory channels are a long way off
                    puts("Memory channels not implemented.\n");
                    cmd_err = 3;
                    continue;
                }

                if (idx < 0 || idx > 1) {cmd_err = 1; continue; }

                sp = 0;  // Turn off split

                if ((*vfoLR[0])->vfo != idx) // If the selected vfo is not the operational one
                {
                    swapvfos(vfoLR);  // Make it so
                }
            }
        }
        else if (strncmp(buf, "FT", 2) == 0)
        {
            // Transmitter Function ( VFO A / VFO B )
            int idx;

            if (buf[2] == ';')
            {
                // Read
                idx = sp && !tfset;
                snprintf(buf, sizeof(buf), "FT%d;", (*vfoLR[idx])->vfo);
                OUTPUT(buf);
            }
            else
            {
                // Set
                idx = buf[2] - '0';

                if (idx < 0 || idx > 1) {cmd_err = 1; continue; }

                sp = idx != (*vfoLR[0])->vfo;    // Turn split on if vfos differ, else off
            }
        }
        else if (buf[0] == 'B' && (buf[1] == 'D' || buf[1] == 'U'))  // BU/BD
        {
            // Frequency Band Selection(Setting 1)/[UP}/{DOWN] Operating(Setting 2)
            int band, idx, newfreq;
            int dir = buf[1] == 'D' ? -1 : +1;
            kvfop_t ovfo = *vfoLR[0];  // Current operating VFO

            if (buf[2] == ';')
            {
                // Setting 2
                /* The TS-890S doesn't have a real BAND_UP/BAND_DOWN command
                 * This one just does a simple UP/DOWN. As the manual says, just
                 * like pushing the UP/DOWN button on the mic
                 */
                int class = mode2classtab[ovfo->mode];

                if (class < 0 || class > 3) {cmd_err = 3; continue;} // Shouldn't happen

                newfreq = ovfo->freq + (dir * stepsize[class]);
                //TODO: Checking for band edges needs to go here
                ovfo->freq = newfreq;
            }
            else if (buf[3] == ';')
            {
                // Read
                idx = buf[2] - '0';

                if (idx < 0 || idx > 1) {cmd_err = 1; continue;}

                snprintf(buf + 3, sizeof(buf) - 3, "%d;", bandslot[idx][ovfo->band] + 1);
                OUTPUT(buf);
            }
            else if (buf[5] == ';')
            {
                // Setting 1
                band = atoi(buf + 3);

                if (band < 0 || band >= NBANDS) {cmd_err = 1; continue;}

                if (band == ovfo->band)
                {
                    // Same band, cycle the band memory #
                    bandslot[ovfo->vfo][band] = (bandslot[ovfo->vfo][band] + 1) % nummems;
                }

                *vfoLR[0] = newvfo(ovfo, band);
            }
            else
            {
                cmd_err = 1;
            }
        }
        else if ((strncmp(buf, "DN", 2) == 0) || (strncmp(buf, "UP", 2) == 0))
        {
            // Microphone UP/DOWN Switch Operation
            int dir = buf[0] == 'D' ? -1 : +1;
            int steps = -1;
            kvfop_t ovfo = *vfoLR[0]; // Modify the current operational VFO

            if (buf[2] == ';')
            {
                steps = 1;
            }
            else if (buf[4] == ';')
            {
                steps = atoi(buf + 2);
            }

            if (steps < 0 || steps > 99) {cmd_err = 1; continue;}

            ovfo->freq += dir * steps * stepsize[mode2classtab[ovfo->mode]];
        }
        else if (strncmp(buf, "FC", 2) == 0)
        {
            // Change the Frequency (Tuning Control)
            static const int fc_steps[6] = { 1, 2, 5, 10, 50, 100};
            int dir = buf[2] == '0' ? +1 : -1;
            int stepidx = buf[3] - '0';
            int delta;
            kvfop_t ovfo = *vfoLR[0];

            if (stepidx < 0 || stepidx > 5) {cmd_err = 1; continue;}

            delta = dir * fc_steps[stepidx] * stepsize[mode2classtab[ovfo->mode]];
            //TODO: This really needs a sanity check here
            ovfo->freq += delta;
        }
        else if (strncmp(buf, "UD", 2) == 0)
        {
            // VFO Frequency UP/DOWN
            int idx = buf[2] - '0';
            int dir = buf[3] == '0' ? +1 : -1;
            int steps = -1;
            kvfop_t nvfo;

            if (idx < 0 || idx > 1 || tfset != 0) {cmd_err = 1; continue;}

            nvfo = *vfoAB[idx];

            if (buf[4] == ';')
            {
                steps = 1;
            }
            else if (buf[6] == ';')
            {
                steps = atoi(buf + 4);
            }

            if (steps < 0 || steps > 99) {cmd_err = 1; continue; }

            nvfo->freq += dir * steps * stepsize[mode2classtab[nvfo->mode]];
        }
        else if (strcmp(buf, "RX;") == 0)
        {
            // Receive Function State
            ptt = ptt_mic = ptt_data = ptt_tune = 0;
        }
        else if (strncmp(buf, "TX", 2) == 0)
        {
            // Transmission Mode
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
        else if (strncmp(buf, "SP", 2) == 0)
        {
            // Split Operation Frequency Setting
            if (buf[2] == ';')
            {
                // Read
                snprintf(buf + 2, sizeof(buf) - 2, "%1d;", split_op);
                OUTPUT(buf);
            }
            else if (buf[3] == ';')
            {
                // Set 1
                /* This section needs a lot of work, and a lot
                 * of cooperation from other commands.
                 * AFAICT the split freq can be set by spinning
                 * the big knob, or by other means. When oper=0
                 * is sent, the current freq is used as the split
                 * value. See page 5-1 of the IM, blinking SPLIT
                 */
                switch (buf[2])
                {
                case '0':

                    // Operation complete
                    if (split_op) // If a split setup was in progress,
                    {
                        sp = 1; // split operation is enabled
                    }

                    //TODO: Set split freq VFO
                    split_op = 0;
                    break;

                case '1':
                    // Start split frequency setup
                    split_op = 1;
                    break;

                case '2':
                    // Cancel op
                    split_op = 0;
                    break;

                default:
                    cmd_err = 1;
                }
            }
            else
            {
                // Set 2
                int dir, split, spfreq, band;
                kvfop_t ovfo, svfo;
                sscanf(buf, "SP%1d%1d%1d", &sp, &dir, &split);
                dir = dir == 0 ? +1 : -1;
                split = dir * 1000 * split;  // Convert kHz to +/- Hz
                ovfo = *vfoLR[0];   // Operational VFO
                spfreq = ovfo->freq + split;
                band = freq2band(spfreq);
                svfo = newvfo(*vfoLR[1], band);  // Other VFO
                svfo->freq = spfreq;
                *vfoLR[1] = svfo;
                sp = 1; // Turn On Split
            }
        }
        else if (strncmp(buf, "TB;", 3) == 0)
        {
            // Split
            sprintf(buf, "TB%d;", sp);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "TB", 2) == 0)
        {
            sscanf(buf, "TB%d", &sp);
        }
        else if (strncmp(buf, "TS", 2) == 0)
        {
            // TF-SET
            if (buf[2] == ';')
            {
                snprintf(buf, sizeof buf, "TS%d;", tfset);
                OUTPUT(buf);
            }
            else if (buf[2] >= '0' && buf[2] < '2')
            {
                if (sp && (tfset != buf[2] - '0'))
                {
                    // Split is set and we're changing state of TF-SET
                    swapvfos(vfoLR);  // Reverse vfo functions
                }

                tfset = buf[2] - '0';
            }
            else
            {
                cmd_err = 1;
            }
        }
        else if (strcmp(buf, "EC;") == 0)
        {
            // VFO A and VFO B Frequency Information Exchange
            /* No matter what the title says above, the TS-890S does not
             * have a frequency swap command.  It does, however, have a VFO
             * function exchange - just by swapping the left and right displays.
             * This command is the same as the "A/B" button on the front panel.
             */
            swapvfos(vfoLR);
        }
        else if (strcmp(buf, "VV;") == 0)
        {
            // VFO A to VFO B Copy ([A=B] Operation)
            /* Akin to the EC command above, this isn't really a "VFO A to VFO B"
             * copy, but an "Operational VFO to Secondary VFO" copy. It also
             * mimics the front panel [A=B] action.
             */
            kvfop_t ovfo, svfo;
            ovfo = *vfoLR[0];
            svfo = newvfo(*vfoLR[1], ovfo->band); // Get appropriate vfo for new freq
            svfo->freq = ovfo->freq;
            svfo->mode = ovfo->mode;
            *vfoLR[1] = svfo;
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
        else if (strncmp(buf, "KY", 2) == 0)
            { // CW Keying
                if (buf[2] == ';')
                {
                     // Checking status - we always have room
                     OUTPUT("KY0;");
                }
            else if (buf[3] == ';')
            {
                // Stop sending(?)
                if (buf[2] != '0') {cmd_err = 1; }
            }
            else
            {
                // Send the message
                //printf("CW mesg: %s\n", buf + 2);
            }
        }
        else if (strncmp(buf, "OM", 2) == 0)
        {
            // Operating Mode
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
                    sprintf(buf, "OM%d%X;", tmpvfo, (*vfoLR[tmpvfo])->mode);
                    OUTPUT(buf);
                }
            }
            else
            {
                /* Setting - Only sets the active function(RX/TX),
                 * which is always the left VFO unless split is active and
                 * we are transmitting.
                 */
                int idx = sp && ((ptt + ptt_mic + ptt_data + ptt_tune) > 0);
                sscanf(&buf[3], "%1X", &(*vfoLR[idx])->mode);
            }
        }
        else if (strncmp(buf, "MD", 2) == 0)
        {
            // Sets and reads the operating mode status
#if defined(LEGACY)
            if (buf[2] == ';')
            {
                snprintf(buf, sizeof(buf), "MD%X;", (*vfoLR[0])->mode);
                OUTPUT(buf);
            }
            else
            {
                sscanf(buf, "MD%1X", &(*vfoLR[0])->mode);
            }

#else
            cmd_err = 1;
#endif
        }
        else if (strncmp(buf, "RM", 2) == 0)
        {
            // Meter
            if (buf[2] == ';')
            {
                // Read all enabled meters
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
            {
                // Enable/disable one meter
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
            sprintf(buf, "SL0%02d;", sl);
            printf("R: %s\n", buf);
            OUTPUT(buf);
        }
        else if (strcmp(buf, "SH0;") == 0)
        {
            sprintf(buf, "SH0%03d;", sh);
            printf("R: %s\n", buf);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "SL0", 3) == 0)
        {
            printf("Cmd: %s\n", buf);
            sscanf(buf, "SL0%3d", &sl);
        }
        else if (strncmp(buf, "SH0", 3) == 0)
        {
            printf("Cmd: %s\n", buf);
            sscanf("SH0%3d", "%d", &sh);
        }
        else if (strcmp(buf, "NR;") == 0)
        {
            sprintf(buf, "NR%d;", nr);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "NR", 2) == 0)
        {
            puts(buf);
            sscanf(buf, "NR%d", &nr);
        }
        else if (strcmp(buf, "PA;") == 0)
        {
            sprintf(buf, "PA%d;", pa);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "PA", 2) == 0)
        {
            sscanf(buf, "PA%d", &pa);
        }
        else if (strcmp(buf, "SM;") == 0)
        {
            sprintf(buf, "SM%04d;", sm);
            OUTPUT(buf);
        }
        else if (strcmp(buf, "PC;") == 0)
        {
            sprintf(buf, "PC%03d;", sm);
            OUTPUT(buf);
        }
        else if (strcmp(buf, "NT;") == 0)
        {
            sprintf(buf, "NT%d;", nt);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "NT", 2) == 0)
        {
            sscanf(buf, "NT%d", &nt);
        }
        else if (strcmp(buf, "AG;") == 0)
        {
            sprintf(buf, "AG%03d;", ag);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "AG", 2) == 0)
        {
            sscanf(buf, "AG%d", &ag);
        }
        else if (strcmp(buf, "AC;") == 0)
        {
            sprintf(buf, "AC%03d;", ac);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "AC", 2) == 0)
        {
            sscanf(buf, "AC%d", &ac);
        }
        else if (strcmp(buf, "SQ;") == 0)
        {
            sprintf(buf, "SQ%03d;", sq);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "SQ", 2) == 0)
        {
            sscanf(buf, "SQ%d", &sq);
        }
        else if (strcmp(buf, "RG;") == 0)
        {
            sprintf(buf, "RG%03d;", rg);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "RG", 2) == 0)
        {
            sscanf(buf, "RG%d", &rg);
        }
        else if (strcmp(buf, "MG;") == 0)
        {
            sprintf(buf, "MG%03d;", mg);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "MG", 2) == 0)
        {
            sscanf(buf, "MG%d", &mg);
        }
        else if (strncmp(buf, "RL1;", 3) == 0)
        {
            snprintf(buf, sizeof(buf), "RL%02d;", rl);
            OUTPUT(buf);
        }
        else if (strncmp(buf, "RL1", 2) == 0)
        {
            puts(buf);
            sscanf(buf, "RL1%d", &rl);
        }
        else if (strncmp(buf, "FS", 2) == 0)
        {
            // FINE Function
            if (buf[2] == ';')
            {
                snprintf(buf, sizeof buf, "FS%d%d;", fine, fine); // For now
                OUTPUT(buf);
            }
            else
            {
                if (buf[2] == '0' || buf[2] == '1')
                { fine = buf[2] - '0'; }
                else
                { cmd_err = 1; }
            }
        }
        else if (strcmp(buf, "RC;") == 0)
        {
            // RIT/XIT Frequency Clear
            rxit = 0;
        }
        else if (buf[0] == 'R' && (buf[1] == 'D' || buf[1] == 'U')) // RD/RU
        {
            // RIT/XIT Frequency Up/Down
            int dir = buf[1] == 'D' ? -1 : +1;
            int tempit;

            if (buf[2] == ';')
            {
                tempit = rxit + (dir * (fine ? 1 : 10));
            }
            else
            {
                tempit = rxit + dir * atoi(buf + 2);
            }

            if (abs(tempit) > 9999) {cmd_err = 1; continue;}

            /* Some weird rounding going on here - TBD */
            rxit = tempit;
        }
        else if (strcmp(buf, "RF;") == 0)
        {
            // RIT/XIT Frequency
            snprintf(buf, sizeof buf, "RF%1d%04d;", rxit < 0 ? 1 : 0, abs(rxit));
            OUTPUT(buf);
        }
        else if (strncmp(buf, "RT", 2) == 0)
        {
            // RIT Function State, RIT Shift
            switch (buf[2])
            {
            case ';': // Read
                snprintf(buf, sizeof buf, "RT%d;", rit);
                OUTPUT(buf);
                break;

            case '0': // Set
            case '1':
                rit = buf[2] - '0';
                break;

            case '2': // Shift
                //TODO: set recv freq to vfo+rxit, clear rxit and rit
                break;

            default:
                cmd_err = 1;
            }
        }
        else if (strncmp(buf, "XT", 2) == 0)
        {
            // XIT Function State, XIT Shift
            switch (buf[2])
            {
            case '0': // Set
            case '1':
                xit = buf[2] - '0';
                break;

            case '2': // Shift
                //TODO: set xmit freq to vfo+rxit(Which vfo?), set split, clear rxit and xit
                break;

            default:
                cmd_err = 1;
            }
        }
        else if (strncmp(buf, "CK", 2) == 0)
        {
            // All the clock functions
            switch (buf[2])
            {
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
        else if (strncmp(buf, "BS", 2) == 0)
        {
            // All the Bandscope commands
            switch (toupper(buf[2]))
            {
            case '0': // Scope Display ON/OFF
            case '1': // Scope Display Type
            case '2': // Bandscpoe Operation Mode
            case '3': // Bandscope Span
            case '4': // Bandscope Span
            case '5': // Bandscope Scope Range (Fixed Mode)
            case '6': // Bandscope Display Pause
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
        else if (strncmp(buf, "CD", 2) == 0)
        {
            // CW Communications
            switch (buf[2])
            {
            case '0': // CW Communication Screen Display
            case '1': // CW Morse Decoding Threshold Level
            case '2': // Decoded CW Morse Character Output
            case '3': // CW Communication Screen (Decode Filter)
            case '4': // CW Communication Screen (Quick Mode)
            case '5': // CW Decode
                break;

            default:
                cmd_err = 1;
            }
        }
        else if (strncmp(buf, "CM", 2) == 0)
        {
            // CW Message Memory
            switch (buf[2])
            {
            case '0': // Registration of CW Message (Paddle Input)
            case '1': // Play/Stop the CW Message
            case '2': // Register State of CW Message (Paddle Input)
            case '3': // Clear the CW Message (Paddle Input)
            case '4': // CW Message Memory Name (Paddle Input)
            case '5': // Registering the CW Message Memory (Text Input)
            case '6': // CW Message Channel Repeat
            case '7': // Contest Number
                break;

            default:
                cmd_err = 1;  // Unknown command
            }
        }
        else if (strncmp(buf, "FL", 2) == 0)
        {
            switch (buf[2])
            {
            case '0': // Select the Receive Filter
            case '1': // Roofing Filter
            case '2': // IF Filter Shape
            case '3': // AF Filter Type
                continue; // For now

            default:
                cmd_err = 1;
            }
        }
        else if (strncmp(buf, "FM", 2) == 0)
        {
            // Frequency Markers
            switch (buf[2])
            {
            case '0': // Frequency Marker Function
            case '1': // Frequency Marker List Regiatration
            case '2': // Total Number Registered of Frequency Marker List
            case '3': // Frequency Marker List Readout
            case '4': // Frequency Marker List Delete
                break;

            default:
                cmd_err = 1;
            }
        }
        else if (strncmp(buf, "IP", 2) == 0)
        {
            // Network Config
            switch (buf[2])
            {
            case '0': // DHCP
            case '1': // IP Address (Manual Configuration)
            case '2': // MAC Address
                break;

            default:
                cmd_err = 1;
            }
        }
        else if (strncmp(buf, "LA", 2) == 0)
        {
            // Linear Amplifier Configuration
            switch (buf[2])
            {
            case '0': // Target Band of Linear Amplifier Menu
            case '1': // Linear Amplifier ON/OFF
            case '2': // Linear Amplifier Transmission Control
            case '3': // Linear Amplifier Transmission Delay ON/OFF
            case '4': // Linear Amplifier Transmission Delay Time
            case '5': // Linear Amplifier Relay Control
            case '6': // Linear Amplifier External ALC Voltage
                break;

            default:
                cmd_err = 1;
            }
        }
        else if (strncmp(buf, "MA", 2) == 0)
        {
            // Memory Channel Functions
            switch (buf[2])
            {
            case '0': // Memory Channel Configuration
            case '1': // Memort Channel (Direct Write)
            case '2': // Memory Channel (Channel Name)
            case '3': // Memory Channel (Scan Lockout)
            case '4': // Memory Channel (Channel Copy)
            case '5': // Memory Channel (Channel Deletion)
            case '6': // Programmable VFO End Frequency
            case '7': // Memory Channel (Temporary Change Frequency)
                break;

            default:
                cmd_err = 1;
            }
        }
        else if (strncmp(buf, "PB", 2) == 0)
        {
            // Voice Messages
            switch (buf[2])
            {
            case '0': // Voice Message List Display
            case '1': // Voice Message Playback, etc.
            case '2': // Voice Message Channel Registration State
            case '3': // Voice Message Channel Repeat
            case '4': // Voice Message Channel Name
            case '5': // Voice Message Recording Sound Source
            case '6': // Voice Message Recording Total Remaining Time
                break;

            default:
                cmd_err = 1;
            }
        }
        else if (strncmp(buf, "SC", 2) == 0)
        {
            // Scan functions
            switch (buf[2])
            {
            case '0': // Scan
            case '1': // Scan Speed
            case '2': // Tone Scan/CTCSS Scan
            case '3': // Program Scan/VFO Scan Selection
                break;

            default:
                cmd_err = 1;
            }
        }
        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }

    }

    return 0;
}

/* Convert freq to TS-890S band #
 *
 * Input freq in Hz
 *
 * Returns band # or negative if invalid input
 */
int freq2band(int freq)
{
    int i, retval = -1;  // Assume the worst

    for (i = 0; i < NBANDS; i++)
    {
        if (freq >= band_limits[i].low && freq <= band_limits[i].high)
        {
            retval = i;
            break;
        }
    }

    //printf("%dHz is in band # %d\n", freq, retval);

    return retval;
}

/* Get appropriate vfo for new frequency
 *
 * Input: current vfo
 *        new band
 * Return: new vfo pointer
 */
kvfop_t newvfo(kvfop_t ovfo, int band)
{
    int vfonum, slot;

    vfonum = ovfo->vfo;
    slot = bandslot[vfonum][band];

    return &band_mem[vfonum][band][slot];
}

/* Reverse the function of vfoA and vfoB
 * No status returned
 */
void swapvfos(kvfop_t *vfoset[])
{
    kvfop_t *temp;

    temp = vfoset[0];
    vfoset[0] = vfoset[1];
    vfoset[1] = temp;

    return;
}
