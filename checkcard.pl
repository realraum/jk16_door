#!/usr/bin/perl -w

use Socket;
use strict;
my $fh;
#my $fifofile = "/tmp/door_cmd.fifo";

my $socketfile = "/tmp/door_cmd.socket";
exit(1) unless (-S $socketfile);
my $socketaddr = sockaddr_un($socketfile);

my $keys;
my %good;

open $keys,'/flash/realraum/keys';
while (<$keys>)
{
	if ($_ =~ /([0-9A-Fa-f]{8})\s(\S+)/)
	{
		$good{$1}=$2;
 	}
}

sub send_to_fifo
{
	socket(my $conn, PF_UNIX, SOCK_STREAM,0) || die "socket: $!";
	connect($conn, $socketaddr) || die "socket connect: $!";
	print $conn shift(@_)."\n";
	close($conn);
}

while (sleep 1)
{
	open $fh,'/flash/realraum/mifare-read 0 2>&1 |';
	while (<$fh>)
	{
		next unless /UID/;
		my ($id) = /UID=(\S+)\s+/;
		if ($good{$id})
		{
			send_to_fifo("toggle Card ".$good{$id});
		} else {
			send_to_fifo("log InvalidCard $id");
		}
	}
}

###############################################################
#  mifare-read
#   
#  writes:
#  UID=<4 byte in hex, upper-case>
#
#
###############################################################
# /dev/ttyUSB0: door key printer
#
#
#   Ok
#   Ok, closing now
#   Already closed
#   Already opened
#   close forced manually\nOk
#   open forced manually\nOk
#   Error: .*
#   .* be: unknown command
#          Operation in progress
#          open/close took too long!
#          last open/close operation took to long!
#
# commands:
#  c ... close
#   response: "Ok", "Already closed", "Error: .*"
#  o ... open
#   response: "Ok", "Already opened", "Error: .*"
#  s ... status
#   response: "Status: closed|opened|<->, opening|waiting|closing|idle"
#         or  "Error: .*"
#  r ... reset
#   "Ok, closing now" or "Error: .*"
#
# open/close  will only be accepted if Status: ..., idle
# Reset overrules all other operations in progress
# s will always be accepted
#
###############################################################
#
#
