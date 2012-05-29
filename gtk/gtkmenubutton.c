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

#include "config.h"

#include "gtkmenubutton.h"
#include "gtkmenubuttonprivate.h"
#include "gtkarrow.h"

#include "gtkprivate.h"
#include "gtkintl.h"

struct _GtkMenuButtonPrivate
{
  GtkWidget *menu;
  GMenuModel *model;

  GtkMenuButtonShowMenuCallback func;
  gpointer user_data;

  GtkArrowType arrow_type;
  GtkWidget *parent;
};

enum
{
  PROP_0,
  PROP_MENU,
  PROP_MODEL,
  PROP_PARENT,
  PROP_DIRECTION
};

G_DEFINE_TYPE(GtkMenuButton, gtk_menu_button, GTK_TYPE_TOGGLE_BUTTON)

static void gtk_menu_button_finalize (GObject *object);

static void
gtk_menu_button_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  switch (property_id)
    {
      case PROP_MENU:
        gtk_menu_button_set_menu (GTK_MENU_BUTTON (object), g_value_get_object (value));
        break;
      case PROP_MODEL:
        gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (object), g_value_get_object (value));
        break;
      case PROP_PARENT:
        gtk_menu_button_set_parent (GTK_MENU_BUTTON (object), g_value_get_object (value));
        break;
      case PROP_DIRECTION:
        gtk_menu_button_set_direction (GTK_MENU_BUTTON (object), g_value_get_enum (value));
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
      case PROP_MENU:
        g_value_set_object (value, G_OBJECT (priv->menu));
        break;
      case PROP_MODEL:
        g_value_set_object (value, G_OBJECT (priv->model));
        break;
      case PROP_PARENT:
        g_value_set_object (value, G_OBJECT (priv->parent));
        break;
      case PROP_DIRECTION:
        g_value_set_enum (value, priv->arrow_type);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_menu_button_state_changed (GtkWidget    *widget,
                               GtkStateType  previous_state)
{
  GtkMenuButton *button = GTK_MENU_BUTTON (widget);
  GtkMenuButtonPrivate *priv = button->priv;

  if (!gtk_widget_is_sensitive (widget) && priv->menu)
    {
      gtk_menu_shell_deactivate (GTK_MENU_SHELL (priv->menu));
    }
}

static void
gtk_menu_button_class_init (GtkMenuButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GtkMenuButtonPrivate));

  gobject_class->set_property = gtk_menu_button_set_property;
  gobject_class->get_property = gtk_menu_button_get_property;
  gobject_class->finalize = gtk_menu_button_finalize;

  widget_class->state_changed = gtk_menu_button_state_changed;

  g_object_class_install_property (gobject_class,
                                   PROP_MENU,
                                   g_param_spec_object ("menu",
                                                        P_("menu"),
                                                        P_("The dropdown menu."),
                                                        GTK_TYPE_MENU,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        P_("model"),
                                                        P_("The dropdown menu's model."),
                                                        G_TYPE_MENU_MODEL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_PARENT,
                                   g_param_spec_object ("parent",
                                                        P_("parent"),
                                                        P_("The parent widget which the menu should align with."),
                                                        GTK_TYPE_CONTAINER,
                                                        G_PARAM_READWRITE));
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
menu_position_up_func (GtkMenu         *menu,
                       gint            *x,
                       gint            *y,
                       gboolean        *push_in,
                       GtkMenuButton   *menu_button)
{
  GtkWidget *widget = GTK_WIDGET (menu_button);
  GtkRequisition menu_req;
  GtkTextDirection direction;
  GtkAllocation toggle_allocation;
  GdkRectangle monitor;
  gint monitor_num;
  GdkScreen *screen;
  GdkWindow *window;

  gtk_widget_get_preferred_size (GTK_WIDGET (menu),
                                 &menu_req, NULL);

  direction = gtk_widget_get_direction (widget);
  window = gtk_widget_get_window (widget);

  screen = gtk_widget_get_screen (GTK_WIDGET (menu));
  monitor_num = gdk_screen_get_monitor_at_window (screen, window);
  if (monitor_num < 0)
    monitor_num = 0;
  gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);

  gdk_window_get_origin (gtk_button_get_event_window (GTK_BUTTON (menu_button)), x, y);

  gtk_widget_get_allocation (widget, &toggle_allocation);

  if (direction == GTK_TEXT_DIR_LTR)
    *x += MAX (toggle_allocation.width - menu_req.width, 0);
  else if (menu_req.width > toggle_allocation.width)
    *x -= menu_req.width - toggle_allocation.width;

  if (*y - menu_req.height > monitor.y)
    *y -= menu_req.height + toggle_allocation.y;

  *push_in = FALSE;
}

