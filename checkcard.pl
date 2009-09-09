#!/usr/bin/perl -w

use Socket;
use strict;
my $fh;

my $socketfile = $ARGV[0] || "/var/run/tuer/door_cmd.socket";
sleep(1) while (! -S $socketfile);
my $socketaddr = sockaddr_un($socketfile);

my $keys;
my %good;

open $keys,'/flash/keys';
while (<$keys>)
{
	chomp;
	if ($_ =~ /^([0-9A-Fa-f]{8})\s+(.+)$/)
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
	open $fh,'/flash/tuer/mifare-read 0 2>&1 |';
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
#
#
