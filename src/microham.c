/*
 *  Hamlib Interface - Microham device support for POSIX systems
 *  Copyright (c) 2017 by Christoph van Wüllen
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

//
// This file contains the support for microHam devices
//
// On WIN32, this behaves as if no microHam device is connected
// to the computer since (at least my version of MinGW) does not
// support socketpair().
//

#include <hamlib/config.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>

#ifdef HAVE_ERRNO_H
#  include <errno.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif

//#define FRAME(s, ...)   printf(s, ##__VA_ARGS__)
//#define DEBUG(s, ...)   printf(s, ##__VA_ARGS__)
//#define TRACE(s, ...)   printf(s, ##__VA_ARGS__)
//#define MYERROR(s, ...) printf(s, ##__VA_ARGS__)

#ifndef FRAME
#  define FRAME(s, ...)
#endif

#ifndef DEBUG
#  define DEBUG(s, ...)
#endif

#ifndef TRACE
#  define TRACE(s, ...)
#endif

#ifndef MYERROR
#  define MYERROR(s, ...)
#endif

#ifndef PATH_MAX
// should not happen, should be defined in limits.h
// but better paranoia than a code that does not work
#define PATH_MAX 256
#endif

#if !(defined(WIN32) || !defined(HAVE_GLOB_H))
static char
uh_device_path[PATH_MAX];  // use PATH_MAX since udev names can be VERY long!
#endif
static int uh_device_fd = -1;
static int uh_is_initialized = 0;

static int uh_radio_pair[2] = { -1, -1};
static int uh_ptt_pair[2]   = { -1, -1};
static int uh_wkey_pair[2]  = { -1, -1};

static int uh_radio_in_use;
static int uh_ptt_in_use;
static int uh_wkey_in_use;

static int statusbyte = 0;

#ifdef HAVE_PTHREAD

#include <pthread.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t readthread;
#define getlock()  if (pthread_mutex_lock(&mutex))   perror("GETLOCK:")
#define freelock() if (pthread_mutex_unlock(&mutex)) perror("FREELOCK:")
#else
#define getlock()
#define freelock()
#endif

#if defined(HAVE_SELECT)
//
// time of last heartbeat. Updated by heartbeat()
//
static time_t lastbeat = 0;
#endif

#if defined(HAVE_PTHREAD) && defined(HAVE_SOCKETPAIR) && defined(HAVE_SELECT)
static time_t starttime;

#define TIME ((int) (time(NULL) - starttime))
#endif

//
// close all sockets and mark them free
//
static void close_all_files()
{
    if (uh_radio_pair[0] >= 0)
    {
        close(uh_radio_pair[0]);
    }

    if (uh_radio_pair[1] >= 0)
    {
        close(uh_radio_pair[1]);
    }

    if (uh_ptt_pair[0]   >= 0)
    {
        close(uh_ptt_pair[0]);
    }

    if (uh_ptt_pair[1]   >= 0)
    {
        close(uh_ptt_pair[1]);
    }

    if (uh_wkey_pair[0]  >= 0)
    {
        close(uh_wkey_pair[0]);
    }

    if (uh_wkey_pair[1]  >= 0)
    {
        close(uh_wkey_pair[1]);
    }

    uh_radio_pair[0] = -1;
    uh_radio_pair[1] = -1;
    uh_ptt_pair[0] = -1;
    uh_ptt_pair[1] = -1;
    uh_wkey_pair[0] = -1;
    uh_wkey_pair[1] = -1;
    uh_radio_in_use = 0;
    uh_ptt_in_use = 0;
    uh_wkey_in_use = 0;

//  finally, close connection to microHam device
    if (uh_device_fd >= 0)
    {
        close(uh_device_fd);
    }
}


static void close_microham()
{
    if (!uh_is_initialized)
    {
        return;
    }

    TRACE("%10d:Closing MicroHam device\n", TIME);
    uh_is_initialized = 0;
#ifdef HAVE_PTHREAD
    // wait for read_device thread to finish
    pthread_join(readthread, NULL);
#endif
    close_all_files();
}


#if defined(WIN32) || !defined(HAVE_GLOB_H)
/*
 * On Windows, this is not really needed since we have uhrouter.exe
 * creating virtual COM ports
 *
 * I do now know how to "find" a microham device under Windows.
 *
 * Therefore, a dummy version of finddevices() is included such that it compiles
 * well on WIN32. Since the dummy version does not find anything, no reading thread
 * and no sockets are created.
 *
 *
 * For those who want to implement the WIN32 case here properly:
 *
 * What finddevices() must do:
 * Scan all USB-serial ports with an FTDI chip, and look
 * for its serial number, take the first port you can find where the serial
 * number begins with MK, M2, CK, DK, D2, 2R, 2P or UR. Then, open the serial
 * line with correct serial speed etc. and put a valid fd into uh_device_fd.
 */
