/* GdkPixbuf library - Main header file
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Havoc Pennington <hp@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef GDK_PIXBUF_H
#define GDK_PIXBUF_H

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf-features.h>
#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif



/* Alpha compositing mode */
typedef enum
{
        GDK_PIXBUF_ALPHA_BILEVEL,
        GDK_PIXBUF_ALPHA_FULL
} GdkPixbufAlphaMode;

/* Color spaces; right now only RGB is supported.
 * Note that these values are encoded in inline pixbufs
 * as ints, so don't reorder them
 */
typedef enum {
	GDK_COLORSPACE_RGB
} GdkColorspace;

/* All of these are opaque structures */
typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GdkPixbufAnimation GdkPixbufAnimation;
typedef struct _GdkPixbufAnimationIter GdkPixbufAnimationIter;
typedef struct _GdkPixbufFrame GdkPixbufFrame;

#define GDK_TYPE_PIXBUF              (gdk_pixbuf_get_type ())
#define GDK_PIXBUF(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_PIXBUF, GdkPixbuf))
#define GDK_IS_PIXBUF(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXBUF))

#define GDK_TYPE_PIXBUF_ANIMATION              (gdk_pixbuf_animation_get_type ())
#define GDK_PIXBUF_ANIMATION(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_PIXBUF_ANIMATION, GdkPixbufAnimation))
#define GDK_IS_PIXBUF_ANIMATION(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXBUF_ANIMATION))

#define GDK_TYPE_PIXBUF_ANIMATION_ITER              (gdk_pixbuf_animation_iter_get_type ())
#define GDK_PIXBUF_ANIMATION_ITER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_PIXBUF_ANIMATION_ITER, GdkPixbufAnimationIter))
#define GDK_IS_PIXBUF_ANIMATION_ITER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXBUF_ANIMATION_ITER))

/* Handler that must free the pixel array */
typedef void (* GdkPixbufDestroyNotify) (guchar *pixels, gpointer data);

#define GDK_PIXBUF_ERROR gdk_pixbuf_error_quark ()

typedef enum {
        /* image data hosed */
        GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
        /* no mem to load image */
        GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
        /* bad option value passed to save routine */
        GDK_PIXBUF_ERROR_BAD_OPTION_VALUE,
        /* unsupported image type (sort of an ENOSYS) */
        GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
        /* unsupported operation (load, save) for image type */
        GDK_PIXBUF_ERROR_UNSUPPORTED_OPERATION,
        GDK_PIXBUF_ERROR_FAILED
} GdkPixbufError;

GQuark gdk_pixbuf_error_quark (void) G_GNUC_CONST;



GType gdk_pixbuf_get_type (void) G_GNUC_CONST;

/* Reference counting */

GdkPixbuf *gdk_pixbuf_ref      (GdkPixbuf *pixbuf);
void       gdk_pixbuf_unref    (GdkPixbuf *pixbuf);

/* GdkPixbuf accessors */

GdkColorspace gdk_pixbuf_get_colorspace      (const GdkPixbuf *pixbuf);
int           gdk_pixbuf_get_n_channels      (const GdkPixbuf *pixbuf);
gboolean      gdk_pixbuf_get_has_alpha       (const GdkPixbuf *pixbuf);
int           gdk_pixbuf_get_bits_per_sample (const GdkPixbuf *pixbuf);
guchar       *gdk_pixbuf_get_pixels          (const GdkPixbuf *pixbuf);
int           gdk_pixbuf_get_width           (const GdkPixbuf *pixbuf);
int           gdk_pixbuf_get_height          (const GdkPixbuf *pixbuf);
int           gdk_pixbuf_get_rowstride       (const GdkPixbuf *pixbuf);



/* Create a blank pixbuf with an optimal rowstride and a new buffer */
GdkPixbuf *gdk_pixbuf_new (GdkColorspace colorspace, gboolean has_alpha, int bits_per_sample,
			   int width, int height);

/* Copy a pixbuf */

GdkPixbuf *gdk_pixbuf_copy (const GdkPixbuf *pixbuf);

/* Create a pixbuf which points to the pixels of another pixbuf */
GdkPixbuf *gdk_pixbuf_new_subpixbuf (GdkPixbuf *src_pixbuf,
                                     int        src_x,
                                     int        src_y,
                                     int        width,
                                     int        height);

/* Simple loading */

GdkPixbuf *gdk_pixbuf_new_from_file (const char *filename,
                                     GError    **error);

GdkPixbuf *gdk_pixbuf_new_from_data (const guchar *data,
				     GdkColorspace colorspace,
				     gboolean has_alpha,
				     int bits_per_sample,
				     int width, int height,
				     int rowstride,
				     GdkPixbufDestroyNotify destroy_fn,
				     gpointer destroy_fn_data);

GdkPixbuf *gdk_pixbuf_new_from_xpm_data (const char **data);

/* Read an inline pixbuf */
GdkPixbuf *gdk_pixbuf_new_from_inline   (const guchar *inline_pixbuf,
                                         gboolean      copy_pixels,
                                         int           length,
                                         GError      **error);

