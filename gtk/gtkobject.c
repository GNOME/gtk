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
#include <stdio.h>
#include "gtkobject.h"
#include "gtksignal.h"


enum {
  DESTROY,
  LAST_SIGNAL
};
enum {
  ARG_0,
  ARG_USER_DATA,
  ARG_SIGNAL,
  ARG_SIGNAL_AFTER,
  ARG_OBJECT_SIGNAL,
  ARG_OBJECT_SIGNAL_AFTER
};


void		      gtk_object_init_type       (void);
static void           gtk_object_base_class_init (GtkObjectClass *klass);
static void           gtk_object_class_init      (GtkObjectClass *klass);
static void           gtk_object_init            (GtkObject      *object,
						  GtkObjectClass *klass);
static void           gtk_object_set_arg         (GtkObject      *object,
						  GtkArg         *arg,
						  guint		  arg_id);
static void           gtk_object_get_arg         (GtkObject      *object,
						  GtkArg         *arg,
						  guint		  arg_id);
static void           gtk_object_shutdown        (GtkObject      *object);
static void           gtk_object_real_destroy    (GtkObject      *object);
static void           gtk_object_finalize        (GtkObject      *object);
static void           gtk_object_notify_weaks    (GtkObject      *object);

static guint object_signals[LAST_SIGNAL] = { 0 };

static GHashTable *object_arg_info_ht = NULL;

static GQuark quark_user_data = 0;
static GQuark quark_weakrefs = 0;
static GQuark quark_carg_history = 0;


#ifdef G_ENABLE_DEBUG
static guint obj_count = 0;
static GHashTable *living_objs_ht = NULL;
static void
gtk_object_debug_foreach (gpointer key, gpointer value, gpointer user_data)
{
  GtkObject *object;
  
  object = (GtkObject*) value;
  g_message ("[%p] %s\tref_count=%d%s%s",
	     object,
	     gtk_type_name (GTK_OBJECT_TYPE (object)),
	     object->ref_count,
	     GTK_OBJECT_FLOATING (object) ? " (floating)" : "",
	     GTK_OBJECT_DESTROYED (object) ? " (destroyed)" : "");
}
static void
gtk_object_debug (void)
{
  if (living_objs_ht)
    g_hash_table_foreach (living_objs_ht, gtk_object_debug_foreach, NULL);
  
  g_message ("living objects count = %d", obj_count);
}
#endif	/* G_ENABLE_DEBUG */

void
gtk_object_post_arg_parsing_init (void)
{
#ifdef G_ENABLE_DEBUG
  if (gtk_debug_flags & GTK_DEBUG_OBJECTS)
    g_atexit (gtk_object_debug);
#endif	/* G_ENABLE_DEBUG */
}

/****************************************************
 * GtkObject type, class and instance initialization
 *
 ****************************************************/

void
gtk_object_init_type (void)
{
  static const GtkTypeInfo object_info =
  {
    "GtkObject",
    sizeof (GtkObject),
    sizeof (GtkObjectClass),
    (GtkClassInitFunc) gtk_object_class_init,
    (GtkObjectInitFunc) gtk_object_init,
    /* reserved_1 */ NULL,
    /* reserved_2 */ NULL,
    (GtkClassInitFunc) gtk_object_base_class_init,
  };
  GtkType object_type;

  object_type = gtk_type_unique (0, &object_info);
  g_assert (object_type == GTK_TYPE_OBJECT);
}

GtkType
gtk_object_get_type (void)
{
  return GTK_TYPE_OBJECT;
}

static void
gtk_object_base_class_init (GtkObjectClass *class)
{
  /* reset instance specific fields that don't get inherited */
  class->signals = NULL;
  class->nsignals = 0;
  class->n_args = 0;
  class->construct_args = NULL;

  /* reset instance specifc methods that don't get inherited */
  class->get_arg = NULL;
  class->set_arg = NULL;
}

