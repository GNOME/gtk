/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2003 Ricardo Fernandez Pascual
 * Copyright (C) 2004 Paolo Borelli
 * Copyright (C) 2012 Bastien Nocera
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

/**
 * SECTION:gtkmenubutton
 * @short_description: A widget that shows a popup when clicked on
 * @title: GtkMenuButton
 *
 * The #GtkMenuButton widget is used to display a popup when clicked on.
 * This popup can be provided either as a #GtkMenu, a #GtkPopover or an
 * abstract #GMenuModel.
 *
 * The #GtkMenuButton widget can hold any valid child widget. That is, it
 * can hold almost any other standard #GtkWidget. The most commonly used
 * child is #GtkImage. If no widget is explicitely added to the #GtkMenuButton,
 * a #GtkImage is automatically created, using an arrow image oriented
 * according to #GtkMenuButton:direction or the generic “open-menu-symbolic”
 * icon if the direction is not set.
 *
 * The positioning of the popup is determined by the #GtkMenuButton:direction
 * property of the menu button.
 *
 * For menus, the #GtkWidget:halign and #GtkWidget:valign properties of the
 * menu are also taken into account. For example, when the direction is
 * %GTK_ARROW_DOWN and the horizontal alignment is %GTK_ALIGN_START, the
 * menu will be positioned below the button, with the starting edge
 * (depending on the text direction) of the menu aligned with the starting
 * edge of the button. If there is not enough space below the button, the
 * menu is popped up above the button instead. If the alignment would move
 * part of the menu offscreen, it is “pushed in”.
 *
 * ## Direction = Down
 *
 * - halign = start
 *
 *     ![](down-start.png)
 *
 * - halign = center
 *
 *     ![](down-center.png)
 *
 * - halign = end
 *
 *     ![](down-end.png)
 *
 * ## Direction = Up
 *
 * - halign = start
 *
 *     ![](up-start.png)
 *
 * - halign = center
 *
 *     ![](up-center.png)
 *
 * - halign = end
 *
 *     ![](up-end.png)
 *
 * ## Direction = Left
 *
 * - valign = start
 *
 *     ![](left-start.png)
 *
 * - valign = center
 *
 *     ![](left-center.png)
 *
 * - valign = end
 *
 *     ![](left-end.png)
 *
 * ## Direction = Right
 *
 * - valign = start
 *
 *     ![](right-start.png)
 *
 * - valign = center
 *
 *     ![](right-center.png)
 *
 * - valign = end
 *
 *     ![](right-end.png)
 *
 * # CSS nodes
 *
 * GtkMenuButton has a single CSS node with name button. To differentiate
 * it from a plain #GtkButton, it gets the .popup style class.
 */

#include "config.h"

#include "gtkmenubutton.h"
#include "gtkmenubuttonprivate.h"
#include "gtkbuttonprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwindow.h"
#include "gtkmain.h"
#include "gtkaccessible.h"
#include "gtkpopover.h"
#include "a11y/gtkmenubuttonaccessible.h"

#include "gtkprivate.h"
#include "gtkintl.h"

struct _GtkMenuButtonPrivate
{
  GtkWidget *menu;    /* The menu and the popover are mutually exclusive */
  GtkWidget *popover; /* Only one at a time can be set */
  GMenuModel *model;

  GtkMenuButtonShowMenuCallback func;
  gpointer user_data;

  GtkWidget *align_widget;
  GtkWidget *arrow_widget;
  GtkArrowType arrow_type;
  gboolean use_popover;
  guint press_handled : 1;
};

enum
{
  PROP_0,
  PROP_POPUP,
  PROP_MENU_MODEL,
  PROP_ALIGN_WIDGET,
  PROP_DIRECTION,
  PROP_USE_POPOVER,
  PROP_POPOVER,
  LAST_PROP
};

static GParamSpec *menu_button_props[LAST_PROP];

G_DEFINE_TYPE_WITH_PRIVATE (GtkMenuButton, gtk_menu_button, GTK_TYPE_TOGGLE_BUTTON)

static void gtk_menu_button_dispose (GObject *object);

