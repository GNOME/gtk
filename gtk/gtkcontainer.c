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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <string.h>
#include "gtkcontainer.h"
#include "gtkprivate.h"
#include "gtksignal.h"
#include "gtkmain.h"
#include <stdarg.h>


enum {
  ADD,
  REMOVE,
  CHECK_RESIZE,
  FOCUS,
  SET_FOCUS_CHILD,
  LAST_SIGNAL
};
enum {
  ARG_0,
  ARG_BORDER_WIDTH,
  ARG_RESIZE_MODE,
  ARG_CHILD,
  ARG_REALLOCATE_REDRAWS
};

typedef struct _GtkChildArgInfo	GtkChildArgInfo;
struct _GtkChildArgInfo
{
  gchar *name;
  GtkType type;
  GtkType class_type;
  guint arg_flags;
  guint arg_id;
  guint seq_id;
};

/* The global list of toplevel windows */
static GList *toplevel_list = NULL;

static void gtk_container_base_class_init   (GtkContainerClass *klass);
static void gtk_container_class_init        (GtkContainerClass *klass);
static void gtk_container_init              (GtkContainer      *container);
static void gtk_container_destroy           (GtkObject         *object);
static void gtk_container_get_arg           (GtkObject	       *object,
					     GtkArg            *arg,
					     guint		arg_id);
static void gtk_container_set_arg           (GtkObject	       *object,
					     GtkArg            *arg,
					     guint		arg_id);
static void gtk_container_add_unimplemented (GtkContainer      *container,
					     GtkWidget         *widget);
static void gtk_container_remove_unimplemented (GtkContainer   *container,
						GtkWidget      *widget);
static void gtk_container_real_check_resize (GtkContainer      *container);
static gint gtk_container_real_focus        (GtkContainer      *container,
					     GtkDirectionType   direction);
static void gtk_container_real_set_focus_child (GtkContainer      *container,
					     GtkWidget	       *widget);
static gint gtk_container_focus_tab         (GtkContainer      *container,
					     GList             *children,
					     GtkDirectionType   direction);
static gint gtk_container_focus_up_down     (GtkContainer      *container,
					     GList             *children,
					     GtkDirectionType   direction);
static gint gtk_container_focus_left_right  (GtkContainer      *container,
					     GList             *children,
					     GtkDirectionType   direction);
static gint gtk_container_focus_move        (GtkContainer      *container,
					     GList             *children,
					     GtkDirectionType   direction);
static void gtk_container_children_callback (GtkWidget         *widget,
                                             gpointer           client_data);
static void gtk_container_show_all          (GtkWidget         *widget);
static void gtk_container_hide_all          (GtkWidget         *widget);

static gchar* gtk_container_child_default_composite_name (GtkContainer *container,
							  GtkWidget    *child);



static guint container_signals[LAST_SIGNAL] = { 0 };
static GHashTable *container_child_arg_info_ht = NULL;

static GtkWidgetClass *parent_class = NULL;

static const gchar *vadjustment_key = "gtk-vadjustment";
static guint        vadjustment_key_id = 0;
static const gchar *hadjustment_key = "gtk-hadjustment";
static guint        hadjustment_key_id = 0;
static GSList	   *container_resize_queue = NULL;

GtkType
gtk_container_get_type (void)
{
  static GtkType container_type = 0;

  if (!container_type)
    {
      static const GtkTypeInfo container_info =
      {
	"GtkContainer",
	sizeof (GtkContainer),
	sizeof (GtkContainerClass),
	(GtkClassInitFunc) gtk_container_class_init,
	(GtkObjectInitFunc) gtk_container_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) gtk_container_base_class_init,
      };

      container_type = gtk_type_unique (gtk_widget_get_type (), &container_info);
    }

  return container_type;
}

static void
gtk_container_base_class_init (GtkContainerClass *class)
{
  /* reset instance specifc class fields that don't get inherited */
  class->n_child_args = 0;
  class->set_child_arg = NULL;
  class->get_child_arg = NULL;
}

