/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

#include "gtkmodule.h"

typedef struct _GtkModuleTypeInfo GtkModuleTypeInfo;

struct _GtkModuleTypeInfo 
{
  gboolean  loaded;
  GType     type;
  GType     parent_type;
  GTypeInfo info;
};

static void gtk_module_plugin_ref         (GTypePlugin     *plugin);
static void gtk_module_complete_type_info (GTypePlugin     *plugin,
					   GType            g_type,
					   GTypeInfo       *info,
					   GTypeValueTable *value_table);

static GTypePluginVTable gtk_module_plugin_vtable = {
  gtk_module_plugin_ref,
  (GTypePluginUnRef)gtk_module_unref,
  gtk_module_complete_type_info,
  NULL
};

/**
 * gtk_module_init:
 * @module: a pointer to a #GtkModule structure. This will
 *          typically be included as the first element in
 *          a larger structure that is specific to the
 *          type of module.
 * @name: a human-readable name to use in error messages.
 * @load_func: a function to call to load the module.
 * @unload_func: a function to call to unload the module.
 * 
 * Initializes a structure as a GtkModule. The module is
 * created with a refcount of zero - that is, in the
 * unloaded state. In order to load the module initially,
 * you must call gtk_module_ref().
 **/
void
gtk_module_init (GtkModule          *module,
		 const gchar        *name,
		 GtkModuleLoadFunc   load_func,
		 GtkModuleUnloadFunc unload_func)
{
  module->name = g_strdup (name);
  module->plugin.vtable = &gtk_module_plugin_vtable;
  module->load_func = load_func;
  module->unload_func = unload_func;
  module->ref_count = 0;
  module->type_infos = NULL;
}

static GtkModuleTypeInfo *
gtk_module_find_type_info (GtkModule *module,
			   GType      type)
{
  GSList *tmp_list = module->type_infos;
  while (tmp_list)
    {
      GtkModuleTypeInfo *type_info = tmp_list->data;
      if (type_info->type == type)
	return type_info;
      
      tmp_list = tmp_list->next;
    }

  return NULL;
}

/**
 * gtk_module_ref:
 * @module: a #GtkModule
 * 
 * Increases the reference count of a #GtkModule by one. If the
 * reference count was zero before, the module will be loaded.
 *
 * Return Value: %FALSE if the module needed to be loaded and
 *               loading the module failed.
 **/
gboolean
gtk_module_ref (GtkModule *module)
{
  g_return_val_if_fail (module != NULL, FALSE);

  if (module->ref_count == 0)
    {
      GSList *tmp_list;
      
      if (!module->load_func (module))
	return FALSE;

      tmp_list = module->type_infos;
      while (tmp_list)
	{
	  GtkModuleTypeInfo *type_info = tmp_list->data;
	  if (!type_info->loaded)
	    {
	      g_warning ("module '%s' failed to register type '%s'\n",
			 module->name, g_type_name (type_info->type));
	      return FALSE;
	    }
	  
	  tmp_list = tmp_list->next;
	}
    }
 
  module->ref_count++;
  return TRUE;
}

static void
gtk_module_plugin_ref (GTypePlugin *plugin)
{
  GtkModule *module = (GtkModule *)plugin;

  if (!gtk_module_ref (module))
    {
      g_warning ("Fatal error - Could not reload previously loaded module '%s'\n",
		 module->name);
      exit (1);
    }
}

/**
 * gtk_module_unref:
 * @module: a #GtkModule
 * 
 * Decreases the reference count of a #GtkModule by one. If the
 * result is zero, the module will be unloaded. (However, the
 * #GtkModule will not be freed, and types associated with the
 * #GtkModule are not unregistered. Once a #GtkModule is 
 * initialized, it must exist forever.)x
 **/
void
gtk_module_unref (GtkModule *module)
{
  g_return_if_fail (module != NULL);
  g_return_if_fail (module->ref_count > 0);

  module->ref_count--;

  if (module->ref_count == 0)
    {
      GSList *tmp_list;

      module->unload_func (module);

      tmp_list = module->type_infos;
      while (tmp_list)
	{
	  GtkModuleTypeInfo *type_info = tmp_list->data;
	  type_info->loaded = FALSE;

	  tmp_list = tmp_list->next;
	}
    }
}
	
static void
gtk_module_complete_type_info (GTypePlugin     *plugin,
			       GType            g_type,
			       GTypeInfo       *info,
			       GTypeValueTable *value_table)
{
  GtkModule *module = (GtkModule *)plugin;
  GtkModuleTypeInfo *module_type_info = gtk_module_find_type_info (module, g_type);

  *info = module_type_info->info;
}

/**
 * gtk_module_register_type:
 * @engine:      a #GtkModule
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
gtk_module_register_type (GtkModule       *module,
			  GType            parent_type,
			  const gchar     *type_name,
			  const GTypeInfo *type_info)
{
  GtkModuleTypeInfo *module_type_info = NULL;
  GType type;
  
  g_return_val_if_fail (module != NULL, 0);
  g_return_val_if_fail (type_name != NULL, 0);
  g_return_val_if_fail (type_info != NULL, 0);

  type = g_type_from_name (type_name);
  if (type)
    {
      GtkModule *old_module = (GtkModule *)g_type_get_plugin (type);

      if (old_module != module)
	{
	  g_warning ("Two different modules tried to register '%s'.", type_name);
	  return 0;
	}
    }

  if (type)
    {
      module_type_info = gtk_module_find_type_info (module, type);

      if (module_type_info->parent_type != parent_type)
	{
	  g_warning ("Type '%s' recreated with different parent type.\n"
		     "(was '%s', now '%s')", type_name,
		     g_type_name (module_type_info->parent_type),
		     g_type_name (parent_type));
	  return 0;
	}
    }
  else
    {
      module_type_info = g_new (GtkModuleTypeInfo, 1);
      
      module_type_info->parent_type = parent_type;
      module_type_info->type = g_type_register_dynamic (parent_type, type_name, (GTypePlugin *)module);
      
      module->type_infos = g_slist_prepend (module->type_infos, module_type_info);
    }

  module_type_info->loaded = TRUE;
  module_type_info->info = *type_info;

  return module_type_info->type;
}