static void
gtk_menu_button_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkMenuButton *self = GTK_MENU_BUTTON (object);

  switch (property_id)
    {
      case PROP_POPUP:
        gtk_menu_button_set_popup (self, g_value_get_object (value));
        break;
      case PROP_MENU_MODEL:
        gtk_menu_button_set_menu_model (self, g_value_get_object (value));
        break;
      case PROP_ALIGN_WIDGET:
        gtk_menu_button_set_align_widget (self, g_value_get_object (value));
        break;
      case PROP_DIRECTION:
        gtk_menu_button_set_direction (self, g_value_get_enum (value));
        break;
      case PROP_USE_POPOVER:
        gtk_menu_button_set_use_popover (self, g_value_get_boolean (value));
        break;
      case PROP_POPOVER:
        gtk_menu_button_set_popover (self, g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_menu_button_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkMenuButtonPrivate *priv = GTK_MENU_BUTTON (object)->priv;

  switch (property_id)
    {
      case PROP_POPUP:
        g_value_set_object (value, priv->menu);
        break;
      case PROP_MENU_MODEL:
        g_value_set_object (value, priv->model);
        break;
      case PROP_ALIGN_WIDGET:
        g_value_set_object (value, priv->align_widget);
        break;
      case PROP_DIRECTION:
        g_value_set_enum (value, priv->arrow_type);
        break;
      case PROP_USE_POPOVER:
        g_value_set_boolean (value, priv->use_popover);
        break;
      case PROP_POPOVER:
        g_value_set_object (value, priv->popover);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_menu_button_state_flags_changed (GtkWidget    *widget,
                                     GtkStateFlags previous_state_flags)
{
  GtkMenuButton *button = GTK_MENU_BUTTON (widget);
  GtkMenuButtonPrivate *priv = button->priv;

  if (!gtk_widget_is_sensitive (widget))
    {
      if (priv->menu)
        gtk_menu_shell_deactivate (GTK_MENU_SHELL (priv->menu));
      else if (priv->popover)
        gtk_widget_hide (priv->popover);
    }
}

static void
popup_menu (GtkMenuButton *menu_button,
            GdkEvent      *event)
{
  GtkMenuButtonPrivate *priv = menu_button->priv;
  GdkGravity widget_anchor = GDK_GRAVITY_SOUTH_WEST;
  GdkGravity menu_anchor = GDK_GRAVITY_NORTH_WEST;

  if (priv->func)
    priv->func (priv->user_data);

  if (!priv->menu)
    return;

  switch (priv->arrow_type)
    {
    case GTK_ARROW_UP:
      g_object_set (priv->menu,
                    "anchor-hints", (GDK_ANCHOR_FLIP_Y |
                                     GDK_ANCHOR_SLIDE |
                                     GDK_ANCHOR_RESIZE),
                    NULL);

      switch (gtk_widget_get_halign (priv->menu))
        {
        case GTK_ALIGN_FILL:
        case GTK_ALIGN_START:
        case GTK_ALIGN_BASELINE:
          widget_anchor = GDK_GRAVITY_NORTH_WEST;
          menu_anchor = GDK_GRAVITY_SOUTH_WEST;
          break;

        case GTK_ALIGN_END:
          widget_anchor = GDK_GRAVITY_NORTH_EAST;
          menu_anchor = GDK_GRAVITY_SOUTH_EAST;
          break;

        case GTK_ALIGN_CENTER:
          widget_anchor = GDK_GRAVITY_NORTH;
          menu_anchor = GDK_GRAVITY_SOUTH;
          break;
        }

      break;

    case GTK_ARROW_DOWN:
      /* In the common case the menu button is showing a dropdown menu, set the
       * corresponding type hint on the toplevel, so the WM can omit the top side
       * of the shadows.
       */
      g_object_set (priv->menu,
                    "anchor-hints", (GDK_ANCHOR_FLIP_Y |
                                     GDK_ANCHOR_SLIDE |
                                     GDK_ANCHOR_RESIZE),
                    "menu-type-hint", GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU,
                    NULL);

      switch (gtk_widget_get_halign (priv->menu))
        {
        case GTK_ALIGN_FILL:
        case GTK_ALIGN_START:
        case GTK_ALIGN_BASELINE:
          widget_anchor = GDK_GRAVITY_SOUTH_WEST;
          menu_anchor = GDK_GRAVITY_NORTH_WEST;
          break;

        case GTK_ALIGN_END:
          widget_anchor = GDK_GRAVITY_SOUTH_EAST;
          menu_anchor = GDK_GRAVITY_NORTH_EAST;
          break;

        case GTK_ALIGN_CENTER:
          widget_anchor = GDK_GRAVITY_SOUTH;
          menu_anchor = GDK_GRAVITY_NORTH;
          break;
        }

      break;

    case GTK_ARROW_LEFT:
      g_object_set (priv->menu,
                    "anchor-hints", (GDK_ANCHOR_FLIP_X |
                                     GDK_ANCHOR_SLIDE |
                                     GDK_ANCHOR_RESIZE),
                    NULL);

      switch (gtk_widget_get_valign (priv->menu))
        {
        case GTK_ALIGN_FILL:
        case GTK_ALIGN_START:
        case GTK_ALIGN_BASELINE:
          widget_anchor = GDK_GRAVITY_NORTH_WEST;
          menu_anchor = GDK_GRAVITY_NORTH_EAST;
          break;

        case GTK_ALIGN_END:
          widget_anchor = GDK_GRAVITY_SOUTH_WEST;
          menu_anchor = GDK_GRAVITY_SOUTH_EAST;
          break;

        case GTK_ALIGN_CENTER:
          widget_anchor = GDK_GRAVITY_WEST;
          menu_anchor = GDK_GRAVITY_EAST;
          break;
        }

      break;

    case GTK_ARROW_RIGHT:
      g_object_set (priv->menu,
                    "anchor-hints", (GDK_ANCHOR_FLIP_X |
                                     GDK_ANCHOR_SLIDE |
                                     GDK_ANCHOR_RESIZE),
                    NULL);

      switch (gtk_widget_get_valign (priv->menu))
        {
        case GTK_ALIGN_FILL:
        case GTK_ALIGN_START:
        case GTK_ALIGN_BASELINE:
          widget_anchor = GDK_GRAVITY_NORTH_EAST;
          menu_anchor = GDK_GRAVITY_NORTH_WEST;
          break;

        case GTK_ALIGN_END:
          widget_anchor = GDK_GRAVITY_SOUTH_EAST;
          menu_anchor = GDK_GRAVITY_SOUTH_WEST;
          break;

        case GTK_ALIGN_CENTER:
          widget_anchor = GDK_GRAVITY_EAST;
          menu_anchor = GDK_GRAVITY_WEST;
          break;
        }

      break;

    case GTK_ARROW_NONE:
      g_object_set (priv->menu,
                    "anchor-hints", (GDK_ANCHOR_FLIP_Y |
                                     GDK_ANCHOR_SLIDE |
                                     GDK_ANCHOR_RESIZE),
                    NULL);

      break;
    }

  gtk_menu_popup_at_widget (GTK_MENU (priv->menu),
                            GTK_WIDGET (menu_button),
                            widget_anchor,
                            menu_anchor,
                            event);
}

static void
gtk_menu_button_toggled (GtkToggleButton *button)
{
  GtkMenuButton *menu_button = GTK_MENU_BUTTON (button);
  GtkMenuButtonPrivate *priv = menu_button->priv;
  gboolean active = gtk_toggle_button_get_active (button);

  if (priv->menu)
    {
      if (active && !gtk_widget_get_visible (priv->menu))
        {
          GdkEvent *event;

          event = gtk_get_current_event ();

          popup_menu (menu_button, event);

          if (!event ||
              event->type == GDK_KEY_PRESS ||
              event->type == GDK_KEY_RELEASE)
            gtk_menu_shell_select_first (GTK_MENU_SHELL (priv->menu), FALSE);

          if (event)
            gdk_event_free (event);
        }
    }
  else if (priv->popover)
    {
      if (active)
        gtk_popover_popup (GTK_POPOVER (priv->popover));
      else
        gtk_popover_popdown (GTK_POPOVER (priv->popover));
    }

  if (GTK_TOGGLE_BUTTON_CLASS (gtk_menu_button_parent_class)->toggled)
    GTK_TOGGLE_BUTTON_CLASS (gtk_menu_button_parent_class)->toggled (button);
}

static void
gtk_menu_button_add (GtkContainer *container,
                     GtkWidget    *child)
{
  GtkMenuButton *button = GTK_MENU_BUTTON (container);

  if (button->priv->arrow_widget)
    gtk_container_remove (container, button->priv->arrow_widget);

  GTK_CONTAINER_CLASS (gtk_menu_button_parent_class)->add (container, child);
}

static void
gtk_menu_button_remove (GtkContainer *container,
                        GtkWidget    *child)
{
  GtkMenuButton *button = GTK_MENU_BUTTON (container);

  if (child == button->priv->arrow_widget)
    button->priv->arrow_widget = NULL;

  GTK_CONTAINER_CLASS (gtk_menu_button_parent_class)->remove (container, child);
}

static void
gtk_menu_button_class_init (GtkMenuButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkToggleButtonClass *toggle_button_class = GTK_TOGGLE_BUTTON_CLASS (klass);

  gobject_class->set_property = gtk_menu_button_set_property;
  gobject_class->get_property = gtk_menu_button_get_property;
  gobject_class->dispose = gtk_menu_button_dispose;

  widget_class->state_flags_changed = gtk_menu_button_state_flags_changed;

  container_class->add = gtk_menu_button_add;
  container_class->remove = gtk_menu_button_remove;

  toggle_button_class->toggled = gtk_menu_button_toggled;

  /**
   * GtkMenuButton:popup:
   *
   * The #GtkMenu that will be popped up when the button is clicked.
   *
   * Since: 3.6
   */
  menu_button_props[PROP_POPUP] =
      g_param_spec_object ("popup",
                           P_("Popup"),
                           P_("The dropdown menu."),
                           GTK_TYPE_MENU,
                           GTK_PARAM_READWRITE);

  /**
   * GtkMenuButton:menu-model:
   *
   * The #GMenuModel from which the popup will be created.
   * Depending on the #GtkMenuButton:use-popover property, that may
   * be a menu or a popover.
   *
   * See gtk_menu_button_set_menu_model() for the interaction with the
   * #GtkMenuButton:popup property.
   *
   * Since: 3.6
   */
  menu_button_props[PROP_MENU_MODEL] =
      g_param_spec_object ("menu-model",
                           P_("Menu model"),
                           P_("The model from which the popup is made."),
                           G_TYPE_MENU_MODEL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkMenuButton:align-widget:
   *
   * The #GtkWidget to use to align the menu with.
   *
   * Since: 3.6
   */
  menu_button_props[PROP_ALIGN_WIDGET] =
      g_param_spec_object ("align-widget",
                           P_("Align with"),
                           P_("The parent widget which the menu should align with."),
                           GTK_TYPE_CONTAINER,
                           GTK_PARAM_READWRITE);

  /**
   * GtkMenuButton:direction:
   *
   * The #GtkArrowType representing the direction in which the
   * menu or popover will be popped out.
   *
   * Since: 3.6
   */
  menu_button_props[PROP_DIRECTION] =
      g_param_spec_enum ("direction",
                         P_("Direction"),
                         P_("The direction the arrow should point."),
                         GTK_TYPE_ARROW_TYPE,
                         GTK_ARROW_DOWN,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkMenuButton:use-popover:
   *
   * Whether to construct a #GtkPopover from the menu model,
   * or a #GtkMenu.
   *
   * Since: 3.12
   */
  menu_button_props[PROP_USE_POPOVER] =
      g_param_spec_boolean ("use-popover",
                            P_("Use a popover"),
                            P_("Use a popover instead of a menu"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkMenuButton:popover:
   *
   * The #GtkPopover that will be popped up when the button is clicked.
   *
   * Since: 3.12
   */
  menu_button_props[PROP_POPOVER] =
      g_param_spec_object ("popover",
                           P_("Popover"),
                           P_("The popover"),
                           GTK_TYPE_POPOVER,
                           G_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, LAST_PROP, menu_button_props);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_MENU_BUTTON_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, "button");
}

static void
set_arrow_type (GtkImage     *image,
                GtkArrowType  arrow_type)
{
  switch (arrow_type)
    {
    case GTK_ARROW_NONE:
      gtk_image_set_from_icon_name (image, "open-menu-symbolic", GTK_ICON_SIZE_BUTTON);
      break;
    case GTK_ARROW_DOWN:
      gtk_image_set_from_icon_name (image, "pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
      break;
    case GTK_ARROW_UP:
      gtk_image_set_from_icon_name (image, "pan-up-symbolic", GTK_ICON_SIZE_BUTTON);
      break;
    case GTK_ARROW_LEFT:
      gtk_image_set_from_icon_name (image, "pan-start-symbolic", GTK_ICON_SIZE_BUTTON);
      break;
    case GTK_ARROW_RIGHT:
      gtk_image_set_from_icon_name (image, "pan-end-symbolic", GTK_ICON_SIZE_BUTTON);
      break;
    }
}

static void
add_arrow (GtkMenuButton *menu_button)
{
  GtkWidget *arrow;

  arrow = gtk_image_new ();
  set_arrow_type (GTK_IMAGE (arrow), menu_button->priv->arrow_type);
  gtk_container_add (GTK_CONTAINER (menu_button), arrow);
  gtk_widget_show (arrow);
  menu_button->priv->arrow_widget = arrow;
}

static void
gtk_menu_button_init (GtkMenuButton *menu_button)
{
  GtkMenuButtonPrivate *priv;
  GtkStyleContext *context;

  priv = gtk_menu_button_get_instance_private (menu_button);
  menu_button->priv = priv;
  priv->arrow_type = GTK_ARROW_DOWN;
  priv->use_popover = TRUE;

  add_arrow (menu_button);

  gtk_widget_set_focus_on_click (GTK_WIDGET (menu_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (menu_button), FALSE);

  context = gtk_widget_get_style_context (GTK_WIDGET (menu_button));
  gtk_style_context_add_class (context, "popup");
}

/**
 * gtk_menu_button_new:
 *
 * Creates a new #GtkMenuButton widget with downwards-pointing
 * arrow as the only child. You can replace the child widget
 * with another #GtkWidget should you wish to.
 *
 * Returns: The newly created #GtkMenuButton widget
 *
 * Since: 3.6
 */
GtkWidget *
gtk_menu_button_new (void)
{
  return g_object_new (GTK_TYPE_MENU_BUTTON, NULL);
}

/* Callback for the "deactivate" signal on the pop-up menu.
 * This is used so that we unset the state of the toggle button
 * when the pop-up menu disappears.
 * Also used for the "close" signal on the popover.
 */
static gboolean
menu_deactivate_cb (GtkMenuButton *menu_button)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (menu_button), FALSE);
  gtk_widget_unset_state_flags (GTK_WIDGET (menu_button), GTK_STATE_FLAG_PRELIGHT);

  return TRUE;
}

static void
menu_detacher (GtkWidget *widget,
               GtkMenu   *menu)
{
  GtkMenuButtonPrivate *priv = GTK_MENU_BUTTON (widget)->priv;

  g_return_if_fail (priv->menu == (GtkWidget *) menu);

  priv->menu = NULL;
}

static void
update_sensitivity (GtkMenuButton *menu_button)
{
  GtkMenuButtonPrivate *priv = menu_button->priv;

  if (GTK_BUTTON (menu_button)->priv->action_helper)
    return;

  gtk_widget_set_sensitive (GTK_WIDGET (menu_button),
                            priv->menu != NULL || priv->popover != NULL);
}

/* This function is used in GtkMenuToolButton, the call back will
 * be called when GtkMenuToolButton would have emitted the “show-menu”
 * signal.
 */
void
_gtk_menu_button_set_popup_with_func (GtkMenuButton                 *menu_button,
                                      GtkWidget                     *menu,
                                      GtkMenuButtonShowMenuCallback  func,
                                      gpointer                       user_data)
{
  GtkMenuButtonPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));
  g_return_if_fail (GTK_IS_MENU (menu) || menu == NULL);

  priv = menu_button->priv;
  priv->func = func;
  priv->user_data = user_data;

  if (priv->menu == GTK_WIDGET (menu))
    return;

  if (priv->menu)
    {
      if (gtk_widget_get_visible (priv->menu))
        gtk_menu_shell_deactivate (GTK_MENU_SHELL (priv->menu));

      g_signal_handlers_disconnect_by_func (priv->menu,
                                            menu_deactivate_cb,
                                            menu_button);
      gtk_menu_detach (GTK_MENU (priv->menu));
    }

  priv->menu = menu;

  if (priv->menu)
    {
      gtk_menu_attach_to_widget (GTK_MENU (priv->menu), GTK_WIDGET (menu_button),
                                 menu_detacher);

      gtk_widget_set_visible (priv->menu, FALSE);

      g_signal_connect_swapped (priv->menu, "deactivate",
                                G_CALLBACK (menu_deactivate_cb), menu_button);
    }

  update_sensitivity (menu_button);

  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_POPUP]);
  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_MENU_MODEL]);
}

