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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "gtksignal.h"
#include "gtkargcollector.c"


#define	SIGNAL_BLOCK_SIZE		(100)
#define	HANDLER_BLOCK_SIZE		(200)
#define	EMISSION_BLOCK_SIZE		(100)
#define	DISCONNECT_INFO_BLOCK_SIZE	(64)
#define MAX_SIGNAL_PARAMS		(31)

enum
{
  EMISSION_CONTINUE,
  EMISSION_RESTART,
  EMISSION_DONE
};

#define GTK_RUN_TYPE(x)	 ((x) & GTK_RUN_BOTH)


typedef struct _GtkSignal		GtkSignal;
typedef struct _GtkSignalHash		GtkSignalHash;
typedef struct _GtkHandler		GtkHandler;
typedef struct _GtkEmission		GtkEmission;
typedef struct _GtkEmissionHookData	GtkEmissionHookData;
typedef struct _GtkDisconnectInfo	GtkDisconnectInfo;

typedef void (*GtkSignalMarshaller0) (GtkObject *object,
				      gpointer	 data);

struct _GtkSignal
{
  guint		      signal_id;
  GtkType	      object_type;
  gchar		     *name;
  guint		      function_offset;
  GtkSignalMarshaller marshaller;
  GtkType	      return_val;
  guint		      signal_flags : 16;
  guint		      nparams : 16;
  GtkType	     *params;
  GHookList	     *hook_list;
};

struct _GtkSignalHash
{
  GtkType object_type;
  GQuark  quark;
  guint	  signal_id;
};

struct _GtkHandler
{
  guint		   id;
  GtkHandler	  *next;
  GtkHandler	  *prev;
  guint		   blocked : 20;
  guint		   object_signal : 1;
  guint		   after : 1;
  guint		   no_marshal : 1;
  guint16	   ref_count;
  guint16	   signal_id;
  GtkSignalFunc	   func;
  gpointer	   func_data;
  GtkSignalDestroy destroy_func;
};

struct _GtkEmission
{
  GtkObject   *object;
  guint16      signal_id;
  guint	       in_hook : 1;
  GtkEmission *next;
};

struct _GtkEmissionHookData
{
  GtkObject *object;
  guint signal_id;
  guint n_params;
  GtkArg *params;
};

struct _GtkDisconnectInfo
{
  GtkObject	*object1;
  guint		 disconnect_handler1;
  guint		 signal_handler;
  GtkObject	*object2;
  guint		 disconnect_handler2;
};


static guint	    gtk_signal_hash	       (gconstpointer h);
static gint	    gtk_signal_compare	       (gconstpointer h1,
						gconstpointer h2);
static GtkHandler*  gtk_signal_handler_new     (void);
static void	    gtk_signal_handler_ref     (GtkHandler    *handler);
static void	    gtk_signal_handler_unref   (GtkHandler    *handler,
						GtkObject     *object);
static void	    gtk_signal_handler_insert  (GtkObject     *object,
						GtkHandler    *handler);
static void	    gtk_signal_real_emit       (GtkObject     *object,
						guint	       signal_id,
						GtkArg	      *params);
static guint	    gtk_signal_connect_by_type (GtkObject     *object,
						guint	       signal_id,
						GtkSignalFunc  func,
						gpointer       func_data,
						GtkSignalDestroy destroy_func,
						gint	       object_signal,
						gint	       after,
						gint	       no_marshal);
static guint	    gtk_alive_disconnecter     (GtkDisconnectInfo *info);
static GtkEmission* gtk_emission_new	       (void);
static void	    gtk_emission_add	       (GtkEmission   **emissions,
						GtkObject      *object,
						guint		signal_type);
static void	    gtk_emission_remove	       (GtkEmission   **emissions,
						GtkObject      *object,
						guint		signal_type);
static gint	    gtk_emission_check	       (GtkEmission    *emissions,
						GtkObject      *object,
						guint		signal_type);
static gint	    gtk_handlers_run	       (GtkHandler     *handlers,
						GtkSignal      *signal,
						GtkObject      *object,
						GtkArg         *params,
						gint		after);
static gboolean gtk_emission_hook_marshaller   (GHook          *hook,
						gpointer        data);
static gboolean	gtk_signal_collect_params      (GtkArg	       *params,
						guint		nparams,
						GtkType	       *param_types,
						GtkType		return_type,
						va_list		var_args);

#define LOOKUP_SIGNAL_ID(signal_id)	( \
  signal_id > 0 && signal_id < _gtk_private_n_signals ? \
    (GtkSignal*) _gtk_private_signals + signal_id : \
    (GtkSignal*) 0 \
)


static GtkSignalMarshal global_marshaller = NULL;
static GtkSignalDestroy global_destroy_notify = NULL;

static guint	   		 gtk_handler_id = 1;
static guint	   		 gtk_handler_quark = 0;
static GHashTable  		*gtk_signal_hash_table = NULL;
       GtkSignal   		*_gtk_private_signals = NULL;
       guint	   		 _gtk_private_n_signals = 0;
static GMemChunk   		*gtk_signal_hash_mem_chunk = NULL;
static GMemChunk   		*gtk_disconnect_info_mem_chunk = NULL;
static GtkHandler  		*gtk_handler_free_list = NULL;
static GtkEmission		*gtk_free_emissions = NULL;



static GtkEmission *current_emissions = NULL;
static GtkEmission *stop_emissions = NULL;
static GtkEmission *restart_emissions = NULL;

static GtkSignal*
gtk_signal_next_and_invalidate (void)
{
  static guint gtk_n_free_signals = 0;
  register GtkSignal *signal;
  register guint new_signal_id;
  
  /* don't keep *any* GtkSignal pointers across invokation of this function!!!
   */
  
  if (gtk_n_free_signals == 0)
    {
      register guint i;
      register guint size;
      
      /* nearest pow
       */
      size = _gtk_private_n_signals + SIGNAL_BLOCK_SIZE;
      size *= sizeof (GtkSignal);
      i = 1;
      while (i < size)
	i <<= 1;
      size = i;
      
      _gtk_private_signals = g_realloc (_gtk_private_signals, size);
      
      gtk_n_free_signals = size / sizeof (GtkSignal) - _gtk_private_n_signals;
      
      memset (_gtk_private_signals + _gtk_private_n_signals, 0, gtk_n_free_signals * sizeof (GtkSignal));
    }
  
  new_signal_id = _gtk_private_n_signals++;
  gtk_n_free_signals--;

  g_assert (_gtk_private_n_signals < 65535);
  
  signal = LOOKUP_SIGNAL_ID (new_signal_id);
  if (signal)
    signal->signal_id = new_signal_id;
  
  return signal;
}