static void
gtk_object_class_init (GtkObjectClass *class)
{
  quark_carg_history = g_quark_from_static_string ("gtk-construct-arg-history");

  gtk_object_add_arg_type ("GtkObject::user_data",
			   GTK_TYPE_POINTER,
			   GTK_ARG_READWRITE,
			   ARG_USER_DATA);
  gtk_object_add_arg_type ("GtkObject::signal",
			   GTK_TYPE_SIGNAL,
			   GTK_ARG_WRITABLE,
			   ARG_SIGNAL);
  gtk_object_add_arg_type ("GtkObject::signal_after",
			   GTK_TYPE_SIGNAL,
			   GTK_ARG_WRITABLE,
			   ARG_SIGNAL_AFTER);
  gtk_object_add_arg_type ("GtkObject::object_signal",
			   GTK_TYPE_SIGNAL,
			   GTK_ARG_WRITABLE,
			   ARG_OBJECT_SIGNAL);
  gtk_object_add_arg_type ("GtkObject::object_signal_after",
			   GTK_TYPE_SIGNAL,
			   GTK_ARG_WRITABLE,
			   ARG_OBJECT_SIGNAL_AFTER);

  object_signals[DESTROY] =
    gtk_signal_new ("destroy",
                    GTK_RUN_LAST | GTK_RUN_NO_HOOKS,
                    class->type,
                    GTK_SIGNAL_OFFSET (GtkObjectClass, destroy),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (class, object_signals, LAST_SIGNAL);

  class->get_arg = gtk_object_get_arg;
  class->set_arg = gtk_object_set_arg;
  class->shutdown = gtk_object_shutdown;
  class->destroy = gtk_object_real_destroy;
  class->finalize = gtk_object_finalize;
}

static void
gtk_object_init (GtkObject      *object,
		 GtkObjectClass *klass)
{
  gboolean needs_construction = FALSE;

  GTK_OBJECT_FLAGS (object) = GTK_FLOATING;
  do
    {
      needs_construction |= klass->construct_args != NULL;
      klass = gtk_type_parent_class (klass->type);
    }
  while (klass && !needs_construction);
  if (!needs_construction)
    GTK_OBJECT_FLAGS (object) |= GTK_CONSTRUCTED;

  object->ref_count = 1;
  g_datalist_init (&object->object_data);

#ifdef G_ENABLE_DEBUG
  if (gtk_debug_flags & GTK_DEBUG_OBJECTS)
    {
      obj_count++;
      
      if (!living_objs_ht)
	living_objs_ht = g_hash_table_new (g_direct_hash, NULL);

      g_hash_table_insert (living_objs_ht, object, object);
    }
#endif /* G_ENABLE_DEBUG */
}

/********************************************
 * Functions to end a GtkObject's life time
 *
 ********************************************/
void
gtk_object_destroy (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (GTK_OBJECT_CONSTRUCTED (object));
  
  if (!GTK_OBJECT_DESTROYED (object))
    {
      /* we will hold a reference on the object in this place, so
       * to ease all classes shutdown and destroy implementations.
       * i.e. they don't have to bother about referencing at all.
       */
      gtk_object_ref (object);
      object->klass->shutdown (object);
      gtk_object_unref (object);
    }
}

static void
gtk_object_shutdown (GtkObject *object)
{
  GTK_OBJECT_SET_FLAGS (object, GTK_DESTROYED);
  gtk_signal_emit (object, object_signals[DESTROY]);
}

static void
gtk_object_real_destroy (GtkObject *object)
{
  if (GTK_OBJECT_CONNECTED (object))
    gtk_signal_handlers_destroy (object);
}

static void
gtk_object_finalize (GtkObject *object)
{
  gtk_object_notify_weaks (object);

  g_datalist_clear (&object->object_data);
  
  gtk_type_free (GTK_OBJECT_TYPE (object), object);
}

/*****************************************
 * GtkObject argument handlers
 *
 *****************************************/

static void
gtk_object_set_arg (GtkObject *object,
		    GtkArg    *arg,
		    guint      arg_id)
{
  guint n = 0;

  switch (arg_id)
    {
      gchar *arg_name;

    case ARG_USER_DATA:
      gtk_object_set_user_data (object, GTK_VALUE_POINTER (*arg));
      break;
    case ARG_OBJECT_SIGNAL_AFTER:
      n += 6;
    case ARG_OBJECT_SIGNAL:
      n += 1;
    case ARG_SIGNAL_AFTER:
      n += 6;
    case ARG_SIGNAL:
      n += 6;
      arg_name = gtk_arg_name_strip_type (arg->name);
      if (arg_name &&
	  arg_name[n] == ':' &&
	  arg_name[n + 1] == ':' &&
	  arg_name[n + 2] != 0)
	{
	  gtk_signal_connect_full (object,
				   arg_name + n + 2,
				   GTK_VALUE_SIGNAL (*arg).f, NULL,
				   GTK_VALUE_SIGNAL (*arg).d,
				   NULL,
				   (arg_id == ARG_OBJECT_SIGNAL ||
				    arg_id == ARG_OBJECT_SIGNAL_AFTER),
				   (arg_id == ARG_OBJECT_SIGNAL_AFTER ||
				    arg_id == ARG_SIGNAL_AFTER));
	}
      else
	g_warning ("gtk_object_set_arg(): invalid signal argument: \"%s\"\n", arg->name);
      break;
    default:
      break;
    }
}

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
			      guint          *signals,
			      guint           nsignals)
{
  g_return_if_fail (GTK_IS_OBJECT_CLASS (class));
  if (!nsignals)
    return;
  g_return_if_fail (signals != NULL);
  
  class->signals = g_renew (guint, class->signals, class->nsignals + nsignals);
  memcpy (class->signals + class->nsignals, signals, nsignals * sizeof (guint));
  class->nsignals += nsignals;
}

