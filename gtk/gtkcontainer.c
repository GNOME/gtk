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
#include <string.h>
#include "gtkcontainer.h"
#include "gtkprivate.h"
#include "gtksignal.h"
#include <stdarg.h>


enum {
  ADD,
  REMOVE,
  CHECK_RESIZE,
  FOREACH,
  FOCUS,
  SET_FOCUS_CHILD,
  LAST_SIGNAL
};
enum {
  ARG_0,
  ARG_BORDER_WIDTH,
  ARG_RESIZE_MODE,
  ARG_CHILD
};

typedef struct _GtkLArgInfo	GtkLArgInfo;
struct _GtkLArgInfo
{
  gchar *name;
  GtkType type;
  GtkType class_type;
  guint arg_flags;
  guint arg_id;
  guint seq_id;
};


typedef void (*GtkContainerSignal1) (GtkObject *object,
				     gpointer   arg1,
				     gpointer   data);
typedef void (*GtkContainerSignal2) (GtkObject *object,
				     GtkFunction arg1,
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


static void gtk_container_class_init        (GtkContainerClass *klass);
static void gtk_container_init              (GtkContainer      *container);
static void gtk_container_destroy           (GtkObject         *object);
static void gtk_container_get_arg           (GtkContainer      *container,
					     GtkArg            *arg,
					     guint		arg_id);
static void gtk_container_set_arg           (GtkContainer      *container,
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

GtkArg*     gtk_object_collect_args	    (guint   *n_args,
					     va_list  args1,
					     va_list  args2);



static guint container_signals[LAST_SIGNAL] = { 0 };
static GHashTable *arg_info_ht = NULL;

static GtkWidgetClass *parent_class = NULL;

static const gchar *vadjustment_key = "gtk-vadjustment";
static guint        vadjustment_key_id = 0;
static const gchar *hadjustment_key = "gtk-hadjustment";
static guint        hadjustment_key_id = 0;

GtkType
gtk_container_get_type (void)
{
  static GtkType container_type = 0;

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

  vadjustment_key_id = g_quark_from_static_string (vadjustment_key);
  hadjustment_key_id = g_quark_from_static_string (hadjustment_key);
  
  gtk_object_add_arg_type ("GtkContainer::border_width", GTK_TYPE_ULONG, GTK_ARG_READWRITE, ARG_BORDER_WIDTH);
  gtk_object_add_arg_type ("GtkContainer::resize_mode", GTK_TYPE_RESIZE_MODE, GTK_ARG_READWRITE, ARG_RESIZE_MODE);
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
  container_signals[CHECK_RESIZE] =
    gtk_signal_new ("check_resize",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkContainerClass, check_resize),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
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
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkContainerClass, focus),
                    gtk_container_marshal_signal_3,
		    GTK_TYPE_DIRECTION_TYPE, 1,
                    GTK_TYPE_DIRECTION_TYPE);
  container_signals[SET_FOCUS_CHILD] =
    gtk_signal_new ("set-focus-child",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkContainerClass, set_focus_child),
                    gtk_container_marshal_signal_1,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_WIDGET);
  gtk_object_class_add_signals (object_class, container_signals, LAST_SIGNAL);
  
  object_class->destroy = gtk_container_destroy;
  
  /* Other container classes should overwrite show_all and hide_all,
   * for the purpose of showing internal children also, which are not
   * accessable through gtk_container_foreach.
  */
  widget_class->show_all = gtk_container_show_all;
  widget_class->hide_all = gtk_container_hide_all;

  class->add = gtk_container_add_unimplemented;
  class->remove = gtk_container_remove_unimplemented;
  class->check_resize = gtk_container_real_check_resize;
  class->foreach = NULL;
  class->focus = gtk_container_real_focus;
  class->set_focus_child = gtk_container_real_set_focus_child;

  /* linkage */
  class->child_type = NULL;
  class->get_child_arg = NULL;
  class->set_child_arg = NULL;
}