/* Commenting out the following dummy function to quell the warning from
 * MinGW's GCC of a defined but not used function.
 */
/* static void finddevices() */
/* { */
/* } */

#else

/*
 * POSIX (including APPLE)
 *
 * Finding microHam devices can be a mess.
 * On Apple, the FTDI chips inside the microHam device show up as
 * /dev/tty.usbserial-<SERIAL> where <SERIAL> is the serial number
 * of the chip. So we can easily look for MK, MK-II, DK etc.
 * devices.
 *
 * On LINUX, these show up as /dev/ttyUSBx where x=0,1,2...
 * depending on when the device has been recognized. Fortunately
 * today all LINUX boxes have udev, and there is a symlink
 * in /dev/serial/by-id containing the serial number pointing
 * to the relevant special file in /dev.
 *
 * This technique is used such that we do not need to open
 * ALL serial ports in the system looking which one corresponds
 * to microHam. Note we could use libusb directly, but this
 * probably requires root privileges.
 *
 * Note: StationMaster uses a different protocol, and
 *       the protocol of StationMaster DeLuxe is not
 *       even disclosed.
 *
 * We are using the glob() function to obtain a list
 * of candidates.
 */

#define NUMUHTYPES 9
static struct uhtypes
{
    const char *name;
    const char *device;
} uhtypes[NUMUHTYPES] =
{

#ifdef __APPLE__
    { "microKeyer",      "/dev/tty.usbserial-MK*"},
    { "microKeyer-II",   "/dev/tty.usbserial-M2*"},
    { "microKeyer-III",  "/dev/tty.usbserial-M3*"},
    { "CW Keyer",        "/dev/tty.usbserial-CK*"},
    { "digiKeyer",       "/dev/tty.usbserial-DK*"},
    { "digiKeyer-II",    "/dev/tty.usbserial-D2*"},
    { "micorKeyer-IIR",  "/dev/tty.usbserial-2R*"},
    { "microKeyer-IIR+", "/dev/tty.usbserial-2P*"},
    { "microKeyer-U2R",  "/dev/tty.usbserial-UR*"},
#else
    { "microKeyer",      "/dev/serial/by-id/*microHAM*_MK*"},
    { "microKeyer-II",   "/dev/serial/by-id/*microHAM*_M2*"},
    { "microKeyer-III",  "/dev/serial/by-id/*microHAM*_M3*"},
    { "CW Keyer",        "/dev/serial/by-id/*microHAM*_CK*"},
    { "digiKeyer",       "/dev/serial/by-id/*microHAM*_DK*"},
    { "digiKeyer-II",    "/dev/serial/by-id/*microHAM*_D2*"},
    { "micorKeyer-IIR",  "/dev/serial/by-id/*microHAM*_2R*"},
    { "microKeyer-IIR+", "/dev/serial/by-id/*microHAM*_2P*"},
    { "microKeyer-U2R",  "/dev/serial/by-id/*microHAM*_UR*"},
#endif
};


