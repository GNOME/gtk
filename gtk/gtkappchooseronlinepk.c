/*
 * gtkappchooseronlinepk.c: packagekit module for app-chooser
 *
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Cosimo Cecchi <ccecchi@redhat.com>
 */

#include "config.h"

#include "gtkappchooseronlinepk.h"

#include "gtkappchooseronline.h"
#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

#include <gio/gio.h>

#define gtk_app_chooser_online_pk_get_type _gtk_app_chooser_online_pk_get_type
static void app_chooser_online_iface_init (GtkAppChooserOnlineInterface *iface);
static void app_chooser_online_pk_async_initable_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkAppChooserOnlinePk, gtk_app_chooser_online_pk,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                                app_chooser_online_pk_async_initable_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_APP_CHOOSER_ONLINE,
                                                app_chooser_online_iface_init)
                         g_io_extension_point_implement ("gtkappchooser-online",
                                                         g_define_type_id,
                                                         "packagekit", 10));

struct _GtkAppChooserOnlinePkPrivate {
  GSimpleAsyncResult *init_result;
  guint watch_id;

  GDBusProxy *proxy;
  GSimpleAsyncResult *result;
  GtkWindow *parent;
};

static void
gtk_app_chooser_online_pk_dispose (GObject *obj)
{
  GtkAppChooserOnlinePk *self = GTK_APP_CHOOSER_ONLINE_PK (obj);

  g_clear_object (&self->priv->result);
  g_clear_object (&self->priv->proxy);

  G_OBJECT_CLASS (gtk_app_chooser_online_pk_parent_class)->dispose (obj);
}

static void
gtk_app_chooser_online_pk_class_init (GtkAppChooserOnlinePkClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->dispose = gtk_app_chooser_online_pk_dispose;

  g_type_class_add_private (klass, sizeof (GtkAppChooserOnlinePkPrivate));
}

static void
gtk_app_chooser_online_pk_init (GtkAppChooserOnlinePk *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GTK_TYPE_APP_CHOOSER_ONLINE_PK,
                                            GtkAppChooserOnlinePkPrivate);
}

static gboolean
pk_search_mime_finish (GtkAppChooserOnline *obj,
                       GAsyncResult *res,
                       GError **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);

  return !g_simple_async_result_propagate_error (simple, error);
}

static void
install_mime_types_ready_cb (GObject      *source,
                             GAsyncResult *res,
                             gpointer      user_data)
{
  GtkAppChooserOnlinePk *self = user_data;
  GDBusProxy *proxy = G_DBUS_PROXY (source);
  GError *error = NULL;
  GVariant *variant;

  variant = g_dbus_proxy_call_finish (proxy, res, &error);

  if (variant == NULL)
    {
      /* don't show errors if the user cancelled the installation explicitely
       * or if PK wasn't able to find any apps
       */
      if (g_strcmp0 (g_dbus_error_get_remote_error (error), "org.freedesktop.PackageKit.Modify.Cancelled") != 0 &&
          g_strcmp0 (g_dbus_error_get_remote_error (error), "org.freedesktop.PackageKit.Modify.NoPackagesFound") != 0)
        g_simple_async_result_set_from_error (self->priv->result, error);

      g_error_free (error);
    }

  g_simple_async_result_complete (self->priv->result);
  g_clear_object (&self->priv->result);
}

static void
pk_search_mime_async (GtkAppChooserOnline *obj,
                      const gchar         *content_type,
                      GtkWindow           *parent,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  GtkAppChooserOnlinePk *self = GTK_APP_CHOOSER_ONLINE_PK (obj);
  guint xid = 0;
  GdkWindow *window;
  const gchar *mime_types[2];

  self->priv->result = g_simple_async_result_new (G_OBJECT (self),
                                                  callback, user_data,
                                                  _gtk_app_chooser_online_search_for_mimetype_async);

#ifdef GDK_WINDOWING_X11
  window = gtk_widget_get_window (GTK_WIDGET (parent));
  if (GDK_IS_X11_WINDOW (window))
    xid = GDK_WINDOW_XID (window);
#endif

  mime_types[0] = content_type;
  mime_types[1] = NULL;

  g_dbus_proxy_call (self->priv->proxy,
                     "InstallMimeTypes",
                     g_variant_new ("(u^ass)",
                                    xid,
                                    mime_types,
                                    "hide-confirm-search"),
                     G_DBUS_CALL_FLAGS_NONE,
                     G_MAXINT, /* no timeout */
                     cancellable,
                     install_mime_types_ready_cb,
                     self);
}

static void
app_chooser_online_iface_init (GtkAppChooserOnlineInterface *iface)
{
  iface->search_for_mimetype_async = pk_search_mime_async;
  iface->search_for_mimetype_finish = pk_search_mime_finish;
}

static void
pk_proxy_created_cb (GObject *source,
                     GAsyncResult *result,
                     gpointer user_data)
{
  GtkAppChooserOnlinePk *self = user_data;
  GDBusProxy *proxy;

  proxy = g_dbus_proxy_new_finish (result, NULL);

  if (proxy == NULL)
    {
      g_simple_async_result_set_op_res_gboolean (self->priv->init_result, FALSE);
    }
  else
    {
      g_simple_async_result_set_op_res_gboolean (self->priv->init_result, TRUE);
      self->priv->proxy = proxy;
    }

  g_simple_async_result_complete (self->priv->init_result);
  g_clear_object (&self->priv->init_result);
}

static void
pk_appeared_cb (GDBusConnection *conn,
                const gchar *name,
                const gchar *owner,
                gpointer user_data)
{
  GtkAppChooserOnlinePk *self = user_data;

  /* create the proxy */
  g_dbus_proxy_new (conn, 0, NULL,
                    "org.freedesktop.PackageKit",
                    "/org/freedesktop/PackageKit",
                    "org.freedesktop.PackageKit.Modify",
                    NULL,
                    pk_proxy_created_cb,
                    self);

  g_bus_unwatch_name (self->priv->watch_id);
}

static void
pk_vanished_cb (GDBusConnection *conn,
                const gchar *name,
                gpointer user_data)
{
  GtkAppChooserOnlinePk *self = user_data;

  /* just return */
  g_simple_async_result_set_op_res_gboolean (self->priv->init_result, FALSE);
  g_simple_async_result_complete (self->priv->init_result);

  g_bus_unwatch_name (self->priv->watch_id);

  g_clear_object (&self->priv->init_result);
}

static gboolean
app_chooser_online_pk_init_finish (GAsyncInitable *init,
                                   GAsyncResult *res,
                                   GError **error)
{
  return g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (res));
}

static void
app_chooser_online_pk_init_async (GAsyncInitable *init,
                                  int io_priority,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
  GtkAppChooserOnlinePk *self = GTK_APP_CHOOSER_ONLINE_PK (init);

  self->priv->init_result = g_simple_async_result_new (G_OBJECT (self),
                                                       callback, user_data,
                                                       _gtk_app_chooser_online_get_default_async);

  self->priv->watch_id =
    g_bus_watch_name (G_BUS_TYPE_SESSION,
                      "org.freedesktop.PackageKit",
                      G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                      pk_appeared_cb,
                      pk_vanished_cb,
                      self,
                      NULL);
}

static void
app_chooser_online_pk_async_initable_init (GAsyncInitableIface *iface)
{
  iface->init_async = app_chooser_online_pk_init_async;
  iface->init_finish = app_chooser_online_pk_init_finish;
}
