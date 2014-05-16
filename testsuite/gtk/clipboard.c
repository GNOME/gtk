/* GTK - The GIMP Toolkit
 * Copyright (C) 2013 Benjamin Otte <otte@gnome.org>
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

#include <string.h>

#define SOME_TEXT "Hello World"
#define TARGET_TEXT "UTF8_STRING"

static void
test_text (void)
{
  GtkClipboard *clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (), GDK_SELECTION_CLIPBOARD);
  char *text;

  gtk_clipboard_set_text (clipboard, SOME_TEXT, -1);
  text = gtk_clipboard_wait_for_text (clipboard);
  g_assert_cmpstr (text, ==, SOME_TEXT);
  g_free (text);

  gtk_clipboard_set_text (clipboard, SOME_TEXT SOME_TEXT, strlen (SOME_TEXT));
  text = gtk_clipboard_wait_for_text (clipboard);
  g_assert_cmpstr (text, ==, SOME_TEXT);
  g_free (text);
}

static void
test_with_data_get (GtkClipboard *clipboard,
                    GtkSelectionData *selection_data,
                    guint info,
                    gpointer user_data_or_owner)
{
    gboolean success;

    g_assert_cmpuint (info, ==, 42);

    success = gtk_selection_data_set_text (selection_data, SOME_TEXT, -1);
    g_assert (success);
}

static void
test_with_data_got (GtkClipboard *clipboard,
                    GtkSelectionData *selection_data,
                    gpointer data)
{
    guchar *text;

    text = gtk_selection_data_get_text (selection_data);
    g_assert_cmpstr ((char*)text, ==, SOME_TEXT);
    g_free (text);
}

static void
test_with_data (void)
{
    GtkClipboard *clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (), GDK_SELECTION_CLIPBOARD);
    GtkTargetEntry entries[] = { { .target = TARGET_TEXT, .info = 42 } };

    gtk_clipboard_set_with_data (clipboard, entries, G_N_ELEMENTS(entries), test_with_data_get, NULL, NULL);
    gtk_clipboard_request_contents (clipboard, gdk_atom_intern (TARGET_TEXT, FALSE), test_with_data_got, NULL);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/clipboard/test_text", test_text);
  g_test_add_func ("/clipboard/test_with_data", test_with_data);

  return g_test_run();
}
