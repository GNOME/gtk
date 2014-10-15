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

#include "gtkcontaineraccessible.h"
#include "gtkcontaineraccessibleprivate.h"

#include <gtk/gtk.h>

#include "gtkwidgetprivate.h"

struct _GtkContainerAccessiblePrivate
{
  GList *children;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkContainerAccessible, gtk_container_accessible, GTK_TYPE_WIDGET_ACCESSIBLE)

static void
count_widget (GtkWidget *widget,
              gint      *count)
{
  (*count)++;
}

static gint
gtk_container_accessible_get_n_children (AtkObject* obj)
{
  GtkWidget *widget;
  gint count = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return 0;

  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) count_widget, &count);
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

void
_gtk_container_accessible_add (GtkWidget *parent,
                               GtkWidget *child)
{
  GtkContainerAccessible *accessible;
  GtkContainerAccessibleClass *klass;
  AtkObject *obj;

  obj = _gtk_widget_peek_accessible (GTK_WIDGET (parent));
  if (!GTK_IS_CONTAINER_ACCESSIBLE (obj))
    return;

  accessible = GTK_CONTAINER_ACCESSIBLE (obj);
  klass = GTK_CONTAINER_ACCESSIBLE_GET_CLASS (accessible);

  if (klass->add_gtk)
    klass->add_gtk (GTK_CONTAINER (parent), child, obj);
}

void
_gtk_container_accessible_remove (GtkWidget *parent,
                                  GtkWidget *child)
{
  GtkContainerAccessible *accessible;
  GtkContainerAccessibleClass *klass;
  AtkObject *obj;

  obj = _gtk_widget_peek_accessible (GTK_WIDGET (parent));
  if (!GTK_IS_CONTAINER_ACCESSIBLE (obj))
    return;

  accessible = GTK_CONTAINER_ACCESSIBLE (obj);
  klass = GTK_CONTAINER_ACCESSIBLE_GET_CLASS (accessible);

  if (klass->remove_gtk)
    klass->remove_gtk (GTK_CONTAINER (parent), child, obj);
}

static gint
gtk_container_accessible_real_add_gtk (GtkContainer *container,
                                       GtkWidget    *widget,
                                       gpointer      data)
{
  AtkObject *atk_parent;
  AtkObject *atk_child;
  GtkContainerAccessible *accessible;
  gint index;

  atk_parent = ATK_OBJECT (data);
  atk_child = gtk_widget_get_accessible (widget);
  accessible = GTK_CONTAINER_ACCESSIBLE (atk_parent);

  g_list_free (accessible->priv->children);
  accessible->priv->children = gtk_container_get_children (container);
  index = g_list_index (accessible->priv->children, widget);
  _gtk_container_accessible_add_child (accessible, atk_child, index);

  return 1;
}

static gint
gtk_container_accessible_real_remove_gtk (GtkContainer *container,
                                          GtkWidget    *widget,
                                          gpointer      data)
{
  AtkObject* atk_parent;
  AtkObject *atk_child;
  GtkContainerAccessible *accessible;
  gint index;

  atk_parent = ATK_OBJECT (data);
  atk_child = _gtk_widget_peek_accessible (widget);
  if (atk_child == NULL)
    return 1;
  accessible = GTK_CONTAINER_ACCESSIBLE (atk_parent);

  index = g_list_index (accessible->priv->children, widget);
  g_list_free (accessible->priv->children);
  accessible->priv->children = gtk_container_get_children (container);
  if (index >= 0)
    _gtk_container_accessible_remove_child (accessible, atk_child, index);

  return 1;
}

static void
gtk_container_accessible_real_initialize (AtkObject *obj,
                                          gpointer   data)
{
  GtkContainerAccessible *accessible = GTK_CONTAINER_ACCESSIBLE (obj);

  ATK_OBJECT_CLASS (gtk_container_accessible_parent_class)->initialize (obj, data);

  accessible->priv->children = gtk_container_get_children (GTK_CONTAINER (data));

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

  klass->add_gtk = gtk_container_accessible_real_add_gtk;
  klass->remove_gtk = gtk_container_accessible_real_remove_gtk;
}

static void
gtk_container_accessible_init (GtkContainerAccessible *container)
{
  container->priv = gtk_container_accessible_get_instance_private (container);
}

void
_gtk_container_accessible_add_child (GtkContainerAccessible *accessible,
                                     AtkObject              *child,
                                     gint                    index)
{
  g_object_notify (G_OBJECT (child), "accessible-parent");
  g_signal_emit_by_name (accessible, "children-changed::add", index, child, NULL);
}

void
_gtk_container_accessible_remove_child (GtkContainerAccessible *accessible,
                                        AtkObject              *child,
                                        gint                    index)
{
  g_object_notify (G_OBJECT (child), "accessible-parent");
  g_signal_emit_by_name (accessible, "children-changed::remove", index, child, NULL);
}
