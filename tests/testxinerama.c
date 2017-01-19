/* testmultidisplay.c
 * Copyright (C) 2001 Sun Microsystems Inc.
 * Author: Erwann Chenede <erwann.chenede@sun.com>
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
#include <stdlib.h>
#include <gtk/gtk.h>

static void
label_set_text_for_monitor (GtkLabel   *label,
                            GdkMonitor *monitor)
{
  GdkRectangle monitor_geom; 
  gchar *str;

  gdk_monitor_get_geometry (monitor, &monitor_geom);

  str = g_strdup_printf ("<big><span foreground='white' background='black'>"
                         "%s %s</span></big>\n"
                         "<i>Width - Height       </i>: (%d,%d)\n"
                         "<i>Top left coordinate </i>: (%d,%d)\n"
                         "<i>Primary monitor</i>: %s",
                         gdk_monitor_get_manufacturer (monitor),
                         gdk_monitor_get_model (monitor),
                         monitor_geom.width, monitor_geom.height,
                         monitor_geom.x, monitor_geom.y,
                         gdk_monitor_is_primary (monitor) ? "YES" : "no");
  gtk_label_set_markup (label, str);
  g_free (str);
}

static void
request (GtkWidget      *widget,
	 gpointer        user_data)
{
  GdkDisplay *display = gtk_widget_get_display (widget);
  GdkMonitor *monitor = gdk_display_get_monitor_at_window (display, 
                                              gtk_widget_get_window (widget));

  if (monitor == NULL)
    gtk_label_set_markup (GTK_LABEL (user_data), 
                          "<big><span foreground='white' background='black'>Not on a monitor </span></big>");
  else
    label_set_text_for_monitor (GTK_LABEL (user_data), monitor);
}

static void
monitor_changed_cb (GdkMonitor *monitor,
                    GParamSpec *pspec,
                    GtkWidget  *label)
{
  request (label, label);
}

static void
monitor_added (GdkDisplay *display,
               GdkMonitor *monitor,
               gpointer    unused)
{
  GtkWidget *window, *label, *vbox, *button;
  GdkRectangle monitor_geom; 

  gdk_monitor_get_geometry (monitor, &monitor_geom);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
  gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);
  gtk_window_move (GTK_WINDOW (window), (monitor_geom.width - 200) / 2 + monitor_geom.x,
                   (monitor_geom.height - 200) / 2 + monitor_geom.y);
  
  label = gtk_label_new (NULL);
  label_set_text_for_monitor (GTK_LABEL (label), monitor);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_box_set_homogeneous (GTK_BOX (vbox), TRUE);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_container_add (GTK_CONTAINER (vbox), label);
  button = gtk_button_new_with_label ("Query current monitor");
  g_signal_connect (button, "clicked", G_CALLBACK (request), label);
  gtk_container_add (GTK_CONTAINER (vbox), button);
  button = gtk_button_new_with_label ("Close");
  g_signal_connect (button, "clicked", G_CALLBACK (gtk_main_quit), NULL);
  gtk_container_add (GTK_CONTAINER (vbox), button);
  gtk_widget_show (window);

  g_signal_connect (monitor, "notify",
                    G_CALLBACK (monitor_changed_cb), label);
  g_signal_connect_swapped (monitor, "invalidate",
                            G_CALLBACK (gtk_widget_destroy), window);
}

int
main (int argc, char *argv[])
{
  GdkDisplay *display;
  gint i, num_monitors;

  gtk_init ();

  display = gdk_display_get_default ();

  g_signal_connect (display, "monitor-added", G_CALLBACK (monitor_added), NULL);

  num_monitors = gdk_display_get_n_monitors (display);
  if (num_monitors == 1)
    g_warning ("The current display only has one monitor.");

  for (i = 0; i < num_monitors; i++)
    {
      GdkMonitor *monitor;
      
      monitor = gdk_display_get_monitor (display, i);
      monitor_added (display, monitor, NULL);
    }

  gtk_main ();

  return 0;
}
