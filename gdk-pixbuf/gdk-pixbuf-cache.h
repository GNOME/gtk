#ifndef _GDK_PIXBUF_CACHE_H_
#define _GDK_PIXBUF_CACHE_H_

/* The optional cache interface */
typedef struct {
	int dummy;
} GdkPixBufCache;

GdkPixBufCache  *gdk_pixbuf_cache_new        (long image_cache_limit,
				              long pixmap_bitmap_cache_limit);
void             gdk_pixbuf_cache_destroy    (GdkPixBufCache *cache);

GdkPixBuf       *gdk_pixbuf_cache_load_image (GdkPixBufCache *cache,
					      const char *file);
#endif