guint
gtk_object_class_user_signal_new (GtkObjectClass     *class,
				  const gchar        *name,
				  GtkSignalRunType    signal_flags,
				  GtkSignalMarshaller marshaller,
				  GtkType             return_val,
				  guint               nparams,
				  ...)
{
  GtkType *params;
  guint i;
  va_list args;
  guint signal_id;

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
			       signal_flags,
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

guint
gtk_object_class_user_signal_newv (GtkObjectClass     *class,
				   const gchar        *name,
				   GtkSignalRunType    signal_flags,
				   GtkSignalMarshaller marshaller,
				   GtkType             return_val,
				   guint               nparams,
				   GtkType	      *params)
{
  guint signal_id;

  g_return_val_if_fail (class != NULL, 0);

  if (nparams > 0)
    g_return_val_if_fail (params != NULL, 0);

  signal_id = gtk_signal_newv (name,
			       signal_flags,
			       class->type,
			       0,
			       marshaller,
			       return_val,
			       nparams,
			       params);

  if (signal_id)
    gtk_object_class_add_signals (class, &signal_id, 1);

  return signal_id;
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
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  if (GTK_OBJECT_FLOATING (object))
    {
      GTK_OBJECT_UNSET_FLAGS (object, GTK_FLOATING);
      gtk_object_unref (object);
    }
}

/*****************************************
 * Weak references.
 *
 * Weak refs are very similar to the old "destroy" signal.  They allow
 * one to register a callback that is called when the weakly
 * referenced object is finalized.
 *  
 * They are not implemented as a signal because they really are
 * special and need to be used with great care.  Unlike signals, which
 * should be able to execute any code whatsoever.
 * 
 * A weakref callback is not allowed to retain a reference to the
 * object.  Object data keys may be retrieved in a weak reference
 * callback.
 * 
 * A weakref callback is called at most once.
 *
 *****************************************/

typedef struct _GtkWeakRef	GtkWeakRef;

struct _GtkWeakRef
{
  GtkWeakRef	   *next;
  GtkDestroyNotify  notify;
  gpointer          data;
};

void
gtk_object_weakref (GtkObject        *object,
		    GtkDestroyNotify  notify,
		    gpointer          data)
{
  GtkWeakRef *weak;

  g_return_if_fail (object != NULL);
  g_return_if_fail (notify != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  if (!quark_weakrefs)
    quark_weakrefs = g_quark_from_static_string ("gtk-weakrefs");

  weak = g_new (GtkWeakRef, 1);
  weak->next = gtk_object_get_data_by_id (object, quark_weakrefs);
  weak->notify = notify;
  weak->data = data;
  gtk_object_set_data_by_id (object, quark_weakrefs, weak);
}

void
gtk_object_weakunref (GtkObject        *object,
		      GtkDestroyNotify  notify,
		      gpointer          data)
{
  GtkWeakRef *weaks, *w, **wp;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  if (!quark_weakrefs)
    return;

  weaks = gtk_object_get_data_by_id (object, quark_weakrefs);
  for (wp = &weaks; *wp; wp = &(*wp)->next)
    {
      w = *wp;
      if (w->notify == notify && w->data == data)
	{
	  if (w == weaks)
	    gtk_object_set_data_by_id (object, quark_weakrefs, w->next);
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
  if (quark_weakrefs)
    {
      GtkWeakRef *w1, *w2;
      
      w1 = gtk_object_get_data_by_id (object, quark_weakrefs);
      
      while (w1)
	{
	  w1->notify (w1->data);
	  w2 = w1->next;
	  g_free (w1);
	  w1 = w2;
	}
    }
}

/****************************************************
 * GtkObject argument mechanism and object creation
 *
 ****************************************************/

GtkObject*
gtk_object_new (GtkType      object_type,
		const gchar *first_arg_name,
		...)
{
  GtkObject *object;
  va_list var_args;
  GSList *arg_list = NULL;
  GSList *info_list = NULL;
  gchar *error;

  g_return_val_if_fail (GTK_FUNDAMENTAL_TYPE (object_type) == GTK_TYPE_OBJECT, NULL);

  object = gtk_type_new (object_type);

  va_start (var_args, first_arg_name);
  error = gtk_object_args_collect (GTK_OBJECT_TYPE (object),
				   &arg_list,
				   &info_list,
				   first_arg_name,
				   var_args);
  va_end (var_args);
  
  if (error)
    {
      g_warning ("gtk_object_new(): %s", error);
      g_free (error);
    }
  else
    {
      GSList *slist_arg;
      GSList *slist_info;
      
      slist_arg = arg_list;
      slist_info = info_list;
      while (slist_arg)
	{
	  gtk_object_arg_set (object, slist_arg->data, slist_info->data);
	  slist_arg = slist_arg->next;
	  slist_info = slist_info->next;
	}
      gtk_args_collect_cleanup (arg_list, info_list);
    }

  if (!GTK_OBJECT_CONSTRUCTED (object))
    gtk_object_default_construct (object);

  return object;
}

GtkObject*
gtk_object_newv (GtkType  object_type,
		 guint    n_args,
		 GtkArg  *args)
{
  GtkObject *object;
  GtkArg *max_args;
  
  g_return_val_if_fail (GTK_FUNDAMENTAL_TYPE (object_type) == GTK_TYPE_OBJECT, NULL);
  if (n_args)
    g_return_val_if_fail (args != NULL, NULL);
  
  object = gtk_type_new (object_type);
  
  for (max_args = args + n_args; args < max_args; args++)
    gtk_object_arg_set (object, args, NULL);
  
  if (!GTK_OBJECT_CONSTRUCTED (object))
    gtk_object_default_construct (object);

  return object;
}

void
gtk_object_setv (GtkObject *object,
		 guint      n_args,
		 GtkArg    *args)
{
  GtkArg *max_args;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  if (n_args)
    g_return_if_fail (args != NULL);

  for (max_args = args + n_args; args < max_args; args++)
    gtk_object_arg_set (object, args, NULL);
}

void
gtk_object_getv (GtkObject           *object,
		 guint		      n_args,
		 GtkArg              *args)
{
  GtkArg *max_args;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  if (n_args)
    g_return_if_fail (args != NULL);
  
  for (max_args = args + n_args; args < max_args; args++)
    gtk_object_arg_get (object, args, NULL);
}

void
gtk_object_get (GtkObject   *object,
		const gchar *first_arg_name,
		...)
{
  va_list var_args;
  gchar *name;
  
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  va_start (var_args, first_arg_name);

  name = (gchar*) first_arg_name;
  while (name)
    {
      gpointer value_pointer = va_arg (var_args, gpointer);

      if (value_pointer)
	{
	  GtkArgInfo *info;
	  gchar *error;
	  GtkArg arg;
	  
	  error = gtk_arg_get_info (GTK_OBJECT_TYPE (object),
				    object_arg_info_ht,
				    name,
				    &info);
	  if (error)
	    {
	      g_warning ("gtk_object_get(): %s", error);
	      g_free (error);
	      return;
	    }
	  
	  arg.name = name;
	  gtk_object_arg_get (object, &arg, info);
	  gtk_arg_to_valueloc (&arg, value_pointer);
	}

      name = va_arg (var_args, gchar*);
    }
}

void
gtk_object_set (GtkObject *object,
		const gchar    *first_arg_name,
		...)
{
  va_list var_args;
  GSList *arg_list = NULL;
  GSList *info_list = NULL;
  gchar *error;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  va_start (var_args, first_arg_name);
  error = gtk_object_args_collect (GTK_OBJECT_TYPE (object),
				   &arg_list,
				   &info_list,
				   first_arg_name,
				   var_args);
  va_end (var_args);
  
  if (error)
    {
      g_warning ("gtk_object_set(): %s", error);
      g_free (error);
    }
  else
    {
      GSList *slist_arg;
      GSList *slist_info;
      
      slist_arg = arg_list;
      slist_info = info_list;
      while (slist_arg)
	{
	  gtk_object_arg_set (object, slist_arg->data, slist_info->data);
	  slist_arg = slist_arg->next;
	  slist_info = slist_info->next;
	}
      gtk_args_collect_cleanup (arg_list, info_list);
    }
}

void
gtk_object_arg_set (GtkObject  *object,
		    GtkArg     *arg,
		    GtkArgInfo *info)
{
  GtkObjectClass *oclass;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (arg != NULL);

  if (!info)
    {
      gchar *error;

      error = gtk_arg_get_info (GTK_OBJECT_TYPE (object),
				object_arg_info_ht,
				arg->name,
				&info);
      if (error)
	{
	  g_warning ("gtk_object_arg_set(): %s", error);
	  g_free (error);
	  return;
	}
    }

  if (info->arg_flags & GTK_ARG_CONSTRUCT_ONLY &&
      GTK_OBJECT_CONSTRUCTED (object))
    {
      g_warning ("gtk_object_arg_set(): cannot set argument \"%s\" for constructed object",
		 info->full_name);
      return;
    }
  if (!(info->arg_flags & GTK_ARG_WRITABLE))
    {
      g_warning ("gtk_object_arg_set(): argument \"%s\" is not writable",
		 info->full_name);
      return;
    }
  if (info->type != arg->type)
    {
      g_warning ("gtk_object_arg_set(): argument \"%s\" has invalid type `%s'",
		 info->full_name,
		 gtk_type_name (arg->type));
      return;
    }
  
  oclass = gtk_type_class (info->class_type);
  g_assert (oclass->set_arg != NULL);
  oclass->set_arg (object, arg, info->arg_id);
  if (!GTK_OBJECT_CONSTRUCTED (object) &&
      (info->arg_flags & GTK_ARG_CONSTRUCT_ONLY ||
       info->arg_flags & GTK_ARG_CONSTRUCT))
    {
      GSList *slist;

      slist = gtk_object_get_data_by_id (object, quark_carg_history);
      gtk_object_set_data_by_id (object,
				 quark_carg_history,
				 g_slist_prepend (slist, info));
    }
}

void
gtk_object_arg_get (GtkObject  *object,
		    GtkArg     *arg,
		    GtkArgInfo *info)
{
  GtkObjectClass *oclass;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (arg != NULL);

  if (!info)
    {
      gchar *error;

      error = gtk_arg_get_info (GTK_OBJECT_TYPE (object),
				object_arg_info_ht,
				arg->name,
				&info);
      if (error)
	{
	  g_warning ("gtk_object_arg_get(): %s", error);
	  g_free (error);
	  arg->type = GTK_TYPE_INVALID;
	  return;
	}
    }
  
  if (! (info->arg_flags & GTK_ARG_READABLE))
    {
      g_warning ("gtk_object_arg_get(): argument \"%s\" is not readable",
		 info->full_name);
      arg->type = GTK_TYPE_INVALID;
      return;
    }
  
  oclass = gtk_type_class (info->class_type);
  g_assert (oclass->get_arg != NULL);
  arg->type = info->type;
  oclass->get_arg (object, arg, info->arg_id);
}

void
gtk_object_default_construct (GtkObject *object)
{
  GSList *slist;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  if (!GTK_OBJECT_CONSTRUCTED (object))
    {
      for (slist = object->klass->construct_args;
	   slist && !GTK_OBJECT_CONSTRUCTED (object);
	   slist = slist->next)
	{
	  GSList *history;
	  GtkArgInfo *info;
	  
	  info = slist->data;
	  history = gtk_object_get_data_by_id (object, quark_carg_history);
	  if (!g_slist_find (history, info))
	    {
	      GtkArg arg;
	      
	      /* default application */
	      arg.type = info->type;
	      arg.name = info->name;
	      switch (gtk_type_get_varargs_type (arg.type))
		{
		case GTK_TYPE_FLOAT:
		  GTK_VALUE_FLOAT (arg) = 0.0;
		  break;
		case GTK_TYPE_DOUBLE:
		  GTK_VALUE_DOUBLE (arg) = 0.0;
		  break;
		case GTK_TYPE_BOXED:
		case GTK_TYPE_STRING:
		case GTK_TYPE_POINTER:
		case GTK_TYPE_OBJECT:
		  GTK_VALUE_POINTER (arg) = NULL;
		  break;
		default:
		  memset (&arg.d, 0, sizeof (arg.d));
		  break;
		}
	      gtk_object_arg_set (object, &arg, info);
	    }
	}

      if (!GTK_OBJECT_CONSTRUCTED (object))
	gtk_object_constructed (object);
    }
}

void
gtk_object_constructed (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (GTK_OBJECT_CONSTRUCTED (object) == FALSE);
  
  g_slist_free (gtk_object_get_data_by_id (object, quark_carg_history));
  gtk_object_set_data_by_id (object, quark_carg_history, NULL);
  GTK_OBJECT_FLAGS (object) |= GTK_CONSTRUCTED;
}

void
gtk_object_add_arg_type (const char *arg_name,
			 GtkType     arg_type,
			 guint	     arg_flags,
			 guint	     arg_id)
{
  GtkArgInfo *info;

  g_return_if_fail (arg_name != NULL);
  g_return_if_fail (arg_type > GTK_TYPE_NONE);
  g_return_if_fail (arg_id > 0);
  g_return_if_fail ((arg_flags & GTK_ARG_CHILD_ARG) == 0);
  if (arg_flags & GTK_ARG_CONSTRUCT)
    g_return_if_fail ((arg_flags & GTK_ARG_READWRITE) == GTK_ARG_READWRITE);
  else
    g_return_if_fail ((arg_flags & GTK_ARG_READWRITE) != 0);
  if (arg_flags & GTK_ARG_CONSTRUCT_ONLY)
    g_return_if_fail ((arg_flags & GTK_ARG_WRITABLE) == GTK_ARG_WRITABLE);
    
  if (!object_arg_info_ht)
    object_arg_info_ht = g_hash_table_new (gtk_arg_info_hash,
					   gtk_arg_info_equal);

  info = gtk_arg_type_new_static (GTK_TYPE_OBJECT,
				  arg_name,
				  GTK_STRUCT_OFFSET (GtkObjectClass, n_args),
				  object_arg_info_ht,
				  arg_type,
				  arg_flags,
				  arg_id);
  if (info &&
      (info->arg_flags & GTK_ARG_CONSTRUCT ||
       info->arg_flags & GTK_ARG_CONSTRUCT_ONLY))
    {
      GtkObjectClass *class;

      class = gtk_type_class (info->class_type);
      if (info->arg_flags & GTK_ARG_CONSTRUCT_ONLY)
	class->construct_args = g_slist_prepend (class->construct_args, info);
      else
	class->construct_args = g_slist_append (class->construct_args, info);
    }
}

gchar*
gtk_object_args_collect (GtkType      object_type,
			 GSList      **arg_list_p,
			 GSList      **info_list_p,
			 const gchar  *first_arg_name,
			 va_list       var_args)
{
  return gtk_args_collect (object_type,
			   object_arg_info_ht,
			   arg_list_p,
			   info_list_p,
			   first_arg_name,
			   var_args);
}

gchar*
gtk_object_arg_get_info (GtkType      object_type,
			 const gchar *arg_name,
			 GtkArgInfo **info_p)
{
  return gtk_arg_get_info (object_type,
			   object_arg_info_ht,
			   arg_name,
			   info_p);
}

GtkArg*
gtk_object_query_args (GtkType        class_type,
		       guint32      **arg_flags,
		       guint         *n_args)
{
  g_return_val_if_fail (n_args != NULL, NULL);
  *n_args = 0;
  g_return_val_if_fail (GTK_FUNDAMENTAL_TYPE (class_type) == GTK_TYPE_OBJECT, NULL);

  return gtk_args_query (class_type, object_arg_info_ht, arg_flags, n_args);
}

/*****************************************
 * GtkObject object_data mechanism
 *
 *****************************************/

void
gtk_object_set_data_by_id (GtkObject        *object,
			   GQuark	     data_id,
			   gpointer          data)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  g_datalist_id_set_data (&object->object_data, data_id, data);
}

void
gtk_object_set_data (GtkObject        *object,
		     const gchar      *key,
		     gpointer          data)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);
  
  g_datalist_set_data (&object->object_data, key, data);
}

void
gtk_object_set_data_by_id_full (GtkObject        *object,
				GQuark		  data_id,
				gpointer          data,
				GtkDestroyNotify  destroy)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  g_datalist_id_set_data_full (&object->object_data, data_id, data, destroy);
}

