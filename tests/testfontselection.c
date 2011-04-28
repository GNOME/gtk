/* testfontselection.c
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

#include "config.h"

#include <gtk/gtk.h>


int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *dialog;
  GtkWidget *fontsel;
  
  gtk_init (NULL, NULL);
    
  dialog = gtk_font_selection_dialog_new (NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, 300, 300);
  vbox = gtk_vbox_new (TRUE, 6);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  fontsel = gtk_font_selection_dialog_get_font_selection (GTK_FONT_SELECTION_DIALOG (dialog));
  gtk_container_add (GTK_CONTAINER (vbox), gtk_font_selection_get_size_list (GTK_FONT_SELECTION (fontsel)));
  gtk_container_add (GTK_CONTAINER (vbox), gtk_font_selection_get_family_list (GTK_FONT_SELECTION (fontsel)));
  gtk_container_add (GTK_CONTAINER (vbox), gtk_font_selection_get_face_list (GTK_FONT_SELECTION (fontsel)));

  gtk_widget_show_all (window);

  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
  return 0;
}
