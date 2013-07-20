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
#include "gtkcheckmenuitem.h"
#include "gtkmenuitemprivate.h"
#include "gtkaccellabel.h"
#include "deprecated/gtkactivatable.h"
#include "deprecated/gtktoggleaction.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "a11y/gtkcheckmenuitemaccessible.h"

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
 */


#define INDICATOR_SIZE 16

struct _GtkCheckMenuItemPrivate
{
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

static gint gtk_check_menu_item_draw                 (GtkWidget             *widget,
                                                      cairo_t               *cr);
static void gtk_check_menu_item_activate             (GtkMenuItem           *menu_item);
static void gtk_check_menu_item_toggle_size_request  (GtkMenuItem           *menu_item,
                                                      gint                  *requisition);
static void gtk_real_check_menu_item_draw_indicator  (GtkCheckMenuItem      *check_menu_item,
                                                      cairo_t               *cr);
static void gtk_check_menu_item_set_property         (GObject               *object,
                                                      guint                  prop_id,
                                                      const GValue          *value,
                                                      GParamSpec            *pspec);
static void gtk_check_menu_item_get_property         (GObject               *object,
                                                      guint                  prop_id,
                                                      GValue                *value,
                                                      GParamSpec            *pspec);

static void gtk_check_menu_item_activatable_interface_init (GtkActivatableIface  *iface);
static void gtk_check_menu_item_update                     (GtkActivatable       *activatable,
                                                            GtkAction            *action,
                                                            const gchar          *property_name);
static void gtk_check_menu_item_sync_action_properties     (GtkActivatable       *activatable,
                                                            GtkAction            *action);

static GtkActivatableIface *parent_activatable_iface;
static guint                check_menu_item_signals[LAST_SIGNAL] = { 0 };

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
G_DEFINE_TYPE_WITH_CODE (GtkCheckMenuItem, gtk_check_menu_item, GTK_TYPE_MENU_ITEM,
                         G_ADD_PRIVATE (GtkCheckMenuItem)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
                                                gtk_check_menu_item_activatable_interface_init))
