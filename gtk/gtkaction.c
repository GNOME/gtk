/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998, 1999 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Author: James Henstridge <james@daa.com.au>
 *
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>

#include "gtkaction.h"
#include "gtkactiongroup.h"
#include "gtkaccellabel.h"
#include "gtkbutton.h"
#include "gtkimage.h"
#include "gtkimagemenuitem.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkmenuitem.h"
#include "gtkstock.h"
#include "gtktearoffmenuitem.h"
#include "gtktoolbutton.h"
#include "gtktoolbar.h"
#include "gtkalias.h"


#define GTK_ACTION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_ACTION, GtkActionPrivate))

struct _GtkActionPrivate 
{
  gchar *name;
  gchar *label;
  gchar *short_label;
  gchar *tooltip;
  gchar *stock_id; /* icon */

  guint sensitive          : 1;
  guint visible            : 1;
  guint label_set          : 1; /* these two used so we can set label */
  guint short_label_set    : 1; /* based on stock id */
  guint visible_horizontal : 1;
  guint visible_vertical   : 1;
  guint is_important       : 1;
  guint hide_if_empty      : 1;
  guint visible_overflown  : 1;

  /* accelerator */
  guint          accel_count;
  GtkAccelGroup *accel_group;
  GClosure      *accel_closure;
  GQuark         accel_quark;

  GtkActionGroup *action_group;

  /* list of proxy widgets */
  GSList *proxies;
};

enum 
{
  ACTIVATE,
  LAST_SIGNAL
};

enum 
{
  PROP_0,
  PROP_NAME,
  PROP_LABEL,
  PROP_SHORT_LABEL,
  PROP_TOOLTIP,
  PROP_STOCK_ID,
  PROP_VISIBLE_HORIZONTAL,
  PROP_VISIBLE_VERTICAL,
  PROP_VISIBLE_OVERFLOWN,
  PROP_IS_IMPORTANT,
  PROP_HIDE_IF_EMPTY,
  PROP_SENSITIVE,
  PROP_VISIBLE,
  PROP_ACTION_GROUP
};

static void gtk_action_init       (GtkAction *action);
static void gtk_action_class_init (GtkActionClass *class);

static GQuark       accel_path_id  = 0;
static const gchar accel_path_key[] = "GtkAction::accel_path";

