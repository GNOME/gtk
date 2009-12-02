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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
ebox_expose_event_cb (GtkWidget      *widget,
                      GdkEventExpose *event,
                      gpointer        data)
{
  PangoLayout *layout;
  const double dashes[] = { 6, 18 };
  GtkRequisition natural_size;
  GtkWidget *label = data;
  gint x, y, dx, dy;
  gchar *markup;
  cairo_t *cr;

  gtk_widget_translate_coordinates (label, widget, 0, 0, &x, &y);

  cr = gdk_cairo_create (widget->window);
  cairo_translate (cr, -0.5, -0.5);
  cairo_set_line_width (cr, 1);

  cairo_rectangle (cr,
                   x + 0.5 * (label->allocation.width - label->requisition.width),
                   y + 0.5 * (label->allocation.height - label->requisition.height),
                   label->requisition.width, label->requisition.height);
  cairo_set_source_rgb (cr, 0.8, 0.2, 0.2);
  cairo_set_dash (cr, NULL, 0, 0);
  cairo_stroke (cr);

  cairo_rectangle (cr, x, y, label->allocation.width, label->allocation.height);
  cairo_set_source_rgb (cr, 0.2, 0.2, 0.8);
  cairo_set_dash (cr, dashes, 2, 0.5);
  cairo_stroke (cr);

  gtk_extended_layout_get_desired_size (GTK_EXTENDED_LAYOUT (label),
                                        NULL, &natural_size);

  cairo_rectangle (cr,
                   x + 0.5 * (label->allocation.width - natural_size.width),
                   y + 0.5 * (label->allocation.height - natural_size.height),
                   natural_size.width, natural_size.height);
  cairo_set_source_rgb (cr, 0.2, 0.8, 0.2);
  cairo_set_dash (cr, dashes, 2, 12.5);
  cairo_stroke (cr);

  markup = g_strdup_printf (
    "<span color='#c33'>\342\200\242 requisition:\t%dx%d</span>\n"
    "<span color='#3c3'>\342\200\242 natural size:\t%dx%d</span>\n"
    "<span color='#33c'>\342\200\242 allocation:\t%dx%d</span>",
    label->requisition.width, label->requisition.height,
    natural_size.width, natural_size.height,
    label->allocation.width, label->allocation.height);

  layout = gtk_widget_create_pango_layout (widget, NULL);
  pango_layout_set_markup (layout, markup, -1);
  pango_layout_get_pixel_size (layout, &dx, &dy);

  g_free (markup);

  cairo_translate (cr, 0, widget->allocation.height - dy - 8);

  cairo_set_source_rgba (cr, 1, 1, 1, 0.8);
  cairo_rectangle (cr, 0, 0, dx + 12, dy + 8);
  cairo_fill (cr);

  cairo_translate (cr, 6, 4);
  pango_cairo_show_layout (cr, layout);

  g_object_unref (layout);
  cairo_destroy (cr);

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

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  combo = gtk_combo_box_new_text ();
  scale = gtk_hscale_new_with_range (0, 360, 1);
  label = gtk_label_new ("This label may be ellipsized\nto make it fit.");

  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), "NONE");
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), "START");
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), "MIDDLE");
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), "END");
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
  g_signal_connect_after (ebox, "expose-event", G_CALLBACK (ebox_expose_event_cb), label);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
