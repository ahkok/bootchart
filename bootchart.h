/*
 * bootchart.h
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


#define VERSION "0.1"

#define MAXCPUS         8
#define MAXPIDS     65535
#define MAXSAMPLES   8192


struct block_stat_struct {
	/* /proc/vmstat pgpgin & pgpgout */
	int bi;
	int bo;
};

struct cpu_stat_sample_struct {
	/* /proc/schedstat fields 10 & 11 (after name) */
	double runtime;
	double waittime;
};

struct cpu_stat_struct {
	/* per cpu array */
	struct cpu_stat_sample_struct sample[MAXSAMPLES];
};

/* per process, per sample data we will log */
struct ps_sched_struct {
	/* /proc/<n>/schedstat fields 1 & 2 */
	double runtime;
	double waittime;
	int pss;
};

/* process info */
struct ps_struct {
	struct ps_struct *children;
	struct ps_struct *next;

	/* must match - otherwise it's a new process with same PID */
	char name[16];
	int pid;
	int ppid;

	/* cache fd's */
	int sched;
	int schedstat;
	FILE *smaps;

	/* index to first/last seen timestamps */
	int first;
	int last;

	/* records actual start time, may be way before bootchart runs */
	double starttime;

	/* record human readable total cpu time */
	double total;

	/* largest PSS size found */
	int pss_max;

	/* for drawing connection lines later */
	double pos_x;
	double pos_y;

	struct ps_sched_struct sample[MAXSAMPLES];
};


extern double graph_start;
extern double log_start;
extern double sampletime[];
extern struct ps_struct *ps[]; /* ll */
extern struct block_stat_struct blockstat[];
extern struct cpu_stat_struct cpustat[];
extern int pscount;
extern int relative;
extern int filter;
extern int pss;
extern int samples;
extern int cpus;
extern int len;
extern int hz;
extern int overrun;
extern double interval;

extern FILE *of;

extern double gettime_ns(void);
extern void log_uptime(void);
extern void log_sample(int sample);

extern void svg_do(void);

