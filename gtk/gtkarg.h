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

#ifndef __GTK_ARG_H__
#define __GTK_ARG_H__


#include <gtk/gtktypeutils.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */




typedef struct _GtkArgInfo	 GtkArgInfo;

struct _GtkArgInfo
{
  /* hash key portion */
  GtkType class_type;
  gchar *name;
  
  GtkType type;
  guint arg_flags;
  gchar *full_name;
  
  /* private fields */
  guint arg_id;
  guint seq_id;
};


/* Non-public methods */

GtkArg*		gtk_arg_new		 (GtkType	arg_type);
GtkArg*		gtk_arg_copy		 (GtkArg       *src_arg,
					  GtkArg       *dest_arg);
void		gtk_arg_free		 (GtkArg       *arg,
					  gboolean	free_contents);
void		gtk_arg_reset		 (GtkArg       *arg);
gboolean	gtk_arg_values_equal	 (const GtkArg *arg1,
					  const GtkArg *arg2);
gchar*		gtk_args_collect	 (GtkType	object_type,
					  GHashTable    *arg_info_hash_table,
					  GSList      **arg_list_p,
					  GSList      **info_list_p,
					  const gchar   *first_arg_name,
					  va_list	var_args);
void		gtk_args_collect_cleanup (GSList       *arg_list,
					  GSList       *info_list);
gchar*		gtk_arg_get_info	 (GtkType	object_type,
					  GHashTable    *arg_info_hash_table,
					  const gchar   *arg_name,
					  GtkArgInfo   **info_p);
GtkArgInfo*	gtk_arg_type_new_static	 (GtkType	base_class_type,
					  const gchar   *arg_name,
					  guint		class_n_args_offset,
					  GHashTable    *arg_info_hash_table,
					  GtkType	arg_type,
					  guint		arg_flags,
					  guint		arg_id);
GtkArg*		gtk_args_query		 (GtkType	class_type,
					  GHashTable    *arg_info_hash_table,
					  guint32      **arg_flags,
					  guint	       *n_args_p);
gchar*		gtk_arg_name_strip_type	 (const gchar   *arg_name);
gint		gtk_arg_info_equal	 (gconstpointer	 arg_info_1,
					  gconstpointer	 arg_info_2);
guint		gtk_arg_info_hash	 (gconstpointer	 arg_info);
void		gtk_arg_to_valueloc	 (GtkArg	*arg,
					  gpointer	 value_pointer);
       



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ARG_H__ */