/**
 * gtk_menu_button_set_popup:
 * @menu_button: a #GtkMenuButton
 * @menu: (nullable): a #GtkMenu, or %NULL to unset and disable the button
 *
 * Sets the #GtkMenu that will be popped up when the @menu_button is clicked, or
 * %NULL to dissociate any existing menu and disable the button.
 *
 * If #GtkMenuButton:menu-model or #GtkMenuButton:popover are set, those objects
 * are dissociated from the @menu_button, and those properties are set to %NULL.
 *
 * Since: 3.6
 */
void
gtk_menu_button_set_popup (GtkMenuButton *menu_button,
                           GtkWidget     *menu)
{
  GtkMenuButtonPrivate *priv = menu_button->priv;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));
  g_return_if_fail (GTK_IS_MENU (menu) || menu == NULL);

  g_object_freeze_notify (G_OBJECT (menu_button));

  g_clear_object (&priv->model);

  _gtk_menu_button_set_popup_with_func (menu_button, menu, NULL, NULL);

  if (menu && priv->popover)
    gtk_menu_button_set_popover (menu_button, NULL);

  update_sensitivity (menu_button);

  g_object_thaw_notify (G_OBJECT (menu_button));
}

/**
 * gtk_menu_button_get_popup:
 * @menu_button: a #GtkMenuButton
 *
 * Returns the #GtkMenu that pops out of the button.
 * If the button does not use a #GtkMenu, this function
 * returns %NULL.
 *
 * Returns: (nullable) (transfer none): a #GtkMenu or %NULL
 *
 * Since: 3.6
 */