static void
gtk_container_get_child_arg (GtkContainer *container,
			     GtkWidget    *child,
			     GtkType       type,
			     GtkArg       *arg,
			     guint         arg_id)
{
  GtkContainerClass *class;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));

  class = GTK_CONTAINER_CLASS (GTK_OBJECT (container)->klass);

  if (class->get_child_arg)
    class->get_child_arg (container, child, arg, arg_id);
  else
    arg->type = GTK_TYPE_INVALID;
}

static void
gtk_container_set_child_arg (GtkContainer *container,
			     GtkWidget    *child,
			     GtkType       type,
			     GtkArg       *arg,
			     guint         arg_id)
{
  GtkContainerClass *class;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));

  class = GTK_CONTAINER_CLASS (GTK_OBJECT (container)->klass);

  if (class->set_child_arg)
    class->set_child_arg (container, child, arg, arg_id);
}

GtkType
gtk_container_child_type (GtkContainer      *container)
{
  GtkType slot;
  GtkContainerClass *class;

  g_return_val_if_fail (container != NULL, 0);
  g_return_val_if_fail (GTK_IS_CONTAINER (container), 0);

  slot = GTK_TYPE_NONE;
  class = GTK_CONTAINER_CLASS (GTK_OBJECT (container)->klass);
  if (class->child_type)
    slot = class->child_type (container);

  return slot;
}

void
gtk_container_add_child_arg_type (const gchar       *arg_name,
				  GtkType            arg_type,
				  guint              arg_flags,
				  guint              arg_id)
{
  GtkLArgInfo *info;
  gchar class_part[1024];
  gchar *arg_part;
  GtkType class_type;

  g_return_if_fail (arg_name != NULL);
  g_return_if_fail (arg_type > GTK_TYPE_NONE);
  g_return_if_fail (arg_id > 0);
  g_return_if_fail ((arg_flags & GTK_ARG_READWRITE) == GTK_ARG_READWRITE);

  arg_flags |= GTK_ARG_CHILD_ARG;
  arg_flags &= GTK_ARG_MASK;

  arg_part = strchr (arg_name, ':');
  if (!arg_part || (arg_part[0] != ':') || (arg_part[1] != ':'))
    {
      g_warning ("gtk_container_add_arg_type(): invalid arg name: \"%s\"\n", arg_name);
      return;
    }

  strncpy (class_part, arg_name, (glong) (arg_part - arg_name));
  class_part[(glong) (arg_part - arg_name)] = '\0';

  class_type = gtk_type_from_name (class_part);
  if (!class_type && !gtk_type_is_a (class_type, GTK_TYPE_CONTAINER))
    {
      g_warning ("gtk_container_add_arg_type(): invalid class name in arg: \"%s\"\n", arg_name);
      return;
    }

  info = g_new (GtkLArgInfo, 1);
  info->name = g_strdup (arg_name);
  info->type = arg_type;
  info->class_type = class_type;
  info->arg_flags = arg_flags;
  info->arg_id = arg_id;
  info->seq_id = ++((GtkContainerClass*) gtk_type_class (class_type))->n_child_args;

  if (!arg_info_ht)
    arg_info_ht = g_hash_table_new (g_str_hash, g_str_equal);

  g_hash_table_insert (arg_info_ht, info->name, info);
}

typedef struct
{
  GList *arg_list;
  GtkType class_type;
} GtkQueryLArgData;

static void
gtk_query_larg_foreach (gpointer key,
			gpointer value,
			gpointer user_data)
{
  register GtkLArgInfo *info;
  register GtkQueryLArgData *data;

  info = value;
  data = user_data;

  if (info->class_type == data->class_type)
    data->arg_list = g_list_prepend (data->arg_list, info);
}

