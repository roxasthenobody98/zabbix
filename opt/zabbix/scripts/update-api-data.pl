#!/usr/bin/perl -w

use lib '/opt/zabbix/scripts';

use strict;
use RSM;
use RSMSLV;
use ApiHelper;
use JSON::XS;
use Data::Dumper;

use constant ROOT_ZONE_DIR => 'zz--root'; # map root zone name (.) to something human readable
use constant CONTINUE_FILE => 'last_update.txt'; # name of the file containing the timestamp of the last run with --continue

parse_opts('tld=s', 'service=s', 'period=n', 'continue!', 'ignore-file=s');

# do not write any logs
setopt('nolog');

exit_if_running();

if (opt('debug'))
{
	dbg("command-line parameters:");
	dbg("$_ => ", getopt($_)) foreach (optkeys());
}

my @services;
if (opt('service'))
{
	if (getopt('service') ne 'dns' and getopt('service') ne 'dnssec' and getopt('service') ne 'rdds' and getopt('service') ne 'epp')
	{
		print("Error: \"", getopt('service'), "\" - unknown service\n");
		usage();
	}

	push(@services, lc(getopt('service')));
}
else
{
	push(@services, 'dns', 'dnssec', 'rdds', 'epp');
}

if (opt('tld') and opt('ignore-file'))
{
	print("Error: options --tld and --ignore-file cannot be used together\n");
	usage();
}

if (opt('period') and opt('continue'))
{
	print("Error: options --period and --continue cannot be used together\n");
	usage();
}

my %ignore_hash;

if (opt('ignore-file'))
{
	my $ignore_file = getopt('ignore-file');

	my $handle;
	fail("cannot open ignore file \"$ignore_file\": $!") unless open($handle, '<', $ignore_file);

	chomp(my @lines = <$handle>);

	close($handle);

	%ignore_hash = map { $_ => 1 } @lines;
}

set_slv_config(get_rsm_config());

db_connect();

my $cfg_dns_interval;
my $cfg_dns_valuemaps;
my $cfg_dns_minns;
my $cfg_dns_key_status;
my $cfg_dns_key_rtt;

my $cfg_rdds_interval;
my $cfg_rdds_valuemaps;
my $cfg_rdds_key_status;
my $cfg_rdds_key_43_rtt;
my $cfg_rdds_key_43_ip;
my $cfg_rdds_key_43_upd;
my $cfg_rdds_key_80_rtt;
my $cfg_rdds_key_80_ip;

my $cfg_epp_interval;
my $cfg_epp_valuemaps;
my $cfg_epp_key_status;
my $cfg_epp_key_ip;
my $cfg_epp_key_rtt;

my $cfg_dns_statusmaps = get_statusmaps('dns');

my %services_hash = map { $_ => 1 } @services;

if (exists($services_hash{'dns'}) or exists($services_hash{'dnssec'}))
{
	$cfg_dns_interval = get_macro_dns_udp_delay();
	$cfg_dns_minns = get_macro_minns();
	$cfg_dns_valuemaps = get_valuemaps('dns');
	$cfg_dns_key_status = 'rsm.dns.udp[{$RSM.TLD}]'; # 0 - down, 1 - up
	$cfg_dns_key_rtt = 'rsm.dns.udp.rtt[{$RSM.TLD},';
}
if (exists($services_hash{'rdds'}))
{
	$cfg_rdds_interval = get_macro_rdds_delay();
	$cfg_rdds_valuemaps = get_valuemaps('rdds');
	$cfg_rdds_key_status = 'rsm.rdds[{$RSM.TLD}'; # 0 - down, 1 - up, 2 - only 43, 3 - only 80
	$cfg_rdds_key_43_rtt = 'rsm.rdds.43.rtt[{$RSM.TLD}]';
	$cfg_rdds_key_43_ip = 'rsm.rdds.43.ip[{$RSM.TLD}]';
	$cfg_rdds_key_43_upd = 'rsm.rdds.43.upd[{$RSM.TLD}]';
	$cfg_rdds_key_80_rtt = 'rsm.rdds.80.rtt[{$RSM.TLD}]';
	$cfg_rdds_key_80_ip = 'rsm.rdds.80.ip[{$RSM.TLD}]';
}
if (exists($services_hash{'epp'}))
{
	$cfg_epp_interval = get_macro_epp_delay();
	$cfg_epp_valuemaps = get_valuemaps('epp');
	$cfg_epp_key_status = 'rsm.epp[{$RSM.TLD},'; # 0 - down, 1 - up
	$cfg_epp_key_ip = 'rsm.epp.ip[{$RSM.TLD}]';
	$cfg_epp_key_rtt = 'rsm.epp.rtt[{$RSM.TLD},';
}

my $period = getopt('period');

my $now = time();

my $tlds_ref;
if (opt('tld'))
{
	fail("TLD ", getopt('tld'), " does not exist.") if (tld_exists(getopt('tld')) == 0);

	$tlds_ref = [ getopt('tld') ];
}
else
{
	$tlds_ref = get_tlds();
}

