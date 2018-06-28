/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2012 Red Hat, Inc.
 * Copyright © 2013 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 *         Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkapplicationprivate.h"
#include "gtksettings.h"

G_DEFINE_TYPE (GtkApplicationImplDBus, gtk_application_impl_dbus, GTK_TYPE_APPLICATION_IMPL)

#define GNOME_DBUS_NAME             "org.gnome.SessionManager"
#define GNOME_DBUS_OBJECT_PATH      "/org/gnome/SessionManager"
#define GNOME_DBUS_INTERFACE        "org.gnome.SessionManager"
#define GNOME_DBUS_CLIENT_INTERFACE "org.gnome.SessionManager.ClientPrivate"
#define XFCE_DBUS_NAME              "org.xfce.SessionManager"
#define XFCE_DBUS_OBJECT_PATH       "/org/xfce/SessionManager"
#define XFCE_DBUS_INTERFACE         "org.xfce.Session.Manager"
#define XFCE_DBUS_CLIENT_INTERFACE  "org.xfce.Session.Client"

static void
unregister_client (GtkApplicationImplDBus *dbus)
{
  GError *error = NULL;

  g_debug ("Unregistering client");

  g_dbus_proxy_call_sync (dbus->sm_proxy,
                          "UnregisterClient",
                          g_variant_new ("(o)", dbus->client_path),
                          G_DBUS_CALL_FLAGS_NONE,
                          G_MAXINT,
                          NULL,
                          &error);

  if (error)
    {
      g_warning ("Failed to unregister client: %s", error->message);
      g_error_free (error);
    }

  g_clear_object (&dbus->client_proxy);

  g_free (dbus->client_path);
  dbus->client_path = NULL;
}

static void
send_quit_response (GtkApplicationImplDBus *dbus,
                    gboolean                will_quit,
                    const gchar            *reason)
{
  g_debug ("Calling EndSessionResponse %d '%s'", will_quit, reason);

  g_dbus_proxy_call (dbus->client_proxy,
                     "EndSessionResponse",
                     g_variant_new ("(bs)", will_quit, reason ? reason : ""),
                     G_DBUS_CALL_FLAGS_NONE,
                     G_MAXINT,
                     NULL, NULL, NULL);
}

static void
client_proxy_signal (GDBusProxy  *proxy,
                     const gchar *sender_name,
                     const gchar *signal_name,
                     GVariant    *parameters,
                     gpointer     user_data)
{
  GtkApplicationImplDBus *dbus = user_data;

  if (g_str_equal (signal_name, "QueryEndSession"))
    {
      g_debug ("Received QueryEndSession");
      send_quit_response (dbus, TRUE, NULL);
    }
  else if (g_str_equal (signal_name, "CancelEndSession"))
    {
      g_debug ("Received CancelEndSession");
    }
  else if (g_str_equal (signal_name, "EndSession"))
    {
      g_debug ("Received EndSession");
      send_quit_response (dbus, TRUE, NULL);
      unregister_client (dbus);
      g_application_quit (G_APPLICATION (dbus->impl.application));
    }
  else if (g_str_equal (signal_name, "Stop"))
    {
      g_debug ("Received Stop");
      unregister_client (dbus);
      g_application_quit (G_APPLICATION (dbus->impl.application));
    }
}

static GDBusProxy*
gtk_application_get_proxy_if_service_present (GDBusConnection *connection,
                                              GDBusProxyFlags  flags,
                                              const gchar     *bus_name,
                                              const gchar     *object_path,
                                              const gchar     *interface,
                                              GError         **error)
{
  GDBusProxy *proxy;
  gchar *owner;

  proxy = g_dbus_proxy_new_sync (connection,
                                 flags,
                                 NULL,
                                 bus_name,
                                 object_path,
                                 interface,
                                 NULL,
                                 error);

  if (!proxy)
    return NULL;

  /* is there anyone actually providing the service? */
  owner = g_dbus_proxy_get_name_owner (proxy);
  if (owner == NULL)
    {
      g_clear_object (&proxy);
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER,
                   "The name %s is not owned", bus_name);
    }
  else
    g_free (owner);

  return proxy;
}

#ifdef G_HAS_CONSTRUCTORS
#ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(stash_desktop_autostart_id)
#endif
G_DEFINE_CONSTRUCTOR(stash_desktop_autostart_id)
#endif

