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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
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

#define GTK_TEXT_USE_INTERNAL_UNSUPPORTED_API
#include "config.h"
#include "gtktextdisplay.h"
#include "gtkwidgetprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkintl.h"

/* DO NOT go putting private headers in here. This file should only
 * use the semi-public headers, as with gtktextview.c.
 */

#define GTK_TYPE_TEXT_RENDERER            (_gtk_text_renderer_get_type())
#define GTK_TEXT_RENDERER(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GTK_TYPE_TEXT_RENDERER, GtkTextRenderer))
#define GTK_IS_TEXT_RENDERER(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_TEXT_RENDERER))
#define GTK_TEXT_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_RENDERER, GtkTextRendererClass))
#define GTK_IS_TEXT_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_RENDERER))
#define GTK_TEXT_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TEXT_RENDERER, GtkTextRendererClass))

typedef struct _GtkTextRenderer      GtkTextRenderer;
typedef struct _GtkTextRendererClass GtkTextRendererClass;

enum {
  NORMAL,
  SELECTED,
  CURSOR
};

struct _GtkTextRenderer
{
  PangoRenderer parent_instance;

  GtkWidget *widget;
  cairo_t *cr;
  
  GdkRGBA *error_color;	/* Error underline color for this widget */
  GList *widgets;      	/* widgets encountered when drawing */

  GdkRGBA rgba[4];
  guint8  rgba_set[4];

  guint state : 2;
};

struct _GtkTextRendererClass
{
  PangoRendererClass parent_class;
};

GType _gtk_text_renderer_get_type (void);

G_DEFINE_TYPE (GtkTextRenderer, _gtk_text_renderer, PANGO_TYPE_RENDERER)

static void
text_renderer_set_rgba (GtkTextRenderer *text_renderer,
			PangoRenderPart  part,
			const GdkRGBA   *rgba)
{
  PangoRenderer *renderer = PANGO_RENDERER (text_renderer);
  PangoColor     dummy = { 0, };

  if (rgba)
    {
      text_renderer->rgba[part] = *rgba;
      pango_renderer_set_color (renderer, part, &dummy);
    }
  else
    pango_renderer_set_color (renderer, part, NULL);

  text_renderer->rgba_set[part] = (rgba != NULL);
}

static GtkTextAppearance *
get_item_appearance (PangoItem *item)
{
  GSList *tmp_list = item->analysis.extra_attrs;

  while (tmp_list)
    {
      PangoAttribute *attr = tmp_list->data;

      if (attr->klass->type == gtk_text_attr_appearance_type)
	return &((GtkTextAttrAppearance *)attr)->appearance;

      tmp_list = tmp_list->next;
    }

  return NULL;
}

static void
gtk_text_renderer_prepare_run (PangoRenderer  *renderer,
			       PangoLayoutRun *run)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkTextRenderer *text_renderer = GTK_TEXT_RENDERER (renderer);
  GdkRGBA *bg_rgba = NULL;
  GdkRGBA *fg_rgba = NULL;
  GtkTextAppearance *appearance;

  PANGO_RENDERER_CLASS (_gtk_text_renderer_parent_class)->prepare_run (renderer, run);

  appearance = get_item_appearance (run->item);
  g_assert (appearance != NULL);

  context = gtk_widget_get_style_context (text_renderer->widget);
  state   = gtk_widget_get_state_flags (text_renderer->widget);

  if (appearance->draw_bg && text_renderer->state == NORMAL)
    bg_rgba = appearance->rgba[0];
  else
    bg_rgba = NULL;
  
  text_renderer_set_rgba (text_renderer, PANGO_RENDER_PART_BACKGROUND, bg_rgba);

  if (text_renderer->state == SELECTED)
    {
      state |= GTK_STATE_FLAG_SELECTED;

      gtk_style_context_get (context, state, "color", &fg_rgba, NULL);
    }
  else if (text_renderer->state == CURSOR && gtk_widget_has_focus (text_renderer->widget))
    {
      gtk_style_context_get (context, state,
                             "background-color", &fg_rgba,
                              NULL);
    }
  else
    fg_rgba = appearance->rgba[1];

  text_renderer_set_rgba (text_renderer, PANGO_RENDER_PART_FOREGROUND, fg_rgba);
  text_renderer_set_rgba (text_renderer, PANGO_RENDER_PART_STRIKETHROUGH, fg_rgba);

  if (appearance->underline == PANGO_UNDERLINE_ERROR)
    {
      if (!text_renderer->error_color)
        {
	  GdkColor *color = NULL;

          gtk_style_context_get_style (context,
                                       "error-underline-color", &color,
                                       NULL);

	  if (color)
	    {
	      GdkRGBA   rgba;

	      rgba.red = color->red / 65535.;
	      rgba.green = color->green / 65535.;
	      rgba.blue = color->blue / 65535.;
	      rgba.alpha = 1;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	      gdk_color_free (color);
G_GNUC_END_IGNORE_DEPRECATIONS

	      text_renderer->error_color = gdk_rgba_copy (&rgba);
	    }
	  else
	    {
	      static const GdkRGBA red = { 1, 0, 0, 1 };
	      text_renderer->error_color = gdk_rgba_copy (&red);
	    }
        }

      text_renderer_set_rgba (text_renderer, PANGO_RENDER_PART_UNDERLINE, text_renderer->error_color);
    }
  else
    text_renderer_set_rgba (text_renderer, PANGO_RENDER_PART_UNDERLINE, fg_rgba);

  if (fg_rgba != appearance->rgba[1])
    gdk_rgba_free (fg_rgba);
}

