/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkAccelGroup: Accelerator manager for GtkObjects.
 * Copyright (C) 1998 Tim Janik
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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <ctype.h>
#include <string.h>
#include "gtkaccelgroup.h"
#include "gdk/gdkkeysyms.h"
#include "gtksignal.h"
#include "gtkwidget.h"


/* --- signals --- */
typedef void (*GtkSignalAddAccelerator)	   (GtkObject	    *object,
					    guint	     accel_signal_id,
					    GtkAccelGroup   *accel_group,
					    guint	     accel_key,
					    GdkModifierType  accel_mods,
					    GtkAccelFlags    accel_flags,
					    gpointer	     func_data);
typedef void (*GtkSignalRemoveAccelerator) (GtkObject	    *object,
					    GtkAccelGroup   *accel_group,
					    guint	     accel_key,
					    GdkModifierType  accel_mods,
					    gpointer	     func_data);

/* --- variables --- */
static GtkAccelGroup	*default_accel_group = NULL;
static guint		 default_accel_mod_mask = (GDK_SHIFT_MASK |
						   GDK_CONTROL_MASK |
						   GDK_MOD1_MASK);
static const gchar	*accel_groups_key = "gtk-accel-groups";
static guint		 accel_groups_key_id = 0;
static const gchar	*accel_entries_key = "gtk-accel-entries";
static guint		 accel_entries_key_id = 0;
static GHashTable	*accel_entry_hash_table = NULL;
static GMemChunk	*accel_tables_mem_chunk = NULL;
static GMemChunk	*accel_entries_mem_chunk = NULL;


/* --- functions --- */
static gint
gtk_accel_entries_equal (gconstpointer a,
			 gconstpointer b)
{
  const GtkAccelEntry *e1;
  const GtkAccelEntry *e2;
  
  e1 = a;
  e2 = b;
  
  return ((e1->accel_group == e2->accel_group) &&
	  (e1->accelerator_key == e2->accelerator_key) &&
	  (e1->accelerator_mods == e2->accelerator_mods));
}

static guint
gtk_accel_entries_hash (gconstpointer a)
{
  const GtkAccelEntry *e;
  guint h;
  
  e = a;
  
  h = (gulong) e->accel_group;
  h ^= e->accelerator_key << 16;
  h ^= e->accelerator_key >> 16;
  h ^= e->accelerator_mods;
  
  return h;
}

GtkAccelGroup*
gtk_accel_group_new (void)
{
  GtkAccelGroup *accel_group;
  
  if (!accel_groups_key_id)
    {
      accel_groups_key_id = g_quark_from_static_string (accel_groups_key);
      accel_entries_key_id = g_quark_from_static_string (accel_entries_key);
      
      accel_entry_hash_table = g_hash_table_new (gtk_accel_entries_hash,
						 gtk_accel_entries_equal);
      
      accel_tables_mem_chunk = g_mem_chunk_create (GtkAccelGroup, 8, G_ALLOC_AND_FREE);
      accel_entries_mem_chunk = g_mem_chunk_create (GtkAccelEntry, 64, G_ALLOC_AND_FREE);
    }
  
  accel_group = g_chunk_new (GtkAccelGroup, accel_tables_mem_chunk);
  
  accel_group->ref_count = 1;
  accel_group->lock_count = 0;
  accel_group->modifier_mask = gtk_accelerator_get_default_mod_mask ();
  accel_group->attach_objects = NULL;
  
  return accel_group;
}

GtkAccelGroup*
gtk_accel_group_get_default (void)
{
  if (!default_accel_group)
    default_accel_group = gtk_accel_group_new ();
  
  return default_accel_group;
}

GtkAccelGroup*
gtk_accel_group_ref (GtkAccelGroup	*accel_group)
{
  g_return_val_if_fail (accel_group != NULL, NULL);
  
  accel_group->ref_count += 1;
  
  return accel_group;
}