static inline GtkHandler*
gtk_signal_get_handlers (GtkObject *object,
			 guint	    signal_id)
{
  GtkHandler *handlers;
  
  handlers = gtk_object_get_data_by_id (object, gtk_handler_quark);
  
  while (handlers)
    {
      if (handlers->signal_id == signal_id)
	return handlers;
      handlers = handlers->next;
    }
  
  return NULL;
}

void
gtk_signal_init (void)
{
  if (!gtk_handler_quark)
    {
      GtkSignal *zero;
      
      zero = gtk_signal_next_and_invalidate ();
      g_assert (zero == NULL);
      
      gtk_handler_quark = g_quark_from_static_string ("gtk-signal-handlers");
      
      gtk_signal_hash_mem_chunk =
	g_mem_chunk_new ("GtkSignalHash mem chunk",
			 sizeof (GtkSignalHash),
			 sizeof (GtkSignalHash) * SIGNAL_BLOCK_SIZE,
			 G_ALLOC_ONLY);
      gtk_disconnect_info_mem_chunk =
	g_mem_chunk_new ("GtkDisconnectInfo mem chunk",
			 sizeof (GtkDisconnectInfo),
			 sizeof (GtkDisconnectInfo) * DISCONNECT_INFO_BLOCK_SIZE,
			 G_ALLOC_AND_FREE);
      gtk_handler_free_list = NULL;
      gtk_free_emissions = NULL;
      
      gtk_signal_hash_table = g_hash_table_new (gtk_signal_hash,
						gtk_signal_compare);
    }
}

guint
gtk_signal_newv (const gchar	     *r_name,
		 GtkSignalRunType     signal_flags,
		 GtkType	      object_type,
		 guint		      function_offset,
		 GtkSignalMarshaller  marshaller,
		 GtkType	      return_val,
		 guint		      nparams,
		 GtkType	     *params)
{
  GtkSignal *signal;
  GtkSignalHash *hash;
  GQuark quark;
  guint i;
  gchar *name;
  
  g_return_val_if_fail (r_name != NULL, 0);
  g_return_val_if_fail (marshaller != NULL, 0);
  g_return_val_if_fail (nparams < MAX_SIGNAL_PARAMS, 0);
  if (nparams)
    g_return_val_if_fail (params != NULL, 0);
  
  if (!gtk_handler_quark)
    gtk_signal_init ();


  name = g_strdup (r_name);
  g_strdelimit (name, NULL, '_');

  quark = gtk_signal_lookup (name, object_type);
  if (quark)
    {
      g_warning ("gtk_signal_newv(): signal \"%s\" already exists in the `%s' class ancestry\n",
		 r_name,
		 gtk_type_name (object_type));
      g_free (name);
      return 0;
    }
  
  if (return_val != GTK_TYPE_NONE &&
      (signal_flags & GTK_RUN_BOTH) == GTK_RUN_FIRST)
    {
      g_warning ("gtk_signal_newv(): signal \"%s\" - return value `%s' incompatible with GTK_RUN_FIRST",
		 name, gtk_type_name (return_val));
      g_free (name);
      return 0;
    }
  
  signal = gtk_signal_next_and_invalidate ();
  
  /* signal->signal_id already set */
  
  signal->object_type = object_type;
  signal->name = name;
  signal->function_offset = function_offset;
  signal->marshaller = marshaller;
  signal->return_val = return_val;
  signal->signal_flags = signal_flags;
  signal->nparams = nparams;
  signal->hook_list = NULL;
  
  if (nparams > 0)
    {
      signal->params = g_new (GtkType, nparams);
      
      for (i = 0; i < nparams; i++)
	signal->params[i] = params[i];
    }
  else
    signal->params = NULL;

  /* insert "signal_name" into hash table
   */
  hash = g_chunk_new (GtkSignalHash, gtk_signal_hash_mem_chunk);
  hash->object_type = object_type;
  hash->quark = g_quark_from_string (signal->name);
  hash->signal_id = signal->signal_id;
  g_hash_table_insert (gtk_signal_hash_table, hash, GUINT_TO_POINTER (hash->signal_id));

  /* insert "signal-name" into hash table
   */
  g_strdelimit (signal->name, NULL, '-');
  quark = g_quark_from_static_string (signal->name);
  if (quark != hash->quark)
    {
      hash = g_chunk_new (GtkSignalHash, gtk_signal_hash_mem_chunk);
      hash->object_type = object_type;
      hash->quark = quark;
      hash->signal_id = signal->signal_id;
      g_hash_table_insert (gtk_signal_hash_table, hash, GUINT_TO_POINTER (hash->signal_id));
    }
  
  return signal->signal_id;
}

guint
gtk_signal_new (const gchar	    *name,
		GtkSignalRunType     signal_flags,
		GtkType		     object_type,
		guint		     function_offset,
		GtkSignalMarshaller  marshaller,
		GtkType		     return_val,
		guint		     nparams,
		...)
{
  GtkType *params;
  guint i;
  va_list args;
  guint signal_id;
  
  g_return_val_if_fail (nparams < MAX_SIGNAL_PARAMS, 0);
  
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
			       object_type,
			       function_offset,
			       marshaller,
			       return_val,
			       nparams,
			       params);
  
  g_free (params);
  
  return signal_id;
}

