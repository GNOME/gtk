/* $TOG: miwideline.c /main/60 1998/03/07 17:40:23 kaleb $ */
/*

Copyright 1988, 1998  The Open Group

All Rights Reserved.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/Xserver/mi/miwideline.c,v 1.7 1999/10/13 22:33:13 dawes Exp $ */

/* Author:  Keith Packard, MIT X Consortium */

/*
 * Mostly integer wideline code.  Uses a technique similar to
 * bresenham zero-width lines, except walks an X edge
 */

#include <config.h>
#include <stdio.h>
#ifdef _XOPEN_SOURCE
#include <math.h>
#else
#define _XOPEN_SOURCE	/* to get prototype for hypot on some systems */
#include <math.h>
#undef _XOPEN_SOURCE
#endif

#include "mi.h"
#include "miwideline.h"

#ifdef ICEILTEMPDECL
ICEILTEMPDECL
#endif

static void
miLineArc (GdkDrawable *pDraw, GdkGC *pGC, GdkColor *pixel, SpanDataPtr spanData,
	LineFacePtr leftFace, LineFacePtr rightFace, double xorg, double yorg, gboolean isInt);

/*
 * spans-based polygon filler
 */

void
miFillPolyHelper (GdkDrawable *pDrawable, GdkGC *pGC, GdkColor *pixel,
                  SpanDataPtr spanData, int y, int overall_height,
		  PolyEdgePtr left, PolyEdgePtr right, int left_count,
                  int right_count)
{
    register int left_x = 0, left_e = 0;
    int	left_stepx = 0;
    int	left_signdx = 0;
    int	left_dy = 0, left_dx = 0;

    register int right_x = 0, right_e = 0;
    int	right_stepx = 0;
    int	right_signdx = 0;
    int	right_dy = 0, right_dx = 0;

    int	height = 0;
    int	left_height = 0, right_height = 0;

    register GdkSpan* ppt;
    GdkSpan* pptInit = NULL;
    GdkColor		oldPixel;
    int		xorg;
    Spans	spanRec;

    left_height = 0;
    right_height = 0;
    
    if (!spanData)
    {
    	pptInit = (GdkSpan*) ALLOCATE_LOCAL (overall_height * sizeof(*ppt));
    	if (!pptInit)
	    return;
	ppt = pptInit;
    	oldPixel = GDK_GC_FBDATA(pGC)->values.foreground;
    	if (pixel->pixel != oldPixel.pixel)
    	{
	  gdk_gc_set_foreground(pGC, pixel);
    	}
    }
    else
    {
	spanRec.points = (GdkSpan*) g_malloc (overall_height * sizeof (*ppt));
	if (!spanRec.points)
	    return;
	ppt = spanRec.points;
    }

    xorg = 0;
    while ((left_count || left_height) &&
	   (right_count || right_height))
    {
	MIPOLYRELOADLEFT
	MIPOLYRELOADRIGHT

	height = left_height;
	if (height > right_height)
	    height = right_height;

	left_height -= height;
	right_height -= height;

	while (--height >= 0)
	{
	    if (right_x >= left_x)
	    {
		ppt->y = y;
		ppt->x = left_x + xorg;
		ppt->width = right_x - left_x + 1;
		ppt++;
	    }
    	    y++;
    	
	    MIPOLYSTEPLEFT

	    MIPOLYSTEPRIGHT
	}
    }
    if (!spanData)
    {
      gdk_fb_fill_spans(pDrawable, pGC, pptInit, ppt - pptInit, TRUE);
      DEALLOCATE_LOCAL (pptInit);
      if (pixel->pixel != oldPixel.pixel)
    	{
	  gdk_gc_set_foreground(pGC, &oldPixel);
    	}
    }
    else
    {
	spanRec.count = ppt - spanRec.points;
	AppendSpanGroup (pGC, pixel, &spanRec, spanData)
    }
}

static void
miFillRectPolyHelper (GdkDrawable *pDrawable, GdkGC *pGC, GdkColor *pixel,
                      SpanDataPtr spanData, int x, int y, int w, int h)
{
    register GdkSpan* ppt;
    GdkColor	oldPixel;
    Spans	spanRec;

    if (!spanData)
    {
    	oldPixel = GDK_GC_FBDATA(pGC)->values.foreground;
    	if (pixel->pixel != oldPixel.pixel)
    	{
	  gdk_gc_set_foreground(pGC, pixel);
    	}
        gdk_fb_draw_rectangle(pDrawable, pGC, TRUE, x, y, w, h);
    	if (pixel->pixel != oldPixel.pixel)
    	{
	  gdk_gc_set_foreground(pGC, &oldPixel);
    	}
    }
    else
    {
	spanRec.points = (GdkSpan*) g_malloc (h * sizeof (*ppt));
	if (!spanRec.points)
	    return;
	ppt = spanRec.points;

	while (h--)
	{
	    ppt->x = x;
	    ppt->y = y;
	    ppt->width = w;
	    ppt++;
	    y++;
	}
	spanRec.count = ppt - spanRec.points;
	AppendSpanGroup (pGC, pixel, &spanRec, spanData)
    }
}

/* static */ int
miPolyBuildEdge (double x0, double y0, double k, register int dx,
                 register int dy, int xi, int yi, int left,
                 register PolyEdgePtr edge)
{
    int	    x, y, e;
    int	    xady;

    if (dy < 0)
    {
	dy = -dy;
	dx = -dx;
	k = -k;
    }

#ifdef NOTDEF
    {
	double	realk, kerror;
    	realk = x0 * dy - y0 * dx;
    	kerror = Fabs (realk - k);
    	if (kerror > .1)
	    printf ("realk: %g k: %g\n", realk, k);
    }
#endif
    y = ICEIL (y0);
    xady = ICEIL (k) + y * dx;

    if (xady <= 0)
	x = - (-xady / dy) - 1;
    else
	x = (xady - 1) / dy;

    e = xady - x * dy;

    if (dx >= 0)
    {
	edge->signdx = 1;
	edge->stepx = dx / dy;
	edge->dx = dx % dy;
    }
    else
    {
	edge->signdx = -1;
	edge->stepx = - (-dx / dy);
	edge->dx = -dx % dy;
	e = dy - e + 1;
    }
    edge->dy = dy;
    edge->x = x + left + xi;
    edge->e = e - dy;	/* bias to compare against 0 instead of dy */
    return y + yi;
}

#define StepAround(v, incr, max) (((v) + (incr) < 0) ? (max - 1) : ((v) + (incr) == max) ? 0 : ((v) + (incr)))

/* static */ int
miPolyBuildPoly (register PolyVertexPtr vertices, register PolySlopePtr slopes,
                 int count, int xi, int yi, PolyEdgePtr left, PolyEdgePtr right,
                 int *pnleft, int *pnright, int *h)
{
    int	    top, bottom;
    double  miny, maxy;
    register int i;
    int	    j;
    int	    clockwise;
    int	    slopeoff;
    register int s;
    register int nright, nleft;
    int	    y, lasty = 0, bottomy, topy = 0;

    /* find the top of the polygon */
    maxy = miny = vertices[0].y;
    bottom = top = 0;
    for (i = 1; i < count; i++)
    {
	if (vertices[i].y < miny)
	{
	    top = i;
	    miny = vertices[i].y;
	}
	if (vertices[i].y >= maxy)
	{
	    bottom = i;
	    maxy = vertices[i].y;
	}
    }
    clockwise = 1;
    slopeoff = 0;

    i = top;
    j = StepAround (top, -1, count);

    if (slopes[j].dy * slopes[i].dx > slopes[i].dy * slopes[j].dx)
    {
	clockwise = -1;
	slopeoff = -1;
    }

    bottomy = ICEIL (maxy) + yi;

    nright = 0;

    s = StepAround (top, slopeoff, count);
    i = top;
    while (i != bottom)
    {
	if (slopes[s].dy != 0)
	{
	    y = miPolyBuildEdge (vertices[i].x, vertices[i].y,
			slopes[s].k,
			slopes[s].dx, slopes[s].dy,
			xi, yi, 0,
			&right[nright]);
	    if (nright != 0)
	    	right[nright-1].height = y - lasty;
	    else
	    	topy = y;
	    nright++;
	    lasty = y;
	}

	i = StepAround (i, clockwise, count);
	s = StepAround (s, clockwise, count);
    }
    if (nright != 0)
	right[nright-1].height = bottomy - lasty;

    if (slopeoff == 0)
	slopeoff = -1;
    else
	slopeoff = 0;

    nleft = 0;
    s = StepAround (top, slopeoff, count);
    i = top;
    while (i != bottom)
    {
	if (slopes[s].dy != 0)
	{
	    y = miPolyBuildEdge (vertices[i].x, vertices[i].y,
			   slopes[s].k,
		       	   slopes[s].dx,  slopes[s].dy, xi, yi, 1,
		       	   &left[nleft]);
    
	    if (nleft != 0)
	    	left[nleft-1].height = y - lasty;
	    nleft++;
	    lasty = y;
	}
	i = StepAround (i, -clockwise, count);
	s = StepAround (s, -clockwise, count);
    }
    if (nleft != 0)
	left[nleft-1].height = bottomy - lasty;
    *pnleft = nleft;
    *pnright = nright;
    *h = bottomy - topy;
    return topy;
}

