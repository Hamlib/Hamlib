#! /usr/bin/perl

# testctld.pl - (C) 2008,2010 Nate Bargmann, n0nb@arrl.net
# A Perl test script for the rigctld program.

#  $Id$

# It connects to the rigctld TCP port (default 4532) and queries the daemon
# for some common rig information and sets some values.  It also aims to
# provide a bit of example code for Perl scripting.
#
# This script requires that `rigctld' be invoked with the '-b'|'--block' option.
# Details of the Block protocol can be found in the rigctld(8) manual page.

#############################################################################
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
#
# See the file 'COPYING' in the main Hamlib distribution directory for the
# complete text of the GNU Public License version 2.
#
#############################################################################


# Perl modules this script uses
use warnings;
use strict;
use IO::Socket;
use Getopt::Long;
use Pod::Usage;

# Global variables
my $socket;
my $host = 'localhost';
my $port = 4532;
my @answer;
my $freq = "14250000";
my $mode = "USB";
my $bw = "2400";
my %state = ();     # State of the rig--freq, mode, passband, ptt, etc.

my $man = 0;
my $help = 0;
my $user_in;
my $ret_val;
my @cmd_str;

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

print "Welcome to tesctld.pl a program to test `rigctld'\n\n";
print "Type '?' or 'help' for commands help.\n\n";

# Parse command line options
argv_opts();

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


# Interactive loop
do {

    print "rigctld command: ";
    chomp($user_in = <>);

    # F, \set_freq
    if ($user_in =~ /^F\s\d+\b$/ or $user_in =~ /^\\set_freq\s\d+\b$/) {
        # Get the frequency value
        @cmd_str = split(' ', $user_in);
        $ret_val = set_freq($cmd_str[1]);

        unless ($ret_val eq $errstr{"RIG_OK"}) {
            errmsg ($ret_val);
        }
    }

    # f, \get_freq
    elsif ($user_in =~ /^f\b$/ or $user_in =~ /^\\get_freq\b$/) {
        # Query rig and process result
        $ret_val = get_freq();

        if ($ret_val eq $errstr{"RIG_OK"}) {
            print "Frequency: $state{Frequency}\n\n";
        } else {
            errmsg ($ret_val);
        }
    }

    # M, \set_mode
    elsif ($user_in =~ /^M\s[A-Z]+\s\d+\b$/ or $user_in =~ /^\\set_mode\s[A-Z]+\s\d+\b$/) {
        # Get the mode and passband value
        @cmd_str = split(' ', $user_in);
        $ret_val = set_mode($cmd_str[1], $cmd_str[2]);

        unless ($ret_val eq $errstr{"RIG_OK"}) {
            errmsg ($ret_val);
        }
    }

    # m, \get_mode
    elsif ($user_in =~ /^m\b$/ or $user_in =~ /^\\get_mode\b$/) {
        # Do the same for the mode (reading the mode also returns the bandwidth)
        $ret_val = get_mode();

        if ($ret_val eq $errstr{"RIG_OK"}) {
            print "Mode: $state{Mode}\n";
            print "Passband: $state{Passband}\n\n";
        } else {
            errmsg ($ret_val);
        }
    }

    # ?, help
    elsif ($user_in =~ /^\?$/ or $user_in =~ /^help\b$/i) {
        print <<EOF;

Commands are entered in the same format as described in the rigctld(8)
man page.  e.g. lower case letters call \\get commands and upper case
letters call \\set commands or long command names may be used.

Values passed to set commands are separated by a single space:

F 28400000

\\set_mode USB 2400

See `man rigctld' for complete command descriptions.

Type 'exit' or 'quit' to exit $0.

EOF

    }

    else {
        print "Unrecognized command.  Type '?' or 'help' for command help.\n"
    }

} while ($user_in !~ /^(exit|quit)\b$/i);


# Close the connection before we exit.
close($socket);


#############################################################################
# Subroutines for interacting with rigctld
#
#############################################################################


sub set_freq {
    my $cmd;
    my ($freq) = @_;

    # N.B. Terminate query commands with a newline, e.g. "\n" character.
    print $socket "\\set_freq $freq\n";

    # rigctld echoes the command plus value(s) on "set" along with
    # the Hamlib return value.
    @answer = get_rigctl($socket);

    # At this point the first line of @answer contains the command string echo
    # and the last line contains the ending block marker and the Hamlib rig
    # function return value.  No data lines are returned from a \\set_ command.

    $cmd = shift @answer;

    if ($cmd =~ /^set_freq:/) {
        return get_errno(pop @answer);
    } else {
        # Oops!  Something went very wrong.
        return $errstr{"RIG_EPROTO"};
    }
}


sub get_freq {
    my ($cmd, $errno);

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
        return $errno;
    } else {
        return $errstr{"RIG_EPROTO"};
    }
}


sub set_mode {
    my $cmd;
    my ($mode, $bw) = @_;

    # Setting the mode takes two values, mode and bandwidth.  All on the same line.
    print $socket "\\set_mode $mode $bw\n";

    @answer = get_rigctl($socket);
    $cmd = shift @answer;

    if ($cmd =~ /^set_mode:/) {
        return get_errno(pop @answer);
    } else {
        return $errstr{"RIG_EPROTO"};
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
        return $errno;
    } else {
        return $errstr{"RIG_EPROTO"};
    }
}


#############################################################################
# testctld.pl helper functions
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


# Parse the command line for supported options.  Print help text as needed.
sub argv_opts {

    # Parse options and print usage if there is a syntax error,
    # or if usage was explicitly requested.
    GetOptions('help|?'     => \$help,
                man         => \$man,
                "port=i"    => \$port,
                "host=s"    => \$host
            ) or pod2usage(2);
    pod2usage(1) if $help;
    pod2usage(-verbose => 2) if $man;

}


sub errmsg {

    print "Hamlib returned $errval{$_[0]}\n\n";
}


# POD for pod2usage

__END__

=head1 NAME

testctld.pl - A test and example program for `rigctld' written in Perl.

=head1 SYNOPSIS

testctld.pl [options]

 Options:
    --host          Hostname or IP address of target `rigctld' process
    --port          TCP Port of target `rigctld' process
    --help          Brief help message
    --man           Full documentation

=head1 DESCRIPTION

B<testcld.pl> provides a set of functions to interactively test the Hamlib
`rigctld' TCP/IP network daemon.  It also aims to be an example of programming
code to control a radio via TCP/IP in Hamlib.

=head1 OPTIONS

=over 8

=item B<--host>

Hostname or IP address of the target `rigctld' process.  Default is 'localhost'
which should resolve to 127.0.0.1 if I</etc/hosts> is configured correctly.

=item B<--port>

TCP port of the target `rigctld' process.  Default is 4532.  Mutliple instances
of `rigctld' will require unique port numbers.

=item B<--help>

Prints a brief help message and exits.

=item B<--man>

Prints this manual page and exits.

=back

=cut
