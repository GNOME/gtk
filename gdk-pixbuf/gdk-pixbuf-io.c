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
#include "gdk-pixbuf-private.h"
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

static GdkPixbufModule file_formats [] = {
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

#ifdef USE_GMODULE 
static gboolean
pixbuf_module_symbol (GModule *module, const char *module_name, const char *symbol_name, gpointer *symbol)
{
	char *full_symbol_name = g_strconcat ("gdk_pixbuf__", module_name, "_", symbol_name, NULL);
	gboolean return_value;

	return_value = g_module_symbol (module, full_symbol_name, symbol);
	g_free (full_symbol_name);
	
	return return_value;
}

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
	char *name;
	
        g_return_if_fail (image_module->module == NULL);

	name = image_module->module_name;
	
	module_name = g_strconcat ("pixbuf-", name, NULL);
	path = g_module_build_path (PIXBUF_LIBDIR, module_name);

	module = g_module_open (path, G_MODULE_BIND_LAZY);
	if (!module) {
                /* Debug feature, check in present working directory */
                g_free (path);
                path = g_module_build_path ("", module_name);
                module = g_module_open (path, G_MODULE_BIND_LAZY);

                if (!module) {
                        g_warning ("Unable to load module: %s: %s", path, g_module_error ());
                        g_free (module_name);
                        g_free (path);
                        return;
                }
                g_free (path);
	} else {
                g_free (path);
        }

        g_free (module_name);

	image_module->module = module;

	if (pixbuf_module_symbol (module, name, "image_load", &load_sym))
		image_module->load = load_sym;

        if (pixbuf_module_symbol (module, name, "image_load_xpm_data", &load_sym))
		image_module->load_xpm_data = load_sym;

        if (pixbuf_module_symbol (module, name, "image_begin_load", &load_sym))
		image_module->begin_load = load_sym;

        if (pixbuf_module_symbol (module, name, "image_stop_load", &load_sym))
		image_module->stop_load = load_sym;

        if (pixbuf_module_symbol (module, name, "image_load_increment", &load_sym))
		image_module->load_increment = load_sym;

        if (pixbuf_module_symbol (module, name, "image_load_animation", &load_sym))
		image_module->load_animation = load_sym;
}
#else

#define mname(type,fn) gdk_pixbuf__ ## type ## _image_ ##fn
#define m_load(type)  extern GdkPixbuf * mname(type,load) (FILE *f);
#define m_load_xpm_data(type)  extern GdkPixbuf * mname(type,load_xpm_data) (const char **data);
#define m_begin_load(type)  \
   extern gpointer mname(type,begin_load) (ModulePreparedNotifyFunc prepare_func, \
				 ModuleUpdatedNotifyFunc update_func, \
				 ModuleFrameDoneNotifyFunc frame_done_func,\
				 ModuleAnimationDoneNotifyFunc anim_done_func,\
				 gpointer user_data);
#define m_stop_load(type)  extern void mname(type,stop_load) (gpointer context);
#define m_load_increment(type)  extern gboolean mname(type,load_increment) (gpointer context, const guchar *buf, guint size);
#define m_load_animation(type)  extern GdkPixbufAnimation * mname(type,load_animation) (FILE *f);

m_load (png);
m_begin_load (png);
m_load_increment (png);
m_stop_load (png);
m_load (bmp);
m_begin_load (bmp);
m_load_increment (bmp);
m_stop_load (bmp);
m_load (gif);
m_begin_load (gif);
m_load_increment (gif);
m_stop_load (gif);
m_load_animation (gif);
m_load (ico);
m_begin_load (ico);
m_load_increment (ico);
m_stop_load (ico);
m_load (jpeg);
m_begin_load (jpeg);
m_load_increment (jpeg);
m_stop_load (jpeg);
m_load (pnm);
m_begin_load (pnm);
m_load_increment (pnm);
m_stop_load (pnm);
m_load (ras);
m_begin_load (ras);
m_load_increment (ras);
m_stop_load (ras);
m_load (tiff);
m_begin_load (tiff);
m_load_increment (tiff);
m_stop_load (tiff);
m_load (xpm);
m_load_xpm_data (xpm);

