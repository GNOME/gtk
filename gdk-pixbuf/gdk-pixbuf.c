/*
 * gdk-pixbuf.c: Resource management.
 *
 * Author:
 *    Miguel de Icaza (miguel@gnu.org)
 */
#include <config.h>
#include <glib.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_alphagamma.h>
#include "gdk-pixbuf.h"


static void
gdk_pixbuf_destroy (GdkPixBuf *pixbuf)
{
	art_pixbuf_free (pixbuf->art_pixbuf);
	g_free (pixbuf);
}

void
gdk_pixbuf_ref (GdkPixBuf *pixbuf)
{
    g_return_if_fail (pixbuf != NULL);
    
    pixbuf->ref_count++;
}

void
gdk_pixbuf_unref (GdkPixBuf *pixbuf)
{
    g_return_if_fail (pixbuf != NULL);
    g_return_if_fail (pixbuf->ref_count == 0);
    
    pixbuf->ref_count--;
    if (pixbuf->ref_count)
	gdk_pixbuf_destroy (pixbuf);
}

void
gdk_pixbuf_free (GdkPixBuf *pixbuf)
{
     art_free(pixbuf->art_pixbuf->pixels);
     art_pixbuf_free_shallow(pixbuf->art_pixbuf);
     g_free(pixbuf);
}

GdkPixBuf *
gdk_pixbuf_scale (GdkPixBuf *pixbuf, gint w, gint h)
{
    GdkPixBuf *spb;
    art_u8 *pixels;
    double affine[6];
    ArtAlphaGamma *alphagamma;

    alphagamma = NULL;

    affine[1] = affine[2] = affine[4] = affine[5] = 0;
    
    affine[0] = w / (pixbuf->art_pixbuf->width);
    affine[3] = h / (pixbuf->art_pixbuf->height);

    spb = g_new (GdkPixBuf, 1);

    if (pixbuf->art_pixbuf->has_alpha) {
	 /* Following code is WRONG....of course, the code for this
	  * transform isn't in libart yet.
	  */
#if 0
	 pixels = art_alloc (h * w * 4);
	 art_rgb_affine( pixels, 0, 0, w, h, (w * 4),
			 pixbuf->art_pixbuf->pixels,
			 pixbuf->art_pixbuf->width,
			 pixbuf->art_pixbuf->height,
			 pixbuf->art_pixbuf->rowstride,
			 affine, ART_FILTER_NEAREST, alphagamma);
	 spb->art_pixbuf = art_pixbuf_new_rgba(pixels, w, h, (w * 4));
#endif
    } else {
	 pixels = art_alloc (h * w * 3);
	 art_rgb_affine( pixels, 0, 0, w, h, (w * 3),
			 pixbuf->art_pixbuf->pixels,
			 pixbuf->art_pixbuf->width,
			 pixbuf->art_pixbuf->height,
			 pixbuf->art_pixbuf->rowstride,
			 affine, ART_FILTER_NEAREST, alphagamma);
	 spb->art_pixbuf = art_pixbuf_new_rgb(pixels, w, h, (w * 3)); 
	 spb->ref_count = 0;
	 spb->unref_func = NULL;
    }
}
