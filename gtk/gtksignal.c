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
#include "gtksignal.h"


#define MAX_PARAMS       20
#define DONE             1
#define RESTART          2

#define GTK_RUN_TYPE(x)  ((x) & GTK_RUN_MASK)


typedef struct _GtkSignal       GtkSignal;
typedef struct _GtkSignalInfo   GtkSignalInfo;
typedef struct _GtkHandler      GtkHandler;
typedef struct _GtkHandlerInfo  GtkHandlerInfo;
typedef struct _GtkEmission     GtkEmission;

typedef void (*GtkSignalMarshaller0) (GtkObject *object,
				      gpointer   data);

struct _GtkSignalInfo
{
  gchar *name;
  gint object_type;
  gint signal_type;
};

struct _GtkSignal
{
  GtkSignalInfo info;
  gint function_offset;
  GtkSignalRunType run_type;
  GtkSignalMarshaller marshaller;
  GtkType return_val;
  GtkType *params;
  gint nparams;
};

struct _GtkHandler
{
  guint16 id;
  guint signal_type : 13;
  guint object_signal : 1;
  guint blocked : 1;
  guint after : 1;
  guint no_marshal : 1;
  GtkSignalFunc func;
  gpointer func_data;
  GtkSignalDestroy destroy_func;
  GtkHandler *next;
};

struct _GtkHandlerInfo
{
  GtkObject *object;
  GtkSignalMarshaller marshaller;
  GtkArg *params;
  GtkType *param_types;
  GtkType return_val;
  GtkSignalRunType run_type;
  gint nparams;
  gint signal_type;
};

struct _GtkEmission
{
  GtkObject *object;
  gint signal_type;
};


static void         gtk_signal_init            (void);
static guint        gtk_signal_hash            (gint          *key);
static gint         gtk_signal_compare         (gint          *a,
						gint          *b);
static guint        gtk_signal_info_hash       (GtkSignalInfo *a);
static gint         gtk_signal_info_compare    (GtkSignalInfo *a,
						GtkSignalInfo *b);
static GtkHandler*  gtk_signal_handler_new     (void);
static void         gtk_signal_handler_destroy (GtkHandler    *handler);
static void         gtk_signal_handler_insert  (GtkObject     *object,
						GtkHandler    *handler);
static gint         gtk_signal_real_emit       (GtkObject     *object,
						gint           signal_type,
						va_list        args);
static GtkHandler*  gtk_signal_get_handlers    (GtkObject     *object,
						gint           signal_type);
static gint         gtk_signal_connect_by_type (GtkObject     *object,
						gint           signal_type,
						gint           object_signal,
						GtkSignalFunc  func,
						gpointer       func_data,
						GtkSignalDestroy destroy_func,
						gint           after,
						gint           no_marshal);
static GtkEmission* gtk_emission_new           (void);
static void         gtk_emission_destroy       (GtkEmission    *emission);
static void         gtk_emission_add           (GList         **emissions,
						GtkObject      *object,
						gint            signal_type);
static void         gtk_emission_remove        (GList         **emissions,
						GtkObject      *object,
						gint            signal_type);
static gint         gtk_emission_check         (GList          *emissions,
						GtkObject      *object,
						gint            signal_type);
static gint         gtk_handlers_run           (GtkHandler     *handlers,
						GtkHandlerInfo *info,
						gint            after);
static void         gtk_params_get             (GtkArg         *params,
						gint            nparams,
						GtkType        *param_types,
						GtkType         return_val,
						va_list         args);


static gint initialize = TRUE;
static GHashTable *signal_hash_table = NULL;
static GHashTable *signal_info_hash_table = NULL;
static gint next_signal = 1;
static gint next_handler_id = 1;

static const char *handler_key = "signal_handlers";

static GMemChunk *handler_mem_chunk = NULL;
static GMemChunk *emission_mem_chunk = NULL;

static GList *current_emissions = NULL;
static GList *stop_emissions = NULL;
static GList *restart_emissions = NULL;

static GtkSignalMarshal marshal = NULL;
static GtkSignalDestroy destroy = NULL;


