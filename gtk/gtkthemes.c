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
#include "gtkthemes.h"
#include "gtkmain.h"
#include "gtkrc.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtkwidget.h"
#include "config.h"
#include "gtkintl.h"

/*****************************
 *****************************
 * temporary compat code, make GtkThemeEnginePlugin a GObject plus GTypePlugin interface
 */
typedef struct _GtkThemeEnginePlugin GtkThemeEnginePlugin;
typedef struct _GObjectClass         GtkThemeEnginePluginClass;
static void gtk_theme_engine_plugin_use                (GTypePlugin     *plugin);
static void gtk_theme_engine_plugin_unuse              (GTypePlugin     *plugin);
static void gtk_theme_engine_plugin_complete_type_info (GTypePlugin     *plugin,
							GType            g_type,
							GTypeInfo       *info,
							GTypeValueTable *value_table);
GType	    gtk_theme_engine_plugin_get_type	       (void);
struct _GtkThemeEnginePlugin
{
  GObject parent_instance;

  GtkThemeEngine *engine;
  gchar *engine_name;
  GTypeInfo info;
  GType type;
  GType parent_type;
};
#define GTK_TYPE_THEME_ENGINE_PLUGIN              (gtk_theme_engine_plugin_get_type ())
#define GTK_THEME_ENGINE_PLUGIN(plugin)           (G_TYPE_CHECK_INSTANCE_CAST ((plugin), GTK_TYPE_THEME_ENGINE_PLUGIN, GtkThemeEnginePlugin))
#define GTK_THEME_ENGINE_PLUGIN_CLASS(class)      (G_TYPE_CHECK_CLASS_CAST ((class), GTK_TYPE_THEME_ENGINE_PLUGIN, GtkThemeEnginePluginClass))
#define GTK_IS_THEME_ENGINE_PLUGIN(plugin)        (G_TYPE_CHECK_INSTANCE_TYPE ((plugin), GTK_TYPE_THEME_ENGINE_PLUGIN))
#define GTK_IS_THEME_ENGINE_PLUGIN_CLASS(class)   (G_TYPE_CHECK_CLASS_TYPE ((class), GTK_TYPE_THEME_ENGINE_PLUGIN))
#define GTK_THEME_ENGINE_PLUGIN_GET_CLASS(plugin) (G_TYPE_INSTANCE_GET_CLASS ((plugin), GTK_TYPE_THEME_ENGINE_PLUGIN, GtkThemeEnginePluginClass))
static void
gtk_theme_engine_plugin_shutdown (GObject *object)
{
  GtkThemeEnginePlugin *plugin = GTK_THEME_ENGINE_PLUGIN (object);

  g_warning (G_STRLOC ": shutdown should never happen for static type plugins");

  g_object_ref (object);

  /* chain parent class handler */
  G_OBJECT_CLASS (g_type_class_peek_parent (GTK_THEME_ENGINE_PLUGIN_GET_CLASS (plugin)))->shutdown (object);
}
static void
gtk_theme_engine_plugin_class_init (GtkThemeEnginePluginClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->shutdown = gtk_theme_engine_plugin_shutdown;
}
static void
theme_engine_plugin_iface_init (GTypePluginClass *iface)
{
  iface->use_plugin = gtk_theme_engine_plugin_use;
  iface->unuse_plugin = gtk_theme_engine_plugin_unuse;
  iface->complete_type_info = gtk_theme_engine_plugin_complete_type_info;
}
GType
gtk_theme_engine_plugin_get_type (void)
{
  static GType theme_engine_plugin_type = 0;

  if (!theme_engine_plugin_type)
    {
      static const GTypeInfo theme_engine_plugin_info = {
	sizeof (GtkThemeEnginePluginClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) gtk_theme_engine_plugin_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (GtkThemeEnginePlugin),
	0,		/* n_preallocs */
	NULL,		/* instance_init */
      };
      static const GInterfaceInfo iface_info = {
	(GInterfaceInitFunc) theme_engine_plugin_iface_init,
	NULL,               /* interface_finalize */
	NULL,               /* interface_data */
      };

      theme_engine_plugin_type = g_type_register_static (G_TYPE_OBJECT, "GtkThemeEnginePlugin", &theme_engine_plugin_info, 0);

      g_type_add_interface_static (theme_engine_plugin_type, G_TYPE_TYPE_PLUGIN, &iface_info);
    }

  return theme_engine_plugin_type;
}
/* end of GtkThemeEnginePlugin object implementation stuff
 *****************************
 *****************************/

struct _GtkThemeEngine
{
  GModule *library;

  void (*init) (GtkThemeEngine *);
  void (*exit) (void);
  GtkRcStyle *(*create_rc_style) ();

  gchar *name;

  GSList *plugins;		/* TypePlugins for this engine */
  
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
  GtkThemeEngine *result;
  
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
	    result = g_new (GtkThemeEngine, 1);
	    
	    result->refcount = 1;
	    result->name = g_strdup (name);
	    result->library = library;
	    result->plugins = NULL;
	    
