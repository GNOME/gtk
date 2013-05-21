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

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */
#include <gtk/gtk.h>

static void
test_click_expander (void)
{
  GtkWidget *window = gtk_test_create_simple_window ("Test Window", "Test click on expander");
  GtkWidget *expander = gtk_expander_new ("Test Expander");
  GtkWidget *label = gtk_label_new ("Test Label");
  gboolean expanded;
  gboolean simsuccess;
  gtk_container_add (GTK_CONTAINER (expander), label);
  gtk_container_add (GTK_CONTAINER (gtk_bin_get_child (GTK_BIN (window))), expander);
  gtk_widget_show (expander);
  gtk_widget_show (label);
  gtk_widget_show_now (window);
  /* check initial expander state */
  expanded = gtk_expander_get_expanded (GTK_EXPANDER (expander));
  g_assert (!expanded);
  /* check expanding */
  simsuccess = gtk_test_widget_click (expander, 1, 0);
  g_assert (simsuccess == TRUE);

  gtk_test_widget_wait_for_draw (expander);

  expanded = gtk_expander_get_expanded (GTK_EXPANDER (expander));
  g_assert (expanded);
  /* check collapsing */
  simsuccess = gtk_test_widget_click (expander, 1, 0);
  g_assert (simsuccess == TRUE);

  gtk_test_widget_wait_for_draw (expander);

  expanded = gtk_expander_get_expanded (GTK_EXPANDER (expander));
  g_assert (!expanded);
}

static void
test_click_content_widget (void)
{
  GtkWidget *window = gtk_test_create_simple_window ("Test Window", "Test click on content widget");
  GtkWidget *expander = gtk_expander_new ("Test Expander");
  GtkWidget *entry = gtk_entry_new ();
  gboolean expanded;
  gboolean simsuccess;
  gtk_container_add (GTK_CONTAINER (expander), entry);
  gtk_container_add (GTK_CONTAINER (gtk_bin_get_child (GTK_BIN (window))), expander);
  gtk_expander_set_expanded (GTK_EXPANDER (expander), TRUE);
  gtk_widget_show (expander);
  gtk_widget_show (entry);
  gtk_widget_show_now (window);

  /* check click on content with expander open */
  expanded = gtk_expander_get_expanded (GTK_EXPANDER (expander));
  g_assert (expanded);
  simsuccess = gtk_test_widget_click (entry, 1, 0);
  g_assert (simsuccess == TRUE);

  gtk_test_widget_wait_for_draw (expander);

  expanded = gtk_expander_get_expanded (GTK_EXPANDER (expander));
  g_assert (expanded);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);
  g_test_add_func ("/expander/click-expander", test_click_expander);
  g_test_add_func ("/expander/click-content-widget", test_click_content_widget);
  return g_test_run();
}