static void
set_color (GtkTextRenderer *text_renderer,
           PangoRenderPart  part)
{
  cairo_save (text_renderer->cr);

  if (text_renderer->rgba_set[part])
    gdk_cairo_set_source_rgba (text_renderer->cr, &text_renderer->rgba[part]);
}

static void
unset_color (GtkTextRenderer *text_renderer)
{
  cairo_restore (text_renderer->cr);
}

static void
gtk_text_renderer_draw_glyphs (PangoRenderer     *renderer,
                               PangoFont         *font,
                               PangoGlyphString  *glyphs,
                               int                x,
                               int                y)
{
  GtkTextRenderer *text_renderer = GTK_TEXT_RENDERER (renderer);

  set_color (text_renderer, PANGO_RENDER_PART_FOREGROUND);

  cairo_move_to (text_renderer->cr, (double)x / PANGO_SCALE, (double)y / PANGO_SCALE);
  pango_cairo_show_glyph_string (text_renderer->cr, font, glyphs);

  unset_color (text_renderer);
}

static void
gtk_text_renderer_draw_glyph_item (PangoRenderer     *renderer,
                                   const char        *text,
                                   PangoGlyphItem    *glyph_item,
                                   int                x,
                                   int                y)
{
  GtkTextRenderer *text_renderer = GTK_TEXT_RENDERER (renderer);

  set_color (text_renderer, PANGO_RENDER_PART_FOREGROUND);

  cairo_move_to (text_renderer->cr, (double)x / PANGO_SCALE, (double)y / PANGO_SCALE);
  pango_cairo_show_glyph_item (text_renderer->cr, text, glyph_item);

  unset_color (text_renderer);
}

static void
gtk_text_renderer_draw_rectangle (PangoRenderer     *renderer,
				  PangoRenderPart    part,
				  int                x,
				  int                y,
				  int                width,
				  int                height)
{
  GtkTextRenderer *text_renderer = GTK_TEXT_RENDERER (renderer);

  set_color (text_renderer, part);

  cairo_rectangle (text_renderer->cr,
                   (double)x / PANGO_SCALE, (double)y / PANGO_SCALE,
		   (double)width / PANGO_SCALE, (double)height / PANGO_SCALE);
  cairo_fill (text_renderer->cr);

  unset_color (text_renderer);
}

static void
gtk_text_renderer_draw_trapezoid (PangoRenderer     *renderer,
				  PangoRenderPart    part,
				  double             y1_,
				  double             x11,
				  double             x21,
				  double             y2,
				  double             x12,
				  double             x22)
{
  GtkTextRenderer *text_renderer = GTK_TEXT_RENDERER (renderer);
  cairo_t *cr;
  cairo_matrix_t matrix;

  set_color (text_renderer, part);

  cr = text_renderer->cr;

  cairo_get_matrix (cr, &matrix);
  matrix.xx = matrix.yy = 1.0;
  matrix.xy = matrix.yx = 0.0;
  cairo_set_matrix (cr, &matrix);

  cairo_move_to (cr, x11, y1_);
  cairo_line_to (cr, x21, y1_);
  cairo_line_to (cr, x22, y2);
  cairo_line_to (cr, x12, y2);
  cairo_close_path (cr);

  cairo_fill (cr);

  unset_color (text_renderer);
}

