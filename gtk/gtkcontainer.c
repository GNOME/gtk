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
#include <stdlib.h>

#include "gtkcontainer.h"
#include "gtkprivate.h"
#include "gtksignal.h"
#include "gtkmain.h"
#include "gtkwindow.h"
#include "gtkintl.h"
#include <gobject/gobjectnotifyqueue.c>
#include <gobject/gvaluecollector.h>


enum {
  ADD,
  REMOVE,
  CHECK_RESIZE,
  SET_FOCUS_CHILD,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_BORDER_WIDTH,
  PROP_RESIZE_MODE,
  PROP_CHILD,
};

#define PARAM_SPEC_PARAM_ID(pspec)              ((pspec)->param_id)
#define PARAM_SPEC_SET_PARAM_ID(pspec, id)      ((pspec)->param_id = (id))


/* --- prototypes --- */
static void     gtk_container_base_class_init      (GtkContainerClass *klass);
static void     gtk_container_base_class_finalize  (GtkContainerClass *klass);
static void     gtk_container_class_init           (GtkContainerClass *klass);
static void     gtk_container_init                 (GtkContainer      *container);
static void     gtk_container_destroy              (GtkObject         *object);
static void     gtk_container_set_property         (GObject         *object,
						    guint            prop_id,
						    const GValue    *value,
						    GParamSpec      *pspec);
static void     gtk_container_get_property         (GObject         *object,
						    guint            prop_id,
						    GValue          *value,
						    GParamSpec      *pspec);
static void     gtk_container_add_unimplemented    (GtkContainer      *container,
						    GtkWidget         *widget);
static void     gtk_container_remove_unimplemented (GtkContainer      *container,
						    GtkWidget         *widget);
static void     gtk_container_real_check_resize    (GtkContainer      *container);
static gboolean gtk_container_focus                (GtkWidget         *widget,
						    GtkDirectionType   direction);
static void     gtk_container_real_set_focus_child (GtkContainer      *container,
						    GtkWidget         *widget);
static gboolean gtk_container_focus_tab            (GtkContainer      *container,
						    GList             *children,
						    GtkDirectionType   direction);
static gboolean gtk_container_focus_up_down        (GtkContainer      *container,
						    GList            **children,
						    GtkDirectionType   direction);
static gboolean gtk_container_focus_left_right     (GtkContainer      *container,
						    GList            **children,
						    GtkDirectionType   direction);
static gboolean gtk_container_focus_move           (GtkContainer      *container,
						    GList             *children,
						    GtkDirectionType   direction);
static void     gtk_container_children_callback    (GtkWidget         *widget,
						    gpointer           client_data);
static void     gtk_container_show_all             (GtkWidget         *widget);
static void     gtk_container_hide_all             (GtkWidget         *widget);
static gint     gtk_container_expose               (GtkWidget         *widget,
						    GdkEventExpose    *event);
static void     gtk_container_map                  (GtkWidget         *widget);
static void     gtk_container_unmap                (GtkWidget         *widget);

static gchar* gtk_container_child_default_composite_name (GtkContainer *container,
							  GtkWidget    *child);


/* --- variables --- */
static const gchar          *vadjustment_key = "gtk-vadjustment";
static guint                 vadjustment_key_id = 0;
static const gchar          *hadjustment_key = "gtk-hadjustment";
static guint                 hadjustment_key_id = 0;
static GSList	            *container_resize_queue = NULL;
static guint                 container_signals[LAST_SIGNAL] = { 0 };
static GtkWidgetClass       *parent_class = NULL;
extern GParamSpecPool       *_gtk_widget_child_property_pool;
extern GObjectNotifyContext *_gtk_widget_child_property_notify_context;


/* --- functions --- */
GtkType
gtk_container_get_type (void)
{
  static GtkType container_type = 0;

  if (!container_type)
    {
      static GTypeInfo container_info = {
	sizeof (GtkContainerClass),
	(GBaseInitFunc) gtk_container_base_class_init,
	(GBaseFinalizeFunc) gtk_container_base_class_finalize,
	(GClassInitFunc) gtk_container_class_init,
	NULL        /* class_destroy */,
	NULL        /* class_data */,
	sizeof (GtkContainer),
	0           /* n_preallocs */,
	(GInstanceInitFunc) gtk_container_init,
	NULL,       /* value_table */
      };

      container_type = g_type_register_static (GTK_TYPE_WIDGET, "GtkContainer", &container_info, 0);
    }

  return container_type;
}

static void
gtk_container_base_class_init (GtkContainerClass *class)
{
  /* reset instance specifc class fields that don't get inherited */
  class->set_child_property = NULL;
  class->get_child_property = NULL;
}

static void
gtk_container_base_class_finalize (GtkContainerClass *class)
{
  GList *list, *node;

  list = g_param_spec_pool_list_owned (_gtk_widget_child_property_pool, G_OBJECT_CLASS_TYPE (class));
  for (node = list; node; node = node->next)
    {
      GParamSpec *pspec = node->data;

      g_param_spec_pool_remove (_gtk_widget_child_property_pool, pspec);
      PARAM_SPEC_SET_PARAM_ID (pspec, 0);
      g_param_spec_unref (pspec);
    }
  g_list_free (list);
}

static void
gtk_container_class_init (GtkContainerClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  vadjustment_key_id = g_quark_from_static_string (vadjustment_key);
  hadjustment_key_id = g_quark_from_static_string (hadjustment_key);
  
  gobject_class->set_property = gtk_container_set_property;
  gobject_class->get_property = gtk_container_get_property;

  object_class->destroy = gtk_container_destroy;

  widget_class->show_all = gtk_container_show_all;
  widget_class->hide_all = gtk_container_hide_all;
  widget_class->expose_event = gtk_container_expose;
  widget_class->map = gtk_container_map;
  widget_class->unmap = gtk_container_unmap;
  widget_class->focus = gtk_container_focus;
  
  class->add = gtk_container_add_unimplemented;
  class->remove = gtk_container_remove_unimplemented;
  class->check_resize = gtk_container_real_check_resize;
  class->forall = NULL;
  class->set_focus_child = gtk_container_real_set_focus_child;
  class->child_type = NULL;
  class->composite_name = gtk_container_child_default_composite_name;

  g_object_class_install_property (gobject_class,
                                   PROP_RESIZE_MODE,
                                   g_param_spec_enum ("resize_mode",
                                                      _("Resize mode"),
                                                      _("Specify how resize events are handled"),
                                                      GTK_TYPE_RESIZE_MODE,
                                                      GTK_RESIZE_PARENT,
                                                      G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_BORDER_WIDTH,
                                   g_param_spec_uint ("border_width",
                                                      _("Border width"),
                                                      _("The width of the empty border outside the containers children."),
						      0,
						      G_MAXINT,
						      0,
                                                      G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_CHILD,
                                   g_param_spec_object ("child",
                                                      _("Child"),
                                                      _("Can be used to add a new child to the container."),
                                                      GTK_TYPE_WIDGET,
						      G_PARAM_WRITABLE));
  container_signals[ADD] =
    gtk_signal_new ("add",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkContainerClass, add),
                    gtk_marshal_VOID__OBJECT,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_WIDGET);
  container_signals[REMOVE] =
    gtk_signal_new ("remove",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkContainerClass, remove),
                    gtk_marshal_VOID__OBJECT,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_WIDGET);
  container_signals[CHECK_RESIZE] =
    gtk_signal_new ("check_resize",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkContainerClass, check_resize),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  container_signals[SET_FOCUS_CHILD] =
    gtk_signal_new ("set-focus-child",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkContainerClass, set_focus_child),
                    gtk_marshal_VOID__OBJECT,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_WIDGET);
}

GtkType
gtk_container_child_type (GtkContainer *container)
{
  GtkType slot;
  GtkContainerClass *class;

  g_return_val_if_fail (GTK_IS_CONTAINER (container), 0);

  class = GTK_CONTAINER_GET_CLASS (container);
  if (class->child_type)
    slot = class->child_type (container);
  else
    slot = GTK_TYPE_NONE;

  return slot;
}

