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

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/clipboard/test_text", test_text);

  return g_test_run();
}