guint
gtk_signal_lookup (const gchar *name,
		   GtkType	object_type)
{
  GtkSignalHash hash;
  GtkType lookup_type;
  gpointer class = NULL;

  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (gtk_type_is_a (object_type, GTK_TYPE_OBJECT), 0);
  
 relookup:

  lookup_type = object_type;
  hash.quark = g_quark_try_string (name);
  if (hash.quark)
    {
      while (lookup_type)
	{
	  guint signal_id;
	  
	  hash.object_type = lookup_type;
	  
	  signal_id = GPOINTER_TO_UINT (g_hash_table_lookup (gtk_signal_hash_table, &hash));
	  if (signal_id)
	    return signal_id;
	  
	  lookup_type = gtk_type_parent (lookup_type);
	}
    }

  if (!class)
    {
      class = gtk_type_class (object_type);
      goto relookup;
    }

  return 0;
}

GtkSignalQuery*
gtk_signal_query (guint signal_id)
{
  GtkSignalQuery *query;
  GtkSignal *signal;
  
  g_return_val_if_fail (signal_id >= 1, NULL);
  
  signal = LOOKUP_SIGNAL_ID (signal_id);
  if (signal)
    {
      query = g_new (GtkSignalQuery, 1);
      
      query->object_type = signal->object_type;
      query->signal_id = signal_id;
      query->signal_name = signal->name;
      query->is_user_signal = signal->function_offset == 0;
      query->signal_flags = signal->signal_flags;
      query->return_val = signal->return_val;
      query->nparams = signal->nparams;
      query->params = signal->params;
    }
  else
    query = NULL;
  
  return query;
}

gchar*
gtk_signal_name (guint signal_id)
{
  GtkSignal *signal;
  
  g_return_val_if_fail (signal_id >= 1, NULL);
  
  signal = LOOKUP_SIGNAL_ID (signal_id);
  if (signal)
    return signal->name;
  
  return NULL;
}

void
gtk_signal_emitv (GtkObject           *object,
		  guint                signal_id,
		  GtkArg              *params)
{
  GtkSignal *signal;

  g_return_if_fail (object != NULL);
  g_return_if_fail (signal_id >= 1);
  
  signal = LOOKUP_SIGNAL_ID (signal_id);
  g_return_if_fail (signal != NULL);
  g_return_if_fail (gtk_type_is_a (GTK_OBJECT_TYPE (object), signal->object_type));

  if (signal->nparams > 0)
    g_return_if_fail (params != NULL);

  gtk_signal_real_emit (object, signal_id, params);
}

void
gtk_signal_emit (GtkObject *object,
		 guint	    signal_id,
		 ...)
{
  GtkSignal *signal;
  va_list    args;
  GtkArg     params[MAX_SIGNAL_PARAMS + 1];
  gboolean   abort;

  g_return_if_fail (object != NULL);
  g_return_if_fail (signal_id >= 1);
  
  signal = LOOKUP_SIGNAL_ID (signal_id);
  g_return_if_fail (signal != NULL);
  g_return_if_fail (gtk_type_is_a (GTK_OBJECT_TYPE (object), signal->object_type));

  va_start (args, signal_id);
  abort = gtk_signal_collect_params (params,
				     signal->nparams,
				     signal->params,
				     signal->return_val,
				     args);
  va_end (args);

  if (!abort)
    gtk_signal_real_emit (object, signal_id, params);
}

void
gtk_signal_emitv_by_name (GtkObject           *object,
			  const gchar         *name,
			  GtkArg              *params)
{
  guint signal_id;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (name != NULL);
  g_return_if_fail (params != NULL);
  
  signal_id = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  
  if (signal_id >= 1)
    {
      GtkSignal *signal;
      
      signal = LOOKUP_SIGNAL_ID (signal_id);
      g_return_if_fail (signal != NULL);
      g_return_if_fail (gtk_type_is_a (GTK_OBJECT_TYPE (object), signal->object_type));

      gtk_signal_real_emit (object, signal_id, params);
    }
  else
    {
      g_warning ("gtk_signal_emitv_by_name(): could not find signal \"%s\" in the `%s' class ancestry",
		 name,
		 gtk_type_name (GTK_OBJECT_TYPE (object)));
    }
}

void
gtk_signal_emit_by_name (GtkObject	 *object,
			 const gchar	 *name,
			 ...)
{
  guint signal_id;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (name != NULL);
  
  signal_id = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  
  if (signal_id >= 1)
    {
      GtkSignal *signal;
      GtkArg     params[MAX_SIGNAL_PARAMS + 1];
      va_list    args;
      gboolean   abort;
      
      signal = LOOKUP_SIGNAL_ID (signal_id);
      g_return_if_fail (signal != NULL);
      g_return_if_fail (gtk_type_is_a (GTK_OBJECT_TYPE (object), signal->object_type));

      va_start (args, name);
      abort = gtk_signal_collect_params (params,
					 signal->nparams,
					 signal->params,
					 signal->return_val,
					 args);
      va_end (args);

      if (!abort)
	gtk_signal_real_emit (object, signal_id, params);
    }
  else
    {
      g_warning ("gtk_signal_emit_by_name(): could not find signal \"%s\" in the `%s' class ancestry",
		 name,
		 gtk_type_name (GTK_OBJECT_TYPE (object)));
    }
}

void
gtk_signal_emit_stop (GtkObject *object,
		      guint	  signal_id)
{
  gint state;

  g_return_if_fail (object != NULL);
  g_return_if_fail (signal_id >= 1);
  
  state = gtk_emission_check (current_emissions, object, signal_id);
  if (state > 1)
    g_warning ("gtk_signal_emit_stop(): emission (%u) for object `%s' cannot be stopped from emission hook",
	       signal_id,
	       gtk_type_name (GTK_OBJECT_TYPE (object)));
  else if (state)
    {
      if (!gtk_emission_check (stop_emissions, object, signal_id))
	gtk_emission_add (&stop_emissions, object, signal_id);
    }
  else
    g_warning ("gtk_signal_emit_stop(): no current emission (%u) for object `%s'",
	       signal_id,
	       gtk_type_name (GTK_OBJECT_TYPE (object)));
}

void
gtk_signal_emit_stop_by_name (GtkObject	      *object,
			      const gchar     *name)
{
  guint signal_id;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (name != NULL);
  
  signal_id = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (signal_id)
    gtk_signal_emit_stop (object, signal_id);
  else
    g_warning ("gtk_signal_emit_stop_by_name(): could not find signal \"%s\" in the `%s' class ancestry",
	       name,
	       gtk_type_name (GTK_OBJECT_TYPE (object)));
}

