/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtksignallistitemfactory.h"

#include "gtkintl.h"
#include "gtklistitemfactoryprivate.h"
#include "gtklistitem.h"

/**
 * SECTION:gtksignallistitemfactory
 * @title: GtkSignalListItemFactory
 * @short_description: A listitem factory providing signals
 * @see_also: #GtkListItemFactory
 *
 * #GtkSignalListItemFactory is a #GtkListItemFactory that provides signals
 * that user code can connect to to manage listitems.
 * Signals are emitted for every listitem in the same order:
 *
 *  1. #GtkSignalListItemFactory::setup is emitted to set up permanent things
 *  on the listitem. This usually means constructing the widgets used in the
 *  row and adding them to the listitem.
 *
 *  2. #GtkSignalListItemFactory::bind is emitted to bind the item passed via
 *  #GtkListItem:item to the widgets that have been created in step 1 or to
 *  add item-specific widgets. Signals are connected to listen to changes -
 *  both to changes in the item to update the widgets or to changes in the
 *  widgets to update the item. After this signal has been called, the
 *  listitem may be shown in a list widget.
 *
 *  3. #GtkSignalListItemFactory::unbind is emitted to undo everything done
 *  in step 2. Usually this means disconnecting signal handlers. Once this
 *  signal has been called, the listitem will no longer be used in a list
 *  widget.
 *
 *  4. #GtkSignalListItemFactory::bind and #GtkSignalListItemFactory::unbind
 *  may be emitted multiple times again to bind the listitem for use with
 *  new items. By reusing listitems, potentially costly setup can be
 *  avoided. However, it means code needs to make sure to properly clean
 *  up the listitem in step 3 so that no information from the previous
 *  use leaks into the next use.
 *
 * 5. #GtkSignalListItemFactory::teardown is emitted to allow undoing the
 * effects of #GtkSignalListItemFactory::setup. After this signal was emitted
 * on a listitem, the listitem will be destroyed and not be used again.
 *
 * Note that during the signal emissions, changing properties on the
 * #GtkListItems passed will not trigger notify signals as the listitem's
 * notifications are frozen. See g_object_freeze_notify() for details.
 *
 * For tracking changes in other properties in the #GtkListItem, the
 * #GtkListItem::notify signal is recommended. The signal can be connected
 * in the #GtkSignalListItemFactory::setup signal and removed again during
 * #GtkSignalListItemFactory::teardown.
 */

struct _GtkSignalListItemFactory
{
  GtkListItemFactory parent_instance;
};

struct _GtkSignalListItemFactoryClass
{
  GtkListItemFactoryClass parent_class;

  void                  (* setup)                               (GtkSignalListItemFactory *self,
                                                                 GtkListItem              *list_item);
  void                  (* teardown)                            (GtkSignalListItemFactory *self,
                                                                 GtkListItem              *list_item);
  void                  (* bind)                                (GtkSignalListItemFactory *self,
                                                                 GtkListItem              *list_item);
  void                  (* unbind)                              (GtkSignalListItemFactory *self,
                                                                 GtkListItem              *list_item);
};

enum {
  SETUP,
  BIND,
  UNBIND,
  TEARDOWN,

  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkSignalListItemFactory, gtk_signal_list_item_factory, GTK_TYPE_LIST_ITEM_FACTORY)
static guint signals[LAST_SIGNAL] = { 0 };

static void
gtk_signal_list_item_factory_setup (GtkListItemFactory *factory,
                                    GtkListItemWidget  *widget,
                                    GtkListItem        *list_item)
{
  g_signal_emit (factory, signals[SETUP], 0, list_item);

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_signal_list_item_factory_parent_class)->setup (factory, widget, list_item);

  if (gtk_list_item_get_item (list_item))
    g_signal_emit (factory, signals[BIND], 0, list_item);
}

static void                  
gtk_signal_list_item_factory_update (GtkListItemFactory *factory,
                                     GtkListItemWidget  *widget,
                                     GtkListItem        *list_item,
                                     guint               position,
                                     gpointer            item,
                                     gboolean            selected)
{
  if (gtk_list_item_get_item (list_item))
    g_signal_emit (factory, signals[UNBIND], 0, list_item);

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_signal_list_item_factory_parent_class)->update (factory, widget, list_item, position, item, selected);

  if (item)
    g_signal_emit (factory, signals[BIND], 0, list_item);
}

