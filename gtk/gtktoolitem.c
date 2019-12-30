/* gtktoolitem.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2003 Soeren Sandmann <sandmann@daimi.au.dk>
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

#include "gtktoolitem.h"

#include <string.h>

#include "gtkmarshalers.h"
#include "gtktoolshell.h"
#include "gtksizerequest.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"

/**
 * SECTION:gtktoolitem
 * @short_description: The base class of widgets that can be added to GtkToolShell
 * @Title: GtkToolItem
 * @see_also: #GtkToolbar, #GtkToolButton, #GtkSeparatorToolItem
 *
 * #GtkToolItems are widgets that can appear on a toolbar. To
 * create a toolbar item that contain something else than a button, use
 * gtk_tool_item_new(). Use gtk_container_add() to add a child
 * widget to the tool item.
 *
 * For toolbar items that contain buttons, see the #GtkToolButton,
 * #GtkToggleToolButton and #GtkRadioToolButton classes.
 *
 * See the #GtkToolbar class for a description of the toolbar widget, and
 * #GtkToolShell for a description of the tool shell interface.
 */

/**
 * GtkToolItem:
 *
 * The GtkToolItem struct contains only private data.
 * It should only be accessed through the functions described below.
 */

enum {
  CREATE_MENU_PROXY,
  TOOLBAR_RECONFIGURED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_VISIBLE_HORIZONTAL,
  PROP_VISIBLE_VERTICAL,
  PROP_IS_IMPORTANT,
  PROP_HOMOGENEOUS,
  PROP_EXPAND_ITEM,
  PROP_OVERFLOW_TEXT
};


struct _GtkToolItemPrivate
{
  gchar *tip_text;
  gchar *tip_private;

  guint visible_horizontal    : 1;
  guint visible_vertical      : 1;
  guint homogeneous           : 1;
  guint expand                : 1;
  guint is_important          : 1;

  char *overflow_text;
};

static void gtk_tool_item_finalize     (GObject         *object);
static void gtk_tool_item_parent_cb    (GObject         *object,
                                        GParamSpec      *pspec,
                                        gpointer         user_data);
static void gtk_tool_item_set_property (GObject         *object,
					guint            prop_id,
					const GValue    *value,
					GParamSpec      *pspec);
static void gtk_tool_item_get_property (GObject         *object,
					guint            prop_id,
					GValue          *value,
					GParamSpec      *pspec);

static guint toolitem_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkToolItem, gtk_tool_item, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkToolItem));