static void
gtk_container_class_init (GtkContainerClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = gtk_type_class (gtk_widget_get_type ());

  container_child_arg_info_ht = g_hash_table_new (gtk_arg_info_hash,
						  gtk_arg_info_equal);

  vadjustment_key_id = g_quark_from_static_string (vadjustment_key);
  hadjustment_key_id = g_quark_from_static_string (hadjustment_key);
  
  gtk_object_add_arg_type ("GtkContainer::border_width", GTK_TYPE_ULONG, GTK_ARG_READWRITE, ARG_BORDER_WIDTH);
  gtk_object_add_arg_type ("GtkContainer::resize_mode", GTK_TYPE_RESIZE_MODE, GTK_ARG_READWRITE, ARG_RESIZE_MODE);
  gtk_object_add_arg_type ("GtkContainer::child", GTK_TYPE_WIDGET, GTK_ARG_WRITABLE, ARG_CHILD);
  gtk_object_add_arg_type ("GtkContainer::reallocate_redraws", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_REALLOCATE_REDRAWS);

  container_signals[ADD] =
    gtk_signal_new ("add",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkContainerClass, add),
                    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_WIDGET);
  container_signals[REMOVE] =
    gtk_signal_new ("remove",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkContainerClass, remove),
                    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_WIDGET);
  container_signals[CHECK_RESIZE] =
    gtk_signal_new ("check_resize",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkContainerClass, check_resize),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  container_signals[FOCUS] =
    gtk_signal_new ("focus",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkContainerClass, focus),
                    gtk_marshal_ENUM__ENUM,
		    GTK_TYPE_DIRECTION_TYPE, 1,
                    GTK_TYPE_DIRECTION_TYPE);
  container_signals[SET_FOCUS_CHILD] =
    gtk_signal_new ("set-focus-child",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkContainerClass, set_focus_child),
                    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_WIDGET);
  gtk_object_class_add_signals (object_class, container_signals, LAST_SIGNAL);

  object_class->get_arg = gtk_container_get_arg;
  object_class->set_arg = gtk_container_set_arg;
  object_class->destroy = gtk_container_destroy;

  widget_class->show_all = gtk_container_show_all;
  widget_class->hide_all = gtk_container_hide_all;
  
  class->add = gtk_container_add_unimplemented;
  class->remove = gtk_container_remove_unimplemented;
  class->check_resize = gtk_container_real_check_resize;
  class->forall = NULL;
  class->focus = gtk_container_real_focus;
  class->set_focus_child = gtk_container_real_set_focus_child;
  class->child_type = NULL;
  class->composite_name = gtk_container_child_default_composite_name;
}

GtkType
gtk_container_child_type (GtkContainer      *container)
{
  GtkType slot;
  GtkContainerClass *class;

  g_return_val_if_fail (container != NULL, 0);
  g_return_val_if_fail (GTK_IS_CONTAINER (container), 0);

  class = GTK_CONTAINER_CLASS (GTK_OBJECT (container)->klass);
  if (class->child_type)
    slot = class->child_type (container);
  else
    slot = GTK_TYPE_NONE;

  return slot;
}

/****************************************************
 * GtkContainer child argument mechanism
 *
 ****************************************************/

void
gtk_container_add_with_args (GtkContainer      *container,
			     GtkWidget         *widget,
			     const gchar       *first_arg_name,
			     ...)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->parent == NULL);

  gtk_widget_ref (GTK_WIDGET (container));
  gtk_widget_ref (widget);

  if (!GTK_OBJECT_CONSTRUCTED (widget))
    gtk_object_default_construct (GTK_OBJECT (widget));
  gtk_signal_emit (GTK_OBJECT (container), container_signals[ADD], widget);
  
  if (widget->parent)
    {
      va_list var_args;
      GSList *arg_list = NULL;
      GSList *info_list = NULL;
      gchar *error;
      
      va_start (var_args, first_arg_name);
      error = gtk_container_child_args_collect (GTK_OBJECT_TYPE (container),
						&arg_list,
						&info_list,
						first_arg_name,
						var_args);
      va_end (var_args);

      if (error)
	{
	  g_warning ("gtk_container_add_with_args(): %s", error);
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
	      gtk_container_arg_set (container, widget, slist_arg->data, slist_info->data);
	      slist_arg = slist_arg->next;
	      slist_info = slist_info->next;
	    }
	  gtk_args_collect_cleanup (arg_list, info_list);
	}
    }

  gtk_widget_unref (widget);
  gtk_widget_unref (GTK_WIDGET (container));
}

