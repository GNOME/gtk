/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#undef GTK_DISABLE_DEPRECATED

#include	<config.h>
#include	"gtksignal.h"
#include "gtkalias.h"

/* the real parameter limit is of course given by GSignal, bu we need
 * an upper limit for the implementations. so this should be adjusted
 * with any future changes on the GSignal side of things.
 */
#define	SIGNAL_MAX_PARAMS	12


/* --- functions --- */
guint
gtk_signal_newv (const gchar         *name,
		 GtkSignalRunType     signal_flags,
		 GType                object_type,
		 guint                function_offset,
		 GSignalCMarshaller   marshaller,
		 GType                return_val,
		 guint                n_params,
		 GType               *params)
{
  GClosure *closure;
  
  g_return_val_if_fail (n_params < SIGNAL_MAX_PARAMS, 0);
  
  closure = function_offset ? g_signal_type_cclosure_new (object_type, function_offset) : NULL;
  
  return g_signal_newv (name, object_type, (GSignalFlags)signal_flags, closure,
			NULL, NULL, marshaller, return_val, n_params, params);
}

guint
gtk_signal_new (const gchar         *name,
		GtkSignalRunType     signal_flags,
		GType                object_type,
		guint                function_offset,
		GSignalCMarshaller   marshaller,
		GType                return_val,
		guint                n_params,
		...)
{
  GType *params;
  guint signal_id;
  
  if (n_params)
    {
      va_list args;
      guint i;
      
      params = g_new (GType, n_params);
      va_start (args, n_params);
      for (i = 0; i < n_params; i++)
	params[i] = va_arg (args, GType);
      va_end (args);
    }
  else
    params = NULL;
  signal_id = gtk_signal_newv (name,
			       signal_flags,
			       object_type,
			       function_offset,
			       marshaller,
			       return_val,
			       n_params,
			       params);
  g_free (params);
  
  return signal_id;
}

void
gtk_signal_emit_stop_by_name (GtkObject   *object,
			      const gchar *name)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  g_signal_stop_emission (object, g_signal_lookup (name, G_OBJECT_TYPE (object)), 0);
}

void
gtk_signal_connect_object_while_alive (GtkObject    *object,
				       const gchar  *name,
				       GCallback     func,
				       GtkObject    *alive_object)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  g_signal_connect_closure_by_id (object,
				  g_signal_lookup (name, G_OBJECT_TYPE (object)), 0,
				  g_cclosure_new_object_swap (func, G_OBJECT (alive_object)),
				  FALSE);
}

void
gtk_signal_connect_while_alive (GtkObject    *object,
				const gchar  *name,
				GCallback     func,
				gpointer      func_data,
				GtkObject    *alive_object)
{
  GClosure *closure;

  g_return_if_fail (GTK_IS_OBJECT (object));

  closure = g_cclosure_new (func, func_data, NULL);
  g_object_watch_closure (G_OBJECT (alive_object), closure);
  g_signal_connect_closure_by_id (object,
				  g_signal_lookup (name, G_OBJECT_TYPE (object)), 0,
				  closure,
				  FALSE);
}

gulong
gtk_signal_connect_full (GtkObject           *object,
			 const gchar         *name,
			 GCallback            func,
			 GtkCallbackMarshal   unsupported,
			 gpointer             data,
			 GDestroyNotify       destroy_func,
			 gint                 object_signal,
			 gint                 after)
{
  g_return_val_if_fail (GTK_IS_OBJECT (object), 0);
  g_return_val_if_fail (unsupported == NULL, 0);
  
  return g_signal_connect_closure_by_id (object,
					 g_signal_lookup (name, G_OBJECT_TYPE (object)), 0,
					 (object_signal
					  ? g_cclosure_new_swap
					  : g_cclosure_new) (func,
							     data,
							     (GClosureNotify) destroy_func),
					 after);
}