G_GNUC_END_IGNORE_DEPRECATIONS;

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

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         P_("Active"),
                                                         P_("Whether the menu item is checked"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_INCONSISTENT,
                                   g_param_spec_boolean ("inconsistent",
                                                         P_("Inconsistent"),
                                                         P_("Whether to display an \"inconsistent\" state"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_DRAW_AS_RADIO,
                                   g_param_spec_boolean ("draw-as-radio",
                                                         P_("Draw as radio menu item"),
                                                         P_("Whether the menu item looks like a radio menu item"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));
  
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("indicator-size",
                                                             P_("Indicator Size"),
                                                             P_("Size of check or radio indicator"),
                                                             0,
                                                             G_MAXINT,
                                                             INDICATOR_SIZE,
                                                             GTK_PARAM_READABLE));

  widget_class->draw = gtk_check_menu_item_draw;

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_CHECK_MENU_ITEM_ACCESSIBLE);

  menu_item_class->activate = gtk_check_menu_item_activate;
  menu_item_class->hide_on_activate = FALSE;
  menu_item_class->toggle_size_request = gtk_check_menu_item_toggle_size_request;
  
  klass->toggled = NULL;
  klass->draw_indicator = gtk_real_check_menu_item_draw_indicator;

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
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void 
gtk_check_menu_item_activatable_interface_init (GtkActivatableIface  *iface)
{
  parent_activatable_iface = g_type_interface_peek_parent (iface);
  iface->update = gtk_check_menu_item_update;
  iface->sync_action_properties = gtk_check_menu_item_sync_action_properties;
}

static void
gtk_check_menu_item_update (GtkActivatable *activatable,
                            GtkAction      *action,
                            const gchar    *property_name)
{
  GtkCheckMenuItem *check_menu_item;
  gboolean use_action_appearance;

  check_menu_item = GTK_CHECK_MENU_ITEM (activatable);

  parent_activatable_iface->update (activatable, action, property_name);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (strcmp (property_name, "active") == 0)
    {
      gtk_action_block_activate (action);
      gtk_check_menu_item_set_active (check_menu_item, gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
      gtk_action_unblock_activate (action);
    }

  use_action_appearance = gtk_activatable_get_use_action_appearance (activatable);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (!use_action_appearance)
    return;

  if (strcmp (property_name, "draw-as-radio") == 0)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_check_menu_item_set_draw_as_radio (check_menu_item,
                                             gtk_toggle_action_get_draw_as_radio (GTK_TOGGLE_ACTION (action)));
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
}

static void
gtk_check_menu_item_sync_action_properties (GtkActivatable *activatable,
                                            GtkAction      *action)
{
  GtkCheckMenuItem *check_menu_item;
  gboolean use_action_appearance;
  gboolean is_toggle_action;

  check_menu_item = GTK_CHECK_MENU_ITEM (activatable);

  parent_activatable_iface->sync_action_properties (activatable, action);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  is_toggle_action = GTK_IS_TOGGLE_ACTION (action);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (!is_toggle_action)
    return;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_action_block_activate (action);

  gtk_check_menu_item_set_active (check_menu_item, gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
  gtk_action_unblock_activate (action);
  use_action_appearance = gtk_activatable_get_use_action_appearance (activatable);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (!use_action_appearance)
    return;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_check_menu_item_set_draw_as_radio (check_menu_item,
                                         gtk_toggle_action_get_draw_as_radio (GTK_TOGGLE_ACTION (action)));
  G_GNUC_END_IGNORE_DEPRECATIONS;
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
 * Sets the active state of the menu item's check box.
 */
void
gtk_check_menu_item_set_active (GtkCheckMenuItem *check_menu_item,
                                gboolean          is_active)
{
  GtkCheckMenuItemPrivate *priv;

  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item));

  priv = check_menu_item->priv;

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
 * Return value: %TRUE if the menu item is checked.
 */
gboolean
gtk_check_menu_item_get_active (GtkCheckMenuItem *check_menu_item)
{
  g_return_val_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item), FALSE);

  return check_menu_item->priv->active;
}

static void
gtk_check_menu_item_toggle_size_request (GtkMenuItem *menu_item,
                                         gint        *requisition)
{
  guint toggle_spacing;
  guint indicator_size;
  
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (menu_item));
  
  gtk_widget_style_get (GTK_WIDGET (menu_item),
                        "toggle-spacing", &toggle_spacing,
                        "indicator-size", &indicator_size,
                        NULL);

  *requisition = indicator_size + toggle_spacing;
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

/**
 * gtk_check_menu_item_set_inconsistent:
 * @check_menu_item: a #GtkCheckMenuItem
 * @setting: %TRUE to display an "inconsistent" third state check
 *
 * If the user has selected a range of elements (such as some text or
 * spreadsheet cells) that are affected by a boolean setting, and the
 * current values in that range are inconsistent, you may want to
 * display the check in an "in between" state. This function turns on
 * "in between" display.  Normally you would turn off the inconsistent
 * state again if the user explicitly selects a setting. This has to be
 * done manually, gtk_check_menu_item_set_inconsistent() only affects
 * visual appearance, it doesn't affect the semantics of the widget.
 * 
 **/
void
gtk_check_menu_item_set_inconsistent (GtkCheckMenuItem *check_menu_item,
                                      gboolean          setting)
{
  GtkCheckMenuItemPrivate *priv;

  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item));

  priv = check_menu_item->priv;
  
  setting = setting != FALSE;

  if (setting != priv->inconsistent)
    {
      priv->inconsistent = setting;
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
 * Return value: %TRUE if inconsistent
 **/
gboolean
gtk_check_menu_item_get_inconsistent (GtkCheckMenuItem *check_menu_item)
{
  g_return_val_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item), FALSE);

  return check_menu_item->priv->inconsistent;
}

/**
 * gtk_check_menu_item_set_draw_as_radio:
 * @check_menu_item: a #GtkCheckMenuItem
 * @draw_as_radio: whether @check_menu_item is drawn like a #GtkRadioMenuItem
 *
 * Sets whether @check_menu_item is drawn like a #GtkRadioMenuItem
 *
 * Since: 2.4
 **/
void
gtk_check_menu_item_set_draw_as_radio (GtkCheckMenuItem *check_menu_item,
                                       gboolean          draw_as_radio)
{
  GtkCheckMenuItemPrivate *priv;

  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item));

  priv = check_menu_item->priv;

  draw_as_radio = draw_as_radio != FALSE;

  if (draw_as_radio != priv->draw_as_radio)
    {
      priv->draw_as_radio = draw_as_radio;

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
 * Return value: Whether @check_menu_item looks like a #GtkRadioMenuItem
 * 
 * Since: 2.4
 **/
gboolean
gtk_check_menu_item_get_draw_as_radio (GtkCheckMenuItem *check_menu_item)
{
  g_return_val_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item), FALSE);
  
  return check_menu_item->priv->draw_as_radio;
}

