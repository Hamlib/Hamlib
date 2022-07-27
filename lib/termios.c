#include <hamlib/rig.h>
#include <hamlib/config.h>

#if defined(WIN32) && !defined(HAVE_TERMIOS_H)

#undef DEBUG
//#undef TRACE

#ifdef DEBUG
#define DEBUG_VERBOSE
#define DEBUG_ERRORS
#define report(a) fprintf(stderr,a)
#define report_warning(a) fprintf(stderr,a)
#define report_error(a) fprintf(stderr,a)
#else
#define report(a) do {} while (0)
#define report_warning(a) do {} while (0)
#define report_error(a) do {} while (0)
#endif /* DEBUG */
/*-------------------------------------------------------------------------
|   rxtx is a native interface to serial ports in java.
|   Copyright 1997-2002 by Trent Jarvi taj@www.linux.org.uk.
|   Copyright 1997-2006 by Trent Jarvi taj@www.linux.org.uk.
|
|   This library is free software; you can redistribute it and/or
|   modify it under the terms of the GNU Lesser General Public
|   License as published by the Free Software Foundation; either
|   version 2.1 of the License, or (at your option) any later version.
|
|   If you compile this program with cygwin32 tools this package falls
|   under the GPL.  See COPYING.CYGNUS for details.
|
|   This library is distributed in the hope that it will be useful,
|   but WITHOUT ANY WARRANTY; without even the implied warranty of
|   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
|   Lesser General Public License for more details.
|
|   You should have received a copy of the GNU Lesser General Public
|   License along with this library; if not, write to the Free Software
|   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
|
| This file was taken from rxtx-2.1-7 and adapted for Hamlib.
--------------------------------------------------------------------------*/
#include <windows.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include "win32termios.h"

/*
 * odd malloc.h error with lcc compiler
 * winsock has FIONREAD with lcc
 */

#ifdef __LCC__
#   include <winsock.h>
#else
#   include <malloc.h>
#endif /* __LCC__ */

#define SIGIO 0

int my_errno;
#if 0
extern int errno;
#endif
struct termios_list
{
    char filename[512];
    int my_errno;
    int interrupt;
    int event_flag;
    int tx_happened;
    unsigned long *hComm;
    struct termios *ttyset;
    struct serial_struct *sstruct;
    /* for DTR DSR */
    unsigned char MSR;
    struct async_struct *astruct;
    struct serial_icounter_struct *sis;
    int open_flags;
    OVERLAPPED rol;
    OVERLAPPED wol;
    OVERLAPPED sol;
    int fd;
    struct termios_list *next;
    struct termios_list *prev;
};
struct termios_list *first_tl = NULL;

static struct termios_list *find_port(int);

/*----------------------------------------------------------
serial_test

   accept: filename to test
   perform:
   return:      1 on success 0 on failure
   exceptions:
   win32api:    CreateFile CloseHandle
   comments:    if the file opens it should be ok.
----------------------------------------------------------*/
int win32_serial_test(char *filename)
{
    unsigned long *hcomm;
    int ret;
    hcomm = CreateFile(filename, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                       0, 0);

    if (hcomm == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_ACCESS_DENIED)
        {
            ret = 1;
        }
        else
        {
            ret = 0;
        }
    }
    else
    {
        ret = 1;
    }

    CloseHandle(hcomm);
    return (ret);
}

static void termios_setflags(int fd, int termios_flags[])
{
    struct termios_list *index = find_port(fd);
    int i, result;
    int windows_flags[11] = { 0, EV_RXCHAR, EV_TXEMPTY, EV_CTS, EV_DSR,
                              EV_RING | 0x2000, EV_RLSD, EV_ERR,
                              EV_ERR, EV_ERR, EV_BREAK
                            };

    if (!index)
    {
        LEAVE("termios_setflags");
        return;
    }

    index->event_flag = 0;

    for (i = 0; i < 11; i++)
        if (termios_flags[i])
        {
            index->event_flag |= windows_flags[i];
        }

    result = SetCommMask(index->hComm, index->event_flag);

    /*
       This is rank.  0x2000 was used to detect the trailing edge of ring.
       The leading edge is detedted by EV_RING.

       The trailing edge is reliable.  The leading edge is not.
       Softie no longer allows the trailing edge to be detected in NTsp2
       and beyond.

       So... Try the reliable option above and if it fails, use the less
       reliable means.

       The screams for a giveio solution that bypasses the kernel.
    */
    if (index->event_flag & 0x2000 && result == 0)
    {
        index->event_flag &= ~0x2000;
        SetCommMask(index->hComm, index->event_flag);
    }
}

#if 0
/*----------------------------------------------------------
get_fd()

   accept:      filename
   perform:     find the file descriptor associated with the filename
   return:      fd
   exceptions:
   win32api:    None
   comments:    This is not currently called by anything
----------------------------------------------------------*/

int get_fd(char *filename)
{
    struct termios_list *index = first_tl;

    ENTER("get_fd");

    if (!index)
    {
        return -1;
    }

    while (strcmp(index->filename, filename))
    {
        index = index->next;

        if (!index->next)
        {
            return (-1);
        }
    }

    LEAVE("get_fd");
    return (index->fd);
}

/*----------------------------------------------------------
get_filename()

   accept:      file descriptor
   perform:     find the filename associated with the file descriptor
   return:      the filename associated with the fd
   exceptions:  None
   win32api:    None
   comments:    This is not currently called by anything
----------------------------------------------------------*/

char *get_filename(int fd)
{
    struct termios_list *index = first_tl;

    ENTER("get_filename");

    if (!index)
    {
        return ("bad");
    }

    while (index->fd != fd)
    {
        if (index->next == NULL)
        {
            return ("bad");
        }

        index = index->next;
    }

    LEAVE("get_filename");
    return (index->filename);
}

/*----------------------------------------------------------
dump_termios_list()

   accept:      string to print out.
   perform:
   return:
   exceptions:
   win32api:    None
   comments:    used only for debugging eg serial_close()
----------------------------------------------------------*/

void dump_termios_list(char *foo)
{
#ifdef DEBUG
    struct termios_list *index = first_tl;
    printf("============== %s start ===============\n", foo);

    if (index)
    {
        printf("%i filename | %s\n", index->fd, index->filename);
    }

    /*
        if ( index->next )
        {
            printf( "%i filename | %s\n", index->fd, index->filename );
        }
    */
    printf("============== %s end  ===============\n", foo);
#endif
}
#endif

/*----------------------------------------------------------
set_errno()

   accept:
   perform:
   return:
   exceptions:
   win32api:    None
   comments:   FIXME
----------------------------------------------------------*/

static void set_errno(int error)
{
    my_errno = error;
}

#if 0
/*----------------------------------------------------------
usleep()

   accept:
   perform:
   return:
   exceptions:
   win32api:    Sleep()
   comments:
----------------------------------------------------------*/

static void usleep(unsigned long usec)
{
    Sleep(usec / 1000);
}
#endif

/*----------------------------------------------------------
CBR_toB()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

static int CBR_to_B(int Baud)
{
    ENTER("CBR_to_B");

    switch (Baud)
    {

    case 0:         return (B0);

    case 50:        return (B50);

    case 75:        return (B75);

    case CBR_110:       return (B110);

    case 134:       return (B134);

    case 150:       return (B150);

    case 200:       return (B200);

    case CBR_300:       return (B300);

    case CBR_600:       return (B600);

    case CBR_1200:      return (B1200);

    case 1800:      return (B1800);

    case CBR_2400:      return (B2400);

    case CBR_4800:      return (B4800);

    case CBR_9600:      return (B9600);

    case CBR_14400:     return (B14400);

    case CBR_19200:     return (B19200);

    case CBR_28800:     return (B28800);

    case CBR_38400:     return (B38400);

    case CBR_57600:     return (B57600);

    case CBR_115200:    return (B115200);

    case CBR_128000:    return (B128000);

    case CBR_230400:    return (B230400);

    case CBR_256000:    return (B256000);

    case CBR_460800:    return (B460800);

    case CBR_500000:    return (B500000);

    case CBR_576000:    return (B576000);

    case CBR_921600:    return (B921600);

    case CBR_1000000:   return (B1000000);

    case CBR_1152000:   return (B1152000);

    case CBR_1500000:   return (B1500000);

    case CBR_2000000:   return (B2000000);

    case CBR_2500000:   return (B2500000);

    case CBR_3000000:   return (B3000000);

    case CBR_3500000:   return (B3500000);

    case CBR_4000000:   return (B4000000);

    default:
        /* assume custom baudrate */
        return (Baud);
    }
}

/*----------------------------------------------------------
B_to_CBR()

   accept:
   perform:
   return:
   exceptions:
   win32api:
   comments:      None
----------------------------------------------------------*/

static int B_to_CBR(int Baud)
{
    int ret;
    ENTER("B_to_CBR");

    switch (Baud)
    {
    case 0:     ret = 0;        break;

    case B50:   ret = 50;       break;

    case B75:   ret = 75;       break;

    case B110:  ret = CBR_110;      break;

    case B134:  ret = 134;      break;

    case B150:  ret = 150;      break;

    case B200:  ret = 200;      break;

    case B300:  ret = CBR_300;      break;

    case B600:  ret = CBR_600;      break;

    case B1200: ret = CBR_1200;     break;

    case B1800: ret = 1800;     break;

    case B2400: ret = CBR_2400;     break;

    case B4800: ret = CBR_4800;     break;

    case B9600: ret = CBR_9600;     break;

    case B14400:    ret = CBR_14400;    break;

    case B19200:    ret = CBR_19200;    break;

    case B28800:    ret = CBR_28800;    break;

    case B38400:    ret = CBR_38400;    break;

    case B57600:    ret = CBR_57600;    break;

    case B115200:   ret = CBR_115200;   break;

    case B128000:   ret = CBR_128000;   break;

    case B230400:   ret = CBR_230400;   break;

    case B256000:   ret = CBR_256000;   break;

    case B460800:   ret = CBR_460800;   break;

    case B500000:   ret = CBR_500000;   break;

    case B576000:   ret = CBR_576000;   break;

    case B921600:   ret = CBR_921600;   break;

    case B1000000:  ret = CBR_1000000;  break;

    case B1152000:  ret = CBR_1152000;  break;

    case B1500000:  ret = CBR_1500000;  break;

    case B2000000:  ret = CBR_2000000;  break;

    case B2500000:  ret = CBR_2500000;  break;

    case B3000000:  ret = CBR_3000000;  break;

    case B3500000:  ret = CBR_3500000;  break;

    case B4000000:  ret = CBR_4000000;  break;

    default:
        /* assume custom baudrate */
        return Baud;
    }

    LEAVE("B_to_CBR");
    return ret;
}

/*----------------------------------------------------------
bytesize_to_termios()

   accept:
   perform:
   return:
   exceptions:
   win32api:      None
   comments:
----------------------------------------------------------*/

static int bytesize_to_termios(int ByteSize)
{
    ENTER("bytesize_to_termios");

    switch (ByteSize)
    {
    case 5: return (CS5);

    case 6: return (CS6);

    case 7: return (CS7);

    case 8:
    default: return (CS8);
    }
}

