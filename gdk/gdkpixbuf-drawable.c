/*
 * Creates an GdkPixBuf from a Drawable
 *
 * Author:
 *   Cody Russell <bratsche@dfw.net>
 */
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-drawable.h"

/* private function */

static GdkPixBuf *
gdk_pixbuf_from_drawable_core (GdkWindow *window,
			       gint x, gint y,
			       gint width, gint height,
			       gint with_alpha)
{
	GdkImage *image;
	ArtPixBuf *art_pixbuf;
	GdkColormap *colormap;
	art_u8 *buff;
	art_u8 *pixels;
	gulong pixel;
	gint rowstride;
	art_u8 r, g, b;
	gint xx, yy;
	gint fatness;
	gint screen_width, screen_height;
	gint window_width, window_height, window_x, window_y;

	g_return_val_if_fail (window != NULL, NULL);

	screen_width = gdk_screen_width();
	screen_height = gdk_screen_height();
	gdk_window_get_geometry(window, NULL, NULL,
				&window_width, &window_height, NULL);
	gdk_window_get_origin(window, &window_x, &window_y);

	if(window_x < 0)
	{
		x = ABS(window_x);
		width = window_width - x;
	}
	else
	{
		width = CLAMP(window_x + window_width, window_x,
				screen_width) - window_x;
	}

	if(window_y < 0)
	{
		y = ABS(window_y);
		height = window_height - y;
	}
	else
	{
		height = CLAMP(window_y + window_height, window_y,
				screen_height) - window_y;
	}

	image = gdk_image_get (window, x, y, width, height);
	colormap = gdk_rgb_get_cmap ();

	fatness = with_alpha ? 4 : 3;
	rowstride = width * fatness;

	buff = art_alloc (rowstride * height);
	pixels = buff;

	switch (image->depth)
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
		for (yy = 0; yy < height; yy++)
		{
			for (xx = 0; xx < width; xx++)
			{
				pixel = gdk_image_get_pixel (image, xx, yy);
				pixels[0] = colormap->colors[pixel].red;
				pixels[1] = colormap->colors[pixel].green;
				pixels[2] = colormap->colors[pixel].blue;
				if (with_alpha)
					pixels[3] = 0;
				pixels += fatness;
           
			}
		}
		break;

	case 16:
	case 15:
		for (yy = 0; yy < height; yy++)
		{
			for (xx = 0; xx < width; xx++)
			{
				pixel = gdk_image_get_pixel (image, xx, yy);
				r =  (pixel >> 8) & 0xf8;
				g =  (pixel >> 3) & 0xfc;
				b =  (pixel << 3) & 0xf8;
				pixels[0] = r;
				pixels[1] = g;
				pixels[2] = b;
				if (with_alpha)
					pixels[3] = 0; /* GdkImages don't have alpha =) */
				pixels += fatness;
			}
		}
		break;

	case 24:
	case 32:
		for (yy = 0; yy < height; yy++)
		{
			for (xx = 0; xx < width; xx++)
			{
				pixel = gdk_image_get_pixel (image, xx, yy);
				r =  (pixel >> 16) & 0xff;
				g =  (pixel >> 8)  & 0xff;
				b = pixel & 0xff;
				pixels[0] = r;
				pixels[1] = g;
				pixels[2] = b;
				if (with_alpha)
					pixels[3] = 0;  /* GdkImages don't have alpha.. */
				pixels += fatness;
			}
		}
		break;

	default:
		g_error ("art_pixbuf_from_drawable_core (): Unknown depth.");
	}

	art_pixbuf = with_alpha ? art_pixbuf_new_rgba (buff, width,  height, rowstride) :
		art_pixbuf_new_rgb (buff, width, height, rowstride);

	return gdk_pixbuf_new(art_pixbuf, NULL);
}

/* Public functions */

GdkPixBuf *
gdk_pixbuf_rgb_from_drawable  (GdkWindow *window,
			      gint x, gint y,
			      gint width, gint height)
{
	return gdk_pixbuf_from_drawable_core  (window, x, y, width, height, 0);
}
          
GdkPixBuf *
gdk_pixbuf_rgba_from_drawable  (GdkWindow *window,
			       gint x, gint y,
			       gint width, gint height)
{
	return gdk_pixbuf_from_drawable_core  (window, x, y, width, height, 1);
}