void
gtk_object_set_data_full (GtkObject        *object,
			  const gchar      *key,
			  gpointer          data,
			  GtkDestroyNotify  destroy)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);

  g_datalist_set_data_full (&object->object_data, key, data, destroy);
}

gpointer
gtk_object_get_data_by_id (GtkObject   *object,
			   GQuark       data_id)
{
  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);

  return g_datalist_id_get_data (&object->object_data, data_id);
}

gpointer
gtk_object_get_data (GtkObject   *object,
		     const gchar *key)
{
  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_datalist_get_data (&object->object_data, key);
}

void
gtk_object_remove_data_by_id (GtkObject   *object,
			      GQuark       data_id)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  g_datalist_id_remove_data (&object->object_data, data_id);
}

void
gtk_object_remove_data (GtkObject   *object,
			const gchar *key)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);

  g_datalist_remove_data (&object->object_data, key);
}

void
gtk_object_remove_no_notify_by_id (GtkObject      *object,
				   GQuark          key_id)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  g_datalist_id_remove_no_notify (&object->object_data, key_id);
}

void
gtk_object_remove_no_notify (GtkObject       *object,
			     const gchar     *key)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);

  g_datalist_remove_no_notify (&object->object_data, key);
}

void
gtk_object_set_user_data (GtkObject *object,
			  gpointer   data)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  if (!quark_user_data)
    quark_user_data = g_quark_from_static_string ("user_data");

  g_datalist_id_set_data (&object->object_data, quark_user_data, data);
}