//
// Find a microHamDevice. Here we assume that the device special
// file has a name from which we can tell this is a microHam device
// This is the case for MacOS and LINUX (for LINUX: use udev)
//
#include <termios.h>
#include <sys/stat.h>
#include <glob.h>
static void finddevices()
{
    struct stat st;
    glob_t gbuf;
    int i, j;
    struct termios TTY;
    int fd;

    uh_device_fd = -1; // indicates "no device is found"

    //
    // Check ALL device special files that might be relevant,
    //
    for (i = 0; i < NUMUHTYPES; i++)
    {
        DEBUG("Checking for %s device\n", uhtypes[i].device);
        glob(uhtypes[i].device, 0, NULL, &gbuf);

        for (j = 0; j < gbuf.gl_pathc; j++)
        {
            DEBUG("Found file: %s\n", gbuf.gl_pathv[j]);

            if (!stat(gbuf.gl_pathv[j], &st))
            {
                if (S_ISCHR(st.st_mode))
                {
                    // found a character special device with correct name
                    if (strlen(gbuf.gl_pathv[j]) >= PATH_MAX)
                    {
                        // I do not know if this can happen, but if it happens, we just skip the device.
                        MYERROR("Name too long: %s\n", gbuf.gl_pathv[j]);
                        continue;
                    }

                    DEBUG("%s is a character special file\n", gbuf.gl_pathv[j]);
                    strcpy(uh_device_path, gbuf.gl_pathv[j]);
                    TRACE("Found a %s, Device=%s\n", uhtypes[i].name, uh_device_path);

                    fd = open(uh_device_path, O_RDWR | O_NONBLOCK | O_NOCTTY);

                    if (fd < 0)
                    {
                        MYERROR("Cannot open serial port %s\n", uh_device_path);
                        perror("Open:");
                        continue;
                    }

                    tcflush(fd, TCIFLUSH);

                    if (tcgetattr(fd, &TTY))
                    {
                        MYERROR("Cannot get comm params\n");
                        close(fd);
                        continue;
                    }

                    // microHam devices always use 230400 baud, 8N1, only TxD/RxD is used (no h/w handshake)
                    // 8 data bits
                    TTY.c_cflag &= ~CSIZE;
                    TTY.c_cflag |= CS8;
                    // enable receiver, set local mode
                    TTY.c_cflag |= (CLOCAL | CREAD);
                    // no parity
                    TTY.c_cflag &= ~PARENB;
                    // 1 stop bit
                    TTY.c_cflag &= ~CSTOPB;

                    cfsetispeed(&TTY, B230400);
                    cfsetospeed(&TTY, B230400);

                    // NO h/w handshake
                    // TTY.c_cflag &= ~CRTSCTS;
                    // raw input
                    TTY.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
                    // raw output
                    TTY.c_oflag &= ~OPOST;
                    // software flow control disabled
                    // TTY.c_iflag &= ~IXON;
                    // do not translate CR to NL
                    // TTY.c_iflag &= ~ICRNL;

                    // timeouts
                    TTY.c_cc[VMIN] = 0;
                    TTY.c_cc[VTIME] = 255;

                    if (tcsetattr(fd, TCSANOW, &TTY))
                    {
                        MYERROR("Can't set device communication parameters");
                        close(fd);
                        continue;
                    }

                    TRACE("SerialPort opened: %s fd=%d\n", uh_device_path, fd);
                    uh_device_fd = fd;
                    // The first time we were successful, we skip all what might come
                    return;
                }
            }
        }
    }
}

#endif

#if defined(HAVE_SELECT)
//
// parse a frame received from the keyer
// This is called from the "device reading" thread
// once a complete frame has been received
// Send Radio and Winkey bytes received to the client sockets.
//
static int frameseq = 0;
static int incontrol = 0;
static unsigned char controlstring[256];
static int numcontrolbytes = 0;

