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
test_search_full_buffer (void)
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

  /* check also that different composition of utf8 characters
     (e.g. accented letters) match */

  check_found_forward ("This is some \303\200 text", "\303\200", 0, 13, 14, "\303\200");
  check_found_forward ("This is some \303\200 text", "some \303\200", 0, 8, 14, "some \303\200");
  check_found_forward ("This is some \303\200 text", "\303\200 text", 0, 13, 19, "\303\200 text");
  check_found_forward ("This is some \303\200 text", "some \303\200 text", 0, 8, 19, "some \303\200 text");
  check_found_backward ("This is some \303\240 text", "\303\240", 0, 13, 14, "\303\240");
  check_found_backward ("This is some \303\240 text", "some \303\240", 0, 8, 14, "some \303\240");
  check_found_backward ("This is some \303\240 text", "\303\240 text", 0, 13, 19, "\303\240 text");
  check_found_backward ("This is some \303\240 text", "some \303\240 text", 0, 8, 19, "some \303\240 text");
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

static void
test_forward_to_tag_toggle (void)
{
  GtkTextBuffer *buffer;
  GtkTextTag *bold_tag;
  GtkTextTag *editable_tag;
  GtkTextIter iter;
  gint offset;

  buffer = gtk_text_buffer_new (NULL);

  bold_tag = gtk_text_buffer_create_tag (buffer, "bold",
                                         "weight", PANGO_WEIGHT_BOLD,
                                         NULL);

  editable_tag = gtk_text_buffer_create_tag (buffer, "not-editable",
                                             "editable", FALSE,
                                             NULL);

  gtk_text_buffer_get_start_iter (buffer, &iter);

  gtk_text_buffer_insert (buffer, &iter, "a", -1);
  gtk_text_buffer_insert_with_tags (buffer, &iter, "b", -1, bold_tag, NULL);
  gtk_text_buffer_insert_with_tags (buffer, &iter, "c", -1, editable_tag, NULL);

  /* Go to the first "on" toggle */
  gtk_text_buffer_get_start_iter (buffer, &iter);
  g_assert (gtk_text_iter_forward_to_tag_toggle (&iter, NULL));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 1);

  /* Go to the last "off" toggle for the bold tag */
  g_assert (gtk_text_iter_forward_to_tag_toggle (&iter, bold_tag));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 2);

  g_assert (!gtk_text_iter_forward_to_tag_toggle (&iter, bold_tag));

  /* Go to the first "on" toggle for the editable tag */
  gtk_text_buffer_get_start_iter (buffer, &iter);
  g_assert (gtk_text_iter_forward_to_tag_toggle (&iter, editable_tag));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 2);

  /* Test with the end iter */
  gtk_text_buffer_get_end_iter (buffer, &iter);
  g_assert (!gtk_text_iter_forward_to_tag_toggle (&iter, editable_tag));

  g_object_unref (buffer);
}

static void
check_word_boundaries (const gchar *buffer_text,
                       gint         offset,
                       gboolean     starts_word,
                       gboolean     ends_word,
                       gboolean     inside_word)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, buffer_text, -1);

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);

  g_assert_cmpint (starts_word, ==, gtk_text_iter_starts_word (&iter));
  g_assert_cmpint (ends_word, ==, gtk_text_iter_ends_word (&iter));
  g_assert_cmpint (inside_word, ==, gtk_text_iter_inside_word (&iter));

  g_object_unref (buffer);
}

static void
check_forward_word_end (const gchar *buffer_text,
                        gint         initial_offset,
                        gint         result_offset,
                        gboolean     ret)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, buffer_text, -1);

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, initial_offset);

  g_assert_cmpint (ret, ==, gtk_text_iter_forward_word_end (&iter));
  g_assert_cmpint (result_offset, ==, gtk_text_iter_get_offset (&iter));

  g_object_unref (buffer);
}

static void
check_backward_word_start (const gchar *buffer_text,
                           gint         initial_offset,
                           gint         result_offset,
                           gboolean     ret)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, buffer_text, -1);

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, initial_offset);

  g_assert_cmpint (ret, ==, gtk_text_iter_backward_word_start (&iter));
  g_assert_cmpint (result_offset, ==, gtk_text_iter_get_offset (&iter));

  g_object_unref (buffer);
}

