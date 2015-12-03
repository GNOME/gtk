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
  const gchar *class1;
  const gchar *class2;
} PathElt;

static GtkStyleContext *
get_style (PathElt *pelt, GtkStyleContext *parent)
{
  GtkWidgetPath *path;
  GtkStyleContext *context;

  if (parent)
    path = gtk_widget_path_copy (gtk_style_context_get_path (parent));
  else
    path = gtk_widget_path_new ();

  gtk_widget_path_append_type (path, pelt->type);
  if (pelt->name)
    gtk_widget_path_iter_set_object_name (path, -1, pelt->name);
  if (pelt->class1)
    gtk_widget_path_iter_add_class (path, -1, pelt->class1);
  if (pelt->class2)
    gtk_widget_path_iter_add_class (path, -1, pelt->class2);

  context = gtk_style_context_new ();
  gtk_style_context_set_path (context, path);
  gtk_style_context_set_parent (context, parent);
  gtk_widget_path_unref (path);

  return context;
}

static void
draw_horizontal_scrollbar (GtkWidget     *widget,
                           cairo_t       *cr,
                           gint           x,
                           gint           y,
                           gint           width,
                           gint           height,
                           gint           position,
                           GtkStateFlags  state)
{
  GtkStyleContext *scrollbar_context;
  GtkStyleContext *trough_context;
  GtkStyleContext *slider_context;

  /* This information is taken from the GtkScrollbar docs, see "CSS nodes" */
  PathElt path[3] = {
    { GTK_TYPE_SCROLLBAR, "scrollbar", "horizontal", NULL },
    { G_TYPE_NONE, "trough", NULL, NULL },
    { G_TYPE_NONE, "slider", NULL, NULL }
  };

  scrollbar_context = get_style (&path[0], NULL);
  trough_context = get_style (&path[1], scrollbar_context);
  slider_context = get_style (&path[2], trough_context);

  gtk_style_context_set_state (scrollbar_context, state);
  gtk_style_context_set_state (trough_context, state);
  gtk_style_context_set_state (slider_context, state);

  gtk_render_background (trough_context, cr, x, y, width, height);
  gtk_render_frame (trough_context, cr, x, y, width, height);
  gtk_render_slider (slider_context, cr, x + position, y + 1, 30, height - 2, GTK_ORIENTATION_HORIZONTAL);

  g_object_unref (slider_context);
  g_object_unref (trough_context);
  g_object_unref (scrollbar_context);
}

static void
draw_text (GtkWidget     *widget,
           cairo_t       *cr,
           gint           x,
           gint           y,
           gint           width,
           gint           height,
           const gchar   *text,
           GtkStateFlags  state)
{
  GtkStyleContext *label_context;
  GtkStyleContext *selection_context;
  GtkStyleContext *context;
  PangoLayout *layout;

  /* This information is taken from the GtkLabel docs, see "CSS nodes" */
  PathElt path[2] = {
    { GTK_TYPE_LABEL, "label", "view", NULL },
    { G_TYPE_NONE, "selection", NULL, NULL }
  };

  label_context = get_style (&path[0], NULL);
  selection_context = get_style (&path[1], label_context);

  gtk_style_context_set_state (label_context, state);

  if (state & GTK_STATE_FLAG_SELECTED)
    context = selection_context;
  else
    context = label_context;

  layout = gtk_widget_create_pango_layout (widget, text);

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);
  gtk_render_layout (context, cr, x, y, layout);

  g_object_unref (layout);

  g_object_unref (selection_context);
  g_object_unref (label_context);
}

static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr)
{
  gint width;

  width = gtk_widget_get_allocated_width (widget);

  draw_horizontal_scrollbar (widget, cr, 10, 10, width - 20, 10, 30, GTK_STATE_FLAG_NORMAL);
  draw_horizontal_scrollbar (widget, cr, 10, 30, width - 20, 10, 40, GTK_STATE_FLAG_PRELIGHT);
  draw_horizontal_scrollbar (widget, cr, 10, 50, width - 20, 10, 50, GTK_STATE_FLAG_ACTIVE|GTK_STATE_FLAG_PRELIGHT);

  draw_text (widget, cr, 10,  70, width - 20, 20, "Not selected", GTK_STATE_FLAG_NORMAL);
  draw_text (widget, cr, 10, 100, width - 20, 20, "Selected", GTK_STATE_FLAG_SELECTED);

  return FALSE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *da;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_app_paintable (window, TRUE);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (window), box);
  da = gtk_drawing_area_new ();
  gtk_widget_set_size_request (da, 200, 200);
  gtk_widget_set_app_paintable (da, TRUE);
  gtk_container_add (GTK_CONTAINER (box), da);

  g_signal_connect (da, "draw", G_CALLBACK (draw_cb), NULL);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
