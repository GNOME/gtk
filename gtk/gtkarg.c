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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdarg.h>
#include <string.h>
#include "gtkobject.h"
#include "gtkargcollector.c"


#define	MAX_ARG_LENGTH	(256)


/* --- typedefs --- */
typedef struct _GtkArgQueryData	GtkArgQueryData;


/* --- structures --- */
struct _GtkArgQueryData
{
  GList *arg_list;
  GtkType class_type;
};



/* --- functions --- */
GtkArgInfo*
gtk_arg_type_new_static (GtkType      base_class_type,
			 const gchar *arg_name,
			 guint	      class_n_args_offset,
			 GHashTable  *arg_info_hash_table,
			 GtkType      arg_type,
			 guint	      arg_flags,
			 guint	      arg_id)
{
  GtkArgInfo *info;
  gchar class_part[MAX_ARG_LENGTH];
  gchar *arg_part;
  GtkType class_type;
  guint class_offset;
  guint *n_args_p;
  gchar *p;

  g_return_val_if_fail (arg_name != NULL, NULL);
  g_return_val_if_fail (GTK_FUNDAMENTAL_TYPE (base_class_type) == GTK_TYPE_OBJECT, NULL);
  g_return_val_if_fail (class_n_args_offset != 0, NULL);
  g_return_val_if_fail (arg_info_hash_table != NULL, NULL);
  g_return_val_if_fail (arg_type > GTK_TYPE_NONE, NULL);
  g_return_val_if_fail (arg_id > 0, NULL);
  g_return_val_if_fail ((arg_flags & GTK_ARG_READWRITE) != 0, NULL);
  /* g_return_val_if_fail ((arg_flags & GTK_ARG_CHILD_ARG) == 0, NULL); */
  
  arg_flags &= GTK_ARG_MASK;

  arg_part = strchr (arg_name, ':');
  if (!arg_part || (arg_part[0] != ':') || (arg_part[1] != ':'))
    {
      g_warning ("gtk_arg_type_new(): invalid arg name: \"%s\"\n", arg_name);
      return NULL;
    }

  class_offset = (guint) (arg_part - arg_name);
  strncpy (class_part, arg_name, class_offset);
  class_part[class_offset] = 0;

  class_type = gtk_type_from_name (class_part);
  if (!gtk_type_is_a (class_type, base_class_type))
    {
      g_warning ("gtk_arg_type_new(): argument class in \"%s\" is not in the `%s' ancestry",
		 arg_name,
		 gtk_type_name (base_class_type));
      return NULL;
    }

  p = gtk_type_class (class_type);
  p += class_n_args_offset;
  n_args_p = (guint*) p;
  *n_args_p += 1;

  info = g_new (GtkArgInfo, 1);
  info->class_type = class_type;
  info->full_name = (gchar*) arg_name; /* _static */
  info->name = info->full_name + class_offset + 2;
  info->type = arg_type;
  info->arg_flags = arg_flags;
  info->arg_id = arg_id;
  info->seq_id = *n_args_p;

  g_hash_table_insert (arg_info_hash_table, info, info);

  return info;
}

gchar*
gtk_arg_name_strip_type (const gchar   *arg_name)
{
  gchar buffer[MAX_ARG_LENGTH];
  gchar *p;

  /* security audit
   */
  if (!arg_name || strlen (arg_name) > MAX_ARG_LENGTH - 8)
    return NULL;

  p = strchr (arg_name, ':');
  if (p)
    {
      guint len;

      if ((p[0] != ':') || (p[1] != ':') || (p[2] == 0))
	return NULL;
      len = (guint) (p - arg_name);
      strncpy (buffer, arg_name, len);
      buffer[len] = 0;

      if (gtk_type_from_name (buffer) != GTK_TYPE_INVALID)
	return p + 2;
    }

  return (gchar*) arg_name;
}