static void
miLineOnePoint (GdkDrawable *pDrawable, GdkGC *pGC, GdkColor *pixel,
                SpanDataPtr spanData, int x, int y)
{
    GdkColor oldPixel;
    GdkSpan  span;

    MILINESETPIXEL (pDrawable, pGC, pixel, oldPixel);
    span.x = x;
    span.y = y;
    span.width = 1;

    gdk_fb_fill_spans(pDrawable, pGC, &span, 1, TRUE);
    MILINERESETPIXEL (pDrawable, pGC, pixel, oldPixel);
}

static void
miLineJoin (GdkDrawable *pDrawable, GdkGC *pGC, GdkColor *pixel,
            SpanDataPtr spanData, LineFacePtr pLeft, LineFacePtr pRight)
{
    double	    mx, my;
    double	    denom = 0.0;
    PolyVertexRec   vertices[4];
    PolySlopeRec    slopes[4];
    int		    edgecount;
    PolyEdgeRec	    left[4], right[4];
    int		    nleft, nright;
    int		    y, height;
    int		    swapslopes;
    int		    joinStyle = GDK_GC_FBDATA(pGC)->values.join_style;
    int		    lw = GDK_GC_FBDATA(pGC)->values.line_width;

    if (lw == 1 && !spanData) {
	/* Lines going in the same direction have no join */
	if (pLeft->dx >= 0 == pRight->dx <= 0)
	    return;
	if (joinStyle != GDK_JOIN_ROUND) {
    	    denom = - pLeft->dx * (double)pRight->dy + pRight->dx * (double)pLeft->dy;
    	    if (denom == 0)
	    	return;	/* no join to draw */
	}
	if (joinStyle != GDK_JOIN_MITER) {
	    miLineOnePoint (pDrawable, pGC, pixel, spanData, pLeft->x, pLeft->y);
	    return;
	}
    } else {
    	if (joinStyle == GDK_JOIN_ROUND)
    	{
	    miLineArc(pDrawable, pGC, pixel, spanData,
		      pLeft, pRight,
		      (double)0.0, (double)0.0, TRUE);
	    return;
    	}
    	denom = - pLeft->dx * (double)pRight->dy + pRight->dx * (double)pLeft->dy;
    	if (denom == 0.0)
	    return;	/* no join to draw */
    }

    swapslopes = 0;
    if (denom > 0)
    {
	pLeft->xa = -pLeft->xa;
	pLeft->ya = -pLeft->ya;
	pLeft->dx = -pLeft->dx;
	pLeft->dy = -pLeft->dy;
    }
    else
    {
	swapslopes = 1;
	pRight->xa = -pRight->xa;
	pRight->ya = -pRight->ya;
	pRight->dx = -pRight->dx;
	pRight->dy = -pRight->dy;
    }

    vertices[0].x = pRight->xa;
    vertices[0].y = pRight->ya;
    slopes[0].dx = -pRight->dy;
    slopes[0].dy =  pRight->dx;
    slopes[0].k = 0;

    vertices[1].x = 0;
    vertices[1].y = 0;
    slopes[1].dx =  pLeft->dy;
    slopes[1].dy = -pLeft->dx;
    slopes[1].k = 0;

    vertices[2].x = pLeft->xa;
    vertices[2].y = pLeft->ya;

    if (joinStyle == GDK_JOIN_MITER)
    {
    	my = (pLeft->dy  * (pRight->xa * pRight->dy - pRight->ya * pRight->dx) -
              pRight->dy * (pLeft->xa  * pLeft->dy  - pLeft->ya  * pLeft->dx )) /
	      denom;
    	if (pLeft->dy != 0)
    	{
	    mx = pLeft->xa + (my - pLeft->ya) *
			    (double) pLeft->dx / (double) pLeft->dy;
    	}
    	else
    	{
	    mx = pRight->xa + (my - pRight->ya) *
			    (double) pRight->dx / (double) pRight->dy;
    	}
	/* check miter limit */
	if ((mx * mx + my * my) * 4 > SQSECANT * lw * lw)
	    joinStyle = GDK_JOIN_BEVEL;
    }

    if (joinStyle == GDK_JOIN_MITER)
    {
	slopes[2].dx = pLeft->dx;
	slopes[2].dy = pLeft->dy;
	slopes[2].k =  pLeft->k;
	if (swapslopes)
	{
	    slopes[2].dx = -slopes[2].dx;
	    slopes[2].dy = -slopes[2].dy;
	    slopes[2].k  = -slopes[2].k;
	}
	vertices[3].x = mx;
	vertices[3].y = my;
	slopes[3].dx = pRight->dx;
	slopes[3].dy = pRight->dy;
	slopes[3].k  = pRight->k;
	if (swapslopes)
	{
	    slopes[3].dx = -slopes[3].dx;
	    slopes[3].dy = -slopes[3].dy;
	    slopes[3].k  = -slopes[3].k;
	}
	edgecount = 4;
    }
    else
    {
	double	scale, dx, dy, adx, ady;

	adx = dx = pRight->xa - pLeft->xa;
	ady = dy = pRight->ya - pLeft->ya;
	if (adx < 0)
	    adx = -adx;
	if (ady < 0)
	    ady = -ady;
	scale = ady;
	if (adx > ady)
	    scale = adx;
	slopes[2].dx = (dx * 65536) / scale;
	slopes[2].dy = (dy * 65536) / scale;
	slopes[2].k = ((pLeft->xa + pRight->xa) * slopes[2].dy -
		       (pLeft->ya + pRight->ya) * slopes[2].dx) / 2.0;
	edgecount = 3;
    }

    y = miPolyBuildPoly (vertices, slopes, edgecount, pLeft->x, pLeft->y,
		   left, right, &nleft, &nright, &height);
    miFillPolyHelper (pDrawable, pGC, pixel, spanData, y, height, left, right, nleft, nright);
}

static int
miLineArcI (GdkDrawable *pDraw, GdkGC *pGC, int xorg, int yorg, GdkSpan *points)
{
    register GdkSpan* tpts, *bpts;
    register int x, y, e, ex, slw;

    tpts = points;
    slw = GDK_GC_FBDATA(pGC)->values.line_width;
    if (slw == 1)
    {
	tpts->x = xorg;
	tpts->y = yorg;
	tpts->width = 1;
	return 1;
    }
    bpts = tpts + slw;
    y = (slw >> 1) + 1;
    if (slw & 1)
	e = - ((y << 2) + 3);
    else
	e = - (y << 3);
    ex = -4;
    x = 0;
    while (y)
    {
	e += (y << 3) - 4;
	while (e >= 0)
	{
	    x++;
	    e += (ex = -((x << 3) + 4));
	}
	y--;
	slw = (x << 1) + 1;
	if ((e == ex) && (slw > 1))
	    slw--;
	tpts->x = xorg - x;
	tpts->y = yorg - y;
	tpts->width = slw;
	tpts++;
	if ((y != 0) && ((slw > 1) || (e != ex)))
	{
	    bpts--;
	    bpts->x = xorg - x;
	    bpts->y = yorg + y;
	    bpts->width = slw;
	}
    }
    return (GDK_GC_FBDATA(pGC)->values.line_width);
}