void
gtk_accel_group_unref (GtkAccelGroup  *accel_group)
{
  g_return_if_fail (accel_group != NULL);
  g_return_if_fail (accel_group->ref_count > 0);
  
  accel_group->ref_count -= 1;
  if (accel_group->ref_count == 0)
    {
      g_return_if_fail (accel_group != default_accel_group);
      g_return_if_fail (accel_group->attach_objects == NULL);
      
      g_chunk_free (accel_group, accel_tables_mem_chunk);
    }
}

static void
gtk_accel_group_object_destroy (GtkObject *object)
{
  GSList *free_list, *slist;
  
  free_list = gtk_object_get_data_by_id (object, accel_groups_key_id);
  gtk_object_set_data_by_id (object, accel_groups_key_id, NULL);
  
  for (slist = free_list; slist; slist = slist->next)
    {
      GtkAccelGroup *accel_group;
      
      accel_group = slist->data;
      accel_group->attach_objects = g_slist_remove (accel_group->attach_objects, object);
      gtk_accel_group_unref (accel_group);
    }
  g_slist_free (free_list);
}

void
gtk_accel_group_attach (GtkAccelGroup	*accel_group,
			GtkObject	*object)
{
  GSList *slist;
  
  g_return_if_fail (accel_group != NULL);
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (g_slist_find (accel_group->attach_objects, object) == NULL);
  
  accel_group->attach_objects = g_slist_prepend (accel_group->attach_objects, object);
  gtk_accel_group_ref (accel_group);
  slist = gtk_object_get_data_by_id (object, accel_groups_key_id);
  if (!slist)
    gtk_signal_connect (object,
			"destroy",
			GTK_SIGNAL_FUNC (gtk_accel_group_object_destroy),
			NULL);
  slist = g_slist_prepend (slist, accel_group);
  gtk_object_set_data_by_id (object, accel_groups_key_id, slist);
}

void
gtk_accel_group_detach (GtkAccelGroup	*accel_group,
		        GtkObject	*object)
{
  GSList *slist;
  
  g_return_if_fail (accel_group != NULL);
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (g_slist_find (accel_group->attach_objects, object) != NULL);
  
  accel_group->attach_objects = g_slist_remove (accel_group->attach_objects, object);
  gtk_accel_group_unref (accel_group);
  slist = gtk_object_get_data_by_id (object, accel_groups_key_id);
  slist = g_slist_remove (slist, accel_group);
  if (!slist)
    gtk_signal_disconnect_by_func (object,
				   GTK_SIGNAL_FUNC (gtk_accel_group_object_destroy),
				   NULL);
  gtk_object_set_data_by_id (object, accel_groups_key_id, slist);
}

void
gtk_accel_group_lock (GtkAccelGroup	 *accel_group)
{
  g_return_if_fail (accel_group != NULL);
  
  accel_group->lock_count += 1;
}

void
gtk_accel_group_unlock (GtkAccelGroup  *accel_group)
{
  g_return_if_fail (accel_group != NULL);
  
  if (accel_group->lock_count)
    accel_group->lock_count -= 1;
}

static GtkAccelEntry*
gtk_accel_group_lookup (GtkAccelGroup	*accel_group,
			guint		 accel_key,
			GdkModifierType	 accel_mods)
{
  GtkAccelEntry key_entry = { 0 };
  
  key_entry.accel_group = accel_group;
  key_entry.accelerator_key = gdk_keyval_to_lower (accel_key);
  key_entry.accelerator_mods = accel_mods & accel_group->modifier_mask;
  
  return g_hash_table_lookup (accel_entry_hash_table, &key_entry);
}

gboolean
gtk_accel_group_activate (GtkAccelGroup	 *accel_group,
			  guint		  accel_key,
			  GdkModifierType accel_mods)
{
  GtkAccelEntry *entry;
  
  g_return_val_if_fail (accel_group != NULL, FALSE);
  
  entry = gtk_accel_group_lookup (accel_group, accel_key, accel_mods);
  if (entry && entry->signal_id &&
      (!GTK_IS_WIDGET (entry->object) || GTK_WIDGET_IS_SENSITIVE (entry->object)))
    {
      gtk_signal_emit (entry->object, entry->signal_id);
      return TRUE;
    }
  return FALSE;
}

