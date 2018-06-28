/*
 * Copyright © 2018 Benjamin Otte
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

#include "config.h"

#include "gtkpictureaccessibleprivate.h"

#include "gtkpicture.h"

struct _GtkPictureAccessible
{
  GtkWidgetAccessible parent_instance;
};

struct _GtkPictureAccessibleClass
{
  GtkWidgetAccessibleClass parent_class;
};

static const gchar *
gtk_picture_accessible_get_image_description (AtkImage *image)
{
  GtkWidget* widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (image));
  if (widget == NULL)
    return NULL;

  return gtk_picture_get_alternative_text (GTK_PICTURE (widget));
}

static void
gtk_picture_accessible_get_image_position (AtkImage     *image,
                                           gint         *x,
                                           gint         *y,
                                           AtkCoordType  coord_type)
{
  atk_component_get_extents (ATK_COMPONENT (image), x, y, NULL, NULL,
                             coord_type);
}

static void
gtk_picture_accessible_get_image_size (AtkImage *image,
                                       gint     *width,
                                       gint     *height)
{
  GtkWidget* widget;
  GdkPaintable *paintable;

  *width = -1;
  *height = -1;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (image));
  if (widget == NULL)
    return;

  paintable = gtk_picture_get_paintable (GTK_PICTURE (widget));
  if (paintable == NULL)
    return;

  *width = gdk_paintable_get_intrinsic_width (paintable);
  if (*width == 0)
    *width = -1;

  *height = gdk_paintable_get_intrinsic_height (paintable);
  if (*height == 0)
    *height = -1;
}

static void
gtk_picture_accessible_image_init (AtkImageIface *iface)
{
  iface->get_image_description = gtk_picture_accessible_get_image_description;
  iface->get_image_position = gtk_picture_accessible_get_image_position;
  iface->get_image_size = gtk_picture_accessible_get_image_size;
}

G_DEFINE_TYPE_WITH_CODE (GtkPictureAccessible, gtk_picture_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE, gtk_picture_accessible_image_init))

static void
gtk_picture_accessible_initialize (AtkObject *accessible,
                                 gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_picture_accessible_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_IMAGE;
}

static const gchar *
gtk_picture_accessible_get_name (AtkObject *accessible)
{
  GtkWidget* widget;
  const gchar *name;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return NULL;

  name = ATK_OBJECT_CLASS (gtk_picture_accessible_parent_class)->get_name (accessible);
  if (name)
    return name;

  return gtk_picture_get_alternative_text (GTK_PICTURE (widget));
}

static void
gtk_picture_accessible_class_init (GtkPictureAccessibleClass *klass)
{
  AtkObjectClass *atkobject_class = ATK_OBJECT_CLASS (klass);

  atkobject_class->initialize = gtk_picture_accessible_initialize;
  atkobject_class->get_name = gtk_picture_accessible_get_name;
}

static void
gtk_picture_accessible_init (GtkPictureAccessible *image)
{
}

