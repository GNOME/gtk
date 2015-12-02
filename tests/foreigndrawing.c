/* foreign-drawing.c
 * Copyright (C) 2015 Red Hat, Inc
 * Author: Matthias Clasen
 *
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

#include <gtk/gtk.h>

typedef struct {
  GType type;
  const gchar *name;
  const gchar *class;
  GtkStateFlags state;
} PathElt;

static GtkStyleContext *
get_style (PathElt pelt[], gint n_elts)
{
  GtkWidgetPath *path;
  gint i;
  GtkStyleContext *context;

  path = gtk_widget_path_new ();

  for (i = 0; i < n_elts; i++)
    {
      gtk_widget_path_append_type (path, pelt[i].type);
      if (pelt[i].name)
        gtk_widget_path_iter_set_object_name (path, i, pelt[i].name);
      if (pelt[i].class)
        gtk_widget_path_iter_add_class (path, i, pelt[i].class);
      gtk_widget_path_iter_set_state (path, i, pelt[i].state);
    }

  context = gtk_style_context_new ();
  gtk_style_context_set_path (context, path);
  gtk_widget_path_unref (path);

  return context;
}

static void
draw_horizontal_scrollbar (GtkWidget *widget,
                           cairo_t   *cr,
                           gint       x,
                           gint       y,
                           gint       width,
                           gint       height,
                           gint       position)
{
  GtkStyleContext *context;

  PathElt trough[2] = {
    { GTK_TYPE_SCROLLBAR, "scrollbar", "horizontal", GTK_STATE_FLAG_NORMAL },
    { G_TYPE_NONE, "trough", NULL, GTK_STATE_FLAG_NORMAL }
  };

  PathElt slider[3] = {
    { GTK_TYPE_SCROLLBAR, "scrollbar", "horizontal", GTK_STATE_FLAG_NORMAL },
    { G_TYPE_NONE, "trough", NULL, GTK_STATE_FLAG_NORMAL },
    { G_TYPE_NONE, "slider", NULL, GTK_STATE_FLAG_NORMAL }
  };

  context = get_style (trough, G_N_ELEMENTS (trough));

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);

  g_object_unref (context);

  context = get_style (slider, G_N_ELEMENTS (slider));

  gtk_render_slider (context, cr, x + position, y + 1, position + 30, height - 2, GTK_ORIENTATION_HORIZONTAL);

  g_object_unref (context);
}

static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr)
{
  gint width;

  width = gtk_widget_get_allocated_width (widget);

  draw_horizontal_scrollbar (widget, cr, 10, 10, width - 20, 10, 30);

  return FALSE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, 200, 200);
  gtk_widget_set_app_paintable (window, TRUE);

  g_signal_connect (window, "draw", G_CALLBACK (draw_cb), NULL);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
