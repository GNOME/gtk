/* gtktextdisplay.c - display layed-out text
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

#include "gtktextdisplay.h"
#include "gtktextiterprivate.h"

#include <pango/pango.h>

typedef struct _GtkTextRenderState GtkTextRenderState;

struct _GtkTextRenderState
{
  GtkWidget *widget;
  
  GtkTextAppearance *last_appearance;
  GdkGC *fg_gc;
  GdkGC *bg_gc;
  GdkRectangle clip_rect;
};

static void       get_item_properties (PangoItem           *item,
				       GtkTextAppearance  **appearance);
static GdkRegion *get_selected_clip   (GtkTextRenderState  *render_state,
				       PangoLayout         *layout,
				       PangoLayoutLine     *line,
				       int                  x,
				       int                  y,
				       int                  height,
				       int                  start_index,
				       int                  end_index);

static GtkTextRenderState *
gtk_text_render_state_new (GtkWidget    *widget,
			   GdkDrawable  *drawable,
			   GdkRectangle *clip_rect)
{
  GtkTextRenderState *state = g_new0 (GtkTextRenderState, 1);

  state->widget = widget;
  state->fg_gc = gdk_gc_new (drawable);
  state->bg_gc = gdk_gc_new (drawable);
  state->clip_rect = *clip_rect;

  return state;
}

static void
gtk_text_render_state_destroy (GtkTextRenderState *state)
{
  gdk_gc_unref (state->fg_gc);
  gdk_gc_unref (state->bg_gc);
  
  g_free (state);
}

static void
gtk_text_render_state_set_color (GtkTextRenderState *state,
				 GdkGC              *gc,
				 GdkColor           *color)
{
  gdk_colormap_alloc_color (gtk_widget_get_colormap (state->widget), color, FALSE, TRUE);
  gdk_gc_set_foreground (gc, color);
}

static void
gtk_text_render_state_update (GtkTextRenderState *state,
			      GtkTextAppearance  *new_appearance)
{
  if (!state->last_appearance ||
      !gdk_color_equal (&new_appearance->fg_color, &state->last_appearance->fg_color))
    gtk_text_render_state_set_color (state, state->fg_gc, &new_appearance->fg_color);
  
  if (!state->last_appearance ||
      new_appearance->fg_stipple != state->last_appearance->fg_stipple)
    {
      if (new_appearance->fg_stipple)
	{
	  gdk_gc_set_fill(state->fg_gc, GDK_STIPPLED);
	  gdk_gc_set_stipple(state->fg_gc, new_appearance->fg_stipple);
	}
      else
	{
	  gdk_gc_set_fill(state->fg_gc, GDK_SOLID);
	}
    }
  
  if (new_appearance->draw_bg)
    {
      if (!state->last_appearance ||
	  !gdk_color_equal (&new_appearance->bg_color, &state->last_appearance->bg_color))
	gtk_text_render_state_set_color (state, state->bg_gc, &new_appearance->bg_color);
  
      if (!state->last_appearance ||
	  new_appearance->bg_stipple != state->last_appearance->bg_stipple)
	{
	  if (new_appearance->bg_stipple)
	    {
	      gdk_gc_set_fill(state->bg_gc, GDK_STIPPLED);
	      gdk_gc_set_stipple(state->bg_gc, new_appearance->bg_stipple);
	    }
	  else
	    {
	      gdk_gc_set_fill(state->bg_gc, GDK_SOLID);
	    }
	}
    }

  state->last_appearance = new_appearance;
}

static void 
render_layout_line (GdkDrawable        *drawable,
		    GtkTextRenderState *render_state,
		    PangoLayoutLine    *line,
		    GSList            **pixmap_pointer,
		    int                 x, 
		    int                 y,
		    gboolean            selected)
{
  GSList *tmp_list = line->runs;
  PangoRectangle overall_rect;
  PangoRectangle logical_rect;
  PangoRectangle ink_rect;
  
  gint x_off = 0;

  pango_layout_line_get_extents (line, NULL, &overall_rect);

  while (tmp_list)
    {
      PangoLayoutRun *run = tmp_list->data;
      GtkTextAppearance *appearance;
      
      tmp_list = tmp_list->next;

      get_item_properties (run->item, &appearance);

      if (appearance)		/* A text segment */
	{
	  GdkGC *fg_gc;
	  
	  if (selected)
	    {
	      fg_gc = render_state->widget->style->fg_gc[GTK_STATE_SELECTED];
	    }
	  else
	    {
	      gtk_text_render_state_update (render_state, appearance);

	      fg_gc = render_state->fg_gc;
	    }
	  
	  if (appearance->underline == PANGO_UNDERLINE_NONE && !appearance->overstrike)
	    pango_glyph_string_extents (run->glyphs, run->item->analysis.font,
					NULL, &logical_rect);
	  else
	    pango_glyph_string_extents (run->glyphs, run->item->analysis.font,
					&ink_rect, &logical_rect);

	  if (appearance->draw_bg && !selected)
	    gdk_draw_rectangle (drawable, render_state->bg_gc, TRUE,
				x + (x_off + logical_rect.x) / PANGO_SCALE,
				y + logical_rect.y / PANGO_SCALE,
				logical_rect.width / PANGO_SCALE,
				logical_rect.height / PANGO_SCALE);

	  gdk_draw_glyphs (drawable, fg_gc,
			   run->item->analysis.font, 
			   x + x_off / PANGO_SCALE, y, run->glyphs);

	  switch (appearance->underline)
	    {
	    case PANGO_UNDERLINE_NONE:
	      break;
	    case PANGO_UNDERLINE_DOUBLE:
	      gdk_draw_line (drawable, fg_gc,
			 x + (x_off + ink_rect.x) / PANGO_SCALE - 1, y + 4,
			 x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE, y + 4);
	      /* Fall through */
	    case PANGO_UNDERLINE_SINGLE:
	      gdk_draw_line (drawable, fg_gc,
			 x + (x_off + ink_rect.x) / PANGO_SCALE - 1, y + 2,
			 x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE, y + 2);
	      break;
	    case PANGO_UNDERLINE_LOW:
	      gdk_draw_line (drawable, fg_gc,
			 x + (x_off + ink_rect.x) / PANGO_SCALE - 1, y + (ink_rect.y + ink_rect.height) / PANGO_SCALE + 2,
			 x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE, y + (ink_rect.y + ink_rect.height) / PANGO_SCALE + 2);
	      break;
	    }

	  if (appearance->overstrike)
	    {
	      gint overstrike_y = y + (0.3 * logical_rect.y) / PANGO_SCALE;
	      gdk_draw_line (drawable, fg_gc,
			     x + (x_off + ink_rect.x) / PANGO_SCALE - 1, overstrike_y,
			     x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE, overstrike_y);
	    }

	  x_off += logical_rect.width;
	}
      else			/* Pixmap segment */
	{
	  GtkTextPixmap *pixmap = (*pixmap_pointer)->data;
	  gint width, height;
	  GdkRectangle pixmap_rect, draw_rect;
	  
	  *pixmap_pointer = (*pixmap_pointer)->next;

	  gdk_drawable_get_size (pixmap->pixmap, &width, &height);

	  pixmap_rect.x = x + x_off / PANGO_SCALE;
	  pixmap_rect.y = y - height;
	  pixmap_rect.width = width;
	  pixmap_rect.height = height;

	  gdk_rectangle_intersect (&pixmap_rect, &render_state->clip_rect, &draw_rect);

	  if (pixmap->mask)
	    {
	      gdk_gc_set_clip_mask (render_state->fg_gc, pixmap->mask);
	      gdk_gc_set_clip_origin (render_state->fg_gc,
				      pixmap_rect.x, pixmap_rect.y);
	    }
	  
	  gdk_draw_drawable (drawable, render_state->fg_gc, pixmap->pixmap,
			     draw_rect.x - pixmap_rect.x, draw_rect.y - pixmap_rect.y,
			     draw_rect.x, draw_rect.y, draw_rect.width, draw_rect.height);

	  if (pixmap->mask)
	    gdk_gc_set_clip_rectangle (render_state->fg_gc, &render_state->clip_rect);

	  x_off += width * PANGO_SCALE;
	}
    }
}

