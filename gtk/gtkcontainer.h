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

#ifndef __GTK_CONTAINER_H__
#define __GTK_CONTAINER_H__


#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkadjustment.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_CONTAINER	        (gtk_container_get_type ())
#define GTK_CONTAINER(obj)	        (GTK_CHECK_CAST ((obj), GTK_TYPE_CONTAINER, GtkContainer))
#define GTK_CONTAINER_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_CONTAINER, GtkContainerClass))
#define GTK_IS_CONTAINER(obj)	        (GTK_CHECK_TYPE ((obj), GTK_TYPE_CONTAINER))
#define GTK_IS_CONTAINER_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CONTAINER))

#define GTK_IS_RESIZE_CONTAINER(widget) (GTK_IS_CONTAINER (widget) && ((GtkContainer*) (widget))->resize_mode != GTK_RESIZE_PARENT)


typedef struct _GtkContainer	   GtkContainer;
typedef struct _GtkContainerClass  GtkContainerClass;

struct _GtkContainer
{
  GtkWidget widget;
  
  GtkWidget *focus_child;
  
  guint border_width : 16;
  guint need_resize : 1;
  guint resize_mode : 2;
  guint reallocate_redraws : 1;
  
  /* The list of children that requested a resize
   */
  GSList *resize_widgets;
};

struct _GtkContainerClass
{
  GtkWidgetClass parent_class;
  
  guint   n_child_args;

  void (* add)	       		(GtkContainer	 *container,
				 GtkWidget	 *widget);
  void (* remove)      		(GtkContainer	 *container,
				 GtkWidget	 *widget);
  void (* check_resize)		(GtkContainer	 *container);
  void (* forall)     		(GtkContainer	 *container,
				 gboolean	  include_internals,
				 GtkCallback	  callback,
				 gpointer	  callbabck_data);
  gint (* focus)       		(GtkContainer	 *container,
				 GtkDirectionType  direction);
  void (* set_focus_child)	(GtkContainer	 *container,
				 GtkWidget	 *widget);
  GtkType (*child_type)		(GtkContainer	*container);
  void    (*set_child_arg)	(GtkContainer 	*container,
				 GtkWidget    	*child,
				 GtkArg       	*arg,
				 guint        	 arg_id);
  void    (*get_child_arg)	(GtkContainer	*container,
				 GtkWidget    	*child,
				 GtkArg       	*arg,
				 guint        	 arg_id);
  gchar*  (*composite_name)	(GtkContainer	*container,
				 GtkWidget	*child);

  /* Padding for future expansion */
  GtkFunction pad1;
  GtkFunction pad2;
};

/* Application-level methods */

GtkType gtk_container_get_type		 (void);
void    gtk_container_set_border_width	 (GtkContainer	   *container,
					  guint		    border_width);
void    gtk_container_add		 (GtkContainer	   *container,
					  GtkWidget	   *widget);
void    gtk_container_remove		 (GtkContainer	   *container,
					  GtkWidget	   *widget);

void    gtk_container_set_resize_mode    (GtkContainer     *container,
					  GtkResizeMode     resize_mode);

void    gtk_container_check_resize       (GtkContainer     *container);

void    gtk_container_foreach		 (GtkContainer	   *container,
					  GtkCallback	    callback,
					  gpointer	    callback_data);
void    gtk_container_foreach_full	 (GtkContainer	   *container,
					  GtkCallback	    callback,
					  GtkCallbackMarshal marshal,
					  gpointer	    callback_data,
					  GtkDestroyNotify  notify);
GList* gtk_container_children		 (GtkContainer	   *container);
gint   gtk_container_focus		   (GtkContainer     *container,
					    GtkDirectionType  direction);

/* Widget-level methods */

void   gtk_container_set_reallocate_redraws (GtkContainer    *container,
					     gboolean         needs_redraws);
void   gtk_container_set_focus_child	   (GtkContainer     *container,
					    GtkWidget	     *child);