GType
gtk_action_get_type (void)
{
  static GtkType type = 0;

  if (!type)
    {
      static const GTypeInfo type_info =
      {
        sizeof (GtkActionClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gtk_action_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        
        sizeof (GtkAction),
        0, /* n_preallocs */
        (GInstanceInitFunc) gtk_action_init,
      };

      type = g_type_register_static (G_TYPE_OBJECT,
				     "GtkAction",
				     &type_info, 0);
    }
  return type;
}

static void gtk_action_finalize     (GObject *object);
static void gtk_action_set_property (GObject         *object,
				     guint            prop_id,
				     const GValue    *value,
				     GParamSpec      *pspec);
static void gtk_action_get_property (GObject         *object,
				     guint            prop_id,
				     GValue          *value,
				     GParamSpec      *pspec);
static void gtk_action_set_action_group (GtkAction	    *action,
					 GtkActionGroup *action_group);

static GtkWidget *create_menu_item    (GtkAction *action);
static GtkWidget *create_tool_item    (GtkAction *action);
static void       connect_proxy       (GtkAction     *action,
				       GtkWidget     *proxy);
static void       disconnect_proxy    (GtkAction *action,
				       GtkWidget *proxy);
static void       closure_accel_activate (GClosure     *closure,
					  GValue       *return_value,
					  guint         n_param_values,
					  const GValue *param_values,
					  gpointer      invocation_hint,
					  gpointer      marshal_data);

static GObjectClass *parent_class = NULL;
static guint         action_signals[LAST_SIGNAL] = { 0 };


static void
gtk_action_class_init (GtkActionClass *klass)
{
  GObjectClass *gobject_class;

  accel_path_id = g_quark_from_static_string (accel_path_key);

  parent_class = g_type_class_peek_parent (klass);
  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = gtk_action_finalize;
  gobject_class->set_property = gtk_action_set_property;
  gobject_class->get_property = gtk_action_get_property;

  klass->activate = NULL;

  klass->create_menu_item = create_menu_item;
  klass->create_tool_item = create_tool_item;
  klass->connect_proxy = connect_proxy;
  klass->disconnect_proxy = disconnect_proxy;

  klass->menu_item_type = GTK_TYPE_IMAGE_MENU_ITEM;
  klass->toolbar_item_type = GTK_TYPE_TOOL_BUTTON;

  g_object_class_install_property (gobject_class,
				   PROP_NAME,
				   g_param_spec_string ("name",
							P_("Name"),
							P_("A unique name for the action."),
							NULL,
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class,
				   PROP_LABEL,
				   g_param_spec_string ("label",
							P_("Label"),
							P_("The label used for menu items and buttons "
							   "that activate this action."),
							NULL,
							G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SHORT_LABEL,
				   g_param_spec_string ("short_label",
							P_("Short label"),
							P_("A shorter label that may be used on toolbar buttons."),
							NULL,
							G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_TOOLTIP,
				   g_param_spec_string ("tooltip",
							P_("Tooltip"),
							P_("A tooltip for this action."),
							NULL,
							G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_STOCK_ID,
				   g_param_spec_string ("stock_id",
							P_("Stock Icon"),
							P_("The stock icon displayed in widgets representing "
							   "this action."),
							NULL,
							G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_VISIBLE_HORIZONTAL,
				   g_param_spec_boolean ("visible_horizontal",
							 P_("Visible when horizontal"),
							 P_("Whether the toolbar item is visible when the toolbar "
							    "is in a horizontal orientation."),
							 TRUE,
							 G_PARAM_READWRITE));
  /**
   * GtkAction:visible-overflown:
   *
   * When %TRUE, toolitem proxies for this action are represented in the 
   * toolbar overflow menu.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_VISIBLE_OVERFLOWN,
				   g_param_spec_boolean ("visible_overflown",
							 P_("Visible when overflown"),
							 P_("When TRUE, toolitem proxies for this action "
							    "are represented in the toolbar overflow menu."),
							 TRUE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_VISIBLE_VERTICAL,
				   g_param_spec_boolean ("visible_vertical",
							 P_("Visible when vertical"),
							 P_("Whether the toolbar item is visible when the toolbar "
							    "is in a vertical orientation."),
							 TRUE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_IS_IMPORTANT,
				   g_param_spec_boolean ("is_important",
							 P_("Is important"),
							 P_("Whether the action is considered important. "
							    "When TRUE, toolitem proxies for this action "
							    "show text in GTK_TOOLBAR_BOTH_HORIZ mode."),
							 FALSE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_HIDE_IF_EMPTY,
				   g_param_spec_boolean ("hide_if_empty",
							 P_("Hide if empty"),
							 P_("When TRUE, empty menu proxies for this action are hidden."),
							 TRUE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SENSITIVE,
				   g_param_spec_boolean ("sensitive",
							 P_("Sensitive"),
							 P_("Whether the action is enabled."),
							 TRUE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_VISIBLE,
				   g_param_spec_boolean ("visible",
							 P_("Visible"),
							 P_("Whether the action is visible."),
							 TRUE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_ACTION_GROUP,
				   g_param_spec_object ("action_group",
							 P_("Action Group"),
							 P_("The GtkActionGroup this GtkAction is associated with, or NULL (for internal use)."),
							 GTK_TYPE_ACTION_GROUP,
							 G_PARAM_READWRITE));

  /**
   * GtkAction::activate:
   * @action: the #GtkAction
   *
   * The "activate" signal is emitted when the action is activated.
   *
   * Since: 2.4
   */
  action_signals[ACTIVATE] =
    g_signal_new ("activate",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkActionClass, activate),  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (GtkActionPrivate));
}


static void
gtk_action_init (GtkAction *action)
{
  action->private_data = GTK_ACTION_GET_PRIVATE (action);

  action->private_data->name = NULL;
  action->private_data->label = NULL;
  action->private_data->short_label = NULL;
  action->private_data->tooltip = NULL;
  action->private_data->stock_id = NULL;
  action->private_data->visible_horizontal = TRUE;
  action->private_data->visible_vertical   = TRUE;
  action->private_data->visible_overflown  = TRUE;
  action->private_data->is_important = FALSE;
  action->private_data->hide_if_empty = TRUE;

  action->private_data->sensitive = TRUE;
  action->private_data->visible = TRUE;

  action->private_data->label_set = FALSE;
  action->private_data->short_label_set = FALSE;

  action->private_data->accel_count = 0;
  action->private_data->accel_group = NULL;
  action->private_data->accel_quark = 0;
  action->private_data->accel_closure = 
    g_closure_new_object (sizeof (GClosure), G_OBJECT (action));
  g_closure_set_marshal (action->private_data->accel_closure, 
			 closure_accel_activate);
  g_closure_ref (action->private_data->accel_closure);
  g_closure_sink (action->private_data->accel_closure);

  action->private_data->action_group = NULL;

  action->private_data->proxies = NULL;
}

/**
 * gtk_action_new:
 * @name: A unique name for the action
 * @label: the label displayed in menu items and on buttons
 * @tooltip: a tooltip for the action
 * @stock_id: the stock icon to display in widgets representing the action
 *
 * Creates a new #GtkAction object. To add the action to a
 * #GtkActionGroup and set the accelerator for the action,
 * call gtk_action_group_add_action_with_accel().
 * See <xref linkend="XML-UI"/> for information on allowed action
 * names.
 *
 * Return value: a new #GtkAction
 *
 * Since: 2.4
 */
GtkAction *
gtk_action_new (const gchar *name,
		const gchar *label,
		const gchar *tooltip,
		const gchar *stock_id)
{
  GtkAction *action;

  action = g_object_new (GTK_TYPE_ACTION,
			 "name", name,
			 "label", label,
			 "tooltip", tooltip,
			 "stock_id", stock_id,
			 NULL);

  return action;
}

static void
gtk_action_finalize (GObject *object)
{
  GtkAction *action;
  action = GTK_ACTION (object);

  g_free (action->private_data->name);
  g_free (action->private_data->label);
  g_free (action->private_data->short_label);
  g_free (action->private_data->tooltip);
  g_free (action->private_data->stock_id);

  g_closure_unref (action->private_data->accel_closure);
  if (action->private_data->accel_group)
    g_object_unref (action->private_data->accel_group);

  G_OBJECT_CLASS (parent_class)->finalize (object);  
}

static void
gtk_action_set_property (GObject         *object,
			 guint            prop_id,
			 const GValue    *value,
			 GParamSpec      *pspec)
{
  GtkAction *action;
  gchar *tmp;
  
  action = GTK_ACTION (object);

  switch (prop_id)
    {
    case PROP_NAME:
      tmp = action->private_data->name;
      action->private_data->name = g_value_dup_string (value);
      g_free (tmp);
      break;
    case PROP_LABEL:
      tmp = action->private_data->label;
      action->private_data->label = g_value_dup_string (value);
      g_free (tmp);
      action->private_data->label_set = (action->private_data->label != NULL);
      /* if label is unset, then use the label from the stock item */
      if (!action->private_data->label_set && action->private_data->stock_id)
	{
	  GtkStockItem stock_item;

	  if (gtk_stock_lookup (action->private_data->stock_id, &stock_item))
	    action->private_data->label = g_strdup (stock_item.label);
	}
      /* if short_label is unset, set short_label=label */
      if (!action->private_data->short_label_set)
	{
	  tmp = action->private_data->short_label;
	  action->private_data->short_label = g_strdup (action->private_data->label);
	  g_free (tmp);
	  g_object_notify (object, "short-label");
	}
      break;
    case PROP_SHORT_LABEL:
      tmp = action->private_data->short_label;
      action->private_data->short_label = g_value_dup_string (value);
      g_free (tmp);
      action->private_data->short_label_set = (action->private_data->short_label != NULL);
      /* if short_label is unset, then use the value of label */
      if (!action->private_data->short_label_set)
	{
	  action->private_data->short_label = g_strdup (action->private_data->label);
	}
      break;
    case PROP_TOOLTIP:
      tmp = action->private_data->tooltip;
      action->private_data->tooltip = g_value_dup_string (value);
      g_free (tmp);
      break;
    case PROP_STOCK_ID:
      tmp = action->private_data->stock_id;
      action->private_data->stock_id = g_value_dup_string (value);
      g_free (tmp);
      /* update label and short_label if appropriate */
      if (!action->private_data->label_set)
	{
	  GtkStockItem stock_item;

	  g_free (action->private_data->label);
	  if (gtk_stock_lookup (action->private_data->stock_id, &stock_item))
	    action->private_data->label = g_strdup (stock_item.label);
	  else
	    action->private_data->label = NULL;
	  g_object_notify (object, "label");
	}
      if (!action->private_data->short_label_set)
	{
	  tmp = action->private_data->short_label;
	  action->private_data->short_label = g_strdup (action->private_data->label);
	  g_free (tmp);
	  g_object_notify (object, "short-label");
	}
      break;
    case PROP_VISIBLE_HORIZONTAL:
      action->private_data->visible_horizontal = g_value_get_boolean (value);
      break;
    case PROP_VISIBLE_VERTICAL:
      action->private_data->visible_vertical = g_value_get_boolean (value);
      break;
    case PROP_VISIBLE_OVERFLOWN:
      action->private_data->visible_overflown = g_value_get_boolean (value);
      break;
    case PROP_IS_IMPORTANT:
      action->private_data->is_important = g_value_get_boolean (value);
      break;
    case PROP_HIDE_IF_EMPTY:
      action->private_data->hide_if_empty = g_value_get_boolean (value);
      break;
    case PROP_SENSITIVE:
      action->private_data->sensitive = g_value_get_boolean (value);
      break;
    case PROP_VISIBLE:
      action->private_data->visible = g_value_get_boolean (value);
      break;
    case PROP_ACTION_GROUP:
      gtk_action_set_action_group (action, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_action_get_property (GObject    *object,
			 guint       prop_id,
			 GValue     *value,
			 GParamSpec *pspec)
{
  GtkAction *action;

  action = GTK_ACTION (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, action->private_data->name);
      break;
    case PROP_LABEL:
      g_value_set_string (value, action->private_data->label);
      break;
    case PROP_SHORT_LABEL:
      g_value_set_string (value, action->private_data->short_label);
      break;
    case PROP_TOOLTIP:
      g_value_set_string (value, action->private_data->tooltip);
      break;
    case PROP_STOCK_ID:
      g_value_set_string (value, action->private_data->stock_id);
      break;
    case PROP_VISIBLE_HORIZONTAL:
      g_value_set_boolean (value, action->private_data->visible_horizontal);
      break;
    case PROP_VISIBLE_VERTICAL:
      g_value_set_boolean (value, action->private_data->visible_vertical);
      break;
    case PROP_VISIBLE_OVERFLOWN:
      g_value_set_boolean (value, action->private_data->visible_overflown);
      break;
    case PROP_IS_IMPORTANT:
      g_value_set_boolean (value, action->private_data->is_important);
      break;
    case PROP_HIDE_IF_EMPTY:
      g_value_set_boolean (value, action->private_data->hide_if_empty);
      break;
    case PROP_SENSITIVE:
      g_value_set_boolean (value, action->private_data->sensitive);
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, action->private_data->visible);
      break;
    case PROP_ACTION_GROUP:
      g_value_set_object (value, action->private_data->action_group);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GtkWidget *
create_menu_item (GtkAction *action)
{
  GType menu_item_type;

  menu_item_type = GTK_ACTION_GET_CLASS (action)->menu_item_type;

  return g_object_new (menu_item_type, NULL);
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
  GType toolbar_item_type;

  toolbar_item_type = GTK_ACTION_GET_CLASS (action)->toolbar_item_type;

  return g_object_new (toolbar_item_type, NULL);
}

static void
remove_proxy (GtkWidget *proxy, 
	      GtkAction *action)
{
  if (GTK_IS_MENU_ITEM (proxy))
    gtk_action_disconnect_accelerator (action);

  action->private_data->proxies = g_slist_remove (action->private_data->proxies, proxy);
}

static void
gtk_action_sync_sensitivity (GtkAction  *action, 
			     GParamSpec *pspec,
			     GtkWidget  *proxy)
{
  gtk_widget_set_sensitive (proxy, gtk_action_is_sensitive (action));
}

static void
gtk_action_sync_property (GtkAction  *action, 
			  GParamSpec *pspec,
			  GtkWidget  *proxy)
{
  const gchar *property;
  GValue value = { 0, };

  property = g_param_spec_get_name (pspec);

  g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  g_object_get_property (G_OBJECT (action), property, &value);

  g_object_set_property (G_OBJECT (proxy), property, &value);
  g_value_unset (&value);
}

/**
 * _gtk_action_sync_menu_visible:
 * @action: a #GtkAction, or %NULL to determine the action from @proxy
 * @proxy: a proxy menu item
 * @empty: whether the submenu attached to @proxy is empty
 * 
 * Updates the visibility of @proxy from the visibility of @action
 * according to the following rules:
 * <itemizedlist>
 * <listitem><para>if @action is invisible, @proxy is too
 * </para></listitem>
 * <listitem><para>if @empty is %TRUE, hide @proxy unless the "hide-if-empty" 
 *   property of @action indicates otherwise
 * </para></listitem>
 * </itemizedlist>
 * 
 * This function is used in the implementation of #GtkUIManager.
 **/
void
_gtk_action_sync_menu_visible (GtkAction *action,
			       GtkWidget *proxy,
			       gboolean   empty)
{
  gboolean visible, hide_if_empty;

  g_return_if_fail (GTK_IS_MENU_ITEM (proxy));
  g_return_if_fail (action == NULL || GTK_IS_ACTION (action));
  
  if (action == NULL)
    action = g_object_get_data (G_OBJECT (proxy), "gtk-action");

  visible = gtk_action_is_visible (action);
  hide_if_empty = action->private_data->hide_if_empty;

  if (visible && !(empty && hide_if_empty))
    gtk_widget_show (proxy);
  else
    gtk_widget_hide (proxy);
}

gboolean _gtk_menu_is_empty (GtkWidget *menu);

static void
gtk_action_sync_visible (GtkAction  *action, 
			 GParamSpec *pspec,
			 GtkWidget  *proxy)
{
  if (GTK_IS_MENU_ITEM (proxy))
    {
      GtkWidget *menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (proxy));

      _gtk_action_sync_menu_visible (action, proxy, _gtk_menu_is_empty (menu));
    }
  else
    {
      if (gtk_action_is_visible (action))
	gtk_widget_show (proxy);
      else
	gtk_widget_hide (proxy);
    }
}

static void
gtk_action_sync_label (GtkAction  *action, 
		       GParamSpec *pspec, 
		       GtkWidget  *proxy)
{
  GtkWidget *label = NULL;

  g_return_if_fail (GTK_IS_MENU_ITEM (proxy));
  label = GTK_BIN (proxy)->child;

  if (GTK_IS_LABEL (label))
    gtk_label_set_label (GTK_LABEL (label), action->private_data->label);
}

static void
gtk_action_sync_short_label (GtkAction  *action, 
			     GParamSpec *pspec,
			     GtkWidget  *proxy)
{
  GValue value = { 0, };

  g_value_init (&value, G_TYPE_STRING);
  g_object_get_property (G_OBJECT (action), "short_label", &value);

  g_object_set_property (G_OBJECT (proxy), "label", &value);
  g_value_unset (&value);
}

static void
gtk_action_sync_stock_id (GtkAction  *action, 
			  GParamSpec *pspec,
			  GtkWidget  *proxy)
{
  GtkWidget *image = NULL;

  if (GTK_IS_IMAGE_MENU_ITEM (proxy))
    {
      image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (proxy));

      if (GTK_IS_IMAGE (image))
	gtk_image_set_from_stock (GTK_IMAGE (image),
				  action->private_data->stock_id, GTK_ICON_SIZE_MENU);
    }
}

static void
gtk_action_sync_button_stock_id (GtkAction  *action, 
				 GParamSpec *pspec,
				 GtkWidget  *proxy)
{
  g_object_set (G_OBJECT (proxy),
                "stock-id",
                action->private_data->stock_id,
                NULL);
}

static void
gtk_action_sync_tooltip (GtkAction  *action, 
			 GParamSpec *pspec, 
			 GtkWidget  *proxy)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (proxy));

  if (GTK_IS_TOOLBAR (gtk_widget_get_parent (proxy)))
    {
      GtkToolbar *toolbar = GTK_TOOLBAR (gtk_widget_get_parent (proxy));
      
      gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (proxy), 
				 toolbar->tooltips,
				 action->private_data->tooltip,
				 NULL);
    }
}


static gboolean
gtk_action_create_menu_proxy (GtkToolItem *tool_item, 
			      GtkAction   *action)
{
  GtkWidget *menu_item;
  
  if (action->private_data->visible_overflown)
    {
      menu_item = gtk_action_create_menu_item (action);

      g_object_ref (menu_item);
      gtk_object_sink (GTK_OBJECT (menu_item));
      
      gtk_tool_item_set_proxy_menu_item (tool_item, 
					 "gtk-action-menu-item", menu_item);
      g_object_unref (menu_item);
    }
  else
    gtk_tool_item_set_proxy_menu_item (tool_item, 
				       "gtk-action-menu-item", NULL);

  return TRUE;
}

static void
connect_proxy (GtkAction     *action, 
	       GtkWidget     *proxy)
{
  g_object_ref (action);
  g_object_set_data_full (G_OBJECT (proxy), "gtk-action", action,
			  g_object_unref);

  /* add this widget to the list of proxies */
  action->private_data->proxies = g_slist_prepend (action->private_data->proxies, proxy);
  g_signal_connect (proxy, "destroy",
		    G_CALLBACK (remove_proxy), action);

  g_signal_connect_object (action, "notify::sensitive",
			   G_CALLBACK (gtk_action_sync_sensitivity), proxy, 0);
  gtk_widget_set_sensitive (proxy, gtk_action_is_sensitive (action));

  g_signal_connect_object (action, "notify::visible",
			   G_CALLBACK (gtk_action_sync_visible), proxy, 0);
  if (gtk_action_is_visible (action))
    gtk_widget_show (proxy);
  else
    gtk_widget_hide (proxy);
  gtk_widget_set_no_show_all (proxy, TRUE);

  if (GTK_IS_MENU_ITEM (proxy))
    {
      GtkWidget *label;
      /* menu item specific synchronisers ... */
      
      if (action->private_data->accel_quark)
 	{
	  gtk_action_connect_accelerator (action);
 	  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (proxy),
 					g_quark_to_string (action->private_data->accel_quark));
 	}
      
      label = GTK_BIN (proxy)->child;

      /* make sure label is a label */
      if (label && !GTK_IS_LABEL (label))
	{
	  gtk_container_remove (GTK_CONTAINER (proxy), label);
	  label = NULL;
	}

      if (!label)
	label = g_object_new (GTK_TYPE_ACCEL_LABEL,
			      "use_underline", TRUE,
			      "xalign", 0.0,
			      "visible", TRUE,
			      "parent", proxy,
			      NULL);
      
      if (GTK_IS_ACCEL_LABEL (label) && action->private_data->accel_quark)
	g_object_set (label,
		      "accel_closure", action->private_data->accel_closure,
		      NULL);

      gtk_label_set_label (GTK_LABEL (label), action->private_data->label);
      g_signal_connect_object (action, "notify::label",
			       G_CALLBACK (gtk_action_sync_label), proxy, 0);

      if (GTK_IS_IMAGE_MENU_ITEM (proxy))
	{
	  GtkWidget *image;

	  image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (proxy));
	  if (image && !GTK_IS_IMAGE (image))
	    {
	      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (proxy), NULL);
	      image = NULL;
	    }
	  if (!image)
	    {
	      image = gtk_image_new_from_stock (NULL,
						GTK_ICON_SIZE_MENU);
	      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (proxy),
					     image);
	      gtk_widget_show (image);
	    }
	  gtk_image_set_from_stock (GTK_IMAGE (image),
				    action->private_data->stock_id, GTK_ICON_SIZE_MENU);
	  g_signal_connect_object (action, "notify::stock-id",
				   G_CALLBACK (gtk_action_sync_stock_id),
				   proxy, 0);
	}

      if (gtk_menu_item_get_submenu (GTK_MENU_ITEM (proxy)) == NULL)
	g_signal_connect_object (proxy, "activate",
				 G_CALLBACK (gtk_action_activate), action,
				 G_CONNECT_SWAPPED);

    }
  else if (GTK_IS_TOOL_ITEM (proxy))
    {
      GParamSpec *pspec;

      /* toolbar item specific synchronisers ... */

      g_object_set (proxy,
		    "visible_horizontal", action->private_data->visible_horizontal,
		    "visible_vertical",	  action->private_data->visible_vertical,
		    "is_important", action->private_data->is_important,
		    NULL);

      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (action),
                                            "tooltip");
      gtk_action_sync_tooltip (action, pspec, proxy);

      g_signal_connect_object (action, "notify::visible-horizontal",
			       G_CALLBACK (gtk_action_sync_property), 
			       proxy, 0);
      g_signal_connect_object (action, "notify::visible-vertical",
			       G_CALLBACK (gtk_action_sync_property), 
			       proxy, 0);
      g_signal_connect_object (action, "notify::is-important",
			       G_CALLBACK (gtk_action_sync_property), 
			       proxy, 0);
      g_signal_connect_object (action, "notify::tooltip",
			       G_CALLBACK (gtk_action_sync_tooltip), 
			       proxy, 0);

      g_signal_connect_object (proxy, "create_menu_proxy",
			       G_CALLBACK (gtk_action_create_menu_proxy),
			       action, 0);

      gtk_tool_item_rebuild_menu (GTK_TOOL_ITEM (proxy));

      /* toolbar button specific synchronisers ... */
      if (GTK_IS_TOOL_BUTTON (proxy))
	{
	  g_object_set (proxy,
			"label", action->private_data->short_label,
			"use_underline", TRUE,
			"stock_id", action->private_data->stock_id,
			NULL);

	  g_signal_connect_object (action, "notify::short-label",
				   G_CALLBACK (gtk_action_sync_short_label),
				   proxy, 0);      
	  g_signal_connect_object (action, "notify::stock-id",
				   G_CALLBACK (gtk_action_sync_property), 
				   proxy, 0);
	  g_signal_connect_object (proxy, "clicked",
				   G_CALLBACK (gtk_action_activate), action,
				   G_CONNECT_SWAPPED);
	}
    }
  else if (GTK_IS_BUTTON (proxy))
    {
      /* button specific synchronisers ... */
      if (gtk_button_get_use_stock (GTK_BUTTON (proxy)))
	{
	  /* synchronise stock-id */
	  g_object_set (proxy,
			"stock-id", action->private_data->stock_id,
			NULL);
	  g_signal_connect_object (action, "notify::stock-id",
				   G_CALLBACK (gtk_action_sync_button_stock_id),
				   proxy, 0);
	}
      else if (GTK_IS_LABEL(GTK_BIN(proxy)->child))
	{
	  /* synchronise the label */
	  g_object_set (proxy,
			"label", action->private_data->short_label,
			"use_underline", TRUE,
			NULL);
	  g_signal_connect_object (action, "notify::short-label",
				   G_CALLBACK (gtk_action_sync_short_label),
				   proxy, 0);
	  
	}
      
      /* we leave the button alone if there is a custom child */
      g_signal_connect_object (proxy, "clicked",
			       G_CALLBACK (gtk_action_activate), action,
			       G_CONNECT_SWAPPED);
    }

  if (action->private_data->action_group)
    _gtk_action_group_emit_connect_proxy (action->private_data->action_group, action, proxy);
}