guint
gtk_signal_n_emissions (GtkObject  *object,
			guint	    signal_id)
{
  GtkEmission *emission;
  guint n;
  
  g_return_val_if_fail (object != NULL, 0);
  g_return_val_if_fail (GTK_IS_OBJECT (object), 0);
  
  n = 0;
  for (emission = current_emissions; emission; emission = emission->next)
    {
      if (emission->object == object && emission->signal_id == signal_id)
	n++;
    }
  
  return n;
}

guint
gtk_signal_n_emissions_by_name (GtkObject	*object,
				const gchar	*name)
{
  guint signal_id;
  guint n;
  
  g_return_val_if_fail (object != NULL, 0);
  g_return_val_if_fail (GTK_IS_OBJECT (object), 0);
  g_return_val_if_fail (name != NULL, 0);
  
  signal_id = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (signal_id)
    n = gtk_signal_n_emissions (object, signal_id);
  else
    {
      g_warning ("gtk_signal_n_emissions_by_name(): could not find signal \"%s\" in the `%s' class ancestry",
		 name,
		 gtk_type_name (GTK_OBJECT_TYPE (object)));
      n = 0;
    }
  
  return n;
}

guint
gtk_signal_connect (GtkObject	  *object,
		    const gchar	  *name,
		    GtkSignalFunc  func,
		    gpointer	   func_data)
{
  guint signal_id;
  
  g_return_val_if_fail (object != NULL, 0);
  g_return_val_if_fail (GTK_IS_OBJECT (object), 0);
  
  signal_id = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!signal_id)
    {
      g_warning ("gtk_signal_connect(): could not find signal \"%s\" in the `%s' class ancestry",
		 name,
		 gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }
  
  return gtk_signal_connect_by_type (object, signal_id, 
				     func, func_data, NULL,
				     FALSE, FALSE, FALSE);
}

guint
gtk_signal_connect_after (GtkObject	*object,
			  const gchar	*name,
			  GtkSignalFunc	 func,
			  gpointer	 func_data)
{
  guint signal_id;
  
  g_return_val_if_fail (object != NULL, 0);
  
  signal_id = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!signal_id)
    {
      g_warning ("gtk_signal_connect_after(): could not find signal \"%s\" in the `%s' class ancestry",
		 name,
		 gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }
  
  return gtk_signal_connect_by_type (object, signal_id, 
				     func, func_data, NULL,
				     FALSE, TRUE, FALSE);
}

guint	
gtk_signal_connect_full (GtkObject	     *object,
			 const gchar	     *name,
			 GtkSignalFunc	      func,
			 GtkCallbackMarshal   marshal,
			 gpointer	      func_data,
			 GtkDestroyNotify     destroy_func,
			 gint		      object_signal,
			 gint		      after)
{
  guint signal_id;
  
  g_return_val_if_fail (object != NULL, 0);
  
  signal_id = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!signal_id)
    {
      g_warning ("gtk_signal_connect_full(): could not find signal \"%s\" in the `%s' class ancestry",
		 name,
		 gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }
  
  if (marshal)
    return gtk_signal_connect_by_type (object, signal_id, (GtkSignalFunc) marshal, 
				       func_data, destroy_func, 
				       object_signal, after, TRUE);
  else
    return gtk_signal_connect_by_type (object, signal_id, func, 
				       func_data, destroy_func, 
				       object_signal, after, FALSE);
}

guint
gtk_signal_connect_object (GtkObject	 *object,
			   const gchar	 *name,
			   GtkSignalFunc  func,
			   GtkObject	 *slot_object)
{
  guint signal_id;
  
  g_return_val_if_fail (object != NULL, 0);
  /* slot_object needs to be treated as ordinary pointer
   */
  
  signal_id = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!signal_id)
    {
      g_warning ("gtk_signal_connect_object(): could not find signal \"%s\" in the `%s' class ancestry",
		 name,
		 gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }
  
  return gtk_signal_connect_by_type (object, signal_id, 
				     func, slot_object, NULL,
				     TRUE, FALSE, FALSE);
}

guint
gtk_signal_connect_object_after (GtkObject     *object,
				 const gchar   *name,
				 GtkSignalFunc	func,
				 GtkObject     *slot_object)
{
  guint signal_id;
  
  g_return_val_if_fail (object != NULL, 0);
  
  signal_id = gtk_signal_lookup (name, GTK_OBJECT_TYPE (object));
  if (!signal_id)
    {
      g_warning ("gtk_signal_connect_object_after(): could not find signal \"%s\" in the `%s' class ancestry",
		 name,
		 gtk_type_name (GTK_OBJECT_TYPE (object)));
      return 0;
    }
  
  return gtk_signal_connect_by_type (object, signal_id, 
				     func, slot_object, NULL,
				     TRUE, TRUE, FALSE);
}

void
gtk_signal_connect_while_alive (GtkObject	 *object,
				const gchar	 *signal,
				GtkSignalFunc	  func,
				gpointer	  func_data,
				GtkObject	 *alive_object)
{
  GtkDisconnectInfo *info;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (signal != NULL);
  g_return_if_fail (func != NULL);
  g_return_if_fail (alive_object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (alive_object));
  
  info = g_chunk_new (GtkDisconnectInfo, gtk_disconnect_info_mem_chunk);
  info->object1 = object;
  info->object2 = alive_object;
  
  info->signal_handler = gtk_signal_connect (object, signal, func, func_data);
  info->disconnect_handler1 =
    gtk_signal_connect_object (info->object1,
			       "destroy",
			       GTK_SIGNAL_FUNC (gtk_alive_disconnecter),
			       (GtkObject*) info);
  info->disconnect_handler2 =
    gtk_signal_connect_object (info->object2,
			       "destroy",
			       GTK_SIGNAL_FUNC (gtk_alive_disconnecter),
			       (GtkObject*) info);
}

