/* testextendedlayout.c
 * Copyright (C) 2007 Mathias Hasselmann <mathias.hasselmann@gmx.de>
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

//#include <config.h>
#include <gtk/gtk.h>

GList *allocation_guides = NULL;

static void
append_natural_size_box (GtkWidget          *vbox,
                         const gchar        *caption,
                         PangoEllipsizeMode  ellipsize)
{
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *label;

  hbox = gtk_hbox_new (FALSE, 12);

  label = gtk_label_new ("The small Button");
  gtk_label_set_ellipsize (GTK_LABEL (label), ellipsize);

  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), label);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);

  if (PANGO_ELLIPSIZE_NONE == ellipsize)
    allocation_guides = g_list_append (allocation_guides, button);

  button = gtk_button_new_with_label ("The large Button");
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), caption); 
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
}

static GtkWidget*
create_natural_size ()
{
  GtkWidget *vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  append_natural_size_box (vbox, "<b>No ellipsizing</b>", PANGO_ELLIPSIZE_NONE);
  append_natural_size_box (vbox, "<b>Ellipsizing at start</b>", PANGO_ELLIPSIZE_START);
  append_natural_size_box (vbox, "<b>Ellipsizing in the middle</b>", PANGO_ELLIPSIZE_MIDDLE);
  append_natural_size_box (vbox, "<b>Ellipsizing at end</b>", PANGO_ELLIPSIZE_END);

  return vbox;
}

static GtkWidget*
create_height_for_width ()
{
  GtkWidget *vbox;

  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

  return vbox;
}

static GtkWidget*
create_baseline ()
{
  GtkWidget *table;

  table = gtk_table_new (4, 3, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);

  return table;
}

static gboolean           
expose_page (GtkWidget      *page,
             GdkEventExpose *event __attribute__((unused)),
             gpointer        user_data)
{
  cairo_t *cr = gdk_cairo_create (page->window);
  GList *guides = user_data;
  GList *iter;

/*  cairo_rectangle (cr, event->area.x, event->area.y,
                       event->area.width, event->area.height);
  cairo_clip (cr);*/

  cairo_translate (cr, page->allocation.x - 0.5, 
                       page->allocation.y - 0.5);

  cairo_set_line_width (cr, 1);
  cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 0.5);

  for (iter = guides; iter; iter = iter->next)
    {
      GtkWidget *child = iter->data;
      gint x0, y0, x1, y1;

      if (GTK_WIDGET_VISIBLE (child) &&
          gtk_widget_translate_coordinates (child, page, 0, 0, &x0, &y0))
        {
          x1 = x0 + child->allocation.width;
          y1 = y0 + child->allocation.height;

          cairo_move_to (cr, x0, 0);
          cairo_line_to (cr, x0, page->allocation.height - 1);

          cairo_move_to (cr, x1, 0);
          cairo_line_to (cr, x1, page->allocation.height - 1);

          cairo_move_to (cr, 0, y0);
          cairo_line_to (cr, page->allocation.width - 1, y0);

          cairo_move_to (cr, 0, y1);
          cairo_line_to (cr, page->allocation.width - 1, y1);

          cairo_stroke (cr);
        }
    }

  cairo_destroy (cr);
  return FALSE;
}

static void
append_testcase(GtkWidget   *notebook,
                GtkWidget   *testcase,
                const gchar *caption)
{
  GtkWidget *alignment;

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (alignment), testcase);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), alignment,
                            gtk_label_new (caption));

  g_signal_connect_after (testcase, "expose-event",
                          G_CALLBACK (expose_page),
                          allocation_guides);

  allocation_guides = NULL;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *notebook;

  gtk_init (&argc, &argv);

  notebook = gtk_notebook_new ();

  append_testcase (notebook, create_natural_size (), "Natural Size");
  append_testcase (notebook, create_height_for_width (), "Height for Width");
  append_testcase (notebook, create_baseline (), "Baseline Alignment");

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_window_set_title (GTK_WINDOW (window), "Testing GtkExtendedLayout");
  gtk_container_add (GTK_CONTAINER (window), notebook);
  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}

/* vim: set sw=2 sta et: */
