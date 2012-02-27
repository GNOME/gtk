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

static void
test_toolkit_name (void)
{
  const gchar *s;

  s = atk_get_toolkit_name ();
  g_assert_cmpstr (s, ==, "gtk");
}

static void
test_toolkit_version (void)
{
  const gchar *s;

  s = atk_get_toolkit_version ();
  g_assert_cmpstr (s, ==, GTK_VERSION);
}

static void
test_root (void)
{
  AtkObject *obj;

  obj = atk_get_root ();

  g_assert (atk_object_get_role (obj) == ATK_ROLE_APPLICATION);
  g_assert (atk_object_get_parent (obj) == NULL);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/util/toolkit-name", test_toolkit_name);
  g_test_add_func ("/util/toolkit-version", test_toolkit_version);
  g_test_add_func ("/util/root", test_root);

  return g_test_run ();
}