void
gtk_container_addv (GtkContainer      *container,
		    GtkWidget         *widget,
		    guint              n_args,
		    GtkArg            *args)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->parent == NULL);

  gtk_widget_ref (GTK_WIDGET (container));
  gtk_widget_ref (widget);

  if (!GTK_OBJECT_CONSTRUCTED (widget))
    gtk_object_default_construct (GTK_OBJECT (widget));
  gtk_signal_emit (GTK_OBJECT (container), container_signals[ADD], widget);
  
  if (widget->parent)
    {
      GtkArg *max_args;

      for (max_args = args + n_args; args < max_args; args++)
	gtk_container_arg_set (container, widget, args, NULL);
    }

  gtk_widget_unref (widget);
  gtk_widget_unref (GTK_WIDGET (container));
}

void
gtk_container_child_setv (GtkContainer      *container,
			  GtkWidget         *child,
			  guint              n_args,
			  GtkArg            *args)
{
  GtkArg *max_args;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent != NULL);
  if (n_args)
    g_return_if_fail (args != NULL);

  for (max_args = args + n_args; args < max_args; args++)
    gtk_container_arg_set (container, child, args, NULL);
}

void
gtk_container_child_getv (GtkContainer      *container,
			  GtkWidget         *child,
			  guint              n_args,
			  GtkArg            *args)
{
  GtkArg *max_args;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent != NULL);
  if (n_args)
    g_return_if_fail (args != NULL);

  for (max_args = args + n_args; args < max_args; args++)
    gtk_container_arg_get (container, child, args, NULL);
}

void
gtk_container_child_set (GtkContainer      *container,
			 GtkWidget         *child,
			 const gchar       *first_arg_name,
			 ...)
{
  va_list var_args;
  GSList *arg_list = NULL;
  GSList *info_list = NULL;
  gchar *error;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent != NULL);

  va_start (var_args, first_arg_name);
  error = gtk_container_child_args_collect (GTK_OBJECT_TYPE (container),
					    &arg_list,
					    &info_list,
					    first_arg_name,
					    var_args);
  va_end (var_args);

  if (error)
    {
      g_warning ("gtk_container_child_set(): %s", error);
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
	  gtk_container_arg_set (container, child, slist_arg->data, slist_info->data);
	  slist_arg = slist_arg->next;
	  slist_info = slist_info->next;
	}
      gtk_args_collect_cleanup (arg_list, info_list);
    }
}

void
gtk_container_arg_set (GtkContainer *container,
		       GtkWidget    *child,
		       GtkArg       *arg,
		       GtkArgInfo   *info)
{
  GtkContainerClass *class;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (arg != NULL);
  
  if (!info)
    {
      gchar *error;
      
      error = gtk_arg_get_info (GTK_OBJECT_TYPE (container),
				container_child_arg_info_ht,
				arg->name,
				&info);
      if (error)
	{
	  g_warning ("gtk_container_arg_set(): %s", error);
	  g_free (error);
	  return;
	}
    }
  g_return_if_fail (info->arg_flags & GTK_ARG_CHILD_ARG);
  
  if (! (info->arg_flags & GTK_ARG_WRITABLE))
    {
      g_warning ("gtk_container_arg_set(): argument \"%s\" is not writable",
		 info->full_name);
      return;
    }
  if (info->type != arg->type)
    {
      g_warning ("gtk_container_arg_set(): argument \"%s\" has invalid type `%s'",
		 info->full_name,
		 gtk_type_name (arg->type));
      return;
    }
  
  class = gtk_type_class (info->class_type);
  g_assert (class->set_child_arg != NULL);
  class->set_child_arg (container, child, arg, info->arg_id);
}