gpointer
gtk_object_get_user_data (GtkObject *object)
{
  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);

  return g_datalist_id_get_data (&object->object_data, quark_user_data);
}

/*******************************************
 * GtkObject referencing and unreferencing
 *
 *******************************************/

#undef	gtk_object_ref
#undef	gtk_object_unref

void
gtk_object_ref (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (object->ref_count > 0);

  object->ref_count += 1;
}

void
gtk_object_unref (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (object->ref_count > 0);
  
  if (object->ref_count == 1)
    {
      gtk_object_destroy (object);
  
      g_return_if_fail (object->ref_count > 0);
    }

  object->ref_count -= 1;

  if (object->ref_count == 0)
    {
#ifdef G_ENABLE_DEBUG
      if (gtk_debug_flags & GTK_DEBUG_OBJECTS)
	{
	  g_assert (g_hash_table_lookup (living_objs_ht, object) == object);
	  g_hash_table_remove (living_objs_ht, object);
	  obj_count--;
	}
#endif /* G_ENABLE_DEBUG */      
      object->klass->finalize (object);
    }
}

static GtkObject *gtk_trace_object = NULL;
void
gtk_trace_referencing (GtkObject   *object,
		       const gchar *func,
		       guint	   dummy,
		       guint	   line,
		       gboolean	   do_ref)
{
  if (gtk_debug_flags & GTK_DEBUG_OBJECTS)
    {
      gboolean exists = TRUE;

      g_return_if_fail (object != NULL);
      g_return_if_fail (GTK_IS_OBJECT (object));

#ifdef	G_ENABLE_DEBUG
      exists = g_hash_table_lookup (living_objs_ht, object) != NULL;
#endif	/* G_ENABLE_DEBUG */
      
      if (exists &&
	  (object == gtk_trace_object ||
	   gtk_trace_object == (void*)42))
	fprintf (stdout, "trace: object_%s: (%s:%p)->ref_count=%d %s (%s:%d)\n",
		 do_ref ? "ref" : "unref",
		 gtk_type_name (GTK_OBJECT_TYPE (object)),
		 object,
		 object->ref_count,
		 do_ref ? "+ 1" : "- 1",
		 func,
		 line);
      else if (!exists)
	fprintf (stdout, "trace: object_%s(%p): no such object! (%s:%d)\n",
		 do_ref ? "ref" : "unref",
		 object,
		 func,
		 line);
    }
  
  if (do_ref)
    gtk_object_ref (object);
  else
    gtk_object_unref (object);
}