void
gtk_signal_connect_object_while_alive (GtkObject	*object,
				       const gchar	*signal,
				       GtkSignalFunc	 func,
				       GtkObject	*alive_object)
{
  GtkDisconnectInfo *info;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (signal != NULL);
  g_return_if_fail (func != NULL);
  g_return_if_fail (alive_object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (alive_object));
  
  info = g_chunk_new (GtkDisconnectInfo, gtk_disconnect_info_mem_chunk);
  info->object1 = object;
  info->object2 = alive_object;
  
  info->signal_handler = gtk_signal_connect_object (object, signal, func, alive_object);
  info->disconnect_handler1 =
    gtk_signal_connect_object (info->object1,
			       "destroy",
			       GTK_SIGNAL_FUNC (gtk_alive_disconnecter),
			       (GtkObject*) info);
  info->disconnect_handler2 =
    gtk_signal_connect_object (info->object2,
			       "destroy",
			       GTK_SIGNAL_FUNC (gtk_alive_disconnecter),
			       (GtkObject*) info);
}

void
gtk_signal_disconnect (GtkObject *object,
		       guint	  handler_id)
{
  GtkHandler *handler;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (handler_id > 0);
  
  handler = gtk_object_get_data_by_id (object, gtk_handler_quark);
  
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
			       gpointer	      data)
{
  GtkHandler *handler;
  gint found_one;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (func != NULL);
  
  found_one = FALSE;
  handler = gtk_object_get_data_by_id (object, gtk_handler_quark);
  
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
			       gpointer	  data)
{
  GtkHandler *handler;
  gint found_one;
  
  g_return_if_fail (object != NULL);
  
  found_one = FALSE;
  handler = gtk_object_get_data_by_id (object, gtk_handler_quark);
  
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
  
  handler = gtk_object_get_data_by_id (object, gtk_handler_quark);
  
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
gtk_signal_handler_block_by_func (GtkObject	*object,
				  GtkSignalFunc	 func,
				  gpointer	 data)
{
  GtkHandler *handler;
  gint found_one;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (func != NULL);
  
  found_one = FALSE;
  handler = gtk_object_get_data_by_id (object, gtk_handler_quark);
  
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
  handler = gtk_object_get_data_by_id (object, gtk_handler_quark);
  
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
  
  handler = gtk_object_get_data_by_id (object, gtk_handler_quark);
  
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
gtk_signal_handler_unblock_by_func (GtkObject	  *object,
				    GtkSignalFunc  func,
				    gpointer	   data)
{
  GtkHandler *handler;
  gint found_one;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (func != NULL);
  
  found_one = FALSE;
  handler = gtk_object_get_data_by_id (object, gtk_handler_quark);
  
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
  handler = gtk_object_get_data_by_id (object, gtk_handler_quark);
  
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
  
  handler = gtk_object_get_data_by_id (object, gtk_handler_quark);
  if (handler)
    {
      handler = handler->next;
      while (handler)
	{
	  GtkHandler *next;
	  
	  next = handler->next;
	  if (handler->id > 0)
	    {
	      handler->id = 0;
	      handler->blocked += 1;
	      gtk_signal_handler_unref (handler, object);
	    }
	  handler = next;
	}
      handler = gtk_object_get_data_by_id (object, gtk_handler_quark);
      if (handler->id > 0)
	{
	  handler->id = 0;
	  handler->blocked += 1;
	  gtk_signal_handler_unref (handler, object);
	}
    }
}

void
gtk_signal_set_funcs (GtkSignalMarshal marshal_func,
		      GtkSignalDestroy destroy_func)
{
  global_marshaller = marshal_func;
  global_destroy_notify = destroy_func;
}

static guint
gtk_signal_hash (gconstpointer h)
{
  register const GtkSignalHash *hash = h;
  
  return hash->object_type ^ hash->quark;
}

static gint
gtk_signal_compare (gconstpointer h1,
		    gconstpointer h2)
{
  register const GtkSignalHash *hash1 = h1;
  register const GtkSignalHash *hash2 = h2;
  
  return (hash1->quark == hash2->quark &&
	  hash1->object_type == hash2->object_type);
}

static guint
gtk_alive_disconnecter (GtkDisconnectInfo *info)
{
  g_return_val_if_fail (info != NULL, 0);
  
  gtk_signal_disconnect (info->object1, info->disconnect_handler1);
  gtk_signal_disconnect (info->object1, info->signal_handler);
  gtk_signal_disconnect (info->object2, info->disconnect_handler2);
  
  g_mem_chunk_free (gtk_disconnect_info_mem_chunk, info);
  
  return 0;
}

static GtkHandler*
gtk_signal_handler_new (void)
{
  GtkHandler *handler;

  if (!gtk_handler_free_list)
    {
      GtkHandler *handler_block;
      guint i;

      handler_block = g_new0 (GtkHandler, HANDLER_BLOCK_SIZE);
      for (i = 1; i < HANDLER_BLOCK_SIZE; i++)
	{
	  (handler_block + i)->next = gtk_handler_free_list;
	  gtk_handler_free_list = (handler_block + i);
	}
      
      handler = handler_block;
    }
  else
    {
      handler = gtk_handler_free_list;
      gtk_handler_free_list = handler->next;
    }
  
  handler->id = 0;
  handler->blocked = 0;
  handler->signal_id = 0;
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
  if (!handler->ref_count)
    {
      /* FIXME: i wanna get removed somewhen */
      g_warning ("gtk_signal_handler_unref(): handler with ref_count==0!");
      return;
    }
  
  handler->ref_count -= 1;
  
  if (handler->ref_count == 0)
    {
      if (handler->destroy_func)
	(* handler->destroy_func) (handler->func_data);
      else if (!handler->func && global_destroy_notify)
	(* global_destroy_notify) (handler->func_data);
      
      if (handler->prev)
	handler->prev->next = handler->next;
      else if (handler->next)
	gtk_object_set_data_by_id (object, gtk_handler_quark, handler->next);
      else
	{
	  GTK_OBJECT_UNSET_FLAGS (object, GTK_CONNECTED);
	  gtk_object_set_data_by_id (object, gtk_handler_quark, NULL);
	}
      if (handler->next)
	handler->next->prev = handler->prev;
      
      handler->next = gtk_handler_free_list;
      gtk_handler_free_list = handler;
    }
}

static void
gtk_signal_handler_insert (GtkObject  *object,
			   GtkHandler *handler)
{
  GtkHandler *tmp;
  
  /* FIXME: remove */ g_assert (handler->next == NULL);
  /* FIXME: remove */ g_assert (handler->prev == NULL);
  
  tmp = gtk_object_get_data_by_id (object, gtk_handler_quark);
  if (!tmp)
    {
      GTK_OBJECT_SET_FLAGS (object, GTK_CONNECTED);
      gtk_object_set_data_by_id (object, gtk_handler_quark, handler);
    }
  else
    while (tmp)
      {
	if (tmp->signal_id < handler->signal_id)
	  {
	    if (tmp->prev)
	      {
		tmp->prev->next = handler;
		handler->prev = tmp->prev;
	      }
	    else
	      gtk_object_set_data_by_id (object, gtk_handler_quark, handler);
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


#ifdef  G_ENABLE_DEBUG
/* value typically set via gdb */
static GtkObject *gtk_trace_signal_object = NULL;
#endif  /* G_ENABLE_DEBUG */


static void
gtk_signal_real_emit (GtkObject *object,
		      guint      signal_id,
		      GtkArg	*params)
{
  GtkSignal     signal;
  GtkHandler	*handlers;
  GtkSignalFunc  signal_func;
  GtkEmission   *emission;

  /* gtk_handlers_run() expects a reentrant GtkSignal*, so we allocate
   * it locally on the stack. we save some lookups ourselves with this as well.
   */
  signal = *LOOKUP_SIGNAL_ID (signal_id);
  if (signal.function_offset)
    signal_func = G_STRUCT_MEMBER (GtkSignalFunc, object->klass, signal.function_offset);
  else
    signal_func = NULL;
  
#ifdef  G_ENABLE_DEBUG
  if (gtk_debug_flags & GTK_DEBUG_SIGNALS ||
      object == gtk_trace_signal_object)
    g_message ("%s::%s emitted (object=%p class-method=%p)\n",
	       gtk_type_name (GTK_OBJECT_TYPE (object)),
	       signal.name,
	       object,
	       signal_func);
#endif  /* G_ENABLE_DEBUG */
  
  if (signal.signal_flags & GTK_RUN_NO_RECURSE)
    {
      gint state;
      
      state = gtk_emission_check (current_emissions, object, signal_id);
      if (state)
	{
	  if (state > 1)
	    g_warning ("gtk_signal_real_emit(): emission (%u) for object `%s' cannot be restarted from emission hook",
		       signal_id,
		       gtk_type_name (GTK_OBJECT_TYPE (object)));
	  else if (!gtk_emission_check (restart_emissions, object, signal_id))
	    gtk_emission_add (&restart_emissions, object, signal_id);
	  
	  return;
	}
    }
  
  gtk_object_ref (object);
  
  gtk_emission_add (&current_emissions, object, signal_id);
  emission = current_emissions;
  
 emission_restart:
  
  if (signal.signal_flags & GTK_RUN_FIRST && signal_func)
    {
      signal.marshaller (object, signal_func, NULL, params);
      
      if (stop_emissions && gtk_emission_check (stop_emissions, object, signal_id))
	{
	  gtk_emission_remove (&stop_emissions, object, signal_id);
	  goto emission_done;
	}
      else if (restart_emissions &&
	       signal.signal_flags & GTK_RUN_NO_RECURSE &&
	       gtk_emission_check (restart_emissions, object, signal_id))
	{
	  gtk_emission_remove (&restart_emissions, object, signal_id);
	  
	  goto emission_restart;
	}
    }
  
  if (signal.hook_list && !GTK_OBJECT_DESTROYED (object))
    {
      GtkEmissionHookData data;

      data.object = object;
      data.n_params = signal.nparams;
      data.params = params;
      data.signal_id = signal_id;
      emission->in_hook = 1;
      g_hook_list_marshal_check (signal.hook_list, TRUE, gtk_emission_hook_marshaller, &data);
      emission->in_hook = 0;
    }

  if (GTK_OBJECT_CONNECTED (object))
    {
      handlers = gtk_signal_get_handlers (object, signal_id);
      if (handlers)
	{
	  gint return_val;
	  
	  return_val = gtk_handlers_run (handlers, &signal, object, params, FALSE);
	  switch (return_val)
	    {
	    case EMISSION_CONTINUE:
	      break;
	    case EMISSION_RESTART:
	      goto emission_restart;
	    case EMISSION_DONE:
	      goto emission_done;
	    }
	}
    }
  
  if (signal.signal_flags & GTK_RUN_LAST && signal_func)
    {
      signal.marshaller (object, signal_func, NULL, params);
      
      if (stop_emissions && gtk_emission_check (stop_emissions, object, signal_id))
	{
	  gtk_emission_remove (&stop_emissions, object, signal_id);
	  goto emission_done;
	}
      else if (restart_emissions &&
	       signal.signal_flags & GTK_RUN_NO_RECURSE &&
	       gtk_emission_check (restart_emissions, object, signal_id))
	{
	  gtk_emission_remove (&restart_emissions, object, signal_id);
	  
	  goto emission_restart;
	}
    }
  
  if (GTK_OBJECT_CONNECTED (object))
    {
      handlers = gtk_signal_get_handlers (object, signal_id);
      if (handlers)
	{
	  gint return_val;
	  
	  return_val = gtk_handlers_run (handlers, &signal, object, params, TRUE);
	  switch (return_val)
	    {
	    case  EMISSION_CONTINUE:
	      break;
	    case  EMISSION_RESTART:
	      goto emission_restart;
	    case  EMISSION_DONE:
	      goto emission_done;
	    }
	}
    }
  
 emission_done:
  if (restart_emissions && signal.signal_flags & GTK_RUN_NO_RECURSE)
    gtk_emission_remove (&restart_emissions, object, signal_id);
  
  gtk_emission_remove (&current_emissions, object, signal_id);
  
  gtk_object_unref (object);
}

guint
gtk_signal_handler_pending (GtkObject		*object,
			    guint		 signal_id,
			    gboolean		 may_be_blocked)
{
  GtkHandler *handlers;
  guint handler_id;
  
  g_return_val_if_fail (object != NULL, 0);
  g_return_val_if_fail (signal_id >= 1, 0);

  if (GTK_OBJECT_CONNECTED (object))
    handlers = gtk_signal_get_handlers (object, signal_id);
  else
    return 0;
  
  handler_id = 0;
  while (handlers && handlers->signal_id == signal_id)
    {
      if (handlers->id > 0 &&
	  (may_be_blocked || handlers->blocked == FALSE))
	{
	  handler_id = handlers->id;
	  break;
	}
      
      handlers = handlers->next;
    }
  
  return handler_id;
}

guint
gtk_signal_handler_pending_by_func (GtkObject           *object,
				    guint                signal_id,
				    gboolean             may_be_blocked,
				    GtkSignalFunc        func,
				    gpointer             data)
{
  GtkHandler *handlers;
  guint handler_id;
  
  g_return_val_if_fail (object != NULL, 0);
  g_return_val_if_fail (func != NULL, 0);
  g_return_val_if_fail (signal_id >= 1, 0);

  if (GTK_OBJECT_CONNECTED (object))
    handlers = gtk_signal_get_handlers (object, signal_id);
  else
    return 0;
  
  handler_id = 0;
  while (handlers && handlers->signal_id == signal_id)
    {
      if (handlers->id > 0 &&
	  handlers->func == func &&
	  handlers->func_data == data &&
	  (may_be_blocked || handlers->blocked == 0))
	{
	  handler_id = handlers->id;
	  break;
	}
      
      handlers = handlers->next;
    }
  
  return handler_id;
}

gint
gtk_signal_handler_pending_by_id (GtkObject *object,
				  guint      handler_id,
				  gboolean   may_be_blocked)
{
  GtkHandler *handlers;
  
  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (handler_id >= 1, FALSE);

  if (GTK_OBJECT_CONNECTED (object))
    handlers = gtk_object_get_data_by_id (object, gtk_handler_quark);
  else
    return FALSE;
  
  while (handlers)
    {
      if (handlers->id == handler_id)
	return may_be_blocked || handlers->blocked == 0;
      
      handlers = handlers->next;
    }
  
  return FALSE;
}

guint
gtk_signal_add_emission_hook (guint           signal_id,
			      GtkEmissionHook hook_func,
			      gpointer        data)
{
  return gtk_signal_add_emission_hook_full (signal_id, hook_func, data, NULL);
}

guint
gtk_signal_add_emission_hook_full (guint           signal_id,
				   GtkEmissionHook hook_func,
				   gpointer        data,
				   GDestroyNotify  destroy)
{
  static guint seq_hook_id = 1;
  GtkSignal *signal;
  GHook *hook;

  g_return_val_if_fail (signal_id > 0, 0);
  g_return_val_if_fail (hook_func != NULL, 0);
  
  signal = LOOKUP_SIGNAL_ID (signal_id);
  g_return_val_if_fail (signal != NULL, 0);
  if (signal->signal_flags & GTK_RUN_NO_HOOKS)
    {
      g_warning ("gtk_signal_add_emission_hook_full(): signal \"%s\" does not support emission hooks",
		 signal->name);
      return 0;
    }

  if (!signal->hook_list)
    {
      signal->hook_list = g_new (GHookList, 1);
      g_hook_list_init (signal->hook_list, sizeof (GHook));
    }

  hook = g_hook_alloc (signal->hook_list);
  hook->data = data;
  hook->func = (gpointer)hook_func;
  hook->destroy = destroy;

  signal->hook_list->seq_id = seq_hook_id;
  g_hook_prepend (signal->hook_list, hook);
  seq_hook_id = signal->hook_list->seq_id;

  return hook->hook_id;
}

void
gtk_signal_remove_emission_hook (guint signal_id,
				 guint hook_id)
{
  GtkSignal *signal;

  g_return_if_fail (signal_id > 0);
  g_return_if_fail (hook_id > 0);

  signal = LOOKUP_SIGNAL_ID (signal_id);
  g_return_if_fail (signal != NULL);

  if (!signal->hook_list || !g_hook_destroy (signal->hook_list, hook_id))
    g_warning ("gtk_signal_remove_emission_hook(): could not find hook (%u)", hook_id);
}

static gboolean
gtk_emission_hook_marshaller (GHook   *hook,
			      gpointer data_p)
{
  GtkEmissionHookData *data = data_p;
  GtkEmissionHook func;

  func = (gpointer)hook->func;

  if (!GTK_OBJECT_DESTROYED (data->object))
    return func (data->object, data->signal_id,
		 data->n_params, data->params,
		 hook->data);
  else
    return TRUE;
}

static guint
gtk_signal_connect_by_type (GtkObject	    *object,
			    guint	     signal_id,
			    GtkSignalFunc    func,
			    gpointer	     func_data,
			    GtkSignalDestroy destroy_func,
			    gint	     object_signal,
			    gint	     after,
			    gint	     no_marshal)
{
  GtkObjectClass *class;
  GtkHandler *handler;
  gint found_it;
  GtkSignal *signal;
 
  g_return_val_if_fail (object != NULL, 0);
  g_return_val_if_fail (object->klass != NULL, 0);
  
  signal = LOOKUP_SIGNAL_ID (signal_id);

  /* Search through the signals for this object and make
   *  sure the one we are adding is valid. We need to perform
   *  the lookup on the objects parents as well. If it isn't
   *  valid then issue a warning and return.
   * As of now (1998-05-27) this lookup shouldn't be neccessarry
   *  anymore since gtk_signal_lookup() has been reworked to only
   *  return correct signal ids per class-branch.
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
	if (object_signals[i] == signal_id)
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
		 signal_id,
		 gtk_type_name (object->klass->type));
      return 0;
    }
  
  handler = gtk_signal_handler_new ();
  handler->id = gtk_handler_id++;
  handler->signal_id = signal_id;
  handler->object_signal = object_signal != FALSE;
  handler->func = func;
  handler->func_data = func_data;
  handler->destroy_func = destroy_func;
  handler->after = after != FALSE;
  handler->no_marshal = no_marshal;
  
  gtk_signal_handler_insert (object, handler);
  return handler->id;
}

static GtkEmission*
gtk_emission_new (void)
{
  GtkEmission *emission;
  
  if (!gtk_free_emissions)
    {
      GtkEmission *emission_block;
      guint i;

      emission_block = g_new0 (GtkEmission, EMISSION_BLOCK_SIZE);
      for (i = 1; i < EMISSION_BLOCK_SIZE; i++)
	{
	  (emission_block + i)->next = gtk_free_emissions;
	  gtk_free_emissions = (emission_block + i);
	}

      emission = emission_block;
    }
  else
    {
      emission = gtk_free_emissions;
      gtk_free_emissions = emission->next;
    }

  emission->object = NULL;
  emission->signal_id = 0;
  emission->in_hook = 0;
  emission->next = NULL;
  
  return emission;
}

static void
gtk_emission_add (GtkEmission **emissions,
		  GtkObject    *object,
		  guint	        signal_id)
{
  GtkEmission *emission;
  
  g_return_if_fail (emissions != NULL);
  g_return_if_fail (object != NULL);
  
  emission = gtk_emission_new ();
  emission->object = object;
  emission->signal_id = signal_id;

  emission->next = *emissions;
  *emissions = emission;
}

static void
gtk_emission_remove (GtkEmission **emissions,
		     GtkObject	  *object,
		     guint	   signal_id)
{
  GtkEmission *emission, *last;
  
  g_return_if_fail (emissions != NULL);

  last = NULL;
  emission = *emissions;
  while (emission)
    {
      if (emission->object == object && emission->signal_id == signal_id)
	{
	  if (last)
	    last->next = emission->next;
	  else
	    *emissions = emission->next;

	  emission->next = gtk_free_emissions;
	  gtk_free_emissions = emission;
	  break;
	}

      last = emission;
      emission = last->next;
    }
}

static gint
gtk_emission_check (GtkEmission *emission,
		    GtkObject   *object,
		    guint        signal_id)
{
  while (emission)
    {
      if (emission->object == object && emission->signal_id == signal_id)
	return 1 + emission->in_hook;
      emission = emission->next;
    }
  return FALSE;
}

static gint
gtk_handlers_run (GtkHandler	 *handlers,
		  GtkSignal      *signal,
		  GtkObject      *object,
		  GtkArg	 *params,
		  gint		  after)
{
  /* *signal is a local copy on the stack of gtk_signal_real_emit(),
   * so we don't need to look it up every time we invoked a function.
   */
  while (handlers && handlers->signal_id == signal->signal_id)
    {
      GtkHandler *handlers_next;
      
      gtk_signal_handler_ref (handlers);
      
      if (!handlers->blocked && handlers->after == after)
	{
	  if (handlers->func)
	    {
	      if (handlers->no_marshal)
		(* (GtkCallbackMarshal) handlers->func) (object,
							 handlers->func_data,
							 signal->nparams,
							 params);
	      else if (handlers->object_signal)
		/* don't cast with GTK_OBJECT () */
		(* signal->marshaller) ((GtkObject*) handlers->func_data,
					handlers->func,
					object,
					params);
	      else
		(* signal->marshaller) (object,
					handlers->func,
					handlers->func_data,
					params);
	    }
	  else if (global_marshaller)
	    (* global_marshaller) (object,
				   handlers->func_data,
				   signal->nparams,
				   params,
				   signal->params,
				   signal->return_val);
	  
	  if (stop_emissions && gtk_emission_check (stop_emissions,
						    object,
						    signal->signal_id))
	    {
	      gtk_emission_remove (&stop_emissions, object, signal->signal_id);
	      
	      gtk_signal_handler_unref (handlers, object);

	      return EMISSION_DONE;
	    }
	  else if (restart_emissions &&
		   signal->signal_flags & GTK_RUN_NO_RECURSE &&
		   gtk_emission_check (restart_emissions, object, signal->signal_id))
	    {
	      gtk_emission_remove (&restart_emissions, object, signal->signal_id);

	      gtk_signal_handler_unref (handlers, object);

	      return EMISSION_RESTART;
	    }
	}
      
      handlers_next = handlers->next;
      gtk_signal_handler_unref (handlers, object);
      handlers = handlers_next;
    }
  
  return EMISSION_CONTINUE;
}

static gboolean
gtk_signal_collect_params (GtkArg	       *params,
			   guint		n_params,
			   GtkType	       *param_types,
			   GtkType		return_type,
			   va_list		var_args)
{
  register GtkArg *last_param;
  register gboolean failed = FALSE;

  for (last_param = params + n_params; params < last_param; params++)
    {
      register gchar *error;

      params->name = NULL;
      params->type = *(param_types++);
      GTK_ARG_COLLECT_VALUE (params,
			     var_args,
			     error);
      if (error)
	{
	  failed = TRUE;
	  g_warning ("gtk_signal_collect_params(): %s", error);
	  g_free (error);
	}
    }

  params->type = return_type;
  params->name = NULL;

  return_type = GTK_FUNDAMENTAL_TYPE (return_type);
  if (return_type != GTK_TYPE_NONE)
    {
      if ((return_type >= GTK_TYPE_FLAT_FIRST &&
	   return_type <= GTK_TYPE_FLAT_LAST) ||
	  (return_type == GTK_TYPE_OBJECT))
	{
	  GTK_VALUE_POINTER (*params) = va_arg (var_args, gpointer);
	  
	  if (GTK_VALUE_POINTER (*params) == NULL)
	    {
	      failed = TRUE;
	      g_warning ("gtk_signal_collect_params(): invalid NULL pointer for return argument type `%s'",
			 gtk_type_name (params->type));
	    }
	}
      else
	{
	  failed = TRUE;
	  g_warning ("gtk_signal_collect_params(): unsupported return argument type `%s'",
		     gtk_type_name (params->type));
	}
    }
  else
    GTK_VALUE_POINTER (*params) = NULL;

  return failed;
}
