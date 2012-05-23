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

/*
 * Alloc a static 4k buffer for stdio - primarily used to increase
 * PSS buffering from the default 1k stdin buffer to reduce
 * read() overhead.
 */
static char smaps_buf[4096];
DIR *proc;


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

	log_start = gettime_ns();

	/* start graph at kernel boot time */
	if (relative)
		graph_start = log_start;
	else
		graph_start = log_start - uptime;
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
			c = atoi((const char*)(key+3));
			if (c > MAXCPUS)
				/* Oops, we only have room for MAXCPUS data */
				break;
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
		struct ps_struct *ps;

		if ((ent->d_name[0] < '0') || (ent->d_name[0] > '9'))
			continue;

		pid = atoi(ent->d_name);

		if (pid >= MAXPIDS)
			continue;

		ps = ps_first;
		while (ps->next_ps) {
			ps = ps->next_ps;
			if (ps->pid == pid)
				break;
		}

		/* end of our LL? then append a new record */
		if (ps->pid != pid) {
			char t[32];
			struct ps_struct *parent;

			ps->next_ps = malloc(sizeof(struct ps_struct));
			if (!ps->next_ps) {
				perror("malloc(ps_struct)");
				exit (EXIT_FAILURE);
			}
			memset(ps->next_ps, 0, sizeof(struct ps_struct));
			ps = ps->next_ps;
			ps->pid = pid;

			pscount++;

			/* mark our first sample */
			ps->first = sample;

			/* get name, start time */
			if (!ps->sched) {
				sprintf(filename, "/proc/%d/sched", pid);
				ps->sched = open(filename, O_RDONLY);
				if (ps->sched == -1)
					continue;
			}

			s = pread(ps->sched, buf, sizeof(buf) - 1, 0);
			if (s <= 0) {
				close(ps->sched);
				continue;
			}

			if (!sscanf(buf, "%s %*s %*s", key))
				continue;

			strncpy(ps->name, key, 16);
			/* discard line 2 */
			m = bufgetline(buf);
			if (!m)
				continue;

			m = bufgetline(m);
			if (!m)
				continue;

			if (!sscanf(m, "%*s %*s %s", t))
				continue;

			ps->starttime = strtod(t, NULL) / 1000.0;

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
			ps->ppid = p;

			/*
			 * setup child pointers
			 *
			 * these are used to paint the tree coherently later
			 * each parent has a LL of children, and a LL of siblings
			 */
			if (pid == 1)
				continue; /* nothing to do for init atm */

			/* kthreadd has ppid=0, which breaks our tree ordering */
			if (ps->ppid == 0)
				ps->ppid = 1;

			parent = ps_first;
			while ((parent->next_ps && parent->pid != ps->ppid))
				parent = parent->next_ps;

			if ((!parent) || (parent->pid != ps->ppid)) {
				/* orphan */
				ps->ppid = 1;
				parent = ps_first->next_ps;
			}

			ps->parent = parent;

			if (!parent->children) {
				/* it's the first child */
				parent->children = ps;
			} else {
				/* walk all children and append */
				struct ps_struct *children;
				children = parent->children;
				while (children->next)
					children = children->next;
				children->next = ps;
			}
		}

		/* else -> found pid, append data in ps */

		/* below here is all continuous logging parts - we get here on every
		 * iteration */

		/* rt, wt */
		if (!ps->schedstat) {
			sprintf(filename, "/proc/%d/schedstat", pid);
			ps->schedstat = open(filename, O_RDONLY);
			if (ps->schedstat == -1)
				continue;
		}

		if (pread(ps->schedstat, buf, sizeof(buf) - 1, 0) <= 0) {
			/* clean up our file descriptors - assume that the process exited */
			close(ps->schedstat);
			if (ps->sched)
				close(ps->sched);
			//if (ps->smaps)
			//	fclose(ps->smaps);
			continue;
		}
		if (!sscanf(buf, "%s %s %*s", rt, wt))
			continue;

		ps->last = sample;
		ps->sample[sample].runtime = atoll(rt);
		ps->sample[sample].waittime = atoll(wt);

		ps->total = (ps->sample[ps->last].runtime
				 - ps->sample[ps->first].runtime)
				 / 1000000000.0;

		if (!pss)
			goto catch_rename;
		/* Pss */
		if (!ps->smaps) {
			sprintf(filename, "/proc/%d/smaps", pid);
			ps->smaps = fopen(filename, "r");
			setvbuf(ps->smaps, smaps_buf, _IOFBF, sizeof(smaps_buf));
			if (!ps->smaps)
				continue;
		} else {
			rewind(ps->smaps);
		}

		while (1) {
			/* skip one line, this contains the object mapped */
			if (fgets(buf, sizeof(buf) -1, ps->smaps) == NULL)
				break;
			/* then there's a 28 char 14 line block */
			if (fread(buf, 1, 28 * 14, ps->smaps) != 28 * 14)
				break;

			int p;
			p = atoi(&buf[61]);
			ps->sample[sample].pss += p;
		}

		if (ps->sample[sample].pss > ps->pss_max)
			ps->pss_max = ps->sample[sample].pss;

catch_rename:
		/* catch process rename, try to randomize time */
		if (((samples - ps->first) + pid) % (hz / 4) == 0) {

			/* re-fetch name */
			/* get name, start time */
			if (!ps->sched) {
				sprintf(filename, "/proc/%d/sched", pid);
				ps->sched = open(filename, O_RDONLY);
				if (ps->sched == -1)
					continue;
			}
			if (pread(ps->sched, buf, sizeof(buf) - 1, 0) <= 0) {
				/* clean up file descriptors */
				close(ps->sched);
				if (ps->schedstat)
					close(ps->schedstat);
				//if (ps->smaps)
				//	fclose(ps->smaps);
				continue;
			}

			if (!sscanf(buf, "%s %*s %*s", key))
				continue;

			strncpy(ps->name, key, 16);
		}
	}
}