static char *client_id = NULL;

static void
stash_desktop_autostart_id (void)
{
  const char *desktop_autostart_id;

  desktop_autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");
  client_id = g_strdup (desktop_autostart_id ? desktop_autostart_id : "");

  /* Unset DESKTOP_AUTOSTART_ID in order to avoid child processes to
   * use the same client id.
   */
  g_unsetenv ("DESKTOP_AUTOSTART_ID");
}

static void
gtk_application_impl_dbus_startup (GtkApplicationImpl *impl,
                                   gboolean            register_session)
{
  GtkApplicationImplDBus *dbus = (GtkApplicationImplDBus *) impl;
  GError *error = NULL;
  GVariant *res;
  gboolean same_bus;
  const char *bus_name;
  const char *client_interface;

#ifndef G_HAS_CONSTRUCTORS
  stash_desktop_autostart_id ();
#endif

  dbus->session = g_application_get_dbus_connection (G_APPLICATION (impl->application));

  if (!dbus->session)
    goto out;

  dbus->application_id = g_application_get_application_id (G_APPLICATION (impl->application));
  dbus->object_path = g_application_get_dbus_object_path (G_APPLICATION (impl->application));
  dbus->unique_name = g_dbus_connection_get_unique_name (dbus->session);

  g_debug ("Connecting to session manager");

  /* Try the GNOME session manager first */
  dbus->sm_proxy = gtk_application_get_proxy_if_service_present (dbus->session,
                                                                 G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
                                                                 G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                                 G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                                 GNOME_DBUS_NAME,
                                                                 GNOME_DBUS_OBJECT_PATH,
                                                                 GNOME_DBUS_INTERFACE,
                                                                 &error);

  if (error)
    {
      g_debug ("Failed to get the GNOME session proxy: %s", error->message);
      g_clear_error (&error);
    }

  if (!dbus->sm_proxy)
    {
      /* Fallback to trying the Xfce session manager */
      dbus->sm_proxy = gtk_application_get_proxy_if_service_present (dbus->session,
                                                                     G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
                                                                     G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                                     G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                                     XFCE_DBUS_NAME,
                                                                     XFCE_DBUS_OBJECT_PATH,
                                                                     XFCE_DBUS_INTERFACE,
                                                                     &error);

      if (error)
        {
          g_debug ("Failed to get the Xfce session proxy: %s", error->message);
          g_clear_error (&error);
          goto out;
        }
    }

  if (!register_session)
    goto out;

  g_debug ("Registering client '%s' '%s'", dbus->application_id, client_id);

  res = g_dbus_proxy_call_sync (dbus->sm_proxy,
                                "RegisterClient",
                                g_variant_new ("(ss)", dbus->application_id, client_id),
                                G_DBUS_CALL_FLAGS_NONE,
                                G_MAXINT,
                                NULL,
                                &error);

  if (error)
    {
      g_warning ("Failed to register client: %s", error->message);
      g_clear_error (&error);
      g_clear_object (&dbus->sm_proxy);
      goto out;
    }

  g_variant_get (res, "(o)", &dbus->client_path);
  g_variant_unref (res);

  g_debug ("Registered client at '%s'", dbus->client_path);

  if (g_str_equal (g_dbus_proxy_get_name (dbus->sm_proxy), GNOME_DBUS_NAME))
    {
      bus_name = GNOME_DBUS_NAME;
      client_interface = GNOME_DBUS_CLIENT_INTERFACE;
    }
  else
    {
      bus_name = XFCE_DBUS_NAME;
      client_interface = XFCE_DBUS_CLIENT_INTERFACE;
    }

  dbus->client_proxy = g_dbus_proxy_new_sync (dbus->session, 0,
                                              NULL,
                                              bus_name,
                                              dbus->client_path,
                                              client_interface,
                                              NULL,
                                              &error);
  if (error)
    {
      g_warning ("Failed to get client proxy: %s", error->message);
      g_clear_error (&error);
      g_free (dbus->client_path);
      dbus->client_path = NULL;
      goto out;
    }

  g_signal_connect (dbus->client_proxy, "g-signal", G_CALLBACK (client_proxy_signal), dbus);

 out:
  same_bus = FALSE;

  if (dbus->session)
    {
      const gchar *id;
      const gchar *id2;
      GValue value = G_VALUE_INIT;

      g_value_init (&value, G_TYPE_STRING);
      gdk_display_get_setting (gdk_display_get_default (), "gtk-session-bus-id", &value);
      id = g_value_get_string (&value);

      if (id && id[0])
        {
          res = g_dbus_connection_call_sync (dbus->session,
                                             "org.freedesktop.DBus",
                                             "/org/freedesktop/DBus",
                                             "org.freedesktop.DBus",
                                             "GetId",
                                             NULL,
                                             NULL,
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL,
                                             NULL);
          if (res)
            {
              g_variant_get (res, "(&s)", &id2);

              if (g_strcmp0 (id, id2) == 0)
                same_bus = TRUE;

              g_variant_unref (res);
            }
        }
      else
        same_bus = TRUE;

      g_value_unset (&value);
    }

  if (!same_bus)
    g_object_set (gtk_settings_get_default (),
                  "gtk-shell-shows-app-menu", FALSE,
                  "gtk-shell-shows-menubar", FALSE,
                  NULL);

  if (dbus->sm_proxy == NULL)
    {
      dbus->inhibit_proxy = gtk_application_get_proxy_if_service_present (dbus->session,
                                                                          G_DBUS_PROXY_FLAGS_NONE,
                                                                          "org.freedesktop.portal.Desktop",
                                                                          "/org/freedesktop/portal/desktop",
                                                                          "org.freedesktop.portal.Inhibit",
                                                                          &error);
      if (error)
        {
          g_debug ("Failed to get an inhibit portal proxy: %s", error->message);
          g_clear_error (&error);
        }
    }
}