GtkMenu *
gtk_menu_button_get_popup (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), NULL);

  return GTK_MENU (menu_button->priv->menu);
}

/**
 * gtk_menu_button_set_menu_model:
 * @menu_button: a #GtkMenuButton
 * @menu_model: (nullable): a #GMenuModel, or %NULL to unset and disable the
 *   button
 *
 * Sets the #GMenuModel from which the popup will be constructed,
 * or %NULL to dissociate any existing menu model and disable the button.
 *
 * Depending on the value of #GtkMenuButton:use-popover, either a
 * #GtkMenu will be created with gtk_menu_new_from_model(), or a
 * #GtkPopover with gtk_popover_new_from_model(). In either case,
 * actions will be connected as documented for these functions.
 *
 * If #GtkMenuButton:popup or #GtkMenuButton:popover are already set, those
 * widgets are dissociated from the @menu_button, and those properties are set
 * to %NULL.
 *
 * Since: 3.6
 */
void
gtk_menu_button_set_menu_model (GtkMenuButton *menu_button,
                                GMenuModel    *menu_model)
{
  GtkMenuButtonPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));
  g_return_if_fail (G_IS_MENU_MODEL (menu_model) || menu_model == NULL);

  priv = menu_button->priv;

  g_object_freeze_notify (G_OBJECT (menu_button));

  if (menu_model)
    g_object_ref (menu_model);

  if (menu_model)
    {
      if (priv->use_popover)
        {
          GtkWidget *popover;

          popover = gtk_popover_new_from_model (GTK_WIDGET (menu_button), menu_model);
          gtk_menu_button_set_popover (menu_button, popover);
        }
      else
        {
          GtkWidget *menu;

          menu = gtk_menu_new_from_model (menu_model);
          gtk_widget_show_all (menu);
          gtk_menu_button_set_popup (menu_button, menu);
        }
    }
  else
    {
      gtk_menu_button_set_popup (menu_button, NULL);
      gtk_menu_button_set_popover (menu_button, NULL);
    }

  priv->model = menu_model;
  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_MENU_MODEL]);

  g_object_thaw_notify (G_OBJECT (menu_button));
}