gboolean
gtk_accel_groups_activate (GtkObject	    *object,
			   guint	     accel_key,
			   GdkModifierType   accel_mods)
{
  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_OBJECT (object), FALSE);
  
  if (gtk_accelerator_valid (accel_key, accel_mods))
    {
      GSList *slist;
      
      for (slist = gtk_accel_groups_from_object (object); slist; slist = slist->next)
	if (gtk_accel_group_activate (slist->data, accel_key, accel_mods))
	  return TRUE;
      return gtk_accel_group_activate (gtk_accel_group_get_default (), accel_key, accel_mods);
    }
  
  return FALSE;
}

void
gtk_accel_group_lock_entry (GtkAccelGroup	 *accel_group,
			    guint		  accel_key,
			    GdkModifierType	  accel_mods)
{
  GtkAccelEntry *entry;
  
  g_return_if_fail (accel_group != NULL);
  
  entry = gtk_accel_group_lookup (accel_group, accel_key, accel_mods);
  if (entry)
    entry->accel_flags |= GTK_ACCEL_LOCKED;
}

void
gtk_accel_group_unlock_entry (GtkAccelGroup	*accel_group,
			      guint		 accel_key,
			      GdkModifierType	 accel_mods)
{
  GtkAccelEntry *entry;
  
  g_return_if_fail (accel_group != NULL);
  
  entry = gtk_accel_group_lookup (accel_group, accel_key, accel_mods);
  if (entry)
    entry->accel_flags &= ~GTK_ACCEL_LOCKED;
}

GtkAccelEntry*
gtk_accel_group_get_entry (GtkAccelGroup    *accel_group,
			   guint             accel_key,
			   GdkModifierType   accel_mods)
{
  g_return_val_if_fail (accel_group != NULL, 0);
  
  return gtk_accel_group_lookup (accel_group, accel_key, accel_mods);
}