gint
gtk_signal_new (const gchar         *name,
		GtkSignalRunType     run_type,
		gint                 object_type,
		gint                 function_offset,
		GtkSignalMarshaller  marshaller,
		GtkType              return_val,
		gint                 nparams,
		...)
{
  GtkType *params;
  GtkSignal *signal;
  GtkSignalInfo info;
  gint *type;
  gint i;
  va_list args;

  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (marshaller != NULL, 0);
  g_return_val_if_fail (nparams < 10, 0);

  if (initialize)
    gtk_signal_init ();

  info.name = (char*)name;
  info.object_type = object_type;

  type = g_hash_table_lookup (signal_info_hash_table, &info);
  if (type)
    {
      g_warning ("signal \"%s\" already exists in the \"%s\" class ancestry\n",
		 name, gtk_type_name (object_type));
      return 0;
    }

  signal = g_new (GtkSignal, 1);
  signal->info.name = g_strdup(name);
  signal->info.object_type = object_type;
  signal->info.signal_type = next_signal++;
  signal->function_offset = function_offset;
  signal->run_type = run_type;
  signal->marshaller = marshaller;
  signal->return_val = return_val;
  signal->params = NULL;
  signal->nparams = nparams;

  g_hash_table_insert (signal_hash_table, &signal->info.signal_type, signal);
  g_hash_table_insert (signal_info_hash_table, &signal->info, &signal->info.signal_type);

  if (nparams > 0)
    {
      signal->params = g_new (GtkType, nparams);
      params = signal->params;

      va_start (args, nparams);

      for (i = 0; i < nparams; i++)
	params[i] = va_arg (args, GtkType);

      va_end (args);
    }

  return signal->info.signal_type;
}

gint
gtk_signal_lookup (const gchar *name,
		   gint         object_type)
{
  GtkSignalInfo info;
  gint *type;

  g_return_val_if_fail (name != NULL, 0);

  if (initialize)
    gtk_signal_init ();

  info.name = (char*)name;

  while (object_type)
    {
      info.object_type = object_type;

      type = g_hash_table_lookup (signal_info_hash_table, &info);
      if (type)
	return *type;

      object_type = gtk_type_parent (object_type);
    }

  return 0;
}

gchar*
gtk_signal_name (gint signal_num)
{
  GtkSignal *signal;

  signal = g_hash_table_lookup (signal_hash_table, &signal_num);
  if (signal)
    return signal->info.name;

  return NULL;
}

gint
gtk_signal_emit (GtkObject *object,
		 gint       signal_type,
		 ...)
{
  gint return_val;

  va_list args;

  g_return_val_if_fail (object != NULL, FALSE);

  if (initialize)
    gtk_signal_init ();

  va_start (args, signal_type);

  return_val = gtk_signal_real_emit (object, signal_type, args);

  va_end (args);

  return return_val;
}

gint
gtk_signal_emit_by_name (GtkObject       *object,
			 const gchar     *name,
			 ...)
{
  gint return_val;
  gint type;
  va_list args;

  g_return_val_if_fail (object != NULL, FALSE);

  if (initialize)
    gtk_signal_init ();

  return_val = TRUE;
  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));

  if (type)
    {
      va_start (args, name);

      return_val = gtk_signal_real_emit (object, type, args);

      va_end (args);
    }

  return return_val;
}

void
gtk_signal_emit_stop (GtkObject *object,
		      gint       signal_type)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (signal_type >= 1);

  if (initialize)
    gtk_signal_init ();

  if (gtk_emission_check (current_emissions, object, signal_type))
    gtk_emission_add (&stop_emissions, object, signal_type);
}

void
gtk_signal_emit_stop_by_name (GtkObject       *object,
			      const gchar     *name)
{
  gint type;

  g_return_if_fail (object != NULL);
  g_return_if_fail (name != NULL);

  if (initialize)
    gtk_signal_init ();

  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (type)
    gtk_signal_emit_stop (object, type);
}

gint
gtk_signal_connect (GtkObject     *object,
		    const gchar   *name,
		    GtkSignalFunc  func,
		    gpointer       func_data)
{
  gint type;

  g_return_val_if_fail (object != NULL, 0);

  if (initialize)
    gtk_signal_init ();

  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!type)
    {
      g_warning ("could not find signal type \"%s\" in the \"%s\" class ancestry",
		 name, gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }

  return gtk_signal_connect_by_type (object, type, FALSE,
				     func, func_data, NULL,
				     FALSE, FALSE);
}