/**
 * gtk_menu_button_get_menu_model:
 * @menu_button: a #GtkMenuButton
 *
 * Returns the #GMenuModel used to generate the popup.
 *
 * Returns: (nullable) (transfer none): a #GMenuModel or %NULL
 *
 * Since: 3.6
 */
GMenuModel *
gtk_menu_button_get_menu_model (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), NULL);

  return menu_button->priv->model;
}

static void
set_align_widget_pointer (GtkMenuButton *menu_button,
                          GtkWidget     *align_widget)
{
  GtkMenuButtonPrivate *priv;

  priv = menu_button->priv;

  if (priv->align_widget)
    g_object_remove_weak_pointer (G_OBJECT (priv->align_widget), (gpointer *) &priv->align_widget);

  priv->align_widget = align_widget;

  if (priv->align_widget)
    g_object_add_weak_pointer (G_OBJECT (priv->align_widget), (gpointer *) &priv->align_widget);
}

/**
 * gtk_menu_button_set_align_widget:
 * @menu_button: a #GtkMenuButton
 * @align_widget: (allow-none): a #GtkWidget
 *
 * Sets the #GtkWidget to use to line the menu with when popped up.
 * Note that the @align_widget must contain the #GtkMenuButton itself.
 *
 * Setting it to %NULL means that the menu will be aligned with the
 * button itself.
 *
 * Note that this property is only used with menus currently,
 * and not for popovers.
 *
 * Since: 3.6
 */