/* Mutations */
void       gdk_pixbuf_fill              (GdkPixbuf    *pixbuf,
                                         guint32       pixel);

/* Saving */

gboolean gdk_pixbuf_save           (GdkPixbuf  *pixbuf, 
                                    const char *filename, 
                                    const char *type, 
                                    GError    **error,
                                    ...);

gboolean gdk_pixbuf_savev          (GdkPixbuf  *pixbuf, 
                                    const char *filename, 
                                    const char *type,
                                    char      **option_keys,
                                    char      **option_values,
                                    GError    **error);

/* Adding an alpha channel */
GdkPixbuf *gdk_pixbuf_add_alpha (const GdkPixbuf *pixbuf, gboolean substitute_color,
				 guchar r, guchar g, guchar b);

/* Copy an area of a pixbuf onto another one */
void gdk_pixbuf_copy_area (const GdkPixbuf *src_pixbuf,
			   int src_x, int src_y,
			   int width, int height,
			   GdkPixbuf *dest_pixbuf,
			   int dest_x, int dest_y);

/* Brighten/darken and optionally make it pixelated-looking */
void gdk_pixbuf_saturate_and_pixelate (const GdkPixbuf *src,
                                       GdkPixbuf       *dest,
                                       gfloat           saturation,
                                       gboolean         pixelate);



/* Rendering to a drawable */


/* Scaling */

/* Interpolation modes */
typedef enum {
	GDK_INTERP_NEAREST,
	GDK_INTERP_TILES,
	GDK_INTERP_BILINEAR,
	GDK_INTERP_HYPER
} GdkInterpType;

void gdk_pixbuf_scale           (const GdkPixbuf *src,
				 GdkPixbuf       *dest,
				 int              dest_x,
				 int              dest_y,
				 int              dest_width,
				 int              dest_height,
				 double           offset_x,
				 double           offset_y,
				 double           scale_x,
				 double           scale_y,
				 GdkInterpType    interp_type);
void gdk_pixbuf_composite       (const GdkPixbuf *src,
				 GdkPixbuf       *dest,
				 int              dest_x,
				 int              dest_y,
				 int              dest_width,
				 int              dest_height,
				 double           offset_x,
				 double           offset_y,
				 double           scale_x,
				 double           scale_y,
				 GdkInterpType    interp_type,
				 int              overall_alpha);
void gdk_pixbuf_composite_color (const GdkPixbuf *src,
				 GdkPixbuf       *dest,
				 int              dest_x,
				 int              dest_y,
				 int              dest_width,
				 int              dest_height,
				 double           offset_x,
				 double           offset_y,
				 double           scale_x,
				 double           scale_y,
				 GdkInterpType    interp_type,
				 int              overall_alpha,
				 int              check_x,
				 int              check_y,
				 int              check_size,
				 guint32          color1,
				 guint32          color2);

GdkPixbuf *gdk_pixbuf_scale_simple           (const GdkPixbuf *src,
					      int              dest_width,
					      int              dest_height,
					      GdkInterpType    interp_type);

GdkPixbuf *gdk_pixbuf_composite_color_simple (const GdkPixbuf *src,
					      int              dest_width,
					      int              dest_height,
					      GdkInterpType    interp_type,
					      int              overall_alpha,
					      int              check_size,
					      guint32          color1,
					      guint32          color2);



/* Animation support */

GType               gdk_pixbuf_animation_get_type        (void) G_GNUC_CONST;

GdkPixbufAnimation *gdk_pixbuf_animation_new_from_file   (const char         *filename,
                                                          GError            **error);

GdkPixbufAnimation *gdk_pixbuf_animation_ref             (GdkPixbufAnimation *animation);
void                gdk_pixbuf_animation_unref           (GdkPixbufAnimation *animation);

int                 gdk_pixbuf_animation_get_width       (GdkPixbufAnimation *animation);
int                 gdk_pixbuf_animation_get_height      (GdkPixbufAnimation *animation);
gboolean            gdk_pixbuf_animation_is_static_image  (GdkPixbufAnimation *animation);
GdkPixbuf          *gdk_pixbuf_animation_get_static_image (GdkPixbufAnimation *animation);

GdkPixbufAnimationIter *gdk_pixbuf_animation_get_iter                        (GdkPixbufAnimation     *animation,
                                                                              const GTimeVal         *start_time);
GType                   gdk_pixbuf_animation_iter_get_type                   (void) G_GNUC_CONST;
int                     gdk_pixbuf_animation_iter_get_delay_time             (GdkPixbufAnimationIter *iter);
GdkPixbuf              *gdk_pixbuf_animation_iter_get_pixbuf                 (GdkPixbufAnimationIter *iter);
gboolean                gdk_pixbuf_animation_iter_on_currently_loading_frame (GdkPixbufAnimationIter *iter);
gboolean                gdk_pixbuf_animation_iter_advance                    (GdkPixbufAnimationIter *iter,
                                                                              const GTimeVal         *current_time);


 
#include <gdk-pixbuf/gdk-pixbuf-loader.h>



#ifdef __cplusplus
}
#endif

#endif