GtkArg*
gtk_container_query_child_args (GtkType	           class_type,
				guint32          **arg_flags,
				guint             *n_args)
{
  GtkArg *args;
  GtkQueryLArgData query_data;

  if (arg_flags)
    *arg_flags = NULL;
  g_return_val_if_fail (n_args != NULL, NULL);
  *n_args = 0;
  g_return_val_if_fail (gtk_type_is_a (class_type, GTK_TYPE_CONTAINER), NULL);

  if (!arg_info_ht)
    return NULL;

  /* make sure the types class has been initialized, because
   * the argument setup happens in the gtk_*_class_init() functions.
   */
  gtk_type_class (class_type);

  query_data.arg_list = NULL;
  query_data.class_type = class_type;
  g_hash_table_foreach (arg_info_ht, gtk_query_larg_foreach, &query_data);

  if (query_data.arg_list)
    {
      register GList    *list;
      register guint    len;

      list = query_data.arg_list;
      len = 1;
      while (list->next)
	{
	  len++;
	  list = list->next;
	}
      g_assert (len == ((GtkContainerClass*) gtk_type_class (class_type))->n_child_args); /* paranoid */

      args = g_new0 (GtkArg, len);
      *n_args = len;
      if (arg_flags)
	*arg_flags = g_new (guint32, len);

      do
	{
	  GtkLArgInfo *info;

	  info = list->data;
	  list = list->prev;

	  g_assert (info->seq_id > 0 && info->seq_id <= len); /* paranoid */

	  args[info->seq_id - 1].type = info->type;
	  args[info->seq_id - 1].name = info->name;
	  if (arg_flags)
	    (*arg_flags)[info->seq_id - 1] = info->arg_flags;
	}
      while (list);

      g_list_free (query_data.arg_list);
    }
  else
    args = NULL;

  return args;
}

void
gtk_container_child_arg_getv (GtkContainer      *container,
			      GtkWidget         *child,
			      guint              n_args,
			      GtkArg            *args)
{
  guint i;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (!arg_info_ht)
    return;

  for (i = 0; i < n_args; i++)
    {
      GtkLArgInfo *info;
      gchar *lookup_name;
      gchar *d;


      /* hm, the name cutting shouldn't be needed on gets, but what the heck...
       */
      lookup_name = g_strdup (args[i].name);
      d = strchr (lookup_name, ':');
      if (d && d[1] == ':')
	{
	  d = strchr (d + 2, ':');
	  if (d)
	    *d = 0;

	  info = g_hash_table_lookup (arg_info_ht, lookup_name);
	}
      else
	info = NULL;

      if (!info)
	{
	  g_warning ("gtk_container_child_arg_getv(): invalid arg name: \"%s\"\n",
		     lookup_name);
	  args[i].type = GTK_TYPE_INVALID;
	  g_free (lookup_name);
	  continue;
	}
      else if (!gtk_type_is_a (GTK_OBJECT_TYPE (container), info->class_type))
	{
	  g_warning ("gtk_container_child_arg_getv(): invalid arg for %s: \"%s\"\n",
		     gtk_type_name (GTK_OBJECT_TYPE (container)), lookup_name);
	  args[i].type = GTK_TYPE_INVALID;
	  g_free (lookup_name);
	  continue;
	}
      else if (! (info->arg_flags & GTK_ARG_READABLE))
	{
	  g_warning ("gtk_container_child_arg_getv(): arg is not supplied for read-access: \"%s\"\n",
		     lookup_name);
	  args[i].type = GTK_TYPE_INVALID;
	  g_free (lookup_name);
	  continue;
	}
      else
	g_free (lookup_name);

      args[i].type = info->type;
      gtk_container_get_child_arg (container, child, info->class_type, &args[i], info->arg_id);
    }
}

