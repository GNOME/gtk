/* gtktextlayout.c - calculate the layout of the text
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000 Red Hat, Inc.
 * Tk->Gtk port by Havoc Pennington
 * Pango support by Owen Taylor
 *
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

#include "gtksignal.h"
#include "gtktextlayout.h"
#include "gtktextbtree.h"
#include "gtktextiterprivate.h"

#include <stdlib.h>
#include <string.h>

static GtkTextLineData    *gtk_text_line_data_new                 (GtkTextLayout     *layout,
								   GtkTextLine       *line);

static GtkTextLineData *gtk_text_layout_real_wrap (GtkTextLayout *layout,
                                                     GtkTextLine *line,
                                                     /* may be NULL */
                                                     GtkTextLineData *line_data);

static void gtk_text_layout_real_get_log_attrs (GtkTextLayout  *layout,
						GtkTextLine    *line,
						PangoLogAttr  **attrs,
						gint           *n_attrs);

static void gtk_text_layout_invalidated     (GtkTextLayout     *layout);

static void gtk_text_layout_real_invalidate     (GtkTextLayout     *layout,
						 const GtkTextIter *start,
						 const GtkTextIter *end);
static void gtk_text_layout_real_free_line_data (GtkTextLayout     *layout,
						 GtkTextLine       *line,
						 GtkTextLineData   *line_data);

static void gtk_text_layout_invalidate_all (GtkTextLayout *layout);

static PangoAttribute *gtk_text_attr_appearance_new (const GtkTextAppearance *appearance);

enum {
  INVALIDATED,
  CHANGED,
  LAST_SIGNAL
};

enum {
  ARG_0,
  LAST_ARG
};

static void gtk_text_layout_init (GtkTextLayout *text_layout);
static void gtk_text_layout_class_init (GtkTextLayoutClass *klass);
static void gtk_text_layout_destroy (GtkObject *object);
static void gtk_text_layout_finalize (GObject *object);

void gtk_marshal_NONE__INT_INT_INT_INT (GtkObject  *object,
                                        GtkSignalFunc func,
                                        gpointer func_data,
                                        GtkArg  *args);

static GtkObjectClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

PangoAttrType gtk_text_attr_appearance_type = 0;

GtkType
gtk_text_layout_get_type (void)
{
  static GtkType our_type = 0;

  if (our_type == 0)
    {
      static const GtkTypeInfo our_info =
      {
        "GtkTextLayout",
        sizeof (GtkTextLayout),
        sizeof (GtkTextLayoutClass),
        (GtkClassInitFunc) gtk_text_layout_class_init,
        (GtkObjectInitFunc) gtk_text_layout_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };

    our_type = gtk_type_unique (GTK_TYPE_OBJECT, &our_info);
  }

  return our_type;
}

static void
gtk_text_layout_class_init (GtkTextLayoutClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

  parent_class = gtk_type_class (GTK_TYPE_OBJECT);

  signals[INVALIDATED] =
    gtk_signal_new ("invalidated",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextLayoutClass, invalidated),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE,
                    0);

  signals[CHANGED] =
    gtk_signal_new ("changed",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextLayoutClass, changed),
                    gtk_marshal_NONE__INT_INT_INT,
                    GTK_TYPE_NONE,
                    3,
                    GTK_TYPE_INT,
                    GTK_TYPE_INT,
                    GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
  
  object_class->destroy = gtk_text_layout_destroy;
  gobject_class->finalize = gtk_text_layout_finalize;

  klass->wrap = gtk_text_layout_real_wrap;
  klass->get_log_attrs = gtk_text_layout_real_get_log_attrs;
  klass->invalidate = gtk_text_layout_real_invalidate;
  klass->free_line_data = gtk_text_layout_real_free_line_data;
}

void
gtk_text_layout_init (GtkTextLayout *text_layout)
{
}

GtkTextLayout*
gtk_text_layout_new (void)
{
  return GTK_TEXT_LAYOUT (gtk_type_new (gtk_text_layout_get_type ()));
}

static void
free_style_cache (GtkTextLayout *text_layout)
{
  if (text_layout->one_style_cache)
    {
      gtk_text_style_values_unref (text_layout->one_style_cache);
      text_layout->one_style_cache = NULL;
    }
}

static void
gtk_text_layout_destroy (GtkObject *object)
{
  GtkTextLayout *layout;

  layout = GTK_TEXT_LAYOUT (object);

  gtk_text_layout_set_buffer (layout, NULL);  

  if (layout->default_style)
    gtk_text_style_values_unref (layout->default_style);
  layout->default_style = NULL;

  if (layout->ltr_context)
    {
      g_object_unref (G_OBJECT (layout->ltr_context));
      layout->ltr_context = NULL;
    }
  if (layout->rtl_context)
    {
      g_object_unref (G_OBJECT (layout->rtl_context));
      layout->rtl_context = NULL;
    }
  
  (* parent_class->destroy) (object);
}

static void
gtk_text_layout_finalize (GObject *object)
{
  GtkTextLayout *text_layout;

  text_layout = GTK_TEXT_LAYOUT (object);

  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

void
gtk_text_layout_set_buffer (GtkTextLayout *layout,
                            GtkTextBuffer *buffer)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (buffer == NULL || GTK_IS_TEXT_BUFFER (buffer));
  
  if (layout->buffer == buffer)
    return;

  free_style_cache (layout);
  
  if (layout->buffer)
    {
      gtk_text_btree_remove_view (layout->buffer->tree, layout);
      
      gtk_object_unref (GTK_OBJECT (layout->buffer));
      layout->buffer = NULL;
    }

  if (buffer)
    {
      layout->buffer = buffer;

      gtk_object_sink (GTK_OBJECT (buffer));
      gtk_object_ref (GTK_OBJECT (buffer));

      gtk_text_btree_add_view (buffer->tree, layout);
    }
}

void
gtk_text_layout_default_style_changed (GtkTextLayout *layout)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  
  gtk_text_layout_invalidate_all (layout);
}

void
gtk_text_layout_set_default_style (GtkTextLayout *layout,
                                   GtkTextStyleValues *values)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (values != NULL);

  if (values == layout->default_style)
    return;

  gtk_text_style_values_ref (values);
  
  if (layout->default_style)
    gtk_text_style_values_unref (layout->default_style);

  layout->default_style = values;

  gtk_text_layout_default_style_changed (layout);
}

void
gtk_text_layout_set_contexts (GtkTextLayout *layout,
			      PangoContext  *ltr_context,
			      PangoContext  *rtl_context)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  if (layout->ltr_context)
    g_object_unref (G_OBJECT (ltr_context));

  layout->ltr_context = ltr_context;
  g_object_ref (G_OBJECT (ltr_context));
  
  if (layout->rtl_context)
    g_object_unref (G_OBJECT (rtl_context));

  layout->rtl_context = rtl_context;
  g_object_ref (G_OBJECT (rtl_context));
  
  gtk_text_layout_invalidate_all (layout);
}

void
gtk_text_layout_set_screen_width (GtkTextLayout *layout, gint width)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (width >= 0);
  g_return_if_fail (layout->wrap_loop_count == 0);
  
  if (layout->screen_width == width)
    return;
  
  layout->screen_width = width;
  
  gtk_text_layout_invalidate_all (layout);
}

void
gtk_text_layout_get_size (GtkTextLayout *layout,
                          gint *width,
                          gint *height)
{
  gint w, h;
  
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  
  gtk_text_btree_get_view_size (layout->buffer->tree, layout,
                                &w, &h);

  layout->width = w;
  layout->height = h;
  
  if (width)
    *width = layout->width;

  if (height)
    *height = layout->height;
}

static void
gtk_text_layout_invalidated (GtkTextLayout *layout)
{
  gtk_signal_emit (GTK_OBJECT (layout), signals[INVALIDATED]);
}