static void
gtk_application_impl_dbus_shutdown (GtkApplicationImpl *impl)
{
}

GQuark gtk_application_impl_dbus_export_id_quark (void);

G_DEFINE_QUARK (GtkApplicationImplDBus export id, gtk_application_impl_dbus_export_id)

static void
gtk_application_impl_dbus_window_added (GtkApplicationImpl *impl,
                                        GtkWindow          *window)
{
  GtkApplicationImplDBus *dbus = (GtkApplicationImplDBus *) impl;
  GActionGroup *actions;
  gchar *path;
  guint id;

  if (!dbus->session || !GTK_IS_APPLICATION_WINDOW (window))
    return;

  /* Export the action group of this window, based on its id */
  actions = gtk_application_window_get_action_group (GTK_APPLICATION_WINDOW (window));

  path = gtk_application_impl_dbus_get_window_path (dbus, window);
  id = g_dbus_connection_export_action_group (dbus->session, path, actions, NULL);
  g_free (path);

  g_object_set_qdata (G_OBJECT (window), gtk_application_impl_dbus_export_id_quark (), GUINT_TO_POINTER (id));
}

static void
gtk_application_impl_dbus_window_removed (GtkApplicationImpl *impl,
                                          GtkWindow          *window)
{
  GtkApplicationImplDBus *dbus = (GtkApplicationImplDBus *) impl;
  guint id;

  id = GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (window), gtk_application_impl_dbus_export_id_quark ()));
  if (id)
    {
      g_dbus_connection_unexport_action_group (dbus->session, id);
      g_object_set_qdata (G_OBJECT (window), gtk_application_impl_dbus_export_id_quark (), NULL);
    }
}

static void
gtk_application_impl_dbus_active_window_changed (GtkApplicationImpl *impl,
                                                 GtkWindow          *window)
{
}

static void
gtk_application_impl_dbus_publish_menu (GtkApplicationImplDBus  *dbus,
                                        const gchar             *type,
                                        GMenuModel              *model,
                                        guint                   *id,
                                        gchar                  **path)
{
  gint i;

  if (dbus->session == NULL)
    return;

  /* unexport any existing menu */
  if (*id)
    {
      g_dbus_connection_unexport_menu_model (dbus->session, *id);
      g_free (*path);
      *path = NULL;
      *id = 0;
    }

  /* export the new menu, if there is one */
  if (model != NULL)
    {
      /* try getting the preferred name */
      *path = g_strconcat (dbus->object_path, "/menus/", type, NULL);
      *id = g_dbus_connection_export_menu_model (dbus->session, *path, model, NULL);

      /* keep trying until we get a working name... */
      for (i = 0; *id == 0; i++)
        {
          g_free (*path);
          *path = g_strdup_printf ("%s/menus/%s%d", dbus->object_path, type, i);
          *id = g_dbus_connection_export_menu_model (dbus->session, *path, model, NULL);
        }
    }
}

