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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <string.h>

static void
test_scrolled_window_children (void)
{
  GtkWidget *sw;
  AtkObject *accessible;

  sw = gtk_scrolled_window_new (NULL, NULL);
  g_object_ref_sink (sw);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), gtk_label_new ("Bla"));

  accessible = gtk_widget_get_accessible (sw);
  g_assert_cmpint (atk_object_get_n_accessible_children (accessible), ==, 3);

  g_object_unref (sw);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/scrolledwindow/children", test_scrolled_window_children);
  return g_test_run ();
}