void
gtk_text_layout_changed (GtkTextLayout *layout,
			 gint           y,
			 gint           old_height,
			 gint           new_height)
{
  gtk_signal_emit (GTK_OBJECT (layout), signals[CHANGED], y, old_height, new_height);
}

void
gtk_text_layout_free_line_data (GtkTextLayout     *layout,
				GtkTextLine       *line,
				GtkTextLineData   *line_data)
{
  (* GTK_TEXT_LAYOUT_GET_CLASS (layout)->free_line_data)
    (layout, line, line_data);
}

void
gtk_text_layout_invalidate (GtkTextLayout *layout,
                            const GtkTextIter *start_index,
                            const GtkTextIter *end_index)
{
  (* GTK_TEXT_LAYOUT_GET_CLASS (layout)->invalidate)
    (layout, start_index, end_index);
}

GtkTextLineData*
gtk_text_layout_wrap (GtkTextLayout *layout,
                       GtkTextLine  *line,
                       /* may be NULL */
                       GtkTextLineData *line_data)
{
  return (* GTK_TEXT_LAYOUT_GET_CLASS (layout)->wrap) (layout, line, line_data);
}

void
gtk_text_layout_get_log_attrs (GtkTextLayout  *layout,
			       GtkTextLine    *line,
			       PangoLogAttr  **attrs,
			       gint           *n_attrs)
{
  (* GTK_TEXT_LAYOUT_GET_CLASS (layout)->get_log_attrs)
    (layout, line, attrs, n_attrs);
}

GSList*
gtk_text_layout_get_lines (GtkTextLayout *layout,
                           /* [top_y, bottom_y) */
                           gint top_y, 
                           gint bottom_y,
                           gint *first_line_y)
{
  GtkTextLine *first_btree_line;
  GtkTextLine *last_btree_line;
  GtkTextLine *line;
  GSList *retval;
  
  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), NULL);
  g_return_val_if_fail (bottom_y > top_y, NULL);

  retval = NULL;
  
  first_btree_line = gtk_text_btree_find_line_by_y (layout->buffer->tree, layout, top_y, first_line_y);
  if (first_btree_line == NULL)
    {
      g_assert (top_y > 0);
      /* off the bottom */
      return NULL;
    }
  
  /* -1 since bottom_y is one past */
  last_btree_line = gtk_text_btree_find_line_by_y (layout->buffer->tree, layout, bottom_y - 1, NULL);

  if (!last_btree_line)
    last_btree_line = gtk_text_btree_get_line (layout->buffer->tree,
                                               gtk_text_btree_line_count (layout->buffer->tree) - 1, NULL);

  {
    GtkTextLineData *ld = gtk_text_line_get_data (last_btree_line, layout);
    if (ld->height == 0)
      G_BREAKPOINT();
  }
  
  g_assert (last_btree_line != NULL);

  line = first_btree_line;
  while (TRUE)
    {
      retval = g_slist_prepend (retval, line);

      if (line == last_btree_line)
        break;
      
      line = gtk_text_line_next (line);
    }
  
  retval = g_slist_reverse (retval);

  return retval;
}

static void
invalidate_cached_style (GtkTextLayout *layout)
{
  free_style_cache (layout);
}

/* These should be called around a loop which wraps a CONTIGUOUS bunch
 * of display lines. If the lines aren't contiguous you can't call
 * these.
 */
void
gtk_text_layout_wrap_loop_start (GtkTextLayout *layout)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (layout->one_style_cache == NULL);
  
  layout->wrap_loop_count += 1;
}

void
gtk_text_layout_wrap_loop_end (GtkTextLayout *layout)
{
  g_return_if_fail (layout->wrap_loop_count > 0);
  
  layout->wrap_loop_count -= 1;

  if (layout->wrap_loop_count == 0)
    {
      /* We cache a some stuff if we're iterating over some lines wrapping
       * them. This cleans it up.
       */
      /* Nuke our cached style */
      invalidate_cached_style (layout);
      g_assert (layout->one_style_cache == NULL);
    }
}

static void
gtk_text_layout_invalidate_all (GtkTextLayout *layout)
{
  GtkTextIter start;
  GtkTextIter end;
  
  if (layout->buffer == NULL)
    return;

  gtk_text_buffer_get_bounds (layout->buffer, &start, &end);

  gtk_text_layout_invalidate (layout, &start, &end);
}

/* FIXME: This is now completely generic, and we could probably be
 * moved into gtktextbtree.c.
 */
static void
gtk_text_layout_real_invalidate (GtkTextLayout *layout,
                                 const GtkTextIter *start,
                                 const GtkTextIter *end)
{
  GtkTextLine *line;
  GtkTextLine *last_line;
  
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (layout->wrap_loop_count == 0);

#if 0
  gtk_text_view_index_spew (start_index, "invalidate start");
  gtk_text_view_index_spew (end_index, "invalidate end");
#endif
  
  last_line = gtk_text_iter_get_line (end);
  line = gtk_text_iter_get_line (start);

  while (TRUE)
    {
      GtkTextLineData *line_data = gtk_text_line_get_data (line, layout);

      if (line_data &&
	  (line != last_line || !gtk_text_iter_starts_line (end)))
	{
	  if (layout->one_display_cache &&
	      line == layout->one_display_cache->line)
	    {
	      GtkTextLineDisplay *tmp_display = layout->one_display_cache;
	      layout->one_display_cache = NULL;
	      gtk_text_layout_free_line_display (layout, tmp_display);
	    }
	  gtk_text_line_invalidate_wrap (line, line_data);
	}
      
      if (line == last_line)
        break;
      
      line = gtk_text_line_next (line);
    }
  
  gtk_text_layout_invalidated (layout);
}

static void
gtk_text_layout_real_free_line_data (GtkTextLayout     *layout,
				     GtkTextLine       *line,
				     GtkTextLineData   *line_data)
{
  if (layout->one_display_cache && line == layout->one_display_cache->line)
    {
      GtkTextLineDisplay *tmp_display = layout->one_display_cache;
      layout->one_display_cache = NULL;
      gtk_text_layout_free_line_display (layout, tmp_display);
    }
  
  g_free (line_data);
}



/**
 * gtk_text_layout_is_valid:
 * @layout: a #GtkTextLayout
 * 
 * Check if there are any invalid regions in a #GtkTextLayout's buffer
 * 
 * Return value: #TRUE if any invalid regions were found
 **/
gboolean
gtk_text_layout_is_valid (GtkTextLayout *layout)
{
  g_return_val_if_fail (layout != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), FALSE);
  
  return gtk_text_btree_is_valid (layout->buffer->tree, layout);
}

/**
 * gtk_text_layout_validate_yrange:
 * @layout: a #GtkTextLayout
 * @anchor: iter pointing into a line that will be used as the
 *          coordinate origin
 * @y0: offset from the top of the line pointed to by @anchor at
 *      which to begin validation. (The offset here is in pixels
 *      after validation.)
 * @y1: offset from the top of the line pointed to by @anchor at
 *      which to end validation. (The offset here is in pixels
 *      after validation.)
 * 
 * Ensure that a region of a #GtkTextLayout is valid. The ::changed 
 * signal will be emitted if any lines are validated.
 **/
