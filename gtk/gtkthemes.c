/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Themes added by The Rasterman <raster@redhat.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <gmodule.h>
#include "gtkmodule.h"
#include "gtkthemes.h"
#include "gtkrc.h"
#include "config.h"
#include "gtkintl.h"

struct _GtkThemeEngine
{
  GtkModule base_module;
  
  GModule *library;

  void (*init) (GtkModule *);
  void (*exit) (void);
  GtkRcStyle *(*create_rc_style) ();

  gchar *name;
};

static GHashTable *engine_hash = NULL;

static gboolean
gtk_theme_engine_load (GtkModule *module)
{
  GtkThemeEngine *engine = (GtkThemeEngine *)module;
  
  gchar *fullname;
  gchar *engine_path;
      
  fullname = g_module_build_path (NULL, engine->name);
  engine_path = gtk_rc_find_module_in_path (fullname);
  
  if (!engine_path)
    {
      g_warning (_("Unable to locate loadable module in module_path: \"%s\","),
		 fullname);
      
      g_free (fullname);
      return FALSE;
    }
    
  g_free (fullname);
       
  /* load the lib */
  
  GTK_NOTE (MISC, g_message ("Loading Theme %s\n", engine_path));
       
  engine->library = g_module_open (engine_path, 0);
  g_free(engine_path);
  if (!engine->library)
    {
      g_warning (g_module_error());
      return FALSE;
    }
  
  /* extract symbols from the lib */
  if (!g_module_symbol (engine->library, "theme_init",
			(gpointer *)&engine->init) ||
      !g_module_symbol (engine->library, "theme_exit", 
			(gpointer *)&engine->exit) ||
      !g_module_symbol (engine->library, "theme_create_rc_style", 
			(gpointer *)&engine->create_rc_style))
    {
      g_warning (g_module_error());
      g_module_close (engine->library);
      
      return FALSE;
    }
	    
  /* call the theme's init (theme_init) function to let it */
  /* setup anything it needs to set up. */
  engine->init (module);

  return TRUE;
}

static void
gtk_theme_engine_unload (GtkModule *module)
{
  GtkThemeEngine *engine = (GtkThemeEngine *)module;

  engine->exit();

  g_module_close (engine->library);
  engine->library = NULL;

  engine->init = NULL;
  engine->exit = NULL;
  engine->create_rc_style = NULL;
}

GtkThemeEngine*
gtk_theme_engine_get (const gchar *name)
{
  GtkThemeEngine *result;
  
  if (!engine_hash)
    engine_hash = g_hash_table_new (g_str_hash, g_str_equal);

  /* get the library name for the theme */
  
  result = g_hash_table_lookup (engine_hash, name);

  if (!result)
    {
      result = g_new0 (GtkThemeEngine, 1);
      result->name = g_strdup (name);

      gtk_module_init ((GtkModule *)result, name,
		       gtk_theme_engine_load, gtk_theme_engine_unload);

      g_hash_table_insert (engine_hash, result->name, result);
    }

  if (!gtk_module_ref ((GtkModule *)result))
    return NULL;

  return (GtkThemeEngine *)result;
}

void
gtk_theme_engine_ref (GtkThemeEngine *engine)
{
  g_return_if_fail (engine != NULL);

  gtk_module_ref ((GtkModule *)engine);
}

void
gtk_theme_engine_unref (GtkThemeEngine *engine)
{
  g_return_if_fail (engine != NULL);

  gtk_module_unref ((GtkModule *)engine);
}

GtkRcStyle *
gtk_theme_engine_create_rc_style (GtkThemeEngine *engine)
{
  g_return_val_if_fail (engine != NULL, NULL);

  return engine->create_rc_style ();
}
