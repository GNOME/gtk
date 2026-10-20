/* GtkUIManager lifetime tests
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

#define GDK_DISABLE_DEPRECATION_WARNINGS
#include <gtk/gtk.h>

typedef struct
{
  GtkUIManager *manager;
  GtkUIManager *weak_manager;
  gboolean release_on_connect;
  guint proxies;
} UpdateData;

static void
connect_proxy (GtkUIManager *manager,
               GtkAction    *action,
               GtkWidget    *proxy,
               UpdateData   *data)
{
  data->proxies++;

  if (data->release_on_connect)
    g_clear_object (&data->manager);

  g_assert_nonnull (data->weak_manager);
}

static void
prepare_update (UpdateData *data,
                gboolean    release_on_connect)
{
  static const GtkActionEntry entries[] = {
    { "first", NULL, "First", NULL, NULL, NULL },
    { "second", NULL, "Second", NULL, NULL, NULL },
    { "third", NULL, "Third", NULL, NULL, NULL }
  };
  static const gchar ui[] =
    "<ui><popup name='popup'>"
      "<menuitem action='first'/>"
      "<menuitem action='second'/>"
      "<menuitem action='third'/>"
    "</popup></ui>";
  GtkActionGroup *group;
  GError *error = NULL;
  guint merge_id;

  data->manager = gtk_ui_manager_new ();
  data->weak_manager = data->manager;
  data->release_on_connect = release_on_connect;
  g_object_add_weak_pointer (G_OBJECT (data->manager),
                             (gpointer *) &data->weak_manager);

  group = gtk_action_group_new ("actions");
  gtk_action_group_add_actions (group, entries, G_N_ELEMENTS (entries), NULL);
  gtk_ui_manager_insert_action_group (data->manager, group, 0);
  g_object_unref (group);

  g_signal_connect (data->manager, "connect-proxy",
                    G_CALLBACK (connect_proxy), data);
  merge_id = gtk_ui_manager_add_ui_from_string (data->manager, ui, -1, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (merge_id, !=, 0);
}

static void
drain_events (void)
{
  while (g_main_context_iteration (NULL, FALSE))
    ;
}

static void
test_idle_update_lifetime (void)
{
  UpdateData data = { 0 };

  prepare_update (&data, TRUE);
  drain_events ();

  g_assert_null (data.manager);
  g_assert_null (data.weak_manager);
  g_assert_cmpuint (data.proxies, ==, 3);
}

static void
test_synchronous_update_lifetime (void)
{
  UpdateData data = { 0 };

  prepare_update (&data, TRUE);
  gtk_ui_manager_ensure_update (data.manager);

  g_assert_null (data.manager);
  g_assert_null (data.weak_manager);
  g_assert_cmpuint (data.proxies, ==, 3);

  drain_events ();
}

static void
test_cancel_update (void)
{
  UpdateData data = { 0 };

  prepare_update (&data, FALSE);
  g_clear_object (&data.manager);
  g_assert_null (data.weak_manager);

  drain_events ();
  g_assert_cmpuint (data.proxies, ==, 0);
}

static void
test_normal_update (void)
{
  UpdateData data = { 0 };

  prepare_update (&data, FALSE);
  drain_events ();

  g_assert_nonnull (data.manager);
  g_assert_true (data.weak_manager == data.manager);
  g_assert_cmpuint (data.proxies, ==, 3);

  g_clear_object (&data.manager);
  g_assert_null (data.weak_manager);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/uimanager/lifetime/idle", test_idle_update_lifetime);
  g_test_add_func ("/uimanager/lifetime/synchronous", test_synchronous_update_lifetime);
  g_test_add_func ("/uimanager/lifetime/cancel", test_cancel_update);
  g_test_add_func ("/uimanager/lifetime/normal", test_normal_update);

  return g_test_run ();
}