static void 
render_para (GdkDrawable        *drawable,
	     GtkTextRenderState *render_state,
	     GtkTextLineDisplay *line_display,
	     int                 x, 
	     int                 y,
	     int                 selection_start_index,
	     int                 selection_end_index)
{
  PangoRectangle logical_rect;
  GSList *pixmap_pointer = line_display->pixmaps;
  GSList *tmp_list;
  PangoAlignment align;
  PangoLayout *layout = line_display->layout;
  int indent;
  int total_width;
  int y_offset = 0;
  int byte_offset = 0; 

  gboolean first = TRUE;
  
  indent = pango_layout_get_indent (layout);
  total_width = pango_layout_get_width (layout);
  align = pango_layout_get_alignment (layout);

  if (total_width < 0)
    total_width = line_display->total_width * PANGO_SCALE;

  tmp_list = pango_layout_get_lines (layout);
  while (tmp_list)
    {
      PangoLayoutLine *line = tmp_list->data;
      int x_offset;
      int selection_y, selection_height;
      
      pango_layout_line_get_extents (line, NULL, &logical_rect);

      x_offset = line_display->left_margin * PANGO_SCALE;

      switch (align)
	{
	case PANGO_ALIGN_RIGHT:
	  x_offset += total_width - logical_rect.width;
	  break;
	case PANGO_ALIGN_CENTER:
	  x_offset += (total_width - logical_rect.width) / 2;
	  break;
	default:
	  break;
	}

      if (first)
	{
	  if (indent > 0)
	    {
	      if (align == PANGO_ALIGN_LEFT)
		x_offset += indent;
	      else
		x_offset -= indent;
	    }
	}
      else
	{
	  if (indent < 0)
	    {
	      if (align == PANGO_ALIGN_LEFT)
		x_offset -= indent;
	      else
		x_offset += indent;
	    }
	}

      selection_y = y + y_offset / PANGO_SCALE;
      selection_height = logical_rect.height / PANGO_SCALE;

      if (first)
	{
	  selection_y -= line_display->top_margin;
	  selection_height += line_display->top_margin;
	}
      if (!tmp_list->next)
	selection_height += line_display->bottom_margin;

      first = FALSE;
      
      if (selection_start_index < byte_offset &&
	  selection_end_index > line->length + byte_offset) /* All selected */
	{
	  gdk_draw_rectangle (drawable,
			      render_state->widget->style->bg_gc[GTK_STATE_SELECTED],
			      TRUE,
			      x + line_display->left_margin, selection_y,
			      total_width / PANGO_SCALE, selection_height);
	  render_layout_line (drawable, render_state, line, &pixmap_pointer,
			      x + x_offset / PANGO_SCALE, y + (y_offset - logical_rect.y) / PANGO_SCALE,
			      TRUE);
	}
      else
	{
	  GSList *pixmap_pointer_tmp = pixmap_pointer;
	  
	  render_layout_line (drawable, render_state, line, &pixmap_pointer,
			      x + x_offset / PANGO_SCALE, y + (y_offset - logical_rect.y) / PANGO_SCALE,
			      FALSE);

	  if (selection_start_index < byte_offset + line->length &&
	      selection_end_index > byte_offset) /* Some selected */
	    {
	      GdkRegion *clip_region = get_selected_clip (render_state, layout, line,
							  x + line_display->x_offset,
							  selection_y, selection_height,
							  selection_start_index, selection_end_index);

	      gdk_gc_set_clip_region (render_state->widget->style->fg_gc [GTK_STATE_SELECTED], clip_region);
	      gdk_gc_set_clip_region (render_state->widget->style->bg_gc [GTK_STATE_SELECTED], clip_region);

	      gdk_draw_rectangle (drawable,
				  render_state->widget->style->bg_gc[GTK_STATE_SELECTED],
				  TRUE,
				  x + x_offset / PANGO_SCALE, selection_y,
				  logical_rect.width / PANGO_SCALE,
				  selection_height);
	      
	      render_layout_line (drawable, render_state, line, &pixmap_pointer_tmp,
				  x + x_offset / PANGO_SCALE, y + (y_offset - logical_rect.y) / PANGO_SCALE,
				  TRUE);

	      gdk_gc_set_clip_region (render_state->widget->style->fg_gc [GTK_STATE_SELECTED], NULL);
	      gdk_gc_set_clip_region (render_state->widget->style->bg_gc [GTK_STATE_SELECTED], NULL);

	      gdk_region_destroy (clip_region);

	      /* Paint in the ends of the line */
	      if (x_offset > line_display->left_margin * PANGO_SCALE &&
		  ((line_display->direction == GTK_TEXT_DIR_LTR && selection_start_index < byte_offset) ||
		   (line_display->direction == GTK_TEXT_DIR_RTL && selection_end_index > byte_offset + line->length)))
		{
		  gdk_draw_rectangle (drawable,
				      render_state->widget->style->bg_gc[GTK_STATE_SELECTED],
				      TRUE,
				      x + line_display->left_margin, selection_y,
				      x + x_offset / PANGO_SCALE - line_display->left_margin,
				      selection_height);
		}

	      if (x_offset + logical_rect.width < line_display->left_margin * PANGO_SCALE + total_width &&
		  ((line_display->direction == GTK_TEXT_DIR_LTR && selection_end_index > byte_offset + line->length) ||
		   (line_display->direction == GTK_TEXT_DIR_RTL && selection_start_index < byte_offset)))
		{
		  
		  
		  gdk_draw_rectangle (drawable,
				      render_state->widget->style->bg_gc[GTK_STATE_SELECTED],
				      TRUE,
				      x + (x_offset + logical_rect.width) / PANGO_SCALE,
				      selection_y,
				      x + (line_display->left_margin * PANGO_SCALE + total_width - x_offset - logical_rect.width) / PANGO_SCALE,
				      selection_height);
		}
	    }
	}

      byte_offset += line->length;
      y_offset += logical_rect.height;
      tmp_list = tmp_list->next;
    }
}