/* --- GtkContainer child property mechanism --- */
static inline void
container_get_child_property (GtkContainer *container,
			      GtkWidget    *child,
			      GParamSpec   *pspec,
			      GValue       *value)
{
  GtkContainerClass *class = g_type_class_peek (pspec->owner_type);
  
  class->get_child_property (container, child, PARAM_SPEC_PARAM_ID (pspec), value, pspec);
}

static inline void
container_set_child_property (GtkContainer       *container,
			      GtkWidget		 *child,
			      GParamSpec         *pspec,
			      const GValue       *value,
			      GObjectNotifyQueue *nqueue)
{
  GValue tmp_value = { 0, };
  GtkContainerClass *class = g_type_class_peek (pspec->owner_type);

  /* provide a copy to work from, convert (if necessary) and validate */
  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  if (!g_value_transform (value, &tmp_value))
    g_warning ("unable to set child property `%s' of type `%s' from value of type `%s'",
	       pspec->name,
	       g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
	       G_VALUE_TYPE_NAME (value));
  else if (g_param_value_validate (pspec, &tmp_value) && !(pspec->flags & G_PARAM_LAX_VALIDATION))
    {
      gchar *contents = g_strdup_value_contents (value);

      g_warning ("value \"%s\" of type `%s' is invalid for property `%s' of type `%s'",
		 contents,
		 G_VALUE_TYPE_NAME (value),
		 pspec->name,
		 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
      g_free (contents);
    }
  else
    {
      class->set_child_property (container, child, PARAM_SPEC_PARAM_ID (pspec), &tmp_value, pspec);
      g_object_notify_queue_add (G_OBJECT (child), nqueue, pspec);
    }
  g_value_unset (&tmp_value);
}

void
gtk_container_child_get_valist (GtkContainer *container,
				GtkWidget    *child,
				const gchar  *first_property_name,
				va_list       var_args)
{
  const gchar *name;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (container));

  g_object_ref (container);
  g_object_ref (child);

  name = first_property_name;
  while (name)
    {
      GValue value = { 0, };
      GParamSpec *pspec;
      gchar *error;

      pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool,
					name,
					G_OBJECT_TYPE (container),
					TRUE);
      if (!pspec)
	{
	  g_warning ("%s: container class `%s' has no child property named `%s'",
		     G_STRLOC,
		     G_OBJECT_TYPE_NAME (container),
		     name);
	  break;
	}
      if (!(pspec->flags & G_PARAM_READABLE))
	{
	  g_warning ("%s: child property `%s' of container class `%s' is not readable",
		     G_STRLOC,
		     pspec->name,
		     G_OBJECT_TYPE_NAME (container));
	  break;
	}
      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      container_get_child_property (container, child, pspec, &value);
      G_VALUE_LCOPY (&value, var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);
	  g_value_unset (&value);
	  break;
	}
      g_value_unset (&value);
      name = va_arg (var_args, gchar*);
    }

  g_object_unref (child);
  g_object_unref (container);
}

void
gtk_container_child_get_property (GtkContainer *container,
				  GtkWidget    *child,
				  const gchar  *property_name,
				  GValue       *value)
{
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (container));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));
  
  g_object_ref (container);
  g_object_ref (child);
  pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool, property_name,
				    G_OBJECT_TYPE (container), TRUE);
  if (!pspec)
    g_warning ("%s: container class `%s' has no child property named `%s'",
	       G_STRLOC,
	       G_OBJECT_TYPE_NAME (container),
	       property_name);
  else if (!(pspec->flags & G_PARAM_READABLE))
    g_warning ("%s: child property `%s' of container class `%s' is not readable",
	       G_STRLOC,
	       pspec->name,
	       G_OBJECT_TYPE_NAME (container));
  else
    {
      GValue *prop_value, tmp_value = { 0, };

      /* auto-conversion of the callers value type
       */
      if (G_VALUE_TYPE (value) == G_PARAM_SPEC_VALUE_TYPE (pspec))
	{
	  g_value_reset (value);
	  prop_value = value;
	}
      else if (!g_value_type_transformable (G_PARAM_SPEC_VALUE_TYPE (pspec), G_VALUE_TYPE (value)))
	{
	  g_warning ("can't retrieve child property `%s' of type `%s' as value of type `%s'",
		     pspec->name,
		     g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
		     G_VALUE_TYPE_NAME (value));
	  g_object_unref (child);
	  g_object_unref (container);
	  return;
	}
      else
	{
	  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
	  prop_value = &tmp_value;
	}
      container_get_child_property (container, child, pspec, prop_value);
      if (prop_value != value)
	{
	  g_value_transform (prop_value, value);
	  g_value_unset (&tmp_value);
	}
    }
  g_object_unref (child);
  g_object_unref (container);
}

void
gtk_container_child_set_valist (GtkContainer *container,
				GtkWidget    *child,
				const gchar  *first_property_name,
				va_list       var_args)
{
  GObject *object;
  GObjectNotifyQueue *nqueue;
  const gchar *name;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (container));

  g_object_ref (container);
  g_object_ref (child);

  object = G_OBJECT (container);
  nqueue = g_object_notify_queue_freeze (G_OBJECT (child), _gtk_widget_child_property_notify_context);
  name = first_property_name;
  while (name)
    {
      GValue value = { 0, };
      gchar *error = NULL;
      GParamSpec *pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool,
						    name,
						    G_OBJECT_TYPE (container),
						    TRUE);
      if (!pspec)
	{
	  g_warning ("%s: container class `%s' has no child property named `%s'",
		     G_STRLOC,
		     G_OBJECT_TYPE_NAME (container),
		     name);
	  break;
	}
      if (!(pspec->flags & G_PARAM_WRITABLE))
	{
	  g_warning ("%s: child property `%s' of container class `%s' is not writable",
		     G_STRLOC,
		     pspec->name,
		     G_OBJECT_TYPE_NAME (container));
	  break;
	}
      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      G_VALUE_COLLECT (&value, var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);

	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occoured
	   */
	  break;
	}
      container_set_child_property (container, child, pspec, &value, nqueue);
      g_value_unset (&value);
      name = va_arg (var_args, gchar*);
    }
  g_object_notify_queue_thaw (G_OBJECT (child), nqueue);

  g_object_unref (container);
  g_object_unref (child);
}

void
gtk_container_child_set_property (GtkContainer *container,
				  GtkWidget    *child,
				  const gchar  *property_name,
				  const GValue *value)
{
  GObject *object;
  GObjectNotifyQueue *nqueue;
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (container));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));
  
  g_object_ref (container);
  g_object_ref (child);

  object = G_OBJECT (container);
  nqueue = g_object_notify_queue_freeze (G_OBJECT (child), _gtk_widget_child_property_notify_context);
  pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool, property_name,
				    G_OBJECT_TYPE (container), TRUE);
  if (!pspec)
    g_warning ("%s: container class `%s' has no child property named `%s'",
	       G_STRLOC,
	       G_OBJECT_TYPE_NAME (container),
	       property_name);
  else if (!(pspec->flags & G_PARAM_WRITABLE))
    g_warning ("%s: child property `%s' of container class `%s' is not writable",
	       G_STRLOC,
	       pspec->name,
	       G_OBJECT_TYPE_NAME (container));
  else
    {
      container_set_child_property (container, child, pspec, value, nqueue);
    }
  g_object_notify_queue_thaw (G_OBJECT (child), nqueue);
  g_object_unref (container);
  g_object_unref (child);
}

void
gtk_container_add_with_properties (GtkContainer *container,
				   GtkWidget    *widget,
				   const gchar  *first_prop_name,
				   ...)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->parent == NULL);

  gtk_widget_ref (GTK_WIDGET (container));
  gtk_widget_ref (widget);
  gtk_widget_freeze_child_notify (widget);

  gtk_signal_emit (GTK_OBJECT (container), container_signals[ADD], widget);
  if (widget->parent)
    {
      va_list var_args;

      va_start (var_args, first_prop_name);
      gtk_container_child_set_valist (container, widget, first_prop_name, var_args);
      va_end (var_args);
    }

  gtk_widget_thaw_child_notify (widget);
  gtk_widget_unref (widget);
  gtk_widget_unref (GTK_WIDGET (container));
}

