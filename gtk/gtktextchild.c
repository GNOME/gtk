/* gtktextchild.c - child pixmaps and widgets
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

#include "gtktextchild.h"
#include "gtktextbtree.h"

static GtkTextLineSegment *
pixbuf_segment_cleanup_func (GtkTextLineSegment *seg,
                             GtkTextLine        *line)
{
  /* nothing */
  return seg;
}

static int
pixbuf_segment_delete_func(GtkTextLineSegment *seg,
                           GtkTextLine        *line,
                           gboolean            tree_gone)
{
  if (seg->body.pixbuf.pixbuf)
    g_object_unref(G_OBJECT (seg->body.pixbuf.pixbuf));

  g_free(seg);

  return 0;
}

static void
pixbuf_segment_check_func(GtkTextLineSegment *seg,
                          GtkTextLine        *line)
{
  if (seg->next == NULL)
    g_error("pixbuf segment is the last segment in a line");

  if (seg->byte_count != 3)
    g_error("pixbuf segment has byte count of %d", seg->byte_count);
  
  if (seg->char_count != 1)
    g_error("pixbuf segment has char count of %d", seg->char_count);  
}


GtkTextLineSegmentClass gtk_text_pixbuf_type = {
  "pixbuf",                          /* name */
  FALSE,                                            /* leftGravity */
  NULL,                                          /* splitFunc */
  pixbuf_segment_delete_func,                             /* deleteFunc */
  pixbuf_segment_cleanup_func,                            /* cleanupFunc */
  NULL,                                                    /* lineChangeFunc */
  pixbuf_segment_check_func                               /* checkFunc */

};

#define PIXBUF_SEG_SIZE ((unsigned) (G_STRUCT_OFFSET(GtkTextLineSegment, body) \
	+ sizeof(GtkTextPixbuf)))

GtkTextLineSegment *
_gtk_pixbuf_segment_new (GdkPixbuf *pixbuf)
{
  GtkTextLineSegment *seg;

  seg = g_malloc (PIXBUF_SEG_SIZE);

  seg->type = &gtk_text_pixbuf_type;

  seg->next = NULL;

  seg->byte_count = 3; /* We convert to the 0xFFFD "unknown character",
                          a 3-byte sequence in UTF-8 */
  seg->char_count = 1;
  
  seg->body.pixbuf.pixbuf = pixbuf;

  g_object_ref (G_OBJECT (pixbuf));
  
  return seg;
}


static GtkTextLineSegment *
child_segment_cleanup_func (GtkTextLineSegment *seg,
                            GtkTextLine        *line)
{
  seg->body.child.line = line;
  
  return seg;
}

static int
child_segment_delete_func (GtkTextLineSegment *seg,
                           GtkTextLine       *line,
                           gboolean           tree_gone)
{
  _gtk_widget_segment_unref (seg);

  return 0;
}

static void
child_segment_check_func (GtkTextLineSegment *seg,
                          GtkTextLine        *line)
{
  if (seg->next == NULL)
    g_error("child segment is the last segment in a line");

  if (seg->byte_count != 3)
    g_error("child segment has byte count of %d", seg->byte_count);
  
  if (seg->char_count != 1)
    g_error("child segment has char count of %d", seg->char_count);  
}

GtkTextLineSegmentClass gtk_text_child_type = {
  "child-widget",                                        /* name */
  FALSE,                                                 /* leftGravity */
  NULL,                                                  /* splitFunc */
  child_segment_delete_func,                             /* deleteFunc */
  child_segment_cleanup_func,                            /* cleanupFunc */
  NULL,                                                  /* lineChangeFunc */
  child_segment_check_func                               /* checkFunc */
};

#define WIDGET_SEG_SIZE ((unsigned) (G_STRUCT_OFFSET (GtkTextLineSegment, body) \
	+ sizeof(GtkTextChildBody)))

