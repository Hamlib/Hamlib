/*-------------------------------------------------------------------------
|   rxtx is a native interface to serial ports in java.
|   Copyright 1997-2002 by Trent Jarvi taj@www.linux.org.uk.
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
| This file was taken from rxtx-2.1-7pre16 and adaptated for Hamlib.
--------------------------------------------------------------------------*/

#ifndef _WIN32TERMIOS_H
#define _WIN32TERMIOS_H

#ifndef _WIN32S_H_
#define _WIN32S_H_
#include <windows.h>
#include <sys/types.h>
#include <io.h>
#include <stdint.h>
#ifdef TRACE
#define ENTER(x) report("entering "x" \n");
#define LEAVE(x) report("leaving "x" \n");
#else
#define ENTER(x)
#define LEAVE(x)
#endif /* TRACE */
#define YACK() \
{ \
	char *allocTextBuf, message[1024]; \
	unsigned int errorCode = GetLastError(); \
	FormatMessage ( \
		FORMAT_MESSAGE_ALLOCATE_BUFFER | \
		FORMAT_MESSAGE_FROM_SYSTEM, \
		NULL, \
		errorCode, \
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
		(LPSTR)&allocTextBuf, \
		16, \
		NULL ); \
	snprintf( message, sizeof (message), "Error 0x%x at %s(%d): %s\n", errorCode, __FILE__, __LINE__, allocTextBuf); \
	report_error( message ); \
	LocalFree(allocTextBuf); \
}

typedef unsigned char   cc_t;
typedef int32_t    speed_t;
typedef int32_t    tcflag_t;

/* structs are from linux includes or linux man pages to match
   interfaces.
*/

#ifndef _TIMESPEC_DEFINED
struct timespec
{
	time_t	tv_sec;
	long	tv_nsec;
};
#endif

#define NCCS 32
struct termios
  {
    tcflag_t c_iflag;           /* input mode flags */
    tcflag_t c_oflag;           /* output mode flags */
    tcflag_t c_cflag;           /* control mode flags */
    tcflag_t c_lflag;           /* local mode flags */
    cc_t c_cc[NCCS];            /* control characters */
    cc_t c_line;                /* line discipline (== c_cc[33]) */
    speed_t c_ispeed;           /* input speed */
    speed_t c_ospeed;           /* output speed */
  };

/*  for TIOCGSERIAL and TIOCSSERIAL of interest are baud_base and
 *  custom_divisor
 *  --- NOTE:  This is not used.  Win32 sets custom speeds on the
 *             kernel side.
 */
struct serial_struct {
/*
	Mainly we are after baud_base/custom_diviser to match
	the ioctl() in SerialImp.c
*/
	int custom_divisor;   /* use to set unsupported speeds */
	int baud_base;        /* use to set unsupported speeds */

	unsigned short	close_delay, closing_wait, iomem_reg_shift;
	int type, line, irq, flags, xmit_fifo_size, hub6;
	unsigned int	port, port_high;
	char		io_type;
	unsigned char	*iomem_base;
};
struct serial_icounter_struct {
	int cts;		/* clear to send count */
	int dsr;		/* data set ready count */
	int rng;		/* ring count */
	int dcd;		/* carrier detect count */
	int rx;			/* received byte count */
	int tx;			/* transmitted byte count */
	int frame;		/* frame error count */
	int overrun;		/* hardware overrun error count */
	int parity;		/* parity error count */
	int brk;		/* break count */
	int buf_overrun;	/* buffer overrun count */
	int reserved[9]; 	/* unused */
};

int win32_serial_test( char * );
int win32_serial_open(const char *File, int flags, ... );
int win32_serial_close(int fd);
int win32_serial_read(int fd, void *b, int size);
int win32_serial_write(int fd, const char *Str, int length);
int win32_serial_ioctl(int fd, int request, ... );
int win32_serial_fcntl(int fd, int command, ...);
/*
 * lcc winsock.h conflicts
 */
#ifndef __LCC__
int win32_serial_select(int, struct fd_set *, struct fd_set *, struct fd_set *, struct timeval *);
#define SELECT win32_serial_select
#endif

#define OPEN win32_serial_open
#define CLOSE win32_serial_close
#define READ win32_serial_read
#define WRITE win32_serial_write
#define IOCTL win32_serial_ioctl
#define FCNTL win32_serial_fcntl

#if 0
/* local functions */
void termios_interrupt_event_loop( int , int );
void termios_setflags( int , int[] );
struct termios_list *find_port( int );
int usleep(unsigned int usec);
const char *get_dos_port(const char *);
void set_errno(int);
char *sterror(int);
int B_to_CBR(int);
int CBR_to_B(int);
int termios_to_bytesize(int);
int bytesize_to_termios(int);
#endif

