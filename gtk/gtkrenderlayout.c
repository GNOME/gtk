/* GTK - The GIMP Toolkit
 * Copyright (C) 2022 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkrenderlayoutprivate.h"

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssshadowvalueprivate.h"
#include "gtkpangoprivate.h"
#include "gtksnapshotprivate.h"
#include "gtktypebuiltins.h"
#include "gtksettings.h"
#include "gdkcairoprivate.h"
#include "gdkcolorprivate.h"


static void
get_text_bounds (PangoLayout     *layout,
                 graphene_rect_t *bounds)
{
  PangoRectangle ink_rect;
  pango_layout_get_pixel_extents (layout, &ink_rect, NULL);
  graphene_rect_init (bounds, ink_rect.x, ink_rect.y, ink_rect.width, ink_rect.height);
}

void
gtk_css_style_snapshot_layout (GtkCssBoxes *boxes,
                               GtkSnapshot *snapshot,
                               int          x,
                               int          y,
                               PangoLayout *layout)
{
  GtkCssStyle *style;
  GdkColor text_color;

  gtk_snapshot_push_debug (snapshot, "Layout");

  if (x != 0 || y != 0)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
    }

  style = boxes->style;
  gtk_css_color_to_color (gtk_css_color_value_get_color (style->used->color), &text_color);

  if (!gtk_css_shadow_value_is_clear (style->used->text_shadow))
    {
      for (guint i = 0; i < gtk_css_shadow_value_get_n_shadows (style->used->text_shadow); i++)
        {
          graphene_point_t offset;
          GdkColor color;
          double radius;

          gtk_css_shadow_value_get_offset (style->used->text_shadow, i, &offset);
          gtk_css_shadow_value_get_color (style->used->text_shadow, i, &color);
          radius = gtk_css_shadow_value_get_radius (style->used->text_shadow, i);

          gtk_snapshot_save (snapshot);
          gtk_snapshot_translate (snapshot, &offset);

          if (radius != 0)
            gtk_snapshot_push_blur (snapshot, radius);

          if (gtk_pango_layout_has_color_glyphs (layout))
            {
              GdkColor black = GDK_COLOR_SRGB (0, 0, 0, 1);
              graphene_rect_t bounds;

              get_text_bounds (layout, &bounds);
              gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_ALPHA);
              gtk_snapshot_add_layout (snapshot, layout, &black);
              gtk_snapshot_pop (snapshot);
              gtk_snapshot_add_color (snapshot, &color, &bounds);
              gtk_snapshot_pop (snapshot);
              gdk_color_finish (&black);
            }
          else
            {
              gtk_snapshot_add_layout (snapshot, layout, &color);
            }

          if (radius != 0)
            gtk_snapshot_pop (snapshot);

          gtk_snapshot_restore (snapshot);
        }
    }

  gtk_snapshot_add_layout (snapshot, layout, &text_color);

  gdk_color_finish (&text_color);

  if (x != 0 || y != 0)
    gtk_snapshot_restore (snapshot);

  gtk_snapshot_pop (snapshot);
}

static void
draw_insertion_cursor (cairo_t         *cr,
                       double           x,
                       double           y,
                       double           width,
                       double           height,
                       double           aspect_ratio,
                       const GdkColor  *color,
                       PangoDirection   direction,
                       gboolean         draw_arrow)
{
  int stem_width;
  double angle;
  double dx, dy;
  double xx1, yy1, xx2, yy2;

  cairo_save (cr);
  cairo_new_path (cr);

  gdk_cairo_set_source_color (cr, GDK_COLOR_STATE_SRGB, color);

  stem_width = height * aspect_ratio + 1;

  yy1 = y;
  yy2 = y + height;

  if (width < 0)
    {
      xx1 = x;
      xx2 = x - width;
    }
  else
    {
      xx1 = x + width;
      xx2 = x;
    }

  angle = atan2 (height, width);

  dx = (stem_width/2.0) * cos (M_PI/2 - angle);
  dy = (stem_width/2.0) * sin (M_PI/2 - angle);

  if (draw_arrow)
    {
      if (direction == PANGO_DIRECTION_RTL)
        {
          double x0, y0, x1, y1, x2, y2;

          x0 = xx2 - dx + 2 * dy;
          y0 = yy2 - dy - 2 * dx;

          x1 = x0 + 4 * dy;
          y1 = y0 - 4 * dx;
          x2 = x0 + 2 * dy - 3 * dx;
          y2 = y0 - 2 * dx - 3 * dy;

          cairo_move_to (cr, xx1 + dx, yy1 + dy);
          cairo_line_to (cr, xx2 + dx, yy2 + dy);
          cairo_line_to (cr, x2, y2);
          cairo_line_to (cr, x1, y1);
          cairo_line_to (cr, xx1 - dx, yy1 - dy);
        }
      else if (direction == PANGO_DIRECTION_LTR)
        {
          double x0, y0, x1, y1, x2, y2;

          x0 = xx2 + dx + 2 * dy;
          y0 = yy2 + dy - 2 * dx;

          x1 = x0 + 4 * dy;
          y1 = y0 - 4 * dx;
          x2 = x0 + 2 * dy + 3 * dx;
          y2 = y0 - 2 * dx + 3 * dy;

          cairo_move_to (cr, xx1 - dx, yy1 - dy);
          cairo_line_to (cr, xx2 - dx, yy2 - dy);
          cairo_line_to (cr, x2, y2);
          cairo_line_to (cr, x1, y1);
          cairo_line_to (cr, xx1 + dx, yy1 + dy);
        }
      else
        g_assert_not_reached();
    }
  else
    {
      cairo_move_to (cr, xx1 + dx, yy1 + dy);
      cairo_line_to (cr, xx2 + dx, yy2 + dy);
      cairo_line_to (cr, xx2 - dx, yy2 - dy);
      cairo_line_to (cr, xx1 - dx, yy1 - dy);
    }

  cairo_fill (cr);

  cairo_restore (cr);
}

static void
get_insertion_cursor_bounds (double           width,
                             double           height,
                             double           aspect_ratio,
                             PangoDirection   direction,
                             gboolean         draw_arrow,
                             graphene_rect_t *bounds)
{
  int stem_width;

  if (width < 0)
    width = - width;

  stem_width = height * aspect_ratio + 1;

  graphene_rect_init (bounds,
                      - 2 * stem_width, - stem_width,
                      width + 4 * stem_width, height + 2 * stem_width);
}

static void
snapshot_insertion_cursor (GtkSnapshot     *snapshot,
                           GtkCssStyle     *style,
                           double           width,
                           double           height,
                           double           aspect_ratio,
                           gboolean         is_primary,
                           PangoDirection   direction,
                           gboolean         draw_arrow)
{
  GdkColor color;

  gtk_css_color_to_color (is_primary
                            ? gtk_css_color_value_get_color (style->used->caret_color)
                            : gtk_css_color_value_get_color (style->used->secondary_caret_color),
                          &color);

  if (width != 0 || draw_arrow)
    {
      cairo_t *cr;
      graphene_rect_t bounds;

      get_insertion_cursor_bounds (width, height, aspect_ratio, direction, draw_arrow, &bounds);
      cr = gtk_snapshot_append_cairo (snapshot, &bounds);

      draw_insertion_cursor (cr, 0, 0, width, height, aspect_ratio, &color, direction, draw_arrow);

      cairo_destroy (cr);
    }
  else
    {
      int stem_width;
      int offset;

      stem_width = height * aspect_ratio + 1;

      /* put (stem_width % 2) on the proper side of the cursor */
      if (direction == PANGO_DIRECTION_LTR)
        offset = stem_width / 2;
      else
        offset = stem_width - stem_width / 2;

      gtk_snapshot_add_color (snapshot,
                              &color,
                              &GRAPHENE_RECT_INIT (- offset, 0, stem_width, height));
    }
}

