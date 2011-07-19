#!/usr/bin/perl -w

use Socket;
use strict;
my $fh;

my $socketfile = $ARGV[0] || "/var/run/tuer/door_cmd.socket";
my $keysfile = '/flash/keys';

sleep(1) while (! -S $socketfile);
my $socketaddr = sockaddr_un($socketfile);

my %good;
my $keys_last_read=0;

sub read_keys
{
  %good=();
  my $keys;
  open $keys,$keysfile;
  while (<$keys>)
  {
    chomp;
    if ($_ =~ /^([0-9A-Fa-f]{8})\s+(.+)$/)
    {
      $good{$1}=$2;
    }
  }
  close $keys;
  $keys_last_read = -M ($keysfile);
}

sub send_to_fifo
{
	socket(my $conn, PF_UNIX, SOCK_STREAM,0) || die "socket: $!";
	connect($conn, $socketaddr) || die "socket connect: $!";
	print $conn shift(@_)."\n";
	close($conn);
}

read_keys();


while (sleep 2)
{
  send_to_fifo("log starting mifare-read");
	open $fh,'/flash/tuer/mifare-read 0 2>&1 |';

  read_keys() unless ($keys_last_read == -M ($keysfile));

  READLOOP: while (<$fh>)
  while (<$fh>)
	{		
		unless (/UID/)
    {
      close($fh);
      system("/usr/bin/killall -9 mifare-read");
			send_to_fifo("log restarting mifare-read after invalid output");
      last READLOOP;
    }
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
