/*
 * Copyright © 2011, 2013 Canonical Limited
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

#include "gtkaccellabel.h"
#include "gtkimage.h"
#include "gtkbox.h"

struct _GtkModelMenuItem
{
  GtkCheckMenuItem parent_instance;
  GtkMenuTrackerItemRole role;
  gboolean has_indicator;
};

typedef GtkCheckMenuItemClass GtkModelMenuItemClass;

G_DEFINE_TYPE (GtkModelMenuItem, gtk_model_menu_item, GTK_TYPE_CHECK_MENU_ITEM)

enum
{
  PROP_0,
  PROP_ACTION_ROLE,
  PROP_ICON,
  PROP_TEXT,
  PROP_TOGGLED,
  PROP_ACCEL
};

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
gtk_model_menu_item_activate (GtkMenuItem *item)
{
  /* block the automatic toggle behaviour -- just do nothing */
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
gtk_model_menu_item_set_has_indicator (GtkModelMenuItem *item,
                                       gboolean          has_indicator)
{
  if (has_indicator == item->has_indicator)
    return;

  item->has_indicator = has_indicator;

  gtk_widget_queue_resize (GTK_WIDGET (item));
}

static void
gtk_model_menu_item_set_action_role (GtkModelMenuItem       *item,
                                     GtkMenuTrackerItemRole  role)
{
  AtkObject *accessible;
  AtkRole a11y_role;

  if (role == item->role)
    return;

  gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (item), role == GTK_MENU_TRACKER_ITEM_ROLE_RADIO);
  gtk_model_menu_item_set_has_indicator (item, role != GTK_MENU_TRACKER_ITEM_ROLE_NORMAL);

  accessible = gtk_widget_get_accessible (GTK_WIDGET (item));
  switch (role)
    {
    case GTK_MENU_TRACKER_ITEM_ROLE_NORMAL:
      a11y_role = ATK_ROLE_MENU_ITEM;
      break;

    case GTK_MENU_TRACKER_ITEM_ROLE_CHECK:
      a11y_role = ATK_ROLE_CHECK_MENU_ITEM;
      break;

    case GTK_MENU_TRACKER_ITEM_ROLE_RADIO:
      a11y_role = ATK_ROLE_RADIO_MENU_ITEM;
      break;

    default:
      g_assert_not_reached ();
    }

  atk_object_set_role (accessible, a11y_role);
  g_object_notify (G_OBJECT (item), "action-role");
}

static void
gtk_model_menu_item_set_icon (GtkModelMenuItem *item,
                              GIcon            *icon)
{
  GtkWidget *child;

  g_return_if_fail (GTK_IS_MODEL_MENU_ITEM (item));
  g_return_if_fail (icon == NULL || G_IS_ICON (icon));

  child = gtk_bin_get_child (GTK_BIN (item));

  /* There are only three possibilities here:
   *
   *   - no child
   *   - accel label child
   *   - already a box
   *
   * Handle the no-child case by having GtkMenuItem create the accel
   * label, then we will only have two possible cases.
   */
  if (child == NULL)
    {
      gtk_menu_item_get_label (GTK_MENU_ITEM (item));
      child = gtk_bin_get_child (GTK_BIN (item));
      g_assert (GTK_IS_LABEL (child));
    }

  /* If it is a box, make sure there are no images inside of it already.
   */
  if (GTK_IS_BOX (child))
    {
      GList *children;

      children = gtk_container_get_children (GTK_CONTAINER (child));
      while (children)
        {
          if (GTK_IS_IMAGE (children->data))
            gtk_widget_destroy (children->data);

          children = g_list_delete_link (children, children);
        }
    }
  else
    {
      GtkWidget *box;

      if (icon == NULL)
        return;

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

      /* Reparent the child without destroying it */
      g_object_ref (child);
      gtk_container_remove (GTK_CONTAINER (item), child);
      gtk_box_pack_end (GTK_BOX (box), child, TRUE, TRUE, 0);
      g_object_unref (child);

      gtk_container_add (GTK_CONTAINER (item), box);
      gtk_widget_show (box);

      /* Now we have a box */
      child = box;
    }

  g_assert (GTK_IS_BOX (child));

  /* child is now a box containing a label and no image.  Add the icon,
   * if appropriate.
   */
  if (icon != NULL)
    {
      GtkWidget *image;

      image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
      gtk_box_pack_start (GTK_BOX (child), image, FALSE, FALSE, 0);
      gtk_widget_show (image);
    }

  g_object_notify (G_OBJECT (item), "icon");
}

static GIcon *
gtk_model_menu_item_get_icon (GtkModelMenuItem *item)
{
  GtkWidget *child;
  GIcon *icon = NULL;

  child = gtk_bin_get_child (GTK_BIN (item));
  if (GTK_IS_BOX (child))
    {
      GList *children, *l;

      children = gtk_container_get_children (GTK_CONTAINER (child));
      for (l = children; l; l = l->next)
        {
          if (GTK_IS_IMAGE (l->data))
            {
              gtk_image_get_gicon (GTK_IMAGE (l->data), &icon, NULL);
              break;
            }
        }
      g_list_free (children);
    }

  return icon;
}

static void
gtk_model_menu_item_set_text (GtkModelMenuItem *item,
                              const gchar      *text)
{
  GtkWidget *child;
  GList *children;

  child = gtk_bin_get_child (GTK_BIN (item));
  if (child == NULL)
    {
      gtk_menu_item_get_label (GTK_MENU_ITEM (item));
      child = gtk_bin_get_child (GTK_BIN (item));
      g_assert (GTK_IS_LABEL (child));
    }

  if (GTK_IS_LABEL (child))
    {
      gtk_label_set_text_with_mnemonic (GTK_LABEL (child), text);
      return;
    }

  if (!GTK_IS_CONTAINER (child))
    return;

  children = gtk_container_get_children (GTK_CONTAINER (child));

  while (children)
    {
      if (GTK_IS_LABEL (children->data))
        gtk_label_set_label (GTK_LABEL (children->data), text);

      children = g_list_delete_link (children, children);
    }

  g_object_notify (G_OBJECT (item), "text");
}

