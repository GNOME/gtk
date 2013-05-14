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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

static void
test_empty_search ()
{
  GtkTextBuffer *buffer;
  GtkTextIter it, s, e;
  gboolean res;

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, "This is some foo text", -1);

  /* search from start forward */
  gtk_text_buffer_get_start_iter (buffer, &it);
  res = gtk_text_iter_forward_search (&it, "", 0, &s, &e, NULL);
  g_assert (res);
  g_assert_cmpint (gtk_text_iter_get_offset (&s), ==, gtk_text_iter_get_offset (&e));
  g_assert_cmpint (gtk_text_iter_get_offset (&s), ==, 1);

  /* search from end backward */
  gtk_text_buffer_get_end_iter (buffer, &it);
  res = gtk_text_iter_backward_search (&it, "", 0, &s, &e, NULL);
  g_assert (res);
  g_assert_cmpint (gtk_text_iter_get_offset (&s), ==, gtk_text_iter_get_offset (&e));
  g_assert_cmpint (gtk_text_iter_get_offset (&s), ==, 20);
}

static void
check_found_forward (const gchar *haystack,
                     const gchar *needle,
                     GtkTextSearchFlags flags,
                     int expected_start,
                     int expected_end,
                     const gchar *expected_string)
{
  GtkTextBuffer *buffer;
  GtkTextIter i, s, e;
  gboolean res;
  gchar *text;

  buffer = gtk_text_buffer_new (NULL);

  gtk_text_buffer_set_text (buffer, haystack, -1);

  /* TODO: add test with limit before, after and in the middle
     of expected start and end */

  /* search from start forward */
  gtk_text_buffer_get_start_iter (buffer, &i);
  res = gtk_text_iter_forward_search (&i, needle, flags, &s, &e, NULL);
  g_assert (res);
  g_assert_cmpint (expected_start, ==, gtk_text_iter_get_offset (&s));
  g_assert_cmpint (expected_end, ==, gtk_text_iter_get_offset (&e));
  text = gtk_text_iter_get_text (&s, &e);
  g_assert_cmpstr (expected_string, ==, text);
  g_free (text);

  g_object_unref (buffer);
}

static void
check_found_backward (const gchar *haystack,
                      const gchar *needle,
                      GtkTextSearchFlags flags,
                      int expected_start,
                      int expected_end,
                      const gchar *expected_string)
{
  GtkTextBuffer *buffer;
  GtkTextIter i, s, e;
  gboolean res;
  gchar *text;

  buffer = gtk_text_buffer_new (NULL);

  gtk_text_buffer_set_text (buffer, haystack, -1);

  /* search from end backward */
  gtk_text_buffer_get_end_iter (buffer, &i);
  res = gtk_text_iter_backward_search (&i, needle, flags, &s, &e, NULL);
  g_assert (res);
  g_assert_cmpint (expected_start, ==, gtk_text_iter_get_offset (&s));
  g_assert_cmpint (expected_end, ==, gtk_text_iter_get_offset (&e));
  text = gtk_text_iter_get_text (&s, &e);
  g_assert_cmpstr (expected_string, ==, text);
  g_free (text);

  g_object_unref (buffer);
}

static void
check_not_found (const gchar *haystack,
                 const gchar *needle,
                 GtkTextSearchFlags flags)
{
  GtkTextBuffer *buffer;
  GtkTextIter i, s, e;
  gboolean res;

  buffer = gtk_text_buffer_new (NULL);

  gtk_text_buffer_set_text (buffer, haystack, -1);

  /* search from start forward */
  gtk_text_buffer_get_start_iter (buffer, &i);
  res = gtk_text_iter_forward_search (&i, needle, flags, &s, &e, NULL);
  g_assert (res == FALSE);

  /* search from end backward */
  gtk_text_buffer_get_end_iter (buffer, &i);
  res = gtk_text_iter_backward_search (&i, needle, flags, &s, &e, NULL);
  g_assert (res == FALSE);

  g_object_unref (buffer);
}

static void
test_full_buffer (void)
{
  check_found_forward ("foo", "foo", 0, 0, 3, "foo");
  check_found_backward ("foo", "foo", 0, 0, 3, "foo");
  check_found_forward ("foo", "foo", GTK_TEXT_SEARCH_CASE_INSENSITIVE, 0, 3, "foo");
  check_found_backward ("foo", "foo", GTK_TEXT_SEARCH_CASE_INSENSITIVE, 0, 3, "foo");
  check_found_forward ("foo", "Foo", GTK_TEXT_SEARCH_CASE_INSENSITIVE, 0, 3, "foo");
  check_found_backward ("foo", "Foo", GTK_TEXT_SEARCH_CASE_INSENSITIVE, 0, 3, "foo");
}

