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

#ifndef __GTK_ACCEL_GROUP_H__
#define __GTK_ACCEL_GROUP_H__


#include <gdk/gdk.h>
#include <gtk/gtkobject.h>
#include <gtk/gtkenums.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct _GtkAccelGroup	GtkAccelGroup;
typedef struct _GtkAccelEntry	GtkAccelEntry;

typedef enum
{
  /* should the accelerator appear in
   * the widget's display?
   */
  GTK_ACCEL_VISIBLE        = 1 << 0,
  /* should the signal associated with
   * this accelerator be also visible?
   */
  GTK_ACCEL_SIGNAL_VISIBLE = 1 << 1,
  /* may the accelerator be removed
   * again?
   */
  GTK_ACCEL_LOCKED         = 1 << 2,
  GTK_ACCEL_MASK           = 0x07
} GtkAccelFlags;

struct _GtkAccelGroup
{
  guint	          ref_count;
  guint	          lock_count;
  GdkModifierType modifier_mask;
  GSList         *attach_objects;
};

struct _GtkAccelEntry
{
  /* key portion
   */
  GtkAccelGroup		*accel_group;
  guint			 accelerator_key;
  GdkModifierType	 accelerator_mods;
  
  GtkAccelFlags		 accel_flags;
  GtkObject		*object;
  guint			 signal_id;
};


/* Accelerators
 */
gboolean gtk_accelerator_valid		      (guint	        keyval,
					       GdkModifierType  modifiers);
void	 gtk_accelerator_parse		      (const gchar     *accelerator,
					       guint	       *accelerator_key,
					       GdkModifierType *accelerator_mods);
gchar*	 gtk_accelerator_name		      (guint	        accelerator_key,
					       GdkModifierType  accelerator_mods);
void	 gtk_accelerator_set_default_mod_mask (GdkModifierType  default_mod_mask);
guint	 gtk_accelerator_get_default_mod_mask (void);


/* Accelerator Groups
 */
GtkAccelGroup*  gtk_accel_group_new	      	(void);
GtkAccelGroup*  gtk_accel_group_get_default    	(void);
GtkAccelGroup*  gtk_accel_group_ref	     	(GtkAccelGroup	*accel_group);
void	        gtk_accel_group_unref	      	(GtkAccelGroup	*accel_group);
void		gtk_accel_group_lock		(GtkAccelGroup	*accel_group);
void		gtk_accel_group_unlock		(GtkAccelGroup	*accel_group);
gboolean        gtk_accel_groups_activate      	(GtkObject	*object,
						 guint		 accel_key,
						 GdkModifierType accel_mods);

/* internal functions
 */
gboolean        gtk_accel_group_activate	(GtkAccelGroup	*accel_group,
						 guint		 accel_key,
						 GdkModifierType accel_mods);
void		gtk_accel_group_attach		(GtkAccelGroup	*accel_group,
						 GtkObject	*object);
void		gtk_accel_group_detach		(GtkAccelGroup	*accel_group,
						 GtkObject	*object);

/* Accelerator Group Entries (internal)
 */
GtkAccelEntry* 	gtk_accel_group_get_entry      	(GtkAccelGroup  *accel_group,
						 guint           accel_key,
						 GdkModifierType accel_mods);
void		gtk_accel_group_lock_entry	(GtkAccelGroup	*accel_group,
						 guint		 accel_key,
						 GdkModifierType accel_mods);
void		gtk_accel_group_unlock_entry	(GtkAccelGroup	*accel_group,
						 guint		 accel_key,
						 GdkModifierType accel_mods);
void		gtk_accel_group_add		(GtkAccelGroup	*accel_group,
						 guint		 accel_key,
						 GdkModifierType accel_mods,
						 GtkAccelFlags	 accel_flags,
						 GtkObject	*object,
						 const gchar	*accel_signal);
void		gtk_accel_group_remove		(GtkAccelGroup	*accel_group,
						 guint		 accel_key,
						 GdkModifierType accel_mods,
						 GtkObject	*object);

/* Accelerator Signals (internal)
 */
void		gtk_accel_group_handle_add	(GtkObject	*object,
						 guint		 accel_signal_id,
						 GtkAccelGroup	*accel_group,
						 guint		 accel_key,
						 GdkModifierType accel_mods,
						 GtkAccelFlags   accel_flags);
void		gtk_accel_group_handle_remove	(GtkObject	*object,
						 GtkAccelGroup	*accel_group,
						 guint		 accel_key,
						 GdkModifierType accel_mods);
guint		gtk_accel_group_create_add	(GtkType	 class_type,
						 GtkSignalRunType signal_flags,
						 guint		 handler_offset);
guint		gtk_accel_group_create_remove	(GtkType	 class_type,
						 GtkSignalRunType signal_flags,
						 guint		 handler_offset);

/* Miscellaneous (internal)
 */
GSList*	gtk_accel_groups_from_object		(GtkObject	*object);
GSList*	gtk_accel_group_entries_from_object	(GtkObject	*object);



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ACCEL_GROUP_H__ */