void
gtk_container_child_arg_setv (GtkContainer      *container,
			      GtkWidget         *child,
			      guint              n_args,
			      GtkArg            *args)
{
  guint i;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (!arg_info_ht)
    return;

  for (i = 0; i < n_args; i++)
    {
      GtkLArgInfo *info;
      gchar *lookup_name;
      gchar *d;
      gboolean arg_ok;

      lookup_name = g_strdup (args[i].name);
      d = strchr (lookup_name, ':');
      if (d && d[1] == ':')
	{
	  d = strchr (d + 2, ':');
	  if (d)
	    *d = 0;

	  info = g_hash_table_lookup (arg_info_ht, lookup_name);
	}
      else
	info = NULL;

      arg_ok = TRUE;

      if (!info)
	{
	  g_warning ("gtk_container_child_arg_setv(): invalid arg name: \"%s\"\n",
		     lookup_name);
	  arg_ok = FALSE;
	}
      else if (info->type != args[i].type)
	{
	  g_warning ("gtk_container_child_arg_setv(): invalid arg type for: \"%s\"\n",
		     lookup_name);
	  arg_ok = FALSE;
	}
      else if (!gtk_type_is_a (GTK_OBJECT_TYPE (container), info->class_type))
	{
	  g_warning ("gtk_container_child_arg_setv(): invalid arg for %s: \"%s\"\n",
		     gtk_type_name (GTK_OBJECT_TYPE (container)), lookup_name);
	  arg_ok = FALSE;
	}
      else if (! (info->arg_flags & GTK_ARG_WRITABLE))
	{
	  g_warning ("gtk_container_child_arg_setv(): arg is not supplied for write-access: \"%s\"\n",
		     lookup_name);
	  arg_ok = FALSE;
	}

      g_free (lookup_name);

      if (!arg_ok)
	continue;

      gtk_container_set_child_arg (container, child, info->class_type, &args[i], info->arg_id);
    }
}

void
gtk_container_add_with_args (GtkContainer      *container,
			     GtkWidget         *widget,
			     ...)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->parent == NULL);

  gtk_widget_ref (GTK_WIDGET (container));
  gtk_widget_ref (widget);

  gtk_signal_emit (GTK_OBJECT (container), container_signals[ADD], widget);
  
  if (widget->parent)
    {
      GtkArg *args;
      guint n_args;
      va_list args1;
      va_list args2;

      va_start (args1, widget);
      va_start (args2, widget);

      args = gtk_object_collect_args (&n_args, args1, args2);
      gtk_container_child_arg_setv (container, widget, n_args, args);
      g_free (args);

      va_end (args1);
      va_end (args2);
    }

  gtk_widget_unref (widget);
  gtk_widget_unref (GTK_WIDGET (container));
}

void
gtk_container_add_with_argv (GtkContainer      *container,
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

  gtk_signal_emit (GTK_OBJECT (container), container_signals[ADD], widget);

  if (widget->parent)
    gtk_container_child_arg_setv (container, widget, n_args, args);

  gtk_widget_unref (widget);
  gtk_widget_unref (GTK_WIDGET (container));
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
  container->resize_widgets = NULL;
}

