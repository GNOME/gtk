/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2003 Ricardo Fernandez Pascual
 * Copyright (C) 2004 Paolo Borelli
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

#include "gtkmenutoolbutton.h"

#include "gtktogglebutton.h"
#include "gtkmenubutton.h"
#include "gtkmenubuttonprivate.h"
#include "gtkbox.h"
#include "gtkmenu.h"
#include "gtkmain.h"
#include "gtksizerequest.h"
#include "gtkbuildable.h"

#include "gtkprivate.h"
#include "gtkintl.h"


/**
 * SECTION:gtkmenutoolbutton
 * @Short_description: A GtkToolItem containing a button with an additional dropdown menu
 * @Title: GtkMenuToolButton
 * @See_also: #GtkToolbar, #GtkToolButton
 *
 * A #GtkMenuToolButton is a #GtkToolItem that contains a button and
 * a small additional button with an arrow. When clicked, the arrow
 * button pops up a dropdown menu.
 *
 * Use gtk_menu_tool_button_new() to create a new
 * #GtkMenuToolButton.
 *
 * # GtkMenuToolButton as GtkBuildable
 *
 * The GtkMenuToolButton implementation of the GtkBuildable interface
 * supports adding a menu by specifying “menu” as the “type” attribute
 * of a `<child>` element.
 *
 * An example for a UI definition fragment with menus:
 *
 * |[<!-- language="xml" -->
 * <object class="GtkMenuToolButton">
 *   <child type="menu">
 *     <object class="GtkMenu"/>
 *   </child>
 * </object>
 * ]|
 */


struct _GtkMenuToolButtonPrivate
{
  GtkWidget *button;
  GtkWidget *arrow_button;
  GtkWidget *box;
};

static void gtk_menu_tool_button_buildable_interface_init (GtkBuildableIface   *iface);
static void gtk_menu_tool_button_buildable_add_child      (GtkBuildable        *buildable,
							   GtkBuilder          *builder,
							   GObject             *child,
							   const gchar         *type);

enum
{
  SHOW_MENU,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_MENU
};

static gint signals[LAST_SIGNAL];

static GtkBuildableIface *parent_buildable_iface;

G_DEFINE_TYPE_WITH_CODE (GtkMenuToolButton, gtk_menu_tool_button, GTK_TYPE_TOOL_BUTTON,
                         G_ADD_PRIVATE (GtkMenuToolButton)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_menu_tool_button_buildable_interface_init))

static void
gtk_menu_tool_button_construct_contents (GtkMenuToolButton *button)
{
  GtkMenuToolButtonPrivate *priv = button->priv;
  GtkWidget *box;
  GtkWidget *parent;
  GtkOrientation orientation;

  orientation = gtk_tool_item_get_orientation (GTK_TOOL_ITEM (button));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_menu_button_set_direction (GTK_MENU_BUTTON (priv->arrow_button), GTK_ARROW_DOWN);
    }
  else
    {
      GtkTextDirection direction;
      GtkArrowType type;

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      direction = gtk_widget_get_direction (GTK_WIDGET (button));
      type = (direction == GTK_TEXT_DIR_LTR ? GTK_ARROW_RIGHT : GTK_ARROW_LEFT);
      gtk_menu_button_set_direction (GTK_MENU_BUTTON (priv->arrow_button), type);
    }

  parent = gtk_widget_get_parent (priv->button);
  if (priv->button && parent)
    {
      g_object_ref (priv->button);
      gtk_container_remove (GTK_CONTAINER (parent),
                            priv->button);
      gtk_container_add (GTK_CONTAINER (box), priv->button);
      g_object_unref (priv->button);
    }

  parent = gtk_widget_get_parent (priv->arrow_button);
  if (priv->arrow_button && parent)
    {
      g_object_ref (priv->arrow_button);
      gtk_container_remove (GTK_CONTAINER (parent),
                            priv->arrow_button);
      gtk_box_pack_end (GTK_BOX (box), priv->arrow_button,
                        FALSE, FALSE, 0);
      g_object_unref (priv->arrow_button);
    }

  if (priv->box)
    {
      gchar *tmp;

      /* Transfer a possible tooltip to the new box */
      g_object_get (priv->box, "tooltip-markup", &tmp, NULL);

      if (tmp)
        {
	  g_object_set (box, "tooltip-markup", tmp, NULL);
	  g_free (tmp);
	}

      /* Note: we are not destroying the button and the arrow_button
       * here because they were removed from their container above
       */
      gtk_widget_destroy (priv->box);
    }

  priv->box = box;

  gtk_container_add (GTK_CONTAINER (button), priv->box);
  gtk_widget_show_all (priv->box);

  gtk_button_set_relief (GTK_BUTTON (priv->arrow_button),
			 gtk_tool_item_get_relief_style (GTK_TOOL_ITEM (button)));
  
  gtk_widget_queue_resize (GTK_WIDGET (button));
}

