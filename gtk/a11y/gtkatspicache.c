/* gtkatspicache.c: AT-SPI object cache
 *
 * Copyright 2020  holder
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

#include "gtkatspicacheprivate.h"

#include "gtkdebug.h"

#include "a11y/atspi/atspi-cache.h"

struct _GtkAtSpiCache
{
  GObject parent_instance;

  char *cache_path;
  GDBusConnection *connection;

  GHashTable *contexts;
};

enum
{
  PROP_CACHE_PATH = 1,
  PROP_CONNECTION,

  N_PROPS
};

static GParamSpec *obj_props[N_PROPS];

G_DEFINE_TYPE (GtkAtSpiCache, gtk_at_spi_cache, G_TYPE_OBJECT)

static void
gtk_at_spi_cache_finalize (GObject *gobject)
{
  GtkAtSpiCache *self = GTK_AT_SPI_CACHE (gobject);

  g_clear_pointer (&self->contexts, g_hash_table_unref);
  g_clear_object (&self->connection);
  g_free (self->cache_path);

  G_OBJECT_CLASS (gtk_at_spi_cache_parent_class)->finalize (gobject);
}

static void
gtk_at_spi_cache_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkAtSpiCache *self = GTK_AT_SPI_CACHE (gobject);

  switch (prop_id)
    {
    case PROP_CACHE_PATH:
      g_free (self->cache_path);
      self->cache_path = g_value_dup_string (value);
      break;

    case PROP_CONNECTION:
      g_clear_object (&self->connection);
      self->connection = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
handle_cache_method (GDBusConnection       *connection,
                     const gchar           *sender,
                     const gchar           *object_path,
                     const gchar           *interface_name,
                     const gchar           *method_name,
                     GVariant              *parameters,
                     GDBusMethodInvocation *invocation,
                     gpointer               user_data)
{
  GTK_NOTE (A11Y,
            g_message ("[Cache] Method '%s' on interface '%s' for object '%s' from '%s'\n",
                       method_name, interface_name, object_path, sender));

}

static GVariant *
handle_cache_get_property (GDBusConnection       *connection,
                           const gchar           *sender,
                           const gchar           *object_path,
                           const gchar           *interface_name,
                           const gchar           *property_name,
                           GError               **error,
                           gpointer               user_data)
{
  GVariant *res = NULL;

  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
               "Unknown property '%s'", property_name);

  return res;
}


static const GDBusInterfaceVTable cache_vtable = {
  handle_cache_method,
  handle_cache_get_property,
  NULL,
};

static void
gtk_at_spi_cache_constructed (GObject *gobject)
{
  GtkAtSpiCache *self = GTK_AT_SPI_CACHE (gobject);

  g_assert (self->connection);
  g_assert (self->cache_path);

  g_dbus_connection_register_object (self->connection,
                                     self->cache_path,
                                     (GDBusInterfaceInfo *) &atspi_cache_interface,
                                     &cache_vtable,
                                     self,
                                     NULL,
                                     NULL);

  GTK_NOTE (A11Y, g_message ("Cache registered at %s", self->cache_path));

  G_OBJECT_CLASS (gtk_at_spi_cache_parent_class)->constructed (gobject);
}

static void
gtk_at_spi_cache_class_init (GtkAtSpiCacheClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gtk_at_spi_cache_constructed;
  gobject_class->set_property = gtk_at_spi_cache_set_property;
  gobject_class->finalize = gtk_at_spi_cache_finalize;

  obj_props[PROP_CACHE_PATH] =
    g_param_spec_string ("cache-path", NULL, NULL,
                         NULL,
                         G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  obj_props[PROP_CONNECTION] =
    g_param_spec_object ("connection", NULL, NULL,
                         G_TYPE_DBUS_CONNECTION,
                         G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

static void
gtk_at_spi_cache_init (GtkAtSpiCache *self)
{
  self->contexts = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          g_free,
                                          NULL);
}

GtkAtSpiCache *
gtk_at_spi_cache_new (GDBusConnection *connection,
                      const char *cache_path)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (cache_path != NULL, NULL);

  return g_object_new (GTK_TYPE_AT_SPI_CACHE,
                       "connection", connection,
                       "cache-path", cache_path,
                       NULL);
}

void
gtk_at_spi_cache_add_context (GtkAtSpiCache *self,
                              const char *path,
                              GtkATContext *context)
{
  g_return_if_fail (GTK_IS_AT_SPI_CACHE (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (GTK_IS_AT_CONTEXT (context));

  if (g_hash_table_contains (self->contexts, path))
    return;

  g_hash_table_insert (self->contexts, g_strdup (path), context);
}

GtkATContext *
gtk_at_spi_cache_get_context (GtkAtSpiCache *self,
                              const char *path)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_CACHE (self), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return g_hash_table_lookup (self->contexts, path);
}