void
gtk_menu_button_set_align_widget (GtkMenuButton *menu_button,
                                  GtkWidget     *align_widget)
{
  GtkMenuButtonPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));
  g_return_if_fail (align_widget == NULL || gtk_widget_is_ancestor (GTK_WIDGET (menu_button), align_widget));

  priv = menu_button->priv;
  if (priv->align_widget == align_widget)
    return;

  set_align_widget_pointer (menu_button, align_widget);

  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_ALIGN_WIDGET]);
}

/**
 * gtk_menu_button_get_align_widget:
 * @menu_button: a #GtkMenuButton
 *
 * Returns the parent #GtkWidget to use to line up with menu.
 *
 * Returns: (nullable) (transfer none): a #GtkWidget value or %NULL
 *
 * Since: 3.6
 */
GtkWidget *
gtk_menu_button_get_align_widget (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), NULL);

  return menu_button->priv->align_widget;
}

static void
update_popover_direction (GtkMenuButton *menu_button)
{
  GtkMenuButtonPrivate *priv = menu_button->priv;

  if (!priv->popover)
    return;

  switch (priv->arrow_type)
    {
    case GTK_ARROW_UP:
      gtk_popover_set_position (GTK_POPOVER (priv->popover), GTK_POS_TOP);
      break;
    case GTK_ARROW_DOWN:
    case GTK_ARROW_NONE:
      gtk_popover_set_position (GTK_POPOVER (priv->popover), GTK_POS_BOTTOM);
      break;
    case GTK_ARROW_LEFT:
      gtk_popover_set_position (GTK_POPOVER (priv->popover), GTK_POS_LEFT);
      break;
    case GTK_ARROW_RIGHT:
      gtk_popover_set_position (GTK_POPOVER (priv->popover), GTK_POS_RIGHT);
      break;
    }
}