/*----------------------------------------------------------
termios_to_bytesize()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

static int termios_to_bytesize(int cflag)
{
    ENTER("termios_to_bytesize");

    switch (cflag & CSIZE)
    {
    case CS5: return (5);

    case CS6: return (6);

    case CS7: return (7);

    case CS8:
    default: return (8);
    }
}

#if 0
/*----------------------------------------------------------
get_dos_port()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

static const char *get_dos_port(char const *name)
{
    ENTER("get_dos_port");

    if (!strcmp(name, "/dev/cua0")) { return ("COM1"); }

    if (!strcmp(name, "/dev/cua1")) { return ("COM2"); }

    if (!strcmp(name, "/dev/cua2")) { return ("COM3"); }

    if (!strcmp(name, "/dev/cua3")) { return ("COM4"); }

    LEAVE("get_dos_port");
    return ((const char *) name);
}
#endif

/*----------------------------------------------------------
ClearErrors()

   accept:
   perform:      keep track of errors for the eventLoop() (SerialImp.c)
   return:       the return value of ClearCommError()
   exceptions:
   win32api:     ClearCommError()
   comments:
----------------------------------------------------------*/

static int ClearErrors(struct termios_list *index, COMSTAT *Stat)
{
    unsigned long ErrCode;
    int ret;

    ret = ClearCommError(index->hComm, &ErrCode, Stat);

    if (ret == 0)
    {
        YACK();
        return (ret);
    }

#ifdef DEBUG_ERRORS

    if (ErrCode)
    {
        printf("%i frame %i %i overrun %li %i  parity %u %i brk %i %i\n",
               (int) ErrCode,
               (int) ErrCode & CE_FRAME,
               index->sis->frame,
               (int)(ErrCode & CE_OVERRUN) | (ErrCode & CE_RXOVER),
               index->sis->overrun,
               (int) ErrCode & CE_RXPARITY,
               index->sis->parity,
               (int) ErrCode & CE_BREAK,
               index->sis->brk
              );
    }

#endif /* DEBUG_ERRORS */

    if (ErrCode & CE_FRAME)
    {
        index->sis->frame++;
        ErrCode &= ~CE_FRAME;
    }

#ifdef LIFE_IS_GOOD
    FIXME OVERRUN is spewing

    if (ErrCode & CE_OVERRUN)
    {
        index->sis->overrun++;
        ErrCode &= ~CE_OVERRUN;
    }
    /* should this be here? */
    else if (ErrCode & CE_RXOVER)
    {
        index->sis->overrun++;
        ErrCode &= ~CE_OVERRUN;
    }

#endif /* LIFE_IS_GOOD */

    if (ErrCode & CE_RXPARITY)
    {
        index->sis->parity++;
        ErrCode &= ~CE_RXPARITY;
    }

    if (ErrCode & CE_BREAK)
    {
        index->sis->brk++;
        ErrCode &= ~CE_BREAK;
    }

    return (ret);
}

#if 0
/*----------------------------------------------------------
FillDCB()

   accept:
   perform:
   return:
   exceptions:
   win32api:     GetCommState(),  SetCommState(), SetCommTimeouts()
   comments:
----------------------------------------------------------*/

static BOOL FillDCB(DCB *dcb, unsigned long *hCommPort, COMMTIMEOUTS Timeout)
{

    ENTER("FillDCB");
    dcb->DCBlength = sizeof(dcb);

    if (!GetCommState(hCommPort, dcb))
    {
        report("GetCommState\n");
        return (-1);
    }

    dcb->BaudRate        = CBR_9600 ;
    dcb->ByteSize        = 8;
    dcb->Parity          = NOPARITY;
    dcb->StopBits        = ONESTOPBIT;
    dcb->fDtrControl     = DTR_CONTROL_ENABLE;
    dcb->fRtsControl     = RTS_CONTROL_ENABLE;
    dcb->fOutxCtsFlow    = FALSE;
    dcb->fOutxDsrFlow    = FALSE;
    dcb->fDsrSensitivity = FALSE;
    dcb->fOutX           = FALSE;
    dcb->fInX            = FALSE;
    dcb->fTXContinueOnXoff = FALSE;
    dcb->XonChar         = 0x11;
    dcb->XoffChar        = 0x13;
    dcb->XonLim          = 0;
    dcb->XoffLim         = 0;
    dcb->fParity = TRUE;

    if (EV_BREAK | EV_CTS | EV_DSR | EV_ERR | EV_RING | (EV_RLSD & EV_RXFLAG))
    {
        dcb->EvtChar = '\n';
    }
    else { dcb->EvtChar = '\0'; }

    if (!SetCommState(hCommPort, dcb))
    {
        report("SetCommState\n");
        YACK();
        return (-1);
    }

    if (!SetCommTimeouts(hCommPort, &Timeout))
    {
        YACK();
        report("SetCommTimeouts\n");
        return (-1);
    }

    LEAVE("FillDCB");
    return (TRUE) ;
}
#endif

/*----------------------------------------------------------
serial_close()

   accept:
   perform:
   return:
   exceptions:
   win32api:      SetCommMask(), CloseHandle()
   comments:
----------------------------------------------------------*/

int win32_serial_close(int fd)
{
    struct termios_list *index;
    /* char message[80]; */

    ENTER("serial_close");

    if (!first_tl || !first_tl->hComm)
    {
        report("gotit!");
        return (0);
    }

    index = find_port(fd);

    if (!index)
    {
        LEAVE("serial_close");
        return -1;
    }

    /* WaitForSingleObject( index->wol.hEvent, INFINITE ); */
    /*
        if ( index->hComm != INVALID_HANDLE_VALUE )
        {
            if ( !SetCommMask( index->hComm, EV_RXCHAR ) )
            {
                YACK();
                report( "eventLoop hung\n" );
            }
            CloseHandle( index->hComm );
        }
        else
        {
            SNPRINTF( message, sizeof(message), sizeof(message), "serial_ close():  Invalid Port Reference for %s\n",
                index->filename );
            report( message );
        }
    */
    if (index->next  && index->prev)
    {
        index->next->prev = index->prev;
        index->prev->next = index->next;
    }
    else if (index->prev)
    {
        index->prev->next = NULL;
    }
    else if (index->next)
    {
        index->next->prev = NULL;
        first_tl = index->next;
    }
    else
    {
        first_tl = NULL;
    }

    if (index->rol.hEvent) { CloseHandle(index->rol.hEvent); }

    if (index->wol.hEvent) { CloseHandle(index->wol.hEvent); }

    if (index->sol.hEvent) { CloseHandle(index->sol.hEvent); }

    if (index->hComm) { CloseHandle(index->hComm); }

    if (index->ttyset) { free(index->ttyset); }

    if (index->astruct) { free(index->astruct); }

    if (index->sstruct) { free(index->sstruct); }

    if (index->sis) { free(index->sis); }

    /* had problems with strdup
    if ( index->filename ) free( index->filename );
    */
    free(index);


    LEAVE("serial_close");
    return 0;
}

/*----------------------------------------------------------
cfmakeraw()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

void cfmakeraw(struct termios *s_termios)
{
    ENTER("cfmakeraw");
    s_termios->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                            | INLCR | IGNCR | ICRNL | IXON);
    s_termios->c_oflag &= ~OPOST;
    s_termios->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    s_termios->c_cflag &= ~(CSIZE | PARENB);
    s_termios->c_cflag |= CS8;
    LEAVE("cfmakeraw");
}

/*----------------------------------------------------------
init_serial_struct()

   accept:
   perform:
   return:
   exceptions:
   win32api:
   comments:
----------------------------------------------------------*/

static BOOL init_serial_struct(struct serial_struct *sstruct)
{
    ENTER("init_serial_struct");

    /*
    FIXME

    This needs to use inb() to read the actual baud_base
    and divisor from the UART registers.  Question is how
    far do we take this?

    */

    sstruct->custom_divisor = 0;
    sstruct->baud_base = 115200;

    /* not currently used check values before using */

    /* unsigned short */

    sstruct->close_delay = 0;
    sstruct->closing_wait = 0;
    sstruct->iomem_reg_shift = 0;

    /* int */

    sstruct->type = 0;
    sstruct->line = 0;
    sstruct->irq = 0;
    sstruct->flags = 0;
    sstruct->xmit_fifo_size = 0;
    sstruct->hub6 = 0;

    /* unsigned int */

    sstruct->port = 0;
    sstruct->port_high = 0;

    /* char */

    sstruct->io_type = 0;

    /* unsigned char * */

    sstruct->iomem_base = NULL;

    LEAVE("init_serial_struct");
    return TRUE;

}
/*----------------------------------------------------------
init_termios()

   accept:
   perform:
   return:
   exceptions:
   win32api:
   comments:
----------------------------------------------------------*/

static BOOL init_termios(struct termios *ttyset)
{
    ENTER("init_termios");

    if (!ttyset)
    {
        return FALSE;
    }

    memset(ttyset, 0, sizeof(struct termios));
    cfsetospeed(ttyset, B9600);
    cfmakeraw(ttyset);
    ttyset->c_cc[VINTR] = 0x03; /* 0: C-c */
    ttyset->c_cc[VQUIT] = 0x1c; /* 1: C-\ */
    ttyset->c_cc[VERASE] = 0x7f;    /* 2: <del> */
    ttyset->c_cc[VKILL] = 0x15; /* 3: C-u */
    ttyset->c_cc[VEOF] = 0x04;  /* 4: C-d */
    ttyset->c_cc[VTIME] = 0;    /* 5: read timeout */
    ttyset->c_cc[VMIN] = 1;     /* 6: read returns after this
                        many bytes */
    ttyset->c_cc[VSUSP] = 0x1a; /* 10: C-z */
    ttyset->c_cc[VEOL] = '\r';  /* 11: */
    ttyset->c_cc[VREPRINT] = 0x12;  /* 12: C-r */
    /*
        ttyset->c_cc[VDISCARD] = 0x;       13: IEXTEN only
    */
    ttyset->c_cc[VWERASE] = 0x17;   /* 14: C-w */
    ttyset->c_cc[VLNEXT] = 0x16;    /* 15: C-w */
    ttyset->c_cc[VEOL2] = '\n'; /* 16: */
    LEAVE("init_termios");
    return TRUE;
    /* default VTIME = 0, VMIN = 1: read blocks forever until one byte */
}

/*----------------------------------------------------------
port_opened()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

static int port_opened(const char *filename)
{
    struct termios_list *index = first_tl;

    ENTER("port_opened");

    if (! index)
    {
        return 0;
    }

    if (!strcmp(index->filename, filename))
    {
        return index->fd;
    }

    while (index->next)
    {
        index = index->next;

        if (!strcmp(index->filename, filename))
        {
            return index->fd;
        }
    }

    LEAVE("port_opened");
    return 0;
}

/*----------------------------------------------------------
open_port()

   accept:
   perform:
   return:
   exceptions:
   win32api:   CreateFile(), SetupComm(), CreateEvent()
   comments:
    FILE_FLAG_OVERLAPPED allows one to break out the select()
    so RXTXPort.close() does not hang.

    The setDTR() and setDSR() are the functions that noticed
    to be blocked in the java close.  Basically ioctl(TIOCM[GS]ET)
    are where it hangs.

    FILE_FLAG_OVERLAPPED also means we need to create valid OVERLAPPED
    structure in Serial_select.
----------------------------------------------------------*/

static int open_port(struct termios_list *port)
{
    ENTER("open_port");
    port->hComm = CreateFile(port->filename,
                             GENERIC_READ | GENERIC_WRITE,
                             0,
                             0,
                             OPEN_EXISTING,
                             FILE_FLAG_OVERLAPPED,
                             0
                            );

    if (port->hComm == INVALID_HANDLE_VALUE)
    {
        YACK();
        set_errno(EINVAL);
        /*
                printf( "serial_open failed %s\n", port->filename );
        */
        return -1;
    }

    if (!SetupComm(port->hComm, 2048, 1024))
    {
        YACK();
        return -1;
    }

    memset(&port->rol, 0, sizeof(OVERLAPPED));
    memset(&port->wol, 0, sizeof(OVERLAPPED));
    memset(&port->sol, 0, sizeof(OVERLAPPED));

    port->rol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!port->rol.hEvent)
    {
        YACK();
        report("Could not create read overlapped\n");
        goto fail;
    }

    port->sol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!port->sol.hEvent)
    {
        YACK();
        report("Could not create select overlapped\n");
        goto fail;
    }

    port->wol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!port->wol.hEvent)
    {
        YACK();
        report("Could not create write overlapped\n");
        goto fail;
    }

    LEAVE("open_port");
    return (0);
