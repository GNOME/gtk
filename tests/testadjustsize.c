/* testadjustsize.c
 * Copyright (C) 2010 Havoc Pennington
 *
 * Author:
 *      Havoc Pennington <hp@pobox.com>
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

static GtkWidget *test_window;

enum {
  TEST_WIDGET_LABEL,
  TEST_WIDGET_VERTICAL_LABEL,
  TEST_WIDGET_WRAP_LABEL,
  TEST_WIDGET_ALIGNMENT,
  TEST_WIDGET_IMAGE,
  TEST_WIDGET_BUTTON,
  TEST_WIDGET_LAST
};

static GtkWidget *test_widgets[TEST_WIDGET_LAST];

static GtkWidget*
create_image (void)
{
  return gtk_image_new_from_stock (GTK_STOCK_OPEN,
                                   GTK_ICON_SIZE_BUTTON);
}

static GtkWidget*
create_label (gboolean vertical,
              gboolean wrap)
{
  GtkWidget *widget;

  widget = gtk_label_new ("This is a label, label label label");

  if (vertical)
    gtk_label_set_angle (GTK_LABEL (widget), 90);

  if (wrap)
    gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);

  return widget;
}

static GtkWidget*
create_button (void)
{
  return gtk_button_new_with_label ("BUTTON!");
}

static gboolean
on_expose_alignment (GtkWidget      *widget,
                     GdkEventExpose *event,
                     void           *data)
{
  cairo_t *cr;
  GtkAllocation allocation;

  cr = gdk_cairo_create (event->window);

  cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
  gtk_widget_get_allocation (widget, &allocation);
  cairo_rectangle (cr,
                   allocation.x,
                   allocation.y,
                   allocation.width,
                   allocation.height);
  cairo_fill (cr);

  cairo_destroy (cr);

  return FALSE;
}

static GtkWidget*
create_alignment (void)
{
  GtkWidget *alignment;

  alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);

  /* make the alignment visible */
  gtk_widget_set_redraw_on_allocate (alignment, TRUE);
  g_signal_connect (G_OBJECT (alignment),
                    "expose-event",
                    G_CALLBACK (on_expose_alignment),
                    NULL);

  return alignment;
}

static void
open_test_window (void)
{
  GtkWidget *table;
  int i;

  test_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_signal_connect (test_window, "delete-event",
                    G_CALLBACK (gtk_main_quit), test_window);

  gtk_window_set_resizable (GTK_WINDOW (test_window), FALSE);

  test_widgets[TEST_WIDGET_IMAGE] = create_image ();
  test_widgets[TEST_WIDGET_LABEL] = create_label (FALSE, FALSE);
  test_widgets[TEST_WIDGET_VERTICAL_LABEL] = create_label (TRUE, FALSE);
  test_widgets[TEST_WIDGET_WRAP_LABEL] = create_label (FALSE, TRUE);
  test_widgets[TEST_WIDGET_BUTTON] = create_button ();
  test_widgets[TEST_WIDGET_ALIGNMENT] = create_alignment ();

  table = gtk_table_new (2, 3, FALSE);

  gtk_container_add (GTK_CONTAINER (test_window), table);

  for (i = 0; i < TEST_WIDGET_LAST; ++i)
    {
      gtk_table_attach (GTK_TABLE (table),
                        test_widgets[i],
                        i % 3,
                        i % 3 + 1,
                        i / 3,
                        i / 3 + 1,
                        0, 0, 0, 0);
    }

  gtk_widget_show_all (test_window);
}

static void
on_toggle_border_widths (GtkToggleButton *button,
                         void            *data)
{
  gboolean has_border;
  int i;

  has_border = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  for (i = 0; i < TEST_WIDGET_LAST; ++i)
    {
      if (GTK_IS_CONTAINER (test_widgets[i]))
        {
          gtk_container_set_border_width (GTK_CONTAINER (test_widgets[i]),
                                          has_border ? 50 : 0);
        }
    }
}

static void
on_set_small_size_requests (GtkToggleButton *button,
                            void            *data)
{
  gboolean has_small_size_requests;
  int i;

  has_small_size_requests = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  for (i = 0; i < TEST_WIDGET_LAST; ++i)
    {
      gtk_widget_set_size_request (test_widgets[i],
                                   has_small_size_requests ? 5 : -1,
                                   has_small_size_requests ? 5 : -1);
    }
}

static void
on_set_large_size_requests (GtkToggleButton *button,
                            void            *data)
{
  gboolean has_large_size_requests;
  int i;

  has_large_size_requests = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  for (i = 0; i < TEST_WIDGET_LAST; ++i)
    {
      gtk_widget_set_size_request (test_widgets[i],
                                   has_large_size_requests ? 200 : -1,
                                   has_large_size_requests ? 200 : -1);
    }
}

static void
open_control_window (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *toggle;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_signal_connect (window, "delete-event",
                    G_CALLBACK (gtk_main_quit), window);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), box);

  toggle =
    gtk_toggle_button_new_with_label ("Containers have borders");
  g_signal_connect (G_OBJECT (toggle),
                    "toggled", G_CALLBACK (on_toggle_border_widths),
                    NULL);
  gtk_container_add (GTK_CONTAINER (box), toggle);

  toggle =
    gtk_toggle_button_new_with_label ("Set small size requests");
  g_signal_connect (G_OBJECT (toggle),
                    "toggled", G_CALLBACK (on_set_small_size_requests),
                    NULL);
  gtk_container_add (GTK_CONTAINER (box), toggle);

  toggle =
    gtk_toggle_button_new_with_label ("Set large size requests");
  g_signal_connect (G_OBJECT (toggle),
                    "toggled", G_CALLBACK (on_set_large_size_requests),
                    NULL);
  gtk_container_add (GTK_CONTAINER (box), toggle);


  gtk_widget_show_all (window);
}

int
main (int argc, char *argv[])
{
  gtk_init (&argc, &argv);

  open_test_window ();
  open_control_window ();

  gtk_main ();

  return 0;
}
