/* simple.c
 * Copyright (C) 1997  Red Hat, Inc
 * Author: Elliot Lee
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
#include "config.h"
#include <gtk/gtk.h>

static void
drag_begin_cb (GtkWidget      *widget,
               GdkDragContext *context,
               gpointer        data)
{
  char **uris;
  char *cwd;

  cwd = g_get_current_dir ();
  uris = g_new0 (char *, 2);
  uris[0] = g_strconcat ("file://", cwd, "/README.md", NULL);
  g_free (cwd);

  g_signal_handlers_disconnect_by_func (widget, drag_begin_cb, NULL);
  gtk_drag_set_icon_default (context);

  g_object_set_data_full (G_OBJECT (widget), "uris", g_strdupv ((char **)uris), (GDestroyNotify) g_strfreev);
}

static void
drag_data_get (GtkWidget        *widget,
               GdkDragContext   *context,
               GtkSelectionData *selection,
               unsigned int      target_info,
               unsigned int      time,
               gpointer          data)
{
  char **uris = (char **)g_object_get_data (G_OBJECT (widget), "uris");

  gtk_selection_data_set_uris (selection, uris);
}

static void
drag_data_received (GtkWidget        *widget,
                    GdkDragContext   *context,
                    int               x,
                    int               y,
                    GtkSelectionData *selection_data,
                    unsigned int      info,
                    unsigned int      time,
                    gpointer          user_data)
{
  GtkLabel *label = user_data;
  char **uris;

  uris = gtk_selection_data_get_uris (selection_data);

  if (uris)
    {
      gtk_label_set_label (label, uris[0]);
      g_strfreev (uris);
    }
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *label, *eventbox, *box;
  GtkTargetEntry targets[] = {
    { "application/vnd.portal.files", 0, 0 },
  };

  gtk_init (&argc, &argv);

  window = g_object_connect (g_object_new (gtk_window_get_type (),
                                           "type", GTK_WINDOW_TOPLEVEL,
                                           "title", "hello world",
                                           "resizable", FALSE,
                                           "border_width", 10,
                                           NULL),
                             "signal::destroy", gtk_main_quit, NULL,
                             NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (window), box);

  eventbox = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (box), eventbox);
  gtk_widget_show (eventbox);
  gtk_event_box_set_above_child (GTK_EVENT_BOX (eventbox), TRUE);

  label = gtk_label_new ("drag me");
  gtk_container_add (GTK_CONTAINER (eventbox), label);

  gtk_drag_source_set (eventbox, GDK_BUTTON1_MASK, targets, G_N_ELEMENTS (targets), GDK_ACTION_COPY);
  g_signal_connect (eventbox, "drag-begin", G_CALLBACK (drag_begin_cb), NULL);
  g_signal_connect (eventbox, "drag-data-get", G_CALLBACK (drag_data_get), NULL);
  gtk_widget_show (label);

  eventbox = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (box), eventbox);
  gtk_widget_show (eventbox);
  gtk_event_box_set_above_child (GTK_EVENT_BOX (eventbox), TRUE);

  label = gtk_label_new ("drop here");
  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (eventbox), label);
  gtk_drag_dest_set (eventbox, GTK_DEST_DEFAULT_ALL, targets, G_N_ELEMENTS (targets), GDK_ACTION_COPY);

  g_signal_connect (eventbox, "drag-data-received", G_CALLBACK (drag_data_received), label);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