fail:
    return (-1);
}

/*----------------------------------------------------------
termios_list()

   accept:       fd which is a fake # for the port assigned when the port
         is opened
   perform:      walk through a double linked list to see if the given
         fd is in any of the termios_list members.
   return:       the termios_list if it is found.
         NULL if no matches are found.
   exceptions:   None
   win32api:     None
   comments:
----------------------------------------------------------*/

static struct termios_list *find_port(int fd)
{

    char message[80];
    struct termios_list *index = first_tl;

    ENTER("find_port");

    if (fd <= 0 || !first_tl) { goto fail; }

    while (index->fd)
    {
        if (index->fd == fd)
        {
            LEAVE("find_port");
            return index;
        }

        if (!index->next)
        {
            break;
        }

        index = index->next;
    }

fail:
    SNPRINTF(message, sizeof(message), "No info known about the port. %i\n", fd);
    report(message);
    set_errno(EBADF);
    LEAVE("find_port");
    return NULL;
}

/*----------------------------------------------------------
get_free_fd()

   accept:
   perform:
   return:
   exceptions:
   win32api:       None
   comments:
----------------------------------------------------------*/

static int get_free_fd()
{
    int next, last;
    struct termios_list *index = first_tl;

    ENTER("get_free_fd");

    if (!index)
    {
        return (1);
    }

    if (!index->fd)
    {
        report("!index->fd\n");
        return (1);
    }

    if (index->fd > 1)
    {
        first_tl = index;
        return (1);
    }

    last = index->fd;

    while (index->next)
    {
        next = index->next->fd;

        if (next !=  last + 1)
        {
            return (last + 1);

        }

        index = index->next;
        last = next;
    }

    LEAVE("get_free_fd");
    return (index->fd + 1);
}

/*----------------------------------------------------------
add_port()

   accept:
   perform:
   return:
   exceptions:
   win32api:      None
   comments:
----------------------------------------------------------*/

static struct termios_list *add_port(const char *filename)
{
    struct termios_list *index = first_tl;
    struct termios_list *port;

    ENTER("add_port");

    port = calloc(1, sizeof(struct termios_list));

    if (!port)
    {
        goto fail;
    }

    memset(port, 0, sizeof(struct termios_list));

    port->ttyset = calloc(1, sizeof(struct termios));

    if (! port->ttyset)
    {
        goto fail;
    }

    memset(port->ttyset, 0, sizeof(struct termios));

    port->sstruct = calloc(1, sizeof(struct serial_struct));

    if (! port->sstruct)
    {
        goto fail;
    }

    memset(port->sstruct, 0, sizeof(struct serial_struct));
    port->sis = calloc(1, sizeof(struct serial_icounter_struct));

    if (! port->sis)
    {
        goto fail;
    }

    memset(port->sis, 0, sizeof(struct serial_icounter_struct));

    /*  FIXME  the async_struct is being defined by mingw32 headers?
        port->astruct = calloc(1, sizeof( struct async_struct ) );
        if( ! port->astruct )
            goto fail;
        memset( port->astruct, 0, sizeof( struct async_struct ) );
    */
    port->MSR = 0;

    strncpy(port->filename, filename, sizeof(port->filename) - 1);

    port->fd = get_free_fd();


    if (!first_tl)
    {
        port->prev = NULL;
        first_tl = port;
    }
    else
    {
        while (index->next)
        {
            index = index->next;
        }

        if (port == first_tl)
        {
            port->prev = NULL;
            port->next = first_tl;
            first_tl->prev = port;
            first_tl = port;
        }
        else
        {
            port->prev = index;
            index->next = port;
        }
    }

    port->next = NULL;
    LEAVE("add_port");
    return port;

fail:
    report("add_port:  Out Of Memory\n");

    if (port->ttyset) { free(port->ttyset); }

    if (port->astruct) { free(port->astruct); }

    if (port->sstruct) { free(port->sstruct); }

    if (port->sis) { free(port->sis); }

    if (port) { free(port); }

    return NULL;
}

/*----------------------------------------------------------
check_port_capabilities()

   accept:
   perform:
   return:
   exceptions:
   win32api:      GetCommProperties(), GetCommState()
   comments:
----------------------------------------------------------*/

static int check_port_capabilities(struct termios_list *index)
{
    COMMPROP cp;
    DCB dcb;
    char message[80];

    ENTER("check_port_capabilities");
    /* check for capabilities */
    GetCommProperties(index->hComm, &cp);

    if (!(cp.dwProvCapabilities & PCF_DTRDSR))
    {
        SNPRINTF(message, sizeof(message),
                 "%s: no DTR & DSR support\n", __func__);
        report(message);
    }

    if (!(cp.dwProvCapabilities & PCF_RLSD))
    {
        SNPRINTF(message, sizeof(message), "%s: no carrier detect (RLSD) support\n",
                 __func__);
        report(message);
    }

    if (!(cp.dwProvCapabilities & PCF_RTSCTS))
    {
        SNPRINTF(message, sizeof(message),
                 "%s: no RTS & CTS support\n", __func__);
        report(message);
    }

    if (!(cp.dwProvCapabilities & PCF_TOTALTIMEOUTS))
    {
        SNPRINTF(message, sizeof(message), "%s: no timeout support\n", __func__);
        report(message);
    }

    if (!GetCommState(index->hComm, &dcb))
    {
        YACK();
        report("GetCommState\n");
        return -1;
    }

    LEAVE("check_port_capabilities");
    return 0;

}

/*----------------------------------------------------------
serial_open()

   accept:
   perform:
   return:
   exceptions:
   win32api:    None
   comments:
----------------------------------------------------------*/

int win32_serial_open(const char *filename, int flags, ...)
{
    struct termios_list *index;
    char message[756];
    char fullfilename[256];

    ENTER("serial_open");

    fullfilename[sizeof(fullfilename) - 1] = '\0';

    /* according to http://support.microsoft.com/kb/115831
     * this is necessary for COM ports larger than COM9 */
    if (memcmp(filename, "\\\\.\\", 4) != 0)
    {
        SNPRINTF(fullfilename, sizeof(fullfilename) - 1, "\\\\.\\%s", filename);
    }
    else
    {
        strncpy(fullfilename, filename, sizeof(fullfilename) - 1);
    }

    if (port_opened(fullfilename))
    {
        report("Port is already opened\n");
        return (-1);
    }

    index = add_port(fullfilename);

    if (!index)
    {
        report("serial_open !index\n");
        return (-1);
    }

    index->interrupt = 0;
    index->tx_happened = 0;

    if (open_port(index))
    {
        SNPRINTF(message, sizeof(message),
                 "serial_open():  Invalid Port Reference for %s\n",
                 fullfilename);
        report(message);
        win32_serial_close(index->fd);
        return -1;
    }

    if (check_port_capabilities(index))
    {
        report("check_port_capabilities!");
        win32_serial_close(index->fd);
        return -1;
    }

    init_termios(index->ttyset);
    init_serial_struct(index->sstruct);

    /* set default condition */
    tcsetattr(index->fd, 0, index->ttyset);

    /* if opened with non-blocking, then operating non-blocking */
    if (flags & O_NONBLOCK)
    {
        index->open_flags = O_NONBLOCK;
    }
    else
    {
        index->open_flags = 0;
    }


    if (!first_tl->hComm)
    {
        SNPRINTF(message, sizeof(message), "open():  Invalid Port Reference for %s\n",
                 index->filename);
        report(message);
    }

    if (first_tl->hComm == INVALID_HANDLE_VALUE)
    {
        report("serial_open: test\n");
    }

    LEAVE("serial_open");
    return (index->fd);
}


/*----------------------------------------------------------
serial_write()

   accept:
   perform:
   return:
   exceptions:
   win32api:     WriteFile(), GetLastError(),
                 WaitForSingleObject(),  GetOverlappedResult(),
                 FlushFileBuffers(), Sleep()
   comments:
----------------------------------------------------------*/

int win32_serial_write(int fd, const char *Str, int length)
{
    unsigned long nBytes;
    struct termios_list *index;
    /* COMSTAT Stat; */
    int old_flag;

    ENTER("serial_write");

    if (fd <= 0)
    {
        return 0;
    }

    index = find_port(fd);

    if (!index)
    {
        LEAVE("serial_write");
        return -1;
    }

    old_flag = index->event_flag;
    /*
        index->event_flag &= ~EV_TXEMPTY;
        SetCommMask( index->hComm, index->event_flag );
        index->tx_happened = 1;
    */
    index->wol.Offset = index->wol.OffsetHigh = 0;
    ResetEvent(index->wol.hEvent);

    if (!WriteFile(index->hComm, Str, length, &nBytes, &index->wol))
    {
        WaitForSingleObject(index->wol.hEvent, 100);

        if (GetLastError() != ERROR_IO_PENDING)
        {
            /* ClearErrors( index, &Stat ); */
            report("serial_write error\n");
            /* report("Condition 1 Detected in write()\n"); */
            YACK();
            errno = EIO;
            nBytes = -1;
            goto end;
        }
        /* This is breaking on Win2K, WinXP for some reason */
        else while (!GetOverlappedResult(index->hComm, &index->wol,
                                             &nBytes, TRUE))
            {
                if (GetLastError() != ERROR_IO_INCOMPLETE)
                {
                    /* report("Condition 2 Detected in write()\n"); */
                    YACK();
                    errno = EIO;
                    nBytes = -1;
                    goto end;
                    /* ClearErrors( index, &Stat ); */
                }
            }
    }
    else
    {
        /* Write finished synchronously.  That is ok!
         * I have seen this with USB to Serial
         * devices like TI's.
         */
    }

end:
    /* FlushFileBuffers( index->hComm ); */
    index->event_flag |= EV_TXEMPTY;
    /* ClearErrors( index, &Stat ); */
    SetCommMask(index->hComm, index->event_flag);
    /* ClearErrors( index, &Stat ); */
    index->event_flag = old_flag;
    index->tx_happened = 1;
    LEAVE("serial_write");
    return nBytes;
}

/*----------------------------------------------------------
serial_read()

   accept:
   perform:
   return:
   exceptions:
   win32api:      ReadFile(), GetLastError(), WaitForSingleObject()
                  GetOverLappedResult()
   comments:    If setting errno make sure not to use EWOULDBLOCK
                In that case use EAGAIN.  See SerialImp.c:testRead()
----------------------------------------------------------*/