static void
disconnect_proxy (GtkAction *action, 
		  GtkWidget *proxy)
{
  g_object_set_data (G_OBJECT (proxy), "gtk-action", NULL);

  /* remove proxy from list of proxies */
  g_signal_handlers_disconnect_by_func (proxy,
					G_CALLBACK (remove_proxy),
					action);
  remove_proxy (proxy, action);

  /* disconnect the activate handler */
  g_signal_handlers_disconnect_by_func (proxy,
					G_CALLBACK (gtk_action_activate),
					action);

  /* disconnect handlers for notify::* signals */
  g_signal_handlers_disconnect_by_func (action,
					G_CALLBACK (gtk_action_sync_sensitivity),
					proxy);
  g_signal_handlers_disconnect_by_func (action,
					G_CALLBACK (gtk_action_sync_property),
					proxy);

  g_signal_handlers_disconnect_by_func (action,
				G_CALLBACK (gtk_action_sync_stock_id), proxy);

  /* menu item specific synchronisers ... */
  g_signal_handlers_disconnect_by_func (action,
					G_CALLBACK (gtk_action_sync_label),
					proxy);
  
  /* toolbar button specific synchronisers ... */
  g_signal_handlers_disconnect_by_func (action,
					G_CALLBACK (gtk_action_sync_short_label),
					proxy);

  g_signal_handlers_disconnect_by_func (proxy,
					G_CALLBACK (gtk_action_create_menu_proxy),
					action);

  if (action->private_data->action_group)
    _gtk_action_group_emit_disconnect_proxy (action->private_data->action_group, action, proxy);
}