static void
gtk_menu_tool_button_toolbar_reconfigured (GtkToolItem *toolitem)
{
  gtk_menu_tool_button_construct_contents (GTK_MENU_TOOL_BUTTON (toolitem));

  /* chain up */
  GTK_TOOL_ITEM_CLASS (gtk_menu_tool_button_parent_class)->toolbar_reconfigured (toolitem);
}

static void
gtk_menu_tool_button_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkMenuToolButton *button = GTK_MENU_TOOL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_MENU:
      gtk_menu_tool_button_set_menu (button, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_tool_button_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkMenuToolButton *button = GTK_MENU_TOOL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_MENU:
      g_value_set_object (value, gtk_menu_button_get_popup (GTK_MENU_BUTTON (button->priv->arrow_button)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_tool_button_class_init (GtkMenuToolButtonClass *klass)
{
  GObjectClass *object_class;
  GtkToolItemClass *toolitem_class;

  object_class = (GObjectClass *)klass;
  toolitem_class = (GtkToolItemClass *)klass;

  object_class->set_property = gtk_menu_tool_button_set_property;
  object_class->get_property = gtk_menu_tool_button_get_property;

  toolitem_class->toolbar_reconfigured = gtk_menu_tool_button_toolbar_reconfigured;

  /**
   * GtkMenuToolButton::show-menu:
   * @button: the object on which the signal is emitted
   *
   * The ::show-menu signal is emitted before the menu is shown.
   *
   * It can be used to populate the menu on demand, using
   * gtk_menu_tool_button_set_menu().

   * Note that even if you populate the menu dynamically in this way,
   * you must set an empty menu on the #GtkMenuToolButton beforehand,
   * since the arrow is made insensitive if the menu is not set.
   */
  signals[SHOW_MENU] =
    g_signal_new (I_("show-menu"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuToolButtonClass, show_menu),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
                                   PROP_MENU,
                                   g_param_spec_object ("menu",
                                                        P_("Menu"),
                                                        P_("The dropdown menu"),
                                                        GTK_TYPE_MENU,
                                                        GTK_PARAM_READWRITE));
}

static void
gtk_menu_tool_button_init (GtkMenuToolButton *button)
{
  GtkWidget *box;
  GtkWidget *arrow_button;
  GtkWidget *real_button;

  button->priv = gtk_menu_tool_button_get_instance_private (button);

  gtk_tool_item_set_homogeneous (GTK_TOOL_ITEM (button), FALSE);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  real_button = gtk_bin_get_child (GTK_BIN (button));
  g_object_ref (real_button);
  gtk_container_remove (GTK_CONTAINER (button), real_button);
  gtk_container_add (GTK_CONTAINER (box), real_button);
  g_object_unref (real_button);

  arrow_button = gtk_menu_button_new ();
  gtk_box_pack_end (GTK_BOX (box), arrow_button,
                    FALSE, FALSE, 0);

  /* the arrow button is insentive until we set a menu */
  gtk_widget_set_sensitive (arrow_button, FALSE);

  gtk_widget_show_all (box);

  gtk_container_add (GTK_CONTAINER (button), box);
  gtk_menu_button_set_align_widget (GTK_MENU_BUTTON (arrow_button),
                                    GTK_WIDGET (button));

  button->priv->button = real_button;
  button->priv->arrow_button = arrow_button;
  button->priv->box = box;
}

static void
gtk_menu_tool_button_buildable_add_child (GtkBuildable *buildable,
					  GtkBuilder   *builder,
					  GObject      *child,
					  const gchar  *type)
{
  if (type && strcmp (type, "menu") == 0)
    gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (buildable),
                                   GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_menu_tool_button_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_menu_tool_button_buildable_add_child;
}

/**
 * gtk_menu_tool_button_new:
 * @icon_widget: (allow-none): a widget that will be used as icon widget, or %NULL
 * @label: (allow-none): a string that will be used as label, or %NULL
 *
 * Creates a new #GtkMenuToolButton using @icon_widget as icon and
 * @label as label.
 *
 * Returns: the new #GtkMenuToolButton
 *
 * Since: 2.6
 **/
GtkToolItem *
gtk_menu_tool_button_new (GtkWidget   *icon_widget,
                          const gchar *label)
{
  GtkMenuToolButton *button;

  button = g_object_new (GTK_TYPE_MENU_TOOL_BUTTON, NULL);

  if (label)
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (button), label);

  if (icon_widget)
    gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (button), icon_widget);

  return GTK_TOOL_ITEM (button);
}

/**
 * gtk_menu_tool_button_new_from_stock:
 * @stock_id: the name of a stock item
 *
 * Creates a new #GtkMenuToolButton.
 * The new #GtkMenuToolButton will contain an icon and label from
 * the stock item indicated by @stock_id.
 *
 * Returns: the new #GtkMenuToolButton
 *
 * Since: 2.6
 *
 * Deprecated: 3.10: Use gtk_menu_tool_button_new() instead.
 **/
GtkToolItem *
gtk_menu_tool_button_new_from_stock (const gchar *stock_id)
{
  GtkMenuToolButton *button;

  g_return_val_if_fail (stock_id != NULL, NULL);

  button = g_object_new (GTK_TYPE_MENU_TOOL_BUTTON,
			 "stock-id", stock_id,
			 NULL);

  return GTK_TOOL_ITEM (button);
}

static void
_show_menu_emit (gpointer user_data)
{
  GtkMenuToolButton *button = (GtkMenuToolButton *) user_data;
  g_signal_emit (button, signals[SHOW_MENU], 0);
}

/**
 * gtk_menu_tool_button_set_menu:
 * @button: a #GtkMenuToolButton
 * @menu: the #GtkMenu associated with #GtkMenuToolButton
 *
 * Sets the #GtkMenu that is popped up when the user clicks on the arrow.
 * If @menu is NULL, the arrow button becomes insensitive.
 *
 * Since: 2.6
 **/
void
gtk_menu_tool_button_set_menu (GtkMenuToolButton *button,
                               GtkWidget         *menu)
{
  GtkMenuToolButtonPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_TOOL_BUTTON (button));
  g_return_if_fail (GTK_IS_MENU (menu) || menu == NULL);

  priv = button->priv;

  _gtk_menu_button_set_popup_with_func (GTK_MENU_BUTTON (priv->arrow_button),
                                        menu,
                                        _show_menu_emit,
                                        button);

  g_object_notify (G_OBJECT (button), "menu");
}

