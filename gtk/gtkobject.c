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
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "gtkobject.h"
#include "gtksignal.h"


#define GTK_OBJECT_DATA_ID_BLOCK_SIZE	(1024)
#define	GTK_OBJECT_DATA_BLOCK_SIZE	(1024)

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


typedef struct _GtkObjectData  GtkObjectData;

struct _GtkObjectData
{
  guint id;
  gpointer data;
  GtkDestroyNotify destroy;
  GtkObjectData *next;
};


void		      gtk_object_init_type       (void);
static void           gtk_object_base_class_init (GtkObjectClass *klass);
static void           gtk_object_class_init      (GtkObjectClass *klass);
static void           gtk_object_init            (GtkObject      *object);
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

static const gchar *user_data_key = "user_data";
static guint user_data_key_id = 0;
static const gchar *weakrefs_key = "gtk-weakrefs";
static guint weakrefs_key_id = 0;

static GtkObjectData *gtk_object_data_free_list = NULL;

#define GTK_OBJECT_DATA_DESTROY( odata )	{ \
  if (odata->destroy) \
    odata->destroy (odata->data); \
  odata->next = gtk_object_data_free_list; \
  gtk_object_data_free_list = odata; \
}


#ifdef G_ENABLE_DEBUG
static guint obj_count = 0;
static GHashTable *living_objs_ht = NULL;
static void
gtk_object_debug_foreach (gpointer key, gpointer value, gpointer user_data)
{
  GtkObject *object;
  
  object = (GtkObject*) value;
  g_print ("GTK-DEBUG: %p: %s ref_count=%d%s%s\n",
	   object,
	   gtk_type_name (GTK_OBJECT_TYPE (object)),
	   object->ref_count,
	   GTK_OBJECT_FLOATING (object) ? " (floating)" : "",
	   GTK_OBJECT_DESTROYED (object) ? " (destroyed)" : "");
}
static void
gtk_object_debug (void)
{
  g_hash_table_foreach (living_objs_ht, gtk_object_debug_foreach, NULL);

  g_print ("GTK-DEBUG: living objects count = %d\n", obj_count);
}
#endif	/* G_ENABLE_DEBUG */

/****************************************************
 * GtkObject type, class and instance initialization
 *
 ****************************************************/

void
gtk_object_init_type (void)
{
  GtkType object_type = 0;
  GtkTypeInfo object_info =
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

  object_type = gtk_type_unique (0, &object_info);
  g_assert (object_type == GTK_TYPE_OBJECT);

#ifdef G_ENABLE_DEBUG
  if (gtk_debug_flags & GTK_DEBUG_OBJECTS)
    ATEXIT (gtk_object_debug);
#endif	/* G_ENABLE_DEBUG */
}

GtkType
gtk_object_get_type (void)
{
  return GTK_TYPE_OBJECT;
}

static void
gtk_object_base_class_init (GtkObjectClass *class)
{
  /* reset instance specific fields that don't get inhrited */
  class->signals = NULL;
  class->nsignals = 0;
  class->n_args = 0;

  /* reset instance specifc methods that don't get inherited */
  class->get_arg = NULL;
  class->set_arg = NULL;
}

static void
gtk_object_class_init (GtkObjectClass *class)
{
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
                    GTK_RUN_LAST,
                    class->type,
                    GTK_SIGNAL_OFFSET (GtkObjectClass, destroy),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (class, object_signals, LAST_SIGNAL);

  class->get_arg = gtk_object_get_arg;
  class->set_arg = gtk_object_set_arg;
  class->shutdown = gtk_object_shutdown;
  class->destroy = gtk_object_real_destroy;
  class->finalize = gtk_object_finalize;
}

static void
gtk_object_init (GtkObject *object)
{
  GTK_OBJECT_FLAGS (object) = GTK_FLOATING;

  object->ref_count = 1;
  object->object_data = NULL;

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

  while (object->object_data)
    {
      GtkObjectData *odata;

      odata = object->object_data;
      object->object_data = odata->next;
      GTK_OBJECT_DATA_DESTROY (odata);
    }
  
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
  guint *new_signals;
  guint i;

  g_return_if_fail (class != NULL);

  new_signals = g_new (guint, class->nsignals + nsignals);
  for (i = 0; i < class->nsignals; i++)
    new_signals[i] = class->signals[i];
  for (i = 0; i < nsignals; i++)
    new_signals[class->nsignals + i] = signals[i];

  g_free (class->signals);
  class->signals = new_signals;
  class->nsignals += nsignals;
}

guint
gtk_object_class_add_user_signal (GtkObjectClass     *class,
				  const gchar        *name,
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

  if (!weakrefs_key_id)
    weakrefs_key_id = g_quark_from_static_string (weakrefs_key);

  weak = g_new (GtkWeakRef, 1);
  weak->next = gtk_object_get_data_by_id (object, weakrefs_key_id);
  weak->notify = notify;
  weak->data = data;
  gtk_object_set_data_by_id (object, weakrefs_key_id, weak);
}

