/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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
 */

#ifndef __GTK_STYLE_PROVIDER_H__
#define __GTK_STYLE_PROVIDER_H__

#include <glib-object.h>
#include "gtkwidgetpath.h"
#include "gtkiconfactory.h"
#include "gtkstyleproperties.h"
#include "gtkenums.h"

G_BEGIN_DECLS

#define GTK_TYPE_STYLE_PROVIDER          (gtk_style_provider_get_type ())
#define GTK_STYLE_PROVIDER(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_STYLE_PROVIDER, GtkStyleProvider))
#define GTK_IS_STYLE_PROVIDER(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_STYLE_PROVIDER))
#define GTK_STYLE_PROVIDER_GET_IFACE(o)  (G_TYPE_INSTANCE_GET_INTERFACE ((o), GTK_TYPE_STYLE_PROVIDER, GtkStyleProviderIface))

#define GTK_STYLE_PROVIDER_PRIORITY_FALLBACK      1
#define GTK_STYLE_PROVIDER_PRIORITY_DEFAULT     200
#define GTK_STYLE_PROVIDER_PRIORITY_SETTINGS    400
#define GTK_STYLE_PROVIDER_PRIORITY_APPLICATION 600
#define GTK_STYLE_PROVIDER_PRIORITY_USER        800

typedef struct _GtkStyleProviderIface GtkStyleProviderIface;
typedef struct _GtkStyleProvider GtkStyleProvider; /* dummy typedef */

/**
 * GtkStyleProviderIface
 * @get_style: Gets a set of style information that applies to a widget path.
 * @get_style_property: Gets the value of a widget style property that applies to a widget path.
 * @get_icon_factory: Gets the icon factory that applies to a widget path.
 */
struct _GtkStyleProviderIface
{
  GTypeInterface g_iface;

  GtkStyleProperties * (* get_style) (GtkStyleProvider *provider,
                                      GtkWidgetPath    *path);

  gboolean (* get_style_property) (GtkStyleProvider *provider,
                                   GtkWidgetPath    *path,
                                   const gchar      *property_name,
                                   GValue           *value);

  GtkIconFactory * (* get_icon_factory) (GtkStyleProvider *provider,
					 GtkWidgetPath    *path);
};

GType gtk_style_provider_get_type (void) G_GNUC_CONST;

GtkStyleProperties *gtk_style_provider_get_style (GtkStyleProvider *provider,
                                                  GtkWidgetPath    *path);

gboolean gtk_style_provider_get_style_property (GtkStyleProvider *provider,
                                                GtkWidgetPath    *path,
                                                const gchar      *property_name,
                                                GValue           *value);

GtkIconFactory * gtk_style_provider_get_icon_factory (GtkStyleProvider *provider,
						      GtkWidgetPath    *path);

G_END_DECLS

#endif /* __GTK_STYLE_PROVIDER_H__ */