static void
test_word_boundaries (void)
{
  /* Test with trivial content. The word boundaries are anyway determined by
   * Pango and can change in the future for corner cases.
   */

  check_word_boundaries ("ab ", 0, TRUE, FALSE, TRUE);
  check_word_boundaries ("ab ", 1, FALSE, FALSE, TRUE);
  check_word_boundaries ("ab ", 2, FALSE, TRUE, FALSE);
  check_word_boundaries ("ab ", 3, FALSE, FALSE, FALSE);

  check_forward_word_end ("ab ", 0, 2, TRUE);
  check_forward_word_end ("ab ", 1, 2, TRUE);
  check_forward_word_end ("ab ", 2, 2, FALSE);
  check_forward_word_end ("ab ", 3, 3, FALSE);
  check_forward_word_end ("ab", 0, 2, FALSE);

  check_backward_word_start (" ab", 3, 1, TRUE);
  check_backward_word_start (" ab", 2, 1, TRUE);
  check_backward_word_start (" ab", 1, 1, FALSE);
  check_backward_word_start (" ab", 0, 0, FALSE);
  check_backward_word_start ("ab", 2, 0, TRUE);
}

static void
check_forward_visible_word_end (GtkTextBuffer *buffer,
                                gint           initial_offset,
                                gint           result_offset,
                                gboolean       ret)
{
  GtkTextIter iter;

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, initial_offset);

  g_assert_cmpint (ret, ==, gtk_text_iter_forward_visible_word_end (&iter));
  g_assert_cmpint (result_offset, ==, gtk_text_iter_get_offset (&iter));
}

static void
check_backward_visible_word_start (GtkTextBuffer *buffer,
                                   gint           initial_offset,
                                   gint           result_offset,
                                   gboolean       ret)
{
  GtkTextIter iter;

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, initial_offset);

  g_assert_cmpint (ret, ==, gtk_text_iter_backward_visible_word_start (&iter));
  g_assert_cmpint (result_offset, ==, gtk_text_iter_get_offset (&iter));
}

static void
test_visible_word_boundaries (void)
{
  /* Test with trivial content. The word boundaries are anyway determined by
   * Pango and can change in the future for corner cases.
   */

  GtkTextBuffer *buffer;
  GtkTextTag *invisible_tag;
  GtkTextIter iter;

  buffer = gtk_text_buffer_new (NULL);

  invisible_tag = gtk_text_buffer_create_tag (buffer, NULL,
                                              "invisible", TRUE,
                                              NULL);

  /* Buffer contents: " a b c " with " b " invisible */
  gtk_text_buffer_get_start_iter (buffer, &iter);
  gtk_text_buffer_insert (buffer, &iter, " a", -1);
  gtk_text_buffer_insert_with_tags (buffer, &iter, " b ", -1, invisible_tag, NULL);
  gtk_text_buffer_insert (buffer, &iter, "c ", -1);

  check_forward_visible_word_end (buffer, 0, 6, TRUE);
  check_forward_visible_word_end (buffer, 1, 6, TRUE);
  check_forward_visible_word_end (buffer, 2, 6, TRUE);
  check_forward_visible_word_end (buffer, 3, 6, TRUE);
  check_forward_visible_word_end (buffer, 4, 6, TRUE);
  check_forward_visible_word_end (buffer, 5, 6, TRUE);
  check_forward_visible_word_end (buffer, 6, 6, FALSE);
  check_forward_visible_word_end (buffer, 7, 7, FALSE);

  check_backward_visible_word_start (buffer, 7, 5, TRUE); /* FIXME result_offset should be 1 */
  check_backward_visible_word_start (buffer, 6, 5, TRUE); /* FIXME result_offset should be 1 */
  check_backward_visible_word_start (buffer, 5, 1, TRUE);
  check_backward_visible_word_start (buffer, 4, 1, TRUE);
  check_backward_visible_word_start (buffer, 3, 1, TRUE);
  check_backward_visible_word_start (buffer, 2, 1, TRUE);
  check_backward_visible_word_start (buffer, 1, 1, FALSE);
  check_backward_visible_word_start (buffer, 0, 0, FALSE);

  gtk_text_buffer_set_text (buffer, "ab", -1);
  check_forward_visible_word_end (buffer, 0, 2, FALSE);

  /* Buffer contents: "b c " with "b" invisible */
  gtk_text_buffer_set_text (buffer, "", -1);
  gtk_text_buffer_get_start_iter (buffer, &iter);
  gtk_text_buffer_insert_with_tags (buffer, &iter, "b", -1, invisible_tag, NULL);
  gtk_text_buffer_insert (buffer, &iter, " c ", -1);

  check_forward_visible_word_end (buffer, 0, 1, TRUE); /* FIXME result_offset should be 3 */

  g_object_unref (buffer);
}