static void
popover_destroy_cb (GtkMenuButton *menu_button)
{
  gtk_menu_button_set_popover (menu_button, NULL);
}

/**
 * gtk_menu_button_set_direction:
 * @menu_button: a #GtkMenuButton
 * @direction: a #GtkArrowType
 *
 * Sets the direction in which the popup will be popped up, as
 * well as changing the arrow’s direction. The child will not
 * be changed to an arrow if it was customized.
 *
 * If the does not fit in the available space in the given direction,
 * GTK+ will its best to keep it inside the screen and fully visible.
 *
 * If you pass %GTK_ARROW_NONE for a @direction, the popup will behave
 * as if you passed %GTK_ARROW_DOWN (although you won’t see any arrows).
 *
 * Since: 3.6
 */
void
gtk_menu_button_set_direction (GtkMenuButton *menu_button,
                               GtkArrowType   direction)
{
  GtkMenuButtonPrivate *priv = menu_button->priv;
  GtkWidget *child;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  if (priv->arrow_type == direction)
    return;

  priv->arrow_type = direction;
  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_DIRECTION]);

  /* Is it custom content? We don't change that */
  child = gtk_bin_get_child (GTK_BIN (menu_button));
  if (priv->arrow_widget != child)
    return;

  set_arrow_type (GTK_IMAGE (child), priv->arrow_type);
  update_popover_direction (menu_button);
}

/**
 * gtk_menu_button_get_direction:
 * @menu_button: a #GtkMenuButton
 *
 * Returns the direction the popup will be pointing at when popped up.
 *
 * Returns: a #GtkArrowType value
 *
 * Since: 3.6
 */
GtkArrowType
gtk_menu_button_get_direction (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), GTK_ARROW_DOWN);

  return menu_button->priv->arrow_type;
}

static void
gtk_menu_button_dispose (GObject *object)
{
  GtkMenuButtonPrivate *priv = GTK_MENU_BUTTON (object)->priv;

  if (priv->menu)
    {
      g_signal_handlers_disconnect_by_func (priv->menu,
                                            menu_deactivate_cb,
                                            object);
      gtk_menu_detach (GTK_MENU (priv->menu));
      priv->menu = NULL;
    }

  if (priv->popover)
    {
      g_signal_handlers_disconnect_by_func (priv->popover,
                                            menu_deactivate_cb,
                                            object);
      g_signal_handlers_disconnect_by_func (priv->popover,
                                            popover_destroy_cb,
                                            object);
      gtk_popover_set_relative_to (GTK_POPOVER (priv->popover), NULL);
      priv->popover = NULL;
    }

  set_align_widget_pointer (GTK_MENU_BUTTON (object), NULL);

  g_clear_object (&priv->model);

  G_OBJECT_CLASS (gtk_menu_button_parent_class)->dispose (object);
}

