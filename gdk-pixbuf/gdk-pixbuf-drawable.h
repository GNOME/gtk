#ifndef _GDK_PIXBUF_DRAWABLE_H_
#define _GDK_PIXBUF_DRAWABLE_H_

#include <gdk/gdk.h>
#include <gdk-pixbuf.h>

GdkPixbuf *gdk_pixbuf_rgb_from_drawable (GdkWindow *window, gint x, gint y, gint width, gint height);
GdkPixbuf *gdk_pixbuf_rgba_from_drawable (GdkWindow *window, gint x, gint y, gint width, gint height);

#endif /* _GDK_PIXBUF_DRAWABLE_H_ */
