/* simple.c
 * Copyright (C) 2017  Red Hat, Inc
 * Author: Benjamin Otte
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
#include "config.h"
#include <gtk/gtk.h>


const char text[] = 
"This library is free software; you can redistribute it and/or\n"
"modify it under the terms of the GNU Library General Public\n"
"License as published by the Free Software Foundation; either\n"
"version 2 of the License, or (at your option) any later version.\n"
"\n"
"This library is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
"Library General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU Library General Public\n"
"License along with this library. If not, see <http://www.gnu.org/licenses/>.\n";

static GtkWidget *tv;
static GtkTextBuffer *buffer;
static int len;

static GtkTextMark **marks;
static guint marks_timeout;

static gboolean toggle_mark (gpointer data)
{
  int pos;
  GtkTextMark *mark;

  pos = g_random_int_range (0, len);
  mark = marks[pos];

  gtk_text_mark_set_visible (mark, !gtk_text_mark_get_visible (mark));

  return G_SOURCE_CONTINUE;
}

static void
toggle_marks (GtkToggleButton *button)
{
  int i;
  gboolean enable;

  enable = gtk_toggle_button_get_active (button);

  if (!marks)
    {
      marks = g_new (GtkTextMark*, len);

      for (i = 0; i < len; i++)
        {
          marks[i] = gtk_text_mark_new (NULL, TRUE);
          gtk_text_mark_set_visible (marks[i], i % 2);
        }
     }

  if (enable)
    {
      for (i = 0; i < len; i++)
        {
          GtkTextIter iter;

          gtk_text_buffer_get_iter_at_offset (buffer, &iter, i);
          gtk_text_buffer_add_mark (buffer, marks[i], &iter);
        }

      marks_timeout = g_timeout_add (16, toggle_mark, NULL);
    }
  else
    {
      for (i = 0; i < len; i++)
        gtk_text_buffer_delete_mark (buffer, marks[i]);

      if (marks_timeout)
        g_source_remove (marks_timeout);
      marks_timeout = 0;
    }
}

static gboolean
move_insert (gpointer data)
{
  GtkTextMark *mark;
  GtkTextIter iter, start, end;

  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
  gtk_text_buffer_get_bounds (buffer, &start, &end);

  if (gtk_text_iter_equal (&iter, &end))
    gtk_text_iter_assign (&iter, &start);
  else
    gtk_text_iter_forward_cursor_position (&iter);

  gtk_text_buffer_place_cursor (buffer, &iter);

  return G_SOURCE_CONTINUE;
}

static guint cursor_timeout;

static void
toggle_cursor (GtkToggleButton *button)
{
  gboolean enable;

  enable = gtk_toggle_button_get_active (button);
  if (enable)
    cursor_timeout = g_timeout_add (16, move_insert, NULL);
  else
    {
      if (cursor_timeout)
        g_source_remove (cursor_timeout);
      cursor_timeout = 0;
    }
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *sw, *box, *box2, *button;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_widget_set_vexpand (sw, TRUE);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_container_add (GTK_CONTAINER (box), sw);

  tv = gtk_text_view_new ();
  gtk_container_add (GTK_CONTAINER (sw), tv);

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (tv), buffer);

  gtk_text_buffer_set_text (buffer, text, -1);

  len = strlen (text);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (box), box2);

  button = gtk_toggle_button_new_with_label ("Random marks");
  g_signal_connect (button, "notify::active", G_CALLBACK (toggle_marks), NULL);
  gtk_container_add (GTK_CONTAINER (box2), button);

  button = gtk_toggle_button_new_with_label ("Wandering cursor");
  g_signal_connect (button, "notify::active", G_CALLBACK (toggle_cursor), NULL);
  gtk_container_add (GTK_CONTAINER (box2), button);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
