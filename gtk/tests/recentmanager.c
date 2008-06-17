/* GTK - The GIMP Toolkit
 * gtkrecentmanager.c: a manager for the recently used resources
 *
 * Copyright (C) 2006 Emmanuele Bassi
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>

const gchar *uri = "file:///tmp/testrecentchooser.txt";

static void
recent_manager_get_default (void)
{
  GtkRecentManager *manager;
  GtkRecentManager *manager2;

  manager = gtk_recent_manager_get_default ();
  g_assert (manager != NULL);

  manager2 = gtk_recent_manager_get_default ();
  g_assert (manager == manager2);
}

static void
recent_manager_add (void)
{
  GtkRecentManager *manager;
  GtkRecentData *recent_data;
  gboolean res;

  manager = gtk_recent_manager_get_default ();

  recent_data = g_slice_new0 (GtkRecentData);

  /* mime type is mandatory */
  recent_data->mime_type = NULL;
  recent_data->app_name = "testrecentchooser";
  recent_data->app_exec = "testrecentchooser %u";
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      res = gtk_recent_manager_add_full (manager,
                                         uri,
                                         recent_data);
    }
  g_test_trap_assert_failed ();

  /* app name is mandatory */
  recent_data->mime_type = "text/plain";
  recent_data->app_name = NULL;
  recent_data->app_exec = "testrecentchooser %u";
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      res = gtk_recent_manager_add_full (manager,
                                         uri,
                                         recent_data);
    }
  g_test_trap_assert_failed ();

  /* app exec is mandatory */
  recent_data->mime_type = "text/plain";
  recent_data->app_name = "testrecentchooser";
  recent_data->app_exec = NULL;
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      res = gtk_recent_manager_add_full (manager,
                                         uri,
                                         recent_data);
    }
  g_test_trap_assert_failed ();

  recent_data->mime_type = "text/plain";
  recent_data->app_name = "testrecentchooser";
  recent_data->app_exec = "testrecentchooser %u";
  res = gtk_recent_manager_add_full (manager,
                                     uri,
                                     recent_data);
  g_assert (res == TRUE);

  g_slice_free (GtkRecentData, recent_data);
}

static void
recent_manager_has_item (void)
{
  GtkRecentManager *manager;
  gboolean res;

  manager = gtk_recent_manager_get_default ();

  res = gtk_recent_manager_has_item (manager, "file:///tmp/testrecentdoesnotexist.txt");
  g_assert (res == FALSE);

  res = gtk_recent_manager_has_item (manager, uri);
  g_assert (res == TRUE);
}

static void
recent_manager_remove_item (void)
{
  GtkRecentManager *manager;
  gboolean res;
  GError *error;

  manager = gtk_recent_manager_get_default ();

  error = NULL;
  res = gtk_recent_manager_remove_item (manager,
                                        "file:///tmp/testrecentdoesnotexist.txt",
                                        &error);
  g_assert (res == FALSE);
  g_assert (error != NULL);
  g_assert (error->domain == GTK_RECENT_MANAGER_ERROR);
  g_assert (error->code == GTK_RECENT_MANAGER_ERROR_NOT_FOUND);
  g_error_free (error);

  /* remove an item that's actually there */
  error = NULL;
  res = gtk_recent_manager_remove_item (manager, uri, &error);
  g_assert (res == TRUE);
  g_assert (error == NULL);

  res = gtk_recent_manager_has_item (manager, uri);
  g_assert (res == FALSE);
}

int
main (int    argc,
      char **argv)
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/recent-manager/get-default",
                   recent_manager_get_default);
  g_test_add_func ("/recent-manager/add",
                   recent_manager_add);
  g_test_add_func ("/recent-manager/has-item",
                   recent_manager_has_item);
  g_test_add_func ("/recent-manager/remove-item",
                   recent_manager_remove_item);

  return g_test_run ();
}
