/*
 * gdk-pixbuf-io.c: Code to load images into GdkPixBufs
 *
 * Author:
 *    Miguel de Icaza (miguel@gnu.org)
 */
#include <config.h>
#include "gdk-pixbuf.h"

static struct {
	char      *module_name;
	gboolean   (*format_check)(char *buffer, int size);
	GdkPixBuf *(*load)(char *filename);
	int        (*save)(char *filename, ...);
} loaders [] = {
	{ "png",  pixbuf_check_png,  NULL, NULL },
	{ "jpeg", pixbuf_check_jpeg, NULL, NULL },
	{ "tiff", pixbuf_check_tiff, NULL, NULL },
	{ "gif",  pixbuf_check_gif,  NULL, NULL },
	{ "xpm",  pixbuf_check_xpm,  pixbuf_xpm_load, pixbuf_xpm_save },
	{ "bmp",  pixbuf_check_bmp,  NULL, NULL },
	{ "ppm",  pixbuf_check_ppm,  NULL, NULL },
	{ NULL, NULL, NULL, NULL },
};

static int
image_file_format (const char *file)
{
	FILE *f = fopen (file);

	if (!f)
		return -1;
}

static void
image_loader_load (int idx)
{
}

GdkPixBuf *
gdk_pixbuf_load_image (const char *file)
{
	GdkPixBuf *pixbuf;
	FormatLoader format_loader;
	FILE *f;
	char buffer [128];

	f = fopen (file);
	if (!f)
		return NULL;
	n = fread (&buffer, 1, sizeof (buffer), f);
	fclose (f);
	if (n == 0)
		return NULL;

	for (i = 0; loaders [i].module_name; i++){
		if ((*loaders [i].format_check)(buffer, n)){
			if (!loaders [i].load)
				image_loader_load (i);

			if (!loaders [i].load)
				return NULL;

			return (*loaders [i].load)(file);
		}
	}

	return NULL;
}
