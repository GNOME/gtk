#ifndef _GDK_PIXBUF_H_
#define _GDK_PIXBUF_H_

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_pixbuf.h>
#include <glib.h>

typedef struct _GdkPixBuf GdkPixBuf;
typedef void (*GdkPixBufUnrefFunc) (GdkPixBuf *pixbuf);

struct _GdkPixBuf
{
	int                 ref_count;
	ArtPixBuf          *art_pixbuf;
	GdkPixBufUnrefFunc *unref_fn;
};

GdkPixBuf *gdk_pixbuf_load_image (const char *file);
void       gdk_pixbuf_save_image (const char *format_id, const char *file, ...);
GdkPixBuf *gdk_pixbuf_new        (ArtPixBuf          *art_pixbuf,
				  GdkPixBufUnrefFunc *unref_fn);
void       gdk_pixbuf_ref        (GdkPixBuf *pixbuf);
void       gdk_pixbuf_unref      (GdkPixBuf *pixbuf);
GdkPixBuf *gdk_pixbuf_duplicate  (const GdkPixBuf *pixbuf);
GdkPixBuf *gdk_pixbuf_scale	 (const GdkPixBuf *pixbuf, gint w, gint h);
GdkPixBuf *gdk_pixbuf_rotate     (GdkPixBuf *pixbuf, gdouble angle);

void	   gdk_pixbuf_destroy    (GdkPixBuf *pixbuf);

#endif /* _GDK_PIXBUF_H_ */
