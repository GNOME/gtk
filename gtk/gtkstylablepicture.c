/* gtktreesortable.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include "config.h"

#include "gtkstylablepicture.h"
#include "gtkintl.h"


/**
 * SECTION:gtkstylablepicture
 * @Short_description: Pictures that can be styled when attached to widgets
 * @Title: GtkStylablePicture
 * @See_also:#GdkPicture, #GtkWidget
 *
 * #GtkStylablePicture is an interface to be implemented by pictures that can be
 * styled according to a #GtkWidget's #GtkStyleContext. 
 */

G_DEFINE_INTERFACE (GtkStylablePicture, gtk_stylable_picture, GDK_TYPE_PICTURE)

static void
gtk_stylable_picture_default_init (GtkStylablePictureInterface *iface)
{
}

GdkPicture *
gtk_widget_style_picture (GtkWidget  *widget,
                          GdkPicture *picture)
{
  GtkStylablePictureInterface *iface;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (GDK_IS_PICTURE (picture), NULL);

  if (!GTK_IS_STYLABLE_PICTURE (picture))
    return g_object_ref (picture);

  iface = GTK_STYLABLE_PICTURE_GET_IFACE (picture);
  if (iface->attach == NULL)
    return g_object_ref (picture);

  return (* iface->attach) (picture, widget);
}

GdkPicture *
gtk_picture_get_unstyled (GdkPicture *styled)
{
  GtkStylablePictureInterface *iface;

  g_return_val_if_fail (GDK_IS_PICTURE (styled), NULL);

  if (!GTK_IS_STYLABLE_PICTURE (styled))
    return styled;

  iface = GTK_STYLABLE_PICTURE_GET_IFACE (styled);
  if (iface->get_unstyled == NULL)
    return styled;

  return iface->get_unstyled (styled);
}
