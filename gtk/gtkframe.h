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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_FRAME_H__
#define __GTK_FRAME_H__


#include <gdk/gdk.h>
#include <gtk/gtkbin.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_FRAME                  (gtk_frame_get_type ())
#define GTK_FRAME(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_FRAME, GtkFrame))
#define GTK_FRAME_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_FRAME, GtkFrameClass))
#define GTK_IS_FRAME(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_FRAME))
#define GTK_IS_FRAME_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FRAME))


typedef struct _GtkFrame       GtkFrame;
typedef struct _GtkFrameClass  GtkFrameClass;

struct _GtkFrame
{
  GtkBin bin;
  
  gchar *label;
  gint16 shadow_type;
  gint16 label_width;
  gint16 label_height;
  gfloat label_xalign;
  gfloat label_yalign;
};

struct _GtkFrameClass
{
  GtkBinClass parent_class;
};


GtkType    gtk_frame_get_type        (void);
GtkWidget* gtk_frame_new             (const gchar   *label);
void       gtk_frame_set_label       (GtkFrame      *frame,
				      const gchar   *label);
void       gtk_frame_set_label_align (GtkFrame      *frame,
				      gfloat         xalign,
				      gfloat         yalign);
void       gtk_frame_set_shadow_type (GtkFrame      *frame,
				      GtkShadowType  type);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_FRAME_H__ */
