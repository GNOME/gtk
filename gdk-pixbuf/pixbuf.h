#ifndef _GDK_PIXBUF_H_
#define _GDK_PIXBUF_H_

#include <libart_lgpl/art_pixbuf.h>

typedef struct {
	int ref_count;
	ArtPixBuf *pixbuf;
	void (*unref_func)(void *gdkpixbuf);
} GdkPixBuf;

GdkPixBuf *gdk_pixbuf_load_image (const char *file);
void       gdk_pixbuf_save_image (const char *format_id, const char *file, ...);
void       gdk_pixbuf_ref        (GdkPixBuf *pixbuf);
void       gdk_pixbuf_unref      (GdkPixBuf *pixbuf);
GdkPixBuf  gdk_pixbuf_duplicate  (GdkPixBuf *pixbuf);

#endif /* _GDK_PIXBUF_H_ */
