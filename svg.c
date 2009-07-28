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

#include "bootchart.h"


#define SCALE_X 100.0  /* 100px per second */
#define SCALE_Y 20   /* 1 process bar is 16px high */

#define time_to_graph(t) ((t) * SCALE_X)
#define ps_to_graph(n) ((n) * SCALE_Y)
#define to_color(n) (192.0 - ((n) * 192.0))

#define svg(a...) do { char str[8092]; sprintf(str, ## a); fputs(str, of); } while (0)


void svg_header(void)
{
	svg("<?xml version=\"1.0\" standalone=\"no\"?>\n");
	svg("<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" ");
	svg("\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");

	svg("<svg width=\"100%%\" height=\"100%%\" version=\"1.1\" ");
	svg("xmlns=\"http://www.w3.org/2000/svg\">\n\n");

	/* style sheet */
	svg("<defs>\n  <style type=\"text/css\">\n    <![CDATA[\n");

	svg("      rect.cpu  { fill: rgb(64,64,240); stroke-width: 0; }\n");
	svg("      rect.wait { fill: rgb(240,240,0); stroke-width: 0; }\n");
	svg("      rect.bi   { fill: rgb(240,128,128); stroke-width: 0; }\n");
	svg("      rect.bo   { fill: rgb(192,64,64); stroke-width: 0; }\n");
	svg("      rect.ps   { fill: rgb(192,192,192); stroke-width: 1; stroke: rgb(128,128,128); fill-opacity: 0.5; }\n");
	svg("      rect.box  { fill: rgb(240,240,240); stroke-width: 1; stroke: rgb(192,192,192); }\n");

	svg("      line.box1 { stroke: rgb(64,64,64); stroke-width: 1; }\n");
	svg("      line.box2 { stroke: rgb(64,64,64); stroke-width: 2; }\n");

	svg("      text.sec  { font-family: Verdana; font-size: 10; }\n");
	svg("      text.psnm { font-family: Verdana; font-size: 10; }\n");
	
	svg("    ]]>\n   </style>\n</defs>\n\n");

}


void svg_graph_box(int height)
{
	double d;
	int i = 0;

	/* outside box, fill */
	svg("<rect class=\"box\" x=\"%.03f\" y=\"0\" width=\"%.03f\" height=\"%i\" />\n",
	    time_to_graph(0),
	    time_to_graph(sampletime[samples-1] - graph_start),
	    ps_to_graph(height));

	for (d = sampletime[0]; d <= sampletime[samples-1]; d += 1.0) {
		/* lines for each second */
		if (i % 5)
			svg("<line class=\"box1\" x1=\"%.03f\" y1=\"0\" x2=\"%.03f\" y2=\"%i\" />\n",
			    time_to_graph(d - graph_start),
			    time_to_graph(d - graph_start),
			    ps_to_graph(height));
		else
			svg("<line class=\"box2\" x1=\"%.03f\" y1=\"0\" x2=\"%.03f\" y2=\"%i\" />\n",
			    time_to_graph(d - graph_start),
			    time_to_graph(d - graph_start),
			    ps_to_graph(height));

		/* time label */
		svg("  <text class=\"sec\" x=\"%.03f\" y=\"%i\" >%.01fs</text>\n",
		    time_to_graph(d - graph_start),
		    -5,
		    d - graph_start);

		i++;
	}
}


void svg_io_bar(void)
{
	double max;
	double max_bi = 0.0;
	double max_bo = 0.0;
	int max_bi_here = 0;
	int max_bo_here = 0;
	int i;

	svg("<!-- IO utilization graph -->\n");

	/* surrounding box */
	svg_graph_box(5);

	/* find the max IO first */
	for (i = 1; i < samples; i++) {
		if ((blockstat[i].bi - blockstat[i - 1].bi)  > max_bi) {
			max_bi = blockstat[i].bi - blockstat[i - 1].bi;
			max_bi_here = i;
		}
		if ((blockstat[i].bo - blockstat[i - 1].bo) > max_bo) {
			max_bo = blockstat[i].bo - blockstat[i - 1].bo;
			max_bo_here = i;
		}
	}


	if (max_bi > max_bo)
		max = max_bi;
	else
		max = max_bo;

	/* plot bi/bo */
	for (i = 1; i < samples; i++) {
		double pbi;
		double pbo;

		pbi = (blockstat[i].bi - blockstat[i - 1].bi) / max;

		if (pbi > 0.001)
			svg("<rect class=\"bi\" x=\"%.03f\" y=\"%.03f\" width=\"%.03f\" height=\"%.03f\" />\n",
			    time_to_graph(sampletime[i - 1] - graph_start),
			    100.0 - (pbi * 100.0),
			    time_to_graph(sampletime[i] - sampletime[i - 1]),
			    pbi * 100.0);

		/* draw bo OVER bi (assume bo < bi) */
		pbo = (blockstat[i].bo - blockstat[i - 1].bo) / max;

		if (pbo > 0.001)
			svg("<rect class=\"bo\" x=\"%.03f\" y=\"%.03f\" width=\"%.03f\" height=\"%.03f\" />\n",
			    time_to_graph(sampletime[i - 1] - graph_start),
			    100.0 - (pbo * 100.0),
			    time_to_graph(sampletime[i] - sampletime[i - 1]),
			    pbo * 100.0);

		/* labels around highest bi/bo values */
		if (i == max_bi_here) {
			svg("  <text class=\"sec\" x=\"%.03f\" y=\"%.03f\">%0.2fmb/sec</text>\n",
			    time_to_graph(sampletime[i] - graph_start) + 5,
			    100.0 - (pbi * 100.0),
			    max_bi / 1024.0 / (interval / 1000000000.0));
		}

		if (i == max_bo_here) {
			svg("  <text class=\"sec\" x=\"%.03f\" y=\"%.03f\">%0.2fmb/sec</text>\n",
			    time_to_graph(sampletime[i] - graph_start) + 5,
			    100.0 - (pbo * 100.0),
			    max_bo / 1024.0 / (interval / 1000000000.0));
		}
	}
}


void svg_cpu_bar(void)
{
	int i;

	svg("<!-- CPU utilization graph -->\n");

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


void svg_ps_bars(void)
{
	int i;
	int j = 0;

	svg("<!-- Process graph -->\n");

	/* surrounding box */
	svg_graph_box(pscount);

	/* ps boxes */
	for (i = 0; i < MAXPIDS; i++) {
		double starttime;
		int t;

		if (!ps[i])
			continue;

		/* filters */
		if (filter) {
			if (ps[i]->first == ps[i]->last)
				continue;

			/*
			 * TODO
			 *
			 * filter out:
			 * - inactive kernel threads
			 */
		}

		starttime = sampletime[ps[i]->first];

		if (starttime < graph_start)
			starttime = graph_start;

		svg("  <rect class=\"ps\" x=\"%.03f\" y=\"%i\" width=\"%.03f\" height=\"%i\" />\n",
		    time_to_graph(starttime - graph_start), ps_to_graph(j),
		    time_to_graph(sampletime[ps[i]->last] - starttime), ps_to_graph(1));

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

			svg("    <rect class=\"cpu\" x=\"%.03f\" y=\"%.03f\" width=\"%.03f\" height=\"%.03f\" />\n",
			    time_to_graph(sampletime[t - 1] - graph_start),
			    ps_to_graph(j + (1.0 - prt)),
			    time_to_graph(sampletime[t] - sampletime[t - 1]),
			    ps_to_graph(prt));

			svg("    <rect class=\"wait\" x=\"%.03f\" y=\"%i\" width=\"%.03f\" height=\"%.03f\" />\n",
			    time_to_graph(sampletime[t - 1] - graph_start),
			    ps_to_graph(j),
			    time_to_graph(sampletime[t] - sampletime[t - 1]),
			    ps_to_graph(wrt));
		}

		/* text label of process name */
		svg("  <text class=\"psnm\" x=\"%.03f\" y=\"%i\">%s [%i]</text>\n",
		    time_to_graph(sampletime[ps[i]->first] - graph_start) + 5,
		    ps_to_graph(j) + 15,
		    ps[i]->name,
		    ps[i]->pid);

		j++; /* count boxes */

		svg("\n");
	}
}


void svg_do(void)
{
	svg_header();

	svg("<g transform=\"translate(0,0)\">\n");
	svg_io_bar();
	svg("</g>\n\n");

	svg("<g transform=\"translate(0,200)\">\n");
	svg_cpu_bar();
	svg("</g>\n\n");

	svg("<g transform=\"translate(0,350)\">\n");
	svg_wait_bar();
	svg("</g>\n\n");

	svg("<g transform=\"translate(0,500)\">\n");
	svg_ps_bars();
	svg("</g>\n\n");

	/* svg footer */
	svg("\n</svg>\n");
}

