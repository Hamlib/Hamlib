#! /usr/bin/perl

# testctld.pl - (C) Nate Bargmann 2008
# A Perl test script for the rigctld program.

#  $Id$

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
use IO::Socket::INET;


# Local variables
my $socket;
my @answer;
my $get_freq;
my $get_mode;
my $get_bw;
my $flags;
# values to set rig
my $set_freq = "14250000";
my $set_mode = "USB";
my $set_bw = "2400";


# Thanks to Uri Guttman on comp.lang.perl.misc for this function
sub get_results {

	my ($sock) = @_;
	my @lines;
	my $errno;
	my $line;
#	my $x;

	do {
	while ( !($line = $sock->getline)) { ;}
		print $line;

#		return @lines if $line =~ /^RPRT\s+0$/;
		if ($line) {
			print $line;
			push @lines, $line;
		}
#		else {
#			return @lines;
#		}
		#if ($line =~ /^RPRT.*$/) {
			#print $line;
			#$errno = (split $line)[1];
			#print $errno;
			#unless ($errno) {
				#return @lines;
			#}
			#else {
				#return $errno * -1;
			#}
		#}
		#else {
			#push @lines, $line;
		#}
	} until ($line ne "");
	return @lines;
}

# Create the new socket.
# 'localhost' may be replaced by any hostname or IP address where a
# rigctld instance is running.
# Timeout is set to 5 seconds.
$socket = IO::Socket::INET->new(PeerAddr    => 'localhost',
                                PeerPort    => 4532,
                                Proto       => 'tcp',
                                Type        => SOCK_STREAM,
                                Timeout     => 5,
                                Blocking	=> 0 )
    or die $@;

# Query rigctld for the rig's frequency
# N.B. Terminate query commands with a newline, e.g. "\n" character.
print $socket "f\n";

# Get the rig's frequency from rigctld and print it to STDOUT
# N.B. Replies are newline terminated, so lines in @answer end with '\n'.
@answer = get_results($socket);
#$get_freq = <$socket>;
#$get_freq = $socket->getline;
#chomp($get_freq);

print "The rig's frequency is: $answer[0]";
#print "The rig's frequency is: $get_freq\n";

# Extra newline for screen formatting.
print "\n";

# Do the same for the mode (reading the mode also returns the bandwidth)
print $socket "m\n";
@answer = get_results($socket);
#$get_mode = <$socket>;
#chomp($get_mode);
#$get_bw = <$socket>;
#chomp($get_bw);

#print "The rig's mode is: $get_mode\n";
#print "The rig's bandwidth is: $get_bw\n";
print "The rig's mode is: $answer[0]";
print "The rig's bandwidth is: $answer[1]";
print "\n";

# Now set the rig's frequency
#print "Setting the rig's frequency to: $set_freq\n";
#print $socket "F $set_freq\n";
#<$socket>;
#print $socket "f\n";
#@answer = get_results($socket);
#$get_freq = <$socket>;
#chomp($get_freq);
#print "The rig's frequency is now: $get_freq\n";
#print "\n";

# Setting the mode takes two parameters, mode and bandwidth
#print "Setting the rig's mode to $set_mode and bandwidth to $set_bw\n";
#print $socket "\\set_mode $set_mode $set_bw\n";
#<$socket>;
#print $socket "\\get_mode\n";
#@answer = get_results($socket);
#$get_mode = <$socket>;
#chomp($get_mode);
#$get_bw = <$socket>;
#chomp($get_bw);
#print "The rig's mode is now: $get_mode\n";
#print "The rig's bandwidth is now: $get_bw\n";
#print "\n";

# Close the connection before we exit.
close($socket);

