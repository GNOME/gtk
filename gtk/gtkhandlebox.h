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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/* The GtkHandleBox is to allow widgets to be dragged in and out of
 their parents */
#ifndef __GTK_HANDLE_BOX_H__
#define __GTK_HANDLE_BOX_H__


#include <gdk/gdk.h>
#include <gtk/gtkeventbox.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_HANDLE_BOX(obj)          GTK_CHECK_CAST (obj, gtk_handle_box_get_type (), GtkHandleBox)
#define GTK_HANDLE_BOX_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_handle_box_get_type (), GtkHandleBoxClass)
#define GTK_IS_HANDLE_BOX(obj)       GTK_CHECK_TYPE (obj, gtk_handle_box_get_type ())


typedef struct _GtkHandleBox       GtkHandleBox;
typedef struct _GtkHandleBoxClass  GtkHandleBoxClass;

struct _GtkHandleBox
{
  GtkEventBox event_box;
  GtkWidget *real_parent;
  GtkRequisition real_requisition;
  gboolean is_being_dragged, is_onroot;
};

struct _GtkHandleBoxClass
{
  GtkEventBoxClass parent_class;
};

guint          gtk_handle_box_get_type        (void);
GtkWidget*     gtk_handle_box_new             (void);
/* the x and y coordinates (relative to root window, of course)
   are only needed if you pass in_root = TRUE */
void           gtk_handle_box_set_location    (GtkWidget *widget,
					       gboolean in_root,
					       gint x, gint y);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_HANDLE_BOX_H__ */
