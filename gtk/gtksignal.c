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
#include "gtksignal.h"


#define MAX_PARAMS       20
#define DONE             1
#define RESTART          2

#define GTK_RUN_TYPE(x)  ((x) & GTK_RUN_MASK)


typedef struct _GtkSignal       GtkSignal;
typedef struct _GtkSignalInfo   GtkSignalInfo;
typedef struct _GtkHandler	GtkHandler;
typedef struct _GtkHandlerInfo  GtkHandlerInfo;
typedef struct _GtkEmission     GtkEmission;

typedef void (*GtkSignalMarshaller0) (GtkObject *object,
				      gpointer   data);

struct _GtkSignalInfo
{
  gchar *name;
  GtkType object_type;
  guint signal_type;
};

struct _GtkSignal
{
  GtkSignalInfo info;
  guint function_offset;
  GtkSignalRunType run_type;
  GtkSignalMarshaller marshaller;
  GtkType return_val;
  GtkType *params;
  guint nparams;
};

struct _GtkHandler
{
  guint id;
  guint blocked : 20;
  guint object_signal : 1;
  guint after : 1;
  guint no_marshal : 1;
  guint16 ref_count;
  guint16 signal_type;
  GtkSignalFunc func;
  gpointer func_data;
  GtkSignalDestroy destroy_func;
  GtkHandler *prev;
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
  guint nparams;
  guint signal_type;
};

struct _GtkEmission
{
  GtkObject *object;
  guint signal_type;
};


static void         gtk_signal_init            (void);
static guint        gtk_signal_hash            (guint         *key);
static gint         gtk_signal_compare         (guint         *a,
						guint         *b);
static guint        gtk_signal_info_hash       (GtkSignalInfo *a);
static gint         gtk_signal_info_compare    (GtkSignalInfo *a,
						GtkSignalInfo *b);
static GtkHandler*  gtk_signal_handler_new     (void);
static void         gtk_signal_handler_ref     (GtkHandler    *handler);
static void         gtk_signal_handler_unref   (GtkHandler    *handler,
						GtkObject     *object);
static void         gtk_signal_handler_insert  (GtkObject     *object,
						GtkHandler    *handler);
static void         gtk_signal_real_emit       (GtkObject     *object,
						guint          signal_type,
						va_list        args);
static GtkHandler*  gtk_signal_get_handlers    (GtkObject     *object,
						guint          signal_type);
static guint        gtk_signal_connect_by_type (GtkObject     *object,
						guint          signal_type,
						GtkSignalFunc  func,
						gpointer       func_data,
						GtkSignalDestroy destroy_func,
						gint           object_signal,
						gint           after,
						gint           no_marshal);
static GtkEmission* gtk_emission_new           (void);
static void         gtk_emission_destroy       (GtkEmission    *emission);
static void         gtk_emission_add           (GList         **emissions,
						GtkObject      *object,
						guint           signal_type);
static void         gtk_emission_remove        (GList         **emissions,
						GtkObject      *object,
						guint           signal_type);
static gint         gtk_emission_check         (GList          *emissions,
						GtkObject      *object,
						guint           signal_type);
static gint         gtk_handlers_run           (GtkHandler     *handlers,
						GtkHandlerInfo *info,
						gint            after);
static void         gtk_params_get             (GtkArg         *params,
						guint           nparams,
						GtkType        *param_types,
						GtkType         return_val,
						va_list         args);


static gint initialize = TRUE;
static GHashTable *signal_hash_table = NULL;
static GHashTable *signal_info_hash_table = NULL;
static guint next_signal = 1;
static guint next_handler_id = 1;

static const gchar *handler_key = "gtk-signal-handlers";
static guint        handler_key_id = 0;

static GMemChunk *handler_mem_chunk = NULL;
static GMemChunk *emission_mem_chunk = NULL;

static GList *current_emissions = NULL;
static GList *stop_emissions = NULL;
static GList *restart_emissions = NULL;

static GtkSignalMarshal global_marshaller = NULL;
static GtkSignalDestroy global_destroy_notify = NULL;


guint
gtk_signal_new (const gchar         *name,
		GtkSignalRunType     run_type,
		GtkType              object_type,
		guint                function_offset,
		GtkSignalMarshaller  marshaller,
		GtkType              return_val,
		guint                nparams,
		...)
{
  GtkType *params;
  guint i;
  va_list args;
  guint return_id;
  
  g_return_val_if_fail (nparams < 16, 0);
  
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
  
  return_id = gtk_signal_newv (name,
			       run_type,
			       object_type,
			       function_offset,
			       marshaller,
			       return_val,
			       nparams,
			       params);
  
  g_free (params);
  
  return return_id;
}

