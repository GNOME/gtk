/* GdkPixbuf library - Main loading interface.
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
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
#include <string.h>
#include <glib.h>
#include "gdk-pixbuf-io.h"



static gboolean
pixbuf_check_png (guchar *buffer, int size)
{
	if (size < 28)
		return FALSE;

	if (buffer [0] != 0x89 ||
	    buffer [1] != 'P' ||
	    buffer [2] != 'N' ||
	    buffer [3] != 'G' ||
	    buffer [4] != 0x0d ||
	    buffer [5] != 0x0a ||
	    buffer [6] != 0x1a ||
	    buffer [7] != 0x0a)
		return FALSE;

	return TRUE;
}

static gboolean
pixbuf_check_jpeg (guchar *buffer, int size)
{
	if (size < 10)
		return FALSE;

	if (buffer [0] != 0xff || buffer [1] != 0xd8)
		return FALSE;

	return TRUE;
}

static gboolean
pixbuf_check_tiff (guchar *buffer, int size)
{
	if (size < 10)
		return FALSE;

	if (buffer [0] == 'M' &&
	    buffer [1] == 'M' &&
	    buffer [2] == 0   &&
	    buffer [3] == 0x2a)
		return TRUE;

	if (buffer [0] == 'I' &&
	    buffer [1] == 'I' &&
	    buffer [2] == 0x2a &&
	    buffer [3] == 0)
		return TRUE;

	return FALSE;
}

static gboolean
pixbuf_check_gif (guchar *buffer, int size)
{
	if (size < 20)
		return FALSE;

	if (strncmp (buffer, "GIF8", 4) == 0)
		return TRUE;

	return FALSE;
}

static gboolean
pixbuf_check_xpm (guchar *buffer, int size)
{
	if (size < 20)
		return FALSE;

	if (strncmp (buffer, "/* XPM */", 9) == 0)
		return TRUE;

	return FALSE;
}

static gboolean
pixbuf_check_pnm (guchar *buffer, int size)
{
	if (size < 20)
		return FALSE;

	if (buffer [0] == 'P') {
		if (buffer [1] == '1' ||
		    buffer [1] == '2' ||
		    buffer [1] == '3' ||
		    buffer [1] == '4' ||
		    buffer [1] == '5' ||
		    buffer [1] == '6')
			return TRUE;
	}
	return FALSE;
}
static gboolean
pixbuf_check_sunras (guchar *buffer, int size)
{
	if (size < 32)
		return FALSE;

	if (buffer [0] != 0x59 ||
	    buffer [1] != 0xA6 ||
	    buffer [2] != 0x6A ||
	    buffer [3] != 0x95)
		return FALSE;

	return TRUE;
}

static gboolean
pixbuf_check_ico (guchar *buffer, int size)
{
	/* Note that this may cause false positives, but .ico's don't
	   have a magic number.*/
	if (size < 6)
		return FALSE;
	if (buffer [0] != 0x0 ||
	    buffer [1] != 0x0 ||
	    ((buffer [2] != 0x1)&&(buffer[2]!=0x2)) ||
	    buffer [3] != 0x0 ||
	    buffer [5] != 0x0 )
		return FALSE;

	return TRUE;
}


static gboolean
pixbuf_check_bmp (guchar *buffer, int size)
{
	if (size < 20)
		return FALSE;

	if (buffer [0] != 'B' || buffer [1] != 'M')
		return FALSE;

	return TRUE;
}


GdkPixbufModule file_formats [] = {
	{ "png",  pixbuf_check_png, NULL,  NULL, NULL, NULL, NULL, NULL },
	{ "jpeg", pixbuf_check_jpeg, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "tiff", pixbuf_check_tiff, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "gif",  pixbuf_check_gif, NULL,  NULL, NULL, NULL, NULL, NULL },
#define XPM_FILE_FORMAT_INDEX 4
	{ "xpm",  pixbuf_check_xpm, NULL,  NULL, NULL, NULL, NULL, NULL },
	{ "pnm",  pixbuf_check_pnm, NULL,  NULL, NULL, NULL, NULL, NULL },
	{ "ras",  pixbuf_check_sunras, NULL,  NULL, NULL, NULL, NULL, NULL },
	{ "ico",  pixbuf_check_ico, NULL,  NULL, NULL, NULL, NULL, NULL },
	{ "bmp",  pixbuf_check_bmp, NULL,  NULL, NULL, NULL, NULL, NULL },
	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};


/* actually load the image handler - gdk_pixbuf_get_module only get a */
/* reference to the module to load, it doesn't actually load it       */
/* perhaps these actions should be combined in one function           */
void
gdk_pixbuf_load_module (GdkPixbufModule *image_module)
{
	char *module_name;
	char *path;
	GModule *module;
	gpointer load_sym;

        g_return_if_fail(image_module->module == NULL);

	module_name = g_strconcat ("pixbuf-", image_module->module_name, NULL);
	path = g_module_build_path (PIXBUF_LIBDIR, module_name);

	module = g_module_open (path, G_MODULE_BIND_LAZY);
	if (!module) {
                /* Debug feature, check in present working directory */
                g_free(path);
                path = g_module_build_path("", module_name);
                module = g_module_open(path, G_MODULE_BIND_LAZY);

                if (!module) {
                        g_warning ("Unable to load module: %s: %s", path, g_module_error());
                        g_free (module_name);
                        g_free(path);
                        return;
                }
                g_free(path);
	} else {
                g_free (path);
        }

        g_free (module_name);

	image_module->module = module;

	if (g_module_symbol (module, "image_load", &load_sym))
		image_module->load = load_sym;

        if (g_module_symbol (module, "image_load_xpm_data", &load_sym))
		image_module->load_xpm_data = load_sym;

        if (g_module_symbol (module, "image_begin_load", &load_sym))
		image_module->begin_load = load_sym;

        if (g_module_symbol (module, "image_stop_load", &load_sym))
		image_module->stop_load = load_sym;

        if (g_module_symbol (module, "image_load_increment", &load_sym))
		image_module->load_increment = load_sym;

        if (g_module_symbol (module, "image_load_animation", &load_sym))
		image_module->load_animation = load_sym;
}



