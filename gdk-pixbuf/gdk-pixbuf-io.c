/* GdkPixbuf library - Main loading interface.
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <string.h>
#include <glib.h>
#include <errno.h>
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

static gboolean
pixbuf_check_wbmp (guchar *buffer, int size)
{
  if(size < 10) /* at least */
    return FALSE;

  if(buffer[0] == '\0' /* We only handle type 0 wbmp's for now */)
    return TRUE;

  return FALSE;
}


static gboolean
pixbuf_check_xbm (guchar *buffer, int size)
{
	if (size < 20)
		return FALSE;

	if (buffer [0] != '#'
	    || buffer [1] != 'd'
	    || buffer [2] != 'e'
	    || buffer [3] != 'f'
	    || buffer [4] != 'i'
	    || buffer [5] != 'n'
	    || buffer [6] != 'e'
	    || buffer [7] != ' ')
		return FALSE;

	return TRUE;
}

static GdkPixbufModule file_formats [] = {
	{ "png",  pixbuf_check_png, NULL,  NULL, NULL, NULL, NULL, NULL, NULL, },
	{ "jpeg", pixbuf_check_jpeg, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "tiff", pixbuf_check_tiff, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "gif",  pixbuf_check_gif, NULL,  NULL, NULL, NULL, NULL, NULL, NULL },
#define XPM_FILE_FORMAT_INDEX 4
	{ "xpm",  pixbuf_check_xpm, NULL,  NULL, NULL, NULL, NULL, NULL, NULL },
	{ "pnm",  pixbuf_check_pnm, NULL,  NULL, NULL, NULL, NULL, NULL, NULL },
	{ "ras",  pixbuf_check_sunras, NULL,  NULL, NULL, NULL, NULL, NULL, NULL },
	{ "ico",  pixbuf_check_ico, NULL,  NULL, NULL, NULL, NULL, NULL, NULL },
	{ "bmp",  pixbuf_check_bmp, NULL,  NULL, NULL, NULL, NULL, NULL, NULL },
	{ "wbmp", pixbuf_check_wbmp, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "xbm",  pixbuf_check_xbm, NULL,  NULL, NULL, NULL, NULL, NULL, NULL },
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

#ifdef G_OS_WIN32

static char *
get_libdir (void)
{
  static char *libdir = NULL;

  if (libdir == NULL)
    libdir = g_win32_get_package_installation_subdirectory
      (GETTEXT_PACKAGE,
       g_strdup_printf ("gdk_pixbug-%d.%d.dll",
			GDK_PIXBUF_MAJOR, GDK_PIXBUF_MINOR),
       "loaders");

  return libdir;
}

#define PIXBUF_LIBDIR get_libdir ()

#endif

/* actually load the image handler - gdk_pixbuf_get_module only get a */
/* reference to the module to load, it doesn't actually load it       */
/* perhaps these actions should be combined in one function           */
gboolean
gdk_pixbuf_load_module (GdkPixbufModule *image_module,
                        GError         **error)
{
	char *module_name;
	char *path;
	GModule *module;
	gpointer sym;
	char *name;
        gboolean retval;
        char *dir;
	
        g_return_val_if_fail (image_module->module == NULL, FALSE);

	name = image_module->module_name;
	
	module_name = g_strconcat ("pixbufloader-", name, NULL);

        /* This would be an exploit in an suid binary using gdk-pixbuf,
         * but see http://www.gtk.org/setuid.html or the BugTraq
         * archives for why you should report this as a bug against
         * setuid apps using this library rather than the library
         * itself.
         */
        dir = g_getenv ("GDK_PIXBUF_MODULEDIR");

        if (dir == NULL || *dir == '\0') {
                
                path = g_module_build_path (PIXBUF_LIBDIR, module_name);
                module = g_module_open (path, G_MODULE_BIND_LAZY);
        } else {
                path = g_module_build_path (dir, module_name);
                module = g_module_open (path, G_MODULE_BIND_LAZY);                
        }        

        if (!module) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_FAILED,
                             _("Unable to load image-loading module: %s: %s"),
                             path, g_module_error ());
                g_free (module_name);
                g_free (path);
                return FALSE;
        }

        g_free (module_name);

	image_module->module = module;        
        
        if (pixbuf_module_symbol (module, name, "fill_vtable", &sym)) {
                ModuleFillVtableFunc func = sym;
                (* func) (image_module);
                retval = TRUE;
        } else {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_FAILED,
                             _("Image-loading module %s does not export the proper interface; perhaps it's from a different GTK version?"),
                             path);
                retval = FALSE;
        }

        g_free (path);

        return retval;
}
#else