void
gtk_text_layout_validate_yrange (GtkTextLayout *layout,
				 GtkTextIter   *anchor,
				 gint           y0,
				 gint           y1)
{
  GtkTextLine *line;
  GtkTextLine *first_line = NULL;
  GtkTextLine *last_line = NULL;
  gint seen;
  gint delta_height = 0;
  gint first_line_y = 0;	/* Quiet GCC */
  gint last_line_y = 0;		/* Quiet GCC */
  
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  
  if (y0 > 0)
    y0 = 0;
  if (y1 < 0)
    y1 = 0;
  
  /* Validate backwards from the anchor line to y0
   */
  line = gtk_text_iter_get_line (anchor);
  seen = 0;
  while (line && seen < -y0)
    {
      GtkTextLineData *line_data = gtk_text_line_get_data (line, layout);
      if (!line_data || !line_data->valid)
	{
	  gint old_height = line_data ? line_data->height : 0;
	  
	  gtk_text_btree_validate_line (layout->buffer->tree, line, layout);
	  line_data = gtk_text_line_get_data (line, layout);

	  delta_height += line_data->height - old_height;

	  first_line = line;
	  first_line_y = -seen;
	  if (!last_line)
	    {
	      last_line = line;
	      last_line_y = -seen + line_data->height;
	    }
	}
      
      seen += line_data->height;
      line = gtk_text_line_previous (line);
    }

  /* Validate forwards to y1 */
  line = gtk_text_iter_get_line (anchor);
  seen = 0;
  while (line && seen < y1)
    {
      GtkTextLineData *line_data = gtk_text_line_get_data (line, layout);
      if (!line_data || !line_data->valid)
	{
	  gint old_height = line_data ? line_data->height : 0;
	  
	  gtk_text_btree_validate_line (layout->buffer->tree, line, layout);
	  line_data = gtk_text_line_get_data (line, layout);

	  delta_height += line_data->height - old_height;

	  if (!first_line)
	    {
	      first_line = line;
	      first_line_y = seen;
	    }
	  last_line = line;
	  last_line_y = seen + line_data->height;
	}
      
      seen += line_data->height;
      line = gtk_text_line_next (line);
    }

  /* If we found and validated any invalid lines, emit the changed singal
   */
  if (first_line)
    {
      gint line_top = gtk_text_btree_find_line_top (layout->buffer->tree, first_line, layout);            
      
      gtk_text_layout_changed (layout,
			       line_top,
			       last_line_y - first_line_y - delta_height,
			       last_line_y - first_line_y);
    }
}

/**
 * gtk_text_layout_validate:
 * @tree: a #GtkTextLayout
 * @max_pixels: the maximum number of pixels to validate. (No more
 *              than one paragraph beyond this limit will be validated)
 * 
 * Validate regions of a #GtkTextLayout. The ::changed signal will
 * be emitted for each region validated.
 **/
void
gtk_text_layout_validate (GtkTextLayout *layout,
			  gint           max_pixels)
{
  gint y, old_height, new_height;

  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  
  while (max_pixels > 0 &&
	 gtk_text_btree_validate (layout->buffer->tree, layout,  max_pixels,
				  &y, &old_height, &new_height))
    {
      max_pixels -= new_height;
      gtk_text_layout_changed (layout, y, old_height, new_height);
    }
}

static GtkTextLineData*
gtk_text_layout_real_wrap (GtkTextLayout   *layout,
			   GtkTextLine     *line,
                            /* may be NULL */
			   GtkTextLineData *line_data)
{
  GtkTextLineDisplay *display;
  
  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), NULL);
  
  if (line_data == NULL)
    {
      line_data = gtk_text_line_data_new (layout, line);
      gtk_text_line_add_data (line, line_data);
    }

  display = gtk_text_layout_get_line_display (layout, line, TRUE);
  line_data->width = display->width;
  line_data->height = display->height;
  line_data->valid = TRUE;
  gtk_text_layout_free_line_display (layout, display);

  return line_data;
}

static void
gtk_text_layout_real_get_log_attrs (GtkTextLayout  *layout,
				    GtkTextLine    *line,
				    PangoLogAttr  **attrs,
				    gint           *n_attrs)
{
  GtkTextLineDisplay *display;
  
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  
  display = gtk_text_layout_get_line_display (layout, line, TRUE);
  pango_layout_get_log_attrs (display->layout, attrs, n_attrs);
  gtk_text_layout_free_line_display (layout, display);
}

/*
 * Layout utility functions
 */

/* If you get the style with get_style () you need to call
   release_style () to free it. */
static GtkTextStyleValues*
get_style (GtkTextLayout *layout,
	   const GtkTextIter *iter)
{
  GtkTextTag** tags;
  gint tag_count = 0;
  GtkTextStyleValues *style;
  
  /* If we have the one-style cache, then it means
     that we haven't seen a toggle since we filled in the
     one-style cache.
  */
  if (layout->one_style_cache != NULL)
    {
      gtk_text_style_values_ref (layout->one_style_cache);
      return layout->one_style_cache;
    }

  g_assert (layout->one_style_cache == NULL);
  
  /* Get the tags at this spot */
  tags = gtk_text_btree_get_tags (iter, &tag_count);

  /* No tags, use default style */
  if (tags == NULL || tag_count == 0)
    {
      /* One ref for the return value, one ref for the
         layout->one_style_cache reference */
      gtk_text_style_values_ref (layout->default_style);
      gtk_text_style_values_ref (layout->default_style);
      layout->one_style_cache = layout->default_style;

      if (tags)
        g_free (tags);

      return layout->default_style;
    }
  
  /* Sort tags in ascending order of priority */
  gtk_text_tag_array_sort (tags, tag_count);
  
  style = gtk_text_style_values_new ();
  
  gtk_text_style_values_copy (layout->default_style,
                              style);

  gtk_text_style_values_fill_from_tags (style,
                                        tags,
                                        tag_count);

  g_free (tags);

  g_assert (style->refcount == 1);

  /* Leave this style as the last one seen */
  g_assert (layout->one_style_cache == NULL);
  gtk_text_style_values_ref (style); /* ref held by layout->one_style_cache */
  layout->one_style_cache = style;
  
  /* Returning yet another refcount */
  return style;
}

static void
release_style (GtkTextLayout *layout,
	       GtkTextStyleValues *style)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->refcount > 0);

  gtk_text_style_values_unref (style);
}

/*
 * Lines
 */

/* This function tries to optimize the case where a line
   is completely invisible */
static gboolean
totally_invisible_line (GtkTextLayout *layout,
			GtkTextLine   *line,
			GtkTextIter   *iter)
{
  GtkTextLineSegment *seg;
  int bytes = 0;
  
  /* If we have a cached style, then we know it does actually apply
     and we can just see if it is elided. */
  if (layout->one_style_cache &&
      !layout->one_style_cache->elide)
    return FALSE;
  /* Without the cache, we check if the first char is visible, if so
     we are partially visible.  Note that we have to check this since
     we don't know the current elided/nonelided toggle state; this
     function can use the whole btree to get it right. */
  else
    {
      gtk_text_btree_get_iter_at_line(layout->buffer->tree, iter, line, 0);
      
      if (!gtk_text_btree_char_is_invisible (iter))
	return FALSE;
    }

  bytes = 0;
  seg = line->segments;

  while (seg != NULL)
    {
      if (seg->byte_count > 0)
        bytes += seg->byte_count;

      /* Note that these two tests can cause us to bail out
         when we shouldn't, because a higher-priority tag
         may override these settings. However the important
         thing is to only elide really-elided lines, rather
         than to elide all really-elided lines. */
      
      else if (seg->type == &gtk_text_toggle_on_type)
        {
          invalidate_cached_style (layout);
          
          /* Bail out if an elision-unsetting tag begins */
          if (seg->body.toggle.info->tag->elide_set &&
              !seg->body.toggle.info->tag->values->elide)
            break;
        }
      else if (seg->type == &gtk_text_toggle_off_type)
        {
          invalidate_cached_style (layout);
          
          /* Bail out if an elision-setting tag ends */
          if (seg->body.toggle.info->tag->elide_set &&
              seg->body.toggle.info->tag->values->elide)
            break;
        }

      seg = seg->next;
    }

  if (seg != NULL)       /* didn't reach line end */
    return FALSE;

  return TRUE;
}