#define CLIPSTEPEDGE(edgey,edge,edgeleft) \
    if (ybase == edgey) \
    { \
	if (edgeleft) \
	{ \
	    if (edge->x > xcl) \
		xcl = edge->x; \
	} \
	else \
	{ \
	    if (edge->x < xcr) \
		xcr = edge->x; \
	} \
	edgey++; \
	edge->x += edge->stepx; \
	edge->e += edge->dx; \
	if (edge->e > 0) \
	{ \
	    edge->x += edge->signdx; \
	    edge->e -= edge->dy; \
	} \
    }

static int
miLineArcD (GdkDrawable *pDraw, GdkGC *pGC, double xorg, double yorg, GdkSpan *points,
	    PolyEdgePtr edge1, int edgey1, gboolean edgeleft1,
            PolyEdgePtr edge2, int edgey2, gboolean edgeleft2)
{
    register GdkSpan* pts;
    double radius, x0, y0, el, er, yk, xlk, xrk, k;
    int xbase, ybase, y, boty, xl, xr, xcl, xcr;
    int ymin, ymax;
    gboolean edge1IsMin, edge2IsMin;
    int ymin1, ymin2;

    pts = points;
    xbase = floor(xorg);
    x0 = xorg - xbase;
    ybase = ICEIL (yorg);
    y0 = yorg - ybase;
    xlk = x0 + x0 + 1.0;
    xrk = x0 + x0 - 1.0;
    yk = y0 + y0 - 1.0;
    radius = ((double)GDK_GC_FBDATA(pGC)->values.line_width) / 2.0;
    y = floor(radius - y0 + 1.0);
    ybase -= y;
    ymin = ybase;
    ymax = 65536;
    edge1IsMin = FALSE;
    ymin1 = edgey1;
    if (edge1->dy >= 0)
    {
    	if (!edge1->dy)
    	{
	    if (edgeleft1)
	    	edge1IsMin = TRUE;
	    else
	    	ymax = edgey1;
	    edgey1 = 65536;
    	}
    	else
    	{
	    if ((edge1->signdx < 0) == edgeleft1)
	    	edge1IsMin = TRUE;
    	}
    }
    edge2IsMin = FALSE;
    ymin2 = edgey2;
    if (edge2->dy >= 0)
    {
    	if (!edge2->dy)
    	{
	    if (edgeleft2)
	    	edge2IsMin = TRUE;
	    else
	    	ymax = edgey2;
	    edgey2 = 65536;
    	}
    	else
    	{
	    if ((edge2->signdx < 0) == edgeleft2)
	    	edge2IsMin = TRUE;
    	}
    }
    if (edge1IsMin)
    {
	ymin = ymin1;
	if (edge2IsMin && ymin1 > ymin2)
	    ymin = ymin2;
    } else if (edge2IsMin)
	ymin = ymin2;
    el = radius * radius - ((y + y0) * (y + y0)) - (x0 * x0);
    er = el + xrk;
    xl = 1;
    xr = 0;
    if (x0 < 0.5)
    {
	xl = 0;
	el -= xlk;
    }
    boty = (y0 < -0.5) ? 1 : 0;
    if (ybase + y - boty > ymax)
	boty = ymax - ybase - y;
    while (y > boty)
    {
	k = (y << 1) + yk;
	er += k;
	while (er > 0.0)
	{
	    xr++;
	    er += xrk - (xr << 1);
	}
	el += k;
	while (el >= 0.0)
	{
	    xl--;
	    el += (xl << 1) - xlk;
	}
	y--;
	ybase++;
	if (ybase < ymin)
	    continue;
	xcl = xl + xbase;
	xcr = xr + xbase;
	CLIPSTEPEDGE(edgey1, edge1, edgeleft1);
	CLIPSTEPEDGE(edgey2, edge2, edgeleft2);
	if (xcr >= xcl)
	{
	    pts->x = xcl;
	    pts->y = ybase;
	    pts->width = xcr - xcl + 1;
	    pts++;
	}
    }
    er = xrk - (xr << 1) - er;
    el = (xl << 1) - xlk - el;
    boty = floor(-y0 - radius + 1.0);
    if (ybase + y - boty > ymax)
	boty = ymax - ybase - y;
    while (y > boty)
    {
	k = (y << 1) + yk;
	er -= k;
	while ((er >= 0.0) && (xr >= 0))
	{
	    xr--;
	    er += xrk - (xr << 1);
	}
	el -= k;
	while ((el > 0.0) && (xl <= 0))
	{
	    xl++;
	    el += (xl << 1) - xlk;
	}
	y--;
	ybase++;
	if (ybase < ymin)
	    continue;
	xcl = xl + xbase;
	xcr = xr + xbase;
	CLIPSTEPEDGE(edgey1, edge1, edgeleft1);
	CLIPSTEPEDGE(edgey2, edge2, edgeleft2);
	if (xcr >= xcl)
	{
	    pts->x = xcl;
	    pts->y = ybase;
	    pts->width = xcr - xcl + 1;
	    pts++;
	}
    }
    return (pts - points);
}

int
miRoundJoinFace (register LineFacePtr face, register PolyEdgePtr edge,
                 gboolean *leftEdge)
{
    int	    y;
    int	    dx, dy;
    double  xa, ya;
    gboolean	left;

    dx = -face->dy;
    dy = face->dx;
    xa = face->xa;
    ya = face->ya;
    left = 1;
    if (ya > 0)
    {
	ya = 0.0;
	xa = 0.0;
    }
    if (dy < 0 || (dy == 0 && dx > 0))
    {
	dx = -dx;
	dy = -dy;
	left = !left;
    }
    if (dx == 0 && dy == 0)
	dy = 1;
    if (dy == 0)
    {
	y = ICEIL (face->ya) + face->y;
	edge->x = -32767;
	edge->stepx = 0;
	edge->signdx = 0;
	edge->e = -1;
	edge->dy = 0;
	edge->dx = 0;
	edge->height = 0;
    }
    else
    {
	y = miPolyBuildEdge (xa, ya, 0.0, dx, dy, face->x, face->y, !left, edge);
	edge->height = 32767;
    }
    *leftEdge = !left;
    return y;
}

void
miRoundJoinClip (register LineFacePtr pLeft, register LineFacePtr pRight,
                 PolyEdgePtr edge1, PolyEdgePtr edge2, int *y1, int *y2,
                 gboolean *left1, gboolean *left2)
{
    double	denom;

    denom = - pLeft->dx * (double)pRight->dy + pRight->dx * (double)pLeft->dy;

    if (denom >= 0)
    {
	pLeft->xa = -pLeft->xa;
	pLeft->ya = -pLeft->ya;
    }
    else
    {
	pRight->xa = -pRight->xa;
	pRight->ya = -pRight->ya;
    }
    *y1 = miRoundJoinFace (pLeft, edge1, left1);
    *y2 = miRoundJoinFace (pRight, edge2, left2);
}

int
miRoundCapClip (register LineFacePtr face, gboolean isInt,
                register PolyEdgePtr edge, gboolean *leftEdge)
{
    int	    y;
    register int dx, dy;
    double  xa, ya, k;
    gboolean	left;

    dx = -face->dy;
    dy = face->dx;
    xa = face->xa;
    ya = face->ya;
    k = 0.0;
    if (!isInt)
	k = face->k;
    left = 1;
    if (dy < 0 || (dy == 0 && dx > 0))
    {
	dx = -dx;
	dy = -dy;
	xa = -xa;
	ya = -ya;
	left = !left;
    }
    if (dx == 0 && dy == 0)
	dy = 1;
    if (dy == 0)
    {
	y = ICEIL (face->ya) + face->y;
	edge->x = -32767;
	edge->stepx = 0;
	edge->signdx = 0;
	edge->e = -1;
	edge->dy = 0;
	edge->dx = 0;
	edge->height = 0;
    }
    else
    {
	y = miPolyBuildEdge (xa, ya, k, dx, dy, face->x, face->y, !left, edge);
	edge->height = 32767;
    }
    *leftEdge = !left;
    return y;
}

