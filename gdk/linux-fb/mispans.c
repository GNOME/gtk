/* $XFree86: xc/programs/Xserver/mi/mispans.c,v 3.1 1998/10/04 09:39:33 dawes Exp $ */
/***********************************************************

Copyright 1989, 1998  The Open Group

All Rights Reserved.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1989 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/* $TOG: mispans.c /main/7 1998/02/09 14:48:44 kaleb $ */

#include <config.h>
#include "mi.h"
#include "mispans.h"
#include <string.h>		/* for memmove */

/*

These routines maintain lists of Spans, in order to implement the
``touch-each-pixel-once'' rules of wide lines and arcs.

Written by Joel McCormack, Summer 1989.

*/


void miInitSpanGroup(SpanGroup *spanGroup)
{
    spanGroup->size = 0;
    spanGroup->count = 0;
    spanGroup->group = NULL;
    spanGroup->ymin = SHRT_MAX;
    spanGroup->ymax = SHRT_MIN;
} /* InitSpanGroup */

#define YMIN(spans) (spans->points[0].y)
#define YMAX(spans)  (spans->points[spans->count-1].y)

void miSubtractSpans (SpanGroup *spanGroup, Spans *sub)
{
    int		i, subCount, spansCount;
    int		ymin, ymax, xmin, xmax;
    Spans	*spans;
    GdkSpan*	subPt, *spansPt;
    int		extra;

    ymin = YMIN(sub);
    ymax = YMAX(sub);
    spans = spanGroup->group;
    for (i = spanGroup->count; i; i--, spans++) {
	if (YMIN(spans) <= ymax && ymin <= YMAX(spans)) {
	    subCount = sub->count;
	    subPt = sub->points;
	    spansCount = spans->count;
	    spansPt = spans->points;
	    extra = 0;
	    for (;;)
 	    {
		while (spansCount && spansPt->y < subPt->y)
		{
		    spansPt++; spansCount--;
		}
		if (!spansCount)
		    break;
		while (subCount && subPt->y < spansPt->y)
		{
		    subPt++; subCount--;
		}
		if (!subCount)
		    break;
		if (subPt->y == spansPt->y)
		{
		    xmin = subPt->x;
		    xmax = xmin + subPt->width;
		    if (xmin >= (spansPt->x + spansPt->width) || spansPt->x >= xmax)
		    {
			;
		    }
		    else if (xmin <= spansPt->x)
		    {
			if (xmax >= (spansPt->x + spansPt->width))
			{
			    g_memmove (spansPt, spansPt + 1, sizeof *spansPt * (spansCount - 1));
			    spansPt--;
			    spans->count--;
			    extra++;
			}
			else 
			{
			  spansPt->width -= xmax - spansPt->x;
			  spansPt->x = xmax;
			}
		    }
		    else
		    {
			if (xmax >= (spansPt->x + spansPt->width))
			{
			  spansPt->width = xmin - spansPt->x;
			}
			else
			{
			    if (!extra) {
				GdkSpan* newPt;

#define EXTRA 8
				newPt = (GdkSpan*) g_realloc (spans->points, (spans->count + EXTRA) * sizeof (GdkSpan));
				if (!newPt)
				    break;
				spansPt = newPt + (spansPt - spans->points);
				spans->points = newPt;
				extra = EXTRA;
			    }
			    g_memmove (spansPt + 1, spansPt, sizeof *spansPt * (spansCount));
			    spans->count++;
			    extra--;
			    spansPt->width = xmin - spansPt->x;
			    spansPt++;
			    spansPt->width -= xmax - spansPt->x;
			    spansPt->x = xmax;
			}
		    }
		}
		spansPt++; spansCount--;
	    }
	}
    }
}
    
void miAppendSpans(SpanGroup *spanGroup, SpanGroup *otherGroup, Spans *spans)
{
    register    int ymin, ymax;
    register    int spansCount;

    spansCount = spans->count;
    if (spansCount > 0) {
	if (spanGroup->size == spanGroup->count) {
	    spanGroup->size = (spanGroup->size + 8) * 2;
	    spanGroup->group = (Spans *)
		g_realloc(spanGroup->group, sizeof(Spans) * spanGroup->size);
	 }

	spanGroup->group[spanGroup->count] = *spans;
	(spanGroup->count)++;
	ymin = spans->points[0].y;
	if (ymin < spanGroup->ymin) spanGroup->ymin = ymin;
	ymax = spans->points[spansCount - 1].y;
	if (ymax > spanGroup->ymax) spanGroup->ymax = ymax;
	if (otherGroup &&
	    otherGroup->ymin < ymax &&
	    ymin < otherGroup->ymax)
	{
	    miSubtractSpans (otherGroup, spans);
	}
    }
    else
    {
	g_free (spans->points);
    }
} /* AppendSpans */