static void parseFrame(const unsigned char *frame)
{
    unsigned char byte;
    FRAME("RCV frame %02x %02x %02x %02x\n", frame[0], frame[1], frame[2],
          frame[3]);

    // frames come in sequences. The first frame of a sequence has bit6 cleared in the headerbyte.
    if ((frame[0] & 0x40) == 0)
    {
        frameseq = 0;
    }
    else
    {
        frameseq++;
    }

    // A frame is of the form header-byte1-byte2-byte3
    // byte 1 is always RADIO, byte 2 is always RADIO2/AUX (if validity bit set)
    // byte 3 has different meaning, depending on the sequence number of the frame:
    // seq=0  -> Flags
    // seq=1  -> control
    // seq=2  -> WinKey
    // seq=3  -> PS2

    // if RADIO byte is valid, send it to client
    if ((frame[0] & 0x20) != 0)
    {
        byte = frame[1] & 0x7F;

        if ((frame[0] & 0x04) != 0)
        {
            byte |= 0x80;
        }

        DEBUG("%10d:FromRadio: %02x\n", TIME, byte);

        if (write(uh_radio_pair[0], &byte, 1) != 1)
        {
            MYERROR("Write Radio Socket\n");
        }
    }

    // ignore AUX/RADIO2 for the time being

    // check the shared channel for validity, if it is the CONTROL channel it is always valid
    if ((frame[0] & 0x08) || (frameseq == 1))
    {
        byte = frame[3] & 0x7F;

        if (frame[0] & 0x01)
        {
            byte |= 0x80;
        }

        switch (frameseq)
        {
        case 0:  // Flag byte
            DEBUG("%10d:RCV: Flags=%02x\n", TIME, byte);
            // No reason to pass the flags to clients
            break;

        case 1:  // part of control string
            if ((frame[0] & 0x08) == 0 && !incontrol)
            {
                // start or end of a control sequence
                numcontrolbytes = 1;
                controlstring[0] = byte;
                incontrol = 1;
                break;
            }

            if ((frame[0] & 0x08) == 0 && incontrol)
            {
                int i;
                // end of a control sequence
                controlstring[numcontrolbytes++] = byte;
                DEBUG("%10d:FromControl:", TIME);

                for (i = 0; i < numcontrolbytes; i++) { DEBUG(" %02x", controlstring[i]); }

                DEBUG(".\n");
                incontrol = 0;
                // printing control messages is only used for debugging.
                // Note that we can get a lot of unsolicited control messages
                // here (squelch, voltage change, etc.)
                break;
            }

            // in the middle of a control string
            controlstring[numcontrolbytes++] = byte;
            break;

        case 2: // message from WinKey chip
            DEBUG("%10d:RCV: WinKey=%02x\n", TIME, byte);

            if (write(uh_wkey_pair[0], &byte, 1) != 1)
            {
                MYERROR("Write Winkey socket\n");
            }

            break;

        case 3: // Key pressed on PS2 keyboard connected to microHam device
            DEBUG("%10d:RCV: PS2=%02x\n", TIME, byte);
            break;
        }
    }
}
#endif  /* HAVE_SELECT */


#if defined(HAVE_SELECT)
//
// Send radio bytes to microHam device
//
static void writeRadio(const unsigned char *bytes, int len)
{
    unsigned char seq[4];
    int i;

    DEBUG("%10d:Send radio data: ", TIME);

    for (i = 0; i < len; i++) { DEBUG(" %02x", (int) bytes[i]); }

    DEBUG(".\n");

    getlock();

    for (i = 0; i < len; i++)
    {
        int ret;

        seq[0] = 0x28;
        seq[1] = 0x80 | bytes[i];
        seq[2] = 0x80;
        seq[3] = 0X80 | statusbyte;

        if (statusbyte & 0x80)
        {
            seq[0] |= 0x01;
        }

        if (bytes[i]   & 0x80)
        {
            seq[0] |= 0x04;
        }

        if ((ret = write(uh_device_fd, seq, 4)) < 4)
        {
            MYERROR("WriteRadio failed with %d\n", ret);

            if (ret < 0)
            {
                perror("WriteRadioError:");
            }
        }
    }

    freelock();
}
#endif  /* HAVE_SELECT */


