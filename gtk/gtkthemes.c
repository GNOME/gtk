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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <gmodule.h>
#include "gtkthemes.h"
#include "gtkmain.h"
#include "gtkrc.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtkwidget.h"
#include "config.h"
#include "gtkintl.h"

typedef struct _GtkThemeEnginePrivate GtkThemeEnginePrivate;

struct _GtkThemeEnginePrivate {
  GtkThemeEngine engine;
  
  GModule *library;
  void *name;

  void (*init) (GtkThemeEngine *);
  void (*exit) (void);

  guint refcount;
};

static GHashTable *engine_hash = NULL;

#ifdef __EMX__
static void gen_8_3_dll_name(gchar *name, gchar *fullname)
{
    /* 8.3 dll filename restriction */
    fullname[0] = '_';
    strncpy (fullname + 1, name, 7);
    fullname[8] = '\0';
    strcat (fullname, ".dll");
}                                                	
#endif

GtkThemeEngine*
gtk_theme_engine_get (const gchar *name)
{
  GtkThemeEnginePrivate *result;
  
  if (!engine_hash)
    engine_hash = g_hash_table_new (g_str_hash, g_str_equal);

  /* get the library name for the theme */
  
  result = g_hash_table_lookup (engine_hash, name);

  if (!result)
    {
       gchar *fullname;
       gchar *engine_path;
       GModule *library;
      
#ifndef __EMX__
       fullname = g_module_build_path (NULL, name);
#else
       fullname = g_malloc (13);
       gen_8_3_dll_name(name, fullname);
#endif
       engine_path = gtk_rc_find_module_in_path (fullname);
#ifdef __EMX__
       if (!engine_path)
	 {
	   /* try theme name without prefix '_' */
	   memmove(fullname, fullname + 1, strlen(fullname));
	   engine_path = gtk_rc_find_module_in_path (fullname);
	 }
#endif

       if (!engine_path)
	 {
	   g_warning (_("Unable to locate loadable module in module_path: \"%s\","),
		      fullname);
	   
	   g_free (fullname);
	   return NULL;
	 }
       g_free (fullname);
       
       /* load the lib */

       GTK_NOTE (MISC, g_message ("Loading Theme %s\n", engine_path));
       
       library = g_module_open (engine_path, 0);
       g_free(engine_path);
       if (!library)
	 {
	   g_warning (g_module_error());
	   return NULL;
	 }
       else
	 {
	    result = g_new (GtkThemeEnginePrivate, 1);
	    
	    result->refcount = 1;
	    result->name = g_strdup (name);
	    result->library = library;
	    
	    /* extract symbols from the lib */
	    if (!g_module_symbol (library, "theme_init",
				  (gpointer *)&result->init) ||
		!g_module_symbol (library, "theme_exit", 
				  (gpointer *)&result->exit))
	      {
		g_warning (g_module_error());
		g_free (result);
		return NULL;
	      }
	    
	    /* call the theme's init (theme_init) function to let it */
	    /* setup anything it needs to set up. */
	    result->init((GtkThemeEngine *)result);
	    
	    g_hash_table_insert (engine_hash, result->name, result);
	 }
    }
  else
    result->refcount++;

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
  private = (GtkThemeEnginePrivate *)engine;

  g_return_if_fail (engine != NULL);
  g_return_if_fail (private->refcount > 0);

  private->refcount--;

  if (private->refcount == 0)
    {
      private->exit();
      
      g_hash_table_remove (engine_hash, private->name);
      
      g_module_close (private->library);
      g_free (private->name);
      g_free (private);
    }
}