void
gtk_accel_group_add (GtkAccelGroup	*accel_group,
		     guint		 accel_key,
		     GdkModifierType	 accel_mods,
		     GtkAccelFlags	 accel_flags,
		     GtkObject		*object,
		     const gchar	*accel_signal)
{
  guint accel_signal_id = 0;
  guint add_accelerator_signal_id = 0;
  guint remove_accelerator_signal_id = 0;
  gchar *signal;
  GtkSignalQuery *query;
  GSList *slist;
  GSList *groups;
  GSList *attach_objects;
  GtkAccelEntry *entry;
  
  g_return_if_fail (accel_group != NULL);
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (accel_signal != NULL);
  
  /* check for required signals in the objects branch
   */
  signal = (gchar*) accel_signal;
  accel_signal_id = gtk_signal_lookup (signal, GTK_OBJECT_TYPE (object));
  if (accel_signal_id)
    {
      signal = "add-accelerator";
      add_accelerator_signal_id = gtk_signal_lookup (signal, GTK_OBJECT_TYPE (object));
    }
  if (add_accelerator_signal_id)
    {
      signal = "remove-accelerator";
      remove_accelerator_signal_id = gtk_signal_lookup (signal, GTK_OBJECT_TYPE (object));
    }
  if (!accel_signal_id ||
      !add_accelerator_signal_id ||
      !remove_accelerator_signal_id)
    {
      g_warning ("gtk_accel_group_add(): could not find signal \"%s\""
		 "in the `%s' class ancestry",
		 signal,
		 gtk_type_name (GTK_OBJECT_TYPE (object)));
      return;
    }
  query = gtk_signal_query (accel_signal_id);
  if (!query ||
      query->nparams > 0)
    {
      g_warning ("gtk_accel_group_add(): signal \"%s\" in the `%s' class ancestry"
		 "cannot be used as accelerator signal",
		 accel_signal,
		 gtk_type_name (GTK_OBJECT_TYPE (object)));
      if (query)
	g_free (query);

      return;
    }
  g_free (query);

  /* prematurely abort if the group/entry is already locked
   */
  if (accel_group->lock_count > 0)
    return;
  entry = gtk_accel_group_lookup (accel_group, accel_key, accel_mods);
  if (entry && entry->accel_flags & GTK_ACCEL_LOCKED)
    return;
  
  /* make sure our structures stay alive
   */
  gtk_accel_group_ref (accel_group);
  gtk_object_ref (object);
  
  /* remove an existing entry
   */
  if (entry)
    gtk_signal_emit (entry->object, remove_accelerator_signal_id,
		     accel_group,
		     gdk_keyval_to_lower (accel_key),
		     accel_mods & accel_group->modifier_mask);
  
  /* abort if the entry still exists
   */
  entry = gtk_accel_group_lookup (accel_group, accel_key, accel_mods);
  if (entry)
    {
      gtk_accel_group_unref (accel_group);
      gtk_object_unref (object);
      
      return;
    }
  
  /* collect accel groups and remove existing entries
   */
  attach_objects = accel_group->attach_objects;
  groups = NULL;
  for (attach_objects = accel_group->attach_objects; attach_objects; attach_objects = attach_objects->next)
    {
      GSList *tmp_groups;
      
      tmp_groups = gtk_object_get_data_by_id (attach_objects->data, accel_groups_key_id);
      while (tmp_groups)
	{
	  groups = g_slist_prepend (groups, tmp_groups->data);
	  gtk_accel_group_ref (tmp_groups->data);
	  tmp_groups = tmp_groups->next;
	}
    }
  for (slist = groups; slist; slist = slist->next)
    {
      GtkAccelGroup *tmp_group;
      
      tmp_group = slist->data;
      
      /* we only remove the accelerator if neccessary
       */
      if (tmp_group->lock_count == 0)
	{
	  entry = gtk_accel_group_lookup (tmp_group, accel_key, accel_mods);
	  if (entry && !(entry->accel_flags & GTK_ACCEL_LOCKED))
	    gtk_signal_emit (entry->object, remove_accelerator_signal_id,
			     tmp_group,
			     gdk_keyval_to_lower (accel_key),
			     accel_mods & tmp_group->modifier_mask);
	}
      gtk_accel_group_unref (tmp_group);
    }
  g_slist_free (groups);
  
  /* now install the new accelerator
   */
  entry = gtk_accel_group_lookup (accel_group, accel_key, accel_mods);
  if (!entry)
    gtk_signal_emit (object, add_accelerator_signal_id,
		     accel_signal_id,
		     accel_group,
		     gdk_keyval_to_lower (accel_key),
		     accel_mods & accel_group->modifier_mask,
		     accel_flags & GTK_ACCEL_MASK);
  
  /* and release the structures again
   */
  gtk_accel_group_unref (accel_group);
  gtk_object_unref (object);
}

static void
gtk_accel_group_delete_entries (GtkObject *object)
{
  GSList *free_slist, *slist;
  
  gtk_signal_disconnect_by_func (object,
				 GTK_SIGNAL_FUNC (gtk_accel_group_delete_entries),
				 NULL);

  /* we remove all entries of this object the hard
   * way (i.e. without signal emission).
   */
  free_slist = gtk_object_get_data_by_id (object, accel_entries_key_id);
  gtk_object_set_data_by_id (object, accel_entries_key_id, NULL);
  for (slist = free_slist; slist; slist = slist->next)
    {
      GtkAccelEntry *entry;
      
      entry = slist->data;
      
      g_hash_table_remove (accel_entry_hash_table, entry);
      gtk_accel_group_unref (entry->accel_group);
      g_chunk_free (entry, accel_entries_mem_chunk);
    }
  g_slist_free (free_slist);
}

