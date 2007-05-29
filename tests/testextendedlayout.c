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

typedef struct _TestCase TestCase;

struct _TestCase
{
  const gchar *name;
  GtkWidget *widget;
  GList *guides;
  guint idle;
};

static void
append_natural_size_box (TestCase           *test,
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
    test->guides = g_list_append (test->guides, button);

  button = gtk_button_new_with_label ("The large Button");
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), caption); 
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  gtk_box_pack_start (GTK_BOX (test->widget), label, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (test->widget), hbox, FALSE, TRUE, 0);
}

static TestCase*
create_natural_size_test ()
{
  TestCase *test = g_new0 (TestCase, 1);

  test->name = "Natural Size";
  test->widget = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (test->widget), 12);

  append_natural_size_box (test,
                           "<b>No ellipsizing</b>",
                           PANGO_ELLIPSIZE_NONE);
  append_natural_size_box (test,
                           "<b>Ellipsizing at start</b>",
                           PANGO_ELLIPSIZE_START);
  append_natural_size_box (test,
                           "<b>Ellipsizing in the middle</b>",
                           PANGO_ELLIPSIZE_MIDDLE);
  append_natural_size_box (test,
                           "<b>Ellipsizing at end</b>",
                           PANGO_ELLIPSIZE_END);

  return test;
}

static TestCase*
create_height_for_width_test ()
{
  TestCase *test = g_new0 (TestCase, 1);

  test->name = "Height for Width";
  test->widget = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (test->widget), 12);

  return test;
}

static TestCase*
create_baseline_test ()
{
  GtkWidget *child;
  GtkWidget *view;
  GtkWidget *label;

  TestCase *test = g_new0 (TestCase, 1);

  test->name = "Baseline Alignment";
  test->widget = gtk_table_new (3, 3, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (test->widget), 12);
  gtk_table_set_col_spacings (GTK_TABLE (test->widget), 6);
  gtk_table_set_row_spacings (GTK_TABLE (test->widget), 6);

  child = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (child), "Test...");
  gtk_table_attach (GTK_TABLE (test->widget), child, 1, 2, 0, 1, 
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  label = gtk_label_new_with_mnemonic ("_Title:");
  test->guides = g_list_append (test->guides, label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_label_set_mnemonic_widget  (GTK_LABEL (label), child);
  gtk_table_attach (GTK_TABLE (test->widget), label, 0, 1, 0, 1, 
                    GTK_FILL, GTK_FILL, 0, 0);

  label = gtk_label_new_with_mnemonic ("Notice on\ntwo rows.");
  test->guides = g_list_append (test->guides, label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach (GTK_TABLE (test->widget), label, 2, 3, 0, 2, 
                    GTK_FILL, GTK_FILL, 0, 0);

  child = gtk_font_button_new ();
  gtk_table_attach (GTK_TABLE (test->widget), child, 1, 2, 1, 2, 
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  label = gtk_label_new_with_mnemonic ("_Font:");
  test->guides = g_list_append (test->guides, label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_label_set_mnemonic_widget  (GTK_LABEL (label), child);
  gtk_table_attach (GTK_TABLE (test->widget), label, 0, 1, 1, 2, 
                    GTK_FILL, GTK_FILL, 0, 0);

  view = gtk_text_view_new ();
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view),
                               GTK_WRAP_WORD);
  test->guides = g_list_append (test->guides, view);
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)),
                            "Lorem ipsum dolor sit amet, consectetuer "
                            "adipiscing elit. Aliquam sed erat. Proin lectus "
                            "orci, venenatis pharetra, egestas id, tincidunt "
                            "vel, eros. Integer fringilla. Aenean justo ipsum, "        
                            "luctus ut, volutpat laoreet, vehicula in, libero.", 
                            -1);

  child = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (child),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (child),
                                       GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (child), view);

  gtk_table_attach (GTK_TABLE (test->widget), child, 1, 3, 2, 3, 
                    GTK_FILL | GTK_EXPAND,
                    GTK_FILL | GTK_EXPAND, 
                    0, 0);

  label = gtk_label_new_with_mnemonic ("_Comment:");
  test->guides = g_list_append (test->guides, label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_label_set_mnemonic_widget  (GTK_LABEL (label), child);
  gtk_table_attach (GTK_TABLE (test->widget), label, 0, 1, 2, 3,
                    GTK_FILL, GTK_FILL, 0, 0);

  return test;
}

static gboolean
get_extends (GtkWidget    *widget,
             GtkWidget    *toplevel,
             GdkRectangle *extends)
{
  *extends = widget->allocation;

  return
    GTK_WIDGET_VISIBLE (widget) &&
    gtk_widget_translate_coordinates (widget, toplevel, 0, 0, 
                                      &extends->x, &extends->y);
}

static gint
get_baseline_of_layout (PangoLayout *layout)
{
  PangoLayoutLine *line = pango_layout_get_line_readonly (layout, 0);
  PangoRectangle log;

  pango_layout_line_get_extents (line, NULL, &log);
  return PANGO_PIXELS (PANGO_ASCENT (log));
}

static gint
get_baseline_of_label (GtkLabel *label)
{
  PangoLayout *layout = gtk_label_get_layout (label);
  gint base;

  gtk_label_get_layout_offsets (label, NULL, &base);

  base -= GTK_WIDGET (label)->allocation.y;
  base += get_baseline_of_layout (layout);

  return base;
}

