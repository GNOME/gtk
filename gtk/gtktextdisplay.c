/* gtktextdisplay.c - display layed-out text
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000 Red Hat, Inc.
 * Tk->Gtk port by Havoc Pennington
 *
 * This file can be used under your choice of two licenses, the LGPL
 * and the original Tk license.
 *
 * LGPL:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Original Tk license:
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
/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "gtktextdisplay.h"
/* DO NOT go putting private headers in here. This file should only
 * use the semi-public headers, as with gtktextview.c.
 */

#include <pango/pango.h>

typedef struct _GtkTextRenderState GtkTextRenderState;

struct _GtkTextRenderState
{
  GtkWidget *widget;

  GtkTextAppearance *last_appearance;
  GtkTextAppearance *last_bg_appearance;
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
          gdk_gc_set_fill (state->fg_gc, GDK_STIPPLED);
          gdk_gc_set_stipple (state->fg_gc, new_appearance->fg_stipple);
        }
      else
        {
          gdk_gc_set_fill (state->fg_gc, GDK_SOLID);
        }
    }

  if (new_appearance->draw_bg)
    {
      if (!state->last_bg_appearance ||
          !gdk_color_equal (&new_appearance->bg_color, &state->last_bg_appearance->bg_color))
        gtk_text_render_state_set_color (state, state->bg_gc, &new_appearance->bg_color);

      if (!state->last_bg_appearance ||
          new_appearance->bg_stipple != state->last_bg_appearance->bg_stipple)
        {
          if (new_appearance->bg_stipple)
            {
              gdk_gc_set_fill (state->bg_gc, GDK_STIPPLED);
              gdk_gc_set_stipple (state->bg_gc, new_appearance->bg_stipple);
            }
          else
            {
              gdk_gc_set_fill (state->bg_gc, GDK_SOLID);
            }
        }

      state->last_bg_appearance = new_appearance;
    }

  state->last_appearance = new_appearance;
}

static void
get_shape_extents (PangoLayoutRun *run,
                   PangoRectangle *ink_rect,
                   PangoRectangle *logical_rect)
{
  GSList *tmp_list = run->item->analysis.extra_attrs;
    
  while (tmp_list)
    {
      PangoAttribute *attr = tmp_list->data;

      if (attr->klass->type == PANGO_ATTR_SHAPE)
	{          
	  if (logical_rect)
	    *logical_rect = ((PangoAttrShape *)attr)->logical_rect;

	  if (ink_rect)
	    *ink_rect = ((PangoAttrShape *)attr)->ink_rect;

          return;
        }

      tmp_list = tmp_list->next;
    }

  g_assert_not_reached ();
}

