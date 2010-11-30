/*
 * log.c
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

#define _GNU_SOURCE 1
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>


#include "bootchart.h"


double gettime_ns(void)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);

	return (now.tv_sec + (now.tv_nsec / 1000000000.0));
}


void log_uptime(void)
{
	FILE *f;
	char str[32];
	double now;
	double uptime;

	f = fopen("/proc/uptime", "r");
	if (!f)
		return;
	if (!fscanf(f, "%s %*s", str)) {
		fclose(f);
		return;
	}
	fclose(f);
	uptime = strtod(str, NULL);

	now = gettime_ns();

	/* start graph at kernel boot time */
	if (relative)
		graph_start = now;
	else
		graph_start = now - uptime;
}


static char *bufgetline(char *buf)
{
	char *c;

	if (!buf)
		return NULL;

	c = strchr(buf, '\n');
	if (c)
		c++;
	return c;
}


void log_sample(int sample)
{
	static int vmstat;
	static int schedstat;
	static DIR *proc;
	FILE *stat;
	char buf[4095];
	char key[256];
	char val[256];
	char rt[256];
	char wt[256];
	char *m;
	int c;
	int p;
	int s;
	int n;
	struct dirent *ent;

	if (!vmstat) {
		/* block stuff */
		vmstat = open("/proc/vmstat", O_RDONLY);
		if (vmstat == -1) {
			perror("open /proc/vmstat");
			exit (EXIT_FAILURE);
		}
	}

	n = pread(vmstat, buf, sizeof(buf) - 1, 0);
	if (n <= 0) {
		close(vmstat);
		return;
	}
	buf[n] = '\0';

	m = buf;
	while (m) {
		if (sscanf(m, "%s %s", key, val) < 2)
			goto vmstat_next;
		if (!strcmp(key, "pgpgin"))
			blockstat[sample].bi = atoi(val);
		if (!strcmp(key, "pgpgout")) {
			blockstat[sample].bo = atoi(val);
			break;
		}
vmstat_next:
		m = bufgetline(m);
		if (!m)
			break;
	}

	if (!schedstat) {
		/* overall CPU utilization */
		schedstat = open("/proc/schedstat", O_RDONLY);
		if (schedstat == -1) {
			perror("open /proc/schedstat");
			exit (EXIT_FAILURE);
		}
	}

	n = pread(schedstat, buf, sizeof(buf) - 1, 0);
	if (n <= 0) {
		close(schedstat);
		return;
	}
	buf[n] = '\0';

	m = buf;
	while (m) {
		if (sscanf(m, "%s %*s %*s %*s %*s %*s %*s %s %s", key, rt, wt) < 3)
			goto schedstat_next;

		if (strstr(key, "cpu")) {
			c = key[3] - '0';
			cpustat[c].sample[sample].runtime = atoll(rt);
			cpustat[c].sample[sample].waittime = atoll(wt);

			if (c == cpus)
				cpus = c + 1;
		}
schedstat_next:
		m = bufgetline(m);
		if (!m)
			break;
	}

	/* all the per-process stuff goes here */
	if (!proc) {
		/* find all processes */
		proc = opendir("/proc");
		if (!proc)
			return;
	} else {
		rewinddir(proc);
	}

	while ((ent = readdir(proc)) != NULL) {
		char filename[PATH_MAX];
		int pid;

		if ((ent->d_name[0] < '0') || (ent->d_name[0] > '9'))
			continue;

		pid = atoi(ent->d_name);

		if (pid > MAXPIDS)
			continue;

		if (!ps[pid]) {
			char t[32];
			struct ps_struct *parent;

			pscount++;

			/* alloc & insert */
			ps[pid] = malloc(sizeof(struct ps_struct));
			if (!ps[pid]) {
				perror("malloc ps[pid]");
				exit (EXIT_FAILURE);
			}
			memset(ps[pid], 0, sizeof(struct ps_struct));

			/* mark our first sample */
			ps[pid]->first = sample;

			/* get name, start time */
			if (!ps[pid]->sched) {
				sprintf(filename, "/proc/%d/sched", pid);
				ps[pid]->sched = open(filename, O_RDONLY);
				if (ps[pid]->sched == -1)
					continue;
			}

			s = pread(ps[pid]->sched, buf, sizeof(buf) - 1, 0);
			if (s <= 0) {
				close(ps[pid]->sched);
				continue;
			}

			if (!sscanf(buf, "%s %*s %*s", key))
				continue;

			strncpy(ps[pid]->name, key, 16);
			/* discard line 2 */
			m = bufgetline(buf);
			if (!m)
				continue;

			m = bufgetline(m);
			if (!m)
				continue;

			if (!sscanf(m, "%*s %*s %s", t))
				continue;

			ps[pid]->starttime = strtod(t, NULL) / 1000.0;

			/* ppid */
			sprintf(filename, "/proc/%d/stat", pid);
			stat = fopen(filename, "r");
			if (!stat)
				continue;
			if (!fscanf(stat, "%*s %*s %*s %i", &p)) {
				fclose(stat);
				continue;
			}
			fclose(stat);
			ps[pid]->ppid = p;

			/*
			 * setup child pointers
			 *
			 * these are used to paint the tree coherently later
			 * each parent has a LL of children, and a LL of siblings
			 */
			if (pid == 1)
				continue; /* nothing to do for init atm */

			parent = ps[ps[pid]->ppid];

			if (!parent) {
				/* fix this asap */
				ps[pid]->ppid = 1;
				parent = ps[1];
			}

			if (!parent->children) {
				/* it's the first child */
				parent->children = ps[pid];
			} else {
				/* walk all children and append */
				struct ps_struct *children;
				children = parent->children;
				while (children->next)
					children = children->next;
				children->next = ps[pid];
			}
		}

		if (!ps[pid]->schedstat) {
			sprintf(filename, "/proc/%d/schedstat", pid);
			ps[pid]->schedstat = open(filename, O_RDONLY);
			if (ps[pid]->schedstat == -1)
				continue;
		}

		if (pread(ps[pid]->schedstat, buf, sizeof(buf) - 1, 0) <= 0) {
			close(ps[pid]->schedstat);
			continue;
		}
		if (!sscanf(buf, "%s %s %*s", rt, wt))
			continue;

		ps[pid]->pid = pid;
		ps[pid]->last = sample;
		ps[pid]->sample[sample].runtime = atoll(rt);
		ps[pid]->sample[sample].waittime = atoll(wt);

		ps[pid]->total = (ps[pid]->sample[ps[pid]->last].runtime
				 - ps[pid]->sample[ps[pid]->first].runtime)
				 / 1000000000.0;

		/* catch process rename, try to randomize time */
		if (((samples - ps[pid]->first) + pid) % (hz / 4) == 0) {

			/* re-fetch name */
			/* get name, start time */
			if (!ps[pid]->sched) {
				sprintf(filename, "/proc/%d/sched", pid);
				ps[pid]->sched = open(filename, O_RDONLY);
				if (ps[pid]->sched == -1)
					continue;
			}
			if (pread(ps[pid]->sched, buf, sizeof(buf) - 1, 0) <= 0) {
				close(ps[pid]->sched);
				continue;
			}

			if (!sscanf(buf, "%s %*s %*s", key))
				continue;

			strncpy(ps[pid]->name, key, 16);
		}
	}
}

