#!/usr/bin/perl -w
#
# Benchmarking script for running compressor using different data sets.
#
# @author jkataja

use strict;
use warnings;
use Time::HiRes;
use FindBin;
use File::Copy;
use File::Basename;
use File::Temp qw/tempdir/;

my $Suffix = 'out';
my $SuffixFlip = 'out.flip';
 
my %tests;

# Path component removed from file/cmd
sub pretty_file {
	my ($file) = @_;
	$file = (split /\//,$file)[-1];
	return $file;
}

# Group size digits
sub pretty_size {
	my ($size) = @_;
	$size =~ s/(\d{1,3}?)(?=(\d{3})+$)/$1 /g; 
	return $size;
}

# More readable time display
sub pretty_time {
	my ($time) = @_;
	if ($time < 0.1) { return "< 0.1s"; }
	if ($time <= 10) { return sprintf("%.1f", $time)."s"; }
	if ($time <= 60) { return int($time)."s"; }
	if ($time <= 600) { return sprintf("%.1f", $time/60)."m"; }
	return int($time/60)."m";
}

# Commands (program+arguments) which are listed in cmd hash
sub list_cmds {
	my ($cmd) = @_;
	my @cmd_arg;
	foreach my $bin (@$cmd) {
		my $p = $bin->{p};
		foreach my $arg (@{ $bin->{args} }) {
			push @cmd_arg, &pretty_file($p).' '.$arg;
		}
	}
	return @cmd_arg;
}

# Call compressor and decompressor programs with input, take time
# and check output checksum
sub run_test {
	my ($cmd, $path, $data, $tests) = @_;
	my $file = $path.'/'.$data;
	my $outfile = $path.'/'.$data.$Suffix;
	die "File '$file' not readable\n" unless -r $file;

	# 7z doesn't like //
	$file =~ s/\/\//\//g;

	my $bin = $cmd->{p};
	my $c = $cmd->{c};
	my $d = $cmd->{d};
	foreach my $arg (@{ $cmd->{args} }) {
		print STDERR "\tRunning '$bin' options '$arg'\n";

		# options
		my $full_c = "$bin $c $arg";
		my $full_d = "$bin $d";
		my $tmpdir;
		unless (defined $cmd->{f}) { # pipe
			$full_c .= " < '$file' > '$file.$Suffix'";
			$full_c .= " 2>/dev/null";

			$full_d .= " < '$file.$Suffix' > '$file.$SuffixFlip'";
			$full_d .= " 2>/dev/null";
		} else { # 7zip doesn't like pipes with ppmd
			$full_c .= " '$file.$Suffix' '$file'";
			$full_c .= " 2>&1 >/dev/null";

			$tmpdir = tempdir( CLEANUP => 1 );
			$full_d .= " -o'$tmpdir' '$file.$Suffix'";
			$full_d .= " 2>&1 >/dev/null";
		}

		# compress
		my $startc = [ Time::HiRes::gettimeofday( ) ];
		system("$full_c");
		die "Command '$full_c' returned error\n" if ($? != 0 );
		my $timec = Time::HiRes::tv_interval( $startc );
	
		# decompress
		my $startd = [ Time::HiRes::gettimeofday( ) ];
		system("$full_d 2>/dev/null");
		die "Command '$full_d' returned error\n" if ($? != 0 );
		my $timed = Time::HiRes::tv_interval( $startd );
		
		# move file from temp path
		if (defined $cmd->{f}) {
			move("$tmpdir/".basename($file), "$file.$SuffixFlip");
		}

		# test integrity
		my $size = -s $file.'.'.$Suffix;
		my $md5orig = ( split /\s/,`md5sum '$file'` )[0];
		my $md5flip = ( split /\s/, `md5sum '$file.$SuffixFlip'` )[0];

		die "uncompressed file does not match original: $bin '$file'" 
		if $md5orig ne $md5flip;

		unlink("$file.$Suffix", "$file.$SuffixFlip");

		$tests->{$file}->{ &pretty_file($bin).' '.$arg } = {
			'size' => $size, 
			'timec' => $timec,
			'timed' => $timed 
		};
	}
}

# Run all set cases and output LaTeX tabular
sub run_compress_tests {
	my ($cmd, $corpuspath, $collection) = @_;
	my $origsizesum;
	my @sizesum;
	my @timesum;
	my %tests;

	# tests
	foreach my $data (@$collection) {
		printf STDERR "processing '$data'\n";
		foreach my $bin (@$cmd) {
			&run_test($bin, $corpuspath, $data, \%tests);
		}
	}

	# title
	my $title_fmt = "l" x scalar &list_cmds($cmd);
	my $title_cells = join ' & ',&list_cmds($cmd);
	print <<END;
\\begin{tabular}[t]{|ll|$title_fmt|}
    \\hline
           File & Size & $title_cells \\\\
    \\hline
END

	# rows
	foreach my $file (sort keys %tests) {
		my $origsize = -s $file;
		$origsizesum += $origsize;
		my @bpcs;
		my @times;
		# bpc
		my $n = 0;
		foreach my $cmd_arg (&list_cmds($cmd)) {
			# bpc
			my $size = $tests{$file}->{$cmd_arg}->{'size'};
			$sizesum[$n] += $size;
			my $bpc = ($size/$origsize) * 8.0;
			push @bpcs, sprintf("%.3f", $bpc); 

			# time
			my $timec = $tests{$file}->{$cmd_arg}->{'timec'};
			my $timed = $tests{$file}->{$cmd_arg}->{'timed'};
			$timesum[$n] += $timec + $timed;
			push @times, &pretty_time($timec+$timed); 

			$n++;
		}
		# table
		my $fil = &pretty_file($file);
		my $siz = &pretty_size($origsize);
		my $file_bpc = join ' & ',@bpcs;
		my $file_times = join ' & ',@times;
		print <<END;
           $fil & $siz & $file_bpc \\\\
                &      & $file_times \\\\
    \\hline
END

	}

	# total sums
	my $n = 0;
	my @bpcs;
	my @times;
	foreach my $cmd_arg (&list_cmds($cmd)) {
		# bpc
		my $bpc = ($sizesum[$n]/$origsizesum) * 8.0;
		push @bpcs, sprintf("%.3f", $bpc); 
	
		# time
		push @times, &pretty_time($timesum[$n]);

		++$n;
	}

	my $siz = &pretty_size($origsizesum);
	my $total_bpc = join ' & ',@bpcs;
	my $total_times = join ' & ',@times;
	print <<END;
    \\hline
           Total & $siz & $total_bpc \\\\
                &      & $total_times \\\\
    \\hline
\\end{tabular}
END
}

my $dir = shift;
die "Usage: $0 in/path/\n" unless defined $dir && -d $dir; 

# Which benchmark files to use (input directory)
my %files;
opendir(DIR, $dir) or die $!;
while (my $file = readdir(DIR)) {
	next if $file =~ /^\./; # starts with .
	next if $file =~ /\.(xz|7z|out|flip|gz)$/; # working files
	next if $file eq 'md5sums' || $file eq 'README'; # cruft
	$files{$file}++;
}

die "No input files found in path\n" unless scalar keys %files;

# Which benchmark commands (programs+arguments) to run
my @cmds = ( 
	{ p => 'gzip', c => '', d => '-d', args => [ '-9' ] },
	{ p => 'bzip2', c => '', d => '-d', args => [ '-9' ] },
	{ p => '7z', c => 'a', d => 'e', f => 1,
		args => [ '-m0=lzma -mx=9', '-m0=ppmd -mx=9' ] },
	{ p => 'pompom', c => '', d => '-d',
		args => ['-o6 -m2048' ] },
#	{ p => 'pompom', c => '', d => '-d',
#		args => [ '-o3 -m8', '-o5 -m64', '-o6 -m256' ] },
);

# Find out full path to command
foreach my $cmd (@cmds) {
	my $p = $cmd->{p};
	my $binpath = `which '$p'`; chomp $binpath;
	$binpath = $FindBin::Bin."/../bin/$p" if $binpath eq '' ;
	$cmd->{p} = $binpath;
	die "Program '$p' not found\n" unless -x $binpath;
}

my @fails = sort keys %files;
&run_compress_tests(\@cmds, $dir, \@fails);