void
_gtk_action_emit_activate (GtkAction *action)
{
  GtkActionGroup *group = action->private_data->action_group;

  if (group != NULL) 
    {
      g_object_ref (group);
      _gtk_action_group_emit_pre_activate (group, action);
    }

    g_signal_emit (action, action_signals[ACTIVATE], 0);

  if (group != NULL) 
    {
      _gtk_action_group_emit_post_activate (group, action);
      g_object_unref (group);
    }
}

/**
 * gtk_action_activate:
 * @action: the action object
 *
 * Emits the "activate" signal on the specified action, if it isn't 
 * insensitive. This gets called by the proxy widgets when they get 
 * activated.
 *
 * It can also be used to manually activate an action.
 *
 * Since: 2.4
 */
void
gtk_action_activate (GtkAction *action)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  
  if (gtk_action_is_sensitive (action))
    _gtk_action_emit_activate (action);
}

/**
 * gtk_action_create_icon:
 * @action: the action object
 * @icon_size: the size of the icon that should be created.
 *
 * This function is intended for use by action implementations to
 * create icons displayed in the proxy widgets.
 *
 * Returns: a widget that displays the icon for this action.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_action_create_icon (GtkAction *action, GtkIconSize icon_size)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  if (action->private_data->stock_id)
    return gtk_image_new_from_stock (action->private_data->stock_id, icon_size);
  else
    return NULL;
}

/**
 * gtk_action_create_menu_item:
 * @action: the action object
 *
 * Creates a menu item widget that proxies for the given action.
 *
 * Returns: a menu item connected to the action.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_action_create_menu_item (GtkAction *action)
{
  GtkWidget *menu_item;

  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  menu_item = (* GTK_ACTION_GET_CLASS (action)->create_menu_item) (action);

  (* GTK_ACTION_GET_CLASS (action)->connect_proxy) (action, menu_item);

  return menu_item;
}

/**
 * gtk_action_create_tool_item:
 * @action: the action object
 *
 * Creates a toolbar item widget that proxies for the given action.
 *
 * Returns: a toolbar item connected to the action.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_action_create_tool_item (GtkAction *action)
{
  GtkWidget *button;

  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  button = (* GTK_ACTION_GET_CLASS (action)->create_tool_item) (action);

  (* GTK_ACTION_GET_CLASS (action)->connect_proxy) (action, button);

  return button;
}

/**
 * gtk_action_connect_proxy:
 * @action: the action object
 * @proxy: the proxy widget
 *
 * Connects a widget to an action object as a proxy.  Synchronises 
 * various properties of the action with the widget (such as label 
 * text, icon, tooltip, etc), and attaches a callback so that the 
 * action gets activated when the proxy widget does.
 *
 * If the widget is already connected to an action, it is disconnected
 * first.
 *
 * Since: 2.4
 */
