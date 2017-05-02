/* testbuttons.c
 * Copyright (C) 2009 Red Hat, Inc.
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

/* various combinations of use_underline and use_stock */

int main (int argc, char *argv[])
{
	GtkWidget *window, *box, *button, *hbox;
        gchar *text;
	gboolean use_underline, use_stock;
	GtkWidget *image, *label;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	box = gtk_vbox_new (0, FALSE);

	gtk_container_add (GTK_CONTAINER (window), box);

	hbox = gtk_hbox_new (0, FALSE);
	gtk_container_add (GTK_CONTAINER (box), hbox);
	button = gtk_button_new_from_stock (GTK_STOCK_SAVE);
	gtk_container_add (GTK_CONTAINER (hbox), button);

	g_object_get (button,
                      "label", &text,
                      "use-stock", &use_stock,
                      "use-underline", &use_underline,
		      "image", &image,
                      NULL);
	text = g_strdup_printf ("label: \"%s\" image: %p use-stock: %s use-underline: %s\n", text, image, use_stock ? "TRUE" : "FALSE", use_underline ? "TRUE" : "FALSE");
	label = gtk_label_new (text);
	g_free (text);
	gtk_container_add (GTK_CONTAINER (hbox), label);

	hbox = gtk_hbox_new (0, FALSE);
	gtk_container_add (GTK_CONTAINER (box), hbox);
	button = g_object_new (GTK_TYPE_BUTTON,
                               "label", "gtk-save",
			       "use-stock", TRUE,
			       NULL);
	gtk_container_add (GTK_CONTAINER (hbox), button);

	g_object_get (button,
                      "label", &text,
                      "use-stock", &use_stock,
                      "use-underline", &use_underline,
		      "image", &image,
                      NULL);
	text = g_strdup_printf ("label: \"%s\" image: %p use-stock: %s use-underline: %s\n", text, image, use_stock ? "TRUE" : "FALSE", use_underline ? "TRUE" : "FALSE");
	label = gtk_label_new (text);
	g_free (text);
	gtk_container_add (GTK_CONTAINER (hbox), label);

	hbox = gtk_hbox_new (0, FALSE);
	gtk_container_add (GTK_CONTAINER (box), hbox);
	button = gtk_button_new_with_label ("_Save");
	gtk_container_add (GTK_CONTAINER (hbox), button);

	g_object_get (button,
                      "label", &text,
                      "use-stock", &use_stock,
                      "use-underline", &use_underline,
		      "image", &image,
                      NULL);
	text = g_strdup_printf ("label: \"%s\" image: %p use-stock: %s use-underline: %s\n", text, image, use_stock ? "TRUE" : "FALSE", use_underline ? "TRUE" : "FALSE");
	label = gtk_label_new (text);
	g_free (text);
	gtk_container_add (GTK_CONTAINER (hbox), label);

	hbox = gtk_hbox_new (0, FALSE);
	gtk_container_add (GTK_CONTAINER (box), hbox);
	button = gtk_button_new_with_mnemonic ("_Save");
	gtk_container_add (GTK_CONTAINER (hbox), button);

	g_object_get (button,
                      "label", &text,
                      "use-stock", &use_stock,
                      "use-underline", &use_underline,
		      "image", &image,
                      NULL);
	text = g_strdup_printf ("label: \"%s\" image: %p use-stock: %s use-underline: %s\n", text, image, use_stock ? "TRUE" : "FALSE", use_underline ? "TRUE" : "FALSE");
	label = gtk_label_new (text);
	g_free (text);
	gtk_container_add (GTK_CONTAINER (hbox), label);

	hbox = gtk_hbox_new (0, FALSE);
	gtk_container_add (GTK_CONTAINER (box), hbox);
	button = gtk_button_new_with_label ("_Save");
	gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_stock (GTK_STOCK_ABOUT, GTK_ICON_SIZE_BUTTON));
	gtk_container_add (GTK_CONTAINER (hbox), button);

	g_object_get (button,
                      "label", &text,
                      "use-stock", &use_stock,
                      "use-underline", &use_underline,
		      "image", &image,
                      NULL);
	text = g_strdup_printf ("label: \"%s\" image: %p use-stock: %s use-underline: %s\n", text, image, use_stock ? "TRUE" : "FALSE", use_underline ? "TRUE" : "FALSE");
	label = gtk_label_new (text);
	g_free (text);
	gtk_container_add (GTK_CONTAINER (hbox), label);

	hbox = gtk_hbox_new (0, FALSE);
	gtk_container_add (GTK_CONTAINER (box), hbox);
	button = gtk_button_new_with_mnemonic ("_Save");
	gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_stock (GTK_STOCK_ABOUT, GTK_ICON_SIZE_BUTTON));
	gtk_container_add (GTK_CONTAINER (hbox), button);
	g_object_get (button,
                      "label", &text,
                      "use-stock", &use_stock,
                      "use-underline", &use_underline,
		      "image", &image,
                      NULL);
	text = g_strdup_printf ("label: \"%s\" image: %p use-stock: %s use-underline: %s\n", text, image, use_stock ? "TRUE" : "FALSE", use_underline ? "TRUE" : "FALSE");
	label = gtk_label_new (text);
	g_free (text);
	gtk_container_add (GTK_CONTAINER (hbox), label);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}