void
gtk_object_weakunref (GtkObject        *object,
		      GtkDestroyNotify  notify,
		      gpointer          data)
{
  GtkWeakRef *weaks, *w, **wp;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  if (!weakrefs_key_id)
    return;

  weaks = gtk_object_get_data_by_id (object, weakrefs_key_id);
  for (wp = &weaks; *wp; wp = &(*wp)->next)
    {
      w = *wp;
      if (w->notify == notify && w->data == data)
	{
	  if (w == weaks)
	    gtk_object_set_data_by_id (object, weakrefs_key_id, w->next);
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
  if (weakrefs_key_id)
    {
      GtkWeakRef *w1, *w2;
      
      w1 = gtk_object_get_data_by_id (object, weakrefs_key_id);
      
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
gtk_object_new (GtkType object_type,
		...)
{
  GtkObject *object;
  va_list var_args;
  GSList *arg_list = NULL;
  GSList *info_list = NULL;
  gchar *error;

  g_return_val_if_fail (GTK_FUNDAMENTAL_TYPE (object_type) == GTK_TYPE_OBJECT, NULL);

  object = gtk_type_new (object_type);

  va_start (var_args, object_type);
  error = gtk_object_args_collect (GTK_OBJECT_TYPE (object),
				   &arg_list,
				   &info_list,
				   &var_args);
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
gtk_object_set (GtkObject *object,
		...)
{
  va_list var_args;
  GSList *arg_list = NULL;
  GSList *info_list = NULL;
  gchar *error;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  va_start (var_args, object);
  error = gtk_object_args_collect (GTK_OBJECT_TYPE (object),
				   &arg_list,
				   &info_list,
				   &var_args);
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
gtk_object_arg_set (GtkObject *object,
		    GtkArg      *arg,
		    GtkArgInfo  *info)
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
  
  if (! (info->arg_flags & GTK_ARG_WRITABLE))
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
}

void
gtk_object_arg_get (GtkObject           *object,
		    GtkArg              *arg,
		    GtkArgInfo		*info)
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
gtk_object_add_arg_type (const char *arg_name,
			 GtkType     arg_type,
			 guint	     arg_flags,
			 guint	     arg_id)
{
  g_return_if_fail (arg_name != NULL);
  g_return_if_fail (arg_type > GTK_TYPE_NONE);
  g_return_if_fail (arg_id > 0);
  g_return_if_fail ((arg_flags & GTK_ARG_READWRITE) != 0);
  g_return_if_fail ((arg_flags & GTK_ARG_CHILD_ARG) == 0);

  if (!object_arg_info_ht)
    object_arg_info_ht = g_hash_table_new (gtk_arg_info_hash,
					   gtk_arg_info_equal);

  gtk_arg_type_new_static (GTK_TYPE_OBJECT,
			   arg_name,
			   GTK_STRUCT_OFFSET (GtkObjectClass, n_args),
			   object_arg_info_ht,
			   arg_type,
			   arg_flags,
			   arg_id);
}

gchar*
gtk_object_args_collect (GtkType      object_type,
			 GSList      **arg_list_p,
			 GSList      **info_list_p,
			 gpointer      var_args_p)
{
  return gtk_args_collect (object_type,
			   object_arg_info_ht,
			   arg_list_p,
			   info_list_p,
			   var_args_p);
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

/********************************************************
 * GtkObject and GtkObjectClass cast checking functions
 *
 ********************************************************/

static gchar*
gtk_object_descriptive_type_name (GtkType type)
{
  gchar *name;

  name = gtk_type_name (type);
  if (!name)
    name = "(unknown)";

  return name;
}

GtkObject*
gtk_object_check_cast (GtkObject *obj,
		       GtkType    cast_type)
{
  if (!obj)
    {
      g_warning ("invalid cast from (NULL) pointer to `%s'",
		 gtk_object_descriptive_type_name (cast_type));
      return obj;
    }
  if (!obj->klass)
    {
      g_warning ("invalid unclassed pointer in cast to `%s'",
		 gtk_object_descriptive_type_name (cast_type));
      return obj;
    }
  if (obj->klass->type < GTK_TYPE_OBJECT)
    {
      g_warning ("invalid class type `%s' in cast to `%s'",
		 gtk_object_descriptive_type_name (obj->klass->type),
		 gtk_object_descriptive_type_name (cast_type));
      return obj;
    }
  if (!gtk_type_is_a (obj->klass->type, cast_type))
    {
      g_warning ("invalid cast from `%s' to `%s'",
		 gtk_object_descriptive_type_name (obj->klass->type),
		 gtk_object_descriptive_type_name (cast_type));
      return obj;
    }
  
  return obj;
}

GtkObjectClass*
gtk_object_check_class_cast (GtkObjectClass *klass,
			     GtkType         cast_type)
{
  if (!klass)
    {
      g_warning ("invalid class cast from (NULL) pointer to `%s'",
		 gtk_object_descriptive_type_name (cast_type));
      return klass;
    }
  if (klass->type < GTK_TYPE_OBJECT)
    {
      g_warning ("invalid class type `%s' in class cast to `%s'",
		 gtk_object_descriptive_type_name (klass->type),
		 gtk_object_descriptive_type_name (cast_type));
      return klass;
    }
  if (!gtk_type_is_a (klass->type, cast_type))
    {
      g_warning ("invalid class cast from `%s' to `%s'",
		 gtk_object_descriptive_type_name (klass->type),
		 gtk_object_descriptive_type_name (cast_type));
      return klass;
    }

  return klass;
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
  g_return_if_fail (data_id > 0);
  
  gtk_object_set_data_by_id_full (object, data_id, data, NULL);
}

void
gtk_object_set_data (GtkObject        *object,
		     const gchar      *key,
		     gpointer          data)
{
  g_return_if_fail (key != NULL);
  
  gtk_object_set_data_by_id_full (object, gtk_object_data_force_id (key), data, NULL);
}

void
gtk_object_set_data_by_id_full (GtkObject        *object,
				GQuark		  data_id,
				gpointer          data,
				GtkDestroyNotify  destroy)
{
  GtkObjectData *odata;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (data_id > 0);

  odata = object->object_data;
  if (!data)
    {
      GtkObjectData *prev;
      
      prev = NULL;
      
      while (odata)
	{
	  if (odata->id == data_id)
	    {
	      if (prev)
		prev->next = odata->next;
	      else
		object->object_data = odata->next;
	      
	      GTK_OBJECT_DATA_DESTROY (odata);
	      return;
	    }
	  
	  prev = odata;
	  odata = odata->next;
	}
    }
  else
    {
      while (odata)
	{
	  if (odata->id == data_id)
	    {
              register GtkDestroyNotify dfunc;
	      register gpointer ddata;

	      dfunc = odata->destroy;
	      ddata = odata->data;
	      odata->destroy = destroy;
	      odata->data = data;

	      /* we need to have updated all structures prior to
	       * invokation of the destroy function
	       */
	      if (dfunc)
		dfunc (ddata);

	      return;
	    }

	  odata = odata->next;
	}
      
      if (gtk_object_data_free_list)
	{
	  odata = gtk_object_data_free_list;
	  gtk_object_data_free_list = odata->next;
	}
      else
	{
	  GtkObjectData *odata_block;
	  guint i;

	  odata_block = g_new0 (GtkObjectData, GTK_OBJECT_DATA_BLOCK_SIZE);
	  for (i = 1; i < GTK_OBJECT_DATA_BLOCK_SIZE; i++)
	    {
	      (odata_block + i)->next = gtk_object_data_free_list;
	      gtk_object_data_free_list = (odata_block + i);
	    }

	  odata = odata_block;
	}
      
      odata->id = data_id;
      odata->data = data;
      odata->destroy = destroy;
      odata->next = object->object_data;
      
      object->object_data = odata;
    }
}

void
gtk_object_set_data_full (GtkObject        *object,
			  const gchar      *key,
			  gpointer          data,
			  GtkDestroyNotify  destroy)
{
  g_return_if_fail (key != NULL);

  gtk_object_set_data_by_id_full (object, gtk_object_data_force_id (key), data, destroy);
}

gpointer
gtk_object_get_data_by_id (GtkObject   *object,
			   GQuark       data_id)
{
  GtkObjectData *odata;

  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);

  if (data_id)
    {
      odata = object->object_data;
      while (odata)
	{
	  if (odata->id == data_id)
	    return odata->data;
	  odata = odata->next;
	}
    }
  
  return NULL;
}

gpointer
gtk_object_get_data (GtkObject   *object,
		     const gchar *key)
{
  guint id;

  g_return_val_if_fail (key != NULL, NULL);

  id = gtk_object_data_try_key (key);
  if (id)
    return gtk_object_get_data_by_id (object, id);

  return NULL;
}

void
gtk_object_remove_data_by_id (GtkObject   *object,
			      GQuark       data_id)
{
  if (data_id)
    gtk_object_set_data_by_id_full (object, data_id, NULL, NULL);
}

void
gtk_object_remove_data (GtkObject   *object,
			const gchar *key)
{
  gint id;

  g_return_if_fail (key != NULL);

  id = gtk_object_data_try_key (key);
  if (id)
    gtk_object_set_data_by_id_full (object, id, NULL, NULL);
}

void
gtk_object_set_user_data (GtkObject *object,
			  gpointer   data)
{
  if (!user_data_key_id)
    user_data_key_id = g_quark_from_static_string (user_data_key);

  gtk_object_set_data_by_id_full (object, user_data_key_id, data, NULL);
}

gpointer
gtk_object_get_user_data (GtkObject *object)
{
  if (user_data_key_id)
    return gtk_object_get_data_by_id (object, user_data_key_id);

  return NULL;
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

  object->ref_count += 1;
}

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
