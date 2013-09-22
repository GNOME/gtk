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
 * @short_description: A widget that shows a menu when clicked on
 * @title: GtkMenuButton
 *
 * The #GtkMenuButton widget is used to display a menu when clicked on.
 * This menu can be provided either as a #GtkMenu, or an abstract #GMenuModel.
 *
 * The #GtkMenuButton widget can hold any valid child widget. That is, it
 * can hold almost any other standard #GtkWidget. The most commonly used
 * child is the provided #GtkArrow.
 *
 * The positioning of the menu is determined by the #GtkMenuButton:direction
 * property of the menu button and the #GtkWidget:halign or #GtkWidget:valign
 * properties of the menu. For example, when the direction is %GTK_ARROW_DOWN
 * and the horizontal alignment is %GTK_ALIGN_START, the menu will be
 * positioned below the button, with the starting edge (depending on the
 * text direction) of the menu aligned with the starting edge of the button.
 * If there is not enough space below the button, the menu is popped up above
 * the button instead. If the alignment would move part of the menu offscreen,
 * it is 'pushed in'.
 *
 * <informaltable>
 *   <tgroup cols="4">
 *     <tbody>
 *       <row>
 *         <entry></entry>
 *         <entry>halign = start</entry>
 *         <entry>halign = center</entry>
 *         <entry>halign = end</entry>
 *       </row>
 *     <row>
 *       <entry>direction = down</entry>
 *       <entry>
 *         <inlinemediaobject>
 *           <imageobject><imagedata fileref="down-start.png" format="PNG"/></imageobject>
 *         </inlinemediaobject>
 *       </entry>
 *       <entry>
 *         <inlinemediaobject>
 *           <imageobject><imagedata fileref="down-center.png" format="PNG"/></imageobject>
 *         </inlinemediaobject>
 *       </entry>
 *       <entry>
 *         <inlinemediaobject>
 *           <imageobject><imagedata fileref="down-end.png" format="PNG"/></imageobject>
 *         </inlinemediaobject>
 *       </entry>
 *     </row>
 *     <row>
 *       <entry>direction = up</entry>
 *       <entry>
 *         <inlinemediaobject>
 *           <imageobject><imagedata fileref="up-start.png" format="PNG"/></imageobject>
 *         </inlinemediaobject>
 *       </entry>
 *       <entry>
 *         <inlinemediaobject>
 *           <imageobject><imagedata fileref="up-center.png" format="PNG"/></imageobject>
 *         </inlinemediaobject>
 *       </entry>
 *       <entry>
 *         <inlinemediaobject>
 *           <imageobject><imagedata fileref="up-end.png" format="PNG"/></imageobject>
 *         </inlinemediaobject>
 *       </entry>
 *      </row>
 *     </tbody>
 *   </tgroup>
 * </informaltable>
 * <informaltable>
 *   <tgroup cols="3">
 *     <tbody>
 *       <row>
 *         <entry></entry>
 *         <entry>direction = left</entry>
 *         <entry>direction = right</entry>
 *       </row>
 *     <row>
 *       <entry>valign = start</entry>
 *       <entry>
 *         <inlinemediaobject>
 *           <imageobject><imagedata fileref="left-start.png" format="PNG"/></imageobject>
 *         </inlinemediaobject>
 *       </entry>
 *       <entry>
 *         <inlinemediaobject>
 *           <imageobject><imagedata fileref="right-start.png" format="PNG"/></imageobject>
 *         </inlinemediaobject>
 *       </entry>
 *     </row>
 *     <row>
 *       <entry>valign = center</entry>
 *       <entry>
 *         <inlinemediaobject>
 *           <imageobject><imagedata fileref="left-center.png" format="PNG"/></imageobject>
 *         </inlinemediaobject>
 *       </entry>
 *       <entry>
 *         <inlinemediaobject>
 *           <imageobject><imagedata fileref="right-center.png" format="PNG"/></imageobject>
 *         </inlinemediaobject>
 *       </entry>
 *     </row>
 *     <row>
 *       <entry>valign = end</entry>
 *       <entry>
 *         <inlinemediaobject>
 *           <imageobject><imagedata fileref="left-end.png" format="PNG"/></imageobject>
 *         </inlinemediaobject>
 *       </entry>
 *       <entry>
 *         <inlinemediaobject>
 *           <imageobject><imagedata fileref="right-end.png" format="PNG"/></imageobject>
 *         </inlinemediaobject>
 *       </entry>
 *      </row>
 *     </tbody>
 *   </tgroup>
 * </informaltable>
 */

#include "config.h"