static void
set_para_values (GtkTextLayout      *layout,
		 GtkTextStyleValues *style,
		 GtkTextLineDisplay *display,
		 gdouble            *align)
{
  PangoAlignment pango_align = PANGO_ALIGN_LEFT;
  int layout_width;

  display->direction = style->direction;

  if (display->direction == GTK_TEXT_DIR_LTR)
    display->layout = pango_layout_new (layout->ltr_context);
  else
    display->layout = pango_layout_new (layout->rtl_context);

  switch (style->justify)
    {
    case GTK_JUSTIFY_LEFT:
      pango_align = (style->direction == GTK_TEXT_DIR_LTR) ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT;
      break;
    case GTK_JUSTIFY_RIGHT:
      pango_align = (style->direction == GTK_TEXT_DIR_LTR) ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT;
      break;
    case GTK_JUSTIFY_CENTER:
      pango_align = PANGO_ALIGN_CENTER;
      break;
    case GTK_JUSTIFY_FILL:
      g_warning ("FIXME we don't support GTK_JUSTIFY_FILL yet");
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  switch (pango_align)
    {
      case PANGO_ALIGN_LEFT:
	*align = 0.0;
	break;
      case PANGO_ALIGN_RIGHT:
	*align = 1.0;
	break;
      case PANGO_ALIGN_CENTER:
	*align = 0.5;
	break;
    }

  pango_layout_set_alignment (display->layout, pango_align);
  pango_layout_set_spacing (display->layout, style->pixels_inside_wrap * PANGO_SCALE);

  display->top_margin = style->pixels_above_lines;
  display->height = style->pixels_above_lines + style->pixels_below_lines;
  display->bottom_margin = style->pixels_below_lines;
  display->x_offset = display->left_margin = MIN (style->left_margin, style->left_wrapped_line_margin);
  display->right_margin = style->right_margin;

  pango_layout_set_indent (display->layout, style->left_margin - style->left_wrapped_line_margin);

  switch (style->wrap_mode)
    {
    case GTK_WRAPMODE_CHAR:
      /* FIXME: Handle this; for now, fall-through */
    case GTK_WRAPMODE_WORD:
      display->total_width = -1;
      layout_width = layout->screen_width - display->x_offset - style->right_margin;
      pango_layout_set_width (display->layout, layout_width * PANGO_SCALE);
      break;
    case GTK_WRAPMODE_NONE:
      display->total_width = MAX (layout->screen_width, layout->width) - display->x_offset - style->right_margin;
      break;
    }
}

static PangoAttribute *
gtk_text_attr_appearance_copy (const PangoAttribute *attr)
{
  const GtkTextAttrAppearance *appearance_attr = (const GtkTextAttrAppearance *)attr;
  
  return gtk_text_attr_appearance_new (&appearance_attr->appearance);
}

static void
gtk_text_attr_appearance_destroy (PangoAttribute *attr)
{
  GtkTextAppearance *appearance = &((GtkTextAttrAppearance *)attr)->appearance;

  if (appearance->bg_stipple)
    gdk_drawable_unref (appearance->bg_stipple);
  if (appearance->fg_stipple)
    gdk_drawable_unref (appearance->fg_stipple);
  
  g_free (attr);
}

static gboolean
gtk_text_attr_appearance_compare (const PangoAttribute *attr1,
				  const PangoAttribute *attr2)
{
  const GtkTextAppearance *appearance1 = &((const GtkTextAttrAppearance *)attr1)->appearance;
  const GtkTextAppearance *appearance2 = &((const GtkTextAttrAppearance *)attr2)->appearance;

  return (gdk_color_equal (&appearance1->fg_color, &appearance2->fg_color) &&
	  gdk_color_equal (&appearance1->bg_color, &appearance2->bg_color) &&
	  appearance1->fg_stipple ==  appearance2->fg_stipple &&
	  appearance1->bg_stipple ==  appearance2->bg_stipple &&
	  appearance1->underline == appearance2->underline &&
	  appearance1->overstrike == appearance2->overstrike &&
	  appearance1->draw_bg == appearance2->draw_bg);
	  
}

/**
 * gtk_text_attr_appearance_new:
 * @desc: 
 * 
 * Create a new font description attribute. (This attribute
 * allows setting family, style, weight, variant, stretch,
 * and size simultaneously.)
 * 
 * Return value: 
 **/
static PangoAttribute *
gtk_text_attr_appearance_new (const GtkTextAppearance *appearance)
{
  static PangoAttrClass klass = {
    0,
    gtk_text_attr_appearance_copy,
    gtk_text_attr_appearance_destroy,
    gtk_text_attr_appearance_compare
  };

  GtkTextAttrAppearance *result;

  if (!klass.type)
    klass.type = gtk_text_attr_appearance_type =
      pango_attr_type_register ("GtkTextAttrAppearance");

  result = g_new (GtkTextAttrAppearance, 1);
  result->attr.klass = &klass;

  result->appearance = *appearance;
    
  if (appearance->bg_stipple)
    gdk_drawable_ref (appearance->bg_stipple);
  if (appearance->fg_stipple)
    gdk_drawable_ref (appearance->fg_stipple);

  return (PangoAttribute *)result;
}

static void
add_text_attrs (GtkTextLayout      *layout,
		GtkTextStyleValues *style,
		gint                byte_count,
		PangoAttrList      *attrs,
		gint                start,
		gboolean            size_only)
{
  PangoAttribute *attr;

  attr = pango_attr_font_desc_new (style->font_desc);
  attr->start_index = start;
  attr->end_index = start + byte_count;

  pango_attr_list_insert (attrs, attr);

  if (!size_only)
    {
      attr = gtk_text_attr_appearance_new (&style->appearance);
  
      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr); 
    }
}

static void
add_pixmap_attrs (GtkTextLayout      *layout,
		  GtkTextLineDisplay *display,
		  GtkTextStyleValues *style,
		  GtkTextLineSegment *seg,
		  PangoAttrList      *attrs,
		  gint                start)
{
  PangoAttribute *attr;
  PangoRectangle logical_rect;
  GtkTextPixmap *pixmap = &seg->body.pixmap;
  gint width, height;

  gdk_drawable_get_size (pixmap->pixmap, &width, &height);
  logical_rect.x = 0;
  logical_rect.y = -height * PANGO_SCALE;
  logical_rect.width = width * PANGO_SCALE;
  logical_rect.height = height * PANGO_SCALE;

  attr = pango_attr_shape_new (&logical_rect, &logical_rect);
  attr->start_index = start;
  attr->end_index = start + seg->byte_count;
  pango_attr_list_insert (attrs, attr);

  display->pixmaps = g_slist_append (display->pixmaps, pixmap);
}

static void
add_cursor (GtkTextLayout      *layout,
	    GtkTextLineDisplay *display,
	    GtkTextLineSegment *seg,
	    gint                start)
{
  GtkTextIter selection_start, selection_end;
  
  PangoRectangle strong_pos, weak_pos;
  GtkTextCursorDisplay *cursor;

  /* Hide insertion cursor when we have a selection
   */
  if (gtk_text_btree_mark_is_insert (layout->buffer->tree, (GtkTextMark*)seg) &&
      gtk_text_buffer_get_selection_bounds (layout->buffer, &selection_start, &selection_end))
    return;
  
  pango_layout_get_cursor_pos (display->layout, start, &strong_pos, &weak_pos);
  
  cursor = g_new (GtkTextCursorDisplay, 1);
  
  cursor->x = strong_pos.x / PANGO_SCALE;
  cursor->y = strong_pos.y / PANGO_SCALE;
  cursor->height = strong_pos.height / PANGO_SCALE;
  cursor->is_strong = TRUE;
  display->cursors = g_slist_prepend (display->cursors, cursor);
  
  if (weak_pos.x == strong_pos.x)
    cursor->is_weak = TRUE;
  else
    {
      cursor->is_weak = FALSE;
      
      cursor = g_new (GtkTextCursorDisplay, 1);
      
      cursor->x = weak_pos.x / PANGO_SCALE;
      cursor->y = weak_pos.y / PANGO_SCALE;
      cursor->height = weak_pos.height / PANGO_SCALE;
      cursor->is_strong = FALSE;
      cursor->is_weak = TRUE;
      display->cursors = g_slist_prepend (display->cursors, cursor);
    }
}