static void
check_cursor_position (const gchar *buffer_text,
                       gboolean     forward,
                       gint         initial_offset,
                       gint         result_offset,
                       gboolean     ret)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, buffer_text, -1);

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, initial_offset);

  if (forward)
    g_assert_cmpint (ret, ==, gtk_text_iter_forward_cursor_position (&iter));
  else
    g_assert_cmpint (ret, ==, gtk_text_iter_backward_cursor_position (&iter));

  g_assert_cmpint (result_offset, ==, gtk_text_iter_get_offset (&iter));

  g_object_unref (buffer);
}

static void
test_cursor_positions (void)
{
  /* forward */
  check_cursor_position ("a\r\nb", TRUE, 0, 1, TRUE);
  check_cursor_position ("a\r\nb", TRUE, 1, 3, TRUE);
  check_cursor_position ("a\r\nb", TRUE, 2, 3, TRUE);
  check_cursor_position ("a\r\nb", TRUE, 3, 4, FALSE);
  check_cursor_position ("a\r\nb", TRUE, 4, 4, FALSE);

  /* backward */
  check_cursor_position ("a\r\nb", FALSE, 4, 3, TRUE);
  check_cursor_position ("a\r\nb", FALSE, 3, 1, TRUE);
  check_cursor_position ("a\r\nb", FALSE, 2, 1, TRUE);
  check_cursor_position ("a\r\nb", FALSE, 1, 0, TRUE);
  check_cursor_position ("a\r\nb", FALSE, 0, 0, FALSE);
}

static void
check_visible_cursor_position (GtkTextBuffer *buffer,
                               gboolean       forward,
                               gint           initial_offset,
                               gint           result_offset,
                               gboolean       ret)
{
  GtkTextIter iter;

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, initial_offset);

  if (forward)
    g_assert_cmpint (ret, ==, gtk_text_iter_forward_visible_cursor_position (&iter));
  else
    g_assert_cmpint (ret, ==, gtk_text_iter_backward_visible_cursor_position (&iter));

  g_assert_cmpint (result_offset, ==, gtk_text_iter_get_offset (&iter));
}

static void
test_visible_cursor_positions (void)
{
  GtkTextBuffer *buffer;
  GtkTextTag *invisible_tag;
  GtkTextIter iter;

  buffer = gtk_text_buffer_new (NULL);

  invisible_tag = gtk_text_buffer_create_tag (buffer, NULL,
                                              "invisible", TRUE,
                                              NULL);

  /* Buffer contents: "abcd" with 'bc' invisible */
  gtk_text_buffer_get_start_iter (buffer, &iter);
  gtk_text_buffer_insert (buffer, &iter, "a", -1);
  gtk_text_buffer_insert_with_tags (buffer, &iter, "bc", -1, invisible_tag, NULL);
  gtk_text_buffer_insert (buffer, &iter, "d", -1);

  /* forward */
  check_visible_cursor_position (buffer, TRUE, 0, 3, TRUE);
  check_visible_cursor_position (buffer, TRUE, 1, 3, TRUE);
  check_visible_cursor_position (buffer, TRUE, 2, 3, TRUE);
  check_visible_cursor_position (buffer, TRUE, 3, 4, FALSE);
  check_visible_cursor_position (buffer, TRUE, 4, 4, FALSE);

  /* backward */
  check_visible_cursor_position (buffer, FALSE, 4, 3, TRUE);
  check_visible_cursor_position (buffer, FALSE, 3, 0, TRUE);
  check_visible_cursor_position (buffer, FALSE, 2, 0, TRUE);
  check_visible_cursor_position (buffer, FALSE, 1, 0, TRUE);
  check_visible_cursor_position (buffer, FALSE, 0, 0, FALSE);

  g_object_unref (buffer);
}

