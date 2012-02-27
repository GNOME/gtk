/*
 * Copyright © 2011 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_CSS_CUSTOM_PROPERTY_PRIVATE_H__
#define __GTK_CSS_CUSTOM_PROPERTY_PRIVATE_H__

#include "gtk/gtkcssstylepropertyprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_CUSTOM_PROPERTY           (_gtk_css_custom_property_get_type ())
#define GTK_CSS_CUSTOM_PROPERTY(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_CUSTOM_PROPERTY, GtkCssCustomProperty))
#define GTK_CSS_CUSTOM_PROPERTY_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_CUSTOM_PROPERTY, GtkCssCustomPropertyClass))
#define GTK_IS_CSS_CUSTOM_PROPERTY(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_CUSTOM_PROPERTY))
#define GTK_IS_CSS_CUSTOM_PROPERTY_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_CUSTOM_PROPERTY))
#define GTK_CSS_CUSTOM_PROPERTY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_CUSTOM_PROPERTY, GtkCssCustomPropertyClass))

typedef struct _GtkCssCustomProperty           GtkCssCustomProperty;
typedef struct _GtkCssCustomPropertyClass      GtkCssCustomPropertyClass;

struct _GtkCssCustomProperty
{
  GtkCssStyleProperty parent;

  GParamSpec *pspec;
  GtkStylePropertyParser property_parse_func;
};

struct _GtkCssCustomPropertyClass
{
  GtkCssStylePropertyClass parent_class;
};

GType                   _gtk_css_custom_property_get_type        (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __GTK_CSS_CUSTOM_PROPERTY_PRIVATE_H__ */
