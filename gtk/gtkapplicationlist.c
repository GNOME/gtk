/*
 * Copyright © 2020 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkapplicationlist.h"

#include "gtksettings.h"
#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkapplicationlist
 * @title: GtkApplicationList
 * @short_description: A list model for applications
 * @see_also: #GListModel, #GAppInfo
 *
 * #GtkApplicationList is a list model that wraps #GAppInfo.
 * It presents a #GListModel and fills it with the #GAppInfos for
 * the available applications.
 *
 * The #GtkApplicationList is monitoring for changes and emits
 * #GListModel::items-changed when the list of applications changes.
 */

struct _GtkApplicationList
{
  GObject parent_instance;

  GAppInfoMonitor *monitor;

  GSequence *items;
};

struct _GtkApplicationListClass
{
  GObjectClass parent_class;
};

static GType
gtk_application_list_get_item_type (GListModel *list)
{
  return G_TYPE_APP_INFO;
}

static guint
gtk_application_list_get_n_items (GListModel *list)
{
  GtkApplicationList *self = GTK_APPLICATION_LIST (list);

  return g_sequence_get_length (self->items);
}

static gpointer
gtk_application_list_get_item (GListModel *list,
                               guint       position)
{
  GtkApplicationList *self = GTK_APPLICATION_LIST (list);
  GSequenceIter *iter;

  iter = g_sequence_get_iter_at_pos (self->items, position);

  if (g_sequence_iter_is_end (iter))
    return NULL;
  else
    return g_object_ref (g_sequence_get (iter));
}

static void
gtk_application_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_application_list_get_item_type;
  iface->get_n_items = gtk_application_list_get_n_items;
  iface->get_item = gtk_application_list_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkApplicationList, gtk_application_list, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_application_list_model_init))

static void
apps_changed (GAppInfoMonitor    *monitor,
              GtkApplicationList *self)
{
  guint before;
  guint after;
  GList *apps, *l;

  before = g_sequence_get_length (self->items);
  if (before > 0)
    g_sequence_remove_range (g_sequence_get_begin_iter (self->items),
                             g_sequence_get_end_iter (self->items));

  apps = g_app_info_get_all ();
  for (l = apps; l; l = l->next)
    {
      GAppInfo *info = l->data;
      g_sequence_append (self->items, g_object_ref (info));
    }
  g_list_free_full (apps, g_object_unref);

  after = g_sequence_get_length (self->items);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, before, after);
}

static void
gtk_application_list_dispose (GObject *object)
{
  GtkApplicationList *self = GTK_APPLICATION_LIST (object);

  g_clear_pointer (&self->items, g_sequence_free);

  g_signal_handlers_disconnect_by_func (self->monitor, G_CALLBACK (apps_changed), self);
  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (gtk_application_list_parent_class)->dispose (object);
}

static void
gtk_application_list_class_init (GtkApplicationListClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose = gtk_application_list_dispose;
}

static void
gtk_application_list_init (GtkApplicationList *self)
{
  self->items = g_sequence_new (g_object_unref);
  self->monitor = g_app_info_monitor_get ();
  g_signal_connect (self->monitor, "changed", G_CALLBACK (apps_changed), self);
  apps_changed (self->monitor, self);
}

/**
 * gtk_application_list_new:
 *
 * Creates a new #GtkApplicationList.
 *
 * Returns: a new #GtkApplicationList
 **/
GtkApplicationList *
gtk_application_list_new (void)
{
  return g_object_new (GTK_TYPE_APPLICATION_LIST, NULL);
}