gint
gtk_signal_connect_after (GtkObject     *object,
			  const gchar   *name,
			  GtkSignalFunc  func,
			  gpointer       func_data)
{
  gint type;

  g_return_val_if_fail (object != NULL, 0);

  if (initialize)
    gtk_signal_init ();

  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!type)
    {
      g_warning ("could not find signal type \"%s\" in the \"%s\" class ancestry",
		 name, gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }

  return gtk_signal_connect_by_type (object, type, FALSE,
				     func, func_data, NULL,
				     TRUE, FALSE);
}

gint
gtk_signal_connect_interp (GtkObject         *object,
			   gchar             *name,
			   GtkCallbackMarshal func,
			   gpointer           func_data,
			   GtkDestroyNotify   destroy_func,
			   gint               after)
{
  gint type;

  g_return_val_if_fail (object != NULL, 0);

  if (initialize)
    gtk_signal_init ();

  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!type)
    {
      g_warning ("could not find signal type \"%s\" in the \"%s\" class ancestry",
		 name, gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }

  return gtk_signal_connect_by_type (object, type, FALSE,
				     (GtkSignalFunc) func, func_data,
				     destroy_func, after, TRUE);
}

gint
gtk_signal_connect_object (GtkObject     *object,
			   const gchar   *name,
			   GtkSignalFunc  func,
			   GtkObject     *slot_object)
{
  gint type;

  g_return_val_if_fail (object != NULL, 0);
  /* slot_object needs to be treated as ordinary pointer */

  if (initialize)
    gtk_signal_init ();

  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!type)
    {
      g_warning ("could not find signal type \"%s\" in the \"%s\" class ancestry",
		 name, gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }

  return gtk_signal_connect_by_type (object, type, TRUE,
				     func, slot_object, NULL,
				     FALSE, FALSE);
}

gint
gtk_signal_connect_object_after (GtkObject     *object,
				 const gchar   *name,
				 GtkSignalFunc  func,
				 GtkObject     *slot_object)
{
  gint type;

  g_return_val_if_fail (object != NULL, 0);

  if (initialize)
    gtk_signal_init ();

  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!type)
    {
      g_warning ("could not find signal type \"%s\" in the \"%s\" class ancestry",
		 name, gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }

  return gtk_signal_connect_by_type (object, type, TRUE,
				     func, slot_object, NULL,
				     TRUE, FALSE);
}

void
gtk_signal_disconnect (GtkObject *object,
		       gint       anid)
{
  GtkHandler *tmp;
  GtkHandler *prev;

  g_return_if_fail (object != NULL);

  if (initialize)
    gtk_signal_init ();

  prev = NULL;
  tmp = gtk_object_get_data (object, handler_key);

  while (tmp)
    {
      if (tmp->id == anid)
	{
	  if (prev)
	    prev->next = tmp->next;
	  else
	    gtk_object_set_data (object, handler_key, tmp->next);
	  gtk_signal_handler_destroy (tmp);
	  return;
	}

      prev = tmp;
      tmp = tmp->next;
    }

  g_warning ("could not find handler (%d)", anid);
}

void
gtk_signal_disconnect_by_data (GtkObject *object,
			       gpointer   data)
{
  GtkHandler *start;
  GtkHandler *tmp;
  GtkHandler *prev;
  gint found_one;

  g_return_if_fail (object != NULL);

  if (initialize)
    gtk_signal_init ();

  prev = NULL;
  start = gtk_object_get_data (object, handler_key);
  tmp = start;
  found_one = FALSE;

  while (tmp)
    {
      if (tmp->func_data == data)
	{
	  found_one = TRUE;

	  if (prev)
	    prev->next = tmp->next;
	  else
	    start = tmp->next;

	  gtk_signal_handler_destroy (tmp);

	  if (prev)
	    {
	      tmp = prev->next;
	    }
	  else
	    {
	      prev = NULL;
	      tmp = start;
	    }
	}
      else
	{
	  prev = tmp;
	  tmp = tmp->next;
	}
    }

  gtk_object_set_data (object, handler_key, start);

  if (!found_one)
    g_warning ("could not find handler containing data (0x%0lX)", (long) data);
}