void
gtk_container_arg_get (GtkContainer *container,
		       GtkWidget    *child,
		       GtkArg       *arg,
		       GtkArgInfo   *info)
{
  GtkContainerClass *class;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (arg != NULL);
  
  if (!info)
    {
      gchar *error;
      
      error = gtk_arg_get_info (GTK_OBJECT_TYPE (container),
				container_child_arg_info_ht,
				arg->name,
				&info);
      if (error)
	{
	  g_warning ("gtk_container_arg_get(): %s", error);
	  g_free (error);
	  arg->type = GTK_TYPE_INVALID;
	  return;
	}
    }
  g_return_if_fail (info->arg_flags & GTK_ARG_CHILD_ARG);
  
  if (! (info->arg_flags & GTK_ARG_READABLE))
    {
      g_warning ("gtk_container_arg_get(): argument \"%s\" is not readable",
		 info->full_name);
      arg->type = GTK_TYPE_INVALID;
      return;
    }
  
  class = gtk_type_class (info->class_type);
  g_assert (class->get_child_arg != NULL);
  arg->type = info->type;
  class->get_child_arg (container, child, arg, info->arg_id);
}

void
gtk_container_add_child_arg_type (const gchar       *arg_name,
				  GtkType            arg_type,
				  guint              arg_flags,
				  guint              arg_id)
{
  g_return_if_fail (arg_name != NULL);
  g_return_if_fail (arg_type > GTK_TYPE_NONE);
  g_return_if_fail (arg_id > 0);
  g_return_if_fail ((arg_flags & GTK_ARG_READWRITE) == GTK_ARG_READWRITE);
  /* g_return_if_fail ((arg_flags & GTK_ARG_CHILD_ARG) != 0); */

  arg_flags |= GTK_ARG_CHILD_ARG;
  arg_flags &= GTK_ARG_MASK;

  gtk_arg_type_new_static (GTK_TYPE_CONTAINER,
			   arg_name,
			   GTK_STRUCT_OFFSET (GtkContainerClass, n_child_args),
			   container_child_arg_info_ht,
			   arg_type,
			   arg_flags,
			   arg_id);
}

gchar*
gtk_container_child_args_collect (GtkType       object_type,
				  GSList      **arg_list_p,
				  GSList      **info_list_p,
				  const gchar  *first_arg_name,
				  va_list	var_args)
{
  return gtk_args_collect (object_type,
			   container_child_arg_info_ht,
			   arg_list_p,
			   info_list_p,
			   first_arg_name,
			   var_args);
}

gchar*
gtk_container_child_arg_get_info (GtkType       object_type,
				  const gchar  *arg_name,
				  GtkArgInfo  **info_p)
{
  return gtk_arg_get_info (object_type,
			   container_child_arg_info_ht,
			   arg_name,
			   info_p);
}

GtkArg*
gtk_container_query_child_args (GtkType	           class_type,
				guint32          **arg_flags,
				guint             *n_args)
{
  g_return_val_if_fail (n_args != NULL, NULL);
  *n_args = 0;
  g_return_val_if_fail (gtk_type_is_a (class_type, GTK_TYPE_CONTAINER), NULL);

  return gtk_args_query (class_type, container_child_arg_info_ht, arg_flags, n_args);
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
    gtk_container_dequeue_resize_handler (container);
  if (container->resize_widgets)
    gtk_container_clear_resize_widgets (container);
  
  gtk_container_foreach (container, (GtkCallback) gtk_widget_destroy, NULL);
  
  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_container_set_arg (GtkObject    *object,
		       GtkArg       *arg,
		       guint	     arg_id)
{
  GtkContainer *container;

  container = GTK_CONTAINER (object);

  switch (arg_id)
    {
    case ARG_BORDER_WIDTH:
      gtk_container_set_border_width (container, GTK_VALUE_ULONG (*arg));
      break;
    case ARG_RESIZE_MODE:
      gtk_container_set_resize_mode (container, GTK_VALUE_ENUM (*arg));
      break;
    case ARG_REALLOCATE_REDRAWS:
      gtk_container_set_reallocate_redraws (container, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_CHILD:
      gtk_container_add (container, GTK_WIDGET (GTK_VALUE_OBJECT (*arg)));
      break;
    default:
      break;
    }
}

static void
gtk_container_get_arg (GtkObject    *object,
		       GtkArg       *arg,
		       guint	     arg_id)
{
  GtkContainer *container;

  container = GTK_CONTAINER (object);
  
  switch (arg_id)
    {
    case ARG_BORDER_WIDTH:
      GTK_VALUE_ULONG (*arg) = container->border_width;
      break;
    case ARG_RESIZE_MODE:
      GTK_VALUE_ENUM (*arg) = container->resize_mode;
      break;
    case ARG_REALLOCATE_REDRAWS:
      GTK_VALUE_BOOL (*arg) = container->reallocate_redraws;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

void
gtk_container_set_border_width (GtkContainer *container,
				guint         border_width)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

  if (container->border_width != border_width)
    {
      container->border_width = border_width;

      if (GTK_WIDGET_REALIZED (container))
	gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

void
gtk_container_add (GtkContainer *container,
		   GtkWidget    *widget)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->parent == NULL);

  if (!GTK_OBJECT_CONSTRUCTED (widget))
    gtk_object_default_construct (GTK_OBJECT (widget));
  gtk_signal_emit (GTK_OBJECT (container), container_signals[ADD], widget);
}

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
gtk_container_dequeue_resize_handler (GtkContainer *container)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_CONTAINER_RESIZE_PENDING (container));

  container_resize_queue = g_slist_remove (container_resize_queue, container);
  GTK_PRIVATE_UNSET_FLAG (container, GTK_RESIZE_PENDING);
}

void
gtk_container_clear_resize_widgets (GtkContainer *container)
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
    resize_mode = GTK_RESIZE_QUEUE;
  
  if (container->resize_mode != resize_mode)
    {
      container->resize_mode = resize_mode;
      
      if (resize_mode == GTK_RESIZE_IMMEDIATE)
	gtk_container_check_resize (container);
      else
	{
	  gtk_container_clear_resize_widgets (container);
	  gtk_widget_queue_resize (GTK_WIDGET (container));
	}
    }
}

