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
#include <pango/pangox.h>
#include "x11/gdkx.h"

typedef struct _GtkTextRenderState GtkTextRenderState;

struct _GtkTextRenderState
{
  GtkWidget *widget;
  
  GtkTextAppearance *last_appearance;
  GdkGC *fg_gc;
  GdkGC *bg_gc;
};

void get_item_properties (PangoItem          *item,
			  GtkTextAppearance **appearance);


static GtkTextRenderState *
gtk_text_render_state_new (GtkWidget   *widget,
			   GdkDrawable *drawable)
{
  GtkTextRenderState *state = g_new0 (GtkTextRenderState, 1);

  state->widget = widget;
  state->fg_gc = gdk_gc_new (drawable);
  state->bg_gc = gdk_gc_new (drawable);

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
  /* If the new_appearance is inside the selection, we actually modify its
   * foreground, background, and stipples accordingly. This is pretty
   * gross, but we own the appearance attribute, safe, and its simpler than
   * the alternatives
   */
  if (new_appearance->inside_selection)
    {
      if (new_appearance->bg_stipple)
	{
	  gdk_drawable_unref (new_appearance->bg_stipple);
	  new_appearance->bg_stipple = NULL;
	}

      new_appearance->fg_color = state->widget->style->fg[GTK_STATE_SELECTED];
      new_appearance->bg_color = state->widget->style->bg[GTK_STATE_SELECTED];
      new_appearance->draw_bg = TRUE;
    }
  
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
		    int                 x, 
		    int                 y)
{
  GSList *tmp_list = line->runs;
  PangoRectangle overall_rect;
  PangoRectangle logical_rect;
  PangoRectangle ink_rect;
  
  int x_off = 0;

  pango_layout_line_get_extents (line,NULL, &overall_rect);
  
  while (tmp_list)
    {
      PangoLayoutRun *run = tmp_list->data;
      GtkTextAppearance *appearance;
      
      tmp_list = tmp_list->next;

      get_item_properties (run->item, &appearance);

      if (appearance)		/* A text segment */
	{
	  gtk_text_render_state_update (render_state, appearance);
	  
	  if (appearance->underline == PANGO_UNDERLINE_NONE)
	    pango_glyph_string_extents (run->glyphs, run->item->analysis.font,
					NULL, &logical_rect);
	  else
	    pango_glyph_string_extents (run->glyphs, run->item->analysis.font,
					&ink_rect, &logical_rect);

	  if (appearance->draw_bg)
	    gdk_draw_rectangle (drawable, render_state->bg_gc, TRUE,
				x + (x_off + logical_rect.x) / PANGO_SCALE,
				y + overall_rect.y / PANGO_SCALE,
				logical_rect.width / PANGO_SCALE,
				overall_rect.height / PANGO_SCALE);

	  gdk_draw_glyphs (drawable, render_state->fg_gc,
			   run->item->analysis.font, 
			   x + x_off / PANGO_SCALE, y, run->glyphs);

	  switch (appearance->underline)
	    {
	    case PANGO_UNDERLINE_NONE:
	      break;
	    case PANGO_UNDERLINE_DOUBLE:
	      gdk_draw_line (drawable, render_state->fg_gc,
			 x + (x_off + ink_rect.x) / PANGO_SCALE - 1, y + 4,
			 x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE, y + 4);
	      /* Fall through */
	    case PANGO_UNDERLINE_SINGLE:
	      gdk_draw_line (drawable, render_state->fg_gc,
			 x + (x_off + ink_rect.x) -1, y + 2,
			 x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE, y + 2);
	      break;
	    case PANGO_UNDERLINE_LOW:
	      gdk_draw_line (drawable, render_state->fg_gc,
			 x + (x_off + ink_rect.x) / PANGO_SCALE - 1, y + (ink_rect.y + ink_rect.height) / PANGO_SCALE + 2,
			 x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE, y + (ink_rect.y + ink_rect.height) / PANGO_SCALE + 2);
	      break;
	    }

	  x_off += logical_rect.width;
	}
    }
}

