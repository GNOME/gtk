/* gtktextlayout.c - calculate the layout of the text
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000 Red Hat, Inc.
 * Tk->Gtk port by Havoc Pennington
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

#include "gtktextlayout.h"
#include "gtktextbtree.h"
#include "gtktextiterprivate.h"

#include <stdlib.h>

static GtkTextLineData    *gtk_text_line_data_new                 (GtkTextLayout     *layout,
								   GtkTextLine       *line);

static GtkTextLineData *gtk_text_layout_real_wrap (GtkTextLayout *layout,
                                                     GtkTextLine *line,
                                                     /* may be NULL */
                                                     GtkTextLineData *line_data);

static void gtk_text_layout_real_invalidate (GtkTextLayout *layout,
                                             const GtkTextIter *start,
                                             const GtkTextIter *end);

static void line_data_destructor (gpointer data);

static void gtk_text_layout_invalidate_all (GtkTextLayout *layout);

static PangoAttribute *gtk_text_attr_appearance_new (const GtkTextAppearance *appearance);

enum {
  NEED_REPAINT,
  LAST_SIGNAL
};

enum {
  ARG_0,
  LAST_ARG
};

static void gtk_text_layout_init (GtkTextLayout *text_layout);
static void gtk_text_layout_class_init (GtkTextLayoutClass *klass);
static void gtk_text_layout_destroy (GtkObject *object);
static void gtk_text_layout_finalize (GtkObject *object);

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
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) klass;

  parent_class = gtk_type_class (GTK_TYPE_OBJECT);

  signals[NEED_REPAINT] =
    gtk_signal_new ("need_repaint",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkTextLayoutClass, need_repaint),
                    gtk_marshal_NONE__INT_INT_INT_INT,
                    GTK_TYPE_NONE,
                    4,
                    GTK_TYPE_INT,
                    GTK_TYPE_INT,
                    GTK_TYPE_INT,
                    GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
  
  object_class->destroy = gtk_text_layout_destroy;
  object_class->finalize = gtk_text_layout_finalize;

  klass->wrap = gtk_text_layout_real_wrap;
  klass->invalidate = gtk_text_layout_real_invalidate;
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
      gtk_text_view_style_values_unref (text_layout->one_style_cache);
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
    gtk_text_view_style_values_unref (layout->default_style);
  layout->default_style = NULL;
  
  (* parent_class->destroy) (object);
}

static void
gtk_text_layout_finalize (GtkObject *object)
{
  GtkTextLayout *text_layout;

  text_layout = GTK_TEXT_LAYOUT (object);

  (* parent_class->finalize) (object);
}

void
gtk_text_layout_set_buffer (GtkTextLayout *layout,
                            GtkTextBuffer *buffer)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (buffer == NULL || GTK_IS_TEXT_VIEW_BUFFER (buffer));
  
  if (layout->buffer == buffer)
    return;

  free_style_cache (layout);
  
  if (layout->buffer)
    {
      gtk_text_btree_remove_view (buffer->tree, layout);
      
      gtk_object_unref (GTK_OBJECT (layout->buffer));
      layout->buffer = NULL;
    }

  if (buffer)
    {
      layout->buffer = buffer;

      gtk_object_sink (GTK_OBJECT (buffer));
      gtk_object_ref (GTK_OBJECT (buffer));

      gtk_text_btree_add_view (buffer->tree, layout, line_data_destructor);
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

  gtk_text_view_style_values_ref (values);
  
  if (layout->default_style)
    gtk_text_view_style_values_unref (layout->default_style);

  layout->default_style = values;

  gtk_text_layout_default_style_changed (layout);
}

void
gtk_text_layout_set_context (GtkTextLayout *layout,
			     PangoContext  *context)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  if (layout->context)
    pango_context_unref (context);

  layout->context = context;
  pango_context_ref (context);
  
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

void
gtk_text_layout_need_repaint (GtkTextLayout *layout,
                              gint x, gint y,
                              gint width, gint height)
{
  gtk_signal_emit (GTK_OBJECT (layout), signals[NEED_REPAINT],
                  x, y, width, height);
}

void
gtk_text_layout_invalidate (GtkTextLayout *layout,
                            const GtkTextIter *start_index,
                            const GtkTextIter *end_index)
{
  (* GTK_TEXT_LAYOUT_CLASS (GTK_OBJECT (layout)->klass)->invalidate)
    (layout, start_index, end_index);
}