my $check_back = $now - 1800; # check back for latest availability data

foreach (@$tlds_ref)
{
	$tld = $_;

	my $api_tld = $tld;

	$api_tld = ROOT_ZONE_DIR if ($tld eq ".");

	if (__tld_ignored($tld) == SUCCESS)
	{
		dbg("tld \"$tld\" ignored");
		next;
	}

	foreach my $service (@services)
	{
		if (tld_service_enabled($tld, $service) != SUCCESS)
		{
			if (opt('dry-run'))
			{
				__prnt(uc($service), " DISABLED");
			}
			else
			{
				if (ah_save_alarmed($api_tld, $service, AH_ALARMED_DISABLED) != AH_SUCCESS)
				{
					fail("cannot save alarmed: ", ah_get_error());
				}
			}

			next;
		}

		my $hostid = get_hostid($tld);
		my $key = "rsm.slv.$service.avail";
		my $avail_itemid = get_itemid_by_hostid($hostid, $key);

		if ($avail_itemid < 0)
		{
			if ($avail_itemid == E_ID_NONEXIST)
			{
				wrn("configuration error: service $service enabled but item \"$key\" not found");
			}
			elsif ($avail_itemid == E_ID_MULTIPLE)
			{
				wrn("configuration error: multiple items with key \"$key\" found");
			}
			else
			{
				wrn("cannot get ID of $service item ($key): unknown error");
			}

			next;
		}

		# we need down time in minutes, not percent, that's why we can't use "rsm.slv.$service.rollweek" value
		my ($rollweek_from, $rollweek_till) = get_rollweek_bounds();
		my $downtime = get_downtime($avail_itemid, $rollweek_from, $rollweek_till);
		my $lastclock = get_lastclock($tld, "rsm.slv.$service.rollweek");

		my ($continue_clock, $continue_file);

		if (opt('continue'))
		{
			$continue_file = AH_BASE_DIR."/$api_tld/$service/".CONTINUE_FILE;
			my $handle;

			if (-e $continue_file)
			{
				fail("cannot open continue file $continue_file\": $!") unless (open($handle, '<', $continue_file));

				chomp(my @lines = <$handle>);

				close($handle);

				$continue_clock = $lines[0];
			}
		}

		my $till = $lastclock + RESULT_TIMESTAMP_SHIFT; # include the whole minute
		my $from;

		if (defined($continue_clock))
		{
			$from = $continue_clock;
		}
		elsif (defined($period))
		{
			$from = $till - $period * 60 + 1;
		}

		if (defined($from))
		{
			fail("option --continue used too soon, please wait till new data is available in the database") if ($from > $till);
		}

		__prnt(uc($service), " period: ", __selected_period($from, $till)) if (opt('dry-run') or opt('debug'));

		if (opt('dry-run'))
		{
			__prnt(uc($service), " service availability $downtime (", ts_str($lastclock), ")");
		}
		else
		{
			if (ah_save_service_availability($api_tld, $service, $downtime, $lastclock) != AH_SUCCESS)
			{
				fail("cannot save service availability: ", ah_get_error());
			}
		}

		dbg("getting current $service availability");

		# get availability
		my $incidents = get_incidents($avail_itemid, $now);

		my $alarmed_status = AH_ALARMED_NO;
		if (scalar(@$incidents) != 0)
		{
			if ($incidents->[0]->{'false_positive'} == 0 and not defined($incidents->[0]->{'end'}))
			{
				$alarmed_status = AH_ALARMED_YES;
			}
		}

		if (opt('dry-run'))
		{
			__prnt(uc($service), " alarmed:$alarmed_status");
		}
		else
		{
			if (ah_save_alarmed($api_tld, $service, $alarmed_status, $lastclock) != AH_SUCCESS)
			{
				fail("cannot save alarmed: ", ah_get_error());
			}
		}

		my ($nsips_ref, $dns_items_ref, $rdds_dbl_items_ref, $rdds_str_items_ref, $epp_dbl_items_ref, $epp_str_items_ref, $interval);

		if ($service eq 'dns' or $service eq 'dnssec')
		{
			$interval = $cfg_dns_interval;
			$nsips_ref = get_nsips($tld, $cfg_dns_key_rtt, 1); # templated
			$dns_items_ref = __get_dns_itemids($nsips_ref, $cfg_dns_key_rtt, $tld);
		}
		elsif ($service eq 'rdds')
		{
			$interval = $cfg_rdds_interval;
			$rdds_dbl_items_ref = __get_rdds_dbl_itemids($tld);
			$rdds_str_items_ref = __get_rdds_str_itemids($tld);
		}
		elsif ($service eq 'epp')
		{
			$interval = $cfg_epp_interval;
			$epp_dbl_items_ref = __get_epp_dbl_itemids($tld);
			$epp_str_items_ref = __get_epp_str_itemids($tld);
		}

		$incidents = get_incidents($avail_itemid, $from, $till);

		foreach (@$incidents)
		{
			my $eventid = $_->{'eventid'};
			my $event_start = $_->{'start'};
			my $event_end = $_->{'end'};
			my $false_positive = $_->{'false_positive'};

			my $start = $event_start;
			my $end = $event_end;

			if (defined($from) and $from > $event_start)
			{
				$start = $from;
			}

			if (defined($till))
			{
				if (not defined($event_end) or (defined($event_end) and $till < $event_end))
				{
					$end = $till;
				}
			}

			# get results within incidents
			my $rows_ref = db_select(
				"select value,clock".
				" from history_uint".
				" where itemid=$avail_itemid".
					" and ".sql_time_condition($start, $end).
				" order by clock");

			my @test_results;

			my $status_up = 0;
			my $status_down = 0;

			foreach my $row_ref (@$rows_ref)
			{
				my $value = $row_ref->[0];
				my $clock = $row_ref->[1];

				my $result;

				$result->{'tld'} = $tld;
				$result->{'status'} = get_result_string($cfg_dns_statusmaps, $value);
				$result->{'clock'} = $clock;

				# time bounds for results from proxies
				$result->{'start'} = $clock - $interval + RESULT_TIMESTAMP_SHIFT + 1; # we need to start at 0
				$result->{'end'} = $clock + RESULT_TIMESTAMP_SHIFT;

				if (opt('dry-run'))
				{
					if ($value == UP)
					{
						$status_up++;
					}
					elsif ($value == DOWN)
					{
						$status_down++;
					}
					else
					{
						wrn("invalid status: $value");
					}
				}

				push(@test_results, $result);
			}

			my $test_results_count = scalar(@test_results);

			if ($test_results_count == 0)
			{
				wrn("$service: no results within incident (id:$eventid clock:$event_start)");
				last;
			}

			if (opt('dry-run'))
			{
				__prnt(uc($service), " incident id:$eventid start:", ts_str($event_start), " end:" . ($event_end ? ts_str($event_end) : "ACTIVE") . " fp:$false_positive");
				__prnt(uc($service), " listing successful:$status_up failed:$status_down");
			}
			else
			{
				if (ah_save_incident($api_tld, $service, $eventid, $event_start, $event_end, $false_positive, $lastclock) != AH_SUCCESS)
				{
					fail("cannot save incident: ", ah_get_error());
				}
			}

			my $values_from = $test_results[0]->{'start'};
			my $values_till = $test_results[$test_results_count - 1]->{'end'};

			if ($service eq 'dns' or $service eq 'dnssec')
			{
				my $values_ref = __get_dns_test_values($dns_items_ref, $values_from, $values_till);

				# run through values from probes (ordered by clock)
				foreach my $probe (keys(%$values_ref))
				{
					my $nsips_ref = $values_ref->{$probe};

					dbg("probe:$probe");

					foreach my $nsip (keys(%$nsips_ref))
					{
						my $endvalues_ref = $nsips_ref->{$nsip};

						my ($ns, $ip) = split(',', $nsip);

						dbg("  ", scalar(keys(%$endvalues_ref)), " values for $nsip:") if (opt('debug'));

						my $test_result_index = 0;

						foreach my $clock (sort(keys(%$endvalues_ref))) # must be sorted by clock
						{
							if ($clock < $test_results[$test_result_index]->{'start'})
							{
								wrn("no aggregated result of $service test of $nsip (probe:$probe service:$service time:", ts_str($clock), ") found in the database");
								next;
							}

							# move to corresponding test result
							$test_result_index++ while ($test_result_index < $test_results_count and $clock > $test_results[$test_result_index]->{'end'});

							if ($test_result_index == $test_results_count)
							{
								wrn("no aggregated result of $service test of $nsip (probe:$probe service:$service time:", ts_str($clock), ") found in the database");
								next;
							}

							my $tr_ref = $test_results[$test_result_index];

							$tr_ref->{'probes'}->{$probe}->{'status'} = 'No result' unless (exists($tr_ref->{'probes'}->{$probe}->{'status'}));

							$tr_ref->{'probes'}->{$probe}->{'details'}->{$ns}->{$clock} = {'rtt' => get_detailed_result($cfg_dns_valuemaps, $endvalues_ref->{$clock}), 'ip' => $ip};
						}
					}
				}

				# get results from probes: number of working Name Servers
				my $itemids_ref = __get_status_itemids($tld, $cfg_dns_key_status);
				my $statuses_ref = __get_probe_statuses($itemids_ref, $values_from, $values_till);

				foreach my $tr_ref (@test_results)
				{
					# set status
					my $tr_start = $tr_ref->{'start'};
					my $tr_end = $tr_ref->{'end'};

					delete($tr_ref->{'start'});
					delete($tr_ref->{'end'});

					my $probes_ref = $tr_ref->{'probes'};
					foreach my $probe (keys(%$probes_ref))
					{
						foreach my $status_ref (@{$statuses_ref->{$probe}})
						{
							next if ($status_ref->{'clock'} < $tr_start);
							last if ($status_ref->{'clock'} > $tr_end);

							$tr_ref->{'probes'}->{$probe}->{'status'} = ($status_ref->{'value'} >= $cfg_dns_minns ? "Up" : "Down");
						}
					}

					if (opt('dry-run'))
					{
						__prnt_json($tr_ref);
					}
					else
					{
						if (ah_save_incident_json($api_tld, $service, $eventid, $event_start, encode_json($tr_ref), $tr_ref->{'clock'}) != AH_SUCCESS)
						{
							fail("cannot save incident: ", ah_get_error());
						}
					}
				}
			}
			elsif ($service eq 'rdds')
			{
				my $values_ref = __get_rdds_test_values($rdds_dbl_items_ref, $rdds_str_items_ref, $values_from, $values_till);

				dbg(Dumper($values_ref)) if (opt('debug'));

				# run through values from probes (ordered by clock)
				foreach my $probe (keys(%$values_ref))
				{
					my $ports_ref = $values_ref->{$probe};

					dbg("probe:$probe");

					foreach my $port (keys(%$ports_ref))
					{
						my $endvalues_ref = $ports_ref->{$port};

						dbg("  ", scalar(keys(%$endvalues_ref)), " values for $port:") if (opt('debug'));

						my $test_result_index = 0;

						foreach my $clock (sort(keys(%$endvalues_ref))) # must be sorted by clock
						{
							#dbg(Dumper($endvalues_ref->{$clock}))if (opt('debug'));

							if ($clock < $test_results[$test_result_index]->{'start'})
							{
								wrn("no aggregated result of $service$port test (probe:$probe service:$service time:", ts_str($clock), ") found in the database");
								next;
							}

							# move to corresponding test result
							$test_result_index++ while ($test_result_index < $test_results_count and $clock > $test_results[$test_result_index]->{'end'});

							if ($test_result_index == $test_results_count)
							{
								wrn("no aggregated result of $service$port test (probe:$probe service:$service time:", ts_str($clock), ") found in the database");
								next;
							}

							my $tr_ref = $test_results[$test_result_index];

							$tr_ref->{'ports'}->{$port}->{$probe}->{'status'} = 'No result' unless (exists($tr_ref->{'ports'}->{$port}->{$probe}->{'status'}));

							$tr_ref->{'ports'}->{$port}->{$probe}->{'details'}->{$clock} = $endvalues_ref->{$clock};
						}
					}
				}

				# get results from probes: working services (rdds43, rdds80)
				my $itemids_ref = __get_status_itemids($tld, $cfg_rdds_key_status);
				my $statuses_ref = __get_probe_statuses($itemids_ref, $values_from, $values_till);

				foreach my $tr_ref (@test_results)
				{
					# set status
					my $tr_start = $tr_ref->{'start'};
					my $tr_end = $tr_ref->{'end'};

					delete($tr_ref->{'start'});
					delete($tr_ref->{'end'});

					my $ports_ref = $tr_ref->{'ports'};

					foreach my $port (keys(%$ports_ref))
					{
						my $probes_ref = $ports_ref->{$port};

						foreach my $probe (keys(%$probes_ref))
						{
							foreach my $status_ref (@{$statuses_ref->{$probe}})
							{
								next if ($status_ref->{'clock'} < $tr_start);
								last if ($status_ref->{'clock'} > $tr_end);

								my $service_only = ($port eq "43" ? 2 : 3); # 0 - down, 1 - up, 2 - only 43, 3 - only 80

								$tr_ref->{'ports'}->{$port}->{$probe}->{'status'} = ($status_ref->{'value'} == 1 or $status_ref->{'value'} == $service_only ? "Up" : "Down");
							}
						}
					}

					if (opt('dry-run'))
					{
						__prnt_json($tr_ref);
					}
					else
					{
						if (ah_save_incident_json($api_tld, $service, $eventid, $event_start, encode_json($tr_ref), $tr_ref->{'clock'}) != AH_SUCCESS)
						{
							fail("cannot save incident: ", ah_get_error());
						}
					}
				}
			}
			elsif ($service eq 'epp')
			{
				dbg("EPP results calculation is not implemented yet");

				my $values_ref = __get_epp_test_values($epp_dbl_items_ref, $epp_str_items_ref, $values_from, $values_till);

				dbg(Dumper($values_ref)) if (opt('debug'));
			}
			else
			{
				fail("THIS SHOULD NEVER HAPPEN (unknown service \"$service\")");
			}
		}

		if (defined($continue_file) and not opt('dry-run'))
		{
			my $updated = $till + 1;

			unless (write_file($continue_file, $updated) == SUCCESS)
			{
				wrn("cannot update continue file \"$continue_file\": $!");
				next;
			}

			dbg("$service: updated till ", ts_str($updated));
		}
	}
}