#include "gtkmenubutton.h"
#include "gtkmenubuttonprivate.h"
#include "gtkarrow.h"
#include "gtktypebuiltins.h"
#include "gtkwindow.h"
#include "gtkmain.h"

#include "gtkprivate.h"
#include "gtkintl.h"

struct _GtkMenuButtonPrivate
{
  GtkWidget *popup;
  GMenuModel *model;

  GtkMenuButtonShowMenuCallback func;
  gpointer user_data;

  GtkWidget *align_widget;
  gpointer arrow_widget;
  GtkArrowType arrow_type;
};

enum
{
  PROP_0,
  PROP_POPUP,
  PROP_MODEL,
  PROP_ALIGN_WIDGET,
  PROP_DIRECTION
};

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
      case PROP_MODEL:
        gtk_menu_button_set_menu_model (self, g_value_get_object (value));
        break;
      case PROP_ALIGN_WIDGET:
        gtk_menu_button_set_align_widget (self, g_value_get_object (value));
        break;
      case PROP_DIRECTION:
        gtk_menu_button_set_direction (self, g_value_get_enum (value));
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
        g_value_set_object (value, priv->popup);
        break;
      case PROP_MODEL:
        g_value_set_object (value, priv->model);
        break;
      case PROP_ALIGN_WIDGET:
        g_value_set_object (value, priv->align_widget);
        break;
      case PROP_DIRECTION:
        g_value_set_enum (value, priv->arrow_type);
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

  if (!gtk_widget_is_sensitive (widget) && priv->popup)
    gtk_menu_shell_deactivate (GTK_MENU_SHELL (priv->popup));
}

static void
menu_position_up_down_func (GtkMenu       *menu,
                            gint          *x,
                            gint          *y,
                            gboolean      *push_in,
                            GtkMenuButton *menu_button)
{
  GtkMenuButtonPrivate *priv = menu_button->priv;
  GtkWidget *widget = GTK_WIDGET (menu_button);
  GtkWidget *toplevel;
  GtkRequisition menu_req;
  GtkTextDirection direction;
  GdkRectangle monitor;
  gint monitor_num;
  GdkScreen *screen;
  GdkWindow *window;
  GtkAllocation allocation, arrow_allocation;
  GtkAlign align;

  /* In the common case the menu button is showing a dropdown menu, set the
   * corresponding type hint on the toplevel, so the WM can omit the top side
   * of the shadows.
   */
  if (priv->arrow_type == GTK_ARROW_DOWN)
    {
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->popup));
      gtk_window_set_type_hint (GTK_WINDOW (toplevel), GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU);
    }

  gtk_widget_get_preferred_size (GTK_WIDGET (priv->popup),
                                 &menu_req, NULL);

  align = gtk_widget_get_halign (GTK_WIDGET (priv->popup));
  direction = gtk_widget_get_direction (widget);
  window = gtk_widget_get_window (priv->align_widget ? priv->align_widget : widget);

  screen = gtk_widget_get_screen (GTK_WIDGET (menu));
  monitor_num = gdk_screen_get_monitor_at_window (screen, window);
  if (monitor_num < 0)
    monitor_num = 0;
  gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);

  gtk_widget_get_allocation (priv->align_widget ? priv->align_widget : widget, &allocation);
  gtk_widget_get_allocation (widget, &arrow_allocation);

  gdk_window_get_origin (window, x, y);
  *x += allocation.x;
  *y += allocation.y;

  /* treat the default align value like START */
  if (align == GTK_ALIGN_FILL)
    align = GTK_ALIGN_START;

  if (align == GTK_ALIGN_CENTER)
    *x -= (menu_req.width - allocation.width) / 2;
  else if ((align == GTK_ALIGN_START && direction == GTK_TEXT_DIR_LTR) ||
           (align == GTK_ALIGN_END && direction == GTK_TEXT_DIR_RTL))
    *x += MAX (allocation.width - menu_req.width, 0);
  else if (menu_req.width > allocation.width)
    *x -= menu_req.width - allocation.width;

  if (priv->arrow_type == GTK_ARROW_UP && *y - menu_req.height >= monitor.y)
    {
      *y -= menu_req.height;
    }
  else
    {
      if ((*y + arrow_allocation.height + menu_req.height) <= monitor.y + monitor.height)
        *y += arrow_allocation.height;
      else if ((*y - menu_req.height) >= monitor.y)
        *y -= menu_req.height;
      else if (monitor.y + monitor.height - (*y + arrow_allocation.height) > *y)
        *y += arrow_allocation.height;
      else
        *y -= menu_req.height;
    }

  *push_in = FALSE;
}