gchar*
gtk_arg_get_info (GtkType       object_type,
		  GHashTable   *arg_info_hash_table,
		  const gchar  *arg_name,
		  GtkArgInfo  **info_p)
{
  GtkType otype;
  gchar buffer[MAX_ARG_LENGTH];
  guint len;
  gchar *p;
  
  *info_p = NULL;
  
  /* security audit
   */
  if (!arg_name || strlen (arg_name) > MAX_ARG_LENGTH - 8)
    return g_strdup ("argument name exceeds maximum size.");

  /* split off the object-type part
   */
  p = strchr (arg_name, ':');
  if (p)
    {
      if ((p[0] != ':') || (p[1] != ':'))
	return g_strconcat ("invalid argument syntax: \"",
			    arg_name,
			    "\"",
			    NULL);
      len = (guint) (p - arg_name);
      strncpy (buffer, arg_name, len);
      buffer[len] = 0;

      otype = gtk_type_from_name (buffer);
      if (otype != GTK_TYPE_INVALID)
	arg_name = p + 2;
    }
  else
    otype = GTK_TYPE_INVALID;

  /* split off the argument name
   */
  p = strchr (arg_name, ':');
  if (p)
    {
      if ((p[0] != ':') || (p[1] != ':'))
	return g_strconcat ("invalid argument syntax: \"",
			    arg_name,
			    "\"",
			    NULL);
      len = (guint) (p - arg_name);
      strncpy (buffer, arg_name, len);
      buffer[len] = 0;
      arg_name = buffer;
    }

  /* lookup the argument
   */
  if (otype != GTK_TYPE_INVALID)
    {
      GtkArgInfo info;

      info.class_type = otype;
      info.name = (gchar*) arg_name;

      *info_p = g_hash_table_lookup (arg_info_hash_table, &info);
      if (*info_p && !gtk_type_is_a (object_type, (*info_p)->class_type))
	*info_p = NULL;
    }
  else
    {
      otype = object_type;
      while (!*info_p && GTK_FUNDAMENTAL_TYPE (otype) == GTK_TYPE_OBJECT)
	{
	  GtkArgInfo info;
	  
	  info.class_type = otype;
	  info.name = (gchar*) arg_name;
	  
	  *info_p = g_hash_table_lookup (arg_info_hash_table, &info);
	  
	  otype = gtk_type_parent (otype);
	}
    }
  
  if (!*info_p)
    return g_strconcat ("could not find argument \"",
			arg_name,
			"\" in the `",
			gtk_type_name (object_type),
			"' class ancestry",
			NULL);

  return NULL;
}

gchar*
gtk_args_collect (GtkType	  object_type,
		  GHashTable     *arg_info_hash_table,
		  GSList	**arg_list_p,
		  GSList	**info_list_p,
		  const gchar   *first_arg_name,
		  va_list	 var_args)
{
  GSList *arg_list;
  GSList *info_list;
  const gchar *arg_name;

  g_return_val_if_fail (arg_list_p != NULL, NULL);
  *arg_list_p = NULL;
  g_return_val_if_fail (info_list_p != NULL, NULL);
  *info_list_p = 0;
  g_return_val_if_fail (arg_info_hash_table != NULL, NULL);

  arg_list = NULL;
  info_list = NULL;
  arg_name = first_arg_name;
  while (arg_name)
    {
      GtkArgInfo *info = NULL;
      gchar *error;

      error = gtk_arg_get_info (object_type, arg_info_hash_table, arg_name, &info);
      if (!error)
	{
	  GtkArg *arg;

	  info_list = g_slist_prepend (info_list, info);

	  arg = gtk_arg_new (info->type);
	  arg->name = (gchar*) arg_name;
	  GTK_ARG_COLLECT_VALUE (arg, var_args, error);
	  arg_list = g_slist_prepend (arg_list, arg);
	}
      if (error)
	{
	  gtk_args_collect_cleanup (arg_list, info_list);

	  return error;
	}

      arg_name = va_arg (var_args, gchar*);
    }

  *arg_list_p = g_slist_reverse (arg_list);
  *info_list_p = g_slist_reverse (info_list);

  return NULL;
}

void
gtk_args_collect_cleanup (GSList	*arg_list,
			  GSList	*info_list)
{
  GSList *slist;
  
  g_slist_free (info_list);

  for (slist = arg_list; slist; slist = slist->next)
    gtk_arg_free (slist->data, FALSE);
  g_slist_free (arg_list);
}

static void
gtk_args_query_foreach (gpointer key,
			gpointer value,
			gpointer user_data)
{
  register GtkArgInfo *info;
  register GtkArgQueryData *data;

  g_assert (key == value); /* paranoid */

  info = value;
  data = user_data;

  if (info->class_type == data->class_type)
    data->arg_list = g_list_prepend (data->arg_list, info);
}

