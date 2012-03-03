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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_CONTAINER_H__
#define __GTK_CONTAINER_H__


#include <gtk/gtkwidget.h>


G_BEGIN_DECLS

#define GTK_TYPE_CONTAINER              (gtk_container_get_type ())
#define GTK_CONTAINER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CONTAINER, GtkContainer))
#define GTK_CONTAINER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CONTAINER, GtkContainerClass))
#define GTK_IS_CONTAINER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CONTAINER))
#define GTK_IS_CONTAINER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CONTAINER))
#define GTK_CONTAINER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CONTAINER, GtkContainerClass))


typedef struct _GtkContainer              GtkContainer;
typedef struct _GtkContainerPrivate       GtkContainerPrivate;
typedef struct _GtkContainerClass         GtkContainerClass;

struct _GtkContainer
{
  GtkWidget widget;

  /*< private >*/
  GtkContainerPrivate *priv;
};

struct _GtkContainerClass
{
  GtkWidgetClass parent_class;

  void    (*add)       		(GtkContainer	 *container,
				 GtkWidget	 *widget);
  void    (*remove)    		(GtkContainer	 *container,
				 GtkWidget	 *widget);
  void    (*check_resize)	(GtkContainer	 *container);
  void    (*forall)    		(GtkContainer	 *container,
				 gboolean	  include_internals,
				 GtkCallback	  callback,
				 gpointer	  callback_data);
  void    (*set_focus_child)	(GtkContainer	 *container,
				 GtkWidget	 *child);
  GType   (*child_type)		(GtkContainer	 *container);
  gchar*  (*composite_name)	(GtkContainer	 *container,
				 GtkWidget	 *child);
  void    (*set_child_property) (GtkContainer    *container,
				 GtkWidget       *child,
				 guint            property_id,
				 const GValue    *value,
				 GParamSpec      *pspec);
  void    (*get_child_property) (GtkContainer    *container,
                                 GtkWidget       *child,
				 guint            property_id,
				 GValue          *value,
				 GParamSpec      *pspec);
  GtkWidgetPath * (*get_path_for_child) (GtkContainer *container,
                                         GtkWidget    *child);


  /*< private >*/

  unsigned int _handle_border_width : 1;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
  void (*_gtk_reserved7) (void);
  void (*_gtk_reserved8) (void);
};


/* Application-level methods */

GType   gtk_container_get_type		 (void) G_GNUC_CONST;
void    gtk_container_set_border_width	 (GtkContainer	   *container,
					  guint		    border_width);
guint   gtk_container_get_border_width   (GtkContainer     *container);
void    gtk_container_add		 (GtkContainer	   *container,
					  GtkWidget	   *widget);
void    gtk_container_remove		 (GtkContainer	   *container,
					  GtkWidget	   *widget);

void    gtk_container_set_resize_mode    (GtkContainer     *container,
					  GtkResizeMode     resize_mode);
GtkResizeMode gtk_container_get_resize_mode (GtkContainer     *container);

void    gtk_container_check_resize       (GtkContainer     *container);

void     gtk_container_foreach      (GtkContainer       *container,
				     GtkCallback         callback,
				     gpointer            callback_data);
GList*   gtk_container_get_children     (GtkContainer       *container);

void     gtk_container_propagate_draw   (GtkContainer   *container,
					 GtkWidget      *child,
					 cairo_t        *cr);

void     gtk_container_set_focus_chain  (GtkContainer   *container,
                                         GList          *focusable_widgets);
gboolean gtk_container_get_focus_chain  (GtkContainer   *container,
					 GList         **focusable_widgets);
void     gtk_container_unset_focus_chain (GtkContainer  *container);

#define GTK_IS_RESIZE_CONTAINER(widget) (GTK_IS_CONTAINER (widget) && \
                                        (gtk_container_get_resize_mode (GTK_CONTAINER (widget)) != GTK_RESIZE_PARENT))

/* Widget-level methods */

void   gtk_container_set_reallocate_redraws (GtkContainer    *container,
					     gboolean         needs_redraws);
void   gtk_container_set_focus_child	   (GtkContainer     *container,
					    GtkWidget	     *child);
GtkWidget *
       gtk_container_get_focus_child	   (GtkContainer     *container);
void   gtk_container_set_focus_vadjustment (GtkContainer     *container,
					    GtkAdjustment    *adjustment);
GtkAdjustment *gtk_container_get_focus_vadjustment (GtkContainer *container);
void   gtk_container_set_focus_hadjustment (GtkContainer     *container,
					    GtkAdjustment    *adjustment);
GtkAdjustment *gtk_container_get_focus_hadjustment (GtkContainer *container);

void    gtk_container_resize_children      (GtkContainer     *container);

GType   gtk_container_child_type	   (GtkContainer     *container);


void         gtk_container_class_install_child_property (GtkContainerClass *cclass,
							 guint		    property_id,
							 GParamSpec	   *pspec);
GParamSpec*  gtk_container_class_find_child_property	(GObjectClass	   *cclass,
							 const gchar	   *property_name);
GParamSpec** gtk_container_class_list_child_properties	(GObjectClass	   *cclass,
							 guint		   *n_properties);
void         gtk_container_add_with_properties		(GtkContainer	   *container,
							 GtkWidget	   *widget,
							 const gchar	   *first_prop_name,
							 ...) G_GNUC_NULL_TERMINATED;
void         gtk_container_child_set			(GtkContainer	   *container,
							 GtkWidget	   *child,
							 const gchar	   *first_prop_name,
							 ...) G_GNUC_NULL_TERMINATED;
void         gtk_container_child_get			(GtkContainer	   *container,
							 GtkWidget	   *child,
							 const gchar	   *first_prop_name,
							 ...) G_GNUC_NULL_TERMINATED;
void         gtk_container_child_set_valist		(GtkContainer	   *container,
							 GtkWidget	   *child,
							 const gchar	   *first_property_name,
							 va_list	    var_args);
void         gtk_container_child_get_valist		(GtkContainer	   *container,
							 GtkWidget	   *child,
							 const gchar	   *first_property_name,
							 va_list	    var_args);
void	     gtk_container_child_set_property		(GtkContainer	   *container,
							 GtkWidget	   *child,
							 const gchar	   *property_name,
							 const GValue	   *value);
void	     gtk_container_child_get_property		(GtkContainer	   *container,
							 GtkWidget	   *child,
							 const gchar	   *property_name,
	                                                 GValue		   *value);

GDK_AVAILABLE_IN_3_2
void gtk_container_child_notify (GtkContainer *container,
                                 GtkWidget    *child,
                                 const gchar  *child_property);

/**
 * GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID:
 * @object: the #GObject on which set_child_property() or get_child_property()
 *  was called
 * @property_id: the numeric id of the property
 * @pspec: the #GParamSpec of the property
 *
 * This macro should be used to emit a standard warning about unexpected
 * properties in set_child_property() and get_child_property() implementations.
 */
#define GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID(object, property_id, pspec) \
    G_OBJECT_WARN_INVALID_PSPEC ((object), "child property id", (property_id), (pspec))


void    gtk_container_forall		     (GtkContainer *container,
					      GtkCallback   callback,
					      gpointer	    callback_data);

void    gtk_container_class_handle_border_width (GtkContainerClass *klass);

GtkWidgetPath * gtk_container_get_path_for_child (GtkContainer      *container,
                                                  GtkWidget         *child);

G_END_DECLS

#endif /* __GTK_CONTAINER_H__ */
