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
#include "gtkmodelmenu.h"

struct _GtkModelMenuItem
{
  GtkCheckMenuItem parent_instance;

  GActionGroup *actions;
  const gchar *action_name;
  gboolean has_indicator;
  gboolean can_activate;
  GVariant *target;
};

typedef GtkCheckMenuItemClass GtkModelMenuItemClass;

static void gtk_model_menu_item_observer_iface_init (GActionObserverInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GtkModelMenuItem, gtk_model_menu_item, GTK_TYPE_CHECK_MENU_ITEM,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_OBSERVER, gtk_model_menu_item_observer_iface_init))

static void
gtk_model_menu_item_activate (GtkMenuItem *menu_item)
{
  GtkModelMenuItem *item = GTK_MODEL_MENU_ITEM (menu_item);

  if (item->can_activate)
    g_action_group_activate_action (item->actions, item->action_name, item->target);
}

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
gtk_model_menu_item_set_active (GtkModelMenuItem *item,
                                gboolean          active)
{
  GtkCheckMenuItem *checkitem = GTK_CHECK_MENU_ITEM (item);

  if (gtk_check_menu_item_get_active (checkitem) != active)
    {
      _gtk_check_menu_item_set_active (checkitem, active);
      g_object_notify (G_OBJECT (checkitem), "active");
      gtk_check_menu_item_toggled (checkitem);
      gtk_widget_queue_draw (GTK_WIDGET (item));
    }
}

static void
gtk_model_menu_item_action_added (GActionObserver    *observer,
                                  GActionObservable  *observable,
                                  const gchar        *action_name,
                                  const GVariantType *parameter_type,
                                  gboolean            enabled,
                                  GVariant           *state)
{
  GtkModelMenuItem *item = GTK_MODEL_MENU_ITEM (observer);

  /* we can only activate the item if we have the correct type of parameter */
  item->can_activate = (item->target == NULL && parameter_type == NULL) ||
                       (item->target != NULL && parameter_type != NULL &&
                        g_variant_is_of_type (item->target, parameter_type));

  if (item->can_activate)
    {
      if (item->target != NULL && state != NULL)
        {
          /* actions with states and targets are radios */
          gboolean selected;

          selected = g_variant_equal (state, item->target);
          gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (item), TRUE);
          gtk_model_menu_item_set_active (item, selected);
          item->has_indicator = TRUE;
        }

      else if (state != NULL && g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
        {
          /* boolean state actions without target are checks */
          gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (item), FALSE);
          gtk_model_menu_item_set_active (item, g_variant_get_boolean (state));
          item->has_indicator = TRUE;
        }

      else
        {
          /* stateless items are just plain actions */
          gtk_model_menu_item_set_active (item, FALSE);
          item->has_indicator = FALSE;
        }

      gtk_widget_set_sensitive (GTK_WIDGET (item), enabled);
      gtk_widget_queue_resize (GTK_WIDGET (item));
    }
}

static void
gtk_model_menu_item_action_enabled_changed (GActionObserver   *observer,
                                            GActionObservable *observable,
                                            const gchar       *action_name,
                                            gboolean           enabled)
{
  GtkModelMenuItem *item = GTK_MODEL_MENU_ITEM (observer);

  if (!item->can_activate)
    return;

  gtk_widget_set_sensitive (GTK_WIDGET (item), item->can_activate && enabled);
}