void
gtk_accel_group_handle_add (GtkObject	      *object,
			    guint	       accel_signal_id,
			    GtkAccelGroup     *accel_group,
			    guint	       accel_key,
			    GdkModifierType    accel_mods,
			    GtkAccelFlags      accel_flags)
{
  GtkAccelEntry *entry;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (accel_group != NULL);
  g_return_if_fail (accel_signal_id > 0);

  if (!gtk_accelerator_valid (accel_key, accel_mods))
    return;
  
  entry = gtk_accel_group_lookup (accel_group, accel_key, accel_mods);
  if (!entry)
    {
      GSList *slist;
      
      gtk_accel_group_ref (accel_group);
      
      entry = g_chunk_new (GtkAccelEntry, accel_entries_mem_chunk);
      entry->accel_group = accel_group;
      entry->accelerator_key = gdk_keyval_to_lower (accel_key);
      entry->accelerator_mods = accel_mods & accel_group->modifier_mask;
      entry->accel_flags = accel_flags & GTK_ACCEL_MASK;
      entry->object = object;
      entry->signal_id = accel_signal_id;
      
      g_hash_table_insert (accel_entry_hash_table, entry, entry);
      
      slist = gtk_object_get_data_by_id (object, accel_entries_key_id);
      if (!slist)
	gtk_signal_connect (object,
			    "destroy",
			    GTK_SIGNAL_FUNC (gtk_accel_group_delete_entries),
			    NULL);
      slist = g_slist_prepend (slist, entry);
      gtk_object_set_data_by_id (object, accel_entries_key_id, slist);
    }
}

void
gtk_accel_group_remove (GtkAccelGroup	  *accel_group,
			guint		   accel_key,
			GdkModifierType	   accel_mods,
			GtkObject	  *object)
{
  GtkAccelEntry *entry;
  guint remove_accelerator_signal_id = 0;
  
  g_return_if_fail (accel_group != NULL);
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  /* check for required signals in the objects branch
   */
  remove_accelerator_signal_id = gtk_signal_lookup ("remove-accelerator", GTK_OBJECT_TYPE (object));
  if (!remove_accelerator_signal_id)
    {
      g_warning ("gtk_accel_group_remove(): could not find signal \"%s\""
		 "in the `%s' class ancestry",
		 "remove-accelerator",
		 gtk_type_name (GTK_OBJECT_TYPE (object)));
      return;
    }
  
  /* prematurely abort if the entry is locked
   */
  if (accel_group->lock_count > 0)
    return;
  entry = gtk_accel_group_lookup (accel_group, accel_key, accel_mods);
  if (!entry ||
      entry->accel_flags & GTK_ACCEL_LOCKED)
    return;
  if (entry->object != object)
    {
      g_warning ("gtk_accel_group_remove(): invalid object reference for accel-group entry");
      return;
    }
  
  /* make sure our structures stay alive
   */
  gtk_accel_group_ref (accel_group);
  gtk_object_ref (object);
  
  /* remove the entry
   */
  gtk_signal_emit (entry->object, remove_accelerator_signal_id,
		   accel_group,
		   gdk_keyval_to_lower (accel_key),
		   accel_mods & accel_group->modifier_mask);
  
  /* and release the structures again
   */
  gtk_accel_group_unref (accel_group);
  gtk_object_unref (object);
}

void
gtk_accel_group_handle_remove (GtkObject	 *object,
			       GtkAccelGroup	 *accel_group,
			       guint		  accel_key,
			       GdkModifierType	  accel_mods)
{
  GtkAccelEntry *entry;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (accel_group != NULL);
  
  entry = gtk_accel_group_lookup (accel_group, accel_key, accel_mods);
  if (entry)
    {
      if (entry->object == object)
	{
	  GSList *slist;
	  
	  g_hash_table_remove (accel_entry_hash_table, entry);
	  
	  slist = gtk_object_get_data_by_id (object, accel_entries_key_id);
	  if (slist)
	    {
	      slist = g_slist_remove (slist, entry);
	      if (!slist)
		gtk_signal_disconnect_by_func (object,
					       GTK_SIGNAL_FUNC (gtk_accel_group_delete_entries),
					       NULL);
	      gtk_object_set_data_by_id (object, accel_entries_key_id, slist);
	      
	      gtk_accel_group_unref (accel_group);
	      
	      g_chunk_free (entry, accel_entries_mem_chunk);
	    }
	}
      else
	g_warning ("gtk_accel_group_handle_remove(): invalid object reference for accel-group entry");
    }
  else
    g_warning ("gtk_accel_group_handle_remove(): attempt to remove unexisting accel-group entry");
}

