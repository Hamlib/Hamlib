#! /usr/bin/perl

# testrotctld.pl - (C) 2008,2010 Nate Bargmann, n0nb@arrl.net
# A Perl test script for the rotctld program.
#
#
# It connects to the rotctld TCP port (default 4533) and queries the daemon
# for some common rot information and sets some values.  It also aims to
# provide a bit of example code for Perl scripting.
#
# This program utilizes the Extended Response protocol of rotctld in line
# response mode.  See the rotctld(1) man page for details.

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
my $port = 4533;
my %rot_state = ();     # State of the rotor--position, etc.
my %rot_caps = ();      # Rotor capabilities from \dump_caps

my $man = 0;
my $help = 0;
my $debug = 0;
my $user_in;
my $ret_val;

# Error values returned from rotctld by Hamlib name
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
    RIG_ERJCTED     => "-9",    # Command rejected by the rot
    RIG_ETRUNC      => "-10",   # Command performed, but arg truncated
    RIG_ENAVAIL     => "-11",   # function not available
    RIG_ENTARGET    => "-12",   # VFO not targetable
    RIG_BUSERROR    => "-13",   # Error talking on the bus
    RIG_BUSBUSY     => "-14",   # Collision on the bus
    RIG_EARG        => "-15",   # NULL RIG handle or any invalid pointer parameter in get arg
    RIG_EVFO        => "-16",   # Invalid VFO
    RIG_EDOM        => "-17",   # Argument out of domain of func
# testctld specific error values from -100 onward
    CTLD_OK         => "-100",  # testrotctld -- No error
    CTLD_ENIMPL     => "-103",  # testrotctld -- %rot_caps reports backend function not implemented
    CTLD_EPROTO     => "-108",  # testrotctld -- Echoed command mismatch or other error
);

# Error values returned from rotctld by Hamlib value
my %errval = reverse %errstr;


# Rotor '\move' command token values
my %direct = (
    UP      =>  '2',
    DOWN    =>  '4',
    LEFT    =>  '8',
    CCW     =>  '8',    # Synonym for LEFT
    RIGHT   =>  '16',
    CW      =>  '16',   # Synonym for RIGHT
);


#############################################################################
# Main program
#
#############################################################################

# Parse command line options
argv_opts();

# Create the new socket.
# 'localhost' may be replaced by any hostname or IP address where a
# rotctld instance is running.
# Timeout is set to 5 seconds.
$socket = new IO::Socket::INET (PeerAddr    => $host,
                                PeerPort    => $port,
                                Proto       => 'tcp',
                                Type        => SOCK_STREAM,
                                Timeout     => 5 )
    or die $@;


print "Welcome to testrotctld.pl a program to test 'rotctld'\n";
print "Type '?' or 'help' for commands help.\n\n";


# Populate %rot_caps from \dump_caps
$ret_val = dump_caps();

# Tell user what rotor rotctld is working with
if ($ret_val eq $errstr{'RIG_OK'}) {
    print "Hamlib Model: " . $rot_caps{'Caps dump for model'} . "\t";
    print "Common Name: " . $rot_caps{'Mfg name'} . ' ' . $rot_caps{'Model name'} . "\n\n\n";
} else {
    errmsg ($ret_val);
}


