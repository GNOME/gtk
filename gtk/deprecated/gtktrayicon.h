/* gtktrayicon.h
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
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

#ifndef __GTK_TRAY_ICON_H__
#define __GTK_TRAY_ICON_H__

#include "gtkplug.h"

G_BEGIN_DECLS

#define GTK_TYPE_TRAY_ICON		(gtk_tray_icon_get_type ())
#define GTK_TRAY_ICON(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TRAY_ICON, GtkTrayIcon))
#define GTK_TRAY_ICON_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TRAY_ICON, GtkTrayIconClass))
#define GTK_IS_TRAY_ICON(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TRAY_ICON))
#define GTK_IS_TRAY_ICON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TRAY_ICON))
#define GTK_TRAY_ICON_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TRAY_ICON, GtkTrayIconClass))
	
typedef struct _GtkTrayIcon	   GtkTrayIcon;
typedef struct _GtkTrayIconPrivate GtkTrayIconPrivate;
typedef struct _GtkTrayIconClass   GtkTrayIconClass;

struct _GtkTrayIcon
{
  GtkPlug parent_instance;

  GtkTrayIconPrivate *priv;
};

struct _GtkTrayIconClass
{
  GtkPlugClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1);
  void (*_gtk_reserved2);
  void (*_gtk_reserved3);
  void (*_gtk_reserved4);
};

GDK_AVAILABLE_IN_ALL
GType          gtk_tray_icon_get_type         (void) G_GNUC_CONST;

GtkTrayIcon   *_gtk_tray_icon_new_for_screen  (GdkScreen   *screen,
					       const gchar *name);

GtkTrayIcon   *_gtk_tray_icon_new             (const gchar *name);

guint          _gtk_tray_icon_send_message    (GtkTrayIcon *icon,
					       gint         timeout,
					       const gchar *message,
					       gint         len);
void           _gtk_tray_icon_cancel_message  (GtkTrayIcon *icon,
					       guint        id);

GtkOrientation _gtk_tray_icon_get_orientation (GtkTrayIcon *icon);
gint           _gtk_tray_icon_get_padding     (GtkTrayIcon *icon);
gint           _gtk_tray_icon_get_icon_size   (GtkTrayIcon *icon);

G_END_DECLS

#endif /* __GTK_TRAY_ICON_H__ */
