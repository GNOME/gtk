/* -*- mode: C; c-file-style: "linux" -*- */
/* GdkPixbuf library
 * queryloaders.c:
 *
 * Copyright (C) 2002 The Free Software Foundation
 * 
 * Author: Matthias Clasen <maclas@gmx.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gmodule.h>

#include <errno.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "gdk-pixbuf/gdk-pixbuf.h"
#include "gdk-pixbuf/gdk-pixbuf-private.h"
#include "gdk-pixbuf/gdk-pixbuf-io.h"

#if USE_LA_MODULES
#define SOEXT ".la"
#else
#define SOEXT ("." G_MODULE_SUFFIX)
#endif
#define SOEXT_LEN (strlen (SOEXT))

static void
print_escaped (const char *str)
{
	gchar *tmp = g_strescape (str, "");
	g_printf ("\"%s\" ", tmp);
	g_free (tmp);
}

static void
query_module (const char *dir, const char *file)
{
	char *path;
	GModule *module;
	void                    (*fill_info)     (GdkPixbufFormat *info);
	void                    (*fill_vtable)   (GdkPixbufModule *module);
	char **mime; 
	char **ext; 
	const GdkPixbufModulePattern *pattern;

	if (g_path_is_absolute (dir)) 
		path = g_build_filename (dir, file, NULL);
	else {
		char *cwd = g_get_current_dir ();
		path = g_build_filename (cwd, dir, file, NULL);
		g_free (cwd);
	}	       
	

	module = g_module_open (path, 0);
	if (module &&
	    g_module_symbol (module, "fill_info", (gpointer *) &fill_info) &&
	    g_module_symbol (module, "fill_vtable", (gpointer *) &fill_vtable)) {
		GdkPixbufFormat *info;
		g_printf("\"%s\"\n", path);
		info = g_new0 (GdkPixbufFormat, 1);
		(*fill_info) (info);
		g_printf ("\"%s\" %d \"%s\" \"%s\"\n", 
		       info->name, info->flags, 
		       info->domain ? info->domain : GETTEXT_PACKAGE, info->description);
		for (mime = info->mime_types; *mime; mime++) {
			g_printf ("\"%s\" ", *mime);
		}
		g_printf ("\"\"\n");
		for (ext = info->extensions; *ext; ext++) {
			g_printf ("\"%s\" ", *ext);
		}
		g_printf ("\"\"\n");
		for (pattern = info->signature; pattern->prefix; pattern++) {
			print_escaped (pattern->prefix);
			print_escaped (pattern->mask ? (const char *)pattern->mask : "");
			g_printf ("%d\n", pattern->relevance);
		}
		g_printf ("\n");
		g_free (info);
	}
	else {
		g_fprintf (stderr, "Cannot load loader %s\n", path);
	}
	if (module)
		g_module_close (module);
	g_free (path);
}

int main (int argc, char **argv)
{
	gint i;

	g_printf ("# GdkPixbuf Image Loader Modules file\n"
		"# Automatically generated file, do not edit\n"
		"#\n");
  
	if (argc == 1) {
#ifdef USE_GMODULE
		const char *path;
		GDir *dir;
    
		path = g_getenv ("GDK_PIXBUF_MODULEDIR");
		if (path == NULL || *path == '\0')
			path = PIXBUF_LIBDIR;

		g_printf ("# LoaderDir = %s\n#\n", path);

		dir = g_dir_open (path, 0, NULL);
		if (dir) {
			const char *dent;

			while ((dent = g_dir_read_name (dir))) {
				gint len = strlen (dent);
				if (len > SOEXT_LEN && 
				    strcmp (dent + len - SOEXT_LEN, SOEXT) == 0) {
					query_module (path, dent);
				}
			}
			g_dir_close (dir);
		}
#else
		g_printf ("# dynamic loading of modules not supported\n");
#endif
	}
	else {
		char *cwd = g_get_current_dir ();

		for (i = 1; i < argc; i++)
			query_module (cwd, argv[i]);

		g_free (cwd);
	}

	return 0;
}