int win32_serial_read(int fd, void *vb, int size)
{
    long start, now;
    unsigned long nBytes = 0, total = 0;
    /* unsigned long waiting = 0; */
    int err;
    struct termios_list *index;
//    char message[80];
    COMSTAT stat;
    clock_t c;
    unsigned char *dest = vb;

    start = GetTickCount();
    ENTER("serial_read");

    if (fd <= 0)
    {
        return 0;
    }

    index = find_port(fd);

    if (!index)
    {
        LEAVE("serial_read");
        return -1;
    }

    /* FIXME: CREAD: without this, data cannot be read
       FIXME: PARMRK: mark framing & parity errors
       FIXME: IGNCR: ignore \r
       FIXME: ICRNL: convert \r to \n
       FIXME: INLCR: convert \n to \r
    */

    if (index->open_flags & O_NONBLOCK)
    {
        /* pull mucho-cpu here? */
        do
        {
#ifdef DEBUG_VERBOSE
            report("vmin=0\n");
#endif /* DEBUG_VERBOSE */
            ClearErrors(index, &stat);

            if (stat.cbInQue < index->ttyset->c_cc[VMIN])
            {
                /*
                  hl_usleep(50);
                */
                hl_usleep(100);     /* don't hog the CPU while waiting */

                /* we should use -1 instead of 0 for disabled timeout */
                now = GetTickCount();

                if (index->ttyset->c_cc[VTIME] &&
                        now - start >= (index->ttyset->c_cc[VTIME] * 100))
                {
                    /*
                      SNPRINTF( message, sizeof(message), "now = %i start = %i time = %i total =%i\n", now, start, index->ttyset->c_cc[VTIME]*100, total);
                      report( message );
                    */
                    return total; /* read timeout */
                }
            }
        }
        while (size > 1 && stat.cbInQue < index->ttyset->c_cc[VMIN]);
    }
    else
    {
        /* VTIME is in units of 0.1 seconds */

#ifdef DEBUG_VERBOSE
        report("vmin!=0\n");
#endif /* DEBUG_VERBOSE */
        /* vmin = index->ttyset->c_cc[VMIN]; */

        c = clock() + index->ttyset->c_cc[VTIME] * CLOCKS_PER_SEC / 10;

        do
        {
            ClearErrors(index, &stat);
            hl_usleep(1000);
        }
        while (stat.cbInQue < index->ttyset->c_cc[VMIN] && c > clock());

    }

    total = 0;

    while (size > 0)
    {
        nBytes = 0;
        /* ClearErrors( index, &stat); */

        index->rol.Offset = index->rol.OffsetHigh = 0;
        ResetEvent(index->rol.hEvent);

        err = ReadFile(index->hComm, dest + total, size, &nBytes, &index->rol);
#ifdef DEBUG_VERBOSE
        /* warning Roy Rogers! */
        SNPRINTF(message, sizeof(message), " ========== ReadFile = %i 0x%x\n",
                 (int) nBytes, *((char *) dest + total));
        report(message);
#endif /* DEBUG_VERBOSE */

        if (!err)
        {
            switch (GetLastError())
            {
            case ERROR_BROKEN_PIPE:
                report("ERROR_BROKEN_PIPE\n ");
                nBytes = 0;
                break;

            case ERROR_MORE_DATA:
                /*
                  hl_usleep(1000);
                */
                report("ERROR_MORE_DATA\n");
                break;

            case ERROR_IO_PENDING:
                while (! GetOverlappedResult(
                            index->hComm,
                            &index->rol,
                            &nBytes,
                            TRUE))
                {
                    if (GetLastError() !=
                            ERROR_IO_INCOMPLETE)
                    {
                        ClearErrors(
                            index,
                            &stat);
                        return (total);
                    }
                }

                size -= nBytes;
                total += nBytes;
                return (total);

#if 0

                if (size > 0)
                {
                    now = GetTickCount();
                    SNPRINTF(message, sizeof(message), "size > 0: spent=%ld have=%d\n", now - start,
                             index->ttyset->c_cc[VTIME] * 100);
                    report(message);

                    /* we should use -1 for disabled
                       timouts */
                    if (index->ttyset->c_cc[VTIME]
                            && now - start >= (index->ttyset->c_cc[VTIME] * 100))
                    {
                        report("TO ");
                        /* read timeout */
                        return total;
                    }
                }

                SNPRINTF(message, sizeof(message), "end nBytes=%lu] ", nBytes);
                report(message);
                /*
                  hl_usleep(1000);
                */
                report("ERROR_IO_PENDING\n");
                break;
#endif

            default:
                /*
                  hl_usleep(1000);
                */
                YACK();
                return -1;
            }
        }
        else
        {
            size -= nBytes;
            total += nBytes;

            /*
              hl_usleep(1000);
            */
            ClearErrors(index, &stat);
            return (total);
        }
    }

    LEAVE("serial_read");
    return total;
}

#ifdef asdf
int win32_serial_read(int fd, void *vb, int size)
{
    long start, now;
    unsigned long nBytes = 0, total = 0, error;
    /* unsigned long waiting = 0; */
    int err, vmin;
    struct termios_list *index;
    char message[80];
    COMSTAT Stat;
    clock_t c;
    unsigned char *dest = vb;

    start = GetTickCount();
    ENTER("serial_read");

    if (fd <= 0)
    {
        printf("1\n");
        return 0;
    }

    index = find_port(fd);

    if (!index)
    {
        LEAVE("serial_read 7");
        errno = EIO;
        printf("2\n");
        return -1;
    }

    /* FIXME: CREAD: without this, data cannot be read
       FIXME: PARMRK: mark framing & parity errors
       FIXME: IGNCR: ignore \r
       FIXME: ICRNL: convert \r to \n
       FIXME: INLCR: convert \n to \r
    */

    ClearErrors(index, &Stat);

    if (index->open_flags & O_NONBLOCK)
    {
        vmin = 0;

        /* pull mucho-cpu here? */
        do
        {
#ifdef DEBUG_VERBOSE
            report("vmin=0\n");
#endif /* DEBUG_VERBOSE */
            ClearErrors(index, &Stat);
            /*
                        hl_usleep(1000);
                        hl_usleep(50);
            */
            /* we should use -1 instead of 0 for disabled timeout */
            now = GetTickCount();

            if (index->ttyset->c_cc[VTIME] &&
                    now - start >= (index->ttyset->c_cc[VTIME] * 100))
            {
                /*
                                SNPRINTF( message, sizeof(message), "now = %i start = %i time = %i total =%i\n", now, start, index->ttyset->c_cc[VTIME]*100, total);
                                report( message );
                */
                errno = EAGAIN;
                printf("3\n");
                return -1;  /* read timeout */
            }
        }
        while (Stat.cbInQue < size && size > 1);
    }
    else
    {
        /* VTIME is in units of 0.1 seconds */

#ifdef DEBUG_VERBOSE
        report("vmin!=0\n");
#endif /* DEBUG_VERBOSE */
        vmin = index->ttyset->c_cc[VMIN];

        c = clock() + index->ttyset->c_cc[VTIME] * CLOCKS_PER_SEC / 10;

        do
        {
            error = ClearErrors(index, &Stat);
            hl_usleep(1000);
        }
        while (c > clock());

    }

    total = 0;

    while (size > 0)
    {
        nBytes = 0;
        /* ClearErrors( index, &Stat); */

        index->rol.Offset = index->rol.OffsetHigh = 0;
        ResetEvent(index->rol.hEvent);

        err = ReadFile(index->hComm, dest + total, size, &nBytes, &index->rol);
#ifdef DEBUG_VERBOSE
        /* warning Roy Rogers! */
        SNPRINTF(message, sizeof(message), " ========== ReadFile = %i %s\n",
                 (int) nBytes, (char *) dest + total);
        report(message);
#endif /* DEBUG_VERBOSE */

        if (!err)
        {
            switch (GetLastError())
            {
            case ERROR_BROKEN_PIPE:
                report("ERROR_BROKEN_PIPE\n ");
                nBytes = 0;
                break;

            case ERROR_MORE_DATA:
                /*
                                    hl_usleep(1000);
                */
                report("ERROR_MORE_DATA\n");
                break;

            case ERROR_IO_PENDING:
                while (! GetOverlappedResult(
                            index->hComm,
                            &index->rol,
                            &nBytes,
                            TRUE))
                {
                    if (GetLastError() !=
                            ERROR_IO_INCOMPLETE)
                    {
                        ClearErrors(
                            index,
                            &Stat);
                        printf("4\n");
                        return (total);
                    }
                }

                size -= nBytes;
                total += nBytes;
                return (total);
#if 0

                if (size > 0)
                {
                    now = GetTickCount();
                    SNPRINTF(message, sizeof(message), "size > 0: spent=%ld have=%d\n", now - start,
                             index->ttyset->c_cc[VTIME] * 100);
                    report(message);

                    /* we should use -1 for disabled
                       timouts */
                    if (index->ttyset->c_cc[VTIME]
                            && now - start >= (index->ttyset->c_cc[VTIME] * 100))
                    {
                        report("TO ");
                        /* read timeout */
                        printf("5\n");
                        return total;
                    }
                }

                SNPRINTF(message, sizeof(message), "end nBytes=%ld] ", nBytes);
                report(message);
                /*
                                    hl_usleep(1000);
                */
                report("ERROR_IO_PENDING\n");
                break;
#endif

            default:
                /*
                                    hl_usleep(1000);
                */
                YACK();
                errno = EIO;
                printf("6\n");
                return -1;
            }
        }
        else
        {
            size -= nBytes;
            total += nBytes;

            /*
                        hl_usleep(1000);
            */
            ClearErrors(index, &Stat);
            printf("7\n");
            return (total);
        }
    }

    LEAVE("serial_read");
    ClearErrors(index, &Stat);
    return total;
}
#endif /* asdf */

/*----------------------------------------------------------
cfsetospeed()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

int cfsetospeed(struct termios *s_termios, speed_t speed)
{
    char message[80];
    ENTER("cfsetospeed");
    /* clear baudrate */
    s_termios->c_cflag &= ~CBAUD;

    if (speed & ~CBAUD)
    {
        SNPRINTF(message, sizeof(message), "cfsetospeed: not speed: %#o\n", speed);
        report(message);
        /* continue assuming its a custom baudrate */
        s_termios->c_cflag |= B38400;  /* use 38400 during custom */
        s_termios->c_cflag |= CBAUDEX; /* use CBAUDEX for custom */
    }
    else if (speed)
    {
        s_termios->c_cflag |= speed;
    }
    else
    {
        /* PC blows up with speed 0 handled in Java */
        s_termios->c_cflag |= B9600;
    }

    s_termios->c_ispeed = s_termios->c_ospeed = speed;
    LEAVE("cfsetospeed");
    return 1;
}

/*----------------------------------------------------------
cfsetispeed()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

int cfsetispeed(struct termios *s_termios, speed_t speed)
{
    return cfsetospeed(s_termios, speed);
}

/*----------------------------------------------------------
cfsetspeed()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

int cfsetspeed(struct termios *s_termios, speed_t speed)
{
    return cfsetospeed(s_termios, speed);
}

/*----------------------------------------------------------
cfgetospeed()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

speed_t cfgetospeed(struct termios *s_termios)
{
    ENTER("cfgetospeed");
    return s_termios->c_ospeed;
}

/*----------------------------------------------------------
cfgetispeed()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

speed_t cfgetispeed(struct termios *s_termios)
{
    ENTER("cfgetospeed");
    return s_termios->c_ispeed;
}

/*----------------------------------------------------------
serial_struct_to_DCB()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/
int serial_struct_to_DCB(struct serial_struct *sstruct, DCB *dcb)
{
    /* 5 Baud rate fix
    sstruct.baud_base
    sstruct.custom_divisor = ( sstruct.baud_base/cspeed );
    */
    return (0);
}