static void
miLineArc (GdkDrawable *pDraw, GdkGC *pGC, GdkColor *pixel,
           SpanDataPtr spanData, register LineFacePtr leftFace,
           register LineFacePtr rightFace, double xorg, double yorg,
           gboolean isInt)
{
    GdkSpan* points;
    int xorgi = 0, yorgi = 0;
    GdkColor		oldPixel;
    Spans spanRec;
    int n;
    PolyEdgeRec	edge1, edge2;
    int		edgey1, edgey2;
    gboolean	edgeleft1, edgeleft2;

    if (isInt)
    {
	xorgi = leftFace ? leftFace->x : rightFace->x;
	yorgi = leftFace ? leftFace->y : rightFace->y;
    }
    edgey1 = 65536;
    edgey2 = 65536;
    edge1.x = 0; /* not used, keep memory checkers happy */
    edge1.dy = -1;
    edge2.x = 0; /* not used, keep memory checkers happy */
    edge2.dy = -1;
    edgeleft1 = FALSE;
    edgeleft2 = FALSE;
    if (((GDK_GC_FBDATA(pGC)->values.line_style != GDK_LINE_SOLID || GDK_GC_FBDATA(pGC)->values.line_width > 2) &&
	 (GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_ROUND && GDK_GC_FBDATA(pGC)->values.join_style != GDK_JOIN_ROUND)) ||
	(GDK_GC_FBDATA(pGC)->values.join_style == GDK_JOIN_ROUND && GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_BUTT))
    {
	if (isInt)
	{
	    xorg = (double) xorgi;
	    yorg = (double) yorgi;
	}
	if (leftFace && rightFace)
	{
	    miRoundJoinClip (leftFace, rightFace, &edge1, &edge2,
			     &edgey1, &edgey2, &edgeleft1, &edgeleft2);
	}
	else if (leftFace)
	{
	    edgey1 = miRoundCapClip (leftFace, isInt, &edge1, &edgeleft1);
	}
	else if (rightFace)
	{
	    edgey2 = miRoundCapClip (rightFace, isInt, &edge2, &edgeleft2);
	}
	isInt = FALSE;
    }
    if (!spanData)
    {
    	points = (GdkSpan*)ALLOCATE_LOCAL(sizeof(GdkSpan) * GDK_GC_FBDATA(pGC)->values.line_width);
    	if (!points)
	    return;
    	oldPixel = GDK_GC_FBDATA(pGC)->values.foreground;
    	if (pixel->pixel != oldPixel.pixel)
    	{
	  gdk_gc_set_foreground(pGC, pixel);
    	}
    }
    else
    {
	points = (GdkSpan*) g_malloc (GDK_GC_FBDATA(pGC)->values.line_width * sizeof (GdkSpan));
	if (!points)
	    return;
	spanRec.points = points;
    }
    if (isInt)
	n = miLineArcI(pDraw, pGC, xorgi, yorgi, points);
    else
	n = miLineArcD(pDraw, pGC, xorg, yorg, points,
		       &edge1, edgey1, edgeleft1,
		       &edge2, edgey2, edgeleft2);

    if (!spanData)
    {
      gdk_fb_fill_spans(pDraw, pGC, points, n, TRUE);
      DEALLOCATE_LOCAL(points);
      if (pixel->pixel != oldPixel.pixel)
    	{
	  gdk_gc_set_foreground(pGC, &oldPixel);
    	}
    }
    else
    {
	spanRec.count = n;
	AppendSpanGroup (pGC, pixel, &spanRec, spanData)
    }
}

void
miLineProjectingCap (GdkDrawable *pDrawable, register GdkGC *pGC, GdkColor *pixel,
                     SpanDataPtr spanData, register LineFacePtr face,
                     gboolean isLeft, double xorg, double yorg, gboolean isInt)
{
    int	xorgi = 0, yorgi = 0;
    int	lw;
    PolyEdgeRec	lefts[2], rights[2];
    int		lefty, righty, topy, bottomy;
    PolyEdgePtr left, right;
    PolyEdgePtr	top, bottom;
    double	xa,ya;
    double	k;
    double	xap, yap;
    int		dx, dy;
    double	projectXoff, projectYoff;
    double	maxy;
    int		finaly;
    
    if (isInt)
    {
	xorgi = face->x;
	yorgi = face->y;
    }
    lw = GDK_GC_FBDATA(pGC)->values.line_width;
    dx = face->dx;
    dy = face->dy;
    k = face->k;
    if (dy == 0)
    {
	lefts[0].height = lw;
	lefts[0].x = xorgi;
	if (isLeft)
	    lefts[0].x -= (lw >> 1);
	lefts[0].stepx = 0;
	lefts[0].signdx = 1;
	lefts[0].e = -lw;
	lefts[0].dx = 0;
	lefts[0].dy = lw;
	rights[0].height = lw;
	rights[0].x = xorgi;
	if (!isLeft)
	    rights[0].x += ((lw + 1) >> 1);
	rights[0].stepx = 0;
	rights[0].signdx = 1;
	rights[0].e = -lw;
	rights[0].dx = 0;
	rights[0].dy = lw;
	miFillPolyHelper (pDrawable, pGC, pixel, spanData, yorgi - (lw >> 1), lw,
			  lefts, rights, 1, 1);
    }
    else if (dx == 0)
    {
	topy = yorgi;
	bottomy = yorgi + dy;
	if (isLeft)
	    topy -= (lw >> 1);
	else
	    bottomy += (lw >> 1);
	lefts[0].height = bottomy - topy;
	lefts[0].x = xorgi - (lw >> 1);
	lefts[0].stepx = 0;
	lefts[0].signdx = 1;
	lefts[0].e = -dy;
	lefts[0].dx = dx;
	lefts[0].dy = dy;

	rights[0].height = bottomy - topy;
	rights[0].x = lefts[0].x + (lw-1);
	rights[0].stepx = 0;
	rights[0].signdx = 1;
	rights[0].e = -dy;
	rights[0].dx = dx;
	rights[0].dy = dy;
	miFillPolyHelper (pDrawable, pGC, pixel, spanData, topy, bottomy - topy, lefts, rights, 1, 1);
    }
    else
    {
	xa = face->xa;
	ya = face->ya;
	projectXoff = -ya;
	projectYoff = xa;
	if (dx < 0)
	{
	    right = &rights[1];
	    left = &lefts[0];
	    top = &rights[0];
	    bottom = &lefts[1];
	}
	else
	{
	    right = &rights[0];
	    left = &lefts[1];
	    top = &lefts[0];
	    bottom = &rights[1];
	}
	if (isLeft)
	{
	    righty = miPolyBuildEdge (xa, ya,
		     k, dx, dy, xorgi, yorgi, 0, right);
	    
	    xa = -xa;
	    ya = -ya;
	    k = -k;
	    lefty = miPolyBuildEdge (xa - projectXoff, ya - projectYoff,
				     k, dx, dy, xorgi, yorgi, 1, left);
	    if (dx > 0)
	    {
		ya = -ya;
		xa = -xa;
	    }
	    xap = xa - projectXoff;
	    yap = ya - projectYoff;
	    topy = miPolyBuildEdge (xap, yap, xap * dx + yap * dy,
				    -dy, dx, xorgi, yorgi, dx > 0, top);
	    bottomy = miPolyBuildEdge (xa, ya,
				       0.0, -dy, dx, xorgi, yorgi, dx < 0, bottom);
	    maxy = -ya;
	}
	else
	{
	    righty = miPolyBuildEdge (xa - projectXoff, ya - projectYoff,
		     k, dx, dy, xorgi, yorgi, 0, right);
	    
	    xa = -xa;
	    ya = -ya;
	    k = -k;
	    lefty = miPolyBuildEdge (xa, ya,
		    k, dx, dy, xorgi, yorgi, 1, left);
	    if (dx > 0)
	    {
		ya = -ya;
		xa = -xa;
	    }
	    xap = xa - projectXoff;
	    yap = ya - projectYoff;
	    topy = miPolyBuildEdge (xa, ya, 0.0, -dy, dx, xorgi, xorgi, dx > 0, top);
	    bottomy = miPolyBuildEdge (xap, yap, xap * dx + yap * dy,
				       -dy, dx, xorgi, xorgi, dx < 0, bottom);
	    maxy = -ya + projectYoff;
	}
	finaly = ICEIL(maxy) + yorgi;
	if (dx < 0)
	{
	    left->height = bottomy - lefty;
	    right->height = finaly - righty;
	    top->height = righty - topy;
	}
	else
	{
	    right->height =  bottomy - righty;
	    left->height = finaly - lefty;
	    top->height = lefty - topy;
	}
	bottom->height = finaly - bottomy;
	miFillPolyHelper (pDrawable, pGC, pixel, spanData, topy,
		     bottom->height + bottomy - topy, lefts, rights, 2, 2);
    }
}

