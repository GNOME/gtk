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
#include <stdio.h>
#include <stdlib.h>
#include "gtkobject.h"
#include "gtksignal.h"

#define	GTK_OBJECT_DEBUG 1

#define OBJECT_DATA_ID_CHUNK  1024


enum {
  DESTROY,
  LAST_SIGNAL
};
enum {
  ARG_0,
  ARG_USER_DATA,
  ARG_SIGNAL,
  ARG_OBJECT_SIGNAL
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
  GtkType class_type;
  guint arg_id;
  guint seq_id;
};


static void           gtk_object_class_init    (GtkObjectClass *klass);
static void           gtk_object_init          (GtkObject      *object);
static void           gtk_object_set_arg       (GtkObject      *object,
						GtkArg         *arg,
						guint		arg_id);
static void           gtk_object_get_arg       (GtkObject      *object,
						GtkArg         *arg,
						guint		arg_id);
static void           gtk_object_real_destroy  (GtkObject      *object);
static void           gtk_object_finalize      (GtkObject      *object);
static void           gtk_object_notify_weaks  (GtkObject      *object);
static void           gtk_object_data_init     (void);
static GtkObjectData* gtk_object_data_new      (void);
static void           gtk_object_data_destroy  (GtkObjectData  *odata);
static guint*         gtk_object_data_id_alloc (void);

GtkArg*               gtk_object_collect_args  (guint   *nargs,
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


#ifdef	GTK_OBJECT_DEBUG
static int obj_count = 0;
static GSList *living_objs = NULL;

static void
gtk_object_debug (void)
{
  GSList *node;
  
  printf ("%d living objects\n", obj_count);
  for (node = living_objs; node; node = node->next)
    {
      GtkObject *obj;

      obj = (GtkObject*) node->data;
      /*
	printf ("%p: %s %d %s\n",
	obj, gtk_type_name (GTK_OBJECT_TYPE (obj)),
	obj->ref_count,
	GTK_OBJECT_FLOATING (obj)? "floating" : "");
	*/
    }
}
#endif	GTK_OBJECT_DEBUG

/*****************************************
 * gtk_object_init_type:
 *
 *   arguments:
 *
 *   results:
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
    gtk_object_set_arg,
    gtk_object_get_arg,
  };

  object_type = gtk_type_unique (0, &object_info);
  g_assert (object_type == GTK_TYPE_OBJECT);

#ifdef	GTK_OBJECT_DEBUG
  ATEXIT (gtk_object_debug);
#endif	GTK_OBJECT_DEBUG
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
  class->n_args = 0;

  gtk_object_add_arg_type ("GtkObject::user_data", GTK_TYPE_POINTER, ARG_USER_DATA);
  gtk_object_add_arg_type ("GtkObject::signal", GTK_TYPE_SIGNAL, ARG_SIGNAL);
  gtk_object_add_arg_type ("GtkObject::object_signal", GTK_TYPE_SIGNAL, ARG_OBJECT_SIGNAL);

  object_signals[DESTROY] =
    gtk_signal_new ("destroy",
                    GTK_RUN_LAST,
                    class->type,
                    GTK_SIGNAL_OFFSET (GtkObjectClass, destroy),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (class, object_signals, LAST_SIGNAL);

  class->destroy = gtk_object_real_destroy;
  class->finalize = gtk_object_finalize;
}

/*****************************************
 * gtk_object_real_destroy:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_object_real_destroy (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  /* FIXME: gtk_signal_handlers_destroy (object); */

  /* object->klass = gtk_type_class (gtk_destroyed_get_type ()); */
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
  GTK_OBJECT_FLAGS (object) = GTK_FLOATING;

  object->ref_count = 1;
  object->object_data = NULL;

#ifdef	GTK_OBJECT_DEBUG
  obj_count++;
  living_objs = g_slist_prepend (living_objs, object);
#endif	GTK_OBJECT_DEBUG
}

