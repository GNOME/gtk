/*
 * gdk-pixbuf-data.c: Code to load images into GdkPixBufs from constant data
 *
 * Author:
 *    Michael Fulbright (drmike@redhat.com)
 *    Copyright (C) 1999 Red Hat, Inc.
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "gdk-pixbuf.h"

/* This function does all the work. */

static GdkPixBuf *
_pixbuf_create_from_rgb_d(unsigned char *data, int w, int h)
{
	GdkPixBuf *pixbuf;
	ArtPixBuf *art_pixbuf;
	art_u8 *pixels;

	pixels = art_alloc (w*h*3);
	if (!pixels) {
		g_warning ("RGBD: Cannot alloc ArtBuf");
		return NULL;
	}

	memcpy (pixels, data, w*h*3);

	art_pixbuf = art_pixbuf_new_rgb (pixels, w, h, (w*3));
	pixbuf = gdk_pixbuf_new (art_pixbuf, NULL);

	if (!pixbuf)
		art_free (pixels);

	return pixbuf;
}


GdkPixBuf *
gdk_pixbuf_load_image_from_rgb_d (unsigned char *data, 
				  int rgb_width, int rgb_height)
{
	g_return_val_if_fail (data != NULL, NULL);

	return _pixbuf_create_from_rgb_d(data, rgb_width, rgb_height);
}

