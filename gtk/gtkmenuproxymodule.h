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

#ifndef __GTK_MENU_PROXY_MODULE_H__
#define __GTK_MENU_PROXY_MODULE_H__

#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS

#define GTK_TYPE_MENU_PROXY_MODULE         (gtk_menu_proxy_module_get_type ())
#define GTK_MENU_PROXY_MODULE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_MENU_PROXY_MODULE, GtkMenuProxyModule))
#define GTK_MENU_PROXY_MODULE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_MENU_PROXY_MODULE, GtkMenuProxyModuleClass))
#define GTK_IS_MENU_MODULE_PROXY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_MENU_PROXY_MODULE))
#define GTK_IS_MENU_PROXY_MODULE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_MENU_PROXY_MODULE))
#define GTK_MENU_PROXY_MODULE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_MENU_PROXY_MODULE, GtkMenuProxyModuleClass))

typedef struct _GtkMenuProxyModule        GtkMenuProxyModule;
typedef struct _GtkMenuProxyModuleClass   GtkMenuProxyModuleClass;
typedef struct _GtkMenuProxyModulePrivate GtkMenuProxyModulePrivate;

struct _GtkMenuProxyModule
{
  GTypeModule parent_instance;

  GtkMenuProxyModulePrivate *priv;

  GModule *library;
  gchar   *name;

  void        (* load)     (GtkMenuProxyModule *module);
  void        (* unload)   (GtkMenuProxyModule *module);
};

struct _GtkMenuProxyModuleClass
{
  GTypeModuleClass parent_class;
};

GType               gtk_menu_proxy_module_get_type (void) G_GNUC_CONST;

GtkMenuProxyModule *gtk_menu_proxy_module_get      (void);

G_END_DECLS

#endif /* __GTK_MENU_PROXY_MODULE_H__ */