/*----------------------------------------------------------
termios_to_DCB()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/
static int termios_to_DCB(struct termios *s_termios, DCB *dcb)
{
    ENTER("termios_to_DCB");

    if (!(s_termios->c_cflag & CBAUDEX))
    {
        s_termios->c_ispeed = s_termios->c_ospeed = s_termios->c_cflag & CBAUD;
    }

    dcb->BaudRate        = B_to_CBR(s_termios->c_ispeed);
    dcb->ByteSize = termios_to_bytesize(s_termios->c_cflag);

    if (s_termios->c_cflag & PARENB)
    {
        if (s_termios->c_cflag & PARODD
                && s_termios->c_cflag & CMSPAR)
        {
            dcb->Parity = MARKPARITY;
        }
        else if (s_termios->c_cflag & PARODD)
        {
            dcb->Parity = ODDPARITY;
        }
        else if (s_termios->c_cflag & CMSPAR)
        {
            dcb->Parity = SPACEPARITY;
        }
        else
        {
            dcb->Parity = EVENPARITY;
        }
    }
    else
    {
        dcb->Parity = NOPARITY;
    }

    if (s_termios->c_cflag & CSTOPB) { dcb->StopBits = TWOSTOPBITS; }
    else { dcb->StopBits = ONESTOPBIT; }

    if (s_termios->c_cflag & HARDWARE_FLOW_CONTROL)
    {
        dcb->fRtsControl = RTS_CONTROL_HANDSHAKE;
        dcb->fOutxCtsFlow = TRUE;
    }
    else
    {
        dcb->fRtsControl = RTS_CONTROL_DISABLE;
        dcb->fOutxCtsFlow = FALSE;
    }

    LEAVE("termios_to_DCB");
    return 0;
}

/*----------------------------------------------------------
DCB_to_serial_struct()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/
int DCB_to_serial_struct(DCB *dcb, struct serial_struct *sstruct)
{
    return (0);
}
/*----------------------------------------------------------
DCB_to_termios()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/
static void DCB_to_termios(DCB *dcb, struct termios *s_termios)
{
    ENTER("DCB_to_termios");
    s_termios->c_ispeed = CBR_to_B(dcb->BaudRate);
    s_termios->c_ospeed = s_termios->c_ispeed;
    s_termios->c_cflag |= s_termios->c_ispeed & CBAUD;
    LEAVE("DCB_to_termios");
}

/*----------------------------------------------------------
show_DCB()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/
static void show_DCB(DCB myDCB)
{

#ifdef DEBUG_HOSED
    char message[80];

    SNPRINTF(message, sizeof(message), "DCBlength: %ld\n", myDCB.DCBlength);
    report(message);
    SNPRINTF(message, sizeof(message), "BaudRate: %ld\n", myDCB.BaudRate);
    report(message);

    if (myDCB.fBinary)
    {
        report("fBinary\n");
    }

    if (myDCB.fParity)
    {
        report("fParity: ");

        if (myDCB.fErrorChar)
        {
            SNPRINTF(message, sizeof(message), "fErrorChar: %#x\n", myDCB.ErrorChar);
            report(message);
        }
        else
        {
            report("fErrorChar == false\n");
        }
    }

    if (myDCB.fOutxCtsFlow)
    {
        report("fOutxCtsFlow\n");
    }

    if (myDCB.fOutxDsrFlow)
    {
        report("fOutxDsrFlow\n");
    }

    if (myDCB.fDtrControl & DTR_CONTROL_HANDSHAKE)
    {
        report("DTR_CONTROL_HANDSHAKE\n");
    }

    if (myDCB.fDtrControl & DTR_CONTROL_ENABLE)
    {
        report("DTR_CONTROL_ENABLE\n");
    }

    if (myDCB.fDtrControl & DTR_CONTROL_DISABLE)
    {
        report("DTR_CONTROL_DISABLE\n");
    }

    if (myDCB.fDsrSensitivity)
    {
        report("fDsrSensitivity\n");
    }

    if (myDCB.fTXContinueOnXoff)
    {
        report("fTXContinueOnXoff\n");
    }

    if (myDCB.fOutX)
    {
        report("fOutX\n");
    }

    if (myDCB.fInX)
    {
        report("fInX\n");
    }

    if (myDCB.fNull)
    {
        report("fNull\n");
    }

    if (myDCB.fRtsControl & RTS_CONTROL_TOGGLE)
    {
        report("RTS_CONTROL_TOGGLE\n");
    }

    if (myDCB.fRtsControl == 0)
    {
        report("RTS_CONTROL_HANDSHAKE ( fRtsControl==0 )\n");
    }

    if (myDCB.fRtsControl & RTS_CONTROL_HANDSHAKE)
    {
        report("RTS_CONTROL_HANDSHAKE\n");
    }

    if (myDCB.fRtsControl & RTS_CONTROL_ENABLE)
    {
        report("RTS_CONTROL_ENABLE\n");
    }

    if (myDCB.fRtsControl & RTS_CONTROL_DISABLE)
    {
        report("RTS_CONTROL_DISABLE\n");
    }

    if (myDCB.fAbortOnError)
    {
        report("fAbortOnError\n");
    }

    SNPRINTF(message, sizeof(message), "XonLim: %d\n", myDCB.XonLim);
    report(message);
    SNPRINTF(message, sizeof(message), "XoffLim: %d\n", myDCB.XoffLim);
    report(message);
    SNPRINTF(message, sizeof(message), "ByteSize: %d\n", myDCB.ByteSize);
    report(message);

    switch (myDCB.Parity)
    {
    case EVENPARITY:
        report("EVENPARITY");
        break;

    case MARKPARITY:
        report("MARKPARITY");
        break;

    case NOPARITY:
        report("NOPARITY");
        break;

    case ODDPARITY:
        report("ODDPARITY");
        break;

    default:
        SNPRINTF(message, sizeof(message),
                 "unknown Parity (%#x ):", myDCB.Parity);
        report(message);
        break;
    }

    report("\n");

    switch (myDCB.StopBits)
    {
    case ONESTOPBIT:
        report("ONESTOPBIT");
        break;

    case ONE5STOPBITS:
        report("ONE5STOPBITS");
        break;

    case TWOSTOPBITS:
        report("TWOSTOPBITS");
        break;

    default:
        report("unknown StopBits (%#x ):", myDCB.StopBits);
        break;
    }

    report("\n");
    SNPRINTF(message, sizeof(message),  "XonChar: %#x\n", myDCB.XonChar);
    report(message);
    SNPRINTF(message, sizeof(message), "XoffChar: %#x\n", myDCB.XoffChar);
    report(message);
    SNPRINTF(message, sizeof(message), "EofChar: %#x\n", myDCB.EofChar);
    report(message);
    SNPRINTF(message, sizeof(message), "EvtChar: %#x\n", myDCB.EvtChar);
    report(message);
    report("\n");
#endif /* DEBUG_HOSED */
}

/*----------------------------------------------------------
tcgetattr()

   accept:
   perform:
   return:
   exceptions:
   win32api:    GetCommState(), GetCommTimeouts()
   comments:
----------------------------------------------------------*/

int tcgetattr(int fd, struct termios *s_termios)
{
    DCB myDCB;
    COMMTIMEOUTS timeouts;
    struct termios_list *index;
    char message[80];

    ENTER("tcgetattr");

    if (fd <= 0)
    {
        return 0;
    }

    index = find_port(fd);

    if (!index)
    {
        LEAVE("tcgetattr");
        return -1;
    }

    if (!GetCommState(index->hComm, &myDCB))
    {
        SNPRINTF(message, sizeof(message), "GetCommState failed\n");
        report(message);
        return -1;
    }

    memcpy(s_termios, index->ttyset, sizeof(struct termios));

    show_DCB(myDCB);

    /***** input mode flags (c_iflag ) ****/
    /* parity check enable */
    if (myDCB.fParity)
    {
        s_termios->c_iflag |= INPCK;
        s_termios->c_iflag &= ~IGNPAR;
    }
    else
    {
        s_termios->c_iflag &= ~INPCK;
        s_termios->c_iflag |= IGNPAR;
    }

    /* FIXME: IGNBRK: ignore break */
    /* FIXME: BRKINT: interrupt on break */

    if (myDCB.fOutX)
    {
        s_termios->c_iflag |= IXON;
    }
    else
    {
        /* IXON: output start/stop control */
        s_termios->c_iflag &= ~IXON;
    }

    if (myDCB.fInX)
    {
        s_termios->c_iflag |= IXOFF;
    }
    else
    {
        /* IXOFF: input start/stop control */
        s_termios->c_iflag &= ~IXOFF;
    }

    if (myDCB.fTXContinueOnXoff)
    {
        s_termios->c_iflag |= IXANY;
    }
    else
    {
        /* IXANY: any char restarts output */
        s_termios->c_iflag &= ~IXANY;
    }

    /* FIXME: IMAXBEL: if input buffer full, send bell */

    /***** control mode flags (c_cflag ) *****/
    /* FIXME: CLOCAL: DONT send SIGHUP on modem disconnect */
    /* FIXME: HUPCL: generate modem disconnect when all has closed or
        exited */
    /* CSTOPB two stop bits ( otherwise one) */
    if (myDCB.StopBits == TWOSTOPBITS)
    {
        s_termios->c_cflag |= CSTOPB;
    }

    if (myDCB.StopBits == ONESTOPBIT)
    {
        s_termios->c_cflag &= ~CSTOPB;
    }

    /* PARENB enable parity bit */
    s_termios->c_cflag &= ~(PARENB | PARODD | CMSPAR);
    myDCB.fParity = 1;

#if 0 // redundant

    if (myDCB.fParity)
    {
#endif
        report("tcgetattr getting parity\n");
        s_termios->c_cflag |= PARENB;

        if (myDCB.Parity == MARKPARITY)
        {
            s_termios->c_cflag |= (PARODD | CMSPAR);
        }
        else if (myDCB.Parity == SPACEPARITY)
        {
            s_termios->c_cflag |= CMSPAR;
        }
        else if (myDCB.Parity == ODDPARITY)
        {
            report("ODDPARITY\n");
            s_termios->c_cflag |= PARODD;
        }
        else if (myDCB.Parity == EVENPARITY)
        {
            report("EVENPARITY\n");
            s_termios->c_cflag &= ~PARODD;
        }
        else if (myDCB.Parity == NOPARITY)
        {
            s_termios->c_cflag &= ~(PARODD | CMSPAR | PARENB);
        }

#if 0 // see redundant above
    }
    else
    {
        s_termios->c_cflag &= ~PARENB;
    }

#endif

    /* CSIZE */
    s_termios->c_cflag |= bytesize_to_termios(myDCB.ByteSize);

    /* HARDWARE_FLOW_CONTROL: hardware flow control */
    if ((myDCB.fOutxCtsFlow == TRUE) ||
            (myDCB.fRtsControl == RTS_CONTROL_HANDSHAKE))
    {
        s_termios->c_cflag |= HARDWARE_FLOW_CONTROL;
    }
    else
    {
        s_termios->c_cflag &= ~HARDWARE_FLOW_CONTROL;
    }

    /* MDMBUF: carrier based flow control of output */
    /* CIGNORE: tcsetattr will ignore control modes & baudrate */

    /***** NOT SUPPORTED: local mode flags (c_lflag) *****/
    /* ICANON: canonical (not raw) mode */
    /* ECHO: echo back to terminal */
    /* ECHOE: echo erase */
    /* ECHOPRT: hardcopy echo erase */
    /* ECHOK: show KILL char */
    /* ECHOKE: BSD ECHOK */
    /* ECHONL: ICANON only: echo newline even with no ECHO */
    /* ECHOCTL: if ECHO, then control-A are printed as '^A' */
    /* ISIG: recognize INTR, QUIT & SUSP */
    /* IEXTEN: implementation defined */
    /* NOFLSH: dont clear i/o queues on INTR, QUIT or SUSP */
    /* TOSTOP: background process generate SIGTTOU */
    /* ALTWERASE: alt-w erase distance */
    /* FLUSHO: user DISCARD char */
    /* NOKERNINFO: disable STATUS char */
    /* PENDIN: input line needsd reprinting, set by REPRINT char */
    /***** END - NOT SUPPORTED *****/

    /***** control characters (c_cc[NCCS] ) *****/

    if (!GetCommTimeouts(index->hComm, &timeouts))
    {
        YACK();
        report("GetCommTimeouts\n");
        return -1;
    }

    s_termios->c_cc[VTIME] = timeouts.ReadTotalTimeoutConstant / 100;
    /*
        handled in SerialImp.c?
        s_termios->c_cc[VMIN] = ?
    */

    s_termios->c_cc[VSTART] = myDCB.XonChar;
    s_termios->c_cc[VSTOP] = myDCB.XoffChar;
    s_termios->c_cc[VEOF] = myDCB.EofChar;

#ifdef DEBUG_VERBOSE
    SNPRINTF(message, sizeof(message),
             "tcgetattr: VTIME:%d, VMIN:%d\n", s_termios->c_cc[VTIME],
             s_termios->c_cc[VMIN]);
    report(message);
#endif /* DEBUG_VERBOSE */

    /***** line discipline ( c_line ) ( == c_cc[33] ) *****/

    DCB_to_termios(&myDCB, s_termios);   /* baudrate */
    LEAVE("tcgetattr");
    return 0;
}