void
gtk_action_connect_proxy (GtkAction *action,
			  GtkWidget *proxy)
{
  GtkAction *prev_action;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (GTK_IS_WIDGET (proxy));

  prev_action = g_object_get_data (G_OBJECT (proxy), "gtk-action");

  if (prev_action)
    (* GTK_ACTION_GET_CLASS (action)->disconnect_proxy) (prev_action, proxy);  

  (* GTK_ACTION_GET_CLASS (action)->connect_proxy) (action, proxy);
}

/**
 * gtk_action_disconnect_proxy:
 * @action: the action object
 * @proxy: the proxy widget
 *
 * Disconnects a proxy widget from an action.  
 * Does <emphasis>not</emphasis> destroy the widget, however.
 *
 * Since: 2.4
 */
void
gtk_action_disconnect_proxy (GtkAction *action,
			     GtkWidget *proxy)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (GTK_IS_WIDGET (proxy));

  g_return_if_fail (g_object_get_data (G_OBJECT (proxy), "gtk-action") == action);

  (* GTK_ACTION_GET_CLASS (action)->disconnect_proxy) (action, proxy);  
}

/**
 * gtk_action_get_proxies:
 * @action: the action object
 * 
 * Returns the proxy widgets for an action.
 * 
 * Return value: a #GSList of proxy widgets. The list is owned by the action and
 * must not be modified.
 *
 * Since: 2.4
 **/
