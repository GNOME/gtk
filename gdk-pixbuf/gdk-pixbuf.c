/*
 * gdk-pixbuf.c: Resource management.
 *
 * Author:
 *    Miguel de Icaza (miguel@gnu.org)
 */
#include <config.h>
#include <glib.h>
#include <math.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_alphagamma.h>
#include "gdk-pixbuf.h"


void
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

GdkPixBuf *
gdk_pixbuf_scale (GdkPixBuf *pixbuf, gint w, gint h)
{
    GdkPixBuf *spb;
    art_u8 *pixels;
    gint rowstride;
    double affine[6];
    ArtAlphaGamma *alphagamma;
    ArtPixBuf *art_pixbuf = NULL;

    alphagamma = NULL;

    affine[1] = affine[2] = affine[4] = affine[5] = 0;
    
    
    affine[0] = w / (double)(pixbuf->art_pixbuf->width);
    affine[3] = h / (double)(pixbuf->art_pixbuf->height);

    /*    rowstride = w * pixbuf->art_pixbuf->n_channels; */
    rowstride = w * 3;

    pixels = art_alloc (h * rowstride);
    art_rgb_pixbuf_affine( pixels, 0, 0, w, h, rowstride,
			   pixbuf->art_pixbuf,
			   affine, ART_FILTER_NEAREST, alphagamma);

    if (pixbuf->art_pixbuf->has_alpha)
      /* should be rgba */
      art_pixbuf = art_pixbuf_new_rgb(pixels, w, h, rowstride); 
    else 
      art_pixbuf = art_pixbuf_new_rgb(pixels, w, h, rowstride); 

    art_pixbuf_free (pixbuf->art_pixbuf);
    pixbuf->art_pixbuf = art_pixbuf;

    return pixbuf;
}

GdkPixBuf *
gdk_pixbuf_rotate (GdkPixBuf *pixbuf, gdouble angle)
{
     GdkPixBuf *rotate;
     art_u8 *pixels;
     gint rowstride, w, h;
     gdouble rad;
     double affine[6];
     ArtAlphaGamma *alphagamma = NULL;
     ArtPixBuf *art_pixbuf = NULL;

     w = pixbuf->art_pixbuf->width;
     h = pixbuf->art_pixbuf->height;

     rad = M_PI * angle / 180.0;

     affine[0] = cos(rad);
     affine[1] = sin(rad);
     affine[2] = -sin(rad);
     affine[3] = cos(rad);
     affine[4] = affine[5] = 0;

     /* rowstride = w * pixbuf->art_pixbuf->n_channels; */
     rowstride = w * 3;

     pixels = art_alloc (h * rowstride);
     art_rgb_pixbuf_affine (pixels, 0, 0, w, h, rowstride,
			    pixbuf->art_pixbuf,
			    affine, ART_FILTER_NEAREST, alphagamma);
      if (pixbuf->art_pixbuf->has_alpha)
      /* should be rgba */
      art_pixbuf = art_pixbuf_new_rgb(pixels, w, h, rowstride); 
    else 
      art_pixbuf = art_pixbuf_new_rgb(pixels, w, h, rowstride); 

    art_pixbuf_free (pixbuf->art_pixbuf);
    pixbuf->art_pixbuf = art_pixbuf;

    return pixbuf;
}