#define mname(type,fn) gdk_pixbuf__ ## type ## _image_ ##fn
#define m_fill_vtable(type) extern void mname(type,fill_vtable) (GdkPixbufModule *module)

m_fill_vtable (png);
m_fill_vtable (bmp);
m_fill_vtable (wbmp);
m_fill_vtable (gif);
m_fill_vtable (ico);
m_fill_vtable (jpeg);
m_fill_vtable (pnm);
m_fill_vtable (ras);
m_fill_vtable (tiff);
m_fill_vtable (xpm);
m_fill_vtable (xbm);

gboolean
gdk_pixbuf_load_module (GdkPixbufModule *image_module,
                        GError         **error)
{
        ModuleFillVtableFunc fill_vtable = NULL;
	image_module->module = (void *) 1;

        if (FALSE) {
                /* Ugly hack so we can use else if unconditionally below ;-) */
        }
        
#ifdef INCLUDE_png	
	else if (strcmp (image_module->module_name, "png") == 0){
                fill_vtable = mname (png, fill_vtable);
	}
#endif

#ifdef INCLUDE_bmp	
	else if (strcmp (image_module->module_name, "bmp") == 0){
                fill_vtable = mname (bmp, fill_vtable);
	}
#endif

#ifdef INCLUDE_wbmp
	else if (strcmp (image_module->module_name, "wbmp") == 0){
                fill_vtable = mname (wbmp, fill_vtable);
	}
#endif

#ifdef INCLUDE_gif
	else if (strcmp (image_module->module_name, "gif") == 0){
                fill_vtable = mname (gif, fill_vtable);
	}
#endif

#ifdef INCLUDE_ico
	else if (strcmp (image_module->module_name, "ico") == 0){
                fill_vtable = mname (ico, fill_vtable);
	}
#endif

#ifdef INCLUDE_jpeg
	else if (strcmp (image_module->module_name, "jpeg") == 0){
                fill_vtable = mname (jpeg, fill_vtable);
	}
#endif

#ifdef INCLUDE_pnm
	else if (strcmp (image_module->module_name, "pnm") == 0){
                fill_vtable = mname (pnm, fill_vtable);
	}
#endif

#ifdef INCLUDE_ras
	else if (strcmp (image_module->module_name, "ras") == 0){
                fill_vtable = mname (ras, fill_vtable);
	}
#endif

#ifdef INCLUDE_tiff
	else if (strcmp (image_module->module_name, "tiff") == 0){
                fill_vtable = mname (tiff, fill_vtable);
	}
#endif

#ifdef INCLUDE_xpm
	else if (strcmp (image_module->module_name, "xpm") == 0){
                fill_vtable = mname (xpm, fill_vtable);
	}
#endif

#ifdef INCLUDE_xbm
	else if (strcmp (image_module->module_name, "xbm") == 0){
                fill_vtable = mname (xbm, fill_vtable);
	}
#endif

        
        if (fill_vtable) {
                (* fill_vtable) (image_module);
                return TRUE;
        } else {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                             _("Image type '%s' is not supported"),
                             image_module->module_name);

                return FALSE;
        }
}


#endif



GdkPixbufModule *
gdk_pixbuf_get_named_module (const char *name,
                             GError **error)
{
	int i;

	for (i = 0; file_formats [i].module_name; i++) {
		if (!strcmp(name, file_formats[i].module_name))
			return &(file_formats[i]);
	}

        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                     _("Image type '%s' is not supported"),
                     name);
        
	return NULL;
}

GdkPixbufModule *
gdk_pixbuf_get_module (guchar *buffer, guint size,
                       const gchar *filename,
                       GError **error)
{
	int i;

	for (i = 0; file_formats [i].module_name; i++) {
		if ((* file_formats [i].format_check) (buffer, size))
			return &(file_formats[i]);
	}

        if (filename)
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                             _("Couldn't recognize the image file format for file '%s'"),
                             filename);        
        else
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                             _("Unrecognized image file format"));

        
	return NULL;
}