GSList*
gtk_action_get_proxies (GtkAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  return action->private_data->proxies;
}


/**
 * gtk_action_get_name:
 * @action: the action object
 * 
 * Returns the name of the action.
 * 
 * Return value: the name of the action. The string belongs to GTK+ and should not
 *   be freed.
 *
 * Since: 2.4
 **/
G_CONST_RETURN gchar *
gtk_action_get_name (GtkAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  return action->private_data->name;
}

/**
 * gtk_action_is_sensitive:
 * @action: the action object
 * 
 * Returns whether the action is effectively sensitive.
 *
 * Return value: %TRUE if the action and its associated action group 
 * are both sensitive.
 *
 * Since: 2.4
 **/
gboolean
gtk_action_is_sensitive (GtkAction *action)
{
  GtkActionPrivate *priv;
  g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

  priv = action->private_data;
  return priv->sensitive &&
    (priv->action_group == NULL ||
     gtk_action_group_get_sensitive (priv->action_group));
}

/**
 * gtk_action_get_sensitive:
 * @action: the action object
 * 
 * Returns whether the action itself is sensitive. Note that this doesn't 
 * necessarily mean effective sensitivity. See gtk_action_is_sensitive() 
 * for that.
 *
 * Return value: %TRUE if the action itself is sensitive.
 *
 * Since: 2.4
 **/