void
gtk_signal_handler_block (GtkObject *object,
			  gint      anid)
{
  GtkHandler *tmp;

  g_return_if_fail (object != NULL);

  if (initialize)
    gtk_signal_init ();

  tmp = gtk_object_get_data (object, handler_key);

  while (tmp)
    {
      if (tmp->id == anid)
	{
	  tmp->blocked = TRUE;
	  return;
	}

      tmp = tmp->next;
    }

  g_warning ("could not find handler (%d)", anid);
}

void
gtk_signal_handler_block_by_data (GtkObject *object,
				  gpointer   data)
{
  GtkHandler *tmp;
  gint found_one;

  g_return_if_fail (object != NULL);

  if (initialize)
    gtk_signal_init ();

  found_one = FALSE;
  tmp = gtk_object_get_data (object, handler_key);

  while (tmp)
    {
      if (tmp->func_data == data)
	{
	  tmp->blocked = TRUE;
	  found_one = TRUE;
	}

      tmp = tmp->next;
    }

  if (!found_one)
    g_warning ("could not find handler containing data (0x%0lX)", (long) data);
}

void
gtk_signal_handler_unblock (GtkObject *object,
			    gint       anid)
{
  GtkHandler *tmp;

  g_return_if_fail (object != NULL);

  if (initialize)
    gtk_signal_init ();

  tmp = gtk_object_get_data (object, handler_key);

  while (tmp)
    {
      if (tmp->id == anid)
	{
	  tmp->blocked = FALSE;
	  return;
	}

      tmp = tmp->next;
    }

  g_warning ("could not find handler (%d)", anid);
}

void
gtk_signal_handler_unblock_by_data (GtkObject *object,
				    gpointer   data)
{
  GtkHandler *tmp;
  gint found_one;

  g_return_if_fail (object != NULL);

  if (initialize)
    gtk_signal_init ();

  found_one = FALSE;
  tmp = gtk_object_get_data (object, handler_key);

  while (tmp)
    {
      if (tmp->func_data == data)
	{
	  tmp->blocked = FALSE;
	  found_one = TRUE;
	}

      tmp = tmp->next;
    }

  if (!found_one)
    g_warning ("could not find handler containing data (0x%0lX)", (long) data);
}

void
gtk_signal_handlers_destroy (GtkObject *object)
{
  GtkHandler *list;
  GtkHandler *handler;

  list = gtk_object_get_data (object, handler_key);
  if (list)
    {
      while (list)
	{
	  handler = list->next;
	  gtk_signal_handler_destroy (list);
	  list = handler;
	}

      gtk_object_remove_data (object, handler_key);
    }
}

void
gtk_signal_default_marshaller (GtkObject      *object,
			       GtkSignalFunc   func,
			       gpointer        func_data,
			       GtkArg         *params)
{
  GtkSignalMarshaller0 rfunc;

  rfunc = (GtkSignalMarshaller0) func;

  (* rfunc) (object, func_data);
}

void
gtk_signal_set_funcs (GtkSignalMarshal marshal_func,
		      GtkSignalDestroy destroy_func)
{
  marshal = marshal_func;
  destroy = destroy_func;
}


static void
gtk_signal_init ()
{
  if (initialize)
    {
      initialize = FALSE;
      signal_hash_table = g_hash_table_new ((GHashFunc) gtk_signal_hash,
					    (GCompareFunc) gtk_signal_compare);
      signal_info_hash_table = g_hash_table_new ((GHashFunc) gtk_signal_info_hash,
						 (GCompareFunc) gtk_signal_info_compare);
    }
}

static guint
gtk_signal_hash (gint *key)
{
  return (guint) *key;
}

static gint
gtk_signal_compare (gint *a,
		    gint *b)
{
  return (*a == *b);
}

static guint
gtk_signal_info_hash (GtkSignalInfo *a)
{
  return (g_str_hash (a->name) + a->object_type);
}

static gint
gtk_signal_info_compare (GtkSignalInfo *a,
			 GtkSignalInfo *b)
{
  return ((a->object_type == b->object_type) &&
	  g_str_equal (a->name, b->name));
}

static GtkHandler*
gtk_signal_handler_new ()
{
  GtkHandler *handler;

  if (!handler_mem_chunk)
    handler_mem_chunk = g_mem_chunk_new ("handler mem chunk", sizeof (GtkHandler),
					 1024, G_ALLOC_AND_FREE);

  handler = g_chunk_new (GtkHandler, handler_mem_chunk);

  handler->id = 0;
  handler->signal_type = 0;
  handler->object_signal = FALSE;
  handler->blocked = FALSE;
  handler->after = FALSE;
  handler->no_marshal = FALSE;
  handler->func = NULL;
  handler->func_data = NULL;
  handler->destroy_func = NULL;
  handler->next = NULL;

  return handler;
}