static void
gtk_text_renderer_draw_error_underline (PangoRenderer *renderer,
					int            x,
					int            y,
					int            width,
					int            height)
{
  GtkTextRenderer *text_renderer = GTK_TEXT_RENDERER (renderer);

  set_color (text_renderer, PANGO_RENDER_PART_UNDERLINE);

  pango_cairo_show_error_underline (text_renderer->cr,
                                    (double)x / PANGO_SCALE, (double)y / PANGO_SCALE,
                                    (double)width / PANGO_SCALE, (double)height / PANGO_SCALE);

  unset_color (text_renderer);
}

static void
gtk_text_renderer_draw_shape (PangoRenderer   *renderer,
			      PangoAttrShape  *attr,
			      int              x,
			      int              y)
{
  GtkTextRenderer *text_renderer = GTK_TEXT_RENDERER (renderer);

  if (attr->data == NULL)
    {
      /* This happens if we have an empty widget anchor. Draw
       * something empty-looking.
       */
      GdkRectangle shape_rect;
      cairo_t *cr;

      shape_rect.x = PANGO_PIXELS (x);
      shape_rect.y = PANGO_PIXELS (y + attr->logical_rect.y);
      shape_rect.width = PANGO_PIXELS (x + attr->logical_rect.width) - shape_rect.x;
      shape_rect.height = PANGO_PIXELS (y + attr->logical_rect.y + attr->logical_rect.height) - shape_rect.y;

      set_color (text_renderer, PANGO_RENDER_PART_FOREGROUND);

      cr = text_renderer->cr;

      cairo_set_line_width (cr, 1.0);

      cairo_rectangle (cr,
                       shape_rect.x + 0.5, shape_rect.y + 0.5,
                       shape_rect.width - 1, shape_rect.height - 1);
      cairo_move_to (cr, shape_rect.x + 0.5, shape_rect.y + 0.5);
      cairo_line_to (cr, 
                     shape_rect.x + shape_rect.width - 0.5,
                     shape_rect.y + shape_rect.height - 0.5);
      cairo_move_to (cr, shape_rect.x + 0.5,
                     shape_rect.y + shape_rect.height - 0.5);
      cairo_line_to (cr, shape_rect.x + shape_rect.width - 0.5,
                     shape_rect.y + 0.5);

      cairo_stroke (cr);

      unset_color (text_renderer);
    }
  else if (GDK_IS_PIXBUF (attr->data))
    {
      cairo_t *cr = text_renderer->cr;
      GdkPixbuf *pixbuf = GDK_PIXBUF (attr->data);
      
      cairo_save (cr);

      gdk_cairo_set_source_pixbuf (cr, pixbuf,
                                   PANGO_PIXELS (x),
                                   PANGO_PIXELS (y) -  gdk_pixbuf_get_height (pixbuf));
      cairo_paint (cr);

      cairo_restore (cr);
    }
  else if (GTK_IS_WIDGET (attr->data))
    {
      GtkWidget *widget;
      
      widget = GTK_WIDGET (attr->data);

      text_renderer->widgets = g_list_prepend (text_renderer->widgets,
					       g_object_ref (widget));
    }
  else
    g_assert_not_reached (); /* not a pixbuf or widget */
}

static void
gtk_text_renderer_finalize (GObject *object)
{
  G_OBJECT_CLASS (_gtk_text_renderer_parent_class)->finalize (object);
}

static void
_gtk_text_renderer_init (GtkTextRenderer *renderer)
{
}

static void
_gtk_text_renderer_class_init (GtkTextRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  PangoRendererClass *renderer_class = PANGO_RENDERER_CLASS (klass);
  
  renderer_class->prepare_run = gtk_text_renderer_prepare_run;
  renderer_class->draw_glyphs = gtk_text_renderer_draw_glyphs;
  renderer_class->draw_glyph_item = gtk_text_renderer_draw_glyph_item;
  renderer_class->draw_rectangle = gtk_text_renderer_draw_rectangle;
  renderer_class->draw_trapezoid = gtk_text_renderer_draw_trapezoid;
  renderer_class->draw_error_underline = gtk_text_renderer_draw_error_underline;
  renderer_class->draw_shape = gtk_text_renderer_draw_shape;

  object_class->finalize = gtk_text_renderer_finalize;
}

