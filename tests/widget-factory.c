/* widget-factory: a collection of widgets in a single page, for easy
 *                 theming
 *
 * Copyright (C) 2011 Canonical Ltd
 *
 * This  library is free  software; you can  redistribute it and/or
 * modify it  under  the terms  of the  GNU Lesser  General  Public
 * License  as published  by the Free  Software  Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed  in the hope that it will be useful,
 * but  WITHOUT ANY WARRANTY; without even  the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License  along  with  this library;  if not,  write to  the Free
 * Software Foundation, Inc., 51  Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Authored by Andrea Cimitan <andrea.cimitan@canonical.com>
 *
 */

#include <gtk/gtk.h>

int
main (int argc, char *argv[])
{
  GtkBuilder *builder;
  GtkWidget  *window;

  gtk_init (&argc, &argv);

  if (argc > 1 && 
      (g_strcmp0 (argv[1], "--dark") == 0))
    g_object_set (gtk_settings_get_default (),
                  "gtk-application-prefer-dark-theme", TRUE,
                  NULL);

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, "./widget-factory.ui", NULL);

  window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
  gtk_builder_connect_signals (builder, NULL);

  g_object_unref (G_OBJECT (builder));

  gtk_widget_show (window);
  gtk_main ();

  return 0;
}
