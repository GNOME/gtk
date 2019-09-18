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
add_border_window (GtkTextView       *text_view,
                   GtkTextWindowType  window_type)
{
  GtkWidget *box;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_size_request (box, 30, 30);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_widget_set_vexpand (box, TRUE);
  gtk_text_view_set_gutter (text_view, window_type, box);
}

G_MODULE_EXPORT void
add_border_windows (GtkTextView *text_view)
{
  add_border_window (text_view, GTK_TEXT_WINDOW_LEFT);
  add_border_window (text_view, GTK_TEXT_WINDOW_RIGHT);
  add_border_window (text_view, GTK_TEXT_WINDOW_TOP);
  add_border_window (text_view, GTK_TEXT_WINDOW_BOTTOM);
}
