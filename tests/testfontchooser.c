/* testfontchooser.c
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

static void
notify_font_name_cb (GObject *fontchooser, GParamSpec *pspec, gpointer data)
{
  g_debug ("Changed font name %s", gtk_font_chooser_get_font_name (GTK_FONT_CHOOSER (fontchooser)));
}

static void
notify_preview_text_cb (GObject *fontchooser, GParamSpec *pspec, gpointer data)
{
  g_debug ("Changed preview text %s", gtk_font_chooser_get_preview_text (GTK_FONT_CHOOSER (fontchooser)));
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *fontchooser;

  gtk_init (NULL, NULL);

  fontchooser = gtk_font_chooser_new ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, 600, 600);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_container_add (GTK_CONTAINER (box), fontchooser);

  gtk_widget_show_all (window);

  g_signal_connect (window, "delete-event",
                    G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (fontchooser, "notify::font-name",
                    G_CALLBACK (notify_font_name_cb), NULL);
  g_signal_connect (fontchooser, "notify::preview-text",
                    G_CALLBACK (notify_preview_text_cb), NULL);

  gtk_font_chooser_set_font_name (GTK_FONT_CHOOSER (fontchooser), "Bitstream Vera Sans 45");
  gtk_font_chooser_set_preview_text (GTK_FONT_CHOOSER (fontchooser), "[user@host ~]$ ");
  gtk_font_chooser_set_show_preview_entry (GTK_FONT_CHOOSER (fontchooser), FALSE);

  gtk_main ();

  return 0;
}