/**
 * gtk_menu_button_set_use_popover:
 * @menu_button: a #GtkMenuButton
 * @use_popover: %TRUE to construct a popover from the menu model
 *
 * Sets whether to construct a #GtkPopover instead of #GtkMenu
 * when gtk_menu_button_set_menu_model() is called. Note that
 * this property is only consulted when a new menu model is set.
 *
 * Since: 3.12
 */
void
gtk_menu_button_set_use_popover (GtkMenuButton *menu_button,
                                 gboolean       use_popover)
{
  GtkMenuButtonPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  priv = menu_button->priv;

  use_popover = use_popover != FALSE;

  if (priv->use_popover == use_popover)
    return;

  priv->use_popover = use_popover;

  g_object_freeze_notify (G_OBJECT (menu_button));

  if (priv->model)
    gtk_menu_button_set_menu_model (menu_button, priv->model);

  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_USE_POPOVER]);

  g_object_thaw_notify (G_OBJECT (menu_button));
}

/**
 * gtk_menu_button_get_use_popover:
 * @menu_button: a #GtkMenuButton
 *
 * Returns whether a #GtkPopover or a #GtkMenu will be constructed
 * from the menu model.
 *
 * Returns: %TRUE if using a #GtkPopover
 *
 * Since: 3.12
 */
gboolean
gtk_menu_button_get_use_popover (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), FALSE);

  return menu_button->priv->use_popover;
}

/**
 * gtk_menu_button_set_popover:
 * @menu_button: a #GtkMenuButton
 * @popover: (nullable): a #GtkPopover, or %NULL to unset and disable the button
 *
 * Sets the #GtkPopover that will be popped up when the @menu_button is clicked,
 * or %NULL to dissociate any existing popover and disable the button.
 *
 * If #GtkMenuButton:menu-model or #GtkMenuButton:popup are set, those objects
 * are dissociated from the @menu_button, and those properties are set to %NULL.
 *
 * Since: 3.12
 */
void
gtk_menu_button_set_popover (GtkMenuButton *menu_button,
                             GtkWidget     *popover)
{
  GtkMenuButtonPrivate *priv = menu_button->priv;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));
  g_return_if_fail (GTK_IS_POPOVER (popover) || popover == NULL);

  g_object_freeze_notify (G_OBJECT (menu_button));

  g_clear_object (&priv->model);

  if (priv->popover)
    {
      if (gtk_widget_get_visible (priv->popover))
        gtk_widget_hide (priv->popover);

      g_signal_handlers_disconnect_by_func (priv->popover,
                                            menu_deactivate_cb,
                                            menu_button);
      g_signal_handlers_disconnect_by_func (priv->popover,
                                            popover_destroy_cb,
                                            menu_button);

      gtk_popover_set_relative_to (GTK_POPOVER (priv->popover), NULL);
    }

  priv->popover = popover;

  if (popover)
    {
      gtk_popover_set_relative_to (GTK_POPOVER (priv->popover), GTK_WIDGET (menu_button));
      g_signal_connect_swapped (priv->popover, "closed",
                                G_CALLBACK (menu_deactivate_cb), menu_button);
      g_signal_connect_swapped (priv->popover, "destroy",
                                G_CALLBACK (popover_destroy_cb), menu_button);
      update_popover_direction (menu_button);
      gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (menu_button)), "menu-button");
    }

  if (popover && priv->menu)
    gtk_menu_button_set_popup (menu_button, NULL);

  update_sensitivity (menu_button);

  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_POPOVER]);
  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_MENU_MODEL]);
  g_object_thaw_notify (G_OBJECT (menu_button));
}

/**
 * gtk_menu_button_get_popover:
 * @menu_button: a #GtkMenuButton
 *
 * Returns the #GtkPopover that pops out of the button.
 * If the button is not using a #GtkPopover, this function
 * returns %NULL.
 *
 * Returns: (nullable) (transfer none): a #GtkPopover or %NULL
 *
 * Since: 3.12
 */
GtkPopover *
gtk_menu_button_get_popover (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), NULL);

  return GTK_POPOVER (menu_button->priv->popover);
}