void
gtk_container_child_set (GtkContainer      *container,
			 GtkWidget         *child,
			 const gchar       *first_prop_name,
			 ...)
{
  va_list var_args;
  
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (container));

  va_start (var_args, first_prop_name);
  gtk_container_child_set_valist (container, child, first_prop_name, var_args);
  va_end (var_args);
}

void
gtk_container_child_get (GtkContainer      *container,
			 GtkWidget         *child,
			 const gchar       *first_prop_name,
			 ...)
{
  va_list var_args;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (container));

  va_start (var_args, first_prop_name);
  gtk_container_child_get_valist (container, child, first_prop_name, var_args);
  va_end (var_args);
}

void
gtk_container_class_install_child_property (GtkContainerClass *class,
					    guint              property_id,
					    GParamSpec        *pspec)
{
  g_return_if_fail (GTK_IS_CONTAINER_CLASS (class));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  if (pspec->flags & G_PARAM_WRITABLE)
    g_return_if_fail (class->set_child_property != NULL);
  if (pspec->flags & G_PARAM_READABLE)
    g_return_if_fail (class->get_child_property != NULL);
  g_return_if_fail (property_id > 0);
  g_return_if_fail (PARAM_SPEC_PARAM_ID (pspec) == 0);  /* paranoid */
  if (pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY))
    g_return_if_fail ((pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)) == 0);

  if (g_param_spec_pool_lookup (_gtk_widget_child_property_pool, pspec->name, G_OBJECT_CLASS_TYPE (class), FALSE))
    {
      g_warning (G_STRLOC ": class `%s' already contains a property named `%s'",
		 G_OBJECT_CLASS_NAME (class),
		 pspec->name);
      return;
    }
  g_param_spec_ref (pspec);
  g_param_spec_sink (pspec);
  PARAM_SPEC_SET_PARAM_ID (pspec, property_id);
  g_param_spec_pool_insert (_gtk_widget_child_property_pool, pspec, G_OBJECT_CLASS_TYPE (class));
}

GParamSpec*
gtk_container_class_find_child_property (GObjectClass *class,
					 const gchar  *property_name)
{
  g_return_val_if_fail (GTK_IS_CONTAINER_CLASS (class), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  return g_param_spec_pool_lookup (_gtk_widget_child_property_pool,
				   property_name,
				   G_OBJECT_CLASS_TYPE (class),
				   TRUE);
}

GParamSpec** /* free result */
gtk_container_class_list_child_properties (GObjectClass *class,
					   guint        *n_properties)
{
  GParamSpec **pspecs;
  guint n;

  g_return_val_if_fail (GTK_IS_CONTAINER_CLASS (class), NULL);

  pspecs = g_param_spec_pool_list (_gtk_widget_child_property_pool,
				   G_OBJECT_CLASS_TYPE (class),
				   &n);
  if (n_properties)
    *n_properties = n;

  return pspecs;
}

static void
gtk_container_add_unimplemented (GtkContainer     *container,
				 GtkWidget        *widget)
{
  g_warning ("GtkContainerClass::add not implemented for `%s'", gtk_type_name (GTK_OBJECT_TYPE (container)));
}

static void
gtk_container_remove_unimplemented (GtkContainer     *container,
				    GtkWidget        *widget)
{
  g_warning ("GtkContainerClass::remove not implemented for `%s'", gtk_type_name (GTK_OBJECT_TYPE (container)));
}

static void
gtk_container_init (GtkContainer *container)
{
  container->focus_child = NULL;
  container->border_width = 0;
  container->need_resize = FALSE;
  container->resize_mode = GTK_RESIZE_PARENT;
  container->reallocate_redraws = FALSE;
  container->resize_widgets = NULL;
}

static void
gtk_container_destroy (GtkObject *object)
{
  GtkContainer *container;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (object));

  container = GTK_CONTAINER (object);
  
  if (GTK_CONTAINER_RESIZE_PENDING (container))
    _gtk_container_dequeue_resize_handler (container);
  if (container->resize_widgets)
    _gtk_container_clear_resize_widgets (container);

  /* do this before walking child widgets, to avoid
   * removing children from focus chain one by one.
   */
  if (container->has_focus_chain)
    gtk_container_unset_focus_chain (container);
  
  gtk_container_foreach (container, (GtkCallback) gtk_widget_destroy, NULL);
  
  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_container_set_property (GObject         *object,
			    guint            prop_id,
			    const GValue    *value,
			    GParamSpec      *pspec)
{
  GtkContainer *container;

  container = GTK_CONTAINER (object);

  switch (prop_id)
    {
    case PROP_BORDER_WIDTH:
      gtk_container_set_border_width (container, g_value_get_uint (value));
      break;
    case PROP_RESIZE_MODE:
      gtk_container_set_resize_mode (container, g_value_get_enum (value));
      break;
    case PROP_CHILD:
      gtk_container_add (container, GTK_WIDGET (g_value_get_object (value)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_container_get_property (GObject         *object,
			    guint            prop_id,
			    GValue          *value,
			    GParamSpec      *pspec)
{
  GtkContainer *container;

  container = GTK_CONTAINER (object);
  
  switch (prop_id)
    {
    case PROP_BORDER_WIDTH:
      g_value_set_uint (value, container->border_width);
      break;
    case PROP_RESIZE_MODE:
      g_value_set_enum (value, container->resize_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_container_set_border_width:
 * @container: a #GtkContainer
 * @border_width: amount of blank space to leave <emphasis>outside</emphasis> the container
 *
 * The border width of a container is the amount of space to leave
 * around the outside of the container. The only exception to this is
 * #GtkWindow; because toplevel windows can't leave space outside,
 * they leave the space inside. The border is added on all sides of
 * the container. To add space to only one side, one approach is to
 * create a #GtkAlignment widget, call gtk_widget_set_usize() to give
 * it a size, and place it on the side of the container as a spacer.
 * 
 **/
void
gtk_container_set_border_width (GtkContainer *container,
				guint         border_width)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

  if (container->border_width != border_width)
    {
      container->border_width = border_width;
      g_object_notify (G_OBJECT (container), "border_width");
      
      if (GTK_WIDGET_REALIZED (container))
	gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

/**
 * gtk_container_get_border_width:
 * @container: a #GtkContainer
 * 
 * Retrieves the border width of the container. See
 * gtk_container_set_border_width().
 *
 * Return value: the current border width
 **/
guint
gtk_container_get_border_width (GtkContainer *container)
{
  g_return_val_if_fail (GTK_IS_CONTAINER (container), 0);

  return container->border_width;
}

/**
 * gtk_container_add:
 * @container: a #GtkContainer
 * @widget: a widget to be placed inside @container
 * 
 * Adds @widget to @container. Typically used for simple containers
 * such as #GtkWindow, #GtkFrame, or #GtkButton; for more complicated
 * layout containers such as #GtkBox or #GtkTable, this function will
 * pick default packing parameters that may not be correct.  So
 * consider functions such as gtk_box_pack_start() and
 * gtk_table_attach() as an alternative to gtk_container_add() in
 * those cases. A widget may be added to only one container at a time;
 * you can't place the same widget inside two different containers.
 **/
void
gtk_container_add (GtkContainer *container,
		   GtkWidget    *widget)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (widget->parent != NULL)
    {
      g_warning ("Attempting to add a widget with type %s to a container of "
                 "type %s, but the widget is already inside a container of type %s, "
                 "the GTK+ FAQ at http://www.gtk.org/faq/ explains how to reparent a widget.",
                 g_type_name (G_OBJECT_TYPE (widget)),
                 g_type_name (G_OBJECT_TYPE (container)),
                 g_type_name (G_OBJECT_TYPE (widget->parent)));
      return;
    }

  gtk_signal_emit (GTK_OBJECT (container), container_signals[ADD], widget);
}

/**
 * gtk_container_remove:
 * @container: a #GtkContainer
 * @widget: a current child of @container
 * 
 * Removes @widget from @container. @widget must be inside @container.
 * Note that @container will own a reference to @widget, and that this
 * may be the last reference held; so removing a widget from its
 * container can destroy that widget. If you want to use @widget
 * again, you need to add a reference to it while it's not inside
 * a container, using g_object_ref().
 **/
void
gtk_container_remove (GtkContainer *container,
		      GtkWidget    *widget)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->parent == GTK_WIDGET (container));
  
  gtk_signal_emit (GTK_OBJECT (container), container_signals[REMOVE], widget);
}

void
_gtk_container_dequeue_resize_handler (GtkContainer *container)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_CONTAINER_RESIZE_PENDING (container));

  container_resize_queue = g_slist_remove (container_resize_queue, container);
  GTK_PRIVATE_UNSET_FLAG (container, GTK_RESIZE_PENDING);
}

void
_gtk_container_clear_resize_widgets (GtkContainer *container)
{
  GSList *node;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

  node = container->resize_widgets;

  while (node)
    {
      GtkWidget *widget = node->data;

      GTK_PRIVATE_UNSET_FLAG (widget, GTK_RESIZE_NEEDED);
      node = node->next;
    }
  
  g_slist_free (container->resize_widgets);
  container->resize_widgets = NULL;
}

void
gtk_container_set_resize_mode (GtkContainer  *container,
			       GtkResizeMode  resize_mode)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (resize_mode <= GTK_RESIZE_IMMEDIATE);
  
  if (GTK_WIDGET_TOPLEVEL (container) &&
      resize_mode == GTK_RESIZE_PARENT)
    {
      resize_mode = GTK_RESIZE_QUEUE;
      g_object_notify (G_OBJECT (container), "resize_mode");
    }
  
  if (container->resize_mode != resize_mode)
    {
      container->resize_mode = resize_mode;
      
      if (resize_mode == GTK_RESIZE_IMMEDIATE)
	gtk_container_check_resize (container);
      else
	{
	  _gtk_container_clear_resize_widgets (container);
	  gtk_widget_queue_resize (GTK_WIDGET (container));
	}
       g_object_notify (G_OBJECT (container), "resize_mode");
    }
}

/**
 * gtk_container_get_resize_mode:
 * @container: a #GtkContainer
 * 
 * Returns the resize mode for the container. See
 * gtk_container_set_resize_mode ().
 *
 * Return value: the current resize mode
 **/
GtkResizeMode
gtk_container_get_resize_mode (GtkContainer *container)
{
  g_return_val_if_fail (GTK_IS_CONTAINER (container), GTK_RESIZE_PARENT);

  return container->resize_mode;
}

void
gtk_container_set_reallocate_redraws (GtkContainer *container,
				      gboolean      needs_redraws)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));

  container->reallocate_redraws = needs_redraws ? TRUE : FALSE;
}

