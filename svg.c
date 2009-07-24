/*
 * svg.c
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


void svg_ps_bars(void)
{
	int i;
	int j = 0;

	/* surrounding box */
	svg("\n\n<!-- bounding box -->\n");
	svg("<rect x=\"%.03f\" y=\"0\" width=\"%.03f\" height=\"%i\" ",
	    time_to_graph(0),
	    time_to_graph(sampletime[samples-1] - graph_start), ps_to_graph(pscount));
	svg("style=\"fill:rgb(240,240,240);stroke-width:1;");
	svg("stroke:rgb(192,192,192)\" />\n");

	/* lines for each second */
	svg("\n\n<!-- tick marks per second -->\n");
	for (i = graph_start; i <= sampletime[samples-1]; i++) {
		svg("<line x1=\"%.03f\" y1=\"0\" x2=\"%.03f\" y2=\"%i\" ",
		    time_to_graph(i - graph_start), time_to_graph(i - graph_start), ps_to_graph(pscount));
		svg("style=\"stroke-width:%i;", i % 5 ? 1 : 2);
		svg("stroke:rgb(64,64,64)\" />\n");
	}

	/* ps boxes */
	svg("\n\n<!-- ps boxes -->\n");
	for (i = 0; i < MAXPIDS; i++) {
		int t;

		if (!ps[i])
			continue;
		svg("  <rect x=\"%.03f\" y=\"%i\" width=\"%.03f\" height=\"%i\" ",
		    time_to_graph(sampletime[ps[i]->first] - graph_start), ps_to_graph(j),
		    time_to_graph(sampletime[ps[i]->last] - sampletime[ps[i]->first]), ps_to_graph(1));
		svg("style=\"fill:rgb(192,192,192);stroke-width:1;");
		svg("stroke:rgb(128,128,128);fill-opacity:0.5\" />\n");

		/* text label of process name */
		svg("  <text x=\"%.03f\" y=\"%i\" font-family=\"Verdana\" font-size=\"10\">\n",
		    time_to_graph(sampletime[ps[i]->first] - graph_start) + 5, ps_to_graph(j) + 15);
		svg("%s [%i]\n", ps[i]->name, ps[i]->pid);
		svg("  </text>\n");

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

			/* draw a color box */
			svg("    <rect x=\"%.03f\" y=\"%i\" width=\"%.03f\" height=\"%i\" ",
			    time_to_graph(sampletime[t-1] - graph_start), ps_to_graph(j),
			    time_to_graph(sampletime[t] - sampletime[t-1]), ps_to_graph(1));
			svg("style=\"fill:rgb(%.0f,0,%.0f);stroke-width:0;",
			    to_color(wrt), to_color(prt));
			svg("fill-opacity:0.5\" />\n");
		}
		svg("\n");


		j++; /* count boxes */

	}

	/* svg footer */
	svg("\n</svg>\n");
}


void svg_do(void)
{
	svg_header();

	svg_ps_bars();
}

