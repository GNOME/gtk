/* GTK+ - accessibility implementations
 * Copyright 2020 Red Hat, Inc.
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

#include "gtkcompositeaccessible.h"

#include <gtk/gtk.h>

#include "gtkwidgetprivate.h"

G_DEFINE_TYPE (GtkCompositeAccessible, gtk_composite_accessible, GTK_TYPE_WIDGET_ACCESSIBLE)

static int
gtk_composite_accessible_get_n_children (AtkObject *obj)
{
  GtkWidget *widget;
  GtkWidget *child;
  int count = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return 0;

  for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child))
    count++;

  return count;
}

static AtkObject *
gtk_composite_accessible_ref_child (AtkObject *obj,
                                    int        i)
{
  GtkWidget *widget;
  GtkWidget *child;
  int pos;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  for (child = gtk_widget_get_first_child (widget), pos = 0; child && pos < i; child = gtk_widget_get_next_sibling (child), pos++);

  if (child)
    return g_object_ref (gtk_widget_get_accessible (GTK_WIDGET (child)));

  return NULL;
}

static void
gtk_composite_accessible_initialize (AtkObject *obj,
                                     gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_composite_accessible_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_FILLER;
}

static void
gtk_composite_accessible_class_init (GtkCompositeAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gtk_composite_accessible_initialize;
  class->get_n_children = gtk_composite_accessible_get_n_children;
  class->ref_child = gtk_composite_accessible_ref_child;
}

static void
gtk_composite_accessible_init (GtkCompositeAccessible *composite)
{
}