# unset TLD (for the logs)
$tld = undef;

slv_exit(SUCCESS);

# values are organized like this:
# {
#           'WashingtonDC' => {
#                               'ns1,192.0.34.201' => {
#                                                       '1418994681' => '-204.0000',
#                                                       '1418994621' => '-204.0000'
#                                                     },
#                               'ns2,2620:0:2d0:270::1:201' => {
#                                                                '1418994681' => '-204.0000',
#                                                                '1418994621' => '-204.0000'
#                                                              }
#                             },
# ...
sub __get_dns_test_values
{
	my $dns_items_ref = shift;
	my $start = shift;
	my $end = shift;

	my %result;

	# generate list if itemids
	my $itemids_str = '';
	foreach my $probe (keys(%$dns_items_ref))
	{
		my $itemids_ref = $dns_items_ref->{$probe};

		foreach my $itemid (keys(%$itemids_ref))
		{
			$itemids_str .= ',' unless ($itemids_str eq '');
			$itemids_str .= $itemid;
		}
	}

	if ($itemids_str ne '')
	{
		my $rows_ref = db_select("select itemid,value,clock from history where itemid in ($itemids_str) and " . sql_time_condition($start, $end). " order by clock");

		foreach my $row_ref (@$rows_ref)
		{
			my $itemid = $row_ref->[0];
			my $value = $row_ref->[1];
			my $clock = $row_ref->[2];

			my ($nsip, $probe);
			my $last = 0;

			foreach my $pr (keys(%$dns_items_ref))
			{
				my $itemids_ref = $dns_items_ref->{$pr};

				foreach my $i (keys(%$itemids_ref))
				{
					if ($i == $itemid)
					{
						$nsip = $dns_items_ref->{$pr}->{$i};
						$probe = $pr;
						$last = 1;
						last;
					}
				}
				last if ($last == 1);
			}

			unless (defined($nsip))
			{
				wrn("internal error: Name Server,IP pair of item $itemid not found");
				next;
			}

			$result{$probe}->{$nsip}->{$clock} = $value;
		}
	}

	return \%result;
}

