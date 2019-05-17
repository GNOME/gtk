/* gtkseparatortoolitem.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
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

#include "gtkseparatortoolitem.h"

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkseparatormenuitem.h"
#include "gtkstylecontext.h"
#include "gtktoolbarprivate.h"
#include "gtkwidgetprivate.h"

/**
 * SECTION:gtkseparatortoolitem
 * @Short_description: A toolbar item that separates groups of other
 *   toolbar items
 * @Title: GtkSeparatorToolItem
 * @See_also: #GtkToolbar, #GtkRadioToolButton
 *
 * A #GtkSeparatorToolItem is a #GtkToolItem that separates groups of other
 * #GtkToolItems. Depending on the theme, a #GtkSeparatorToolItem will
 * often look like a vertical line on horizontally docked toolbars.
 *
 * If the #GtkToolbar child property “expand” is %TRUE and the property
 * #GtkSeparatorToolItem:draw is %FALSE, a #GtkSeparatorToolItem will act as
 * a “spring” that forces other items to the ends of the toolbar.
 *
 * Use gtk_separator_tool_item_new() to create a new #GtkSeparatorToolItem.
 *
 * # CSS nodes
 *
 * GtkSeparatorToolItem has a single CSS node with name separator.
 */

typedef struct _GtkSeparatorToolItemClass   GtkSeparatorToolItemClass;

struct _GtkSeparatorToolItem
{
  GtkToolItem parent_instance;
};

struct _GtkSeparatorToolItemClass
{
  GtkToolItemClass parent_class;
};

#define MENU_ID "gtk-separator-tool-item-menu-id"

enum {
  PROP_0,
  PROP_DRAW
};

static gboolean gtk_separator_tool_item_create_menu_proxy (GtkToolItem               *item);
static void     gtk_separator_tool_item_set_property      (GObject                   *object,
                                                           guint                      prop_id,
                                                           const GValue              *value,
                                                           GParamSpec                *pspec);
static void     gtk_separator_tool_item_get_property      (GObject                   *object,
                                                           guint                      prop_id,
                                                           GValue                    *value,
                                                           GParamSpec                *pspec);
static void     gtk_separator_tool_item_add               (GtkContainer              *container,
                                                           GtkWidget                 *child);

G_DEFINE_TYPE (GtkSeparatorToolItem, gtk_separator_tool_item, GTK_TYPE_TOOL_ITEM)

static void
gtk_separator_tool_item_class_init (GtkSeparatorToolItemClass *class)
{
  GObjectClass *object_class;
  GtkContainerClass *container_class;
  GtkToolItemClass *toolitem_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GObjectClass *)class;
  container_class = (GtkContainerClass *)class;
  toolitem_class = (GtkToolItemClass *)class;
  widget_class = (GtkWidgetClass *)class;

  object_class->set_property = gtk_separator_tool_item_set_property;
  object_class->get_property = gtk_separator_tool_item_get_property;

  toolitem_class->create_menu_proxy = gtk_separator_tool_item_create_menu_proxy;
  
  container_class->add = gtk_separator_tool_item_add;
  
  g_object_class_install_property (object_class,
                                   PROP_DRAW,
                                   g_param_spec_boolean ("draw",
                                                         P_("Draw"),
                                                         P_("Whether the separator is drawn, or just blank"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_css_name (widget_class, I_("separator"));
}

static void
gtk_separator_tool_item_init (GtkSeparatorToolItem *separator_item)
{
}

static void
gtk_separator_tool_item_add (GtkContainer *container,
                             GtkWidget    *child)
{
  g_warning ("attempt to add a child to an GtkSeparatorToolItem");
}

static gboolean
gtk_separator_tool_item_create_menu_proxy (GtkToolItem *item)
{
  GtkWidget *menu_item = NULL;
  
  menu_item = gtk_separator_menu_item_new ();
  
  gtk_tool_item_set_proxy_menu_item (item, MENU_ID, menu_item);
  
  return TRUE;
}

static void
gtk_separator_tool_item_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkSeparatorToolItem *item = GTK_SEPARATOR_TOOL_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_DRAW:
      gtk_separator_tool_item_set_draw (item, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_tool_item_get_property (GObject      *object,
                                      guint         prop_id,
                                      GValue       *value,
                                      GParamSpec   *pspec)
{
  GtkSeparatorToolItem *item = GTK_SEPARATOR_TOOL_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_DRAW:
      g_value_set_boolean (value, gtk_separator_tool_item_get_draw (item));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_separator_tool_item_new:
 * 
 * Create a new #GtkSeparatorToolItem
 * 
 * Returns: the new #GtkSeparatorToolItem
 */
GtkToolItem *
gtk_separator_tool_item_new (void)
{
  GtkToolItem *self;
  
  self = g_object_new (GTK_TYPE_SEPARATOR_TOOL_ITEM,
                       NULL);
  
  return self;
}

/**
 * gtk_separator_tool_item_get_draw:
 * @item: a #GtkSeparatorToolItem 
 * 
 * Returns whether @item is drawn as a line, or just blank. 
 * See gtk_separator_tool_item_set_draw().
 * 
 * Returns: %TRUE if @item is drawn as a line, or just blank.
 */
gboolean
gtk_separator_tool_item_get_draw (GtkSeparatorToolItem *item)
{
  GtkStyleContext *context;

  g_return_val_if_fail (GTK_IS_SEPARATOR_TOOL_ITEM (item), FALSE);

  context = gtk_widget_get_style_context (GTK_WIDGET (item));

  return !gtk_style_context_has_class (context, "invisible");
}

/**
 * gtk_separator_tool_item_set_draw:
 * @item: a #GtkSeparatorToolItem
 * @draw: whether @item is drawn as a vertical line
 * 
 * Whether @item is drawn as a vertical line, or just blank.
 * Setting this to %FALSE along with gtk_tool_item_set_expand() is useful
 * to create an item that forces following items to the end of the toolbar.
 */
void
gtk_separator_tool_item_set_draw (GtkSeparatorToolItem *item,
                                  gboolean              draw)
{
  g_return_if_fail (GTK_IS_SEPARATOR_TOOL_ITEM (item));

  draw = !!draw;

  if (draw == gtk_separator_tool_item_get_draw (item))
    return;

  if (draw)
    gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (item)),
                                    "invisible");
  else
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (item)),
                                 "invisible");

  g_object_notify (G_OBJECT (item), "draw");
}
