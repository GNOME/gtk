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
#include <gobject/gobject.h>

#ifdef __cplusplus
extern "C" {
#endif



/* Color spaces; right now only RGB is supported */
typedef enum {
	GDK_COLORSPACE_RGB
} GdkColorspace;

/* All of these are opaque structures */
typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GdkPixbufAnimation GdkPixbufAnimation;
typedef struct _GdkPixbufFrame GdkPixbufFrame;

#define GDK_TYPE_PIXBUF              (gdk_pixbuf_get_type ())
#define GDK_PIXBUF(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_PIXBUF, GdkPixbuf))
#define GDK_IS_PIXBUF(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXBUF))

#define GDK_TYPE_PIXBUF_ANIMATION              (gdk_pixbuf_animation_get_type ())
#define GDK_PIXBUF_ANIMATION(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_PIXBUF_ANIMATION, GdkPixbufAnimation))
#define GDK_IS_PIXBUF_ANIMATION(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXBUF_ANIMATION))


/* Handler that must free the pixel array */
typedef void (* GdkPixbufDestroyNotify) (guchar *pixels, gpointer data);



GType gdk_pixbuf_get_type (void);

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

/* Simple loading */

GdkPixbuf *gdk_pixbuf_new_from_file (const char *filename);

GdkPixbuf *gdk_pixbuf_new_from_data (const guchar *data,
				     GdkColorspace colorspace,
				     gboolean has_alpha,
				     int bits_per_sample,
				     int width, int height,
				     int rowstride,
				     GdkPixbufDestroyNotify destroy_fn,
				     gpointer destroy_fn_data);

GdkPixbuf *gdk_pixbuf_new_from_xpm_data (const char **data);

/* Adding an alpha channel */
GdkPixbuf *gdk_pixbuf_add_alpha (const GdkPixbuf *pixbuf, gboolean substitute_color,
				 guchar r, guchar g, guchar b);

/* Copy an area of a pixbuf onto another one */
void gdk_pixbuf_copy_area (const GdkPixbuf *src_pixbuf,
			   int src_x, int src_y,
			   int width, int height,
			   GdkPixbuf *dest_pixbuf,
			   int dest_x, int dest_y);



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

/* GIF-like animation overlay modes for frames */
typedef enum {
	GDK_PIXBUF_FRAME_RETAIN,
	GDK_PIXBUF_FRAME_DISPOSE,
	GDK_PIXBUF_FRAME_REVERT
} GdkPixbufFrameAction;

GType               gdk_pixbuf_animation_get_type        (void);

GdkPixbufAnimation *gdk_pixbuf_animation_new_from_file   (const char         *filename);

GdkPixbufAnimation *gdk_pixbuf_animation_ref             (GdkPixbufAnimation *animation);
void                gdk_pixbuf_animation_unref           (GdkPixbufAnimation *animation);

int                 gdk_pixbuf_animation_get_width       (GdkPixbufAnimation *animation);
int                 gdk_pixbuf_animation_get_height      (GdkPixbufAnimation *animation);
GList              *gdk_pixbuf_animation_get_frames      (GdkPixbufAnimation *animation);
int                 gdk_pixbuf_animation_get_num_frames  (GdkPixbufAnimation *animation);

/* Frame accessors */

GdkPixbuf           *gdk_pixbuf_frame_get_pixbuf     (GdkPixbufFrame *frame);
int                  gdk_pixbuf_frame_get_x_offset   (GdkPixbufFrame *frame);
int                  gdk_pixbuf_frame_get_y_offset   (GdkPixbufFrame *frame);
int                  gdk_pixbuf_frame_get_delay_time (GdkPixbufFrame *frame);
GdkPixbufFrameAction gdk_pixbuf_frame_get_action     (GdkPixbufFrame *frame);


/* General (presently empty) initialization hooks, primarily for gnome-libs */
void gdk_pixbuf_preinit  (gpointer app, gpointer modinfo);
void gdk_pixbuf_postinit (gpointer app, gpointer modinfo);
/* A more user-friendly init function */
void gdk_pixbuf_init     (void);



#ifdef __cplusplus
}
#endif

#endif