//
// send statusbyte to microHam device
//
static void writeFlags()
{
    unsigned char seq[4];
    int ret;

    DEBUG("%10d:Sending FlagByte: %02x\n", TIME, statusbyte);
    getlock();
    seq[0] = 0x08;

    if (statusbyte & 0x80)
    {
        seq[0] = 0x09;
    }

    seq[1] = 0x80;
    seq[2] = 0x80;
    seq[3] = 0x80 | statusbyte;

    if ((ret = write(uh_device_fd, seq, 4)) < 4)
    {
        MYERROR("WriteFlags failed with %d\n", ret);

        if (ret < 0)
        {
            perror("WriteFlagsError:");
        }
    }

    freelock();
}


#if defined(HAVE_SELECT)
//
// Send bytes to the WinKeyer within microHam device
//
static void writeWkey(const unsigned char *bytes, int len)
{
    unsigned char seq[12];
    int i;
    DEBUG("%10d:Send WinKey data: ", TIME);

    for (i = 0; i < len; i++) { DEBUG(" %02x", (int) bytes[i]); }

    DEBUG(".\n");
    // Winkey data is in the third frame of a sequence,
    // So send two no-ops first. Include statusbyte in first frame
    getlock();

    for (i = 0; i < len; i++)
    {
        int ret;

        seq[ 0] = 0x08;
        seq[ 1] = 0x80;
        seq[ 2] = 0x80;
        seq[ 3] = 0X80 | statusbyte;
        seq[ 4] = 0x40;
        seq[ 5] = 0x80;
        seq[ 6] = 0x80;
        seq[ 7] = 0x80;
        seq[ 8] = 0x48;
        seq[ 9] = 0x80;
        seq[10] = 0x80;
        seq[11] = 0x80 | bytes[i];

        if (statusbyte & 0x80)
        {
            seq[ 0] |= 0x01;
        }

        if (bytes[i]   & 0x80)
        {
            seq[ 8] |= 0x01;
        }

        if ((ret = write(uh_device_fd, seq, 12)) < 12)
        {
            MYERROR("WriteWINKEY failed with %d\n", ret);

            if (ret < 0)
            {
                perror("WriteWinkeyError:");
            }
        }
    }

    freelock();
}
#endif  /* HAVE_SELECT */


//
// Write a control string to the microHam device
//
static void writeControl(const unsigned char *data, int len)
{
    int i;
    unsigned char seq[8];

    DEBUG("%10d:WriteControl:", TIME);

    for (i = 0; i < len; i++) { DEBUG(" %02x", data[i]); }

    DEBUG(".\n");
    // Control data is in the second frame of a sequence,
    // So send a no-op first. Include statusbyte in first frame.
    // First and last byte of the control message is NOT marked "valid"
    getlock();

    for (i = 0; i < len; i++)
    {
        int ret;

        // encode statusbyte in first frame
        seq[0] = 0x08;
        seq[1] = 0x80;
        seq[2] = 0x80;
        seq[3] = 0x80 | statusbyte;
        seq[4] = 0x48; // marked valid
        seq[5] = 0x80;
        seq[6] = 0x80;
        seq[7] = 0x80 | data[i];

        if (statusbyte & 0x80)
        {
            seq[0] |= 1;
        }

        if (i == 0 || i == len - 1)
        {
            seq[4] = 0x40; // un-mark valid
        }

        if (data[i] & 0x80)
        {
            seq[4] |= 0x01;
        }

        if ((ret = write(uh_device_fd, seq, 8)) < 8)
        {
            MYERROR("WriteControl failed, ret=%d\n", ret);

            if (ret < 0)
            {
                perror("WriteControlError:");
            }
        }
    }

    freelock();
}


#if defined(HAVE_PTHREAD) && defined(HAVE_SOCKETPAIR) && defined(HAVE_SELECT)
//
// send a heartbeat and record time
// The "last heartbeat" time is recorded in a global variable
// such that the service thread can decice whether a new
// heartbeat is due.
//
static void heartbeat()
{
    unsigned char seq[2];

    seq[0] = 0x7e;
    seq[1] = 0xfe;
    writeControl(seq, 2);
    lastbeat = time(NULL);
}
#endif /* defined(HAVE_PTHREAD) && defined(HAVE_SOCKETPAIR) && defined(HAVE_SELECT) */


#if defined(HAVE_SELECT)
//
// This thread reads from the microHam device and puts data on the sockets
// it also issues periodic heartbeat messages
// it also reads the sockets if data is available
//
static void *read_device(void *p)
{
    unsigned char frame[4];
    int framepos = 0;
    unsigned char buf[2];
    fd_set fds;
    struct timeval tv;

    // the bytes from the microHam decive come in "frames"
    // a frame is a four-byte sequence. The first byte has the MSB unset,
    // then come three bytes with the MSB set
    // What comes here is an "infinite" loop. However this thread
    // terminates if the device is closed.
    for (;;)
    {
        int ret;
        int maxdev;

        //
        // setting uh_is_initialized to zero in the main thread
        // tells this one that it is all over now
        //
        if (!uh_is_initialized)
        {
            return NULL;
        }


#if defined(HAVE_PTHREAD) && defined(HAVE_SOCKETPAIR) && defined(HAVE_SELECT)

        //
        // This is the right place to ensure that a heartbeat is sent
        // to the microham device regularly (15 sec delay is the maximum
        // allowed, let us use 5 secs to be on the safe side).
        //
        if ((time(NULL) - lastbeat) > 5)
        {
            heartbeat();
        }

#endif

        //
        // Wait for something to arrive, either from the microham device
        // or from the sockets used for I/O from hamlib.
        // If nothing arrives within 100 msec, restart the "infinite loop".
        //
        FD_ZERO(&fds);
        FD_SET(uh_device_fd, &fds);
        FD_SET(uh_radio_pair[0], &fds);
        FD_SET(uh_ptt_pair[0], &fds);
        FD_SET(uh_wkey_pair[0], &fds);

        // determine max of these fd's for use in select()
        maxdev = uh_device_fd;

        if (uh_radio_pair[0] > maxdev)
        {
            maxdev = uh_radio_pair[0];
        }

        if (uh_ptt_pair[0]   > maxdev)
        {
            maxdev = uh_ptt_pair[0];
        }

        if (uh_wkey_pair[0]  > maxdev)
        {
            maxdev = uh_wkey_pair[0];
        }

        tv.tv_usec = 100000;
        tv.tv_sec = 0;
        ret = select(maxdev + 1, &fds, NULL, NULL, &tv);

        //
        // select returned error, or nothing has arrived:
        // continue "infinite" loop.
        //
        if (ret <= 0)
        {
            continue;
        }

        //
        // Take care of the incoming data (microham device, sockets)
        //
        if (FD_ISSET(uh_device_fd, &fds))
        {
            // compose frame, if it is complete, all parseFrame
            while (read(uh_device_fd, buf, 1) > 0)
            {
                if (!(buf[0] & 0x80) && framepos != 0)
                {
                    MYERROR("FrameSyncStartError\n");
                    framepos = 0;
                }

                if ((buf[0] & 0x80) && framepos == 0)
                {
                    MYERROR("FrameSyncStartError\n");
                    framepos = 0;
                    continue;
                }

                frame[framepos++] = buf[0];

                if (framepos >= 4)
                {
                    framepos = 0;
                    parseFrame(frame);
                }
            }
        }

        if (FD_ISSET(uh_ptt_pair[0], &fds))
        {
            // we do not expect any data here, but drain the socket
            while (read(uh_ptt_pair[0], buf, 1) > 0)
            {
                // do nothing
            }
        }

        if (FD_ISSET(uh_radio_pair[0], &fds))
        {
            // read everything that is there, and send it to the radio
            while (read(uh_radio_pair[0], buf, 1) > 0)
            {
                writeRadio(buf, 1);
            }
        }

        if (FD_ISSET(uh_wkey_pair[0], &fds))
        {
            // read everything that is there, and send it to the WinKey chip
            while (read(uh_wkey_pair[0], buf, 1) > 0)
            {
                writeWkey(buf, 1);
            }
        }
    }

//    return NULL;
}
#endif


