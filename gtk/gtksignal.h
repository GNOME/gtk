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
#ifndef __GTK_SIGNAL_H__
#define __GTK_SIGNAL_H__


#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkobject.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

  
#ifdef offsetof
#define GTK_SIGNAL_OFFSET(t, f) ((gint) offsetof (t, f))
#else /* offsetof */
#define GTK_SIGNAL_OFFSET(t, f) ((gint) ((gchar*) &((t*) 0)->f))
#endif /* offsetof */
  
  
typedef void (*GtkSignalMarshal)    (GtkObject	    *object,
				     gpointer	     data,
				     guint	     nparams,
				     GtkArg	    *args,
				     GtkType	    *arg_types,
				     GtkType	     return_type);
typedef void (*GtkSignalDestroy)    (gpointer	     data);

typedef struct _GtkSignalQuery		GtkSignalQuery;

struct	_GtkSignalQuery
{
  GtkType	   object_type;
  guint		   signal_id;
  const gchar	  *signal_name;
  guint		   is_user_signal : 1;
  GtkSignalRunType run_type;
  GtkType	   return_val;
  guint		   nparams;
  const GtkType	  *params;
};

void   gtk_signal_init			  (void);
guint  gtk_signal_new			  (const gchar	       *name,
					   GtkSignalRunType	run_type,
					   GtkType		object_type,
					   guint		function_offset,
					   GtkSignalMarshaller	marshaller,
					   GtkType		return_val,
					   guint		nparams,
					   ...);
guint  gtk_signal_newv			  (const gchar	       *name,
					   GtkSignalRunType	run_type,
					   GtkType		object_type,
					   guint		function_offset,
					   GtkSignalMarshaller	marshaller,
					   GtkType		return_val,
					   guint		nparams,
					   GtkType	       *params);
guint  gtk_signal_lookup		  (const gchar	       *name,
					   GtkType		object_type);
gchar* gtk_signal_name			  (guint		signal_id);
void   gtk_signal_emit			  (GtkObject	       *object,
					   guint		signal_id,
					   ...);
void   gtk_signal_emit_by_name		  (GtkObject	       *object,
					   const gchar	       *name,
					   ...);
void   gtk_signal_emitv			  (GtkObject           *object,
					   guint                signal_id,
					   GtkArg              *params);
void   gtk_signal_emitv_by_name		  (GtkObject           *object,
					   const gchar	       *name,
					   GtkArg              *params);
guint  gtk_signal_n_emissions		  (GtkObject   	       *object,
					   guint                signal_id);
guint  gtk_signal_n_emissions_by_name	  (GtkObject   	       *object,
					   const gchar         *name);
void   gtk_signal_emit_stop		  (GtkObject	       *object,
					   guint		signal_id);
void   gtk_signal_emit_stop_by_name	  (GtkObject	       *object,
					   const gchar	       *name);
guint  gtk_signal_connect		  (GtkObject	       *object,
					   const gchar	       *name,
					   GtkSignalFunc	func,
					   gpointer		func_data);
guint  gtk_signal_connect_after		  (GtkObject	       *object,
					   const gchar	       *name,
					   GtkSignalFunc	func,
					   gpointer		func_data);
guint  gtk_signal_connect_object	  (GtkObject	       *object,
					   const gchar	       *name,
					   GtkSignalFunc	func,
					   GtkObject	       *slot_object);
guint  gtk_signal_connect_object_after	  (GtkObject	       *object,
					   const gchar	       *name,
					   GtkSignalFunc	func,
					   GtkObject	       *slot_object);
guint  gtk_signal_connect_full		  (GtkObject	       *object,
					   const gchar	       *name,
					   GtkSignalFunc	func,
					   GtkCallbackMarshal	marshal,
					   gpointer		data,
					   GtkDestroyNotify	destroy_func,
					   gint			object_signal,
					   gint			after);
guint  gtk_signal_connect_interp	  (GtkObject	       *object,
					   const gchar	       *name,
					   GtkCallbackMarshal	func,
					   gpointer		data,
					   GtkDestroyNotify	destroy_func,
					   gint			after);

void   gtk_signal_connect_object_while_alive (GtkObject	       *object,
					      const gchar      *signal,
					      GtkSignalFunc	func,
					      GtkObject	       *alive_object);
void   gtk_signal_connect_while_alive	     (GtkObject	       *object,
					      const gchar      *signal,
					      GtkSignalFunc	func,
					      gpointer		func_data,
					      GtkObject	       *alive_object);

void   gtk_signal_disconnect		  (GtkObject	       *object,
					   guint		handler_id);
void   gtk_signal_disconnect_by_func	  (GtkObject	       *object,
					   GtkSignalFunc	func,
					   gpointer		data);
void   gtk_signal_disconnect_by_data	  (GtkObject	       *object,
					   gpointer		data);
void   gtk_signal_handler_block		  (GtkObject	       *object,
					   guint		handler_id);
void   gtk_signal_handler_block_by_func	  (GtkObject	       *object,
					   GtkSignalFunc	func,
					   gpointer		data);
void   gtk_signal_handler_block_by_data	  (GtkObject	       *object,
					   gpointer		data);
void   gtk_signal_handler_unblock	  (GtkObject	       *object,
					   guint		handler_id);
void   gtk_signal_handler_unblock_by_func (GtkObject	       *object,
					   GtkSignalFunc	func,
					   gpointer		data);
void   gtk_signal_handler_unblock_by_data (GtkObject	       *object,
					   gpointer		data);
guint  gtk_signal_handler_pending	  (GtkObject	       *object,
					   guint		signal_id,
					   gboolean		may_be_blocked);
void   gtk_signal_handlers_destroy	  (GtkObject	       *object);
void   gtk_signal_default_marshaller	  (GtkObject	       *object,
					   GtkSignalFunc	func,
					   gpointer		func_data,
					   GtkArg	       *args);
void   gtk_signal_set_funcs		  (GtkSignalMarshal	marshal_func,
					   GtkSignalDestroy	destroy_func);

/* Report internal information about a signal. The caller has the response
 *  to invoke a supsequent g_free (returned_data); but must leave the
 *  contents of GtkSignalQuery untouched.
 */
GtkSignalQuery* gtk_signal_query	  (guint		signal_id);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SIGNAL_H__ */
