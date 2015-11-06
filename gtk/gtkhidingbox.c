/*
 * Copyright (C) 2015 Rafał Lużyński <digitalfreak@lingonborough.com>
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

#include "gtkhidingboxprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkintl.h"
#include "gtksizerequest.h"
#include "gtkbuildable.h"

struct _GtkHidingBoxPrivate
{
  GList *children;
  gint16 spacing;
  gint inverted :1;
};

static void
gtk_hiding_box_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const gchar  *type)
{
  if (!type)
    gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (GTK_HIDING_BOX (buildable), type);
}

static void
gtk_hiding_box_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = gtk_hiding_box_buildable_add_child;
}

G_DEFINE_TYPE_WITH_CODE (GtkHidingBox, gtk_hiding_box, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkHidingBox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_hiding_box_buildable_init))

enum {
  PROP_0,
  PROP_SPACING,
  PROP_INVERTED,
  LAST_PROP
};

static GParamSpec *hiding_box_properties[LAST_PROP] = { NULL, };

static void
gtk_hiding_box_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkHidingBox *box = GTK_HIDING_BOX (object);

  switch (prop_id)
    {
    case PROP_SPACING:
      gtk_hiding_box_set_spacing (box, g_value_get_int (value));
      break;
    case PROP_INVERTED:
      gtk_hiding_box_set_inverted (box, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_hiding_box_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkHidingBox *box = GTK_HIDING_BOX (object);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);

  switch (prop_id)
    {
    case PROP_SPACING:
      g_value_set_int (value, priv->spacing);
      break;
    case PROP_INVERTED:
      g_value_set_boolean (value, priv->inverted);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_hiding_box_add (GtkContainer *container,
                    GtkWidget    *widget)
{
  GtkHidingBox *box = GTK_HIDING_BOX (container);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);

  priv->children = g_list_append (priv->children, widget);
  gtk_widget_set_parent (widget, GTK_WIDGET (box));
}

static void
gtk_hiding_box_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
  GList *child;
  GtkHidingBox *box = GTK_HIDING_BOX (container);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);

  for (child = priv->children; child != NULL; child = child->next)
    {
      if (child->data == widget)
        {
          gboolean was_visible = gtk_widget_get_visible (widget) &&
                                 gtk_widget_get_child_visible (widget);

          gtk_widget_unparent (widget);
          priv->children = g_list_delete_link (priv->children, child);

          if (was_visible)
            gtk_widget_queue_resize (GTK_WIDGET (container));

          break;
        }
    }
}

static void
gtk_hiding_box_forall (GtkContainer *container,
                       gboolean      include_internals,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  GtkHidingBox *box = GTK_HIDING_BOX (container);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
  GtkWidget *child;
  GList *children;

  children = priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      (* callback) (child, callback_data);
    }
}

static void
update_children_visibility (GtkHidingBox     *box,
                            GtkAllocation    *allocation,
                            GtkRequestedSize *sizes,
                            gint             *children_size,
                            gint             *n_visible_children,
                            gint             *n_visible_children_expanding)
{
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
  GtkWidget *child_widget;
  GList *child;
  GtkRequestedSize *sizes_temp;
  gint i;
  GList *children;
  gboolean allocate_more_children = TRUE;

  *n_visible_children = 0;
  *n_visible_children_expanding = 0;
  *children_size = -priv->spacing;
  children = g_list_copy (priv->children);
  sizes_temp = g_newa (GtkRequestedSize, g_list_length (priv->children));
  if (priv->inverted)
    children = g_list_reverse (children);

  /* Retrieve desired size for visible children. */
  for (i = 0, child = children; child != NULL; i++, child = child->next)
    {
      child_widget = GTK_WIDGET (child->data);
      if (!gtk_widget_get_visible (child_widget) || !allocate_more_children)
        {
          gtk_widget_set_child_visible (child_widget, FALSE);
          continue;
        }

      gtk_widget_get_preferred_width_for_height (child_widget,
                                                 allocation->height,
                                                 &sizes_temp[i].minimum_size,
                                                 &sizes_temp[i].natural_size);
      /* Assert the api is working properly */
      if (sizes_temp[i].minimum_size < 0)
        g_error ("GtkHidingBox child %s minimum width: %d < 0 for height %d",
                 gtk_widget_get_name (child_widget),
                 sizes_temp[i].minimum_size, allocation->height);

      if (sizes_temp[i].natural_size < sizes_temp[i].minimum_size)
        g_error ("GtkHidingBox child %s natural width: %d < minimum %d for height %d",
                 gtk_widget_get_name (child_widget),
                 sizes_temp[i].natural_size, sizes_temp[i].minimum_size,
                 allocation->height);

      *children_size += sizes_temp[i].minimum_size + priv->spacing;
      sizes_temp[i].data = child_widget;

      if (*children_size > allocation->width)
        {
          gtk_widget_set_child_visible (child_widget, FALSE);
          allocate_more_children = FALSE;
          continue;
        }

      if (gtk_widget_get_hexpand (child_widget))
        (*n_visible_children_expanding)++;
      (*n_visible_children)++;
      gtk_widget_set_child_visible (child_widget, TRUE);
    }

  for (i = 0; i < *n_visible_children; i++)
    {
      if (priv->inverted)
        {
          sizes[*n_visible_children - i - 1].minimum_size = sizes_temp[i].minimum_size;
          sizes[*n_visible_children - i - 1].natural_size = sizes_temp[i].natural_size;
        }
      else
        {
          sizes[i].minimum_size = sizes_temp[i].minimum_size;
          sizes[i].natural_size = sizes_temp[i].natural_size;
        }
    }

  g_list_free (children);
}

