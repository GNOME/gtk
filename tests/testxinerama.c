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

static gint num_monitors;
static gint primary_monitor;

static void
request (GtkWidget      *widget,
	 gpointer        user_data)
{
  gchar *str;
  GdkScreen *screen = gtk_widget_get_screen (widget);
  gint i = gdk_screen_get_monitor_at_window (screen,
                                             gtk_widget_get_window (widget));

  if (i < 0)
    str = g_strdup ("<big><span foreground='white' background='black'>Not on a monitor </span></big>");
  else
    {
      GdkRectangle monitor;

      gdk_screen_get_monitor_geometry (screen,
                                       i, &monitor);
      primary_monitor = gdk_screen_get_primary_monitor (screen);

      str = g_strdup_printf ("<big><span foreground='white' background='black'>"
			     "Monitor %d of %d</span></big>\n"
			     "<i>Width - Height       </i>: (%d,%d)\n"
			     "<i>Top left coordinate </i>: (%d,%d)\n"
                             "<i>Primary monitor: %d</i>",
                             i + 1, num_monitors,
			     monitor.width, monitor.height,
                             monitor.x, monitor.y,
                             primary_monitor);
    }

  gtk_label_set_markup (GTK_LABEL (user_data), str);
  g_free (str);
}

static void
monitors_changed_cb (GdkScreen *screen,
                     gpointer   data)
{
  GtkWidget *label = (GtkWidget *)data;

  request (label, label);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *label, *vbox, *button;
  GdkScreen *screen;
  gint i;

  gtk_init (&argc, &argv);

  screen = gdk_screen_get_default ();

  num_monitors = gdk_screen_get_n_monitors (screen);
  if (num_monitors == 1)
    g_warning ("The default screen of the current display only has one monitor.");

  primary_monitor = gdk_screen_get_primary_monitor (screen);

  for (i = 0; i < num_monitors; i++)
    {
      GdkRectangle monitor; 
      gchar *str;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gdk_screen_get_monitor_geometry (screen, i, &monitor);
      gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);
      gtk_window_move (GTK_WINDOW (window), (monitor.width - 200) / 2 + monitor.x,
		       (monitor.height - 200) / 2 + monitor.y);
      
      label = gtk_label_new (NULL);
      str = g_strdup_printf ("<big><span foreground='white' background='black'>"
			     "Monitor %d of %d</span></big>\n"
			     "<i>Width - Height       </i>: (%d,%d)\n"
			     "<i>Top left coordinate </i>: (%d,%d)\n"
                             "<i>Primary monitor: %d</i>",
                             i + 1, num_monitors,
			     monitor.width, monitor.height,
                             monitor.x, monitor.y,
                             primary_monitor);
      gtk_label_set_markup (GTK_LABEL (label), str);
      g_free (str);
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
      gtk_widget_show_all (window);

      g_signal_connect (screen, "monitors-changed",
                        G_CALLBACK (monitors_changed_cb), label);
    }

  gtk_main ();

  return 0;
}