static void
gtk_signal_handler_destroy (GtkHandler *handler)
{
  if (!handler->func && destroy)
    (* destroy) (handler->func_data);
  else if (handler->destroy_func)
    (* handler->destroy_func) (handler->func_data);
  g_mem_chunk_free (handler_mem_chunk, handler);
}

static void
gtk_signal_handler_insert (GtkObject  *object,
			   GtkHandler *handler)
{
  GtkHandler *start;
  GtkHandler *tmp;
  GtkHandler *prev;

  start = gtk_object_get_data (object, handler_key);
  if (!start)
    {
      gtk_object_set_data (object, handler_key, handler);
    }
  else
    {
      prev = NULL;
      tmp = start;

      while (tmp)
	{
	  if (tmp->signal_type < handler->signal_type)
	    {
	      if (prev)
		prev->next = handler;
	      else
		gtk_object_set_data (object, handler_key, handler);
	      handler->next = tmp;
	      break;
	    }

	  if (!tmp->next)
	    {
	      tmp->next = handler;
	      break;
	    }

	  prev = tmp;
	  tmp = tmp->next;
	}
    }
}

static gint
gtk_signal_real_emit (GtkObject *object,
		      gint       signal_type,
		      va_list    args)
{
  gint old_value;
  GtkSignal *signal;
  GtkHandler *handlers;
  GtkHandlerInfo info;
  guchar **signal_func_offset;
  gint being_destroyed;
  GtkArg         params[MAX_PARAMS];

  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (signal_type >= 1, TRUE);

  being_destroyed = GTK_OBJECT_BEING_DESTROYED (object);
  if (!GTK_OBJECT_NEED_DESTROY (object))
    {
      signal = g_hash_table_lookup (signal_hash_table, &signal_type);
      g_return_val_if_fail (signal != NULL, TRUE);
      g_return_val_if_fail (gtk_type_is_a (GTK_OBJECT_TYPE (object), signal->info.object_type), TRUE);

      if ((signal->run_type & GTK_RUN_NO_RECURSE) &&
	  gtk_emission_check (current_emissions, object, signal_type))
	{
	  gtk_emission_add (&restart_emissions, object, signal_type);
	  return TRUE;
	}

      old_value = GTK_OBJECT_IN_CALL (object);
      GTK_OBJECT_SET_FLAGS (object, GTK_IN_CALL);

      gtk_params_get (params, signal->nparams, signal->params, signal->return_val, args);

      gtk_emission_add (&current_emissions, object, signal_type);

    restart:
      if (GTK_RUN_TYPE (signal->run_type) != GTK_RUN_LAST)
	{
	  signal_func_offset = (guchar**) ((guchar*) object->klass + signal->function_offset);
	  if (*signal_func_offset)
	    (* signal->marshaller) (object, (GtkSignalFunc) *signal_func_offset, NULL, params);
	  if (GTK_OBJECT_NEED_DESTROY (object))
	    goto done;
	}

      info.object = object;
      info.marshaller = signal->marshaller;
      info.params = params;
      info.param_types = signal->params;
      info.return_val = signal->return_val;
      info.nparams = signal->nparams;
      info.run_type = signal->run_type;
      info.signal_type = signal_type;

      handlers = gtk_signal_get_handlers (object, signal_type);
      switch (gtk_handlers_run (handlers, &info, FALSE))
	{
	case DONE:
	  goto done;
	case RESTART:
	  goto restart;
	}

      if (GTK_RUN_TYPE (signal->run_type) != GTK_RUN_FIRST)
	{
	  signal_func_offset = (guchar**) ((guchar*) object->klass + signal->function_offset);
	  if (*signal_func_offset)
	    (* signal->marshaller) (object, (GtkSignalFunc) *signal_func_offset, NULL, params);
	  if (being_destroyed || GTK_OBJECT_NEED_DESTROY (object))
	    goto done;
	}

      switch (gtk_handlers_run (handlers, &info, TRUE))
	{
	case DONE:
	  goto done;
	case RESTART:
	  goto restart;
	}

    done:
      gtk_emission_remove (&current_emissions, object, signal_type);

      if (signal->run_type & GTK_RUN_NO_RECURSE)
	gtk_emission_remove (&restart_emissions, object, signal_type);

      if (!being_destroyed && !old_value)
	GTK_OBJECT_UNSET_FLAGS (object, GTK_IN_CALL);
    }

  if (!being_destroyed && GTK_OBJECT_NEED_DESTROY (object) && !GTK_OBJECT_IN_CALL (object))
    {
      gtk_object_destroy (object);
      return FALSE;
    }

  return TRUE;
}