static void
test_search (void)
{
  /* simple match */
  check_found_forward ("This is some foo text", "foo", 0, 13, 16, "foo");
  check_found_backward ("This is some foo text", "foo", 0, 13, 16, "foo");
  check_not_found ("This is some foo text", "Foo", 0);

  /* different matches for forward and backward */
  check_found_forward ("This is some foo foo text", "foo", 0, 13, 16, "foo");
  check_found_backward ("This is some foo foo text", "foo", 0, 17, 20, "foo");

  /* new lines in the haystack */
  check_found_forward ("This is some\nfoo text", "foo", 0, 13, 16, "foo");
  check_found_backward ("This is some\nfoo text", "foo", 0, 13, 16, "foo");
  check_found_forward ("This is some foo\nfoo text", "foo", 0, 13, 16, "foo");
  check_found_backward ("This is some foo\nfoo text", "foo", 0, 17, 20, "foo");
  check_not_found ("This is some\nfoo text", "Foo", 0);

  /* end of buffer */
  check_found_forward ("This is some\ntext foo", "foo", 0, 18, 21, "foo");
  check_found_backward ("This is some\ntext foo", "foo", 0, 18, 21, "foo");
  check_not_found ("This is some\ntext foo", "Foo", 0);

  /* multiple lines in the needle */
  check_found_forward ("This is some foo\nfoo text", "foo\nfoo", 0, 13, 20, "foo\nfoo");
  check_found_backward ("This is some foo\nfoo text", "foo\nfoo", 0, 13, 20, "foo\nfoo");
  check_not_found ("This is some foo\nfoo text", "Foo\nfoo", 0);
}