void   gtk_container_set_focus_vadjustment (GtkContainer     *container,
					    GtkAdjustment    *adjustment);
void   gtk_container_set_focus_hadjustment (GtkContainer     *container,
					    GtkAdjustment    *adjustment);
void    gtk_container_register_toplevel	   (GtkContainer     *container);
void    gtk_container_unregister_toplevel  (GtkContainer     *container);
GList*  gtk_container_get_toplevels        (void);

void    gtk_container_resize_children      (GtkContainer     *container);

GtkType gtk_container_child_type	   (GtkContainer     *container);

/* the `arg_name' argument needs to be a const static string */
void    gtk_container_add_child_arg_type   (const gchar      *arg_name,
					    GtkType           arg_type,
					    guint             arg_flags,
					    guint             arg_id);
     
/* Allocate a GtkArg array of size nargs that hold the
 * names and types of the args that can be used with
 * gtk_container_child_getv/gtk_container_child_setv.
 * if (arg_flags!=NULL),
 * (*arg_flags) will be set to point to a newly allocated
 * guint array that holds the flags of the args.
 * It is the callers response to do a
 * g_free (returned_args); g_free (*arg_flags).
 */
GtkArg* gtk_container_query_child_args	   (GtkType	       class_type,
					    guint32          **arg_flags,
					    guint             *nargs);

/* gtk_container_child_getv() sets an arguments type and value, or just
 * its type to GTK_TYPE_INVALID.
 * if GTK_FUNDAMENTAL_TYPE (arg->type) == GTK_TYPE_STRING, it's the callers
 * response to do a g_free (GTK_VALUE_STRING (arg));
 */
void    gtk_container_child_getv	   (GtkContainer      *container,
					    GtkWidget	      *child,
					    guint	       n_args,
					    GtkArg	      *args);
void    gtk_container_child_setv   	   (GtkContainer      *container,
					    GtkWidget	      *child,
					    guint	       n_args,
					    GtkArg	      *args);

/* gtk_container_add_with_args() takes a variable argument list of the form:
 * (..., gchar *arg_name, ARG_VALUES, [repeatedly name/value pairs,] NULL)
 * where ARG_VALUES type depend on the argument and can consist of
 * more than one c-function argument.
 */
void    gtk_container_add_with_args	   (GtkContainer      *container,
					    GtkWidget	      *widget,
					    const gchar	      *first_arg_name,
					    ...);
void    gtk_container_addv		   (GtkContainer      *container,
					    GtkWidget	      *widget,
					    guint	       n_args,
					    GtkArg	      *args);
void	gtk_container_child_set		   (GtkContainer      *container,
					    GtkWidget         *child,
					    const gchar	      *first_arg_name,
					    ...);
     

/* Non-public methods */

void	gtk_container_queue_resize	     (GtkContainer *container);
void    gtk_container_clear_resize_widgets   (GtkContainer *container);
void    gtk_container_arg_set		     (GtkContainer *container,
					      GtkWidget	   *child,
					      GtkArg       *arg,
					      GtkArgInfo   *info);
void    gtk_container_arg_get		     (GtkContainer *container,
					      GtkWidget	   *child,
					      GtkArg       *arg,
					      GtkArgInfo   *info);
gchar*	gtk_container_child_args_collect     (GtkType       object_type,
					      GSList      **arg_list_p,
					      GSList      **info_list_p,
					      const gchar  *first_arg_name,
					      va_list       args);
gchar*  gtk_container_child_arg_get_info     (GtkType       object_type,
					      const gchar  *arg_name,
					      GtkArgInfo  **info_p);
void    gtk_container_forall		     (GtkContainer *container,
					      GtkCallback   callback,
					      gpointer	    callback_data);
gchar*	gtk_container_child_composite_name   (GtkContainer *container,
					      GtkWidget	   *child);
void    gtk_container_dequeue_resize_handler (GtkContainer *container);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_CONTAINER_H__ */
