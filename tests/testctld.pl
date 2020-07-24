#! /usr/bin/perl

# testctld.pl - (C) 2008,2010 Nate Bargmann, n0nb@arrl.net
# A Perl test script for the rigctld program.
#
#
# It connects to the rigctld TCP port (default 4532) and queries the daemon
# for some common rig information and sets some values.  It also aims to
# provide a bit of example code for Perl scripting.
#
# This program utilizes the Extended Response protocol of rigctld in line
# response mode.  See the rigctld(1) man page for details.

#############################################################################
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
my $vfo = '';
my %rig_state = ();     # State of the rig--freq, mode, passband, ptt, etc.
my %rig_caps = ();      # Rig capabilities from \dump_caps

my $man = 0;
my $help = 0;
my $debug = 0;
my $user_in;
my $ret_val;

# Error values returned from rigctld by Hamlib name
my %errstr = (
    RIG_OK          => "0",     # No error, operation completed successfully
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


print "Welcome to testctld.pl a program to test 'rigctld'\n";
print "Type '?' or 'help' for commands help.\n\n";


# Populate %rig_caps from \dump_caps
$ret_val = dump_caps();

# Tell user what radio rigctld is working with
if ($ret_val eq $errstr{'RIG_OK'}) {
    print "Hamlib Model: " . $rig_caps{'Caps dump for model'} . "\t";
    print "Common Name: " . $rig_caps{'Mfg name'} . ' ' . $rig_caps{'Model name'} . "\n\n\n";
} else {
    errmsg ($ret_val);
}


# Check rigctld's response to the \chk_vfo command to see if it was
# invoked with the -o|--vfo option.  If true, all commands must include VFO as
# first parameter after the command
if (chk_opt($socket, 'CHKVFO')) {
    $vfo = 'currVFO';       # KISS--One could use the VFO key from %rig_state after calling the \get_vfo command...
}


# Interactive loop
do {

    print "rigctld command: ";
    chomp($user_in = <>);

    # F, \set_freq
    if ($user_in =~ /^(F|\\set_freq)\s+(\d+)\b$/) {
        if ($rig_caps{'Can set Frequency'} eq 'Y') {
            # Get the entered frequency value
            print "Freq = $2\n" if $debug;
            $ret_val = rig_cmd('set_freq', $vfo, $2);

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # f, \get_freq
    elsif ($user_in =~ /^(f|\\get_freq)\b$/) {
        if ($rig_caps{'Can get Frequency'} eq 'Y') {
            # Query rig and process result
            $ret_val = rig_cmd('get_freq', $vfo);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "Frequency: " . $rig_state{Frequency} . "\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # M, \set_mode
    elsif ($user_in =~ /^(M|\\set_mode)\s+([A-Z]+)\s+(\d+)\b$/) {
        if ($rig_caps{'Can set Mode'} eq 'Y') {
            # Get the entered mode and passband values
            print "Mode = $2, Passband = $3\n" if $debug;
            $ret_val = rig_cmd('set_mode', $vfo, $2, $3);

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
        }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # m, \get_mode
    elsif ($user_in =~ /^(m|\\get_mode)\b$/) {
        if ($rig_caps{'Can get Mode'} eq 'Y') {

            # Do the same for the mode (reading the mode also returns the bandwidth)
            $ret_val = rig_cmd('get_mode', $vfo);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "Mode: " . $rig_state{Mode} . "\n";
                print "Passband: " . $rig_state{Passband} . "\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # V, \set_vfo
    elsif ($user_in =~ /^(V|\\set_vfo)\s+([A-Za-z]+)\b$/) {
        if ($rig_caps{'Can set VFO'} eq 'Y') {
            print "VFO = $2\n" if $debug;
            $ret_val = rig_cmd('set_vfo', $2); # $vfo not used!

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # v, \get_vfo
    elsif ($user_in =~ /^(v|\\get_vfo)\b$/) {
        if ($rig_caps{'Can get VFO'} eq 'Y') {
            $ret_val = rig_cmd('get_vfo', $vfo);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "VFO: " . $rig_state{VFO} . "\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # J, \set_rit
    elsif ($user_in =~ /^(J|\\set_rit)\s+([+-]?\d+)\b$/) {
        if ($rig_caps{'Can set RIT'} eq 'Y') {
            print "RIT freq = $2\n" if $debug;
            $ret_val = rig_cmd('set_rit', $vfo, $2);

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # j, \get_rit
    elsif ($user_in =~ /^(j|\\get_rit)\b$/) {
        if ($rig_caps{'Can get RIT'} eq 'Y') {
            $ret_val = rig_cmd('get_rit', $vfo);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "RIT: " . $rig_state{RIT} . "\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # Z, \set_xit
    elsif ($user_in =~ /^(Z|\\set_xit)\s+([+-]?\d+)\b$/) {
        if ($rig_caps{'Can set XIT'} eq 'Y') {
            print "XIT freq = $2\n" if $debug;
            $ret_val = rig_cmd('set_xit', $vfo, $2);

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # z, \get_xit
    elsif ($user_in =~ /^(z|\\get_xit)\b$/) {
        if ($rig_caps{'Can get XIT'} eq 'Y') {
            $ret_val = rig_cmd('get_xit', $vfo);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "XIT: " . $rig_state{XIT} . "\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # T, \set_ptt
    elsif ($user_in =~ /^(T|\\set_ptt)\s+(\d)\b$/) {
        if ($rig_caps{'Can set PTT'} eq 'Y') {
            print "PTT = $2\n" if $debug;
            $ret_val = rig_cmd('set_ptt', $vfo, $2);

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # t, \get_ptt
    elsif ($user_in =~ /^(t|\\get_ptt)\b$/) {
        if ($rig_caps{'Can get PTT'} eq 'Y') {
            $ret_val = rig_cmd('get_ptt', $vfo);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "PTT: " . $rig_state{PTT} . "\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # S, \set_split_vfo
    elsif ($user_in =~ /^(S|\\set_split_vfo)\s+(\d)\s+([A-Za-z]+)\b$/) {
        if ($rig_caps{'Can set Split VFO'} eq 'Y') {
            print "split = $2, VFO = $3\n" if $debug;
            $ret_val = rig_cmd('set_split_vfo', $vfo, $2, $3);

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # s, \get_split_vfo
    elsif ($user_in =~ /^(s|\\get_split_vfo)\b$/) {
        if ($rig_caps{'Can get Split VFO'} eq 'Y') {
            $ret_val = rig_cmd('get_split_vfo', $vfo);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "Split: " . $rig_state{Split} . "\n";
                print "TX VFO: " . $rig_state{'TX VFO'} . "\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # I, \set_split_freq
    elsif ($user_in =~ /^(I|\\set_split_freq)\s+(\d+)\b$/) {
        if ($rig_caps{'Can set Split Freq'} eq 'Y') {
            print "TX VFO freq = $2\n" if $debug;
            $ret_val = rig_cmd('set_split_freq', $vfo, $2);

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # i, \get_split_freq
    elsif ($user_in =~ /^(i|\\get_split_freq)\b$/) {
        if ($rig_caps{'Can get Split Freq'} eq 'Y') {
            $ret_val = rig_cmd('get_split_freq', $vfo);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "TX Frequency: " . $rig_state{'TX Frequency'} . "\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # X, \set_split_mode
    elsif ($user_in =~ /^(X|\\set_split_mode)\s+([A-Z]+)\s+(\d+)\b$/) {
        if ($rig_caps{'Can set Split Mode'} eq 'Y') {
            # Get the entered mode and passband values
            print "TX Mode = $2, TX Passband = $3\n" if $debug;
            $ret_val = rig_cmd('set_split_mode', $vfo, $2, $3);

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
        }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # x, \get_split_mode
    elsif ($user_in =~ /^(x|\\get_split_mode)\b$/) {
        if ($rig_caps{'Can get Split Mode'} eq 'Y') {

            # Do the same for the mode (reading the mode also returns the bandwidth)
            $ret_val = rig_cmd('get_split_mode', $vfo);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "TX Mode: " . $rig_state{'TX Mode'} . "\n";
                print "TX Passband: " . $rig_state{'TX Passband'} . "\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # 2, \power2mW
    elsif ($user_in =~ /^(2|\\power2mW)\s+(\d\.\d+)\s+(\d+)\s+([A-Za-z]+)\b$/) {
        if ($rig_caps{'Can get power2mW'} eq 'Y') {
            print "Power = $2, freq = $3, VFO = $4\n" if $debug;
            $ret_val = rig_cmd('power2mW', $2, $3, $4);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "Power mW: " . $rig_state{'Power mW'} . "\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # 4, \mW2power
    elsif ($user_in =~ /^(4|\\mW2power)\s+(\d+)\s+(\d+)\s+([A-Za-z]+)\b$/) {
        if ($rig_caps{'Can get mW2power'} eq 'Y') {
            print "mW = $2, freq = $3, VFO = $4\n" if $debug;
            $ret_val = rig_cmd('mW2power', $2, $3, $4);

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "Power [0.0..1.0]: " . $rig_state{'Power [0.0..1.0]'} . "\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # 1, \dump_caps
    elsif ($user_in =~ /^(1|\\dump_caps)\b$/) {
        $ret_val = dump_caps();

        if ($ret_val eq $errstr{'RIG_OK'}) {
            print "Model: " . $rig_caps{'Caps dump for model'} . "\n";
            print "Manufacturer: " . $rig_caps{'Mfg name'} . "\n";
            print "Name: " . $rig_caps{'Model name'} . "\n\n";
        } else {
            errmsg ($ret_val);
        }
    }

    # ?, help
    elsif ($user_in =~ /^\?|^help\b$/) {
        print <<EOF;

Commands are entered in the same format as described in the rigctld(1)
man page.  e.g. lower case letters call \\get commands and upper case
letters call \\set commands or long command names may be used.

Values passed to set commands are separated by a single space:

F 28400000

\\set_mode USB 2400

See 'man rigctld' for complete command descriptions.

Type 'q' or 'exit' to exit $0.

EOF

    }

    elsif ($user_in !~ /^(exit|q)\b$/i) {
        print "Unrecognized command.  Type '?' or 'help' for command help.\n"
    }

} while ($user_in !~ /^(exit|q)\b$/i);


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

    if (defined $p1) {
        # "Stringify" parameter value then add a space to the beginning of the string
        $p1 .= '';
        $p1 = sprintf("%*s", 1 + length $p1, $p1);
    } else { $p1 = ''; }

    if (defined $p2) {
        $p2 .= '';
        $p2 = sprintf("%*s", 1 + length $p2, $p2);
    } else { $p2 = ''; }

    if (defined $p3) {
        $p3 .= '';
        $p3 = sprintf("%*s", 1 + length $p3, $p3);
    } else { $p3 = ''; }

    print '+\\' . $cmd . $vfo . $p1 . $p2 . $p3 . "\n\n" if $debug;

    # N.B. Terminate query commands with a newline, e.g. "\n" character.
    # N.B. Preceding '+' char to request line separated extended response protocol
    print $socket '+\\' . $cmd . $vfo . $p1 . $p2 . $p3 . "\n";

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

    print $socket "+\\dump_caps\n";

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


# check for VFO mode from rigctld
sub chk_opt {
    my $sock = shift @_;
    my @lines;

    if ($_[0] =~ /^CHKVFO/) {
        print $sock "\\chk_vfo\n";
    }

    while (<$sock>) {
        # rigctld terminates each line with '\n'
        chomp;
        push @lines, $_;     # Should only be one line, but be sure
        last if $_ =~ /^$_[0]/;
    }

    # The CHK* line will have a space separated integer of 0 or 1
    # for 'rigctld' invocation without and with -b|--block or
    # -o|--vfo options respectively
    foreach (@lines) {
        if ($_ =~ /^$_[0]\s(\d)/) {
            return $1;
        }
    }

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


# Parse the command line for supported options.  Print help text as needed.
sub argv_opts {

    # Parse options and print usage if there is a syntax error,
    # or if usage was explicitly requested.
    GetOptions('help|?'     => \$help,
                man         => \$man,
                "port=i"    => \$port,
                "host=s"    => \$host,
                debug       => \$debug
            ) or pod2usage(2);
    pod2usage(1) if $help;
    pod2usage(-verbose => 2) if $man;

}


# POD for pod2usage

__END__

=head1 NAME

testctld.pl - A test and example program for 'rigctld' written in Perl.

=head1 SYNOPSIS

testctld.pl [options]

 Options:
    --host          Hostname or IP address of target 'rigctld' process
    --port          TCP Port of target 'rigctld' process
    --help          Brief help message
    --man           Full documentation
    --debug         Enable debugging output

=head1 DESCRIPTION

B<testcld.pl> provides a set of functions to interactively test the Hamlib
I<rigctld> TCP/IP network daemon.  It also aims to be an example of programming
code to control a radio via TCP/IP in Hamlib.

=head1 OPTIONS

=over 8

=item B<--host>

Hostname or IP address of the target I<rigctld> process.  Default is I<localhost>
which should resolve to 127.0.0.1 if I</etc/hosts> is configured correctly.

=item B<--port>

TCP port of the target I<rigctld> process.  Default is 4532.  Multiple instances
of I<rigctld> will require unique port numbers.

=item B<--help>

Prints a brief help message and exits.

=item B<--man>

Prints this manual page and exits.

=item B<--debug>

Enables debugging output to the console.

=back

=head1 COMMANDS

Commands are the same as described in the rigctld(1) man page.  This is only
a brief summary.

    F, \set_freq        Set frequency in Hz
    f, \get_freq        Get frequency in Hz
    M, \set_mode        Set mode including passband in Hz
    m, \get_mode        Get mode including passband in Hz
    V, \set_vfo         Set VFO (VFOA, VFOB, etc.)
    v, \get_vfo         Get VFO (VFOA, VFOB, etc.)
    J, \set_rit         Set RIT in +/-Hz, '0' to clear
    j, \get_rit         Get RIT in +/-Hz, '0' indicates Off
    Z, \set_xit         Set XIT in +/-Hz, '0' to clear
    z, \get_rit         Get XIT in +/-Hz, '0' indicates Off
    T, \set_ptt         Set PTT, '1' On, '0' Off
    t, \get_ptt         Get PTT, '1' indicates On, '0' indicates Off
    S, \set_split_vfo   Set rig into "split" VFO mode, '1' On, '0' Off
    s, \get_split_vfo   Get status of :split" VFO mode, '1' On, '0' Off
    I, \set_split_freq  Set TX VFO frequency in Hz
    i, \get_split_freq  Get TX VFO frequency in Hz
    X, \set_split_mode  Set TX VFO mode including passband in Hz
    x, \get_split_mode  Get TX VFO mode including passband in Hz
    2, \power2mW        Translate a power value [0.0..1.0] to milliWatts
    4, \mW2power        Translate milliWatts to a power value [0.0..1.0]
    1, \dump_caps       Get the rig capabilities and display select values.

=cut
