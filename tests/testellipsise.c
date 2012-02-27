/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <gtk/gtk.h>

static void
redraw_event_box (GtkWidget *widget)
{
  while (widget)
    {
      if (GTK_IS_EVENT_BOX (widget))
        {
          gtk_widget_queue_draw (widget);
          break;
        }

      widget = gtk_widget_get_parent (widget);
    }
}

static void
combo_changed_cb (GtkWidget *combo,
		  gpointer   data)
{
  GtkWidget *label = GTK_WIDGET (data);
  gint active;

  active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
  gtk_label_set_ellipsize (GTK_LABEL (label), (PangoEllipsizeMode)active);
  redraw_event_box (label);
}

static void
scale_changed_cb (GtkRange *range,
		  gpointer   data)
{
  double angle = gtk_range_get_value (range);
  GtkWidget *label = GTK_WIDGET (data);
  
  gtk_label_set_angle (GTK_LABEL (label), angle);
  redraw_event_box (label);
}

static gboolean
ebox_draw_cb (GtkWidget *widget,
              cairo_t   *cr,
              gpointer   data)
{
  PangoLayout *layout;
  const double dashes[] = { 6, 18 };
  GtkAllocation label_allocation;
  GtkRequisition minimum_size, natural_size;
  GtkWidget *label = data;
  gint x, y;

  cairo_translate (cr, -0.5, -0.5);
  cairo_set_line_width (cr, 1);

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);

  gtk_widget_translate_coordinates (label, widget, 0, 0, &x, &y);
  layout = gtk_widget_create_pango_layout (widget, "");

  gtk_widget_get_preferred_size (label, &minimum_size, &natural_size); 

  pango_layout_set_markup (layout,
    "<span color='#c33'>\342\227\217 requisition</span>\n"
    "<span color='#3c3'>\342\227\217 natural size</span>\n"
    "<span color='#33c'>\342\227\217 allocation</span>", -1);

  pango_cairo_show_layout (cr, layout);
  g_object_unref (layout);

  gtk_widget_get_allocation (label, &label_allocation);

  cairo_rectangle (cr,
                   x + 0.5 * (label_allocation.width - minimum_size.width),
                   y + 0.5 * (label_allocation.height - minimum_size.height),
                   minimum_size.width, minimum_size.height);
  cairo_set_source_rgb (cr, 0.8, 0.2, 0.2);
  cairo_set_dash (cr, NULL, 0, 0);
  cairo_stroke (cr);

  cairo_rectangle (cr, x, y, label_allocation.width, label_allocation.height);
  cairo_set_source_rgb (cr, 0.2, 0.2, 0.8);
  cairo_set_dash (cr, dashes, 2, 0.5);
  cairo_stroke (cr);

  cairo_rectangle (cr,
                   x + 0.5 * (label_allocation.width - natural_size.width),
                   y + 0.5 * (label_allocation.height - natural_size.height),
                   natural_size.width, natural_size.height);
  cairo_set_source_rgb (cr, 0.2, 0.8, 0.2);
  cairo_set_dash (cr, dashes, 2, 12.5);
  cairo_stroke (cr);

  return FALSE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *vbox, *label;
  GtkWidget *combo, *scale, *align, *ebox;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  combo = gtk_combo_box_text_new ();
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                    0, 360, 1);
  label = gtk_label_new ("This label may be ellipsized\nto make it fit.");

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "NONE");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "START");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "MIDDLE");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "END");
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);

  ebox = gtk_event_box_new ();
  gtk_widget_set_app_paintable (ebox, TRUE);
  gtk_container_add (GTK_CONTAINER (ebox), align);

  gtk_box_pack_start (GTK_BOX (vbox), combo, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), scale, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), ebox, TRUE, TRUE, 0);

  g_object_set_data (G_OBJECT (label), "combo", combo);

  g_signal_connect (combo, "changed", G_CALLBACK (combo_changed_cb), label);
  g_signal_connect (scale, "value-changed", G_CALLBACK (scale_changed_cb), label);
  g_signal_connect (ebox, "draw", G_CALLBACK (ebox_draw_cb), label);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
