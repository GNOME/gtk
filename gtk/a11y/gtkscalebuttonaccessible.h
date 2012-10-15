/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2008 Jan Arne Petersen <jap@gnome.org>
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

#ifndef __GTK_SCALE_BUTTON_ACCESSIBLE_H__
#define __GTK_SCALE_BUTTON_ACCESSIBLE_H__

#include "gtkbuttonaccessible.h"

G_BEGIN_DECLS

#define GTK_TYPE_SCALE_BUTTON_ACCESSIBLE                     (_gtk_scale_button_accessible_get_type ())
#define GTK_SCALE_BUTTON_ACCESSIBLE(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SCALE_BUTTON_ACCESSIBLE, GtkScaleButtonAccessible))
#define GTK_SCALE_BUTTON_ACCESSIBLE_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SCALE_BUTTON_ACCESSIBLE, GtkScaleButtonAccessibleClass))
#define GTK_IS_SCALE_BUTTON_ACCESSIBLE(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SCALE_BUTTON_ACCESSIBLE))
#define GTK_IS_SCALE_BUTTON_ACCESSIBLE_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SCALE_BUTTON_ACCESSIBLE))
#define GTK_SCALE_BUTTON_ACCESSIBLE_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SCALE_BUTTON_ACCESSIBLE, GtkScaleButtonAccessibleClass))

typedef struct _GtkScaleButtonAccessible        GtkScaleButtonAccessible;
typedef struct _GtkScaleButtonAccessibleClass   GtkScaleButtonAccessibleClass;
typedef struct _GtkScaleButtonAccessiblePrivate GtkScaleButtonAccessiblePrivate;

struct _GtkScaleButtonAccessible
{
  GtkButtonAccessible parent;

  GtkScaleButtonAccessiblePrivate *priv;
};

struct _GtkScaleButtonAccessibleClass
{
  GtkButtonAccessibleClass parent_class;
};

GType _gtk_scale_button_accessible_get_type (void);

G_END_DECLS

#endif /* __GTK_SCALE_BUTTON_ACCESSIBLE_H__ */