sub __find_probe_key_by_itemid
{
	my $itemid = shift;
	my $items_ref = shift;

	my ($probe, $key);
	my $last = 0;

	foreach my $pr (keys(%$items_ref))
	{
		my $itemids_ref = $items_ref->{$pr};

		foreach my $i (keys(%$itemids_ref))
		{
			if ($i == $itemid)
			{
				$probe = $pr;
				$key = $items_ref->{$pr}->{$i};
				$last = 1;
				last;
			}
		}
		last if ($last == 1);
	}

	return ($probe, $key);
}

# values are organized like this:
# {
#           'WashingtonDC' => {
#                               '80' => {
#                                         '1418994206' => {
#                                                           'ip' => '192.0.34.201',
#                                                           'rtt' => '127.0000'
#                                                         },
#                                         '1418994086' => {
#                                                           'ip' => '192.0.34.201',
#                                                           'rtt' => '127.0000'
#                                                         },
#                               '43' => {
#                                         '1418994206' => {
#                                                           'ip' => '192.0.34.201',
#                                                           'rtt' => '127.0000'
#                                                         },
#                                         '1418994086' => {
#                                                           'ip' => '192.0.34.201',
#                                                           'rtt' => '127.0000'
#                                                         },
# ...
sub __get_rdds_test_values
{
	my $rdds_dbl_items_ref = shift;
	my $rdds_str_items_ref = shift;
	my $start = shift;
	my $end = shift;

	my %result;

	# generate list if itemids
	my $dbl_itemids_str = '';
	foreach my $probe (keys(%$rdds_dbl_items_ref))
	{
		my $itemids_ref = $rdds_dbl_items_ref->{$probe};

		foreach my $itemid (keys(%$itemids_ref))
		{
			$dbl_itemids_str .= ',' unless ($dbl_itemids_str eq '');
			$dbl_itemids_str .= $itemid;
		}
	}

	my $str_itemids_str = '';
	foreach my $probe (keys(%$rdds_str_items_ref))
	{
		my $itemids_ref = $rdds_str_items_ref->{$probe};

		foreach my $itemid (keys(%$itemids_ref))
		{
			$str_itemids_str .= ',' unless ($str_itemids_str eq '');
			$str_itemids_str .= $itemid;
		}
	}

	return \%result if ($dbl_itemids_str eq '' or $str_itemids_str eq '');

	my $dbl_rows_ref = db_select("select itemid,value,clock from history where itemid in ($dbl_itemids_str) and " . sql_time_condition($start, $end). " order by clock");

	foreach my $row_ref (@$dbl_rows_ref)
	{
		my $itemid = $row_ref->[0];
		my $value = $row_ref->[1];
		my $clock = $row_ref->[2];

		my ($probe, $key) = __find_probe_key_by_itemid($itemid, $rdds_dbl_items_ref);

		fail("internal error: cannot get Probe-key pair by itemid:$itemid") unless (defined($probe) and defined($key));

		my $port = __get_rdds_port($key);
		my $type = __get_rdds_dbl_type($key);

		$result{$probe}->{$port}->{$clock}->{$type} = ($type eq 'rtt' ? get_detailed_result($cfg_rdds_valuemaps, $value) : $value);
	}

	my $str_rows_ref = db_select("select itemid,value,clock from history_str where itemid in ($str_itemids_str) and " . sql_time_condition($start, $end). " order by clock");

	foreach my $row_ref (@$str_rows_ref)
	{
		my $itemid = $row_ref->[0];
		my $value = $row_ref->[1];
		my $clock = $row_ref->[2];

		my ($probe, $key) = __find_probe_key_by_itemid($itemid, $rdds_str_items_ref);

		fail("internal error: cannot get Probe-key pair by itemid:$itemid") unless (defined($probe) and defined($key));

		my $port = __get_rdds_port($key);
		my $type = __get_rdds_str_type($key);

		$result{$probe}->{$port}->{$clock}->{$type} = $value;
	}

	return \%result;
}

