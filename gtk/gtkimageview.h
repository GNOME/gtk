/*  Copyright 2016 Timm Bäder
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_IMAGE_VIEW_H__
#define __GTK_IMAGE_VIEW_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>
#include "gtkabstractimage.h"


G_BEGIN_DECLS


#define GTK_TYPE_IMAGE_VIEW            (gtk_image_view_get_type ())
#define GTK_IMAGE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_IMAGE_VIEW, GtkImageView))
#define GTK_IMAGE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_IMAGE_VIEW, GtkImageViewClass))
#define GTK_IS_IMAGE_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_IMAGE_VIEW))
#define GTK_IS_IMAGE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IMAGE_VIEW))
#define GTK_IMAGE_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_IMAGE_VIEW, GtkImageViewClass))


typedef struct _GtkImageView         GtkImageView;
typedef struct _GtkImageViewPrivate  GtkImageViewPrivate;
typedef struct _GtkImageViewClass    GtkImageViewClass;


struct _GtkImageView
{
  GtkWidget parent_instance;
};

struct _GtkImageViewClass
{
  GtkWidgetClass parent_class;
};


GDK_AVAILABLE_IN_3_20
GType         gtk_image_view_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_20
GtkWidget *   gtk_image_view_new      (void);

GDK_AVAILABLE_IN_3_20
void gtk_image_view_set_pixbuf (GtkImageView    *image_view,
                                const GdkPixbuf *pixbuf,
                                int              scale_factor);

GDK_AVAILABLE_IN_3_20
void gtk_image_view_set_surface (GtkImageView    *image_view,
                                 cairo_surface_t *surface);

GDK_AVAILABLE_IN_3_20
void gtk_image_view_set_animation (GtkImageView       *image_view,
                                   GdkPixbufAnimation *animation,
                                   int                 scale_factor);


GDK_AVAILABLE_IN_3_20
void gtk_image_view_load_from_file_async (GtkImageView        *image_view,
                                          GFile               *file,
                                          int                  scale_factor,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data);
GDK_AVAILABLE_IN_3_20
gboolean gtk_image_view_load_from_file_finish (GtkImageView  *image_view,
                                               GAsyncResult  *result,
                                               GError       **error);
GDK_AVAILABLE_IN_3_20
void gtk_image_view_load_from_stream_async (GtkImageView        *image_view,
                                            GInputStream        *input_stream,
                                            int                  scale_factor,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data);

GDK_AVAILABLE_IN_3_20
gboolean gtk_image_view_load_from_stream_finish (GtkImageView  *image_view,
                                                 GAsyncResult  *result,
                                                 GError       **error);

GDK_AVAILABLE_IN_3_20
void   gtk_image_view_set_scale (GtkImageView *image_view,
                                 double        scale);

GDK_AVAILABLE_IN_3_20
double gtk_image_view_get_scale (GtkImageView *image_view);

GDK_AVAILABLE_IN_3_20
void  gtk_image_view_set_angle (GtkImageView *image_view,
                                double        angle);

GDK_AVAILABLE_IN_3_20
double gtk_image_view_get_angle (GtkImageView *image_view);

GDK_AVAILABLE_IN_3_20
void gtk_image_view_set_snap_angle (GtkImageView *image_view,
                                    gboolean      snap_angle);

GDK_AVAILABLE_IN_3_20
gboolean gtk_image_view_get_snap_angle (GtkImageView *image_view);

GDK_AVAILABLE_IN_3_20
void gtk_image_view_set_fit_allocation (GtkImageView *image_view,
                                        gboolean      fit_allocation);

GDK_AVAILABLE_IN_3_20
gboolean gtk_image_view_get_fit_allocation (GtkImageView *image_view);

GDK_AVAILABLE_IN_3_20
void gtk_image_view_set_rotatable (GtkImageView *image_view,
                                   gboolean      rotatable);

GDK_AVAILABLE_IN_3_20
gboolean gtk_image_view_get_rotatable (GtkImageView *image_view);

GDK_AVAILABLE_IN_3_20
void gtk_image_view_set_zoomable (GtkImageView *image_view,
                                  gboolean      zoomable);

GDK_AVAILABLE_IN_3_20
gboolean gtk_image_view_get_zoomable (GtkImageView *image_view);

GDK_AVAILABLE_IN_3_20
gboolean gtk_image_view_get_scale_set (GtkImageView *image_view);

GDK_AVAILABLE_IN_3_20
void gtk_image_view_set_transitions_enabled (GtkImageView *image_view,
                                             gboolean      transitions_enabled);

GDK_AVAILABLE_IN_3_20
gboolean gtk_image_view_get_transitions_enabled (GtkImageView *image_view);

GDK_AVAILABLE_IN_3_20
void gtk_image_view_set_abstract_image (GtkImageView *image_view, GtkAbstractImage *abstract_image);

G_END_DECLS

#endif
