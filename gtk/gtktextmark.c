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

const char *
gtk_text_mark_get_name (GtkTextMark *mark)
{
  GtkTextLineSegment *seg;

  seg = (GtkTextLineSegment*)mark;

  return seg->body.mark.name;
}


GtkTextMark *
gtk_text_mark_ref (GtkTextMark *mark)
{
  GtkTextLineSegment *seg;

  seg = (GtkTextLineSegment*)mark;

  mark_segment_ref (seg);

  return mark;
}

void
gtk_text_mark_unref (GtkTextMark *mark)
{
  GtkTextLineSegment *seg;

  seg = (GtkTextLineSegment*)mark;
  
  mark_segment_unref (seg);
}

gboolean
gtk_text_mark_get_deleted (GtkTextMark *mark)
{
  GtkTextLineSegment *seg;
  
  g_return_val_if_fail (mark != NULL, FALSE);

  seg = (GtkTextLineSegment*)mark;
  
  return seg->body.mark.tree == NULL;
}

/*
 * Macro that determines the size of a mark segment:
 */

#define MSEG_SIZE ((unsigned) (G_STRUCT_OFFSET(GtkTextLineSegment, body) \
	+ sizeof(GtkTextMarkBody)))


GtkTextLineSegment*
mark_segment_new (GtkTextBTree *tree,
                  gboolean left_gravity,
                  const gchar *name)
{
  GtkTextLineSegment *mark;

  mark = (GtkTextLineSegment *) g_malloc0 (MSEG_SIZE);
  mark->body.mark.name = g_strdup (name);

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
  g_return_if_fail (mark != NULL);
  g_return_if_fail (mark->type == &gtk_text_right_mark_type ||
                    mark->type == &gtk_text_left_mark_type);
  g_return_if_fail (mark->body.mark.refcount > 0);
  
  mark->body.mark.refcount += 1;
}

void
mark_segment_unref(GtkTextLineSegment *mark)
{
  g_return_if_fail (mark != NULL);
  g_return_if_fail (mark->type == &gtk_text_right_mark_type ||
                    mark->type == &gtk_text_left_mark_type);
  g_return_if_fail (mark->body.mark.refcount > 0);

  mark->body.mark.refcount -= 1;

  if (mark->body.mark.refcount == 0)
    {
      g_free(mark->body.mark.name);
      
      g_free(mark);
    }
}


static int                 mark_segment_delete_func  (GtkTextLineSegment *segPtr,
                                                      GtkTextLine        *line,
                                                      int                 treeGone);
static GtkTextLineSegment *mark_segment_cleanup_func (GtkTextLineSegment *segPtr,
                                                      GtkTextLine        *line);
static void                mark_segment_check_func   (GtkTextLineSegment *segPtr,
                                                      GtkTextLine        *line);


/*
 * The following structures declare the "mark" segment types.
 * There are actually two types for marks, one with left gravity
 * and one with right gravity.  They are identical except for
 * their gravity property.
 */

GtkTextLineSegmentClass gtk_text_right_mark_type = {
    "mark",					        /* name */
    FALSE,						/* leftGravity */
    NULL,		                        	/* splitFunc */
    mark_segment_delete_func,				/* deleteFunc */
    mark_segment_cleanup_func,				/* cleanupFunc */
    NULL,		                                /* lineChangeFunc */
    mark_segment_check_func				/* checkFunc */
};

GtkTextLineSegmentClass gtk_text_left_mark_type = {
    "mark",					        /* name */
    TRUE,						/* leftGravity */
    NULL,			                        /* splitFunc */
    mark_segment_delete_func,				/* deleteFunc */
    mark_segment_cleanup_func,				/* cleanupFunc */
    NULL,		                                /* lineChangeFunc */
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

static gboolean
mark_segment_delete_func (GtkTextLineSegment *segPtr,
                          GtkTextLine        *line,
                          gboolean            tree_gone)
{
  return TRUE;
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
mark_segment_cleanup_func(GtkTextLineSegment *seg,
                          GtkTextLine        *line)
{
  /* not sure why Tk did this here and not in LineChangeFunc */
  seg->body.mark.line = line;
  return seg;
}

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
mark_segment_check_func(GtkTextLineSegment *seg,
                        GtkTextLine        *line)
{
  if (seg->body.mark.line != line)
    g_error("mark_segment_check_func: seg->body.mark.line bogus");
}
