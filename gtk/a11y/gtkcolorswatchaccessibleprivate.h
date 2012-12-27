/* GTK+ - accessibility implementations
 * Copyright 2012, Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_COLOR_SWATCH_ACCESSIBLE_H__
#define __GTK_COLOR_SWATCH_ACCESSIBLE_H__

#include <gtk/a11y/gtkwidgetaccessible.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLOR_SWATCH_ACCESSIBLE                         (_gtk_color_swatch_accessible_get_type ())
#define GTK_COLOR_SWATCH_ACCESSIBLE(obj)                         (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COLOR_SWATCH_ACCESSIBLE, GtkColorSwatchAccessible))
#define GTK_COLOR_SWATCH_ACCESSIBLE_CLASS(klass)                       (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_COLOR_SWATCH_ACCESSIBLE, GtkColorSwatchAccessibleClass))
#define GTK_IS_COLOR_SWATCH_ACCESSIBLE(obj)                      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COLOR_SWATCH_ACCESSIBLE))
#define GTK_IS_COLOR_SWATCH_ACCESSIBLE_CLASS(klass)              (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_COLOR_SWATCH_ACCESSIBLE))
#define GTK_COLOR_SWATCH_ACCESSIBLE_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_COLOR_SWATCH_ACCESSIBLE, GtkColorSwatchAccessibleClass))

typedef struct _GtkColorSwatchAccessible        GtkColorSwatchAccessible;
typedef struct _GtkColorSwatchAccessibleClass   GtkColorSwatchAccessibleClass;
typedef struct _GtkColorSwatchAccessiblePrivate GtkColorSwatchAccessiblePrivate;

struct _GtkColorSwatchAccessible
{
  GtkWidgetAccessible parent;

  GtkColorSwatchAccessiblePrivate *priv;
};

struct _GtkColorSwatchAccessibleClass
{
  GtkWidgetAccessibleClass parent_class;
};

GType _gtk_color_swatch_accessible_get_type (void);

G_END_DECLS

#endif /* __GTK_COLOR_SWATCH_ACCESSIBLE_H__ */
