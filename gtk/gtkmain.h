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
#ifndef __GTK_MAIN_H__
#define __GTK_MAIN_H__


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_PRIORITY_HIGH      -20
#define GTK_PRIORITY_INTERNAL  -10
#define GTK_PRIORITY_DEFAULT	 0
#define GTK_PRIORITY_LOW	10

typedef gint	(*GtkKeySnoopFunc)	(GtkWidget	*grab_widget,
					 GdkEventKey	*event,
					 gpointer	 func_data);

/* Gtk version.
 */
extern const guint gtk_major_version;
extern const guint gtk_minor_version;
extern const guint gtk_micro_version;

/* Initialization, exit, mainloop and miscellaneous routines
 */
void	   gtk_init		 (int	       *argc,
				  char	     ***argv);
void	   gtk_exit		 (gint		error_code);
gchar*	   gtk_set_locale	 (void);
gint	   gtk_events_pending	 (void);
void	   gtk_main		 (void);
guint	   gtk_main_level	 (void);
void	   gtk_main_quit	 (void);
gint	   gtk_main_iteration	 (void);
/* gtk_main_iteration() calls gtk_main_iteration_do(TRUE) */
gint	   gtk_main_iteration_do (gboolean blocking);

gint	   gtk_true		   (void);
gint	   gtk_false		   (void);

void	   gtk_grab_add		   (GtkWidget	       *widget);
GtkWidget* gtk_grab_get_current	   (void);
void	   gtk_grab_remove	   (GtkWidget	       *widget);

void	   gtk_init_add		   (GtkFunction	       function,
				    gpointer	       data);
void	   gtk_quit_add_destroy	   (guint	       main_level,
				    GtkObject	      *object);
guint	   gtk_quit_add		   (guint	       main_level,
				    GtkFunction	       function,
				    gpointer	       data);
guint	   gtk_quit_add_full	   (guint	       main_level,
				    GtkFunction	       function,
				    GtkCallbackMarshal marshal,
				    gpointer	       data,
				    GtkDestroyNotify   destroy);
void	   gtk_quit_remove	   (guint	       quit_handler_id);
void	   gtk_quit_remove_by_data (gpointer	       data);
guint	   gtk_timeout_add_full	   (guint32	       interval,
				    GtkFunction	       function,
				    GtkCallbackMarshal marshal,
				    gpointer	       data,
				    GtkDestroyNotify   destroy);
guint	   gtk_timeout_add	   (guint32	       interval,
				    GtkFunction	       function,
				    gpointer	       data);
guint	   gtk_timeout_add_interp  (guint32	       interval,
				    GtkCallbackMarshal function,
				    gpointer	       data,
				    GtkDestroyNotify   notify);
void	   gtk_timeout_remove	   (guint	       timeout_handler_id);

guint	   gtk_idle_add		   (GtkFunction	       function,
				    gpointer	       data);
guint	   gtk_idle_add_priority   (gint	       priority,
				    GtkFunction	       function,
				    gpointer	       data);
guint	   gtk_idle_add_full	   (gint	       priority,
				    GtkFunction	       function,
				    GtkCallbackMarshal marshal,
				    gpointer	       data,
				    GtkDestroyNotify   destroy);
guint	   gtk_idle_add_interp	   (GtkCallbackMarshal marshal,
				    gpointer	       data,
				    GtkDestroyNotify   destroy);
void	   gtk_idle_remove	   (guint	       idle_handler_id);
void	   gtk_idle_remove_by_data (gpointer	       data);
guint	   gtk_input_add_full	   (gint	       source,
				    GdkInputCondition  condition,
				    GdkInputFunction   function,
				    GtkCallbackMarshal marshal,
				    gpointer	       data,
				    GtkDestroyNotify   destroy);
void	   gtk_input_remove	   (guint	       input_handler_id);


guint	   gtk_key_snooper_install (GtkKeySnoopFunc snooper,
				    gpointer	    func_data);
void	   gtk_key_snooper_remove  (guint	    snooper_handler_id);
  
GdkEvent*  gtk_get_current_event   (void);
GtkWidget* gtk_get_event_widget	 (GdkEvent	*event);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_MAIN_H__ */
