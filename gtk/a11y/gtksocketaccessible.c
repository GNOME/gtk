/* GTK+ - accessibility implementations
 * Copyright 2019 Samuel Thibault <sthibault@hypra.fr>
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

#include <gtk/gtk.h>
#include "gtksocketaccessible.h"

/* We can not make GtkSocketAccessible inherit both from GtkContainerAccessible
 * and GtkSocket, so we make it the atk parent of an AtkSocket */

struct _GtkSocketAccessiblePrivate
{
  AtkObject *accessible_socket;
};

G_DEFINE_TYPE_WITH_CODE (GtkSocketAccessible, gtk_socket_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkSocketAccessible))

static AtkObject*
gtk_socket_accessible_ref_child (AtkObject *obj, int i)
{
  GtkSocketAccessible *socket = GTK_SOCKET_ACCESSIBLE (obj);

  if (i != 0)
    return NULL;

  return g_object_ref (socket->priv->accessible_socket);
}

static int
gtk_socket_accessible_get_n_children (AtkObject *obj)
{
  return 1;
}

static void
gtk_socket_accessible_finalize (GObject *object)
{
  GtkSocketAccessible *socket = GTK_SOCKET_ACCESSIBLE (object);
  GtkSocketAccessiblePrivate *priv = socket->priv;

  g_clear_object (&priv->accessible_socket);

  G_OBJECT_CLASS (gtk_socket_accessible_parent_class)->finalize (object);
}

static void
gtk_socket_accessible_initialize (AtkObject *socket, gpointer data)
{
  AtkObject *atk_socket;

  ATK_OBJECT_CLASS (gtk_socket_accessible_parent_class)->initialize (socket, data);

  atk_socket = atk_socket_new ();

  GTK_SOCKET_ACCESSIBLE(socket)->priv->accessible_socket = atk_socket;
  atk_object_set_parent (atk_socket, socket);
}

static void
gtk_socket_accessible_class_init (GtkSocketAccessibleClass *klass)
{
  GtkContainerAccessibleClass *container_class = (GtkContainerAccessibleClass*)klass;
  AtkObjectClass              *atk_class       = ATK_OBJECT_CLASS (klass);
  GObjectClass                *gobject_class   = G_OBJECT_CLASS (klass);

  container_class->add_gtk    = NULL;
  container_class->remove_gtk = NULL;

  atk_class->initialize     = gtk_socket_accessible_initialize;
  atk_class->get_n_children = gtk_socket_accessible_get_n_children;
  atk_class->ref_child      = gtk_socket_accessible_ref_child;

  gobject_class->finalize = gtk_socket_accessible_finalize;
}

static void
gtk_socket_accessible_init (GtkSocketAccessible *socket)
{
  socket->priv = gtk_socket_accessible_get_instance_private (socket);
}

void
gtk_socket_accessible_embed (GtkSocketAccessible *socket, gchar *path)
{
  atk_socket_embed (ATK_SOCKET (socket->priv->accessible_socket), path);
}
