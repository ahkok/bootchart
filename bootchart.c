/*
 * bootchart.c
 *
 * Copyright (c) 2009 Intel Coproration
 * Authors:
 *   Auke Kok <auke-jan.h.kok@intel.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <sys/time.h>
#include <sys/types.h>
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
int samples;
double interval;
FILE *of;

int exiting = 0;


void signal_handler(int signum)
{
	signum++;
	exiting = 1;
}


int main(int argc, char *argv[])
{
	struct sigaction sig;
	int len = 150; /* we record len+1 (1 start sample) */
	int hz = 10;   /* 15 seconds log time */
	char output_path[PATH_MAX] = "/var/log/";
	char output_file[PATH_MAX];
	char datestr[200];
	time_t t;
	long overrun = 0;

	while (1) {
		static struct option opts[] = {
			{"rel", 0, NULL, 'r'},
			{"freq", 1, NULL, 'f'},
			{"samples", 1, NULL, 'n'},
			{"output", 1, NULL, 'o'},
			{"help", 0, NULL, 'h'},
			{0, 0, NULL, 0}
		};

		int index = 0, c;

		c = getopt_long(argc, argv, "rf:n:o:t:h", opts, &index);
		if (c == -1)
			break;
		switch (c) {
		case 'r':
			relative = 1;
			break;
		case 'f':
			hz = atoi(optarg);
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
			fprintf(stderr, " --help,    -h            Display this message\n");
			exit (EXIT_FAILURE);
			break;
		default:
			break;
		}
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
		}

		if (++samples > len)
			break;

	}

	t = time(NULL);
	strftime(datestr, sizeof(datestr), "%Y%m%d-%H%M", localtime(&t));
	sprintf(output_file, "%s/bootchart-%s.svg", output_path, datestr);

	of = fopen(output_file, "w");
	if (!of)
		exit (EXIT_FAILURE);

	svg_do();

	fclose(of);

	if (overrun)
		fprintf(stderr, "Warning: sample time overrun %lu times", overrun);

	return 0;
}
