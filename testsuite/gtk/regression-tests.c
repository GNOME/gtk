/* Regression tests
 *
 * Copyright (C) 2011, Red Hat, Inc.
 * Authors: Benjamin Otte <otte@gnome.org>
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

static void
test_9d6da33ff5c5e41e3521e1afd63d2d67bc915753 (void)
{
  GtkWidget *window, *label;

  window = gtk_window_new ();
  label = gtk_label_new ("I am sensitive.");
  gtk_window_set_child (GTK_WINDOW (window), label);

  gtk_widget_set_sensitive (label, FALSE);
  gtk_widget_set_sensitive (window, FALSE);
  gtk_widget_set_sensitive (label, TRUE);
  gtk_widget_set_sensitive (window, TRUE);

  g_assert_true (gtk_widget_get_sensitive (label));

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_94f00eb04dd1433cf1cc9a3341f485124e38abd1 (void)
{
  GtkWidget *window, *label;

  window = gtk_window_new ();
  label = gtk_label_new ("I am insensitive.");
  gtk_window_set_child (GTK_WINDOW (window), label);

  gtk_widget_set_sensitive (window, FALSE);
  gtk_widget_set_sensitive (label, FALSE);
  gtk_widget_set_sensitive (label, TRUE);

  g_assert_false (gtk_widget_is_sensitive (label));

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_picture_dispose_twice (void)
{
  /* https://gitlab.gnome.org/GNOME/gtk/-/issues/8335
   *
   * gtk_picture_clear_paintable() left self->paintable dangling after
   * dispose. Dispose can run more than once when something else still
   * references the picture (e.g. a gesture's point data referencing the
   * event target), and the second dispose then unreffed the dangling
   * paintable: a stolen reference, or a use-after-free once the paintable
   * was already finalized.
   */
  GtkWidget *picture;
  GdkTexture *texture;
  GBytes *bytes;
  gpointer weak;

  bytes = g_bytes_new_take (g_malloc0 (4 * 4 * 4), 4 * 4 * 4);
  texture = gdk_memory_texture_new (4, 4, GDK_MEMORY_R8G8B8A8, bytes, 16);
  g_bytes_unref (bytes);
  weak = texture;
  g_object_add_weak_pointer (G_OBJECT (texture), &weak);

  picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (texture));
  g_object_ref_sink (picture);
  g_object_ref (picture); /* an outside reference keeps the picture alive */

  g_object_run_dispose (G_OBJECT (picture));
  g_object_unref (picture);
  /* the last unref disposes the zombie a second time; it must not touch
   * the paintable released by the first dispose */
  g_object_unref (picture);

  g_assert_nonnull (weak); /* our texture reference must still be alive */
  g_object_remove_weak_pointer (G_OBJECT (texture), &weak);
  g_object_unref (texture);
}

static void
test_text_view_reset_buffer (gconstpointer use_property)
{
  GtkWidget *window = gtk_window_new ();
  GtkTextView *view = GTK_TEXT_VIEW (gtk_text_view_new ());
  GtkTextBuffer *original = g_object_ref (gtk_text_view_get_buffer (view));
  GtkTextBuffer *replacement;
  GtkTextIter iter;
  GdkRectangle rect;

  gtk_widget_set_focusable (GTK_WIDGET (view), FALSE);
  gtk_text_buffer_set_text (original, "First paragraph\nSecond paragraph", -1);
  gtk_window_set_child (GTK_WINDOW (window), GTK_WIDGET (view));
  gtk_window_present (GTK_WINDOW (window));
  gtk_test_widget_wait_for_draw (GTK_WIDGET (view));

  if (use_property)
    g_object_set (view, "buffer", NULL, NULL);
  else
    gtk_text_view_set_buffer (view, NULL);

  gtk_test_widget_wait_for_draw (GTK_WIDGET (view));
  replacement = gtk_text_view_get_buffer (view);
  g_assert_true (replacement != original);
  g_assert_cmpint (gtk_text_buffer_get_char_count (replacement), ==, 0);

  gtk_text_buffer_set_text (replacement, "Replacement", -1);
  gtk_text_buffer_get_start_iter (replacement, &iter);
  gtk_text_view_get_iter_location (view, &iter, &rect);
  g_assert_cmpint (rect.height, >, 0);

  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (original);
}

static void
test_text_view_detached_layout_cursor (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkTextView *view = GTK_TEXT_VIEW (gtk_text_view_new ());

  gtk_window_set_child (GTK_WINDOW (window), GTK_WIDGET (view));
  gtk_window_present (GTK_WINDOW (window));
  gtk_test_widget_wait_for_draw (GTK_WIDGET (view));

  gtk_window_set_focus (GTK_WINDOW (window), GTK_WIDGET (view));
  g_assert_true (gtk_widget_is_focus (GTK_WIDGET (view)));

  gtk_text_view_set_buffer (view, NULL);
  gtk_window_set_focus (GTK_WINDOW (window), NULL);

  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/regression/94f00eb04dd1433cf1cc9a3341f485124e38abd1", test_94f00eb04dd1433cf1cc9a3341f485124e38abd1);
  g_test_add_func ("/regression/9d6da33ff5c5e41e3521e1afd63d2d67bc915753", test_9d6da33ff5c5e41e3521e1afd63d2d67bc915753);
  g_test_add_func ("/regression/picture-dispose-twice", test_picture_dispose_twice);
  g_test_add_data_func ("/regression/textview/reset-buffer", NULL, test_text_view_reset_buffer);
  g_test_add_data_func ("/regression/textview/reset-buffer-property", GINT_TO_POINTER (1), test_text_view_reset_buffer);
  g_test_add_func ("/regression/textview/detached-layout-cursor", test_text_view_detached_layout_cursor);

  return g_test_run ();
}
