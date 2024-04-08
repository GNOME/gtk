/* gtkaccesskitroot.c: AccessKit root object
 *
 * Copyright 2024  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkaccesskitrootprivate.h"
#include "gtkaccesskitcontextprivate.h"
#include "gtkroot.h"

#include <accesskit.h>

struct _GtkAccessKitRoot
{
  GObject parent_instance;

  GtkRoot *root_widget;

  accesskit_node_class_set *node_classes;
  accesskit_node_id next_id;
  /* TODO */
};

enum
{
  PROP_ROOT_WIDGET = 1,

  N_PROPS
};

static GParamSpec *obj_props[N_PROPS];

G_DEFINE_TYPE (GtkAccessKitRoot, gtk_accesskit_root, G_TYPE_OBJECT)

static void
gtk_accesskit_root_finalize (GObject *gobject)
{
  GtkAccessKitRoot *self = GTK_ACCESSKIT_ROOT (gobject);

  g_clear_pointer (&self->node_classes, accesskit_node_class_set_free);
  /* TODO */

  G_OBJECT_CLASS (gtk_accesskit_root_parent_class)->finalize (gobject);
}

static void
gtk_accesskit_root_dispose (GObject *gobject)
{
  /* TODO */

  G_OBJECT_CLASS (gtk_accesskit_root_parent_class)->dispose (gobject);
}

static void
gtk_accesskit_root_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkAccessKitRoot *self = GTK_ACCESSKIT_ROOT (gobject);

  switch (prop_id)
    {
    case PROP_ROOT_WIDGET:
      self->root_widget = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_accesskit_root_get_property (GObject    *gobject,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkAccessKitRoot *self = GTK_ACCESSKIT_ROOT (gobject);

  switch (prop_id)
    {
    case PROP_ROOT_WIDGET:
      g_value_set_object (value, self->root_widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_accesskit_root_constructed (GObject *gobject)
{
  GtkAccessKitRoot *self = GTK_ACCESSKIT_ROOT (gobject);

  self->node_classes = accesskit_node_class_set_new ();
  /* TODO */

  G_OBJECT_CLASS (gtk_accesskit_root_parent_class)->constructed (gobject);
}

static void
gtk_accesskit_root_class_init (GtkAccessKitRootClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gtk_accesskit_root_constructed;
  gobject_class->set_property = gtk_accesskit_root_set_property;
  gobject_class->get_property = gtk_accesskit_root_get_property;
  gobject_class->dispose = gtk_accesskit_root_dispose;
  gobject_class->finalize = gtk_accesskit_root_finalize;

  obj_props[PROP_ROOT_WIDGET] =
    g_param_spec_object ("root-widget", NULL, NULL,
                         GTK_TYPE_ROOT,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

static void
gtk_accesskit_root_init (GtkAccessKitRoot *self)
{
}

static void
add_subtree_to_update (GtkAccessKitRoot      *self,
                       accesskit_tree_update *update,
                       GtkAccessible         *accessible)
{
  GtkATContext *ctx = gtk_accessible_get_at_context (accessible);
  GtkAccessKitContext *accesskit_ctx = GTK_ACCESSKIT_CONTEXT (ctx);
  accesskit_node_id id;
  accesskit_node *node;
  GtkAccessible *child = gtk_accessible_get_first_accessible_child (accessible);

  gtk_at_context_realize (ctx);
  id = gtk_accesskit_context_get_id (accesskit_ctx);
  node = gtk_accesskit_context_build_node (accesskit_ctx, self->node_classes);
  accesskit_tree_update_push_node (update, id, node);

  while (child)
    {
      add_subtree_to_update (self, update, child);
      child = gtk_accessible_get_next_accessible_sibling (child);
    }
}

/* TODO */

GtkAccessKitRoot *
gtk_accesskit_root_new (GtkRoot *root_widget)
{
  return g_object_new (GTK_TYPE_ACCESSKIT_ROOT,
                       "root-widget", root_widget,
                       NULL);
}

accesskit_node_id
gtk_accesskit_root_add_context (GtkAccessKitRoot    *self,
                                GtkAccessKitContext *context)
{
  accesskit_node_id id = self->next_id++;
  /* TODO: add to a hash table */
  return id;
}

void
gtk_accesskit_root_remove_context (GtkAccessKitRoot *self, accesskit_node_id id)
{
  /* TODO: remove from a hash table */
}
