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

#ifndef __GTK_PROGRESS_H__
#define __GTK_PROGRESS_H__


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkadjustment.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_PROGRESS            (gtk_progress_get_type ())
#define GTK_PROGRESS(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_PROGRESS, GtkProgress))
#define GTK_PROGRESS_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_PROGRESS, GtkProgressClass))
#define GTK_IS_PROGRESS(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_PROGRESS))
#define GTK_IS_PROGRESS_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PROGRESS))


typedef struct _GtkProgress       GtkProgress;
typedef struct _GtkProgressClass  GtkProgressClass;


struct _GtkProgress
{
  GtkWidget widget;

  GtkAdjustment *adjustment;
  GdkPixmap     *offscreen_pixmap;
  gchar         *format;
  gfloat         x_align;
  gfloat         y_align;

  guint          show_text : 1;
  guint          activity_mode : 1;
};

struct _GtkProgressClass
{
  GtkWidgetClass parent_class;

  void (* paint)            (GtkProgress *progress);
  void (* update)           (GtkProgress *progress);
  void (* act_mode_enter)   (GtkProgress *progress);
};


GtkType    gtk_progress_get_type            (void);
void       gtk_progress_set_show_text       (GtkProgress   *progress,
					     gint           show_text);
void       gtk_progress_set_text_alignment  (GtkProgress   *progress,
					     gfloat         x_align,
					     gfloat         y_align);
void       gtk_progress_set_format_string   (GtkProgress   *progress,
					     const gchar   *format);
void       gtk_progress_set_adjustment      (GtkProgress   *progress,
					     GtkAdjustment *adjustment);
void       gtk_progress_configure           (GtkProgress   *progress,
					     gfloat         value,
					     gfloat         min,
					     gfloat         max);
void       gtk_progress_set_percentage      (GtkProgress   *progress,
					     gfloat         percentage);
void       gtk_progress_set_value           (GtkProgress   *progress,
					     gfloat         value);
gfloat     gtk_progress_get_value           (GtkProgress   *progress);
void       gtk_progress_set_activity_mode   (GtkProgress   *progress,
					     guint          activity_mode);
gchar*     gtk_progress_get_current_text    (GtkProgress   *progress);
gchar*     gtk_progress_get_text_from_value (GtkProgress   *progress,
					     gfloat         value);
gfloat     gtk_progress_get_current_percentage (GtkProgress *progress);
gfloat     gtk_progress_get_percentage_from_value (GtkProgress *progress,
						   gfloat       value);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_PROGRESS_H__ */
