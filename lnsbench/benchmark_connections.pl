#!/usr/bin/perl
#
use strict;
use warnings;

use Time::HiRes qw( usleep ualarm gettimeofday tv_interval nanosleep clock stat );


# Configuration
my $close_sessions = 0; # Whether to leave sessions open or not.
my $tunnels = 2;
my $sessions_per_thread = 4;
my $threads = 1; #using processes instead in reality.
my $client = '127.0.0.1'; # The computer running openl2tpd as a client
my $host = '123.200.179.244'; # The lns cluster vip

my $base_user = 'user.'; # The basename of the user.
my $base_pass = 'pass.'; # The base password
my $base_sessionname = 'session_'; # base part of the session name
my $base_tunnelname = 'tunnel_';

# Basic args to l2tpconfig
my $cmd_base = qq| l2tpconfig -R $client |;

# End args
#################################


sub print_results($$) {

	my $arrayref = shift;
	my $thread_id = shift;
	my @r = @$arrayref;
	
	foreach my $h (@r) {
		print "$thread_id\t$h->{'action'}\t$h->{'result'}\t$h->{'time'}\n";
	}

}


# End funcs
####################

my @results;
my @summary_results;
our @tunnelnames;

# Some setup

# First we do setup of tunnels - this is UNTHREADED deliberately.
my $starttime = [Time::HiRes::gettimeofday];
for (my $i=0;$i<$tunnels;$i++) {
	my $tunnelname = $base_tunnelname . $i;
	my $cmd_opentunnel = qq| $cmd_base tunnel create tunnel_name=$tunnelname dest_ipaddr=$host persist=no |;
	my $result = system($cmd_opentunnel);
	push @results, { 'action'=>'opentunnel', 'result'=>$result, 'time'=>tv_interval($starttime, [Time::HiRes::gettimeofday]) };
	push @tunnelnames, $tunnelname;
}
push @summary_results, {'action'=>'opentunnels', 'result'=>0, 'time'=>tv_interval($starttime, [Time::HiRes::gettimeofday])};

sleep 10; # wait for tunnels to form.

# TODO: Add multithreading HERE.
my $thread_id = 0;
THREADLOOP: foreach (my $i=1; $i<=$threads; $i++) {
	my $pid = fork();
	# Don't fork bomb!
	if ($pid == 0) {
		$thread_id = $i;
		last THREADLOOP;
	}
}

if ($thread_id != 0) {
	# We're a child.
	# Clear our result arrays.
	@results = ();
	@summary_results = ();

	my @sessions;
	# Now we open all of the sessions.
	$starttime = [Time::HiRes::gettimeofday];
	for (my $i=0;$i<$sessions_per_thread;$i++) {
		my $tunnelname = $tunnelnames[$i % @tunnelnames]; # use modulo arithmetic to cycle through tunnel names
		my $sessionname = $tunnelname . "." . $base_sessionname . $i . ".thread_$thread_id";
		my $user = $base_user . $sessionname;
		my $pass = $base_pass . $sessionname;
		my $cmd_opensession = qq| $cmd_base session create tunnel_name="$tunnelname" session_name="$sessionname" user_name="$user" user_password="$pass" |;
		my $result = system($cmd_opensession);
		push @results, { 'action'=>'opensession', 'result'=>$result, 'time'=>tv_interval($starttime, [Time::HiRes::gettimeofday])};
		push @sessions, {'sessionname' => $sessionname, 'tunnelname' => $tunnelname};
	}
	push @summary_results, {'action'=>'opensessions', 'result'=>0, 'time'=>tv_interval($starttime, [Time::HiRes::gettimeofday])};

	if ($close_sessions) {
		# Now we close all of the sessions.
		$starttime = [Time::HiRes::gettimeofday];	
		foreach my $sessioninfo (@sessions) {
			my $tunnelname = $sessioninfo->{'tunnelname'};
			my $sessionname = $sessioninfo->{'sessionname'};
			my $cmd_closesession = qq| $cmd_base session delete tunnel_name="$tunnelname" session_name="$sessionname" |;
			my $result = system($cmd_closesession);
			push @results, {'action'=>'closesession', 'result'=>$result, 'time'=>tv_interval($starttime, [Time::HiRes::gettimeofday])};
		}
		push @summary_results, {'action'=>'closesessions', 'result'=>0, 'time'=>tv_interval($starttime, [Time::HiRes::gettimeofday])};
	}

	# Print out all of the results to stdout.
	print_results(\@results, "thread_$thread_id");
	print_results(\@summary_results, "thread_$thread_id");

} else {
	# We're the parent -  wait for children. 
	for(my $i=0; $i<$threads; $i++) {
		wait();
	}

	if ($close_sessions) {
		$starttime = [Time::HiRes::gettimeofday];	
		# Close all of the tunnels
		foreach my $tunnelname (@tunnelnames) { 
			my $cmd_closetunnel = qq| $cmd_base tunnel delete tunnel_name="$tunnelname" |;
			my $result = system($cmd_closetunnel);
			push @results, { 'action'=>'closetunnel', 'result'=>$result, 'time'=>tv_interval($starttime, [Time::HiRes::gettimeofday])};
		}
		push @summary_results, {'action'=>'closesessions', 'result'=>0, 'time'=>tv_interval($starttime, [Time::HiRes::gettimeofday])};
	}

	# Print out some more results.
	print_results(\@results, 'parent');
	print_results(\@summary_results, 'parent');

}