static void
miWideSegment (GdkDrawable *pDrawable, GdkGC *pGC, GdkColor *pixel, SpanDataPtr spanData,
	       int x1, int y1, int x2, int y2, gboolean projectLeft, gboolean projectRight, LineFacePtr leftFace, LineFacePtr rightFace)
{
    double	l, L, r;
    double	xa, ya;
    double	projectXoff = 0.0, projectYoff = 0.0;
    double	k;
    double	maxy;
    int		x, y;
    int		dx, dy;
    int		finaly;
    PolyEdgePtr left, right;
    PolyEdgePtr	top, bottom;
    int		lefty, righty, topy, bottomy;
    int		signdx;
    PolyEdgeRec	lefts[2], rights[2];
    LineFacePtr	tface;
    int		lw = GDK_GC_FBDATA(pGC)->values.line_width;

    g_assert(leftFace);
    /* draw top-to-bottom always */
    if (y2 < y1 || (y2 == y1 && x2 < x1))
    {
	x = x1;
	x1 = x2;
	x2 = x;

	y = y1;
	y1 = y2;
	y2 = y;

	x = projectLeft;
	projectLeft = projectRight;
	projectRight = x;

	tface = leftFace;
	leftFace = rightFace;
	rightFace = tface;
    }

    dy = y2 - y1;
    signdx = 1;
    dx = x2 - x1;
    if (dx < 0)
	signdx = -1;

    g_assert(leftFace);
    leftFace->x = x1;
    leftFace->y = y1;
    leftFace->dx = dx;
    leftFace->dy = dy;

    rightFace->x = x2;
    rightFace->y = y2;
    rightFace->dx = -dx;
    rightFace->dy = -dy;

    if (dy == 0)
    {
	rightFace->xa = 0;
	rightFace->ya = (double) lw / 2.0;
	rightFace->k = -(double) (lw * dx) / 2.0;
	leftFace->xa = 0;
	leftFace->ya = -rightFace->ya;
	leftFace->k = rightFace->k;
	x = x1;
	if (projectLeft)
	    x -= (lw >> 1);
	y = y1 - (lw >> 1);
	dx = x2 - x;
	if (projectRight)
	    dx += ((lw + 1) >> 1);
	dy = lw;
	miFillRectPolyHelper (pDrawable, pGC, pixel, spanData,
			      x, y, dx, dy);
    }
    else if (dx == 0)
    {
	leftFace->xa =  (double) lw / 2.0;
	leftFace->ya = 0;
	leftFace->k = (double) (lw * dy) / 2.0;
	rightFace->xa = -leftFace->xa;
	rightFace->ya = 0;
	rightFace->k = leftFace->k;
	y = y1;
	if (projectLeft)
	    y -= lw >> 1;
	x = x1 - (lw >> 1);
	dy = y2 - y;
	if (projectRight)
	    dy += ((lw + 1) >> 1);
	dx = lw;
	miFillRectPolyHelper (pDrawable, pGC, pixel, spanData,
			      x, y, dx, dy);
    }
    else
    {
    	l = ((double) lw) / 2.0;
    	L = hypot ((double) dx, (double) dy);

	if (dx < 0)
	{
	    right = &rights[1];
	    left = &lefts[0];
	    top = &rights[0];
	    bottom = &lefts[1];
	}
	else
	{
	    right = &rights[0];
	    left = &lefts[1];
	    top = &lefts[0];
	    bottom = &rights[1];
	}
	r = l / L;

	/* coord of upper bound at integral y */
	ya = -r * dx;
	xa = r * dy;

	if (projectLeft | projectRight)
	{
	    projectXoff = -ya;
	    projectYoff = xa;
	}

    	/* xa * dy - ya * dx */
	k = l * L;

	leftFace->xa = xa;
	leftFace->ya = ya;
	leftFace->k = k;
	rightFace->xa = -xa;
	rightFace->ya = -ya;
	rightFace->k = k;

	if (projectLeft)
	    righty = miPolyBuildEdge (xa - projectXoff, ya - projectYoff,
				      k, dx, dy, x1, y1, 0, right);
	else
	    righty = miPolyBuildEdge (xa, ya,
				      k, dx, dy, x1, y1, 0, right);

	/* coord of lower bound at integral y */
	ya = -ya;
	xa = -xa;

	/* xa * dy - ya * dx */
	k = - k;

	if (projectLeft)
	    lefty = miPolyBuildEdge (xa - projectXoff, ya - projectYoff,
				     k, dx, dy, x1, y1, 1, left);
	else
	    lefty = miPolyBuildEdge (xa, ya,
				     k, dx, dy, x1, y1, 1, left);

	/* coord of top face at integral y */

	if (signdx > 0)
	{
	    ya = -ya;
	    xa = -xa;
	}

	if (projectLeft)
	{
	    double xap = xa - projectXoff;
	    double yap = ya - projectYoff;
	    topy = miPolyBuildEdge (xap, yap, xap * dx + yap * dy,
				    -dy, dx, x1, y1, dx > 0, top);
	}
	else
	    topy = miPolyBuildEdge (xa, ya, 0.0, -dy, dx, x1, y1, dx > 0, top);

	/* coord of bottom face at integral y */

	if (projectRight)
	{
	    double xap = xa + projectXoff;
	    double yap = ya + projectYoff;
	    bottomy = miPolyBuildEdge (xap, yap, xap * dx + yap * dy,
				       -dy, dx, x2, y2, dx < 0, bottom);
	    maxy = -ya + projectYoff;
	}
	else
	{
	    bottomy = miPolyBuildEdge (xa, ya,
				       0.0, -dy, dx, x2, y2, dx < 0, bottom);
	    maxy = -ya;
	}

	finaly = ICEIL (maxy) + y2;

	if (dx < 0)
	{
	    left->height = bottomy - lefty;
	    right->height = finaly - righty;
	    top->height = righty - topy;
	}
	else
	{
	    right->height =  bottomy - righty;
	    left->height = finaly - lefty;
	    top->height = lefty - topy;
	}
	bottom->height = finaly - bottomy;
	miFillPolyHelper (pDrawable, pGC, pixel, spanData, topy,
			  bottom->height + bottomy - topy, lefts, rights, 2, 2);
    }
}

SpanDataPtr
miSetupSpanData (register GdkGC *pGC, SpanDataPtr spanData, int npt)
{
    if ((npt < 3 && GDK_GC_FBDATA(pGC)->values.cap_style != GDK_CAP_ROUND) || miSpansEasyRop(GDK_GC_FBDATA(pGC)->alu))
	return (SpanDataPtr) NULL;
    if (GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_DOUBLE_DASH)
	miInitSpanGroup (&spanData->bgGroup);
    miInitSpanGroup (&spanData->fgGroup);
    return spanData;
}

void
miCleanupSpanData (GdkDrawable *pDrawable, GdkGC *pGC, SpanDataPtr spanData)
{
    if (GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_DOUBLE_DASH)
    {
	GdkColor oldPixel, pixel;
	
	pixel = GDK_GC_FBDATA(pGC)->values.background;
	oldPixel = GDK_GC_FBDATA(pGC)->values.foreground;
    	if (pixel.pixel != oldPixel.pixel)
	  gdk_gc_set_foreground(pGC, &pixel);
	miFillUniqueSpanGroup (pDrawable, pGC, &spanData->bgGroup);
	miFreeSpanGroup (&spanData->bgGroup);
    	if (pixel.pixel != oldPixel.pixel)
	  gdk_gc_set_foreground(pGC, &oldPixel);
    }
    miFillUniqueSpanGroup (pDrawable, pGC, &spanData->fgGroup);
    miFreeSpanGroup (&spanData->fgGroup);
}