/*****************************************
 * gtk_object_set_arg:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_object_set_arg (GtkObject *object,
		    GtkArg    *arg,
		    guint      arg_id)
{
  switch (arg_id)
    {
    case ARG_USER_DATA:
      gtk_object_set_user_data (object, GTK_VALUE_POINTER (*arg));
      break;
    case ARG_SIGNAL:
      if ((arg->name[9 + 2 + 6] != ':') || (arg->name[9 + 2 + 7] != ':'))
	{
	  g_warning ("invalid signal argument: \"%s\"\n", arg->name);
	  return;
	}
      gtk_signal_connect (object, arg->name + 9 + 2 + 6 + 2,
			  (GtkSignalFunc) GTK_VALUE_SIGNAL (*arg).f,
			  GTK_VALUE_SIGNAL (*arg).d);
      break;
    case ARG_OBJECT_SIGNAL:
      if ((arg->name[9 + 2 + 13] != ':') || (arg->name[9 + 2 + 14] != ':'))
	{
	  g_warning ("invalid signal argument: \"%s\"\n", arg->name);
	  return;
	}
      gtk_signal_connect_object (object, arg->name + 9 + 2 + 13 + 2,
				 (GtkSignalFunc) GTK_VALUE_SIGNAL (*arg).f,
				 (GtkObject*) GTK_VALUE_SIGNAL (*arg).d);
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

/*****************************************
 * gtk_object_get_arg:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_object_get_arg (GtkObject *object,
		    GtkArg    *arg,
		    guint      arg_id)
{
  switch (arg_id)
    {
    case ARG_USER_DATA:
      GTK_VALUE_POINTER (*arg) = gtk_object_get_user_data (object);
      break;
    case ARG_SIGNAL:
    case ARG_OBJECT_SIGNAL:
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
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

  g_free (class->signals);
  class->signals = new_signals;
  class->nsignals += nsignals;
}

/*****************************************
 * gtk_object_class_add_user_signal:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

gint
gtk_object_class_add_user_signal (GtkObjectClass     *class,
				  const gchar        *name,
				  GtkSignalMarshaller marshaller,
				  GtkType             return_val,
				  gint                nparams,
				  ...)
{
  GtkType *params;
  guint i;
  va_list args;
  gint signal_id;

  g_return_val_if_fail (class != NULL, 0);

  if (nparams > 0)
    {
      params = g_new (GtkType, nparams);

      va_start (args, nparams);

      for (i = 0; i < nparams; i++)
	params[i] = va_arg (args, GtkType);

      va_end (args);
    }
  else
    params = NULL;

  signal_id = gtk_signal_newv (name,
			       0,
			       class->type,
			       0,
			       marshaller,
			       return_val,
			       nparams,
			       params);

  g_free (params);

  if (signal_id)
    gtk_object_class_add_signals (class, &signal_id, 1);

  return signal_id;
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
  
  if (object->ref_count == 1)
    gtk_object_destroy (object);
  
  if (object->ref_count > 0)
    object->ref_count -= 1;
  
  if (object->ref_count == 0)
    {
      obj_count--;
      
#ifdef	GTK_OBJECT_DEBUG
      g_assert (g_slist_find (living_objs, object));
      living_objs = g_slist_remove (living_objs, object);
      /*
	fprintf (stderr, "finalizing %p %s\n", object, gtk_type_name (object->klass->type));
	*/
#endif	GTK_OBJECT_DEBUG
      
      object->klass->finalize (object);
    }
}

/*****************************************
 * gtk_object_finalize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_object_finalize (GtkObject *object)
{
  GtkObjectData *odata;
  
  gtk_object_notify_weaks (object);
  
  if (object->object_data)
    {
      odata = object->object_data;
      while (odata->next)
	odata = odata->next;
      
      odata->next = object_data_free_list;
      object_data_free_list = object->object_data;
      object->object_data = NULL;
    }
  
  g_free (object);
}

/*****************************************
 * gtk_object_sink:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_object_sink (GtkObject *object)
{
  if (GTK_OBJECT_FLOATING (object))
    {
      GTK_OBJECT_UNSET_FLAGS (object, GTK_FLOATING);
      gtk_object_unref (object);
    }
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

  if (!GTK_OBJECT_DESTROYED (object))
    {
      GTK_OBJECT_SET_FLAGS (object, GTK_DESTROYED);
      gtk_signal_emit (object, object_signals[DESTROY]);
    }
}

/*****************************************
 * Weak references.
 *
 * Weak refs are very similar to the old "destroy" signal.  They allow
 * one to register a callback that is called when the weakly
 * referenced object is destroyed.
 *  
 * They are not implemented as a signal because they really are
 * special and need to be used with great care.  Unlike signals, who
 * should be able to execute any code whatsoever.
 * 
 * A weakref callback is not allowed to retain a reference to the
 * object.  In fact, the object is no longer there at all when it is
 * called.
 * 
 * A weakref callback is called atmost once.
 *
 *****************************************/

typedef struct _GtkWeakRef	GtkWeakRef;