GtkTextLineData*
gtk_text_layout_wrap (GtkTextLayout *layout,
                       GtkTextLine  *line,
                       /* may be NULL */
                       GtkTextLineData *line_data)
{
  return (* GTK_TEXT_LAYOUT_CLASS (GTK_OBJECT (layout)->klass)->wrap) (layout, line, line_data);
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
  
  /* If the end is at byte 0 of a line we don't actually need to nuke
     last_line, but for now we just nuke it anyway. */
  last_line = gtk_text_iter_get_line (end);
  
  line = gtk_text_iter_get_line (start);

  while (TRUE)
    {
      GtkTextLineData *line_data = gtk_text_line_get_data (line, layout);

      if (line_data)
	gtk_text_line_invalidate_wrap (line, line_data);
      
      if (line == last_line)
        break;
      
      line = gtk_text_line_next (line);
    }
  
  /* FIXME yeah we could probably do a bit better here */
  gtk_text_layout_need_repaint (layout, 0, 0,
                                layout->width, layout->height);
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

  display = gtk_text_layout_get_line_display (layout, line);
  line_data->width = display->width;
  line_data->height = display->height;
  gtk_text_layout_free_line_display (layout, line, display);

  return line_data;
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
      gtk_text_view_style_values_ref (layout->one_style_cache);
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
      gtk_text_view_style_values_ref (layout->default_style);
      gtk_text_view_style_values_ref (layout->default_style);
      layout->one_style_cache = layout->default_style;

      if (tags)
        g_free (tags);

      return layout->default_style;
    }
  
  /* Sort tags in ascending order of priority */
  gtk_text_tag_array_sort (tags, tag_count);
  
  style = gtk_text_view_style_values_new ();
  
  gtk_text_view_style_values_copy (layout->default_style,
				   style);

  gtk_text_view_style_values_fill_from_tags (style,
					     tags,
					     tag_count);

  g_free (tags);

  g_assert (style->refcount == 1);

  /* Leave this style as the last one seen */
  g_assert (layout->one_style_cache == NULL);
  gtk_text_view_style_values_ref (style); /* ref held by layout->one_style_cache */
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

  gtk_text_view_style_values_unref (style);
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
      
      else if (seg->type == &gtk_text_view_toggle_on_type)
        {
          invalidate_cached_style (layout);
          
          /* Bail out if an elision-unsetting tag begins */
          if (seg->body.toggle.info->tag->elide_set &&
              !seg->body.toggle.info->tag->values->elide)
            break;
        }
      else if (seg->type == &gtk_text_view_toggle_off_type)
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
		 gint               *total_width,
		 gdouble            *align)
{
  PangoAlignment pango_align;
  
  switch (style->justify)
    {
    case GTK_JUSTIFY_LEFT:
      pango_align = PANGO_ALIGN_LEFT;
      *align = 0.0;
      break;
    case GTK_JUSTIFY_RIGHT:
      pango_align = PANGO_ALIGN_RIGHT;
      *align = 1.0;
      break;
    case GTK_JUSTIFY_CENTER:
      pango_align = PANGO_ALIGN_CENTER;
      *align = 0.5;
      break;
    case GTK_JUSTIFY_FILL:
      g_warning ("FIXME we don't support GTK_JUSTIFY_FILL yet");
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  pango_layout_set_alignment (display->layout, *align);
  pango_layout_set_spacing (display->layout, style->pixels_inside_wrap * PANGO_SCALE);

  display->top_margin = style->pixels_above_lines;
  display->height = style->pixels_above_lines + style->pixels_below_lines;
  display->bottom_margin = style->pixels_below_lines;

  pango_layout_set_indent (display->layout, style->left_wrapped_line_margin - style->left_margin);
  display->x_offset = MIN (style->left_margin, style->left_wrapped_line_margin);
  
  switch (style->wrap_mode)
    {
    case GTK_WRAPMODE_CHAR:
      /* FIXME: Handle this; for now, fall-through */
    case GTK_WRAPMODE_WORD:
      *total_width = layout->width - MIN (style->left_margin, style->left_wrapped_line_margin) - style->right_margin;
      pango_layout_set_width (display->layout, *total_width * PANGO_SCALE);
      break;
    case GTK_WRAPMODE_NONE:
      *total_width = -1;
      break;
    }
  
  /* Unhandled:
   * left_margin
   * left_wrapped_line_margin
   * right_margin
  */
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
		GtkTextLineSegment *seg,
		gboolean            inside_selection,
		PangoAttrList      *attrs,
		gint                start)
{
  PangoAttribute *attr;

  attr = pango_attr_font_desc_new (style->font_desc);
  attr->start_index = start;
  attr->end_index = start + seg->byte_count;

  pango_attr_list_insert (attrs, attr);

  attr = gtk_text_attr_appearance_new (&style->appearance);
  ((GtkTextAttrAppearance *)attr)->appearance.inside_selection = inside_selection;
  
  attr->start_index = start;
  attr->end_index = start + seg->byte_count;

  pango_attr_list_insert (attrs, attr);
}

static void
add_pixmap_attrs (GtkTextLayout      *layout,
		  GtkTextStyleValues *style,
		  GtkTextLineSegment *seg,
		  gboolean            inside_selection,
		  PangoAttrList      *attrs,
		  gint                start)
{
}

static void
add_cursor (GtkTextLineDisplay *display,
	    gint                start)
{
  PangoRectangle strong_pos, weak_pos;
  GtkTextCursorDisplay *cursor;

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
				  GtkTextLine   *line)
{
  GtkTextLineDisplay *display;
  GtkTextLineSegment *seg;
  GtkTextIter iter;
  GtkTextStyleValues *style;
  gchar *text;
  PangoAttrList *attrs;
  gint byte_count, byte_offset;
  gint total_width;
  gdouble align;
  PangoRectangle extents;
  gboolean inside_selection = FALSE;
  gboolean have_selection = FALSE;
  GSList *cursor_byte_offsets = NULL;
  GSList *tmp_list;

  GtkTextIter selection_start;
  GtkTextIter selection_end;
  
  g_return_val_if_fail (line != NULL, NULL);

  display = g_new0 (GtkTextLineDisplay, 1);
  
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

  display->layout = pango_layout_new (layout->context);

  style = get_style (layout, &iter);
  set_para_values (layout, style, display, &total_width, &align);
  
  /* Selection handling */
  if (gtk_text_buffer_get_selection_bounds (layout->buffer,
                                            &selection_start,
                                            &selection_end))
    {
      if (gtk_text_iter_compare (&iter, &selection_start) >= 0 &&
          gtk_text_iter_compare (&iter, &selection_end) < 0)
	{
	  inside_selection = TRUE;
	}

      have_selection = TRUE;
    }
  
  /* Iterate over segments, creating display chunks for them. */
  byte_offset = 0;
  seg = gtk_text_iter_get_any_segment (&iter);
  while (seg != NULL)
    {
      /* Displayable segments */
      if (seg->type == &gtk_text_view_char_type ||
	  seg->type == &gtk_text_pixmap_type)
        {
          if (!style)
	    {
	      gtk_text_btree_get_iter_at_line (layout->buffer->tree,
					       &iter, line,
					       byte_offset);
	      style = get_style (layout, &iter);
	    }

          /* First see if the chunk is elided, and ignore it if so. Tk
	   * looked at tabs, wrap mode, etc. before doing this, but
	   * that made no sense to me, so I am just skipping the
	   * elided chunks
	   */
          if (!style->elide)
            {
	      const char *chars;
	      
	      if (seg->type == &gtk_text_view_char_type)
		{
		  chars = seg->body.chars;
		  add_text_attrs (layout, style, seg, inside_selection, attrs, byte_offset);
		}
	      else
		{
		  chars = gtk_text_unknown_char_utf8;
		  add_pixmap_attrs (layout, style, seg, inside_selection, attrs, byte_offset);
		}
		
              memcpy (text + byte_offset, chars, seg->byte_count);
	      byte_offset += seg->byte_count;
	    }

        }

      /* Toggles */
      else if (seg->type == &gtk_text_view_toggle_on_type ||
               seg->type == &gtk_text_view_toggle_off_type)
        {
          /* Style may have changed, drop our
             current cached style */
          invalidate_cached_style (layout);
        }
      
      /* Marks */
      else if (seg->type == &gtk_text_view_right_mark_type ||
               seg->type == &gtk_text_view_left_mark_type)
        {
	  /* Handle selection start/end */

	  if (inside_selection || have_selection)
	    {
	      gtk_text_btree_get_iter_at_line (layout->buffer->tree,
					       &iter, line,
					       byte_offset);
	      if (inside_selection)
		{
		  if (gtk_text_iter_equal (&iter, &selection_end))
		    {
		      inside_selection = FALSE;
		      have_selection = FALSE;
		    }
		}
	      else if (have_selection)
		{
		  if (gtk_text_iter_equal (&iter, &selection_start))
		    inside_selection = TRUE;
		}
	    }
	  
          /* Display visible marks */

          if (seg->body.mark.visible)
	    cursor_byte_offsets = g_slist_prepend (cursor_byte_offsets, GINT_TO_POINTER (byte_offset));
        }

      else
        g_error ("Unknown segment type: %s", seg->type->name);

      if (style)
	{
	  release_style (layout, style);
	  style = NULL;
	}
      
      seg = seg->next;
    }

  /* Pango doesn't want the trailing new line */
  if (byte_offset > 0 && text[byte_offset - 1] == '\n')
    byte_offset--;
  
  pango_layout_set_text (display->layout, text, byte_offset);
  pango_layout_set_attributes (display->layout, attrs);

  tmp_list = cursor_byte_offsets;
  while (tmp_list)
    {
      add_cursor (display, GPOINTER_TO_INT (tmp_list->data));
      tmp_list = tmp_list->next;
    }
  g_slist_free (tmp_list);

  pango_layout_get_extents (display->layout, NULL, &extents);

  if (total_width >= 0)
    display->x_offset += (total_width - extents.width / PANGO_SCALE) * align;

  display->width = extents.width / PANGO_SCALE;
  display->height += extents.height / PANGO_SCALE;
 
  /* Free this if we aren't in a loop */
  if (layout->wrap_loop_count == 0)
    invalidate_cached_style (layout);

  g_free (text);
  pango_attr_list_unref (attrs);

  return display;
}

void
gtk_text_layout_free_line_display (GtkTextLayout      *layout,
				   GtkTextLine        *line,
				   GtkTextLineDisplay *display)
{
  pango_layout_unref (display->layout);

  if (display->cursors)
    {
      g_slist_foreach (display->cursors, (GFunc)g_free, NULL);
      g_slist_free (display->cursors);
    }
    
  g_free (display);
}

/* FIXME: This really doesn't belong in this file ... */
static GtkTextLineData*
gtk_text_line_data_new (GtkTextLayout *layout,
			GtkTextLine   *line)
{
  GtkTextLineData *list;

  list = g_new (GtkTextLineData, 1);

  list->view_id = layout;
  list->next = NULL;
  list->width = -1;
  list->height = -1;
  
  return list;
}

static void
line_data_destructor (gpointer data)
{
  g_free (data);
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
  if (y < 0)
    y = 0;
  if (x > layout->width)
    x = layout->width;
  if (y > layout->height)
    y = layout->height;
  
  line = gtk_text_btree_find_line_by_y (layout->buffer->tree, layout, y, &line_top);
  if (line == NULL)
    line = gtk_text_btree_get_line (layout->buffer->tree,
				    gtk_text_btree_line_count (layout->buffer->tree) - 1, NULL);
  
  display = gtk_text_layout_get_line_display (layout, line);

  /* We clamp y to the area of the actual layout so that the layouts
   * hit testing works OK on the space above and below the layout
   */
  y -= line_top + display->top_margin;
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
  
  gtk_text_layout_free_line_display (layout, line, display);
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

  display = gtk_text_layout_get_line_display (layout, line);

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
      rect->y += display->top_margin + pango_rect.y / PANGO_SCALE;
      rect->width = 0;
      rect->height = pango_rect.height / PANGO_SCALE;
    }
  else
    {
      byte_index = gtk_text_iter_get_line_byte (iter);
  
      pango_layout_index_to_pos (display->layout, byte_index, &pango_rect);
      
      rect->x = display->x_offset + pango_rect.x / PANGO_SCALE;
      rect->y += display->top_margin + pango_rect.y / PANGO_SCALE;
      rect->width = pango_rect.width / PANGO_SCALE;
      rect->height = pango_rect.height / PANGO_SCALE;
    }
  
  gtk_text_layout_free_line_display (layout, line, display);
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



