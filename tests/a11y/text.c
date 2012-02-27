/*
 * Copyright (C) 2011 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
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
#include <string.h>

static void
set_text (GtkWidget   *widget,
          const gchar *text)
{
  if (GTK_IS_LABEL (widget))
    gtk_label_set_text (GTK_LABEL (widget), text);
  else if (GTK_IS_ENTRY (widget))
    gtk_entry_set_text (GTK_ENTRY (widget), text);
  else if (GTK_IS_TEXT_VIEW (widget))
    gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget)), text, -1);
  else
    g_assert_not_reached ();
}

static void
test_basic (GtkWidget *widget)
{
  AtkText *atk_text;
  const gchar *text = "Text goes here";
  gchar *ret;
  gint count;
  gunichar c;

  atk_text = ATK_TEXT (gtk_widget_get_accessible (widget));

  set_text (widget, text);
  ret = atk_text_get_text (atk_text, 5, 9);
  g_assert_cmpstr (ret, ==, "goes");
  g_free (ret);

  ret = atk_text_get_text (atk_text, 0, 14);
  g_assert_cmpstr (ret, ==, text);
  g_free (ret);

  ret = atk_text_get_text (atk_text, 0, -1);
  g_assert_cmpstr (ret, ==, text);
  g_free (ret);

  count = atk_text_get_character_count (atk_text);
  g_assert_cmpint (count, ==, g_utf8_strlen (text, -1));

  c = atk_text_get_character_at_offset (atk_text, 0);
  g_assert_cmpint (c, ==, 'T');

  c = atk_text_get_character_at_offset (atk_text, 13);
  g_assert_cmpint (c, ==, 'e');
}

typedef struct {
  gint count;
  gint position;
  gint length;
} SignalData;

static void
text_deleted (AtkText *atk_text, gint position, gint length, SignalData *data)
{
  data->count++;
  data->position = position;
  data->length = length;
}

static void
text_inserted (AtkText *atk_text, gint position, gint length, SignalData *data)
{
  data->count++;
  data->position = position;
  data->length = length;
}

static void
test_text_changed (GtkWidget *widget)
{
  AtkText *atk_text;
  const gchar *text = "Text goes here";
  const gchar *text2 = "Text again";
  SignalData delete_data;
  SignalData insert_data;

  atk_text = ATK_TEXT (gtk_widget_get_accessible (widget));

  delete_data.count = 0;
  insert_data.count = 0;

  g_signal_connect (atk_text, "text_changed::delete",
                    G_CALLBACK (text_deleted), &delete_data);
  g_signal_connect (atk_text, "text_changed::insert",
                    G_CALLBACK (text_inserted), &insert_data);

  set_text (widget, text);

  g_assert_cmpint (delete_data.count, ==, 0);

  g_assert_cmpint (insert_data.count, ==, 1);
  g_assert_cmpint (insert_data.position, ==, 0);
  g_assert_cmpint (insert_data.length, ==, g_utf8_strlen (text, -1));

  set_text (widget, text2);

  g_assert_cmpint (delete_data.count, ==, 1);
  g_assert_cmpint (delete_data.position, ==, 0);
  g_assert_cmpint (delete_data.length, ==, g_utf8_strlen (text, -1));

  g_assert_cmpint (insert_data.count, ==, 2);
  g_assert_cmpint (insert_data.position, ==, 0);
  g_assert_cmpint (insert_data.length, ==, g_utf8_strlen (text2, -1));

  set_text (widget, "");

  g_assert_cmpint (delete_data.count, ==, 2);
  g_assert_cmpint (delete_data.position, ==, 0);
  g_assert_cmpint (delete_data.length, ==, g_utf8_strlen (text2, -1));

  g_assert_cmpint (insert_data.count, ==, 2);

  g_signal_handlers_disconnect_by_func (atk_text, G_CALLBACK (text_deleted), &delete_data);
  g_signal_handlers_disconnect_by_func (atk_text, G_CALLBACK (text_inserted), &insert_data);
}

typedef struct {
  gint gravity;
  gint offset;
  AtkTextBoundary boundary;
  gint start;
  gint end;
  const gchar *word;
} Word;

#ifdef DUMP_RESULTS
static const gchar *
boundary (AtkTextBoundary b)
{
  switch (b)
    {
    case ATK_TEXT_BOUNDARY_CHAR:           return "ATK_TEXT_BOUNDARY_CHAR,          ";
    case ATK_TEXT_BOUNDARY_WORD_START:     return "ATK_TEXT_BOUNDARY_WORD_START,    ";
    case ATK_TEXT_BOUNDARY_WORD_END:       return "ATK_TEXT_BOUNDARY_WORD_END,      ";
    case ATK_TEXT_BOUNDARY_SENTENCE_START: return "ATK_TEXT_BOUNDARY_SENTENCE_START,";
    case ATK_TEXT_BOUNDARY_SENTENCE_END:   return "ATK_TEXT_BOUNDARY_SENTENCE_END,  ";
    case ATK_TEXT_BOUNDARY_LINE_START:     return "ATK_TEXT_BOUNDARY_LINE_START,    ";
    case ATK_TEXT_BOUNDARY_LINE_END:       return "ATK_TEXT_BOUNDARY_LINE_END,      ";
    default: g_assert_not_reached ();
    }
}

static const gchar *
gravity (gint g)
{
  if (g < 0) return "before";
  else if (g > 0) return "after";
  else return "around";
}

const gchar *
char_rep (gunichar c)
{
  static gchar out[6];

  switch (c)
    {
      case '\n': return "\\n";
      case 196: return "?";
      case 214: return "?";
      case 220: return "?";
      default:
        memset (out, 0, 6);
        g_unichar_to_utf8 (c, out);
        return out;
    }
}

gchar *
escape (const gchar *p)
{
  GString *s;

  s = g_string_new ("");

  while (*p)
    {
      if (*p == '\n')
        g_string_append (s, "\\n");
      else
        g_string_append_c (s, *p);
      p++;
    }

  return g_string_free (s, FALSE);
}
#endif

#ifdef SHOW_TEXT_ATTRIBUTES
static void
show_text_attributes (PangoLayout *l)
{
  const PangoLogAttr *attr;
  gint n_attrs;
  const gchar *s;
  gchar e;
  const gchar *p;
  gint i;
  const gchar *text;
  GSList *lines, *li;
  glong so, eo;

  printf ("\n");

  text = pango_layout_get_text (l);
  attr = pango_layout_get_log_attrs_readonly (l, &n_attrs);

  p = text;
  while (*p)
    {
      s = char_rep (g_utf8_get_char (p));
      printf (" %s", s);
      p = g_utf8_next_char (p);
    }
  printf ("\n");
  p = text;
  i = 0;
  do
    {
      if (*p)
        s = char_rep (g_utf8_get_char (p));
      else
        s = "";
      if (attr[i].is_word_start && attr[i].is_word_end)
        e = '|';
      else if (attr[i].is_word_start)
        e = '<';
      else if (attr[i].is_word_end)
        e = '>';
      else
        e = ' ';
      printf ("%c%*s", e, strlen (s), "");
      if (*p)
        p = g_utf8_next_char (p);
      i++;
    }
  while (*p || i < n_attrs);
  printf ("\n");

  p = text;
  i = 0;
  do
    {
      if (*p)
        s = char_rep (g_utf8_get_char (p));
      else
        s = "";
      if (attr[i].is_sentence_start && attr[i].is_sentence_end)
        e = '|';
      else if (attr[i].is_sentence_start)
        e = '<';
      else if (attr[i].is_sentence_end)
        e = '>';
      else
        e = ' ';
      printf ("%c%*s", e, strlen (s), "");
      if (*p)
        p = g_utf8_next_char (p);
      i++;
    }
  while (*p || i < n_attrs);
  printf ("\n");

  lines = pango_layout_get_lines_readonly (l);
  p = text;
  i = 0;
  do
    {
      gboolean start, end;

      if (*p)
        s = char_rep (g_utf8_get_char (p));
      else
        s = "";
      start = end = FALSE;
      for (li = lines; li; li = li->next)
        {
          PangoLayoutLine *line = li->data;
          so = g_utf8_pointer_to_offset (text, text + line->start_index);
          eo = g_utf8_pointer_to_offset (text, text + line->start_index + line->length);
          if (so == i)
            start = TRUE;
          if (eo == i)
            end = TRUE;
        }
      if (start && end)
        e = '|';
      else if (start)
        e = '<';
      else if (end)
        e = '>';
      else
        e = ' ';
      printf ("%c%*s", e, strlen (s), "");
      if (*p)
        p = g_utf8_next_char (p);
      i++;
    }
  while (*p || i < n_attrs);
  printf ("\n");
}
#endif

static void
test_words (GtkWidget *widget)
{
  AtkText *atk_text;
  const gchar *text = "abc! def\nghi jkl\nmno";
  Word expected[] = {
    { -1,  0, ATK_TEXT_BOUNDARY_CHAR,            0,  0, "" },
    { -1,  1, ATK_TEXT_BOUNDARY_CHAR,            0,  1, "a" },
    { -1,  2, ATK_TEXT_BOUNDARY_CHAR,            1,  2, "b" },
    { -1,  3, ATK_TEXT_BOUNDARY_CHAR,            2,  3, "c" },
    { -1,  4, ATK_TEXT_BOUNDARY_CHAR,            3,  4, "!" },
    { -1,  5, ATK_TEXT_BOUNDARY_CHAR,            4,  5, " " },
    { -1,  6, ATK_TEXT_BOUNDARY_CHAR,            5,  6, "d" },
    { -1,  7, ATK_TEXT_BOUNDARY_CHAR,            6,  7, "e" },
    { -1,  8, ATK_TEXT_BOUNDARY_CHAR,            7,  8, "f" },
    { -1,  9, ATK_TEXT_BOUNDARY_CHAR,            8,  9, "\n" },
    { -1, 10, ATK_TEXT_BOUNDARY_CHAR,            9, 10, "g" },
    { -1, 11, ATK_TEXT_BOUNDARY_CHAR,           10, 11, "h" },
    { -1, 12, ATK_TEXT_BOUNDARY_CHAR,           11, 12, "i" },
    { -1, 13, ATK_TEXT_BOUNDARY_CHAR,           12, 13, " " },
    { -1, 14, ATK_TEXT_BOUNDARY_CHAR,           13, 14, "j" },
    { -1, 15, ATK_TEXT_BOUNDARY_CHAR,           14, 15, "k" },
    { -1, 16, ATK_TEXT_BOUNDARY_CHAR,           15, 16, "l" },
    { -1, 17, ATK_TEXT_BOUNDARY_CHAR,           16, 17, "\n" },
    { -1, 18, ATK_TEXT_BOUNDARY_CHAR,           17, 18, "m" },
    { -1, 19, ATK_TEXT_BOUNDARY_CHAR,           18, 19, "n" },
    { -1, 20, ATK_TEXT_BOUNDARY_CHAR,           19, 20, "o" },
    { -1,  0, ATK_TEXT_BOUNDARY_WORD_START,      0,  0, "" },
    { -1,  1, ATK_TEXT_BOUNDARY_WORD_START,      0,  0, "" },
    { -1,  2, ATK_TEXT_BOUNDARY_WORD_START,      0,  0, "" },
    { -1,  3, ATK_TEXT_BOUNDARY_WORD_START,      0,  0, "" },
    { -1,  4, ATK_TEXT_BOUNDARY_WORD_START,      0,  0, "" },
    { -1,  5, ATK_TEXT_BOUNDARY_WORD_START,      0,  5, "abc! " },
    { -1,  6, ATK_TEXT_BOUNDARY_WORD_START,      0,  5, "abc! " },
    { -1,  7, ATK_TEXT_BOUNDARY_WORD_START,      0,  5, "abc! " },
    { -1,  8, ATK_TEXT_BOUNDARY_WORD_START,      0,  5, "abc! " },
    { -1,  9, ATK_TEXT_BOUNDARY_WORD_START,      5,  9, "def\n" },
    { -1, 10, ATK_TEXT_BOUNDARY_WORD_START,      5,  9, "def\n" },
    { -1, 11, ATK_TEXT_BOUNDARY_WORD_START,      5,  9, "def\n" },
    { -1, 12, ATK_TEXT_BOUNDARY_WORD_START,      5,  9, "def\n" },
    { -1, 13, ATK_TEXT_BOUNDARY_WORD_START,      9, 13, "ghi " },
    { -1, 14, ATK_TEXT_BOUNDARY_WORD_START,      9, 13, "ghi " },
    { -1, 15, ATK_TEXT_BOUNDARY_WORD_START,      9, 13, "ghi " },
    { -1, 16, ATK_TEXT_BOUNDARY_WORD_START,      9, 13, "ghi " },
    { -1, 17, ATK_TEXT_BOUNDARY_WORD_START,     13, 17, "jkl\n" },
    { -1, 18, ATK_TEXT_BOUNDARY_WORD_START,     13, 17, "jkl\n" },
    { -1, 19, ATK_TEXT_BOUNDARY_WORD_START,     13, 17, "jkl\n" },
    { -1, 20, ATK_TEXT_BOUNDARY_WORD_START,     13, 17, "jkl\n" },
    { -1,  0, ATK_TEXT_BOUNDARY_WORD_END,        0,  0, "" },
    { -1,  1, ATK_TEXT_BOUNDARY_WORD_END,        0,  0, "" },
    { -1,  2, ATK_TEXT_BOUNDARY_WORD_END,        0,  0, "" },
    { -1,  3, ATK_TEXT_BOUNDARY_WORD_END,        0,  3, "abc" },
    { -1,  4, ATK_TEXT_BOUNDARY_WORD_END,        0,  3, "abc" },
    { -1,  5, ATK_TEXT_BOUNDARY_WORD_END,        0,  3, "abc" },
    { -1,  6, ATK_TEXT_BOUNDARY_WORD_END,        0,  3, "abc" },
    { -1,  7, ATK_TEXT_BOUNDARY_WORD_END,        0,  3, "abc" },
    { -1,  8, ATK_TEXT_BOUNDARY_WORD_END,        3,  8, "! def" },
    { -1,  9, ATK_TEXT_BOUNDARY_WORD_END,        3,  8, "! def" },
    { -1, 10, ATK_TEXT_BOUNDARY_WORD_END,        3,  8, "! def" },
    { -1, 11, ATK_TEXT_BOUNDARY_WORD_END,        3,  8, "! def" },
    { -1, 12, ATK_TEXT_BOUNDARY_WORD_END,        8, 12, "\nghi" },
    { -1, 13, ATK_TEXT_BOUNDARY_WORD_END,        8, 12, "\nghi" },
    { -1, 14, ATK_TEXT_BOUNDARY_WORD_END,        8, 12, "\nghi" },
    { -1, 15, ATK_TEXT_BOUNDARY_WORD_END,        8, 12, "\nghi" },
    { -1, 16, ATK_TEXT_BOUNDARY_WORD_END,       12, 16, " jkl" },
    { -1, 17, ATK_TEXT_BOUNDARY_WORD_END,       12, 16, " jkl" },
    { -1, 18, ATK_TEXT_BOUNDARY_WORD_END,       12, 16, " jkl" },
    { -1, 19, ATK_TEXT_BOUNDARY_WORD_END,       12, 16, " jkl" },
    { -1, 20, ATK_TEXT_BOUNDARY_WORD_END,       16, 20, "\nmno" },
    { -1,  0, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  0, "" },
    { -1,  1, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  0, "" },
    { -1,  2, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  0, "" },
    { -1,  3, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  0, "" },
    { -1,  4, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  0, "" },
    { -1,  5, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  5, "abc! " },
    { -1,  6, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  5, "abc! " },
    { -1,  7, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  5, "abc! " },
    { -1,  8, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  5, "abc! " },
    { -1,  9, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    { -1, 10, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    { -1, 11, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    { -1, 12, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    { -1, 13, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    { -1, 14, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    { -1, 15, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    { -1, 16, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    { -1, 17, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    { -1, 18, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    { -1, 19, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    { -1, 20, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    { -1,  0, ATK_TEXT_BOUNDARY_SENTENCE_END,    0,  0, "" },
    { -1,  1, ATK_TEXT_BOUNDARY_SENTENCE_END,    0,  0, "" },
    { -1,  2, ATK_TEXT_BOUNDARY_SENTENCE_END,    0,  0, "" },
    { -1,  3, ATK_TEXT_BOUNDARY_SENTENCE_END,    0,  0, "" },
    { -1,  4, ATK_TEXT_BOUNDARY_SENTENCE_END,    0,  4, "abc!" },
    { -1,  5, ATK_TEXT_BOUNDARY_SENTENCE_END,    0,  4, "abc!" },
    { -1,  6, ATK_TEXT_BOUNDARY_SENTENCE_END,    0,  4, "abc!" },
    { -1,  7, ATK_TEXT_BOUNDARY_SENTENCE_END,    0,  4, "abc!" },
    { -1,  8, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    { -1,  9, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    { -1, 10, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    { -1, 11, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    { -1, 12, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    { -1, 13, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    { -1, 14, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    { -1, 15, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    { -1, 16, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    { -1, 17, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    { -1, 18, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    { -1, 19, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    { -1, 20, ATK_TEXT_BOUNDARY_SENTENCE_END,   16, 20, "\nmno" },
    { -1,  0, ATK_TEXT_BOUNDARY_LINE_START,      0,  0, "" },
    { -1,  1, ATK_TEXT_BOUNDARY_LINE_START,      0,  0, "" },
    { -1,  2, ATK_TEXT_BOUNDARY_LINE_START,      0,  0, "" },
    { -1,  3, ATK_TEXT_BOUNDARY_LINE_START,      0,  0, "" },
    { -1,  4, ATK_TEXT_BOUNDARY_LINE_START,      0,  0, "" },
    { -1,  5, ATK_TEXT_BOUNDARY_LINE_START,      0,  0, "" },
    { -1,  6, ATK_TEXT_BOUNDARY_LINE_START,      0,  0, "" },
    { -1,  7, ATK_TEXT_BOUNDARY_LINE_START,      0,  0, "" },
    { -1,  8, ATK_TEXT_BOUNDARY_LINE_START,      0,  0, "" },
    { -1,  9, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    { -1, 10, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    { -1, 11, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    { -1, 12, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    { -1, 13, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    { -1, 14, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    { -1, 15, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    { -1, 16, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    { -1, 17, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    { -1, 18, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    { -1, 19, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    { -1, 20, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    { -1,  0, ATK_TEXT_BOUNDARY_LINE_END,        0,  0, "" },
    { -1,  1, ATK_TEXT_BOUNDARY_LINE_END,        0,  0, "" },
    { -1,  2, ATK_TEXT_BOUNDARY_LINE_END,        0,  0, "" },
    { -1,  3, ATK_TEXT_BOUNDARY_LINE_END,        0,  0, "" },
    { -1,  4, ATK_TEXT_BOUNDARY_LINE_END,        0,  0, "" },
    { -1,  5, ATK_TEXT_BOUNDARY_LINE_END,        0,  0, "" },
    { -1,  6, ATK_TEXT_BOUNDARY_LINE_END,        0,  0, "" },
    { -1,  7, ATK_TEXT_BOUNDARY_LINE_END,        0,  0, "" },
    { -1,  8, ATK_TEXT_BOUNDARY_LINE_END,        0,  0, "" },
    { -1,  9, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    { -1, 10, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    { -1, 11, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    { -1, 12, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    { -1, 13, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    { -1, 14, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    { -1, 15, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    { -1, 16, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    { -1, 17, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    { -1, 18, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    { -1, 19, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    { -1, 20, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  0,  0, ATK_TEXT_BOUNDARY_CHAR,            0,  1, "a" },
    {  0,  1, ATK_TEXT_BOUNDARY_CHAR,            1,  2, "b" },
    {  0,  2, ATK_TEXT_BOUNDARY_CHAR,            2,  3, "c" },
    {  0,  3, ATK_TEXT_BOUNDARY_CHAR,            3,  4, "!" },
    {  0,  4, ATK_TEXT_BOUNDARY_CHAR,            4,  5, " " },
    {  0,  5, ATK_TEXT_BOUNDARY_CHAR,            5,  6, "d" },
    {  0,  6, ATK_TEXT_BOUNDARY_CHAR,            6,  7, "e" },
    {  0,  7, ATK_TEXT_BOUNDARY_CHAR,            7,  8, "f" },
    {  0,  8, ATK_TEXT_BOUNDARY_CHAR,            8,  9, "\n" },
    {  0,  9, ATK_TEXT_BOUNDARY_CHAR,            9, 10, "g" },
    {  0, 10, ATK_TEXT_BOUNDARY_CHAR,           10, 11, "h" },
    {  0, 11, ATK_TEXT_BOUNDARY_CHAR,           11, 12, "i" },
    {  0, 12, ATK_TEXT_BOUNDARY_CHAR,           12, 13, " " },
    {  0, 13, ATK_TEXT_BOUNDARY_CHAR,           13, 14, "j" },
    {  0, 14, ATK_TEXT_BOUNDARY_CHAR,           14, 15, "k" },
    {  0, 15, ATK_TEXT_BOUNDARY_CHAR,           15, 16, "l" },
    {  0, 16, ATK_TEXT_BOUNDARY_CHAR,           16, 17, "\n" },
    {  0, 17, ATK_TEXT_BOUNDARY_CHAR,           17, 18, "m" },
    {  0, 18, ATK_TEXT_BOUNDARY_CHAR,           18, 19, "n" },
    {  0, 19, ATK_TEXT_BOUNDARY_CHAR,           19, 20, "o" },
    {  0, 20, ATK_TEXT_BOUNDARY_CHAR,           20, 20, "" },
    {  0,  0, ATK_TEXT_BOUNDARY_WORD_START,      0,  5, "abc! " },
    {  0,  1, ATK_TEXT_BOUNDARY_WORD_START,      0,  5, "abc! " },
    {  0,  2, ATK_TEXT_BOUNDARY_WORD_START,      0,  5, "abc! " },
    {  0,  3, ATK_TEXT_BOUNDARY_WORD_START,      0,  5, "abc! " },
    {  0,  4, ATK_TEXT_BOUNDARY_WORD_START,      0,  5, "abc! " },
    {  0,  5, ATK_TEXT_BOUNDARY_WORD_START,      5,  9, "def\n" },
    {  0,  6, ATK_TEXT_BOUNDARY_WORD_START,      5,  9, "def\n" },
    {  0,  7, ATK_TEXT_BOUNDARY_WORD_START,      5,  9, "def\n" },
    {  0,  8, ATK_TEXT_BOUNDARY_WORD_START,      5,  9, "def\n" },
    {  0,  9, ATK_TEXT_BOUNDARY_WORD_START,      9, 13, "ghi " },
    {  0, 10, ATK_TEXT_BOUNDARY_WORD_START,      9, 13, "ghi " },
    {  0, 11, ATK_TEXT_BOUNDARY_WORD_START,      9, 13, "ghi " },
    {  0, 12, ATK_TEXT_BOUNDARY_WORD_START,      9, 13, "ghi " },
    {  0, 13, ATK_TEXT_BOUNDARY_WORD_START,     13, 17, "jkl\n" },
    {  0, 14, ATK_TEXT_BOUNDARY_WORD_START,     13, 17, "jkl\n" },
    {  0, 15, ATK_TEXT_BOUNDARY_WORD_START,     13, 17, "jkl\n" },
    {  0, 16, ATK_TEXT_BOUNDARY_WORD_START,     13, 17, "jkl\n" },
    {  0, 17, ATK_TEXT_BOUNDARY_WORD_START,     17, 20, "mno" },
    {  0, 18, ATK_TEXT_BOUNDARY_WORD_START,     17, 20, "mno" },
    {  0, 19, ATK_TEXT_BOUNDARY_WORD_START,     17, 20, "mno" },
    {  0, 20, ATK_TEXT_BOUNDARY_WORD_START,     17, 20, "mno" },
    {  0,  0, ATK_TEXT_BOUNDARY_WORD_END,        0,  3, "abc" },
    {  0,  1, ATK_TEXT_BOUNDARY_WORD_END,        0,  3, "abc" },
    {  0,  2, ATK_TEXT_BOUNDARY_WORD_END,        0,  3, "abc" },
    {  0,  3, ATK_TEXT_BOUNDARY_WORD_END,        3,  8, "! def" },
    {  0,  4, ATK_TEXT_BOUNDARY_WORD_END,        3,  8, "! def" },
    {  0,  5, ATK_TEXT_BOUNDARY_WORD_END,        3,  8, "! def" },
    {  0,  6, ATK_TEXT_BOUNDARY_WORD_END,        3,  8, "! def" },
    {  0,  7, ATK_TEXT_BOUNDARY_WORD_END,        3,  8, "! def" },
    {  0,  8, ATK_TEXT_BOUNDARY_WORD_END,        8, 12, "\nghi" },
    {  0,  9, ATK_TEXT_BOUNDARY_WORD_END,        8, 12, "\nghi" },
    {  0, 10, ATK_TEXT_BOUNDARY_WORD_END,        8, 12, "\nghi" },
    {  0, 11, ATK_TEXT_BOUNDARY_WORD_END,        8, 12, "\nghi" },
    {  0, 12, ATK_TEXT_BOUNDARY_WORD_END,       12, 16, " jkl" },
    {  0, 13, ATK_TEXT_BOUNDARY_WORD_END,       12, 16, " jkl" },
    {  0, 14, ATK_TEXT_BOUNDARY_WORD_END,       12, 16, " jkl" },
    {  0, 15, ATK_TEXT_BOUNDARY_WORD_END,       12, 16, " jkl" },
    {  0, 16, ATK_TEXT_BOUNDARY_WORD_END,       16, 20, "\nmno" },
    {  0, 17, ATK_TEXT_BOUNDARY_WORD_END,       16, 20, "\nmno" },
    {  0, 18, ATK_TEXT_BOUNDARY_WORD_END,       16, 20, "\nmno" },
    {  0, 19, ATK_TEXT_BOUNDARY_WORD_END,       16, 20, "\nmno" },
    {  0, 20, ATK_TEXT_BOUNDARY_WORD_END,       20, 20, "" },
    {  0,  0, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  5, "abc! " },
    {  0,  1, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  5, "abc! " },
    {  0,  2, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  5, "abc! " },
    {  0,  3, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  5, "abc! " },
    {  0,  4, ATK_TEXT_BOUNDARY_SENTENCE_START,  0,  5, "abc! " },
    {  0,  5, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    {  0,  6, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    {  0,  7, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    {  0,  8, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    {  0,  9, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    {  0, 10, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    {  0, 11, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    {  0, 12, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    {  0, 13, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    {  0, 14, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    {  0, 15, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    {  0, 16, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    {  0, 17, ATK_TEXT_BOUNDARY_SENTENCE_START, 17, 20, "mno" },
    {  0, 18, ATK_TEXT_BOUNDARY_SENTENCE_START, 17, 20, "mno" },
    {  0, 19, ATK_TEXT_BOUNDARY_SENTENCE_START, 17, 20, "mno" },
    {  0, 20, ATK_TEXT_BOUNDARY_SENTENCE_START, 17, 20, "mno" },
    {  0,  0, ATK_TEXT_BOUNDARY_SENTENCE_END,    0,  4, "abc!" },
    {  0,  1, ATK_TEXT_BOUNDARY_SENTENCE_END,    0,  4, "abc!" },
    {  0,  2, ATK_TEXT_BOUNDARY_SENTENCE_END,    0,  4, "abc!" },
    {  0,  3, ATK_TEXT_BOUNDARY_SENTENCE_END,    0,  4, "abc!" },
    {  0,  4, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    {  0,  5, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    {  0,  6, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    {  0,  7, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    {  0,  8, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    {  0,  9, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    {  0, 10, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    {  0, 11, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    {  0, 12, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    {  0, 13, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    {  0, 14, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    {  0, 15, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    {  0, 16, ATK_TEXT_BOUNDARY_SENTENCE_END,   16, 20, "\nmno" },
    {  0, 17, ATK_TEXT_BOUNDARY_SENTENCE_END,   16, 20, "\nmno" },
    {  0, 18, ATK_TEXT_BOUNDARY_SENTENCE_END,   16, 20, "\nmno" },
    {  0, 19, ATK_TEXT_BOUNDARY_SENTENCE_END,   16, 20, "\nmno" },
    {  0, 20, ATK_TEXT_BOUNDARY_SENTENCE_END,   20, 20, "" },
    {  0,  0, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    {  0,  1, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    {  0,  2, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    {  0,  3, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    {  0,  4, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    {  0,  5, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    {  0,  6, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    {  0,  7, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    {  0,  8, ATK_TEXT_BOUNDARY_LINE_START,      0,  9, "abc! def\n" },
    {  0,  9, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  0, 10, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  0, 11, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  0, 12, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  0, 13, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  0, 14, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  0, 15, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  0, 16, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  0, 17, ATK_TEXT_BOUNDARY_LINE_START,     17, 20, "mno" },
    {  0, 18, ATK_TEXT_BOUNDARY_LINE_START,     17, 20, "mno" },
    {  0, 19, ATK_TEXT_BOUNDARY_LINE_START,     17, 20, "mno" },
    {  0, 20, ATK_TEXT_BOUNDARY_LINE_START,     17, 20, "mno" },
    {  0,  0, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    {  0,  1, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    {  0,  2, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    {  0,  3, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    {  0,  4, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    {  0,  5, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    {  0,  6, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    {  0,  7, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    {  0,  8, ATK_TEXT_BOUNDARY_LINE_END,        0,  8, "abc! def" },
    {  0,  9, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  0, 10, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  0, 11, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  0, 12, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  0, 13, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  0, 14, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  0, 15, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  0, 16, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  0, 17, ATK_TEXT_BOUNDARY_LINE_END,       16, 20, "\nmno" },
    {  0, 18, ATK_TEXT_BOUNDARY_LINE_END,       16, 20, "\nmno" },
    {  0, 19, ATK_TEXT_BOUNDARY_LINE_END,       16, 20, "\nmno" },
    {  0, 20, ATK_TEXT_BOUNDARY_LINE_END,       16, 20, "\nmno" },
    {  1,  0, ATK_TEXT_BOUNDARY_CHAR,            1,  2, "b" },
    {  1,  1, ATK_TEXT_BOUNDARY_CHAR,            2,  3, "c" },
    {  1,  2, ATK_TEXT_BOUNDARY_CHAR,            3,  4, "!" },
    {  1,  3, ATK_TEXT_BOUNDARY_CHAR,            4,  5, " " },
    {  1,  4, ATK_TEXT_BOUNDARY_CHAR,            5,  6, "d" },
    {  1,  5, ATK_TEXT_BOUNDARY_CHAR,            6,  7, "e" },
    {  1,  6, ATK_TEXT_BOUNDARY_CHAR,            7,  8, "f" },
    {  1,  7, ATK_TEXT_BOUNDARY_CHAR,            8,  9, "\n" },
    {  1,  8, ATK_TEXT_BOUNDARY_CHAR,            9, 10, "g" },
    {  1,  9, ATK_TEXT_BOUNDARY_CHAR,           10, 11, "h" },
    {  1, 10, ATK_TEXT_BOUNDARY_CHAR,           11, 12, "i" },
    {  1, 11, ATK_TEXT_BOUNDARY_CHAR,           12, 13, " " },
    {  1, 12, ATK_TEXT_BOUNDARY_CHAR,           13, 14, "j" },
    {  1, 13, ATK_TEXT_BOUNDARY_CHAR,           14, 15, "k" },
    {  1, 14, ATK_TEXT_BOUNDARY_CHAR,           15, 16, "l" },
    {  1, 15, ATK_TEXT_BOUNDARY_CHAR,           16, 17, "\n" },
    {  1, 16, ATK_TEXT_BOUNDARY_CHAR,           17, 18, "m" },
    {  1, 17, ATK_TEXT_BOUNDARY_CHAR,           18, 19, "n" },
    {  1, 18, ATK_TEXT_BOUNDARY_CHAR,           19, 20, "o" },
    {  1, 19, ATK_TEXT_BOUNDARY_CHAR,           20, 20, "" },
    {  1, 20, ATK_TEXT_BOUNDARY_CHAR,           20, 20, "" },
    {  1,  0, ATK_TEXT_BOUNDARY_WORD_START,      5,  9, "def\n" },
    {  1,  1, ATK_TEXT_BOUNDARY_WORD_START,      5,  9, "def\n" },
    {  1,  2, ATK_TEXT_BOUNDARY_WORD_START,      5,  9, "def\n" },
    {  1,  3, ATK_TEXT_BOUNDARY_WORD_START,      5,  9, "def\n" },
    {  1,  4, ATK_TEXT_BOUNDARY_WORD_START,      5,  9, "def\n" },
    {  1,  5, ATK_TEXT_BOUNDARY_WORD_START,      9, 13, "ghi " },
    {  1,  6, ATK_TEXT_BOUNDARY_WORD_START,      9, 13, "ghi " },
    {  1,  7, ATK_TEXT_BOUNDARY_WORD_START,      9, 13, "ghi " },
    {  1,  8, ATK_TEXT_BOUNDARY_WORD_START,      9, 13, "ghi " },
    {  1,  9, ATK_TEXT_BOUNDARY_WORD_START,     13, 17, "jkl\n" },
    {  1, 10, ATK_TEXT_BOUNDARY_WORD_START,     13, 17, "jkl\n" },
    {  1, 11, ATK_TEXT_BOUNDARY_WORD_START,     13, 17, "jkl\n" },
    {  1, 12, ATK_TEXT_BOUNDARY_WORD_START,     13, 17, "jkl\n" },
    {  1, 13, ATK_TEXT_BOUNDARY_WORD_START,     17, 20, "mno" },
    {  1, 14, ATK_TEXT_BOUNDARY_WORD_START,     17, 20, "mno" },
    {  1, 15, ATK_TEXT_BOUNDARY_WORD_START,     17, 20, "mno" },
    {  1, 16, ATK_TEXT_BOUNDARY_WORD_START,     17, 20, "mno" },
    {  1, 17, ATK_TEXT_BOUNDARY_WORD_START,     20, 20, "" },
    {  1, 18, ATK_TEXT_BOUNDARY_WORD_START,     20, 20, "" },
    {  1, 19, ATK_TEXT_BOUNDARY_WORD_START,     20, 20, "" },
    {  1, 20, ATK_TEXT_BOUNDARY_WORD_START,     20, 20, "" },
    {  1,  0, ATK_TEXT_BOUNDARY_WORD_END,        3,  8, "! def" },
    {  1,  1, ATK_TEXT_BOUNDARY_WORD_END,        3,  8, "! def" },
    {  1,  2, ATK_TEXT_BOUNDARY_WORD_END,        3,  8, "! def" },
    {  1,  3, ATK_TEXT_BOUNDARY_WORD_END,        8, 12, "\nghi" },
    {  1,  4, ATK_TEXT_BOUNDARY_WORD_END,        8, 12, "\nghi" },
    {  1,  5, ATK_TEXT_BOUNDARY_WORD_END,        8, 12, "\nghi" },
    {  1,  6, ATK_TEXT_BOUNDARY_WORD_END,        8, 12, "\nghi" },
    {  1,  7, ATK_TEXT_BOUNDARY_WORD_END,        8, 12, "\nghi" },
    {  1,  8, ATK_TEXT_BOUNDARY_WORD_END,       12, 16, " jkl" },
    {  1,  9, ATK_TEXT_BOUNDARY_WORD_END,       12, 16, " jkl" },
    {  1, 10, ATK_TEXT_BOUNDARY_WORD_END,       12, 16, " jkl" },
    {  1, 11, ATK_TEXT_BOUNDARY_WORD_END,       12, 16, " jkl" },
    {  1, 12, ATK_TEXT_BOUNDARY_WORD_END,       16, 20, "\nmno" },
    {  1, 13, ATK_TEXT_BOUNDARY_WORD_END,       16, 20, "\nmno" },
    {  1, 14, ATK_TEXT_BOUNDARY_WORD_END,       16, 20, "\nmno" },
    {  1, 15, ATK_TEXT_BOUNDARY_WORD_END,       16, 20, "\nmno" },
    {  1, 16, ATK_TEXT_BOUNDARY_WORD_END,       20, 20, "" },
    {  1, 17, ATK_TEXT_BOUNDARY_WORD_END,       20, 20, "" },
    {  1, 18, ATK_TEXT_BOUNDARY_WORD_END,       20, 20, "" },
    {  1, 19, ATK_TEXT_BOUNDARY_WORD_END,       20, 20, "" },
    {  1, 20, ATK_TEXT_BOUNDARY_WORD_END,       20, 20, "" },
    {  1,  0, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    {  1,  1, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    {  1,  2, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    {  1,  3, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    {  1,  4, ATK_TEXT_BOUNDARY_SENTENCE_START,  5,  9, "def\n" },
    {  1,  5, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    {  1,  6, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    {  1,  7, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    {  1,  8, ATK_TEXT_BOUNDARY_SENTENCE_START,  9, 17, "ghi jkl\n" },
    {  1,  9, ATK_TEXT_BOUNDARY_SENTENCE_START, 17, 20, "mno" },
    {  1, 10, ATK_TEXT_BOUNDARY_SENTENCE_START, 17, 20, "mno" },
    {  1, 11, ATK_TEXT_BOUNDARY_SENTENCE_START, 17, 20, "mno" },
    {  1, 12, ATK_TEXT_BOUNDARY_SENTENCE_START, 17, 20, "mno" },
    {  1, 13, ATK_TEXT_BOUNDARY_SENTENCE_START, 17, 20, "mno" },
    {  1, 14, ATK_TEXT_BOUNDARY_SENTENCE_START, 17, 20, "mno" },
    {  1, 15, ATK_TEXT_BOUNDARY_SENTENCE_START, 17, 20, "mno" },
    {  1, 16, ATK_TEXT_BOUNDARY_SENTENCE_START, 17, 20, "mno" },
    {  1, 17, ATK_TEXT_BOUNDARY_SENTENCE_START, 20, 20, "" },
    {  1, 18, ATK_TEXT_BOUNDARY_SENTENCE_START, 20, 20, "" },
    {  1, 19, ATK_TEXT_BOUNDARY_SENTENCE_START, 20, 20, "" },
    {  1, 20, ATK_TEXT_BOUNDARY_SENTENCE_START, 20, 20, "" },
    {  1,  0, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    {  1,  1, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    {  1,  2, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    {  1,  3, ATK_TEXT_BOUNDARY_SENTENCE_END,    4,  8, " def" },
    {  1,  4, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    {  1,  5, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    {  1,  6, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    {  1,  7, ATK_TEXT_BOUNDARY_SENTENCE_END,    8, 16, "\nghi jkl" },
    {  1,  8, ATK_TEXT_BOUNDARY_SENTENCE_END,   16, 20, "\nmno" },
    {  1,  9, ATK_TEXT_BOUNDARY_SENTENCE_END,   16, 20, "\nmno" },
    {  1, 10, ATK_TEXT_BOUNDARY_SENTENCE_END,   16, 20, "\nmno" },
    {  1, 11, ATK_TEXT_BOUNDARY_SENTENCE_END,   16, 20, "\nmno" },
    {  1, 12, ATK_TEXT_BOUNDARY_SENTENCE_END,   16, 20, "\nmno" },
    {  1, 13, ATK_TEXT_BOUNDARY_SENTENCE_END,   16, 20, "\nmno" },
    {  1, 14, ATK_TEXT_BOUNDARY_SENTENCE_END,   16, 20, "\nmno" },
    {  1, 15, ATK_TEXT_BOUNDARY_SENTENCE_END,   16, 20, "\nmno" },
    {  1, 16, ATK_TEXT_BOUNDARY_SENTENCE_END,   20, 20, "" },
    {  1, 17, ATK_TEXT_BOUNDARY_SENTENCE_END,   20, 20, "" },
    {  1, 18, ATK_TEXT_BOUNDARY_SENTENCE_END,   20, 20, "" },
    {  1, 19, ATK_TEXT_BOUNDARY_SENTENCE_END,   20, 20, "" },
    {  1, 20, ATK_TEXT_BOUNDARY_SENTENCE_END,   20, 20, "" },
    {  1,  0, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  1,  1, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  1,  2, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  1,  3, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  1,  4, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  1,  5, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  1,  6, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  1,  7, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  1,  8, ATK_TEXT_BOUNDARY_LINE_START,      9, 17, "ghi jkl\n" },
    {  1,  9, ATK_TEXT_BOUNDARY_LINE_START,     17, 20, "mno" },
    {  1, 10, ATK_TEXT_BOUNDARY_LINE_START,     17, 20, "mno" },
    {  1, 11, ATK_TEXT_BOUNDARY_LINE_START,     17, 20, "mno" },
    {  1, 12, ATK_TEXT_BOUNDARY_LINE_START,     17, 20, "mno" },
    {  1, 13, ATK_TEXT_BOUNDARY_LINE_START,     17, 20, "mno" },
    {  1, 14, ATK_TEXT_BOUNDARY_LINE_START,     17, 20, "mno" },
    {  1, 15, ATK_TEXT_BOUNDARY_LINE_START,     17, 20, "mno" },
    {  1, 16, ATK_TEXT_BOUNDARY_LINE_START,     17, 20, "mno" },
    {  1, 17, ATK_TEXT_BOUNDARY_LINE_START,     20, 20, "" },
    {  1, 18, ATK_TEXT_BOUNDARY_LINE_START,     20, 20, "" },
    {  1, 19, ATK_TEXT_BOUNDARY_LINE_START,     20, 20, "" },
    {  1, 20, ATK_TEXT_BOUNDARY_LINE_START,     20, 20, "" },
    {  1,  0, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  1,  1, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  1,  2, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  1,  3, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  1,  4, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  1,  5, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  1,  6, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  1,  7, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  1,  8, ATK_TEXT_BOUNDARY_LINE_END,        8, 16, "\nghi jkl" },
    {  1,  9, ATK_TEXT_BOUNDARY_LINE_END,       16, 20, "\nmno" },
    {  1, 10, ATK_TEXT_BOUNDARY_LINE_END,       16, 20, "\nmno" },
    {  1, 11, ATK_TEXT_BOUNDARY_LINE_END,       16, 20, "\nmno" },
    {  1, 12, ATK_TEXT_BOUNDARY_LINE_END,       16, 20, "\nmno" },
    {  1, 13, ATK_TEXT_BOUNDARY_LINE_END,       16, 20, "\nmno" },
    {  1, 14, ATK_TEXT_BOUNDARY_LINE_END,       16, 20, "\nmno" },
    {  1, 15, ATK_TEXT_BOUNDARY_LINE_END,       16, 20, "\nmno" },
    {  1, 16, ATK_TEXT_BOUNDARY_LINE_END,       16, 20, "\nmno" },
    {  1, 17, ATK_TEXT_BOUNDARY_LINE_END,       20, 20, "" },
    {  1, 18, ATK_TEXT_BOUNDARY_LINE_END,       20, 20, "" },
    {  1, 19, ATK_TEXT_BOUNDARY_LINE_END,       20, 20, "" },
    {  1, 20, ATK_TEXT_BOUNDARY_LINE_END,       20, 20, "" },
    {  0, -1, }
  };
  gint start, end;
  gchar *word;
  gint i;

  atk_text = ATK_TEXT (gtk_widget_get_accessible (widget));

  set_text (widget, text);
#ifdef SHOW_TEXT_ATTRIBUTES
  if (GTK_IS_LABEL (widget))
    show_text_attributes (gtk_label_get_layout (GTK_LABEL (widget)));
  else if (GTK_IS_ENTRY (widget))
    show_text_attributes (gtk_entry_get_layout (GTK_ENTRY (widget)));
#endif

#ifdef DUMP_RESULTS
  for (i = -1; i <= 1; i++)
    {
      gint j, k;
      for (j = ATK_TEXT_BOUNDARY_CHAR; j <= ATK_TEXT_BOUNDARY_LINE_END; j++)
        for (k = 0; k <= strlen (text); k++)
          {
            switch (i)
              {
              case -1:
                word = atk_text_get_text_before_offset (atk_text, k, j, &start, &end);
                break;
              case 0:
                word = atk_text_get_text_at_offset (atk_text, k, j, &start, &end);
                break;
              case 1:
                word = atk_text_get_text_after_offset (atk_text, k, j, &start, &end);
                break;
              default:
                g_assert_not_reached ();
                break;
              }
            printf ("    { %2d, %2d, %s %2d, %2d, \"%s\" },\n", i, k, boundary(j), start, end, escape (word));
            g_free (word);
          }
    }
#endif

  for (i = 0; expected[i].offset != -1; i++)
    {
      if (GTK_IS_ENTRY (widget))
        {
          /* GtkEntry sets single-paragraph mode on its pango layout */
          if (expected[i].boundary == ATK_TEXT_BOUNDARY_LINE_START ||
              expected[i].boundary == ATK_TEXT_BOUNDARY_LINE_END)
            continue;
        }

      switch (expected[i].gravity)
        {
          case -1:
            word = atk_text_get_text_before_offset (atk_text,
                                                    expected[i].offset,
                                                    expected[i].boundary,
                                                    &start, &end);
            break;
          case 0:
            word = atk_text_get_text_at_offset (atk_text,
                                                expected[i].offset,
                                                expected[i].boundary,
                                                &start, &end);
            break;
          case 1:
            word = atk_text_get_text_after_offset (atk_text,
                                                   expected[i].offset,
                                                   expected[i].boundary,
                                                   &start, &end);
            break;
          default:
            g_assert_not_reached ();
            break;
        }

      g_assert_cmpstr (word, ==, expected[i].word);
      g_assert_cmpint (start, ==, expected[i].start);
      g_assert_cmpint (end, ==, expected[i].end);
      g_free (word);
    }
}