# values are organized like this:
# {
#         'WashingtonDC' => {
#                 '1418994206' => {
#                               'ip' => '192.0.34.201',
#                               'login' => '127.0000',
#                               'update' => '366.0000'
#                               'info' => '366.0000'
#                 },
#                 '1418994456' => {
#                               'ip' => '192.0.34.202',
#                               'login' => '121.0000',
#                               'update' => '263.0000'
#                               'info' => '321.0000'
#                 },
# ...
sub __get_epp_test_values
{
	my $epp_dbl_items_ref = shift;
	my $epp_str_items_ref = shift;
	my $start = shift;
	my $end = shift;

	my %result;

	# generate list if itemids
	my $dbl_itemids_str = '';
	foreach my $probe (keys(%$epp_dbl_items_ref))
	{
		my $itemids_ref = $epp_dbl_items_ref->{$probe};

		foreach my $itemid (keys(%$itemids_ref))
		{
			$dbl_itemids_str .= ',' unless ($dbl_itemids_str eq '');
			$dbl_itemids_str .= $itemid;
		}
	}

	my $str_itemids_str = '';
	foreach my $probe (keys(%$epp_str_items_ref))
	{
		my $itemids_ref = $epp_str_items_ref->{$probe};

		foreach my $itemid (keys(%$itemids_ref))
		{
			$str_itemids_str .= ',' unless ($str_itemids_str eq '');
			$str_itemids_str .= $itemid;
		}
	}

	return \%result if ($dbl_itemids_str eq '' or $str_itemids_str eq '');

	my $dbl_rows_ref = db_select("select itemid,value,clock from history where itemid in ($dbl_itemids_str) and " . sql_time_condition($start, $end). " order by clock");

	foreach my $row_ref (@$dbl_rows_ref)
	{
		my $itemid = $row_ref->[0];
		my $value = $row_ref->[1];
		my $clock = $row_ref->[2];

		my ($probe, $key) = __find_probe_key_by_itemid($itemid, $epp_dbl_items_ref);

		fail("internal error: cannot get Probe-key pair by itemid:$itemid") unless (defined($probe) and defined($key));

		my $type = __get_epp_dbl_type($key);

		$result{$probe}->{$clock}->{$type} = get_detailed_result($cfg_epp_valuemaps, $value);
	}

	my $str_rows_ref = db_select("select itemid,value,clock from history_str where itemid in ($str_itemids_str) and " . sql_time_condition($start, $end). " order by clock");

	foreach my $row_ref (@$str_rows_ref)
	{
		my $itemid = $row_ref->[0];
		my $value = $row_ref->[1];
		my $clock = $row_ref->[2];

		my ($probe, $key) = __find_probe_key_by_itemid($itemid, $epp_str_items_ref);

		fail("internal error: cannot get Probe-key pair by itemid:$itemid") unless (defined($probe) and defined($key));

		my $type = __get_epp_str_type($key);

		$result{$probe}->{$clock}->{$type} = $value;
	}

	return \%result;
}

