#include <stdio.h>
#include <string.h>

/* DB25 Pins
    1 - GND
    2 - TXD
    3 - RXD
    4 - RTS
    5 - CTS
    6 - DSR
    7 - Signal ground (tied to 1?)
    8 - DCD
    9 - Tx Audio 600Ohm - Pin 7 is gnd
    10 - RUS Receiver Unsquelched Sensor
    11 - Rx Audio 600Ohm - Pin 7 is gnd
    12 - Radio Inhibit (Sleep).
    13 - Reserved
    14 - PTT +5V
*/

void serial_num(char *s)
{
    char model[16];
    char *operation;
    char *modem;
    char *rxfreq, *txfreq;
    char *bandwidth;
    char *diagnostics;
    char *agency;

    memcpy(model, s, 5);
    model[5] = 0;

    if (s[5] == 'B') { operation = "Base"; }
    else { operation = "Remote"; }

    if (s[8] == 'B') { modem = "9600"; }
    else { modem = "19200"; }

    if (s[9] == '0')
        diagnostics = "None";
    else
        diagnostics = "Non-Intrusive";
    if (s[10] == '1') { bandwidth = "12.5KHz"; }
    else { bandwidth = "25KHz"; }

    if (s[14] == 'N')
        agency = "N/A";
        else
            agency = "FCC/IC";

/*
/              01234567890123456
               9710AXN1B11C00FFA
               5 = X/N Base/Remote
               6 = N Non-redundant
               7 = Input voltage (1=10.5 to 16VDC)
               8 = Modem (B=9600 C=19200)
               9 = Diagnostics (0=None, 1=Non-Intrusive)
               10 = Bandwidth (1=12.5 2=25)
               11 = Rx Freq (A=800-860, B=860-900, C=900-960)
               12 = Tx Freq (1=800-880, 2=880-960)
               13 = 0 = Full
               14 = Agency (N=N/A F=FCC/IC)
               */

if (s[0] == '9')
{
    switch (s[11])
    {
    case 'A':
        rxfreq = "800-850MHz";
        break;

    case 'B':
        rxfreq = "860-899MHz";
        break;

    case 'C':
        rxfreq = "900-960MHz";
        break;

    default:
        rxfreq = "Unknown!!";
    }

    switch (s[12])
    {
    case '1':
        txfreq = "800-880MHz";
        break;

    case '2':
        txfreq = "880-960MHz";
        break;

    default:
        txfreq = "Unknown!!";
    }

}

printf("Model %s %s Modem(%s) Rx(%s) Tx(%s) Bandwidth(%s) Agency=%s Diagnostics=%s\n",
       model, operation,
       modem, rxfreq, txfreq, bandwidth, agency, diagnostics);
}

int
main()
{
    serial_num("9710BXN1B11C9ONFA");
    serial_num("9710AXN1B11C00FFA");
    return 0;
}