static void
select_region (GtkWidget *widget,
               gint       start,
               gint       end)
{
  if (GTK_IS_EDITABLE (widget))
    gtk_editable_select_region (GTK_EDITABLE (widget), start, end);
  else if (GTK_IS_LABEL (widget))
    gtk_label_select_region (GTK_LABEL (widget), start, end);
  else if (GTK_IS_TEXT_VIEW (widget))
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GtkTextIter start_iter, end_iter;

      gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, end);
      gtk_text_buffer_get_iter_at_offset (buffer, &end_iter, start);

      gtk_text_buffer_select_range (buffer, &start_iter, &end_iter);
    }
  else
    g_assert_not_reached ();
}

typedef struct {
  gint count;
  gint position;
  gint bound;
} SelectionData;

static void
caret_moved_cb (AtkText *text, gint position, SelectionData *data)
{
  data->count++;
  data->position = position;
}

static void
selection_changed_cb (AtkText *text, SelectionData *data)
{
  data->count++;

  atk_text_get_selection (text, 0, &data->bound, &data->position);
}

static void
test_selection (GtkWidget *widget)
{
  AtkText *atk_text;
  const gchar *text = "Bla bla bla";
  gint n;
  gchar *ret;
  gint start, end;
  SelectionData data1;
  SelectionData data2;

  if (GTK_IS_LABEL (widget))
    gtk_label_set_selectable (GTK_LABEL (widget), TRUE);

  atk_text = ATK_TEXT (gtk_widget_get_accessible (widget));

  data1.count = 0;
  data2.count = 0;
  g_signal_connect (atk_text, "text_caret_moved",
                    G_CALLBACK (caret_moved_cb), &data1);
  g_signal_connect (atk_text, "text_selection_changed",
                    G_CALLBACK (selection_changed_cb), &data2);

  set_text (widget, text);

  n = atk_text_get_n_selections (atk_text);
  g_assert_cmpint (n, ==, 0);

  if (data1.count == 1)
    /* insertion before cursor */
    g_assert_cmpint (data1.position, ==, 11);
  else
    /* insertion after cursor */
    g_assert_cmpint (data1.count, ==, 0);
  g_assert_cmpint (data2.count, ==, 0);

  select_region (widget, 4, 7);

  g_assert_cmpint (data1.count, >=, 1);
  g_assert_cmpint (data1.position, ==, 7);
  g_assert_cmpint (data2.count, >=, 1);
  g_assert_cmpint (data2.bound, ==, 4);
  g_assert_cmpint (data2.position, ==, 7);

  n = atk_text_get_n_selections (atk_text);
  g_assert_cmpint (n, ==, 1);

  ret = atk_text_get_selection (atk_text, 0, &start, &end);
  g_assert_cmpstr (ret, ==, "bla");
  g_assert_cmpint (start, ==, 4);
  g_assert_cmpint (end, ==, 7);
  g_free (ret);

  atk_text_remove_selection (atk_text, 0);
  n = atk_text_get_n_selections (atk_text);
  g_assert_cmpint (n, ==, 0);

  g_assert_cmpint (data1.count, >=, 1);
  g_assert_cmpint (data2.count, >=, 2);
  g_assert_cmpint (data2.position, ==, 7);
  g_assert_cmpint (data2.bound, ==, 7);
}

