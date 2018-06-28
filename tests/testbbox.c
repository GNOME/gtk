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

#define N_BUTTONS 3

GtkWidget *bbox = NULL;
GtkWidget *hbbox = NULL, *vbbox = NULL;

static const char* styles[] = { "GTK_BUTTONBOX_SPREAD",
				"GTK_BUTTONBOX_EDGE",
				"GTK_BUTTONBOX_START",
				"GTK_BUTTONBOX_END",
				"GTK_BUTTONBOX_CENTER",
				"GTK_BUTTONBOX_EXPAND",
				NULL};

static const char* types[] = { "GtkHButtonBox",
			       "GtkVButtonBox",
			       NULL};

static void
populate_combo_with (GtkComboBoxText *combo, const char** elements)
{
  int i;
  
  for (i = 0; elements[i] != NULL; i++) {
    gtk_combo_box_text_append_text (combo, elements[i]);
  }
  
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
}

static void
combo_changed_cb (GtkComboBoxText *combo,
		  gpointer user_data)
{
  gint active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox),
			     (GtkButtonBoxStyle) (active + 1));
}

static void
reparent_widget (GtkWidget *widget,
		 GtkWidget *old_parent,
		 GtkWidget *new_parent)
{
  g_object_ref (widget);
  gtk_container_remove (GTK_CONTAINER (old_parent), widget);
  gtk_container_add (GTK_CONTAINER (new_parent), widget);
  g_object_unref (widget);
}

static void
combo_types_changed_cb (GtkComboBoxText *combo,
			GtkWidget **buttons)
{
  int i;
  GtkWidget *old_parent, *new_parent;
  GtkButtonBoxStyle style;

  gint active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  if (active == GTK_ORIENTATION_HORIZONTAL) {
    old_parent = vbbox;
    new_parent = hbbox;
  } else {
    old_parent = hbbox;
    new_parent = vbbox;
  }

  bbox = new_parent;

  for (i = 0; i < N_BUTTONS; i++) {
    reparent_widget (buttons[i], old_parent, new_parent);
  }
  
  gtk_widget_hide (old_parent);
  style = gtk_button_box_get_layout (GTK_BUTTON_BOX (old_parent));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (new_parent), style);
  gtk_widget_show (new_parent);
}

static void
option_cb (GtkToggleButton *option,
	   GtkWidget *button)
{
  gboolean active = gtk_toggle_button_get_active (option);
  
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (bbox),
				      button, active);
}

static const char* strings[] = { "Ok", "Cancel", "Help" };

int
main (int    argc,
      char **argv)
{
  GtkWidget *window, *buttons[N_BUTTONS];
  GtkWidget *vbox, *hbox, *combo_styles, *combo_types, *option;
  int i;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  /* GtkHButtonBox */

  hbbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (vbox), hbbox);

  for (i = 0; i < N_BUTTONS; i++) {
    buttons[i] = gtk_button_new_with_label (strings[i]);
    gtk_container_add (GTK_CONTAINER (hbbox), buttons[i]);
  }

  bbox = hbbox;

  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbbox), GTK_BUTTONBOX_SPREAD);

  /* GtkVButtonBox */
  vbbox = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_pack_start (GTK_BOX (vbox), vbbox);

  /* Options */

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox);

  combo_types = gtk_combo_box_text_new ();
  populate_combo_with (GTK_COMBO_BOX_TEXT (combo_types), types);
  g_signal_connect (G_OBJECT (combo_types), "changed", G_CALLBACK (combo_types_changed_cb), buttons);
  gtk_box_pack_start (GTK_BOX (hbox), combo_types);

  combo_styles = gtk_combo_box_text_new ();
  populate_combo_with (GTK_COMBO_BOX_TEXT (combo_styles), styles);
  g_signal_connect (G_OBJECT (combo_styles), "changed", G_CALLBACK (combo_changed_cb), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), combo_styles);

  option = gtk_check_button_new_with_label ("Help is secondary");
  g_signal_connect (G_OBJECT (option), "toggled", G_CALLBACK (option_cb), buttons[N_BUTTONS - 1]);

  gtk_box_pack_start (GTK_BOX (hbox), option);

  gtk_widget_show (window);
  gtk_widget_hide (vbbox);

  gtk_main ();

  return 0;
}
