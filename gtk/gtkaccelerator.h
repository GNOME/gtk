/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
#ifndef __GTK_ACCELERATOR_H__
#define __GTK_ACCELERATOR_H__


#include <gdk/gdk.h>
#include <gtk/gtkobject.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


typedef struct _GtkAcceleratorTable GtkAcceleratorTable;

struct _GtkAcceleratorTable
{
  GList *entries[256];
  guint  ref_count;
  guint8 modifier_mask;
};


/* Accelerator tables.
 */
GtkAcceleratorTable* gtk_accelerator_table_new  (void);
GtkAcceleratorTable* gtk_accelerator_table_find (GtkObject	*object,
						 const gchar	*signal_name,
						 guchar		accelerator_key,
						 guint8		accelerator_mods);

GtkAcceleratorTable *gtk_accelerator_table_ref (GtkAcceleratorTable *table);
void gtk_accelerator_table_unref   (GtkAcceleratorTable *table);
void gtk_accelerator_table_install (GtkAcceleratorTable *table,
				    GtkObject           *object,
				    const gchar         *signal_name,
				    guchar               accelerator_key,
				    guint8               accelerator_mods);
void gtk_accelerator_table_remove  (GtkAcceleratorTable *table,
				    GtkObject           *object,
				    const gchar         *signal_name);
gint gtk_accelerator_table_check   (GtkAcceleratorTable *table,
				    const guchar         accelerator_key,
				    guint8               accelerator_mods);
void gtk_accelerator_tables_delete (GtkObject		*object);


void gtk_accelerator_table_set_mod_mask (GtkAcceleratorTable *table,
				         guint8               modifier_mask);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ACCELERATOR_H__ */