void
miWideLine (GdkDrawable *pDrawable, GdkGC *pGC, int mode,
            register int npt, register GdkPoint *pPts)
{
    int		    x1, y1, x2, y2;
    SpanDataRec	    spanDataRec;
    SpanDataPtr	    spanData;
    GdkColor pixel;
    gboolean	    projectLeft, projectRight;
    LineFaceRec	    leftFace, rightFace, prevRightFace;
    LineFaceRec	    firstFace;
    register int    first;
    gboolean	    somethingDrawn = FALSE;
    gboolean	    selfJoin;

    spanData = miSetupSpanData (pGC, &spanDataRec, npt);
    pixel = GDK_GC_FBDATA(pGC)->values.foreground;
    x2 = pPts->x;
    y2 = pPts->y;
    first = TRUE;
    selfJoin = FALSE;
    if (npt > 1)
    {
      if (0 /* mode == CoordModePrevious*/)
    	{
	    int nptTmp;
	    GdkPoint* pPtsTmp;
    
	    x1 = x2;
	    y1 = y2;
	    nptTmp = npt;
	    pPtsTmp = pPts + 1;
	    while (--nptTmp)
	    {
	    	x1 += pPtsTmp->x;
	    	y1 += pPtsTmp->y;
	    	++pPtsTmp;
	    }
	    if (x2 == x1 && y2 == y1)
	    	selfJoin = TRUE;
    	}
    	else if (x2 == pPts[npt-1].x && y2 == pPts[npt-1].y)
    	{
	    selfJoin = TRUE;
    	}
    }
    projectLeft = GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_PROJECTING && !selfJoin;
    projectRight = FALSE;
    while (--npt)
    {
	x1 = x2;
	y1 = y2;
	++pPts;
	x2 = pPts->x;
	y2 = pPts->y;
	if (0 /* mode == CoordModePrevious */)
	{
	    x2 += x1;
	    y2 += y1;
	}
	if (x1 != x2 || y1 != y2)
	{
	    somethingDrawn = TRUE;
	    if (npt == 1 && GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_PROJECTING && !selfJoin)
	    	projectRight = TRUE;
	    miWideSegment (pDrawable, pGC, &pixel, spanData, x1, y1, x2, y2,
		       	   projectLeft, projectRight, &leftFace, &rightFace);
	    if (first)
	    {
	    	if (selfJoin)
		    firstFace = leftFace;
	    	else if (GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_ROUND)
		{
		    if (GDK_GC_FBDATA(pGC)->values.line_width == 1 && !spanData)
			miLineOnePoint (pDrawable, pGC, &pixel, spanData, x1, y1);
		    else
		    	miLineArc (pDrawable, pGC, &pixel, spanData,
			       	   &leftFace, (LineFacePtr) NULL,
 			       	   (double)0.0, (double)0.0,
			       	   TRUE);
		}
	    }
	    else
	    {
	    	miLineJoin (pDrawable, pGC, &pixel, spanData, &leftFace,
		            &prevRightFace);
	    }
	    prevRightFace = rightFace;
	    first = FALSE;
	    projectLeft = FALSE;
	}
	if (npt == 1 && somethingDrawn)
 	{
	    if (selfJoin)
		miLineJoin (pDrawable, pGC, &pixel, spanData, &firstFace,
			    &rightFace);
	    else if (GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_ROUND)
	    {
		if (GDK_GC_FBDATA(pGC)->values.line_width == 1 && !spanData)
		    miLineOnePoint (pDrawable, pGC, &pixel, spanData, x2, y2);
		else
		    miLineArc (pDrawable, pGC, &pixel, spanData,
			       (LineFacePtr) NULL, &rightFace,
			       (double)0.0, (double)0.0,
			       TRUE);
	    }
	}
    }
    /* handle crock where all points are coincedent */
    if (!somethingDrawn)
    {
	projectLeft = GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_PROJECTING;
	miWideSegment (pDrawable, pGC, &pixel, spanData,
		       x2, y2, x2, y2, projectLeft, projectLeft,
		       &leftFace, &rightFace);
	if (GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_ROUND)
	{
	    miLineArc (pDrawable, pGC, &pixel, spanData,
		       &leftFace, (LineFacePtr) NULL,
		       (double)0.0, (double)0.0,
		       TRUE);
	    rightFace.dx = -1;	/* sleezy hack to make it work */
	    miLineArc (pDrawable, pGC, &pixel, spanData,
		       (LineFacePtr) NULL, &rightFace,
 		       (double)0.0, (double)0.0,
		       TRUE);
	}
    }
    if (spanData)
	miCleanupSpanData (pDrawable, pGC, spanData);
}

#define V_TOP	    0
#define V_RIGHT	    1
#define V_BOTTOM    2
#define V_LEFT	    3

