/*
 * gdk-pixbuf.c: Resource management.
 *
 * Author:
 *    Miguel de Icaza (miguel@gnu.org)
 */
#include <config.h>
#include <glib.h>
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

GdkPixBuf *
gdk_pixbuf_scale (GdkPixBuf *pixbuf, gint w, gint h)
{
}