void miFreeSpanGroup(SpanGroup *spanGroup)
{
    if (spanGroup->group != NULL) g_free(spanGroup->group);
}

static void QuickSortSpansX(register GdkSpan points[], register int numSpans)
{
    register int	    x;
    register int	    i, j, m;
    register GdkSpan*    r;

/* Always called with numSpans > 1 */
/* Sorts only by x, as all y should be the same */

#define ExchangeSpans(a, b)				    \
{							    \
    GdkSpan     tpt;				    \
							    \
    tpt = points[a]; points[a] = points[b]; points[b] = tpt;    \
}

    do {
	if (numSpans < 9) {
	    /* Do insertion sort */
	    register int xprev;

	    xprev = points[0].x;
	    i = 1;
	    do { /* while i != numSpans */
		x = points[i].x;
		if (xprev > x) {
		    /* points[i] is out of order.  Move into proper location. */
		    GdkSpan tpt;
		    int	    k;

		    for (j = 0; x >= points[j].x; j++) {}
		    tpt = points[i];
		    for (k = i; k != j; k--) {
			points[k] = points[k-1];
		    }
		    points[j] = tpt;
		    x = points[i].x;
		} /* if out of order */
		xprev = x;
		i++;
	    } while (i != numSpans);
	    return;
	}

	/* Choose partition element, stick in location 0 */
	m = numSpans / 2;
	if (points[m].x > points[0].x)		ExchangeSpans(m, 0);
	if (points[m].x > points[numSpans-1].x) ExchangeSpans(m, numSpans-1);
	if (points[m].x > points[0].x)		ExchangeSpans(m, 0);
	x = points[0].x;

        /* Partition array */
        i = 0;
        j = numSpans;
        do {
	    r = &(points[i]);
	    do {
		r++;
		i++;
            } while (i != numSpans && r->x < x);
	    r = &(points[j]);
	    do {
		r--;
		j--;
            } while (x < r->x);
            if (i < j) ExchangeSpans(i, j);
        } while (i < j);

        /* Move partition element back to middle */
        ExchangeSpans(0, j);

	/* Recurse */
        if (numSpans-j-1 > 1)
	    QuickSortSpansX(&points[j+1], numSpans-j-1);
        numSpans = j;
    } while (numSpans > 1);
} /* QuickSortSpans */


static int UniquifySpansX(Spans *spans, register GdkSpan *newPoints)
{
    register int newx1, newx2, oldpt, i, y;
    GdkSpan    *oldPoints, *startNewPoints = newPoints;

/* Always called with numSpans > 1 */
/* Uniquify the spans, and stash them into newPoints and newWidths.  Return the
   number of unique spans. */


    oldPoints = spans->points;

    y = oldPoints->y;
    newx1 = oldPoints->x;
    newx2 = newx1 + oldPoints->width;

    for (i = spans->count-1; i != 0; i--) {
	oldPoints++;
	oldpt = oldPoints->x;
	if (oldpt > newx2) {
	    /* Write current span, start a new one */
	    newPoints->x = newx1;
	    newPoints->y = y;
	    newPoints->width = newx2 - newx1;
	    newPoints++;
	    newx1 = oldpt;
	    newx2 = oldpt + oldPoints->width;
	} else {
	    /* extend current span, if old extends beyond new */
	    oldpt = oldpt + oldPoints->width;
	    if (oldpt > newx2) newx2 = oldpt;
	}
    } /* for */

    /* Write final span */
    newPoints->x = newx1;
    newPoints->width = newx2 - newx1;
    newPoints->y = y;

    return (newPoints - startNewPoints) + 1;
} /* UniquifySpansX */

void
miDisposeSpanGroup (SpanGroup *spanGroup)
{
    int	    i;
    Spans   *spans;

    for (i = 0; i < spanGroup->count; i++)
    {
	spans = spanGroup->group + i;
	g_free (spans->points);
    }
}

