/*
 * gtkappchooseronline.h: an extension point for online integration
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

#ifndef __GTK_APP_CHOOSER_ONLINE_H__
#define __GTK_APP_CHOOSER_ONLINE_H__

#include <glib.h>

#include <gtk/gtkwindow.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GTK_TYPE_APP_CHOOSER_ONLINE           (_gtk_app_chooser_online_get_type ())
#define GTK_APP_CHOOSER_ONLINE(o)             (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_APP_CHOOSER_ONLINE, GtkAppChooserOnline))
#define GTK_IS_APP_CHOOSER_ONLINE(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_APP_CHOOSER_ONLINE))
#define GTK_APP_CHOOSER_ONLINE_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GTK_TYPE_APP_CHOOSER_ONLINE, GtkAppChooserOnlineInterface))

typedef struct _GtkAppChooserOnline GtkAppChooserOnline;
typedef struct _GtkAppChooserOnlineInterface GtkAppChooserOnlineInterface;

struct _GtkAppChooserOnlineInterface {
  GTypeInterface g_iface;

  /* Methods */
  void (*search_for_mimetype_async)      (GtkAppChooserOnline  *self,
                                          const gchar          *content_type,
                                          GtkWindow            *parent,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);

  gboolean (*search_for_mimetype_finish) (GtkAppChooserOnline  *self,
                                          GAsyncResult         *res,
                                          GError              **error);
};

GType                 _gtk_app_chooser_online_get_type                   (void) G_GNUC_CONST;

void                  _gtk_app_chooser_online_get_default_async          (GAsyncReadyCallback   callback,
                                                                          gpointer              user_data);
GtkAppChooserOnline * _gtk_app_chooser_online_get_default_finish         (GObject             *source,
                                                                          GAsyncResult        *result);

void                  _gtk_app_chooser_online_search_for_mimetype_async  (GtkAppChooserOnline  *self,
                                                                          const gchar          *content_type,
                                                                          GtkWindow            *parent,
                                                                          GCancellable         *cancellable,
                                                                          GAsyncReadyCallback   callback,
                                                                          gpointer              user_data);
gboolean              _gtk_app_chooser_online_search_for_mimetype_finish (GtkAppChooserOnline  *self,
                                                                          GAsyncResult         *res,
                                                                          GError              **error);

#endif /* __GTK_APP_CHOOSER_ONLINE_H__ */
