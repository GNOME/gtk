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
#include <string.h>
#include "gtkcontainer.h"
#include "gtkprivate.h"
#include "gtksignal.h"


enum {
  ADD,
  REMOVE,
  NEED_RESIZE,
  FOREACH,
  FOCUS,
  LAST_SIGNAL
};
enum {
  ARG_0,
  ARG_BORDER_WIDTH,
  ARG_AUTO_RESIZE,
  ARG_BLOCK_RESIZE,
  ARG_CHILD
};


typedef void (*GtkContainerSignal1) (GtkObject *object,
				     gpointer   arg1,
				     gpointer   data);
typedef void (*GtkContainerSignal2) (GtkObject *object,
				     gpointer   arg1,
				     gpointer   arg2,
				     gpointer   data);
typedef gint (*GtkContainerSignal3) (GtkObject *object,
				     gint       arg1,
				     gpointer   data);
typedef gint (*GtkContainerSignal4) (GtkObject *object,
				     gpointer   data);


static void gtk_container_marshal_signal_1 (GtkObject      *object,
					    GtkSignalFunc   func,
					    gpointer        func_data,
					    GtkArg         *args);
static void gtk_container_marshal_signal_2 (GtkObject      *object,
					    GtkSignalFunc   func,
					    gpointer        func_data,
					    GtkArg         *args);
static void gtk_container_marshal_signal_3 (GtkObject      *object,
					    GtkSignalFunc   func,
					    gpointer        func_data,
					    GtkArg         *args);
static void gtk_container_marshal_signal_4 (GtkObject      *object,
					    GtkSignalFunc   func,
					    gpointer        func_data,
					    GtkArg         *args);


static void gtk_container_class_init        (GtkContainerClass *klass);
static void gtk_container_init              (GtkContainer      *container);
static void gtk_container_destroy           (GtkObject         *object);
static void gtk_container_get_arg           (GtkContainer      *container,
					     GtkArg            *arg,
					     guint		arg_id);
static void gtk_container_set_arg           (GtkContainer      *container,
					     GtkArg            *arg,
					     guint		arg_id);
static gint gtk_real_container_need_resize  (GtkContainer      *container);
static gint gtk_real_container_focus        (GtkContainer      *container,
					     GtkDirectionType   direction);
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



static guint container_signals[LAST_SIGNAL] = { 0 };

static GtkWidgetClass *parent_class = NULL;

guint
gtk_container_get_type ()
{
  static guint container_type = 0;

  if (!container_type)
    {
      GtkTypeInfo container_info =
      {
	"GtkContainer",
	sizeof (GtkContainer),
	sizeof (GtkContainerClass),
	(GtkClassInitFunc) gtk_container_class_init,
	(GtkObjectInitFunc) gtk_container_init,
	(GtkArgSetFunc) gtk_container_set_arg,
	(GtkArgGetFunc) gtk_container_get_arg,
      };

      container_type = gtk_type_unique (gtk_widget_get_type (), &container_info);
    }

  return container_type;
}

static void
gtk_container_class_init (GtkContainerClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  parent_class = gtk_type_class (gtk_widget_get_type ());
  
  gtk_object_add_arg_type ("GtkContainer::border_width", GTK_TYPE_LONG, GTK_ARG_READWRITE, ARG_BORDER_WIDTH);
  gtk_object_add_arg_type ("GtkContainer::auto_resize", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_AUTO_RESIZE);
  gtk_object_add_arg_type ("GtkContainer::block_resize", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_BLOCK_RESIZE);
  gtk_object_add_arg_type ("GtkContainer::child", GTK_TYPE_WIDGET, GTK_ARG_WRITABLE, ARG_CHILD);

  container_signals[ADD] =
    gtk_signal_new ("add",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkContainerClass, add),
                    gtk_container_marshal_signal_1,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_WIDGET);
  container_signals[REMOVE] =
    gtk_signal_new ("remove",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkContainerClass, remove),
                    gtk_container_marshal_signal_1,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_WIDGET);
  container_signals[NEED_RESIZE] =
    gtk_signal_new ("need_resize",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkContainerClass, need_resize),
                    gtk_container_marshal_signal_4,
		    GTK_TYPE_BOOL, 0);
  container_signals[FOREACH] =
    gtk_signal_new ("foreach",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkContainerClass, foreach),
                    gtk_container_marshal_signal_2,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_C_CALLBACK);
  container_signals[FOCUS] =
    gtk_signal_new ("focus",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkContainerClass, focus),
                    gtk_container_marshal_signal_3,
		    GTK_TYPE_DIRECTION_TYPE, 1,
                    GTK_TYPE_DIRECTION_TYPE);

  gtk_object_class_add_signals (object_class, container_signals, LAST_SIGNAL);
  
  object_class->destroy = gtk_container_destroy;
  
  /* Other container classes should overwrite show_all and hide_all,
   * for the purpose of showing internal children also, which are not
   * accessable through gtk_container_foreach.
  */
  widget_class->show_all = gtk_container_show_all;
  widget_class->hide_all = gtk_container_hide_all;  
  
  class->need_resize = gtk_real_container_need_resize;
  class->focus = gtk_real_container_focus;
}