static void
text_renderer_set_state (GtkTextRenderer *text_renderer,
			 int              state)
{
  text_renderer->state = state;
}

static void
text_renderer_begin (GtkTextRenderer *text_renderer,
                     GtkWidget       *widget,
                     cairo_t         *cr)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRGBA color;

  text_renderer->widget = widget;
  text_renderer->cr = cr;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);

  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_color (context, state, &color);

  cairo_save (cr);

  gdk_cairo_set_source_rgba (cr, &color);
}

/* Returns a GSList of (referenced) widgets encountered while drawing.
 */
static GList *
text_renderer_end (GtkTextRenderer *text_renderer)
{
  GtkStyleContext *context;
  GList *widgets = text_renderer->widgets;

  cairo_restore (text_renderer->cr);

  context = gtk_widget_get_style_context (text_renderer->widget);

  gtk_style_context_restore (context);

  text_renderer->widget = NULL;
  text_renderer->cr = NULL;

  text_renderer->widgets = NULL;

  if (text_renderer->error_color)
    {
      gdk_rgba_free (text_renderer->error_color);
      text_renderer->error_color = NULL;
    }

  return widgets;
}

static cairo_region_t *
get_selected_clip (GtkTextRenderer    *text_renderer,
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
  cairo_region_t *clip_region = cairo_region_create ();

  pango_layout_line_get_x_ranges (line, start_index, end_index, &ranges, &n_ranges);

  for (i=0; i < n_ranges; i++)
    {
      GdkRectangle rect;

      rect.x = x + PANGO_PIXELS (ranges[2*i]);
      rect.y = y;
      rect.width = PANGO_PIXELS (ranges[2*i + 1]) - PANGO_PIXELS (ranges[2*i]);
      rect.height = height;
      
      cairo_region_union_rectangle (clip_region, &rect);
    }

  g_free (ranges);
  return clip_region;
}

static void
render_para (GtkTextRenderer    *text_renderer,
             GtkTextLineDisplay *line_display,
             int                 selection_start_index,
             int                 selection_end_index)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  PangoLayout *layout = line_display->layout;
  int byte_offset = 0;
  PangoLayoutIter *iter;
  PangoRectangle layout_logical;
  int screen_width;
  GdkRGBA selection;
  gboolean first = TRUE;

  iter = pango_layout_get_iter (layout);

  pango_layout_iter_get_layout_extents (iter, NULL, &layout_logical);

  /* Adjust for margins */
  
  layout_logical.x += line_display->x_offset * PANGO_SCALE;
  layout_logical.y += line_display->top_margin * PANGO_SCALE;

  screen_width = line_display->total_width;

  context = gtk_widget_get_style_context (text_renderer->widget);
  gtk_style_context_save (context);

  state = gtk_style_context_get_state (context);
  state |= GTK_STATE_FLAG_SELECTED;
  gtk_style_context_set_state (context, state);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_style_context_get_background_color (context, state, &selection);
