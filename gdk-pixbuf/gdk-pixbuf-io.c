/* -*- mode: C; c-file-style: "linux" -*- */
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <errno.h>
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-io.h"

#ifdef G_OS_WIN32
#define STRICT
#include <windows.h>
#undef STRICT
#endif

static gint 
format_check (GdkPixbufModule *module, guchar *buffer, int size)
{
	int j;
	gchar m;
	GdkPixbufModulePattern *pattern;

	for (pattern = module->info->signature; pattern->prefix; pattern++) {
		for (j = 0; j < size && pattern->prefix[j] != 0; j++) {
			m = pattern->mask ? pattern->mask[j] : ' ';
			if (m == ' ') {
				if (buffer[j] != pattern->prefix[j])
					break;
			}
			else if (m == '!') {
				if (buffer[j] == pattern->prefix[j])
					break;
			}
			else if (m == 'z') {
				if (buffer[j] != 0)
					break;
			}
			else if (m == 'n') {
				if (buffer[j] == 0)
					break;
			}
		} 
		if (pattern->prefix[j] == 0) 
			return pattern->relevance;
	}
	return 0;
}

static GSList *file_formats = NULL;

static void gdk_pixbuf_io_init ();

static GSList *
get_file_formats ()
{
	if (file_formats == NULL)
		gdk_pixbuf_io_init ();
	
	return file_formats;
}


#ifdef USE_GMODULE 

static gboolean
scan_string (const char **pos, GString *out)
{
	const char *p = *pos, *q = *pos;
	char *tmp, *tmp2;
	gboolean quoted;
	
	while (g_ascii_isspace (*p))
		p++;
	
	if (!*p)
		return FALSE;
	else if (*p == '"') {
		p++;
		quoted = FALSE;
		for (q = p; (*q != '"') || quoted; q++) {
			if (!*q)
				return FALSE;
			quoted = (*q == '\\') && !quoted;
		}
		
		tmp = g_strndup (p, q - p);
		tmp2 = g_strcompress (tmp);
		g_string_truncate (out, 0);
		g_string_append (out, tmp2);
		g_free (tmp);
		g_free (tmp2);
	}
	
	q++;
	*pos = q;
	
	return TRUE;
}

static gboolean
scan_int (const char **pos, int *out)
{
	int i = 0;
	char buf[32];
	const char *p = *pos;
	
	while (g_ascii_isspace (*p))
		p++;
	
	if (*p < '0' || *p > '9')
		return FALSE;
	
	while ((*p >= '0') && (*p <= '9') && i < sizeof (buf)) {
		buf[i] = *p;
		i++;
		p++;
	}
	
	if (i == sizeof (buf))
  		return FALSE;
	else
		buf[i] = '\0';
	
	*out = atoi (buf);
	
	*pos = p;

	return TRUE;
}

static gboolean
skip_space (const char **pos)
{
	const char *p = *pos;
	
	while (g_ascii_isspace (*p))
		p++;
  
	*pos = p;
	
	return !(*p == '\0');
}
  
static gchar *
gdk_pixbuf_get_module_file (void)
{
  gchar *result = g_strdup (g_getenv ("GDK_PIXBUF_MODULE_FILE"));

  if (!result)
	  result = g_build_filename (GTK_SYSCONFDIR, "gtk-2.0", "gdk-pixbuf.loaders", NULL);

  return result;
}