static GtkContainer*
gtk_container_get_resize_container (GtkContainer *container)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (container);

  while (widget->parent)
    {
      widget = widget->parent;
      if (GTK_IS_RESIZE_CONTAINER (widget) && !GTK_WIDGET_RESIZE_NEEDED (widget))
	break;
    }

  return GTK_IS_RESIZE_CONTAINER (widget) ? (GtkContainer*) widget : NULL;
}

static gboolean
gtk_container_idle_sizer (gpointer data)
{
  GDK_THREADS_ENTER ();

  /* we may be invoked with a container_resize_queue of NULL, because
   * queue_resize could have been adding an extra idle function while
   * the queue still got processed. we better just ignore such case
   * than trying to explicitely work around them with some extra flags,
   * since it doesn't cause any actual harm.
   */
  while (container_resize_queue)
    {
      GSList *slist;
      GtkWidget *widget;

      slist = container_resize_queue;
      container_resize_queue = slist->next;
      widget = slist->data;
      g_slist_free_1 (slist);

      GTK_PRIVATE_UNSET_FLAG (widget, GTK_RESIZE_PENDING);
      gtk_container_check_resize (GTK_CONTAINER (widget));
    }

  gdk_window_process_all_updates ();

  GDK_THREADS_LEAVE ();

  return FALSE;
}

void
_gtk_container_queue_resize (GtkContainer *container)
{
  GtkContainer *resize_container;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

  /* clear resize widgets for resize containers
   * before aborting prematurely. this is especially
   * important for toplevels which may need imemdiate
   * processing or their resize handler to be queued.
   */
  if (GTK_IS_RESIZE_CONTAINER (container))
    _gtk_container_clear_resize_widgets (container);
  if (GTK_OBJECT_DESTROYED (container) ||
      GTK_WIDGET_RESIZE_NEEDED (container))
    return;
  
  resize_container = gtk_container_get_resize_container (container);
  
  if (resize_container)
    {
      if (GTK_WIDGET_VISIBLE (resize_container) &&
	  (GTK_WIDGET_TOPLEVEL (resize_container) || GTK_WIDGET_DRAWABLE (resize_container)))
	{
	  switch (resize_container->resize_mode)
	    {
	    case GTK_RESIZE_QUEUE:
	      if (!GTK_CONTAINER_RESIZE_PENDING (resize_container))
		{
		  GTK_PRIVATE_SET_FLAG (resize_container, GTK_RESIZE_PENDING);
		  if (container_resize_queue == NULL)
		    gtk_idle_add_priority (GTK_PRIORITY_RESIZE,
					   gtk_container_idle_sizer,
					   NULL);
		  container_resize_queue = g_slist_prepend (container_resize_queue, resize_container);
		}
	      
	      GTK_PRIVATE_SET_FLAG (container, GTK_RESIZE_NEEDED);
	      resize_container->resize_widgets =
		g_slist_prepend (resize_container->resize_widgets, container);
	      break;

	    case GTK_RESIZE_IMMEDIATE:
	      GTK_PRIVATE_SET_FLAG (container, GTK_RESIZE_NEEDED);
	      resize_container->resize_widgets =
		g_slist_prepend (resize_container->resize_widgets, container);
	      gtk_container_check_resize (resize_container);
	      break;

	    case GTK_RESIZE_PARENT:
	      /* Ignore, should not be reached */
	      break;
	    }
	}
      else
	{
	  /* we need to let hidden resize containers know that something
	   * changed while they where hidden (currently only evaluated by
	   * toplevels).
	   */
	  resize_container->need_resize = TRUE;
	}
    }
}

void
gtk_container_check_resize (GtkContainer *container)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  
  gtk_signal_emit (GTK_OBJECT (container), container_signals[CHECK_RESIZE]);
}

static void
gtk_container_real_check_resize (GtkContainer *container)
{
  GtkWidget *widget;
  GtkRequisition requisition;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  
  widget = GTK_WIDGET (container);
  
  gtk_widget_size_request (widget, &requisition);
  
  if (requisition.width > widget->allocation.width ||
      requisition.height > widget->allocation.height)
    {
      if (GTK_IS_RESIZE_CONTAINER (container))
	gtk_widget_size_allocate (GTK_WIDGET (container),
				  &GTK_WIDGET (container)->allocation);
      else
	gtk_widget_queue_resize (widget);
    }
  else
    {
      gtk_container_resize_children (container);
    }
}

/* The container hasn't changed size but one of its children
 *  queued a resize request. Which means that the allocation
 *  is not sufficient for the requisition of some child.
 *  We've already performed a size request at this point,
 *  so we simply need to run through the list of resize
 *  widgets and reallocate their sizes appropriately. We
 *  make the optimization of not performing reallocation
 *  for a widget who also has a parent in the resize widgets
 *  list. GTK_RESIZE_NEEDED is used for flagging those
 *  parents inside this function.
 */