static void
gtk_model_menu_item_action_state_changed (GActionObserver   *observer,
                                          GActionObservable *observable,
                                          const gchar       *action_name,
                                          GVariant          *state)
{
  GtkModelMenuItem *item = GTK_MODEL_MENU_ITEM (observer);

  if (!item->can_activate)
    return;

  if (item->target)
    gtk_model_menu_item_set_active (item, g_variant_equal (state, item->target));

  else if (g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
    gtk_model_menu_item_set_active (item, g_variant_get_boolean (state));
}

static void
gtk_model_menu_item_action_removed (GActionObserver   *observer,
                                    GActionObservable *observable,
                                    const gchar       *action_name)
{
  GtkModelMenuItem *item = GTK_MODEL_MENU_ITEM (observer);

  if (!item->can_activate)
    return;

  gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
  gtk_model_menu_item_set_active (item, FALSE);
  item->has_indicator = FALSE;

  gtk_widget_queue_resize (GTK_WIDGET (item));
}

static void
gtk_model_menu_item_setup (GtkModelMenuItem  *item,
                           GMenuModel        *model,
                           gint               item_index,
                           GActionObservable *actions,
                           GtkAccelGroup     *accels)
{
  GMenuAttributeIter *iter;
  GMenuModel *submenu;
  const gchar *key;
  GVariant *value;

  if ((submenu = g_menu_model_get_item_link (model, item_index, "submenu")))
    {
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), gtk_model_menu_create_menu (submenu, actions, accels));
      g_object_unref (submenu);
    }

  iter = g_menu_model_iterate_item_attributes (model, item_index);
  while (g_menu_attribute_iter_get_next (iter, &key, &value))
    {
      if (g_str_equal (key, "label") && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        gtk_menu_item_set_label (GTK_MENU_ITEM (item), g_variant_get_string (value, NULL));

      else if (g_str_equal (key, "action") && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        item->action_name = g_variant_get_string (value, NULL);

      else if (g_str_equal (key, "target"))
        item->target = g_variant_ref (value);

      g_variant_unref (value);
    }
  g_object_unref (iter);

  gtk_menu_item_set_use_underline (GTK_MENU_ITEM (item), TRUE);

  if (item->action_name)
    {
      const GVariantType *type;
      gboolean enabled;
      GVariant *state;
      gchar *path;

      /* observer already causes us to hold a hard ref on the group */
      item->actions = G_ACTION_GROUP (actions);

      if (actions)
        {
          g_action_observable_register_observer (actions, item->action_name, G_ACTION_OBSERVER (item));

          if (g_action_group_query_action (G_ACTION_GROUP (actions), item->action_name, &enabled, &type, NULL, NULL, &state))
            {
              gtk_model_menu_item_action_added (G_ACTION_OBSERVER (item), actions, item->action_name, type, enabled, state);
              if (state != NULL)
                g_variant_unref (state);
            }
          else
            gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
        }
      else
        gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);

      path = _gtk_accel_path_for_action (item->action_name, item->target);
      gtk_menu_item_set_accel_path (GTK_MENU_ITEM (item), path);
      g_free (path);
    }
}

static void
gtk_model_menu_item_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_model_menu_item_parent_class)
    ->finalize (object);
}

static void
gtk_model_menu_item_init (GtkModelMenuItem *item)
{
}

static void
gtk_model_menu_item_observer_iface_init (GActionObserverInterface *iface)
{
  iface->action_added = gtk_model_menu_item_action_added;
  iface->action_enabled_changed = gtk_model_menu_item_action_enabled_changed;
  iface->action_state_changed = gtk_model_menu_item_action_state_changed;
  iface->action_removed = gtk_model_menu_item_action_removed;
}

static void
gtk_model_menu_item_class_init (GtkModelMenuItemClass *class)
{
  GtkCheckMenuItemClass *check_class = GTK_CHECK_MENU_ITEM_CLASS (class);
  GtkMenuItemClass *item_class = GTK_MENU_ITEM_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  check_class->draw_indicator = gtk_model_menu_item_draw_indicator;

  item_class->activate = gtk_model_menu_item_activate;
  item_class->toggle_size_request = gtk_model_menu_item_toggle_size_request;

  object_class->finalize = gtk_model_menu_item_finalize;
}

GtkMenuItem *
gtk_model_menu_item_new (GMenuModel        *model,
                         gint               item_index,
                         GActionObservable *actions,
                         GtkAccelGroup     *accels)
{
  GtkModelMenuItem *item;

  item = g_object_new (GTK_TYPE_MODEL_MENU_ITEM, NULL);

  gtk_model_menu_item_setup (item, model, item_index, actions, accels);

  return GTK_MENU_ITEM (item);
}