static void
menu_position_side_func (GtkMenu         *menu,
                         int             *x,
                         int             *y,
                         gboolean        *push_in,
                         GtkMenuButton   *menu_button)
{
  GtkMenuButtonPrivate *priv = menu_button->priv;
  GtkAllocation toggle_allocation;
  GtkWidget *widget = GTK_WIDGET (menu_button);
  GtkRequisition menu_req;
  GdkRectangle monitor;
  gint monitor_num;
  GdkScreen *screen;
  GdkWindow *window;

  gtk_widget_get_preferred_size (GTK_WIDGET (priv->menu),
                                 &menu_req, NULL);

  window = gtk_widget_get_window (widget);

  screen = gtk_widget_get_screen (GTK_WIDGET (menu));
  monitor_num = gdk_screen_get_monitor_at_window (screen, window);
  if (monitor_num < 0)
    monitor_num = 0;
  gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);

  gdk_window_get_origin (gtk_button_get_event_window (GTK_BUTTON (menu_button)), x, y);

  gtk_widget_get_allocation (widget, &toggle_allocation);

  if (priv->arrow_type == GTK_ARROW_RIGHT)
    *x += toggle_allocation.width;
  else
    *x -= menu_req.width;

  if (*y + menu_req.height > monitor.y + monitor.height &&
      *y + toggle_allocation.height - monitor.y > monitor.y + monitor.height - *y)
    *y += toggle_allocation.height - menu_req.height;

  *push_in = FALSE;
}

static void
menu_position_down_func (GtkMenu         *menu,
                         int             *x,
                         int             *y,
                         gboolean        *push_in,
                         GtkMenuButton   *menu_button)
{
  GtkMenuButtonPrivate *priv = menu_button->priv;
  GtkWidget *widget = GTK_WIDGET (menu_button);
  GtkRequisition menu_req;
  GtkTextDirection direction;
  GdkRectangle monitor;
  gint monitor_num;
  GdkScreen *screen;
  GdkWindow *window;
  GtkAllocation allocation, arrow_allocation;

  gtk_widget_get_preferred_size (GTK_WIDGET (priv->menu),
                                 &menu_req, NULL);

  direction = gtk_widget_get_direction (widget);
  window = gtk_widget_get_window (priv->parent ? priv->parent : widget);

  screen = gtk_widget_get_screen (GTK_WIDGET (menu));
  monitor_num = gdk_screen_get_monitor_at_window (screen, window);
  if (monitor_num < 0)
    monitor_num = 0;
  gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);

  gtk_widget_get_allocation (priv->parent ? priv->parent : widget, &allocation);
  gtk_widget_get_allocation (widget, &arrow_allocation);

  gdk_window_get_origin (window, x, y);
  *x += allocation.x;
  *y += allocation.y;

  if (direction == GTK_TEXT_DIR_LTR)
    *x += MAX (allocation.width - menu_req.width, 0);
  else if (menu_req.width > allocation.width)
    *x -= menu_req.width - allocation.width;

  if ((*y + arrow_allocation.height + menu_req.height) <= monitor.y + monitor.height)
    *y += arrow_allocation.height;
  else if ((*y - menu_req.height) >= monitor.y)
    *y -= menu_req.height;
  else if (monitor.y + monitor.height - (*y + arrow_allocation.height) > *y)
    *y += arrow_allocation.height;
  else
    *y -= menu_req.height;

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

  if (!priv->menu)
    return;

  switch (priv->arrow_type)
    {
      case GTK_ARROW_UP:
        func = (GtkMenuPositionFunc) menu_position_up_func;
        break;
      case GTK_ARROW_LEFT:
      case GTK_ARROW_RIGHT:
        func = (GtkMenuPositionFunc) menu_position_side_func;
        break;
      default:
        func = (GtkMenuPositionFunc) menu_position_down_func;
        break;
  }

  gtk_menu_popup (GTK_MENU (priv->menu), NULL, NULL,
                  func,
                  GTK_WIDGET (menu_button),
                  event ? event->button : 0,
                  event ? event->time : gtk_get_current_event_time ());
}

static void
menu_button_toggled_cb (GtkMenuButton *menu_button)
{
  GtkMenuButtonPrivate *priv = menu_button->priv;

  if (!priv->menu)
    return;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (menu_button)) &&
      !gtk_widget_get_visible (GTK_WIDGET (priv->menu)))
    {
      /* we get here only when the menu is activated by a key
       * press, so that we can select the first menu item */
      popup_menu (menu_button, NULL);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (priv->menu), FALSE);
    }
}

