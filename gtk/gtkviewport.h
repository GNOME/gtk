/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
#ifndef __GTK_VIEWPORT_H__
#define __GTK_VIEWPORT_H__


#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkbin.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_VIEWPORT(obj)          GTK_CHECK_CAST (obj, gtk_viewport_get_type (), GtkViewport)
#define GTK_VIEWPORT_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_viewport_get_type (), GtkViewportClass)
#define GTK_IS_VIEWPORT(obj)       GTK_CHECK_TYPE (obj, gtk_viewport_get_type ())


typedef struct _GtkViewport       GtkViewport;
typedef struct _GtkViewportClass  GtkViewportClass;

struct _GtkViewport
{
  GtkBin bin;

  gint shadow_type;
  GdkWindow *view_window;
  GdkWindow *bin_window;
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
};

struct _GtkViewportClass
{
  GtkBinClass parent_class;
};


guint          gtk_viewport_get_type        (void);
GtkWidget*     gtk_viewport_new             (GtkAdjustment *hadjustment,
					     GtkAdjustment *vadjustment);
GtkAdjustment* gtk_viewport_get_hadjustment (GtkViewport   *viewport);
GtkAdjustment* gtk_viewport_get_vadjustment (GtkViewport   *viewport);
void           gtk_viewport_set_hadjustment (GtkViewport   *viewport,
					     GtkAdjustment *adjustment);
void           gtk_viewport_set_vadjustment (GtkViewport   *viewport,
					     GtkAdjustment *adjustment);
void           gtk_viewport_set_shadow_type (GtkViewport   *viewport,
					     GtkShadowType  type);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_VIEWPORT_H__ */