	    /* extract symbols from the lib */
	    if (!g_module_symbol (library, "theme_init",
				  (gpointer *)&result->init) ||
		!g_module_symbol (library, "theme_exit", 
				  (gpointer *)&result->exit) ||
		!g_module_symbol (library, "theme_create_rc_style", 
				  (gpointer *)&result->create_rc_style))
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
  
  engine->refcount++;
}

void
gtk_theme_engine_unref (GtkThemeEngine *engine)
{
  GSList *tmp_list;

  g_return_if_fail (engine != NULL);
  g_return_if_fail (engine->refcount > 0);

  engine->refcount--;

  if (engine->refcount == 0)
    {
      engine->exit();

      g_hash_table_remove (engine_hash, engine->name);

      tmp_list = engine->plugins;
      while (tmp_list)
	{
	  GtkThemeEnginePlugin *plugin = tmp_list->data;
	  plugin->engine = NULL;

	  tmp_list = tmp_list->next;
	}
      g_slist_free (engine->plugins);
      
      g_module_close (engine->library);
      g_free (engine->name);
      g_free (engine);
    }
}

GtkRcStyle *
gtk_theme_engine_create_rc_style (GtkThemeEngine *engine)
{
  g_return_val_if_fail (engine != NULL, NULL);
  
  return engine->create_rc_style ();
}

static void
gtk_theme_engine_plugin_use (GTypePlugin *plugin)
{
  GtkThemeEnginePlugin *theme_plugin = GTK_THEME_ENGINE_PLUGIN (plugin);

  if (theme_plugin->engine == NULL)
    {
      gtk_theme_engine_get (theme_plugin->engine_name);
      if (!theme_plugin->engine)
	{
	  g_warning ("An attempt to create an instance of a type from\n"
		     "a previously loaded theme engine was made after the engine\n"
		     "was unloaded, but the engine could not be reloaded or no longer\n"
		     "implements the type. Bad things will happen.\n");
	}
    }
  else
    gtk_theme_engine_ref (theme_plugin->engine);
}

static void
gtk_theme_engine_plugin_unuse (GTypePlugin *plugin)
{
  GtkThemeEnginePlugin *theme_plugin = GTK_THEME_ENGINE_PLUGIN (plugin);

  g_return_if_fail (theme_plugin->engine != NULL);
  
  gtk_theme_engine_unref (theme_plugin->engine);
}
			       
static void
gtk_theme_engine_plugin_complete_type_info (GTypePlugin     *plugin,
					    GType            g_type,
					    GTypeInfo       *info,
					    GTypeValueTable *value_table)
{
  GtkThemeEnginePlugin *theme_plugin = GTK_THEME_ENGINE_PLUGIN (plugin);

  *info = theme_plugin->info;
}

/**
 * gtk_theme_engine_register_type:
 * @engine:      a #GtkThemeEngine
 * @parent_type: the type for the parent class
 * @type_name:   name for the type
 * @type_info:   type information structure
 * 
 * Looks up or registers a type that is implemented with a particular
 * theme engine. If a type with name @type_name is already registered,
 * the #GType identifier for the type is returned, otherwise the type
 * is newly registered, and the resulting #GType identifier returned.
 *
 * As long as any instances of the type exist, the a reference will be
 * held to the theme engine and the theme engine will not be unloaded.
 *
 * Return value: the type identifier for the class.
 **/
GType
gtk_theme_engine_register_type (GtkThemeEngine  *engine,
				GType            parent_type,
				const gchar     *type_name,
				const GTypeInfo *type_info)
{
  GtkThemeEnginePlugin *plugin;
  GType type;
  
  g_return_val_if_fail (engine != NULL, 0);
  g_return_val_if_fail (type_name != NULL, 0);
  g_return_val_if_fail (type_info != NULL, 0);

  type = g_type_from_name (type_name);
  if (type)
    plugin = GTK_THEME_ENGINE_PLUGIN (g_type_get_plugin (type));
  else
    {
      plugin = g_object_new (GTK_TYPE_THEME_ENGINE_PLUGIN, NULL);

      plugin->engine = NULL;
      plugin->engine_name = NULL;
      plugin->parent_type = parent_type;
      plugin->type = g_type_register_dynamic (parent_type, type_name, G_TYPE_PLUGIN (plugin), 0);
    }
  
  if (plugin->engine)
    {
      if (plugin->engine != engine)
	{
	  g_warning ("Two different theme engines tried to register '%s'.", type_name);
	  return 0;
	}

      if (plugin->parent_type != parent_type)
	{
	  g_warning ("Type '%s' recreated with different parent type.\n"
		     "(was '%s', now '%s')", type_name,
		     g_type_name (plugin->parent_type),
		     g_type_name (parent_type));
	  return 0;
	}
    }
  else
    {
      plugin->engine = engine;
      if (plugin->engine_name)
	g_free (plugin->engine_name);

      plugin->engine_name = g_strdup (engine->name);
      
      plugin->info = *type_info;

      engine->plugins = g_slist_prepend (engine->plugins, plugin);
    }

  return plugin->type;
}

