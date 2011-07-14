/*
 * bootchart.c
 *
 * Copyright (c) 2009 Intel Coproration
 * Authors:
 *   Auke Kok <auke-jan.h.kok@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */


#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <limits.h>


#include "bootchart.h"

double graph_start;
double sampletime[MAXSAMPLES];
struct ps_struct *ps[MAXPIDS]; /* ll */
struct block_stat_struct blockstat[MAXSAMPLES];
struct cpu_stat_struct cpustat[MAXCPUS];
int pscount;
int relative;
int filter = 1;
int samples;
int cpus;
double interval;
FILE *of;
int len = 500; /* we record len+1 (1 start sample) */
int hz = 25;   /* 20 seconds log time */
int overrun = 0;

int exiting = 0;

struct rlimit rlim;

void signal_handler(int sig)
{
	if (sig++)
		sig--;
	exiting = 1;
}


int main(int argc, char *argv[])
{
	struct sigaction sig;
	char output_path[PATH_MAX] = "/var/log";
	char output_file[PATH_MAX];
	char datestr[200];
	time_t t;
	FILE *f;
	int i;

	memset(&t, 0, sizeof(time_t));

	rlim.rlim_cur = 4096;
	rlim.rlim_max = 4096;
	(void) setrlimit(RLIMIT_NOFILE, &rlim);

	f = fopen("/etc/bootchartd.conf", "r");
	if (f) {
		char buf[256];
		char *key;
		char *val;

		while (fgets(buf, 80, f) != NULL) {
			char *c;

			c = strchr(buf, '\n');
			if (c) *c = 0; /* remove trailing \n */

			if (buf[0] == '#')
				continue; /* comment line */

			key = strtok(buf, "=");
			if (!key)
				continue;
			val = strtok(NULL, "=");
			if (!val)
				continue;

			// todo: filter leading/trailing whitespace

			if (!strcmp(key, "samples"))
				len = atoi(val);
			if (!strcmp(key, "freq"))
				hz = atoi(val);
			if (!strcmp(key, "rel"))
				relative = atoi(val);;
			if (!strcmp(key, "filter"))
				filter = atoi(val);;
			if (!strcmp(key, "output"))
				strncpy(output_path, val, PATH_MAX - 1);
		}
		fclose(f);
	}

	while (1) {
		static struct option opts[] = {
			{"rel", 0, NULL, 'r'},
			{"freq", 1, NULL, 'f'},
			{"samples", 1, NULL, 'n'},
			{"output", 1, NULL, 'o'},
			{"filter", 0, NULL, 'F'},
			{"help", 0, NULL, 'h'},
			{0, 0, NULL, 0}
		};

		int index = 0, c;

		c = getopt_long(argc, argv, "rf:n:o:Fh", opts, &index);
		if (c == -1)
			break;
		switch (c) {
		case 'r':
			relative = 1;
			break;
		case 'f':
			hz = atoi(optarg);
			break;
		case 'F':
			filter = 0;
			break;
		case 'n':
			len = atoi(optarg);
			break;
		case 'o':
			strncpy(output_path, optarg, PATH_MAX - 1);
			break;
		case 'h':
			fprintf(stderr, "Usage: %s [OPTIONS]\n", argv[0]);
			fprintf(stderr, " --rel,     -r            Record time relative to recording\n");
			fprintf(stderr, " --freq,    -f N          Sample frequency [%d]\n", hz);
			fprintf(stderr, " --samples, -n N          Stop sampling at [%d] samples\n", len);
			fprintf(stderr, " --output,  -o [PATH]     Path to output files [%s]\n", output_path);
			fprintf(stderr, " --filter,  -F            Disable filtering of processes from the graph\n");
			fprintf(stderr, "                          that are of less importance or short-lived\n");
			fprintf(stderr, " --help,    -h            Display this message\n");
			fprintf(stderr, "See the installed README and bootchartd.conf.example for more information.\n");
			exit (EXIT_SUCCESS);
			break;
		default:
			break;
		}
	}

	if (len > MAXSAMPLES) {
		fprintf(stderr, "Error: samples exceeds maximum\n");
		exit(EXIT_FAILURE);
	}

	/*
	 * If the kernel executed us through init=/sbin/bootchartd, then
	 * fork:
	 * - parent execs /sbin/init as pid=1
	 * - child logs data
	 */
	if (getpid() == 1) {
		if (fork()) {
			/* parent */
			execl("/sbin/init", "/sbin/init", NULL);
		}
	}

	/* handle TERM/INT nicely */
	memset(&sig, 0, sizeof(struct sigaction));
	sig.sa_handler = signal_handler;
	sigaction(SIGTERM, &sig, NULL);
	sigaction(SIGINT, &sig, NULL);

	interval = (1.0 / hz) * 1000000000.0;

	log_uptime();

	/* main program loop */
	while (!exiting) {
		int res;
		double sample_stop;
		struct timespec req;
		long newint_ns;

		sampletime[samples] = gettime_ns();

		/* wait for /proc to become available, discarding samples */
		if (!graph_start)
			log_uptime();
		else
			log_sample(samples);

		sample_stop = gettime_ns();

		req.tv_sec = 0;
		newint_ns = interval - ((sample_stop - sampletime[samples]) * 1000000000);

		/*
		 * check if we have not consumed our entire timeslice. If we
		 * do, don't sleep and take a new sample right away.
		 * we'll lose all the missed samples and overrun our total
		 * time
		 */
		if (newint_ns > 0) {
			req.tv_nsec = newint_ns;

			res = nanosleep(&req, NULL);
			if (res) {
				perror("nanosleep()");
				exit (EXIT_FAILURE);
			}
		} else {
			overrun++;
			/* calculate how many samples we lost and scrap them */
			len = len + ((int)(newint_ns / interval));
		}

		samples++;

		if (samples > len)
			break;

	}

	/* do some cleanup, close fd's */
	for ( i = 0; i < samples ; i++) {
		if (!ps[i])
			continue;
		if (ps[i]-> schedstat)
			close(ps[i]->schedstat);
		if (ps[i]->sched)
			close(ps[i]->sched);
		if (ps[i]->smaps)
			fclose(ps[i]->smaps);
	}

	t = time(NULL);
	strftime(datestr, sizeof(datestr), "%Y%m%d-%H%M", localtime(&t));
	sprintf(output_file, "%s/bootchart-%s.svg", output_path, datestr);

	of = fopen(output_file, "w");
	if (!of) {
		perror("open output_file");
		exit (EXIT_FAILURE);
	}

	svg_do();

	fclose(of);

	/* don't complain when overrun once, happens most commonly on 1st sample */
	if (overrun > 1)
		fprintf(stderr, "Warning: sample time overrun %i times\n", overrun);

	return 0;
}