void
gtk_container_resize_children (GtkContainer *container)
{
  GtkWidget *widget;
  GtkWidget *resize_container;
  GSList *resize_widgets;
  GSList *resize_containers;
  GSList *node;
  
  /* resizing invariants:
   * toplevels have *always* resize_mode != GTK_RESIZE_PARENT set.
   * containers with resize_mode==GTK_RESIZE_PARENT have to have resize_widgets
   * set to NULL.
   * containers that are flagged RESIZE_NEEDED must have resize_widgets set to
   * NULL, or are toplevels (thus have ->parent set to NULL).
   * widgets that are in some container->resize_widgets list must be flagged with
   * RESIZE_NEEDED.
   * widgets that have RESIZE_NEEDED set must be referenced in some
   * GTK_IS_RESIZE_CONTAINER (container)->resize_widgets list.
   * containers that have an idle sizer pending must be flagged with
   * RESIZE_PENDING.
   */
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

  /* we first check out if we actually need to perform a resize,
   * which is not the case if we got another container queued for
   * a resize in our ancestry. also we can skip the whole
   * resize_widgets checks if we are a toplevel and NEED_RESIZE.
   * this code assumes that our allocation is sufficient for our
   * requisition, since otherwise we would NEED_RESIZE.
   */
  resize_container = GTK_WIDGET (container);
  while (resize_container)
    {
      if (GTK_WIDGET_RESIZE_NEEDED (resize_container))
	break;
      resize_container = resize_container->parent;
    }
  if (resize_container)
    {
      /* queue_resize and size_allocate both clear our
       * resize_widgets list.
       */
      if (resize_container->parent)
	_gtk_container_queue_resize (container);
      else
	gtk_widget_size_allocate (GTK_WIDGET (container),
				  &GTK_WIDGET (container)->allocation);
      return;
    }

  resize_container = GTK_WIDGET (container);

  /* we now walk the ancestry for all resize widgets as long
   * as they are our children and as long as their allocation
   * is insufficient, since we don't need to reallocate below that.
   */
  resize_widgets = container->resize_widgets;
  container->resize_widgets = NULL;
  for (node = resize_widgets; node; node = node->next)
    {
      widget = node->data;

      GTK_PRIVATE_UNSET_FLAG (widget, GTK_RESIZE_NEEDED);

      while (widget->parent != resize_container &&
	     ((widget->allocation.width < widget->requisition.width) ||
	      (widget->allocation.height < widget->requisition.height)))
	widget = widget->parent;
      
      GTK_PRIVATE_SET_FLAG (widget, GTK_RESIZE_NEEDED);
      node->data = widget;
    }

  /* for the newly setup resize_widgets list, we now walk each widget's
   * ancestry to sort those widgets out that have RESIZE_NEEDED parents.
   * we can safely stop the walk if we are the parent, since we checked
   * our own ancestry already.
   */
  resize_containers = NULL;
  for (node = resize_widgets; node; node = node->next)
    {
      GtkWidget *parent;

      widget = node->data;
      
      if (!GTK_WIDGET_RESIZE_NEEDED (widget))
	continue;
      
      parent = widget->parent;
      
      while (parent != resize_container)
	{
	  if (GTK_WIDGET_RESIZE_NEEDED (parent))
	    {
	      GTK_PRIVATE_UNSET_FLAG (widget, GTK_RESIZE_NEEDED);
	      widget = parent;
	    }
	  parent = parent->parent;
	}
      
      if (!g_slist_find (resize_containers, widget))
	{
	  resize_containers = g_slist_prepend (resize_containers, widget);
	  gtk_widget_ref (widget);
	}
    }
  g_slist_free (resize_widgets);
  
  for (node = resize_containers; node; node = node->next)
    {
      widget = node->data;
      
      GTK_PRIVATE_UNSET_FLAG (widget, GTK_RESIZE_NEEDED);

      gtk_widget_size_allocate (widget, &widget->allocation);

      gtk_widget_unref (widget);
    }
  g_slist_free (resize_containers);
}

/**
 * gtk_container_forall:
 * @container: a #GtkContainer
 * @callback: a callback
 * @callback_data: callback user data
 * 
 * Invokes @callback on each child of @container, including children
 * that are considered "internal" (implementation details of the
 * container). "Internal" children generally weren't added by the user
 * of the container, but were added by the container implementation
 * itself.  Most applications should use gtk_container_foreach(),
 * rather than gtk_container_forall().
 **/
void
gtk_container_forall (GtkContainer *container,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkContainerClass *class;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (callback != NULL);

  class = GTK_CONTAINER_GET_CLASS (container);

  if (class->forall)
    class->forall (container, TRUE, callback, callback_data);
}

/**
 * gtk_container_foreach:
 * @container: a #GtkContainer
 * @callback: a callback
 * @callback_data: callback user data
 * 
 * Invokes @callback on each non-internal child of @container.  See
 * gtk_container_forall() for details on what constitutes an
 * "internal" child.  Most applications should use
 * gtk_container_foreach(), rather than gtk_container_forall().
 **/
void
gtk_container_foreach (GtkContainer *container,
		       GtkCallback   callback,
		       gpointer      callback_data)
{
  GtkContainerClass *class;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (callback != NULL);

  class = GTK_CONTAINER_GET_CLASS (container);

  if (class->forall)
    class->forall (container, FALSE, callback, callback_data);
}

typedef struct _GtkForeachData	GtkForeachData;
struct _GtkForeachData
{
  GtkObject         *container;
  GtkCallbackMarshal callback;
  gpointer           callback_data;
};

static void
gtk_container_foreach_unmarshal (GtkWidget *child,
				 gpointer data)
{
  GtkForeachData *fdata = (GtkForeachData*) data;
  GtkArg args[2];
  
  /* first argument */
  args[0].name = NULL;
  args[0].type = GTK_OBJECT_TYPE (child);
  GTK_VALUE_OBJECT (args[0]) = GTK_OBJECT (child);
  
  /* location for return value */
  args[1].name = NULL;
  args[1].type = GTK_TYPE_NONE;
  
  fdata->callback (fdata->container, fdata->callback_data, 1, args);
}

void
gtk_container_foreach_full (GtkContainer       *container,
			    GtkCallback         callback,
			    GtkCallbackMarshal  marshal,
			    gpointer            callback_data,
			    GtkDestroyNotify    notify)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

  if (marshal)
    {
      GtkForeachData fdata;
  
      fdata.container     = GTK_OBJECT (container);
      fdata.callback      = marshal;
      fdata.callback_data = callback_data;

      gtk_container_foreach (container, gtk_container_foreach_unmarshal, &fdata);
    }
  else
    {
      g_return_if_fail (callback != NULL);

      gtk_container_foreach (container, callback, &callback_data);
    }

  if (notify)
    notify (callback_data);
}

void
gtk_container_set_focus_child (GtkContainer *container,
			       GtkWidget    *widget)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  if (widget)
    g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_signal_emit (GTK_OBJECT (container), container_signals[SET_FOCUS_CHILD], widget);
}

GList*
gtk_container_get_children (GtkContainer *container)
{
  GList *children;

  children = NULL;

  gtk_container_foreach (container,
			 gtk_container_children_callback,
			 &children);

  return g_list_reverse (children);
}

static void
gtk_container_child_position_callback (GtkWidget *widget,
				       gpointer   client_data)
{
  struct {
    GtkWidget *child;
    guint i;
    guint index;
  } *data = client_data;

  data->i++;
  if (data->child == widget)
    data->index = data->i;
}

static gchar*
gtk_container_child_default_composite_name (GtkContainer *container,
					    GtkWidget    *child)
{
  struct {
    GtkWidget *child;
    guint i;
    guint index;
  } data;
  gchar *name;

  /* fallback implementation */
  data.child = child;
  data.i = 0;
  data.index = 0;
  gtk_container_forall (container,
			gtk_container_child_position_callback,
			&data);
  
  name = g_strdup_printf ("%s-%u",
			  gtk_type_name (GTK_OBJECT_TYPE (child)),
			  data.index);

  return name;
}

gchar*
_gtk_container_child_composite_name (GtkContainer *container,
				    GtkWidget    *child)
{
  g_return_val_if_fail (container != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (child != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);
  g_return_val_if_fail (child->parent == GTK_WIDGET (container), NULL);

  if (GTK_WIDGET_COMPOSITE_CHILD (child))
    {
      static GQuark quark_composite_name = 0;
      gchar *name;

      if (!quark_composite_name)
	quark_composite_name = g_quark_from_static_string ("gtk-composite-name");

      name = gtk_object_get_data_by_id (GTK_OBJECT (child), quark_composite_name);
      if (!name)
	{
	  GtkContainerClass *class;

	  class = GTK_CONTAINER_GET_CLASS (container);
	  if (class->composite_name)
	    name = class->composite_name (container, child);
	}
      else
	name = g_strdup (name);

      return name;
    }
  
  return NULL;
}

static void
gtk_container_real_set_focus_child (GtkContainer     *container,
				    GtkWidget        *child)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  if (child)
    g_return_if_fail (GTK_IS_WIDGET (child));

  if (child != container->focus_child)
    {
      if (container->focus_child)
	gtk_widget_unref (container->focus_child);
      container->focus_child = child;
      if (container->focus_child)
	gtk_widget_ref (container->focus_child);
    }


  /* check for h/v adjustments
   */
  if (container->focus_child)
    {
      GtkAdjustment *adjustment;
      
      adjustment = gtk_object_get_data_by_id (GTK_OBJECT (container), vadjustment_key_id);
      if (adjustment)
	gtk_adjustment_clamp_page (adjustment,
				   container->focus_child->allocation.y,
				   (container->focus_child->allocation.y +
				    container->focus_child->allocation.height));

      adjustment = gtk_object_get_data_by_id (GTK_OBJECT (container), hadjustment_key_id);
      if (adjustment)
	gtk_adjustment_clamp_page (adjustment,
				   container->focus_child->allocation.x,
				   (container->focus_child->allocation.x +
				    container->focus_child->allocation.width));
    }
}