guint
gtk_signal_newv (const gchar         *name,
		 GtkSignalRunType     run_type,
		 GtkType              object_type,
		 guint                function_offset,
		 GtkSignalMarshaller  marshaller,
		 GtkType              return_val,
		 guint                nparams,
		 GtkType	     *params)
{
  GtkSignal *signal;
  GtkSignalInfo info;
  guint *type;
  guint i;
  
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (marshaller != NULL, 0);
  g_return_val_if_fail (nparams < 16, 0);
  if (nparams)
    g_return_val_if_fail (params != NULL, 0);
  
  if (initialize)
    gtk_signal_init ();
  
  info.name = (char*)name;
  info.object_type = object_type;
  
  type = g_hash_table_lookup (signal_info_hash_table, &info);
  if (type)
    {
      g_warning ("gtk_signal_newv(): signal \"%s\" already exists in the `%s' class ancestry\n",
		 name, gtk_type_name (object_type));
      return 0;
    }
  
  signal = g_new (GtkSignal, 1);
  signal->info.name = g_strdup (name);
  signal->info.object_type = object_type;
  signal->info.signal_type = next_signal++;
  signal->function_offset = function_offset;
  signal->run_type = run_type;
  signal->marshaller = marshaller;
  signal->return_val = return_val;
  signal->nparams = nparams;
  
  if (nparams > 0)
    {
      signal->params = g_new (GtkType, nparams);
      
      for (i = 0; i < nparams; i++)
	signal->params[i] = params[i];
    }
  else
    signal->params = NULL;
  
  g_hash_table_insert (signal_hash_table, &signal->info.signal_type, signal);
  g_hash_table_insert (signal_info_hash_table, &signal->info, &signal->info.signal_type);
  
  return signal->info.signal_type;
}

GtkSignalQuery*
gtk_signal_query (guint signal_id)
{
  GtkSignalQuery *query;
  GtkSignal *signal;
  
  g_return_val_if_fail (signal_id >= 1, NULL);
  
  signal = g_hash_table_lookup (signal_hash_table, &signal_id);
  if (signal)
    {
      query = g_new (GtkSignalQuery, 1);
      
      query->object_type = signal->info.object_type;
      query->signal_id = signal_id;
      query->signal_name = signal->info.name;
      query->is_user_signal = signal->function_offset == 0;
      query->run_type = signal->run_type;
      query->return_val = signal->return_val;
      query->nparams = signal->nparams;
      query->params = signal->params;
    }
  else
    query = NULL;
  
  return query;
}

guint
gtk_signal_lookup (const gchar *name,
		   GtkType      object_type)
{
  GtkSignalInfo info;
  
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (gtk_type_is_a (object_type, GTK_TYPE_OBJECT), 0);
  
  if (initialize)
    gtk_signal_init ();
  
  info.name = (char*)name;
  
  while (object_type)
    {
      guint *type;
      
      info.object_type = object_type;
      
      type = g_hash_table_lookup (signal_info_hash_table, &info);
      if (type)
	return *type;
      
      object_type = gtk_type_parent (object_type);
    }
  
  return 0;
}

gchar*
gtk_signal_name (guint signal_id)
{
  GtkSignal *signal;
  
  g_return_val_if_fail (signal_id >= 1, NULL);
  
  signal = g_hash_table_lookup (signal_hash_table, &signal_id);
  if (signal)
    return signal->info.name;
  
  return NULL;
}

void
gtk_signal_emit (GtkObject *object,
		 guint       signal_id,
		 ...)
{
  va_list args;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (signal_id >= 1);
  
  if (initialize)
    gtk_signal_init ();
  
  va_start (args, signal_id);
  
  gtk_signal_real_emit (object, signal_id, args);
  
  va_end (args);
}

void
gtk_signal_emit_by_name (GtkObject       *object,
			 const gchar     *name,
			 ...)
{
  guint type;
  va_list args;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (name != NULL);
  
  if (initialize)
    gtk_signal_init ();
  
  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  
  if (type >= 1)
    {
      va_start (args, name);
      
      gtk_signal_real_emit (object, type, args);
      
      va_end (args);
    }
  else
    {
      g_warning ("gtk_signal_emit_by_name(): could not find signal \"%s\" in the `%s' class ancestry",
		 name, gtk_type_name (GTK_OBJECT_TYPE (object)));
    }
}