static void
render_layout_line (GdkDrawable        *drawable,
                    GtkTextRenderState *render_state,
                    PangoLayoutLine    *line,
                    GSList            **shaped_pointer,
                    int                 x,
                    int                 y,
                    gboolean            selected)
{
  GSList *tmp_list = line->runs;
  PangoRectangle overall_rect;
  PangoRectangle logical_rect;
  PangoRectangle ink_rect;
  gint x_off = 0;
  GdkGC *fg_gc;
  
  pango_layout_line_get_extents (line, NULL, &overall_rect);

  while (tmp_list)
    {
      PangoLayoutRun *run = tmp_list->data;
      GtkTextAppearance *appearance;
      gint risen_y;
      gint shaped_width_pixels = 0;
      gboolean need_ink = FALSE;
      
      tmp_list = tmp_list->next;

      get_item_properties (run->item, &appearance);

      g_assert (appearance != NULL);
      
      risen_y = y - PANGO_PIXELS (appearance->rise);
      
      if (selected)
        {
          fg_gc = render_state->widget->style->text_gc[GTK_STATE_SELECTED];
        }
      else
        {
          gtk_text_render_state_update (render_state, appearance);
          
          fg_gc = render_state->fg_gc;
        }
      
      if (appearance->underline != PANGO_UNDERLINE_NONE ||
          appearance->strikethrough)
        need_ink = TRUE;
      
      if (appearance->is_text)
        {
          if (need_ink)
            pango_glyph_string_extents (run->glyphs, run->item->analysis.font,
                                        &ink_rect, &logical_rect);
          else
            pango_glyph_string_extents (run->glyphs, run->item->analysis.font,
                                        NULL, &logical_rect);
        }
      else
        {
          if (need_ink)
            get_shape_extents (run, &ink_rect, &logical_rect);
          else
            get_shape_extents (run, NULL, &logical_rect);
        }
      
      if (appearance->draw_bg && !selected)
        gdk_draw_rectangle (drawable, render_state->bg_gc, TRUE,
                            x + PANGO_PIXELS (x_off) + PANGO_PIXELS (logical_rect.x),
                            risen_y + PANGO_PIXELS (logical_rect.y),
                            PANGO_PIXELS (logical_rect.width),
                            PANGO_PIXELS (logical_rect.height));

      if (appearance->is_text)
        gdk_draw_glyphs (drawable, fg_gc,
                         run->item->analysis.font,
                         x + PANGO_PIXELS (x_off),
                         risen_y, run->glyphs);
      else
        {
          GObject *shaped = (*shaped_pointer)->data;

          *shaped_pointer = (*shaped_pointer)->next;
          
          if (GDK_IS_PIXBUF (shaped))
            {
              gint width, height;
              GdkRectangle pixbuf_rect, draw_rect;
              GdkPixbuf *pixbuf;

              pixbuf = GDK_PIXBUF (shaped);
              
              width = gdk_pixbuf_get_width (pixbuf);
              height = gdk_pixbuf_get_height (pixbuf);

              pixbuf_rect.x = x + x_off / PANGO_SCALE;
              pixbuf_rect.y = risen_y - height;
              pixbuf_rect.width = width;
              pixbuf_rect.height = height;

              if (gdk_rectangle_intersect (&pixbuf_rect, &render_state->clip_rect,
                                           &draw_rect))
                {
                  GdkBitmap *mask = NULL;
              
                  if (gdk_pixbuf_get_has_alpha (pixbuf))
                    {
                      mask = gdk_pixmap_new (drawable,
                                             gdk_pixbuf_get_width (pixbuf),
                                             gdk_pixbuf_get_height (pixbuf),
                                             1);

                      gdk_pixbuf_render_threshold_alpha (pixbuf, mask,
                                                         0, 0, 0, 0,
                                                         gdk_pixbuf_get_width (pixbuf),
                                                         gdk_pixbuf_get_height (pixbuf),
                                                         128);

                    }

                  if (mask)
                    {
                      gdk_gc_set_clip_mask (render_state->fg_gc, mask);
                      gdk_gc_set_clip_origin (render_state->fg_gc,
                                              pixbuf_rect.x, pixbuf_rect.y);
                    }

                  gdk_pixbuf_render_to_drawable (pixbuf,
                                                 drawable,
                                                 render_state->fg_gc,
                                                 draw_rect.x - pixbuf_rect.x,
                                                 draw_rect.y - pixbuf_rect.y,
                                                 draw_rect.x, draw_rect.y,
                                                 draw_rect.width,
                                                 draw_rect.height,
                                                 GDK_RGB_DITHER_NORMAL,
                                                 0, 0);

                  if (mask)
                    {
                      gdk_gc_set_clip_rectangle (render_state->fg_gc,
                                                 &render_state->clip_rect);
                      g_object_unref (G_OBJECT (mask));
                    }
                }

              shaped_width_pixels = width;
            }
          else if (GTK_IS_WIDGET (shaped))
            {
              gint width, height;
              GdkRectangle draw_rect;
              GtkWidget *widget;
              
              widget = GTK_WIDGET (shaped);
              
              width = widget->allocation.width;
              height = widget->allocation.height;

              g_print ("widget allocation at %d,%d %d x %d\n",
		       widget->allocation.x,
		       widget->allocation.y,
		       widget->allocation.width,
		       widget->allocation.height);
              
              if (GTK_WIDGET_DRAWABLE (widget) &&
                  gdk_rectangle_intersect (&widget->allocation,
                                           &render_state->clip_rect,
                                           &draw_rect))

                {
                  g_print ("drawing widget area %d,%d %d x %d\n",
			   draw_rect.x,
			   draw_rect.y,
			   draw_rect.width,
			   draw_rect.height);

                  gtk_widget_draw (widget, &draw_rect);
                }

              shaped_width_pixels = width;
            }
          else
            g_assert_not_reached (); /* not a pixbuf or widget */
        }

      switch (appearance->underline)
        {
        case PANGO_UNDERLINE_NONE:
          break;
        case PANGO_UNDERLINE_DOUBLE:
          g_assert (need_ink);
          gdk_draw_line (drawable, fg_gc,
                         x + (x_off + ink_rect.x) / PANGO_SCALE - 1,
                         risen_y + 3,
                         x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE,
                         risen_y + 3);
          /* Fall through */
        case PANGO_UNDERLINE_SINGLE:
          g_assert (need_ink);
          gdk_draw_line (drawable, fg_gc,
                         x + (x_off + ink_rect.x) / PANGO_SCALE - 1,
                         risen_y + 1,
                         x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE,
                         risen_y + 1);
          break;
        case PANGO_UNDERLINE_LOW:
          g_assert (need_ink);
          gdk_draw_line (drawable, fg_gc,
                         x + (x_off + ink_rect.x) / PANGO_SCALE - 1,
                         risen_y + (ink_rect.y + ink_rect.height) / PANGO_SCALE + 1,
                         x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE,
                         risen_y + (ink_rect.y + ink_rect.height) / PANGO_SCALE + 1);
          break;
        }

      if (appearance->strikethrough)
        {          
          gint strikethrough_y = risen_y + (0.3 * logical_rect.y) / PANGO_SCALE;

          g_assert (need_ink);
          
          gdk_draw_line (drawable, fg_gc,
                         x + (x_off + ink_rect.x) / PANGO_SCALE - 1, strikethrough_y,
                         x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE, strikethrough_y);
        }

      if (appearance->is_text)
        x_off += logical_rect.width;
      else
        x_off += shaped_width_pixels * PANGO_SCALE;
    }
}

