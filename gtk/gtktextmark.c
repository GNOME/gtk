/* 
 * tkTextMark.c --
 *
 *	This file contains the procedure that implement marks for
 *	text widgets.
 *
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "gtktextbtree.h"

gboolean
gtk_text_mark_is_visible(GtkTextMark *mark)
{
  GtkTextLineSegment *seg;

  seg = (GtkTextLineSegment*)mark;

  return seg->body.mark.visible;
}

char *
gtk_text_mark_get_name (GtkTextMark *mark)
{
  GtkTextLineSegment *seg;

  seg = (GtkTextLineSegment*)mark;

  return g_strdup (seg->body.mark.name);
}

/*
 * Macro that determines the size of a mark segment:
 */

#define MSEG_SIZE ((unsigned) (G_STRUCT_OFFSET(GtkTextLineSegment, body) \
	+ sizeof(GtkTextMarkBody)))


GtkTextLineSegment*
mark_segment_new(GtkTextBTree *tree,
                 gboolean left_gravity,
                 const gchar *name)
{
  GtkTextLineSegment *mark;

  mark = (GtkTextLineSegment *) g_malloc0(MSEG_SIZE);
  mark->body.mark.name = g_strdup(name);

  if (left_gravity)
    mark->type = &gtk_text_left_mark_type;
  else
    mark->type = &gtk_text_right_mark_type;

  mark->byte_count = 0;
  mark->char_count = 0;

  mark->body.mark.tree = tree;
  mark->body.mark.line = NULL;
  mark->next = NULL;

  mark->body.mark.refcount = 1;

  mark->body.mark.visible = FALSE;
  mark->body.mark.not_deleteable = FALSE;
  
  return mark;
}

void
mark_segment_ref(GtkTextLineSegment *mark)
{
  g_return_if_fail(mark != NULL);
  g_return_if_fail(mark->type == &gtk_text_right_mark_type ||
                   mark->type == &gtk_text_left_mark_type);
  g_return_if_fail(mark->body.mark.refcount > 0);
  
  mark->body.mark.refcount += 1;
}

void
mark_segment_unref(GtkTextLineSegment *mark)
{
  g_return_if_fail(mark != NULL);
  g_return_if_fail(mark->type == &gtk_text_right_mark_type ||
                   mark->type == &gtk_text_left_mark_type);
  g_return_if_fail(mark->body.mark.refcount > 0);

  mark->body.mark.refcount -= 1;

  if (mark->body.mark.refcount == 0)
    {
      g_free(mark->body.mark.name);
      
      g_free(mark);
    }
}

/*
 * Forward references for procedures defined in this file:
 */

static int              mark_segment_delete_func  (GtkTextLineSegment   *segPtr,
                                                   GtkTextLine      *line,
                                                   int                treeGone);
static GtkTextLineSegment *mark_segment_cleanup_func (GtkTextLineSegment   *segPtr,
                                                   GtkTextLine      *line);
static void             mark_segment_check_func   (GtkTextLineSegment   *segPtr,
                                                   GtkTextLine      *line);

/*
 * The following structures declare the "mark" segment types.
 * There are actually two types for marks, one with left gravity
 * and one with right gravity.  They are identical except for
 * their gravity property.
 */

GtkTextLineSegmentClass gtk_text_right_mark_type = {
    "mark",					/* name */
    FALSE,						/* leftGravity */
    (GtkTextLineSegmentSplitFunc) NULL,			/* splitFunc */
    mark_segment_delete_func,				/* deleteFunc */
    mark_segment_cleanup_func,				/* cleanupFunc */
    (GtkTextLineSegmentLineChangeFunc) NULL,		/* lineChangeFunc */
    mark_segment_check_func				/* checkFunc */
};

GtkTextLineSegmentClass gtk_text_left_mark_type = {
    "mark",					/* name */
    TRUE,						/* leftGravity */
    (GtkTextLineSegmentSplitFunc) NULL,			/* splitFunc */
    mark_segment_delete_func,				/* deleteFunc */
    mark_segment_cleanup_func,				/* cleanupFunc */
    (GtkTextLineSegmentLineChangeFunc) NULL,		/* lineChangeFunc */
    mark_segment_check_func				/* checkFunc */
};


