#ifndef __GDK_PIXMAP_H__
#define __GDK_PIXMAP_H__

#include <gdk/gdktypes.h>
#include <gdk/gdkdrawable.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GdkPixmapObject GdkPixmapObject;
typedef struct _GdkPixmapObjectClass GdkPixmapObjectClass;

#define GDK_TYPE_PIXMAP              (gdk_pixmap_get_type ())
#define GDK_PIXMAP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_PIXMAP, GdkPixmap))
#define GDK_PIXMAP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXMAP, GdkPixmapObjectClass))
#define GDK_IS_PIXMAP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXMAP))
#define GDK_IS_PIXMAP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXMAP))
#define GDK_PIXMAP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXMAP, GdkPixmapClass))
#define GDK_PIXMAP_OBJECT(object)    ((GdkPixmapObject *) GDK_PIXMAP (object))

struct _GdkPixmapObject
{
  GdkDrawable parent_instance;
  
  GdkDrawable *impl;  /* window-system-specific delegate object */

  gint depth;
};

struct _GdkPixmapObjectClass
{
  GdkDrawableClass parent_class;

};

GType      gdk_pixmap_get_type          (void);

/* Pixmaps
 */
GdkPixmap* gdk_pixmap_new		(GdkWindow  *window,
					 gint	     width,
					 gint	     height,
					 gint	     depth);
#ifdef GDK_WINDOWING_WIN32
GdkPixmap* gdk_pixmap_create_on_shared_image
					(GdkImage  **image_return,
					 GdkWindow  *window,
					 GdkVisual  *visual,
					 gint        width,
					 gint        height,
					 gint        depth);
#endif
GdkBitmap* gdk_bitmap_create_from_data	(GdkWindow   *window,
					 const gchar *data,
					 gint	      width,
					 gint	      height);
GdkPixmap* gdk_pixmap_create_from_data	(GdkWindow   *window,
					 const gchar *data,
					 gint	      width,
					 gint	      height,
					 gint	      depth,
					 GdkColor    *fg,
					 GdkColor    *bg);
GdkPixmap* gdk_pixmap_create_from_xpm	(GdkWindow  *window,
					 GdkBitmap **mask,
					 GdkColor   *transparent_color,
					 const gchar *filename);
GdkPixmap* gdk_pixmap_colormap_create_from_xpm 
                                        (GdkWindow   *window,
					 GdkColormap *colormap,
					 GdkBitmap  **mask,
					 GdkColor    *transparent_color,
					 const gchar *filename);
GdkPixmap* gdk_pixmap_create_from_xpm_d (GdkWindow  *window,
					 GdkBitmap **mask,
					 GdkColor   *transparent_color,
					 gchar	   **data);
GdkPixmap* gdk_pixmap_colormap_create_from_xpm_d 
                                        (GdkWindow   *window,
					 GdkColormap *colormap,
					 GdkBitmap  **mask,
					 GdkColor    *transparent_color,
					 gchar     **data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_PIXMAP_H__ */
