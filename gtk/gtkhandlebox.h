/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998 Elliot Lee
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

/* The GtkHandleBox is to allow widgets to be dragged in and out of
 * their parents.
 */


#ifndef __GTK_HANDLE_BOX_H__
#define __GTK_HANDLE_BOX_H__


#include <gdk/gdk.h>
#include <gtk/gtkbin.h>


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
  GtkBin bin;

  GdkWindow      *bin_window;	/* parent window for children */
  GdkWindow      *float_window;
  guint		  handle_position : 2;
  guint		  float_window_mapped : 1;
  guint		  child_detached : 1;
  guint		  in_drag : 1;
  guint		  shrink_on_detach : 1;
  GdkCursor      *fleur_cursor;

  gint dragoff_x, dragoff_y; /* start drag position (wrt widget->window) */
};

struct _GtkHandleBoxClass
{
  GtkBinClass parent_class;

  void	(*child_attached)	(GtkHandleBox	*handle_box,
				 GtkWidget	*child);
  void	(*child_detached)	(GtkHandleBox	*handle_box,
				 GtkWidget	*child);
};


guint          gtk_handle_box_get_type        (void);
GtkWidget*     gtk_handle_box_new             (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_HANDLE_BOX_H__ */
