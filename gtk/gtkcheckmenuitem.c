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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include "gtkcheckmenuitemprivate.h"
#include "gtkmenuitemprivate.h"
#include "gtkaccellabel.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "a11y/gtkcheckmenuitemaccessible.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkiconprivate.h"

/**
 * SECTION:gtkcheckmenuitem
 * @Short_description: A menu item with a check box
 * @Title: GtkCheckMenuItem
 *
 * A #GtkCheckMenuItem is a menu item that maintains the state of a boolean
 * value in addition to a #GtkMenuItem usual role in activating application
 * code.
 *
 * A check box indicating the state of the boolean value is displayed
 * at the left side of the #GtkMenuItem.  Activating the #GtkMenuItem
 * toggles the value.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * menuitem
 * ├── check.left
 * ╰── <child>
 * ]|
 *
 * GtkCheckMenuItem has a main CSS node with name menuitem, and a subnode
 * with name check, which gets the .left or .right style class.
 */

typedef struct _GtkCheckMenuItemPrivate GtkCheckMenuItemPrivate;

struct _GtkCheckMenuItemPrivate
{
  GtkWidget *indicator_widget;

  guint active             : 1;
  guint draw_as_radio      : 1;
  guint inconsistent       : 1;
};

enum {
  TOGGLED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_INCONSISTENT,
  PROP_DRAW_AS_RADIO
};

static void gtk_check_menu_item_activate             (GtkMenuItem           *menu_item);
static void gtk_check_menu_item_toggle_size_request  (GtkMenuItem           *menu_item,
                                                      gint                  *requisition);
static void gtk_check_menu_item_set_property         (GObject               *object,
                                                      guint                  prop_id,
                                                      const GValue          *value,
                                                      GParamSpec            *pspec);
static void gtk_check_menu_item_get_property         (GObject               *object,
                                                      guint                  prop_id,
                                                      GValue                *value,
                                                      GParamSpec            *pspec);

static void gtk_check_menu_item_state_flags_changed (GtkWidget        *widget,
                                                     GtkStateFlags     previous_state);
static void gtk_check_menu_item_direction_changed   (GtkWidget        *widget,
                                                     GtkTextDirection  previous_dir);

static guint                check_menu_item_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkCheckMenuItem, gtk_check_menu_item, GTK_TYPE_MENU_ITEM,
                         G_ADD_PRIVATE (GtkCheckMenuItem))

static void
gtk_check_menu_item_size_allocate (GtkWidget *widget,
                                   int        width,
                                   int        height,
                                   int        baseline)
{
  GtkAllocation indicator_alloc;
  GtkCheckMenuItem *check_menu_item = GTK_CHECK_MENU_ITEM (widget);
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);
  gint toggle_size;

  GTK_WIDGET_CLASS (gtk_check_menu_item_parent_class)->size_allocate (widget,
                                                                      width,
                                                                      height,
                                                                      baseline);

  gtk_widget_measure (priv->indicator_widget,
                      GTK_ORIENTATION_HORIZONTAL,
                      -1,
                      &indicator_alloc.width, NULL,
                      NULL, NULL);
  gtk_widget_measure (priv->indicator_widget,
                      GTK_ORIENTATION_VERTICAL,
                      -1,
                      &indicator_alloc.height, NULL,
                      NULL, NULL);
  toggle_size = GTK_MENU_ITEM (check_menu_item)->priv->toggle_size;

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    indicator_alloc.x = (toggle_size - indicator_alloc.width) / 2;
  else
    indicator_alloc.x = width - toggle_size +
      (toggle_size - indicator_alloc.width) / 2;

  indicator_alloc.y = (height - indicator_alloc.height) / 2;

  gtk_widget_size_allocate (priv->indicator_widget,
                            &indicator_alloc,
                            baseline);
}

static void
gtk_check_menu_item_finalize (GObject *object)
{
  GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM (object);
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (item);

  gtk_widget_unparent (priv->indicator_widget);

  G_OBJECT_CLASS (gtk_check_menu_item_parent_class)->finalize (object);
}