gboolean
gtk_action_get_sensitive (GtkAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

  return action->private_data->sensitive;
}

/**
 * gtk_action_set_sensitive:
 * @action: the action object
 * @sensitive: %TRUE to make the action sensitive
 * 
 * Sets the ::sensitive property of the action to @sensitive. Note that 
 * this doesn't necessarily mean effective sensitivity. See 
 * gtk_action_is_sensitive() 
 * for that.
 *
 * Since: 2.6
 **/
void
gtk_action_set_sensitive (GtkAction *action,
			  gboolean   sensitive)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  sensitive = sensitive != FALSE;
  
  if (action->private_data->sensitive != sensitive)
    {
      action->private_data->sensitive = sensitive;

      g_object_notify (G_OBJECT (action), "sensitive");
    }
}

/**
 * gtk_action_is_visible:
 * @action: the action object
 * 
 * Returns whether the action is effectively visible.
 *
 * Return value: %TRUE if the action and its associated action group 
 * are both visible.
 *
 * Since: 2.4
 **/
gboolean
gtk_action_is_visible (GtkAction *action)
{
  GtkActionPrivate *priv;
  g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

  priv = action->private_data;
  return priv->visible &&
    (priv->action_group == NULL ||
     gtk_action_group_get_visible (priv->action_group));
}

/**
 * gtk_action_get_visible:
 * @action: the action object
 * 
 * Returns whether the action itself is visible. Note that this doesn't 
 * necessarily mean effective visibility. See gtk_action_is_sensitive() 
 * for that.
 *
 * Return value: %TRUE if the action itself is visible.
 *
 * Since: 2.4
 **/
gboolean
gtk_action_get_visible (GtkAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

  return action->private_data->visible;
}

/**
 * gtk_action_set_visible:
 * @action: the action object
 * @visible: %TRUE to make the action visible
 * 
 * Sets the ::visible property of the action to @visible. Note that 
 * this doesn't necessarily mean effective visibility. See 
 * gtk_action_is_visible() 
 * for that.
 *
 * Since: 2.6
 **/