# return itemids grouped by Probes:
#
# {
#    'Amsterdam' => {
#         'itemid1' => 'ns2,2620:0:2d0:270::1:201',
#         'itemid2' => 'ns1,192.0.34.201'
#    },
#    'London' => {
#         'itemid3' => 'ns2,2620:0:2d0:270::1:201',
#         'itemid4' => 'ns1,192.0.34.201'
#    }
# }
sub __get_dns_itemids
{
	my $nsips_ref = shift; # array reference of NS,IP pairs
	my $key = shift;
	my $tld = shift;

	my @keys;
	push(@keys, "'" . $key . $_ . "]'") foreach (@$nsips_ref);

	my $keys_str = join(',', @keys);

	my $rows_ref = db_select(
		"select h.host,i.itemid,i.key_".
		" from items i,hosts h".
		" where i.hostid=h.hostid".
			" and h.host like '$tld %'".
			" and i.templateid is not null".
			" and i.key_ in ($keys_str)");

	my %result;

	my $tld_length = length($tld) + 1; # white space
	foreach my $row_ref (@$rows_ref)
	{
		my $host = $row_ref->[0];
		my $itemid = $row_ref->[1];
		my $key = $row_ref->[2];

		# remove TLD from host name to get just the Probe name
		my $probe = substr($host, $tld_length);

		$result{$probe}->{$itemid} = get_ns_from_key($key);
	}

	fail("cannot find items ($keys_str) at host ($tld *)") if (scalar(keys(%result)) == 0);

	return \%result;
}

