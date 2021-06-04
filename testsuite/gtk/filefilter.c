/* GtkFileFilter tests
 *
 * Copyright (C) 2021, Red Hat, Inc.
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

#include <locale.h>

#include <gtk/gtk.h>

static void
test_basic (void)
{
  GtkFileFilter *filter;

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "test");
  g_assert_cmpstr (gtk_file_filter_get_name (filter), ==, "test");

  g_object_unref (filter);
}

static void
test_pattern (void)
{
  GtkFileFilter *filter;
  char *attrs;
  GFileInfo *info;

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "D*.st[xy]");
  attrs = g_strjoinv (",", (char **)gtk_file_filter_get_attributes (filter));

  info = g_file_info_new ();

  g_file_info_set_display_name (info, "abracadabra");
  g_assert_false (gtk_filter_match (GTK_FILTER (filter), info));

  g_file_info_set_display_name (info, "dro.stx");
  g_assert_false (gtk_filter_match (GTK_FILTER (filter), info));

  g_file_info_set_display_name (info, "Dro.sty");
  g_assert_true (gtk_filter_match (GTK_FILTER (filter), info));

  g_object_unref (info);
  g_free (attrs);
  g_object_unref (filter);
}

static void
test_suffix (void)
{
  GtkFileFilter *filter;
  char *attrs;
  GFileInfo *info;

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_suffix (filter, "txt");
  attrs = g_strjoinv (",", (char **)gtk_file_filter_get_attributes (filter));

  info = g_file_info_new ();

  g_file_info_set_display_name (info, "abracadabra");
  g_assert_false (gtk_filter_match (GTK_FILTER (filter), info));

  g_file_info_set_display_name (info, "dro.txt");
  g_assert_true (gtk_filter_match (GTK_FILTER (filter), info));

  g_file_info_set_display_name (info, "dro.TXT");
  g_assert_true (gtk_filter_match (GTK_FILTER (filter), info));

  g_object_unref (info);
  g_free (attrs);
  g_object_unref (filter);
}

static void
test_mime_type (void)
{
  GtkFileFilter *filter;
  char *attrs;
  GFileInfo *info;

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "image/png");
  attrs = g_strjoinv (",", (char **)gtk_file_filter_get_attributes (filter));

  info = g_file_info_new ();

  g_file_info_set_display_name (info, "abracadabra");
  g_file_info_set_content_type (info, "text/plain");
  g_assert_false (gtk_filter_match (GTK_FILTER (filter), info));

  g_file_info_set_display_name (info, "dro.png");
  g_file_info_set_content_type (info, "image/png");
  g_assert_true (gtk_filter_match (GTK_FILTER (filter), info));

  g_file_info_set_display_name (info, "dro.PNG");
  g_file_info_set_content_type (info, "image/png");
  g_assert_true (gtk_filter_match (GTK_FILTER (filter), info));

  g_object_unref (info);
  g_free (attrs);
  g_object_unref (filter);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/filefilter/basic", test_basic);
  g_test_add_func ("/filefilter/pattern", test_pattern);
  g_test_add_func ("/filefilter/suffix", test_suffix);
  g_test_add_func ("/filefilter/mimetype", test_mime_type);

  return g_test_run ();
}
