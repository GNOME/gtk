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

#ifndef __GTK_MODULE_H__
#define __GTK_MODULE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <glib-object.h>

typedef struct _GtkModule GtkModule;

typedef gboolean (*GtkModuleLoadFunc)   (GtkModule *module);
typedef void (*GtkModuleUnloadFunc) (GtkModule *module);

struct _GtkModule 
{
  /*< private >*/
  GTypePlugin plugin;

  guint ref_count;

  GtkModuleLoadFunc   load_func;
  GtkModuleUnloadFunc unload_func;

  GSList *type_infos;

  gchar *name;
};

gboolean gtk_module_ref           (GtkModule           *module);
void     gtk_module_unref         (GtkModule           *module);
void     gtk_module_init          (GtkModule           *module,
				   const gchar         *name,
				   GtkModuleLoadFunc    load_func,
				   GtkModuleUnloadFunc  unload_func);
GType    gtk_module_register_type (GtkModule           *module,
				   GType                parent_type,
				   const gchar         *type_name,
				   const GTypeInfo     *type_info);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif __GTK_MODULE_H__