GtkTextLineDisplay *
gtk_text_layout_get_line_display (GtkTextLayout *layout,
				  GtkTextLine   *line,
				  gboolean       size_only)
{
  GtkTextLineDisplay *display;
  GtkTextLineSegment *seg;
  GtkTextIter iter;
  GtkTextStyleValues *style;
  gchar *text;
  PangoAttrList *attrs;
  gint byte_count, byte_offset;
  gdouble align;
  PangoRectangle extents;
  gboolean para_values_set = FALSE;
  GSList *cursor_byte_offsets = NULL;
  GSList *cursor_segs = NULL;
  GSList *tmp_list1, *tmp_list2;

  g_return_val_if_fail (line != NULL, NULL);

  if (layout->one_display_cache)
    {
      if (line == layout->one_display_cache->line &&
	  (size_only || !layout->one_display_cache->size_only))
	return layout->one_display_cache;
      else
	{
	  GtkTextLineDisplay *tmp_display = layout->one_display_cache;
	  layout->one_display_cache = NULL;
	  gtk_text_layout_free_line_display (layout, tmp_display);
	}
    }
  
  display = g_new0 (GtkTextLineDisplay, 1);

  display->size_only = size_only;
  display->line = line;

  gtk_text_btree_get_iter_at_line (layout->buffer->tree, &iter, line, 0);
  
  /* Special-case optimization for completely
   * invisible lines; makes it faster to deal
   * with sequences of invisible lines.
   */
  if (totally_invisible_line (layout, line, &iter))
    return display;

  /* Allocate space for flat text for buffer
   */
  byte_count = gtk_text_line_byte_count (line);
  text = g_malloc (byte_count);

  attrs = pango_attr_list_new ();

  /* Iterate over segments, creating display chunks for them. */
  byte_offset = 0;
  seg = gtk_text_iter_get_any_segment (&iter);
  while (seg != NULL)
    {
      /* Displayable segments */
      if (seg->type == &gtk_text_char_type ||
	  seg->type == &gtk_text_pixmap_type)
        {
	  gtk_text_btree_get_iter_at_line (layout->buffer->tree,
					   &iter, line,
					   byte_offset);
	  style = get_style (layout, &iter);
	  
	  /* We have to delay setting the paragraph values until we
	   * hit the first pixmap or text segment because toggles at
	   * the beginning of the paragraph should affect the
	   * paragraph-global values
	   */
	  if (!para_values_set)
	    {
	      set_para_values (layout, style, display, &align);
	      para_values_set = TRUE;
	    }

          /* First see if the chunk is elided, and ignore it if so. Tk
	   * looked at tabs, wrap mode, etc. before doing this, but
	   * that made no sense to me, so I am just skipping the
	   * elided chunks
	   */
          if (!style->elide)
            {
	      if (seg->type == &gtk_text_char_type)
		{
		  /* We don't want to split segments because of marks, so we scan forward
		   * for more segments only separated from us by marks. In theory, we
		   * should also merge segments with identical styles, even if there
		   * are toggles in-between
		   */

		  gint byte_count = 0;

		  while (TRUE)
		    {
		      if (seg->type == &gtk_text_char_type)
			{
			  memcpy (text + byte_offset, seg->body.chars, seg->byte_count);
			  byte_offset += seg->byte_count;
			  byte_count += seg->byte_count;
			}
		      else if (seg->body.mark.visible)
			{
			  cursor_byte_offsets = g_slist_prepend (cursor_byte_offsets, GINT_TO_POINTER (byte_offset));
			  cursor_segs = g_slist_prepend (cursor_segs, seg);
			}

		      if (!seg->next ||
			  (seg->next->type != &gtk_text_right_mark_type &&
			   seg->next->type != &gtk_text_left_mark_type &&
			   seg->next->type != &gtk_text_char_type))
			break;

		      seg = seg->next;
		    }
		  
		  add_text_attrs (layout, style, byte_count, attrs, byte_offset - byte_count, size_only);
		}
	      else
		{
		  add_pixmap_attrs (layout, display, style, seg, attrs, byte_offset);
		  memcpy (text + byte_offset, gtk_text_unknown_char_utf8, seg->byte_count);
		  byte_offset += seg->byte_count;
		}
	    }

	  release_style (layout, style);
        }

      /* Toggles */
      else if (seg->type == &gtk_text_toggle_on_type ||
               seg->type == &gtk_text_toggle_off_type)
        {
          /* Style may have changed, drop our
             current cached style */
          invalidate_cached_style (layout);
        }
      
      /* Marks */
      else if (seg->type == &gtk_text_right_mark_type ||
               seg->type == &gtk_text_left_mark_type)
        {
          /* Display visible marks */

          if (seg->body.mark.visible)
	    {
	      cursor_byte_offsets = g_slist_prepend (cursor_byte_offsets, GINT_TO_POINTER (byte_offset));
	      cursor_segs = g_slist_prepend (cursor_segs, seg);
	    }
        }

      else
        g_error ("Unknown segment type: %s", seg->type->name);

      seg = seg->next;
    }

  if (!para_values_set)
    {
      style = get_style (layout, &iter);
      set_para_values (layout, style, display, &align);
      release_style (layout, style);
    }
	      
  /* Pango doesn't want the trailing new line */
  if (byte_offset > 0 && text[byte_offset - 1] == '\n')
    byte_offset--;
  
  pango_layout_set_text (display->layout, text, byte_offset);
  pango_layout_set_attributes (display->layout, attrs);

  tmp_list1 = cursor_byte_offsets;
  tmp_list2 = cursor_segs;
  while (tmp_list1)
    {
      add_cursor (layout, display, tmp_list2->data, GPOINTER_TO_INT (tmp_list1->data));
      tmp_list1 = tmp_list1->next;
      tmp_list2 = tmp_list2->next;
    }
  g_slist_free (cursor_byte_offsets);
  g_slist_free (cursor_segs);

  pango_layout_get_extents (display->layout, NULL, &extents);

  if (display->total_width >= 0)
    display->x_offset += (display->total_width - extents.width / PANGO_SCALE) * align;

  display->width = extents.width / PANGO_SCALE + display->left_margin + display->right_margin;
  display->height += extents.height / PANGO_SCALE;
 
  /* Free this if we aren't in a loop */
  if (layout->wrap_loop_count == 0)
    invalidate_cached_style (layout);

  g_free (text);
  pango_attr_list_unref (attrs);

  layout->one_display_cache = display;
  
  return display;
}

void
gtk_text_layout_free_line_display (GtkTextLayout      *layout,
				   GtkTextLineDisplay *display)
{
  if (display != layout->one_display_cache)
    {
      g_object_unref (G_OBJECT (display->layout));

      if (display->cursors)
	{
	  g_slist_foreach (display->cursors, (GFunc)g_free, NULL);
	  g_slist_free (display->cursors);
	  g_slist_free (display->pixmaps);
	}
      
      g_free (display);
    }
}