static void
gtk_check_menu_item_class_init (GtkCheckMenuItemClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkMenuItemClass *menu_item_class;
  
  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = (GtkWidgetClass*) klass;
  menu_item_class = (GtkMenuItemClass*) klass;
  
  gobject_class->set_property = gtk_check_menu_item_set_property;
  gobject_class->get_property = gtk_check_menu_item_get_property;
  gobject_class->finalize = gtk_check_menu_item_finalize;

  widget_class->size_allocate = gtk_check_menu_item_size_allocate;
  widget_class->state_flags_changed = gtk_check_menu_item_state_flags_changed;
  widget_class->direction_changed = gtk_check_menu_item_direction_changed;

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         P_("Active"),
                                                         P_("Whether the menu item is checked"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  g_object_class_install_property (gobject_class,
                                   PROP_INCONSISTENT,
                                   g_param_spec_boolean ("inconsistent",
                                                         P_("Inconsistent"),
                                                         P_("Whether to display an “inconsistent” state"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  g_object_class_install_property (gobject_class,
                                   PROP_DRAW_AS_RADIO,
                                   g_param_spec_boolean ("draw-as-radio",
                                                         P_("Draw as radio menu item"),
                                                         P_("Whether the menu item looks like a radio menu item"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  menu_item_class->activate = gtk_check_menu_item_activate;
  menu_item_class->hide_on_activate = FALSE;
  menu_item_class->toggle_size_request = gtk_check_menu_item_toggle_size_request;
  
  klass->toggled = NULL;

  /**
   * GtkCheckMenuItem::toggled:
   * @checkmenuitem: the object which received the signal.
   *
   * This signal is emitted when the state of the check box is changed.
   *
   * A signal handler can use gtk_check_menu_item_get_active()
   * to discover the new state.
   */
  check_menu_item_signals[TOGGLED] =
    g_signal_new (I_("toggled"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCheckMenuItemClass, toggled),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_CHECK_MENU_ITEM_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("menuitem"));
}

/**
 * gtk_check_menu_item_new:
 *
 * Creates a new #GtkCheckMenuItem.
 *
 * Returns: a new #GtkCheckMenuItem.
 */
GtkWidget*
gtk_check_menu_item_new (void)
{
  return g_object_new (GTK_TYPE_CHECK_MENU_ITEM, NULL);
}

/**
 * gtk_check_menu_item_new_with_label:
 * @label: the string to use for the label.
 *
 * Creates a new #GtkCheckMenuItem with a label.
 *
 * Returns: a new #GtkCheckMenuItem.
 */
GtkWidget*
gtk_check_menu_item_new_with_label (const gchar *label)
{
  return g_object_new (GTK_TYPE_CHECK_MENU_ITEM, 
                       "label", label,
                       NULL);
}


/**
 * gtk_check_menu_item_new_with_mnemonic:
 * @label: The text of the button, with an underscore in front of the
 *     character
 *
 * Creates a new #GtkCheckMenuItem containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the menu item.
 *
 * Returns: a new #GtkCheckMenuItem
 */
GtkWidget*
gtk_check_menu_item_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_CHECK_MENU_ITEM, 
                       "label", label,
                       "use-underline", TRUE,
                       NULL);
}

/**
 * gtk_check_menu_item_set_active:
 * @check_menu_item: a #GtkCheckMenuItem.
 * @is_active: boolean value indicating whether the check box is active.
 *
 * Sets the active state of the menu item’s check box.
 */
void
gtk_check_menu_item_set_active (GtkCheckMenuItem *check_menu_item,
                                gboolean          is_active)
{
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);

  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item));

  is_active = is_active != 0;

  if (priv->active != is_active)
    gtk_menu_item_activate (GTK_MENU_ITEM (check_menu_item));
}

/**
 * gtk_check_menu_item_get_active:
 * @check_menu_item: a #GtkCheckMenuItem
 * 
 * Returns whether the check menu item is active. See
 * gtk_check_menu_item_set_active ().
 * 
 * Returns: %TRUE if the menu item is checked.
 */
gboolean
gtk_check_menu_item_get_active (GtkCheckMenuItem *check_menu_item)
{
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);

  g_return_val_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item), FALSE);

  return priv->active;
}

static void
gtk_check_menu_item_toggle_size_request (GtkMenuItem *menu_item,
                                         gint        *requisition)
{
  GtkCheckMenuItem *check_menu_item = GTK_CHECK_MENU_ITEM (menu_item);
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);

  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (menu_item));

  gtk_widget_measure (priv->indicator_widget,
                      GTK_ORIENTATION_HORIZONTAL,
                      -1,
                      requisition, NULL,
                      NULL, NULL);
}

