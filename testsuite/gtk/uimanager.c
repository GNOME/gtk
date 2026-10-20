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
  gboolean reenter_on_connect;
  guint release_at;
  guint proxies;
} UpdateData;

static void
connect_proxy (GtkUIManager *manager,
               GtkAction    *action,
               GtkWidget    *proxy,
               UpdateData   *data)
{
  data->proxies++;

  if (data->release_on_connect &&
      (data->release_at == 0 || data->proxies == data->release_at))
    g_clear_object (&data->manager);

  if (data->reenter_on_connect && data->proxies == 1)
    gtk_ui_manager_ensure_update (manager);

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
test_nested_update (gconstpointer release_owner)
{
  UpdateData data = { 0 };

  prepare_update (&data, GPOINTER_TO_INT (release_owner));
  data.reenter_on_connect = TRUE;
  drain_events ();

  g_assert_cmpuint (data.proxies, ==, 3);
  if (data.release_on_connect)
    {
      g_assert_null (data.manager);
      g_assert_null (data.weak_manager);
    }
  else
    {
      g_assert_true (data.weak_manager == data.manager);
      g_clear_object (&data.manager);
      g_assert_null (data.weak_manager);
    }
}

static void
test_last_proxy_release (void)
{
  UpdateData data = { 0 };

  prepare_update (&data, TRUE);
  data.release_at = 3;
  drain_events ();

  g_assert_cmpuint (data.proxies, ==, 3);
  g_assert_null (data.manager);
  g_assert_null (data.weak_manager);
}

static void
release_signal_owner (GtkUIManager *manager,
                      GtkAction    *action,
                      GtkWidget    *proxy,
                      GObject      *owner)
{
  UpdateData *data = g_object_get_data (owner, "test-data");

  data->proxies++;
  g_assert_cmpuint (data->proxies, ==, 1);
  g_object_unref (owner);
  g_assert_nonnull (data->weak_manager);
}

static void
test_signal_owner_release (void)
{
  UpdateData data = { 0 };
  GObject *owner;
  GObject *weak_owner;

  prepare_update (&data, FALSE);
  g_signal_handlers_disconnect_by_func (data.manager, connect_proxy, &data);

  owner = g_object_new (G_TYPE_OBJECT, NULL);
  weak_owner = owner;
  g_object_add_weak_pointer (owner, (gpointer *) &weak_owner);
  g_object_set_data (owner, "test-data", &data);
  g_object_set_data_full (owner, "manager", data.manager, g_object_unref);
  data.manager = NULL;
  g_signal_connect_object (data.weak_manager, "connect-proxy",
                           G_CALLBACK (release_signal_owner), owner, 0);

  drain_events ();

  g_assert_cmpuint (data.proxies, ==, 1);
  g_assert_null (weak_owner);
  g_assert_null (data.weak_manager);
}

static void
test_get_ui_lifetime (gconstpointer release_owner)
{
  UpdateData data = { 0 };
  gchar *ui;

  prepare_update (&data, GPOINTER_TO_INT (release_owner));
  ui = gtk_ui_manager_get_ui (data.manager);

  g_assert_cmpuint (data.proxies, ==, 3);
  if (data.release_on_connect)
    {
      g_assert_null (data.manager);
      g_assert_null (data.weak_manager);
    }
  else
    {
      g_assert_true (data.weak_manager == data.manager);
      g_clear_object (&data.manager);
      g_assert_null (data.weak_manager);
    }

  /* The returned XML must remain usable after the manager is finalized. */
  g_assert_nonnull (ui);
  g_assert_nonnull (strstr (ui, "action=\"first\""));
  g_assert_nonnull (strstr (ui, "action=\"second\""));
  g_assert_nonnull (strstr (ui, "action=\"third\""));
  g_free (ui);
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
  g_test_add_data_func ("/uimanager/lifetime/nested-normal",
                        GINT_TO_POINTER (FALSE), test_nested_update);
  g_test_add_data_func ("/uimanager/lifetime/nested-release",
                        GINT_TO_POINTER (TRUE), test_nested_update);
  g_test_add_func ("/uimanager/lifetime/last-proxy-release", test_last_proxy_release);
  g_test_add_func ("/uimanager/lifetime/signal-owner-release", test_signal_owner_release);
  g_test_add_data_func ("/uimanager/lifetime/get-ui-normal",
                        GINT_TO_POINTER (FALSE), test_get_ui_lifetime);
  g_test_add_data_func ("/uimanager/lifetime/get-ui-release",
                        GINT_TO_POINTER (TRUE), test_get_ui_lifetime);

  return g_test_run ();
}