static void
check_sentence_boundaries (const gchar *buffer_text,
                           gint         offset,
                           gboolean     starts_sentence,
                           gboolean     ends_sentence,
                           gboolean     inside_sentence)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, buffer_text, -1);

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);

  g_assert_cmpint (starts_sentence, ==, gtk_text_iter_starts_sentence (&iter));
  g_assert_cmpint (ends_sentence, ==, gtk_text_iter_ends_sentence (&iter));
  g_assert_cmpint (inside_sentence, ==, gtk_text_iter_inside_sentence (&iter));

  g_object_unref (buffer);
}

static void
check_forward_sentence_end (const gchar *buffer_text,
                            gint         initial_offset,
                            gint         result_offset,
                            gboolean     ret)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, buffer_text, -1);

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, initial_offset);

  g_assert_cmpint (ret, ==, gtk_text_iter_forward_sentence_end (&iter));
  g_assert_cmpint (result_offset, ==, gtk_text_iter_get_offset (&iter));

  g_object_unref (buffer);
}

static void
check_backward_sentence_start (const gchar *buffer_text,
                               gint         initial_offset,
                               gint         result_offset,
                               gboolean     ret)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, buffer_text, -1);

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, initial_offset);

  g_assert_cmpint (ret, ==, gtk_text_iter_backward_sentence_start (&iter));
  g_assert_cmpint (result_offset, ==, gtk_text_iter_get_offset (&iter));

  g_object_unref (buffer);
}

static void
test_sentence_boundaries (void)
{
  check_sentence_boundaries ("Hi. ", 0, TRUE, FALSE, TRUE);
  check_sentence_boundaries ("Hi. ", 1, FALSE, FALSE, TRUE);
  check_sentence_boundaries ("Hi. ", 2, FALSE, FALSE, TRUE);
  check_sentence_boundaries ("Hi. ", 3, FALSE, TRUE, FALSE);
  check_sentence_boundaries ("Hi. ", 4, FALSE, FALSE, FALSE);

  check_forward_sentence_end ("Hi. ", 0, 3, TRUE);
  check_forward_sentence_end ("Hi. ", 1, 3, TRUE);
  check_forward_sentence_end ("Hi. ", 2, 3, TRUE);
  check_forward_sentence_end ("Hi. ", 3, 3, FALSE);
  check_forward_sentence_end ("Hi. ", 4, 4, FALSE);
  check_forward_sentence_end ("Hi.", 0, 3, FALSE);

  check_backward_sentence_start (" Hi.", 4, 1, TRUE);
  check_backward_sentence_start (" Hi.", 3, 1, TRUE);
  check_backward_sentence_start (" Hi.", 2, 1, TRUE);
  check_backward_sentence_start (" Hi.", 1, 1, FALSE);
  check_backward_sentence_start (" Hi.", 0, 0, FALSE);
}

int
main (int argc, char** argv)
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/TextIter/Search Empty", test_empty_search);
  g_test_add_func ("/TextIter/Search Full Buffer", test_search_full_buffer);
  g_test_add_func ("/TextIter/Search", test_search);
  g_test_add_func ("/TextIter/Search Caseless", test_search_caseless);
  g_test_add_func ("/TextIter/Forward To Tag Toggle", test_forward_to_tag_toggle);
  g_test_add_func ("/TextIter/Word Boundaries", test_word_boundaries);
  g_test_add_func ("/TextIter/Visible Word Boundaries", test_visible_word_boundaries);
  g_test_add_func ("/TextIter/Cursor Positions", test_cursor_positions);
  g_test_add_func ("/TextIter/Visible Cursor Positions", test_visible_cursor_positions);
  g_test_add_func ("/TextIter/Sentence Boundaries", test_sentence_boundaries);

  return g_test_run();
}