GtkArg*
gtk_args_query (GtkType	    class_type,
		GHashTable *arg_info_hash_table,
		guint32	  **arg_flags,
		guint      *n_args_p)
{
  GtkArg *args;
  GtkArgQueryData query_data;

  if (arg_flags)
    *arg_flags = NULL;
  g_return_val_if_fail (n_args_p != NULL, NULL);
  *n_args_p = 0;
  g_return_val_if_fail (arg_info_hash_table != NULL, NULL);

  /* make sure the types class has been initialized, because
   * the argument setup happens in the gtk_*_class_init() functions.
   */
  gtk_type_class (class_type);

  query_data.arg_list = NULL;
  query_data.class_type = class_type;
  g_hash_table_foreach (arg_info_hash_table, gtk_args_query_foreach, &query_data);

  if (query_data.arg_list)
    {
      register GList	*list;
      register guint	len;

      list = query_data.arg_list;
      len = 1;
      while (list->next)
	{
	  len++;
	  list = list->next;
	}

      args = g_new0 (GtkArg, len);
      *n_args_p = len;
      if (arg_flags)
	*arg_flags = g_new (guint32, len);

      do
	{
	  GtkArgInfo *info;

	  info = list->data;
	  list = list->prev;

	  g_assert (info->seq_id > 0 && info->seq_id <= len); /* paranoid */

	  args[info->seq_id - 1].type = info->type;
	  args[info->seq_id - 1].name = info->full_name;
	  if (arg_flags)
	    (*arg_flags)[info->seq_id - 1] = info->arg_flags;
	}
      while (list);

      g_list_free (query_data.arg_list);
    }
  else
    args = NULL;

  return args;
}

GtkArg*
gtk_arg_new (GtkType  arg_type)
{
  GtkArg *arg;

  arg = g_new0 (GtkArg, 1);
  arg->type = arg_type;
  arg->name = NULL;

  return arg;
}

GtkArg*
gtk_arg_copy (GtkArg         *src_arg,
	      GtkArg         *dest_arg)
{
  g_return_val_if_fail (src_arg != NULL, NULL);

  if (!dest_arg)
    {
      dest_arg = g_new0 (GtkArg, 1);
      dest_arg->name = src_arg->name;
    }

  dest_arg->type = src_arg->type;
  dest_arg->d = src_arg->d;

  if (GTK_FUNDAMENTAL_TYPE (src_arg->type) == GTK_TYPE_STRING)
    GTK_VALUE_STRING (*dest_arg) = g_strdup (GTK_VALUE_STRING (*src_arg));

  return dest_arg;
}

void
gtk_arg_free (GtkArg  *arg,
	      gboolean free_contents)
{
  g_return_if_fail (arg != NULL);

  if (free_contents)
    gtk_arg_reset (arg);
  g_free (arg);
}

void
gtk_arg_reset (GtkArg *arg)
{
  GtkType fundamental_type;

  g_return_if_fail (arg != NULL);

  fundamental_type = GTK_FUNDAMENTAL_TYPE (arg->type);
  if (fundamental_type > GTK_TYPE_FUNDAMENTAL_LAST)
    {
      fundamental_type = gtk_type_get_varargs_type (fundamental_type);
      if (!fundamental_type)
        fundamental_type = GTK_FUNDAMENTAL_TYPE (arg->type);
    }

  if (fundamental_type == GTK_TYPE_STRING)
    {
      g_free (GTK_VALUE_STRING (*arg));
      arg->type = GTK_TYPE_INVALID;
    }
  else if (arg->type != GTK_TYPE_INVALID)
    arg->type = GTK_TYPE_INVALID;
}

gint
gtk_arg_info_equal (gconstpointer arg_info_1,
		    gconstpointer arg_info_2)
{
  register const GtkArgInfo *info1 = arg_info_1;
  register const GtkArgInfo *info2 = arg_info_2;
  
  return ((info1->class_type == info2->class_type) &&
	  strcmp (info1->name, info2->name) == 0);
}

guint
gtk_arg_info_hash (gconstpointer arg_info)
{
  register const GtkArgInfo *info = arg_info;
  register const gchar *p;
  register guint h = info->class_type >> 8;
  
  for (p = info->name; *p; p++)
    {
      register guint g;
      
      h = (h << 4) + *p;
      g = h & 0xf0000000;
      if (g)
	{
	  h = h ^ (g >> 24);
	  h = h ^ g;
	}
    }
  
  return h;
}

