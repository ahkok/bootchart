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
	svg("\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n\n");

	svg("<svg width=\"100%%\" height=\"100%%\" version=\"1.1\" ");
	svg("xmlns=\"http://www.w3.org/2000/svg\">\n\n");
}


void svg_cpu_bar(void)
{
	int i;

	/* surrounding box */
	svg("\n\n<!-- CPU utilization graph -->\n");
	svg("<rect x=\"%.03f\" y=\"0\" width=\"%.03f\" height=\"%i\" ",
	    time_to_graph(0),
	    time_to_graph(sampletime[samples-1] - graph_start), ps_to_graph(5));
	svg("style=\"fill:rgb(240,240,240);stroke-width:1;");
	svg("stroke:rgb(192,192,192)\" />\n\n");

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

		svg("<rect x=\"%.03f\" y=\"%.03f\" width=\"%.03f\" height=\"%.03f\" ",
		    time_to_graph(sampletime[i - 1] - graph_start),
		    100.0 - (ptrt * 100.0),
		    time_to_graph(sampletime[i] - sampletime[i - 1]),
		    ptrt * 100.0);
		svg("style=\"fill:rgb(64,240,64);stroke-width:0;\" />\n");
	}
}

void svg_wait_bar(void)
{
	int i;

	/* surrounding box */
	svg("\n\n<!-- Wait time aggregation box -->\n");
	svg("<rect x=\"%.03f\" y=\"0\" width=\"%.03f\" height=\"%i\" ",
	    time_to_graph(0),
	    time_to_graph(sampletime[samples-1] - graph_start), ps_to_graph(5));
	svg("style=\"fill:rgb(240,240,240);stroke-width:1;");
	svg("stroke:rgb(192,192,192)\" />\n\n");

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

		svg("<rect x=\"%.03f\" y=\"%.03f\" width=\"%.03f\" height=\"%.03f\" ",
		    time_to_graph(sampletime[i - 1] - graph_start),
		    (100.0 - (ptwt * 100.0)),
		    time_to_graph(sampletime[i] - sampletime[i - 1]),
		    ptwt * 100.0);
		svg("style=\"fill:rgb(64,64,240);stroke-width:0;\" />\n");
	}
}


void svg_ps_bars(void)
{
	int i;
	int j = 0;
	double d;

	/* surrounding box */
	svg("\n\n<!-- Process graph -->\n");
	svg("<rect x=\"%.03f\" y=\"0\" width=\"%.03f\" height=\"%i\" ",
	    time_to_graph(0),
	    time_to_graph(sampletime[samples-1] - graph_start), ps_to_graph(pscount));
	svg("style=\"fill:rgb(240,240,240);stroke-width:1;");
	svg("stroke:rgb(192,192,192)\" />\n");

	/* ps boxes */
	for (i = 0; i < MAXPIDS; i++) {
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

		svg("  <rect x=\"%.03f\" y=\"%i\" width=\"%.03f\" height=\"%i\" ",
		    time_to_graph(sampletime[ps[i]->first] - graph_start), ps_to_graph(j),
		    time_to_graph(sampletime[ps[i]->last] - sampletime[ps[i]->first]), ps_to_graph(1));
		svg("style=\"fill:rgb(192,192,192);stroke-width:1;");
		svg("stroke:rgb(128,128,128);fill-opacity:0.5\" />\n");

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

			svg("<rect x=\"%.03f\" y=\"%.03f\" width=\"%.03f\" height=\"%.03f\" ",
			    time_to_graph(sampletime[t - 1] - graph_start),
			    ps_to_graph(j + (1.0 - prt)),
			    time_to_graph(sampletime[t] - sampletime[t - 1]),
			    ps_to_graph(prt));
			svg("style=\"fill:rgb(64,240,64);stroke-width:0;\n");
			svg("fill-opacity:%.03f\" />\n", prt);

			svg("<rect x=\"%.03f\" y=\"%i\" width=\"%.03f\" height=\"%.03f\" ",
			    time_to_graph(sampletime[t - 1] - graph_start),
			    ps_to_graph(j),
			    time_to_graph(sampletime[t] - sampletime[t - 1]),
			    ps_to_graph(wrt));
			svg("style=\"fill:rgb(64,64,240);stroke-width:0;\n");
			svg("fill-opacity:%.03f\" />\n", wrt);
		}
		svg("\n");

		/* text label of process name */
		svg("  <text x=\"%.03f\" y=\"%i\" font-family=\"Verdana\" font-size=\"10\">\n",
		    time_to_graph(sampletime[ps[i]->first] - graph_start) + 5, ps_to_graph(j) + 15);
		svg("%s [%i]\n", ps[i]->name, ps[i]->pid);
		svg("  </text>\n");


		j++; /* count boxes */

	}

	/* bounding box, ticks */
	for (d = sampletime[0]; d <= sampletime[samples-1]; d++) {
		/* lines for each second */
		svg("<line x1=\"%.03f\" y1=\"0\" x2=\"%.03f\" y2=\"%i\" ",
		    time_to_graph(d - graph_start), time_to_graph(d - graph_start),
		    ps_to_graph(pscount));
		svg("style=\"stroke-width:%i;", ((long)(d - graph_start)) % 5 ? 1 : 2);
		svg("stroke:rgb(64,64,64)\" />\n");

		/* time label */
		svg("  <text x=\"%.03f\" y=\"%i\" font-family=\"Verdana\" font-size=\"10\">",
		    time_to_graph(d - graph_start), -5);
		svg("%.0f</text>\n", (d - graph_start));
	}

}


void svg_do(void)
{
	svg_header();

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