static void
gtk_hiding_box_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GtkHidingBox *box = GTK_HIDING_BOX (widget);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
  GtkTextDirection direction;
  GtkAllocation child_allocation;
  GtkRequestedSize *sizes;
  gint extra_space = 0;
  gint x = 0;
  gint i;
  GList *child;
  GtkWidget *child_widget;
  gint spacing = priv->spacing;
  gint n_visible_children = 0;
  gint n_visible_children_expanding = 0;
  gint children_size = 0;

  gtk_widget_set_allocation (widget, allocation);

  sizes = g_newa (GtkRequestedSize, g_list_length (priv->children));
  update_children_visibility (box, allocation, sizes, &children_size,
                              &n_visible_children, &n_visible_children_expanding);

  /* If there is no visible child, simply return. */
  if (n_visible_children == 0)
    return;

  direction = gtk_widget_get_direction (widget);

  /* Bring children up to allocation width first */
  extra_space = allocation->width - (n_visible_children - 1) * spacing - children_size;
  extra_space = gtk_distribute_natural_allocation (MAX (0, extra_space), n_visible_children, sizes);

  /* Distribute extra space on the expanding children */
  if (n_visible_children > 1)
    extra_space = extra_space / MAX (1, n_visible_children_expanding);

  x = allocation->x;
  for (i = 0, child = priv->children; child != NULL; child = child->next)
    {

      child_widget = GTK_WIDGET (child->data);
      if (!gtk_widget_get_child_visible (child_widget))
        continue;

      child_allocation.x = x;
      child_allocation.y = allocation->y;
      if (gtk_widget_get_hexpand (child_widget))
        child_allocation.width = sizes[i].minimum_size + extra_space;
      else
        child_allocation.width = sizes[i].minimum_size;

      child_allocation.height = allocation->height;
      if (direction == GTK_TEXT_DIR_RTL)
        child_allocation.x = allocation->x + allocation->width - (child_allocation.x - allocation->x) - child_allocation.width;

      /* Let this child be visible */
      gtk_widget_size_allocate (child_widget, &child_allocation);
      x += child_allocation.width + spacing;
      ++i;
    }

  _gtk_widget_set_simple_clip (widget, NULL);
}

static void
gtk_hiding_box_get_preferred_width (GtkWidget *widget,
                                    gint      *minimum_width,
                                    gint      *natural_width)
{
  GtkHidingBox *box = GTK_HIDING_BOX (widget);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
  gint child_minimum_width;
  gint child_natural_width;
  GList *child;
  gint n_visible_children;
  gboolean have_min = FALSE;
  GList *children;

  *minimum_width = 0;
  *natural_width = 0;

  children = g_list_copy (priv->children);
  if (priv->inverted)
    children = g_list_reverse (children);

  n_visible_children = 0;
  for (child = children; child != NULL; child = child->next)
    {
      if (!gtk_widget_is_visible (child->data))
        continue;

      ++n_visible_children;
      gtk_widget_get_preferred_width (child->data, &child_minimum_width, &child_natural_width);
      /* Minimum is a minimum of the first visible child */
      if (!have_min)
        {
          *minimum_width = child_minimum_width;
          have_min = TRUE;
        }
      /* Natural is a sum of all visible children */
      *natural_width += child_natural_width;
    }

  /* Natural must also include the spacing */
  if (priv->spacing && n_visible_children > 1)
    *natural_width += priv->spacing * (n_visible_children - 1);

  g_list_free (children);
}