static void 
gdk_pixbuf_io_init ()
{
	GIOChannel *channel;
	gchar *line_buf;
	gsize term;
	GString *tmp_buf = g_string_new (NULL);
	gboolean have_error = FALSE;
	GdkPixbufModule *module = NULL;
	gchar *filename = gdk_pixbuf_get_module_file ();
	int flags;
	int n_patterns = 0;
	GdkPixbufModulePattern *pattern;
	GError *error = NULL;

	channel = g_io_channel_new_file (filename, "r",  &error);
	if (!channel) {
		g_warning ("Can not open pixbuf loader module file '%s': %s",
			   filename, error->message);
		return;
	}
	
	while (!have_error && g_io_channel_read_line (channel, &line_buf, NULL, &term, NULL) == G_IO_STATUS_NORMAL) {
		const char *p;
		
		p = line_buf;

		line_buf[term] = 0;

		if (!skip_space (&p)) {
				/* Blank line marking the end of a module
				 */
			if (module && *p != '#') {
				file_formats = g_slist_prepend (file_formats, module);
				module = NULL;
			}
			
			goto next_line;
		}

		if (*p == '#') 
			goto next_line;
		
		if (!module) {
				/* Read a module location
				 */
			module = g_new0 (GdkPixbufModule, 1);
			n_patterns = 0;
			
			if (!scan_string (&p, tmp_buf)) {
				g_warning ("Error parsing loader info in '%s'\n  %s", 
					   filename, line_buf);
				have_error = TRUE;
			}
			module->module_path = g_strdup (tmp_buf->str);
		}
		else if (!module->module_name) {
			module->info = g_new0 (GdkPixbufFormat, 1);
			if (!scan_string (&p, tmp_buf)) {
				g_warning ("Error parsing loader info in '%s'\n  %s", 
					   filename, line_buf);
				have_error = TRUE;
			}
			module->info->name =  g_strdup (tmp_buf->str);
			module->module_name = module->info->name;

			if (!scan_int (&p, &flags)) {
				g_warning ("Error parsing loader info in '%s'\n  %s", 
					   filename, line_buf);
				have_error = TRUE;
			}
			module->info->flags = flags;
			
			if (!scan_string (&p, tmp_buf)) {
				g_warning ("Error parsing loader info in '%s'\n  %s", 
					   filename, line_buf);
				have_error = TRUE;
			}			
			if (tmp_buf->str[0] != 0)
				module->info->domain = g_strdup (tmp_buf->str);

			if (!scan_string (&p, tmp_buf)) {
				g_warning ("Error parsing loader info in '%s'\n  %s", 
					   filename, line_buf);
				have_error = TRUE;
			}			
			module->info->description = g_strdup (tmp_buf->str);
		}
		else if (!module->info->mime_types) {
			int n = 1;
			module->info->mime_types = g_new0 (gchar*, 1);
			while (scan_string (&p, tmp_buf)) {
				if (tmp_buf->str[0] != 0) {
					module->info->mime_types =
						g_realloc (module->info->mime_types, (n + 1) * sizeof (gchar*));
					module->info->mime_types[n - 1] = g_strdup (tmp_buf->str);
					module->info->mime_types[n] = 0;
					n++;
				}
			}
		}
		else if (!module->info->extensions) {
			int n = 1;
			module->info->extensions = g_new0 (gchar*, 1);
			while (scan_string (&p, tmp_buf)) {
				if (tmp_buf->str[0] != 0) {
					module->info->extensions =
						g_realloc (module->info->extensions, (n + 1) * sizeof (gchar*));
					module->info->extensions[n - 1] = g_strdup (tmp_buf->str);
					module->info->extensions[n] = 0;
					n++;
				}
			}
		}
		else {
			n_patterns++;
			module->info->signature = (GdkPixbufModulePattern *)
				g_realloc (module->info->signature, (n_patterns + 1) * sizeof (GdkPixbufModulePattern));
			pattern = module->info->signature + n_patterns;
			pattern->prefix = NULL;
			pattern->mask = NULL;
			pattern->relevance = 0;
			pattern--;
			if (!scan_string (&p, tmp_buf))
				goto context_error;
			pattern->prefix = g_strdup (tmp_buf->str);
			
			if (!scan_string (&p, tmp_buf))
				goto context_error;
			if (*tmp_buf->str)
				pattern->mask = g_strdup (tmp_buf->str);
			else
				pattern->mask = NULL;
			
			if (!scan_int (&p, &pattern->relevance))
				goto context_error;
			
			goto next_line;

		context_error:
			g_free (pattern->prefix);
			g_free (pattern->mask);
			g_free (pattern);
			g_warning ("Error parsing loader info in '%s'\n  %s", 
				   filename, line_buf);
			have_error = TRUE;
		}
	next_line:
		g_free (line_buf);
	}
	g_string_free (tmp_buf, TRUE);
	g_io_channel_unref (channel);
	g_free (filename);
}