static GdkRegion *
get_selected_clip (GtkTextRenderState *render_state,
		   PangoLayout        *layout,
		   PangoLayoutLine    *line,
		   int                 x,
		   int                 y,
		   int                 height,
		   int                 start_index,
		   int                 end_index)
{
  gint *ranges;
  gint n_ranges, i;
  GdkRegion *clip_region = gdk_region_new ();
  GdkRegion *tmp_region;
  PangoRectangle logical_rect;

  pango_layout_line_get_extents (line, NULL, &logical_rect);
  pango_layout_line_get_x_ranges (line, start_index, end_index, &ranges, &n_ranges);

  for (i=0; i < n_ranges; i++)
    {
      GdkRectangle rect;

      rect.x = x + ranges[2*i] / PANGO_SCALE;
      rect.y = y;
      rect.width = (ranges[2*i + 1] - ranges[2*i]) / PANGO_SCALE;
      rect.height = height;
	      
      gdk_region_union_with_rect (clip_region, &rect);
    }

  tmp_region = gdk_region_rectangle (&render_state->clip_rect);
  gdk_region_intersect (clip_region, tmp_region);
  gdk_region_destroy (tmp_region);

  g_free (ranges);
  return clip_region;
}

static void
get_item_properties (PangoItem          *item,
		     GtkTextAppearance **appearance)
{
  GSList *tmp_list = item->extra_attrs;
  
  *appearance = NULL;
  
  while (tmp_list)
    {
      PangoAttribute *attr = tmp_list->data;

      if (attr->klass->type == gtk_text_attr_appearance_type)
	{
	  *appearance = &((GtkTextAttrAppearance *)attr)->appearance;
	}

      tmp_list = tmp_list->next;
    }
}

