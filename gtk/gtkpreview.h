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
#ifndef __GTK_PREVIEW_H__
#define __GTK_PREVIEW_H__


#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_PREVIEW(obj)          GTK_CHECK_CAST (obj, gtk_preview_get_type (), GtkPreview)
#define GTK_PREVIEW_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_preview_get_type (), GtkPreviewClass)
#define GTK_IS_PREVIEW(obj)       GTK_CHECK_TYPE (obj, gtk_preview_get_type ())


typedef struct _GtkPreview       GtkPreview;
typedef struct _GtkPreviewInfo   GtkPreviewInfo;
typedef union  _GtkDitherInfo    GtkDitherInfo;
typedef struct _GtkPreviewClass  GtkPreviewClass;

struct _GtkPreview
{
  GtkWidget widget;

  guchar *buffer;
  guint16 buffer_width;
  guint16 buffer_height;

  guint type : 1;
  guint expand : 1;
};

struct _GtkPreviewInfo
{
  GdkVisual *visual;
  GdkColormap *cmap;

  gulong *color_pixels;
  gulong *gray_pixels;
  gulong *reserved_pixels;

  gulong *lookup_red;
  gulong *lookup_green;
  gulong *lookup_blue;

  GtkDitherInfo *dither_red;
  GtkDitherInfo *dither_green;
  GtkDitherInfo *dither_blue;
  GtkDitherInfo *dither_gray;
  guchar ***dither_matrix;

  guint nred_shades;
  guint ngreen_shades;
  guint nblue_shades;
  guint ngray_shades;
  guint nreserved;

  guint bpp;
  gint cmap_alloced;
  gdouble gamma;
};

union _GtkDitherInfo
{
  gushort s[2];
  guchar c[4];
};

struct _GtkPreviewClass
{
  GtkWidgetClass parent_class;

  GtkPreviewInfo info;

  GdkImage *image;
};


guint           gtk_preview_get_type           (void);
void            gtk_preview_uninit             (void);
GtkWidget*      gtk_preview_new                (GtkPreviewType   type);
void            gtk_preview_size               (GtkPreview      *preview,
						gint             width,
						gint             height);
void            gtk_preview_put                (GtkPreview      *preview,
						GdkWindow       *window,
						GdkGC           *gc,
						gint             srcx,
						gint             srcy,
						gint             destx,
						gint             desty,
						gint             width,
						gint             height);
void            gtk_preview_put_row            (GtkPreview      *preview,
						guchar          *src,
						guchar          *dest,
						gint             x,
						gint             y,
						gint             w);
void            gtk_preview_draw_row           (GtkPreview      *preview,
						guchar          *data,
						gint             x,
						gint             y,
						gint             w);
void            gtk_preview_set_expand         (GtkPreview      *preview,
						gint             expand);

void            gtk_preview_set_gamma          (double           gamma);
void            gtk_preview_set_color_cube     (guint            nred_shades,
						guint            ngreen_shades,
						guint            nblue_shades,
						guint            ngray_shades);
void            gtk_preview_set_install_cmap   (gint             install_cmap);
void            gtk_preview_set_reserved       (gint             nreserved);
GdkVisual*      gtk_preview_get_visual         (void);
GdkColormap*    gtk_preview_get_cmap           (void);
GtkPreviewInfo* gtk_preview_get_info           (void);

/* This function reinitializes the preview colormap and visual from
 * the current gamma/color_cube/install_cmap settings. It must only
 * be called if there are no previews or users's of the preview
 * colormap in existence.
 */
void            gtk_preview_reset              (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_PREVIEW_H__ */