/**
 * gtk_check_menu_item_toggled:
 * @check_menu_item: a #GtkCheckMenuItem.
 *
 * Emits the #GtkCheckMenuItem::toggled signal.
 */
void
gtk_check_menu_item_toggled (GtkCheckMenuItem *check_menu_item)
{
  g_signal_emit (check_menu_item, check_menu_item_signals[TOGGLED], 0);
}

static void
update_node_state (GtkCheckMenuItem *check_menu_item)
{
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);
  GtkStateFlags state;

  state = gtk_widget_get_state_flags (GTK_WIDGET (check_menu_item));
  state &= ~(GTK_STATE_FLAG_CHECKED | GTK_STATE_FLAG_INCONSISTENT);

  if (priv->inconsistent)
    state |= GTK_STATE_FLAG_INCONSISTENT;
  if (priv->active)
    state |= GTK_STATE_FLAG_CHECKED;

  gtk_widget_set_state_flags (priv->indicator_widget, state, TRUE);
}

/**
 * gtk_check_menu_item_set_inconsistent:
 * @check_menu_item: a #GtkCheckMenuItem
 * @setting: %TRUE to display an “inconsistent” third state check
 *
 * If the user has selected a range of elements (such as some text or
 * spreadsheet cells) that are affected by a boolean setting, and the
 * current values in that range are inconsistent, you may want to
 * display the check in an “in between” state. This function turns on
 * “in between” display.  Normally you would turn off the inconsistent
 * state again if the user explicitly selects a setting. This has to be
 * done manually, gtk_check_menu_item_set_inconsistent() only affects
 * visual appearance, it doesn’t affect the semantics of the widget.
 * 
 **/
void
gtk_check_menu_item_set_inconsistent (GtkCheckMenuItem *check_menu_item,
                                      gboolean          setting)
{
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);

  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item));

  setting = setting != FALSE;

  if (setting != priv->inconsistent)
    {
      priv->inconsistent = setting;
      update_node_state (check_menu_item);
      gtk_widget_queue_draw (GTK_WIDGET (check_menu_item));
      g_object_notify (G_OBJECT (check_menu_item), "inconsistent");
    }
}

/**
 * gtk_check_menu_item_get_inconsistent:
 * @check_menu_item: a #GtkCheckMenuItem
 * 
 * Retrieves the value set by gtk_check_menu_item_set_inconsistent().
 * 
 * Returns: %TRUE if inconsistent
 **/
gboolean
gtk_check_menu_item_get_inconsistent (GtkCheckMenuItem *check_menu_item)
{
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);

  g_return_val_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item), FALSE);

  return priv->inconsistent;
}

/**
 * gtk_check_menu_item_set_draw_as_radio:
 * @check_menu_item: a #GtkCheckMenuItem
 * @draw_as_radio: whether @check_menu_item is drawn like a #GtkRadioMenuItem
 *
 * Sets whether @check_menu_item is drawn like a #GtkRadioMenuItem
 **/
void
gtk_check_menu_item_set_draw_as_radio (GtkCheckMenuItem *check_menu_item,
                                       gboolean          draw_as_radio)
{
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);
  GtkCssNode *indicator_node;

  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item));

  draw_as_radio = draw_as_radio != FALSE;

  if (draw_as_radio != priv->draw_as_radio)
    {
      priv->draw_as_radio = draw_as_radio;
      indicator_node = gtk_widget_get_css_node (priv->indicator_widget);
      if (draw_as_radio)
        gtk_css_node_set_name (indicator_node, I_("radio"));
      else
        gtk_css_node_set_name (indicator_node, I_("check"));

      gtk_widget_queue_draw (GTK_WIDGET (check_menu_item));

      g_object_notify (G_OBJECT (check_menu_item), "draw-as-radio");
    }
}

/**
 * gtk_check_menu_item_get_draw_as_radio:
 * @check_menu_item: a #GtkCheckMenuItem
 * 
 * Returns whether @check_menu_item looks like a #GtkRadioMenuItem
 * 
 * Returns: Whether @check_menu_item looks like a #GtkRadioMenuItem
 **/
gboolean
gtk_check_menu_item_get_draw_as_radio (GtkCheckMenuItem *check_menu_item)
{
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);

  g_return_val_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item), FALSE);
  
  return priv->draw_as_radio;
}