/*
    `TCSANOW'
        Make the change immediately.

    `TCSADRAIN'
        Make the change after waiting until all queued output has
        been written.  You should usually use this option when
        changing parameters that affect output.

    `TCSAFLUSH'
        This is like `TCSADRAIN', but also discards any queued input.

    `TCSASOFT'
        This is a flag bit that you can add to any of the above
        alternatives.  Its meaning is to inhibit alteration of the
        state of the terminal hardware.  It is a BSD extension; it is
        only supported on BSD systems and the GNU system.

        Using `TCSASOFT' is exactly the same as setting the `CIGNORE'
        bit in the `c_cflag' member of the structure TERMIOS-P points
        to.  *Note Control Modes::, for a description of `CIGNORE'.
*/

/*----------------------------------------------------------
tcsetattr()

   accept:
   perform:
   return:
   exceptions:
   win32api:     GetCommState(), GetCommTimeouts(), SetCommState(),
                 SetCommTimeouts()
   comments:
----------------------------------------------------------*/
int tcsetattr(int fd, int when, struct termios *s_termios)
{
    int vtime;
    DCB dcb;
    COMMTIMEOUTS timeouts;
    struct termios_list *index;

    ENTER("tcsetattr");

    if (fd <= 0)
    {
        return 0;
    }

    index = find_port(fd);

    if (!index)
    {
        LEAVE("tcsetattr");
        return -1;
    }

    fflush(stdout);

    if (s_termios->c_lflag & ICANON)
    {
        report("tcsetattr: no canonical mode support\n");
        /* and all other c_lflags too */
        return -1;
    }

    if (!GetCommState(index->hComm, &dcb))
    {
        YACK();
        report("tcsetattr:GetCommState\n");
        return -1;
    }

    if (!GetCommTimeouts(index->hComm, &timeouts))
    {
        YACK();
        report("tcsetattr:GetCommTimeouts\n");
        return -1;
    }

    /*** control flags, c_cflag **/
    if (!(s_termios->c_cflag & CIGNORE))
    {
        dcb.fParity = 1;

        /* CIGNORE: ignore control modes and baudrate */
        /* baudrate */
        if (termios_to_DCB(s_termios, &dcb) < 0) { return -1; }
    }
    else
    {
    }

    /*** input flags, c_iflag **/
    /*  This is wrong.  It disables Parity  FIXME
        if( ( s_termios->c_iflag & INPCK ) && !( s_termios->c_iflag & IGNPAR ) )
        {
            dcb.fParity = TRUE;
        } else
        {
            dcb.fParity = FALSE;
        }
    */
    /* not in win95?
       Some years later...
       eww..  FIXME This is used for changing the Parity
       error character

       I think this code is hosed.  See VEOF below

       Trent
    */

    if (s_termios->c_iflag & ISTRIP) { dcb.fBinary = FALSE; }
    /* ISTRIP: strip to seven bits */
    else { dcb.fBinary = TRUE; }

    /* FIXME: IGNBRK: ignore break */
    /* FIXME: BRKINT: interrupt on break */
    if (s_termios->c_iflag & IXON)
    {
        dcb.fOutX = TRUE;
    }
    else
    {
        dcb.fOutX = FALSE;
    }

    if (s_termios->c_iflag & IXOFF)
    {
        dcb.fInX = TRUE;
    }
    else
    {
        dcb.fInX = FALSE;
    }

    dcb.fTXContinueOnXoff = (s_termios->c_iflag & IXANY) ? TRUE : FALSE;
    /* FIXME: IMAXBEL: if input buffer full, send bell */

    /* no DTR control in termios? */
    dcb.fDtrControl     = DTR_CONTROL_DISABLE;
    /* no DSR control in termios? */
    dcb.fOutxDsrFlow    = FALSE;
    /* DONT ignore rx bytes when DSR is OFF */
    dcb.fDsrSensitivity = FALSE;
    dcb.XonChar         = s_termios->c_cc[VSTART];
    dcb.XoffChar        = s_termios->c_cc[VSTOP];
    dcb.XonLim          = 0;    /* ? */
    dcb.XoffLim         = 0;    /* ? */
    dcb.EofChar         = s_termios->c_cc[VEOF];

    if (dcb.EofChar != '\0')
    {
        dcb.fBinary = FALSE;
    }
    else
    {
        dcb.fBinary = TRUE;
    }

    if (EV_BREAK | EV_CTS | EV_DSR | EV_ERR | EV_RING | (EV_RLSD & EV_RXFLAG))
    {
        dcb.EvtChar = '\n';
    }
    else
    {
        dcb.EvtChar = '\0';
    }

    if (!SetCommState(index->hComm, &dcb))
    {
        report("SetCommState error\n");
        YACK();
        return -1;
    }

#ifdef DEBUG_VERBOSE
    {
        char message[32];
        SNPRINTF(message, sizeof(message), "VTIME:%d, VMIN:%d\n",
                 s_termios->c_cc[VTIME],
                 s_termios->c_cc[VMIN]);
        report(message);
    }
#endif /* DEBUG_VERBOSE */
    vtime = s_termios->c_cc[VTIME] * 100;
    timeouts.ReadTotalTimeoutConstant = vtime;
    timeouts.ReadIntervalTimeout = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;

    timeouts.WriteTotalTimeoutConstant = vtime;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    /* max between bytes */
    if (s_termios->c_cc[VMIN] > 0 && vtime > 0)
    {
        /* read blocks forever on VMIN chars */
    }
    else if (s_termios->c_cc[VMIN] == 0 && vtime == 0)
    {
        /* read returns immediately */
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.ReadTotalTimeoutMultiplier = 0;
    }

#ifdef DEBUG_VERBOSE
    {
        char message[64];
        SNPRINTF(message, sizeof(message), "ReadIntervalTimeout=%ld\n",
                 timeouts.ReadIntervalTimeout);
        report(message);
        SNPRINTF(message, sizeof(message), "c_cc[VTIME] = %d, c_cc[VMIN] = %d\n",
                 s_termios->c_cc[VTIME], s_termios->c_cc[VMIN]);
        report(message);
        SNPRINTF(message, sizeof(message), "ReadTotalTimeoutConstant: %ld\n",
                 timeouts.ReadTotalTimeoutConstant);
        report(message);
        SNPRINTF(message, sizeof(message), "ReadIntervalTimeout : %ld\n",
                 timeouts.ReadIntervalTimeout);
        report(message);
        SNPRINTF(message, sizeof(message), "ReadTotalTimeoutMultiplier: %ld\n",
                 timeouts.ReadTotalTimeoutMultiplier);
        report(message);
    }
#endif /* DEBUG_VERBOSE */

    if (!SetCommTimeouts(index->hComm, &timeouts))
    {
        YACK();
        report("SetCommTimeouts\n");
        return -1;
    }

    memcpy(index->ttyset, s_termios, sizeof(struct termios));
    LEAVE("tcsetattr");
    return 0;
}

/*----------------------------------------------------------
tcsendbreak()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
        break for duration*0.25 seconds or
        0.25 seconds if duration = 0.
----------------------------------------------------------*/

int tcsendbreak(int fd, int duration)
{
    struct termios_list *index;
    COMSTAT Stat;

    ENTER("tcsendbreak");

    index = find_port(fd);

    if (!index)
    {
        LEAVE("tcdrain");
        return -1;
    }

    if (duration <= 0) { duration = 1; }

    if (!SetCommBreak(index->hComm))
    {
        ClearErrors(index, &Stat);
    }

    /* 0.25 seconds == 250000 usec */
    hl_usleep(duration * 250000);

    if (!ClearCommBreak(index->hComm))
    {
        ClearErrors(index, &Stat);
    }

    LEAVE("tcsendbreak");
    return 1;
}

/*----------------------------------------------------------
tcdrain()

   accept:       file descriptor
   perform:      wait for output to be written.
   return:       0 on success, -1 otherwise
   exceptions:   None
   win32api:     FlushFileBuffers
   comments:
----------------------------------------------------------*/

int tcdrain(int fd)
{
    struct termios_list *index;
    char message[80];
    int old_flag;

    ENTER("tcdrain");
    index = find_port(fd);

    if (!index)
    {
        LEAVE("tcdrain");
        return -1;
    }

    old_flag = index->event_flag;

    /*
        index->event_flag &= ~EV_TXEMPTY;
        SetCommMask( index->hComm, index->event_flag );
        index->tx_happened = 1;
    */
    if (!FlushFileBuffers(index->hComm))
    {
        /* FIXME  Need to figure out what the various errors are in
                  windows.  YACK() should report them and we can
              handle them as we find them


              Something funky is happening on NT.  GetLastError =
              0.
        */
        SNPRINTF(message, sizeof(message),  "FlushFileBuffers() %i\n",
                 (int) GetLastError());
        report(message);

        if (GetLastError() == 0)
        {
            set_errno(0);
            return (0);
        }

        set_errno(EAGAIN);
        YACK();
        LEAVE("tcdrain");
        return -1;
    }

    /*
        SNPRINTF( message, sizeof(message),  "FlushFileBuffers() %i\n",
            (int) GetLastError() );
        report( message );
    */
    LEAVE("tcdrain success");
    index->event_flag |= EV_TXEMPTY;
    SetCommMask(index->hComm, index->event_flag);
    index->event_flag = old_flag;
    /*
        index->tx_happened = 1;
    */
    return 0;
}

/*----------------------------------------------------------
tcflush()

   accept:       file descriptor, queue_selector
   perform:      discard data not transmitted or read
         TCIFLUSH:  flush data not read
         TCOFLUSH:  flush data not transmitted
         TCIOFLUSH: flush both
   return:       0 on success, -1 on error
   exceptions:   none
   win32api:     PurgeComm
   comments:
----------------------------------------------------------*/

int tcflush(int fd, int queue_selector)
{
    struct termios_list *index;
    int old_flag;

    ENTER("tcflush");

    index = find_port(fd);

    if (!index)
    {
        LEAVE("tclflush");
        return (-1);
    }

    old_flag = index->event_flag;
    /*
        index->event_flag &= ~EV_TXEMPTY;
        SetCommMask( index->hComm, index->event_flag );
        index->tx_happened = 1;
    */

    index->tx_happened = 1;

    switch (queue_selector)
    {
    case TCIFLUSH:
        if (!PurgeComm(index->hComm, PURGE_RXABORT | PURGE_RXCLEAR))
        {
            goto fail;
        }

        break;

    case TCOFLUSH:
        if (!PurgeComm(index->hComm, PURGE_TXABORT | PURGE_TXCLEAR))
        {
            goto fail;
        }

        break;

    case TCIOFLUSH:
        if (!PurgeComm(index->hComm, PURGE_TXABORT | PURGE_TXCLEAR))
        {
            goto fail;
        }

        if (!PurgeComm(index->hComm, PURGE_RXABORT | PURGE_RXCLEAR))
        {
            goto fail;
        }

        break;

    default:
        /*
                    set_errno( ENOTSUP );
        */
        report("tcflush: Unknown queue_selector\n");
        LEAVE("tcflush");
        return -1;
    }

    index->event_flag |= EV_TXEMPTY;
    SetCommMask(index->hComm, index->event_flag);
    index->event_flag = old_flag;
    index->tx_happened = 1;
    LEAVE("tcflush");
    return (0);

    /* FIXME  Need to figure out what the various errors are in
              windows.  YACK() should report them and we can
          handle them as we find them
    */

fail:
    LEAVE("tcflush");
    set_errno(EAGAIN);
    YACK();
    return -1;

}