void
gtk_text_layout_draw (GtkTextLayout *layout,
		      GtkWidget *widget,
		      GdkDrawable *drawable,
		      /* Location of the layout
			 in buffer coordinates */
		      gint x_offset,
		      gint y_offset,
		      /* Region of the layout to
			 render */
		      gint x,
		      gint y,
		      gint width,
		      gint height)
{
  GdkRectangle clip;
  gint current_y;
  GSList *line_list;
  GSList *tmp_list;
  GSList *cursor_list;
  GtkTextRenderState *render_state;
  GtkTextIter selection_start, selection_end;
  gboolean have_selection = FALSE;
  
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (layout->default_style != NULL);
  g_return_if_fail (layout->buffer != NULL);
  g_return_if_fail (drawable != NULL);
  g_return_if_fail (x_offset >= 0);
  g_return_if_fail (y_offset >= 0);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  if (width == 0 || height == 0)
    return;

  line_list =  gtk_text_layout_get_lines (layout, y + y_offset, y + y_offset + height, &current_y);
  current_y -= y_offset;

  if (line_list == NULL)
    return; /* nothing on the screen */

  clip.x = x;
  clip.y = y;
  clip.width = width;
  clip.height = height;

  render_state = gtk_text_render_state_new (widget, drawable, &clip);

  gdk_gc_set_clip_rectangle (render_state->fg_gc, &clip);
  gdk_gc_set_clip_rectangle (render_state->bg_gc, &clip);

  gtk_text_layout_wrap_loop_start (layout);
  
  if (gtk_text_buffer_get_selection_bounds (layout->buffer,
					    &selection_start,
					    &selection_end))
    have_selection = TRUE;
  
  tmp_list = line_list;
  while (tmp_list != NULL)
    {
      GtkTextLineDisplay *line_display;
      gint selection_start_index = -1;
      gint selection_end_index = -1;

      GtkTextLine *line = tmp_list->data;
     
      line_display = gtk_text_layout_get_line_display (layout, line, FALSE);

      if (have_selection)
	{
	  GtkTextIter line_start, line_end;
	  gint byte_count = gtk_text_line_byte_count (line);
	  
	  gtk_text_btree_get_iter_at_line (_gtk_text_buffer_get_btree (layout->buffer),
                                           &line_start,
					   line, 0);
	  gtk_text_btree_get_iter_at_line (_gtk_text_buffer_get_btree (layout->buffer),
                                           &line_end,
					   line, byte_count - 1);

	  if (gtk_text_iter_compare (&selection_start, &line_end) < 0 &&
	      gtk_text_iter_compare (&selection_end, &line_start) > 0)
	    {
	      if (gtk_text_iter_compare (&selection_start, &line_start) >= 0)
		selection_start_index = gtk_text_iter_get_line_index (&selection_start);
	      else
		selection_start_index = -1;

	      if (gtk_text_iter_compare (&selection_end, &line_end) <= 0)
		selection_end_index = gtk_text_iter_get_line_index (&selection_end);
	      else
		selection_end_index = byte_count;
	    }
	}
  
      render_para (drawable, render_state, line_display,
		   - x_offset,
		   current_y + line_display->top_margin,
		   selection_start_index, selection_end_index);

      
      /* We paint the cursors last, because they overlap another chunk
         and need to appear on top. */

      cursor_list = line_display->cursors;
      while (cursor_list)
	{
	  GtkTextCursorDisplay *cursor = cursor_list->data;
	  GdkGC *gc;
	  
	  if (cursor->is_strong)
	    gc = widget->style->bg_gc[GTK_STATE_SELECTED];
	  else
	    gc = widget->style->fg_gc[GTK_STATE_NORMAL];
	  
	  gdk_draw_line (drawable, gc,
			 line_display->x_offset + cursor->x,
			 current_y + line_display->top_margin + cursor->y,
			 line_display->x_offset + cursor->x,
			 current_y + line_display->top_margin + cursor->y + cursor->height);
	  
	  cursor_list = cursor_list->next;
	}

      current_y += line_display->height;
      gtk_text_layout_free_line_display (layout, line_display);
      
      tmp_list = g_slist_next (tmp_list);
    }

  gtk_text_layout_wrap_loop_end (layout);
  gtk_text_render_state_destroy (render_state);

  g_slist_free (line_list);
}