void
gdk_pixbuf_load_module (GdkPixbufModule *image_module)
{
	image_module->module = (void *) 1;
	
	if (strcmp (image_module->module_name, "png") == 0){
		image_module->load           = mname (png,load);
		image_module->begin_load     = mname (png,begin_load);
		image_module->load_increment = mname (png,load_increment);
		image_module->stop_load      = mname (png,stop_load);
		return;
	}

	if (strcmp (image_module->module_name, "bmp") == 0){
		image_module->load           = mname (bmp,load);
		image_module->begin_load     = mname (bmp,begin_load);
		image_module->load_increment = mname (bmp,load_increment);
		image_module->stop_load      = mname (bmp,stop_load);
		return;
	}

	if (strcmp (image_module->module_name, "gif") == 0){
		image_module->load           = mname (gif,load);
		image_module->begin_load     = mname (gif,begin_load);
		image_module->load_increment = mname (gif,load_increment);
		image_module->stop_load      = mname (gif,stop_load);
		image_module->load_animation = mname (gif,load_animation);
		return;
	}

	if (strcmp (image_module->module_name, "ico") == 0){
		image_module->load           = mname (ico,load);
		image_module->begin_load     = mname (ico,begin_load);
		image_module->load_increment = mname (ico,load_increment);
		image_module->stop_load      = mname (ico,stop_load);
		return;
	}

	if (strcmp (image_module->module_name, "jpeg") == 0){
		image_module->load           = mname (jpeg,load);
		image_module->begin_load     = mname (jpeg,begin_load);
		image_module->load_increment = mname (jpeg,load_increment);
		image_module->stop_load      = mname (jpeg,stop_load);
		return;
	}
	if (strcmp (image_module->module_name, "pnm") == 0){
		image_module->load           = mname (pnm,load);
		image_module->begin_load     = mname (pnm,begin_load);
		image_module->load_increment = mname (pnm,load_increment);
		image_module->stop_load      = mname (pnm,stop_load);
		return;
	}
	if (strcmp (image_module->module_name, "ras") == 0){
		image_module->load           = mname (ras,load);
		image_module->begin_load     = mname (ras,begin_load);
		image_module->load_increment = mname (ras,load_increment);
		image_module->stop_load      = mname (ras,stop_load);
		return;
	}
	if (strcmp (image_module->module_name, "tiff") == 0){
		image_module->load           = mname (tiff,load);
		image_module->begin_load     = mname (tiff,begin_load);
		image_module->load_increment = mname (tiff,load_increment);
		image_module->stop_load      = mname (tiff,stop_load);
		return;
	}
	if (strcmp (image_module->module_name, "xpm") == 0){
		image_module->load           = mname (xpm,load);
		image_module->load_xpm_data  = mname (xpm,load_xpm_data);
		return;
	}
}


#endif



GdkPixbufModule *
gdk_pixbuf_get_module (guchar *buffer, guint size)
{
	int i;

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
	int size;
	FILE *f;
	guchar buffer [128];
	GdkPixbufModule *image_module;

	g_return_val_if_fail (filename != NULL, NULL);

	f = fopen (filename, "r");
	if (!f)
		return NULL;

	size = fread (&buffer, 1, sizeof (buffer), f);
	if (size == 0) {
		fclose (f);
		return NULL;
	}

	image_module = gdk_pixbuf_get_module (buffer, size);
	if (!image_module) {
		g_warning ("Unable to find handler for file: %s", filename);
		fclose (f);
		return NULL;
	}

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
		g_assert (pixbuf->ref_count > 0);

	return pixbuf;
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
gdk_pixbuf_new_from_xpm_data (const char **data)
{
	GdkPixbuf *(* load_xpm_data) (const char **data);
	GdkPixbuf *pixbuf;

	if (file_formats[XPM_FILE_FORMAT_INDEX].module == NULL)
		gdk_pixbuf_load_module (&file_formats[XPM_FILE_FORMAT_INDEX]);

	if (file_formats[XPM_FILE_FORMAT_INDEX].module == NULL) {
		g_warning ("Can't find gdk-pixbuf module for parsing inline XPM data");
		return NULL;
	} else if (file_formats[XPM_FILE_FORMAT_INDEX].load_xpm_data == NULL) {
		g_warning ("gdk-pixbuf XPM module lacks XPM data capability");
		return NULL;
	} else
		load_xpm_data = file_formats[XPM_FILE_FORMAT_INDEX].load_xpm_data;

	pixbuf = (* load_xpm_data) (data);
	return pixbuf;
}
