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
# my @answer;
my $freq = "14250000";
my $mode = "USB";
my $bw = "2400";
my $vfo = '';
my %rig_state = ();     # State of the rig--freq, mode, passband, ptt, etc.
my %rig_caps = ();      # Rig capabilities from \dump_caps

my $man = 0;
my $help = 0;
my $user_in;
my $ret_val;

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
# testctld specific error values from -100 onward
    CTLD_OK         => "-100",  # testctld -- No error
    CTLD_ENIMPL     => "-103",  # testctld -- %rig_caps reports backend function not implemented
    CTLD_EPROTO     => "-108",  # testctld -- Echoed command mismatch or other error
);

# Error values returned from rigctld by Hamlib value
my %errval = reverse %errstr;


#############################################################################
# Main program
#
#############################################################################

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


# Check rigctld's response to the \chk_blk command to be sure it was
# invoked with the -b|--block option
unless (chk_opt($socket, 'CHKBLK')) {
    die "`rigctld' must be invoked with '-b' or '--block' option for $0\n";
}


print "Welcome to tesctld.pl a program to test `rigctld'\n";
print "Type '?' or 'help' for commands help.\n\n";


# Populate %rig_caps from \dump_caps
$ret_val = dump_caps();

# Tell user what radio rigctld is working with
if ($ret_val eq $errstr{'RIG_OK'}) {
    print "Hamlib Model: $rig_caps{'Caps dump for model'}\t";
    print "Common Name: $rig_caps{'Mfg name'} $rig_caps{'Model name'}\n\n\n";
} else {
    errmsg ($ret_val);
}


# Check rigctld's response to the \chk_vfo command to see if it was
# invoked with the -o|--vfo option.  If true, all commands must include VFO as
# first parameter after the command
if (chk_opt($socket, 'CHKVFO')) {
    $vfo = 'currVFO';       # KISS  One could use the VFO key from %rig_state after calling the \get_vfo command...
}


