/* GTK+ - accessibility implementations
 * Copyright (C) 2016  Timm Bäder <mail@baedert.org>
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

#include <string.h>
#include <gtk/gtk.h>
#include "gtkstackaccessible.h"
#include "gtkwidgetprivate.h"


G_DEFINE_TYPE (GtkStackAccessible, gtk_stack_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE)

static AtkObject*
gtk_stack_accessible_ref_child (AtkObject *obj,
                                int        i)
{
  GtkWidget *stack = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  GtkWidget *visible_child;

  if (stack == NULL)
    return NULL;

  if (i != 0)
    return NULL;

  visible_child = gtk_stack_get_visible_child (GTK_STACK (stack));

  if (visible_child == NULL)
    return NULL;

  return g_object_ref (gtk_widget_get_accessible (visible_child));
}

static int
gtk_stack_accessible_get_n_children (AtkObject *obj)
{
  GtkWidget *stack = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));

  if (stack == NULL)
    return 0;

  if (gtk_stack_get_visible_child (GTK_STACK (stack)))
    return 1;

  return 0;
}

static void
gtk_stack_accessible_class_init (GtkStackAccessibleClass *klass)
{
  AtkObjectClass *class                        = ATK_OBJECT_CLASS (klass);
  GtkContainerAccessibleClass *container_class = (GtkContainerAccessibleClass*)klass;

  class->get_n_children = gtk_stack_accessible_get_n_children;
  class->ref_child      = gtk_stack_accessible_ref_child;
  /*
   * As we report the stack as having only the visible child,
   * we are not interested in add and remove signals
   */
  container_class->add_gtk    = NULL;
  container_class->remove_gtk = NULL;
}

static void
gtk_stack_accessible_init (GtkStackAccessible *bar) {}


void
gtk_stack_accessible_update_visible_child (GtkStack  *stack,
                                           GtkWidget *old_visible_child,
                                           GtkWidget *new_visible_child)
{
  AtkObject *stack_accessible = _gtk_widget_peek_accessible (GTK_WIDGET (stack));

  if (stack_accessible == NULL)
    return;

  if (old_visible_child)
    {
      AtkObject *accessible = gtk_widget_get_accessible (old_visible_child);
      g_object_notify (G_OBJECT (accessible), "accessible-parent");
      g_signal_emit_by_name (stack_accessible, "children-changed::remove", 0, accessible, NULL);
    }

  if (new_visible_child)
    {
      AtkObject *accessible = gtk_widget_get_accessible (new_visible_child);
      g_object_notify (G_OBJECT (accessible), "accessible-parent");
      g_signal_emit_by_name (stack_accessible, "children-changed::add", 0, accessible, NULL);
    }
}