static const gchar *
gtk_model_menu_item_get_text (GtkModelMenuItem *item)
{
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (item));

  if (GTK_IS_LABEL (child))
    return gtk_label_get_text (GTK_LABEL (child));

  if (GTK_IS_CONTAINER (child))
    {
      GList *children, *l;
      const gchar *text = NULL;

      children = gtk_container_get_children (GTK_CONTAINER (child));
      for (l = children; l; l = l->next)
        {
          if (GTK_IS_LABEL (l->data))
            {
              text = gtk_label_get_text (GTK_LABEL (l->data));
              break;
            }
        }
      g_list_free (children);

      return text;
    }

  return NULL;
}

static void
gtk_model_menu_item_set_accel (GtkModelMenuItem *item,
                               const gchar      *accel)
{
  GtkWidget *child;
  GList *children;
  GdkModifierType modifiers;
  guint key;

  if (accel)
    {
      gtk_accelerator_parse (accel, &key, &modifiers);
      if (!key)
        modifiers = 0;
    }
  else
    {
      key = 0;
      modifiers = 0;
    }

  child = gtk_bin_get_child (GTK_BIN (item));
  if (child == NULL)
    {
      gtk_menu_item_get_label (GTK_MENU_ITEM (item));
      child = gtk_bin_get_child (GTK_BIN (item));
      g_assert (GTK_IS_LABEL (child));
    }

  if (GTK_IS_ACCEL_LABEL (child))
    {
      gtk_accel_label_set_accel (GTK_ACCEL_LABEL (child), key, modifiers);
      return;
    }

  if (!GTK_IS_CONTAINER (child))
    return;

  children = gtk_container_get_children (GTK_CONTAINER (child));

  while (children)
    {
      if (GTK_IS_ACCEL_LABEL (children->data))
        gtk_accel_label_set_accel (children->data, key, modifiers);

      children = g_list_delete_link (children, children);
    }
}

static gchar *
gtk_model_menu_item_get_accel (GtkModelMenuItem *item)
{
  GtkWidget *child;
  GtkWidget *accel_label = NULL;

  child = gtk_bin_get_child (GTK_BIN (item));

  if (GTK_IS_ACCEL_LABEL (child))
    accel_label = child;
  else if (GTK_IS_CONTAINER (child))
    {
      GList *children, *l;

      children = gtk_container_get_children (GTK_CONTAINER (child));
      for (l = children; l; l = l->next)
        {
          if (GTK_IS_ACCEL_LABEL (l->data))
            {
              accel_label = GTK_WIDGET (l->data);
              break;
            }
        }
      g_list_free (children);
    }

  if (accel_label)
    {
      guint key;
      GdkModifierType mods;

      gtk_accel_label_get_accel (GTK_ACCEL_LABEL (accel_label), &key, &mods);

      return gtk_accelerator_name (key, mods);
    }

  return NULL;
}

static void
gtk_model_menu_item_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkModelMenuItem *item = GTK_MODEL_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_ACTION_ROLE:
      g_value_set_enum (value, item->role);
      break;

    case PROP_ICON:
      g_value_set_object (value, gtk_model_menu_item_get_icon (item));
      break;

    case PROP_TEXT:
      g_value_set_string (value, gtk_model_menu_item_get_text (item));
      break;

    case PROP_TOGGLED:
      g_value_set_boolean (value, gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)));
      break;

    case PROP_ACCEL:
      g_value_take_string (value, gtk_model_menu_item_get_accel (item));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_model_menu_item_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkModelMenuItem *item = GTK_MODEL_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_ACTION_ROLE:
      gtk_model_menu_item_set_action_role (item, g_value_get_enum (value));
      break;

    case PROP_ICON:
      gtk_model_menu_item_set_icon (item, g_value_get_object (value));
      break;

    case PROP_TEXT:
      gtk_model_menu_item_set_text (item, g_value_get_string (value));
      break;

    case PROP_TOGGLED:
      _gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), g_value_get_boolean (value));
      g_object_notify (object, "active");
      break;

    case PROP_ACCEL:
      gtk_model_menu_item_set_accel (item, g_value_get_string (value));
      break;

    default:
      g_assert_not_reached ();
    }
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
  item_class->activate = gtk_model_menu_item_activate;

  object_class->get_property = gtk_model_menu_item_get_property;
  object_class->set_property = gtk_model_menu_item_set_property;

  g_object_class_install_property (object_class, PROP_ACTION_ROLE,
                                   g_param_spec_enum ("action-role", "action role", "action role",
                                                      GTK_TYPE_MENU_TRACKER_ITEM_ROLE,
                                                      GTK_MENU_TRACKER_ITEM_ROLE_NORMAL,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class, PROP_ICON,
                                   g_param_spec_object ("icon", "icon", "icon", G_TYPE_ICON,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class, PROP_TEXT,
                                   g_param_spec_string ("text", "text", "text", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class, PROP_TOGGLED,
                                   g_param_spec_boolean ("toggled", "toggled", "toggled", FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class, PROP_ACCEL,
                                   g_param_spec_string ("accel", "accel", "accel", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_accessible_role (GTK_WIDGET_CLASS (class), ATK_ROLE_MENU_ITEM);
}

GtkWidget *
gtk_model_menu_item_new (void)
{
  return g_object_new (GTK_TYPE_MODEL_MENU_ITEM, NULL);
}
