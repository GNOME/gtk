/* testfontchooserdialog.c
 * Copyright (C) 2011 Alberto Ruiz <aruiz@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include "prop-editor.h"

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *font_button;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  font_button = gtk_font_button_new ();
  gtk_container_add (GTK_CONTAINER (window), font_button);
  gtk_widget_show_all (window);

  g_signal_connect (window, "delete-event",
                    G_CALLBACK (gtk_main_quit), NULL);

  create_prop_editor (G_OBJECT (font_button), 0);

  gtk_main ();

  return 0;
}
