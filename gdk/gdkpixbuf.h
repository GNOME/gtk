#ifndef __GDK_PIXBUF_H__
#define __GDK_PIXBUF_H__

#include <gdk/gdktypes.h>
#include <gdk/gdkrgb.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Rendering to a drawable */

void gdk_pixbuf_render_threshold_alpha   (GdkPixbuf           *pixbuf,
					  GdkBitmap           *bitmap,
					  int                  src_x,
					  int                  src_y,
					  int                  dest_x,
					  int                  dest_y,
					  int                  width,
					  int                  height,
					  int                  alpha_threshold);
#ifndef GDK_DISABLE_DEPRECATED
void gdk_pixbuf_render_to_drawable       (GdkPixbuf           *pixbuf,
					  GdkDrawable         *drawable,
					  GdkGC               *gc,
					  int                  src_x,
					  int                  src_y,
					  int                  dest_x,
					  int                  dest_y,
					  int                  width,
					  int                  height,
					  GdkRgbDither         dither,
					  int                  x_dither,
					  int                  y_dither);
void gdk_pixbuf_render_to_drawable_alpha (GdkPixbuf           *pixbuf,
					  GdkDrawable         *drawable,
					  int                  src_x,
					  int                  src_y,
					  int                  dest_x,
					  int                  dest_y,
					  int                  width,
					  int                  height,
					  GdkPixbufAlphaMode   alpha_mode,
					  int                  alpha_threshold,
					  GdkRgbDither         dither,
					  int                  x_dither,
					  int                  y_dither);
#endif /* GDK_DISABLE_DEPRECATED */
void gdk_pixbuf_render_pixmap_and_mask_for_colormap (GdkPixbuf    *pixbuf,
						     GdkColormap  *colormap,
						     GdkPixmap   **pixmap_return,
						     GdkBitmap   **mask_return,
						     int           alpha_threshold);
#ifndef GDK_MULTIHEAD_SAFE
void gdk_pixbuf_render_pixmap_and_mask              (GdkPixbuf    *pixbuf,
						     GdkPixmap   **pixmap_return,
						     GdkBitmap   **mask_return,
						     int           alpha_threshold);
#endif


/* Fetching a region from a drawable */
GdkPixbuf *gdk_pixbuf_get_from_drawable (GdkPixbuf   *dest,
					 GdkDrawable *src,
					 GdkColormap *cmap,
					 int          src_x,
					 int          src_y,
					 int          dest_x,
					 int          dest_y,
					 int          width,
					 int          height);

GdkPixbuf *gdk_pixbuf_get_from_image    (GdkPixbuf   *dest,
                                         GdkImage    *src,
                                         GdkColormap *cmap,
                                         int          src_x,
                                         int          src_y,
                                         int          dest_x,
                                         int          dest_y,
                                         int          width,
                                         int          height);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_PIXBUF_H__ */