static void
gtk_tool_item_class_init (GtkToolItemClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;

  object_class->set_property = gtk_tool_item_set_property;
  object_class->get_property = gtk_tool_item_get_property;
  object_class->finalize     = gtk_tool_item_finalize;

  g_object_class_install_property (object_class,
				   PROP_VISIBLE_HORIZONTAL,
				   g_param_spec_boolean ("visible-horizontal",
							 P_("Visible when horizontal"),
							 P_("Whether the toolbar item is visible when the toolbar is in a horizontal orientation."),
							 TRUE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class,
				   PROP_VISIBLE_VERTICAL,
				   g_param_spec_boolean ("visible-vertical",
							 P_("Visible when vertical"),
							 P_("Whether the toolbar item is visible when the toolbar is in a vertical orientation."),
							 TRUE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class,
 				   PROP_IS_IMPORTANT,
 				   g_param_spec_boolean ("is-important",
 							 P_("Is important"),
 							 P_("Whether the toolbar item is considered important. When TRUE, toolbar buttons show text in GTK_TOOLBAR_BOTH_HORIZ mode"),
 							 FALSE,
 							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
                                   PROP_EXPAND_ITEM,
                                   g_param_spec_boolean ("expand-item",
                                                         P_("Expand Item"),
                                                         P_("Whether the item should receive extra space when the toolbar grows"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
                                   PROP_HOMOGENEOUS,
                                   g_param_spec_boolean ("homogeneous",
                                                         P_("Homogeneous"),
                                                         P_("Whether the item should be the same size as other homogeneous items"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkToolItem:overflow-text:
   *
   * The text to use as label in the overflow menu item for this
   * toolitem.
   *
   * If unset, #GtkToolButton:label is used for #GtkToolButtons.
   */
  g_object_class_install_property (object_class,
                                   PROP_OVERFLOW_TEXT,
                                   g_param_spec_string ("overflow-text",
                                                         P_("Overflow text"),
                                                         P_("Label to use in the overflow menu"),
                                                         NULL,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

/**
 * GtkToolItem::toolbar-reconfigured:
 * @tool_item: the object the signal was emitted on
 *
 * This signal is emitted when some property of the toolbar that the
 * item is a child of changes. For custom subclasses of #GtkToolItem,
 * the default handler of this signal use the functions
 * - gtk_tool_shell_get_orientation()
 * - gtk_tool_shell_get_style()
 * - gtk_tool_shell_get_icon_size()
 * to find out what the toolbar should look like and change
 * themselves accordingly.
 **/
  toolitem_signals[TOOLBAR_RECONFIGURED] =
    g_signal_new (I_("toolbar-reconfigured"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkToolItemClass, toolbar_reconfigured),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, I_("toolitem"));
}

static void
gtk_tool_item_init (GtkToolItem *toolitem)
{
  gtk_widget_set_can_focus (GTK_WIDGET (toolitem), FALSE);

  toolitem->priv = gtk_tool_item_get_instance_private (toolitem);
  toolitem->priv->visible_horizontal = TRUE;
  toolitem->priv->visible_vertical = TRUE;
  toolitem->priv->homogeneous = FALSE;
  toolitem->priv->expand = FALSE;

  g_signal_connect (toolitem, "notify::parent", G_CALLBACK (gtk_tool_item_parent_cb), NULL);
}

static void
gtk_tool_item_finalize (GObject *object)
{
  GtkToolItemPrivate *priv = gtk_tool_item_get_instance_private (GTK_TOOL_ITEM (object));

  g_clear_pointer (&priv->overflow_text, g_free);

  G_OBJECT_CLASS (gtk_tool_item_parent_class)->finalize (object);
}

static void
gtk_tool_item_parent_cb (GObject    *object,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  GtkToolItem *toolitem = GTK_TOOL_ITEM (object);

  if (gtk_widget_get_parent (GTK_WIDGET (toolitem)) != NULL)
    gtk_tool_item_toolbar_reconfigured (toolitem);
}

static void
gtk_tool_item_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  GtkToolItem *toolitem = GTK_TOOL_ITEM (object);

  switch (prop_id)
    {
    case PROP_VISIBLE_HORIZONTAL:
      gtk_tool_item_set_visible_horizontal (toolitem, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE_VERTICAL:
      gtk_tool_item_set_visible_vertical (toolitem, g_value_get_boolean (value));
      break;
    case PROP_IS_IMPORTANT:
      gtk_tool_item_set_is_important (toolitem, g_value_get_boolean (value));
      break;
    case PROP_EXPAND_ITEM:
      gtk_tool_item_set_expand (toolitem, g_value_get_boolean (value));
      break;
    case PROP_HOMOGENEOUS:
      gtk_tool_item_set_homogeneous (toolitem, g_value_get_boolean (value));
      break;
    case PROP_OVERFLOW_TEXT:
      gtk_tool_item_set_overflow_text (toolitem, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tool_item_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  GtkToolItem *toolitem = GTK_TOOL_ITEM (object);

  switch (prop_id)
    {
    case PROP_VISIBLE_HORIZONTAL:
      g_value_set_boolean (value, toolitem->priv->visible_horizontal);
      break;
    case PROP_VISIBLE_VERTICAL:
      g_value_set_boolean (value, toolitem->priv->visible_vertical);
      break;
    case PROP_IS_IMPORTANT:
      g_value_set_boolean (value, toolitem->priv->is_important);
      break;
    case PROP_EXPAND_ITEM:
      g_value_set_boolean (value, toolitem->priv->expand);
      break;
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, toolitem->priv->homogeneous);
      break;
    case PROP_OVERFLOW_TEXT:
      g_value_set_string (value, toolitem->priv->overflow_text);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_tool_item_new:
 * 
 * Creates a new #GtkToolItem
 * 
 * Returns: the new #GtkToolItem
 **/
GtkToolItem *
gtk_tool_item_new (void)
{
  GtkToolItem *item;

  item = g_object_new (GTK_TYPE_TOOL_ITEM, NULL);

  return item;
}

/**
 * gtk_tool_item_get_ellipsize_mode:
 * @tool_item: a #GtkToolItem
 *
 * Returns the ellipsize mode used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function to find out how text should
 * be ellipsized.
 *
 * Returns: a #PangoEllipsizeMode indicating how text in @tool_item
 * should be ellipsized.
 **/
PangoEllipsizeMode
gtk_tool_item_get_ellipsize_mode (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), PANGO_ELLIPSIZE_NONE);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return PANGO_ELLIPSIZE_NONE;

  return gtk_tool_shell_get_ellipsize_mode (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_get_orientation:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns the orientation used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function to find out what size icons
 * they should use.
 * 
 * Returns: a #GtkOrientation indicating the orientation
 * used for @tool_item
 **/
GtkOrientation
gtk_tool_item_get_orientation (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_ORIENTATION_HORIZONTAL);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return GTK_ORIENTATION_HORIZONTAL;

  return gtk_tool_shell_get_orientation (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_get_toolbar_style:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns the toolbar style used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function in the handler of the
 * GtkToolItem::toolbar_reconfigured signal to find out in what style
 * the toolbar is displayed and change themselves accordingly 
 *
 * Possibilities are:
 * - %GTK_TOOLBAR_BOTH, meaning the tool item should show
 *   both an icon and a label, stacked vertically
 * - %GTK_TOOLBAR_ICONS, meaning the toolbar shows only icons
 * - %GTK_TOOLBAR_TEXT, meaning the tool item should only show text
 * - %GTK_TOOLBAR_BOTH_HORIZ, meaning the tool item should show
 *   both an icon and a label, arranged horizontally
 * 
 * Returns: A #GtkToolbarStyle indicating the toolbar style used
 * for @tool_item.
 **/
GtkToolbarStyle
gtk_tool_item_get_toolbar_style (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_TOOLBAR_ICONS);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return GTK_TOOLBAR_ICONS;

  return gtk_tool_shell_get_style (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_get_text_alignment:
 * @tool_item: a #GtkToolItem: 
 * 
 * Returns the text alignment used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function to find out how text should
 * be aligned.
 * 
 * Returns: a #gfloat indicating the horizontal text alignment
 * used for @tool_item
 **/
gfloat
gtk_tool_item_get_text_alignment (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_ORIENTATION_HORIZONTAL);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return 0.5;

  return gtk_tool_shell_get_text_alignment (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_get_text_orientation:
 * @tool_item: a #GtkToolItem
 *
 * Returns the text orientation used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function to find out how text should
 * be orientated.
 *
 * Returns: a #GtkOrientation indicating the text orientation
 * used for @tool_item
 */
GtkOrientation
gtk_tool_item_get_text_orientation (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_ORIENTATION_HORIZONTAL);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return GTK_ORIENTATION_HORIZONTAL;

  return gtk_tool_shell_get_text_orientation (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_get_text_size_group:
 * @tool_item: a #GtkToolItem
 *
 * Returns the size group used for labels in @tool_item.
 * Custom subclasses of #GtkToolItem should call this function
 * and use the size group for labels.
 *
 * Returns: (transfer none): a #GtkSizeGroup
 */
GtkSizeGroup *
gtk_tool_item_get_text_size_group (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), NULL);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return NULL;

  return gtk_tool_shell_get_text_size_group (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_set_expand:
 * @tool_item: a #GtkToolItem
 * @expand: Whether @tool_item is allocated extra space
 *
 * Sets whether @tool_item is allocated extra space when there
 * is more room on the toolbar then needed for the items. The
 * effect is that the item gets bigger when the toolbar gets bigger
 * and smaller when the toolbar gets smaller.
 */
void
gtk_tool_item_set_expand (GtkToolItem *tool_item,
			  gboolean     expand)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));
    
  expand = expand != FALSE;

  if (tool_item->priv->expand != expand)
    {
      tool_item->priv->expand = expand;
      g_object_notify (G_OBJECT (tool_item), "expand-item");
      gtk_widget_queue_resize (GTK_WIDGET (tool_item));
    }
}

/**
 * gtk_tool_item_get_expand:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns whether @tool_item is allocated extra space.
 * See gtk_tool_item_set_expand().
 * 
 * Returns: %TRUE if @tool_item is allocated extra space.
 **/
gboolean
gtk_tool_item_get_expand (GtkToolItem *tool_item)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), FALSE);

  return tool_item->priv->expand;
}

/**
 * gtk_tool_item_set_homogeneous:
 * @tool_item: a #GtkToolItem 
 * @homogeneous: whether @tool_item is the same size as other homogeneous items
 * 
 * Sets whether @tool_item is to be allocated the same size as other
 * homogeneous items. The effect is that all homogeneous items will have
 * the same width as the widest of the items.
 **/
void
gtk_tool_item_set_homogeneous (GtkToolItem *tool_item,
			       gboolean     homogeneous)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));
    
  homogeneous = homogeneous != FALSE;

  if (tool_item->priv->homogeneous != homogeneous)
    {
      tool_item->priv->homogeneous = homogeneous;
      g_object_notify (G_OBJECT (tool_item), "homogeneous");
      gtk_widget_queue_resize (GTK_WIDGET (tool_item));
    }
}

/**
 * gtk_tool_item_get_homogeneous:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns whether @tool_item is the same size as other homogeneous
 * items. See gtk_tool_item_set_homogeneous().
 * 
 * Returns: %TRUE if the item is the same size as other homogeneous
 * items.
 **/
gboolean
gtk_tool_item_get_homogeneous (GtkToolItem *tool_item)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), FALSE);

  return tool_item->priv->homogeneous;
}

/**
 * gtk_tool_item_get_is_important:
 * @tool_item: a #GtkToolItem
 * 
 * Returns whether @tool_item is considered important. See
 * gtk_tool_item_set_is_important()
 * 
 * Returns: %TRUE if @tool_item is considered important.
 **/
gboolean
gtk_tool_item_get_is_important (GtkToolItem *tool_item)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), FALSE);

  return tool_item->priv->is_important;
}

/**
 * gtk_tool_item_set_is_important:
 * @tool_item: a #GtkToolItem
 * @is_important: whether the tool item should be considered important
 * 
 * Sets whether @tool_item should be considered important. The #GtkToolButton
 * class uses this property to determine whether to show or hide its label
 * when the toolbar style is %GTK_TOOLBAR_BOTH_HORIZ. The result is that
 * only tool buttons with the “is_important” property set have labels, an
 * effect known as “priority text”
 **/
void
gtk_tool_item_set_is_important (GtkToolItem *tool_item, gboolean is_important)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  is_important = is_important != FALSE;

  if (is_important != tool_item->priv->is_important)
    {
      tool_item->priv->is_important = is_important;

      gtk_widget_queue_resize (GTK_WIDGET (tool_item));

      g_object_notify (G_OBJECT (tool_item), "is-important");
    }
}

/**
 * gtk_tool_item_set_tooltip_text:
 * @tool_item: a #GtkToolItem 
 * @text: text to be used as tooltip for @tool_item
 *
 * Sets the text to be displayed as tooltip on the item.
 * See gtk_widget_set_tooltip_text().
 **/
void
gtk_tool_item_set_tooltip_text (GtkToolItem *tool_item,
			        const gchar *text)
{
  GtkWidget *child;

  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  child = gtk_bin_get_child (GTK_BIN (tool_item));
  if (child)
    gtk_widget_set_tooltip_text (child, text);
}

/**
 * gtk_tool_item_set_tooltip_markup:
 * @tool_item: a #GtkToolItem 
 * @markup: markup text to be used as tooltip for @tool_item
 *
 * Sets the markup text to be displayed as tooltip on the item.
 * See gtk_widget_set_tooltip_markup().
 **/
void
gtk_tool_item_set_tooltip_markup (GtkToolItem *tool_item,
				  const gchar *markup)
{
  GtkWidget *child;

  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  child = gtk_bin_get_child (GTK_BIN (tool_item));
  if (child)
    gtk_widget_set_tooltip_markup (child, markup);
}

/**
 * gtk_tool_item_set_visible_horizontal:
 * @tool_item: a #GtkToolItem
 * @visible_horizontal: Whether @tool_item is visible when in horizontal mode
 * 
 * Sets whether @tool_item is visible when the toolbar is docked horizontally.
 **/
void
gtk_tool_item_set_visible_horizontal (GtkToolItem *toolitem,
				      gboolean     visible_horizontal)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (toolitem));

  visible_horizontal = visible_horizontal != FALSE;

  if (toolitem->priv->visible_horizontal != visible_horizontal)
    {
      toolitem->priv->visible_horizontal = visible_horizontal;

      g_object_notify (G_OBJECT (toolitem), "visible-horizontal");

      gtk_widget_queue_resize (GTK_WIDGET (toolitem));
    }
}

