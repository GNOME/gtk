/* testfontselectiondialog.c
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#define GDK_DISABLE_DEPRECATION_WARNINGS

#include <gtk/gtk.h>

int
main (int argc, char *argv[])
{
  GtkWidget *dialog;
  GtkWidget *ok G_GNUC_UNUSED;

  gtk_init (&argc, &argv);

  dialog = gtk_font_selection_dialog_new (NULL);

  ok = gtk_font_selection_dialog_get_ok_button (GTK_FONT_SELECTION_DIALOG (dialog));

  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  return 0;
}
