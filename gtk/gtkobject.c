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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
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


extern void	  gtk_object_init_type           (void);	/* for gtktypeutils.h */
static void       gtk_object_base_class_init     (GtkObjectClass *class);
static void       gtk_object_base_class_finalize (GtkObjectClass *class);
static void       gtk_object_class_init          (GtkObjectClass *klass);
static void       gtk_object_init                (GtkObject      *object,
						  GtkObjectClass *klass);
static void       gtk_object_set_arg             (GtkObject      *object,
						  GtkArg         *arg,
						  guint		  arg_id);
static void       gtk_object_get_arg             (GtkObject      *object,
						  GtkArg         *arg,
						  guint		  arg_id);
static void       gtk_object_shutdown            (GObject        *object);
static void       gtk_object_real_destroy        (GtkObject      *object);
static void       gtk_object_finalize            (GObject        *object);
static void       gtk_object_notify_weaks        (GtkObject      *object);

static gpointer    parent_class = NULL;
static guint       object_signals[LAST_SIGNAL] = { 0 };
static GHashTable *object_arg_info_ht = NULL;
static GQuark      quark_user_data = 0;
static GQuark      quark_weakrefs = 0;
static GQuark      quark_carg_history = 0;


/****************************************************
 * GtkObject type, class and instance initialization
 *
 ****************************************************/

GtkType
gtk_object_get_type (void)
{
  static GtkType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
	sizeof (GtkObjectClass),
	(GBaseInitFunc) gtk_object_base_class_init,
	(GBaseFinalizeFunc) gtk_object_base_class_finalize,
	(GClassInitFunc) gtk_object_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkObject),
	16,			/* n_preallocs */
	(GInstanceInitFunc) gtk_object_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT, "GtkObject", &object_info, 0);
    }

  return object_type;
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
gtk_object_base_class_finalize (GtkObjectClass *class)
{
  g_free (class->signals);
  g_return_if_fail (class->construct_args == NULL);
}

static void
gtk_object_class_init (GtkObjectClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_ref (G_TYPE_OBJECT);

  gobject_class->shutdown = gtk_object_shutdown;
  gobject_class->finalize = gtk_object_finalize;

  class->get_arg = gtk_object_get_arg;
  class->set_arg = gtk_object_set_arg;
  class->destroy = gtk_object_real_destroy;

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
                    G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | GTK_RUN_NO_HOOKS,
                    GTK_CLASS_TYPE (class),
                    GTK_SIGNAL_OFFSET (GtkObjectClass, destroy),
                    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (class, object_signals, LAST_SIGNAL);
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
      klass = g_type_class_peek_parent (klass);
    }
  while (klass && GTK_IS_OBJECT_CLASS (klass) && !needs_construction);
  if (!needs_construction)
    GTK_OBJECT_FLAGS (object) |= GTK_CONSTRUCTED;
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
      /* need to hold a reference count around all class method
       * invocations.
       */
      gtk_object_ref (object);
      G_OBJECT_GET_CLASS (object)->shutdown (G_OBJECT (object));
      gtk_object_unref (object);
    }
}

static void
gtk_object_shutdown (GObject *gobject)
{
  GtkObject *object = GTK_OBJECT (gobject);

  /* guard against reinvocations during
   * destruction with the GTK_DESTROYED flag.
   */
  if (!GTK_OBJECT_DESTROYED (object))
    {
      GTK_OBJECT_SET_FLAGS (object, GTK_DESTROYED);
      
      gtk_signal_emit (object, object_signals[DESTROY]);
      
      GTK_OBJECT_UNSET_FLAGS (object, GTK_DESTROYED);
    }

  G_OBJECT_CLASS (parent_class)->shutdown (gobject);
}

static void
gtk_object_real_destroy (GtkObject *object)
{
  g_signal_handlers_destroy (G_OBJECT (object));
}