static void
setup_test (GtkWidget *widget)
{
  set_text (widget, "");
}

static void
add_text_test (const gchar      *prefix,
               GTestFixtureFunc  test_func,
               GtkWidget        *widget)
{
  gchar *path;

  path = g_strdup_printf ("%s/%s", prefix, G_OBJECT_TYPE_NAME (widget));
  g_test_add_vtable (path,
                     0,
                     g_object_ref (widget),
                     (GTestFixtureFunc) setup_test,
                     (GTestFixtureFunc) test_func,
                     (GTestFixtureFunc) g_object_unref);
  g_free (path);
}

static void
add_text_tests (GtkWidget *widget)
{
  g_object_ref_sink (widget);
  add_text_test ("/text/basic", (GTestFixtureFunc) test_basic, widget);
  add_text_test ("/text/words", (GTestFixtureFunc) test_words, widget);
  add_text_test ("/text/changed", (GTestFixtureFunc) test_text_changed, widget);
  add_text_test ("/text/selection", (GTestFixtureFunc) test_selection, widget);
  g_object_unref (widget);
}

static void
test_bold_label (void)
{
  GtkWidget *label;
  AtkObject *atk_obj;
  gchar *text;

  g_test_bug ("126797");

  label = gtk_label_new ("<b>Bold?</b>");
  g_object_ref_sink (label);

  atk_obj = gtk_widget_get_accessible (label);

  text = atk_text_get_text (ATK_TEXT (atk_obj), 0, -1);
  g_assert_cmpstr (text, ==, "<b>Bold?</b>");
  g_free (text);

  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  text = atk_text_get_text (ATK_TEXT (atk_obj), 0, -1);
  g_assert_cmpstr (text, ==, "Bold?");
  g_free (text);

  g_object_unref (label);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/text/bold/GtkLabel", test_bold_label);

  add_text_tests (gtk_label_new (""));
  add_text_tests (gtk_entry_new ());
  add_text_tests (gtk_text_view_new ());

  return g_test_run ();
}
