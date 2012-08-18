/*
 * Copyright © 2011 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkmodelmenuitem.h"

#include "gtkaccelmapprivate.h"
#include "gtkactionhelper.h"
#include "gtkmodelmenu.h"

struct _GtkModelMenuItem
{
  GtkCheckMenuItem parent_instance;
  GtkActionHelperRole role;
  gboolean has_indicator;
};

typedef GtkCheckMenuItemClass GtkModelMenuItemClass;

G_DEFINE_TYPE (GtkModelMenuItem, gtk_model_menu_item, GTK_TYPE_CHECK_MENU_ITEM)

#define PROP_ACTION_ROLE 1

static void
gtk_model_menu_item_toggle_size_request (GtkMenuItem *menu_item,
                                         gint        *requisition)
{
  GtkModelMenuItem *item = GTK_MODEL_MENU_ITEM (menu_item);

  if (item->has_indicator)
    GTK_MENU_ITEM_CLASS (gtk_model_menu_item_parent_class)
      ->toggle_size_request (menu_item, requisition);

  else
    *requisition = 0;
}

static void
gtk_model_menu_item_draw_indicator (GtkCheckMenuItem *check_item,
                                    cairo_t          *cr)
{
  GtkModelMenuItem *item = GTK_MODEL_MENU_ITEM (check_item);

  if (item->has_indicator)
    GTK_CHECK_MENU_ITEM_CLASS (gtk_model_menu_item_parent_class)
      ->draw_indicator (check_item, cr);
}

static void
gtk_model_menu_item_setup (GtkModelMenuItem  *item,
                           GMenuModel        *model,
                           gint               item_index,
                           GtkAccelGroup     *accels)
{
  GMenuAttributeIter *iter;
  GMenuModel *submenu;
  const gchar *key;
  GVariant *value;

  if ((submenu = g_menu_model_get_item_link (model, item_index, "submenu")))
    {
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), gtk_model_menu_create_menu (submenu, accels));
      g_object_unref (submenu);
    }

  iter = g_menu_model_iterate_item_attributes (model, item_index);
  while (g_menu_attribute_iter_get_next (iter, &key, &value))
    {
      if (g_str_equal (key, "label") && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        gtk_menu_item_set_label (GTK_MENU_ITEM (item), g_variant_get_string (value, NULL));

      else if (g_str_equal (key, "action") && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        gtk_actionable_set_action_name (GTK_ACTIONABLE (item), g_variant_get_string (value, NULL));

      else if (g_str_equal (key, "target"))
        gtk_actionable_set_action_target_value (GTK_ACTIONABLE (item), value);

      g_variant_unref (value);
    }
  g_object_unref (iter);

  gtk_menu_item_set_use_underline (GTK_MENU_ITEM (item), TRUE);
}

static void
gtk_model_menu_item_set_has_indicator (GtkModelMenuItem *item,
                                       gboolean          has_indicator)
{
  if (has_indicator == item->has_indicator)
    return;

  item->has_indicator = has_indicator;

  gtk_widget_queue_resize (GTK_WIDGET (item));
}

static void
gtk_model_menu_item_set_property (GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec)
{
  GtkModelMenuItem *item = GTK_MODEL_MENU_ITEM (object);
  GtkActionHelperRole role;
  AtkObject *accessible;
  AtkRole a11y_role;

  g_assert (prop_id == PROP_ACTION_ROLE);

  role = g_value_get_uint (value);

  if (role == item->role)
    return;

  gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (item), role == GTK_ACTION_HELPER_ROLE_RADIO);
  gtk_model_menu_item_set_has_indicator (item, role != GTK_ACTION_HELPER_ROLE_NORMAL);

  accessible = gtk_widget_get_accessible (GTK_WIDGET (item));
  switch (role)
    {
    case GTK_ACTION_HELPER_ROLE_NORMAL:
      a11y_role = ATK_ROLE_MENU_ITEM;
      break;

    case GTK_ACTION_HELPER_ROLE_TOGGLE:
      a11y_role = ATK_ROLE_CHECK_MENU_ITEM;
      break;

    case GTK_ACTION_HELPER_ROLE_RADIO:
      a11y_role = ATK_ROLE_RADIO_MENU_ITEM;
      break;

    default:
      g_assert_not_reached ();
    }

  atk_object_set_role (accessible, a11y_role);
}

static void
gtk_model_menu_item_init (GtkModelMenuItem *item)
{
}

static void
gtk_model_menu_item_class_init (GtkModelMenuItemClass *class)
{
  GtkCheckMenuItemClass *check_class = GTK_CHECK_MENU_ITEM_CLASS (class);
  GtkMenuItemClass *item_class = GTK_MENU_ITEM_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  check_class->draw_indicator = gtk_model_menu_item_draw_indicator;

  item_class->toggle_size_request = gtk_model_menu_item_toggle_size_request;

  object_class->set_property = gtk_model_menu_item_set_property;

  g_object_class_install_property (object_class, PROP_ACTION_ROLE,
                                   g_param_spec_uint ("action-role", "action role", "action role",
                                                      0, 2, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
}

GtkMenuItem *
gtk_model_menu_item_new (GMenuModel        *model,
                         gint               item_index,
                         GtkAccelGroup     *accels)
{
  GtkModelMenuItem *item;

  item = g_object_new (GTK_TYPE_MODEL_MENU_ITEM, NULL);

  gtk_model_menu_item_setup (item, model, item_index, accels);

  return GTK_MENU_ITEM (item);
}
