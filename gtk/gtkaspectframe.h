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
#ifndef __GTK_ASPECT_FRAME_H__
#define __GTK_ASPECT_FRAME_H__


#include <gdk/gdk.h>
#include <gtk/gtkbin.h>
#include <gtk/gtkframe.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define GTK_TYPE_ASPECT_FRAME            (gtk_aspect_frame_get_type ())
#define GTK_ASPECT_FRAME(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_ASPECT_FRAME, GtkAspectFrame))
#define GTK_ASPECT_FRAME_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_ASPECT_FRAME, GtkAspectFrameClass))
#define GTK_IS_ASPECT_FRAME(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_ASPECT_FRAME))
#define GTK_IS_ASPECT_FRAME_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ASPECT_FRAME))



typedef struct _GtkAspectFrame       GtkAspectFrame;
typedef struct _GtkAspectFrameClass  GtkAspectFrameClass;

struct _GtkAspectFrame
{
  GtkFrame frame;

  gfloat xalign;
  gfloat yalign;
  gfloat ratio;
  gint obey_child;

  GtkAllocation center_allocation;
};

struct _GtkAspectFrameClass
{
  GtkBinClass parent_class;
};


GtkType    gtk_aspect_frame_get_type   (void);
GtkWidget* gtk_aspect_frame_new        (const gchar       *label,
					gfloat             xalign,
					gfloat             yalign,
					gfloat             ratio,
					gint               obey_child);
void       gtk_aspect_frame_set        (GtkAspectFrame      *aspect_frame,
					gfloat             xalign,
					gfloat             yalign,
					gfloat             ratio,
					gint               obey_child);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ASPECT_FRAME_H__ */