/**
 * gtk_tool_item_get_visible_horizontal:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns whether the @tool_item is visible on toolbars that are
 * docked horizontally.
 * 
 * Returns: %TRUE if @tool_item is visible on toolbars that are
 * docked horizontally.
 **/
gboolean
gtk_tool_item_get_visible_horizontal (GtkToolItem *toolitem)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (toolitem), FALSE);

  return toolitem->priv->visible_horizontal;
}

/**
 * gtk_tool_item_set_visible_vertical:
 * @tool_item: a #GtkToolItem 
 * @visible_vertical: whether @tool_item is visible when the toolbar
 * is in vertical mode
 *
 * Sets whether @tool_item is visible when the toolbar is docked
 * vertically. Some tool items, such as text entries, are too wide to be
 * useful on a vertically docked toolbar. If @visible_vertical is %FALSE
 * @tool_item will not appear on toolbars that are docked vertically.
 **/
void
gtk_tool_item_set_visible_vertical (GtkToolItem *toolitem,
				    gboolean     visible_vertical)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (toolitem));

  visible_vertical = visible_vertical != FALSE;

  if (toolitem->priv->visible_vertical != visible_vertical)
    {
      toolitem->priv->visible_vertical = visible_vertical;

      g_object_notify (G_OBJECT (toolitem), "visible-vertical");

      gtk_widget_queue_resize (GTK_WIDGET (toolitem));
    }
}