sub __get_rdds_port
{
	my $key = shift;

	# rsm.rdds.43... <-- returns 43 or 80
	return substr($key, 9, 2);
}

sub __get_rdds_dbl_type
{
	my $key = shift;

	# rsm.rdds.43.rtt... rsm.rdds.43.upd[... <-- returns "rtt" or "upd"
	return substr($key, 12, 3);
}

sub __get_rdds_str_type
{
	# NB! This is done for consistency, perhaps in the future there will be more string items, not just "ip".
	return 'ip';
}

sub __get_epp_dbl_type
{
	my $key = shift;

	chop($key); # remove last char ']'

	# rsm.epp.rtt[{$RSM.TLD},login <-- returns "login" (other options: "update", "info")
        return substr($key, 23);
}

sub __get_epp_str_type
{
	# NB! This is done for consistency, perhaps in the future there will be more string items, not just "ip".
	return 'ip';
}

# return itemids of dbl items grouped by Probes:
#
# {
#    'Amsterdam' => {
#         'itemid1' => 'rsm.rdds.43.rtt...',
#         'itemid2' => 'rsm.rdds.43.upd...',
#         'itemid3' => 'rsm.rdds.80.rtt...'
#    },
#    'London' => {
#         'itemid4' => 'rsm.rdds.43.rtt...',
#         'itemid5' => 'rsm.rdds.43.upd...',
#         'itemid6' => 'rsm.rdds.80.rtt...'
#    }
# }
sub __get_rdds_dbl_itemids
{
	my $tld = shift;

	return __get_itemids_by_complete_key($tld, $cfg_rdds_key_43_rtt, $cfg_rdds_key_80_rtt, $cfg_rdds_key_43_upd);
}

# return itemids of string items grouped by Probes:
#
# {
#    'Amsterdam' => {
#         'itemid1' => 'rsm.rdds.43.ip...',
#         'itemid2' => 'rsm.rdds.80.ip...'
#    },
#    'London' => {
#         'itemid3' => 'rsm.rdds.43.ip...',
#         'itemid4' => 'rsm.rdds.80.ip...'
#    }
# }
sub __get_rdds_str_itemids
{
	my $tld = shift;

	return __get_itemids_by_complete_key($tld, $cfg_rdds_key_43_ip, $cfg_rdds_key_80_ip);
}

sub __get_epp_dbl_itemids
{
	my $tld = shift;

	return __get_itemids_by_incomplete_key($tld, $cfg_epp_key_rtt);
}

sub __get_epp_str_itemids
{
	my $tld = shift;

	return __get_itemids_by_complete_key($tld, $cfg_epp_key_ip);
}

# $keys_str - list of complete keys
sub __get_itemids_by_complete_key
{
	my $tld = shift;

	my $keys_str = "'" . join("','", @_) . "'";

	my $rows_ref = db_select(
		"select h.host,i.itemid,i.key_".
		" from items i,hosts h".
		" where i.hostid=h.hostid".
			" and h.host like '$tld %'".
			" and i.key_ in ($keys_str)".
			" and i.templateid is not null");

	my %result;

	my $tld_length = length($tld) + 1; # white space
	foreach my $row_ref (@$rows_ref)
	{
		my $host = $row_ref->[0];
		my $itemid = $row_ref->[1];
		my $key = $row_ref->[2];

		# remove TLD from host name to get just the Probe name
		my $probe = substr($host, $tld_length);

		$result{$probe}->{$itemid} = $key;
	}

	fail("cannot find items ($keys_str) at host ($tld *)") if (scalar(keys(%result)) == 0);

	return \%result;
}

# call this function with list of incomplete keys after $tld, e. g.:
# __get_itemids_by_incomplete_key("example", "aaa[", "bbb[", ...)
sub __get_itemids_by_incomplete_key
{
	my $tld = shift;

	my $keys_cond = "(key_ like '" . join("%' or key_ like '", @_) . "%')";

	my $rows_ref = db_select(
		"select h.host,i.itemid,i.key_".
		" from items i,hosts h".
		" where i.hostid=h.hostid".
			" and h.host like '$tld %'".
			" and i.templateid is not null".
			" and $keys_cond");

	my %result;

	my $tld_length = length($tld) + 1; # white space
	foreach my $row_ref (@$rows_ref)
	{
		my $host = $row_ref->[0];
		my $itemid = $row_ref->[1];
		my $key = $row_ref->[2];

		# remove TLD from host name to get just the Probe name
		my $probe = substr($host, $tld_length);

		$result{$probe}->{$itemid} = $key;
	}

	fail("cannot find items ('", join("','", @_), "') at host ($tld *)") if (scalar(keys(%result)) == 0);

	return \%result;
}

