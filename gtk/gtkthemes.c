/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Themes added by The Rasterman <raster@redhat.com>
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include "gtkthemes.h"
#include "gtkmain.h"
#include "gtkrc.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtkwidget.h"
#include "gtkprivate.h"
#include "config.h"

typedef struct _GtkThemeEnginePrivate GtkThemeEnginePrivate;

struct _GtkThemeEnginePrivate {
  GtkThemeEngine engine;
  
  void *library;
  void *name;

  void (*init) (GtkThemeEngine *);
  void (*exit) (void);

  guint refcount;
};

static GHashTable *engine_hash = NULL;

GtkThemeEngine *
gtk_theme_engine_get (gchar          *name)
{
  GtkThemeEnginePrivate *result;
  
  if (!engine_hash)
    engine_hash = g_hash_table_new (g_str_hash, g_str_equal);
   
  /* get the library name for the theme */
  
  result = g_hash_table_lookup (engine_hash, name);

#ifdef HAVE_LIBDL
  if (!result)
    {
       gchar fullname[1024];
       gchar *engine_path;
       void *library;
      
       g_snprintf (fullname, 1024, "lib%s.so", name);
       engine_path = gtk_rc_find_module_in_path(NULL, fullname);

       if (!engine_path)
	 return NULL;
       
       /* load the lib */
       
       printf ("Loading Theme %s\n", engine_path);
       
       library = dlopen(engine_path, RTLD_NOW);
       g_free(engine_path);
       if (!library)
	 g_error(dlerror());
       else
	 {
	    result = g_new (GtkThemeEnginePrivate, 1);
	    
	    result->refcount = 1;
	    result->name = g_strdup (name);
	    result->library = library;
	    
	    /* extract symbols from the lib */   
	    result->init=dlsym(library, "theme_init");
	    result->exit=dlsym(library ,"theme_exit");
	    
	    /* call the theme's init (theme_init) function to let it */
	    /* setup anything it needs to set up. */
	    result->init((GtkThemeEngine *)result);
	    
	    g_hash_table_insert (engine_hash, name, result);
	 }
    }
#endif HAVE_LIBDL
   
   return (GtkThemeEngine *)result;
}

void
gtk_theme_engine_ref (GtkThemeEngine *engine)
{
  g_return_if_fail (engine != NULL);
  
  ((GtkThemeEnginePrivate *)engine)->refcount++;
}

void
gtk_theme_engine_unref (GtkThemeEngine *engine)
{
  GtkThemeEnginePrivate *private;

  g_return_if_fail (engine != NULL);

  private = (GtkThemeEnginePrivate *)engine;
  private->refcount--;

  if (private->refcount == 0)
    {
      g_hash_table_remove (engine_hash, private);
      
#ifdef HAVE_LIBDL
      dlclose (private->library);
#endif /* HAVE_LIBDL */
      g_free (private->name);
      g_free (private);
    }
}
