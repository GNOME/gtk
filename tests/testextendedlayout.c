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

#include <config.h>
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
  GtkWidget *child;
  GtkWidget *view;
  GtkWidget *label;

  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);

  child = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (child), "Test...");
  gtk_table_attach (GTK_TABLE (table), child, 1, 2, 0, 1, 
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  label = gtk_label_new_with_mnemonic ("_Title:");
  allocation_guides = g_list_append (allocation_guides, label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_label_set_mnemonic_widget  (GTK_LABEL (label), child);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, 
                    GTK_FILL, GTK_FILL, 0, 0);

  label = gtk_label_new_with_mnemonic ("Notice on\ntwo rows.");
  allocation_guides = g_list_append (allocation_guides, label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach (GTK_TABLE (table), label, 2, 3, 0, 2, 
                    GTK_FILL, GTK_FILL, 0, 0);

  child = gtk_font_button_new ();
  gtk_table_attach (GTK_TABLE (table), child, 1, 2, 1, 2, 
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  label = gtk_label_new_with_mnemonic ("_Font:");
  allocation_guides = g_list_append (allocation_guides, label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_label_set_mnemonic_widget  (GTK_LABEL (label), child);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, 
                    GTK_FILL, GTK_FILL, 0, 0);

  view = gtk_text_view_new ();
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)),
                            "Lorem ipsem...", -1);

  child = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (child),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (child),
                                       GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (child), view);

  gtk_table_attach (GTK_TABLE (table), child, 1, 3, 2, 3, 
                    GTK_FILL | GTK_EXPAND,
                    GTK_FILL | GTK_EXPAND, 
                    0, 0);

  label = gtk_label_new_with_mnemonic ("_Comment:");
  allocation_guides = g_list_append (allocation_guides, label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_label_set_mnemonic_widget  (GTK_LABEL (label), child);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                    GTK_FILL, GTK_FILL, 0, 0);

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

  cairo_rectangle (cr, event->area.x, event->area.y,
                       event->area.width, event->area.height);
  cairo_clip (cr);

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

          if (GTK_IS_LABEL (child)) 
            {
              PangoLayout *layout;
              PangoLayoutLine *line;
              PangoRectangle log;
              gint ybase;

              layout = gtk_label_get_layout (GTK_LABEL (child));
              line = pango_layout_get_line (layout, 0);
              pango_layout_line_get_extents (line, NULL, &log);
              gtk_label_get_layout_offsets (GTK_LABEL (child), NULL, &ybase);

              ybase -= child->allocation.y;
              ybase += PANGO_PIXELS (log.height);

              cairo_move_to (cr, 0, y0 + ybase);
              cairo_line_to (cr, page->allocation.width, y0 + ybase);
            }
          else
            {
              cairo_move_to (cr, x0, 0);
              cairo_line_to (cr, x0, page->allocation.height);

              cairo_move_to (cr, x1, 0);
              cairo_line_to (cr, x1, page->allocation.height);

              cairo_move_to (cr, 0, y0);
              cairo_line_to (cr, page->allocation.width, y0);

              cairo_move_to (cr, 0, y1);
              cairo_line_to (cr, page->allocation.width, y1);
            }

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
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), testcase,
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
