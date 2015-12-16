/*
 * Copyright © 2015 Red Hat Inc.
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

#ifndef __GTK_BUILTIN_ICON_PRIVATE_H__
#define __GTK_BUILTIN_ICON_PRIVATE_H__

#include "gtk/gtkcssgadgetprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_BUILTIN_ICON           (gtk_builtin_icon_get_type ())
#define GTK_BUILTIN_ICON(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_BUILTIN_ICON, GtkBuiltinIcon))
#define GTK_BUILTIN_ICON_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_BUILTIN_ICON, GtkBuiltinIconClass))
#define GTK_IS_BUILTIN_ICON(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_BUILTIN_ICON))
#define GTK_IS_BUILTIN_ICON_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_BUILTIN_ICON))
#define GTK_BUILTIN_ICON_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_BUILTIN_ICON, GtkBuiltinIconClass))

typedef struct _GtkBuiltinIcon           GtkBuiltinIcon;
typedef struct _GtkBuiltinIconClass      GtkBuiltinIconClass;

struct _GtkBuiltinIcon
{
  GtkCssGadget parent;
};

struct _GtkBuiltinIconClass
{
  GtkCssGadgetClass  parent_class;
};

GType                   gtk_builtin_icon_get_type               (void) G_GNUC_CONST;

GtkCssGadget *          gtk_builtin_icon_new                    (const char             *name,
                                                                 GtkWidget              *owner,
                                                                 GtkCssGadget           *parent,
                                                                 GtkCssGadget           *next_sibling);
GtkCssGadget *          gtk_builtin_icon_new_for_node           (GtkCssNode             *node,
                                                                 GtkWidget              *owner);

void                    gtk_builtin_icon_set_image              (GtkBuiltinIcon         *icon,
                                                                 GtkCssImageBuiltinType  image);
GtkCssImageBuiltinType  gtk_builtin_icon_get_image              (GtkBuiltinIcon         *icon);
void                    gtk_builtin_icon_set_default_size       (GtkBuiltinIcon         *icon,
                                                                 int                     default_size);
int                     gtk_builtin_icon_get_default_size       (GtkBuiltinIcon         *icon);
void                    gtk_builtin_icon_set_default_size_property (GtkBuiltinIcon      *icon,
                                                                 const char             *property_name);
const char *            gtk_builtin_icon_get_default_size_property (GtkBuiltinIcon      *icon);

G_END_DECLS

#endif /* __GTK_BUILTIN_ICON_PRIVATE_H__ */
