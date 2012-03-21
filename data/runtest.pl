#!/usr/bin/perl -w
#
# Test script for running compressor using different data sets.
#
# @author jkataja

use strict;
use warnings;
use Time::HiRes;

my $Suffix = 'out';
my $SuffixFlip = 'out.flip';
 
my %tests;
	
sub pretty_bin {
	my ($bin) = @_;
	$bin =~ s/^.*\///g; # path
	return $bin;
}

sub pretty_size {
	my ($size) = @_;
	$size =~ s/(\d{1,3}?)(?=(\d{3})+$)/$1 /g; # group digits
	return $size;
}

sub pretty_time {
	my ($time) = @_;
	if ($time < 1) { return "< 1s"; }
	if ($time < 10) { return sprintf("%.1f", $time)."s"; }
	if ($time < 60) { return int($time)."s"; }
	if ($time < 600) { return sprintf("%.1f", $time/60)."s"; }
	return int($time/60)."m";
}

sub run_test {
	my ($bin, $path, $data, $tests) = @_;
	my $file = $path.'/'.$data;
	my $outfile = $path.'/'.$data.$Suffix;
	die "file '$file' not readable\n" unless -r $file;
	
	my $startc = [ Time::HiRes::gettimeofday( ) ];
	system("$bin < '$file' > '$file.$Suffix' 2>/dev/null");
	die "compress '$bin $file' returned error\n" if ($? != 0 );
	my $timec = Time::HiRes::tv_interval( $startc );
	
	my $startd = [ Time::HiRes::gettimeofday( ) ];
	system("$bin -d < '$file.$Suffix' > '$file.$SuffixFlip' 2>/dev/null");
	die "uncompress '$bin -d $file.Suffix' returned error\n" if ($? != 0 );
	my $timed = Time::HiRes::tv_interval( $startd );

	my $size = -s $file.'.'.$Suffix;

	my $md5orig = `md5 < '$file'`; chop $md5orig;
	my $md5flip = `md5 < '$file.$SuffixFlip'`; chop $md5flip;

	die "uncompressed file does not match original: $bin '$file'" 
	if $md5orig ne $md5flip;

	unlink("$file.$Suffix", "$file.$SuffixFlip");

	$tests->{$file}->{ &pretty_bin($bin) } = {
		'size' => $size, 
		'timec' => $timec,
		'timed' => $timed 
	};
}

sub run_compress_tests {
	my ($cmds, $corpuspath, $collection) = @_;
	my $origsizesum;
	my @sizesum;
	my %tests;
	foreach my $data (@$collection) {
		printf STDERR "processing '$data'\n";
		foreach my $bin (@$cmds) {
			&run_test($bin, $corpuspath, $data, \%tests);
		}
	}

	print "Collection,Size";
	foreach my $bin (@$cmds) {
		print ",".&pretty_bin($bin);
	}
	print "\n";
	foreach my $file (sort keys %tests) {
		my $origsize = -s $file;
		$origsizesum += $origsize;
		# bpc
		print "$file,".&pretty_size($origsize);
		my $n = 0;
		foreach my $bin (@$cmds) { 
			$bin = &pretty_bin($bin);
			my $size = $tests{$file}->{$bin}->{'size'};
			$sizesum[$n++] += $size;
			my $bpc = ($size/$origsize) * 8.0;
			print ",$bpc"; 
		}
		print "\n";
		# compress time
		print ",";
		foreach my $bin (@$cmds) { 
			$bin = &pretty_bin($bin);
			my $timec = $tests{$file}->{$bin}->{'timec'};
			print ",".&pretty_time($timec); 
		}
		print "\n";
		# decompress time
		print ",";
		foreach my $bin (@$cmds) { 
			$bin = &pretty_bin($bin);
			my $timed = $tests{$file}->{$bin}->{'timed'};
			print ",".&pretty_time($timed); 
		}
		print "\n";
	}

	# total
	print "total,".&pretty_size($origsizesum);
	my $n = 0;
	foreach my $bin (@$cmds) { 
		my $bpc = ($sizesum[$n++]/$origsizesum) * 8.0;
		print ",$bpc"; 
	}
	print "\n";
}

die "pompom not found\n" unless -x '../bin/pompom'; 

my @Calgary = qw/bib book1 book2 geo news obj1 obj2 paper1 paper2 pic progc progl progp trans/;

my @cmds = ( 'gzip -1', 'gzip -9', 'bzip2 -1', 'bzip2 -9', '../bin/pompom');

&run_compress_tests(\@cmds, 'calgary', \@Calgary);