/* FIXME: This really doesn't belong in this file ... */
static GtkTextLineData*
gtk_text_line_data_new (GtkTextLayout *layout,
			GtkTextLine   *line)
{
  GtkTextLineData *line_data;

  line_data = g_new (GtkTextLineData, 1);

  line_data->view_id = layout;
  line_data->next = NULL;
  line_data->width = 0;
  line_data->height = 0;
  line_data->valid = FALSE;
  
  return line_data;
}

static void
get_line_at_y (GtkTextLayout *layout,
	       gint           y,
	       GtkTextLine  **line,
	       gint          *line_top)
{
  if (y < 0)
    y = 0;
  if (y > layout->height)
    y = layout->height;
  
  *line = gtk_text_btree_find_line_by_y (layout->buffer->tree, layout, y, line_top);
  if (*line == NULL)
    {
      *line = gtk_text_btree_get_line (layout->buffer->tree,
				       gtk_text_btree_line_count (layout->buffer->tree) - 1, NULL);
      if (line_top)
	*line_top = gtk_text_btree_find_line_top (layout->buffer->tree, *line, layout);      
    }
}

/**
 * gtk_text_layout_get_line_at_y:
 * @layout: a #GtkLayout
 * @target_iter: the iterator in which the result is stored
 * @y: the y positition
 * @line_top: location to store the y coordinate of the
 *            top of the line. (Can by %NULL.)
 * 
 * Get the iter at the beginning of the line which is displayed
 * at the given y.
 **/
void
gtk_text_layout_get_line_at_y (GtkTextLayout *layout,
			       GtkTextIter   *target_iter,
			       gint           y,
			       gint          *line_top)
{
  GtkTextLine *line;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (target_iter != NULL);

  get_line_at_y (layout, y, &line, line_top);
  gtk_text_btree_get_iter_at_line (layout->buffer->tree, target_iter, line, 0);
}

void
gtk_text_layout_get_iter_at_pixel (GtkTextLayout *layout,
                                   GtkTextIter *target_iter,
                                   gint x, gint y)
{
  GtkTextLine *line;
  gint byte_index, trailing;
  gint line_top;
  GtkTextLineDisplay *display;
  
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (target_iter != NULL);
  
  /* Adjust pixels to be on-screen. This gives nice
     behavior if the user is dragging with a pointer grab.
  */
  if (x < 0)
    x = 0;
  if (x > layout->width)
    x = layout->width;

  get_line_at_y (layout, y, &line, &line_top);

  display = gtk_text_layout_get_line_display (layout, line, FALSE);

  x -= display->x_offset;
  y -= line_top + display->top_margin;

  /* We clamp y to the area of the actual layout so that the layouts
   * hit testing works OK on the space above and below the layout
   */
  y = CLAMP (y, display->top_margin, display->height - display->top_margin - display->bottom_margin - 1);
  
  if (!pango_layout_xy_to_index (display->layout, x * PANGO_SCALE, y * PANGO_SCALE,
				 &byte_index, &trailing))
    {
      byte_index = gtk_text_line_byte_count (line);
      trailing = 0;
    }

  gtk_text_btree_get_iter_at_line (layout->buffer->tree,
                                   target_iter,
                                   line, byte_index);

  while (trailing--)
    gtk_text_iter_forward_char (target_iter);
  
  gtk_text_layout_free_line_display (layout, display);
}

/**
 * gtk_text_layout_get_cursor_locations
 * @layout: a #GtkTextLayout
 * @iter: a #GtkTextIter
 * @strong_pos: location to store the strong cursor position (may be %NULL)
 * @weak_pos: location to store the weak cursor position (may be %NULL)
 * 
 * Given an iterator within a text laout, determine the positions that of the
 * strong and weak cursors if the insertion point is at that
 * iterator. The position of each cursor is stored as a zero-width
 * rectangle. The strong cursor location is the location where
 * characters of the directionality equal to the base direction of the
 * paragraph are inserted.  The weak cursor location is the location
 * where characters of the directionality opposite to the base
 * direction of the paragraph are inserted.
 **/
void
gtk_text_layout_get_cursor_locations (GtkTextLayout  *layout,
				      GtkTextIter    *iter,
				      GdkRectangle   *strong_pos,
				      GdkRectangle   *weak_pos)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  gint line_top;

  PangoRectangle pango_strong_pos;
  PangoRectangle pango_weak_pos;

  g_return_if_fail (layout != NULL);
  g_return_if_fail (iter != NULL);
  
  line = gtk_text_iter_get_line (iter);
  line_top = gtk_text_btree_find_line_top (layout->buffer->tree, line, layout);

  display = gtk_text_layout_get_line_display (layout, line, FALSE);

  pango_layout_get_cursor_pos (display->layout, gtk_text_iter_get_line_byte (iter),
			       strong_pos ? &pango_strong_pos : NULL,
			       weak_pos ? &pango_weak_pos : NULL);

  if (strong_pos)
    {
      strong_pos->x = display->x_offset + pango_strong_pos.x / PANGO_SCALE;
      strong_pos->y = line_top + display->top_margin + pango_strong_pos.y / PANGO_SCALE;
      strong_pos->width = 0;
      strong_pos->height = pango_strong_pos.height / PANGO_SCALE;
    }

  if (weak_pos)
    {
      weak_pos->x = display->x_offset + pango_weak_pos.x / PANGO_SCALE;
      weak_pos->y = line_top + display->top_margin + pango_weak_pos.y / PANGO_SCALE;
      weak_pos->width = 0;
      weak_pos->height = pango_weak_pos.height / PANGO_SCALE;
    }

  gtk_text_layout_free_line_display (layout, display);
}

/**
 * gtk_text_layout_get_line_y:
 * @layout: a #GtkTextLayout
 * @iter:   a #GtkTextIter
 * 
 * Find the y coordinate of the top of the paragraph containing
 * the given iter.
 * 
 * Return value: the y coordinate, in pixels.
 **/
gint
gtk_text_layout_get_line_y (GtkTextLayout     *layout,
			    const GtkTextIter *iter)
{
  GtkTextLine *line;
  
  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), 0);
  g_return_val_if_fail (gtk_text_iter_get_btree (iter) == layout->buffer->tree, 0);
  
  line = gtk_text_iter_get_line (iter);
  return gtk_text_btree_find_line_top (layout->buffer->tree, line, layout);
}

void
gtk_text_layout_get_iter_location (GtkTextLayout     *layout,
                                   const GtkTextIter *iter,
                                   GdkRectangle      *rect)
{
  PangoRectangle pango_rect;
  GtkTextLine *line;
  GtkTextBTree *tree;
  GtkTextLineDisplay *display;
  gint byte_index;
  
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (gtk_text_iter_get_btree (iter) == layout->buffer->tree);
  g_return_if_fail (rect != NULL);
  
  tree = gtk_text_iter_get_btree (iter);
  line = gtk_text_iter_get_line (iter);

  display = gtk_text_layout_get_line_display (layout, line, FALSE);

  rect->y = gtk_text_btree_find_line_top (tree, line, layout);

  /* pango_layout_index_to_pos() expects the index of a character within the layout,
   * so we have to special case the last character. FIXME: This should be moved
   * to Pango.
   */
  if (gtk_text_iter_ends_line (iter))
    {
      PangoLayoutLine *last_line = g_slist_last (pango_layout_get_lines (display->layout))->data;

      pango_layout_line_get_extents (last_line, NULL, &pango_rect);
      
      rect->x = display->x_offset + (pango_rect.x + pango_rect.width) / PANGO_SCALE;
      rect->y += display->top_margin;
      rect->width = 0;
      rect->height = pango_rect.height / PANGO_SCALE;
    }
  else
    {
      byte_index = gtk_text_iter_get_line_byte (iter);
  
      pango_layout_index_to_pos (display->layout, byte_index, &pango_rect);
      
      rect->x = display->x_offset + pango_rect.x / PANGO_SCALE;
      rect->y += display->top_margin;
      rect->width = pango_rect.width / PANGO_SCALE;
      rect->height = pango_rect.height / PANGO_SCALE;
    }
  
  gtk_text_layout_free_line_display (layout, display);
}

