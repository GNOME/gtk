/*
 * Copyright © 2015 Endless Mobile, Inc.
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
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 */

#ifndef __GTK_ICON_PRIVATE_H__
#define __GTK_ICON_PRIVATE_H__

#include "gtk/gtkwidget.h"

G_BEGIN_DECLS

#define GTK_TYPE_ICON           (gtk_icon_get_type ())
#define GTK_ICON(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_ICON, GtkIcon))
#define GTK_ICON_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_ICON, GtkIconClass))
#define GTK_IS_ICON(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_ICON))
#define GTK_IS_ICON_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_ICON))
#define GTK_ICON_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ICON, GtkIconClass))

typedef struct _GtkIcon           GtkIcon;
typedef struct _GtkIconClass      GtkIconClass;

struct _GtkIcon
{
  GtkWidget parent;
};

struct _GtkIconClass
{
  GtkWidgetClass  parent_class;
};

GType        gtk_icon_get_type               (void) G_GNUC_CONST;

GtkWidget *  gtk_icon_new                    (const char *css_name);
const char * gtk_icon_get_css_name           (GtkIcon    *icon);
void         gtk_icon_set_css_name           (GtkIcon    *icon,
                                              const char *css_name);

G_END_DECLS

#endif /* __GTK_ICON_PRIVATE_H__ */
