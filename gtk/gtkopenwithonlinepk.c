/*
 * gtkopenwithonlinepk.c: packagekit module for open with
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
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Cosimo Cecchi <ccecchi@redhat.com>
 */

#include <config.h>

#include "gtkopenwithonlinepk.h"

#include "gtkopenwithonline.h"
#include "x11/gdkx.h"

#include <gio/gio.h>

#define gtk_open_with_online_pk_get_type _gtk_open_with_online_pk_get_type
static void open_with_online_iface_init (GtkOpenWithOnlineInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkOpenWithOnlinePk, gtk_open_with_online_pk,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_OPEN_WITH_ONLINE,
						open_with_online_iface_init)
			 g_io_extension_point_implement ("gtkopenwith-online",
							 g_define_type_id,
							 "packagekit", 10));

struct _GtkOpenWithOnlinePkPrivate {
  GSimpleAsyncResult *result;
  GtkWindow *parent;
  gchar *content_type;
};

static void
gtk_open_with_online_pk_finalize (GObject *obj)
{
  GtkOpenWithOnlinePk *self = GTK_OPEN_WITH_ONLINE_PK (obj);

  g_free (self->priv->content_type);
  g_clear_object (&self->priv->result);

  G_OBJECT_CLASS (gtk_open_with_online_pk_parent_class)->finalize (obj);
}

static void
gtk_open_with_online_pk_class_init (GtkOpenWithOnlinePkClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->finalize = gtk_open_with_online_pk_finalize;

  g_type_class_add_private (klass, sizeof (GtkOpenWithOnlinePkPrivate));
}

static void
gtk_open_with_online_pk_init (GtkOpenWithOnlinePk *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GTK_TYPE_OPEN_WITH_ONLINE_PK,
					    GtkOpenWithOnlinePkPrivate);
}

static gboolean
pk_search_mime_finish (GtkOpenWithOnline *obj,
		       GAsyncResult *res,
		       GError **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);

  return !g_simple_async_result_propagate_error (simple, error);
}

static void
install_mime_types_ready_cb (GObject *source,
			     GAsyncResult *res,
			     gpointer user_data)
{
  GtkOpenWithOnlinePk *self = user_data;
  GDBusProxy *proxy = G_DBUS_PROXY (source);
  GError *error = NULL;
  GVariant *variant;

  variant = g_dbus_proxy_call_finish (proxy, res, &error);

  if (variant == NULL) {
    /* don't show errors if the user cancelled the installation explicitely */
    if (g_strcmp0 (g_dbus_error_get_remote_error (error), "org.freedesktop.PackageKit.Modify.Cancelled") != 0)
      g_simple_async_result_set_from_error (self->priv->result, error);

    g_error_free (error);
  }

  g_simple_async_result_complete (self->priv->result);
}

static void
pk_proxy_appeared_cb (GObject *source,
		      GAsyncResult *res,
		      gpointer user_data)
{
  GtkOpenWithOnlinePk *self = user_data;
  GDBusProxy *proxy;
  GError *error = NULL;
  guint xid = 0;
  GdkWindow *window;
  const gchar *mime_types[2];

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (error != NULL) {
    g_simple_async_result_set_from_error (self->priv->result, error);
    g_error_free (error);

    g_simple_async_result_complete (self->priv->result);

    return;
  }

  window = gtk_widget_get_window (GTK_WIDGET (self->priv->parent));
  xid = GDK_WINDOW_XID (window);

  mime_types[0] = self->priv->content_type;
  mime_types[1] = NULL;

  g_dbus_proxy_call (proxy,
		     "InstallMimeTypes",
		     g_variant_new ("(u^ass)",
				    xid,
				    mime_types,
				    "hide-confirm-search"),
		     G_DBUS_CALL_FLAGS_NONE,
		     G_MAXINT, /* no timeout */
		     NULL,
		     install_mime_types_ready_cb,
		     self);
}

static void
pk_search_mime_async (GtkOpenWithOnline *obj,
		      const gchar *content_type,
		      GtkWindow *parent,
		      GAsyncReadyCallback callback,
		      gpointer user_data)
{
  GtkOpenWithOnlinePk *self = GTK_OPEN_WITH_ONLINE_PK (obj);

  self->priv->result = g_simple_async_result_new (G_OBJECT (self),
						  callback, user_data,
						  gtk_open_with_online_search_for_mimetype_async);
  self->priv->parent = parent;
  self->priv->content_type = g_strdup (content_type);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
			    G_DBUS_PROXY_FLAGS_NONE,
			    NULL,
			    "org.freedesktop.PackageKit",
			    "/org/freedesktop/PackageKit",
			    "org.freedesktop.PackageKit.Modify",
			    NULL,
			    pk_proxy_appeared_cb,
			    self);
}

static void
open_with_online_iface_init (GtkOpenWithOnlineInterface *iface)
{
  iface->search_for_mimetype_async = pk_search_mime_async;
  iface->search_for_mimetype_finish = pk_search_mime_finish;
}