void
gtk_signal_emit_stop (GtkObject *object,
		      guint       signal_id)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (signal_id >= 1);
  
  if (initialize)
    gtk_signal_init ();
  
  if (gtk_emission_check (current_emissions, object, signal_id))
    gtk_emission_add (&stop_emissions, object, signal_id);
  else
    g_warning ("gtk_signal_emit_stop(): no current emission (%u) for object `%s'",
	       signal_id, gtk_type_name (GTK_OBJECT_TYPE (object)));
}

void
gtk_signal_emit_stop_by_name (GtkObject       *object,
			      const gchar     *name)
{
  guint type;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (name != NULL);
  
  if (initialize)
    gtk_signal_init ();
  
  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (type)
    gtk_signal_emit_stop (object, type);
  else
    g_warning ("gtk_signal_emit_stop_by_name(): could not find signal \"%s\" in the `%s' class ancestry",
	       name, gtk_type_name (GTK_OBJECT_TYPE (object)));
}

guint
gtk_signal_n_emissions (GtkObject  *object,
			guint       signal_id)
{
  GList *tmp;
  guint n;
  
  g_return_val_if_fail (object != NULL, 0);
  g_return_val_if_fail (GTK_IS_OBJECT (object), 0);
  
  tmp = current_emissions;
  n = 0;
  while (tmp)
    {
      GtkEmission *emission;
      
      emission = tmp->data;
      tmp = tmp->next;
      
      if ((emission->object == object) &&
	  (emission->signal_type == signal_id))
	n++;
    }
  
  return n;
}

guint
gtk_signal_n_emissions_by_name (GtkObject       *object,
				const gchar     *name)
{
  guint type;
  guint n;
  
  g_return_val_if_fail (object != NULL, 0);
  g_return_val_if_fail (GTK_IS_OBJECT (object), 0);
  g_return_val_if_fail (name != NULL, 0);
  
  if (initialize)
    gtk_signal_init ();
  
  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (type)
    n = gtk_signal_n_emissions (object, type);
  else
    {
      g_warning ("gtk_signal_n_emissions_by_name(): could not find signal \"%s\" in the `%s' class ancestry",
		 name, gtk_type_name (GTK_OBJECT_TYPE (object)));
      n = 0;
    }

  return n;
}

guint
gtk_signal_connect (GtkObject     *object,
		    const gchar   *name,
		    GtkSignalFunc  func,
		    gpointer       func_data)
{
  guint type;
  
  g_return_val_if_fail (object != NULL, 0);
  g_return_val_if_fail (GTK_IS_OBJECT (object), 0);
  
  if (initialize)
    gtk_signal_init ();
  
  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!type)
    {
      g_warning ("gtk_signal_connect(): could not find signal \"%s\" in the `%s' class ancestry",
		 name, gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }
  
  return gtk_signal_connect_by_type (object, type, 
				     func, func_data, NULL,
				     FALSE, FALSE, FALSE);
}

guint
gtk_signal_connect_after (GtkObject     *object,
			  const gchar   *name,
			  GtkSignalFunc  func,
			  gpointer       func_data)
{
  guint type;
  
  g_return_val_if_fail (object != NULL, 0);
  
  if (initialize)
    gtk_signal_init ();
  
  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!type)
    {
      g_warning ("gtk_signal_connect_after(): could not find signal \"%s\" in the `%s' class ancestry",
		 name, gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }
  
  return gtk_signal_connect_by_type (object, type, 
				     func, func_data, NULL,
				     FALSE, TRUE, FALSE);
}

guint   
gtk_signal_connect_full (GtkObject           *object,
			 const gchar         *name,
			 GtkSignalFunc        func,
			 GtkCallbackMarshal   marshal,
			 gpointer             func_data,
			 GtkDestroyNotify     destroy_func,
			 gint                 object_signal,
			 gint                 after)
{
  guint type;
  
  g_return_val_if_fail (object != NULL, 0);
  
  if (initialize)
    gtk_signal_init ();
  
  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!type)
    {
      g_warning ("gtk_signal_connect_full(): could not find signal \"%s\" in the `%s' class ancestry",
		 name, gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }
  
  if (marshal)
    return gtk_signal_connect_by_type (object, type, (GtkSignalFunc) marshal, 
				       func_data, destroy_func, 
				       object_signal, after, TRUE);
  else
    return gtk_signal_connect_by_type (object, type, func, 
				       func_data, destroy_func, 
				       object_signal, after, FALSE);
}

