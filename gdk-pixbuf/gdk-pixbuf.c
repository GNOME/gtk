/* GdkPixbuf library
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Carsten Haitzler <raster@rasterman.com>
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

#include <config.h>
#include <math.h>
#include <glib.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_pixbuf.h>
#include <libart_lgpl/art_rgb_pixbuf_affine.h>
#include <libart_lgpl/art_alphagamma.h>
#include "gdk-pixbuf.h"



void
gdk_pixbuf_destroy (GdkPixbuf *pixbuf)
{
	art_pixbuf_free (pixbuf->art_pixbuf);
	pixbuf->art_pixbuf = NULL;
	g_free (pixbuf);
}

GdkPixbuf *
gdk_pixbuf_new (ArtPixBuf *art_pixbuf, GdkPixbufUnrefFunc *unref_fn)
{
	GdkPixbuf *pixbuf;

	if (!art_pixbuf)
		return NULL;

	pixbuf = g_new (GdkPixbuf, 1);
	pixbuf->ref_count = 1;
	pixbuf->unref_fn = unref_fn;
	pixbuf->art_pixbuf = art_pixbuf;

	return pixbuf;
}

void
gdk_pixbuf_ref (GdkPixbuf *pixbuf)
{
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (pixbuf->ref_count > 0);

	pixbuf->ref_count++;
}

void
gdk_pixbuf_unref (GdkPixbuf *pixbuf)
{
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (pixbuf->ref_count > 0);

	pixbuf->ref_count--;

	if (pixbuf->ref_count == 0)
		gdk_pixbuf_destroy (pixbuf);
}

GdkPixbuf *
gdk_pixbuf_scale (const GdkPixbuf *pixbuf, gint w, gint h)
{
	art_u8 *pixels;
	gint rowstride;
	double affine[6];
	ArtAlphaGamma *alphagamma;
	ArtPixBuf *art_pixbuf = NULL;
	GdkPixbuf *copy = NULL;

	alphagamma = NULL;

	affine[1] = affine[2] = affine[4] = affine[5] = 0;

	affine[0] = w / (double)(pixbuf->art_pixbuf->width);
	affine[3] = h / (double)(pixbuf->art_pixbuf->height);

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

	copy = gdk_pixbuf_new (art_pixbuf, NULL);

	if (!copy)
		art_free (pixels);

	return copy;
}

GdkPixbuf *
gdk_pixbuf_duplicate (const GdkPixbuf *pixbuf)
{
	GdkPixbuf *copy  = g_new (GdkPixbuf, 1);

	copy->ref_count  = 1;
	copy->unref_fn   = pixbuf->unref_fn;
	copy->art_pixbuf = art_pixbuf_duplicate (pixbuf->art_pixbuf);

	return copy;
}

GdkPixbuf *
gdk_pixbuf_rotate (GdkPixbuf *pixbuf, gdouble angle)
{
	art_u8 *pixels;
	gint rowstride, w, h;
	gdouble rad;
	double rot[6], trans[6], affine[6];
	ArtAlphaGamma *alphagamma = NULL;
	ArtPixBuf *art_pixbuf = NULL;

	w = pixbuf->art_pixbuf->width;
	h = pixbuf->art_pixbuf->height;

	rad = (M_PI * angle / 180.0);

	rot[0] = cos(rad);
	rot[1] = sin(rad);
	rot[2] = -sin(rad);
	rot[3] = cos(rad);
	rot[4] = rot[5] = 0;

	trans[0] = trans[3] = 1;
	trans[1] = trans[2] = 0;
	trans[4] = -(double)w / 2.0;
	trans[5] = -(double)h / 2.0;

	art_affine_multiply(rot, trans, rot);

	trans[0] = trans[3] = 1;
	trans[1] = trans[2] = 0;
	trans[4] = (double)w / 2.0;
	trans[5] = (double)h / 2.0;

	art_affine_multiply(affine, rot, trans);

	g_print("Affine: %e %e %e %e %e %e\n", affine[0], affine[1], affine[2],
		affine[3], affine[4], affine[5]);

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
