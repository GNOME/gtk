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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#ifndef __GTK_SIGNAL_H__
#define __GTK_SIGNAL_H__


#include <gtk/gtkenums.h>
#include <gtk/gtkobject.h>
#include <gtk/gtkmarshal.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

  

#define	gtk_signal_default_marshaller	gtk_marshal_VOID__VOID


/* --- compat defines --- */
#define GTK_SIGNAL_OFFSET	                      GTK_STRUCT_OFFSET
#define	gtk_signal_lookup			      g_signal_lookup
#define	gtk_signal_name				      g_signal_name
#define	gtk_signal_emit_stop(i,s)		      g_signal_stop_emission ((i), (s), 0)
#define	gtk_signal_connect(o,s,f,d)		      gtk_signal_connect_full ((o), (s), (f), 0, (d), 0, 0, 0)
#define	gtk_signal_connect_after(o,s,f,d)	      gtk_signal_connect_full ((o), (s), (f), 0, (d), 0, 0, 1)
#define	gtk_signal_connect_object(o,s,f,d)	      gtk_signal_connect_full ((o), (s), (f), 0, (d), 0, 1, 0)
#define	gtk_signal_connect_object_after(o,s,f,d)      gtk_signal_connect_full ((o), (s), (f), 0, (d), 0, 1, 1)
#define	gtk_signal_disconnect			      g_signal_handler_disconnect
#define	gtk_signal_handler_block		      g_signal_handler_block
#define	gtk_signal_handler_unblock		      g_signal_handler_unblock
#define	gtk_signal_disconnect_by_func(o,f,d)	      gtk_signal_compat_matched ((o), (f), (d), G_SIGNAL_MATCH_FUNC | \
                                                                                                G_SIGNAL_MATCH_DATA, 0)
#define	gtk_signal_disconnect_by_data(o,d)	      gtk_signal_compat_matched ((o), 0, (d), G_SIGNAL_MATCH_DATA, 0)
#define	gtk_signal_handler_block_by_func(o,f,d)	      gtk_signal_compat_matched ((o), (f), (d), G_SIGNAL_MATCH_FUNC | \
                                                                                                G_SIGNAL_MATCH_DATA, 1)
#define	gtk_signal_handler_block_by_data(o,d)	      gtk_signal_compat_matched ((o), 0, (d), G_SIGNAL_MATCH_DATA, 1)
#define	gtk_signal_handler_unblock_by_func(o,f,d)     gtk_signal_compat_matched ((o), (f), (d), G_SIGNAL_MATCH_FUNC | \
                                                                                                G_SIGNAL_MATCH_DATA, 2)
#define	gtk_signal_handler_unblock_by_data(o,d)	      gtk_signal_compat_matched ((o), 0, (d), G_SIGNAL_MATCH_DATA, 2)
#define	gtk_signal_handler_pending(i,s,b)	      g_signal_has_handler_pending ((i), 0, (s), (b))
#define	gtk_signal_handler_pending_by_func(o,s,b,f,d) (g_signal_handler_find ((o), G_SIGNAL_MATCH_ID | \
			                                                           G_SIGNAL_MATCH_FUNC | \
                                                                                   G_SIGNAL_MATCH_DATA | \
                                                                                   ((b) ? 0 : G_SIGNAL_MATCH_UNBLOCKED), \
                                   			                           (s), 0, 0, (f), (d)) != 0)


/* --- compat functions --- */
guint	gtk_signal_newv				(const gchar	    *name,
						 GtkSignalRunType    signal_flags,
						 GtkType	     object_type,
						 guint		     function_offset,
						 GtkSignalMarshaller marshaller,
						 GtkType	     return_val,
						 guint		     n_args,
						 GtkType	    *args);
guint	gtk_signal_new				(const gchar	    *name,
						 GtkSignalRunType    signal_flags,
						 GtkType	     object_type,
						 guint		     function_offset,
						 GtkSignalMarshaller marshaller,
						 GtkType	     return_val,
						 guint		     n_args,
						 ...);
void	gtk_signal_emit_stop_by_name		(GtkObject	    *object,
						 const gchar	    *name);
void	gtk_signal_connect_object_while_alive	(GtkObject	    *object,
						 const gchar        *signal,
						 GtkSignalFunc	     func,
						 GtkObject	    *alive_object);
void	gtk_signal_connect_while_alive		(GtkObject	    *object,
						 const gchar        *signal,
						 GtkSignalFunc	     func,
						 gpointer	     func_data,
						 GtkObject	    *alive_object);
guint	gtk_signal_connect_full			(GtkObject	    *object,
						 const gchar	    *name,
						 GtkSignalFunc	     func,
						 GtkCallbackMarshal  unsupported,
						 gpointer	     data,
						 GtkDestroyNotify    destroy_func,
						 gint		     object_signal,
						 gint		     after);
void	gtk_signal_emitv			(GtkObject	    *object,
						 guint		     signal_id,
						 GtkArg		    *args);
void	gtk_signal_emit				(GtkObject	    *object,
						 guint		     signal_id,
						 ...);
void	gtk_signal_emit_by_name			(GtkObject	    *object,
						 const gchar	    *name,
						 ...);
void	gtk_signal_emitv_by_name		(GtkObject	    *object,
						 const gchar	    *name,
						 GtkArg		    *args);
void	gtk_signal_compat_matched		(GtkObject	    *object,
						 GtkSignalFunc 	     func,
						 gpointer      	     data,
						 GSignalMatchType    match,
						 guint               action);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SIGNAL_H__ */
