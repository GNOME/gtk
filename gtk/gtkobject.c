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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdarg.h>
#include <string.h>
#include "gtkobject.h"
#include "gtksignal.h"


#define OBJECT_DATA_ID_CHUNK  1024


enum {
  DESTROY,
  LAST_SIGNAL
};


typedef struct _GtkObjectData  GtkObjectData;
typedef struct _GtkArgInfo     GtkArgInfo;

struct _GtkObjectData
{
  guint id;
  gpointer data;
  GtkObjectData *next;
};

struct _GtkArgInfo
{
  char *name;
  GtkType type;
};


static void           gtk_object_class_init    (GtkObjectClass *klass);
static void           gtk_object_init          (GtkObject      *object);
static void           gtk_object_arg           (GtkObject      *object,
						GtkArg         *arg);
static void           gtk_real_object_destroy  (GtkObject      *object);
static void           gtk_object_data_init     (void);
static GtkObjectData* gtk_object_data_new      (void);
static void           gtk_object_data_destroy  (GtkObjectData  *odata);
static guint*         gtk_object_data_id_alloc (void);
GtkArg*               gtk_object_collect_args  (gint    *nargs,
						va_list  args1,
						va_list  args2);


static gint object_signals[LAST_SIGNAL] = { 0 };

static gint object_data_init = TRUE;
static GHashTable *object_data_ht = NULL;
static GMemChunk *object_data_mem_chunk = NULL;
static GtkObjectData *object_data_free_list = NULL;
static GSList *object_data_id_list = NULL;
static gint object_data_id_index = 0;

static GHashTable *arg_info_ht = NULL;

static const char *user_data_key = "user_data";


/*****************************************
 * gtk_object_get_type:
 *
 *   arguments:
 *
 *   results:
 *     The type identifier for GtkObject's
 *****************************************/

void
gtk_object_init_type ()
{
  GtkType object_type = 0;
  GtkTypeInfo object_info =
  {
    "GtkObject",
    sizeof (GtkObject),
    sizeof (GtkObjectClass),
    (GtkClassInitFunc) gtk_object_class_init,
    (GtkObjectInitFunc) gtk_object_init,
    (GtkArgFunc) gtk_object_arg,
  };

  object_type = gtk_type_unique (0, &object_info);
  g_assert (object_type == GTK_TYPE_OBJECT);
}

GtkType
gtk_object_get_type ()
{
  return GTK_TYPE_OBJECT;
}