static void
gtk_signal_list_item_factory_teardown (GtkListItemFactory *factory,
                                       GtkListItemWidget  *widget,
                                       GtkListItem        *list_item)
{
  if (gtk_list_item_get_item (list_item))
    g_signal_emit (factory, signals[UNBIND], 0, list_item);

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_signal_list_item_factory_parent_class)->teardown (factory, widget, list_item);

  g_signal_emit (factory, signals[TEARDOWN], 0, list_item);
}

static void
gtk_signal_list_item_factory_class_init (GtkSignalListItemFactoryClass *klass)
{
  GtkListItemFactoryClass *factory_class = GTK_LIST_ITEM_FACTORY_CLASS (klass);

  factory_class->setup = gtk_signal_list_item_factory_setup;
  factory_class->teardown = gtk_signal_list_item_factory_teardown;
  factory_class->update = gtk_signal_list_item_factory_update;

  /**
   * GtkSignalListItemFactory::setup:
   * @self: The #GtkSignalListItemFactory
   * @listitem: The #GtkListItem to set up
   *
   * The ::setup signal is emitted when a new listitem has been created and
   * needs to be setup for use. It is the first signal emitted for every listitem.
   *
   * The GtkSignalListItemFactory::teardown signal is the opposite of this signal
   * and can be used to undo everything done in this signal.
   */
  signals[SETUP] =
    g_signal_new (I_("setup"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkSignalListItemFactoryClass, setup),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_LIST_ITEM);
  g_signal_set_va_marshaller (signals[SETUP],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * GtkSignalListItemFactory::bind:
   * @self: The #GtkSignalListItemFactory
   * @listitem: The #GtkListItem to bind
   *
   * The ::bind signal is emitted when a new GtkListItem:item has been set
   * on the @listitem and should be bound for use.
   *
   * After this signal was emitted, the listitem might be shown in a #GtkListView
   * or other list widget.
   *
   * The GtkSignalListItemFactory::unbind signal is the opposite of this signal
   * and can be used to undo everything done in this signal.
   */
  signals[BIND] =
    g_signal_new (I_("bind"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkSignalListItemFactoryClass, bind),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_LIST_ITEM);
  g_signal_set_va_marshaller (signals[BIND],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * GtkSignalListItemFactory::unbind:
   * @self: The #GtkSignalListItemFactory
   * @listitem: The #GtkListItem to unbind
   *
   * The ::unbind signal is emitted when a listitem has been removed from use
   * in a list widget and its new GtkListItem:item is about to be unset.
   *
   * This signal is the opposite of the GtkSignalListItemFactory::bind signal
   * and should be used to undo everything done in that signal.
   */
  signals[UNBIND] =
    g_signal_new (I_("unbind"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkSignalListItemFactoryClass, unbind),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_LIST_ITEM);
  g_signal_set_va_marshaller (signals[UNBIND],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * GtkSignalListItemFactory::teardown:
   * @self: The #GtkSignalListItemFactory
   * @listitem: The #GtkListItem to teardown
   *
   * The ::teardown signal is emitted when a listitem is about to be destroyed.
   * It is the last signal ever emitted for this @listitem.
   *
   * This signal is the opposite of the GtkSignalListItemFactory::setup signal
   * and should be used to undo everything done in that signal.
   */
  signals[TEARDOWN] =
    g_signal_new (I_("teardown"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkSignalListItemFactoryClass, teardown),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_LIST_ITEM);
  g_signal_set_va_marshaller (signals[TEARDOWN],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);
}

static void
gtk_signal_list_item_factory_init (GtkSignalListItemFactory *self)
{
}

/**
 * gtk_signal_list_item_factory_new:
 *
 * Creates a new #GtkSignalListItemFactory. You need to connect signal
 * handlers before you use it.
 *
 * Returns: a new #GtkSignalListItemFactory
 **/
GtkListItemFactory *
gtk_signal_list_item_factory_new (void)
{
  return g_object_new (GTK_TYPE_SIGNAL_LIST_ITEM_FACTORY, NULL);
}

