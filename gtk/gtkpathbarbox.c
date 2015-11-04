/*
 * Copyright (C) 2016 Carlos Soriano <csoriano@gnome.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include "glib.h"

#include "gtkpathbarboxprivate.h"
#include "gtkpathbarcontainer.h"
#include "gtkwidgetprivate.h"
#include "gtkintl.h"
#include "gtksizerequest.h"
#include "gtkbuildable.h"
#include "gtkrevealer.h"

struct _GtkPathBarBoxPrivate
{
  GList *children;
};

static GtkBuildableIface *parent_buildable_iface;

static void
buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
}

G_DEFINE_TYPE_WITH_CODE (GtkPathBarBox, gtk_path_bar_box, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkPathBarBox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_init))
static void
gtk_path_bar_box_forall (GtkContainer *container,
                  gboolean      include_internals,
                  GtkCallback   callback,
                  gpointer      callback_data)
{
  GtkPathBarBox *self = GTK_PATH_BAR_BOX (container);
  GtkPathBarBoxPrivate *priv = gtk_path_bar_box_get_instance_private (self);
  GList *child;

  for (child = priv->children; child != NULL; child = child->next)
      (* callback) (child->data, callback_data);
}

static void
gtk_path_bar_box_add (GtkContainer *container,
               GtkWidget    *widget)
{
  GtkPathBarBox *self = GTK_PATH_BAR_BOX (container);
  GtkPathBarBoxPrivate *priv = gtk_path_bar_box_get_instance_private (self);

  priv->children = g_list_append (priv->children, widget);
  gtk_widget_set_parent (widget, GTK_WIDGET (self));

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gtk_path_bar_box_remove (GtkContainer *container,
                  GtkWidget    *widget)
{
  GtkPathBarBox *self = GTK_PATH_BAR_BOX (container);
  GtkPathBarBoxPrivate *priv = gtk_path_bar_box_get_instance_private (self);

  priv->children = g_list_remove (priv->children, widget);
  gtk_widget_unparent (widget);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static GtkSizeRequestMode
get_request_mode (GtkWidget *self)
{
  return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
gtk_path_bar_box_size_allocate (GtkWidget     *widget,
                                GtkAllocation *allocation)
{
  GtkPathBarBox *self = GTK_PATH_BAR_BOX (widget);
  GList *children;
  GList *child;
  GtkRequisition child_available_size;
  GtkRequestedSize *sizes;
  GtkAllocation child_allocation;
  gint available_size;
  gint n_visible_children = 0;
  gint current_x = allocation->x;
  gint i;
  GtkRequisition minimum_size;
  GtkRequisition natural_size;
  GtkRequisition distributed_size;

  gtk_widget_get_preferred_size (widget, &minimum_size, &natural_size);

  gtk_widget_set_allocation (widget, allocation);

  available_size = allocation->width;
  children = gtk_container_get_children (GTK_CONTAINER (self));
  sizes = g_newa (GtkRequestedSize, g_list_length (children));

  for (child = children, i = 0; child != NULL; child = g_list_next (child), i++)
    {
      if (!gtk_widget_get_visible (child->data))
        continue;

      gtk_widget_get_preferred_width_for_height (child->data,
                                                 allocation->height,
                                                 &sizes[i].minimum_size,
                                                 &sizes[i].natural_size);
      sizes[i].data = child->data;
      available_size -= sizes[i].minimum_size;
      n_visible_children++;
    }

  gtk_distribute_natural_allocation (MAX (0, available_size),
                                     n_visible_children, sizes);

  for (child = children, i = 0; child != NULL; child = g_list_next (child), i++)
    {
      if (!gtk_widget_get_visible (child->data))
        continue;

      child_available_size.width = sizes[i].minimum_size;
      child_available_size.height = allocation->height;

      if (GTK_IS_PATH_BAR_CONTAINER (child->data))
        {
          gtk_path_bar_container_adapt_to_size (GTK_PATH_BAR_CONTAINER (child->data),
                                                &child_available_size);
          gtk_path_bar_container_get_preferred_size_for_requisition (GTK_WIDGET (child->data),
                                                                     &child_available_size,
                                                                     &minimum_size,
                                                                     &natural_size,
                                                                     &distributed_size);

          sizes[i].minimum_size = MIN (child_available_size.width, distributed_size.width);
        }

      child_allocation.x = current_x;
      child_allocation.y = allocation->y;
      child_allocation.width = sizes[i].minimum_size;
      child_allocation.height = allocation->height;

      gtk_widget_size_allocate (child->data, &child_allocation);

      current_x += sizes[i].minimum_size;
    }
}

static void
gtk_path_bar_box_get_preferred_width (GtkWidget *widget,
                                      gint      *minimum_width,
                                      gint      *natural_width)
{
  GtkPathBarBox *self = GTK_PATH_BAR_BOX (widget);
  GtkPathBarBoxPrivate *priv = gtk_path_bar_box_get_instance_private (self);
  gint child_minimum_width;
  gint child_natural_width;
  GList *child;

  for (child = priv->children; child != NULL; child = child->next)
    {
      if (!gtk_widget_is_visible (child->data))
        continue;

      gtk_widget_get_preferred_width (GTK_WIDGET (child->data),
                                      &child_minimum_width,
                                      &child_natural_width);

      *minimum_width = MAX (*minimum_width, child_minimum_width);
      *natural_width = MAX (*natural_width, child_natural_width);
    }
}

static void
gtk_path_bar_box_get_preferred_width_for_height (GtkWidget *widget,
                                                 gint       height,
                                                 gint      *minimum_width_out,
                                                 gint      *natural_width_out)
{
  gtk_path_bar_box_get_preferred_width (widget, minimum_width_out, natural_width_out);
}

static void
gtk_path_bar_box_get_preferred_height (GtkWidget *widget,
                                       gint      *minimum_height,
                                       gint      *natural_height)
{
  GtkPathBarBox *self = GTK_PATH_BAR_BOX (widget);
  GtkPathBarBoxPrivate *priv = gtk_path_bar_box_get_instance_private (self);
  gint child_minimum_height;
  gint child_natural_height;
  GList *child;

  for (child = priv->children; child != NULL; child = child->next)
    {
      if (!gtk_widget_is_visible (child->data))
        continue;

      gtk_widget_get_preferred_height (child->data,
                                       &child_minimum_height,
                                       &child_natural_height);
      *minimum_height = MAX (*minimum_height, child_minimum_height);
      *natural_height = MAX (*natural_height, child_natural_height);
    }
}

static void
gtk_path_bar_box_init (GtkPathBarBox *self)
{
  GtkPathBarBoxPrivate *priv = gtk_path_bar_box_get_instance_private (self);

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (self), TRUE);

  priv->children = NULL;
}

static void
gtk_path_bar_box_class_init (GtkPathBarBoxClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  widget_class->size_allocate = gtk_path_bar_box_size_allocate;
  widget_class->get_request_mode = get_request_mode;
  widget_class->get_preferred_width = gtk_path_bar_box_get_preferred_width;
  widget_class->get_preferred_height = gtk_path_bar_box_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_path_bar_box_get_preferred_width_for_height;

  container_class->forall = gtk_path_bar_box_forall;
  container_class->add = gtk_path_bar_box_add;
  container_class->remove = gtk_path_bar_box_remove;
}

GtkWidget *
gtk_path_bar_box_new (void)
{
  return g_object_new (GTK_TYPE_PATH_BAR_BOX, NULL);
}