static gboolean
menu_button_button_press_event_cb (GtkWidget         *widget,
                                    GdkEventButton    *event,
                                    GtkMenuButton *button)
{
  if (event->button == GDK_BUTTON_PRIMARY)
    {
      popup_menu (button, event);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
gtk_menu_button_init (GtkMenuButton *menu_button)
{
  GtkMenuButtonPrivate *priv;
  GtkWidget *arrow;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (menu_button, GTK_TYPE_MENU_BUTTON, GtkMenuButtonPrivate);
  menu_button->priv = priv;
  priv->arrow_type = GTK_ARROW_DOWN;

  arrow = gtk_arrow_new (priv->arrow_type, GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (menu_button), arrow);
  gtk_widget_show (arrow);

  gtk_widget_set_sensitive (GTK_WIDGET (menu_button), FALSE);

  g_signal_connect (menu_button, "toggled",
                    G_CALLBACK (menu_button_toggled_cb), menu_button);
  g_signal_connect (menu_button, "button-press-event",
                    G_CALLBACK (menu_button_button_press_event_cb), menu_button);
}

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
menu_deactivate_cb (GtkMenuShell    *menu_shell,
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

  g_return_if_fail (priv->menu == (GtkWidget *) menu);

  priv->menu = NULL;
}

void
_gtk_menu_button_set_menu_with_func (GtkMenuButton                 *menu_button,
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
      if (gtk_widget_get_visible (GTK_WIDGET (priv->menu)))
        gtk_menu_shell_deactivate (GTK_MENU_SHELL (priv->menu));
    }

  if (priv->menu)
    {
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

      gtk_widget_set_sensitive (GTK_WIDGET (menu_button), TRUE);

      g_signal_connect (priv->menu, "deactivate",
                        G_CALLBACK (menu_deactivate_cb), menu_button);
    }
  else
    {
      gtk_widget_set_sensitive (GTK_WIDGET (menu_button), FALSE);
    }

  g_object_notify (G_OBJECT (menu_button), "menu");
}

void
gtk_menu_button_set_menu (GtkMenuButton *menu_button,
                          GtkWidget     *menu)
{
  _gtk_menu_button_set_menu_with_func (menu_button, menu, NULL, NULL);
}

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
      gtk_menu_button_set_menu (menu_button, NULL);
      return;
    }

  priv->model = g_object_ref (menu_model);
  menu = gtk_menu_new_from_model (menu_model);
  gtk_widget_show_all (menu);
  gtk_menu_button_set_menu (menu_button, menu);

  g_object_notify (G_OBJECT (menu_button), "model");
}

void
gtk_menu_button_set_parent (GtkMenuButton *menu_button,
			    GtkWidget     *parent)
{
  GtkMenuButtonPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));
  g_return_if_fail (parent == NULL || gtk_widget_is_ancestor (GTK_WIDGET (menu_button), parent));

  priv = menu_button->priv;
  if (priv->parent == parent)
    return;

  g_clear_object (&priv->parent);

  if (parent == NULL)
    return;

  priv->parent = g_object_ref (G_OBJECT (parent));

  g_object_notify (G_OBJECT (menu_button), "parent");
}

void
gtk_menu_button_set_direction (GtkMenuButton *menu_button,
                                 GtkArrowType     direction)
{
  GtkMenuButtonPrivate *priv = menu_button->priv;
  GtkWidget *arrow;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  if (priv->arrow_type == direction)
    return;

  priv->arrow_type = direction;
  gtk_widget_destroy (gtk_bin_get_child (GTK_BIN (menu_button)));
  arrow = gtk_arrow_new (priv->arrow_type, GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (menu_button), arrow);
  gtk_widget_show (arrow);
}

static void
gtk_menu_button_finalize (GObject *object)
{
  GtkMenuButtonPrivate *priv = GTK_MENU_BUTTON (object)->priv;

  if (priv->menu)
    {
      g_signal_handlers_disconnect_by_func (priv->menu,
                                            menu_deactivate_cb,
                                            object);
      gtk_menu_detach (GTK_MENU (priv->menu));

      g_signal_handlers_disconnect_by_func (object,
                                            menu_button_toggled_cb,
                                            object);
      g_signal_handlers_disconnect_by_func (object,
                                            menu_button_button_press_event_cb,
                                            object);
    }

  G_OBJECT_CLASS (gtk_menu_button_parent_class)->finalize (object);
}