static void
gtk_application_impl_dbus_set_app_menu (GtkApplicationImpl *impl,
                                        GMenuModel         *app_menu)
{
  GtkApplicationImplDBus *dbus = (GtkApplicationImplDBus *) impl;

  gtk_application_impl_dbus_publish_menu (dbus, "appmenu", app_menu, &dbus->app_menu_id, &dbus->app_menu_path);
}

static void
gtk_application_impl_dbus_set_menubar (GtkApplicationImpl *impl,
                                       GMenuModel         *menubar)
{
  GtkApplicationImplDBus *dbus = (GtkApplicationImplDBus *) impl;

  gtk_application_impl_dbus_publish_menu (dbus, "menubar", menubar, &dbus->menubar_id, &dbus->menubar_path);
}

static GVariant *
gtk_application_impl_dbus_real_get_window_system_id (GtkApplicationImplDBus *dbus,
                                                     GtkWindow              *window)
{
  return g_variant_new_uint32 (0);
}

/* returns floating */
static GVariant *
gtk_application_impl_dbus_get_window_system_id (GtkApplicationImplDBus *dbus,
                                                GtkWindow              *window)
{
  return GTK_APPLICATION_IMPL_DBUS_GET_CLASS (dbus)->get_window_system_id (dbus, window);
}

static int next_cookie;

typedef struct {
  char *handle;
  int cookie;
} InhibitHandle;

static void
inhibit_handle_free (gpointer data)
{
  InhibitHandle *handle = data;

  g_free (handle->handle);
  g_free (handle);
}

static guint
gtk_application_impl_dbus_inhibit (GtkApplicationImpl         *impl,
                                   GtkWindow                  *window,
                                   GtkApplicationInhibitFlags  flags,
                                   const gchar                *reason)
{
  GtkApplicationImplDBus *dbus = (GtkApplicationImplDBus *) impl;
  GVariant *res;
  GError *error = NULL;
  guint cookie;
  static gboolean warned = FALSE;

  if (dbus->sm_proxy)
    {
      res = g_dbus_proxy_call_sync (dbus->sm_proxy,
                                    "Inhibit",
                                    g_variant_new ("(s@usu)",
                                                   dbus->application_id,
                                                   window ? gtk_application_impl_dbus_get_window_system_id (dbus, window) : g_variant_new_uint32 (0),
                                                   reason,
                                                   flags),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    G_MAXINT,
                                    NULL,
                                    &error);

      if (res)
        {
          g_variant_get (res, "(u)", &cookie);
          g_variant_unref (res);
          return cookie;
        }

      if (error)
        {
          if (!warned)
            {
              g_warning ("Calling %s.Inhibit failed: %s",
                         g_dbus_proxy_get_interface_name (dbus->sm_proxy),
                         error->message);
              warned = TRUE;
            }
          g_clear_error (&error);
        }
    }
  else if (dbus->inhibit_proxy)
    {
      GVariantBuilder options;

      g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
      g_variant_builder_add (&options, "{sv}", "reason", g_variant_new_string (reason));
      res = g_dbus_proxy_call_sync (dbus->inhibit_proxy,
                                    "Inhibit",
                                    g_variant_new ("(su@a{sv})",
                                                   "", /* window */
                                                   flags,
                                                   g_variant_builder_end (&options)),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    G_MAXINT,
                                    NULL,
                                    &error);
      if (res)
        {
          InhibitHandle *handle;

          handle = g_new (InhibitHandle, 1);
          handle->cookie = ++next_cookie;

          g_variant_get (res, "(o)", &handle->handle);
          g_variant_unref (res);

          dbus->inhibit_handles = g_slist_prepend (dbus->inhibit_handles, handle);

          return handle->cookie;
        }

      if (error)
        {
          if (!warned)
            {
              g_warning ("Calling %s.Inhibit failed: %s",
                         g_dbus_proxy_get_interface_name (dbus->inhibit_proxy),
                         error->message);
              warned = TRUE;
            }
          g_clear_error (&error);
        }
    }

  return 0;
}