# Interactive loop
do {
    my @cmd_str;

    print "rigctld command: ";
    chomp($user_in = <>);

    # F, \set_freq
    if ($user_in =~ /^F\s\d+\b$/ or $user_in =~ /^\\set_freq\s\d+\b$/) {
        if ($rig_caps{'Can set frequency'} eq 'Y') {
            # Get the entered frequency value
            @cmd_str = split(' ', $user_in);
            $ret_val = rig_cmd('set_freq', $vfo, $cmd_str[1]);

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # f, \get_freq
    elsif ($user_in =~ /^f\b$/ or $user_in =~ /^\\get_freq\b$/) {
        if ($rig_caps{'Can get mode'} eq 'Y') {
            # Query rig and process result
            $ret_val = rig_cmd('get_freq', $vfo);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "Frequency: $rig_state{Frequency}\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # M, \set_mode
    elsif ($user_in =~ /^M\s[A-Z]+\s\d+\b$/ or $user_in =~ /^\\set_mode\s[A-Z]+\s\d+\b$/) {
        if ($rig_caps{'Can set mode'} eq 'Y') {
            # Get the entered mode and passband values
            @cmd_str = split(' ', $user_in);
            $ret_val = rig_cmd('set_mode', $vfo, $cmd_str[1], $cmd_str[2]);

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
        }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # m, \get_mode
    elsif ($user_in =~ /^m\b$/ or $user_in =~ /^\\get_mode\b$/) {
        if ($rig_caps{'Can get mode'} eq 'Y') {

            # Do the same for the mode (reading the mode also returns the bandwidth)
            $ret_val = rig_cmd('get_mode', $vfo);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "Mode: $rig_state{Mode}\n";
                print "Passband: $rig_state{Passband}\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # V, \set_vfo
    elsif ($user_in =~ /^V\s[A-Za-z]+\b$/ or $user_in =~ /^\\set_vfo\s[A-Za-z]+\b$/) {
        if ($rig_caps{'Can set vfo'} eq 'Y') {
            @cmd_str = split(' ', $user_in);
            $ret_val = rig_cmd('set_vfo', $cmd_str[1]); # $vfo not used!

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # v, \get_vfo
    elsif ($user_in =~ /^v\b$/ or $user_in =~ /^\\get_vfo\b$/) {
        if ($rig_caps{'Can get vfo'} eq 'Y') {
            $ret_val = rig_cmd('get_vfo', $vfo);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "VFO: $rig_state{VFO}\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # 1, \dump_caps
    elsif ($user_in =~ /^1\b$/ or $user_in =~ /^\\dump_caps\b$/) {
        $ret_val = dump_caps();

        if ($ret_val eq $errstr{'RIG_OK'}) {
            print "Model: $rig_caps{'Caps dump for model'}\n";
            print "Manufacturer: $rig_caps{'Mfg name'}\n";
            print "Name: $rig_caps{'Model name'}\n\n";
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

    elsif ($user_in !~ /^(exit|quit)\b$/i) {
        print "Unrecognized command.  Type '?' or 'help' for command help.\n"
    }

} while ($user_in !~ /^(exit|quit)\b$/i);


# Close the connection before we exit.
close($socket);


#############################################################################
# Subroutines for interacting with rigctld
#
#############################################################################


#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# rig_cmd -- Build command string, check returned data, and populate %rig_state
#
# Passed parameters:
# $cmd      = rigctld command
# $vfo      = VFO argument (may be null)
# $p1 - $p3 = \set command parameters
#
# Returns error value from rigctld or local error value if echoed command mismatch
#
sub rig_cmd {
    my ($errno, @answer);
    my $cmd = shift @_;
    my $vfo = shift @_;
    my $p1 = shift @_;
    my $p2 = shift @_;
    my $p3 = shift @_;

    # Add a space to the beginning of the $vfo and $p? arguments
    if ($vfo) {
        $vfo = sprintf("%*s", 1 + length $vfo, $vfo);
    } else { $vfo = ''; }

    if ($p1) {
        $p1 = sprintf("%*s", 1 + length $p1, $p1);
    } else { $p1 = ''; }

    if ($p2) {
        $p2 = sprintf("%*s", 1 + length $p2, $p2);
    } else { $p2 = ''; }

    if ($p3) {
        $p3 = sprintf("%*s", 1 + length $p3, $p3);
    } else { $p3 = ''; }
print "\\$cmd$vfo$p1$p2$p3\n\n";
    # N.B. Terminate query commands with a newline, e.g. "\n" character.
    print $socket "\\$cmd$vfo$p1$p2$p3\n";

    # rigctld echoes the command plus value(s) on "get" along with
    # separate lines for the queried value(s) and the Hamlib return value.
    @answer = get_rigctl($socket);

    if ((shift @answer) =~ /^$cmd:/) {
        $errno = get_errno(pop @answer);

        if ($errno eq $errstr{'RIG_OK'}) {
            # At this point the first line of @answer which is the command string echo
            # and the last line which is the ending block marker and the Hamlib rig
            # function return value have been removed from the array.  What is left
            # over will be stored in the %state hash as a key: value pair for each
            # returned line.

            if (@answer) { get_state(@answer) } # Empty array on \set commands
        }
        return $errno;

    } else {
        return $errstr{'CTLD_EPROTO'};
    }
}


# Get the rig capabilities from Hamlib and store in the %rig_caps hash.
sub dump_caps {
    my ($cmd, $errno, @answer);

    print $socket "\\dump_caps\n";

    @answer = get_rigctl($socket);
    $cmd = shift @answer;

    if ($cmd =~ /^dump_caps:/) {
        $errno = get_errno(pop @answer);

        if ($errno eq $errstr{'RIG_OK'}) {
            get_caps(@answer);
        }
        return $errno;
    } else {
        return $errstr{'RIG_EPROTO'};
    }
}


#############################################################################
# testctld.pl helper functions
#
#############################################################################

# Thanks to Uri Guttman on comp.lang.perl.misc for this function.
# 'RPRT' is the token returned by rigctld to mark the end of the reply block.
sub get_rigctl {
    my $sock = shift @_;
    my @lines;

    while (<$sock>) {
        # rigctld terminates each line with '\n'
        chomp;
        push @lines, $_;
        return @lines if $_ =~ /^RPRT/;
    }
}


# Builds the %rig_state hash from the lines returned by rigctld which are of the
# form "Frequency: 14250000", "Mode: USB", "Passband: 2400", etc.
sub get_state {
    my ($key, $val);

    foreach (@_) {
        ($key, $val) = split(/: /, $_);
        $rig_state{$key} = $val;
    }
}


# Parse the (large) \dump_caps command response into %rig_caps.
# TODO: process all lines of output
sub get_caps {
    my ($key, $val);

    foreach (@_) {
        if (($_ =~ /^Caps .*:/) or
            ($_ =~ /^Model .*:/) or
            ($_ =~ /^Mfg .*:/) or
            ($_ =~ /^Can .*:/)
            ) {
            ($key, $val) = split(/:\s+/, $_);
            $rig_caps{$key} = $val;
        }
    }
}


# Extract the Hamlib error value returned with the last line from rigctld
sub get_errno {

    chomp @_;
    my @errno = split(/ /, $_[0]);

    return $errno[1];
}


# check for block response or VFO mode from rigctld
sub chk_opt {
    my $sock = shift @_;
    my @lines;

    if ($_[0] =~ /^CHKBLK/) {
        print $sock "\\chk_blk\n";
    }
    elsif ($_[0] =~ /^CHKVFO/) {
        print $sock "\\chk_vfo\n";
    }

    while (<$sock>) {
        # rigctld terminates each line with '\n'
        chomp;
        push @lines, $_;     # Should only be one line, but be sure
        last if $_ =~ /^$_[0]/;
    }

    # The CHK* line will have a space separated interger of 0 or 1
    # for `rigctld' invocation without and with -b|--block or
    # -o|--vfo options respectively
    foreach (@lines) {
        if ($_ =~ /^$_[0]\s(\d)/) {
            return $1;
        }
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

# FIXME:  Better argument handling
sub errmsg {

    unless (($_[0] eq $errstr{'CTLD_EPROTO'}) or ($_[0] eq $errstr{'CTLD_ENIMPL'})) {
        print "rigctld returned Hamlib $errval{$_[0]}\n\n";
    }
    elsif ($_[0] eq $errstr{'CTLD_EPROTO'}) {
        print "Echoed command mismatch\n\n";
    }
    elsif ($_[0] eq $errstr{'CTLD_ENIMPL'}) {
        print "Function not yet implemented in Hamlib rig backend\n\n";
    }
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