GdkPixbufModule *
gdk_pixbuf_get_module (gchar *buffer, gint size)
{
	gint i;

	for (i = 0; file_formats [i].module_name; i++) {
		if ((* file_formats [i].format_check) (buffer, size))
			return &(file_formats[i]);
	}
	return NULL;
}

/**
 * gdk_pixbuf_new_from_file:
 * @filename: Name of file to load.
 *
 * Creates a new pixbuf by loading an image from a file.  The file format is
 * detected automatically.
 *
 * Return value: A newly-created pixbuf with a reference count of 1, or NULL if
 * any of several error conditions occurred:  the file could not be opened,
 * there was no loader for the file's format, there was not enough memory to
 * allocate the image buffer, or the image file contained invalid data.
 **/
GdkPixbuf *
gdk_pixbuf_new_from_file (const char *filename)
{
	GdkPixbuf *pixbuf;
	gint size;
	FILE *f;
	char buffer [128];
	GdkPixbufModule *image_module;

	f = fopen (filename, "r");
	if (!f)
		return NULL;

	size = fread (&buffer, 1, sizeof (buffer), f);

	if (size == 0) {
		fclose (f);
		return NULL;
	}

	image_module = gdk_pixbuf_get_module (buffer, size);
	if (image_module){
		if (image_module->module == NULL)
			gdk_pixbuf_load_module (image_module);

		if (image_module->load == NULL) {
			fclose (f);
			return NULL;
		}

		fseek (f, 0, SEEK_SET);
		pixbuf = (* image_module->load) (f);
		fclose (f);

		if (pixbuf)
			g_assert (pixbuf->ref_count != 0);

		return pixbuf;
	} else {
		g_warning ("Unable to find handler for file: %s", filename);
	}

	fclose (f);
	return NULL;
}

/**
 * gdk_pixbuf_new_from_xpm_data:
 * @data: Pointer to inline XPM data.
 *
 * Creates a new pixbuf by parsing XPM data in memory.  This data is commonly
 * the result of including an XPM file into a program's C source.
 *
 * Return value: A newly-created pixbuf with a reference count of 1.
 **/
GdkPixbuf *
gdk_pixbuf_new_from_xpm_data (const gchar **data)
{
        GdkPixbuf *(* load_xpm_data) (const gchar **data);
        GdkPixbuf *pixbuf;

        if (file_formats[XPM_FILE_FORMAT_INDEX].module == NULL) {
                gdk_pixbuf_load_module(&file_formats[XPM_FILE_FORMAT_INDEX]);
        }

        if (file_formats[XPM_FILE_FORMAT_INDEX].module == NULL) {
                g_warning("Can't find gdk-pixbuf module for parsing inline XPM data");
                return NULL;
        } else if (file_formats[XPM_FILE_FORMAT_INDEX].load_xpm_data == NULL) {
                g_warning("gdk-pixbuf XPM module lacks XPM data capability");
                return NULL;
        } else {
                load_xpm_data = file_formats[XPM_FILE_FORMAT_INDEX].load_xpm_data;
        }

        pixbuf = load_xpm_data(data);

        return pixbuf;
}


/**
 * gdk_pixbuf_animation_new_from_file:
 * @filename: The filename.
 * 
 * Creates a new @GdkPixbufAnimation with @filename loaded as the animation.  If
 * @filename doesn't exist or is an invalid file, the @n_frames member will be
 * 0.  If @filename is a static image (and not an animation) then the @n_frames
 * member will be 1.
 * 
 * Return value: A newly created GdkPixbufAnimation.
 **/
GdkPixbufAnimation *
gdk_pixbuf_animation_new_from_file (const gchar *filename)
{
	GdkPixbufAnimation *animation;
	gint size;
	FILE *f;
	char buffer [128];
	GdkPixbufModule *image_module;

	f = fopen (filename, "r");
	if (!f)
		return NULL;

	size = fread (&buffer, 1, sizeof (buffer), f);

	if (size == 0) {
		fclose (f);
		return NULL;
	}

	image_module = gdk_pixbuf_get_module (buffer, size);
	if (image_module){
		if (image_module->module == NULL)
			gdk_pixbuf_load_module (image_module);

		if (image_module->load_animation == NULL) {
			GdkPixbufFrame *frame;
			if (image_module->load == NULL) {
				fclose (f);
				return NULL;
			}
			animation = g_new (GdkPixbufAnimation, 1);
			frame = g_new (GdkPixbufFrame, 1);

			animation->n_frames = 1;
			animation->frames = g_list_prepend (NULL, (gpointer) frame);

			frame->x_offset = 0;
			frame->y_offset = 0;
			frame->delay_time = -1;
			frame->action = GDK_PIXBUF_FRAME_RETAIN;

			fseek (f, 0, SEEK_SET);
			frame->pixbuf = (* image_module->load) (f);
			fclose (f);
		} else {
			fseek (f, 0, SEEK_SET);
			animation = (* image_module->load_animation) (f);
			fclose (f);
		}

		return animation;
	} else {
		g_warning ("Unable to find handler for file: %s", filename);
	}

	fclose (f);
	return NULL;
}