# Interactive loop
do {

    print "rotctld command: ";
    chomp($user_in = <>);

    # P, \set_pos
    if ($user_in =~ /^(P|\\set_pos)\s+([-+]?([0-9]*\.)?[0-9]+)\s+([-+]?([0-9]*\.)?[0-9]+)\b$/) {
        if ($rot_caps{'Can set Position'} eq 'Y') {
            # Get the entered az and el values
            print "Az = $2, El = $4\n" if $debug;
            $ret_val = rot_cmd('set_pos', $2, $4);

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # p, \get_pos
    elsif ($user_in =~ /^(p|\\get_pos)\b$/) {
        if ($rot_caps{'Can get Position'} eq 'Y') {
            # Query rot and process result
            $ret_val = rot_cmd('get_pos');

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "Azimuth: " . $rot_state{Azimuth} . "\n";
                print "Elevation: " . $rot_state{Elevation} . "\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # M, \move
    elsif ($user_in =~ /^(M|\\move)\s+([A-Z]+)\s+(\d+)\b$/) {
        if ($rot_caps{'Can Move'} eq 'Y') {
            # Get the entered mode and passband values
            print "Move = $direct{$2}, Speed = $3\n" if $debug;
            $ret_val = rot_cmd('move', $direct{$2}, $3);

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
        }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # S, \stop
    elsif ($user_in =~ /^(S|\\stop)\b$/) {
        if ($rot_caps{'Can Stop'} eq 'Y') {
            print "Stop\n" if $debug;
            $ret_val = rot_cmd('stop'); # $vfo not used!

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # K, \park
    elsif ($user_in =~ /^(K|\\park)\b$/) {
        if ($rot_caps{'Can Park'} eq 'Y') {
            print "Park\n" if $debug;
            $ret_val = rot_cmd('park');

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # R, \reset
    elsif ($user_in =~ /^(R|\\reset)\s+(\d)\b$/) {
        if ($rot_caps{'Can Reset'} eq 'Y') {
            print "Reset\n" if $debug;
            $ret_val = rot_cmd('reset', $2);

            unless ($ret_val eq $errstr{'RIG_OK'}) {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # _, \get_info
    elsif ($user_in =~ /^(_|\\get_info)\b$/) {
        if ($rot_caps{'Can get Info'} eq 'Y') {
            print "Get info\n" if $debug;
            $ret_val = rot_cmd('get_info');

            if ($ret_val eq $errstr{'RIG_OK'}) {
                print "Info: " . $rot_state{Info} . "\n\n";
            } else {
                errmsg ($ret_val);
            }
        } else {
            errmsg($errstr{'CTLD_ENIMPL'});
        }
    }

    # L, \lonlat2loc
    elsif ($user_in =~ /^(L|\\lonlat2loc)\s+([-+]?([0-9]*\.)?[0-9]+)\s+([-+]?([0-9]*\.)?[0-9]+)\s+(\d+)\b$/) {
        print "Longitude = $2, Latitude = $4, Length = $6\n" if $debug;
        $ret_val = rot_cmd('lonlat2loc', $2, $4, $6);

        if ($ret_val eq $errstr{'RIG_OK'}) {
             print "Locator: " . $rot_state{Locator} . "\n\n";
        } else {
            errmsg ($ret_val);
        }
    }

    # l, \loc2lonlat
    elsif ($user_in =~ /^(l|\\loc2lonlat)\s+([A-Za-z0-9]+)\b$/) {
        print "Locator = $2\n" if $debug;
        $ret_val = rot_cmd('loc2lonlat', $2);

        if ($ret_val eq $errstr{'RIG_OK'}) {
             print "Longitude: " . $rot_state{Longitude} . "\n";
             print "Latitude: " . $rot_state{Latitude} . "\n\n";
        } else {
            errmsg ($ret_val);
        }
    }

    # D, \dms2dec
    elsif ($user_in =~ /^(D|\\dms2dec)\s+[+-]?(\d+)\s+(\d+)\s+(([0-9]*\.)?[0-9]+)\s+(\d)\b$/) {
        print "Degrees = $2, Minutes = $3, Seconds = $4, S/W = $6\n" if $debug;
        $ret_val = rot_cmd('dms2dec', $2, $3, $4, $6);

        if ($ret_val eq $errstr{'RIG_OK'}) {
             print "Decimal Degrees: " . $rot_state{'Dec Degrees'} . "\n\n";
        } else {
            errmsg ($ret_val);
        }
    }

    # d, \dec2dms
    elsif ($user_in =~ /^(d|\\dec2dms)\s+([-+]?([0-9]*\.)?[0-9]+)\b$/) {
        print "Decimal Degrees = $2\n" if $debug;
        $ret_val = rot_cmd('dec2dms', $2);

        if ($ret_val eq $errstr{'RIG_OK'}) {
             print "Degrees: " . $rot_state{Degrees} . "\n";
             print "Minutes: " . $rot_state{Minutes} . "\n";
             print "Seconds: " . $rot_state{Seconds} . "\n";
             print "South/West: " . $rot_state{'S/W'} . "\n\n";
        } else {
            errmsg ($ret_val);
        }
    }

    # E, \dmmm2dec
    elsif ($user_in =~ /^(E|\\dmmm2dec)\s+[+-]?(\d+)\s+(([0-9]*\.)?[0-9]+)\s+(\d)\b$/) {
        print "Degrees = $2, Minutes = $3, S/W = $5\n" if $debug;
        $ret_val = rot_cmd('dmmm2dec', $2, $3, $5);

        if ($ret_val eq $errstr{'RIG_OK'}) {
             print "Decimal Degrees: " . $rot_state{'Dec Deg'} . "\n\n";
        } else {
            errmsg ($ret_val);
        }
    }

    # e, \dec2dmmm
    elsif ($user_in =~ /^(e|\\dec2dmmm)\s+([-+]?([0-9]*\.)?[0-9]+)\b$/) {
        print "Decimal Degrees = $2\n" if $debug;
        $ret_val = rot_cmd('dec2dmmm', $2);

        if ($ret_val eq $errstr{'RIG_OK'}) {
             print "Degrees: " . $rot_state{Degrees} . "\n";
             print "Decimal Minutes: " . $rot_state{'Dec Minutes'} . "\n";
             print "South/West: " . $rot_state{'S/W'} . "\n\n";
        } else {
            errmsg ($ret_val);
        }
    }


    # B, \qrb
    elsif ($user_in =~ /^(B|\\qrb)\s+([-+]?([0-9]*\.)?[0-9]+)\s+([-+]?([0-9]*\.)?[0-9]+)\s+([-+]?([0-9]*\.)?[0-9]+)\s+([-+]?([0-9]*\.)?[0-9]+)\b$/) {
        print "Lon 1 = $2, Lat 1 = $4, Lon 2 = $6, Lat 2 = $8\n" if $debug;
        $ret_val = rot_cmd('qrb', $2, $4, $6, $8);

        if ($ret_val eq $errstr{'RIG_OK'}) {
             print "Distance (km): " . $rot_state{'QRB Distance'} . "\n";
             print "Azimuth (Deg): " . $rot_state{'QRB Azimuth'} . "\n\n";
        } else {
            errmsg ($ret_val);
        }
    }

    # A, \a_sp2a_lp
    elsif ($user_in =~ /^(A|\\a_sp2a_lp)\s+([-+]?([0-9]*\.)?[0-9]+)\b$/) {
        print "Short Path Degrees = $2\n" if $debug;
        $ret_val = rot_cmd('a_sp2a_lp', $2);

        if ($ret_val eq $errstr{'RIG_OK'}) {
             print "Long Path Degrees: " . $rot_state{'Long Path Deg'} . "\n\n";
            } else {
            errmsg ($ret_val);
        }
    }

    # a, \d_sp2d_lp
    elsif ($user_in =~ /^(a|\\d_sp2d_lp)\s+([-+]?([0-9]*\.)?[0-9]+)\b$/) {
        print "Short Path km = $2\n" if $debug;
        $ret_val = rot_cmd('d_sp2d_lp', $2);

        if ($ret_val eq $errstr{'RIG_OK'}) {
             print "Long Path km: " . $rot_state{'Long Path km'} . "\n\n";
            } else {
            errmsg ($ret_val);
        }
    }

    # 1, \dump_caps
    elsif ($user_in =~ /^(1|\\dump_caps)\b$/) {
        $ret_val = dump_caps();

        if ($ret_val eq $errstr{'RIG_OK'}) {
            print "Model: " . $rot_caps{'Caps dump for model'} . "\n";
            print "Manufacturer: " . $rot_caps{'Mfg name'} . "\n";
            print "Name: " . $rot_caps{'Model name'} . "\n\n";
        } else {
            errmsg ($ret_val);
        }
    }

    # ?, help
    elsif ($user_in =~ /^\?|^help\b$/) {
        print <<EOF;

Commands are entered in the same format as described in the rotctld(1)
man page.  e.g. generally lower case letters call \\get commands and upper
case letters call \\set commands or long command names may be used.  An
exception are the locator commands where paired conversions are arbitrarily
assigned upper and lower case commands.

Values passed to set commands are separated by a single space:

P 150.75 22.5

\\get_pos

See 'man rotctld' for complete command descriptions.

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
# Subroutines for interacting with rotctld
#
#############################################################################


#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# rot_cmd -- Build command string, check returned data, and populate %rot_state
#
# Passed parameters:
# $cmd      = rotctld command
# $p1 - $p4 = \set command parameters
#
# Returns error value from rotctld or local error value if echoed command mismatch
#
sub rot_cmd {
    my ($errno, @answer);
    my $cmd = shift @_;
    my $p1 = shift @_;
    my $p2 = shift @_;
    my $p3 = shift @_;
    my $p4 = shift @_;

    # Add a space to the beginning of the $p? arguments
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

    if (defined $p4) {
        $p4 .= '';
        $p4 = sprintf("%*s", 1 + length $p4, $p4);
    } else { $p4 = ''; }

    print 'Command: +\\' . $cmd . $p1 . $p2 . $p3 . $p4 . "\n\n" if $debug;

    # N.B. Terminate query commands with a newline, e.g. "\n" character.
    # N.B. Preceding '+' char to request block or extended response protocol
    print $socket '+\\' . $cmd . $p1 . $p2 . $p3 . $p4 . "\n";

    # rotctld echoes the command plus value(s) on "get" along with
    # separate lines for the queried value(s) and the Hamlib return value.
    @answer = get_rotctl($socket);

    if ((shift @answer) =~ /^$cmd:/) {
        $errno = get_errno(pop @answer);

        if ($errno eq $errstr{'RIG_OK'}) {
            # At this point the first line of @answer which is the command string echo
            # and the last line which is the ending block marker and the Hamlib rot
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


# Get the rot capabilities from Hamlib and store in the %rot_caps hash.
sub dump_caps {
    my ($cmd, $errno, @answer);

    print $socket "+\\dump_caps\n";

    @answer = get_rotctl($socket);
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
# testrotctld.pl helper functions
#
#############################################################################

# Thanks to Uri Guttman on comp.lang.perl.misc for this function.
# 'RPRT' is the token returned by rotctld to mark the end of the reply block.
sub get_rotctl {
    my $sock = shift @_;
    my @lines;

    while (<$sock>) {
        # rotctld terminates each line with '\n'
        chomp;
        push @lines, $_;
        return @lines if $_ =~ /^RPRT/;
    }
}


# Builds the %rot_state hash from the lines returned by rotctld which are of the
# form "Azimuth: 90.000000, elevation: 45.000000", etc.
sub get_state {
    my ($key, $val);

    foreach (@_) {
        ($key, $val) = split(/: /, $_);
        $rot_state{$key} = $val;
    }
}


# Parse the (large) \dump_caps command response into %rot_caps.
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
            $rot_caps{$key} = $val;
        }
    }
}


# Extract the Hamlib error value returned with the last line from rotctld
sub get_errno {

    chomp @_;
    my @errno = split(/ /, $_[0]);

    return $errno[1];
}


# FIXME:  Better argument handling
sub errmsg {

    unless (($_[0] eq $errstr{'CTLD_EPROTO'}) or ($_[0] eq $errstr{'CTLD_ENIMPL'})) {
        print "rotctld returned Hamlib $errval{$_[0]}\n\n";
    }
    elsif ($_[0] eq $errstr{'CTLD_EPROTO'}) {
        print "Echoed command mismatch\n\n";
    }
    elsif ($_[0] eq $errstr{'CTLD_ENIMPL'}) {
        print "Function not yet implemented in Hamlib rot backend\n\n";
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

testctld.pl - A test and example program for 'rotctld' written in Perl.

=head1 SYNOPSIS

testctld.pl [options]

 Options:
    --host          Hostname or IP address of target 'rotctld' process
    --port          TCP Port of target 'rotctld' process
    --help          Brief help message
    --man           Full documentation
    --debug         Enable debugging output

=head1 DESCRIPTION

B<testcld.pl> provides a set of functions to interactively test the Hamlib
I<rotctld> TCP/IP network daemon.  It also aims to be an example of programming
code to control a rotor via TCP/IP in Hamlib.

=head1 OPTIONS

=over 8

=item B<--host>

Hostname or IP address of the target I<rotctld> process.  Default is I<localhost>
which should resolve to 127.0.0.1 if I</etc/hosts> is configured correctly.

=item B<--port>

TCP port of the target I<rotctld> process.  Default is 4533.  Multiple instances
of I<rotctld> will require unique port numbers.

=item B<--help>

Prints a brief help message and exits.

=item B<--man>

Prints this manual page and exits.

=item B<--debug>

Enables debugging output to the console.

=back

=head1 COMMANDS

Commands are the same as described in the rotctld(1) man page.  This is only
a brief summary.

    P, \set_pos         Set the rotor's Azimuth and Elevation
    p, \get_pos         Get the rotor's Azimuth and Elevation
    M. \move            Move Up, Down, Left, Right at Speed
    S, \stop            Stop rotation
    K, \park            Set the rotor to the park position
    R, \reset           Reset the rotor
    _, \get_info        Get the rotor Model Name
    1, \dump_caps       Get the rot capabilities and display select values

These commands are for the locator support API.

    L, \lonlat2loc      Convert Longitude and Latitude to Maidenhead square
    l, \loc2lonlat      Convert Maidenhead square to Longitude and Latitude
    D, \dms2dec         Convert Degrees, Minutes, Seconds to Decimal Degrees
    d, \dec2dms         Convert Decimal Degrees to Degrees, Minutes, Seconds
    E, \dmmm2dec        Convert Degrees, Decimal Minutes to Decimal Degrees
    e, \dec2dmmm        Convert Decimal Degrees to Degrees, Decimal Minutes
    B, \qrb             Compute distance and azimuth between Lon 1, Lat 1,
                        and Lon 2, Lat 2
    A, \a_sp2a_lp       Compute Long Path Azimuth from Short Path Azimuth
    a, \d_sp2d_lp       Compute Long Path Distance from Short Path Distance

=cut