static void
gtk_application_impl_dbus_uninhibit (GtkApplicationImpl *impl,
                                     guint               cookie)
{
  GtkApplicationImplDBus *dbus = (GtkApplicationImplDBus *) impl;

  if (dbus->sm_proxy)
    {
      g_dbus_proxy_call (dbus->sm_proxy,
                         "Uninhibit",
                         g_variant_new ("(u)", cookie),
                         G_DBUS_CALL_FLAGS_NONE,
                         G_MAXINT,
                         NULL, NULL, NULL);
    }
  else if (dbus->inhibit_proxy)
    {
      GSList *l;

      for (l = dbus->inhibit_handles; l; l = l->next)
        {
          InhibitHandle *handle = l->data;
          if (handle->cookie == cookie)
            {
              g_dbus_connection_call (dbus->session,
                                      "org.freedesktop.portal.Desktop",
                                      handle->handle,
                                      "org.freedesktop.portal.Request",
                                      "Close",
                                      g_variant_new ("()"),
                                      G_VARIANT_TYPE_UNIT,
                                      G_DBUS_CALL_FLAGS_NONE,
                                      G_MAXINT,
                                      NULL, NULL, NULL);
              dbus->inhibit_handles = g_slist_remove (dbus->inhibit_handles, handle);
              inhibit_handle_free (handle);
              break;
            }
        }
    }
}

static gboolean
gtk_application_impl_dbus_prefers_app_menu (GtkApplicationImpl *impl)
{
  static gboolean decided;
  static gboolean result;

  /* We do not support notifying if/when the result changes, so make
   * sure that once we give an answer, we will always give the same one.
   */
  if (!decided)
    {
      GtkSettings *gtk_settings;
      gboolean show_app_menu;
      gboolean show_menubar;

      gtk_settings = gtk_settings_get_default ();
      g_object_get (G_OBJECT (gtk_settings),
                    "gtk-shell-shows-app-menu", &show_app_menu,
                    "gtk-shell-shows-menubar", &show_menubar,
                    NULL);

      /* We prefer traditional menus when we have a shell that doesn't
       * show the appmenu or we have a shell that shows menubars
       * (ie: Unity)
       */
      result = show_app_menu && !show_menubar;
      decided = TRUE;
    }

  return result;
}

static void
gtk_application_impl_dbus_init (GtkApplicationImplDBus *dbus)
{
}

static void
gtk_application_impl_dbus_finalize (GObject *object)
{
  GtkApplicationImplDBus *dbus = (GtkApplicationImplDBus *) object;

  g_clear_object (&dbus->inhibit_proxy);
  g_slist_free_full (dbus->inhibit_handles, inhibit_handle_free);
  g_free (dbus->app_menu_path);
  g_free (dbus->menubar_path);
  g_clear_object (&dbus->sm_proxy);

  G_OBJECT_CLASS (gtk_application_impl_dbus_parent_class)->finalize (object);
}

static void
gtk_application_impl_dbus_class_init (GtkApplicationImplDBusClass *class)
{
  GtkApplicationImplClass *impl_class = GTK_APPLICATION_IMPL_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  class->get_window_system_id = gtk_application_impl_dbus_real_get_window_system_id;

  impl_class->startup = gtk_application_impl_dbus_startup;
  impl_class->shutdown = gtk_application_impl_dbus_shutdown;
  impl_class->window_added = gtk_application_impl_dbus_window_added;
  impl_class->window_removed = gtk_application_impl_dbus_window_removed;
  impl_class->active_window_changed = gtk_application_impl_dbus_active_window_changed;
  impl_class->set_app_menu = gtk_application_impl_dbus_set_app_menu;
  impl_class->set_menubar = gtk_application_impl_dbus_set_menubar;
  impl_class->inhibit = gtk_application_impl_dbus_inhibit;
  impl_class->uninhibit = gtk_application_impl_dbus_uninhibit;
  impl_class->prefers_app_menu = gtk_application_impl_dbus_prefers_app_menu;

  gobject_class->finalize = gtk_application_impl_dbus_finalize;
}

gchar *
gtk_application_impl_dbus_get_window_path (GtkApplicationImplDBus *dbus,
                                           GtkWindow *window)
{
  if (dbus->session && GTK_IS_APPLICATION_WINDOW (window))
    return g_strdup_printf ("%s/window/%d",
                            dbus->object_path,
                            gtk_application_window_get_id (GTK_APPLICATION_WINDOW (window)));
  else
    return NULL;
}
