/* svg-view.c
 * Copyright (C) 2026 Red Hat, Inc
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
#include "config.h"
#include <gtk/gtk.h>


static void
error_cb (GtkSvg *svg, GError *error)
{
  if (error->domain == GTK_SVG_ERROR)
    {
      const GtkSvgLocation *start = gtk_svg_error_get_start (error);
      const GtkSvgLocation *end = gtk_svg_error_get_end (error);
      const char *element = gtk_svg_error_get_element (error);
      const char *attribute = gtk_svg_error_get_attribute (error);

      if (start)
        {
          if (end->lines != start->lines || end->line_chars != start->line_chars)
            g_print ("%" G_GSIZE_FORMAT ".%" G_GSIZE_FORMAT " - %" G_GSIZE_FORMAT ".%" G_GSIZE_FORMAT ": ",
                     start->lines, start->line_chars,
                     end->lines, end->line_chars);
          else
            g_print ("%" G_GSIZE_FORMAT ".%" G_GSIZE_FORMAT ": ", start->lines, start->line_chars);
        }

      if (element && attribute)
        g_print ("(%s / %s) ", element, attribute);
      else if (element)
        g_print ("(%s) ", element);
    }

  g_print ("%s\n", error->message);
}

static void
activate_cb (GtkSvgWidget *svg,
             const char   *id,
             const char   *href,
             GtkWindow    *window)
{
  GtkAlertDialog *alert;

  alert = gtk_alert_dialog_new ("Activated link %s: %s", id, href);
  gtk_alert_dialog_show (alert, window);
  g_object_unref (alert);
}

int
main (int argc, char *argv[])
{
  GFile *file;
  GBytes *bytes;
  GError *error = NULL;
  GtkWidget *window;
  GtkSvgWidget *svg;

  if (argc < 2)
    {
      g_print ("No svg file given.\n");
      return 0;
    }

  gtk_init ();

  file = g_file_new_for_commandline_arg (argv[1]);
  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  if (!bytes)
    g_error ("%s", error->message);

  window = gtk_window_new ();

  svg = gtk_svg_widget_new ();
  g_signal_connect (svg, "error", G_CALLBACK (error_cb), NULL);
  g_signal_connect (svg, "activate", G_CALLBACK (activate_cb), window);
  gtk_svg_widget_load_from_bytes (svg, bytes);

  gtk_window_set_child (GTK_WINDOW (window), GTK_WIDGET (svg));

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()))
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
