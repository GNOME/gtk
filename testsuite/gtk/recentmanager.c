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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gstdio.h>
#include <gtk/gtk.h>

const gchar *uri = "file:///tmp/testrecentchooser.txt";
const gchar *uri2 = "file:///tmp/testrecentchooser2.txt";

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

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

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

  G_GNUC_END_IGNORE_DEPRECATIONS;

  recent_data->mime_type = "text/plain";
  recent_data->app_name = "testrecentchooser";
  recent_data->app_exec = "testrecentchooser %u";
  res = gtk_recent_manager_add_full (manager,
                                     uri,
                                     recent_data);
  g_assert (res == TRUE);

  g_slice_free (GtkRecentData, recent_data);
}

typedef struct {
  GMainLoop *main_loop;
  gint counter;
} AddManyClosure;

static void
check_bulk (GtkRecentManager *manager,
            gpointer          data)
{
  AddManyClosure *closure = data;

  if (g_test_verbose ())
    g_print (G_STRLOC ": counter = %d\n", closure->counter);

  g_assert_cmpint (closure->counter, ==, 100);

  if (g_main_loop_is_running (closure->main_loop))
    g_main_loop_quit (closure->main_loop);
}

static void
recent_manager_add_many (void)
{
  GtkRecentManager *manager = g_object_new (GTK_TYPE_RECENT_MANAGER,
                                            "filename", "recently-used.xbel",
                                            NULL);
  AddManyClosure *closure = g_new (AddManyClosure, 1);
  GtkRecentData *data = g_slice_new0 (GtkRecentData);
  gint i;

  closure->main_loop = g_main_loop_new (NULL, FALSE);
  closure->counter = 0;

  g_signal_connect (manager, "changed", G_CALLBACK (check_bulk), closure);

  for (i = 0; i < 100; i++)
    {
      gchar *new_uri;

      data->mime_type = "text/plain";
      data->app_name = "testrecentchooser";
      data->app_exec = "testrecentchooser %u";

      if (g_test_verbose ())
        g_print (G_STRLOC ": adding item %d\n", i);

      new_uri = g_strdup_printf ("file:///doesnotexist-%d.txt", i);
      gtk_recent_manager_add_full (manager, new_uri, data);
      g_free (new_uri);

      closure->counter += 1;
    }

  g_main_loop_run (closure->main_loop);

  g_main_loop_unref (closure->main_loop);
  g_slice_free (GtkRecentData, data);
  g_free (closure);
  g_object_unref (manager);

  g_assert_cmpint (g_unlink ("recently-used.xbel"), ==, 0);
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
recent_manager_move_item (void)
{
  GtkRecentManager *manager;
  gboolean res;
  GError *error;

  manager = gtk_recent_manager_get_default ();

  error = NULL;
  res = gtk_recent_manager_move_item (manager,
                                      "file:///tmp/testrecentdoesnotexist.txt",
                                      uri2,
                                      &error);
  g_assert (res == FALSE);
  g_assert (error != NULL);
  g_assert (error->domain == GTK_RECENT_MANAGER_ERROR);
  g_assert (error->code == GTK_RECENT_MANAGER_ERROR_NOT_FOUND);
  g_error_free (error);

  error = NULL;
  res = gtk_recent_manager_move_item (manager, uri, uri2, &error);
  g_assert (res == TRUE);
  g_assert (error == NULL);

  res = gtk_recent_manager_has_item (manager, uri);
  g_assert (res == FALSE);

  res = gtk_recent_manager_has_item (manager, uri2);
  g_assert (res == TRUE);
}

static void
recent_manager_lookup_item (void)
{
  GtkRecentManager *manager;
  GtkRecentInfo *info;
  GError *error;

  manager = gtk_recent_manager_get_default ();

  error = NULL;
  info = gtk_recent_manager_lookup_item (manager,
                                         "file:///tmp/testrecentdoesnotexist.txt",
                                         &error);
  g_assert (info == NULL);
  g_assert (error != NULL);
  g_assert (error->domain == GTK_RECENT_MANAGER_ERROR);
  g_assert (error->code == GTK_RECENT_MANAGER_ERROR_NOT_FOUND);
  g_error_free (error);

  error = NULL;
  info = gtk_recent_manager_lookup_item (manager, uri2, &error);
  g_assert (info != NULL);
  g_assert (error == NULL);

  g_assert (gtk_recent_info_has_application (info, "testrecentchooser"));

  gtk_recent_info_unref (info);
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
  res = gtk_recent_manager_remove_item (manager, uri2, &error);
  g_assert (res == TRUE);
  g_assert (error == NULL);

  res = gtk_recent_manager_has_item (manager, uri2);
  g_assert (res == FALSE);
}

static void
recent_manager_purge (void)
{
  GtkRecentManager *manager;
  GtkRecentData *recent_data;
  gint n;
  GError *error;

  manager = gtk_recent_manager_get_default ();

  /* purge, add 1, purge again and check that 1 item has been purged */
  error = NULL;
  n = gtk_recent_manager_purge_items (manager, &error);
  g_assert (error == NULL);

  recent_data = g_slice_new0 (GtkRecentData);
  recent_data->mime_type = "text/plain";
  recent_data->app_name = "testrecentchooser";
  recent_data->app_exec = "testrecentchooser %u";
  gtk_recent_manager_add_full (manager, uri, recent_data);
  g_slice_free (GtkRecentData, recent_data);

  error = NULL;
  n = gtk_recent_manager_purge_items (manager, &error);
  g_assert (error == NULL);
  g_assert (n == 1);
}

int
main (int    argc,
      char **argv)
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/recent-manager/get-default", recent_manager_get_default);
  g_test_add_func ("/recent-manager/add", recent_manager_add);
  g_test_add_func ("/recent-manager/add-many", recent_manager_add_many);
  g_test_add_func ("/recent-manager/has-item", recent_manager_has_item);
  g_test_add_func ("/recent-manager/move-item", recent_manager_move_item);
  g_test_add_func ("/recent-manager/lookup-item", recent_manager_lookup_item);
  g_test_add_func ("/recent-manager/remove-item", recent_manager_remove_item);
  g_test_add_func ("/recent-manager/purge", recent_manager_purge);

  return g_test_run ();
}