void
gtk_css_style_snapshot_caret (GtkCssBoxes    *boxes,
                              GdkDisplay     *display,
                              GtkSnapshot    *snapshot,
                              int             x,
                              int             y,
                              PangoLayout    *layout,
                              int             index,
                              PangoDirection  direction)
{
  gboolean split_cursor;
  double aspect_ratio;
  PangoRectangle strong_pos, weak_pos;
  PangoRectangle *cursor1, *cursor2;
  GdkSeat *seat;
  PangoDirection keyboard_direction;
  PangoDirection direction2;
  gboolean width_was_zero;

  g_object_get (gtk_settings_get_for_display (display),
                "gtk-split-cursor", &split_cursor,
                "gtk-cursor-aspect-ratio", &aspect_ratio,
                NULL);

  keyboard_direction = PANGO_DIRECTION_LTR;
  seat = gdk_display_get_default_seat (display);
  if (seat)
    {
      GdkDevice *keyboard = gdk_seat_get_keyboard (seat);

      if (keyboard)
        keyboard_direction = gdk_device_get_direction (keyboard);
    }

  pango_layout_get_caret_pos (layout, index, &strong_pos, &weak_pos);

  /* We special-case a width of zero here, because we don't want
   * an upright cursor go sloped due to rounding.
   */
  width_was_zero = strong_pos.width == 0;
  pango_extents_to_pixels (&strong_pos, NULL);
  if (width_was_zero)
    strong_pos.width = 0;

  width_was_zero = weak_pos.width == 0;
  pango_extents_to_pixels (&weak_pos, NULL);
  if (width_was_zero)
    weak_pos.width = 0;

  direction2 = PANGO_DIRECTION_NEUTRAL;
  cursor2 = NULL; /* poor MSVC */

  if (split_cursor)
    {
      cursor1 = &strong_pos;

      if (strong_pos.x != weak_pos.x || strong_pos.y != weak_pos.y)
        {
          direction2 = (direction == PANGO_DIRECTION_LTR) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
          cursor2 = &weak_pos;
        }
    }
  else
    {
      if (keyboard_direction == direction)
        cursor1 = &strong_pos;
      else
        cursor1 = &weak_pos;
    }

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x + MIN (cursor1->x, cursor1->x + cursor1->width), y + cursor1->y));
  snapshot_insertion_cursor (snapshot,
                             boxes->style,
                             cursor1->width,
                             cursor1->height,
                             aspect_ratio,
                             TRUE,
                             direction,
                             direction2 != PANGO_DIRECTION_NEUTRAL);
  gtk_snapshot_restore (snapshot);

  if (direction2 != PANGO_DIRECTION_NEUTRAL)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x + MIN (cursor2->x, cursor2->x + cursor2->width), y + cursor2->y));
      snapshot_insertion_cursor (snapshot,
                                 boxes->style,
                                 cursor2->width,
                                 cursor2->height,
                                 aspect_ratio,
                                 FALSE,
                                 direction2,
                                 TRUE);
      gtk_snapshot_restore (snapshot);
    }
}