static gint
get_baseline_of_text_view (GtkTextView *view)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (view);
  GtkTextAttributes *attrs = gtk_text_view_get_default_attributes (view);
  PangoContext *context = gtk_widget_get_pango_context (GTK_WIDGET (view));
  PangoLayout *layout = pango_layout_new (context);

  GtkTextIter start, end;
  GdkRectangle bounds;
  gchar *text;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_iter_get_attributes (&start, attrs);

  end = start;
  gtk_text_iter_forward_to_line_end (&end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  gtk_text_view_get_iter_location (view, &start, &bounds);

  pango_layout_set_width (layout, PANGO_SCALE *
                          GTK_WIDGET (view)->allocation.width);
  pango_layout_set_font_description (layout, attrs->font);
  pango_layout_set_wrap (layout, PANGO_WRAP_WORD);
  pango_layout_set_text (layout, text, -1);

  gtk_text_view_buffer_to_window_coords (view, GTK_TEXT_WINDOW_TEXT,
                                         0, bounds.y, NULL, &bounds.y);
  bounds.y += get_baseline_of_layout (layout);

  gtk_text_attributes_unref (attrs);
  g_object_unref (layout);
  g_free (text);

  return bounds.y;
}

static gint
get_baseline (GtkWidget *widget)
{
  if (GTK_IS_LABEL (widget))
    return get_baseline_of_label (GTK_LABEL (widget));
  if (GTK_IS_TEXT_VIEW (widget))
    return get_baseline_of_text_view (GTK_TEXT_VIEW (widget));

  return -1;
}

static void
draw_baseline (GdkDrawable  *drawable,
               GdkGC        *gc,
               GtkWidget    *toplevel,
               GdkRectangle *extends,
               gint          baseline)
{
  const gint x0 = toplevel->allocation.x;
  const gint y0 = toplevel->allocation.y;
  const gint cx = toplevel->allocation.width;

  const gint xa = x0 + extends->x;
  const gint xe = xa + extends->width - 1;
  const gint ya = y0 + extends->y + baseline;

  gdk_draw_line (drawable, gc, x0, ya, x0 + cx, ya);
  gdk_draw_line (drawable, gc, xa, ya - 5, xa, ya + 2);
  gdk_draw_line (drawable, gc, xe, ya - 5, xe, ya + 2);
}

static void
draw_extends (GdkDrawable  *drawable,
              GdkGC        *gc,
              GtkWidget    *toplevel,
              GdkRectangle *extends)
{
  const gint x0 = toplevel->allocation.x;
  const gint y0 = toplevel->allocation.y;
  const gint cx = toplevel->allocation.width;
  const gint cy = toplevel->allocation.height;

  const gint xa = x0 + extends->x;
  const gint xe = xa + extends->width - 1;

  const gint ya = y0 + extends->y;
  const gint ye = ya + extends->height - 1;

  gdk_draw_line (drawable, gc, xa, y0, xa, y0 + cy);
  gdk_draw_line (drawable, gc, xe, y0, xe, y0 + cy);
  gdk_draw_line (drawable, gc, x0, ya, x0 + cx, ya);
  gdk_draw_line (drawable, gc, x0, ye, x0 + cx, ye);
}

static gboolean
draw_guides (gpointer data)
{
  TestCase *test = data;
  GdkDrawable *drawable;
  const GList *iter;

  GdkGCValues values;
  GdkGC *gc;

  gdk_color_parse ("#f00", &values.foreground);
  values.subwindow_mode = GDK_INCLUDE_INFERIORS;
  drawable = test->widget->window;

  gc = gdk_gc_new_with_values (drawable, &values, 
                               GDK_GC_SUBWINDOW);
  gdk_gc_set_rgb_fg_color (gc, &values.foreground);


  for (iter = test->guides; iter; iter = iter->next)
    {
      GtkWidget *child = iter->data;
      GdkRectangle extends;

      if (get_extends (child, test->widget, &extends))
        {
          const gint baseline = get_baseline (child);

          if (baseline >= 0)
            draw_baseline (drawable, gc, test->widget, &extends, baseline);
          else
            draw_extends (drawable, gc, test->widget, &extends);
        }
    }

  g_object_unref (gc);
  test->idle = 0;

  return FALSE;
}

static gboolean           
on_expose (GtkWidget      *widget,
           GdkEventExpose *event,
           gpointer        data)
{
  TestCase *test = data;

  if (0 == test->idle)
    {
      if (widget != test->widget)
        gtk_widget_queue_draw (test->widget);

      test->idle = g_idle_add (draw_guides, test);
    }

  return FALSE;
}

static void
on_realize (GtkWidget *widget,
            gpointer   data)
{
  TestCase *test = data;

  if (widget->window != test->widget->window)
    g_signal_connect_after (widget, "expose-event",
                            G_CALLBACK (on_expose), test);
}

static void
attach_sub_windows (GtkWidget *widget,
                    gpointer   data)
{
  g_signal_connect_after (widget, "realize", G_CALLBACK (on_realize), data);

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), attach_sub_windows, data);
}

static void
append_testcase(GtkWidget   *notebook,
                TestCase    *test)
{
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), test->widget,
                            gtk_label_new (test->name));

  g_signal_connect_after (test->widget, "expose-event",
                          G_CALLBACK (on_expose), test);
  g_signal_connect_after (test->widget, "realize",
                          G_CALLBACK (on_realize), test);
  g_object_set_data_full (G_OBJECT(test->widget), 
                          "test-case", test, g_free);

  gtk_container_forall (GTK_CONTAINER (test->widget),
                        attach_sub_windows, test);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *notebook;

  gtk_init (&argc, &argv);

  notebook = gtk_notebook_new ();

  append_testcase (notebook, create_natural_size_test ());
  append_testcase (notebook, create_height_for_width_test ());
  append_testcase (notebook, create_baseline_test ());

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_window_set_title (GTK_WINDOW (window), "Testing GtkExtendedLayout");
  gtk_container_add (GTK_CONTAINER (window), notebook);
  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}

/* vim: set sw=2 sta et: */
