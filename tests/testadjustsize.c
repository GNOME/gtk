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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
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
  return gtk_image_new_from_icon_name ("document-open",
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
on_draw_alignment (GtkWidget      *widget,
                   cairo_t        *cr,
                   void           *data)
{
  cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
  cairo_paint (cr);

  return FALSE;
}

static GtkWidget*
create_alignment (void)
{
  GtkWidget *alignment;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
G_GNUC_END_IGNORE_DEPRECATIONS

  /* make the alignment visible */
  gtk_widget_set_redraw_on_allocate (alignment, TRUE);
  g_signal_connect (G_OBJECT (alignment),
                    "draw",
                    G_CALLBACK (on_draw_alignment),
                    NULL);

  return alignment;
}

static void
open_test_window (void)
{
  GtkWidget *grid;
  int i;

  test_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (test_window), "Tests");

  g_signal_connect (test_window, "delete-event",
                    G_CALLBACK (gtk_main_quit), test_window);

  gtk_window_set_resizable (GTK_WINDOW (test_window), FALSE);

  test_widgets[TEST_WIDGET_IMAGE] = create_image ();
  test_widgets[TEST_WIDGET_LABEL] = create_label (FALSE, FALSE);
  test_widgets[TEST_WIDGET_VERTICAL_LABEL] = create_label (TRUE, FALSE);
  test_widgets[TEST_WIDGET_WRAP_LABEL] = create_label (FALSE, TRUE);
  test_widgets[TEST_WIDGET_BUTTON] = create_button ();
  test_widgets[TEST_WIDGET_ALIGNMENT] = create_alignment ();

  grid = gtk_grid_new ();

  gtk_container_add (GTK_CONTAINER (test_window), grid);

  for (i = 0; i < TEST_WIDGET_LAST; ++i)
    {
      gtk_grid_attach (GTK_GRID (grid), test_widgets[i], i % 3, i / 3, 1, 1);
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
  gtk_window_set_title (GTK_WINDOW (window), "Controls");

  g_signal_connect (window, "delete-event",
                    G_CALLBACK (gtk_main_quit), window);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
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

#define TEST_WIDGET(outer) (gtk_bin_get_child (GTK_BIN (gtk_bin_get_child (GTK_BIN(outer)))))

static GtkWidget*
create_widget_visible_border (const char *text)
{
  GtkWidget *outer_box;
  GtkWidget *inner_box;
  GtkWidget *test_widget;
  GtkWidget *label;

  outer_box = gtk_event_box_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (outer_box), "black-bg");

  inner_box = gtk_event_box_new ();
  gtk_container_set_border_width (GTK_CONTAINER (inner_box), 5);
  gtk_style_context_add_class (gtk_widget_get_style_context (inner_box), "blue-bg");

  gtk_container_add (GTK_CONTAINER (outer_box), inner_box);


  test_widget = gtk_event_box_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (test_widget), "red-bg");

  gtk_container_add (GTK_CONTAINER (inner_box), test_widget);

  label = gtk_label_new (text);
  gtk_container_add (GTK_CONTAINER (test_widget), label);

  g_assert (TEST_WIDGET (outer_box) == test_widget);

  gtk_widget_show_all (outer_box);

  return outer_box;
}

static const char*
enum_to_string (GType enum_type,
                int   value)
{
  GEnumValue *v;

  v = g_enum_get_value (g_type_class_peek (enum_type), value);

  return v->value_nick;
}

static GtkWidget*
create_aligned (GtkAlign halign,
                GtkAlign valign)
{
  GtkWidget *widget;
  char *label;

  label = g_strdup_printf ("h=%s v=%s",
                           enum_to_string (GTK_TYPE_ALIGN, halign),
                           enum_to_string (GTK_TYPE_ALIGN, valign));

  widget = create_widget_visible_border (label);

  g_object_set (G_OBJECT (TEST_WIDGET (widget)),
                "halign", halign,
                "valign", valign,
                "hexpand", TRUE,
                "vexpand", TRUE,
                NULL);

  return widget;
}

static void
open_alignment_window (void)
{
  GtkWidget *grid;
  int i;
  GEnumClass *align_class;

  test_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (test_window), "Alignment");

  g_signal_connect (test_window, "delete-event",
                    G_CALLBACK (gtk_main_quit), test_window);

  gtk_window_set_resizable (GTK_WINDOW (test_window), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (test_window), 500, 500);

  align_class = g_type_class_peek (GTK_TYPE_ALIGN);

  grid = gtk_grid_new ();
  gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);
  gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);

  gtk_container_add (GTK_CONTAINER (test_window), grid);

  for (i = 0; i < align_class->n_values; ++i)
    {
      int j;
      for (j = 0; j < align_class->n_values; ++j)
        {
          GtkWidget *child =
            create_aligned(align_class->values[i].value,
                           align_class->values[j].value);

          gtk_grid_attach (GTK_GRID (grid), child, i, j, 1, 1);
        }
    }

  gtk_widget_show_all (test_window);
}

static GtkWidget*
create_margined (const char *propname)
{
  GtkWidget *widget;

  widget = create_widget_visible_border (propname);

  g_object_set (G_OBJECT (TEST_WIDGET (widget)),
                propname, 15,
                "hexpand", TRUE,
                "vexpand", TRUE,
                NULL);

  return widget;
}

static void
open_margin_window (void)
{
  GtkWidget *box;
  int i;
  const char * margins[] = {
    "margin-start",
    "margin-end",
    "margin-top",
    "margin-bottom",
    "margin"
  };

  test_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (test_window), "Margin");

  g_signal_connect (test_window, "delete-event",
                    G_CALLBACK (gtk_main_quit), test_window);

  gtk_window_set_resizable (GTK_WINDOW (test_window), TRUE);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_container_add (GTK_CONTAINER (test_window), box);

  for (i = 0; i < (int) G_N_ELEMENTS (margins); ++i)
    {
      GtkWidget *child =
        create_margined(margins[i]);

      gtk_container_add (GTK_CONTAINER (box), child);
    }

  gtk_widget_show_all (test_window);
}

static void
open_valigned_label_window (void)
{
  GtkWidget *window, *box, *label, *frame;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_signal_connect (test_window, "delete-event",
                    G_CALLBACK (gtk_main_quit), test_window);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (window), box);

  label = gtk_label_new ("Both labels expand");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  label = gtk_label_new ("Some wrapping text with width-chars = 15 and max-width-chars = 35");
  gtk_label_set_line_wrap  (GTK_LABEL (label), TRUE);
  gtk_label_set_width_chars  (GTK_LABEL (label), 15);
  gtk_label_set_max_width_chars  (GTK_LABEL (label), 35);

  gtk_widget_show (label);

  frame  = gtk_frame_new (NULL);
  gtk_widget_show (frame);
  gtk_container_add (GTK_CONTAINER (frame), label);

  gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);

  gtk_box_pack_start (GTK_BOX (box), frame, TRUE, TRUE, 0);

  gtk_window_present (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  GtkCssProvider *provider;

  gtk_init (&argc, &argv);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider,
    ".black-bg { background-color: black; }"
    ".red-bg { background-color: red; }"
    ".blue-bg { background-color: blue; }", -1, NULL);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);
  
  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  open_test_window ();
  open_control_window ();
  open_alignment_window ();
  open_margin_window ();
  open_valigned_label_window ();

  gtk_main ();

  return 0;
}