/*
 *--------------------------------------------------------------
 *
 * mark_segment_delete_func --
 *
 *	This procedure is invoked by the text B-tree code whenever
 *	a mark lies in a range of characters being deleted.
 *
 * Results:
 *	Returns 1 to indicate that deletion has been rejected.
 *
 * Side effects:
 *	None (even if the whole tree is being deleted we don't
 *	free up the mark;  it will be done elsewhere).
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
mark_segment_delete_func(segPtr, line, treeGone)
    GtkTextLineSegment *segPtr;		/* Segment being deleted. */
    GtkTextLine *line;		/* Line containing segment. */
    int treeGone;			/* Non-zero means the entire tree is
					 * being deleted, so everything must
					 * get cleaned up. */
{
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * mark_segment_cleanup_func --
 *
 *	This procedure is invoked by the B-tree code whenever a
 *	mark segment is moved from one line to another.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The line field of the segment gets updated.
 *
 *--------------------------------------------------------------
 */

static GtkTextLineSegment *
mark_segment_cleanup_func(markPtr, line)
    GtkTextLineSegment *markPtr;		/* Mark segment that's being moved. */
    GtkTextLine *line;		/* Line that now contains segment. */
{
    markPtr->body.mark.line = line;
    return markPtr;
}

#if 0


/*
 *--------------------------------------------------------------
 *
 * GtkTextInsertDisplayFunc --
 *
 *	This procedure is called to display the insertion
 *	cursor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Graphics are drawn.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
void
GtkTextInsertDisplayFunc(chunkPtr, x, y, height, baseline, display, dst, screenY)
    GtkTextDisplayChunk *chunkPtr;		/* Chunk that is to be drawn. */
    int x;				/* X-position in dst at which to
					 * draw this chunk (may differ from
					 * the x-position in the chunk because
					 * of scrolling). */
    int y;				/* Y-position at which to draw this
					 * chunk in dst (x-position is in
					 * the chunk itself). */
    int height;				/* Total height of line. */
    int baseline;			/* Offset of baseline from y. */
    Display *display;			/* Display to use for drawing. */
    Drawable dst;			/* Pixmap or window in which to draw
					 * chunk. */
    int screenY;			/* Y-coordinate in text window that
					 * corresponds to y. */
{
    GtkTextView *tkxt = (GtkTextView *) chunkPtr->clientData;
    int halfWidth = tkxt->insertWidth/2;

    if ((x + halfWidth) < 0) {
	/*
	 * The insertion cursor is off-screen.  Just return.
	 */

	return;
    }

    /*
     * As a special hack to keep the cursor visible on mono displays
     * (or anywhere else that the selection and insertion cursors
     * have the same color) write the default background in the cursor
     * area (instead of nothing) when the cursor isn't on.  Otherwise
     * the selection might hide the cursor.
     */

    if (tkxt->flags & INSERT_ON) {
	Tk_Fill3DRectangle(tkxt->tkwin, dst, tkxt->insertBorder,
		x - tkxt->insertWidth/2, y, tkxt->insertWidth,
		height, tkxt->insertBorderWidth, TK_RELIEF_RAISED);
    } else if (tkxt->selBorder == tkxt->insertBorder) {
	Tk_Fill3DRectangle(tkxt->tkwin, dst, tkxt->border,
		x - tkxt->insertWidth/2, y, tkxt->insertWidth,
		height, 0, TK_RELIEF_FLAT);
    }
}

/*
 *--------------------------------------------------------------
 *
 * InsertUndisplayFunc --
 *
 *	This procedure is called when the insertion cursor is no
 *	longer at a visible point on the display.  It does nothing
 *	right now.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static void
InsertUndisplayFunc(tkxt, chunkPtr)
    GtkTextView *tkxt;			/* Overall information about text
					 * widget. */
    GtkTextDisplayChunk *chunkPtr;		/* Chunk that is about to be freed. */
{
    return;
}

#endif
/*
 *--------------------------------------------------------------
 *
 * mark_segment_check_func --
 *
 *	This procedure is invoked by the B-tree code to perform
 *	consistency checks on mark segments.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The procedure panics if it detects anything wrong with
 *	the mark.
 *
 *--------------------------------------------------------------
 */

static void
mark_segment_check_func(markPtr, line)
    GtkTextLineSegment *markPtr;		/* Segment to check. */
    GtkTextLine *line;		/* Line containing segment. */
{
    if (markPtr->body.mark.line != line)
      g_error("mark_segment_check_func: markPtr->body.mark.line bogus");

    /* No longer do this because we don't have access to btree
       struct members */
#if 0
    /*
     * Make sure that the mark is still present in the text's mark
     * hash table.
     */
    for (hPtr = Tcl_FirstHashEntry(&markPtr->body.mark.tkxt->markTable,
	    &search); hPtr != markPtr->body.mark.hPtr;
	    hPtr = Tcl_NextHashEntry(&search)) {
	if (hPtr == NULL) {
	    panic("mark_segment_check_func couldn't find hash table entry for mark");
	}
    }
#endif
}