static void
gtk_hiding_box_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum_height,
                                     gint      *natural_height)
{
  GtkHidingBox *box = GTK_HIDING_BOX (widget);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
  gint child_minimum_height;
  gint child_natural_height;
  GList *child;

  *minimum_height = 0;
  *natural_height = 0;
  for (child = priv->children; child != NULL; child = child->next)
    {
      if (!gtk_widget_is_visible (child->data))
        continue;

      gtk_widget_get_preferred_height (child->data, &child_minimum_height, &child_natural_height);
      *minimum_height = MAX (*minimum_height, child_minimum_height);
      *natural_height = MAX (*natural_height, child_natural_height);
    }
}

static void
gtk_hiding_box_init (GtkHidingBox *box)
{
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);

  gtk_widget_set_has_window (GTK_WIDGET (box), FALSE);
  priv->spacing = 0;
  priv->inverted = FALSE;
}

static void
gtk_hiding_box_class_init (GtkHidingBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->set_property = gtk_hiding_box_set_property;
  object_class->get_property = gtk_hiding_box_get_property;

  widget_class->size_allocate = gtk_hiding_box_size_allocate;
  widget_class->get_preferred_width = gtk_hiding_box_get_preferred_width;
  widget_class->get_preferred_height = gtk_hiding_box_get_preferred_height;

  container_class->add = gtk_hiding_box_add;
  container_class->remove = gtk_hiding_box_remove;
  container_class->forall = gtk_hiding_box_forall;

  hiding_box_properties[PROP_SPACING] =
           g_param_spec_int ("spacing",
                             _("Spacing"),
                             _("The amount of space between children"),
                             0, G_MAXINT, 0,
                             G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  hiding_box_properties[PROP_INVERTED] =
           g_param_spec_int ("inverted",
                             _("Direction of hiding children inverted"),
                             P_("If false the container will start hiding widgets from the end when there is not enough space, and the oposite in case inverted is true."),
                             0, G_MAXINT, 0,
                             G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, hiding_box_properties);
}

/**
 * gtk_hiding_box_new:
 *
 * Creates a new #GtkHidingBox.
 *
 * Returns: a new #GtkHidingBox.
 **/
GtkWidget *
gtk_hiding_box_new (void)
{
  return g_object_new (GTK_TYPE_HIDING_BOX, NULL);
}

/**
 * gtk_hiding_box_set_spacing:
 * @box: a #GtkHidingBox
 * @spacing: the number of pixels to put between children
 *
 * Sets the #GtkHidingBox:spacing property of @box, which is the
 * number of pixels to place between children of @box.
 */
void
gtk_hiding_box_set_spacing (GtkHidingBox *box,
                            gint          spacing)
{
  GtkHidingBoxPrivate *priv ;

  g_return_if_fail (GTK_IS_HIDING_BOX (box));

  priv = gtk_hiding_box_get_instance_private (box);

  if (priv->spacing != spacing)
    {
      priv->spacing = spacing;

      g_object_notify (G_OBJECT (box), "spacing");

      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}

/**
 * gtk_hiding_box_get_spacing:
 * @box: a #GtkHidingBox
 *
 * Gets the value set by gtk_hiding_box_set_spacing().
 *
 * Returns: spacing between children
 **/
gint
gtk_hiding_box_get_spacing (GtkHidingBox *box)
{
  GtkHidingBoxPrivate *priv ;

  g_return_val_if_fail (GTK_IS_HIDING_BOX (box), 0);

  priv = gtk_hiding_box_get_instance_private (box);

  return priv->spacing;
}


void
gtk_hiding_box_set_inverted (GtkHidingBox *box,
                             gboolean      inverted)
{
  GtkHidingBoxPrivate *priv ;

  g_return_if_fail (GTK_IS_HIDING_BOX (box));

  priv = gtk_hiding_box_get_instance_private (box);

  if (priv->inverted != inverted)
    {
      priv->inverted = inverted != FALSE;

      g_object_notify (G_OBJECT (box), "inverted");

      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}

gboolean
gtk_hiding_box_get_inverted (GtkHidingBox *box)
{
  GtkHidingBoxPrivate *priv ;

  g_return_val_if_fail (GTK_IS_HIDING_BOX (box), 0);

  priv = gtk_hiding_box_get_instance_private (box);

  return priv->inverted;
}