void miFillUniqueSpanGroup(GdkDrawable *pDraw, GdkGC *pGC, SpanGroup *spanGroup)
{
    register int    i;
    register Spans  *spans;
    register Spans  *yspans;
    register int    *ysizes;
    register int    ymin, ylength;

    /* Outgoing spans for one big call to FillSpans */
    register GdkSpan*    points;
    register int	    count;

    if (spanGroup->count == 0) return;

    if (spanGroup->count == 1) {
	/* Already should be sorted, unique */
	spans = spanGroup->group;
	gdk_fb_fill_spans(pDraw, pGC, spans->points, spans->count, TRUE);
	g_free(spans->points);
    }
    else
    {
	/* Yuck.  Gross.  Radix sort into y buckets, then sort x and uniquify */
	/* This seems to be the fastest thing to do.  I've tried sorting on
	   both x and y at the same time rather than creating into all those
	   y buckets, but it was somewhat slower. */

	ymin    = spanGroup->ymin;
	ylength = spanGroup->ymax - ymin + 1;

	/* Allocate Spans for y buckets */
	yspans = (Spans *) g_malloc(ylength * sizeof(Spans));
	ysizes = (int *) g_malloc(ylength * sizeof (int));

	if (!yspans || !ysizes)
	{
	    if (yspans)
		g_free (yspans);
	    if (ysizes)
		g_free (ysizes);
	    miDisposeSpanGroup (spanGroup);
	    return;
	}
	
	for (i = 0; i != ylength; i++) {
	    ysizes[i]        = 0;
	    yspans[i].count  = 0;
	    yspans[i].points = NULL;
	}

	/* Go through every single span and put it into the correct bucket */
	count = 0;
	for (i = 0, spans = spanGroup->group;
		i != spanGroup->count;
		i++, spans++) {
	    int		index;
	    int		j;

	    for (j = 0, points = spans->points;
		 j != spans->count;
		 j++, points++) {
		index = points->y - ymin;
		if (index >= 0 && index < ylength) {
		    Spans *newspans = &(yspans[index]);
		    if (newspans->count == ysizes[index]) {
			GdkSpan* newpoints;
			ysizes[index] = (ysizes[index] + 8) * 2;
			newpoints = (GdkSpan*) g_realloc(
			    newspans->points,
			    ysizes[index] * sizeof(GdkSpan));
			if (!newpoints)
			{
			    int	i;

			    for (i = 0; i < ylength; i++)
			    {
				g_free (yspans[i].points);
			    }
			    g_free (yspans);
			    g_free (ysizes);
			    miDisposeSpanGroup (spanGroup);
			    return;
			}
			newspans->points = newpoints;
		    }
		    newspans->points[newspans->count] = *points;
		    (newspans->count)++;
		} /* if y value of span in range */
	    } /* for j through spans */
	    count += spans->count;
	    g_free(spans->points);
	    spans->points = NULL;
	} /* for i thorough Spans */

	/* Now sort by x and uniquify each bucket into the final array */
	points = (GdkSpan*) g_malloc(count * sizeof(GdkSpan));
	if (!points)
	{
	    int	i;

	    for (i = 0; i < ylength; i++)
	    {
		g_free (yspans[i].points);
	    }
	    g_free (yspans);
	    g_free (ysizes);
	    if (points)
		g_free (points);
	    return;
	}
	count = 0;
	for (i = 0; i != ylength; i++) {
	    int ycount = yspans[i].count;
	    if (ycount > 0) {
		if (ycount > 1) {
		    QuickSortSpansX(yspans[i].points, ycount);
		    count += UniquifySpansX
			(&(yspans[i]), &(points[count]));
		} else {
		    points[count] = yspans[i].points[0];
		    count++;
		}
		g_free(yspans[i].points);
	    }
	}

	gdk_fb_fill_spans(pDraw, pGC, points, count, TRUE);
	g_free(points);
	g_free(yspans);
	g_free(ysizes);		/* use (DE)ALLOCATE_LOCAL for these? */
    }

    spanGroup->count = 0;
    spanGroup->ymin = SHRT_MAX;
    spanGroup->ymax = SHRT_MIN;
}


void miFillSpanGroup(GdkDrawable *pDraw, GdkGC *pGC, SpanGroup *spanGroup)
{
    register int    i;
    register Spans  *spans;

    for (i = 0, spans = spanGroup->group; i != spanGroup->count; i++, spans++) {
      gdk_fb_fill_spans(pDraw, pGC, spans->points, spans->count, TRUE);
      g_free(spans->points);
    }

    spanGroup->count = 0;
    spanGroup->ymin = SHRT_MAX;
    spanGroup->ymax = SHRT_MIN;
} /* FillSpanGroup */