static GList*
get_focus_chain (GtkContainer *container)
{
  return g_object_get_data (G_OBJECT (container), "gtk-container-focus-chain");
}

static GList*
filter_unfocusable (GtkContainer *container,
                    GList        *list)
{
  GList *tmp_list;
  GList *tmp_list2;
  
  tmp_list = list;
  while (tmp_list)
    {
      if (GTK_WIDGET_IS_SENSITIVE (tmp_list->data) &&
          GTK_WIDGET_DRAWABLE (tmp_list->data) &&
          (GTK_IS_CONTAINER (tmp_list->data) || GTK_WIDGET_CAN_FOCUS (tmp_list->data)))
        tmp_list = tmp_list->next;
      else
        {
          tmp_list2 = tmp_list;
          tmp_list = tmp_list->next;
          
          list = g_list_remove_link (list, tmp_list2);
          g_list_free_1 (tmp_list2);
        }
    }

  return list;
}

static gboolean
gtk_container_focus (GtkWidget        *widget,
                     GtkDirectionType  direction)
{
  GList *children;
  gint return_val;
  GtkContainer *container;

  g_return_val_if_fail (GTK_IS_CONTAINER (widget), FALSE);

  container = GTK_CONTAINER (widget);

  return_val = FALSE;

  if (GTK_WIDGET_CAN_FOCUS (container))
    {
      if (!GTK_WIDGET_HAS_FOCUS (container))
	{
	  gtk_widget_grab_focus (GTK_WIDGET (container));
	  return_val = TRUE;
	}
    }
  else
    {
      /* Get a list of the containers children, allowing focus
       * chain to override.
       */
      if (container->has_focus_chain)
        {
          children = g_list_copy (get_focus_chain (container));
        }
      else
        {
          children = NULL;
          gtk_container_forall (container,
                                gtk_container_children_callback,
                                &children);
          children = g_list_reverse (children);
        }

      if (children)
	{
	  /* Remove any children which are inappropriate for focus movement
	   */
          children = filter_unfocusable (container, children);
          
	  switch (direction)
	    {
	    case GTK_DIR_TAB_FORWARD:
	    case GTK_DIR_TAB_BACKWARD:
              if (container->has_focus_chain)
                {
                  if (direction == GTK_DIR_TAB_BACKWARD)
                    children = g_list_reverse (children);
                  return_val = gtk_container_focus_move (container, children, direction);
                }
              else
                return_val = gtk_container_focus_tab (container, children, direction);
	      break;
	    case GTK_DIR_UP:
	    case GTK_DIR_DOWN:
	      return_val = gtk_container_focus_up_down (container, &children, direction);
	      break;
	    case GTK_DIR_LEFT:
	    case GTK_DIR_RIGHT:
	      return_val = gtk_container_focus_left_right (container, &children, direction);
	      break;
	    }

	  g_list_free (children);
	}
    }

  return return_val;
}

static gboolean
gtk_container_focus_tab (GtkContainer     *container,
			 GList            *children,
			 GtkDirectionType  direction)
{
  GtkWidget *child;
  GtkWidget *child2;
  GList *tmp_list;
  guint length;
  guint i, j;

  length = g_list_length (children);

  /* sort the children in the y direction */
  for (i = 1; i < length; i++)
    {
      j = i;
      tmp_list = g_list_nth (children, j);
      child = tmp_list->data;

      while (j > 0)
	{
	  child2 = tmp_list->prev->data;
	  if (child->allocation.y < child2->allocation.y)
	    {
	      tmp_list->data = tmp_list->prev->data;
	      tmp_list = tmp_list->prev;
	      j--;
	    }
	  else
	    break;
	}

      tmp_list->data = child;
    }

  /* sort the children in the x direction while
   *  maintaining the y direction sort.
   */
  for (i = 1; i < length; i++)
    {
      j = i;
      tmp_list = g_list_nth (children, j);
      child = tmp_list->data;

      while (j > 0)
	{
	  child2 = tmp_list->prev->data;
	  if ((child->allocation.x < child2->allocation.x) &&
	      (child->allocation.y == child2->allocation.y))
	    {
	      tmp_list->data = tmp_list->prev->data;
	      tmp_list = tmp_list->prev;
	      j--;
	    }
	  else
	    break;
	}

      tmp_list->data = child;
    }

  /* if we are going backwards then reverse the order
   *  of the children.
   */
  if (direction == GTK_DIR_TAB_BACKWARD)
    children = g_list_reverse (children);

  return gtk_container_focus_move (container, children, direction);
}

static gboolean
old_focus_coords (GtkContainer *container, GdkRectangle *old_focus_rect)
{
  GtkWidget *widget = GTK_WIDGET (container);
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
  
  if (toplevel &&
      GTK_IS_WINDOW (toplevel) && GTK_WINDOW (toplevel)->focus_widget &&
      GTK_WIDGET_REALIZED (container) &&
      GTK_WIDGET_REALIZED (GTK_WINDOW (toplevel)->focus_widget))
    {
      GtkWidget *old_focus = GTK_WINDOW (toplevel)->focus_widget;
      GdkWindow *old_parent_window = old_focus->parent ? old_focus->parent->window : old_focus->window;
      GdkWindow *new_parent_window = widget->window;
      GdkWindow *toplevel_window = toplevel->window;
      
      *old_focus_rect = old_focus->allocation;
      
      /* Translate coordinates to the toplevel */
      
      while (old_parent_window != toplevel_window)
	{
	  gint dx, dy;
	  
	  gdk_window_get_position (old_parent_window, &dx, &dy);
	  
	  old_focus_rect->x += dx;
	  old_focus_rect->y += dy;
	  
	  old_parent_window = gdk_window_get_parent (old_parent_window);
	}
      
      /* Translate coordinates back to the new container */
      
      while (new_parent_window != toplevel_window)
	{
	  gint dx, dy;
	  
	  gdk_window_get_position (new_parent_window, &dx, &dy);
	  
	  old_focus_rect->x -= dx;
	  old_focus_rect->y -= dy;
	  
	  new_parent_window = gdk_window_get_parent (new_parent_window);
	}

      return TRUE;
    }

  return FALSE;
}

typedef struct _CompareInfo CompareInfo;

struct _CompareInfo
{
  gint x;
  gint y;
};

static gint
up_down_compare (gconstpointer a,
		 gconstpointer b,
		 gpointer      data)
{
  const GtkWidget *child1 = a;
  const GtkWidget *child2 = b;
  CompareInfo *compare = data;

  gint y1 = child1->allocation.y + child1->allocation.height / 2;
  gint y2 = child2->allocation.y + child2->allocation.height / 2;

  if (y1 == y2)
    {
      gint x1 = abs (child1->allocation.x + child1->allocation.width / 2 - compare->x);
      gint x2 = abs (child2->allocation.x + child2->allocation.width / 2 - compare->x);

      if (compare->y < y1)
	return (x1 < x2) ? -1 : ((x1 == x2) ? 0 : 1);
      else
	return (x1 < x2) ? 1 : ((x1 == x2) ? 0 : -1);
    }
  else
    return (y1 < y2) ? -1 : 1;
}