static void
miWideDashSegment (GdkDrawable *pDrawable, register GdkGC *pGC,
                   SpanDataPtr spanData, int *pDashOffset, int *pDashIndex,
	           int x1, int y1, int x2, int y2, gboolean projectLeft,
                   gboolean projectRight, LineFacePtr leftFace,
                   LineFacePtr rightFace)
{
    int		    dashIndex, dashRemain;
    unsigned char   *pDash;
    double	    L, l;
    double	    k;
    PolyVertexRec   vertices[4];
    PolyVertexRec   saveRight, saveBottom;
    PolySlopeRec    slopes[4];
    PolyEdgeRec	    left[2], right[2];
    LineFaceRec	    lcapFace, rcapFace;
    int		    nleft, nright;
    int		    h;
    int		    y;
    int		    dy, dx;
    GdkColor pixel;
    double	    LRemain;
    double	    r;
    double	    rdx, rdy;
    double	    dashDx, dashDy;
    double	    saveK = 0.0;
    gboolean	    first = TRUE;
    double	    lcenterx, lcentery, rcenterx = 0.0, rcentery = 0.0;
    GdkColor fgPixel, bgPixel;
    
    dx = x2 - x1;
    dy = y2 - y1;
    dashIndex = *pDashIndex;
    pDash = GDK_GC_FBDATA(pGC)->dash_list;
    dashRemain = pDash[dashIndex] - *pDashOffset;
    fgPixel = GDK_GC_FBDATA(pGC)->values.foreground;
    bgPixel = GDK_GC_FBDATA(pGC)->values.background;
    if (GDK_GC_FBDATA(pGC)->values.fill == GDK_OPAQUE_STIPPLED ||
	GDK_GC_FBDATA(pGC)->values.fill == GDK_TILED)
    {
	bgPixel = fgPixel;
    }

    l = ((double) GDK_GC_FBDATA(pGC)->values.line_width) / 2.0;
    if (dx == 0)
    {
	L = dy;
	rdx = 0;
	rdy = l;
	if (dy < 0)
	{
	    L = -dy;
	    rdy = -l;
	}
    }
    else if (dy == 0)
    {
	L = dx;
	rdx = l;
	rdy = 0;
	if (dx < 0)
	{
	    L = -dx;
	    rdx = -l;
	}
    }
    else
    {
	L = hypot ((double) dx, (double) dy);
	r = l / L;

	rdx = r * dx;
	rdy = r * dy;
    }
    k = l * L;
    LRemain = L;
    /* All position comments are relative to a line with dx and dy > 0,
     * but the code does not depend on this */
    /* top */
    slopes[V_TOP].dx = dx;
    slopes[V_TOP].dy = dy;
    slopes[V_TOP].k = k;
    /* right */
    slopes[V_RIGHT].dx = -dy;
    slopes[V_RIGHT].dy = dx;
    slopes[V_RIGHT].k = 0;
    /* bottom */
    slopes[V_BOTTOM].dx = -dx;
    slopes[V_BOTTOM].dy = -dy;
    slopes[V_BOTTOM].k = k;
    /* left */
    slopes[V_LEFT].dx = dy;
    slopes[V_LEFT].dy = -dx;
    slopes[V_LEFT].k = 0;

    /* preload the start coordinates */
    vertices[V_RIGHT].x = vertices[V_TOP].x = rdy;
    vertices[V_RIGHT].y = vertices[V_TOP].y = -rdx;

    vertices[V_BOTTOM].x = vertices[V_LEFT].x = -rdy;
    vertices[V_BOTTOM].y = vertices[V_LEFT].y = rdx;

    if (projectLeft)
    {
	vertices[V_TOP].x -= rdx;
	vertices[V_TOP].y -= rdy;

	vertices[V_LEFT].x -= rdx;
	vertices[V_LEFT].y -= rdy;

	slopes[V_LEFT].k = rdx * dx + rdy * dy;
    }

    lcenterx = x1;
    lcentery = y1;

    if (GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_ROUND)
    {
	lcapFace.dx = dx;
	lcapFace.dy = dy;
	lcapFace.x = x1;
	lcapFace.y = y1;

	rcapFace.dx = -dx;
	rcapFace.dy = -dy;
	rcapFace.x = x1;
	rcapFace.y = y1;
    }
    while (LRemain > dashRemain)
    {
	dashDx = (dashRemain * dx) / L;
	dashDy = (dashRemain * dy) / L;

	rcenterx = lcenterx + dashDx;
	rcentery = lcentery + dashDy;

	vertices[V_RIGHT].x += dashDx;
	vertices[V_RIGHT].y += dashDy;

	vertices[V_BOTTOM].x += dashDx;
	vertices[V_BOTTOM].y += dashDy;

	slopes[V_RIGHT].k = vertices[V_RIGHT].x * dx + vertices[V_RIGHT].y * dy;

	if (GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_DOUBLE_DASH || !(dashIndex & 1))
	{
	    if (GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_ON_OFF_DASH &&
 	        GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_PROJECTING)
	    {
		saveRight = vertices[V_RIGHT];
		saveBottom = vertices[V_BOTTOM];
		saveK = slopes[V_RIGHT].k;
		
		if (!first)
		{
		    vertices[V_TOP].x -= rdx;
		    vertices[V_TOP].y -= rdy;
    
		    vertices[V_LEFT].x -= rdx;
		    vertices[V_LEFT].y -= rdy;

		    slopes[V_LEFT].k = vertices[V_LEFT].x *
				       slopes[V_LEFT].dy -
				       vertices[V_LEFT].y *
				       slopes[V_LEFT].dx;
		}
		
		vertices[V_RIGHT].x += rdx;
		vertices[V_RIGHT].y += rdy;

		vertices[V_BOTTOM].x += rdx;
		vertices[V_BOTTOM].y += rdy;

		slopes[V_RIGHT].k = vertices[V_RIGHT].x *
				   slopes[V_RIGHT].dy -
				   vertices[V_RIGHT].y *
				   slopes[V_RIGHT].dx;
	    }
	    y = miPolyBuildPoly (vertices, slopes, 4, x1, y1,
			     	 left, right, &nleft, &nright, &h);
	    pixel = (dashIndex & 1) ? bgPixel : fgPixel;
	    miFillPolyHelper (pDrawable, pGC, &pixel, spanData, y, h, left, right, nleft, nright);

	    if (GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_ON_OFF_DASH)
	    {
		switch (GDK_GC_FBDATA(pGC)->values.cap_style)
		{
		case GDK_CAP_PROJECTING:
		    vertices[V_BOTTOM] = saveBottom;
		    vertices[V_RIGHT] = saveRight;
		    slopes[V_RIGHT].k = saveK;
		    break;
		case GDK_CAP_ROUND:
		    if (!first)
		    {
		    	if (dx < 0)
		    	{
		    	    lcapFace.xa = -vertices[V_LEFT].x;
		    	    lcapFace.ya = -vertices[V_LEFT].y;
			    lcapFace.k = slopes[V_LEFT].k;
		    	}
		    	else
		    	{
		    	    lcapFace.xa = vertices[V_TOP].x;
		    	    lcapFace.ya = vertices[V_TOP].y;
			    lcapFace.k = -slopes[V_LEFT].k;
		    	}
		    	miLineArc (pDrawable, pGC, &pixel, spanData,
			       	   &lcapFace, (LineFacePtr) NULL,
			       	   lcenterx, lcentery, FALSE);
		    }
		    if (dx < 0)
		    {
		    	rcapFace.xa = vertices[V_BOTTOM].x;
		    	rcapFace.ya = vertices[V_BOTTOM].y;
			rcapFace.k = slopes[V_RIGHT].k;
		    }
		    else
		    {
		    	rcapFace.xa = -vertices[V_RIGHT].x;
		    	rcapFace.ya = -vertices[V_RIGHT].y;
			rcapFace.k = -slopes[V_RIGHT].k;
		    }
		    miLineArc (pDrawable, pGC, &pixel, spanData,
			       (LineFacePtr) NULL, &rcapFace,
			       rcenterx, rcentery, FALSE);
		    break;
		default:
		  break;
	    	}
	    }
	}
	LRemain -= dashRemain;
	++dashIndex;
	if (dashIndex == GDK_GC_FBDATA(pGC)->dash_list_len)
	    dashIndex = 0;
	dashRemain = pDash[dashIndex];

	lcenterx = rcenterx;
	lcentery = rcentery;

	vertices[V_TOP] = vertices[V_RIGHT];
	vertices[V_LEFT] = vertices[V_BOTTOM];
	slopes[V_LEFT].k = -slopes[V_RIGHT].k;
	first = FALSE;
    }

    if (GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_DOUBLE_DASH || !(dashIndex & 1))
    {
    	vertices[V_TOP].x -= dx;
    	vertices[V_TOP].y -= dy;

	vertices[V_LEFT].x -= dx;
	vertices[V_LEFT].y -= dy;

	vertices[V_RIGHT].x = rdy;
	vertices[V_RIGHT].y = -rdx;

	vertices[V_BOTTOM].x = -rdy;
	vertices[V_BOTTOM].y = rdx;

	
	if (projectRight)
	{
	    vertices[V_RIGHT].x += rdx;
	    vertices[V_RIGHT].y += rdy;
    
	    vertices[V_BOTTOM].x += rdx;
	    vertices[V_BOTTOM].y += rdy;
	    slopes[V_RIGHT].k = vertices[V_RIGHT].x *
				slopes[V_RIGHT].dy -
				vertices[V_RIGHT].y *
				slopes[V_RIGHT].dx;
	}
	else
	    slopes[V_RIGHT].k = 0;

	if (!first && GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_ON_OFF_DASH &&
	    GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_PROJECTING)
	{
	    vertices[V_TOP].x -= rdx;
	    vertices[V_TOP].y -= rdy;

	    vertices[V_LEFT].x -= rdx;
	    vertices[V_LEFT].y -= rdy;
	    slopes[V_LEFT].k = vertices[V_LEFT].x *
			       slopes[V_LEFT].dy -
			       vertices[V_LEFT].y *
			       slopes[V_LEFT].dx;
	}
	else
	    slopes[V_LEFT].k += dx * dx + dy * dy;


	y = miPolyBuildPoly (vertices, slopes, 4, x2, y2,
			     left, right, &nleft, &nright, &h);

	pixel = (dashIndex & 1) ? GDK_GC_FBDATA(pGC)->values.background : GDK_GC_FBDATA(pGC)->values.foreground;
	miFillPolyHelper (pDrawable, pGC, &pixel, spanData, y, h, left, right, nleft, nright);
	if (!first && GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_ON_OFF_DASH &&
	    GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_ROUND)
	{
	    lcapFace.x = x2;
	    lcapFace.y = y2;
	    if (dx < 0)
	    {
		lcapFace.xa = -vertices[V_LEFT].x;
		lcapFace.ya = -vertices[V_LEFT].y;
		lcapFace.k = slopes[V_LEFT].k;
	    }
	    else
	    {
		lcapFace.xa = vertices[V_TOP].x;
		lcapFace.ya = vertices[V_TOP].y;
		lcapFace.k = -slopes[V_LEFT].k;
	    }
	    miLineArc (pDrawable, pGC, &pixel, spanData,
		       &lcapFace, (LineFacePtr) NULL,
		       rcenterx, rcentery, FALSE);
	}
    }
    dashRemain = ((double) dashRemain) - LRemain;
    if (dashRemain == 0)
    {
	dashIndex++;
	if (dashIndex == GDK_GC_FBDATA(pGC)->dash_list_len)
	    dashIndex = 0;
	dashRemain = pDash[dashIndex];
    }

    leftFace->x = x1;
    leftFace->y = y1;
    leftFace->dx = dx;
    leftFace->dy = dy;
    leftFace->xa = rdy;
    leftFace->ya = -rdx;
    leftFace->k = k;

    rightFace->x = x2;
    rightFace->y = y2;
    rightFace->dx = -dx;
    rightFace->dy = -dy;
    rightFace->xa = -rdy;
    rightFace->ya = rdx;
    rightFace->k = k;

    *pDashIndex = dashIndex;
    *pDashOffset = pDash[dashIndex] - dashRemain;
}