static void
gtk_check_menu_item_init (GtkCheckMenuItem *check_menu_item)
{
  check_menu_item->priv = gtk_check_menu_item_get_instance_private (check_menu_item);
  check_menu_item->priv->active = FALSE;
}

static gint
gtk_check_menu_item_draw (GtkWidget *widget,
                          cairo_t   *cr)
{
  GtkCheckMenuItem *check_menu_item = GTK_CHECK_MENU_ITEM (widget);

  if (GTK_WIDGET_CLASS (gtk_check_menu_item_parent_class)->draw)
    GTK_WIDGET_CLASS (gtk_check_menu_item_parent_class)->draw (widget, cr);

  if (GTK_CHECK_MENU_ITEM_GET_CLASS (check_menu_item)->draw_indicator)
    GTK_CHECK_MENU_ITEM_GET_CLASS (check_menu_item)->draw_indicator (check_menu_item, cr);

  return FALSE;
}

static void
gtk_check_menu_item_activate (GtkMenuItem *menu_item)
{
  GtkCheckMenuItemPrivate *priv;

  GtkCheckMenuItem *check_menu_item = GTK_CHECK_MENU_ITEM (menu_item);
  priv = check_menu_item->priv;

  priv->active = !priv->active;

  gtk_check_menu_item_toggled (check_menu_item);
  gtk_widget_queue_draw (GTK_WIDGET (check_menu_item));

  GTK_MENU_ITEM_CLASS (gtk_check_menu_item_parent_class)->activate (menu_item);

  g_object_notify (G_OBJECT (check_menu_item), "active");
}

static void
gtk_real_check_menu_item_draw_indicator (GtkCheckMenuItem *check_menu_item,
                                         cairo_t          *cr)
{
  GtkCheckMenuItemPrivate *priv = check_menu_item->priv;
  GtkWidget *widget;
  gint x, y;

  widget = GTK_WIDGET (check_menu_item);

  if (gtk_widget_is_drawable (widget))
    {
      GtkAllocation allocation;
      GtkStyleContext *context;
      guint border_width;
      guint offset;
      guint toggle_size;
      guint toggle_spacing;
      guint horizontal_padding;
      guint indicator_size;
      GtkStateFlags state;
      GtkBorder padding;

      context = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);
      gtk_style_context_get_padding (context, state, &padding);

      gtk_widget_get_allocation (widget, &allocation);

      gtk_widget_style_get (widget,
                            "toggle-spacing", &toggle_spacing,
                            "horizontal-padding", &horizontal_padding,
                            "indicator-size", &indicator_size,
                            NULL);

      toggle_size = GTK_MENU_ITEM (check_menu_item)->priv->toggle_size;
      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
      offset = border_width + padding.left + 2;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
        {
          x = offset + horizontal_padding +
            (toggle_size - toggle_spacing - indicator_size) / 2;
        }
      else
        {
          x = allocation.width -
            offset - horizontal_padding - toggle_size + toggle_spacing +
            (toggle_size - toggle_spacing - indicator_size) / 2;
        }

      y = (allocation.height - indicator_size) / 2;

      gtk_style_context_save (context);

      if (priv->inconsistent)
        state |= GTK_STATE_FLAG_INCONSISTENT;
      else if (priv->active)
        state |= GTK_STATE_FLAG_ACTIVE;

      gtk_style_context_set_state (context, state);

      if (priv->draw_as_radio)
        {
          gtk_style_context_add_class (context, GTK_STYLE_CLASS_RADIO);
          gtk_render_option (context, cr, x, y,
                             indicator_size, indicator_size);
        }
      else
        {
          gtk_style_context_add_class (context, GTK_STYLE_CLASS_CHECK);
          gtk_render_check (context, cr, x, y,
                            indicator_size, indicator_size);
        }

      gtk_style_context_restore (context);
    }
}


static void
gtk_check_menu_item_get_property (GObject     *object,
                                  guint        prop_id,
                                  GValue      *value,
                                  GParamSpec  *pspec)
{
  GtkCheckMenuItem *checkitem = GTK_CHECK_MENU_ITEM (object);
  GtkCheckMenuItemPrivate *priv = checkitem->priv;
  
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
  GtkCheckMenuItemPrivate *priv = check_menu_item->priv;

  priv->active = is_active;
}
