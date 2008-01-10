#! /usr/bin/perl

# testctld.pl - (C) Nate Bargmann 2008
# A Perl test script for the rigctld program.

#  $Id: testctld.pl,v 1.3 2008-01-10 03:42:35 n0nb Exp $

# It connects to the rigctld TCP port (default 4532) and queries
# the daemon for some common rig information.  It also aims to provide
# a bit of example code for Perl scripting.

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


# Local variables
my $socket;
my @answer;
my $freq = "14250000";
my $mode = "USB";
my $bw = "2400";
my $flags;

# Thanks to Uri Guttman on comp.lang.perl.misc for this function
sub get_results {

	my ($sock) = @_;
	my @lines;

	while (my $line = <$sock>) {

		return @lines if $line =~ /^END$/;
		push @lines, $line;
	}
}

# Create the new socket.  
# 'localhost' may be replaced by any hostname or IP address where a 
# rigctld instance is running.
# Timeout is set to 5 seconds.
$socket = new IO::Socket::INET (PeerAddr    => 'localhost',
                                PeerPort    => 4532,
                                Proto       => 'tcp',
                                Type        => SOCK_STREAM,
                                Timeout     => 5 )
    or die $@;

# Query rigctld for the rig's frequency
# N.B. Terminate query commands with a newline, e.g. "\n" character.
print $socket "f\n";

# Get the rig's frequency from rigctld and print it to STDOUT
# N.B. Replies are newline terminated, so lines in @answer end with '\n'.
@answer = get_results($socket);

print "The rig's frequency is: $answer[0]";

# Extra newline for screen formatting.
print "\n";

# Do the same for the mode (reading the mode also returns the bandwidth)
print $socket "m\n";
@answer = get_results($socket);
print "The rig's mode is: $answer[0]";
print "The rig's bandwidth is: $answer[1]";
print "\n";

# Now set the rig's frequency
print "Setting the rig's frequency to: $freq\n";
print $socket "F $freq\n";
print $socket "f\n";
@answer = get_results($socket);
print "The rig's frequency is now: $answer[0]";
print "\n";

# Setting the mode takes two parameters, mode and bandwidth
print "Setting the rig's mode to $mode and bandwidth to $bw\n";
print $socket "\\set_mode $mode $bw\n";
print $socket "\\get_mode\n";
@answer = get_results($socket);
print "The rig's mode is now: $answer[0]";
print "The rig's bandwidth is now: $answer[1]";
print "\n";

# Close the connection before we exit.
close($socket);

