/*
 * Copyright (C) 2006 Nokia Corporation.
 * Author: Xan Lopez <xan.lopez@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <gtk/gtk.h>

int
main (int    argc,
      char **argv)
{
  GtkWidget *window, *label, *entry, *button;
  GtkWidget *vbox, *hbox;
  PangoFontDescription *font;
  int i, j;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  for (j = 0; j < 5; j++)
    {
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 5);

      char *aligns[] = { "FILL", "START", "END", "CENTER", "BASELINE" };
      label = gtk_label_new (aligns[j]);
      gtk_container_add (GTK_CONTAINER (hbox), label);

      for (i = 0; i < 3; i++) {
	label = gtk_label_new ("A string XYyj,Ö...");

	font = pango_font_description_new ();
	pango_font_description_set_size (font, 7*(i+1)* 1024);
	gtk_widget_override_font (label, font);

	gtk_widget_set_valign (label, j);

	gtk_container_add (GTK_CONTAINER (hbox), label);
      }

      for (i = 0; i < 3; i++) {
	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), "A string XYyj,Ö...");

	font = pango_font_description_new ();
	pango_font_description_set_size (font, 9*(i+1)* 1024);
	gtk_widget_override_font (entry, font);

	gtk_widget_set_valign (entry, j);

	gtk_container_add (GTK_CONTAINER (hbox), entry);
      }

    }

  for (j = 0; j < 2; j++)
    {
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 5);

      if (j == 0)
	label = gtk_label_new ("Baseline:");
      else
	label = gtk_label_new ("Normal:");
      gtk_container_add (GTK_CONTAINER (hbox), label);

      for (i = 0; i < 3; i++)
	{
	  button = gtk_button_new_with_label ("│Xyj,Ö");

	  font = pango_font_description_new ();
	  pango_font_description_set_size (font, 7*(i+1)* 1024);
	  gtk_widget_override_font (button, font);

	  if (j == 0)
	    gtk_widget_set_valign (button, GTK_ALIGN_BASELINE);

	  gtk_container_add (GTK_CONTAINER (hbox), button);
	}

      for (i = 0; i < 3; i++)
	{
	  button = gtk_button_new_with_label ("│Xyj,Ö");

	  gtk_button_set_image (GTK_BUTTON (button),
				gtk_image_new_from_icon_name ("face-sad", GTK_ICON_SIZE_BUTTON));
	  gtk_button_set_always_show_image (GTK_BUTTON (button), TRUE);

	  font = pango_font_description_new ();
	  pango_font_description_set_size (font, 7*(i+1)* 1024);
	  gtk_widget_override_font (button, font);

	  if (j == 0)
	    gtk_widget_set_valign (button, GTK_ALIGN_BASELINE);

	  gtk_container_add (GTK_CONTAINER (hbox), button);
	}
    }

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