guint
gtk_accel_group_create_add (GtkType          class_type,
			    GtkSignalRunType signal_flags,
			    guint            handler_offset)
{
  g_return_val_if_fail (gtk_type_is_a (class_type, GTK_TYPE_OBJECT), 0);

  return gtk_signal_new ("add-accelerator",
			 signal_flags,
			 class_type,
			 handler_offset,
			 gtk_marshal_NONE__UINT_POINTER_UINT_UINT_ENUM,
			 GTK_TYPE_NONE, 5,
			 GTK_TYPE_UINT,
			 GTK_TYPE_ACCEL_GROUP,
			 GTK_TYPE_UINT,
			 GTK_TYPE_GDK_MODIFIER_TYPE,
			 GTK_TYPE_ACCEL_FLAGS);
}

guint
gtk_accel_group_create_remove (GtkType          class_type,
			       GtkSignalRunType signal_flags,
			       guint            handler_offset)
{
  g_return_val_if_fail (gtk_type_is_a (class_type, GTK_TYPE_OBJECT), 0);

  return gtk_signal_new ("remove-accelerator",
			 signal_flags,
			 class_type,
			 handler_offset,
			 gtk_marshal_NONE__POINTER_UINT_UINT,
			 GTK_TYPE_NONE, 3,
			 GTK_TYPE_ACCEL_GROUP,
			 GTK_TYPE_UINT,
			 GTK_TYPE_GDK_MODIFIER_TYPE);
}

GSList*
gtk_accel_groups_from_object (GtkObject	     *object)
{
  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);
  
  return gtk_object_get_data_by_id (object, accel_groups_key_id);
}

GSList*
gtk_accel_group_entries_from_object (GtkObject	     *object)
{
  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);
  
  return gtk_object_get_data_by_id (object, accel_entries_key_id);
}

gboolean
gtk_accelerator_valid (guint		  keyval,
		       GdkModifierType	  modifiers)
{
  static const guint invalid_accelerator_vals[] = {
    GDK_BackSpace, GDK_Delete, GDK_KP_Delete,
    GDK_Shift_L, GDK_Shift_R, GDK_Shift_Lock, GDK_Caps_Lock, GDK_ISO_Lock,
    GDK_Control_L, GDK_Control_R, GDK_Meta_L, GDK_Meta_R,
    GDK_Alt_L, GDK_Alt_R, GDK_Super_L, GDK_Super_R, GDK_Hyper_L, GDK_Hyper_R,
    GDK_Mode_switch, GDK_Num_Lock, GDK_Multi_key,
    GDK_Scroll_Lock, GDK_Sys_Req, 
    GDK_Up, GDK_Down, GDK_Left, GDK_Right, GDK_Tab, GDK_ISO_Left_Tab,
    GDK_KP_Up, GDK_KP_Down, GDK_KP_Left, GDK_KP_Right, GDK_KP_Tab,
    GDK_First_Virtual_Screen, GDK_Prev_Virtual_Screen,
    GDK_Next_Virtual_Screen, GDK_Last_Virtual_Screen,
    GDK_Terminate_Server, GDK_AudibleBell_Enable,
    0
  };
  const guint *ac_val;

  modifiers &= GDK_MODIFIER_MASK;
    
  if (keyval <= 0xFF)
    return keyval >= 0x20;

  ac_val = invalid_accelerator_vals;
  while (*ac_val)
    {
      if (keyval == *ac_val++)
	return FALSE;
    }

  return TRUE;
}

static inline gboolean
is_alt (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 'a' || string[1] == 'A') &&
	  (string[2] == 'l' || string[2] == 'L') &&
	  (string[3] == 't' || string[3] == 'T') &&
	  (string[4] == '>'));
}

static inline gboolean
is_ctl (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 'c' || string[1] == 'C') &&
	  (string[2] == 't' || string[2] == 'T') &&
	  (string[3] == 'l' || string[3] == 'L') &&
	  (string[4] == '>'));
}

static inline gboolean
is_modx (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 'm' || string[1] == 'M') &&
	  (string[2] == 'o' || string[2] == 'O') &&
	  (string[3] == 'd' || string[3] == 'D') &&
	  (string[4] >= '1' && string[4] <= '5') &&
	  (string[5] == '>'));
}