void
gtk_container_set_reallocate_redraws (GtkContainer *container,
				      gboolean      needs_redraws)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));

  needs_redraws = needs_redraws ? TRUE : FALSE;
  if (needs_redraws != container->reallocate_redraws)
    {
      container->reallocate_redraws = needs_redraws;
      if (container->reallocate_redraws)
	gtk_widget_queue_draw (GTK_WIDGET (container));
    }
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

  GDK_THREADS_LEAVE ();
  
  return FALSE;
}

void
gtk_container_queue_resize (GtkContainer *container)
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
    gtk_container_clear_resize_widgets (container);
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
   * a resize in our anchestry. also we can skip the whole
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
	gtk_container_queue_resize (container);
      else
	gtk_widget_size_allocate (GTK_WIDGET (container),
				  &GTK_WIDGET (container)->allocation);
      return;
    }

  resize_container = GTK_WIDGET (container);

  /* we now walk the anchestry for all resize widgets as long
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
   * anchestry to sort those widgets out that have RESIZE_NEEDED parents.
   * we can safely stop the walk if we are the parent, since we checked
   * our own anchestry already.
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

void
gtk_container_forall (GtkContainer *container,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkContainerClass *class;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (callback != NULL);

  class = GTK_CONTAINER_CLASS (GTK_OBJECT (container)->klass);

  if (class->forall)
    class->forall (container, TRUE, callback, callback_data);
}

void
gtk_container_foreach (GtkContainer *container,
		       GtkCallback   callback,
		       gpointer      callback_data)
{
  GtkContainerClass *class;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (callback != NULL);

  class = GTK_CONTAINER_CLASS (GTK_OBJECT (container)->klass);

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
  args[0].type = GTK_OBJECT(child)->klass->type;
  GTK_VALUE_OBJECT(args[0]) = GTK_OBJECT (child);
  
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

gint
gtk_container_focus (GtkContainer     *container,
		     GtkDirectionType  direction)
{
  gint return_val;

  g_return_val_if_fail (container != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CONTAINER (container), FALSE);
  
  gtk_signal_emit (GTK_OBJECT (container),
                   container_signals[FOCUS],
                   direction, &return_val);

  return return_val;
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
gtk_container_children (GtkContainer *container)
{
  GList *children;

  children = NULL;

  gtk_container_foreach (container,
			 gtk_container_children_callback,
			 &children);

  return g_list_reverse (children);
}

void
gtk_container_register_toplevel (GtkContainer *container)
{
  g_return_if_fail (container != NULL);
  
  toplevel_list = g_list_prepend (toplevel_list, container);
  
  gtk_widget_ref (GTK_WIDGET (container));
  gtk_object_sink (GTK_OBJECT (container));
}

void
gtk_container_unregister_toplevel (GtkContainer *container)
{
  GList *node;

  g_return_if_fail (container != NULL);

  node = g_list_find (toplevel_list, container);
  g_return_if_fail (node != NULL);

  toplevel_list = g_list_remove_link (toplevel_list, node);
  g_list_free_1 (node);

  gtk_widget_unref (GTK_WIDGET (container));
}

GList*
gtk_container_get_toplevels (void)
{
  /* XXX: fixme we should ref all these widgets and duplicate
   * the list.
   */
  return toplevel_list;
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
gtk_container_child_composite_name (GtkContainer *container,
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

	  class = GTK_CONTAINER_CLASS (GTK_OBJECT (container)->klass);
	  if (class->composite_name)
	    name = class->composite_name (container, child);
	}
      else
	name = g_strdup (name);

      return name;
    }
  
  return NULL;
}

