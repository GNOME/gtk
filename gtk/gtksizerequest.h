/* GTK - The GIMP Toolkit
 * Copyright (C) 2007-2010 Openismus GmbH
 *
 * Authors:
 *      Mathias Hasselmann <mathias@openismus.com>
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
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

#ifndef __GTK_SIZE_REQUEST_H__
#define __GTK_SIZE_REQUEST_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_SIZE_REQUEST            (gtk_size_request_get_type ())
#define GTK_SIZE_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SIZE_REQUEST, GtkSizeRequest))
#define GTK_SIZE_REQUEST_CLASS(klass)    ((GtkSizeRequestIface*)g_type_interface_peek ((klass), GTK_TYPE_SIZE_REQUEST))
#define GTK_IS_SIZE_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SIZE_REQUEST))
#define GTK_SIZE_REQUEST_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GTK_TYPE_SIZE_REQUEST, GtkSizeRequestIface))

typedef struct _GtkSizeRequest           GtkSizeRequest;
typedef struct _GtkSizeRequestIface      GtkSizeRequestIface;


struct _GtkSizeRequestIface
{
  GTypeInterface g_iface;

  /* virtual table */
  GtkSizeRequestMode (* get_request_mode)     (GtkSizeRequest  *widget);

  void               (* get_height)           (GtkSizeRequest  *widget,
					       gint            *minimum_height,
					       gint            *natural_height);
  void               (* get_width_for_height) (GtkSizeRequest  *widget,
					       gint             height,
					       gint            *minimum_width,
					       gint            *natural_width);
  void               (* get_width)            (GtkSizeRequest  *widget,
					       gint            *minimum_width,
					       gint            *natural_width);
  void               (* get_height_for_width) (GtkSizeRequest  *widget,
					       gint             width,
					       gint            *minimum_height,
					       gint            *natural_height);
};

GType               gtk_size_request_get_type             (void) G_GNUC_CONST;

GtkSizeRequestMode  gtk_size_request_get_request_mode     (GtkSizeRequest *widget);
void                gtk_size_request_get_width            (GtkSizeRequest *widget,
							   gint           *minimum_width,
							   gint           *natural_width);
void                gtk_size_request_get_height_for_width (GtkSizeRequest *widget,
							   gint            width,
							   gint           *minimum_height,
							   gint           *natural_height);
void                gtk_size_request_get_height           (GtkSizeRequest *widget,
							   gint           *minimum_height,
							   gint           *natural_height);
void                gtk_size_request_get_width_for_height (GtkSizeRequest *widget,
							   gint            height,
							   gint           *minimum_width,
							   gint           *natural_width);
void                gtk_size_request_get_size             (GtkSizeRequest *widget,
							   GtkRequisition *minimum_size,
							   GtkRequisition *natural_size);

G_END_DECLS

#endif /* __GTK_SIZE_REQUEST_H__ */
