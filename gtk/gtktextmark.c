/* gtktextmark.c - mark segments
 * 
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000      Red Hat, Inc.
 * Tk -> Gtk port by Havoc Pennington <hp@redhat.com>
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The
 * following terms apply to all files associated with the software
 * unless explicitly disclaimed in individual files.
 * 
 * The authors hereby grant permission to use, copy, modify,
 * distribute, and license this software and its documentation for any
 * purpose, provided that existing copyright notices are retained in
 * all copies and that this notice is included verbatim in any
 * distributions. No written agreement, license, or royalty fee is
 * required for any of the authorized uses.  Modifications to this
 * software may be copyrighted by their authors and need not follow
 * the licensing terms described here, provided that the new terms are
 * clearly indicated on the first page of each file where they apply.
 * 
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION,
 * OR ANY DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 * MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense,
 * the software shall be classified as "Commercial Computer Software"
 * and the Government shall have only "Restricted Rights" as defined
 * in Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 * foregoing, the authors grant the U.S. Government and others acting
 * in its behalf permission to use and distribute the software in
 * accordance with the terms specified in this license.
 * 
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

  _mark_segment_ref (seg);

  return mark;
}

void
gtk_text_mark_unref (GtkTextMark *mark)
{
  GtkTextLineSegment *seg;

  seg = (GtkTextLineSegment*)mark;
  
  _mark_segment_unref (seg);
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
_mark_segment_new (GtkTextBTree *tree,
                   gboolean      left_gravity,
                   const gchar  *name)
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
_mark_segment_ref (GtkTextLineSegment *mark)
{
  g_return_if_fail (mark != NULL);
  g_return_if_fail (mark->type == &gtk_text_right_mark_type ||
                    mark->type == &gtk_text_left_mark_type);
  g_return_if_fail (mark->body.mark.refcount > 0);
  
  mark->body.mark.refcount += 1;
}

void
_mark_segment_unref (GtkTextLineSegment *mark)
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
