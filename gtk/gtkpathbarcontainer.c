/* gtkpathbarcontainer.c
 *
 * Copyright (C) 2015 Red Hat
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Carlos Soriano <csoriano@gnome.org>
 */

#include "config.h"

#include "gtkpathbarcontainerprivate.h"
#include "gtkbuildable.h"
#include "gtkwidget.h"
#include "gtkmenubutton.h"
#include "gtksizerequest.h"
#include "gtkhidingboxprivate.h"
#include "gtkwidgetprivate.h"
#include "glib-object.h"

struct _GtkPathBarContainerPrivate
{
  GtkWidget *overflow_button;
  GtkWidget *path_box;
};

static GtkBuildableIface *parent_buildable_iface;

static GObject *
buildable_get_internal_child (GtkBuildable *buildable,
                              GtkBuilder   *builder,
                              const gchar  *childname)
{
  if (g_strcmp0 (childname, "overflow_button") == 0)
    return G_OBJECT (gtk_path_bar_container_get_overflow_button (GTK_PATH_BAR_CONTAINER (buildable)));
  if (g_strcmp0 (childname, "path_box") == 0)
    return G_OBJECT (gtk_path_bar_container_get_path_box (GTK_PATH_BAR_CONTAINER (buildable)));

  return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

static void
buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->get_internal_child = buildable_get_internal_child;
}

G_DEFINE_TYPE_WITH_CODE (GtkPathBarContainer, gtk_path_bar_container, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkPathBarContainer)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_init))
static void
container_forall (GtkContainer *container,
                  gboolean      include_internals,
                  GtkCallback   callback,
                  gpointer      callback_data)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (GTK_PATH_BAR_CONTAINER (container));

  if (include_internals)
    {
      (* callback) (priv->overflow_button, callback_data);
      (* callback) (priv->path_box, callback_data);
    }
}

static GtkSizeRequestMode
get_request_mode (GtkWidget *self)
{
  return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
size_allocate (GtkWidget     *self,
               GtkAllocation *allocation)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (GTK_PATH_BAR_CONTAINER (self));
  gint path_min_width;
  gint path_nat_width;
  gint overflow_button_min_width;
  GtkAllocation path_container_allocation;
  GtkAllocation overflow_button_allocation;
  GtkTextDirection direction;
  gboolean overflow;
  GList *overflow_children;
  GList *children;

  gtk_widget_set_allocation (self, allocation);

  children = gtk_container_get_children (GTK_CONTAINER (priv->path_box));
  gtk_widget_set_child_visible (priv->overflow_button, FALSE);

  if (g_list_length (children) == 0)
    return;

  gtk_widget_get_preferred_width (priv->path_box, &path_min_width, &path_nat_width);

  /* Try to allocate all children with our allocation so we can request if there
   * is some overflow children */
  path_container_allocation.x = allocation->x;
  path_container_allocation.width = allocation->width;
  path_container_allocation.y = allocation->y;
  path_container_allocation.height = allocation->height;
  gtk_widget_size_allocate (priv->path_box, &path_container_allocation);
  gtk_widget_get_preferred_width (priv->overflow_button, &overflow_button_min_width, NULL);
  overflow_children = gtk_hiding_box_get_overflow_children (GTK_HIDING_BOX (priv->path_box));
  overflow = overflow_children != NULL;
  g_list_free (overflow_children);

  direction = gtk_widget_get_direction (self);

  path_container_allocation.x = allocation->x;
  path_container_allocation.width = allocation->width;
  if (overflow)
    {
      if (direction == GTK_TEXT_DIR_LTR)
        {
          path_container_allocation.x = allocation->x + overflow_button_min_width;
          path_container_allocation.width = allocation->width - overflow_button_min_width;

          overflow_button_allocation.y = allocation->y;
          overflow_button_allocation.height = allocation->height;
          overflow_button_allocation.width = overflow_button_min_width;
          overflow_button_allocation.x = allocation->x;
          gtk_widget_set_child_visible (priv->overflow_button, TRUE);
          gtk_widget_size_allocate (priv->overflow_button, &overflow_button_allocation);
        }
      else
        {
          path_container_allocation.width = allocation->width - overflow_button_min_width;
        }
    }

  path_container_allocation.y = allocation->y;
  path_container_allocation.height = allocation->height;
  gtk_widget_size_allocate (priv->path_box, &path_container_allocation);

  if (overflow && direction == GTK_TEXT_DIR_RTL)
    {
      overflow_button_allocation.y = allocation->y;
      overflow_button_allocation.height = allocation->height;
      overflow_button_allocation.width = overflow_button_min_width;
      overflow_button_allocation.x = path_container_allocation.x +
                                     path_container_allocation.width;
      gtk_widget_set_child_visible (priv->overflow_button, TRUE);
      gtk_widget_size_allocate (priv->overflow_button, &overflow_button_allocation);
    }

  _gtk_widget_set_simple_clip (self, NULL);

  g_list_free (children);
}

static void
get_preferred_width (GtkWidget *self,
                     gint      *minimum_width,
                     gint      *natural_width)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (GTK_PATH_BAR_CONTAINER (self));

  gtk_widget_get_preferred_width (priv->path_box, minimum_width, natural_width);
}

static void
get_preferred_height (GtkWidget *self,
                      gint      *minimum_height,
                      gint      *natural_height)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (GTK_PATH_BAR_CONTAINER (self));

  gtk_widget_get_preferred_height (priv->path_box, minimum_height, natural_height);
}

static void
widget_destroy (GtkWidget *object)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (GTK_PATH_BAR_CONTAINER (object));

  if (priv->overflow_button && priv->path_box)
    {
      gtk_widget_unparent (priv->overflow_button);
      gtk_widget_unparent (priv->path_box);

      priv->overflow_button = NULL;
      priv->path_box = NULL;
    }

  GTK_WIDGET_CLASS (gtk_path_bar_container_parent_class)->destroy (object);
}

static void
gtk_path_bar_container_class_init (GtkPathBarContainerClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  widget_class->get_request_mode = get_request_mode;
  widget_class->get_preferred_width = get_preferred_width;
  widget_class->get_preferred_height = get_preferred_height;
  widget_class->size_allocate = size_allocate;
  widget_class->destroy = widget_destroy;

  // Neccesary to draw and realize children
  container_class->forall = container_forall;
}

static void
gtk_path_bar_container_init (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  priv->overflow_button = gtk_menu_button_new ();
  priv->path_box = gtk_hiding_box_new ();

  gtk_widget_set_parent (priv->overflow_button, GTK_WIDGET (self));
  gtk_widget_set_parent (priv->path_box, GTK_WIDGET (self));
}

GtkWidget *
gtk_path_bar_container_new (void)
{
  return g_object_new (GTK_TYPE_PATH_BAR_CONTAINER, NULL);
}

GtkWidget *
gtk_path_bar_container_get_overflow_button (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv;

  g_return_val_if_fail (GTK_IS_PATH_BAR_CONTAINER (self), NULL);

  priv = gtk_path_bar_container_get_instance_private (self);

  return priv->overflow_button;
}

GtkWidget *
gtk_path_bar_container_get_path_box (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv;

  g_return_val_if_fail (GTK_IS_PATH_BAR_CONTAINER (self), NULL);

  priv = gtk_path_bar_container_get_instance_private (self);

  return priv->path_box;
}
