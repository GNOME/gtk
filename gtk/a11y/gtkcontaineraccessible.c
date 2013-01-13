/* GTK+ - accessibility implementations
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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
#include "gtkcontaineraccessible.h"
#include "gtkwidgetprivate.h"

struct _GtkContainerAccessiblePrivate
{
  GList *children;
};

G_DEFINE_TYPE (GtkContainerAccessible, gtk_container_accessible, GTK_TYPE_WIDGET_ACCESSIBLE)

static gint
gtk_container_accessible_get_n_children (AtkObject* obj)
{
  GtkWidget *widget;
  GList *children;
  gint count = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return 0;

  children = gtk_container_get_children (GTK_CONTAINER (widget));
  count = g_list_length (children);
  g_list_free (children);

  return count;
}

static AtkObject *
gtk_container_accessible_ref_child (AtkObject *obj,
                                    gint       i)
{
  GList *children, *tmp_list;
  AtkObject  *accessible;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  children = gtk_container_get_children (GTK_CONTAINER (widget));
  tmp_list = g_list_nth (children, i);
  if (!tmp_list)
    {
      g_list_free (children);
      return NULL;
    }
  accessible = gtk_widget_get_accessible (GTK_WIDGET (tmp_list->data));

  g_list_free (children);
  g_object_ref (accessible);

  return accessible;
}

static void
gtk_container_accessible_add (GtkContainer *container,
                              GtkWidget    *child,
                              gpointer      data)
{
  GtkContainerAccessible *accessible;
  GtkContainerAccessibleClass *klass;

  accessible = GTK_CONTAINER_ACCESSIBLE (_gtk_widget_peek_accessible (GTK_WIDGET (container)));
  if (accessible == NULL)
    return;

  klass = GTK_CONTAINER_ACCESSIBLE_GET_CLASS (accessible);
  if (klass->add == NULL)
    return;

  klass->add (accessible, child);
}
 
static void
gtk_container_accessible_remove (GtkContainer *container,
                                 GtkWidget    *child,
                                 gpointer      data)
{
  GtkContainerAccessible *accessible;
  GtkContainerAccessibleClass *klass;

  accessible = GTK_CONTAINER_ACCESSIBLE (_gtk_widget_peek_accessible (GTK_WIDGET (container)));
  if (accessible == NULL)
    return;

  klass = GTK_CONTAINER_ACCESSIBLE_GET_CLASS (accessible);
  if (klass->remove == NULL)
    return;

  klass->remove (accessible, child);
}

static void
gtk_container_accessible_real_add (GtkContainerAccessible *accessible,
                                   GtkWidget              *widget)
{
  GtkContainer *container;
  AtkObject *atk_child;
  gint index;

  atk_child = gtk_widget_get_accessible (widget);
  container = GTK_CONTAINER (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));

  g_object_notify (G_OBJECT (atk_child), "accessible-parent");
  g_list_free (accessible->priv->children);
  accessible->priv->children = gtk_container_get_children (container);
  index = g_list_index (accessible->priv->children, widget);
  g_signal_emit_by_name (accessible, "children-changed::add", index, atk_child, NULL);
}

static void
gtk_container_accessible_real_remove (GtkContainerAccessible *accessible,
                                      GtkWidget              *widget)
{
  GtkContainer *container;
  AtkObject *atk_child;
  gint index;

  container = GTK_CONTAINER (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));
  atk_child = gtk_widget_get_accessible (widget);
  if (atk_child == NULL)
    return;

  g_object_notify (G_OBJECT (atk_child), "accessible-parent");
  index = g_list_index (accessible->priv->children, widget);
  g_list_free (accessible->priv->children);
  accessible->priv->children = gtk_container_get_children (container);
  if (index >= 0 && index <= g_list_length (accessible->priv->children))
    g_signal_emit_by_name (accessible, "children-changed::remove", index, atk_child, NULL);
}

static void
gtk_container_accessible_real_initialize (AtkObject *obj,
                                          gpointer   data)
{
  GtkContainerAccessible *accessible = GTK_CONTAINER_ACCESSIBLE (obj);

  ATK_OBJECT_CLASS (gtk_container_accessible_parent_class)->initialize (obj, data);

  accessible->priv->children = gtk_container_get_children (GTK_CONTAINER (data));

  g_signal_connect (data, "add", G_CALLBACK (gtk_container_accessible_add), obj);
  g_signal_connect (data, "remove", G_CALLBACK (gtk_container_accessible_remove), obj);

  obj->role = ATK_ROLE_PANEL;
}

static void
gtk_container_accessible_finalize (GObject *object)
{
  GtkContainerAccessible *accessible = GTK_CONTAINER_ACCESSIBLE (object);

  g_list_free (accessible->priv->children);

  G_OBJECT_CLASS (gtk_container_accessible_parent_class)->finalize (object);
}

static void
gtk_container_accessible_class_init (GtkContainerAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_container_accessible_finalize;

  class->get_n_children = gtk_container_accessible_get_n_children;
  class->ref_child = gtk_container_accessible_ref_child;
  class->initialize = gtk_container_accessible_real_initialize;

  klass->add = gtk_container_accessible_real_add;
  klass->remove = gtk_container_accessible_real_remove;

  g_type_class_add_private (klass, sizeof (GtkContainerAccessiblePrivate));
}

static void
gtk_container_accessible_init (GtkContainerAccessible *container)
{
  container->priv = G_TYPE_INSTANCE_GET_PRIVATE (container,
                                                 GTK_TYPE_CONTAINER_ACCESSIBLE,
                                                 GtkContainerAccessiblePrivate);
}
