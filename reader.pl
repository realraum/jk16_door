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
  open $fh,'/flash/realraum/a.out 0 2>&1 |';
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