G_GNUC_END_IGNORE_DEPRECATIONS

 gtk_style_context_restore (context);

  do
    {
      PangoLayoutLine *line = pango_layout_iter_get_line_readonly (iter);
      int selection_y, selection_height;
      int first_y, last_y;
      PangoRectangle line_rect;
      int baseline;
      gboolean at_last_line;
      
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
      selection_y = PANGO_PIXELS (first_y) + line_display->top_margin;
      selection_height = PANGO_PIXELS (last_y) - PANGO_PIXELS (first_y);

      if (first)
        {
          selection_y -= line_display->top_margin;
          selection_height += line_display->top_margin;
        }

      at_last_line = pango_layout_iter_at_last_line (iter);
      if (at_last_line)
        selection_height += line_display->bottom_margin;
      
      first = FALSE;

      if (selection_start_index < byte_offset &&
          selection_end_index > line->length + byte_offset) /* All selected */
        {
          cairo_t *cr = text_renderer->cr;

          cairo_save (cr);
          gdk_cairo_set_source_rgba (cr, &selection);
          cairo_rectangle (cr, 
                           line_display->left_margin, selection_y,
                           screen_width, selection_height);
          cairo_fill (cr);
          cairo_restore(cr);

	  text_renderer_set_state (text_renderer, SELECTED);
	  pango_renderer_draw_layout_line (PANGO_RENDERER (text_renderer),
					   line, 
					   line_rect.x,
					   baseline);
        }
      else
        {
          if (line_display->pg_bg_rgba)
            {
              cairo_t *cr = text_renderer->cr;

              cairo_save (cr);
 
	      gdk_cairo_set_source_rgba (text_renderer->cr, line_display->pg_bg_rgba);
              cairo_rectangle (cr, 
                               line_display->left_margin, selection_y,
                               screen_width, selection_height);
              cairo_fill (cr);

              cairo_restore (cr);
            }
        
	  text_renderer_set_state (text_renderer, NORMAL);
	  pango_renderer_draw_layout_line (PANGO_RENDERER (text_renderer),
					   line, 
					   line_rect.x,
					   baseline);

	  /* Check if some part of the line is selected; the newline
	   * that is after line->length for the last line of the
	   * paragraph counts as part of the line for this
	   */
          if ((selection_start_index < byte_offset + line->length ||
	       (selection_start_index == byte_offset + line->length && pango_layout_iter_at_last_line (iter))) &&
	      selection_end_index > byte_offset)
            {
              cairo_t *cr = text_renderer->cr;
              cairo_region_t *clip_region = get_selected_clip (text_renderer, layout, line,
                                                          line_display->x_offset,
                                                          selection_y,
                                                          selection_height,
                                                          selection_start_index, selection_end_index);

              cairo_save (cr);
              gdk_cairo_region (cr, clip_region);
              cairo_clip (cr);
              cairo_region_destroy (clip_region);

              gdk_cairo_set_source_rgba (cr, &selection);
              cairo_rectangle (cr,
                               PANGO_PIXELS (line_rect.x),
                               selection_y,
                               PANGO_PIXELS (line_rect.width),
                               selection_height);
              cairo_fill (cr);

	      text_renderer_set_state (text_renderer, SELECTED);
	      pango_renderer_draw_layout_line (PANGO_RENDERER (text_renderer),
					       line, 
					       line_rect.x,
					       baseline);

              cairo_restore (cr);

              /* Paint in the ends of the line */
              if (line_rect.x > line_display->left_margin * PANGO_SCALE &&
                  ((line_display->direction == GTK_TEXT_DIR_LTR && selection_start_index < byte_offset) ||
                   (line_display->direction == GTK_TEXT_DIR_RTL && selection_end_index > byte_offset + line->length)))
                {
                  cairo_save (cr);

                  gdk_cairo_set_source_rgba (cr, &selection);
                  cairo_rectangle (cr,
                                   line_display->left_margin,
                                   selection_y,
                                   PANGO_PIXELS (line_rect.x) - line_display->left_margin,
                                   selection_height);
                  cairo_fill (cr);

                  cairo_restore (cr);
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

                  cairo_save (cr);

                  gdk_cairo_set_source_rgba (cr, &selection);
                  cairo_rectangle (cr,
                                   PANGO_PIXELS (line_rect.x) + PANGO_PIXELS (line_rect.width),
                                   selection_y,
                                   nonlayout_width,
                                   selection_height);
                  cairo_fill (cr);

                  cairo_restore (cr);
                }
            }
	  else if (line_display->has_block_cursor &&
		   gtk_widget_has_focus (text_renderer->widget) &&
		   byte_offset <= line_display->insert_index &&
		   (line_display->insert_index < byte_offset + line->length ||
		    (at_last_line && line_display->insert_index == byte_offset + line->length)))
	    {
	      GdkRectangle cursor_rect;
              GdkRGBA cursor_color;
              cairo_t *cr = text_renderer->cr;

              /* we draw text using base color on filled cursor rectangle of cursor color
               * (normally white on black) */
              _gtk_style_context_get_cursor_color (context, &cursor_color, NULL);

	      cursor_rect.x = line_display->x_offset + line_display->block_cursor.x;
	      cursor_rect.y = line_display->block_cursor.y + line_display->top_margin;
	      cursor_rect.width = line_display->block_cursor.width;
	      cursor_rect.height = line_display->block_cursor.height;

              cairo_save (cr);

              gdk_cairo_rectangle (cr, &cursor_rect);
              cairo_clip (cr);

              gdk_cairo_set_source_rgba (cr, &cursor_color);
              cairo_paint (cr);

              /* draw text under the cursor if any */
              if (!line_display->cursor_at_line_end)
                {
                  GdkRGBA color;

                  state = gtk_widget_get_state_flags (text_renderer->widget);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                  gtk_style_context_get_background_color (context, state, &color);
G_GNUC_END_IGNORE_DEPRECATIONS

                  gdk_cairo_set_source_rgba (cr, &color);

		  text_renderer_set_state (text_renderer, CURSOR);

		  pango_renderer_draw_layout_line (PANGO_RENDERER (text_renderer),
						   line,
						   line_rect.x,
						   baseline);
                }

              cairo_restore (cr);
	    }
        }

      byte_offset += line->length;
    }
  while (pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);
}