/* Find the iter for the logical beginning of the first display line whose
 * top y is >= y. If none exists, move the iter to the logical beginning
 * of the last line in the buffer.
 */
static void
find_display_line_below (GtkTextLayout *layout,
			 GtkTextIter   *iter,
			 gint           y)
{
  GtkTextLine *line, *next;
  GtkTextLine *found_line = NULL;
  gint line_top;
  gint found_byte = 0;
  
  line = gtk_text_btree_find_line_by_y (layout->buffer->tree, layout, y, &line_top);
  if (!line)
    {
      line = gtk_text_btree_get_line (layout->buffer->tree,
				      gtk_text_btree_line_count (layout->buffer->tree) - 1, NULL);
      line_top = gtk_text_btree_find_line_top (layout->buffer->tree, line, layout);
    }

  while (line && !found_line)
    {
      GtkTextLineDisplay *display = gtk_text_layout_get_line_display (layout, line, FALSE);
      gint byte_index = 0;
      GSList *tmp_list =  pango_layout_get_lines (display->layout);

      line_top += display->top_margin;
      
      while (tmp_list)
	{
	  PangoRectangle logical_rect;
	  PangoLayoutLine *layout_line = tmp_list->data;

	  found_byte = byte_index;
	  
	  if (line_top >= y)
	    {
	      found_line = line;
	      break;
	    }
	  
	  pango_layout_line_get_extents (layout_line, NULL, &logical_rect);
	  line_top += logical_rect.height / PANGO_SCALE;
	  
	  tmp_list = tmp_list->next;
	}

      line_top += display->bottom_margin;
      gtk_text_layout_free_line_display (layout, display);

      next = gtk_text_line_next (line);
      if (!next)
	found_line = line;

      line = next;
    }
  
  gtk_text_btree_get_iter_at_line (layout->buffer->tree, iter, found_line, found_byte);
}
  
/* Find the iter for the logical beginning of the last display line whose
 * top y is >= y. If none exists, move the iter to the logical beginning
 * of the first line in the buffer.
 */
static void
find_display_line_above (GtkTextLayout *layout,
			 GtkTextIter   *iter,
			 gint           y)
{
  GtkTextLine *line;
  GtkTextLine *found_line = NULL;
  gint line_top;
  gint found_byte = 0;
  
  line = gtk_text_btree_find_line_by_y (layout->buffer->tree, layout, y, &line_top);
  if (!line)
    {
      line = gtk_text_btree_get_line (layout->buffer->tree,
				      gtk_text_btree_line_count (layout->buffer->tree) - 1, NULL);
      line_top = gtk_text_btree_find_line_top (layout->buffer->tree, line, layout);
    }

  while (line && !found_line)
    {
      GtkTextLineDisplay *display = gtk_text_layout_get_line_display (layout, line, FALSE);
      PangoRectangle logical_rect;
      
      gint byte_index = 0;
      GSList *tmp_list;
      gint tmp_top;

      line_top -= display->top_margin + display->bottom_margin;
      pango_layout_get_extents (display->layout, NULL, &logical_rect);
      line_top -= logical_rect.height / PANGO_SCALE;
	  
      tmp_top = line_top + display->top_margin;

      tmp_list =  pango_layout_get_lines (display->layout);
      while (tmp_list)
	{
	  PangoLayoutLine *layout_line = tmp_list->data;

	  found_byte = byte_index;
	  
	  tmp_top += logical_rect.height / PANGO_SCALE;

	  if (tmp_top < y)
	    {
	      found_line = line;
	      found_byte = byte_index;
	    }
	  
	  pango_layout_line_get_extents (layout_line, NULL, &logical_rect);
	  
	  tmp_list = tmp_list->next;
	}

      gtk_text_layout_free_line_display (layout, display);
      
      line = gtk_text_line_previous (line);
    }

  if (found_line)
    gtk_text_btree_get_iter_at_line (layout->buffer->tree, iter, found_line, found_byte);
  else
    gtk_text_buffer_get_iter_at_char (layout->buffer, iter, 0);
}
  
/**
 * gtk_text_layout_clamp_iter_to_vrange:
 * @layout: a #GtkTextLayout
 * @iter:   a #GtkTextIter
 * @top:    the top of the range
 * @bottom: the bottom the range
 * 
 * If the iterator is not fully in the range @top <= y < @bottom,
 * then, if possible, move it the minimum distance so that the
 * iterator in this range.
 *
 * Returns: %TRUE if the iterator was moved, otherwise %FALSE.
 **/
gboolean
gtk_text_layout_clamp_iter_to_vrange (GtkTextLayout *layout,
				      GtkTextIter   *iter,
				      gint           top,
				      gint           bottom)
{
  GdkRectangle iter_rect;

  gtk_text_layout_get_iter_location (layout, iter, &iter_rect);

  /* If the iter is at least partially above the range, put the iter
   * at the first fully visible line after the range.
   */
  if (iter_rect.y < top)
    {
      find_display_line_below (layout, iter, top);
      
      return TRUE;
    }
  /* Otherwise, if the iter is at least partially below the screen, put the
   * iter on the last logical position of the last completely visible
   * line on screen
   */
  else if (iter_rect.y + iter_rect.height > bottom)
    {
      find_display_line_above (layout, iter, bottom);
      
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_text_layout_move_iter_to_next_line:
 * @layout: a #GtkLayout
 * @iter:   a #GtkTextIter
 * 
 * Move the iterator to the beginning of the previous line. The lines
 * of a wrapped paragraph are treated as distinct for this operation.
 **/
void
gtk_text_layout_move_iter_to_previous_line (GtkTextLayout *layout,
					    GtkTextIter   *iter)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  gint line_byte;
  GSList *tmp_list;
  PangoLayoutLine *layout_line;

  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (iter != NULL);
  
  line = gtk_text_iter_get_line (iter);
  line_byte = gtk_text_iter_get_line_byte (iter);
  
  display = gtk_text_layout_get_line_display (layout, line, FALSE);

  tmp_list = pango_layout_get_lines (display->layout);
  layout_line = tmp_list->data;

  if (line_byte < layout_line->length || !tmp_list->next) /* first line of paragraph */
    {
      GtkTextLine *prev_line = gtk_text_line_previous (line);

      if (prev_line)
	{
	  gint byte_offset = 0;
	  
	  gtk_text_layout_free_line_display (layout, display);
	  display = gtk_text_layout_get_line_display (layout, prev_line, FALSE);
	  
	  tmp_list =  pango_layout_get_lines (display->layout);

	  while (tmp_list->next)
	    {
	      layout_line = tmp_list->data;
	      tmp_list = tmp_list->next;

	      byte_offset += layout_line->length;
	    }

	  gtk_text_btree_get_iter_at_line (layout->buffer->tree,
					   iter, prev_line, byte_offset);
	}
      else
	gtk_text_btree_get_iter_at_line (layout->buffer->tree,
					 iter, line, 0);
    }
  else
    {
      gint prev_offset = 0;
      gint byte_offset = layout_line->length;

      tmp_list = tmp_list->next;
      while (tmp_list)
	{
	  layout_line = tmp_list->data;

	  if (line_byte < byte_offset + layout_line->length || !tmp_list->next)
	    {
	      gtk_text_btree_get_iter_at_line (layout->buffer->tree,
					       iter, line, prev_offset);
	      break;
	    }
	  
	  prev_offset = byte_offset;
	  byte_offset += layout_line->length;
	  tmp_list = tmp_list->next;
	}
    }

  gtk_text_layout_free_line_display (layout, display);
}

/**
 * gtk_text_layout_move_iter_to_next_line:
 * @layout: a #GtkLayout
 * @iter:   a #GtkTextIter
 * 
 * Move the iterator to the beginning of the next line. The
 * lines of a wrapped paragraph are treated as distinct for
 * this operation.
 **/
void
gtk_text_layout_move_iter_to_next_line (GtkTextLayout *layout,
					GtkTextIter   *iter)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  gint line_byte;
  
  gboolean found = FALSE;
  gboolean found_after = FALSE;
  
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (iter != NULL);
  
  line = gtk_text_iter_get_line (iter);
  line_byte = gtk_text_iter_get_line_byte (iter);
  
  while (line && !found_after)
    {
      gint byte_offset = 0;
      GSList *tmp_list;
      
      display = gtk_text_layout_get_line_display (layout, line, FALSE);

      tmp_list = pango_layout_get_lines (display->layout);
      while (tmp_list && !found_after)
	{
	  PangoLayoutLine *layout_line = tmp_list->data;

	  if (found)
	    {
	      gtk_text_btree_get_iter_at_line (layout->buffer->tree,
					       iter, line,
					       byte_offset);
	      found_after = TRUE;
	    }
	  else if (line_byte < byte_offset + layout_line->length || !tmp_list->next)
	    found = TRUE;

	  byte_offset += layout_line->length;
	  tmp_list = tmp_list->next;
	}
      
      gtk_text_layout_free_line_display (layout, display);

      line = gtk_text_line_next (line);
    }
}