static void
gtk_object_finalize (GObject *gobject)
{
  GtkObject *object = GTK_OBJECT (gobject);

  gtk_object_notify_weaks (object);
  
  G_OBJECT_CLASS (parent_class)->finalize (gobject);
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
			       GTK_CLASS_TYPE (class),
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
			       GTK_CLASS_TYPE (class),
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

  g_return_val_if_fail (GTK_TYPE_IS_OBJECT (object_type), NULL);

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
  
  g_return_val_if_fail (GTK_TYPE_IS_OBJECT (object_type), NULL);
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
      for (slist = GTK_OBJECT_GET_CLASS (object)->construct_args;
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
	      switch (G_TYPE_FUNDAMENTAL (arg.type))
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
		case G_TYPE_OBJECT:
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
  g_return_val_if_fail (GTK_TYPE_IS_OBJECT (class_type), NULL);

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
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  g_datalist_id_set_data (&G_OBJECT (object)->qdata, data_id, data);
}

void
gtk_object_set_data (GtkObject        *object,
		     const gchar      *key,
		     gpointer          data)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);
  
  g_datalist_set_data (&G_OBJECT (object)->qdata, key, data);
}

void
gtk_object_set_data_by_id_full (GtkObject        *object,
				GQuark		  data_id,
				gpointer          data,
				GtkDestroyNotify  destroy)
{
  g_return_if_fail (GTK_IS_OBJECT (object));

  g_datalist_id_set_data_full (&G_OBJECT (object)->qdata, data_id, data, destroy);
}

void
gtk_object_set_data_full (GtkObject        *object,
			  const gchar      *key,
			  gpointer          data,
			  GtkDestroyNotify  destroy)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);

  g_datalist_set_data_full (&G_OBJECT (object)->qdata, key, data, destroy);
}

gpointer
gtk_object_get_data_by_id (GtkObject   *object,
			   GQuark       data_id)
{
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);

  return g_datalist_id_get_data (&G_OBJECT (object)->qdata, data_id);
}

gpointer
gtk_object_get_data (GtkObject   *object,
		     const gchar *key)
{
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_datalist_get_data (&G_OBJECT (object)->qdata, key);
}

void
gtk_object_remove_data_by_id (GtkObject   *object,
			      GQuark       data_id)
{
  g_return_if_fail (GTK_IS_OBJECT (object));

  g_datalist_id_remove_data (&G_OBJECT (object)->qdata, data_id);
}

void
gtk_object_remove_data (GtkObject   *object,
			const gchar *key)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);

  g_datalist_remove_data (&G_OBJECT (object)->qdata, key);
}

void
gtk_object_remove_no_notify_by_id (GtkObject      *object,
				   GQuark          key_id)
{
  g_return_if_fail (GTK_IS_OBJECT (object));

  g_datalist_id_remove_no_notify (&G_OBJECT (object)->qdata, key_id);
}

void
gtk_object_remove_no_notify (GtkObject       *object,
			     const gchar     *key)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);

  g_datalist_remove_no_notify (&G_OBJECT (object)->qdata, key);
}

void
gtk_object_set_user_data (GtkObject *object,
			  gpointer   data)
{
  g_return_if_fail (GTK_IS_OBJECT (object));

  if (!quark_user_data)
    quark_user_data = g_quark_from_static_string ("user_data");

  g_datalist_id_set_data (&G_OBJECT (object)->qdata, quark_user_data, data);
}

gpointer
gtk_object_get_user_data (GtkObject *object)
{
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);

  return g_datalist_id_get_data (&G_OBJECT (object)->qdata, quark_user_data);
}

GtkObject*
gtk_object_ref (GtkObject *object)
{
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);

  return (GtkObject*) g_object_ref ((GObject*) object);
}

void
gtk_object_unref (GtkObject *object)
{
  g_return_if_fail (GTK_IS_OBJECT (object));

  g_object_unref ((GObject*) object);
}
