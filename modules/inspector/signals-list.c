/*
 * Copyright (c) 2014 Red Hat, Inc.
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
#include <glib/gi18n-lib.h>
#include "signals-list.h"

enum
{
  COLUMN_ENABLED,
  COLUMN_NAME,
  COLUMN_CLASS,
  COLUMN_CONNECTED
};

struct _GtkInspectorSignalsListPrivate
{
  GtkWidget *view;
  GtkListStore *model;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorSignalsList, gtk_inspector_signals_list, GTK_TYPE_BOX)

static void
gtk_inspector_signals_list_init (GtkInspectorSignalsList *sl)
{
  sl->priv = gtk_inspector_signals_list_get_instance_private (sl);
  gtk_widget_init_template (GTK_WIDGET (sl));
}

static GType *
get_types (GObject *object, guint *length)
{
  GHashTable *seen;
  GType *ret;
  GType type;
  GType *iface;
  gint i;

  seen = g_hash_table_new (g_direct_hash, g_direct_equal);

  type = ((GTypeInstance*)object)->g_class->g_type;
  while (type)
    {
      g_hash_table_add (seen, GSIZE_TO_POINTER (type));
      iface = g_type_interfaces (type, NULL);
      for (i = 0; iface[i]; i++)
        g_hash_table_add (seen, GSIZE_TO_POINTER (iface[i]));
      g_free (iface);
      type = g_type_parent (type);
    }
 
  ret = (GType *)g_hash_table_get_keys_as_array (seen, length); 
  g_hash_table_unref (seen);

  return ret;
}

static void
add_signals (GtkInspectorSignalsList *sl,
             GType                    type,
             GObject                 *object)
{
  guint *ids;
  guint n_ids;
  gint i;
  GSignalQuery query;
  GtkTreeIter iter;
  gboolean has_handler;

  if (!G_TYPE_IS_INSTANTIATABLE (type) && !G_TYPE_IS_INTERFACE (type))
    return;

  ids = g_signal_list_ids (type, &n_ids);
  for (i = 0; i < n_ids; i++)
    {
      g_signal_query (ids[i], &query);
      has_handler = g_signal_has_handler_pending (object, ids[i], 0, TRUE);
      gtk_list_store_append (sl->priv->model, &iter);
      gtk_list_store_set (sl->priv->model, &iter,
                          COLUMN_ENABLED, FALSE,
                          COLUMN_NAME, query.signal_name,
                          COLUMN_CLASS, g_type_name (type),
                          COLUMN_CONNECTED, has_handler ? _("Yes") : "",
                          -1);
    }
  g_free (ids);
}

static void
read_signals_from_object (GtkInspectorSignalsList *sl,
                          GObject                 *object)
{
  GType *types;
  guint length;
  gint i;

  types = get_types (object, &length);
  for (i = 0; i < length; i++)
    add_signals (sl, types[i], object);
  g_free (types);
}

void
gtk_inspector_signals_list_set_object (GtkInspectorSignalsList *sl,
                                       GObject                 *object)
{
  gtk_list_store_clear (sl->priv->model);

  read_signals_from_object (sl, object);
}

static void
gtk_inspector_signals_list_class_init (GtkInspectorSignalsListClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/signals-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSignalsList, view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSignalsList, model);
}

GtkWidget *
gtk_inspector_signals_list_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_INSPECTOR_SIGNALS_LIST, NULL));
}

// vim: set et sw=2 ts=2:
