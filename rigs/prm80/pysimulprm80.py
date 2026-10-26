#!/usr/bin/env python3

# This file is part of Hamlib
# (C) 2021 Stephane Fillod
#
# SPDX-License-Identifier: GPL-3

"""
 Protocol simulator of PRM80xx running firmware V5.
 This script needs a POSIX system to run.

 On one terminal, start this script:
    $ ./pysimulprm80.py -l /tmp/prm80simul
    Slave name  /dev/pts/5
    Name symlinked  /tmp/prm80simul
    ...
 On another terminal, test the prm80 backend:
    $ rigctl -vvvvvv -r /tmp/prm80simul -m 28001
""" 
# Implementation from 
# - https://github.com/f4fez/prm80/blob/master/doc/Computer_commands_V4.md
# - https://github.com/f4fez/prm80/blob/master/doc/Computer_control.md
# - https://sourceforge.net/p/hamlib/discussion/25919/thread/93afa09f52/
# - https://github.com/f4fez/prm80/blob/master/src/inc_ser.a51

import argparse
import time
import os, pty, tty, termios

class Prm80Simul:

    def __init__(self, pty_r, pty_w):
        self.pty_r = pty_r
        self.pty_w = pty_w
        self.SetupPseudoSerial(self.pty_r)
        self.SetupPseudoSerial(self.pty_w)

        self.msg_version = "PRM8060 V5.1 430"

        self.mdict = {
            'V' : self.tch_Version,
            'N' : self.tch_Nmem,
            'K' : self.tch_Klock,
            'F' : self.tch_Fsquelch,
            'O' : self.tch_Ovolume,
            'D' : self.tch_Dmode,
            'T' : self.tch_Tstate,
            'U' : self.tch_UprintRAM,
            'P' : self.tch_Peditchannel,
            'Q' : self.tch_Qmaxchan,
            'C' : self.tch_Chanlist,
            'R' : self.tch_Rfreq,
            'E' : self.tch_Estate,
            '#' : self.tch_diese,
            '*' : self.tch_autre,
        }
        # Some live data simulated inside the rig
        self.ChanNum = 0
        self.LockByte = 0
        self.Squelch = 0
        self.Volume = 0x10
        self.ModeByte = 0x16
        self.ChanState = 0x0c
        self.MaxChan = 80
        self.RxPLL = 0x7970 # 410 MHz (w/ IF offset)
        self.TxPLL = 0x8020 # 410 MHz
        # channel memory (FW V5): [nominal PLL word, shift in PLL steps, state]
        self.Channels = [[0x8464, 0, 0x04], [0x81B0, 0x0C0, 0x00], [0x8464, 0, 0x0C]]
        self.MaxChan = len(self.Channels) - 1

    def SetupPseudoSerial(self, fd):
        """ make it raw, at 4800 bauds (defaults to 8N1) """
        tty.setraw(fd)
        term_settings = termios.tcgetattr(fd)
        term_settings[4] = termios.B4800  # ispeed
        term_settings[5] = termios.B4800  # ospeed
        termios.tcsetattr(fd, termios.TCSANOW, term_settings)

    def pty_write(self, a):
        """ helper write+flush """
        self.pty_w.write(a)
        self.flush_w()

    def read_echo(self, n):
        """ read n characters for a prompt, the rig echoes them """
        data = pty_r.read(n)
        self.pty_write(data.upper())
        return data.decode('ascii')


    def tch_autre(self):
        """ Unknown command """
        self.pty_write(b'* ?')

    def tch_Version(self):
        """ Print firmware version """
        self.pty_write(self.msg_version.encode())

    def tch_Nmem(self):
        """ Set current channel """
        self.pty_write(b'Channel : ')
        chan = int(self.read_echo(2))
        if chan <= self.MaxChan:
            self.ChanNum = chan
            self.load_channel()

    def load_channel(self):
        """ tune to the current channel (shift 0 = default shift 600 kHz,
            reverse swaps RX and TX) """
        pll, shift, state = self.Channels[self.ChanNum]
        self.ChanState = state
        if shift == 0:
            shift = 48 # 600 kHz
        tx = pll
        if state & 0x01:
            tx = pll + shift if state & 0x04 else pll - shift
        rx = pll
        if state & 0x02:
            rx, tx = tx, rx
        self.RxPLL = rx - 0x06B0 # 430 MHz band: RX PLL = RX - IF (21.4 MHz)
        self.TxPLL = tx

    def tch_Klock(self):
        """ Set lock byte """
        self.pty_write(b'Lock : ')
        self.LockByte = int(self.read_echo(2), 16)

    def tch_Fsquelch(self):
        """ Set squelch. """
        self.pty_write(b'Squelch : ')
        self.Squelch = int(self.read_echo(2))

    def tch_Ovolume(self):
        """ Set volume. """
        self.pty_write(b'Volume : ')
        self.Volume = int(self.read_echo(2))

    def tch_Dmode(self):
        """ Set "Mode" byte. """
        self.pty_write(b'Mode : ')
        self.ModeByte = int(self.read_echo(2), 16)

    def tch_Tstate(self):
        """ Set Channel State (decimal, despite the '$' prompt). """
        self.pty_write(b'Channel state : $')
        self.Channels[self.ChanNum][2] = int(self.read_echo(2))
        self.load_channel()

    def tch_UprintRAM(self):
        """ Print 80c552 internal RAM. """
        self.pty_w.write(b'Display the 256 bytes from internal RAM : \r\n')
        # 16 lines of 16 bytes each. The command needs about 3 sec.
        # In V4, rssi_hold is at 0x53
        self.pty_w.write(b'$00 : 00 00 00 D5 09 00 00 0F 7F 99 D2 7D B2 93 8F C5\r\n')
        self.pty_w.write(b'$10 : 60 8B 43 CA 31 43 BB BA F9 27 87 47 06 52 BE 55\r\n')
        self.pty_w.write(b'$20 : 81 2F FD 17 01 12 A3 00 13 02 34 96 0B 04 54 7A\r\n')
        self.pty_w.write(b'$30 : 13 03 00 30 B4 00 03 00 FC 58 71 00 0F 08 08 82\r\n')
        self.pty_w.write(b'$40 : 9E 86 EE 60 1F 1F 00 FB FF FF 44 55 FF 73 5A F5\r\n')
        self.pty_w.write(b'$50 : 7E 02 06 72 B7 FF 8D B1 92 79 93 8D 3E 4E 8E 06\r\n')
        self.pty_w.write(b'$60 : DB C7 3D 11 AE 5A 4F 3D 80 82 D1 8A 88 6C 7B E5\r\n')
        self.pty_w.write(b'$70 : 2F 98 4C 72 5B A4 78 5C 7D 45 46 8C 25 23 BB 82\r\n')
        self.pty_w.write(b'$80 : 60 1E 2F 42 28 38 E3 7D 94 BC CA B4 B1 43 AE 84\r\n')
        self.pty_w.write(b'$90 : 81 88 75 1A 2D 93 7E E8 CF 79 B6 E0 0F 31 AD AA\r\n')
        self.pty_w.write(b'$A0 : 4F 1C 0F 12 55 00 24 13 08 D2 0E C0 00 20 AF 0E\r\n')
        self.pty_w.write(b'$B0 : 40 FF 40 00 B8 00 0F 0F B8 0F 10 B8 0F 14 10 FC\r\n')
        self.pty_w.write(b'$C0 : 08 E8 D8 3F 33 20 D9 50 8F 1A A2 FF CC D4 2E 3A\r\n')
        self.pty_w.write(b'$D0 : 12 22 B7 23 2F 8F 64 47 99 21 6C D8 B4 B0 C3 51\r\n')
        self.pty_w.write(b'$E0 : 90 E8 67 5C 18 5E AB 2D BD 71 52 0F 96 6F DA 56\r\n')
        self.pty_w.write(b'$F0 : 1D E1 08 42 C2 2D 3B C7 87 23 88 E3 1A 95 F1 EE\r\n')
        self.flush_w()

    def tch_Qmaxchan(self):
        """ Set channels number. """
        self.pty_write(b'Channels number (00 to 99) : ')
        self.MaxChan = int(self.read_echo(2))
        del self.Channels[self.MaxChan + 1:]
        self.MaxChan = len(self.Channels) - 1
        if self.ChanNum > self.MaxChan:
            self.ChanNum = 0
            self.load_channel()
        # This command gets an extra CRLF ??!
        self.pty_write(b'\r\n')

    def tch_Peditchannel(self):
        """ Edit/Add channel (FW V5: with shift value) """
        self.pty_write(b'Channel to set : ')
        chan_num = int(self.read_echo(2))
        self.pty_write(b'\r\n')

        # NB: here prompts end with '$'
        self.pty_write(b'PLL value to load : $')
        pll_freq_value = int(self.read_echo(4), 16)
        self.pty_write(b'\r\n')

        self.pty_write(b'Shift value : $')
        shift_value = int(self.read_echo(4), 16)
        self.pty_write(b'\r\n')

        self.pty_write(b'Channel state : $')
        chan_state = int(self.read_echo(2), 16)
        self.pty_write(b'\r\n')

        channel = [pll_freq_value, shift_value, chan_state]
        if chan_num > self.MaxChan:
            self.pty_write(b'This channel number doesn\'t exist. Add new channel (Y/N) ? ')
            # response "Y/N"
            yesno = pty_r.read(1).decode('ascii')
            self.pty_write(b'\r\n')
            if yesno.upper() != 'Y':
                return
            # the firmware appends the channel after the last one
            self.Channels.append(channel)
            self.MaxChan = len(self.Channels) - 1
        else:
            self.Channels[chan_num] = channel
        self.pty_write(b'\r\n')
        print("Channels", self.Channels)

    def tch_Estate(self):
        """ Show system state (Mode-Chan-Chanstate-Sql-Vol-Lock-RX freq-TX freq-RSSI) (FW V5)"""
        #self.pty_write(b'16000C0010007970802018')
        rssi=24
        self.pty_write('{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:04X}{:04X}{:02X}'.format(
            self.ModeByte,
            self.ChanNum,
            self.ChanState,
            self.Squelch, # low nibble
            self.Volume,
            self.LockByte,
            self.RxPLL,
            self.TxPLL,
            rssi
            ).encode())

    def tch_Chanlist(self):
        """ Print channels list """
        self.pty_write(b'Channel Frequency Shift State\r\n')
        for i, (pll, shift, state) in enumerate(self.Channels):
            self.pty_write('{:02d} {:04X} {:04X} {:02X}\r\n'.format(i, pll, shift, state).encode())

    def tch_Rfreq(self):
        """ Set synthetiser frequencies """
        self.pty_write(b'RX frequency : ')
        self.RxPLL = int(self.read_echo(4), 16)

        self.pty_write(b'\r\nTX frequency : ')
        self.TxPLL = int(self.read_echo(4), 16)

    def tch_diese(self):
        """ Ping """
        self.pty_write(b'!')

    def exec_cmd(self,cmd):
        default='*'
        self.mdict.get(cmd.upper(),default)() # get() method returns the function matching the argument

        self.pty_write(b'\r\n>')
 
    def flush_w(self):
        self.pty_w.flush()


if __name__ == '__main__':

    master, slave = pty.openpty()
    s_name = os.ttyname(slave)
    m_name = os.ttyname(master)
    pty_r = os.fdopen(master, "rb")
    pty_w = os.fdopen(master, "wb")

    parser = argparse.ArgumentParser()
    parser.add_argument("-l", "--symlink",
            help="make a symlink with the opened pty",
            type=str,
            dest='symlink')
    options = parser.parse_args()

    print ("Slave name ", s_name)

    # Create a symlink
    if options.symlink:
        if os.access(options.symlink, os.F_OK, follow_symlinks=False):
            os.remove(options.symlink)
        os.symlink(s_name, options.symlink)
        print ("Name symlinked ", options.symlink)


    prm80 = Prm80Simul(pty_r, pty_w)

    while True:
        cmd = pty_r.read(1).decode('ascii')
        print ("Received command ["+cmd+"]")

        prm80.exec_cmd(cmd)

        print ("Restart mainloop")