#ifdef G_OS_WIN32

/* DllMain function needed to tuck away the gdk-pixbuf DLL name */

G_WIN32_DLLMAIN_FOR_DLL_NAME (static, dll_name)

static char *
get_libdir (void)
{
  static char *libdir = NULL;

  if (libdir == NULL)
    libdir = g_win32_get_package_installation_subdirectory
      (GETTEXT_PACKAGE, dll_name, "lib\\gtk-2.0\\" GTK_BINARY_VERSION "\\loaders");

  return libdir;
}

#undef PIXBUF_LIBDIR
#define PIXBUF_LIBDIR get_libdir ()

#endif

/* actually load the image handler - gdk_pixbuf_get_module only get a */
/* reference to the module to load, it doesn't actually load it       */
/* perhaps these actions should be combined in one function           */
gboolean
_gdk_pixbuf_load_module (GdkPixbufModule *image_module,
                         GError         **error)
{
	char *path;
	GModule *module;
	gpointer sym;
	
        g_return_val_if_fail (image_module->module == NULL, FALSE);

	path = image_module->module_path;
	module = g_module_open (path, G_MODULE_BIND_LAZY);

        if (!module) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_FAILED,
                             _("Unable to load image-loading module: %s: %s"),
                             path, g_module_error ());
                return FALSE;
        }

	image_module->module = module;        
        
        if (g_module_symbol (module, "fill_vtable", &sym)) {
                GdkPixbufModuleFillVtableFunc func = (GdkPixbufModuleFillVtableFunc) sym;
                (* func) (image_module);
                return TRUE;
        } else {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_FAILED,
                             _("Image-loading module %s does not export the proper interface; perhaps it's from a different GTK version?"),
                             path);
                return FALSE;
        }
}
#else

#define module(type) \
  extern void MODULE_ENTRY (type, fill_info)   (GdkPixbufFormat *info);   \
  extern void MODULE_ENTRY (type, fill_vtable) (GdkPixbufModule *module)

module (png);
module (bmp);
module (wbmp);
module (gif);
module (ico);
module (ani);
module (jpeg);
module (pnm);
module (ras);
module (tiff);
module (xpm);
module (xbm);
module (tga);
module (pcx);