void
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

static gint
gtk_container_real_focus (GtkContainer     *container,
			  GtkDirectionType  direction)
{
  GList *children;
  GList *tmp_list;
  GList *tmp_list2;
  gint return_val;

  g_return_val_if_fail (container != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CONTAINER (container), FALSE);

  /* Fail if the container is inappropriate for focus movement
   */
  if (!GTK_WIDGET_DRAWABLE (container) ||
      !GTK_WIDGET_IS_SENSITIVE (container))
    return FALSE;

  return_val = FALSE;

  if (GTK_WIDGET_CAN_FOCUS (container))
    {
      gtk_widget_grab_focus (GTK_WIDGET (container));
      return_val = TRUE;
    }
  else
    {
      /* Get a list of the containers children
       */
      children = NULL;
      gtk_container_forall (container,
			    gtk_container_children_callback,
			    &children);
      children = g_list_reverse (children);
      /* children = gtk_container_children (container); */

      if (children)
	{
	  /* Remove any children which are inappropriate for focus movement
	   */
	  tmp_list = children;
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
		  
		  children = g_list_remove_link (children, tmp_list2);
		  g_list_free_1 (tmp_list2);
		}
	    }

	  switch (direction)
	    {
	    case GTK_DIR_TAB_FORWARD:
	    case GTK_DIR_TAB_BACKWARD:
	      return_val = gtk_container_focus_tab (container, children, direction);
	      break;
	    case GTK_DIR_UP:
	    case GTK_DIR_DOWN:
	      return_val = gtk_container_focus_up_down (container, children, direction);
	      break;
	    case GTK_DIR_LEFT:
	    case GTK_DIR_RIGHT:
	      return_val = gtk_container_focus_left_right (container, children, direction);
	      break;
	    }

	  g_list_free (children);
	}
    }

  return return_val;
}

static gint
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

