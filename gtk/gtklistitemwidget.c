/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtklistitemwidgetprivate.h"

#include "gtkbinlayout.h"
#include "gtkeventcontrollermotion.h"
#include "gtkgestureclick.h"
#include "gtklistitemfactoryprivate.h"
#include "gtklistitemprivate.h"
#include "gtklistbaseprivate.h"
#include "gtkwidget.h"

G_DEFINE_TYPE (GtkListItemWidget, gtk_list_item_widget, GTK_TYPE_LIST_FACTORY_WIDGET)

static gboolean
gtk_list_item_widget_focus (GtkWidget        *widget,
                            GtkDirectionType  direction)
{
  GtkWidget *child, *focus_child;

  /* The idea of this function is the following:
   * 1. If any child can take focus, do not ever attempt
   *    to take focus.
   * 2. Otherwise, if this item is selectable or activatable,
   *    allow focusing this widget.
   *
   * This makes sure every item in a list is focusable for
   * activation and selection handling, but no useless widgets
   * get focused and moving focus is as fast as possible.
   */

  focus_child = gtk_widget_get_focus_child (widget);
  if (focus_child && gtk_widget_child_focus (focus_child, direction))
    return TRUE;

  for (child = focus_child ? gtk_widget_get_next_sibling (focus_child)
                           : gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (gtk_widget_child_focus (child, direction))
        return TRUE;
    }

  if (focus_child)
    return FALSE;

  if (gtk_widget_is_focus (widget))
    return FALSE;

  return gtk_widget_grab_focus (widget);
}

static gboolean
gtk_list_item_widget_grab_focus (GtkWidget *widget)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (widget);
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (gtk_widget_grab_focus (child))
        return TRUE;
    }

  if (!gtk_list_factory_widget_get_selectable (GTK_LIST_FACTORY_WIDGET (self)))
    return FALSE;

  return GTK_WIDGET_CLASS (gtk_list_item_widget_parent_class)->grab_focus (widget);
}

static gpointer
gtk_list_item_widget_create_object (GtkListFactoryWidget *fw)
{
  return gtk_list_item_new ();
}

static void
gtk_list_item_widget_setup_object (GtkListFactoryWidget *fw,
                                   gpointer              object)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (fw);
  GtkListItem *list_item = object;

  GTK_LIST_FACTORY_WIDGET_CLASS (gtk_list_item_widget_parent_class)->setup_object (fw, object);

  list_item->owner = self;

  if (list_item->child)
    gtk_list_item_widget_add_child (self, list_item->child);

  gtk_list_factory_widget_set_activatable (fw, list_item->activatable);
  gtk_list_factory_widget_set_selectable (fw, list_item->selectable);

  gtk_list_item_do_notify (list_item,
                           gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self)) != NULL,
                           gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)) != GTK_INVALID_LIST_POSITION,
                           gtk_list_item_base_get_selected (GTK_LIST_ITEM_BASE (self)));
}

static void
gtk_list_item_widget_teardown_object (GtkListFactoryWidget *fw,
                                      gpointer              object)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (fw);
  GtkListItem *list_item = object;

  GTK_LIST_FACTORY_WIDGET_CLASS (gtk_list_item_widget_parent_class)->teardown_object (fw, object);

  list_item->owner = NULL;

  if (list_item->child)
    gtk_list_item_widget_remove_child (self, list_item->child);

  gtk_list_factory_widget_set_activatable (fw, FALSE);
  gtk_list_factory_widget_set_selectable (fw, FALSE);

  gtk_list_item_do_notify (list_item,
                           gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self)) != NULL,
                           gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)) != GTK_INVALID_LIST_POSITION,
                           gtk_list_item_base_get_selected (GTK_LIST_ITEM_BASE (self)));

  /* FIXME: This is technically not correct, the child is user code, isn't it? */
  gtk_list_item_set_child (list_item, NULL);
}

static void
gtk_list_item_widget_update_object (GtkListFactoryWidget *fw,
                                    gpointer              object,
                                    guint                 position,
                                    gpointer              item,
                                    gboolean              selected)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (fw);
  GtkListItemBase *base = GTK_LIST_ITEM_BASE (self);
  GtkListItem *list_item = object;
  /* Track notify manually instead of freeze/thaw_notify for performance reasons. */
  gboolean notify_item = FALSE, notify_position = FALSE, notify_selected = FALSE;

  /* FIXME: It's kinda evil to notify external objects from here... */
  notify_item = gtk_list_item_base_get_item (base) != item;
  notify_position = gtk_list_item_base_get_position (base) != position;
  notify_selected = gtk_list_item_base_get_selected (base) != selected;

  GTK_LIST_FACTORY_WIDGET_CLASS (gtk_list_item_widget_parent_class)->update_object (fw,
                                                                                    object,
                                                                                    position,
                                                                                    item,
                                                                                    selected);

  if (list_item)
    gtk_list_item_do_notify (list_item, notify_item, notify_position, notify_selected);
}

static void
gtk_list_item_widget_class_init (GtkListItemWidgetClass *klass)
{
  GtkListFactoryWidgetClass *factory_class = GTK_LIST_FACTORY_WIDGET_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  factory_class->create_object = gtk_list_item_widget_create_object;
  factory_class->setup_object = gtk_list_item_widget_setup_object;
  factory_class->update_object = gtk_list_item_widget_update_object;
  factory_class->teardown_object = gtk_list_item_widget_teardown_object;

  widget_class->focus = gtk_list_item_widget_focus;
  widget_class->grab_focus = gtk_list_item_widget_grab_focus;

  /* This gets overwritten by gtk_list_item_widget_new() but better safe than sorry */
  gtk_widget_class_set_css_name (widget_class, I_("row"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
gtk_list_item_widget_init (GtkListItemWidget *self)
{
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
}

GtkWidget *
gtk_list_item_widget_new (GtkListItemFactory *factory,
                          const char         *css_name,
                          GtkAccessibleRole   role)
{
  g_return_val_if_fail (css_name != NULL, NULL);

  return g_object_new (GTK_TYPE_LIST_ITEM_WIDGET,
                       "css-name", css_name,
                       "accessible-role", role,
                       "factory", factory,
                       NULL);
}

void
gtk_list_item_widget_add_child (GtkListItemWidget *self,
                                GtkWidget         *child)
{
  gtk_widget_set_parent (child, GTK_WIDGET (self));
}

void
gtk_list_item_widget_reorder_child (GtkListItemWidget *self,
                                    GtkWidget         *child,
                                    guint              position)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkWidget *sibling = NULL;

  if (position > 0)
    {
      GtkWidget *c;
      guint i;

      for (c = gtk_widget_get_first_child (widget), i = 0;
           c;
           c = gtk_widget_get_next_sibling (c), i++)
        {
          if (i + 1 == position)
            {
              sibling = c;
              break;
            }
        }
    }

  if (child != sibling)
    gtk_widget_insert_after (child, widget, sibling);
}

void
gtk_list_item_widget_remove_child (GtkListItemWidget *self,
                                   GtkWidget         *child)
{
  gtk_widget_unparent (child);
}

