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

static char *baseline_pos_str[] = {
  "BASELINE_POSITION_TOP",
  "BASELINE_POSITION_CENTER",
  "BASELINE_POSITION_BOTTOM"
};

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

static void
image_size_value_changed (GtkSpinButton *spin_button,
			  GtkImage *image)
{
  gint size = gtk_spin_button_get_value_as_int (spin_button);

  gtk_image_set_pixel_size (GTK_IMAGE (image), size);
}

static void
set_font_size (GtkWidget *widget, gint size)
{
  const gchar *class[3] = { "small-font", "medium-font", "large-font" };

  gtk_style_context_add_class (gtk_widget_get_style_context (widget), class[size]);
}

int
main (int    argc,
      char **argv)
{
  GtkWidget *window, *label, *entry, *button, *grid, *notebook;
  GtkWidget *vbox, *hbox, *grid_hbox, *spin, *spin2, *toggle, *combo, *image;
  GtkAdjustment *adjustment;
  int i, j;
  GtkCssProvider *provider;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider,
    ".small-font { font-size: 5px; }"
    ".medium-font { font-size: 10px; }"
    ".large-font { font-size: 15px; }", -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

  notebook = gtk_notebook_new ();
  gtk_container_add (GTK_CONTAINER (window), notebook);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    vbox, gtk_label_new ("hboxes"));

  for (j = 0; j < 2; j++)
    {
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_container_add (GTK_CONTAINER (vbox), hbox);

      char *aligns_names[] = { "FILL", "BASELINE" };
      GtkAlign aligns[] = { GTK_ALIGN_FILL, GTK_ALIGN_BASELINE};

      label = gtk_label_new (aligns_names[j]);
      gtk_container_add (GTK_CONTAINER (hbox), label);

      for (i = 0; i < 3; i++) {
	label = gtk_label_new ("│XYyj,Ö...");

        set_font_size (label, i);

	gtk_widget_set_valign (label, aligns[j]);

	gtk_container_add (GTK_CONTAINER (hbox), label);
      }

      for (i = 0; i < 3; i++) {
	entry = gtk_entry_new ();
	gtk_editable_set_text (GTK_EDITABLE (entry), "│XYyj,Ö...");

        set_font_size (entry, i);

	gtk_widget_set_valign (entry, aligns[j]);

	gtk_container_add (GTK_CONTAINER (hbox), entry);
      }

      spin = gtk_spin_button_new (NULL, 0, 1);
      gtk_orientable_set_orientation (GTK_ORIENTABLE (spin), GTK_ORIENTATION_VERTICAL);
      gtk_widget_set_valign (spin, aligns[j]);
      gtk_container_add (GTK_CONTAINER (hbox), spin);

      spin = gtk_spin_button_new (NULL, 0, 1);
      gtk_widget_set_valign (spin, aligns[j]);
      gtk_container_add (GTK_CONTAINER (hbox), spin);
    }

  grid_hbox = hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), baseline_pos_str[0]);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), baseline_pos_str[1]);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), baseline_pos_str[2]);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 1);
  gtk_container_add (GTK_CONTAINER (hbox), combo);

  for (j = 0; j < 2; j++)
    {
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_container_add (GTK_CONTAINER (vbox), hbox);

      g_signal_connect (G_OBJECT (combo), "changed",
			G_CALLBACK (baseline_position_changed), hbox);

      if (j == 0)
	label = gtk_label_new ("Baseline:");
      else
	label = gtk_label_new ("Normal:");
      gtk_container_add (GTK_CONTAINER (hbox), label);

      for (i = 0; i < 3; i++)
	{
	  button = gtk_button_new_with_label ("│Xyj,Ö");

          set_font_size (button, i);

	  if (j == 0)
	    gtk_widget_set_valign (button, GTK_ALIGN_BASELINE);

	  gtk_container_add (GTK_CONTAINER (hbox), button);
	}

      for (i = 0; i < 3; i++)
	{
          GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL,  6);
          button = gtk_button_new ();

          gtk_container_add (GTK_CONTAINER (box), gtk_label_new ("│Xyj,Ö"));
          gtk_container_add (GTK_CONTAINER (box), gtk_image_new_from_icon_name ("face-sad"));
          gtk_container_add (GTK_CONTAINER (button), box);

          set_font_size (button, i);

	  if (j == 0)
	    gtk_widget_set_valign (button, GTK_ALIGN_BASELINE);

	  gtk_container_add (GTK_CONTAINER (hbox), button);
	}

      image = gtk_image_new_from_icon_name ("face-sad");
      gtk_image_set_pixel_size (GTK_IMAGE (image), 34);
      if (j == 0)
	gtk_widget_set_valign (image, GTK_ALIGN_BASELINE);
      gtk_container_add (GTK_CONTAINER (hbox), image);

      button = gtk_toggle_button_new_with_label ("│Xyj,Ö");
      if (j == 0)
	gtk_widget_set_valign (button, GTK_ALIGN_BASELINE);
      gtk_container_add (GTK_CONTAINER (hbox), button);

      button = gtk_toggle_button_new_with_label ("│Xyj,Ö");
      if (j == 0)
	gtk_widget_set_valign (button, GTK_ALIGN_BASELINE);
      gtk_container_add (GTK_CONTAINER (hbox), button);

      button = gtk_check_button_new_with_label ("│Xyj,Ö");
      if (j == 0)
	gtk_widget_set_valign (button, GTK_ALIGN_BASELINE);
      gtk_container_add (GTK_CONTAINER (hbox), button);

      button = gtk_radio_button_new_with_label (NULL, "│Xyj,Ö");
      if (j == 0)
	gtk_widget_set_valign (button, GTK_ALIGN_BASELINE);
      gtk_container_add (GTK_CONTAINER (hbox), button);
    }


  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    vbox, gtk_label_new ("grid"));

  grid_hbox = hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

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

          set_font_size (label, i);

	  if (j != 0)
	    gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);

	  gtk_grid_attach (GTK_GRID (grid),
			   label,
			   i+1, j,
			   1, 1);
	}

      for (i = 0; i < 3; i++)
	{
          GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL,  6);
          button = gtk_button_new ();

          gtk_container_add (GTK_CONTAINER (box), gtk_label_new ("│Xyj,Ö"));
          gtk_container_add (GTK_CONTAINER (box), gtk_image_new_from_icon_name ("face-sad"));
          gtk_container_add (GTK_CONTAINER (button), box);

          set_font_size (button, i);

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
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  adjustment = gtk_adjustment_new (0.0, -1.0, 5.0, 1.0, 1.0, 0.0);
  spin = gtk_spin_button_new (adjustment, 1.0, 0);
  g_signal_connect (spin, "value-changed", (GCallback)baseline_row_value_changed, grid);
  gtk_container_add (GTK_CONTAINER (hbox), spin);

  toggle = gtk_toggle_button_new_with_label ("Homogeneous");
  g_signal_connect (toggle, "toggled", (GCallback)homogeneous_changed, grid);
  gtk_container_add (GTK_CONTAINER (hbox), toggle);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), baseline_pos_str[0]);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), baseline_pos_str[1]);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), baseline_pos_str[2]);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 1);
  g_signal_connect (G_OBJECT (combo), "changed",
		    G_CALLBACK (baseline_position_changed), grid_hbox);
  gtk_container_add (GTK_CONTAINER (hbox), combo);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    vbox, gtk_label_new ("button box"));

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  adjustment = gtk_adjustment_new (34.0, 1.0, 64.0, 1.0, 1.0, 0.0);
  spin = gtk_spin_button_new (adjustment, 1.0, 0);
  gtk_container_add (GTK_CONTAINER (hbox), spin);

  adjustment = gtk_adjustment_new (16.0, 1.0, 64.0, 1.0, 1.0, 0.0);
  spin2 = gtk_spin_button_new (adjustment, 1.0, 0);
  gtk_container_add (GTK_CONTAINER (hbox), spin2);

  for (j = 0; j < 3; j++)
    {
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_add (GTK_CONTAINER (vbox), hbox);

      gtk_box_set_baseline_position (GTK_BOX (hbox), j);

      label = gtk_label_new (baseline_pos_str[j]);
      gtk_container_add (GTK_CONTAINER (hbox), label);
      gtk_widget_set_vexpand (label, TRUE);

      image = gtk_image_new_from_icon_name ("face-sad");
      gtk_image_set_pixel_size (GTK_IMAGE (image), 34);
      gtk_container_add (GTK_CONTAINER (hbox), image);

      g_signal_connect (spin, "value-changed", (GCallback)image_size_value_changed, image);

      for (i = 0; i < 3; i++)
	{
	  button = gtk_button_new_with_label ("│Xyj,Ö");

          set_font_size (button, i);

	  if (i != 0)
	    gtk_widget_set_valign (button, GTK_ALIGN_BASELINE);

	  gtk_container_add (GTK_CONTAINER (hbox), button);
	}

      for (i = 0; i < 3; i++)
	{
          GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL,  6);
          button = gtk_button_new ();

          gtk_container_add (GTK_CONTAINER (box), gtk_label_new ("│Xyj,Ö"));
          image = gtk_image_new_from_icon_name ("face-sad");
          gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
          gtk_container_add (GTK_CONTAINER (box), image);
          gtk_container_add (GTK_CONTAINER (button), box);

	  if (i == 0)
	    g_signal_connect (spin2, "value-changed", (GCallback)image_size_value_changed, image);

          set_font_size (button, i);

	  gtk_widget_set_valign (button, GTK_ALIGN_BASELINE);

	  gtk_container_add (GTK_CONTAINER (hbox), button);
	}
    }

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