void
gtk_signal_compat_matched (GtkObject       *object,
			   GCallback        func,
			   gpointer         data,
			   GSignalMatchType match,
			   guint            action)
{
  guint n_handlers;
  
  g_return_if_fail (GTK_IS_OBJECT (object));

  switch (action)
    {
    case 0:  n_handlers = g_signal_handlers_disconnect_matched (object, match, 0, 0, NULL, (gpointer) func, data);	 break;
    case 1:  n_handlers = g_signal_handlers_block_matched (object, match, 0, 0, NULL, (gpointer) func, data);	 break;
    case 2:  n_handlers = g_signal_handlers_unblock_matched (object, match, 0, 0, NULL, (gpointer) func, data);	 break;
    default: n_handlers = 0;										 break;
    }
  
  if (!n_handlers)
    g_warning ("unable to find signal handler for object(%s:%p) with func(%p) and data(%p)",
	       G_OBJECT_TYPE_NAME (object), object, func, data);
}

static inline gboolean
gtk_arg_to_value (GtkArg *arg,
		  GValue *value)
{
  switch (G_TYPE_FUNDAMENTAL (arg->type))
    {
    case G_TYPE_CHAR:		g_value_set_char (value, GTK_VALUE_CHAR (*arg));	break;
    case G_TYPE_UCHAR:		g_value_set_uchar (value, GTK_VALUE_UCHAR (*arg));	break;
    case G_TYPE_BOOLEAN:	g_value_set_boolean (value, GTK_VALUE_BOOL (*arg));	break;
    case G_TYPE_INT:		g_value_set_int (value, GTK_VALUE_INT (*arg));		break;
    case G_TYPE_UINT:		g_value_set_uint (value, GTK_VALUE_UINT (*arg));	break;
    case G_TYPE_LONG:		g_value_set_long (value, GTK_VALUE_LONG (*arg));	break;
    case G_TYPE_ULONG:		g_value_set_ulong (value, GTK_VALUE_ULONG (*arg));	break;
    case G_TYPE_ENUM:		g_value_set_enum (value, GTK_VALUE_ENUM (*arg));	break;
    case G_TYPE_FLAGS:		g_value_set_flags (value, GTK_VALUE_FLAGS (*arg));	break;
    case G_TYPE_FLOAT:		g_value_set_float (value, GTK_VALUE_FLOAT (*arg));	break;
    case G_TYPE_DOUBLE:		g_value_set_double (value, GTK_VALUE_DOUBLE (*arg));	break;
    case G_TYPE_STRING:		g_value_set_string (value, GTK_VALUE_STRING (*arg));	break;
    case G_TYPE_BOXED:		g_value_set_boxed (value, GTK_VALUE_BOXED (*arg));	break;
    case G_TYPE_POINTER:	g_value_set_pointer (value, GTK_VALUE_POINTER (*arg));	break;
    case G_TYPE_OBJECT:		g_value_set_object (value, GTK_VALUE_POINTER (*arg));	break;
    default:
      return FALSE;
    }
  return TRUE;
}

static inline gboolean
gtk_arg_static_to_value (GtkArg *arg,
			 GValue *value)
{
  switch (G_TYPE_FUNDAMENTAL (arg->type))
    {
    case G_TYPE_CHAR:		g_value_set_char (value, GTK_VALUE_CHAR (*arg));		break;
    case G_TYPE_UCHAR:		g_value_set_uchar (value, GTK_VALUE_UCHAR (*arg));		break;
    case G_TYPE_BOOLEAN:	g_value_set_boolean (value, GTK_VALUE_BOOL (*arg));		break;
    case G_TYPE_INT:		g_value_set_int (value, GTK_VALUE_INT (*arg));			break;
    case G_TYPE_UINT:		g_value_set_uint (value, GTK_VALUE_UINT (*arg));		break;
    case G_TYPE_LONG:		g_value_set_long (value, GTK_VALUE_LONG (*arg));		break;
    case G_TYPE_ULONG:		g_value_set_ulong (value, GTK_VALUE_ULONG (*arg));		break;
    case G_TYPE_ENUM:		g_value_set_enum (value, GTK_VALUE_ENUM (*arg));		break;
    case G_TYPE_FLAGS:		g_value_set_flags (value, GTK_VALUE_FLAGS (*arg));		break;
    case G_TYPE_FLOAT:		g_value_set_float (value, GTK_VALUE_FLOAT (*arg));		break;
    case G_TYPE_DOUBLE:		g_value_set_double (value, GTK_VALUE_DOUBLE (*arg));		break;
    case G_TYPE_STRING:		g_value_set_static_string (value, GTK_VALUE_STRING (*arg));	break;
    case G_TYPE_BOXED:		g_value_set_static_boxed (value, GTK_VALUE_BOXED (*arg));	break;
    case G_TYPE_POINTER:	g_value_set_pointer (value, GTK_VALUE_POINTER (*arg));		break;
    case G_TYPE_OBJECT:		g_value_set_object (value, GTK_VALUE_POINTER (*arg));		break;
    default:
      return FALSE;
    }
  return TRUE;
}