static void
gtk_check_menu_item_init (GtkCheckMenuItem *check_menu_item)
{
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);

  priv->active = FALSE;

  priv->indicator_widget = gtk_icon_new ("check");
  gtk_widget_set_parent (priv->indicator_widget, GTK_WIDGET (check_menu_item));
  update_node_state (check_menu_item);
}

static void
gtk_check_menu_item_activate (GtkMenuItem *menu_item)
{
  GtkCheckMenuItem *check_menu_item = GTK_CHECK_MENU_ITEM (menu_item);
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);

  priv->active = !priv->active;

  gtk_check_menu_item_toggled (check_menu_item);
  update_node_state (check_menu_item);
  gtk_widget_queue_draw (GTK_WIDGET (check_menu_item));

  GTK_MENU_ITEM_CLASS (gtk_check_menu_item_parent_class)->activate (menu_item);

  g_object_notify (G_OBJECT (check_menu_item), "active");
}

static void
gtk_check_menu_item_state_flags_changed (GtkWidget     *widget,
                                         GtkStateFlags  previous_state)

{
  GtkCheckMenuItem *check_menu_item = GTK_CHECK_MENU_ITEM (widget);

  update_node_state (check_menu_item);

  GTK_WIDGET_CLASS (gtk_check_menu_item_parent_class)->state_flags_changed (widget, previous_state);
}

static void
gtk_check_menu_item_direction_changed (GtkWidget        *widget,
                                       GtkTextDirection  previous_dir)
{
  GtkCheckMenuItem *check_menu_item = GTK_CHECK_MENU_ITEM (widget);
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);
  GtkStyleContext *context;
  GtkWidget *child;

  context = gtk_widget_get_style_context (priv->indicator_widget);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    {
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_LEFT);
      gtk_style_context_remove_class (context, GTK_STYLE_CLASS_RIGHT);

      child = gtk_widget_get_last_child (widget);

      if (child != priv->indicator_widget)
        gtk_widget_insert_before (priv->indicator_widget, widget, NULL);
    }
  else
    {
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_RIGHT);
      gtk_style_context_remove_class (context, GTK_STYLE_CLASS_LEFT);

      child = gtk_widget_get_first_child (widget);

      if (child != priv->indicator_widget)
        gtk_widget_insert_after (priv->indicator_widget, widget, NULL);
    }

  GTK_WIDGET_CLASS (gtk_check_menu_item_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_check_menu_item_get_property (GObject     *object,
                                  guint        prop_id,
                                  GValue      *value,
                                  GParamSpec  *pspec)
{
  GtkCheckMenuItem *check_menu_item = GTK_CHECK_MENU_ITEM (object);
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);
  
  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, priv->active);
      break;
    case PROP_INCONSISTENT:
      g_value_set_boolean (value, priv->inconsistent);
      break;
    case PROP_DRAW_AS_RADIO:
      g_value_set_boolean (value, priv->draw_as_radio);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
gtk_check_menu_item_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkCheckMenuItem *checkitem = GTK_CHECK_MENU_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_ACTIVE:
      gtk_check_menu_item_set_active (checkitem, g_value_get_boolean (value));
      break;
    case PROP_INCONSISTENT:
      gtk_check_menu_item_set_inconsistent (checkitem, g_value_get_boolean (value));
      break;
    case PROP_DRAW_AS_RADIO:
      gtk_check_menu_item_set_draw_as_radio (checkitem, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Private */

/*
 * _gtk_check_menu_item_set_active:
 * @check_menu_item: a #GtkCheckMenuItem
 * @is_active: whether the action is active or not
 *
 * Sets the #GtkCheckMenuItem:active property directly. This function does
 * not emit signals or notifications: it is left to the caller to do so.
 */
void
_gtk_check_menu_item_set_active (GtkCheckMenuItem *check_menu_item,
                                 gboolean          is_active)
{
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);

  priv->active = is_active;
  update_node_state (check_menu_item);
}

GtkWidget *
_gtk_check_menu_item_get_indicator_widget (GtkCheckMenuItem *check_menu_item)
{
  GtkCheckMenuItemPrivate *priv = gtk_check_menu_item_get_instance_private (check_menu_item);

  return priv->indicator_widget;
}