static void
gtk_container_destroy (GtkObject *object)
{
  GtkContainer *container;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (object));

  container = GTK_CONTAINER (object);
  
  gtk_container_clear_resize_widgets (container);
  
  gtk_container_foreach (container,
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
      gtk_container_border_width (container, GTK_VALUE_ULONG (*arg));
      break;
    case ARG_RESIZE_MODE:
      gtk_container_set_resize_mode (container, GTK_VALUE_ENUM (*arg));
      break;
    case ARG_CHILD:
      gtk_container_add (container, GTK_WIDGET (GTK_VALUE_OBJECT (*arg)));
      break;
    default:
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
      GTK_VALUE_ULONG (*arg) = container->border_width;
      break;
    case ARG_RESIZE_MODE:
      GTK_VALUE_ENUM (*arg) = container->resize_mode;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

void
gtk_container_border_width (GtkContainer *container,
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
gtk_container_disable_resize (GtkContainer *container)
{
  g_warning ("gtk_container_disable_resize does nothing!");
}

void
gtk_container_enable_resize (GtkContainer *container)
{
  g_warning ("gtk_container_enable_resize does nothing!");
}

void
gtk_container_block_resize (GtkContainer *container)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

}

void
gtk_container_unblock_resize (GtkContainer *container)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  
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
      
      if (container->resize_widgets != NULL)
	{
	  if (resize_mode == GTK_RESIZE_IMMEDIATE)
	    gtk_container_check_resize (container);
	  else if (resize_mode == GTK_RESIZE_PARENT)
	    {
	      gtk_container_clear_resize_widgets (container);
	      gtk_widget_queue_resize (GTK_WIDGET (container));
	    }
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

gint    
gtk_container_need_resize (GtkContainer     *container)
{
  gtk_container_check_resize (container);
  return FALSE;
}

static void
gtk_container_real_check_resize (GtkContainer *container)
{
  GtkWidget *widget;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

  widget = GTK_WIDGET (container);

  gtk_widget_size_request (widget, &widget->requisition);
  
  if ((widget->requisition.width > widget->allocation.width) ||
      (widget->requisition.height > widget->allocation.height))
    {
      gtk_container_clear_resize_widgets (container);
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
  GSList *resize_widgets;
  GSList *resize_containers;
  GSList *node;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));

  resize_widgets = container->resize_widgets;
  container->resize_widgets = NULL;
  
  for (node = resize_widgets; node; node = node->next)
    {
      widget = node->data;
      
      GTK_PRIVATE_UNSET_FLAG (widget, GTK_RESIZE_NEEDED);
      
      while (widget && widget->parent &&
	     ((widget->allocation.width < widget->requisition.width) ||
	      (widget->allocation.height < widget->requisition.height)))
	widget = widget->parent;
      
      GTK_PRIVATE_SET_FLAG (widget, GTK_RESIZE_NEEDED);
      node->data = widget;
    }
  
  resize_containers = NULL;
  
  for (node = resize_widgets; node; node = node->next)
    {
      GtkWidget *resize_container;
      
      widget = node->data;
      
      if (!GTK_WIDGET_RESIZE_NEEDED (widget))
	continue;
      
      resize_container = widget->parent;
      
      if (resize_container)
	{
	  GTK_PRIVATE_UNSET_FLAG (widget, GTK_RESIZE_NEEDED);
	  widget = resize_container->parent;
	  
	  while (widget)
	    {
	      if (GTK_WIDGET_RESIZE_NEEDED (widget))
		{
		  GTK_PRIVATE_UNSET_FLAG (resize_container, GTK_RESIZE_NEEDED);
		  resize_container = widget;
		}
	      widget = widget->parent;
	    }
	}
      else
	resize_container = widget;
      
      if (!g_slist_find (resize_containers, resize_container))
	resize_containers = g_slist_prepend (resize_containers,
					     resize_container);
    }
  g_slist_free (resize_widgets);
  
  for (node = resize_containers; node; node = node->next)
    {
      widget = node->data;
      
      GTK_PRIVATE_UNSET_FLAG (widget, GTK_RESIZE_NEEDED);
      gtk_widget_size_allocate (widget, &widget->allocation);
      gtk_widget_queue_draw (widget);
    }
  g_slist_free (resize_containers);
}

void
gtk_container_foreach (GtkContainer *container,
		       GtkCallback   callback,
		       gpointer      callback_data)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (callback != NULL);

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
gtk_container_set_focus_child (GtkContainer     *container,
			       GtkWidget 	  *widget)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (container));
  if (widget)
    g_return_if_fail (GTK_IS_WIDGET (container));

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
  gtk_widget_ref (GTK_WIDGET (container));
  gtk_object_sink (GTK_OBJECT (container));
}

void
gtk_container_unregister_toplevel (GtkContainer *container)
{
  gtk_widget_unref (GTK_WIDGET (container));
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

              if (GTK_WIDGET_VISIBLE (child) &&
		  GTK_IS_CONTAINER (child) &&
		  !GTK_WIDGET_HAS_FOCUS (child))
		if (gtk_container_focus (GTK_CONTAINER (child), direction))
		  return TRUE;
            }
        }
      else if (GTK_WIDGET_VISIBLE (child))
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
