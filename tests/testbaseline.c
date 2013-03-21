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

static void
baseline_row_value_changed (GtkSpinButton *spin_button,
		    GtkGrid *grid)
{
  gint row = gtk_spin_button_get_value_as_int (spin_button);

  gtk_grid_set_baseline_row (grid, row);
}

static void
homogeneous_changed (GtkToggleButton *toggle_button,
		    GtkGrid *grid)
{
  gtk_grid_set_row_homogeneous (grid, gtk_toggle_button_get_active (toggle_button));
}

static void
baseline_position_changed (GtkComboBox *combo,
			   GtkBox *hbox)
{
  int i = gtk_combo_box_get_active (combo);

  gtk_box_set_baseline_position (hbox, i);
}

int
main (int    argc,
      char **argv)
{
  GtkWidget *window, *label, *entry, *button, *grid;
  GtkWidget *vbox, *hbox, *grid_hbox, *spin, *toggle, *combo;
  PangoFontDescription *font;
  GtkAdjustment *adjustment;
  int i, j;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  for (j = 0; j < 2; j++)
    {
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);

      char *aligns_names[] = { "FILL", "BASELINE" };
      GtkAlign aligns[] = { GTK_ALIGN_FILL, GTK_ALIGN_BASELINE};

      label = gtk_label_new (aligns_names[j]);
      gtk_container_add (GTK_CONTAINER (hbox), label);

      for (i = 0; i < 3; i++) {
	label = gtk_label_new ("│XYyj,Ö...");

	font = pango_font_description_new ();
	pango_font_description_set_size (font, 5*(i+1)* 1024);
	gtk_widget_override_font (label, font);

	gtk_widget_set_valign (label, aligns[j]);

	gtk_container_add (GTK_CONTAINER (hbox), label);
      }

      for (i = 0; i < 3; i++) {
	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), "│XYyj,Ö...");

	font = pango_font_description_new ();
	pango_font_description_set_size (font, 5*(i+1)* 1024);
	gtk_widget_override_font (entry, font);

	gtk_widget_set_valign (entry, aligns[j]);

	gtk_container_add (GTK_CONTAINER (hbox), entry);
      }

    }

  for (j = 0; j < 2; j++)
    {
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);

      if (j == 0)
	label = gtk_label_new ("Baseline:");
      else
	label = gtk_label_new ("Normal:");
      gtk_container_add (GTK_CONTAINER (hbox), label);

      for (i = 0; i < 3; i++)
	{
	  button = gtk_button_new_with_label ("│Xyj,Ö");

	  font = pango_font_description_new ();
	  pango_font_description_set_size (font, 5*(i+1)* 1024);
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
	  pango_font_description_set_size (font, 5*(i+1)* 1024);
	  gtk_widget_override_font (button, font);

	  if (j == 0)
	    gtk_widget_set_valign (button, GTK_ALIGN_BASELINE);

	  gtk_container_add (GTK_CONTAINER (hbox), button);
	}
    }

  grid_hbox = hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 5);

  label = gtk_label_new ("Align me:");
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);

  gtk_container_add (GTK_CONTAINER (hbox), label);

  grid = gtk_grid_new ();
  gtk_widget_set_valign (grid, GTK_ALIGN_BASELINE);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 8);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 8);

  for (j = 0; j < 4; j++)
    {
      char *labels[] = { "Normal:", "Baseline (top):", "Baseline (center):", "Baseline (bottom):"};
      label = gtk_label_new (labels[j]);

      gtk_grid_attach (GTK_GRID (grid),
		       label,
		       0, j,
		       1, 1);
      gtk_widget_set_vexpand (label, TRUE);

      if (j != 0)
	gtk_grid_set_row_baseline_position (GTK_GRID (grid),
					    j, (GtkBaselinePosition)(j-1));

      for (i = 0; i < 3; i++)
	{
	  label = gtk_label_new ("Xyjg,Ö.");

	  font = pango_font_description_new ();
	  pango_font_description_set_size (font, 5*(i+1)* 1024);
	  gtk_widget_override_font (label, font);

	  if (j != 0)
	    gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);

	  gtk_grid_attach (GTK_GRID (grid),
			   label,
			   i+1, j,
			   1, 1);
	}

      for (i = 0; i < 3; i++)
	{
	  button = gtk_button_new_with_label ("│Xyj,Ö");

	  gtk_button_set_image (GTK_BUTTON (button),
				gtk_image_new_from_icon_name ("face-sad", GTK_ICON_SIZE_BUTTON));
	  gtk_button_set_always_show_image (GTK_BUTTON (button), TRUE);

	  font = pango_font_description_new ();
	  pango_font_description_set_size (font, 5*(i+1)* 1024);
	  gtk_widget_override_font (button, font);

	  if (j != 0)
	    gtk_widget_set_valign (button, GTK_ALIGN_BASELINE);

	  gtk_grid_attach (GTK_GRID (grid),
			   button,
			   i+4, j,
			   1, 1);
	}

    }

  gtk_container_add (GTK_CONTAINER (hbox), grid);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);

  adjustment = gtk_adjustment_new (0.0, -1.0, 5.0, 1.0, 1.0, 0.0);
  spin = gtk_spin_button_new (adjustment, 1.0, 0);
  g_signal_connect (spin, "value-changed", (GCallback)baseline_row_value_changed, grid);
  gtk_container_add (GTK_CONTAINER (hbox), spin);

  toggle = gtk_toggle_button_new_with_label ("Homogeneous");
  g_signal_connect (toggle, "toggled", (GCallback)homogeneous_changed, grid);
  gtk_container_add (GTK_CONTAINER (hbox), toggle);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "BASELINE_POSITION_TOP");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "BASELINE_POSITION_CENTER");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "BASELINE_POSITION_BOTTOM");
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 1);
  g_signal_connect (G_OBJECT (combo), "changed",
		    G_CALLBACK (baseline_position_changed), grid_hbox);
  gtk_container_add (GTK_CONTAINER (hbox), combo);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