static gint
gtk_container_focus_up_down (GtkContainer     *container,
			     GList            *children,
			     GtkDirectionType  direction)
{
  GtkWidget *child;
  GtkWidget *child2;
  GList *tmp_list;
  gint dist1, dist2;
  gint focus_x;
  gint focus_width;
  guint length;
  guint i, j;

  /* return failure if there isn't a focus child */
  if (container->focus_child)
    {
      focus_width = container->focus_child->allocation.width / 2;
      focus_x = container->focus_child->allocation.x + focus_width;
    }
  else
    {
      focus_width = GTK_WIDGET (container)->allocation.width;
      if (GTK_WIDGET_NO_WINDOW (container))
	focus_x = GTK_WIDGET (container)->allocation.x;
      else
	focus_x = 0;
    }

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

  /* sort the children in distance in the x direction
   *  in distance from the current focus child while maintaining the
   *  sort in the y direction
   */
  for (i = 1; i < length; i++)
    {
      j = i;
      tmp_list = g_list_nth (children, j);
      child = tmp_list->data;
      dist1 = (child->allocation.x + child->allocation.width / 2) - focus_x;

      while (j > 0)
	{
	  child2 = tmp_list->prev->data;
	  dist2 = (child2->allocation.x + child2->allocation.width / 2) - focus_x;

	  if ((dist1 < dist2) &&
	      (child->allocation.y >= child2->allocation.y))
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

  /* go and invalidate any widget which is too
   *  far from the focus widget.
   */
  if (!container->focus_child &&
      (direction == GTK_DIR_UP))
    focus_x += focus_width;

  tmp_list = children;
  while (tmp_list)
    {
      child = tmp_list->data;

      dist1 = (child->allocation.x + child->allocation.width / 2) - focus_x;
      if (((direction == GTK_DIR_DOWN) && (dist1 < 0)) ||
	  ((direction == GTK_DIR_UP) && (dist1 > 0)))
	tmp_list->data = NULL;

      tmp_list = tmp_list->next;
    }

  if (direction == GTK_DIR_UP)
    children = g_list_reverse (children);

  return gtk_container_focus_move (container, children, direction);
}

static gint
gtk_container_focus_left_right (GtkContainer     *container,
				GList            *children,
				GtkDirectionType  direction)
{
  GtkWidget *child;
  GtkWidget *child2;
  GList *tmp_list;
  gint dist1, dist2;
  gint focus_y;
  gint focus_height;
  guint length;
  guint i, j;

  /* return failure if there isn't a focus child */
  if (container->focus_child)
    {
      focus_height = container->focus_child->allocation.height / 2;
      focus_y = container->focus_child->allocation.y + focus_height;
    }
  else
    {
      focus_height = GTK_WIDGET (container)->allocation.height;
      if (GTK_WIDGET_NO_WINDOW (container))
	focus_y = GTK_WIDGET (container)->allocation.y;
      else
	focus_y = 0;
    }

  length = g_list_length (children);

  /* sort the children in the x direction */
  for (i = 1; i < length; i++)
    {
      j = i;
      tmp_list = g_list_nth (children, j);
      child = tmp_list->data;

      while (j > 0)
	{
	  child2 = tmp_list->prev->data;
	  if (child->allocation.x < child2->allocation.x)
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

  /* sort the children in distance in the y direction
   *  in distance from the current focus child while maintaining the
   *  sort in the x direction
   */
  for (i = 1; i < length; i++)
    {
      j = i;
      tmp_list = g_list_nth (children, j);
      child = tmp_list->data;
      dist1 = (child->allocation.y + child->allocation.height / 2) - focus_y;

      while (j > 0)
	{
	  child2 = tmp_list->prev->data;
	  dist2 = (child2->allocation.y + child2->allocation.height / 2) - focus_y;

	  if ((dist1 < dist2) &&
	      (child->allocation.x >= child2->allocation.x))
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

  /* go and invalidate any widget which is too
   *  far from the focus widget.
   */
  if (!container->focus_child &&
      (direction == GTK_DIR_LEFT))
    focus_y += focus_height;

  tmp_list = children;
  while (tmp_list)
    {
      child = tmp_list->data;

      dist1 = (child->allocation.y + child->allocation.height / 2) - focus_y;
      if (((direction == GTK_DIR_RIGHT) && (dist1 < 0)) ||
	  ((direction == GTK_DIR_LEFT) && (dist1 > 0)))
	tmp_list->data = NULL;

      tmp_list = tmp_list->next;
    }

  if (direction == GTK_DIR_LEFT)
    children = g_list_reverse (children);

  return gtk_container_focus_move (container, children, direction);
}

static gint
gtk_container_focus_move (GtkContainer     *container,
			  GList            *children,
			  GtkDirectionType  direction)
{
  GtkWidget *focus_child;
  GtkWidget *child;

  focus_child = container->focus_child;
  gtk_container_set_focus_child (container, NULL);

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

              if (GTK_WIDGET_DRAWABLE (child) &&
		  GTK_IS_CONTAINER (child) &&
		  !GTK_WIDGET_HAS_FOCUS (child))
		if (gtk_container_focus (GTK_CONTAINER (child), direction))
		  return TRUE;
            }
        }
      else if (GTK_WIDGET_DRAWABLE (child))
        {
	  if (GTK_IS_CONTAINER (child))
            {
              if (gtk_container_focus (GTK_CONTAINER (child), direction))
                return TRUE;
            }
          else if (GTK_WIDGET_CAN_FOCUS (child))
            {
              gtk_widget_grab_focus (child);
              return TRUE;
            }
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