/*----------------------------------------------------------
tcflow()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:   FIXME
----------------------------------------------------------*/

int tcflow(int fd, int action)
{
    ENTER("tcflow");

    switch (action)
    {
    /* Suspend transmission of output */
    case TCOOFF: break;

    /* Restart transmission of output */
    case TCOON: break;

    /* Transmit a STOP character */
    case TCIOFF: break;

    /* Transmit a START character */
    case TCION: break;

    default: return -1;
    }

    LEAVE("tcflow");
    return 1;
}
/*----------------------------------------------------------
fstat()

   accept:
   perform:
   return:
   exceptions:
   win32api:
   comments:  this is just to keep the eventLoop happy.
----------------------------------------------------------*/

#if 0
int fstat(int fd, ...)
{
    return (0);
}
#endif

/*----------------------------------------------------------
ioctl()

   accept:
   perform:
   return:
   exceptions:
   win32api:     GetCommError(), GetCommModemStatus, EscapeCommFunction()
   comments:  FIXME
    the DCB struct is:

    typedef struct _DCB
    {
        unsigned long DCBlength, BaudRate, fBinary:1, fParity:1;
        unsigned long fOutxCtsFlow:1, fOutxDsrFlow:1, fDtrControl:2;
        unsigned long fDsrSensitivity:1, fTXContinueOnXoff:1;
        unsigned long fOutX:1, fInX:1, fErrorChar:1, fNull:1;
        unsigned long fRtsControl:2, fAbortOnError:1, fDummy2:17;
        WORD wReserved, XonLim, XoffLim;
        BYTE ByteSize, Parity, StopBits;
        char XonChar, XoffChar, ErrorChar, EofChar, EvtChar;
        WORD wReserved1;
    } DCB;

----------------------------------------------------------*/

int win32_serial_ioctl(int fd, int request, ...)
{
    unsigned long dwStatus = 0;
    va_list ap;
    int *arg, ret, old_flag;
    char message[80];

#ifdef TIOCGSERIAL
    DCB *dcb;
    struct serial_struct *sstruct;
#endif /* TIOCGSERIAL */
    COMSTAT Stat;

    struct termios_list *index;
#ifdef TIOCGICOUNT
    struct serial_icounter_struct *sistruct;
#endif  /* TIOCGICOUNT */

    ENTER("ioctl");

    if (fd <= 0)
    {
        return 0;
    }

    index = find_port(fd);

    if (!index)
    {
        LEAVE("ioctl");
        return -1;
    }

    va_start(ap, request);

    ret = ClearErrors(index, &Stat);

    if (ret == 0)
    {
        set_errno(EBADFD);
        YACK();
        report("ClearError Failed! ernno EBADFD");
        arg = va_arg(ap, int *);
        va_end(ap);
        return -1;
    }

    switch (request)
    {
    case TCSBRK:
        arg = va_arg(ap, int *);
        va_end(ap);
        return -ENOIOCTLCMD;

    case TCSBRKP:
        arg = va_arg(ap, int *);
        va_end(ap);
        return -ENOIOCTLCMD;

    case TIOCGSOFTCAR:
        arg = va_arg(ap, int *);
        va_end(ap);
        return -ENOIOCTLCMD;

    case TIOCSSOFTCAR:
        arg = va_arg(ap, int *);
        va_end(ap);
        return -ENOIOCTLCMD;

    case TIOCCBRK:
    case TIOCSBRK:
        arg = va_arg(ap, int *);

        if (EscapeCommFunction(index->hComm,
                               (request == TIOCSBRK) ? SETBREAK :
                               CLRBREAK))
        {
            report("EscapeCommFunction: True\n");
        }
        else
        {
            report("EscapeCommFunction: False\n");
        }

        break;

    case TIOCMGET:
        arg = va_arg(ap, int *);

        /* DORITOS */
        if (!GetCommModemStatus(index->hComm, &dwStatus))
        {
            report_error("GetCommMOdemStatus failed!\n");
        }

        if (dwStatus & MS_RLSD_ON) { *arg |= TIOCM_CAR; }
        else { *arg &= ~TIOCM_CAR; }

        if (dwStatus & MS_RING_ON) { *arg |= TIOCM_RNG; }
        else { *arg &= ~TIOCM_RNG; }

        if (dwStatus & MS_DSR_ON) { *arg |= TIOCM_DSR; }
        else { *arg &= ~TIOCM_DSR; }

        if (dwStatus & MS_CTS_ON) { *arg |= TIOCM_CTS; }
        else { *arg &= ~TIOCM_CTS; }

        /*  I'm not seeing a way to read the MSR directly
            we store the state using TIOCM_*

            Trent
        */
        if (index->MSR & TIOCM_DTR)
        {
            *arg |= TIOCM_DTR;
        }
        else { *arg &= ~TIOCM_DTR; }

        if (index->MSR & TIOCM_RTS)
        {
            *arg |= TIOCM_RTS;
        }
        else { *arg &= ~TIOCM_RTS; }

        /*

                    TIOCM_LE
                    TIOCM_ST
                    TIOCM_SR
        */
        va_end(ap);
        return (0);

    case TIOCMBIS:
        arg = va_arg(ap, int *);

        if (*arg & TIOCM_DTR)
        {
            index->MSR |= TIOCM_DTR;

            if (EscapeCommFunction(index->hComm, SETDTR))
            {
                report("EscapeCommFunction: True\n");
            }
            else
            {
                report("EscapeCommFunction: False\n");
            }
        }

        if (*arg & TIOCM_RTS)
        {
            index->MSR |= TIOCM_RTS;

            if (EscapeCommFunction(index->hComm, SETRTS))
            {
                report("EscapeCommFunction: True\n");
            }
            else
            {
                report("EscapeCommFunction: False\n");
            }
        }

        break;

    case TIOCMBIC:
        arg = va_arg(ap, int *);

        if (*arg & TIOCM_DTR)
        {
            index->MSR &= ~TIOCM_DTR;

            if (EscapeCommFunction(index->hComm, CLRDTR))
            {
                report("EscapeCommFunction: True\n");
            }
            else
            {
                report("EscapeCommFunction: False\n");
            }
        }

        if (*arg & TIOCM_RTS)
        {
            index->MSR &= ~TIOCM_RTS;

            if (EscapeCommFunction(index->hComm, CLRRTS))
            {
                report("EscapeCommFunction: True\n");
            }
            else
            {
                report("EscapeCommFunction: False\n");
            }
        }

        break;

    case TIOCMSET:
        arg = va_arg(ap, int *);

        if ((*arg & TIOCM_DTR) == (index->MSR & TIOCM_DTR))
        {
            report("DTR is unchanged\n");
        }

        SNPRINTF(message, sizeof(message), "DTR %i %i\n", *arg & TIOCM_DTR,
                 index->MSR & TIOCM_DTR);
        report(message);

        if (*arg & TIOCM_DTR)
        {
            index->MSR |= TIOCM_DTR;
        }
        else
        {
            index->MSR &= ~TIOCM_DTR;
        }

        if (EscapeCommFunction(index->hComm,
                               (*arg & TIOCM_DTR) ? SETDTR :
                               CLRDTR))
        {
            report("EscapeCommFunction: True\n");
        }
        else
        {
            report("EscapeCommFunction: False\n");
        }

        if ((*arg & TIOCM_RTS) == (index->MSR & TIOCM_RTS))
        {
            report("RTS is unchanged\n");
        }

        SNPRINTF(message, sizeof(message), "RTS %i %i\n", *arg & TIOCM_RTS,
                 index->MSR & TIOCM_RTS);
        report(message);

        if (*arg & TIOCM_RTS)
        {
            index->MSR |= TIOCM_RTS;
        }
        else
        {
            index->MSR &= ~TIOCM_RTS;
        }

        if (EscapeCommFunction(index->hComm,
                               (*arg & TIOCM_RTS) ? SETRTS : CLRRTS))
        {
            report("EscapeCommFunction: True\n");
        }
        else
        {
            report("EscapeCommFunction: False\n");
        }

        break;

#ifdef TIOCGSERIAL

    case TIOCGSERIAL:
        report("TIOCGSERIAL\n");

        dcb = calloc(1, sizeof(DCB));

        if (!dcb)
        {
            va_end(ap);
            return -1;
        }

        memset(dcb, 0, sizeof(DCB));
        GetCommState(index->hComm, dcb);

        sstruct = va_arg(ap, struct serial_struct *);

        if (DCB_to_serial_struct(dcb, sstruct) < 0)
        {
            va_end(ap);
            return -1;
        }

        index->sstruct = sstruct;

        report("TIOCGSERIAL\n");
        free(dcb);
        break;

#endif /* TIOCGSERIAL */
#ifdef TIOCSSERIAL

    case TIOCSSERIAL:
        report("TIOCSSERIAL\n");

        dcb = calloc(1, sizeof(DCB));

        if (!dcb)
        {
            va_end(ap);
            return -1;
        }

        memset(dcb, 0, sizeof(DCB));
        GetCommState(index->hComm, dcb);

        index->sstruct = va_arg(ap, struct serial_struct *);

        if (serial_struct_to_DCB(index->sstruct, dcb) < 0)
        {
            va_end(ap);
            return -1;
        }

        report("TIOCSSERIAL\n");
        free(dcb);
        break;

#endif /* TIOCSSERIAL */

    case TIOCSERCONFIG:
    case TIOCSERGETLSR:
        arg = va_arg(ap, int *);
        /*
        do {
            wait = WaitForSingleObject( index->sol.hEvent, 5000 );
        } while ( wait == WAIT_TIMEOUT );
        */
        ret = ClearErrors(index, &Stat);

        if (ret == 0)
        {
            /* FIXME ? */
            set_errno(EBADFD);
            YACK();
            report("TIOCSERGETLSR EBADFD");
            va_end(ap);
            return -1;
        }

        if ((int) Stat.cbOutQue == 0)
        {
            /* output is empty */
            if (index->tx_happened == 1)
            {
                old_flag = index->event_flag;
                index->event_flag &= ~EV_TXEMPTY;
                SetCommMask(index->hComm,
                            index->event_flag);
                index->event_flag = old_flag;
                *arg = 1;
                index->tx_happened = 0;
                report("ioctl: output empty\n");
            }
            else
            {
                *arg = 0;
            }

            ret = 0;
        }
        else
        {
            /* still data out there */
            *arg = 0;
            ret = 0;
        }

        va_end(ap);
        return (0);
        break;

    case TIOCSERGSTRUCT:
    case TIOCSERGETMULTI:
    case TIOCSERSETMULTI:
        va_end(ap);
        return -ENOIOCTLCMD;

    case TIOCMIWAIT:
        arg = va_arg(ap, int *);
        va_end(ap);
        return -ENOIOCTLCMD;
        /*
            On linux this fills a struct with all the line info
            (data available, bytes sent, ...
        */
#ifdef TIOCGICOUNT

    case TIOCGICOUNT:
        sistruct = va_arg(ap, struct  serial_icounter_struct *);
        ret = ClearErrors(index, &Stat);

        if (ret == 0)
        {
            /* FIXME ? */
            report("TIOCGICOUNT failed\n");
            set_errno(EBADFD);
            va_end(ap);
            return -1;
        }

        sistruct->frame = index->sis->frame;
        sistruct->overrun = index->sis->overrun;
        sistruct->parity = index->sis->parity;
        sistruct->brk = index->sis->brk;

        va_end(ap);
        return 0;
        /* abolete ioctls */
#endif /* TIOCGICOUNT */

    case TIOCSERGWILD:
    case TIOCSERSWILD:
        report("TIOCSER[GS]WILD absolete\n");
        va_end(ap);
        return 0;

    /*  number of bytes available for reading */
    case FIONREAD:
        arg = va_arg(ap, int *);
        ret = ClearErrors(index, &Stat);

        if (ret == 0)
        {
            /* FIXME ? */
            report("FIONREAD failed\n");
            set_errno(EBADFD);
            va_end(ap);
            return -1;
        }

        *arg = (int) Stat.cbInQue;
#ifdef DEBUG_VERBOSE
        SNPRINTF(message, sizeof(message), "FIONREAD:  %i bytes available\n",
                 (int) Stat.cbInQue);
        report(message);

        if (*arg)
        {
            SNPRINTF(message, sizeof(message), "FIONREAD: %i\n", *arg);
            report(message);
        }

#endif /* DEBUG_VERBOSE */
        ret = 0;
        break;

    /* pending bytes to be sent */
    case TIOCOUTQ:
        arg = va_arg(ap, int *);
        va_end(ap);
        return -ENOIOCTLCMD;

    default:
        SNPRINTF(message, sizeof(message),
                 "FIXME:  ioctl: unknown request: %#x\n",
                 request);
        report(message);
        va_end(ap);
        return -ENOIOCTLCMD;
    }

    va_end(ap);
    LEAVE("ioctl");
    return 0;
}