static void
menu_position_side_func (GtkMenu       *menu,
                         gint          *x,
                         gint          *y,
                         gboolean      *push_in,
                         GtkMenuButton *menu_button)
{
  GtkMenuButtonPrivate *priv = menu_button->priv;
  GtkAllocation allocation;
  GtkWidget *widget = GTK_WIDGET (menu_button);
  GtkRequisition menu_req;
  GdkRectangle monitor;
  gint monitor_num;
  GdkScreen *screen;
  GdkWindow *window;
  GtkAlign align;
  GtkTextDirection direction;

  gtk_widget_get_preferred_size (GTK_WIDGET (priv->popup),
                                 &menu_req, NULL);

  window = gtk_widget_get_window (widget);

  direction = gtk_widget_get_direction (widget);
  align = gtk_widget_get_valign (GTK_WIDGET (menu));
  screen = gtk_widget_get_screen (GTK_WIDGET (menu));
  monitor_num = gdk_screen_get_monitor_at_window (screen, window);
  if (monitor_num < 0)
    monitor_num = 0;
  gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);

  gdk_window_get_origin (gtk_button_get_event_window (GTK_BUTTON (menu_button)), x, y);

  gtk_widget_get_allocation (widget, &allocation);

  if ((priv->arrow_type == GTK_ARROW_RIGHT && direction == GTK_TEXT_DIR_LTR) ||
      (priv->arrow_type == GTK_ARROW_LEFT && direction == GTK_TEXT_DIR_RTL))

    {
      if (*x + allocation.width + menu_req.width <= monitor.x + monitor.width)
        *x += allocation.width;
      else
        *x -= menu_req.width;
    }
  else
    {
      if (*x - menu_req.width >= monitor.x)
        *x -= menu_req.width;
      else
        *x += allocation.width;
    }

  /* treat the default align value like START */
  if (align == GTK_ALIGN_FILL)
    align = GTK_ALIGN_START;

  if (align == GTK_ALIGN_CENTER)
    *y -= (menu_req.height - allocation.height) / 2;
  else if (align == GTK_ALIGN_END)
    *y -= menu_req.height - allocation.height;

  *push_in = FALSE;
}

static void
popup_menu (GtkMenuButton  *menu_button,
            GdkEventButton *event)
{
  GtkMenuButtonPrivate *priv = menu_button->priv;
  GtkMenuPositionFunc func;

  if (priv->func)
    priv->func (priv->user_data);

  if (!priv->popup)
    return;

  switch (priv->arrow_type)
    {
      case GTK_ARROW_LEFT:
      case GTK_ARROW_RIGHT:
        func = (GtkMenuPositionFunc) menu_position_side_func;
        break;
      default:
        func = (GtkMenuPositionFunc) menu_position_up_down_func;
        break;
  }

  gtk_menu_popup_for_device (GTK_MENU (priv->popup),
                             event ? event->device : NULL,
                             NULL, NULL,
                             func,
                             GTK_WIDGET (menu_button),
                             NULL,
                             event ? event->button : 0,
                             event ? event->time : gtk_get_current_event_time ());
}

static void
gtk_menu_button_toggled (GtkToggleButton *button)
{
  GtkMenuButton *menu_button = GTK_MENU_BUTTON (button);
  GtkMenuButtonPrivate *priv = menu_button->priv;

  if (!priv->popup)
    return;

  if (gtk_toggle_button_get_active (button) &&
      !gtk_widget_get_visible (GTK_WIDGET (priv->popup)))
    {
      /* we get here only when the menu is activated by a key
       * press, so that we can select the first menu item
       */
      popup_menu (menu_button, NULL);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (priv->popup), FALSE);
    }
}

