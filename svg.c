/*
 * svg.c
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <sys/utsname.h>

#include "bootchart.h"


#define SCALE_X 100.0  /* 100px per second */
#define SCALE_Y 20   /* 1 process bar is 16px high */

#define time_to_graph(t) ((t) * SCALE_X)
#define ps_to_graph(n) ((n) * SCALE_Y)
#define to_color(n) (192.0 - ((n) * 192.0))

#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

char str[8092];

#define svg(a...) do { sprintf(str, ## a); fputs(str, of); fflush(of); } while (0)

int filtered;
double idletime = -1.0;


void svg_header(void)
{
	svg("<?xml version=\"1.0\" standalone=\"no\"?>\n");
	svg("<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" ");
	svg("\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");

	svg("<svg width=\"100%%\" height=\"100%%\" version=\"1.1\" ");
	svg("xmlns=\"http://www.w3.org/2000/svg\">\n\n");

	/* style sheet */
	svg("<defs>\n  <style type=\"text/css\">\n    <![CDATA[\n");

	svg("      rect       { stroke-width: 1; }\n");
	svg("      rect.cpu   { fill: rgb(64,64,240); stroke-width: 0; fill-opacity: 0.7; }\n");
	svg("      rect.wait  { fill: rgb(240,240,0); stroke-width: 0; fill-opacity: 0.7; }\n");
	svg("      rect.bi    { fill: rgb(240,128,128); stroke-width: 0; fill-opacity: 0.7; }\n");
	svg("      rect.bo    { fill: rgb(192,64,64); stroke-width: 0; fill-opacity: 0.7; }\n");
	svg("      rect.ps    { fill: rgb(192,192,192); stroke: rgb(128,128,128); fill-opacity: 0.7; }\n");
	svg("      rect.box   { fill: rgb(240,240,240); stroke: rgb(192,192,192); }\n");

	svg("      line       { stroke: rgb(64,64,64); stroke-width: 1; }\n");
	svg("//    line.sec1  { }\n");
	svg("      line.sec5  { stroke-width: 2; }\n");
	svg("      line.sec01 { stroke: rgb(224,224,224); stroke-width: 1; }\n");
	svg("      line.dot   { stroke-dasharray: 1 2; }\n");
	svg("      line.idle  { stroke: rgb(64,64,64); stroke-dasharray: 10 6; stroke-opacity: 0.7; }\n");

	svg("      .run       { font-size: 8; font-style: italic; }\n");
	svg("      text       { font-family: Verdana, Helvetica; font-size: 10; }\n");
	svg("      text.sec   { font-size: 8; }\n");
	svg("      text.t1    { font-size: 24; }\n");
	svg("      text.t2    { font-size: 12; }\n");
	svg("      text.idle  { font-size: 18; }\n");
	
	svg("    ]]>\n   </style>\n</defs>\n\n");

}


void svg_title(void)
{
	char cmdline[256] = "";
	char filename[PATH_MAX];
	char buf[256];
	char rootbdev[16] = "Unknown";
	char model[256] = "Unknown";
	char date[256] = "Unknown";
	char cpu[256] = "Unknown";
	char build[256] = "Unknown";
	char *c;
	FILE *f;
	time_t t;
	struct utsname uts;

	/* grab /proc/cmdline */
	f = fopen("/proc/cmdline", "r");
	if (f) {
		if (!fgets(cmdline, 255, f))
			sprintf(cmdline, "Unknown");
		fclose(f);
	}

	/* extract root fs so we can find disk model name in sysfs */
	c = strstr(cmdline, "root=/dev/");
	if (c) {
		strncpy(rootbdev, &c[10], 3);
		rootbdev[3] = '\0';
	}
	sprintf(filename, "/sys/block/%s/device/model", rootbdev);
	f = fopen(filename, "r");
	if (f) {
		if (!fgets(model, 255, f))
			fprintf(stderr, "Error reading disk model for %s\n", rootbdev);
		fclose(f);
	}

	/* various utsname parameters */
	if (uname(&uts))
		fprintf(stderr, "Error getting uname info\n");

	/* date */
	t = time(NULL);
	strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S %z", localtime(&t));

	/* CPU type */
	f = fopen("/proc/cpuinfo", "r");
	if (f) {
		while (fgets(buf, 255, f)) {
			if (strstr(buf, "model name")) {
				strncpy(cpu, &buf[13], 255);
				break;
			}
		}
		fclose(f);
	}

	/* Build */
	f = fopen("/etc/issue", "r");
	if (f) {
		while (fgets(buf, 255, f)) {
			if (strstr(buf, "Build:")) {
				strncpy(build, &buf[6], 255);
				break;
			}
		}
		fclose(f);
	}

	svg("<text class=\"t1\" x=\"0\" y=\"30\">Bootchart for %s - %s</text>\n",
	    uts.nodename, date);
	svg("<text class=\"t2\" x=\"20\" y=\"50\">System: %s %s %s %s</text>\n",
	    uts.sysname, uts.release, uts.version, uts.machine);
	svg("<text class=\"t2\" x=\"20\" y=\"65\">CPU: %s</text>\n",
	    cpu);
	svg("<text class=\"t2\" x=\"20\" y=\"80\">Disk: %s</text>\n",
	    model);
	svg("<text class=\"t2\" x=\"20\" y=\"95\">Boot options: %s</text>\n",
	    cmdline);
	svg("<text class=\"t2\" x=\"20\" y=\"110\">Build: %s</text>\n",
	    build);
	svg("<text class=\"t2\" x=\"20\" y=\"125\">Idle time: ");
	if (idletime >= 0.0)
		svg("%.03fs", idletime);
	else
		svg("Not detected");
	svg("</text>\n");
	svg("<text class=\"sec\" x=\"20\" y=\"140\">Graph data: %i samples/sec, recorded %i total, dropped %i samples, %i processes, %i filtered</text>\n",
	    hz, len, overrun, pscount, filtered);
}


void svg_graph_box(int height)
{
	double d = 0.0;
	int i = 0;

	/* outside box, fill */
	svg("<rect class=\"box\" x=\"%.03f\" y=\"0\" width=\"%.03f\" height=\"%i\" />\n",
	    time_to_graph(0),
	    time_to_graph(sampletime[samples-1] - graph_start),
	    ps_to_graph(height));

	for (d = graph_start; d <= sampletime[samples-1]; d += 0.1) {
		/* lines for each second */
		if (i % 50 == 0)
			svg("  <line class=\"sec5\" x1=\"%.03f\" y1=\"0\" x2=\"%.03f\" y2=\"%i\" />\n",
			    time_to_graph(d - graph_start),
			    time_to_graph(d - graph_start),
			    ps_to_graph(height));
		else if (i % 10 == 0)
			svg("  <line class=\"sec1\" x1=\"%.03f\" y1=\"0\" x2=\"%.03f\" y2=\"%i\" />\n",
			    time_to_graph(d - graph_start),
			    time_to_graph(d - graph_start),
			    ps_to_graph(height));
		else
			svg("  <line class=\"sec01\" x1=\"%.03f\" y1=\"0\" x2=\"%.03f\" y2=\"%i\" />\n",
			    time_to_graph(d - graph_start),
			    time_to_graph(d - graph_start),
			    ps_to_graph(height));

		/* time label */
		if (i % 10 == 0)
			svg("  <text class=\"sec\" x=\"%.03f\" y=\"%i\" >%.01fs</text>\n",
			    time_to_graph(d - graph_start),
			    -5,
			    d - graph_start);

		i++;
	}
}


void svg_io_bi_bar(void)
{
	double max = 0.0;
	double range;
	int max_here = 0;
	int i;

	svg("<!-- IO utilization graph - In -->\n");

	svg("<text class=\"t2\" x=\"5\" y=\"-15\">IO utilization - read</text>\n");

	/*
	 * calculate rounding range
	 *
	 * We need to round IO data since IO block data is not updated on
	 * each poll. Applying a smoothing function loses some burst data,
	 * so keep the smoothing range short.
	 */
	range = 0.25 / (1.0 / hz);
	if (range < 2.0)
		range = 2.0; /* no smoothing */

	/* surrounding box */
	svg_graph_box(5);

	/* find the max IO first */
	for (i = 1; i < samples; i++) {
		int start;
		int stop;
		double tot;

		start = max(i - ((range / 2) - 1), 0);
		stop = min(i + (range / 2), samples - 1);

		tot = (double)(blockstat[stop].bi - blockstat[start].bi)
		      / (stop - start);
		if (tot > max) {
			max = tot;
			max_here = i;
		}
		tot = (double)(blockstat[stop].bo - blockstat[start].bo)
		      / (stop - start);
		if (tot > max)
			max = tot;
	}

	/* plot bi */
	for (i = 1; i < samples; i++) {
		int start;
		int stop;
		double tot;
		double pbi;

		start = max(i - ((range / 2) - 1), 0);
		stop = min(i + (range / 2), samples);

		tot = (double)(blockstat[stop].bi - blockstat[start].bi)
		      / (stop - start);
		pbi = tot / max;

		if (pbi > 0.001)
			svg("<rect class=\"bi\" x=\"%.03f\" y=\"%.03f\" width=\"%.03f\" height=\"%.03f\" />\n",
			    time_to_graph(sampletime[i - 1] - graph_start),
			    100.0 - (pbi * 100.0),
			    time_to_graph(sampletime[i] - sampletime[i - 1]),
			    pbi * 100.0);

		/* labels around highest value */
		if (i == max_here) {
			svg("  <text class=\"sec\" x=\"%.03f\" y=\"%.03f\">%0.2fmb/sec</text>\n",
			    time_to_graph(sampletime[i] - graph_start) + 5,
			    (100.0 - (pbi * 100.0)) + 15,
			    max / 1024.0 / (interval / 1000000000.0));
		}
	}
}

void svg_io_bo_bar(void)
{
	double max = 0.0;
	double range;
	int max_here = 0;
	int i;

	svg("<!-- IO utilization graph - out -->\n");

	svg("<text class=\"t2\" x=\"5\" y=\"-15\">IO utilization - write</text>\n");

	/*
	 * calculate rounding range
	 *
	 * We need to round IO data since IO block data is not updated on
	 * each poll. Applying a smoothing function loses some burst data,
	 * so keep the smoothing range short.
	 */
	range = 0.25 / (1.0 / hz);
	if (range < 2.0)
		range = 2.0; /* no smoothing */

	/* surrounding box */
	svg_graph_box(5);

	/* find the max IO first */
	for (i = 1; i < samples; i++) {
		int start;
		int stop;
		double tot;

		start = max(i - ((range / 2) - 1), 0);
		stop = min(i + (range / 2), samples - 1);

		tot = (double)(blockstat[stop].bi - blockstat[start].bi)
		      / (stop - start);
		if (tot > max)
			max = tot;
		tot = (double)(blockstat[stop].bo - blockstat[start].bo)
		      / (stop - start);
		if (tot > max) {
			max = tot;
			max_here = i;
		}
	}

	/* plot bo */
	for (i = 1; i < samples; i++) {
		int start;
		int stop;
		double tot;
		double pbo;

		start = max(i - ((range / 2) - 1), 0);
		stop = min(i + (range / 2), samples);

		tot = (double)(blockstat[stop].bo - blockstat[start].bo)
		      / (stop - start);
		pbo = tot / max;

		if (pbo > 0.001)
			svg("<rect class=\"bo\" x=\"%.03f\" y=\"%.03f\" width=\"%.03f\" height=\"%.03f\" />\n",
			    time_to_graph(sampletime[i - 1] - graph_start),
			    100.0 - (pbo * 100.0),
			    time_to_graph(sampletime[i] - sampletime[i - 1]),
			    pbo * 100.0);

		/* labels around highest bo value */
		if (i == max_here) {
			svg("  <text class=\"sec\" x=\"%.03f\" y=\"%.03f\">%0.2fmb/sec</text>\n",
			    time_to_graph(sampletime[i] - graph_start) + 5,
			    (100.0 - (pbo * 100.0)),
			    max / 1024.0 / (interval / 1000000000.0));
		}
	}
}


void svg_cpu_bar(void)
{
	int i;

	svg("<!-- CPU utilization graph -->\n");

	svg("<text class=\"t2\" x=\"5\" y=\"-15\">CPU utilization</text>\n");
	/* surrounding box */
	svg_graph_box(5);

	/* bars for each sample, proportional to the CPU util. */
	for (i = 1; i < samples; i++) {
		int c;
		double trt;
		double ptrt;

		ptrt = trt = 0.0;

		for (c = 0; c < cpus; c++)
			trt += cpustat[c].sample[i].runtime - cpustat[c].sample[i - 1].runtime;

		trt = trt / 1000000000.0;

		trt = trt / (double)cpus;

		if (trt > 0.0)
			ptrt = trt / (sampletime[i] - sampletime[i - 1]);

		if (ptrt > 1.0)
			ptrt = 1.0;

		if (ptrt > 0.001) {
			svg("<rect class=\"cpu\" x=\"%.03f\" y=\"%.03f\" width=\"%.03f\" height=\"%.03f\" />\n",
			    time_to_graph(sampletime[i - 1] - graph_start),
			    100.0 - (ptrt * 100.0),
			    time_to_graph(sampletime[i] - sampletime[i - 1]),
			    ptrt * 100.0);
		}
	}
}

void svg_wait_bar(void)
{
	int i;

	svg("<!-- Wait time aggregation box -->\n");

	svg("<text class=\"t2\" x=\"5\" y=\"-15\">CPU wait</text>\n");

	/* surrounding box */
	svg_graph_box(5);

	/* bars for each sample, proportional to the CPU util. */
	for (i = 1; i < samples; i++) {
		int c;
		double twt;
		double ptwt;

		ptwt = twt = 0.0;

		for (c = 0; c < cpus; c++)
			twt += cpustat[c].sample[i].waittime - cpustat[c].sample[i - 1].waittime;

		twt = twt / 1000000000.0;

		twt = twt / (double)cpus;

		if (twt > 0.0)
			ptwt = twt / (sampletime[i] - sampletime[i - 1]);

		if (ptwt > 1.0)
			ptwt = 1.0;

		if (ptwt > 0.001) {
			svg("<rect class=\"wait\" x=\"%.03f\" y=\"%.03f\" width=\"%.03f\" height=\"%.03f\" />\n",
			    time_to_graph(sampletime[i - 1] - graph_start),
			    (100.0 - (ptwt * 100.0)),
			    time_to_graph(sampletime[i] - sampletime[i - 1]),
			    ptwt * 100.0);
		}
	}
}


int get_next_ps(int start)
{
	/*
	 * walk the list of processes and return the next one to be
	 * painted
	 */

	int here = start;
	struct ps_struct *children;
	struct ps_struct *siblings;

	/* start with init [1] */
	if (here == 0) {
		here = 1;
		return here;
	}

	children = ps[here]->children;

	/* go deep */
	if (children) {
		here = ps[here]->children->pid;
		return here;
	}

	/* find siblings */
	siblings = ps[here]->next;
	if (siblings) {
		here = ps[here]->next->pid;
		return here;
	}

	/* go back for parent siblings */
	while (ps[ps[here]->ppid]) {
		here = ps[ps[here]->ppid]->pid;
		/* go to sibling of parent */
		if (ps[here]->next) {
			here = ps[here]->next->pid;
			return here;
		}
	}

	return 0;
}


int ps_filter(int pid)
{
	if (!filter)
		return 0;

	/* can't draw data when there is only 1 sample (need start + stop) */
	if (ps[pid]->first == ps[pid]->last)
		return -1;

	/* don't filter kthreadd */
	if (pid == 2)
		return 0;

	/* drop stuff that doesn't use any real CPU time */
	if (ps[pid]->total <= 0.001)
		return -1;

	return 0;
}


void svg_ps_bars(void)
{
	int i = 0;
	int j = 0;
	int pc = 0;
	int wt;
	int pid;

	svg("<!-- Process graph -->\n");

	svg("<text class=\"t2\" x=\"5\" y=\"-15\">Processes</text>\n");

	/* pass 1 - pre-count processes to draw */
	while ((i = get_next_ps(i))) {
		if (!ps_filter(i))
			pc++;
		else
			filtered++;
	}

	/* surrounding box */
	svg_graph_box(pc);

	/* pass 2 - ps boxes */
	i = 0;
	while ((i = get_next_ps(i))) {
		double starttime;
		int t;

		if (!ps[i])
			continue;

		/* leave some trace of what we actually filtered etc. */
		svg("<!-- %s [%i] ppid=%i runtime=%.03fs -->\n", ps[i]->name, i,
		    ps[i]->ppid, ps[i]->total);

		/* it would be nice if we could use exec_start from /proc/pid/sched,
		 * but it's unreliable and gives bogus numbers */
		starttime = sampletime[ps[i]->first];

		/* remember where _to_ our children need to draw a line */
		ps[i]->pos_x = time_to_graph(starttime - graph_start);
		ps[i]->pos_y = ps_to_graph(j+1); /* bottom left corner */

		/* filter */
		if (ps_filter(i)) {
// FIXME
			/* if this is the last child, we might still need to draw a connecting line */
			if ((!ps[i]->next) && (ps[ps[i]->ppid]))
				svg("  <line class=\"dot\" x1=\"%.03f\" y1=\"%i\" x2=\"%.03f\" y2=\"%.03f\" />\n",
				    ps[ps[i]->ppid]->pos_x,
				    ps_to_graph(j-1) + 10, /* whee, use the last value here */
				    ps[ps[i]->ppid]->pos_x,
				    ps[ps[i]->ppid]->pos_y);
			continue;
		}

		svg("  <rect class=\"ps\" x=\"%.03f\" y=\"%i\" width=\"%.03f\" height=\"%i\" />\n",
		    time_to_graph(starttime - graph_start),
		    ps_to_graph(j),
		    time_to_graph(sampletime[ps[i]->last] - starttime),
		    ps_to_graph(1));

		/* paint cpu load over these */
		for (t = ps[i]->first + 1; t < ps[i]->last; t++) {
			double rt, prt;
			double wt, wrt;

			/* calculate over interval */
			rt = ps[i]->sample[t].runtime - ps[i]->sample[t-1].runtime;
			wt = ps[i]->sample[t].waittime - ps[i]->sample[t-1].waittime;

			prt = (rt / 1000000000) / (sampletime[t] - sampletime[t-1]);
			wrt = (wt / 1000000000) / (sampletime[t] - sampletime[t-1]);

			/* this can happen if timekeeping isn't accurate enough */
			if (prt > 1.0)
				prt = 1.0;
			if (wrt > 1.0)
				wrt = 1.0;

			if ((prt < 0.1) && (wrt < 0.1)) /* =~ 26 (color threshold) */
				continue;

			svg("    <rect class=\"wait\" x=\"%.03f\" y=\"%i\" width=\"%.03f\" height=\"%.03f\" />\n",
			    time_to_graph(sampletime[t - 1] - graph_start),
			    ps_to_graph(j),
			    time_to_graph(sampletime[t] - sampletime[t - 1]),
			    ps_to_graph(wrt));

			/* draw cpu over wait - TODO figure out how/why run + wait > interval */
			svg("    <rect class=\"cpu\" x=\"%.03f\" y=\"%.03f\" width=\"%.03f\" height=\"%.03f\" />\n",
			    time_to_graph(sampletime[t - 1] - graph_start),
			    ps_to_graph(j + (1.0 - prt)),
			    time_to_graph(sampletime[t] - sampletime[t - 1]),
			    ps_to_graph(prt));
		}

		/* determine where to display the process name */
		if (sampletime[ps[i]->last] - sampletime[ps[i]->first] < 1.5) {
			wt = ps[i]->last;
		} else if (ps[i]->sample[ps[i]->last].runtime - ps[i]->sample[ps[i]->first].runtime < (interval / 100.0)) {
			wt = ps[i]->first;
		} else {
			/* walk the process left-to-right in time and determine when
			 * it's done more than 99%, 95% of it's cpu load, and print the label
			 * -after- that time */
			for (wt = ps[i]->first; wt < ps[i]->last - (1.5 * hz); wt++)
				if (((ps[i]->sample[wt].runtime - ps[i]->sample[ps[i]->first].runtime) /
				     (ps[i]->sample[ps[i]->last].runtime - ps[i]->sample[ps[i]->first].runtime))
				    >= 0.99)
					goto labelpos;
			for (wt = ps[i]->first; wt < ps[i]->last - (1.5 * hz); wt++)
				if (((ps[i]->sample[wt].runtime - ps[i]->sample[ps[i]->first].runtime) /
				     (ps[i]->sample[ps[i]->last].runtime - ps[i]->sample[ps[i]->first].runtime))
				    >= 0.95)
					goto labelpos;
			/* auto-frob at end */
		}

labelpos:
		/* text label of process name */
		svg("  <text x=\"%.03f\" y=\"%i\">%s [%i] <tspan class=\"run\">%.03fs</tspan></text>\n",
		    time_to_graph(sampletime[wt] - graph_start) + 5,
		    ps_to_graph(j) + 14,
		    ps[i]->name,
		    ps[i]->pid,
		    (ps[i]->sample[ps[i]->last].runtime - ps[i]->sample[ps[i]->first].runtime) / 1000000000.0);

		/* paint lines to the parent process */
		if (ps[ps[i]->ppid]) {
			/* horizontal part */
			svg("  <line class=\"dot\" x1=\"%.03f\" y1=\"%i\" x2=\"%.03f\" y2=\"%i\" />\n",
			    time_to_graph(starttime - graph_start),
			    ps_to_graph(j) + 10,
			    ps[ps[i]->ppid]->pos_x,
			    ps_to_graph(j) + 10);

			/* one vertical line connecting all the horizontal ones up */
			if (!ps[i]->next)
				svg("  <line class=\"dot\" x1=\"%.03f\" y1=\"%i\" x2=\"%.03f\" y2=\"%.03f\" />\n",
				    ps[ps[i]->ppid]->pos_x,
				    ps_to_graph(j) + 10,
				    ps[ps[i]->ppid]->pos_x,
				    ps[ps[i]->ppid]->pos_y);
		}

		j++; /* count boxes */

		svg("\n");
	}

	/* last pass - determine when idle */
	pid = getpid();
	for (i = 0; i < samples - (hz / 2); i++) {
		double crt;
		double brt;
		int c;

		/* subtract bootchart cpu utilization from total */
		crt = 0.0;
		for (c = 0; c < cpus; c++)
			crt += cpustat[c].sample[i + (hz / 2)].runtime - cpustat[c].sample[i].runtime;
		brt = ps[pid]->sample[i + (hz / 2)].runtime - ps[pid]->sample[i].runtime;

		/*
		 * our definition of "idle":
		 *
		 * if for (hz / 2) we've used less CPU than (interval / 2) ...
		 * defaults to 4.0%, which experimentally, is where moblin is idling
		 * on atom
		 */
		if ((crt - brt) < (interval / 2)) {
			idletime = sampletime[i] - graph_start;
			svg("\n<!-- idle detected at %.03f seconds -->\n",
			    idletime);
			svg("<line class=\"idle\" x1=\"%.03f\" y1=\"%i\" x2=\"%.03f\" y2=\"%i\" />\n",
			    time_to_graph(idletime),
			    -20,
			    time_to_graph(idletime),
			    ps_to_graph(pc) + 20);
			svg("<text class=\"idle\" x=\"%.03f\" y=\"%i\">%.01fs</text>\n",
			    time_to_graph(idletime) + 5,
			    ps_to_graph(pc) + 20,
			    idletime);
			break;
		}
	}
}


void svg_do(void)
{
	memset(&str, 0, sizeof(str));

	svg_header();

	svg("<g transform=\"translate(10,200)\">\n");
	svg_io_bi_bar();
	svg("</g>\n\n");

	svg("<g transform=\"translate(10,350)\">\n");
	svg_io_bo_bar();
	svg("</g>\n\n");

	svg("<g transform=\"translate(10,500)\">\n");
	svg_cpu_bar();
	svg("</g>\n\n");

	svg("<g transform=\"translate(10,650)\">\n");
	svg_wait_bar();
	svg("</g>\n\n");

	svg("<g transform=\"translate(10,800)\">\n");
	svg_ps_bars();
	svg("</g>\n\n");

	svg("<g transform=\"translate(10,  0)\">\n");
	svg_title();
	svg("</g>\n\n");

	/* svg footer */
	svg("\n</svg>\n");
}

