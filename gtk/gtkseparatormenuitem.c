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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include "gtkaccessibleprivate.h"
#include "gtkseparatormenuitem.h"

/**
 * SECTION:gtkseparatormenuitem
 * @Short_description: A separator used in menus
 * @Title: GtkSeparatorMenuItem
 *
 * The #GtkSeparatorMenuItem is a separator used to group
 * items within a menu. It displays a horizontal line with a shadow to
 * make it appear sunken into the interface.
 */

static AtkObject *gtk_separator_menu_item_get_accessible (GtkWidget *widget);

G_DEFINE_TYPE (GtkSeparatorMenuItem, gtk_separator_menu_item, GTK_TYPE_MENU_ITEM)

static void
gtk_separator_menu_item_class_init (GtkSeparatorMenuItemClass *class)
{
  GTK_CONTAINER_CLASS (class)->child_type = NULL;
  GTK_WIDGET_CLASS (class)->get_accessible = gtk_separator_menu_item_get_accessible;
}

static void 
gtk_separator_menu_item_init (GtkSeparatorMenuItem *item)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (item));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SEPARATOR);
}

/**
 * gtk_separator_menu_item_new:
 *
 * Creates a new #GtkSeparatorMenuItem.
 *
 * Returns: a new #GtkSeparatorMenuItem.
 */
GtkWidget *
gtk_separator_menu_item_new (void)
{
  return g_object_new (GTK_TYPE_SEPARATOR_MENU_ITEM, NULL);
}

typedef struct _GtkSeparatorMenuItemAccessible GtkSeparatorMenuItemAccessible;
typedef struct _GtkSeparatorMenuItemAccessibleClass GtkSeparatorMenuItemAccessibleClass;

ATK_DEFINE_TYPE (GtkSeparatorMenuItemAccessible, _gtk_separator_menu_item_accessible, GTK_TYPE_MENU_ITEM);

static void
_gtk_separator_menu_item_accessible_initialize (AtkObject *accessible,
                                                gpointer   widget)
{
  ATK_OBJECT_CLASS (_gtk_separator_menu_item_accessible_parent_class)->initialize (accessible, widget);

  atk_object_set_role (accessible, ATK_ROLE_SEPARATOR);
}

static void
_gtk_separator_menu_item_accessible_class_init (GtkSeparatorMenuItemAccessibleClass *klass)
{
  AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

  atk_class->initialize = _gtk_separator_menu_item_accessible_initialize;
}

static void
_gtk_separator_menu_item_accessible_init (GtkSeparatorMenuItemAccessible *self)
{
}

typedef AtkObjectFactoryClass   GtkSeparatorMenuItemAccessibleFactoryClass;
typedef AtkObjectFactory        GtkSeparatorMenuItemAccessibleFactory;

G_DEFINE_TYPE (GtkSeparatorMenuItemAccessibleFactory,
               _gtk_separator_menu_item_accessible_factory,
               ATK_TYPE_OBJECT_FACTORY);

static GType
_gtk_separator_menu_item_accessible_factory_get_accessible_type (void)
{
  return _gtk_separator_menu_item_accessible_get_type ();
}

static AtkObject *
_gtk_separator_menu_item_accessible_factory_create_accessible (GObject *obj)
{
  AtkObject *accessible;

  accessible = g_object_new (_gtk_separator_menu_item_accessible_get_type (), NULL);
  atk_object_initialize (accessible, obj);

  return accessible;
}

static void
_gtk_separator_menu_item_accessible_factory_class_init (AtkObjectFactoryClass *klass)
{
  klass->create_accessible = _gtk_separator_menu_item_accessible_factory_create_accessible;
  klass->get_accessible_type = _gtk_separator_menu_item_accessible_factory_get_accessible_type;
}

static void
_gtk_separator_menu_item_accessible_factory_init (AtkObjectFactory *factory)
{
}

static AtkObject *
gtk_separator_menu_item_get_accessible (GtkWidget *widget)
{
  static gboolean initialized = FALSE;

  if (G_UNLIKELY (!initialized))
    {
      _gtk_accessible_set_factory_type (GTK_TYPE_SEPARATOR_MENU_ITEM,
                                        _gtk_separator_menu_item_accessible_factory_get_type ());

      initialized = TRUE;
    }

  return GTK_WIDGET_CLASS (gtk_separator_menu_item_parent_class)->get_accessible (widget);
}
