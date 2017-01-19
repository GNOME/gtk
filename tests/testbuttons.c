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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

/* various combinations of use_underline */

int main (int argc, char *argv[])
{
	GtkWidget *window, *box, *button, *hbox;
        gchar *text;
        const char *icon_name;
	gboolean use_underline;
	GtkWidget *label;

	gtk_init ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	gtk_container_add (GTK_CONTAINER (window), box);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (box), hbox);
	button = g_object_new (GTK_TYPE_BUTTON,
                               "label", "document-save",
			       NULL);
	gtk_container_add (GTK_CONTAINER (hbox), button);

	g_object_get (button,
                      "label", &text,
                      "use-underline", &use_underline,
                      "icon-name", &icon_name,
                      NULL);
	text = g_strdup_printf ("label: \"%s\" icon-name: \"%s\" use-underline: %s\n", text, icon_name, use_underline ? "TRUE" : "FALSE");
	label = gtk_label_new (text);
	g_free (text);
	gtk_container_add (GTK_CONTAINER (hbox), label);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (box), hbox);
	button = gtk_button_new_with_label ("_Save");
	gtk_container_add (GTK_CONTAINER (hbox), button);

	g_object_get (button,
                      "label", &text,
                      "use-underline", &use_underline,
                      "icon-name", &icon_name,
                      NULL);
	text = g_strdup_printf ("label: \"%s\" icon-name: \"%s\" use-underline: %s\n", text, icon_name, use_underline ? "TRUE" : "FALSE");
	label = gtk_label_new (text);
	g_free (text);
	gtk_container_add (GTK_CONTAINER (hbox), label);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (box), hbox);
	button = gtk_button_new_with_mnemonic ("_Save");
	gtk_container_add (GTK_CONTAINER (hbox), button);

	g_object_get (button,
                      "label", &text,
                      "use-underline", &use_underline,
                      "icon-name", &icon_name,
                      NULL);
	text = g_strdup_printf ("label: \"%s\" icon-name: \"%s\" use-underline: %s\n", text, icon_name, use_underline ? "TRUE" : "FALSE");
	label = gtk_label_new (text);
	g_free (text);
	gtk_container_add (GTK_CONTAINER (hbox), label);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (box), hbox);
	button = gtk_button_new_from_icon_name ("help-about", GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (hbox), button);

	g_object_get (button,
                      "label", &text,
                      "use-underline", &use_underline,
                      "icon-name", &icon_name,
                      NULL);
	text = g_strdup_printf ("label: \"%s\" icon-name: \"%s\" use-underline: %s\n", text, icon_name, use_underline ? "TRUE" : "FALSE");
	label = gtk_label_new (text);
	g_free (text);
	gtk_container_add (GTK_CONTAINER (hbox), label);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (box), hbox);
        button = gtk_button_new ();
        gtk_button_set_icon_name (GTK_BUTTON (button), "help-about");
        gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);
	gtk_container_add (GTK_CONTAINER (hbox), button);
	g_object_get (button,
                      "label", &text,
                      "use-underline", &use_underline,
                      "icon-name", &icon_name,
                      NULL);
	text = g_strdup_printf ("label: \"%s\" icon-name: \"%s\" use-underline: %s\n", text, icon_name, use_underline ? "TRUE" : "FALSE");
	label = gtk_label_new (text);
	g_free (text);
	gtk_container_add (GTK_CONTAINER (hbox), label);

	gtk_widget_show (window);

	gtk_main ();

	return 0;
}

