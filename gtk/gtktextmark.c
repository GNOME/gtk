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

static void gtk_text_mark_init       (GtkTextMark      *mark);
static void gtk_text_mark_class_init (GtkTextMarkClass *klass);
static void gtk_text_mark_finalize   (GObject          *obj);


static gpointer parent_class = NULL;

GType
gtk_text_mark_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GtkTextMarkClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gtk_text_mark_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GtkTextMark),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_text_mark_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GtkTextMark",
                                            &object_info);
    }
  
  return object_type;
}

static void
gtk_text_mark_init (GtkTextMark *mark)
{
  mark->segment = NULL;
}

static void
gtk_text_mark_class_init (GtkTextMarkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gtk_text_mark_finalize;
}

static void
gtk_text_mark_finalize (GObject *obj)
{
  GtkTextMark *mark;
  GtkTextLineSegment *seg;

  mark = GTK_TEXT_MARK (obj);

  seg = mark->segment;

  if (seg)
    {
      g_return_if_fail (seg->body.mark.tree == NULL);

      if (seg->body.mark.tree != NULL)
        g_warning ("GtkTextMark being finalized while still in the buffer; "
                   "someone removed a reference they didn't own! Crash "
                   "impending");
      
      g_free (seg->body.mark.name);
      g_free (seg);

      mark->segment = NULL;
    }
}

gboolean
gtk_text_mark_is_visible(GtkTextMark *mark)
{
  GtkTextLineSegment *seg;

  seg = mark->segment;

  return seg->body.mark.visible;
}

const char *
gtk_text_mark_get_name (GtkTextMark *mark)
{
  GtkTextLineSegment *seg;

  seg = mark->segment;

  return seg->body.mark.name;
}

gboolean
gtk_text_mark_get_deleted (GtkTextMark *mark)
{
  GtkTextLineSegment *seg;
  
  g_return_val_if_fail (mark != NULL, FALSE);

  seg = mark->segment;

  if (seg == NULL)
    return TRUE;
  
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

  mark->body.mark.obj = g_object_new (GTK_TYPE_TEXT_MARK, NULL);
  mark->body.mark.obj->segment = mark;
  
  mark->body.mark.tree = tree;
  mark->body.mark.line = NULL;
  mark->next = NULL;

  mark->body.mark.visible = FALSE;
  mark->body.mark.not_deleteable = FALSE;
  
  return mark;
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