static void
test_search_caseless (void)
{
  GtkTextSearchFlags flags;

  flags = GTK_TEXT_SEARCH_CASE_INSENSITIVE;

  /* simple match */
  check_found_forward ("This is some foo text", "foo", flags, 13, 16, "foo");
  check_found_forward ("This is some foo text", "Foo", flags, 13, 16, "foo");
  check_found_forward ("This is some Foo text", "foo", flags, 13, 16, "Foo");
  check_found_backward ("This is some foo text", "foo", flags, 13, 16, "foo");
  check_found_backward ("This is some foo text", "Foo", flags, 13, 16, "foo");
  check_found_backward ("This is some Foo text", "foo", flags, 13, 16, "Foo");

  /* check also that different composition of utf8 characters
     (e.g. accented letters) match */

  /* different matches for forward and backward */
  check_found_forward ("This is some foo foo text", "foo", flags, 13, 16, "foo");
  check_found_forward ("This is some foo foo text", "Foo", flags, 13, 16, "foo");
  check_found_forward ("This is some Foo foo text", "foo", flags, 13, 16, "Foo");
  check_found_forward ("This is some \303\200 \303\240 text", "\303\240", flags, 13, 14, "\303\200");
  check_found_forward ("This is some \303\200 \303\240 text", "\303\200", flags, 13, 14, "\303\200");
  check_found_forward ("This is some \303\200 \303\240 text", "a\314\200", flags, 13, 14, "\303\200");
  check_found_backward ("This is some foo foo text", "foo", flags, 17, 20, "foo");
  check_found_backward ("This is some foo foo text", "Foo", flags, 17, 20, "foo");
  check_found_backward ("This is some foo Foo text", "foo", flags, 17, 20, "Foo");
  check_found_backward ("This is some \303\200 \303\240 text", "\303\240", flags, 15, 16, "\303\240");
  check_found_backward ("This is some \303\200 \303\240 text", "\303\200", flags, 15, 16, "\303\240");
  check_found_backward ("This is some \303\200 \303\240 text", "a\314\200", flags, 15, 16, "\303\240");

  /* new lines in the haystack */
  check_found_forward ("This is some\nfoo text", "foo", flags, 13, 16, "foo");
  check_found_forward ("This is some\nfoo text", "Foo", flags, 13, 16, "foo");
  check_found_forward ("This is some\nFoo text", "foo", flags, 13, 16, "Foo");
  check_found_forward ("This is some\n\303\200 text", "\303\240", flags, 13, 14, "\303\200");
  check_found_forward ("This is some\n\303\200 text", "a\314\200", flags, 13, 14, "\303\200");
  check_found_backward ("This is some\nfoo text", "foo", flags, 13, 16, "foo");
  check_found_backward ("This is some\nfoo text", "Foo", flags, 13, 16, "foo");
  check_found_backward ("This is some\nFoo text", "foo", flags, 13, 16, "Foo");
  check_found_backward ("This is some\n\303\200 text", "\303\240", flags, 13, 14, "\303\200");
  check_found_backward ("This is some\n\303\200 text", "a\314\200", flags, 13, 14, "\303\200");
  check_found_forward ("This is some foo\nfoo text", "foo", flags, 13, 16, "foo");
  check_found_forward ("This is some foo\nfoo text", "Foo", flags, 13, 16, "foo");
  check_found_forward ("This is some Foo\nfoo text", "foo", flags, 13, 16, "Foo");
  check_found_forward ("This is some \303\200\n\303\200 text", "\303\240", flags, 13, 14, "\303\200");
  check_found_forward ("This is some \303\200\n\303\200 text", "a\314\200", flags, 13, 14, "\303\200");
  check_found_backward ("This is some foo\nfoo text", "foo", flags, 17, 20, "foo");
  check_found_backward ("This is some foo\nfoo text", "Foo", flags, 17, 20, "foo");
  check_found_backward ("This is some foo\nFoo text", "foo", flags, 17, 20, "Foo");
  check_found_backward ("This is some \303\200\n\303\200 text", "\303\240", flags, 15, 16, "\303\200");
  check_found_backward ("This is some \303\200\n\303\200 text", "a\314\200", flags, 15, 16, "\303\200");

  /* end of buffer */
  check_found_forward ("This is some\ntext foo", "foo", flags, 18, 21, "foo");
  check_found_forward ("This is some\ntext foo", "Foo", flags, 18, 21, "foo");
  check_found_forward ("This is some\ntext Foo", "foo", flags, 18, 21, "Foo");
  check_found_forward ("This is some\ntext \303\200", "\303\240", flags, 18, 19, "\303\200");
  check_found_forward ("This is some\ntext \303\200", "a\314\200", flags, 18, 19, "\303\200");
  check_found_backward ("This is some\ntext foo", "foo", flags, 18, 21, "foo");
  check_found_backward ("This is some\ntext foo", "Foo", flags, 18, 21, "foo");
  check_found_backward ("This is some\ntext Foo", "foo", flags, 18, 21, "Foo");
  check_found_backward ("This is some\ntext \303\200", "\303\240", flags, 18, 19, "\303\200");
  check_found_backward ("This is some\ntext \303\200", "a\314\200", flags, 18, 19, "\303\200");

  /* multiple lines in the needle */
  check_found_forward ("This is some foo\nfoo text", "foo\nfoo", flags, 13, 20, "foo\nfoo");
  check_found_forward ("This is some foo\nfoo text", "Foo\nFoo", flags, 13, 20, "foo\nfoo");
  check_found_forward ("This is some Foo\nFoo text", "foo\nfoo", flags, 13, 20, "Foo\nFoo");
  check_found_forward ("This is some \303\200\n\303\200 text", "\303\240\n\303\240", flags, 13, 16, "\303\200\n\303\200");
  check_found_forward ("This is some \303\200\n\303\200 text", "a\314\200\na\314\200", flags, 13, 16, "\303\200\n\303\200");
  check_found_backward ("This is some foo\nfoo text", "foo\nfoo", flags, 13, 20, "foo\nfoo");
  check_found_backward ("This is some foo\nfoo text", "Foo\nFoo", flags, 13, 20, "foo\nfoo");
  check_found_backward ("This is some Foo\nFoo text", "foo\nfoo", flags, 13, 20, "Foo\nFoo");
  check_found_backward ("This is some \303\200\n\303\200 text", "\303\240\n\303\240", flags, 13, 16, "\303\200\n\303\200");
  check_found_backward ("This is some \303\200\n\303\200 text", "a\314\200\na\314\200", flags, 13, 16, "\303\200\n\303\200");
}

int
main (int argc, char** argv)
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/TextIter/Search Empty", test_empty_search);
  g_test_add_func ("/TextIter/Search Full Buffer", test_full_buffer);
  g_test_add_func ("/TextIter/Search", test_search);
  g_test_add_func ("/TextIter/Search Caseless", test_search_caseless);

  return g_test_run();
}
