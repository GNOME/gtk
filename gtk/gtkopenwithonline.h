/*
 * gtkopenwithonline.h: an extension point for online integration
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

#ifndef __GTK_OPEN_WITH_ONLINE_H__
#define __GTK_OPEN_WITH_ONLINE_H__

#include <glib.h>

#include <gtk/gtkwindow.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GTK_TYPE_OPEN_WITH_ONLINE\
  (gtk_open_with_online_get_type ())
#define GTK_OPEN_WITH_ONLINE(o)\
  (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_OPEN_WITH_ONLINE, GtkOpenWithOnline))
#define GTK_IS_OPEN_WITH_ONLINE(o)\
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_OPEN_WITH_ONLINE))
#define GTK_OPEN_WITH_ONLINE_GET_IFACE(obj)\
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GTK_TYPE_OPEN_WITH_ONLINE, GtkOpenWithOnlineInterface))

typedef struct _GtkOpenWithOnline GtkOpenWithOnline;
typedef struct _GtkOpenWithOnlineInterface GtkOpenWithOnlineInterface;

struct _GtkOpenWithOnlineInterface {
  GTypeInterface g_iface;

  /* Methods */
  void (*search_for_mimetype_async) (GtkOpenWithOnline *self,
				     const gchar *content_type,
				     GtkWindow *parent,
				     GAsyncReadyCallback callback,
				     gpointer user_data);

  gboolean (*search_for_mimetype_finish) (GtkOpenWithOnline *self,
					  GAsyncResult *res,
					  GError **error);
};

GType gtk_open_with_online_get_type (void) G_GNUC_CONST;
void  gtk_open_with_online_search_for_mimetype_async (GtkOpenWithOnline *self,
						      const gchar *content_type,
						      GtkWindow *parent,
						      GAsyncReadyCallback callback,
						      gpointer user_data);
gboolean gtk_open_with_online_search_for_mimetype_finish (GtkOpenWithOnline *self,
							  GAsyncResult *res,
							  GError **error);

GtkOpenWithOnline * gtk_open_with_online_get_default (void);

#endif /* __GTK_OPEN_WITH_ONLINE_H__ */
