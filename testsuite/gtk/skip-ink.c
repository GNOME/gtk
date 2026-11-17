/* GTK - The GIMP Toolkit
 * Copyright (C) 2026 Christian Hergert <christian@sourceandstack.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

static guint
count_underline_rectangles (GskRenderNode *node)
{
  GskRenderNodeType type = gsk_render_node_get_node_type (node);
  guint count = 0;

  if (type == GSK_COLOR_NODE)
    {
      const GdkRGBA *color = gsk_color_node_get_color (node);

      return color->red == 1 && color->green == 0 && color->blue == 0;
    }
  else if (type == GSK_CONTAINER_NODE)
    {
      for (guint i = 0; i < gsk_container_node_get_n_children (node); i++)
        count += count_underline_rectangles (gsk_container_node_get_child (node, i));
    }
  else if (type == GSK_TRANSFORM_NODE)
    {
      count = count_underline_rectangles (gsk_transform_node_get_child (node));
    }
  else if (type == GSK_CLIP_NODE)
    {
      count = count_underline_rectangles (gsk_clip_node_get_child (node));
    }
  else if (type == GSK_ROUNDED_CLIP_NODE)
    {
      count = count_underline_rectangles (gsk_rounded_clip_node_get_child (node));
    }
  else if (type == GSK_OPACITY_NODE)
    {
      count = count_underline_rectangles (gsk_opacity_node_get_child (node));
    }

  return count;
}

static void
test_skip_ink (gconstpointer data)
{
  const char *kind = data;
  const GdkRGBA red = { 1, 0, 0, 1 };
  GtkWidget *window = gtk_window_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *widget;
  GtkCssProvider *provider = gtk_css_provider_new ();
  PangoAttrList *attrs = pango_attr_list_new ();
  PangoFontMap *fontmap = pango_cairo_font_map_new ();
  GError *error = NULL;
  PangoLayout *layout;
  int width, height;
  guint continuous_rectangles = 0;
  const struct {
    const char *parent_class;
    const char *child_class;
    PangoSkipInk expected;
  } cases[] = {
    { NULL, NULL, PANGO_SKIP_INK_AUTO },
    { "none", NULL, PANGO_SKIP_INK_NONE },
    { "all", NULL, PANGO_SKIP_INK_ALL },
    { "none", "initial", PANGO_SKIP_INK_AUTO },
    { "none", "inherit", PANGO_SKIP_INK_NONE },
    { "all", "unset", PANGO_SKIP_INK_ALL },
    { "none", "all", PANGO_SKIP_INK_ALL },
    { "all", "none", PANGO_SKIP_INK_NONE },
    { "none", "shorthand", PANGO_SKIP_INK_ALL },
  };

  gtk_css_provider_load_from_string (provider,
                                     "window.skip-ink-test { font: 32px Cantarell; }"
                                     ".none { text-decoration-skip-ink: none; }"
                                     ".all { text-decoration-skip-ink: all; }"
                                     ".initial { text-decoration-skip-ink: initial; }"
                                     ".inherit { text-decoration-skip-ink: inherit; }"
                                     ".unset { text-decoration-skip-ink: unset; }"
                                     ".shorthand { text-decoration-skip-ink: all; text-decoration: none; }");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
  gtk_widget_add_css_class (window, "skip-ink-test");
  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 100);
  pango_font_map_add_font_file (fontmap,
                                g_test_get_filename (G_TEST_DIST, "../gsk/fonts/Cantarell-VF.otf", NULL),
                                &error);
  g_assert_no_error (error);
  gtk_widget_set_font_map (window, fontmap);

  pango_attr_list_insert (attrs, pango_attr_underline_new (PANGO_UNDERLINE_SINGLE));
  pango_attr_list_insert (attrs, pango_attr_underline_color_new (65535, 0, 0));

  if (g_str_equal (kind, "label"))
    {
      widget = gtk_label_new ("AgypA");
      gtk_label_set_attributes (GTK_LABEL (widget), attrs);
    }
  else if (g_str_equal (kind, "entry"))
    {
      widget = gtk_entry_new ();
      gtk_editable_set_text (GTK_EDITABLE (widget), "AgypA");
      gtk_entry_set_attributes (GTK_ENTRY (widget), attrs);
    }
  else
    {
      GtkTextBuffer *buffer;
      GtkTextTag *tag;
      GtkTextIter start, end;

      widget = gtk_text_view_new ();
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      gtk_text_buffer_set_text (buffer, "AgypA\nאבג AgypA", -1);
      tag = gtk_text_buffer_create_tag (buffer, NULL,
                                        "underline", PANGO_UNDERLINE_SINGLE,
                                        "underline-rgba", &red,
                                        NULL);
      gtk_text_buffer_get_bounds (buffer, &start, &end);
      gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
    }

  gtk_widget_set_vexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (box), widget);
  gtk_window_present (GTK_WINDOW (window));
  g_test_message ("Renderer: %s", G_OBJECT_TYPE_NAME (gtk_native_get_renderer (GTK_NATIVE (window))));
  layout = gtk_widget_create_pango_layout (widget, "AgypA");
  pango_layout_get_size (layout, &width, &height);

  for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      GtkSnapshot *snapshot;
      GskRenderNode *node;
      guint rectangles;
      int new_width, new_height;

      if (i > 0)
        {
          if (cases[i - 1].parent_class)
            gtk_widget_remove_css_class (box, cases[i - 1].parent_class);
          if (cases[i - 1].child_class)
            gtk_widget_remove_css_class (widget, cases[i - 1].child_class);
        }

      if (cases[i].parent_class)
        gtk_widget_add_css_class (box, cases[i].parent_class);

      if (cases[i].child_class)
        gtk_widget_add_css_class (widget, cases[i].child_class);

      gtk_test_widget_wait_for_draw (window);
      g_assert_cmpint (pango_context_get_skip_ink (gtk_widget_get_pango_context (widget)), ==, cases[i].expected);
      pango_layout_get_size (layout, &new_width, &new_height);
      g_assert_cmpint (new_width, ==, width);
      g_assert_cmpint (new_height, ==, height);
      snapshot = gtk_snapshot_new ();
      gtk_widget_snapshot_child (box, widget, snapshot);
      node = gtk_snapshot_free_to_node (snapshot);
      g_assert_nonnull (node);
      rectangles = count_underline_rectangles (node);
      g_test_message ("%s case %u: %u underline rectangles", kind, i, rectangles);

      if (cases[i].expected == PANGO_SKIP_INK_NONE)
        {
          if (continuous_rectangles == 0)
            continuous_rectangles = rectangles;

          g_assert_cmpuint (rectangles, ==, continuous_rectangles);
          g_assert_cmpuint (rectangles, >=, 1);

          if (g_str_equal (kind, "label"))
            g_assert_cmpuint (rectangles, ==, 1);
        }
      else
        {
          g_assert_cmpuint (rectangles, >, MAX (continuous_rectangles, 1));
        }

      gsk_render_node_unref (node);
    }

  g_object_unref (layout);
  pango_attr_list_unref (attrs);
  gtk_window_destroy (GTK_WINDOW (window));
  gtk_style_context_remove_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (provider));
  g_object_unref (provider);
  g_object_unref (fontmap);
}

static void
parsing_error (GtkCssProvider *provider,
               GtkCssSection  *section,
               const GError   *error,
               guint          *errors)
{
  g_assert_error (error, GTK_CSS_PARSER_ERROR, GTK_CSS_PARSER_ERROR_SYNTAX);
  (*errors)++;
}

static void
test_invalid_skip_ink (void)
{
  const char *values[] = { "true", "false", "normal", "1", "auto none" };
  GtkCssProvider *provider = gtk_css_provider_new ();
  guint errors = 0;

  g_signal_connect (provider, "parsing-error", G_CALLBACK (parsing_error), &errors);

  for (guint i = 0; i < G_N_ELEMENTS (values); i++)
    {
      char *css = g_strdup_printf ("label { text-decoration-skip-ink: %s; }", values[i]);

      gtk_css_provider_load_from_string (provider, css);
      g_assert_cmpuint (errors, ==, i + 1);
      g_free (css);
    }

  g_object_unref (provider);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);
  g_test_add_func ("/skip-ink/invalid", test_invalid_skip_ink);
  g_test_add_data_func ("/skip-ink/label", "label", test_skip_ink);
  g_test_add_data_func ("/skip-ink/entry", "entry", test_skip_ink);
  g_test_add_data_func ("/skip-ink/textview", "textview", test_skip_ink);
  return g_test_run ();
}
