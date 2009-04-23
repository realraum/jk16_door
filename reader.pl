#!/usr/bin/perl

use strict;
my $fh;

my $keys;
my %good;
my $status = 'c';

open $keys,'/flash/realraum/keys';
while (<$keys>)
{
	my ($code,$comment) = split /\s/,$_,2;
	$good{$code}=$comment;
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
		my $newstat;
		if ($status eq 'o')
		{
			$newstat='c';

		} else {
			$newstat = 'o';
		}
		print "$newstat: $good{$id}";
		system ( "echo -n $newstat > /dev/ttyUSB0");
		$status = $newstat;
	} else {
		print "boese\n";
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