struct _GtkWeakRef
{
  GtkWeakRef	   *next;
  GtkDestroyNotify  notify;
  gpointer          data;
};

static const gchar *weakrefs_key = "weakrefs";

void
gtk_object_weakref (GtkObject        *object,
		    GtkDestroyNotify  notify,
		    gpointer          data)
{
  GtkWeakRef *weak;

  g_return_if_fail (object != NULL);
  g_return_if_fail (notify != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  weak = g_new (GtkWeakRef, 1);
  weak->next = gtk_object_get_data (object, weakrefs_key);
  weak->notify = notify;
  weak->data = data;
  gtk_object_set_data (object, weakrefs_key, weak);
}

void
gtk_object_weakunref (GtkObject        *object,
		      GtkDestroyNotify  notify,
		      gpointer          data)
{
  GtkWeakRef *weaks, *w, **wp;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  weaks = gtk_object_get_data (object, weakrefs_key);
  for (wp = &weaks; *wp; wp = &(*wp)->next)
    {
      w = *wp;
      if (w->notify == notify && w->data == data)
	{
	  if (w == weaks)
	    gtk_object_set_data (object, weakrefs_key, w->next);
	  else
	    *wp = w->next;
	  g_free (w);
	  return;
	}
    }
}

static void
gtk_object_notify_weaks (GtkObject *object)
{
  GtkWeakRef *w1, *w2;

  w1 = gtk_object_get_data (object, weakrefs_key);
  gtk_object_set_data (object, weakrefs_key, NULL);

  while (w1)
    {
      w1->notify (w1->data);
      w2 = w1->next;
      g_free (w1);
      w1 = w2;
    }
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
  guint nargs;
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
		 guint    nargs,
		 GtkArg *args)
{
  gpointer obj;

  obj = gtk_type_new (type);
  gtk_object_setv (obj, nargs, args);

  return obj;
}

/*****************************************
 * gtk_object_getv:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_object_getv (GtkObject           *object,
		 guint		     nargs,
		 GtkArg              *args)
{
  int i;
  
  g_return_if_fail (object != NULL);
  
  if (!arg_info_ht)
    return;
  
  for (i = 0; i < nargs; i++)
    {
      GtkArgInfo *info;
      gchar *lookup_name;
      gchar *d;
      
      
      /* hm, the name cutting shouldn't be needed on gets, but what the heck...
       */
      lookup_name = g_strdup (args[i].name);
      d = strchr (lookup_name, ':');
      if (d && d[1] == ':')
	{
	  d = strchr (d + 2, ':');
	  if (d)
	    *d = 0;
	  
	  info = g_hash_table_lookup (arg_info_ht, lookup_name);
	}
      else
	info = NULL;
      
      if (!info)
	{
	  g_warning ("invalid arg name: \"%s\"\n", lookup_name);
	  args[i].type = GTK_TYPE_INVALID;
	  g_free (lookup_name);
	  continue;
	}
      else if (!gtk_type_is_a (object->klass->type, info->class_type))
	{
	  g_warning ("invalid arg for %s: \"%s\"\n", gtk_type_name (object->klass->type), lookup_name);
	  args[i].type = GTK_TYPE_INVALID;
	  g_free (lookup_name);
	  continue;
	}
      else
	g_free (lookup_name);

      args[i].type = info->type;
      gtk_type_get_arg (object, info->class_type, &args[i], info->arg_id);
    }
}

/*****************************************
 * gtk_object_query_args:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

struct _GtkQueryArgData
{
  GList *arg_list;
  GtkType class_type;
};
typedef	struct	_GtkQueryArgData	GtkQueryArgData;

static void
gtk_query_arg_foreach (gpointer key,
		       gpointer value,
		       gpointer user_data)
{
  register GtkArgInfo *info;
  register GtkQueryArgData *data;

  info = value;
  data = user_data;

  if (info->class_type == data->class_type)
    data->arg_list = g_list_prepend (data->arg_list, info);
}

GtkArg*
gtk_object_query_args (GtkType	class_type,
		       guint    *nargs)
{
  GtkArg *args;
  GtkQueryArgData query_data;

  g_return_val_if_fail (nargs != NULL, NULL);
  *nargs = 0;
  g_return_val_if_fail (gtk_type_is_a (class_type, gtk_object_get_type ()), NULL);

  if (!arg_info_ht)
    return NULL;

  query_data.arg_list = NULL;
  query_data.class_type = class_type;
  g_hash_table_foreach (arg_info_ht, gtk_query_arg_foreach, &query_data);

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
      g_assert (len == ((GtkObjectClass*) gtk_type_class (class_type))->n_args); /* paranoid */

      args = g_new0 (GtkArg, len);
      *nargs = len;

      do
	{
	  GtkArgInfo *info;

	  info = list->data;
	  list = list->prev;

	  g_assert (info->seq_id > 0 && info->seq_id <= len); /* paranoid */

	  args[info->seq_id - 1].type = info->type;
	  args[info->seq_id - 1].name = info->name;
	}
      while (list);

      g_list_free (query_data.arg_list);
    }
  else
    args = NULL;

  return args;
}