gboolean
_gdk_pixbuf_load_module (GdkPixbufModule *image_module,
                         GError         **error)
{
	GdkPixbufModuleFillInfoFunc fill_info = NULL;
        GdkPixbufModuleFillVtableFunc fill_vtable = NULL;

	image_module->module = (void *) 1;

        if (FALSE) {
                /* Ugly hack so we can use else if unconditionally below ;-) */
        }
        
#ifdef INCLUDE_png	
	else if (strcmp (image_module->module_name, "png") == 0) {
                fill_info = MODULE_ENTRY (png, fill_info);
                fill_vtable = MODULE_ENTRY (png, fill_vtable);
	}
#endif

#ifdef INCLUDE_bmp	
	else if (strcmp (image_module->module_name, "bmp") == 0) {
                fill_info = MODULE_ENTRY (bmp, fill_info);
                fill_vtable = MODULE_ENTRY (bmp, fill_vtable);
	}
#endif

#ifdef INCLUDE_wbmp
	else if (strcmp (image_module->module_name, "wbmp") == 0) {
                fill_info = MODULE_ENTRY (wbmp, fill_info);
                fill_vtable = MODULE_ENTRY (wbmp, fill_vtable);
	}
#endif

#ifdef INCLUDE_gif
	else if (strcmp (image_module->module_name, "gif") == 0) {
                fill_info = MODULE_ENTRY (gif, fill_info);
                fill_vtable = MODULE_ENTRY (gif, fill_vtable);
	}
#endif

#ifdef INCLUDE_ico
	else if (strcmp (image_module->module_name, "ico") == 0) {
                fill_info = MODULE_ENTRY (ico, fill_info);
                fill_vtable = MODULE_ENTRY (ico, fill_vtable);
	}
#endif

#ifdef INCLUDE_ani
	else if (strcmp (image_module->module_name, "ani") == 0) {
                fill_info = MODULE_ENTRY (ani, fill_info);
                fill_vtable = MODULE_ENTRY (ani, fill_vtable);
	}
#endif

#ifdef INCLUDE_jpeg
	else if (strcmp (image_module->module_name, "jpeg") == 0) {
                fill_info = MODULE_ENTRY (jpeg, fill_info);
                fill_vtable = MODULE_ENTRY (jpeg, fill_vtable);
	}
#endif

#ifdef INCLUDE_pnm
	else if (strcmp (image_module->module_name, "pnm") == 0) {
                fill_info = MODULE_ENTRY (pnm, fill_info);
                fill_vtable = MODULE_ENTRY (pnm, fill_vtable);
	}
#endif

#ifdef INCLUDE_ras
	else if (strcmp (image_module->module_name, "ras") == 0) {
                fill_info = MODULE_ENTRY (ras, fill_info);
                fill_vtable = MODULE_ENTRY (ras, fill_vtable);
	}
#endif

#ifdef INCLUDE_tiff
	else if (strcmp (image_module->module_name, "tiff") == 0) {
                fill_info = MODULE_ENTRY (tiff, fill_info);
                fill_vtable = MODULE_ENTRY (tiff, fill_vtable);
	}
#endif

#ifdef INCLUDE_xpm
	else if (strcmp (image_module->module_name, "xpm") == 0) {
                fill_info = MODULE_ENTRY (xpm, fill_info);
                fill_vtable = MODULE_ENTRY (xpm, fill_vtable);
	}
#endif

#ifdef INCLUDE_xbm
	else if (strcmp (image_module->module_name, "xbm") == 0) {
                fill_info = MODULE_ENTRY (xbm, fill_info);
                fill_vtable = MODULE_ENTRY (xbm, fill_vtable);
	}
#endif

#ifdef INCLUDE_tga
	else if (strcmp (image_module->module_name, "tga") == 0) {
                fill_info = MODULE_ENTRY (tga, fill_info);
		fill_vtable = MODULE_ENTRY (tga, fill_vtable);
	}
#endif
        
#ifdef INCLUDE_pcx
	else if (strcmp (image_module->module_name, "pcx") == 0) {
                fill_info = MODULE_ENTRY (pcx, fill_info);
		fill_vtable = MODULE_ENTRY (pcx, fill_vtable);
	}
#endif
        
        if (fill_vtable) {
                (* fill_vtable) (image_module);
		image_module->info = g_new0 (GdkPixbufFormat, 1);
		(* fill_info) (image_module->info);

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

static void 
gdk_pixbuf_io_init ()
{
	gchar *included_formats[] = { 
		"ani", "png", "bmp", "wbmp", "gif", 
		"ico", "jpeg", "pnm", "ras", "tiff", 
		"xpm", "xbm", "tga", "pcx",
		NULL
	};
	gchar **name;
	GdkPixbufModule *module = NULL;
	
	for (name = included_formats; *name; name++) {
		module = g_new0 (GdkPixbufModule, 1);
		module->module_name = *name;
		if (_gdk_pixbuf_load_module (module, NULL))
			file_formats = g_slist_prepend (file_formats, module);
		else
			g_free (module);
	}
}

#endif



GdkPixbufModule *
_gdk_pixbuf_get_named_module (const char *name,
                              GError **error)
{
	GSList *modules;

	for (modules = get_file_formats (); modules; modules = g_slist_next (modules)) {
		GdkPixbufModule *module = (GdkPixbufModule *)modules->data;
		if (!strcmp (name, module->module_name))
			return module;
	}

        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                     _("Image type '%s' is not supported"),
                     name);
        
	return NULL;
}

GdkPixbufModule *
_gdk_pixbuf_get_module (guchar *buffer, guint size,
                        const gchar *filename,
                        GError **error)
{
	GSList *modules;

	gint score, best = 0;
	GdkPixbufModule *selected = NULL;
	for (modules = get_file_formats (); modules; modules = g_slist_next (modules)) {
		GdkPixbufModule *module = (GdkPixbufModule *)modules->data;
		score = format_check (module, buffer, size);
		if (score > best) {
			best = score; 
			selected = module;
		}
		if (score >= 100) 
			break;
	}
	if (selected != NULL)
		return selected;

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


static void
prepared_notify (GdkPixbuf *pixbuf, 
		 GdkPixbufAnimation *anim, 
		 gpointer user_data)
{
	if (pixbuf != NULL)
		g_object_ref (pixbuf);
	*((GdkPixbuf **)user_data) = pixbuf;
}

GdkPixbuf *
_gdk_pixbuf_generic_image_load (GdkPixbufModule *module,
				FILE *f,
				GError **error)
{
	guchar buffer[4096];
	size_t length;
	GdkPixbuf *pixbuf = NULL;
	gpointer context;

	if (module->load != NULL)
		return (* module->load) (f, error);
	
	context = module->begin_load (NULL, prepared_notify, NULL, &pixbuf, error);
	
	if (!context)
		return NULL;
	
	while (!feof (f)) {
		length = fread (buffer, 1, sizeof (buffer), f);
		if (length > 0)
			if (!module->load_increment (context, buffer, length, error)) {
				module->stop_load (context, NULL);
				if (pixbuf != NULL)
					g_object_unref (pixbuf);
				return NULL;
			}
	}

	if (!module->stop_load (context, error)) {
		if (pixbuf != NULL)
			g_object_unref (pixbuf);
		return NULL;
	}
	
	return pixbuf;
}

/**
 * gdk_pixbuf_new_from_file:
 * @filename: Name of file to load.
 * @error: Return location for an error
 *
 * Creates a new pixbuf by loading an image from a file.  The file format is
 * detected automatically. If %NULL is returned, then @error will be set.
 * Possible errors are in the #GDK_PIXBUF_ERROR and #G_FILE_ERROR domains.
 *
 * Return value: A newly-created pixbuf with a reference count of 1, or %NULL if
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
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);

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

	image_module = _gdk_pixbuf_get_module (buffer, size, filename, error);
        if (image_module == NULL) {
                fclose (f);
                return NULL;
        }

	if (image_module->module == NULL)
                if (!_gdk_pixbuf_load_module (image_module, error)) {
                        fclose (f);
                        return NULL;
                }

	fseek (f, 0, SEEK_SET);
	pixbuf = _gdk_pixbuf_generic_image_load (image_module, f, error);
	fclose (f);

        if (pixbuf == NULL && error != NULL && *error == NULL) {
                /* I don't trust these crufty longjmp()'ing image libs
                 * to maintain proper error invariants, and I don't
                 * want user code to segfault as a result. We need to maintain
                 * the invariastable/gdk-pixbuf/nt that error gets set if NULL is returned.
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
 * gdk_pixbuf_new_from_file_at_size:
 * @filename: Name of file to load.
 * @width: The width the image should have
 * @height: The height the image should have
 * @error: Return location for an error
 *
 * Creates a new pixbuf by loading an image from a file.  The file format is
 * detected automatically. If %NULL is returned, then @error will be set.
 * Possible errors are in the #GDK_PIXBUF_ERROR and #G_FILE_ERROR domains.
 * The image will be scaled to the requested size.
 *
 * Return value: A newly-created pixbuf with a reference count of 1, or %NULL if
 * any of several error conditions occurred:  the file could not be opened,
 * there was no loader for the file's format, there was not enough memory to
 * allocate the image buffer, or the image file contained invalid data.
 *
 * Since: 2.4
 **/
GdkPixbuf *
gdk_pixbuf_new_from_file_at_size (const char *filename,
				  int         width, 
				  int         height,
				  GError    **error)
{
	GdkPixbufLoader *loader;
	GdkPixbuf       *pixbuf;

	guchar buffer [4096];
	int length;
	FILE *f;

	g_return_val_if_fail (filename != NULL, NULL);
        g_return_val_if_fail (width > 0 && height > 0, NULL);

	f = fopen (filename, "rb");
	if (!f) {
                g_set_error (error,
                             G_FILE_ERROR,
                             g_file_error_from_errno (errno),
                             _("Failed to open file '%s': %s"),
                             filename, g_strerror (errno));
		return NULL;
        }

	loader = gdk_pixbuf_loader_new ();
	gdk_pixbuf_loader_set_size (loader, width, height);

	while (!feof (f)) {
		length = fread (buffer, 1, sizeof (buffer), f);
		if (length > 0)
			if (!gdk_pixbuf_loader_write (loader, buffer, length, error)) {
				gdk_pixbuf_loader_close (loader, NULL);
				fclose (f);
				g_object_unref (G_OBJECT (loader));
				return NULL;
			}
	}

	fclose (f);

	if (!gdk_pixbuf_loader_close (loader, error)) {
		g_object_unref (G_OBJECT (loader));
		return NULL;
	}

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	if (!pixbuf) {
		g_object_unref (G_OBJECT (loader));
		g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_FAILED,
                             _("Failed to load image '%s': reason not known, probably a corrupt image file"),
                             filename);
		return NULL;
	}

	g_object_ref (pixbuf);

	g_object_unref (G_OBJECT (loader));

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
        GError *error = NULL;
	GdkPixbufModule *xpm_module = _gdk_pixbuf_get_named_module ("xpm", &error);
	if (xpm_module == NULL) {
		g_warning ("Error loading XPM image loader: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	if (xpm_module->module == NULL) {
                if (!_gdk_pixbuf_load_module (xpm_module, &error)) {
                        g_warning ("Error loading XPM image loader: %s", error->message);
                        g_error_free (error);
                        return NULL;
                }
        }
          
	if (xpm_module->load_xpm_data == NULL) {
		g_warning ("gdk-pixbuf XPM module lacks XPM data capability");
		return NULL;
	} else
		load_xpm_data = xpm_module->load_xpm_data;

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

       image_module = _gdk_pixbuf_get_named_module (type, error);

       if (image_module == NULL)
               return FALSE;
       
       if (image_module->module == NULL)
               if (!_gdk_pixbuf_load_module (image_module, error))
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
 * @pixbuf: a #GdkPixbuf.
 * @filename: name of file to save.
 * @type: name of file format.
 * @error: return location for error, or %NULL
 * @Varargs: list of key-value save options
 *
 * Saves pixbuf to a file in @type, which is currently "jpeg", "png" or
 * "ico".  If @error is set, %FALSE will be returned. Possible errors include 
 * those in the #GDK_PIXBUF_ERROR domain and those in the #G_FILE_ERROR domain.
 *
 * The variable argument list should be %NULL-terminated; if not empty,
 * it should contain pairs of strings that modify the save
 * parameters. For example:
 * <informalexample><programlisting>
 * gdk_pixbuf_save (pixbuf, handle, "jpeg", &amp;error,
 *                  "quality", "100", NULL);
 * </programlisting></informalexample>
 *
 * Currently only few parameters exist. JPEG images can be saved with a 
 * "quality" parameter; its value should be in the range [0,100]. 
 * Text chunks can be attached to PNG images by specifying parameters of
 * the form "tEXt::key", where key is an ASCII string of length 1-79.
 * The values are UTF-8 encoded strings. 
 * ICO images can be saved in depth 16, 24, or 32, by using the "depth"
 * parameter. When the ICO saver is given "x_hot" and "y_hot" parameters,
 * it produces a CUR instead of an ICO.
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

        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
        
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
 * @pixbuf: a #GdkPixbuf.
 * @filename: name of file to save.
 * @type: name of file format.
 * @option_keys: name of options to set, %NULL-terminated
 * @option_values: values for named options
 * @error: return location for error, or %NULL
 *
 * Saves pixbuf to a file in @type, which is currently "jpeg" or "png".
 * If @error is set, %FALSE will be returned. See gdk_pixbuf_save () for more
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
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
       
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

/**
 * gdk_pixbuf_format_get_name:
 * @format: a #GdkPixbufFormat
 *
 * Returns the name of the format.
 * 
 * Return value: the name of the format. 
 *
 * Since: 2.2
 */
gchar *
gdk_pixbuf_format_get_name (GdkPixbufFormat *format)
{
	g_return_val_if_fail (format != NULL, NULL);

	return g_strdup (format->name);
}

/**
 * gdk_pixbuf_format_get_description:
 * @format: a #GdkPixbufFormat
 *
 * Returns a description of the format.
 * 
 * Return value: a description of the format.
 *
 * Since: 2.2
 */
gchar *
gdk_pixbuf_format_get_description (GdkPixbufFormat *format)
{
	gchar *domain;
	gchar *description;
	g_return_val_if_fail (format != NULL, NULL);

	if (format->domain != NULL) 
		domain = format->domain;
	else 
		domain = GETTEXT_PACKAGE;
	description = dgettext (domain, format->description);

	return g_strdup (description);
}

/**
 * gdk_pixbuf_format_get_mime_types:
 * @format: a #GdkPixbufFormat
 *
 * Returns the mime types supported by the format.
 * 
 * Return value: a %NULL-terminated array of mime types which must be freed with 
 * g_strfreev() when it is no longer needed.
 *
 * Since: 2.2
 */
gchar **
gdk_pixbuf_format_get_mime_types (GdkPixbufFormat *format)
{
	g_return_val_if_fail (format != NULL, NULL);

	return g_strdupv (format->mime_types);
}

/**
 * gdk_pixbuf_format_get_extensions:
 * @format: a #GdkPixbufFormat
 *
 * Returns the filename extensions typically used for files in the 
 * given format.
 * 
 * Return value: a %NULL-terminated array of filename extensions which must be
 * freed with g_strfreev() when it is no longer needed.
 *
 * Since: 2.2
 */
gchar **
gdk_pixbuf_format_get_extensions (GdkPixbufFormat *format)
{
	g_return_val_if_fail (format != NULL, NULL);

	return g_strdupv (format->extensions);
}

/**
 * gdk_pixbuf_format_is_writable:
 * @format: a #GdkPixbufFormat
 *
 * Returns whether pixbufs can be saved in the given format.
 * 
 * Return value: whether pixbufs can be saved in the given format.
 *
 * Since: 2.2
 */
gboolean
gdk_pixbuf_format_is_writable (GdkPixbufFormat *format)
{
	g_return_val_if_fail (format != NULL, FALSE);

	return (format->flags & GDK_PIXBUF_FORMAT_WRITABLE) != 0;
}

GdkPixbufFormat *
_gdk_pixbuf_get_format (GdkPixbufModule *module)
{
	g_return_val_if_fail (module != NULL, NULL);

	return module->info;
}

/**
 * gdk_pixbuf_get_formats:
 *
 * Obtains the available information about the image formats supported
 * by GdkPixbuf.
 *
 * Returns: A list of #GdkPixbufFormat<!-- -->s describing the supported 
 * image formats.  The list should be freed when it is no longer needed, 
 * but the structures themselves are owned by #GdkPixbuf and should not be 
 * freed.  
 *
 * Since: 2.2
 */
GSList *
gdk_pixbuf_get_formats (void)
{
	GSList *result = NULL;
	GSList *modules;

	for (modules = get_file_formats (); modules; modules = g_slist_next (modules)) {
		GdkPixbufModule *module = (GdkPixbufModule *)modules->data;
		GdkPixbufFormat *info = _gdk_pixbuf_get_format (module);
		result = g_slist_prepend (result, info);
	}

	return result;
}