static GtkTextRenderer *
get_text_renderer (void)
{
  static GtkTextRenderer *text_renderer = NULL;

  if (!text_renderer)
    text_renderer = g_object_new (GTK_TYPE_TEXT_RENDERER, NULL);

  return text_renderer;
}

void
gtk_text_layout_draw (GtkTextLayout *layout,
                      GtkWidget *widget,
                      cairo_t *cr,
                      GList **widgets)
{
  GtkStyleContext *context;
  gint offset_y;
  GtkTextRenderer *text_renderer;
  GtkTextIter selection_start, selection_end;
  gboolean have_selection;
  GSList *line_list;
  GSList *tmp_list;
  GList *tmp_widgets;
  GdkRectangle clip;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (layout->default_style != NULL);
  g_return_if_fail (layout->buffer != NULL);
  g_return_if_fail (cr != NULL);

  if (!gdk_cairo_get_clip_rectangle (cr, &clip))
    return;

  context = gtk_widget_get_style_context (widget);

  line_list = gtk_text_layout_get_lines (layout, clip.y, clip.y + clip.height, &offset_y);

  if (line_list == NULL)
    return; /* nothing on the screen */

  text_renderer = get_text_renderer ();
  text_renderer_begin (text_renderer, widget, cr);

  /* text_renderer_begin/end does cairo_save/restore */
  cairo_translate (cr, 0, offset_y);

  gtk_text_layout_wrap_loop_start (layout);

  have_selection = gtk_text_buffer_get_selection_bounds (layout->buffer,
                                                         &selection_start,
                                                         &selection_end);

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
              line_end = line_start;
	      if (!gtk_text_iter_ends_line (&line_end))
		gtk_text_iter_forward_to_line_end (&line_end);
              byte_count = gtk_text_iter_get_visible_line_index (&line_end);     

              if (gtk_text_iter_compare (&selection_start, &line_end) <= 0 &&
                  gtk_text_iter_compare (&selection_end, &line_start) >= 0)
                {
                  if (gtk_text_iter_compare (&selection_start, &line_start) >= 0)
                    selection_start_index = gtk_text_iter_get_visible_line_index (&selection_start);
                  else
                    selection_start_index = -1;

                  if (gtk_text_iter_compare (&selection_end, &line_end) <= 0)
                    selection_end_index = gtk_text_iter_get_visible_line_index (&selection_end);
                  else
                    selection_end_index = byte_count + 1; /* + 1 to flag past-the-end */
                }
            }

          render_para (text_renderer, line_display,
                       selection_start_index, selection_end_index);

          /* We paint the cursors last, because they overlap another chunk
           * and need to appear on top.
           */
          if (line_display->cursors != NULL)
            {
              int i;

              for (i = 0; i < line_display->cursors->len; i++)
                {
                  int index;
                  PangoDirection dir;

                  index = g_array_index(line_display->cursors, int, i);
                  dir = (line_display->direction == GTK_TEXT_DIR_RTL) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
                  gtk_render_insertion_cursor (context, cr,
                                               line_display->x_offset, line_display->top_margin,
                                               line_display->layout, index, dir);
                }
            }
        } /* line_display->height > 0 */

      cairo_translate (cr, 0, line_display->height);
      gtk_text_layout_free_line_display (layout, line_display);
      
      tmp_list = g_slist_next (tmp_list);
    }

  gtk_text_layout_wrap_loop_end (layout);

  tmp_widgets = text_renderer_end (text_renderer);
  if (widgets)
    *widgets = tmp_widgets;
  else
    g_list_free_full (tmp_widgets, g_object_unref);

  g_slist_free (line_list);
}