/**
 * gtk_menu_tool_button_get_menu:
 * @button: a #GtkMenuToolButton
 *
 * Gets the #GtkMenu associated with #GtkMenuToolButton.
 *
 * Returns: (transfer none): the #GtkMenu associated
 *     with #GtkMenuToolButton
 *
 * Since: 2.6
 **/
GtkWidget *
gtk_menu_tool_button_get_menu (GtkMenuToolButton *button)
{
  GtkMenu *ret;

  g_return_val_if_fail (GTK_IS_MENU_TOOL_BUTTON (button), NULL);

  ret = gtk_menu_button_get_popup (GTK_MENU_BUTTON (button->priv->arrow_button));
  if (!ret)
    return NULL;

  return GTK_WIDGET (ret);
}

/**
 * gtk_menu_tool_button_set_arrow_tooltip_text:
 * @button: a #GtkMenuToolButton
 * @text: text to be used as tooltip text for button’s arrow button
 *
 * Sets the tooltip text to be used as tooltip for the arrow button which
 * pops up the menu.  See gtk_tool_item_set_tooltip_text() for setting a tooltip
 * on the whole #GtkMenuToolButton.
 *
 * Since: 2.12
 **/
void
gtk_menu_tool_button_set_arrow_tooltip_text (GtkMenuToolButton *button,
					     const gchar       *text)
{
  g_return_if_fail (GTK_IS_MENU_TOOL_BUTTON (button));

  gtk_widget_set_tooltip_text (button->priv->arrow_button, text);
}

/**
 * gtk_menu_tool_button_set_arrow_tooltip_markup:
 * @button: a #GtkMenuToolButton
 * @markup: markup text to be used as tooltip text for button’s arrow button
 *
 * Sets the tooltip markup text to be used as tooltip for the arrow button
 * which pops up the menu.  See gtk_tool_item_set_tooltip_text() for setting
 * a tooltip on the whole #GtkMenuToolButton.
 *
 * Since: 2.12
 **/
void
gtk_menu_tool_button_set_arrow_tooltip_markup (GtkMenuToolButton *button,
					       const gchar       *markup)
{
  g_return_if_fail (GTK_IS_MENU_TOOL_BUTTON (button));

  gtk_widget_set_tooltip_markup (button->priv->arrow_button, markup);
}
