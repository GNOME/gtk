/* GdkPixbuf library - Main header file
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
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

#ifndef GDK_PIXBUF_H
#define GDK_PIXBUF_H

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_pixbuf.h>
#include <gdk/gdk.h>

#ifdef __cplusplus
extern "C" {
#endif



/* GdkPixbuf structures */
typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GdkPixbufFrame GdkPixbufFrame;
typedef struct _GdkPixbufAnimation GdkPixbufAnimation;

struct _GdkPixbuf {
	/* Reference count */
	int ref_count;

	/* Libart pixbuf */
	ArtPixBuf *art_pixbuf;
};

/* GIF-like animation overlay modes for frames */
typedef enum {
	GDK_PIXBUF_FRAME_RETAIN,
	GDK_PIXBUF_FRAME_DISPOSE,
	GDK_PIXBUF_FRAME_REVERT
} GdkPixbufFrameAction;

struct _GdkPixbufFrame {
	/* The pixbuf with this frame's image data */
	GdkPixbuf *pixbuf;

	/* Offsets for overlaying onto the animation's area */
	int x_offset;
	int y_offset;

	/* Frame duration in ms */
	int delay_time;

	/* Overlay mode */
	GdkPixbufFrameAction action;
};

struct _GdkPixbufAnimation {
	/* Reference count */
	int ref_count;

	/* Number of frames */
        int n_frames;

	/* List of GdkPixbufFrame structures */
        GList *frames;
};



/* Convenience functions */

ArtPixFormat gdk_pixbuf_get_format          (GdkPixbuf *pixbuf);
int          gdk_pixbuf_get_n_channels      (GdkPixbuf *pixbuf);
int          gdk_pixbuf_get_has_alpha       (GdkPixbuf *pixbuf);
int          gdk_pixbuf_get_bits_per_sample (GdkPixbuf *pixbuf);
guchar      *gdk_pixbuf_get_pixels          (GdkPixbuf *pixbuf);
int          gdk_pixbuf_get_width           (GdkPixbuf *pixbuf);
int          gdk_pixbuf_get_height          (GdkPixbuf *pixbuf);
int          gdk_pixbuf_get_rowstride       (GdkPixbuf *pixbuf);

/* Reference counting */

void gdk_pixbuf_ref (GdkPixbuf *pixbuf);
void gdk_pixbuf_unref (GdkPixbuf *pixbuf);

/* Wrap a libart pixbuf */
GdkPixbuf *gdk_pixbuf_new_from_art_pixbuf (ArtPixBuf *art_pixbuf);

/* Create a blank pixbuf with an optimal rowstride and a new buffer */
GdkPixbuf *gdk_pixbuf_new (ArtPixFormat format, gboolean has_alpha, int bits_per_sample,
			   int width, int height);

/* Simple loading */

GdkPixbuf *gdk_pixbuf_new_from_file (const char *filename);

GdkPixbuf *gdk_pixbuf_new_from_data (guchar *data,
				     ArtPixFormat format,
				     gboolean has_alpha,
				     int width, int height,
				     int rowstride,
				     ArtDestroyNotify dfunc,
				     gpointer dfunc_data);

GdkPixbuf *gdk_pixbuf_new_from_xpm_data (const char **data);

/* Adding an alpha channel */
GdkPixbuf *gdk_pixbuf_add_alpha (GdkPixbuf *pixbuf, gboolean substitute_color,
				 guchar r, guchar g, guchar b);

/* Rendering to a drawable */

/* Alpha compositing mode */
typedef enum {
	GDK_PIXBUF_ALPHA_BILEVEL,
	GDK_PIXBUF_ALPHA_FULL
} GdkPixbufAlphaMode;

void gdk_pixbuf_render_threshold_alpha (GdkPixbuf *pixbuf, GdkBitmap *bitmap,
					int src_x, int src_y,
					int dest_x, int dest_y,
					int width, int height,
					int alpha_threshold);

void gdk_pixbuf_render_to_drawable (GdkPixbuf *pixbuf,
				    GdkDrawable *drawable, GdkGC *gc,
				    int src_x, int src_y,
				    int dest_x, int dest_y,
				    int width, int height,
				    GdkRgbDither dither,
				    int x_dither, int y_dither);

void gdk_pixbuf_render_to_drawable_alpha (GdkPixbuf *pixbuf, GdkDrawable *drawable,
					  int src_x, int src_y,
					  int dest_x, int dest_y,
					  int width, int height,
					  GdkPixbufAlphaMode alpha_mode,
					  int alpha_threshold,
					  GdkRgbDither dither,
					  int x_dither, int y_dither);

/* Fetching a region from a drawable */
GdkPixbuf *gdk_pixbuf_get_from_drawable (GdkPixbuf *dest,
					 GdkDrawable *src, GdkColormap *cmap,
					 int src_x, int src_y,
					 int dest_x, int dest_y,
					 int width, int height);

/* Animation support */

GdkPixbufAnimation *gdk_pixbuf_animation_new_from_file (const char *filename);

void gdk_pixbuf_animation_ref (GdkPixbufAnimation *animation);
void gdk_pixbuf_animation_unref (GdkPixbufAnimation *animation);



#ifdef __cplusplus
}
#endif

#endif