void
gtk_action_set_visible (GtkAction *action,
			gboolean   visible)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  visible = visible != FALSE;
  
  if (action->private_data->visible != visible)
    {
      action->private_data->visible = visible;

      g_object_notify (G_OBJECT (action), "visible");
    }
}

/**
 * gtk_action_block_activate_from:
 * @action: the action object
 * @proxy: a proxy widget
 *
 * Disables calls to the gtk_action_activate()
 * function by signals on the given proxy widget.  This is used to
 * break notification loops for things like check or radio actions.
 *
 * This function is intended for use by action implementations.
 * 
 * Since: 2.4
 */
void
gtk_action_block_activate_from (GtkAction *action, 
				GtkWidget *proxy)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  
  g_signal_handlers_block_by_func (proxy, G_CALLBACK (gtk_action_activate),
				   action);
}

/**
 * gtk_action_unblock_activate_from:
 * @action: the action object
 * @proxy: a proxy widget
 *
 * Re-enables calls to the gtk_action_activate()
 * function by signals on the given proxy widget.  This undoes the
 * blocking done by gtk_action_block_activate_from().
 *
 * This function is intended for use by action implementations.
 * 
 * Since: 2.4
 */
void
gtk_action_unblock_activate_from (GtkAction *action, 
				  GtkWidget *proxy)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  g_signal_handlers_unblock_by_func (proxy, G_CALLBACK (gtk_action_activate),
				     action);
}

static void
closure_accel_activate (GClosure     *closure,
                        GValue       *return_value,
                        guint         n_param_values,
                        const GValue *param_values,
                        gpointer      invocation_hint,
                        gpointer      marshal_data)
{
  if (gtk_action_is_sensitive (GTK_ACTION (closure->data)))
    {
      _gtk_action_emit_activate (GTK_ACTION (closure->data));
      
      /* we handled the accelerator */
      g_value_set_boolean (return_value, TRUE);
    }
}

static void
gtk_action_set_action_group (GtkAction	    *action,
			     GtkActionGroup *action_group)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  if (action->private_data->action_group == NULL)
    g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));
  else
    g_return_if_fail (action_group == NULL);

  action->private_data->action_group = action_group;
}

/**
 * gtk_action_set_accel_path:
 * @action: the action object
 * @accel_path: the accelerator path
 *
 * Sets the accel path for this action.  All proxy widgets associated
 * with the action will have this accel path, so that their
 * accelerators are consistent.
 *
 * Since: 2.4
 */
void
gtk_action_set_accel_path (GtkAction   *action, 
			   const gchar *accel_path)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  action->private_data->accel_quark = g_quark_from_string (accel_path);
}

/**
 * gtk_action_get_accel_path:
 * @action: the action object
 *
 * Returns the accel path for this action.  
 *
 * Since: 2.6
 *
 * Returns: the accel path for this action, or %NULL
 *   if none is set. The returned string is owned by GTK+
 *   and must not be freed or modified.
 */
G_CONST_RETURN gchar *
gtk_action_get_accel_path (GtkAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  if (action->private_data->accel_quark)
    return g_quark_to_string (action->private_data->accel_quark);
  else
    return NULL;
}


/**
 * gtk_action_set_accel_group:
 * @action: the action object
 * @accel_group: a #GtkAccelGroup or %NULL
 * 
 * Sets the #GtkAccelGroup in which the accelerator for this action
 * will be installed.
 *
 * Since: 2.4
 **/
void
gtk_action_set_accel_group (GtkAction     *action,
			    GtkAccelGroup *accel_group)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (accel_group == NULL || GTK_IS_ACCEL_GROUP (accel_group));
  
  if (accel_group)
    g_object_ref (accel_group);
  if (action->private_data->accel_group)
    g_object_unref (action->private_data->accel_group);

  action->private_data->accel_group = accel_group;
}

/**
 * gtk_action_connect_accelerator:
 * @action: a #GtkAction
 * 
 * Installs the accelerator for @action if @action has an
 * accel path and group. See gtk_action_set_accel_path() and 
 * gtk_action_set_accel_group()
 *
 * Since multiple proxies may independently trigger the installation
 * of the accelerator, the @action counts the number of times this
 * function has been called and doesn't remove the accelerator until
 * gtk_action_disconnect_accelerator() has been called as many times.
 *
 * Since: 2.4
 **/
void 
gtk_action_connect_accelerator (GtkAction *action)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  if (!action->private_data->accel_quark ||
      !action->private_data->accel_group)
    return;

  if (action->private_data->accel_count == 0)
    {
      const gchar *accel_path = 
	g_quark_to_string (action->private_data->accel_quark);
      
      gtk_accel_group_connect_by_path (action->private_data->accel_group,
				       accel_path,
				       action->private_data->accel_closure);
    }

  action->private_data->accel_count++;
}

/**
 * gtk_action_disconnect_accelerator:
 * @action: a #GtkAction
 * 
 * Undoes the effect of one call to gtk_action_connect_accelerator().
 *
 * Since: 2.4
 **/
void 
gtk_action_disconnect_accelerator (GtkAction *action)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  if (!action->private_data->accel_quark ||
      !action->private_data->accel_group)
    return;

  action->private_data->accel_count--;

  if (action->private_data->accel_count == 0)
    gtk_accel_group_disconnect (action->private_data->accel_group,
				action->private_data->accel_closure);
}

#define __GTK_ACTION_C__
#include "gtkaliasdef.c"