int tcgetattr(int Fd, struct termios *s_termios);
int tcsetattr(int Fd, int when, struct termios *);
speed_t cfgetospeed(struct termios *s_termios);
speed_t cfgetispeed(struct termios *s_termios);
int cfsetspeed(struct termios *, speed_t speed);
int cfsetospeed(struct termios *, speed_t speed);
int cfsetispeed ( struct termios *, speed_t speed);
int tcflush ( int , int );
int tcgetpgrp ( int );
int tcsetpgrp ( int , int );
int tcdrain ( int );
int tcflow ( int , int );
int tcsendbreak ( int , int );
/*
int fstat(int fd, ... );
*/
void cfmakeraw(struct termios *s_termios);
#if 0
int termiosGetParityErrorChar( int );
void termiosSetParityError( int, char );
#endif

#define O_NOCTTY	0400	/* not for fcntl */
#define O_NONBLOCK	 00004
#define O_NDELAY	O_NONBLOCK
#define O_SYNC		040000
#define O_FSYNC		O_SYNC
#define O_ASYNC		020000	/* fcntl, for BSD compatibility */

#define F_DUPFD		0	/* dup */
#define F_GETFD		1	/* get f_flags */
#define F_SETFD		2	/* set f_flags */
#define F_GETFL		3	/* more flags (cloexec) */
#define F_SETFL		4
#define F_GETLK		7
#define F_SETLK		8
#define F_SETLKW	9

#define F_SETOWN	5	/*  for sockets. */
#define F_GETOWN	6	/*  for sockets. */

/* for F_[GET|SET]FL */
#define FD_CLOEXEC	1	/* actually anything with low bit set goes */

/* for posix fcntl() and lockf() */
#define F_RDLCK		1
#define F_WRLCK		2
#define F_UNLCK		8

/* for old implementation of bsd flock () */
#define F_EXLCK		16	/* or 3 */
#define F_SHLCK		32	/* or 4 */

/* operations for bsd flock(), also used by the kernel implementation */
#define LOCK_SH		1	/* shared lock */
#define LOCK_EX		2	/* exclusive lock */
#define LOCK_NB		4	/* or'd with one of the above to prevent
				   blocking */
#define LOCK_UN		8	/* remove lock */

/* c_cc characters */
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSWTC 7
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT 15
#define VEOL2 16

/* c_iflag bits */
#define IGNBRK	0000001
#define BRKINT	0000002
#define IGNPAR	0000004
#define PARMRK	0000010
#define INPCK	0000020
#define ISTRIP	0000040
#define INLCR	0000100
#define IGNCR	0000200
#define ICRNL	0000400
#define IXON	0002000
#define IXANY	0004000
#define IXOFF	0010000
#define IMAXBEL	   0020000
#define CRTS_IFLOW 0040000
#define CCTS_OFLOW	0100000
#define CIGNORE		0400000
#define CRTSCTS	  020000000000		/* flow control */
#define HARDWARE_FLOW_CONTROL CRTSCTS
#define CRTSXOFF        010000000000
/* c_oflag bits */
#define OPOST	0000001
#define ONLCR	0000002
#define OLCUC	0000004

#define OCRNL	0000010
#define ONOCR	0000020
#define ONLRET	0000040

#define OFILL	00000100
#define OFDEL	00000200
#define NLDLY	00001400
#define   NL0	00000000
#define   NL1	00000400
#define   NL2	00001000
#define   NL3	00001400
#define TABDLY	00006000
#define   TAB0	00000000
#define   TAB1	00002000
#define   TAB2	00004000
#define   TAB3	00006000
#define CRDLY	00030000
#define   CR0	00000000
#define   CR1	00010000
#define   CR2	00020000
#define   CR3	00030000
#define FFDLY	00040000
#define   FF0	00000000
#define   FF1	00040000
#define BSDLY	00100000
#define   BS0	00000000
#define   BS1	00100000
#define VTDLY	00200000
#define   VT0	00000000
#define   VT1	00200000
#define XTABS	01000000 /* Hmm.. Linux/i386 considers this part of TABDLY.. */

/* c_cflag bit meaning */
# define CBAUD	0010017
#define  B0	0000000		/* hang up */
#define  B50	0000001
#define  B75	0000002
#define  B110	0000003
#define  B134	0000004
#define  B150	0000005
#define  B200	0000006
#define  B300	0000007
#define  B600	0000010
#define  B1200	0000011
#define  B1800	0000012
#define  B2400	0000013
#define  B4800	0000014
#define  B9600	0000015
#define  B19200	0000016
#define  B38400	0000017
#define  B57600	  0010001
#define  B115200  0010002
#define  B230400  0010003
#define  B460800  0010004
#define  B500000  0010005
#define  B576000  0010006
#define  B921600  0010007
#define  B1000000 0010010
#define  B1152000 0010011
#define  B1500000 0010012
#define  B2000000 0010013
#define  B2500000 0010014
#define  B3000000 0010015
#define  B3500000 0010016
#define  B4000000 0010017

