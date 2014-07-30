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
test_popover_parent (void)
{
  GtkWidget *w;
  GtkWidget *p;
  AtkObject *a;

  g_test_bug ("733923");

  w = gtk_entry_new ();

  p = gtk_popover_new (NULL);
  a = gtk_widget_get_accessible (p);

  g_assert (a != NULL);
  g_assert (atk_object_get_parent (a) == NULL);

  gtk_popover_set_relative_to (GTK_POPOVER (p), w);

  g_assert (atk_object_get_parent (a) != NULL);

  gtk_widget_destroy (w);
  gtk_widget_destroy (p);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/popover/accessible-parent", test_popover_parent);

  return g_test_run ();
}

