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
#define GDK_PIXMAP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXMAP, GdkPixmapObjectClass))
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

GType      gdk_pixmap_get_type          (void) G_GNUC_CONST;

/* Pixmaps
 */
GdkPixmap* gdk_pixmap_new		(GdkDrawable *drawable,
					 gint	      width,
					 gint	      height,
					 gint	      depth);
GdkBitmap* gdk_bitmap_create_from_data	(GdkDrawable *drawable,
					 const gchar *data,
					 gint	      width,
					 gint	      height);
GdkPixmap* gdk_pixmap_create_from_data	(GdkDrawable    *drawable,
					 const gchar 	*data,
					 gint	     	 width,
					 gint	     	 height,
					 gint	         depth,
					 const GdkColor *fg,
					 const GdkColor *bg);

GdkPixmap* gdk_pixmap_create_from_xpm            (GdkDrawable    *drawable,
						  GdkBitmap     **mask,
						  const GdkColor *transparent_color,
						  const gchar    *filename);
GdkPixmap* gdk_pixmap_colormap_create_from_xpm   (GdkDrawable    *drawable,
						  GdkColormap    *colormap,
						  GdkBitmap     **mask,
						  const GdkColor *transparent_color,
						  const gchar    *filename);
GdkPixmap* gdk_pixmap_create_from_xpm_d          (GdkDrawable    *drawable,
						  GdkBitmap     **mask,
						  const GdkColor *transparent_color,
						  gchar         **data);
GdkPixmap* gdk_pixmap_colormap_create_from_xpm_d (GdkDrawable    *drawable,
						  GdkColormap    *colormap,
						  GdkBitmap     **mask,
						  const GdkColor *transparent_color,
						  gchar         **data);

/* Functions to create/lookup pixmaps from their native equivalents
 */
#ifndef GDK_MULTIHEAD_SAFE
GdkPixmap*    gdk_pixmap_foreign_new (GdkNativeWindow anid);
GdkPixmap*    gdk_pixmap_lookup      (GdkNativeWindow anid);
#endif /* GDK_MULTIHEAD_SAFE */

GdkPixmap*    gdk_pixmap_foreign_new_for_display (GdkDisplay      *display,
						  GdkNativeWindow  anid);
GdkPixmap*    gdk_pixmap_lookup_for_display      (GdkDisplay      *display,
						  GdkNativeWindow  anid);

#ifndef GDK_DISABLE_DEPRECATED
#define gdk_bitmap_ref                 gdk_drawable_ref
#define gdk_bitmap_unref               gdk_drawable_unref
#define gdk_pixmap_ref                 gdk_drawable_ref
#define gdk_pixmap_unref               gdk_drawable_unref
#endif /* GDK_DISABLE_DEPRECATED */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_PIXMAP_H__ */