# returns hash reference of Probe=>itemid of specified key
#
# {
#    'Amsterdam' => 'itemid1',
#    'London' => 'itemid2',
#    ...
# }
sub __get_status_itemids
{
	my $tld = shift;
	my $key = shift;

	my $key_condition = (substr($key, -1) eq ']' ? "i.key_='$key'" : "i.key_ like '$key%'");

	my $sql =
		"select h.host,i.itemid".
		" from items i,hosts h".
		" where i.hostid=h.hostid".
			" and i.templateid is not null".
			" and $key_condition".
			" and h.host like '$tld %'".
		" group by h.host,i.itemid";

	my $rows_ref = db_select($sql);

	fail("no items matching '$key' found at host '$tld %'") if (scalar(@$rows_ref) == 0);

	my %result;

	my $tld_length = length($tld) + 1; # white space
	foreach my $row_ref (@$rows_ref)
	{
		my $host = $row_ref->[0];
		my $itemid = $row_ref->[1];

		# remove TLD from host name to get just the Probe name
		my $probe = substr($host, $tld_length);

		$result{$probe} = $itemid;
	}

	return \%result;
}

#
# {
#     'Probe1' =>
#     [
#         {
#             'clock' => 1234234234,
#             'value' => 'Up'
#         },
#         {
#             'clock' => 1234234294,
#             'value' => 'Up'
#         }
#     ],
#     'Probe2' =>
#     [
#         {
#             'clock' => 1234234234,
#             'value' => 'Down'
#         },
#         {
#             'clock' => 1234234294,
#             'value' => 'Up'
#         }
#     ]
# }
#
sub __get_probe_statuses
{
	my $itemids_ref = shift;
	my $from = shift;
	my $till = shift;

	my %result;

	# generate list if itemids
	my $itemids_str = '';
	foreach my $probe (keys(%$itemids_ref))
	{
		$itemids_str .= ',' unless ($itemids_str eq '');
		$itemids_str .= $itemids_ref->{$probe};
	}

	if ($itemids_str ne '')
	{
		my $rows_ref = db_select("select itemid,value,clock from history_uint where itemid in ($itemids_str) and " . sql_time_condition($from, $till). " order by clock");

		foreach my $row_ref (@$rows_ref)
		{
			my $itemid = $row_ref->[0];
			my $value = $row_ref->[1];
			my $clock = $row_ref->[2];

			my $probe;
			foreach my $pr (keys(%$itemids_ref))
			{
				my $i = $itemids_ref->{$pr};

				if ($i == $itemid)
				{
					$probe = $pr;

					last;
				}
			}

			fail("internal error: Probe of item (itemid:$itemid) not found") unless (defined($probe));

			push(@{$result{$probe}}, {'value' => $value, 'clock' => $clock});
		}
	}

	return \%result;
}

sub __prnt
{
	print((defined($tld) ? "$tld: " : ''), join('', @_), "\n");
}

sub __prnt_json
{
	my $tr_ref = shift;

	if (opt('debug'))
	{
		dbg(JSON->new->utf8(1)->pretty(1)->encode($tr_ref), "-----------------------------------------------------------");
	}
	else
	{
		__prnt(ts_str($tr_ref->{'clock'}), " ", $tr_ref->{'status'});
	}
}

sub __selected_period
{
	my $from = shift;
	my $till = shift;

	return "till " . ts_str($till) if (!$from and $till);
	return "from " . ts_str($from) if ($from and !$till);
	return "from " . ts_str($from) . " till " . ts_str($till) if ($from and $till);

	return "any time";
}

sub __tld_ignored
{
	my $tld = shift;

	return SUCCESS if (exists($ignore_hash{$tld}));

	return E_FAIL;
}

__END__

=head1 NAME

update-api-data.pl - save information about the incidents to a filesystem

=head1 SYNOPSIS

update-api-data.pl [--service <dns|dnssec|rdds|epp>] [--tld <tld>|--ignore-file <file>] [--period <minutes>|--continue] [--dry-run] [--debug] [--help]

=head1 OPTIONS

=over 8

=item B<--service> service

Process only specified service. Service must be one of: dns, dnssec, rdds or epp.

=item B<--tld> tld

Process only specified TLD. If not specified all TLDs will be processed.

This option cannot be used together with option --ignore-file.

=item B<--ignore-file> file

Specify file containing the list of TLDs that should be ignored. TLDs are specified one per line.

This option cannot be used together with option --tld.

=item B<--period> minutes

Specify number of minutes of the period of calculation. This period is up till the latest available
data in the database.

This option cannot be used together with option --continue.

=item B<--continue>

Continue processing API data from the timestamp of the last run with --continue. In case of first
run with --continue all available data will be processed. The continue token is saved per each
TLD-service pair separately.

Note, that continue token will not be updated if this option was specified together with --dry-run.

This option cannot be used together with option --period.

=item B<--dry-run>

Print data to the screen, do not write anything to the filesystem.

=item B<--debug>

Run the script in debug mode. This means printing more information.

=item B<--help>

Print a brief help message and exit.

=back

=head1 DESCRIPTION

B<This program> will run through all the incidents found at optionally specified time bounds
and store details about each on the filesystem. This information will be used by external
program to provide it for users in convenient way.

=head1 EXAMPLES

./update-api-data.pl --tld example --period 10

This will update API data of the last 10 minutes of DNS, DNSSEC, RDDS and EPP services of TLD example.

=cut