/*****************************************
 * gtk_object_set:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_object_set (GtkObject *object,
		...)
{
  GtkArg *args;
  guint nargs;
  va_list args1;
  va_list args2;

  g_return_if_fail (object != NULL);

  va_start (args1, object);
  va_start (args2, object);

  args = gtk_object_collect_args (&nargs, args1, args2);
  gtk_object_setv (object, nargs, args);
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
gtk_object_setv (GtkObject *object,
		 guint      nargs,
		 GtkArg    *args)
{
  int i;

  g_return_if_fail (object != NULL);

  if (!arg_info_ht)
    return;

  for (i = 0; i < nargs; i++)
    {
      GtkArgInfo *info;
      gchar *lookup_name;
      gchar *d;
      gboolean arg_ok;

      lookup_name = g_strdup (args[i].name);
      d = strchr (lookup_name, ':');
      if (d && d[1] == ':')
	{
	  d = strchr (d + 2, ':');
	  if (d)
	    *d = 0;

	  info = g_hash_table_lookup (arg_info_ht, lookup_name);
	}
      else
	info = NULL;

      arg_ok = TRUE;
      
      if (!info)
	{
	  g_warning ("invalid arg name: \"%s\"\n", lookup_name);
	  arg_ok = FALSE;
	}
      else if (info->type != args[i].type)
	{
	  g_warning ("invalid arg type for: \"%s\"\n", lookup_name);
	  arg_ok = FALSE;
	}
      else if (!gtk_type_is_a (object->klass->type, info->class_type))
	{
	  g_warning ("invalid arg for %s: \"%s\"\n", gtk_type_name (object->klass->type), lookup_name);
	  arg_ok = FALSE;
	}
      
      g_free (lookup_name);

      if (!arg_ok)
	continue;

      gtk_type_set_arg (object, info->class_type, &args[i], info->arg_id);
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
			 GtkType     arg_type,
			 guint	     arg_id)
{
  GtkArgInfo *info;
  gchar class_part[1024];
  gchar *arg_part;
  GtkType class_type;

  g_return_if_fail (arg_id > 0);
  
  arg_part = strchr (arg_name, ':');
  if (!arg_part || (arg_part[0] != ':') || (arg_part[1] != ':'))
    {
      g_warning ("invalid arg name: \"%s\"\n", arg_name);
      return;
    }

  strncpy (class_part, arg_name, (glong) (arg_part - arg_name));
  class_part[(glong) (arg_part - arg_name)] = '\0';

  class_type = gtk_type_from_name (class_part);
  if (!class_type)
    {
      g_warning ("invalid class name in arg: \"%s\"\n", arg_name);
      return;
    }

  info = g_new (GtkArgInfo, 1);
  info->name = g_strdup (arg_name);
  info->type = arg_type;
  info->class_type = class_type;
  info->arg_id = arg_id;
  info->seq_id = ++((GtkObjectClass*) gtk_type_class (class_type))->n_args;

  if (!arg_info_ht)
    arg_info_ht = g_hash_table_new (g_str_hash, g_str_equal);

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
      g_warning ("invalid arg name: \"%s\"\n", arg_name);
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

      object_data_ht = g_hash_table_new (g_str_hash, g_str_equal);
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
 * gtk_object_collect_args:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkArg*
gtk_object_collect_args (guint   *nargs,
			 va_list  args1,
			 va_list  args2)
{
  GtkArg *args;
  GtkType type;
  gchar *name;
  gint done;
  gint i, n;

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
	  g_warning ("invalid arg name: \"%s\" %x\n", name, type);
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
	case GTK_TYPE_DOUBLE:
	  (void) va_arg (args1, gdouble);
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
	    case GTK_TYPE_DOUBLE:
	      GTK_VALUE_DOUBLE(args[i]) = va_arg (args2, gdouble);
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