/*****************************************
 * gtk_object_class_init:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_object_class_init (GtkObjectClass *class)
{
  class->signals = NULL;
  class->nsignals = 0;

  gtk_object_add_arg_type ("GtkObject::user_data", GTK_TYPE_POINTER);
  gtk_object_add_arg_type ("GtkObject::signal", GTK_TYPE_SIGNAL);

  object_signals[DESTROY] =
    gtk_signal_new ("destroy",
                    GTK_RUN_LAST,
                    class->type,
                    GTK_SIGNAL_OFFSET (GtkObjectClass, destroy),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (class, object_signals, LAST_SIGNAL);

  class->destroy = gtk_real_object_destroy;
}

/*****************************************
 * gtk_object_init:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_object_init (GtkObject *object)
{
  object->flags = 0;
  object->ref_count = 0;
  object->object_data = NULL;
}

/*****************************************
 * gtk_object_arg:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_object_arg (GtkObject *object,
		GtkArg    *arg)
{
  if (strcmp (arg->name, "user_data") == 0)
    {
      gtk_object_set_user_data (object, GTK_VALUE_POINTER (*arg));
    }
  else if (strncmp (arg->name, "signal", 6) == 0)
    {
      if ((arg->name[6] != ':') || (arg->name[7] != ':'))
	{
	  g_print ("invalid signal argument: \"%s\"\n", arg->name);
	  return;
	}

      gtk_signal_connect (object, arg->name + 8,
			  (GtkSignalFunc) GTK_VALUE_SIGNAL (*arg).f,
			  GTK_VALUE_SIGNAL (*arg).d);
    }
}

/*****************************************
 * gtk_object_class_add_signals:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_object_class_add_signals (GtkObjectClass *class,
			      gint           *signals,
			      gint            nsignals)
{
  gint *new_signals;
  gint i;

  g_return_if_fail (class != NULL);

  new_signals = g_new (gint, class->nsignals + nsignals);
  for (i = 0; i < class->nsignals; i++)
    new_signals[i] = class->signals[i];
  for (i = 0; i < nsignals; i++)
    new_signals[class->nsignals + i] = signals[i];

  class->signals = new_signals;
  class->nsignals += nsignals;
}

/*****************************************
 * gtk_object_ref:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_object_ref (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  object->ref_count += 1;
}

/*****************************************
 * gtk_object_unref:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_object_unref (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  if (object->ref_count > 0)
    object->ref_count -= 1;
}

/*****************************************
 * gtk_object_new:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkObject*
gtk_object_new (guint type,
		...)
{
  GtkObject *obj;
  GtkArg *args;
  gint nargs;
  va_list args1;
  va_list args2;

  obj = gtk_type_new (type);

  va_start (args1, type);
  va_start (args2, type);

  args = gtk_object_collect_args (&nargs, args1, args2);
  gtk_object_setv (obj, nargs, args);
  g_free (args);

  va_end (args1);
  va_end (args2);

  return obj;
}

/*****************************************
 * gtk_object_newv:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkObject*
gtk_object_newv (guint   type,
		 gint    nargs,
		 GtkArg *args)
{
  gpointer obj;

  obj = gtk_type_new (type);
  gtk_object_setv (obj, nargs, args);

  return obj;
}

/*****************************************
 * gtk_object_set:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_object_set (GtkObject *obj,
		...)
{
  GtkArg *args;
  gint nargs;
  va_list args1;
  va_list args2;

  g_return_if_fail (obj != NULL);

  va_start (args1, obj);
  va_start (args2, obj);

  args = gtk_object_collect_args (&nargs, args1, args2);
  gtk_object_setv (obj, nargs, args);
  g_free (args);

  va_end (args1);
  va_end (args2);
}

/*****************************************
 * gtk_object_setv:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_object_setv (GtkObject *obj,
		 gint       nargs,
		 GtkArg    *args)
{
  guint class_type;
  char class_name[1024];
  char *arg_name;
  int i;

  g_return_if_fail (obj != NULL);

  for (i = 0; i < nargs; i++)
    {
      arg_name = strchr (args[i].name, ':');
      if (!arg_name || (arg_name[0] != ':') || (arg_name[1] != ':'))
	{
	  g_print ("invalid arg name: \"%s\"\n", args[i].name);
	  continue;
	}

      strncpy (class_name, args[i].name, (long) (arg_name - args[i].name));
      class_name[(long) (arg_name - args[i].name)] = '\0';

      args[i].name = arg_name + 2;

      class_type = gtk_type_from_name (class_name);
      gtk_type_set_arg (obj, class_type, &args[i]);
    }
}

/*****************************************
 * gtk_object_add_arg_type:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_object_add_arg_type (const char *arg_name,
			 GtkType     arg_type)
{
  GtkArgInfo *info;

  info = g_new (GtkArgInfo, 1);
  info->name = g_strdup(arg_name);
  info->type = arg_type;

  if (!arg_info_ht)
    arg_info_ht = g_hash_table_new (g_string_hash, g_string_equal);

  g_hash_table_insert (arg_info_ht, info->name, info);
}

/*****************************************
 * gtk_object_get_arg_type:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkType
gtk_object_get_arg_type (const char *arg_name)
{
  GtkArgInfo *info;
  char buffer[1024];
  char *t;

  if (!arg_info_ht)
    return GTK_TYPE_INVALID;

  t = strchr (arg_name, ':');
  if (!t || (t[0] != ':') || (t[1] != ':'))
    {
      g_print ("invalid arg name: \"%s\"\n", arg_name);
      return GTK_TYPE_INVALID;
    }

  t = strchr (t + 2, ':');
  if (t)
    {
      strncpy (buffer, arg_name, (long) (t - arg_name));
      buffer[(long) (t - arg_name)] = '\0';
      arg_name = buffer;
    }

  info = g_hash_table_lookup (arg_info_ht, (gpointer) arg_name);
  if (info)
    return info->type;

  return GTK_TYPE_INVALID;
}

/*****************************************
 * gtk_object_destroy:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_object_destroy (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  if ((object->ref_count > 0) || GTK_OBJECT_IN_CALL (object))
    {
      GTK_OBJECT_SET_FLAGS (object, GTK_NEED_DESTROY);
    }
  else
    {
      GTK_OBJECT_UNSET_FLAGS (object, GTK_NEED_DESTROY);
      GTK_OBJECT_SET_FLAGS (object, GTK_BEING_DESTROYED);

      gtk_signal_emit (object, object_signals[DESTROY]);
    }
}

/*****************************************
 * gtk_object_set_data:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_object_set_data (GtkObject   *object,
		     const gchar *key,
		     gpointer     data)
{
  GtkObjectData *odata;
  GtkObjectData *prev;
  guint *id;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);

  if (object_data_init)
    gtk_object_data_init ();

  id = g_hash_table_lookup (object_data_ht, (gpointer) key);

  if (!data)
    {
      if (id)
	{
	  prev = NULL;
	  odata = object->object_data;

	  while (odata)
	    {
	      if (odata->id == *id)
		{
		  if (prev)
		    prev->next = odata->next;
		  if (odata == object->object_data)
		    object->object_data = odata->next;

		  gtk_object_data_destroy (odata);
		  break;
		}

	      prev = odata;
	      odata = odata->next;
	    }
	}
    }
  else
    {
      if (!id)
	{
	  id = gtk_object_data_id_alloc ();
	  g_hash_table_insert (object_data_ht, (gpointer) key, id);
	}

      odata = object->object_data;
      while (odata)
	{
	  if (odata->id == *id)
	    {
	      odata->data = data;
	      return;
	    }

	  odata = odata->next;
	}

      odata = gtk_object_data_new ();
      odata->id = *id;
      odata->data = data;

      odata->next = object->object_data;
      object->object_data = odata;
    }
}

/*****************************************
 * gtk_object_get_data:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

gpointer
gtk_object_get_data (GtkObject   *object,
		     const gchar *key)
{
  GtkObjectData *odata;
  guint *id;

  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  if (object_data_init)
    gtk_object_data_init ();

  id = g_hash_table_lookup (object_data_ht, (gpointer) key);
  if (id)
    {
      odata = object->object_data;
      while (odata)
	{
	  if (odata->id == *id)
	    return odata->data;
	  odata = odata->next;
	}
    }

  return NULL;
}

/*****************************************
 * gtk_object_remove_data:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_object_remove_data (GtkObject   *object,
			const gchar *key)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);

  gtk_object_set_data (object, key, NULL);
}

/*****************************************
 * gtk_object_set_user_data:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_object_set_user_data (GtkObject *object,
			  gpointer   data)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  gtk_object_set_data (object, user_data_key, data);
}

/*****************************************
 * gtk_object_get_user_data:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

gpointer
gtk_object_get_user_data (GtkObject *object)
{
  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);

  return gtk_object_get_data (object, user_data_key);
}

/*****************************************
 * gtk_object_check_cast:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkObject*
gtk_object_check_cast (GtkObject *obj,
		       GtkType    cast_type)
{
  if (obj && obj->klass && !gtk_type_is_a (obj->klass->type, cast_type))
    {
      gchar *from_name = gtk_type_name (obj->klass->type);
      gchar *to_name = gtk_type_name (cast_type);

      g_warning ("invalid cast from \"%s\" to \"%s\"",
		 from_name ? from_name : "(unknown)",
		 to_name ? to_name : "(unknown)");
    }

  return obj;
}

/*****************************************
 * gtk_object_check_class_cast:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkObjectClass*
gtk_object_check_class_cast (GtkObjectClass *klass,
			     GtkType         cast_type)
{
  if (klass && !gtk_type_is_a (klass->type, cast_type))
    g_warning ("invalid cast from \"%sClass\" to \"%sClass\"",
	       gtk_type_name (klass->type),
	       gtk_type_name (cast_type));

  return klass;
}

/*****************************************
 * gtk_real_object_destroy:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_real_object_destroy (GtkObject *object)
{
  GtkObjectData *odata;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  gtk_signal_handlers_destroy (object);

  if (object->object_data)
    {
      odata = object->object_data;
      while (odata->next)
	odata = odata->next;

      odata->next = object_data_free_list;
      object_data_free_list = object->object_data;
    }

  g_free (object);
}

/*****************************************
 * gtk_object_data_init:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_object_data_init ()
{
  if (object_data_init)
    {
      object_data_init = FALSE;

      object_data_ht = g_hash_table_new (g_string_hash, g_string_equal);
    }
}

/*****************************************
 * gtk_object_data_new:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static GtkObjectData*
gtk_object_data_new ()
{
  GtkObjectData *odata;

  if (!object_data_mem_chunk)
    object_data_mem_chunk = g_mem_chunk_new ("object data mem chunk",
					     sizeof (GtkObjectData),
					     1024, G_ALLOC_AND_FREE);

  odata = g_chunk_new (GtkObjectData, object_data_mem_chunk);

  odata->id = 0;
  odata->data = NULL;
  odata->next = NULL;

  return odata;
}

/*****************************************
 * gtk_object_data_destroy:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_object_data_destroy (GtkObjectData *odata)
{
  g_return_if_fail (odata != NULL);

  g_mem_chunk_free (object_data_mem_chunk, odata);
}

/*****************************************
 * gtk_object_data_id_alloc:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static guint*
gtk_object_data_id_alloc ()
{
  static guint next_id = 1;
  guint *ids;

  if (!object_data_id_list ||
      (object_data_id_index == OBJECT_DATA_ID_CHUNK))
    {
      ids = g_new (guint, OBJECT_DATA_ID_CHUNK);
      object_data_id_index = 0;
      object_data_id_list = g_slist_prepend (object_data_id_list, ids);
    }
  else
    {
      ids = object_data_id_list->data;
    }

  ids[object_data_id_index] = next_id++;
  return &ids[object_data_id_index++];
}

/*****************************************
 * gtk_object_data_id_alloc:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkArg*
gtk_object_collect_args (gint    *nargs,
			 va_list  args1,
			 va_list  args2)
{
  GtkArg *args;
  GtkType type;
  char *name;
  int done;
  int i, n;

  n = 0;
  done = FALSE;

  while (!done)
    {
      name = va_arg (args1, char *);
      if (!name)
	{
	  done = TRUE;
	  continue;
	}

      type = gtk_object_get_arg_type (name);

      switch (GTK_FUNDAMENTAL_TYPE (type))
	{
	case GTK_TYPE_INVALID:
	  g_print ("invalid arg name: \"%s\" %x\n", name, type);
	  (void) va_arg (args1, long);
	  continue;
	case GTK_TYPE_NONE:
	  break;
	case GTK_TYPE_CHAR:
	case GTK_TYPE_BOOL:
	case GTK_TYPE_INT:
	case GTK_TYPE_UINT:
	case GTK_TYPE_ENUM:
	case GTK_TYPE_FLAGS:
	  (void) va_arg (args1, gint);
	  break;
	case GTK_TYPE_LONG:
	case GTK_TYPE_ULONG:
	  (void) va_arg (args1, glong);
	  break;
	case GTK_TYPE_FLOAT:
	  (void) va_arg (args1, gfloat);
	  break;
	case GTK_TYPE_STRING:
	  (void) va_arg (args1, gchar*);
	  break;
	case GTK_TYPE_POINTER:
	case GTK_TYPE_BOXED:
	  (void) va_arg (args1, gpointer);
	  break;
	case GTK_TYPE_SIGNAL:
	  (void) va_arg (args1, GtkFunction);
	  (void) va_arg (args1, gpointer);
	  break;
	case GTK_TYPE_FOREIGN:
	  (void) va_arg (args1, gpointer);
	  (void) va_arg (args1, GtkDestroyNotify);
	  break;
	case GTK_TYPE_CALLBACK:
	  (void) va_arg (args1, GtkCallbackMarshal);
	  (void) va_arg (args1, gpointer);
	  (void) va_arg (args1, GtkDestroyNotify);
	  break;
	case GTK_TYPE_C_CALLBACK:
	  (void) va_arg (args1, GtkFunction);
	  (void) va_arg (args1, gpointer);
	  break;
	case GTK_TYPE_ARGS:
	  (void) va_arg (args1, gint);
	  (void) va_arg (args1, GtkArg*);
	  break;
	case GTK_TYPE_OBJECT:
	  (void) va_arg (args1, GtkObject*);
	  break;
	default:
	  g_error ("unsupported type %s in args", gtk_type_name (type));
	  break;
	}

      n += 1;
    }

  *nargs = n;
  args = NULL;

  if (n > 0)
    {
      args = g_new0 (GtkArg, n);

      for (i = 0; i < n; i++)
	{
	  args[i].name = va_arg (args2, char *);
	  args[i].type = gtk_object_get_arg_type (args[i].name);

	  switch (GTK_FUNDAMENTAL_TYPE (args[i].type))
	    {
	    case GTK_TYPE_INVALID:
	      (void) va_arg (args2, long);
	      i -= 1;
	      continue;
	    case GTK_TYPE_NONE:
	      break;
	    case GTK_TYPE_CHAR:
	      GTK_VALUE_CHAR(args[i]) = va_arg (args2, gint);
	      break;
	    case GTK_TYPE_BOOL:
	      GTK_VALUE_BOOL(args[i]) = va_arg (args2, gint);
	      break;
	    case GTK_TYPE_INT:
	      GTK_VALUE_INT(args[i]) = va_arg (args2, gint);
	      break;
	    case GTK_TYPE_UINT:
	      GTK_VALUE_UINT(args[i]) = va_arg (args2, guint);
	      break;
	    case GTK_TYPE_ENUM:
	      GTK_VALUE_ENUM(args[i]) = va_arg (args2, gint);
	      break;
	    case GTK_TYPE_FLAGS:
	      GTK_VALUE_FLAGS(args[i]) = va_arg (args2, gint);
	      break;
	    case GTK_TYPE_LONG:
	      GTK_VALUE_LONG(args[i]) = va_arg (args2, glong);
	      break;
	    case GTK_TYPE_ULONG:
	      GTK_VALUE_ULONG(args[i]) = va_arg (args2, gulong);
	      break;
	    case GTK_TYPE_FLOAT:
	      GTK_VALUE_FLOAT(args[i]) = va_arg (args2, gfloat);
	      break;
	    case GTK_TYPE_STRING:
	      GTK_VALUE_STRING(args[i]) = va_arg (args2, gchar*);
	      break;
	    case GTK_TYPE_POINTER:
	      GTK_VALUE_POINTER(args[i]) = va_arg (args2, gpointer);
	      break;
	    case GTK_TYPE_BOXED:
	      GTK_VALUE_BOXED(args[i]) = va_arg (args2, gpointer);
	      break;
	    case GTK_TYPE_SIGNAL:
	      GTK_VALUE_SIGNAL(args[i]).f = va_arg (args2, GtkFunction);
	      GTK_VALUE_SIGNAL(args[i]).d = va_arg (args2, gpointer);
	      break;
	    case GTK_TYPE_FOREIGN:
	      GTK_VALUE_FOREIGN(args[i]).data = va_arg (args2, gpointer);
	      GTK_VALUE_FOREIGN(args[i]).notify =
		va_arg (args2, GtkDestroyNotify);
	      break;
	    case GTK_TYPE_CALLBACK:
	      GTK_VALUE_CALLBACK(args[i]).marshal =
		va_arg (args2, GtkCallbackMarshal);
	      GTK_VALUE_CALLBACK(args[i]).data = va_arg (args2, gpointer);
	      GTK_VALUE_CALLBACK(args[i]).notify =
		va_arg (args2, GtkDestroyNotify);
	      break;
	    case GTK_TYPE_C_CALLBACK:
	      GTK_VALUE_C_CALLBACK(args[i]).func = va_arg (args2, GtkFunction);
	      GTK_VALUE_C_CALLBACK(args[i]).func_data =
		va_arg (args2, gpointer);
	      break;
	    case GTK_TYPE_ARGS:
	      GTK_VALUE_ARGS(args[i]).n_args = va_arg (args2, gint);
	      GTK_VALUE_ARGS(args[i]).args = va_arg (args2, GtkArg*);
	      break;
	    case GTK_TYPE_OBJECT:
	      GTK_VALUE_OBJECT(args[i]) = va_arg (args2, GtkObject*);
	      g_assert (GTK_VALUE_OBJECT(args[i]) == NULL ||
			GTK_CHECK_TYPE (GTK_VALUE_OBJECT(args[i]),
					args[i].type));
	      break;
	    default:
	      g_error ("unsupported type %s in args",
		       gtk_type_name (args[i].type));
	      break;
	    }
	}
    }

  return args;
}