static void
render_para (GdkDrawable        *drawable,
             GtkTextRenderState *render_state,
             GtkTextLineDisplay *line_display,
             /* Top-left corner of paragraph including all margins */
             int                 x,
             int                 y,
             int                 selection_start_index,
             int                 selection_end_index)
{
  GSList *shaped_pointer = line_display->shaped_objects;
  PangoLayout *layout = line_display->layout;
  int byte_offset = 0;
  PangoLayoutIter *iter;
  PangoRectangle layout_logical;
  int screen_width;
  
  gboolean first = TRUE;

  iter = pango_layout_get_iter (layout);

  pango_layout_iter_get_layout_extents (iter, NULL, &layout_logical);

  /* Adjust for margins */
  
  layout_logical.x += line_display->x_offset * PANGO_SCALE;
  layout_logical.y += line_display->top_margin * PANGO_SCALE;

  screen_width = line_display->total_width;
  
  do
    {
      PangoLayoutLine *line = pango_layout_iter_get_line (iter);
      int selection_y, selection_height;
      int first_y, last_y;
      PangoRectangle line_rect;
      int baseline;
      
      pango_layout_iter_get_line_extents (iter, NULL, &line_rect);
      baseline = pango_layout_iter_get_baseline (iter);
      pango_layout_iter_get_line_yrange (iter, &first_y, &last_y);
      
      /* Adjust for margins */

      line_rect.x += line_display->x_offset * PANGO_SCALE;
      line_rect.y += line_display->top_margin * PANGO_SCALE;
      baseline += line_display->top_margin * PANGO_SCALE;

      /* Selection is the height of the line, plus top/bottom
       * margin if we're the first/last line
       */
      selection_y = y + PANGO_PIXELS (first_y) + line_display->top_margin;
      selection_height = PANGO_PIXELS (last_y) - PANGO_PIXELS (first_y);

      if (first)
        {
          selection_y -= line_display->top_margin;
          selection_height += line_display->top_margin;
        }
      
      if (pango_layout_iter_at_last_line (iter))
        selection_height += line_display->bottom_margin;
      
      first = FALSE;

      if (selection_start_index < byte_offset &&
          selection_end_index > line->length + byte_offset) /* All selected */
        {
          gdk_draw_rectangle (drawable,
                              render_state->widget->style->base_gc[GTK_STATE_SELECTED],
                              TRUE,
                              x + line_display->left_margin,
                              selection_y,
                              screen_width,
                              selection_height);

          render_layout_line (drawable, render_state, line, &shaped_pointer,
                              x + PANGO_PIXELS (line_rect.x),
                              y + PANGO_PIXELS (baseline),
                              TRUE);
        }
      else
        {
          GSList *shaped_pointer_tmp = shaped_pointer;

          render_layout_line (drawable, render_state,
                              line, &shaped_pointer,
                              x + PANGO_PIXELS (line_rect.x),
                              y + PANGO_PIXELS (baseline),
                              FALSE);

          if (selection_start_index <= byte_offset + line->length &&
              selection_end_index > byte_offset) /* Some selected */
            {
              GdkRegion *clip_region = get_selected_clip (render_state, layout, line,
                                                          x + line_display->x_offset,
                                                          selection_y,
                                                          selection_height,
                                                          selection_start_index, selection_end_index);

              gdk_gc_set_clip_region (render_state->widget->style->text_gc [GTK_STATE_SELECTED], clip_region);
              gdk_gc_set_clip_region (render_state->widget->style->base_gc [GTK_STATE_SELECTED], clip_region);

              gdk_draw_rectangle (drawable,
                                  render_state->widget->style->base_gc[GTK_STATE_SELECTED],
                                  TRUE,
                                  x + PANGO_PIXELS (line_rect.x),
                                  selection_y,
                                  PANGO_PIXELS (line_rect.width),
                                  selection_height);

              render_layout_line (drawable, render_state, line, &shaped_pointer_tmp,
                                  x + PANGO_PIXELS (line_rect.x),
                                  y + PANGO_PIXELS (baseline),
                                  TRUE);

              gdk_gc_set_clip_region (render_state->widget->style->text_gc [GTK_STATE_SELECTED], NULL);
              gdk_gc_set_clip_region (render_state->widget->style->base_gc [GTK_STATE_SELECTED], NULL);

              gdk_region_destroy (clip_region);

              /* Paint in the ends of the line */
              if (line_rect.x > line_display->left_margin * PANGO_SCALE &&
                  ((line_display->direction == GTK_TEXT_DIR_LTR && selection_start_index < byte_offset) ||
                   (line_display->direction == GTK_TEXT_DIR_RTL && selection_end_index > byte_offset + line->length)))
                {
                  gdk_draw_rectangle (drawable,
                                      render_state->widget->style->base_gc[GTK_STATE_SELECTED],
                                      TRUE,
                                      x + line_display->left_margin,
                                      selection_y,
                                      PANGO_PIXELS (line_rect.x) - line_display->left_margin,
                                      selection_height);
                }

              if (line_rect.x + line_rect.width <
                  (screen_width + line_display->left_margin) * PANGO_SCALE &&
                  ((line_display->direction == GTK_TEXT_DIR_LTR && selection_end_index > byte_offset + line->length) ||
                   (line_display->direction == GTK_TEXT_DIR_RTL && selection_start_index < byte_offset)))
                {
                  int nonlayout_width;

                  nonlayout_width =
                    line_display->left_margin + screen_width -
                    PANGO_PIXELS (line_rect.x) - PANGO_PIXELS (line_rect.width);

                  gdk_draw_rectangle (drawable,
                                      render_state->widget->style->base_gc[GTK_STATE_SELECTED],
                                      TRUE,
                                      x + PANGO_PIXELS (line_rect.x) + PANGO_PIXELS (line_rect.width),
                                      selection_y,
                                      nonlayout_width,
                                      selection_height);
                }
            }
        }

      byte_offset += line->length;
    }
  while (pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);
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

  pango_layout_line_get_x_ranges (line, start_index, end_index, &ranges, &n_ranges);

  for (i=0; i < n_ranges; i++)
    {
      GdkRectangle rect;

      rect.x = x + PANGO_PIXELS (ranges[2*i]);
      rect.y = y;
      rect.width = PANGO_PIXELS (ranges[2*i + 1]) - PANGO_PIXELS (ranges[2*i]);
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
  GSList *tmp_list = item->analysis.extra_attrs;

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
                      /* Location of the drawable
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
  gint current_y;
  GSList *cursor_list;
  GtkTextRenderState *render_state;
  GtkTextIter selection_start, selection_end;
  gboolean have_selection = FALSE;
  GSList *line_list;
  GSList *tmp_list;
  
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (layout->default_style != NULL);
  g_return_if_fail (layout->buffer != NULL);
  g_return_if_fail (drawable != NULL);
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

      if (line_display->height > 0)
        {
          g_assert (line_display->layout != NULL);
          
          if (have_selection)
            {
              GtkTextIter line_start, line_end;
              gint byte_count;
              
              gtk_text_layout_get_iter_at_line (layout,
                                                &line_start,
                                                line, 0);
              byte_count = gtk_text_iter_get_bytes_in_line (&line_start);
          
              /* FIXME the -1 assumes a newline I think */
              gtk_text_layout_get_iter_at_line (layout,
                                                &line_end,
                                                line, byte_count - 1);

              if (gtk_text_iter_compare (&selection_start, &line_end) <= 0 &&
                  gtk_text_iter_compare (&selection_end, &line_start) >= 0)
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
                       current_y,
                       selection_start_index, selection_end_index);


          /* We paint the cursors last, because they overlap another chunk
         and need to appear on top. */

          cursor_list = line_display->cursors;
          while (cursor_list)
            {
              GtkTextCursorDisplay *cursor = cursor_list->data;
              GdkGC *gc;

              if (cursor->is_strong)
                gc = widget->style->base_gc[GTK_STATE_SELECTED];
              else
                gc = widget->style->text_gc[GTK_STATE_NORMAL];

              gdk_gc_set_clip_rectangle (gc, &clip);
              gdk_draw_line (drawable, gc,
                             line_display->x_offset + cursor->x - x_offset,
                             current_y + line_display->top_margin + cursor->y,
                             line_display->x_offset + cursor->x - x_offset,
                             current_y + line_display->top_margin + cursor->y + cursor->height - 1);
              gdk_gc_set_clip_rectangle (gc, NULL);

              cursor_list = cursor_list->next;
            }
        } /* line_display->height > 0 */
          
      current_y += line_display->height;
      gtk_text_layout_free_line_display (layout, line_display);
      render_state->last_appearance = NULL;
      render_state->last_bg_appearance = NULL;
      
      tmp_list = g_slist_next (tmp_list);
    }

  gtk_text_layout_wrap_loop_end (layout);
  gtk_text_render_state_destroy (render_state);

  g_slist_free (line_list);
}