static GtkHandler*
gtk_signal_get_handlers (GtkObject *object,
			 gint       signal_type)
{
  GtkHandler *handlers;

  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (signal_type >= 1, NULL);

  handlers = gtk_object_get_data (object, handler_key);

  while (handlers)
    {
      if (handlers->signal_type == signal_type)
	return handlers;
      handlers = handlers->next;
    }

  return NULL;
}

static gint
gtk_signal_connect_by_type (GtkObject       *object,
			    gint             signal_type,
			    gint             object_signal,
			    GtkSignalFunc    func,
			    gpointer         func_data,
			    GtkSignalDestroy destroy_func,
			    gint             after,
			    gint             no_marshal)
{
  GtkHandler *handler;
  gint *object_signals;
  gint nsignals;
  gint found_it;
  gint i;

  g_return_val_if_fail (object != NULL, 0);
  g_return_val_if_fail (object->klass != NULL, 0);
  g_return_val_if_fail (object->klass->signals != NULL, 0);

  /* Search through the signals for this object and make
   *  sure the one we are adding is valid. If it isn't then
   *  issue a warning and return.
   */
  object_signals = object->klass->signals;
  nsignals = object->klass->nsignals;
  found_it = FALSE;

  for (i = 0; i < nsignals; i++)
    if (object_signals[i] == signal_type)
      {
	found_it = TRUE;
	break;
      }

  if (!found_it)
    {
      g_warning ("could not find signal (%d) in object's list of signals", signal_type);
      return 0;
    }

  handler = gtk_signal_handler_new ();
  handler->id = next_handler_id++;
  handler->signal_type = signal_type;
  handler->object_signal = object_signal;
  handler->func = func;
  handler->func_data = func_data;
  handler->destroy_func = destroy_func;

  if (after)
    handler->after = TRUE;
  handler->no_marshal = no_marshal;

  gtk_signal_handler_insert (object, handler);
  return handler->id;
}

static GtkEmission*
gtk_emission_new ()
{
  GtkEmission *emission;

  if (!emission_mem_chunk)
    emission_mem_chunk = g_mem_chunk_new ("emission mem chunk", sizeof (GtkEmission),
					  1024, G_ALLOC_AND_FREE);

  emission = g_chunk_new (GtkEmission, emission_mem_chunk);

  emission->object = NULL;
  emission->signal_type = 0;

  return emission;
}

static void
gtk_emission_destroy (GtkEmission *emission)
{
  g_mem_chunk_free (emission_mem_chunk, emission);
}

static void
gtk_emission_add (GList     **emissions,
		  GtkObject  *object,
		  gint        signal_type)
{
  GtkEmission *emission;

  g_return_if_fail (emissions != NULL);
  g_return_if_fail (object != NULL);

  emission = gtk_emission_new ();
  emission->object = object;
  emission->signal_type = signal_type;

  *emissions = g_list_prepend (*emissions, emission);
}

static void
gtk_emission_remove (GList     **emissions,
		     GtkObject  *object,
		     gint        signal_type)
{
  GtkEmission *emission;
  GList *tmp;

  g_return_if_fail (emissions != NULL);
  g_return_if_fail (object != NULL);

  tmp = *emissions;
  while (tmp)
    {
      emission = tmp->data;

      if ((emission->object == object) &&
	  (emission->signal_type == signal_type))
	{
	  gtk_emission_destroy (emission);
	  *emissions = g_list_remove_link (*emissions, tmp);
	  g_list_free (tmp);
	  break;
	}

      tmp = tmp->next;
    }
}