/*----------------------------------------------------------
fcntl()

   accept:
   perform:
   return:
   exceptions:
   win32api:    None
   comments:    FIXME
----------------------------------------------------------*/

int win32_serial_fcntl(int fd, int command, ...)
{
    int arg, ret = 0;
    va_list ap;
    struct termios_list *index;
    char message[80];

    ENTER("fcntl");

    if (fd <= 0)
    {
        return 0;
    }

    index = find_port(fd);

    if (!index)
    {
        LEAVE("fcntl");
        return -1;
    }

    va_start(ap, command);

    arg = va_arg(ap, int);

    switch (command)
    {
    case F_SETOWN:  /* set ownership of fd */
        break;

    case F_SETFL:   /* set operating flags */
#ifdef DEBUG
        SNPRINTF(message, sizeof(message), "F_SETFL fd=%d flags=%d\n", fd, arg);
        report(message);
#endif
        index->open_flags = arg;
        break;

    case F_GETFL:   /* get operating flags */
        ret = index->open_flags;
        break;

    default:
        SNPRINTF(message, sizeof(message), "unknown fcntl command %#x\n", command);
        report(message);
        break;
    }

    va_end(ap);
    LEAVE("fcntl");
    return ret;
}

#if 0
/*----------------------------------------------------------
termios_interrupt_event_loop()

   accept:
   perform:
   return:  let Serial_select break out so the thread can die
   exceptions:
   win32api:
   comments:
----------------------------------------------------------*/
static void termios_interrupt_event_loop(int fd, int flag)
{
    struct termios_list *index = find_port(fd);

    if (!index)
    {
        LEAVE("termios_interrupt_event_loop");
        return;
    }

    /*
        index->event_flag = 0;
         TRENT SetCommMask( index->hComm, index->event_flag );
        hl_usleep(2000);
        tcdrain( index->fd );
        SetEvent( index->sol.hEvent );
    */
    index->interrupt = flag;
    return;
}
#endif

/*----------------------------------------------------------
Serial_select()

   accept:
   perform:
   return:      number of fd's changed on success or -1 on error.
   exceptions:
   win32api:    SetCommMask(), GetCommEvent(), WaitSingleObject()
   comments:
----------------------------------------------------------*/
#ifndef __LCC__
int  win32_serial_select(int  n,  fd_set  *readfds,  fd_set  *writefds,
                         fd_set *exceptfds, struct timeval *timeout)
{

    unsigned long dwCommEvent, wait = WAIT_TIMEOUT;
    int fd = n - 1;
    struct termios_list *index;
    char message[80];
    COMSTAT Stat;
    int ret;

    ENTER("serial_select");

    if (fd <= 0)
    {
        /*  Baby did a bad baad thing */
        goto fail;
    }

    index = find_port(fd);

    if (!index)
    {
        goto fail;
    }

#define DATA_AVAILABLE     1

    //nativeSetEventFlag( fd, SerialPortEvent.DATA_AVAILABLE, enable );
    if (readfds)
    {
        int eventflags[12];
        memset(eventflags, 0, sizeof(eventflags));

        eventflags[DATA_AVAILABLE] = 1;
        termios_setflags(fd, eventflags);
    }

    if (!index->event_flag)
    {
        /* still setting up the port? hold off for a Sec so
           things can fire up

           this does happen.  loops ~twice on a 350 Mzh with
           hl_usleep(1000000)
        */
        /* hl_usleep(10000); */
        LEAVE("serial_uselect");
        return (0);
    }

    ResetEvent(index->wol.hEvent);
    ResetEvent(index->sol.hEvent);
    ResetEvent(index->rol.hEvent);
    ret = ClearErrors(index, &Stat);
#if 1

    if (ret == 0)
    {
        goto fail;
    }

    /* look only after read */
    if (readfds && !writefds && !exceptfds)
    {
        int timeout_usec = timeout ? timeout->tv_sec * 1000000 + timeout->tv_usec :
                           INT_MAX;

        while (timeout_usec > 0)
        {
            SNPRINTF(message, sizeof(message), "wait for data in read buffer%d\n",
                     (int)Stat.cbInQue);
            report(message);

            if (Stat.cbInQue != 0)
            {
                goto end;
            }

            hl_usleep(10000);
            /* FIXME: not very accurate wrt process time */
            timeout_usec -= 10000;

            report("sleep...\n");

            ret = ClearErrors(index, &Stat);

            if (ret == 0)
            {
                goto fail;
            }
        }

        goto timeout;
    }

#endif

    while (wait == WAIT_TIMEOUT && index->sol.hEvent)
    {
        if (index->interrupt == 1)
        {
            goto fail;
        }

        SetCommMask(index->hComm, index->event_flag);
        ClearErrors(index, &Stat);

        if (!WaitCommEvent(index->hComm, &dwCommEvent,
                           &index->rol))
        {
            /* WaitCommEvent failed probably overlapped though */
            if (GetLastError() != ERROR_IO_PENDING)
            {
                ClearErrors(index, &Stat);
                goto fail;
            }

            /* thought so... */
        }

        /*  could use the select timeout here but it should not
            be needed
        */
        ClearErrors(index, &Stat);
        wait = WaitForSingleObject(index->rol.hEvent, 100);

        switch (wait)
        {
        case WAIT_OBJECT_0:
            goto end;

        case WAIT_TIMEOUT:
            goto timeout;

        case WAIT_ABANDONED:
        default:
            goto fail;

        }
    }

end:
    /*  You may want to chop this out for lower latency */
    /* hl_usleep(1000); */
    LEAVE("serial_select");
    return (1);
timeout:
    LEAVE("serial_select");
    return (0);
fail:
    YACK();
    SNPRINTF(message, sizeof(message), "< select called error %i\n", n);
    report(message);
    errno = EBADFD;
    LEAVE("serial_select");
    return (-1);
}
#ifdef asdf
int  win32_serial_select(int  n,  fd_set  *readfds,  fd_set  *writefds,
                         fd_set *exceptfds, struct timeval *timeout)
{

    unsigned long nBytes, dwCommEvent, wait = WAIT_TIMEOUT;
    int fd = n - 1;
    struct termios_list *index;
    char message[80];

    ENTER("serial_select");

    if (fd <= 0)
    {
        /* hl_usleep(1000); */
        return 1;
    }

    index = find_port(fd);

    if (!index)
    {
        LEAVE("serial_select");
        return -1;
    }

    if (index->interrupt == 1)
    {
        goto end;
    }

    while (!index->event_flag)
    {
        /* hl_usleep(1000); */
        return -1;
    }

    while (wait == WAIT_TIMEOUT && index->sol.hEvent)
    {
        if (index->interrupt == 1)
        {
            goto end;
        }

        if (!index->sol.hEvent)
        {
            return 1;
        }

        if (!WaitCommEvent(index->hComm, &dwCommEvent,
                           &index->sol))
        {
            /* WaitCommEvent failed */
            if (index->interrupt == 1)
            {
                goto end;
            }

            if (GetLastError() != ERROR_IO_PENDING)
            {
                SNPRINTF(message, sizeof(message), "WaitCommEvent filename = %s\n",
                         index->filename);
                report(message);
                return (1);
                /*
                                goto fail;
                */
            }

            return (1);
        }

        if (index->interrupt == 1)
        {
            goto end;
        }

        wait = WaitForSingleObject(index->sol.hEvent, 1000);

        switch (wait)
        {
        case WAIT_OBJECT_0:
            if (index->interrupt == 1)
            {
                goto end;
            }

            if (!index->sol.hEvent) { return (1); }

            if (!GetOverlappedResult(index->hComm,
                                     &index->sol, &nBytes, TRUE))
            {
                goto end;
            }
            else if (index->tx_happened == 1)
            {
                goto end;
            }
            else
            {
                goto end;
            }

            break;

        case WAIT_TIMEOUT:
        default:
            return (1); /* WaitFor error */

        }
    }

end:
    /*
        hl_usleep(1000);
    */
    LEAVE("serial_select");
    return (1);
#ifdef asdf
    /* FIXME this needs to be cleaned up... */
fail:
    SNPRINTF(message, sizeof(message), "< select called error %i\n", n);
    YACK();
    report(message);
    set_errno(EBADFD);
    LEAVE("serial_select");
    return (1);
#endif /* asdf */

}
#endif /* asdf */
#endif /* __LCC__ */

#if 0
/*----------------------------------------------------------
termiosSetParityError()

   accept:      fd The device opened
   perform:     Get the Parity Error Char
   return:      the Parity Error Char
   exceptions:  none
   win32api:    GetCommState()
   comments:    No idea how to do this in Unix  (handle in read?)
----------------------------------------------------------*/

static int termiosGetParityErrorChar(int fd)
{
    struct termios_list *index;
    DCB dcb;

    ENTER("termiosGetParityErrorChar");
    index = find_port(fd);

    if (!index)
    {
        LEAVE("termiosGetParityErrorChar");
        return (-1);
    }

    GetCommState(index->hComm, &dcb);
    LEAVE("termiosGetParityErrorChar");
    return (dcb.ErrorChar);
}

/*----------------------------------------------------------
termiosSetParityError()

   accept:      fd The device opened, value the new Parity Error Char
   perform:     Set the Parity Error Char
   return:      void
   exceptions:  none
   win32api:    GetCommState(), SetCommState()
   comments:    No idea how to do this in Unix  (handle in read?)
----------------------------------------------------------*/

static void termiosSetParityError(int fd, char value)
{
    DCB dcb;
    struct termios_list *index;

    ENTER("termiosSetParityErrorChar");
    index = find_port(fd);

    if (!index)
    {
        LEAVE("termiosSetParityError");
        return;
    }

    GetCommState(index->hComm, &dcb);
    dcb.ErrorChar = value;
    SetCommState(index->hComm, &dcb);
    LEAVE("termiosSetParityErrorChar");
}
#endif

/*----------------------- END OF LIBRARY -----------------*/

#endif  /* WIN32 */