guint
gtk_signal_connect_interp (GtkObject         *object,
			   const gchar       *name,
			   GtkCallbackMarshal func,
			   gpointer           func_data,
			   GtkDestroyNotify   destroy_func,
			   gint               after)
{
  return gtk_signal_connect_full (object, name, NULL, func,
				  func_data, destroy_func, FALSE, after);
}

guint
gtk_signal_connect_object (GtkObject     *object,
			   const gchar   *name,
			   GtkSignalFunc  func,
			   GtkObject     *slot_object)
{
  guint type;
  
  g_return_val_if_fail (object != NULL, 0);
  /* slot_object needs to be treated as ordinary pointer */
  
  if (initialize)
    gtk_signal_init ();
  
  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!type)
    {
      g_warning ("gtk_signal_connect_object(): could not find signal \"%s\" in the `%s' class ancestry",
		 name, gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }
  
  return gtk_signal_connect_by_type (object, type, 
				     func, slot_object, NULL,
				     TRUE, FALSE, FALSE);
}

guint
gtk_signal_connect_object_after (GtkObject     *object,
				 const gchar   *name,
				 GtkSignalFunc  func,
				 GtkObject     *slot_object)
{
  guint type;
  
  g_return_val_if_fail (object != NULL, 0);
  
  if (initialize)
    gtk_signal_init ();
  
  type = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!type)
    {
      g_warning ("gtk_signal_connect_object_after(): could not find signal \"%s\" in the `%s' class ancestry",
		 name, gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }
  
  return gtk_signal_connect_by_type (object, type, 
				     func, slot_object, NULL,
				     TRUE, TRUE, FALSE);
}

typedef struct _GtkDisconnectInfo       GtkDisconnectInfo;
struct _GtkDisconnectInfo
{
  GtkObject     *object1;
  guint          disconnect_handler1;
  guint          signal_handler;
  GtkObject     *object2;
  guint          disconnect_handler2;
};

static guint
gtk_alive_disconnecter (GtkDisconnectInfo *info)
{
  g_return_val_if_fail (info != NULL, 0);
  
  gtk_signal_disconnect (info->object1, info->disconnect_handler1);
  gtk_signal_disconnect (info->object1, info->signal_handler);
  gtk_signal_disconnect (info->object2, info->disconnect_handler2);
  g_free (info);
  
  return 0;
}

void
gtk_signal_connect_while_alive (GtkObject        *object,
				const gchar      *signal,
				GtkSignalFunc     func,
				gpointer          func_data,
				GtkObject        *alive_object)
{
  GtkDisconnectInfo *info;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (signal != NULL);
  g_return_if_fail (func != NULL);
  g_return_if_fail (alive_object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (alive_object));
  
  info = g_new (GtkDisconnectInfo, 1);
  info->object1 = object;
  info->object2 = alive_object;
  
  info->signal_handler = gtk_signal_connect (object, signal, func, func_data);
  info->disconnect_handler1 = gtk_signal_connect_object (info->object1,
							 "destroy",
							 GTK_SIGNAL_FUNC (gtk_alive_disconnecter),
							 (GtkObject*) info);
  info->disconnect_handler2 = gtk_signal_connect_object (info->object2,
							 "destroy",
							 GTK_SIGNAL_FUNC (gtk_alive_disconnecter),
							 (GtkObject*) info);
}

void
gtk_signal_connect_object_while_alive (GtkObject        *object,
				       const gchar      *signal,
				       GtkSignalFunc     func,
				       GtkObject        *alive_object)
{
  GtkDisconnectInfo *info;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (signal != NULL);
  g_return_if_fail (func != NULL);
  g_return_if_fail (alive_object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (alive_object));
  
  info = g_new (GtkDisconnectInfo, 1);
  info->object1 = object;
  info->object2 = alive_object;
  
  info->signal_handler = gtk_signal_connect_object (object, signal, func, alive_object);
  info->disconnect_handler1 = gtk_signal_connect_object (info->object1,
							 "destroy",
							 GTK_SIGNAL_FUNC (gtk_alive_disconnecter),
							 (GtkObject*) info);
  info->disconnect_handler2 = gtk_signal_connect_object (info->object2,
							 "destroy",
							 GTK_SIGNAL_FUNC (gtk_alive_disconnecter),
							 (GtkObject*) info);
}

