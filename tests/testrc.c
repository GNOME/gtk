/* GTK - The GIMP Toolkit

   Copyright (C) 2006 Red Hat, Inc.
   Author: Matthias Clasen <mclasen@redhat.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include "gtk/gtk.h"

/* NOTE to compile this test, GTK+ needs to be build without
 * the _-prefix stripping.
 */
struct {
  gchar    *pattern;
  gchar    *test;
  gboolean  match;
} tests[] = {
  { "", "", TRUE },
  { "<GtkCheckButton>", "GtkToggleButton", FALSE },
  { "<GtkCheckButton>", "GtkCheckButton", TRUE },
  { "<GtkCheckButton>", "GtkRadioButton", TRUE },
  { "abc*.<GtkButton>.<GtkLabel>.*foo", "abcx.GtkToggleButton.GtkLabel.foo", TRUE },
  { "*abc.<GtkButton>.foo*", "abc.GtkToggleButton.bar", FALSE },
  { "*abc.<GtkButton>.foo*", "xabc.GtkToggleButton.fox", FALSE },
  { NULL, NULL, FALSE }
};

static void
load_types (void)
{
  volatile GType type;

  type = gtk_radio_button_get_type ();
  type = gtk_label_get_type ();
}

int
main (int argc, char *argv[])
{
  gint i;

  gtk_init (&argc, &argv);
  load_types ();

  for (i = 0; tests[i].test; i++)
    {
      GSList *list;
      gchar *path, *rpath;
      gboolean result;
      
      list = _gtk_rc_parse_widget_class_path (tests[i].pattern);
      path = g_strdup (tests[i].test);
      rpath = g_utf8_strreverse (path, -1);
      result = _gtk_rc_match_widget_class (list, strlen (path), path, rpath);
      g_print ("%d. \"%s\" \"%s\", expected %d, got %d\n",
	       i, tests[i].pattern, tests[i].test, tests[i].match, result);
      g_assert (result == tests[i].match);
      g_free (path);
    }

  return 0;
}