/*
 * If we do not have pthreads, we cannot use the microham device.
 * This is so because we have to digest unsolicited messages
 * (e.g. voltage change) and since we have to send periodic
 * heartbeats.
 * Nevertheless, the program should compile well even we we do not
 * have pthreads, in this case start_thread is a dummy since uh_is_initialized
 * is never set.
 *
 * If we do not have socketpair(), the same thing applies.
 *
 * If we do not have select(), then the read thread cannot work so we
 * do not spawn it.
 */
static void start_thread()
{
#if defined(HAVE_PTHREAD) && defined(HAVE_SOCKETPAIR) && defined(HAVE_SELECT)
    /*
     * Find a microHam device and open serial port to it.
     * If successful, create sockets for doing I/O from within hamlib
     * and start a thread to listen to the "other ends" of the sockets
     */
    int ret, fail;
    unsigned char buf[4];
    pthread_attr_t attr;

    if (uh_is_initialized)
    {
        return;  // PARANOIA: this should not happen
    }

    finddevices();

    if (uh_device_fd  < 0)
    {
        MYERROR("Could not open any microHam device.\n");
        return;
    }

    // Create socket pairs
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, uh_radio_pair)  < 0)
    {
        perror("RadioPair:");
        return;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, uh_ptt_pair)    < 0)
    {
        perror("PTTPair:");
        return;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, uh_wkey_pair) < 0)
    {
        perror("WkeyPair:");
        return;
    }

    DEBUG("RADIO  sockets: server=%d  client=%d\n", uh_radio_pair[0],
          uh_radio_pair[1]);
    DEBUG("PTT    sockets: server=%d  client=%d\n", uh_ptt_pair[0], uh_ptt_pair[1]);
    DEBUG("WinKey sockets: server=%d  client=%d\n", uh_wkey_pair[0],
          uh_wkey_pair[1]);

    //
    // Make the sockets nonblocking
    // First try if we can set flags, then do set the flags
    //

    fail = 0;

    ret = fcntl(uh_radio_pair[0], F_GETFL, 0);

    if (ret != -1)
    {
        ret = fcntl(uh_radio_pair[0], F_SETFL, ret | O_NONBLOCK);
    }

    if (ret == -1)
    {
        fail = 1;
    }

    ret = fcntl(uh_ptt_pair[0], F_GETFL, 0);

    if (ret != -1)
    {
        ret = fcntl(uh_ptt_pair[0], F_SETFL, ret | O_NONBLOCK);
    }

    if (ret == -1)
    {
        fail = 1;
    }

    ret = fcntl(uh_wkey_pair[0], F_GETFL, 0);

    if (ret != -1)
    {
        ret = fcntl(uh_wkey_pair[0], F_SETFL, ret | O_NONBLOCK);
    }

    if (ret == -1)
    {
        fail = 1;
    }

    ret = fcntl(uh_radio_pair[1], F_GETFL, 0);

    if (ret != -1)
    {
        ret = fcntl(uh_radio_pair[1], F_SETFL, ret | O_NONBLOCK);
    }

    if (ret == -1)
    {
        fail = 1;
    }

    ret = fcntl(uh_ptt_pair[1], F_GETFL, 0);

    if (ret != -1)
    {
        ret = fcntl(uh_ptt_pair[1], F_SETFL, ret | O_NONBLOCK);
    }

    if (ret == -1)
    {
        fail = 1;
    }

    ret = fcntl(uh_wkey_pair[1], F_GETFL, 0);

    if (ret != -1)
    {
        ret = fcntl(uh_wkey_pair[1], F_SETFL, ret | O_NONBLOCK);
    }

    if (ret == -1)
    {
        fail = 1;
    }

    //
    // If something went wrong, close everything and return
    //
    if (fail)
    {
        close_all_files();
        return;
    }

    // drain input from microHam device
    while (read(uh_device_fd, buf, 1) > 0)
    {
        // do_nothing
    }

    uh_is_initialized = 1;
    starttime = time(NULL);

    // Do some heartbeats to sync-in
    heartbeat();
    heartbeat();
    heartbeat();

    // Set keyer mode to DIGITAL
    buf[0] = 0x0A; buf[1] = 0x03; buf[2] = 0x8a; writeControl(buf, 3);

    // Start background thread reading the microham device and the sockets
    pthread_attr_init(&attr);
    ret = pthread_create(&readthread, &attr, read_device, NULL);

    if (ret != 0)
    {
        MYERROR("Could not start read_device thread\n");
        close_all_files();
        uh_is_initialized = 0;
        return;
    }

    //TRACE("Started daemonized thread reading microHam\n");