void
gtk_signal_disconnect (GtkObject *object,
		       guint      handler_id)
{
  GtkHandler *handler;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (handler_id > 0);
  
  handler = gtk_object_get_data_by_id (object, handler_key_id);
  
  while (handler)
    {
      if (handler->id == handler_id)
	{
	  handler->id = 0;
	  handler->blocked += 1;
	  gtk_signal_handler_unref (handler, object);
	  return;
	}
      handler = handler->next;
    }
  
  g_warning ("gtk_signal_disconnect(): could not find handler (%u)", handler_id);
}

void
gtk_signal_disconnect_by_func (GtkObject     *object,
			       GtkSignalFunc  func,
			       gpointer       data)
{
  GtkHandler *handler;
  gint found_one;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (func != NULL);
  
  found_one = FALSE;
  handler = gtk_object_get_data_by_id (object, handler_key_id);
  
  while (handler)
    {
      GtkHandler *handler_next;
      
      handler_next = handler->next;
      if ((handler->id > 0) &&
	  (handler->func == func) &&
	  (handler->func_data == data))
	{
	  found_one = TRUE;
	  handler->id = 0;
	  handler->blocked += 1;
	  gtk_signal_handler_unref (handler, object);
	}
      handler = handler_next;
    }
  
  if (!found_one)
    g_warning ("gtk_signal_disconnect_by_func(): could not find handler (0x%0lX) containing data (0x%0lX)", (long) func, (long) data);
}

void
gtk_signal_disconnect_by_data (GtkObject *object,
			       gpointer   data)
{
  GtkHandler *handler;
  gint found_one;
  
  g_return_if_fail (object != NULL);
  
  found_one = FALSE;
  handler = gtk_object_get_data_by_id (object, handler_key_id);
  
  while (handler)
    {
      GtkHandler *handler_next;
      
      handler_next = handler->next;
      if ((handler->id > 0) &&
	  (handler->func_data == data))
	{
	  found_one = TRUE;
	  handler->id = 0;
	  handler->blocked += 1;
	  gtk_signal_handler_unref (handler, object);
	}
      handler = handler_next;
    }
  
  if (!found_one)
    g_warning ("gtk_signal_disconnect_by_data(): could not find handler containing data (0x%0lX)", (long) data);
}

void
gtk_signal_handler_block (GtkObject *object,
			  guint	     handler_id)
{
  GtkHandler *handler;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (handler_id > 0);
  
  handler = gtk_object_get_data_by_id (object, handler_key_id);
  
  while (handler)
    {
      if (handler->id == handler_id)
	{
	  handler->blocked += 1;
	  return;
	}
      handler = handler->next;
    }
  
  g_warning ("gtk_signal_handler_block(): could not find handler (%u)", handler_id);
}

void
gtk_signal_handler_block_by_func (GtkObject     *object,
				  GtkSignalFunc  func,
				  gpointer       data)
{
  GtkHandler *handler;
  gint found_one;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (func != NULL);
  
  found_one = FALSE;
  handler = gtk_object_get_data_by_id (object, handler_key_id);
  
  while (handler)
    {
      if ((handler->id > 0) &&
	  (handler->func == func) &&
	  (handler->func_data == data))
	{
	  found_one = TRUE;
	  handler->blocked += 1;
	}
      handler = handler->next;
    }
  
  if (!found_one)
    g_warning ("gtk_signal_handler_block_by_func(): could not find handler (0x%0lX) containing data (0x%0lX)", (long) func, (long) data);
}

void
gtk_signal_handler_block_by_data (GtkObject *object,
				  gpointer   data)
{
  GtkHandler *handler;
  gint found_one;
  
  g_return_if_fail (object != NULL);
  
  found_one = FALSE;
  handler = gtk_object_get_data_by_id (object, handler_key_id);
  
  while (handler)
    {
      if ((handler->id > 0) &&
	  (handler->func_data == data))
	{
	  found_one = TRUE;
	  handler->blocked += 1;
	}
      handler = handler->next;
    }
  
  if (!found_one)
    g_warning ("gtk_signal_handler_block_by_data(): could not find handler containing data (0x%0lX)", (long) data);
}