/**
 * gtk_tool_item_get_visible_vertical:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns whether @tool_item is visible when the toolbar is docked vertically.
 * See gtk_tool_item_set_visible_vertical().
 * 
 * Returns: Whether @tool_item is visible when the toolbar is docked vertically
 **/
gboolean
gtk_tool_item_get_visible_vertical (GtkToolItem *toolitem)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (toolitem), FALSE);

  return toolitem->priv->visible_vertical;
}

/**
 * gtk_tool_item_rebuild_menu:
 * @tool_item: a #GtkToolItem
 *
 * Calling this function signals to the toolbar that the
 * overflow menu item for @tool_item has changed. If the
 * overflow menu is visible when this function it called,
 * the menu will be rebuilt.
 *
 * The function must be called when the tool item changes what it
 * will do in response to the #GtkToolItem::create-menu-proxy signal.
 */
void
gtk_tool_item_rebuild_menu (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  widget = GTK_WIDGET (tool_item);

  parent = gtk_widget_get_parent (widget);
  if (GTK_IS_TOOL_SHELL (parent))
    gtk_tool_shell_rebuild_menu (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_toolbar_reconfigured:
 * @tool_item: a #GtkToolItem
 *
 * Emits the signal #GtkToolItem::toolbar_reconfigured on @tool_item.
 * #GtkToolbar and other #GtkToolShell implementations use this function
 * to notify children, when some aspect of their configuration changes.
 **/
void
gtk_tool_item_toolbar_reconfigured (GtkToolItem *tool_item)
{
  /* The slightely inaccurate name "gtk_tool_item_toolbar_reconfigured" was
   * choosen over "gtk_tool_item_tool_shell_reconfigured", since the function
   * emits the "toolbar-reconfigured" signal, not "tool-shell-reconfigured".
   * It's not possible to rename the signal, and emitting another name than
   * indicated by the function name would be quite confusing. That's the
   * price of providing stable APIs.
   */
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  g_signal_emit (tool_item, toolitem_signals[TOOLBAR_RECONFIGURED], 0);

  gtk_widget_queue_resize (GTK_WIDGET (tool_item));
}

/**
 * gtk_tool_item_set_overflow_text:
 * @tool_item: a #GtkToolItem
 * @overflow_text: (nullable): Label to use in the overflow menu
 *
 * Sets the label to use for @tool_item in the overflow menu.
 */
void
gtk_tool_item_set_overflow_text (GtkToolItem *tool_item,
                                 const char  *overflow_text)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  g_free (tool_item->priv->overflow_text);
  tool_item->priv->overflow_text = g_strdup (overflow_text);

  g_object_notify (G_OBJECT (tool_item), "overflow-text");
}

/**
 * gtk_tool_item_get_overflow_text:
 * @tool_item: a #GtkToolItem
 *
 * Gets the label that is used for @tool_item in the overflow menu.
 *
 * Return: the overflow menu label for @tool_item
 */
const char *
gtk_tool_item_get_overflow_text (GtkToolItem *tool_item)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), NULL);

  return tool_item->priv->overflow_text;
}