#endif
// if we do not have pthreads, this function does nothing.
}


/*
 * What comes now are "public" functions that can be called from outside
 *
   void uh_close_XXX()        XXX= ptt, radio, wkey
   void uh_open_XXX()         XXX= ptt, wkey
   void uh_open_radio(int baud, int databits, int stopbits, int rtscts)
   void uh_set_ptt(int ptt)
   int  uh_get_ptt()

 * Note that it is not intended that any I/O is done via the PTT sockets
 * but hamlib needs a valid file descriptor!
 *
 */

/*
 * Close routines:
 * Mark the channel as closed, but close the connection
 * to the microHam device only if ALL channels are closed
 *
 * NOTE: hamlib repeatedly opens/closes the PTT port while keeping the
 *       the radio port open.
 */
void uh_close_ptt()
{
    uh_ptt_in_use = 0;

    if (!uh_radio_in_use && ! uh_wkey_in_use)
    {
        close_microham();
    }
}


void uh_close_radio()
{
    uh_radio_in_use = 0;

    if (!uh_ptt_in_use && ! uh_wkey_in_use)
    {
        close_microham();
    }
}

void uh_close_wkey()
{
    uh_wkey_in_use = 0;

    if (!uh_ptt_in_use && ! uh_radio_in_use)
    {
        close_microham();
    }
}

int uh_open_ptt()
{
    if (!uh_is_initialized)
    {
        start_thread();

        if (!uh_is_initialized)
        {
            return -1;
        }
    }

    uh_ptt_in_use = 1;
    return uh_ptt_pair[1];
}

int uh_open_wkey()
{
    if (!uh_is_initialized)
    {
        start_thread();

        if (!uh_is_initialized)
        {
            return -1;
        }
    }


    uh_wkey_in_use = 1;
    return uh_wkey_pair[1];
}


//
// Number of stop bits must be 1 or 2.
// Number of data bits can be 5,6,7,8
// Hardware handshake (rtscts) can be on of off.
// microHam devices ALWAYS use "no parity".
//
int uh_open_radio(int baud, int databits, int stopbits, int rtscts)
{
    unsigned char string[5];
    int baudrateConst;

    if (!uh_is_initialized)
    {
        start_thread();

        if (!uh_is_initialized)
        {
            return -1;
        }
    }

    baudrateConst = 11059200 / baud ;
    string[0] = 0x01;
    string[1] = baudrateConst & 0xff ;
    string[2] = baudrateConst / 256 ;

    switch (stopbits)
    {
    case 1:  string[3] = 0x00; break;

    case 2:  string[3] = 0x40; break;

    default: return -1;
    }

    if (rtscts)
    {
        string[3] |= 0x10;
    }

    switch (databits)
    {
    case 5: break;

    case 6: string[3] |= 0x20; break;

    case 7: string[3] |= 0x40; break;

    case 8: string[3] |= 0x60; break;

    default: return -1;
    }

    string[4] = 0x81;
    writeControl(string, 5);

    uh_radio_in_use = 1;
    return uh_radio_pair[1];
}


void uh_set_ptt(int ptt)
{
    if (!uh_ptt_in_use)
    {
        MYERROR("%10d:SetPTT but not open\n", TIME);
        return;
    }

    DEBUG("%10d:SET PTT = %d\n", TIME, ptt);

    if (ptt)
    {
        statusbyte |= 0x04;
    }
    else
    {
        statusbyte &= ~0x04;
    }

    writeFlags();
}


int uh_get_ptt()
{
    // Possibly we can do better, but we just reflect
    // what we have done via uh_set_ptt.
    if (statusbyte & 0x04)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