/**
 * gtk_text_layout_move_iter_to_x:
 * @layout: a #GtkTextLayout
 * @iter:   a #GtkTextIter
 * @x:      X coordinate
 *
 * Keeping the iterator on the same line of the layout, move it to the
 * specified X coordinate. The lines of a wrapped paragraph are
 * treated as distinct for this operation.
 **/
void
gtk_text_layout_move_iter_to_x (GtkTextLayout *layout,
				GtkTextIter   *iter,
				gint           x)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  gint line_byte;
  gint byte_offset = 0;
  GSList *tmp_list;
  
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (iter != NULL);
  
  line = gtk_text_iter_get_line (iter);
  line_byte = gtk_text_iter_get_line_byte (iter);
  
  display = gtk_text_layout_get_line_display (layout, line, FALSE);

  tmp_list = pango_layout_get_lines (display->layout);
  while (tmp_list)
    {
      PangoLayoutLine *layout_line = tmp_list->data;

      if (line_byte < byte_offset + layout_line->length || !tmp_list->next)
	{
	  PangoRectangle logical_rect;
	  gint byte_index, trailing;
	  gint align = pango_layout_get_alignment (display->layout);
	  gint x_offset = display->left_margin * PANGO_SCALE;
	  gint width = pango_layout_get_width (display->layout);

	  if (width < 0)
	    width = display->total_width * PANGO_SCALE;

	  pango_layout_line_get_extents (layout_line, NULL, &logical_rect);

	  switch (align)
	    {
	    case PANGO_ALIGN_RIGHT:
	      x_offset += width - logical_rect.width;
	      break;
	    case PANGO_ALIGN_CENTER:
	      x_offset += (width - logical_rect.width) / 2;
	      break;
	    default:
	      break;
	    }

	  pango_layout_line_x_to_index (layout_line,
					x * PANGO_SCALE - x_offset,
					&byte_index, &trailing);

	  gtk_text_btree_get_iter_at_line (layout->buffer->tree,
					   iter,
					   line, byte_index);
	  
	  while (trailing--)
	    gtk_text_iter_forward_char (iter);

	  break;
	}
      
      byte_offset += layout_line->length;
      tmp_list = tmp_list->next;
    }
  
  gtk_text_layout_free_line_display (layout, display);
}

/**
 * gtk_text_layout_move_iter_visually:
 * @layout:  a #GtkTextLayout
 * @iter:    a #GtkTextIter
 * @count:   number of characters to move (negative moves left, positive moves right)
 * 
 * Move the iterator a given number of characters visually, treating
 * it as the strong cursor position. If @count is positive, then the
 * new strong cursor position will be @count positions to the right of
 * the old cursor position. If @count is negative then the new strong
 * cursor position will be @count positions to the left of the old
 * cursor position.
 *
 * In the presence of bidirection text, the correspondence
 * between logical and visual order will depend on the direction
 * of the current run, and there may be jumps when the cursor
 * is moved off of the end of a run.
 **/
 
void
gtk_text_layout_move_iter_visually (GtkTextLayout *layout,
				    GtkTextIter   *iter,
				    gint           count)
{
  g_return_if_fail (layout != NULL);
  g_return_if_fail (iter != NULL);
  
  while (count != 0)
    {
      GtkTextLine *line = gtk_text_iter_get_line (iter);
      gint line_byte = gtk_text_iter_get_line_byte (iter);
      GtkTextLineDisplay *display = gtk_text_layout_get_line_display (layout, line, FALSE);
      int byte_count = gtk_text_line_byte_count (line);
      
      int new_index;
      int new_trailing;
      
      if (count > 0)
	{
	  pango_layout_move_cursor_visually (display->layout, line_byte, 0, 1, &new_index, &new_trailing);
	  count--;
	}
      else
	{
	  pango_layout_move_cursor_visually (display->layout, line_byte, 0, -1, &new_index, &new_trailing);
	  count++;
	}

      gtk_text_layout_free_line_display (layout, display);

      if (new_index < 0)
	{
	  line = gtk_text_line_previous (line);
	  if (!line)
	    return;
	  
	  new_index = gtk_text_line_byte_count (line);
	  
	}
      else if (new_index > byte_count)
	{
	  line = gtk_text_line_next (line);
	  if (!line)
	    return;
	  
	  new_index = 0;
	}

      gtk_text_btree_get_iter_at_line (layout->buffer->tree,
				       iter,
				       line, new_index);
      while (new_trailing--)
	gtk_text_iter_forward_char (iter);
    }

}

typedef void (*GtkSignal_NONE__INT_INT_INT_INT) (GtkObject  *object,
                                                 gint x, gint y,
                                                 gint width, gint height,
                                                 gpointer user_data);

void
gtk_marshal_NONE__INT_INT_INT_INT (GtkObject  *object,
                                   GtkSignalFunc func,
                                   gpointer func_data,
                                   GtkArg  *args)
{
  GtkSignal_NONE__INT_INT_INT_INT rfunc;

  rfunc = (GtkSignal_NONE__INT_INT_INT_INT) func;
  (*rfunc) (object,
            GTK_VALUE_INT (args[0]),
            GTK_VALUE_INT (args[1]),
            GTK_VALUE_INT (args[2]),
            GTK_VALUE_INT (args[3]),
            func_data);
}

void
gtk_text_layout_spew (GtkTextLayout *layout)
{
#if 0
  GtkTextDisplayLine *iter;
  guint wrapped = 0;
  guint paragraphs = 0;
  GtkTextLine *last_line = NULL;
  
  iter = layout->line_list;
   while (iter != NULL)
    {
      if (iter->line != last_line)
        {
          printf ("%5u  paragraph (%p)\n", paragraphs, iter->line);
          ++paragraphs;
          last_line = iter->line;
        }
      
      printf ("  %5u  y: %d len: %d start: %d bytes: %d\n",
             wrapped, iter->y, iter->length, iter->byte_offset,
             iter->byte_count);

      ++wrapped;
      iter = iter->next;
    }

  printf ("Layout %s recompute\n",
         layout->need_recompute ? "needs" : "doesn't need");

  printf ("Layout pars: %u lines: %u size: %d x %d Screen width: %d\n",
         paragraphs, wrapped, layout->width,
         layout->height, layout->screen_width);
#endif
}


