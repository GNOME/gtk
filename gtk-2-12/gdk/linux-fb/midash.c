/***********************************************************

Copyright 1987, 1998  The Open Group

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


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

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
/* $TOG: midash.c /main/14 1998/02/09 14:46:34 kaleb $ */

#include <config.h>
#include "mi.h"

static miDashPtr
CheckDashStorage(miDashPtr *ppseg, int nseg, int *pnsegMax);

/* return a list of DashRec.  there will be an extra
entry at the end holding the last point of the polyline.
   this means that the code that actually draws dashes can
get a pair of points for every dash.  only the point in the last
dash record is useful; the other fields are not used.
   nseg is the number of segments, not the number of points.

example:

   dash1.start
   dash2.start
   dash3.start
   last-point

defines a list of segments
   (dash1.pt, dash2.pt)
   (dash2.pt, dash3.pt)
   (dash3.pt, last-point)
and nseg == 3.

NOTE:
    EVEN_DASH == ~ODD_DASH

NOTE ALSO:
    miDashLines may return 0 segments, going from pt[0] to pt[0] with one dash.
*/

enum { EVEN_DASH=0, ODD_DASH=1 };

#define sign(x) ((x)>0)?1:( ((x)<0)?-1:0 )

miDashPtr
miDashLine(int npt, GdkPoint *ppt, unsigned int nDash,
           unsigned char *pDash, unsigned int offset, int *pnseg)
{
    GdkPoint pt1, pt2;
    int lenCur;		/* npt used from this dash */
    int lenMax;		/* npt in this dash */
    int iDash = 0;	/* index of current dash */
    int which;		/* EVEN_DASH or ODD_DASH */
    miDashPtr pseg;	/* list of dash segments */
    miDashPtr psegBase;	/* start of list */
    int nseg = 0;	/* number of dashes so far */
    int nsegMax = 0;	/* num segs we can fit in this list */

    int x, y, len;
    int adx, ady, signdx, signdy;
    int du, dv, e1, e2, e, base_e = 0;

    lenCur = offset;
    which = EVEN_DASH;
    while(lenCur >= pDash[iDash])
    {
	lenCur -= pDash[iDash];
	iDash++;
	if (iDash >= nDash)
	    iDash = 0;
	which = ~which;
    }
    lenMax = pDash[iDash];

    psegBase = (miDashPtr)NULL;
    pt2 = ppt[0];		/* just in case there is only one point */

    while(--npt)
    {
	if (PtEqual(ppt[0], ppt[1]))
	{
	    ppt++;
	    continue;		/* no duplicated points in polyline */
	}
	pt1 = *ppt++;
	pt2 = *ppt;

	adx = pt2.x - pt1.x;
	ady = pt2.y - pt1.y;
	signdx = sign(adx);
	signdy = sign(ady);
	adx = abs(adx);
	ady = abs(ady);

	if (adx > ady)
	{
	    du = adx;
	    dv = ady;
	    len = adx;
	}
	else
	{
	    du = ady;
	    dv = adx;
	    len = ady;
	}

	e1 = dv * 2;
	e2 = e1 - 2*du;
	e = e1 - du;
	x = pt1.x;
	y = pt1.y;

	nseg++;
	pseg = CheckDashStorage(&psegBase, nseg, &nsegMax);
	if (!pseg)
	    return (miDashPtr)NULL;
	pseg->pt = pt1;
	pseg->e1 = e1;
	pseg->e2 = e2;
	base_e = pseg->e = e;
	pseg->which = which;
	pseg->newLine = 1;

	while (len--)
	{
	    if (adx > ady)
	    {
		/* X_AXIS */
		if (((signdx > 0) && (e < 0)) ||
		    ((signdx <=0) && (e <=0))
		   )
		{
		    e += e1;
		}
		else
		{
		    y += signdy;
		    e += e2;
		}
		x += signdx;
	    }
	    else
	    {
		/* Y_AXIS */
		if (((signdx > 0) && (e < 0)) ||
		    ((signdx <=0) && (e <=0))
		   )
		{
		    e +=e1;
		}
		else
		{
		    x += signdx;
		    e += e2;
		}
		y += signdy;
	    }

	    lenCur++;
	    if (lenCur >= lenMax && (len || npt <= 1))
	    {
		nseg++;
		pseg = CheckDashStorage(&psegBase, nseg, &nsegMax);
		if (!pseg)
		    return (miDashPtr)NULL;
		pseg->pt.x = x;
		pseg->pt.y = y;
		pseg->e1 = e1;
		pseg->e2 = e2;
		pseg->e = e;
		which = ~which;
		pseg->which = which;
		pseg->newLine = 0;

		/* move on to next dash */
		iDash++;
		if (iDash >= nDash)
		    iDash = 0;
		lenMax = pDash[iDash];
		lenCur = 0;
	    }
	} /* while len-- */
    } /* while --npt */

    if (lenCur == 0 && nseg != 0)
    {
	nseg--;
	which = ~which;
    }
    *pnseg = nseg;
    pseg = CheckDashStorage(&psegBase, nseg+1, &nsegMax);
    if (!pseg)
	return (miDashPtr)NULL;
    pseg->pt = pt2;
    pseg->e = base_e;
    pseg->which = which;
    pseg->newLine = 0;
    return psegBase;
} 


#define NSEGDELTA 16

/* returns a pointer to the pseg[nseg-1], growing the storage as
necessary.  this interface seems unnecessarily cumbersome.

*/

static miDashPtr
CheckDashStorage(miDashPtr *ppseg, int nseg, int *pnsegMax)
#if 0
miDashPtr *ppseg;		/* base pointer */
int nseg;			/* number of segment we want to write to */
int *pnsegMax;			/* size (in segments) of list so far */
#endif
{
    if (nseg > *pnsegMax)
    {
	miDashPtr newppseg;

	*pnsegMax += NSEGDELTA;
	newppseg = (miDashPtr)g_realloc(*ppseg,
				       (*pnsegMax)*sizeof(miDashRec));
	if (!newppseg)
	{
	    g_free(*ppseg);
	    return (miDashPtr)NULL;
	}
	*ppseg = newppseg;
    }
    return(*ppseg+(nseg-1));
}

void
miStepDash (int dist, int *pDashIndex, unsigned char *pDash,
            int numInDashList, int *pDashOffset)
#if 0
    int dist;			/* distance to step */
    int *pDashIndex;		/* current dash */
    unsigned char *pDash;	/* dash list */
    int numInDashList;		/* total length of dash list */
    int *pDashOffset;		/* offset into current dash */
#endif
{
    int	dashIndex, dashOffset;
    int totallen;
    int	i;
    
    dashIndex = *pDashIndex;
    dashOffset = *pDashOffset;
    if (dist < pDash[dashIndex] - dashOffset)
    {
	*pDashOffset = dashOffset + dist;
	return;
    }
    dist -= pDash[dashIndex] - dashOffset;
    if (++dashIndex == numInDashList)
	dashIndex = 0;
    totallen = 0;
    for (i = 0; i < numInDashList; i++)
	totallen += pDash[i];
    if (totallen <= dist)
	dist = dist % totallen;
    while (dist >= pDash[dashIndex])
    {
	dist -= pDash[dashIndex];
	if (++dashIndex == numInDashList)
	    dashIndex = 0;
    }
    *pDashIndex = dashIndex;
    *pDashOffset = dist;
}