static inline gboolean
is_ctrl (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 'c' || string[1] == 'C') &&
	  (string[2] == 't' || string[2] == 'T') &&
	  (string[3] == 'r' || string[3] == 'R') &&
	  (string[4] == 'l' || string[4] == 'L') &&
	  (string[5] == '>'));
}

static inline gboolean
is_shft (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 's' || string[1] == 'S') &&
	  (string[2] == 'h' || string[2] == 'H') &&
	  (string[3] == 'f' || string[3] == 'F') &&
	  (string[4] == 't' || string[4] == 'T') &&
	  (string[5] == '>'));
}

static inline gboolean
is_shift (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 's' || string[1] == 'S') &&
	  (string[2] == 'h' || string[2] == 'H') &&
	  (string[3] == 'i' || string[3] == 'I') &&
	  (string[4] == 'f' || string[4] == 'F') &&
	  (string[5] == 't' || string[5] == 'T') &&
	  (string[6] == '>'));
}

static inline gboolean
is_control (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 'c' || string[1] == 'C') &&
	  (string[2] == 'o' || string[2] == 'O') &&
	  (string[3] == 'n' || string[3] == 'N') &&
	  (string[4] == 't' || string[4] == 'T') &&
	  (string[5] == 'r' || string[5] == 'R') &&
	  (string[6] == 'o' || string[6] == 'O') &&
	  (string[7] == 'l' || string[7] == 'L') &&
	  (string[8] == '>'));
}

static inline gboolean
is_release (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 'r' || string[1] == 'R') &&
	  (string[2] == 'e' || string[2] == 'E') &&
	  (string[3] == 'l' || string[3] == 'L') &&
	  (string[4] == 'e' || string[4] == 'E') &&
	  (string[5] == 'a' || string[5] == 'A') &&
	  (string[6] == 's' || string[6] == 'S') &&
	  (string[7] == 'e' || string[7] == 'E') &&
	  (string[8] == '>'));
}

void
gtk_accelerator_parse (const gchar    *accelerator,
		       guint          *accelerator_key,
		       GdkModifierType*accelerator_mods)
{
  guint keyval;
  GdkModifierType mods;
  gint len;
  
  if (accelerator_key)
    *accelerator_key = 0;
  if (accelerator_mods)
    *accelerator_mods = 0;
  g_return_if_fail (accelerator != NULL);
  
  keyval = 0;
  mods = 0;
  len = strlen (accelerator);
  while (len)
    {
      if (*accelerator == '<')
	{
	  if (len >= 9 && is_release (accelerator))
	    {
	      accelerator += 9;
	      len -= 9;
	      mods |= GDK_RELEASE_MASK;
	    }
	  else if (len >= 9 && is_control (accelerator))
	    {
	      accelerator += 9;
	      len -= 9;
	      mods |= GDK_CONTROL_MASK;
	    }
	  else if (len >= 7 && is_shift (accelerator))
	    {
	      accelerator += 7;
	      len -= 7;
	      mods |= GDK_SHIFT_MASK;
	    }
	  else if (len >= 6 && is_shft (accelerator))
	    {
	      accelerator += 6;
	      len -= 6;
	      mods |= GDK_SHIFT_MASK;
	    }
	  else if (len >= 6 && is_ctrl (accelerator))
	    {
	      accelerator += 6;
	      len -= 6;
	      mods |= GDK_CONTROL_MASK;
	    }
	  else if (len >= 6 && is_modx (accelerator))
	    {
	      static const guint mod_vals[] = {
		GDK_MOD1_MASK, GDK_MOD2_MASK, GDK_MOD3_MASK,
		GDK_MOD4_MASK, GDK_MOD5_MASK
	      };

	      len -= 6;
	      accelerator += 4;
	      mods |= mod_vals[*accelerator - '1'];
	      accelerator += 2;
	    }
	  else if (len >= 5 && is_ctl (accelerator))
	    {
	      accelerator += 5;
	      len -= 5;
	      mods |= GDK_CONTROL_MASK;
	    }
	  else if (len >= 5 && is_alt (accelerator))
	    {
	      accelerator += 5;
	      len -= 5;
	      mods |= GDK_MOD1_MASK;
	    }
	  else
	    {
	      gchar last_ch;
	      
	      last_ch = *accelerator;
	      while (last_ch && last_ch != '>')
		{
		  last_ch = *accelerator;
		  accelerator += 1;
		  len -= 1;
		}
	    }
	}
      else
	{
	  keyval = gdk_keyval_from_name (accelerator);
	  accelerator += len;
	  len -= len;
	}
    }
  
  if (accelerator_key)
    *accelerator_key = gdk_keyval_to_lower (keyval);
  if (accelerator_mods)
    *accelerator_mods = mods;
}

