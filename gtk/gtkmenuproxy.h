/*
 * Copyright (C) 2010 Canonical, Ltd.
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
 *
 * Authors: Cody Russell <bratsche@gnome.org>
 */

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_MENU_PROXY_H__
#define __GTK_MENU_PROXY_H__

#include <gtk/gtkwidget.h>
#include <gtk/gtktypeutils.h>

G_BEGIN_DECLS

#define GTK_TYPE_MENU_PROXY                  (gtk_menu_proxy_get_type ())
#define GTK_MENU_PROXY(o)                    (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_MENU_PROXY, GtkMenuProxy))
#define GTK_MENU_PROXY_CLASS(k)              (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_MENU_PROXY, GtkMenuProxyClass))
#define GTK_IS_MENU_PROXY(o)                 (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_MENU_PROXY))
#define GTK_IS_MENU_PROXY_CLASS(k)           (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_MENU_PROXY))
#define GTK_MENU_PROXY_GET_CLASS(o)          (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_MENU_PROXY, GtkMenuProxyClass))


typedef struct _GtkMenuProxy      GtkMenuProxy;
typedef struct _GtkMenuProxyClass GtkMenuProxyClass;

struct _GtkMenuProxy
{
  GObject parent_object;
};

struct _GtkMenuProxyClass
{
  GObjectClass parent_class;

  /* vtable */
  void (* insert)    (GtkMenuProxy *proxy, GtkWidget *child, guint position);

  /* signals */
  void (* inserted)  (GtkMenuProxy *proxy, GtkWidget *child);
};

GType              gtk_menu_proxy_get_type      (void) G_GNUC_CONST;
GtkMenuProxy*      gtk_menu_proxy_get           (void);
void               gtk_menu_proxy_insert        (GtkMenuProxy *proxy,
                                                 GtkWidget    *child,
                                                 guint         position);

G_END_DECLS

#endif /* __GTK_MENU_PROXY_H__ */