/**
 * gdk_pixbuf_new_from_file:
 * @filename: Name of file to load.
 * @error: Return location for an error
 *
 * Creates a new pixbuf by loading an image from a file.  The file format is
 * detected automatically. If NULL is returned, then @error will be set.
 * Possible errors are in the #GDK_PIXBUF_ERROR and #G_FILE_ERROR domains.
 *
 * Return value: A newly-created pixbuf with a reference count of 1, or NULL if
 * any of several error conditions occurred:  the file could not be opened,
 * there was no loader for the file's format, there was not enough memory to
 * allocate the image buffer, or the image file contained invalid data.
 **/
GdkPixbuf *
gdk_pixbuf_new_from_file (const char *filename,
                          GError    **error)
{
	GdkPixbuf *pixbuf;
	int size;
	FILE *f;
	guchar buffer [128];
	GdkPixbufModule *image_module;

	g_return_val_if_fail (filename != NULL, NULL);

	f = fopen (filename, "rb");
	if (!f) {
                g_set_error (error,
                             G_FILE_ERROR,
                             g_file_error_from_errno (errno),
                             _("Failed to open file '%s': %s"),
                             filename, g_strerror (errno));
		return NULL;
        }

	size = fread (&buffer, 1, sizeof (buffer), f);
	if (size == 0) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Image file '%s' contains no data"),
                             filename);
                
		fclose (f);
		return NULL;
	}

	image_module = gdk_pixbuf_get_module (buffer, size, filename, error);
        if (image_module == NULL) {
                fclose (f);
                return NULL;
        }

	if (image_module->module == NULL)
		if (!gdk_pixbuf_load_module (image_module, error)) {
                        fclose (f);
                        return NULL;
                }

	if (image_module->load == NULL) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_UNSUPPORTED_OPERATION,
                             _("Don't know how to load the image in file '%s'"),
                             filename);
                
		fclose (f);
		return NULL;
	}

	fseek (f, 0, SEEK_SET);
	pixbuf = (* image_module->load) (f, error);
	fclose (f);

        if (pixbuf == NULL && error != NULL && *error == NULL) {
                /* I don't trust these crufty longjmp()'ing image libs
                 * to maintain proper error invariants, and I don't
                 * want user code to segfault as a result. We need to maintain
                 * the invariant that error gets set if NULL is returned.
                 */
                
                g_warning ("Bug! gdk-pixbuf loader '%s' didn't set an error on failure.", image_module->module_name);
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_FAILED,
                             _("Failed to load image '%s': reason not known, probably a corrupt image file"),
                             filename);
                
        } else if (error != NULL && *error != NULL) {

          /* Add the filename to the error message */
          GError *e = *error;
          gchar *old;
          
          old = e->message;

          e->message = g_strdup_printf (_("Failed to load image '%s': %s"),
                                        filename, old);

          g_free (old);
        }
                
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
		gdk_pixbuf_load_module (&file_formats[XPM_FILE_FORMAT_INDEX], NULL);

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

static void
collect_save_options (va_list   opts,
                      gchar  ***keys,
                      gchar  ***vals)
{
  gchar *key;
  gchar *val;
  gchar *next;
  gint count;

  count = 0;
  *keys = NULL;
  *vals = NULL;
  
  next = va_arg (opts, gchar*);
  while (next)
    {
      key = next;
      val = va_arg (opts, gchar*);

      ++count;

      /* woo, slow */
      *keys = g_realloc (*keys, sizeof(gchar*) * (count + 1));
      *vals = g_realloc (*vals, sizeof(gchar*) * (count + 1));
      
      (*keys)[count-1] = g_strdup (key);
      (*vals)[count-1] = g_strdup (val);

      (*keys)[count] = NULL;
      (*vals)[count] = NULL;
      
      next = va_arg (opts, gchar*);
    }
}