gchar*
gtk_accelerator_name (guint           accelerator_key,
		      GdkModifierType accelerator_mods)
{
  static const gchar text_release[] = "<Release>";
  static const gchar text_shift[] = "<Shift>";
  static const gchar text_control[] = "<Control>";
  static const gchar text_mod1[] = "<Alt>";
  static const gchar text_mod2[] = "<Mod2>";
  static const gchar text_mod3[] = "<Mod3>";
  static const gchar text_mod4[] = "<Mod4>";
  static const gchar text_mod5[] = "<Mod5>";
  guint l;
  gchar *keyval_name;
  gchar *accelerator;

  accelerator_mods &= GDK_MODIFIER_MASK;

  keyval_name = gdk_keyval_name (gdk_keyval_to_lower (accelerator_key));
  if (!keyval_name)
    keyval_name = "";

  l = 0;
  if (accelerator_mods & GDK_RELEASE_MASK)
    l += sizeof (text_release) - 1;
  if (accelerator_mods & GDK_SHIFT_MASK)
    l += sizeof (text_shift) - 1;
  if (accelerator_mods & GDK_CONTROL_MASK)
    l += sizeof (text_control) - 1;
  if (accelerator_mods & GDK_MOD1_MASK)
    l += sizeof (text_mod1) - 1;
  if (accelerator_mods & GDK_MOD2_MASK)
    l += sizeof (text_mod2) - 1;
  if (accelerator_mods & GDK_MOD3_MASK)
    l += sizeof (text_mod3) - 1;
  if (accelerator_mods & GDK_MOD4_MASK)
    l += sizeof (text_mod4) - 1;
  if (accelerator_mods & GDK_MOD5_MASK)
    l += sizeof (text_mod5) - 1;
  l += strlen (keyval_name);

  accelerator = g_new (gchar, l + 1);

  l = 0;
  accelerator[l] = 0;
  if (accelerator_mods & GDK_RELEASE_MASK)
    {
      strcpy (accelerator + l, text_release);
      l += sizeof (text_release) - 1;
    }
  if (accelerator_mods & GDK_SHIFT_MASK)
    {
      strcpy (accelerator + l, text_shift);
      l += sizeof (text_shift) - 1;
    }
  if (accelerator_mods & GDK_CONTROL_MASK)
    {
      strcpy (accelerator + l, text_control);
      l += sizeof (text_control) - 1;
    }
  if (accelerator_mods & GDK_MOD1_MASK)
    {
      strcpy (accelerator + l, text_mod1);
      l += sizeof (text_mod1) - 1;
    }
  if (accelerator_mods & GDK_MOD2_MASK)
    {
      strcpy (accelerator + l, text_mod2);
      l += sizeof (text_mod2) - 1;
    }
  if (accelerator_mods & GDK_MOD3_MASK)
    {
      strcpy (accelerator + l, text_mod3);
      l += sizeof (text_mod3) - 1;
    }
  if (accelerator_mods & GDK_MOD4_MASK)
    {
      strcpy (accelerator + l, text_mod4);
      l += sizeof (text_mod4) - 1;
    }
  if (accelerator_mods & GDK_MOD5_MASK)
    {
      strcpy (accelerator + l, text_mod5);
      l += sizeof (text_mod5) - 1;
    }
  strcpy (accelerator + l, keyval_name);

  return accelerator;
}

void
gtk_accelerator_set_default_mod_mask (GdkModifierType default_mod_mask)
{
  default_accel_mod_mask = default_mod_mask & GDK_MODIFIER_MASK;
}

guint
gtk_accelerator_get_default_mod_mask (void)
{
  return default_accel_mod_mask;
}