void
miWideDash (GdkDrawable *pDrawable, register GdkGC *pGC, int mode,
            register int npt, register GdkPoint *pPts)
{
    int		    x1, y1, x2, y2;
    GdkColor pixel;
    gboolean	    projectLeft, projectRight;
    LineFaceRec	    leftFace, rightFace, prevRightFace;
    LineFaceRec	    firstFace;
    int		    first;
    int		    dashIndex, dashOffset;
    register int    prevDashIndex;
    SpanDataRec	    spanDataRec;
    SpanDataPtr	    spanData;
    gboolean	    somethingDrawn = FALSE;
    gboolean	    selfJoin;
    gboolean	    endIsFg = FALSE, startIsFg = FALSE;
    gboolean            firstIsFg = FALSE, prevIsFg = FALSE;

#ifndef XFree86Server
    /* XXX backward compatibility */
    if (GDK_GC_FBDATA(pGC)->values.line_width == 0)
    {
	miZeroDashLine (pDrawable, pGC, mode, npt, pPts);
	return;
    }
#endif
    if (GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_DOUBLE_DASH && 
	(GDK_GC_FBDATA(pGC)->values.fill == GDK_OPAQUE_STIPPLED || GDK_GC_FBDATA(pGC)->values.fill == GDK_TILED))
    {
	miWideLine (pDrawable, pGC, mode, npt, pPts);
	return;
    }
    if (npt == 0)
	return;
    spanData = miSetupSpanData (pGC, &spanDataRec, npt);
    x2 = pPts->x;
    y2 = pPts->y;
    first = TRUE;
    selfJoin = FALSE;
    if (0 /* mode == CoordModePrevious */)
    {
	int nptTmp;
	GdkPoint* pPtsTmp;

	x1 = x2;
	y1 = y2;
	nptTmp = npt;
	pPtsTmp = pPts + 1;
	while (--nptTmp)
	{
	    x1 += pPtsTmp->x;
	    y1 += pPtsTmp->y;
	    ++pPtsTmp;
	}
	if (x2 == x1 && y2 == y1)
	    selfJoin = TRUE;
    }
    else if (x2 == pPts[npt-1].x && y2 == pPts[npt-1].y)
    {
	selfJoin = TRUE;
    }
    projectLeft = GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_PROJECTING && !selfJoin;
    projectRight = FALSE;
    dashIndex = 0;
    dashOffset = 0;
    miStepDash (GDK_GC_FBDATA(pGC)->dash_offset, &dashIndex,
	        GDK_GC_FBDATA(pGC)->dash_list, (int)GDK_GC_FBDATA(pGC)->dash_list_len, &dashOffset);
    while (--npt)
    {
	x1 = x2;
	y1 = y2;
	++pPts;
	x2 = pPts->x;
	y2 = pPts->y;
	if (0 /* mode == CoordModePrevious */)
	{
	    x2 += x1;
	    y2 += y1;
	}
	if (x1 != x2 || y1 != y2)
	{
	    somethingDrawn = TRUE;
	    if (npt == 1 && GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_PROJECTING && 
		(!selfJoin || !firstIsFg))
		projectRight = TRUE;
	    prevDashIndex = dashIndex;
	    miWideDashSegment (pDrawable, pGC, spanData, &dashOffset, &dashIndex,
				x1, y1, x2, y2,
				projectLeft, projectRight, &leftFace, &rightFace);
	    startIsFg = !(prevDashIndex & 1);
	    endIsFg = (dashIndex & 1) ^ (dashOffset != 0);
	    if (GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_DOUBLE_DASH || startIsFg)
	    {
	    	pixel = startIsFg ? GDK_GC_FBDATA(pGC)->values.foreground : GDK_GC_FBDATA(pGC)->values.background;
	    	if (first || (GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_ON_OFF_DASH && !prevIsFg))
	    	{
	    	    if (first && selfJoin)
		    {
		    	firstFace = leftFace;
			firstIsFg = startIsFg;
		    }
	    	    else if (GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_ROUND)
		    	miLineArc (pDrawable, pGC, &pixel, spanData,
			       	   &leftFace, (LineFacePtr) NULL,
			       	   (double)0.0, (double)0.0, TRUE);
	    	}
	    	else
	    	{
	    	    miLineJoin (pDrawable, pGC, &pixel, spanData, &leftFace,
		            	&prevRightFace);
	    	}
	    }
	    prevRightFace = rightFace;
	    prevIsFg = endIsFg;
	    first = FALSE;
	    projectLeft = FALSE;
	}
	if (npt == 1 && somethingDrawn)
	{
	    if (GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_DOUBLE_DASH || endIsFg)
	    {
		pixel = endIsFg ? GDK_GC_FBDATA(pGC)->values.foreground : GDK_GC_FBDATA(pGC)->values.background;
		if (selfJoin && (GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_DOUBLE_DASH || firstIsFg))
		{
		    miLineJoin (pDrawable, pGC, &pixel, spanData, &firstFace,
				&rightFace);
		}
		else 
		{
		    if (GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_ROUND)
			miLineArc (pDrawable, pGC, &pixel, spanData,
				    (LineFacePtr) NULL, &rightFace,
				    (double)0.0, (double)0.0, TRUE);
		}
	    }
	    else
	    {
		/* glue a cap to the start of the line if
		 * we're OnOffDash and ended on odd dash
		 */
		if (selfJoin && firstIsFg)
		{
		    pixel = GDK_GC_FBDATA(pGC)->values.foreground;
		    if (GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_PROJECTING)
			miLineProjectingCap (pDrawable, pGC, &pixel, spanData,
				    &firstFace, TRUE,
				    (double)0.0, (double)0.0, TRUE);
		    else if (GDK_GC_FBDATA(pGC)->values.cap_style == GDK_CAP_ROUND)
			miLineArc (pDrawable, pGC, &pixel, spanData,
				    &firstFace, (LineFacePtr) NULL,
				    (double)0.0, (double)0.0, TRUE);
		}
	    }
	}
    }
    /* handle crock where all points are coincident */
    if (!somethingDrawn && (GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_DOUBLE_DASH || !(dashIndex & 1)))
    {
	/* not the same as endIsFg computation above */
	pixel = (dashIndex & 1) ? GDK_GC_FBDATA(pGC)->values.background : GDK_GC_FBDATA(pGC)->values.foreground;
	switch (GDK_GC_FBDATA(pGC)->values.cap_style) {
	case GDK_CAP_ROUND:
	    miLineArc (pDrawable, pGC, &pixel, spanData,
		       (LineFacePtr) NULL, (LineFacePtr) NULL,
		       (double)x2, (double)y2,
		       FALSE);
	    break;
	case GDK_CAP_PROJECTING:
	    x1 = GDK_GC_FBDATA(pGC)->values.line_width;
	    miFillRectPolyHelper (pDrawable, pGC, &pixel, spanData,
				  x2 - (x1 >> 1), y2 - (x1 >> 1), x1, x1);
	    break;
	default:
	  break;
	}
    }
    if (spanData)
	miCleanupSpanData (pDrawable, pGC, spanData);
}

/* these are stubs to allow old ddx miValidateGCs to work without change */

void
miMiter(void)
{
}

void
miNotMiter(void)
{
}