GtkTextLineSegment *
_gtk_widget_segment_new (void)
{
  GtkTextLineSegment *seg;

  seg = g_malloc (WIDGET_SEG_SIZE);

  seg->type = &gtk_text_child_type;

  seg->next = NULL;

  seg->byte_count = 3; /* We convert to the 0xFFFD "unknown character",
                        * a 3-byte sequence in UTF-8
                        */
  seg->char_count = 1;
  
  seg->body.child.ref_count = 1;
  seg->body.child.widgets = NULL;
  seg->body.child.tree = NULL;
  seg->body.child.line = NULL;
  
  return seg;
}

void
_gtk_widget_segment_add    (GtkTextLineSegment *widget_segment,
                        GtkWidget          *child)
{
  g_assert (widget_segment->type = &gtk_text_child_type);
  
  widget_segment->body.child.widgets =
    g_slist_prepend (widget_segment->body.child.widgets,
                     child);

  g_object_ref (G_OBJECT (child));
}

void
_gtk_widget_segment_remove (GtkTextLineSegment *widget_segment,
                        GtkWidget          *child)
{
  g_assert (widget_segment->type = &gtk_text_child_type);

  widget_segment->body.child.widgets =
    g_slist_remove (widget_segment->body.child.widgets,
                    child);

  g_object_unref (G_OBJECT (child));
}

void
_gtk_widget_segment_ref (GtkTextLineSegment *widget_segment)
{
  g_assert (widget_segment->type = &gtk_text_child_type);

  widget_segment->body.child.ref_count += 1;
}

void
_gtk_widget_segment_unref (GtkTextLineSegment *widget_segment)
{
  g_assert (widget_segment->type = &gtk_text_child_type);

  widget_segment->body.child.ref_count -= 1;

  if (widget_segment->body.child.ref_count == 0)
    {
      GSList *tmp_list;

      if (widget_segment->body.child.tree == NULL)
        g_warning ("widget segment destroyed while still in btree");
      
      tmp_list = widget_segment->body.child.widgets;
      while (tmp_list)
        {
          g_object_unref (G_OBJECT (tmp_list->data));

          tmp_list = g_slist_next (tmp_list);
        }

      g_slist_free (widget_segment->body.child.widgets);

      g_free (widget_segment);
    }
}

void
gtk_text_child_anchor_ref (GtkTextChildAnchor *anchor)
{
  GtkTextLineSegment *seg = (GtkTextLineSegment *) anchor;
  
  g_return_if_fail (seg->type = &gtk_text_child_type);
  g_return_if_fail (seg->body.child.ref_count > 0);

  _gtk_widget_segment_ref (seg);
}

void
gtk_text_child_anchor_unref (GtkTextChildAnchor *anchor)
{
  GtkTextLineSegment *seg = (GtkTextLineSegment *) anchor;
  
  g_return_if_fail (seg->type = &gtk_text_child_type);
  g_return_if_fail (seg->body.child.ref_count > 0);

  _gtk_widget_segment_unref (seg);
}

GList*
gtk_text_child_anchor_get_widgets (GtkTextChildAnchor *anchor)
{
  GtkTextLineSegment *seg = (GtkTextLineSegment *) anchor;
  GList *list = NULL;
  GSList *iter;
  
  g_return_val_if_fail (seg->type = &gtk_text_child_type, NULL);
  
  iter = seg->body.child.widgets;
  while (iter != NULL)
    {
      list = g_list_prepend (list, iter->data);

      iter = g_slist_next (iter);
    }

  /* Order is not relevant, so we don't need to reverse the list
   * again.
   */
  return list;
}

gboolean
gtk_text_child_anchor_get_deleted (GtkTextChildAnchor *anchor)
{
  GtkTextLineSegment *seg = (GtkTextLineSegment *) anchor;
  
  g_return_val_if_fail (seg->type = &gtk_text_child_type, TRUE);

  return seg->body.child.tree == NULL;
}