static gboolean
gtk_container_focus_up_down (GtkContainer     *container,
			     GList           **children,
			     GtkDirectionType  direction)
{
  CompareInfo compare;
  GList *tmp_list;

  if (container->focus_child)
    {
      gint compare_x1;
      gint compare_x2;
      gint compare_y;
      
      /* Delete widgets from list that don't match minimum criteria */

      compare_x1 = container->focus_child->allocation.x;
      compare_x2 = container->focus_child->allocation.x + container->focus_child->allocation.width;

      if (direction == GTK_DIR_UP)
	compare_y = container->focus_child->allocation.y;
      else
	compare_y = container->focus_child->allocation.y + container->focus_child->allocation.height;
      
      tmp_list = *children;
      while (tmp_list)
	{
	  GtkWidget *child = tmp_list->data;
	  GList *next = tmp_list->next;
	  gint child_x1, child_x2;
	  
	  if (child != container->focus_child)
	    {
	      child_x1 = child->allocation.x;
	      child_x2 = child->allocation.x + child->allocation.width;
	      
	      if ((child_x2 <= compare_x1 || child_x1 >= compare_x2) /* No horizontal overlap */ ||
		  (direction == GTK_DIR_DOWN && child->allocation.y + child->allocation.height < compare_y) || /* Not below */
		  (direction == GTK_DIR_UP && child->allocation.y > compare_y)) /* Not above */
		{
		  *children = g_list_delete_link (*children, tmp_list);
		}
	    }
	  
	  tmp_list = next;
	}

      compare.x = (compare_x1 + compare_x2) / 2;
      compare.y = container->focus_child->allocation.y + container->focus_child->allocation.height / 2;
    }
  else
    {
      /* No old focus widget, need to figure out starting x,y some other way
       */
      GtkWidget *widget = GTK_WIDGET (container);
      GdkRectangle old_focus_rect;

      if (old_focus_coords (container, &old_focus_rect))
	{
	  compare.x = old_focus_rect.x + old_focus_rect.width / 2;
	}
      else
	{
	  if (GTK_WIDGET_NO_WINDOW (widget))
	    compare.x = widget->allocation.x + widget->allocation.width / 2;
	  else
	    compare.x = widget->allocation.width / 2;
	}
      
      if (GTK_WIDGET_NO_WINDOW (widget))
	compare.y = (direction == GTK_DIR_DOWN) ? widget->allocation.y : widget->allocation.y + widget->allocation.height;
      else
	compare.y = (direction == GTK_DIR_DOWN) ? 0 : + widget->allocation.height;
    }

  *children = g_list_sort_with_data (*children, up_down_compare, &compare);

  if (direction == GTK_DIR_UP)
    *children = g_list_reverse (*children);

  return gtk_container_focus_move (container, *children, direction);
}

static gint
left_right_compare (gconstpointer a,
		    gconstpointer b,
		    gpointer      data)
{
  const GtkWidget *child1 = a;
  const GtkWidget *child2 = b;
  CompareInfo *compare = data;

  gint x1 = child1->allocation.x + child1->allocation.width / 2;
  gint x2 = child2->allocation.x + child2->allocation.width / 2;

  if (x1 == x2)
    {
      gint y1 = abs (child1->allocation.y + child1->allocation.height / 2 - compare->y);
      gint y2 = abs (child2->allocation.y + child2->allocation.height / 2 - compare->y);

      if (compare->x < x1)
	return (y1 < y2) ? -1 : ((y1 == y2) ? 0 : 1);
      else
	return (y1 < y2) ? 1 : ((y1 == y2) ? 0 : -1);
    }
  else
    return (x1 < x2) ? -1 : 1;
}

static gboolean
gtk_container_focus_left_right (GtkContainer     *container,
				GList           **children,
				GtkDirectionType  direction)
{
  CompareInfo compare;
  GList *tmp_list;

  if (container->focus_child)
    {
      gint compare_y1;
      gint compare_y2;
      gint compare_x;
      
      /* Delete widgets from list that don't match minimum criteria */

      compare_y1 = container->focus_child->allocation.y;
      compare_y2 = container->focus_child->allocation.y + container->focus_child->allocation.height;

      if (direction == GTK_DIR_LEFT)
	compare_x = container->focus_child->allocation.x;
      else
	compare_x = container->focus_child->allocation.x + container->focus_child->allocation.width;
      
      tmp_list = *children;
      while (tmp_list)
	{
	  GtkWidget *child = tmp_list->data;
	  GList *next = tmp_list->next;
	  gint child_y1, child_y2;
	  
	  if (child != container->focus_child)
	    {
	      child_y1 = child->allocation.y;
	      child_y2 = child->allocation.y + child->allocation.height;
	      
	      if ((child_y2 <= compare_y1 || child_y1 >= compare_y2) /* No vertical overlap */ ||
		  (direction == GTK_DIR_RIGHT && child->allocation.x + child->allocation.width < compare_x) || /* Not to left */
		  (direction == GTK_DIR_LEFT && child->allocation.x > compare_x)) /* Not to right */
		{
		  *children = g_list_delete_link (*children, tmp_list);
		}
	    }
	  
	  tmp_list = next;
	}

      compare.y = (compare_y1 + compare_y2) / 2;
      compare.x = container->focus_child->allocation.x + container->focus_child->allocation.width / 2;
    }
  else
    {
      /* No old focus widget, need to figure out starting x,y some other way
       */
      GtkWidget *widget = GTK_WIDGET (container);
      GdkRectangle old_focus_rect;

      if (old_focus_coords (container, &old_focus_rect))
	{
	  compare.y = old_focus_rect.y + old_focus_rect.height / 2;
	}
      else
	{
	  if (GTK_WIDGET_NO_WINDOW (widget))
	    compare.y = widget->allocation.y + widget->allocation.height / 2;
	  else
	    compare.y = widget->allocation.height / 2;
	}
      
      if (GTK_WIDGET_NO_WINDOW (widget))
	compare.x = (direction == GTK_DIR_RIGHT) ? widget->allocation.x : widget->allocation.x + widget->allocation.width;
      else
	compare.x = (direction == GTK_DIR_RIGHT) ? 0 : widget->allocation.width;
    }

  *children = g_list_sort_with_data (*children, left_right_compare, &compare);

  if (direction == GTK_DIR_LEFT)
    *children = g_list_reverse (*children);

  return gtk_container_focus_move (container, *children, direction);
}

static gboolean
gtk_container_focus_move (GtkContainer     *container,
			  GList            *children,
			  GtkDirectionType  direction)
{
  GtkWidget *focus_child;
  GtkWidget *child;

  focus_child = container->focus_child;

  while (children)
    {
      child = children->data;
      children = children->next;

      if (!child)
	continue;
      
      if (focus_child)
        {
          if (focus_child == child)
            {
              focus_child = NULL;

		if (gtk_widget_child_focus (child, direction))
		  return TRUE;
            }
        }
      else if (GTK_WIDGET_DRAWABLE (child) &&
               gtk_widget_is_ancestor (child, GTK_WIDGET (container)))
        {
          if (gtk_widget_child_focus (child, direction))
            return TRUE;
        }
    }

  return FALSE;
}


static void
gtk_container_children_callback (GtkWidget *widget,
				 gpointer   client_data)
{
  GList **children;

  children = (GList**) client_data;
  *children = g_list_prepend (*children, widget);
}

static void
chain_widget_destroyed (GtkWidget *widget,
                        gpointer   user_data)
{
  GtkContainer *container;
  GList *chain;
  
  container = GTK_CONTAINER (user_data);

  chain = g_object_get_data (G_OBJECT (container),
                             "gtk-container-focus-chain");

  chain = g_list_remove (chain, widget);

  g_signal_handlers_disconnect_by_func (G_OBJECT (widget),
                                        chain_widget_destroyed,
                                        user_data);
  
  g_object_set_data (G_OBJECT (container),
                     "gtk-container-focus-chain",
                     chain);  
}