static inline gboolean
gtk_arg_set_from_value (GtkArg  *arg,
			GValue  *value,
			gboolean copy_string)
{
  switch (G_TYPE_FUNDAMENTAL (arg->type))
    {
    case G_TYPE_CHAR:		GTK_VALUE_CHAR (*arg) = g_value_get_char (value);	break;
    case G_TYPE_UCHAR:		GTK_VALUE_UCHAR (*arg) = g_value_get_uchar (value);	break;
    case G_TYPE_BOOLEAN:	GTK_VALUE_BOOL (*arg) = g_value_get_boolean (value);	break;
    case G_TYPE_INT:		GTK_VALUE_INT (*arg) = g_value_get_int (value);		break;
    case G_TYPE_UINT:		GTK_VALUE_UINT (*arg) = g_value_get_uint (value);	break;
    case G_TYPE_LONG:		GTK_VALUE_LONG (*arg) = g_value_get_long (value);	break;
    case G_TYPE_ULONG:		GTK_VALUE_ULONG (*arg) = g_value_get_ulong (value);	break;
    case G_TYPE_ENUM:		GTK_VALUE_ENUM (*arg) = g_value_get_enum (value);	break;
    case G_TYPE_FLAGS:		GTK_VALUE_FLAGS (*arg) = g_value_get_flags (value);	break;
    case G_TYPE_FLOAT:		GTK_VALUE_FLOAT (*arg) = g_value_get_float (value);	break;
    case G_TYPE_DOUBLE:		GTK_VALUE_DOUBLE (*arg) = g_value_get_double (value);	break;
    case G_TYPE_BOXED:		GTK_VALUE_BOXED (*arg) = g_value_get_boxed (value);	break;
    case G_TYPE_POINTER:	GTK_VALUE_POINTER (*arg) = g_value_get_pointer (value);	break;
    case G_TYPE_OBJECT:		GTK_VALUE_POINTER (*arg) = g_value_get_object (value);	break;
    case G_TYPE_STRING:		if (copy_string)
      GTK_VALUE_STRING (*arg) = g_value_dup_string (value);
    else
      GTK_VALUE_STRING (*arg) = (char *) g_value_get_string (value);
    break;
    default:
      return FALSE;
    }
  return TRUE;
}

static inline gboolean
gtk_argloc_set_from_value (GtkArg  *arg,
			   GValue  *value,
			   gboolean copy_string)
{
  switch (G_TYPE_FUNDAMENTAL (arg->type))
    {
    case G_TYPE_CHAR:		*GTK_RETLOC_CHAR (*arg) = g_value_get_char (value);	  break;
    case G_TYPE_UCHAR:		*GTK_RETLOC_UCHAR (*arg) = g_value_get_uchar (value);	  break;
    case G_TYPE_BOOLEAN:	*GTK_RETLOC_BOOL (*arg) = g_value_get_boolean (value);	  break;
    case G_TYPE_INT:		*GTK_RETLOC_INT (*arg) = g_value_get_int (value);	  break;
    case G_TYPE_UINT:		*GTK_RETLOC_UINT (*arg) = g_value_get_uint (value);	  break;
    case G_TYPE_LONG:		*GTK_RETLOC_LONG (*arg) = g_value_get_long (value);	  break;
    case G_TYPE_ULONG:		*GTK_RETLOC_ULONG (*arg) = g_value_get_ulong (value);	  break;
    case G_TYPE_ENUM:		*GTK_RETLOC_ENUM (*arg) = g_value_get_enum (value);	  break;
    case G_TYPE_FLAGS:		*GTK_RETLOC_FLAGS (*arg) = g_value_get_flags (value);	  break;
    case G_TYPE_FLOAT:		*GTK_RETLOC_FLOAT (*arg) = g_value_get_float (value);	  break;
    case G_TYPE_DOUBLE:		*GTK_RETLOC_DOUBLE (*arg) = g_value_get_double (value);	  break;
    case G_TYPE_BOXED:		*GTK_RETLOC_BOXED (*arg) = g_value_get_boxed (value);	  break;
    case G_TYPE_POINTER:	*GTK_RETLOC_POINTER (*arg) = g_value_get_pointer (value); break;
    case G_TYPE_OBJECT:		*GTK_RETLOC_POINTER (*arg) = g_value_get_object (value);  break;
    case G_TYPE_STRING:		if (copy_string)
      *GTK_RETLOC_STRING (*arg) = g_value_dup_string (value);
    else
      *GTK_RETLOC_STRING (*arg) = (char *) g_value_get_string (value);
    break;
    default:
      return FALSE;
    }
  return TRUE;
}