static gboolean
gdk_pixbuf_real_save (GdkPixbuf     *pixbuf, 
                      FILE          *filehandle, 
                      const char    *type, 
                      gchar        **keys,
                      gchar        **values,
                      GError       **error)
{
       GdkPixbufModule *image_module = NULL;       

       image_module = gdk_pixbuf_get_named_module (type, error);

       if (image_module == NULL)
               return FALSE;
       
       if (image_module->module == NULL)
               if (!gdk_pixbuf_load_module (image_module, error))
                       return FALSE;

       if (image_module->save == NULL) {
               g_set_error (error,
                            GDK_PIXBUF_ERROR,
                            GDK_PIXBUF_ERROR_UNSUPPORTED_OPERATION,
                            _("This build of gdk-pixbuf does not support saving the image format: %s"),
                            type);
               return FALSE;
       }
               
       return (* image_module->save) (filehandle, pixbuf,
                                      keys, values,
                                      error);
}

 
/**
 * gdk_pixbuf_save:
 * @pixbuf: pointer to GdkPixbuf.
 * @filename: Name of file to save.
 * @type: name of file format.
 * @error: return location for error, or NULL
 * @Varargs: list of key-value save options
 *
 * Saves pixbuf to a file in @type, which is currently "jpeg" or
 * "png".  If @error is set, FALSE will be returned. Possible errors include those
 * in the #GDK_PIXBUF_ERROR domain and those in the #G_FILE_ERROR domain.
 *
 * The variable argument list should be NULL-terminated; if not empty,
 * it should contain pairs of strings that modify the save
 * parameters. For example:
 *
 * <programlisting>
 * gdk_pixbuf_save (pixbuf, handle, "jpeg", &error,
 *                  "quality", "100", NULL);
 * </programlisting>
 *
 * The only save parameter that currently exists is the "quality" field
 * for JPEG images; its value should be in the range [0,100].
 *
 * Return value: whether an error was set
 **/

gboolean
gdk_pixbuf_save (GdkPixbuf  *pixbuf, 
                 const char *filename, 
                 const char *type, 
                 GError    **error,
                 ...)
{
        gchar **keys = NULL;
        gchar **values = NULL;
        va_list args;
        gboolean result;
        
        va_start (args, error);
        
        collect_save_options (args, &keys, &values);
        
        va_end (args);

        result = gdk_pixbuf_savev (pixbuf, filename, type,
                                   keys, values,
                                   error);

        g_strfreev (keys);
        g_strfreev (values);

        return result;
}

/**
 * gdk_pixbuf_savev:
 * @pixbuf: pointer to GdkPixbuf.
 * @filename: Name of file to save.
 * @type: name of file format.
 * @option_keys: name of options to set, NULL-terminated
 * @option_values: values for named options
 * @error: return location for error, or NULL
 *
 * Saves pixbuf to a file in @type, which is currently "jpeg" or "png".
 * If @error is set, FALSE will be returned. See gdk_pixbuf_save () for more
 * details.
 *
 * Return value: whether an error was set
 **/

gboolean
gdk_pixbuf_savev (GdkPixbuf  *pixbuf, 
                  const char *filename, 
                  const char *type,
                  char      **option_keys,
                  char      **option_values,
                  GError    **error)
{
        FILE *f = NULL;
        gboolean result;
        
       
        g_return_val_if_fail (filename != NULL, FALSE);
        g_return_val_if_fail (type != NULL, FALSE);
       
        f = fopen (filename, "wb");
        
        if (f == NULL) {
                g_set_error (error,
                             G_FILE_ERROR,
                             g_file_error_from_errno (errno),
                             _("Failed to open '%s' for writing: %s"),
                             filename, g_strerror (errno));
                return FALSE;
        }

       
       result = gdk_pixbuf_real_save (pixbuf, f, type,
                                      option_keys, option_values,
                                      error);
       
       
       if (!result) {
               g_return_val_if_fail (error == NULL || *error != NULL, FALSE);
               fclose (f);
               return FALSE;
       }

       if (fclose (f) < 0) {
               g_set_error (error,
                            G_FILE_ERROR,
                            g_file_error_from_errno (errno),
                            _("Failed to close '%s' while writing image, all data may not have been saved: %s"),
                            filename, g_strerror (errno));
               return FALSE;
       }
       
       return TRUE;
}