static void 
render_layout (GdkDrawable        *drawable,
	       GtkTextRenderState *render_state,
	       PangoLayout        *layout,
	       int                 x, 
	       int                 y)
{
  PangoRectangle logical_rect;
  GSList *tmp_list;
  PangoAlignment align;
  int indent;
  int width;
  int y_offset = 0;

  gboolean first = FALSE;
  
  g_return_if_fail (layout != NULL);

  indent = pango_layout_get_indent (layout);
  width = pango_layout_get_width (layout);
  align = pango_layout_get_alignment (layout);

  if (width == -1 && align != PANGO_ALIGN_LEFT)
    {
      pango_layout_get_extents (layout, NULL, &logical_rect);
      width = logical_rect.width;
    }
  
  tmp_list = pango_layout_get_lines (layout);
  while (tmp_list)
    {
      PangoLayoutLine *line = tmp_list->data;
      int x_offset;
      
      pango_layout_line_get_extents (line, NULL, &logical_rect);

      if (width != 1 && align == PANGO_ALIGN_RIGHT)
	x_offset = width - logical_rect.width;
      else if (width != 1 && align == PANGO_ALIGN_CENTER)
	x_offset = (width - logical_rect.width) / 2;
      else
	x_offset = 0;

      if (first)
	{
	  if (indent > 0)
	    {
	      if (align == PANGO_ALIGN_LEFT)
		x_offset += indent;
	      else
		x_offset -= indent;
	    }

	  first = FALSE;
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
	  
      render_layout_line (drawable, render_state,
			  line, x + x_offset / PANGO_SCALE, y + (y_offset - logical_rect.y) / PANGO_SCALE);

      y_offset += logical_rect.height;
      tmp_list = tmp_list->next;
    }
}

void
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
	  return;
	}
    }
}

void
gtk_text_layout_draw (GtkTextLayout *layout,
		      GtkWidget *widget,
		      GdkDrawable *drawable,
		      /* Location of the buffer
			 in layout coordinates */
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
  gint buffer_x, buffer_y, current_y;
  GSList *line_list;
  GSList *tmp_list;
  GSList *cursor_list;
  GtkTextRenderState *render_state;
  
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (layout->default_style != NULL);
  g_return_if_fail (layout->buffer != NULL);
  g_return_if_fail (drawable != NULL);
  g_return_if_fail (x_offset >= 0);
  g_return_if_fail (y_offset >= 0);
  g_return_if_fail (x >= 0);
  g_return_if_fail (y >= 0);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  if (width == 0 || height == 0)
    return;

  line_list =  gtk_text_layout_get_lines (layout,
					  y,
					  y + height + 1, /* one past what we draw */
					  &current_y);
  current_y -= y_offset;
  
  if (line_list == NULL)
    return; /* nothing on the screen */

  /* Convert to buffer coordinates */
  buffer_x = x - x_offset;
  buffer_y = y - y_offset;

  /* Don't draw outside the buffer */
  if (buffer_x < 0)
    buffer_x = 0;
  if (buffer_y < 0)
    buffer_y = 0;

  clip.x = buffer_x;
  clip.y = buffer_y;
  clip.width = width;
  clip.height = height;

  render_state = gtk_text_render_state_new (widget, drawable);

  gdk_gc_set_clip_rectangle (render_state->fg_gc, &clip);
  gdk_gc_set_clip_rectangle (render_state->bg_gc, &clip);

  gtk_text_layout_wrap_loop_start (layout);
  
  tmp_list = line_list;
  while (tmp_list != NULL)
    {
      GtkTextLineDisplay *line_display;
      
      line_display = gtk_text_layout_get_line_display (layout, tmp_list->data);

      render_layout (drawable, render_state, line_display->layout,
		     line_display->x_offset,
		     current_y + line_display->top_margin);

      
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
			 current_y + line_display->top_margin + cursor->height);
	  
	  cursor_list = cursor_list->next;
	}

      current_y += line_display->height;
      gtk_text_layout_free_line_display (layout, tmp_list->data, line_display);
      
      tmp_list = g_slist_next (tmp_list);
    }

  gtk_text_layout_wrap_loop_end (layout);
  gtk_text_render_state_destroy (render_state);

  g_slist_free (line_list);
}
