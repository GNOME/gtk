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

/* collect a single argument value from a va_list
 */
static inline gchar*
gtk_arg_collect_value (GtkType  fundamental_type,
		       GtkArg  *arg,
		       va_list  *var_args)
{
  gchar *error_msg;

  error_msg = NULL;
  switch (fundamental_type)
    {
    case GTK_TYPE_INVALID:
      error_msg = g_strdup ("invalid untyped argument");
      break;
    case GTK_TYPE_NONE:
      /* error_msg = g_strdup ("invalid argument type `void'"); */
      break;
    case GTK_TYPE_CHAR:
      GTK_VALUE_CHAR (*arg) = va_arg (*var_args, gchar);
      break;
    case GTK_TYPE_BOOL:
      GTK_VALUE_BOOL (*arg) = va_arg (*var_args, gboolean);
      break;
    case GTK_TYPE_INT:
      GTK_VALUE_INT (*arg) = va_arg (*var_args, gint);
      break;
    case GTK_TYPE_UINT:
      GTK_VALUE_UINT (*arg) = va_arg (*var_args, guint);
      break;
    case GTK_TYPE_ENUM:
      GTK_VALUE_ENUM (*arg) = va_arg (*var_args, gint);
      break;
    case GTK_TYPE_FLAGS:
      GTK_VALUE_FLAGS (*arg) = va_arg (*var_args, gint);
      break;
    case GTK_TYPE_LONG:
      GTK_VALUE_LONG (*arg) = va_arg (*var_args, glong);
      break;
    case GTK_TYPE_ULONG:
      GTK_VALUE_ULONG (*arg) = va_arg (*var_args, gulong);
      break;
    case GTK_TYPE_FLOAT:
      /* GTK_VALUE_FLOAT (*arg) = va_arg (*var_args, gfloat); */
      GTK_VALUE_FLOAT (*arg) = va_arg (*var_args, gdouble);
      break;
    case GTK_TYPE_DOUBLE:
      GTK_VALUE_DOUBLE (*arg) = va_arg (*var_args, gdouble);
      break;
    case GTK_TYPE_STRING:
      GTK_VALUE_STRING (*arg) = va_arg (*var_args, gchar*);
      break;
    case GTK_TYPE_POINTER:
      GTK_VALUE_POINTER (*arg) = va_arg (*var_args, gpointer);
      break;
    case GTK_TYPE_BOXED:
      GTK_VALUE_BOXED (*arg) = va_arg (*var_args, gpointer);
      break;
    case GTK_TYPE_SIGNAL:
      GTK_VALUE_SIGNAL (*arg).f = va_arg (*var_args, GtkFunction);
      GTK_VALUE_SIGNAL (*arg).d = va_arg (*var_args, gpointer);
      break;
    case GTK_TYPE_FOREIGN:
      GTK_VALUE_FOREIGN (*arg).data = va_arg (*var_args, gpointer);
      GTK_VALUE_FOREIGN (*arg).notify = va_arg (*var_args, GtkDestroyNotify);
      break;
    case GTK_TYPE_CALLBACK:
      GTK_VALUE_CALLBACK (*arg).marshal = va_arg (*var_args, GtkCallbackMarshal);
      GTK_VALUE_CALLBACK (*arg).data = va_arg (*var_args, gpointer);
      GTK_VALUE_CALLBACK (*arg).notify = va_arg (*var_args, GtkDestroyNotify);
      break;
    case GTK_TYPE_C_CALLBACK:
      GTK_VALUE_C_CALLBACK (*arg).func = va_arg (*var_args, GtkFunction);
      GTK_VALUE_C_CALLBACK (*arg).func_data = va_arg (*var_args, gpointer);
      break;
    case GTK_TYPE_ARGS:
      GTK_VALUE_ARGS (*arg).n_args = va_arg (*var_args, gint);
      GTK_VALUE_ARGS (*arg).args = va_arg (*var_args, GtkArg*);
      break;
    case GTK_TYPE_OBJECT:
      GTK_VALUE_OBJECT (*arg) = va_arg (*var_args, GtkObject*);
      if (GTK_VALUE_OBJECT (*arg) != NULL)
	{
	  register GtkObject *object = GTK_VALUE_OBJECT (*arg);

	  if (object->klass == NULL ||
	      !gtk_type_is_a (GTK_OBJECT_TYPE (object), arg->type))
	    error_msg = g_strconcat ("invalid object `",
				     gtk_type_name (GTK_OBJECT_TYPE (object)),
				     "' for argument type `",
				     gtk_type_name (arg->type),
				     "'",
				     NULL);
	}
      break;
    default:
      error_msg = g_strconcat ("unsupported argument type `",
			       gtk_type_name (arg->type),
			       "'",
			       NULL);
      break;
    }

  return error_msg;
}