/* glue for unsupported linux speeds see also SerialImp.h.h */

#define B14400		1010001
#define B28800		1010002
#define B128000		1010003
#define B256000		1010004

#define EXTA B19200
#define EXTB B38400
#define CSIZE	0000060
#define   CS5	0000000
#define   CS6	0000020
#define   CS7	0000040
#define   CS8	0000060
#define CSTOPB	0000100
#define CREAD	0000200
#define PARENB	0000400
#define PARODD	0001000
#define HUPCL	0002000
#define CLOCAL	0004000
# define CBAUDEX 0010000
# define CIBAUD	  002003600000		/* input baud rate (not used) */
# define CRTSCTS  020000000000		/* flow control */

/* c_l flag */
#define ISIG    0000001
#define ICANON  0000002
#define XCASE   0000004
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define NOFLSH  0000200
#define TOSTOP  0000400
#define ECHOCTL 0001000
#define ECHOPRT 0002000
#define ECHOKE  0004000
#define FLUSHO  0010000
#define PENDIN  0040000
#define IEXTEN  0100000

/* glue for unsupported windows speeds */

#define CBR_230400	230400
#define CBR_28800	28800
#define CBR_460800	460800
#define CBR_500000	500000
#define CBR_576000	576000
#define CBR_921600	921600
#define CBR_1000000	1000000
#define CBR_1152000	1152000
#define CBR_1500000	1500000
#define CBR_2000000	2000000
#define CBR_2500000	2500000
#define CBR_3000000	3000000
#define CBR_3500000	3500000
#define CBR_4000000	4000000


/* Values for the ACTION argument to `tcflow'.  */
#define	TCOOFF		0
#define	TCOON		1
#define	TCIOFF		2
#define	TCION		3

/* Values for the QUEUE_SELECTOR argument to `tcflush'.  */
#define	TCIFLUSH	0
#define	TCOFLUSH	1
#define	TCIOFLUSH	2

/* Values for the OPTIONAL_ACTIONS argument to `tcsetattr'.  */
#define	TCSANOW		0
#define	TCSADRAIN	1
#define	TCSAFLUSH	2

/* ioctls */
#define TIOCSERGETLSR	0x5459

#endif /*_WIN32S_H_*/

/* unused ioctls */
#define TCSBRK		0x5409
#define TIOCCBRK	0x540a
#define TIOCSBRK	0x540b
#define TIOCOUTQ	0x5411
#define TIOCMGET	0x5415
#define TIOCMBIS	0x5416
#define TIOCMBIC	0x5417
#define TIOCMSET	0x5418
#define TIOCGSOFTCAR	0x5419
#define TIOCSSOFTCAR	0x541a
#define TIOCSER_TEMP	0x01
/*
#define FIONREAD	0x541b
TIOC[GS]SERIAL is not used on win32.  It was dropped after we could not
find a way to get/set buad_base and divisor directly.
#define TIOCGSERIAL	0x541e
#define TIOCSSERIAL	0x541f
*/
#define TCSBRKP		0x5425
#define TIOCSERCONFIG	0x5453
#define TIOCSERGWILD	0x5454
#define TIOCSERSWILD	0x5455
#define TIOCSERGSTRUCT	0x5458
#define TIOCSERGETMULTI	0x545a
#define TIOCSERSETMULTI	0x545b
#define TIOCMIWAIT	0x545c
/* this would require being able to get the number of overruns ... */
/*
	FIXME
	frame and parity errors caused crashes in testing BlackBox
*/
#define TIOCGICOUNT	0x545d

/* ioctl errors */
#define ENOIOCTLCMD	515
#define EBADFD		 77
/* modem lines */
#define TIOCM_LE    0x001
#define TIOCM_DTR   0x002
#define TIOCM_RTS   0x004
#define TIOCM_ST    0x008
#define TIOCM_SR    0x010
#define TIOCM_CTS   0x020
#define TIOCM_CAR   0x040
#define TIOCM_RNG   0x080
#define TIOCM_DSR   0x100
#define TIOCM_CD    TIOCM_CAR
#define TIOCM_RI    TIOCM_RNG


#define CMSPAR      010000000000  /* mark or space parity */

#endif /* _WIN32TERMIOS_H */