static gboolean
gtk_menu_button_button_press_event (GtkWidget      *widget,
                                    GdkEventButton *event)
{
  if (event->button == GDK_BUTTON_PRIMARY)
    {
      popup_menu (GTK_MENU_BUTTON (widget), event);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

      return TRUE;
    }

  return GTK_WIDGET_CLASS (gtk_menu_button_parent_class)->button_press_event (widget, event);
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
  widget_class->button_press_event = gtk_menu_button_button_press_event;

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
  g_object_class_install_property (gobject_class,
                                   PROP_POPUP,
                                   g_param_spec_object ("popup",
                                                        P_("popup"),
                                                        P_("The dropdown menu."),
                                                        GTK_TYPE_MENU,
                                                        G_PARAM_READWRITE));

  /**
   * GtkMenuButton:menu-model:
   *
   * The #GMenuModel from which the menu to pop up will be created.
   * See gtk_menu_button_set_menu_model() for the interaction with the
   * #GtkMenuButton:popup property.
   *
   * Since: 3.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("menu-model",
                                                        P_("menu-model"),
                                                        P_("The dropdown menu's model."),
                                                        G_TYPE_MENU_MODEL,
                                                        G_PARAM_READWRITE));
  /**
   * GtkMenuButton:align-widget:
   *
   * The #GtkWidget to use to align the popup menu with.
   *
   * Since: 3.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ALIGN_WIDGET,
                                   g_param_spec_object ("align-widget",
                                                        P_("align-widget"),
                                                        P_("The parent widget which the menu should align with."),
                                                        GTK_TYPE_CONTAINER,
                                                        G_PARAM_READWRITE));
  /**
   * GtkMenuButton:direction:
   *
   * The #GtkArrowType representing the direction in which the
   * menu will be popped out.
   *
   * Since: 3.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_DIRECTION,
                                   g_param_spec_enum ("direction",
                                                      P_("direction"),
                                                      P_("The direction the arrow should point."),
                                                      GTK_TYPE_ARROW_TYPE,
                                                      GTK_ARROW_DOWN,
                                                      G_PARAM_READWRITE));
}

static void
add_arrow (GtkMenuButton *menu_button)
{
  GtkWidget *arrow;

  arrow = gtk_arrow_new (menu_button->priv->arrow_type, GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (menu_button), arrow);
  gtk_widget_show (arrow);
  menu_button->priv->arrow_widget = arrow;
}

static void
gtk_menu_button_init (GtkMenuButton *menu_button)
{
  GtkMenuButtonPrivate *priv;

  priv = gtk_menu_button_get_instance_private (menu_button);
  menu_button->priv = priv;
  priv->arrow_type = GTK_ARROW_DOWN;

  add_arrow (menu_button);

  gtk_widget_set_sensitive (GTK_WIDGET (menu_button), FALSE);
}

/**
 * gtk_menu_button_new:
 *
 * Creates a new #GtkMenuButton widget with downwards-pointing
 * arrow as the only child. You can replace the child widget
 * with another #GtkWidget should you wish to.
 *
 * Returns: The newly created #GtkMenuButton widget.
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
 */
static int
menu_deactivate_cb (GtkMenuShell  *menu_shell,
                    GtkMenuButton *menu_button)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (menu_button), FALSE);

  return TRUE;
}

static void
menu_detacher (GtkWidget *widget,
               GtkMenu   *menu)
{
  GtkMenuButtonPrivate *priv = GTK_MENU_BUTTON (widget)->priv;

  g_return_if_fail (priv->popup == (GtkWidget *) menu);

  priv->popup = NULL;
}

/* This function is used in GtkMenuToolButton, the call back will
 * be called when GtkMenuToolButton would have emitted the "show-menu"
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

  if (priv->popup == GTK_WIDGET (menu))
    return;

  if (priv->popup)
    {
      if (gtk_widget_get_visible (GTK_WIDGET (priv->popup)))
        gtk_menu_shell_deactivate (GTK_MENU_SHELL (priv->popup));

      g_signal_handlers_disconnect_by_func (priv->popup,
                                            menu_deactivate_cb,
                                            menu_button);
      gtk_menu_detach (GTK_MENU (priv->popup));
    }

  priv->popup = menu;

  if (priv->popup)
    {
      gtk_menu_attach_to_widget (GTK_MENU (priv->popup), GTK_WIDGET (menu_button),
                                 menu_detacher);

      gtk_widget_set_visible (priv->popup, FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (menu_button), TRUE);

      g_signal_connect (priv->popup, "deactivate",
                        G_CALLBACK (menu_deactivate_cb), menu_button);
    }
  else
    {
      gtk_widget_set_sensitive (GTK_WIDGET (menu_button), FALSE);
    }

  g_object_notify (G_OBJECT (menu_button), "popup");
  g_object_notify (G_OBJECT (menu_button), "menu-model");
}

/**
 * gtk_menu_button_set_popup:
 * @menu_button: a #GtkMenuButton
 * @popup: (allow-none): a #GtkMenu
 *
 * Sets the #GtkMenu that will be popped up when the button is clicked,
 * or %NULL to disable the button. If #GtkMenuButton:menu-model is set,
 * it will be set to %NULL.
 *
 * Since: 3.6
 */
void
gtk_menu_button_set_popup (GtkMenuButton *menu_button,
                           GtkWidget     *popup)
{
  GtkMenuButtonPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));
  g_return_if_fail (GTK_IS_MENU (popup) || popup == NULL);

  priv = menu_button->priv;
  g_clear_object (&priv->model);

  _gtk_menu_button_set_popup_with_func (menu_button, popup, NULL, NULL);
}