static gint
gtk_emission_check (GList     *emissions,
		    GtkObject *object,
		    gint       signal_type)
{
  GtkEmission *emission;
  GList *tmp;

  g_return_val_if_fail (object != NULL, FALSE);

  tmp = emissions;
  while (tmp)
    {
      emission = tmp->data;
      tmp = tmp->next;

      if ((emission->object == object) &&
	  (emission->signal_type == signal_type))
	return TRUE;
    }
  return FALSE;
}

static gint
gtk_handlers_run (GtkHandler     *handlers,
		  GtkHandlerInfo *info,
		  gint            after)
{
  while (handlers && (handlers->signal_type == info->signal_type))
    {
      if (!handlers->blocked && (handlers->after == after))
	{
	  if (handlers->func)
	    {
	      if (handlers->no_marshal)
		(* (GtkCallbackMarshal)handlers->func) (info->object,
							handlers->func_data,
							info->nparams,
							info->params);
	      else if (handlers->object_signal)
		(* info->marshaller) ((GtkObject*) handlers->func_data, /* don't GTK_OBJECT() cast */
				      handlers->func,
				      handlers->func_data,
				      info->params);
	      else
		(* info->marshaller) (info->object,
				      handlers->func,
				      handlers->func_data,
				      info->params);
	    }
	  else if (marshal)
	    (* marshal) (info->object,
			 handlers->func_data,
			 info->nparams,
			 info->params,
			 info->param_types,
			 info->return_val);

	  if (GTK_OBJECT_NEED_DESTROY (info->object))
	    return DONE;
	  else if (gtk_emission_check (stop_emissions, info->object, info->signal_type))
	    {
	      gtk_emission_remove (&stop_emissions, info->object, info->signal_type);

	      if (info->run_type & GTK_RUN_NO_RECURSE)
		gtk_emission_remove (&restart_emissions, info->object, info->signal_type);
	      return DONE;
	    }
	  else if ((info->run_type & GTK_RUN_NO_RECURSE) &&
		   gtk_emission_check (restart_emissions, info->object, info->signal_type))
	    {
	      gtk_emission_remove (&restart_emissions, info->object, info->signal_type);
	      return RESTART;
	    }
	}

      handlers = handlers->next;
    }

  return 0;
}

