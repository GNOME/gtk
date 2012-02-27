/*
 * Copyright (C) 2011 Canonical Inc.
 *
 * Author:
 *      Michael Terry <michael.terry@canonical.com>
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
gtk_css_provider_load_data_not_null_terminated (void)
{
  GtkCssProvider *p;
  const gchar data[3] = {'*', '{', '}'};

  p = gtk_css_provider_new();

  gtk_css_provider_load_from_data(p, data, sizeof (data), NULL);

  g_object_unref (p);
}


int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gtk_css_provider_load_data/not_null_terminated",
      gtk_css_provider_load_data_not_null_terminated);

  return g_test_run ();
}