void
gtk_signal_handler_unblock (GtkObject *object,
			    guint      handler_id)
{
  GtkHandler *handler;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (handler_id > 0);
  
  if (initialize)
    gtk_signal_init ();
  
  handler = gtk_object_get_data_by_id (object, handler_key_id);
  
  while (handler)
    {
      if (handler->id == handler_id)
	{
	  if (handler->blocked > 0)
	    handler->blocked -= 1;
	  else
	    g_warning ("gtk_signal_handler_unblock(): handler (%u) is not blocked", handler_id);
	  return;
	}
      handler = handler->next;
    }
  
  g_warning ("gtk_signal_handler_unblock(): could not find handler (%u)", handler_id);
}

void
gtk_signal_handler_unblock_by_func (GtkObject     *object,
				    GtkSignalFunc  func,
				    gpointer       data)
{
  GtkHandler *handler;
  gint found_one;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (func != NULL);
  
  found_one = FALSE;
  handler = gtk_object_get_data_by_id (object, handler_key_id);
  
  while (handler)
    {
      if ((handler->id > 0) &&
	  (handler->func == func) &&
	  (handler->func_data == data) &&
	  (handler->blocked > 0))
	{
	  handler->blocked -= 1;
	  found_one = TRUE;
	}
      handler = handler->next;
    }
  
  if (!found_one)
    g_warning ("gtk_signal_handler_unblock_by_func(): could not find blocked handler (0x%0lX) containing data (0x%0lX)", (long) func, (long) data);
}

void
gtk_signal_handler_unblock_by_data (GtkObject *object,
				    gpointer   data)
{
  GtkHandler *handler;
  gint found_one;
  
  g_return_if_fail (object != NULL);
  
  found_one = FALSE;
  handler = gtk_object_get_data_by_id (object, handler_key_id);
  
  while (handler)
    {
      if ((handler->id > 0) &&
	  (handler->func_data == data) &&
          (handler->blocked > 0))
	{
	  handler->blocked -= 1;
	  found_one = TRUE;
	}
      handler = handler->next;
    }
  
  if (!found_one)
    g_warning ("gtk_signal_handler_unblock_by_data(): could not find blocked handler containing data (0x%0lX)", (long) data);
}