void
gtk_container_set_focus_chain (GtkContainer *container,
                               GList        *focusable_widgets)
{
  GList *chain;
  GList *tmp_list;
  
  g_return_if_fail (GTK_IS_CONTAINER (container));
  
  if (container->has_focus_chain)
    gtk_container_unset_focus_chain (container);

  container->has_focus_chain = TRUE;
  
  chain = NULL;
  tmp_list = focusable_widgets;
  while (tmp_list != NULL)
    {
      g_return_if_fail (GTK_IS_WIDGET (tmp_list->data));
      
      /* In principle each widget in the chain should be a descendant
       * of the container, but we don't want to check that here, it's
       * expensive and also it's allowed to set the focus chain before
       * you pack the widgets, or have a widget in the chain that isn't
       * always packed. So we check for ancestor during actual traversal.
       */

      chain = g_list_prepend (chain, tmp_list->data);

      gtk_signal_connect (GTK_OBJECT (tmp_list->data),
                          "destroy",
                          GTK_SIGNAL_FUNC (chain_widget_destroyed),
                          container);
      
      tmp_list = g_list_next (tmp_list);
    }

  chain = g_list_reverse (chain);
  
  g_object_set_data (G_OBJECT (container),
                     "gtk-container-focus-chain",
                     chain);
}

/**
 * gtk_container_get_focus_chain:
 * @container:         a #GtkContainer
 * @focusable_widgets: location to store the focus chain of the
 *                     container, or %NULL. You should free this list
 *                     using g_list_free() when you are done with it, however
 *                     no additional reference count is added to the
 *                     individual widgets in the focus chain.
 * 
 * Retrieve the focus chain of the container, if one has been
 * set explicitely. If no focus chain has been explicitely
 * set, GTK+ computes the focus chain based on the positions
 * of the children. In that case, GTK+ stores %NULL in
 * @focusable_widgets and returns %FALSE.
 *
 * Return value: %TRUE if the focus chain of the container,
 *   has been set explicitely.
 **/
gboolean
gtk_container_get_focus_chain (GtkContainer *container,
			       GList       **focus_chain)
{
  g_return_val_if_fail (GTK_IS_CONTAINER (container), FALSE);

  if (focus_chain)
    {
      if (container->has_focus_chain)
	*focus_chain = g_list_copy (get_focus_chain (container));
      else
	*focus_chain = NULL;
    }

  return container->has_focus_chain;
}

void
gtk_container_unset_focus_chain (GtkContainer  *container)
{  
  g_return_if_fail (GTK_IS_CONTAINER (container));

  if (container->has_focus_chain)
    {
      GList *chain;
      GList *tmp_list;
      
      chain = get_focus_chain (container);
      
      container->has_focus_chain = FALSE;
      
      g_object_set_data (G_OBJECT (container), "gtk-container-focus-chain",
                         NULL);

      tmp_list = chain;
      while (tmp_list != NULL)
        {
          g_signal_handlers_disconnect_by_func (G_OBJECT (tmp_list->data),
                                                chain_widget_destroyed,
                                                container);
          
          tmp_list = g_list_next (tmp_list);
        }

      g_list_free (chain);
    }
}

void
gtk_container_set_focus_vadjustment (GtkContainer  *container,
				     GtkAdjustment *adjustment)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (adjustment)
    gtk_object_ref (GTK_OBJECT(adjustment));

  gtk_object_set_data_by_id_full (GTK_OBJECT (container),
				  vadjustment_key_id,
				  adjustment,
				  (GtkDestroyNotify) gtk_object_unref);
}

/**
 * gtk_container_get_focus_vadjustment:
 * @container: a #GtkContainer
 *
 * Retrieves the vertical focus adjustment for the container. See
 * gtk_container_set_focus_vadjustment ().
 *
 * Return value: the vertical focus adjustment, or %NULL if
 *   none has been set.
 **/
GtkAdjustment *
gtk_container_get_focus_vadjustment (GtkContainer *container)
{
  GtkAdjustment *vadjustment;
    
  g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);

  vadjustment = gtk_object_get_data_by_id (GTK_OBJECT (container),
					   vadjustment_key_id);

  return vadjustment;
}

void
gtk_container_set_focus_hadjustment (GtkContainer  *container,
				     GtkAdjustment *adjustment)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (adjustment)
    gtk_object_ref (GTK_OBJECT (adjustment));

  gtk_object_set_data_by_id_full (GTK_OBJECT (container),
				  hadjustment_key_id,
				  adjustment,
				  (GtkDestroyNotify) gtk_object_unref);
}

/**
 * gtk_container_get_focus_hadjustment:
 * @container: a #GtkContainer
 *
 * Retrieves the horizontal focus adjustment for the container. See
 * gtk_container_set_focus_hadjustment ().
 *
 * Return value: the horizontal focus adjustment, or %NULL if none
 *   none has been set.
 **/
GtkAdjustment *
gtk_container_get_focus_hadjustment (GtkContainer *container)
{
  GtkAdjustment *hadjustment;

  g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);

  hadjustment = gtk_object_get_data_by_id (GTK_OBJECT (container),
		  			   hadjustment_key_id);

  return hadjustment;
}


static void
gtk_container_show_all (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (widget));

  gtk_container_foreach (GTK_CONTAINER (widget),
			 (GtkCallback) gtk_widget_show_all,
			 NULL);
  gtk_widget_show (widget);
}

static void
gtk_container_hide_all (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (widget));

  gtk_widget_hide (widget);
  gtk_container_foreach (GTK_CONTAINER (widget),
			 (GtkCallback) gtk_widget_hide_all,
			 NULL);
}


static void
gtk_container_expose_child (GtkWidget *child,
			    gpointer   client_data)
{
  struct {
    GtkWidget *container;
    GdkEventExpose *event;
  } *data = client_data;
  
  gtk_container_propagate_expose (GTK_CONTAINER (data->container),
				  child,
				  data->event);
}

static gint 
gtk_container_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  struct {
    GtkWidget *container;
    GdkEventExpose *event;
  } data;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CONTAINER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  
  if (GTK_WIDGET_DRAWABLE (widget)) 
    {
      data.container = widget;
      data.event = event;
      
      gtk_container_foreach (GTK_CONTAINER (widget),
			     gtk_container_expose_child,
			     &data);
    }   
  
  return TRUE;
}

static void
gtk_container_map_child (GtkWidget *child,
			 gpointer   client_data)
{
  if (GTK_WIDGET_VISIBLE (child) &&
      GTK_WIDGET_CHILD_VISIBLE (child) &&
      !GTK_WIDGET_MAPPED (child))
    gtk_widget_map (child);
}

static void
gtk_container_map (GtkWidget *widget)
{
  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  gtk_container_forall (GTK_CONTAINER (widget),
			gtk_container_map_child,
			NULL);

  if (!GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_show (widget->window);
}

static void
gtk_container_unmap (GtkWidget *widget)
{
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

  if (!GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_hide (widget->window);
  else
    gtk_container_forall (GTK_CONTAINER (widget),
			  (GtkCallback)gtk_widget_unmap,
			  NULL);
}

/**
 * gtk_container_propagate_expose:
 * @container: a #GtkContainer
 * @child: a child of @container
 * @event: a expose event sent to container
 *
 *  When a container receives an expose event, it must send synthetic
 * expose events to all children that don't have their own GdkWindows.
 * This function provides a convenient way of doing this. A container,
 * when it receives an expose event, gtk_container_propagate_expose() 
 * once for each child, passing in the event the container received.
 *
 * gtk_container_propagate expose() takes care of deciding whether
 * an expose event needs to be sent to the child, intersecting
 * the event's area with the child area, and sending the event.
 * 
 * In most cases, a container can simply either simply inherit the
 * ::expose implementation from GtkContainer, or, do some drawing 
 * and then chain to the ::expose implementation from GtkContainer.
 **/
void
gtk_container_propagate_expose (GtkContainer   *container,
				GtkWidget      *child,
				GdkEventExpose *event)
{
  GdkEventExpose child_event;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (event != NULL);

  g_assert (child->parent == GTK_WIDGET (container));
  
  if (GTK_WIDGET_DRAWABLE (child) &&
      GTK_WIDGET_NO_WINDOW (child) &&
      (child->window == event->window))
    {
      child_event = *event;

      child_event.region = gtk_widget_region_intersect (child, event->region);
      if (!gdk_region_empty (child_event.region))
	{
	  gdk_region_get_clipbox (child_event.region, &child_event.area);
	  gtk_widget_send_expose (child, (GdkEvent *)&child_event);
	}
      gdk_region_destroy (child_event.region);
    }
}