gboolean
gtk_arg_values_equal (const GtkArg *arg1,
		      const GtkArg *arg2)
{
  GtkType fundamental_type;
  gboolean equal;
  
  g_return_val_if_fail (arg1 != NULL, FALSE);
  g_return_val_if_fail (arg2 != NULL, FALSE);
  g_return_val_if_fail (GTK_FUNDAMENTAL_TYPE (arg1->type) ==
			GTK_FUNDAMENTAL_TYPE (arg2->type), FALSE);
  
  fundamental_type = GTK_FUNDAMENTAL_TYPE (arg1->type);
  if (fundamental_type > GTK_TYPE_FUNDAMENTAL_LAST)
    {
      fundamental_type = gtk_type_get_varargs_type (fundamental_type);
      if (!fundamental_type)
        fundamental_type = GTK_FUNDAMENTAL_TYPE (arg1->type);
    }
  
  switch (fundamental_type)
    {
    case GTK_TYPE_INVALID:
      equal = TRUE;
      break;
    case GTK_TYPE_CHAR:
      equal = GTK_VALUE_CHAR (*arg1) == GTK_VALUE_CHAR (*arg2);
      break;
    case GTK_TYPE_BOOL:
      equal = (GTK_VALUE_BOOL (*arg1) != FALSE) == (GTK_VALUE_BOOL (*arg2) != FALSE);
      break;
    case GTK_TYPE_INT:
      equal = GTK_VALUE_INT (*arg1) == GTK_VALUE_INT (*arg2);
      break;
    case GTK_TYPE_UINT:
      equal = GTK_VALUE_UINT (*arg1) == GTK_VALUE_UINT (*arg2);
      break;
    case GTK_TYPE_LONG:
      equal = GTK_VALUE_LONG (*arg1) == GTK_VALUE_LONG (*arg2);
      break;
    case GTK_TYPE_ULONG:
      equal = GTK_VALUE_ULONG (*arg1) == GTK_VALUE_ULONG (*arg2);
      break;
    case GTK_TYPE_FLOAT:
      equal = GTK_VALUE_FLOAT (*arg1) == GTK_VALUE_FLOAT (*arg2);
      break;
    case GTK_TYPE_DOUBLE:
      equal = GTK_VALUE_DOUBLE (*arg1) == GTK_VALUE_DOUBLE (*arg2);
      break;
    case GTK_TYPE_STRING:
      if (!GTK_VALUE_STRING (*arg1) ||
	  !GTK_VALUE_STRING (*arg2))
	equal = GTK_VALUE_STRING (*arg1) == GTK_VALUE_STRING (*arg2);
      else
	equal = g_str_equal (GTK_VALUE_STRING (*arg1), GTK_VALUE_STRING (*arg2));
      break;
    case GTK_TYPE_ENUM:
      equal = GTK_VALUE_ENUM (*arg1) == GTK_VALUE_ENUM (*arg2);
      break;
    case GTK_TYPE_FLAGS:
      equal = GTK_VALUE_FLAGS (*arg1) == GTK_VALUE_FLAGS (*arg2);
      break;
    case GTK_TYPE_BOXED:
      equal = GTK_VALUE_BOXED (*arg1) == GTK_VALUE_BOXED (*arg2);
      break;
    case GTK_TYPE_FOREIGN:
      equal = (GTK_VALUE_FOREIGN (*arg1).data == GTK_VALUE_FOREIGN (*arg2).data &&
	       GTK_VALUE_FOREIGN (*arg1).notify == GTK_VALUE_FOREIGN (*arg2).notify);
      break;
    case GTK_TYPE_CALLBACK:
      equal = (GTK_VALUE_CALLBACK (*arg1).marshal == GTK_VALUE_CALLBACK (*arg2).marshal &&
	       GTK_VALUE_CALLBACK (*arg1).data == GTK_VALUE_CALLBACK (*arg2).data &&
	       GTK_VALUE_CALLBACK (*arg1).notify == GTK_VALUE_CALLBACK (*arg2).notify);
      break;
    case GTK_TYPE_ARGS:
      equal = (GTK_VALUE_ARGS (*arg1).n_args == GTK_VALUE_ARGS (*arg2).n_args &&
	       GTK_VALUE_ARGS (*arg1).args == GTK_VALUE_ARGS (*arg2).args);
      break;
    case GTK_TYPE_OBJECT:
      equal = GTK_VALUE_OBJECT (*arg1) == GTK_VALUE_OBJECT (*arg2);
      break;
    case GTK_TYPE_POINTER:
      equal = GTK_VALUE_POINTER (*arg1) == GTK_VALUE_POINTER (*arg2);
      break;
    case GTK_TYPE_SIGNAL:
      equal = (GTK_VALUE_SIGNAL (*arg1).f == GTK_VALUE_SIGNAL (*arg2).f &&
	       GTK_VALUE_SIGNAL (*arg1).d == GTK_VALUE_SIGNAL (*arg2).d);
      break;
    case GTK_TYPE_C_CALLBACK:
      equal = (GTK_VALUE_C_CALLBACK (*arg1).func == GTK_VALUE_C_CALLBACK (*arg2).func &&
	       GTK_VALUE_C_CALLBACK (*arg1).func_data == GTK_VALUE_C_CALLBACK (*arg2).func_data);
      break;
    default:
      g_warning ("gtk_arg_values_equal() used with unknown type `%s'", gtk_type_name (arg1->type));
      equal = FALSE;
      break;
    }
  
  return equal;
}