static void
gtk_container_init (GtkContainer *container)
{
  container->focus_child = NULL;
  container->border_width = 0;
  container->auto_resize = TRUE;
  container->need_resize = FALSE;
  container->block_resize = FALSE;
  container->resize_widgets = NULL;
}

static void
gtk_container_destroy (GtkObject *object)
{
  GSList *node;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (object));

  for (node = GTK_CONTAINER (object)->resize_widgets; node; node = node->next)
    {
      GtkWidget *child;
      
      child = (GtkWidget*) node->data;
      GTK_PRIVATE_UNSET_FLAG (child, GTK_RESIZE_NEEDED);
    }
  g_slist_free (GTK_CONTAINER (object)->resize_widgets);
  GTK_CONTAINER (object)->resize_widgets = NULL;
  
  gtk_container_foreach (GTK_CONTAINER (object),
			 (GtkCallback) gtk_widget_destroy, NULL);
  
  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_container_set_arg (GtkContainer *container,
		       GtkArg       *arg,
		       guint	     arg_id)
{
  switch (arg_id)
    {
    case ARG_BORDER_WIDTH:
      gtk_container_border_width (container, GTK_VALUE_LONG (*arg));
      break;
    case ARG_AUTO_RESIZE:
      if (GTK_VALUE_BOOL (*arg))
	gtk_container_enable_resize (container);
      else
	gtk_container_disable_resize (container);
      break;
    case ARG_BLOCK_RESIZE:
      if (GTK_VALUE_BOOL (*arg))
	gtk_container_block_resize (container);
      else
	gtk_container_unblock_resize (container);
      break;
    case ARG_CHILD:
      gtk_container_add (container, GTK_WIDGET (GTK_VALUE_OBJECT (*arg)));
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_container_get_arg (GtkContainer *container,
		       GtkArg       *arg,
		       guint	     arg_id)
{
  switch (arg_id)
    {
    case ARG_BORDER_WIDTH:
      GTK_VALUE_LONG (*arg) = container->border_width;
      break;
    case ARG_AUTO_RESIZE:
      GTK_VALUE_BOOL (*arg) = container->auto_resize;
      break;
    case ARG_BLOCK_RESIZE:
      GTK_VALUE_BOOL (*arg) = container->block_resize;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

void
gtk_container_border_width (GtkContainer *container,
			    gint          border_width)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

  if (container->border_width != border_width)
    {
      container->border_width = border_width;

      if (container->widget.parent && GTK_WIDGET_VISIBLE (container))
	gtk_container_need_resize (GTK_CONTAINER (container->widget.parent));
    }
}

void
gtk_container_add (GtkContainer *container,
		   GtkWidget    *widget)
{
  gtk_signal_emit (GTK_OBJECT (container), container_signals[ADD], widget);
}

void
gtk_container_remove (GtkContainer *container,
		      GtkWidget    *widget)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

  if (container->focus_child == widget)
    container->focus_child = NULL;

  gtk_signal_emit (GTK_OBJECT (container), container_signals[REMOVE], widget);
}

void
gtk_container_disable_resize (GtkContainer *container)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

  container->auto_resize = FALSE;
}

void
gtk_container_enable_resize (GtkContainer *container)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

  container->auto_resize = TRUE;
  if (container->need_resize)
    {
      container->need_resize = FALSE;
      gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

void
gtk_container_block_resize (GtkContainer *container)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

  container->block_resize = TRUE;
}

void
gtk_container_unblock_resize (GtkContainer *container)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  
  container->block_resize = FALSE;
}

gint
gtk_container_need_resize (GtkContainer *container)
{
  gint return_val;
  
  g_return_val_if_fail (container != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CONTAINER (container), FALSE);
  
  return_val = FALSE;
  
  if (!container->block_resize)
    {
      if (container->auto_resize)
	gtk_signal_emit (GTK_OBJECT (container),
                         container_signals[NEED_RESIZE],
                         &return_val);
      else
	container->need_resize = TRUE;
    }
  
  return return_val;
}

