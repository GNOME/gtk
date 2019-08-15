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

#include "gtkheaderbaraccessible.h"

#include "gtkcontainerprivate.h"

G_DEFINE_TYPE (GtkHeaderBarAccessible, gtk_header_bar_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE)

static void
count_widget (GtkWidget *widget,
              gint      *count)
{
  (*count)++;
}

static gint
gtk_header_bar_accessible_get_n_children (AtkObject* obj)
{
  GtkWidget *widget;
  gint count = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return 0;

  gtk_container_forall (GTK_CONTAINER (widget), (GtkCallback) count_widget, &count);
  return count;
}

static AtkObject *
gtk_header_bar_accessible_ref_child (AtkObject *obj,
                                     gint       i)
{
  GList *children, *tmp_list;
  AtkObject  *accessible;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  children = gtk_container_get_all_children (GTK_CONTAINER (widget));
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
gtk_header_bar_accessible_class_init (GtkHeaderBarAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_n_children = gtk_header_bar_accessible_get_n_children;
  class->ref_child = gtk_header_bar_accessible_ref_child;
}

static void
gtk_header_bar_accessible_init (GtkHeaderBarAccessible *header_bar)
{
}

