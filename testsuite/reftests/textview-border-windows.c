/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>

static void
paint_border (GtkTextView       *text_view,
              cairo_t           *cr,
              GtkTextWindowType  type,
              GtkStyleContext   *context,
              const char        *class)
{
  GdkWindow *window;

  window = gtk_text_view_get_window (text_view, type);

  if (window != NULL &&
      gtk_cairo_should_draw_window (cr, window))
    {
      gint w, h;

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, class);

      w = gdk_window_get_width (window);
      h = gdk_window_get_height (window);

      gtk_cairo_transform_to_window (cr, GTK_WIDGET (text_view), window);

      cairo_save (cr);
      gtk_render_background (context, cr, 0, 0, w, h);
      cairo_restore (cr);

      gtk_style_context_restore (context);
    }
}

G_MODULE_EXPORT gboolean
paint_border_windows (GtkTextView *text_view,
                      cairo_t     *cr)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (text_view));

  paint_border (text_view, cr, GTK_TEXT_WINDOW_LEFT, context, "left");
  paint_border (text_view, cr, GTK_TEXT_WINDOW_RIGHT, context, "right");
  paint_border (text_view, cr, GTK_TEXT_WINDOW_TOP, context, "top");
  paint_border (text_view, cr, GTK_TEXT_WINDOW_BOTTOM, context, "bottom");

  return FALSE;
}

G_MODULE_EXPORT void
add_border_windows (GtkTextView *text_view)
{
  gtk_text_view_set_border_window_size (text_view, GTK_TEXT_WINDOW_LEFT, 30);
  gtk_text_view_set_border_window_size (text_view, GTK_TEXT_WINDOW_RIGHT, 30);
  gtk_text_view_set_border_window_size (text_view, GTK_TEXT_WINDOW_TOP, 30);
  gtk_text_view_set_border_window_size (text_view, GTK_TEXT_WINDOW_BOTTOM, 30);
}