/**
 * gtk_menu_button_get_popup:
 * @menu_button: a #GtkMenuButton
 *
 * Returns the #GtkMenu that pops out of the button.
 *
 * Returns: (transfer none): a #GtkMenu or %NULL.
 *
 * Since: 3.6
 */
GtkMenu *
gtk_menu_button_get_popup (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), NULL);

  return GTK_MENU (menu_button->priv->popup);
}

/**
 * gtk_menu_button_set_menu_model:
 * @menu_button: a #GtkMenuButton
 * @menu_model: (allow-none): a #GMenuModel
 *
 * Sets the #GMenuModel from which the #GtkMenuButton:popup property will be
 * filled in, or %NULL to disable the button.
 *
 * The #GtkMenu will be created with gtk_menu_new_from_model(), so actions
 * will be connected as documented there.
 *
 * If #GtkMenuButton:popup is already set, then its content will be lost
 * and replaced by our newly created #GtkMenu.
 *
 * Since: 3.6
 */
void
gtk_menu_button_set_menu_model (GtkMenuButton *menu_button,
                                GMenuModel    *menu_model)
{
  GtkMenuButtonPrivate *priv;
  GtkWidget *menu;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));
  g_return_if_fail (G_IS_MENU_MODEL (menu_model) || menu_model == NULL);

  priv = menu_button->priv;
  g_clear_object (&priv->model);

  if (menu_model == NULL)
    {
      gtk_menu_button_set_popup (menu_button, NULL);
      return;
    }

  priv->model = g_object_ref (menu_model);
  menu = gtk_menu_new_from_model (menu_model);
  gtk_widget_show_all (menu);
  gtk_menu_button_set_popup (menu_button, menu);
}

/**
 * gtk_menu_button_get_menu_model:
 * @menu_button: a #GtkMenuButton
 *
 * Returns the #GMenuModel used to generate the menu.
 *
 * Returns: (transfer none): a #GMenuModel or %NULL.
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

  if (align_widget)
    g_object_add_weak_pointer (G_OBJECT (priv->align_widget), (gpointer *) &priv->align_widget);
}

/**
 * gtk_menu_button_set_align_widget:
 * @menu_button: a #GtkMenuButton
 * @align_widget: (allow-none): a #GtkWidget
 *
 * Sets the #GtkWidget to use to line the menu with when popped up. Note that
 * the @align_widget must contain the #GtkMenuButton itself.
 *
 * Setting it to %NULL means that the popup menu will be aligned with the
 * button itself.
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

  g_object_notify (G_OBJECT (menu_button), "align-widget");
}

/**
 * gtk_menu_button_get_align_widget:
 * @menu_button: a #GtkMenuButton
 *
 * Returns the parent #GtkWidget to use to line up with menu.
 *
 * Returns: (transfer none): a #GtkWidget value or %NULL.
 *
 * Since: 3.6
 */
GtkWidget *
gtk_menu_button_get_align_widget (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), NULL);

  return menu_button->priv->align_widget;
}

/**
 * gtk_menu_button_set_direction:
 * @menu_button: a #GtkMenuButton
 * @direction: a #GtkArrowType
 *
 * Sets the direction in which the menu will be popped up, as
 * well as changing the arrow's direction. The child will not
 * be changed to an arrow if it was customized.
 *
 * If the menu when popped out would have collided with screen edges,
 * we will do our best to keep it inside the screen and fully visible.
 *
 * If you pass %GTK_ARROW_NONE for a @direction, the menu will behave
 * as if you passed %GTK_ARROW_DOWN (although you won't see any arrows).
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

  /* Is it custom content? We don't change that */
  child = gtk_bin_get_child (GTK_BIN (menu_button));
  if (priv->arrow_widget != child)
    return;

  gtk_arrow_set (GTK_ARROW (child), priv->arrow_type, GTK_SHADOW_NONE);
}

/**
 * gtk_menu_button_get_direction:
 * @menu_button: a #GtkMenuButton
 *
 * Returns the direction the menu will be pointing at when popped up.
 *
 * Returns: a #GtkArrowType value.
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

  if (priv->popup)
    {
      g_signal_handlers_disconnect_by_func (priv->popup,
                                            menu_deactivate_cb,
                                            object);
      gtk_menu_detach (GTK_MENU (priv->popup));
      priv->popup = NULL;
    }

  set_align_widget_pointer (GTK_MENU_BUTTON (object), NULL);

  g_clear_object (&priv->model);

  G_OBJECT_CLASS (gtk_menu_button_parent_class)->dispose (object);
}