void
gtk_signal_emitv (GtkObject *object,
		  guint      signal_id,
		  GtkArg    *args)
{
  GSignalQuery query;
  GValue params[SIGNAL_MAX_PARAMS + 1] = { { 0, }, };
  GValue rvalue = { 0, };
  guint i;
  
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  g_signal_query (signal_id, &query);
  g_return_if_fail (query.signal_id != 0);
  g_return_if_fail (g_type_is_a (GTK_OBJECT_TYPE (object), query.itype));
  g_return_if_fail (query.n_params < SIGNAL_MAX_PARAMS);
  if (query.n_params > 0)
    g_return_if_fail (args != NULL);
  
  g_value_init (params + 0, GTK_OBJECT_TYPE (object));
  g_value_set_object (params + 0, G_OBJECT (object));
  for (i = 0; i < query.n_params; i++)
    {
      GValue *value = params + 1 + i;
      GtkArg *arg = args + i;
      
      g_value_init (value, arg->type & ~G_SIGNAL_TYPE_STATIC_SCOPE);
      if (!gtk_arg_static_to_value (arg, value))
	{
	  g_warning ("%s: failed to convert arg type `%s' to value type `%s'",
		     G_STRLOC, g_type_name (arg->type & ~G_SIGNAL_TYPE_STATIC_SCOPE),
		     g_type_name (G_VALUE_TYPE (value)));
	  return;
	}
    }
  if (query.return_type != G_TYPE_NONE)
    g_value_init (&rvalue, query.return_type);
  
  g_signal_emitv (params, signal_id, 0, &rvalue);
  
  if (query.return_type != G_TYPE_NONE)
    {
      gtk_argloc_set_from_value (args + query.n_params, &rvalue, TRUE);
      g_value_unset (&rvalue);
    }
  for (i = 0; i < query.n_params; i++)
    g_value_unset (params + 1 + i);
  g_value_unset (params + 0);
}

void
gtk_signal_emit (GtkObject *object,
		 guint      signal_id,
		 ...)
{
  va_list var_args;
  
  g_return_if_fail (GTK_IS_OBJECT (object));

  va_start (var_args, signal_id);
  g_signal_emit_valist (G_OBJECT (object), signal_id, 0, var_args);
  va_end (var_args);
}

void
gtk_signal_emit_by_name (GtkObject   *object,
			 const gchar *name,
			 ...)
{
  GSignalQuery query;
  va_list var_args;
  
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (name != NULL);
  
  g_signal_query (g_signal_lookup (name, GTK_OBJECT_TYPE (object)), &query);
  g_return_if_fail (query.signal_id != 0);
  
  va_start (var_args, name);
  g_signal_emit_valist (G_OBJECT (object), query.signal_id, 0, var_args);
  va_end (var_args);
}

void
gtk_signal_emitv_by_name (GtkObject   *object,
			  const gchar *name,
			  GtkArg      *args)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  gtk_signal_emitv (object, g_signal_lookup (name, GTK_OBJECT_TYPE (object)), args);
}

#define __GTK_SIGNAL_C__
#include "gtkaliasdef.c"
