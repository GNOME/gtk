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

/* $TOG: mispans.h /main/5 1998/02/09 14:48:48 kaleb $ */

typedef struct {
    int         count;		/* number of spans		    */
    GdkSpan* points;		/* pointer to list of start points  */
} Spans;

typedef struct {
    int		size;		/* Total number of *Spans allocated	*/
    int		count;		/* Number of *Spans actually in group   */
    Spans       *group;		/* List of Spans			*/
    int		ymin, ymax;	/* Min, max y values encountered	*/
} SpanGroup;

/* Initialize SpanGroup.  MUST BE DONE before use. */
extern void miInitSpanGroup(
#if NeedFunctionPrototypes
    SpanGroup * /*spanGroup*/
#endif
);

/* Add a Spans to a SpanGroup. The spans MUST BE in y-sorted order */
extern void miAppendSpans(
#if NeedFunctionPrototypes
    SpanGroup * /*spanGroup*/,
    SpanGroup * /*otherGroup*/,
    Spans * /*spans*/
#endif
);

/* Paint a span group, possibly with some overlap */
extern void miFillSpanGroup(
#if NeedFunctionPrototypes
    GdkDrawable* /*pDraw*/,
    GdkGC* /*pGC*/,
    SpanGroup * /*spanGroup*/
#endif
);

/* Paint a span group, insuring that each pixel is painted at most once */
extern void miFillUniqueSpanGroup(
#if NeedFunctionPrototypes
    GdkDrawable* /*pDraw*/,
    GdkGC* /*pGC*/,
    SpanGroup * /*spanGroup*/
#endif
);

/* Free up data in a span group.  MUST BE DONE or you'll suffer memory leaks */
extern void miFreeSpanGroup(
#if NeedFunctionPrototypes
    SpanGroup * /*spanGroup*/
#endif
);

extern void miSubtractSpans(
#if NeedFunctionPrototypes
    SpanGroup * /*spanGroup*/,
    Spans * /*sub*/
#endif
);

extern void miDisposeSpanGroup(
#if NeedFunctionPrototypes
    SpanGroup * /*spanGroup*/
#endif
);

extern int miClipSpans(
#if NeedFunctionPrototypes
    GdkRegion* /*prgnDst*/,
    GdkPoint* /*ppt*/,
    int * /*pwidth*/,
    int /*nspans*/,
    GdkPoint* /*pptNew*/,
    int * /*pwidthNew*/,
    int /*fSorted*/
#endif
);

/* Rops which must use span groups */
#define miSpansCarefulRop(rop)	(((rop) & 0xc) == 0x8 || ((rop) & 0x3) == 0x2)
#define miSpansEasyRop(rop)	(!miSpansCarefulRop(rop))

