/* $TOG: miwideline.h /main/12 1998/02/09 14:49:26 kaleb $ */
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
/* $XFree86: xc/programs/Xserver/mi/miwideline.h,v 1.7 1998/10/04 09:39:35 dawes Exp $ */

/* Author:  Keith Packard, MIT X Consortium */

#ifndef MI_WIDELINE_H
#define MI_WIDELINE_H 1

#include "mispans.h"

/* 
 * interface data to span-merging polygon filler
 */

typedef struct _SpanData {
    SpanGroup	fgGroup, bgGroup;
} SpanDataRec, *SpanDataPtr;

#define AppendSpanGroup(pGC, pixel, spanPtr, spanData) { \
	SpanGroup   *group, *othergroup = NULL; \
	if (pixel->pixel == GDK_GC_FBDATA(pGC)->values.foreground.pixel) \
	{ \
	    group = &spanData->fgGroup; \
	    if (GDK_GC_FBDATA(pGC)->values.line_style == GDK_LINE_DOUBLE_DASH) \
		othergroup = &spanData->bgGroup; \
	} \
	else \
	{ \
	    group = &spanData->bgGroup; \
	    othergroup = &spanData->fgGroup; \
	} \
	miAppendSpans (group, othergroup, spanPtr); \
}

/*
 * Polygon edge description for integer wide-line routines
 */

typedef struct _PolyEdge {
    int	    height;	/* number of scanlines to process */
    int	    x;		/* starting x coordinate */
    int	    stepx;	/* fixed integral dx */
    int	    signdx;	/* variable dx sign */
    int	    e;		/* initial error term */
    int	    dy;
    int	    dx;
} PolyEdgeRec, *PolyEdgePtr;

#define SQSECANT 108.856472512142 /* 1/sin^2(11/2) - miter limit constant */

/*
 * types for general polygon routines
 */

typedef struct _PolyVertex {
    double  x, y;
} PolyVertexRec, *PolyVertexPtr;

typedef struct _PolySlope {
    int	    dx, dy;
    double  k;	    /* x0 * dy - y0 * dx */
} PolySlopeRec, *PolySlopePtr;

/*
 * Line face description for caps/joins
 */

typedef struct _LineFace {
    double  xa, ya;
    int	    dx, dy;
    int	    x, y;
    double  k;
} LineFaceRec, *LineFacePtr;

/*
 * macros for polygon fillers
 */

#define MIPOLYRELOADLEFT    if (!left_height && left_count) { \
	    	    	    	left_height = left->height; \
	    	    	    	left_x = left->x; \
	    	    	    	left_stepx = left->stepx; \
	    	    	    	left_signdx = left->signdx; \
	    	    	    	left_e = left->e; \
	    	    	    	left_dy = left->dy; \
	    	    	    	left_dx = left->dx; \
	    	    	    	--left_count; \
	    	    	    	++left; \
			    }

#define MIPOLYRELOADRIGHT   if (!right_height && right_count) { \
	    	    	    	right_height = right->height; \
	    	    	    	right_x = right->x; \
	    	    	    	right_stepx = right->stepx; \
	    	    	    	right_signdx = right->signdx; \
	    	    	    	right_e = right->e; \
	    	    	    	right_dy = right->dy; \
	    	    	    	right_dx = right->dx; \
	    	    	    	--right_count; \
	    	    	    	++right; \
			}

#define MIPOLYSTEPLEFT  left_x += left_stepx; \
    	    	    	left_e += left_dx; \
    	    	    	if (left_e > 0) \
    	    	    	{ \
	    	    	    left_x += left_signdx; \
	    	    	    left_e -= left_dy; \
    	    	    	}

#define MIPOLYSTEPRIGHT right_x += right_stepx; \
    	    	    	right_e += right_dx; \
    	    	    	if (right_e > 0) \
    	    	    	{ \
	    	    	    right_x += right_signdx; \
	    	    	    right_e -= right_dy; \
    	    	    	}

#define MILINESETPIXEL(pDrawable, pGC, pixel, oldPixel) { \
    oldPixel = GDK_GC_FBDATA(pGC)->values.foreground; \
    if (pixel->pixel != oldPixel.pixel) { \
        gdk_gc_set_foreground(pGC, pixel); \
    } \
}
#define MILINERESETPIXEL(pDrawable, pGC, pixel, oldPixel) { \
    if (pixel->pixel != oldPixel.pixel) { \
        gdk_gc_set_foreground(pGC, &oldPixel); \
    } \
}

#ifndef ICEIL
#ifdef NOINLINEICEIL
#define ICEIL(x) ((int)ceil(x))
#else
#ifdef __GNUC__
#define ICEIL ICIEL
static __inline int ICEIL(double x)
{
    int _cTmp = x;
    return ((x == _cTmp) || (x < 0.0)) ? _cTmp : _cTmp+1;
}
#else
#define ICEIL(x) ((((x) == (_cTmp = (x))) || ((x) < 0.0)) ? _cTmp : _cTmp+1)
#define ICEILTEMPDECL static int _cTmp;
#endif
#endif
#endif

extern void miFillPolyHelper(GdkDrawable* pDrawable, GdkGC* pGC,
                             GdkColor * pixel, SpanDataPtr spanData, int y,
                             int overall_height, PolyEdgePtr left,
                             PolyEdgePtr right, int left_count,
                             int right_count);

extern int miRoundJoinFace(LineFacePtr face, PolyEdgePtr edge,
                           gboolean * leftEdge);

extern void miRoundJoinClip(LineFacePtr pLeft, LineFacePtr pRight,
                            PolyEdgePtr edge1, PolyEdgePtr edge2, int *y1,
                            int *y2, gboolean *left1, gboolean *left2);

extern int miRoundCapClip(LineFacePtr face, gboolean isInt, PolyEdgePtr edge,
                          gboolean *leftEdge);

extern void miLineProjectingCap(GdkDrawable* pDrawable, GdkGC* pGC,
                                GdkColor *pixel, SpanDataPtr spanData,
                                LineFacePtr face, gboolean isLeft,
                                double xorg, double yorg, gboolean isInt);

extern SpanDataPtr miSetupSpanData(GdkGC* pGC, SpanDataPtr spanData, int npt);

extern void miCleanupSpanData(GdkDrawable* pDrawable, GdkGC* pGC,
                              SpanDataPtr spanData);

extern int miPolyBuildEdge(double x0, double y0, double k, int dx, int dy,
				int xi, int yi, int left, PolyEdgePtr edge);
extern int miPolyBuildPoly(PolyVertexPtr vertices, PolySlopePtr slopes,
				int count, int xi, int yi, PolyEdgePtr left,
				PolyEdgePtr right, int *pnleft, int *pnright,
				int *h);

#endif