void
gtk_container_foreach (GtkContainer *container,
		       GtkCallback   callback,
		       gpointer      callback_data)
{
  gtk_signal_emit (GTK_OBJECT (container),
                   container_signals[FOREACH],
                   callback, callback_data);
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
gtk_container_foreach_interp (GtkContainer       *container,
			      GtkCallbackMarshal  marshal,
			      gpointer            callback_data,
			      GtkDestroyNotify    notify)
{
  gtk_container_foreach_full (container, NULL, marshal, 
			      callback_data, notify);
}

void
gtk_container_foreach_full (GtkContainer       *container,
			    GtkCallback         callback,
			    GtkCallbackMarshal  marshal,
			    gpointer            callback_data,
			    GtkDestroyNotify    notify)
{
  if (marshal)
    {
      GtkForeachData fdata;
  
      fdata.container     = GTK_OBJECT (container);
      fdata.callback      = marshal;
      fdata.callback_data = callback_data;

      gtk_container_foreach (container, gtk_container_foreach_unmarshal, &fdata);
    }
  else
    gtk_container_foreach (container, callback, &callback_data);
  
  notify (callback_data);
}

gint
gtk_container_focus (GtkContainer     *container,
		     GtkDirectionType  direction)
{
  gint return_val;
  
  gtk_signal_emit (GTK_OBJECT (container),
                   container_signals[FOCUS],
                   direction, &return_val);

  return return_val;
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
  gtk_widget_ref (GTK_WIDGET (container));
  gtk_object_sink (GTK_OBJECT (container));
}

void
gtk_container_unregister_toplevel (GtkContainer *container)
{
  gtk_widget_unref (GTK_WIDGET (container));
}

static void
gtk_container_marshal_signal_1 (GtkObject      *object,
				GtkSignalFunc   func,
				gpointer        func_data,
				GtkArg         *args)
{
  GtkContainerSignal1 rfunc;

  rfunc = (GtkContainerSignal1) func;

  (* rfunc) (object, GTK_VALUE_OBJECT (args[0]), func_data);
}

static void
gtk_container_marshal_signal_2 (GtkObject      *object,
				GtkSignalFunc   func,
				gpointer        func_data,
				GtkArg         *args)
{
  GtkContainerSignal2 rfunc;

  rfunc = (GtkContainerSignal2) func;

  (* rfunc) (object,
	     GTK_VALUE_C_CALLBACK(args[0]).func,
	     GTK_VALUE_C_CALLBACK(args[0]).func_data,
	     func_data);
}

static void
gtk_container_marshal_signal_3 (GtkObject      *object,
				GtkSignalFunc   func,
				gpointer        func_data,
				GtkArg         *args)
{
  GtkContainerSignal3 rfunc;
  gint *return_val;

  rfunc = (GtkContainerSignal3) func;
  return_val = GTK_RETLOC_ENUM (args[1]);

  *return_val = (* rfunc) (object, GTK_VALUE_ENUM(args[0]), func_data);
}

static void
gtk_container_marshal_signal_4 (GtkObject      *object,
				GtkSignalFunc   func,
				gpointer        func_data,
				GtkArg         *args)
{
  GtkContainerSignal4 rfunc;
  gint *return_val;

  rfunc = (GtkContainerSignal4) func;
  return_val = GTK_RETLOC_BOOL (args[0]);

  *return_val = (* rfunc) (object, func_data);
}

static gint
gtk_real_container_need_resize (GtkContainer *container)
{
  g_return_val_if_fail (container != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CONTAINER (container), FALSE);

  if (GTK_WIDGET_VISIBLE (container) && container->widget.parent)
    return gtk_container_need_resize (GTK_CONTAINER (container->widget.parent));

  return FALSE;
}

static gint
gtk_real_container_focus (GtkContainer     *container,
			  GtkDirectionType  direction)
{
  GList *children;
  GList *tmp_list;
  GList *tmp_list2;
  gint return_val;

  g_return_val_if_fail (container != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CONTAINER (container), FALSE);

  /* Fail if the container is insensitive
   */
  if (!GTK_WIDGET_SENSITIVE (container))
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
      children = gtk_container_children (container);

      if (children)
	{
	  /* Remove any children which are insensitive
	   */
	  tmp_list = children;
	  while (tmp_list)
	    {
	      if (!GTK_WIDGET_SENSITIVE (tmp_list->data))
		{
		  tmp_list2 = tmp_list;
		  tmp_list = tmp_list->next;

		  children = g_list_remove_link (children, tmp_list2);
		  g_list_free_1 (tmp_list2);
		}
	      else
		tmp_list = tmp_list->next;
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
  container->focus_child = NULL;

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

              if (GTK_WIDGET_VISIBLE (child) &&
		  GTK_IS_CONTAINER (child) &&
		  !GTK_WIDGET_HAS_FOCUS (child))
		if (gtk_container_focus (GTK_CONTAINER (child), direction))
		  return TRUE;
            }
        }
      else if (GTK_WIDGET_VISIBLE (child))
        {
          if (GTK_WIDGET_CAN_FOCUS (child))
            {
              gtk_widget_grab_focus (child);
              return TRUE;
            }
          else if (GTK_IS_CONTAINER (child))
            {
              if (gtk_container_focus (GTK_CONTAINER (child), direction))
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

static void
gtk_container_show_all (GtkWidget *widget)
{
  GtkContainer *container;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (widget));
  container = GTK_CONTAINER (widget);

  /* First show children, then self.
     This makes sure that toplevel windows get shown as last widget.
     Otherwise the user would see the widgets get
     visible one after another.
  */
  gtk_container_foreach (container, (GtkCallback) gtk_widget_show_all, NULL);
  gtk_widget_show (widget);
}


static void
gtk_container_hide_all (GtkWidget *widget)
{
  GtkContainer *container;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (widget));
  container = GTK_CONTAINER (widget);

  /* First hide self, then children.
     This is the reverse order of gtk_container_show_all.
  */
  gtk_widget_hide (widget);  
  gtk_container_foreach (container, (GtkCallback) gtk_widget_hide_all, NULL);
}