void
gtk_signal_handlers_destroy (GtkObject *object)
{
  GtkHandler *handler;
  
  /* we make the "optimization" of destroying the first handler in the last
   * place, since we don't want gtk_signal_handler_unref() to reset the objects
   * handler_key data on each removal
   */
  
  handler = gtk_object_get_data_by_id (object, handler_key_id);
  if (handler)
    {
      handler = handler->next;
      while (handler)
	{
	  GtkHandler *next;
	  
	  next = handler->next;
	  gtk_signal_handler_unref (handler, object);
	  handler = next;
	}
      handler = gtk_object_get_data_by_id (object, handler_key_id);
      gtk_signal_handler_unref (handler, object);
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
  global_marshaller = marshal_func;
  global_destroy_notify = destroy_func;
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
gtk_signal_hash (guint *key)
{
  return *key;
}

static gint
gtk_signal_compare (guint *a,
		    guint *b)
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
  handler->blocked = 0;
  handler->signal_type = 0;
  handler->object_signal = FALSE;
  handler->after = FALSE;
  handler->no_marshal = FALSE;
  handler->ref_count = 1;
  handler->func = NULL;
  handler->func_data = NULL;
  handler->destroy_func = NULL;
  handler->prev = NULL;
  handler->next = NULL;
  
  return handler;
}

static void
gtk_signal_handler_ref (GtkHandler *handler)
{
  handler->ref_count += 1;
}

static void
gtk_signal_handler_unref (GtkHandler *handler,
			  GtkObject  *object)
{
  handler->ref_count -= 1;
  if (handler->ref_count == 0)
    {
      if (handler->destroy_func)
	(* handler->destroy_func) (handler->func_data);
      else if (!handler->func && global_destroy_notify)
	(* global_destroy_notify) (handler->func_data);
      
      if (handler->prev)
	handler->prev->next = handler->next;
      else
	gtk_object_set_data_by_id (object, handler_key_id, handler->next);
      if (handler->next)
	handler->next->prev = handler->prev;
      
      g_mem_chunk_free (handler_mem_chunk, handler);
    }
}

static void
gtk_signal_handler_insert (GtkObject  *object,
			   GtkHandler *handler)
{
  GtkHandler *tmp;
  
  /* FIXME: remove */ g_assert (handler->next == NULL);
  /* FIXME: remove */ g_assert (handler->prev == NULL);
  
  tmp = gtk_object_get_data_by_id (object, handler_key_id);
  if (!tmp)
    {
      if (!handler_key_id)
	handler_key_id = gtk_object_data_force_id (handler_key);
      gtk_object_set_data_by_id (object, handler_key_id, handler);
    }
  else
    while (tmp)
      {
	if (tmp->signal_type < handler->signal_type)
	  {
	    if (tmp->prev)
	      {
		tmp->prev->next = handler;
		handler->prev = tmp->prev;
	      }
	    else
	      gtk_object_set_data_by_id (object, handler_key_id, handler);
	    tmp->prev = handler;
	    handler->next = tmp;
	    break;
	  }
	
	if (!tmp->next)
	  {
	    tmp->next = handler;
	    handler->prev = tmp;
	    break;
	  }
	tmp = tmp->next;
      }
}

static void
gtk_signal_real_emit (GtkObject *object,
		      guint      signal_type,
		      va_list    args)
{
  GtkSignal *signal;
  GtkHandler *handlers;
  GtkHandlerInfo info;
  guchar **signal_func_offset;
  GtkArg         params[MAX_PARAMS];
  
  signal = g_hash_table_lookup (signal_hash_table, &signal_type);
  g_return_if_fail (signal != NULL);
  g_return_if_fail (gtk_type_is_a (GTK_OBJECT_TYPE (object),
				   signal->info.object_type));
  
  if ((signal->run_type & GTK_RUN_NO_RECURSE) &&
      gtk_emission_check (current_emissions, object, signal_type))
    {
      gtk_emission_add (&restart_emissions, object, signal_type);
      return;
    }
  
  gtk_params_get (params, signal->nparams, signal->params,
		  signal->return_val, args);
  
  gtk_emission_add (&current_emissions, object, signal_type);
  
  gtk_object_ref (object);
  
restart:
  if (GTK_RUN_TYPE (signal->run_type) != GTK_RUN_LAST && signal->function_offset != 0)
    {
      signal_func_offset = (guchar**) ((guchar*) object->klass +
				       signal->function_offset);
      if (*signal_func_offset)
	(* signal->marshaller) (object, (GtkSignalFunc) *signal_func_offset,
				NULL, params);
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
  
  if (GTK_RUN_TYPE (signal->run_type) != GTK_RUN_FIRST  && signal->function_offset != 0)
    {
      signal_func_offset = (guchar**) ((guchar*) object->klass +
				       signal->function_offset);
      if (*signal_func_offset)
	(* signal->marshaller) (object, (GtkSignalFunc) *signal_func_offset,
				NULL, params);
    }
  
  handlers = gtk_signal_get_handlers (object, signal_type);
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
  
  gtk_object_unref (object);
}

static GtkHandler*
gtk_signal_get_handlers (GtkObject *object,
			 guint      signal_type)
{
  GtkHandler *handlers;
  
  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (signal_type >= 1, NULL);
  
  handlers = gtk_object_get_data_by_id (object, handler_key_id);
  
  while (handlers)
    {
      if (handlers->signal_type == signal_type)
	return handlers;
      handlers = handlers->next;
    }
  
  return NULL;
}

guint
gtk_signal_handler_pending (GtkObject           *object,
			    guint                signal_id,
			    gboolean		 may_be_blocked)
{
  GtkHandler *handlers;
  guint handler_id;
  
  g_return_val_if_fail (object != NULL, 0);
  g_return_val_if_fail (signal_id >= 1, 0);
  
  handlers = gtk_signal_get_handlers (object, signal_id);
  
  handler_id = 0;
  while (handlers && handlers->signal_type == signal_id)
    {
      if (handlers->id > 0 &&
	  (may_be_blocked || handlers->blocked == 0))
	{
	  handler_id = handlers->id;
	  break;
	}
      
      handlers = handlers->next;
    }
  
  return handler_id;
}

static guint
gtk_signal_connect_by_type (GtkObject       *object,
			    guint            signal_type,
			    GtkSignalFunc    func,
			    gpointer         func_data,
			    GtkSignalDestroy destroy_func,
			    gint             object_signal,
			    gint             after,
			    gint             no_marshal)
{
  GtkObjectClass *class;
  GtkHandler *handler;
  gint found_it;
  
  g_return_val_if_fail (object != NULL, 0);
  g_return_val_if_fail (object->klass != NULL, 0);
  
  /* Search through the signals for this object and make
   *  sure the one we are adding is valid. We need to perform
   *  the lookup on the objects parents as well. If it isn't
   *  valid then issue a warning and return.
   */
  found_it = FALSE;
  class = object->klass;
  while (class)
    {
      GtkType parent;
      guint *object_signals;
      guint nsignals;
      guint i;
      
      object_signals = class->signals;
      nsignals = class->nsignals;
      
      for (i = 0; i < nsignals; i++)
	if (object_signals[i] == signal_type)
	  {
	    found_it = TRUE;
	    break;
	  }
      
      parent = gtk_type_parent (class->type);
      if (parent)
	class = gtk_type_class (parent);
      else
	class = NULL;
    }
  
  if (!found_it)
    {
      g_warning ("gtk_signal_connect_by_type(): could not find signal id (%u) in the `%s' class ancestry",
		 signal_type,
		 gtk_type_name (class->type));
      return 0;
    }
  
  handler = gtk_signal_handler_new ();
  handler->id = next_handler_id++;
  handler->signal_type = signal_type;
  handler->object_signal = object_signal;
  handler->func = func;
  handler->func_data = func_data;
  handler->destroy_func = destroy_func;
  handler->after = after != FALSE;
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
		  guint       signal_type)
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
		     guint       signal_type)
{
  GtkEmission *emission;
  GList *tmp;
  
  g_return_if_fail (emissions != NULL);
  
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
		    guint      signal_type)
{
  GtkEmission *emission;
  GList *tmp;
  
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
  while (handlers && handlers->signal_type == info->signal_type)
    {
      GtkHandler *handlers_next;
      
      gtk_signal_handler_ref (handlers);
      
      if (handlers->blocked == 0 && (handlers->after == after))
	{
	  if (handlers->func)
	    {
	      if (handlers->no_marshal)
		(* (GtkCallbackMarshal) handlers->func) (info->object,
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
	  else if (global_marshaller)
	    (* global_marshaller) (info->object,
				   handlers->func_data,
				   info->nparams,
				   info->params,
				   info->param_types,
				   info->return_val);
	  
          if (gtk_emission_check (stop_emissions, info->object,
				  info->signal_type))
	    {
	      gtk_emission_remove (&stop_emissions, info->object,
				   info->signal_type);
	      
	      if (info->run_type & GTK_RUN_NO_RECURSE)
		gtk_emission_remove (&restart_emissions, info->object,
				     info->signal_type);
	      gtk_signal_handler_unref (handlers, info->object);
	      return DONE;
	    }
	  else if ((info->run_type & GTK_RUN_NO_RECURSE) &&
		   gtk_emission_check (restart_emissions, info->object,
				       info->signal_type))
	    {
	      gtk_emission_remove (&restart_emissions, info->object,
				   info->signal_type);
	      gtk_signal_handler_unref (handlers, info->object);
	      return RESTART;
	    }
	}
      
      handlers_next = handlers->next;
      gtk_signal_handler_unref (handlers, info->object);
      handlers = handlers_next;
    }
  
  return 0;
}

static void
gtk_params_get (GtkArg         *params,
		guint           nparams,
		GtkType        *param_types,
		GtkType         return_val,
		va_list         args)
{
  gint i;
  
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
	  GTK_VALUE_ARGS(params[i]).n_args = va_arg (args, gint);
	  GTK_VALUE_ARGS(params[i]).args = va_arg (args, GtkArg*);
	  break;
	case GTK_TYPE_OBJECT:
	  GTK_VALUE_OBJECT(params[i]) = va_arg (args, GtkObject*);
	  if (GTK_VALUE_OBJECT(params[i]) != NULL &&
	      !GTK_CHECK_TYPE (GTK_VALUE_OBJECT(params[i]), params[i].type))
	    g_warning ("signal arg `%s' is not of type `%s'",
		       gtk_type_name (GTK_OBJECT_TYPE (GTK_VALUE_OBJECT(params[i]))),
		       gtk_type_name (params[i].type));
	  break;
	default:
	  g_error ("unsupported type `%s' in signal arg",
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
      g_error ("unsupported type `%s' in signal return",
	       gtk_type_name (return_val));
      break;
    }
}