static void
gtk_params_get (GtkArg         *params,
		gint            nparams,
		GtkType        *param_types,
		GtkType         return_val,
		va_list         args)
{
  int i;

  for (i = 0; i < nparams; i++)
    {
      params[i].type = param_types[i];
      params[i].name = NULL;

      switch (GTK_FUNDAMENTAL_TYPE (param_types[i]))
	{
	case GTK_TYPE_INVALID:
	  break;
	case GTK_TYPE_NONE:
	  break;
	case GTK_TYPE_CHAR:
	  GTK_VALUE_CHAR(params[i]) = va_arg (args, gint);
	  break;
	case GTK_TYPE_BOOL:
	  GTK_VALUE_BOOL(params[i]) = va_arg (args, gint);
	  break;
	case GTK_TYPE_INT:
	  GTK_VALUE_INT(params[i]) = va_arg (args, gint);
	  break;
	case GTK_TYPE_UINT:
	  GTK_VALUE_UINT(params[i]) = va_arg (args, guint);
	  break;
	case GTK_TYPE_ENUM:
	  GTK_VALUE_ENUM(params[i]) = va_arg (args, gint);
	  break;
	case GTK_TYPE_FLAGS:
	  GTK_VALUE_FLAGS(params[i]) = va_arg (args, gint);
	  break;
	case GTK_TYPE_LONG:
	  GTK_VALUE_LONG(params[i]) = va_arg (args, glong);
	  break;
	case GTK_TYPE_ULONG:
	  GTK_VALUE_ULONG(params[i]) = va_arg (args, gulong);
	  break;
	case GTK_TYPE_FLOAT:
	  GTK_VALUE_FLOAT(params[i]) = va_arg (args, gfloat);
	  break;
	case GTK_TYPE_DOUBLE:
	  GTK_VALUE_DOUBLE(params[i]) = va_arg (args, gdouble);
	  break;
	case GTK_TYPE_STRING:
	  GTK_VALUE_STRING(params[i]) = va_arg (args, gchar*);
	  break;
	case GTK_TYPE_POINTER:
	  GTK_VALUE_POINTER(params[i]) = va_arg (args, gpointer);
	  break;
	case GTK_TYPE_BOXED:
	  GTK_VALUE_BOXED(params[i]) = va_arg (args, gpointer);
	  break;
	case GTK_TYPE_SIGNAL:
	  GTK_VALUE_SIGNAL(params[i]).f = va_arg (args, GtkFunction);
	  GTK_VALUE_SIGNAL(params[i]).d = va_arg (args, gpointer);
	  break;
	case GTK_TYPE_FOREIGN:
	  GTK_VALUE_FOREIGN(params[i]).data = va_arg (args, gpointer);
	  GTK_VALUE_FOREIGN(params[i]).notify = 
	    va_arg (args, GtkDestroyNotify);
	  break;
	case GTK_TYPE_CALLBACK:
	  GTK_VALUE_CALLBACK(params[i]).marshal = 
	    va_arg (args, GtkCallbackMarshal);
	  GTK_VALUE_CALLBACK(params[i]).data = va_arg (args, gpointer);
	  GTK_VALUE_CALLBACK(params[i]).notify =
	    va_arg (args, GtkDestroyNotify);
	  break;
	case GTK_TYPE_C_CALLBACK:
	  GTK_VALUE_C_CALLBACK(params[i]).func = va_arg (args, GtkFunction);
	  GTK_VALUE_C_CALLBACK(params[i]).func_data = va_arg (args, gpointer);
	  break;
	case GTK_TYPE_ARGS:
	  GTK_VALUE_ARGS(params[i]).n_args = va_arg (args, int);
	  GTK_VALUE_ARGS(params[i]).args = va_arg (args, GtkArg*);
	  break;
	case GTK_TYPE_OBJECT:
	  GTK_VALUE_OBJECT(params[i]) = va_arg (args, GtkObject*);
	  g_assert (GTK_VALUE_OBJECT(params[i]) == NULL ||
		    GTK_CHECK_TYPE (GTK_VALUE_OBJECT(params[i]),
				    params[i].type));
	  break;
	default:
	  g_error ("unsupported type %s in signal arg",
		   gtk_type_name (params[i].type));
	  break;
	}
    }

  params[i].type = return_val;
  params[i].name = NULL;

  switch (GTK_FUNDAMENTAL_TYPE (return_val))
    {
    case GTK_TYPE_INVALID:
      break;
    case GTK_TYPE_NONE:
      break;
    case GTK_TYPE_CHAR:
      params[i].d.pointer_data = va_arg (args, gchar*);
      break;
    case GTK_TYPE_BOOL:
      params[i].d.pointer_data = va_arg (args, gint*);
      break;
    case GTK_TYPE_INT:
      params[i].d.pointer_data = va_arg (args, gint*);
      break;
    case GTK_TYPE_UINT:
      params[i].d.pointer_data = va_arg (args, guint*);
      break;
    case GTK_TYPE_ENUM:
      params[i].d.pointer_data = va_arg (args, gint*);
      break;
    case GTK_TYPE_FLAGS:
      params[i].d.pointer_data = va_arg (args, gint*);
      break;
    case GTK_TYPE_LONG:
      params[i].d.pointer_data = va_arg (args, glong*);
      break;
    case GTK_TYPE_ULONG:
      params[i].d.pointer_data = va_arg (args, gulong*);
      break;
    case GTK_TYPE_FLOAT:
      params[i].d.pointer_data = va_arg (args, gfloat*);
      break;
    case GTK_TYPE_DOUBLE:
      params[i].d.pointer_data = va_arg (args, gdouble*);
      break;
    case GTK_TYPE_STRING:
      params[i].d.pointer_data = va_arg (args, gchar**);
      break;
    case GTK_TYPE_POINTER:
      params[i].d.pointer_data = va_arg (args, gpointer*);
      break;
    case GTK_TYPE_BOXED:
      params[i].d.pointer_data = va_arg (args, gpointer*);
      break;
    case GTK_TYPE_OBJECT:
      params[i].d.pointer_data = va_arg (args, GtkObject**);
      break;
    case GTK_TYPE_SIGNAL:
    case GTK_TYPE_FOREIGN:
    case GTK_TYPE_CALLBACK:
    case GTK_TYPE_C_CALLBACK:
    case GTK_TYPE_ARGS:
    default:
      g_error ("unsupported type %s in signal return",
	       gtk_type_name (return_val));
      break;
    }
}
