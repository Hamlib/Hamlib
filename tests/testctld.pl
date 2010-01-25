#! /usr/bin/perl

# testctld.pl - (C) 2008,2010 Nate Bargmann, n0nb@arrl.net
# A Perl test script for the rigctld program.

#  $Id$

# It connects to the rigctld TCP port (default 4532) and queries the daemon
# for some common rig information and sets some values.  It also aims to
# provide a bit of example code for Perl scripting.

# This script requires that `rigctld' be invoked with the '-b'|'block' option.
# Details of the block protocol can be found in the rigctld(8) manual page.

# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


# Perl modules this script uses
use warnings;
use strict;
use IO::Socket;

# Global variables
my $socket;
my $host = 'localhost';
my $port = 4532;
my @answer;
my $freq = "14250000";
my $mode = "USB";
my $bw = "2400";
my %state = ();     # State of the rig--freq, mode, passband, ptt, etc.

# Error values returned from rigctld by Hamlib name
my %errstr = (
    RIG_OK          => "0",     # No error, operation completed sucessfully
    RIG_EINVAL      => "-1",    # invalid parameter
    RIG_ECONF       => "-2",    # invalid configuration (serial,..)
    RIG_ENOMEM      => "-3",    # memory shortage
    RIG_ENIMPL      => "-4",    # function not implemented, but will be
    RIG_ETIMEOUT    => "-5",    # communication timed out
    RIG_EIO         => "-6",    # IO error, including open failed
    RIG_EINTERNAL   => "-7",    # Internal Hamlib error, huh?!
    RIG_EPROTO      => "-8",    # Protocol error
    RIG_ERJCTED     => "-9",    # Command rejected by the rig
    RIG_ETRUNC      => "-10",   # Command performed, but arg truncated
    RIG_ENAVAIL     => "-11",   # function not available
    RIG_ENTARGET    => "-12",   # VFO not targetable
    RIG_BUSERROR    => "-13",   # Error talking on the bus
    RIG_BUSBUSY     => "-14",   # Collision on the bus
    RIG_EARG        => "-15",   # NULL RIG handle or any invalid pointer parameter in get arg
    RIG_EVFO        => "-16",   # Invalid VFO
    RIG_EDOM        => "-17",   # Argument out of domain of func
);

# Error values returned from rigctld by Hamlib value
my %errval = reverse %errstr;


#############################################################################
# Main program
#
#############################################################################

# Create the new socket.
# 'localhost' may be replaced by any hostname or IP address where a
# rigctld instance is running.
# Timeout is set to 5 seconds.
$socket = new IO::Socket::INET (PeerAddr    => $host,
                                PeerPort    => $port,
                                Proto       => 'tcp',
                                Type        => SOCK_STREAM,
                                Timeout     => 5 )
    or die $@;


# Query rigctld for the rig's frequency
get_freq();
print "The rig's frequency is: $state{Frequency}\n";
print "\n";

# Do the same for the mode (reading the mode also returns the bandwidth)
get_mode();
print "The rig's mode is: $state{Mode}\n";
print "The rig's passband is: $state{Passband}\n";
print "\n";

# Setting the mode takes two parameters, mode and bandwidth
print "Setting the rig's mode to $mode and bandwidth to $bw\n";
print "\n";
set_mode($mode, $bw);

get_mode();
print "The rig's mode is now: $state{Mode}\n";
print "The rig's passband is now: $state{Passband}\n";
print "\n";

# Now set the rig's frequency
print "Setting the rig's frequency to: $freq\n";
set_freq($freq);

get_freq();
print "The rig's frequency is now: $state{Frequency}\n";
print "\n";

# Close the connection before we exit.
close($socket);


#############################################################################
# Subroutines
#
#############################################################################


# Thanks to Uri Guttman on comp.lang.perl.misc for this function.
# 'RPRT' is the token returned by rigctld to mark the end of the reply block.
sub get_rigctl {
    my ($sock) = @_;
    my @lines;

    while (my $line = <$sock>) {
        # rigctld terminates each line with '\n'
        chomp $line;
        push @lines, $line;
        return @lines if $line =~ /^RPRT/;
    }
}


# Extract the Hamlib error value returned with the last line from rigctld
sub get_errno {

    chomp @_;
    my @errno = split(/ /, $_[0]);

    return $errno[1];
}


# Builds the %state hash from the lines returned by rigctld which are of the
# form "Frequency: 14250000"
sub get_state {

    foreach my $line (@_) {
        (my $key, my $val) = split(/: /, $line);
        $state{$key} = $val;
    }
}


sub get_freq {
    my ($cmd, $errno);

    # N.B. Terminate query commands with a newline, e.g. "\n" character.
    print $socket "\\get_freq\n";

    # Get the rig's frequency block from rigctld
    @answer = get_rigctl($socket);

    # Make sure we got the right response
    $cmd = shift @answer;

    if ($cmd =~ /^get_freq:/) {
        $errno = get_errno(pop @answer);

        # At this point the first line of @answer which is the command string echo
        # and the last line which is the ending block marker and the Hamlib rig
        # function return value have been removed from the array.  What is left
        # over will be stored in the %state hash as a key: value pair.

        if ($errno eq $errstr{"RIG_OK"}) {
            get_state(@answer);
        }
        elsif ($errno lt $errstr{"RIG_OK"}) {
            print "Hamlib returned $errval{$errno}\n";
            close($socket);
            exit (1);
        }
    }
}


sub get_mode {
    my ($cmd, $errno);

    print $socket "\\get_mode\n";

    @answer = get_rigctl($socket);
    $cmd = shift @answer;

    if ($cmd =~ /^get_mode:/) {
        $errno = get_errno(pop @answer);

        if ($errno eq $errstr{"RIG_OK"}) {
            get_state(@answer);
        }
        elsif ($errno lt $errstr{"RIG_OK"}) {
            print "Hamlib returned $errval{$errno}\n";
            close($socket);
            exit (1);
        }
    }
}

sub set_freq {
    my ($cmd, $errno);
    my ($freq) = @_;

    print $socket "\\set_freq $freq\n";

    # rigctld echoes the command plus value(s) on "set" along with
    # the Hamlib return value.
    @answer = get_rigctl($socket);
    $cmd = shift @answer;

    if ($cmd =~ /^set_freq:/) {
        $errno = get_errno(pop @answer);

        # As $cmd contains a copy of the line printed to $socket as returned
        # by rigctld, it can be split and the value(s) following the command
        # tested to see that it matches the passed in value, etc.

        if ($errno lt $errstr{"RIG_OK"}) {
            print "Hamlib returned $errval{$errno}\n";
            close($socket);
            exit (1);
        }
    }
}


sub set_mode {
    my ($cmd, $errno);
    my ($mode, $bw) = @_;

    # Setting the mode takes two values, mode and bandwidth.  All on the same line.
    print $socket "\\set_mode $mode $bw\n";

    @answer = get_rigctl($socket);
    $cmd = shift @answer;

    if ($cmd =~ /^set_mode:/) {
        $errno = get_errno(pop @answer);

        if ($errno lt $errstr{"RIG_OK"}) {
            print "Hamlib returned $errval{$errno}\n";
            close($socket);
            exit (1);
        }
    }
}