void
gtk_arg_to_valueloc (GtkArg  *arg,
		     gpointer value_pointer)
{
  GtkType fundamental_type;
  
  g_return_if_fail (arg != NULL);
  g_return_if_fail (value_pointer != NULL);
  
  fundamental_type = GTK_FUNDAMENTAL_TYPE (arg->type);
  if (fundamental_type > GTK_TYPE_FUNDAMENTAL_LAST)
    {
      fundamental_type = gtk_type_get_varargs_type (fundamental_type);
      if (!fundamental_type)
        fundamental_type = GTK_FUNDAMENTAL_TYPE (arg->type);
    }
  
  switch (fundamental_type)
    {
      gchar *p_char;
      guchar *p_uchar;
      gboolean *p_boolean;
      gint *p_int;
      guint *p_uint;
      glong *p_long;
      gulong *p_ulong;
      gfloat *p_float;
      gdouble *p_double;
      gpointer *p_pointer;
    case GTK_TYPE_CHAR:
      p_char = value_pointer;
      *p_char = GTK_VALUE_CHAR (*arg);
      break;
    case GTK_TYPE_UCHAR:
      p_uchar = value_pointer;
      *p_uchar = GTK_VALUE_UCHAR (*arg);
      break;
    case GTK_TYPE_BOOL:
      p_boolean = value_pointer;
      *p_boolean = GTK_VALUE_BOOL (*arg);
      break;
    case GTK_TYPE_INT:
    case GTK_TYPE_ENUM:
      p_int = value_pointer;
      *p_int = GTK_VALUE_INT (*arg);
      break;
    case GTK_TYPE_UINT:
    case GTK_TYPE_FLAGS:
      p_uint = value_pointer;
      *p_uint = GTK_VALUE_UINT (*arg);
      break;
    case GTK_TYPE_LONG:
      p_long = value_pointer;
      *p_long = GTK_VALUE_LONG (*arg);
      break;
    case GTK_TYPE_ULONG:
      p_ulong = value_pointer;
      *p_ulong = GTK_VALUE_ULONG (*arg);
      break;
    case GTK_TYPE_FLOAT:
      p_float = value_pointer;
      *p_float = GTK_VALUE_FLOAT (*arg);
      break;
    case GTK_TYPE_DOUBLE:
      p_double = value_pointer;
      *p_double = GTK_VALUE_DOUBLE (*arg);
      break;
    case GTK_TYPE_STRING:
    case GTK_TYPE_POINTER:
    case GTK_TYPE_BOXED:
    case GTK_TYPE_OBJECT:
      p_pointer = value_pointer;
      *p_pointer = GTK_VALUE_POINTER (*arg);
      break;
    case GTK_TYPE_SIGNAL:
    case GTK_TYPE_ARGS:
    case GTK_TYPE_FOREIGN:
    case GTK_TYPE_CALLBACK:
    case GTK_TYPE_C_CALLBACK:
    case GTK_TYPE_NONE:
    case GTK_TYPE_INVALID:
      /* it doesn't make much sense to retrive these values,
       * they either are always read-only args, or require
       * multiple pointers.
       */
      g_warning ("gtk_arg_fill_retloc(): unsupported argument type `%s'",
		 gtk_type_name (arg->type));
      break;
    }
}
