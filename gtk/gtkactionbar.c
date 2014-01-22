/*
 * Copyright (c) 2013 - 2014 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include "gtkactionbar.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkwindowprivate.h"
#include "a11y/gtkcontaineraccessible.h"

#include <string.h>

/**
 * SECTION:gtkactionbar
 * @Short_description: A box with a centered child
 * @Title: GtkActionBar
 * @See_also: #GtkBox
 *
 * GtkActionBar is similar to a horizontal #GtkBox, it allows to place
 * children at the start or the end. In addition, it contains an
 * internal centered box which is centered with respect to the full
 * width of the box, even if the children at either side take up
 * different amounts of space.
 *
 */

#define DEFAULT_SPACING 6

struct _GtkActionBarPrivate
{
  GtkWidget *center_widget;
  gint spacing;

  GList *children;
};

typedef struct _Child Child;
struct _Child
{
  GtkWidget *widget;
  GtkPackType pack_type;
};

enum {
  PROP_0,
  PROP_CENTER_WIDGET,
  PROP_SPACING,
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_PACK_TYPE,
  CHILD_PROP_POSITION
};

static void gtk_action_bar_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkActionBar, gtk_action_bar, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkActionBar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_action_bar_buildable_init));

static void
get_css_padding_and_border (GtkWidget *widget,
                            GtkBorder *border)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder tmp;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, border);
  gtk_style_context_get_border (context, state, &tmp);
  border->top += tmp.top;
  border->right += tmp.right;
  border->bottom += tmp.bottom;
  border->left += tmp.left;
}

static void
gtk_action_bar_init (GtkActionBar *bar)
{
  GtkStyleContext *context;
  GtkActionBarPrivate *priv;

  priv = gtk_action_bar_get_instance_private (bar);

  gtk_widget_set_has_window (GTK_WIDGET (bar), FALSE);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (bar), FALSE);

  priv->center_widget = NULL;
  priv->children = NULL;
  priv->spacing = DEFAULT_SPACING;

  context = gtk_widget_get_style_context (GTK_WIDGET (bar));
  gtk_style_context_add_class (context, "action-bar");
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_HORIZONTAL);
}

static gint
count_visible_children (GtkActionBar *bar)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);
  GList *l;
  Child *child;
  gint n;

  n = 0;
  for (l = priv->children; l; l = l->next)
    {
      child = l->data;
      if (gtk_widget_get_visible (child->widget))
        n++;
    }

  return n;
}

static gboolean
add_child_size (GtkWidget      *child,
                GtkOrientation  orientation,
                gint           *minimum,
                gint           *natural)
{
  gint child_minimum, child_natural;

  if (!gtk_widget_get_visible (child))
    return FALSE;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_widget_get_preferred_width (child, &child_minimum, &child_natural);
  else
    gtk_widget_get_preferred_height (child, &child_minimum, &child_natural);

  if (GTK_ORIENTATION_HORIZONTAL == orientation)
    {
      *minimum += child_minimum;
      *natural += child_natural;
    }
  else
    {
      *minimum = MAX (*minimum, child_minimum);
      *natural = MAX (*natural, child_natural);
    }

  return TRUE;
}

static void
gtk_action_bar_get_size (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         gint           *minimum_size,
                         gint           *natural_size)
{
  GtkActionBar *bar = GTK_ACTION_BAR (widget);
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);
  GList *l;
  gint nvis_children;
  gint minimum, natural;
  GtkBorder css_borders;

  minimum = natural = 0;
  nvis_children = 0;

  for (l = priv->children; l; l = l->next)
    {
      Child *child = l->data;

      if (add_child_size (child->widget, orientation, &minimum, &natural))
        nvis_children += 1;
    }

  if (priv->center_widget != NULL &&
      gtk_widget_get_visible (priv->center_widget))
    {
      if (add_child_size (priv->center_widget, orientation, &minimum, &natural))
        nvis_children += 1;
    }

  if (nvis_children > 0 && orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      minimum += nvis_children * priv->spacing;
      natural += nvis_children * priv->spacing;
    }

  get_css_padding_and_border (widget, &css_borders);

  if (GTK_ORIENTATION_HORIZONTAL == orientation)
    {
      minimum += css_borders.left + css_borders.right;
      natural += css_borders.left + css_borders.right;
    }
  else
    {
      minimum += css_borders.top + css_borders.bottom;
      natural += css_borders.top + css_borders.bottom;
    }

  if (minimum_size)
    *minimum_size = minimum;

  if (natural_size)
    *natural_size = natural;
}

static void
gtk_action_bar_compute_size_for_orientation (GtkWidget *widget,
                                             gint       avail_size,
                                             gint      *minimum_size,
                                             gint      *natural_size)
{
  GtkActionBar *bar = GTK_ACTION_BAR (widget);
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);
  GList *children;
  gint required_size = 0;
  gint required_natural = 0;
  gint child_size;
  gint child_natural;
  gint nvis_children;
  GtkBorder css_borders;

  nvis_children = 0;

  for (children = priv->children; children != NULL; children = children->next)
    {
      Child *child = children->data;

      if (gtk_widget_get_visible (child->widget))
        {
          gtk_widget_get_preferred_width_for_height (child->widget,
                                                     avail_size, &child_size, &child_natural);

          required_size += child_size;
          required_natural += child_natural;

          nvis_children += 1;
        }
    }

  if (priv->center_widget != NULL &&
      gtk_widget_get_visible (priv->center_widget))
    {
      gtk_widget_get_preferred_width (priv->center_widget,
                                      &child_size, &child_natural);
      required_size += child_size;
      required_natural += child_natural;
    }

  if (nvis_children > 0)
    {
      required_size += nvis_children * priv->spacing;
      required_natural += nvis_children * priv->spacing;
    }

  get_css_padding_and_border (widget, &css_borders);

  required_size += css_borders.left + css_borders.right;
  required_natural += css_borders.left + css_borders.right;

  if (minimum_size)
    *minimum_size = required_size;

  if (natural_size)
    *natural_size = required_natural;
}

static void
gtk_action_bar_compute_size_for_opposing_orientation (GtkWidget *widget,
                                                      gint       avail_size,
                                                      gint      *minimum_size,
                                                      gint      *natural_size)
{
  GtkActionBar *bar = GTK_ACTION_BAR (widget);
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);
  Child *child;
  GList *children;
  gint nvis_children;
  gint computed_minimum = 0;
  gint computed_natural = 0;
  GtkRequestedSize *sizes;
  GtkPackType packing;
  gint size = 0;
  gint i;
  gint child_size;
  gint child_minimum;
  gint child_natural;
  GtkBorder css_borders;

  nvis_children = count_visible_children (bar);

  if (nvis_children <= 0)
    return;

  sizes = g_newa (GtkRequestedSize, nvis_children);

  /* Retrieve desired size for visible children */
  for (i = 0, children = priv->children; children; children = children->next)
    {
      child = children->data;

      if (gtk_widget_get_visible (child->widget))
        {
          gtk_widget_get_preferred_width (child->widget,
                                          &sizes[i].minimum_size,
                                          &sizes[i].natural_size);

          size -= sizes[i].minimum_size;
          sizes[i].data = child;
          i += 1;
        }
    }

  /* Bring children up to size first */
  size = gtk_distribute_natural_allocation (MAX (0, avail_size), nvis_children, sizes);

  /* Allocate child positions. */
  for (packing = GTK_PACK_START; packing <= GTK_PACK_END; ++packing)
    {
      for (i = 0, children = priv->children; children; children = children->next)
        {
          child = children->data;

          /* If widget is not visible, skip it. */
          if (!gtk_widget_get_visible (child->widget))
            continue;

          /* If widget is packed differently skip it, but still increment i,
           * since widget is visible and will be handled in next loop
           * iteration.
           */
          if (child->pack_type != packing)
            {
              i++;
              continue;
            }

          child_size = sizes[i].minimum_size;

          gtk_widget_get_preferred_height_for_width (child->widget,
                                                     child_size, &child_minimum, &child_natural);

          computed_minimum = MAX (computed_minimum, child_minimum);
          computed_natural = MAX (computed_natural, child_natural);
        }
      i += 1;
    }

  if (priv->center_widget != NULL &&
      gtk_widget_get_visible (priv->center_widget))
    {
      gtk_widget_get_preferred_height (priv->center_widget,
                                       &child_minimum, &child_natural);
      computed_minimum = MAX (computed_minimum, child_minimum);
      computed_natural = MAX (computed_natural, child_natural);
    }

  get_css_padding_and_border (widget, &css_borders);

  computed_minimum += css_borders.top + css_borders.bottom;
  computed_natural += css_borders.top + css_borders.bottom;

  if (minimum_size)
    *minimum_size = computed_minimum;

  if (natural_size)
    *natural_size = computed_natural;
}

static void
gtk_action_bar_get_preferred_width (GtkWidget *widget,
                                    gint      *minimum_size,
                                    gint      *natural_size)
{
  gtk_action_bar_get_size (widget, GTK_ORIENTATION_HORIZONTAL, minimum_size, natural_size);
}

static void
gtk_action_bar_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum_size,
                                     gint      *natural_size)
{
  gtk_action_bar_get_size (widget, GTK_ORIENTATION_VERTICAL, minimum_size, natural_size);
}

static void
gtk_action_bar_get_preferred_width_for_height (GtkWidget *widget,
                                               gint       height,
                                               gint      *minimum_width,
                                               gint      *natural_width)
{
  gtk_action_bar_compute_size_for_orientation (widget, height, minimum_width, natural_width);
}

static void
gtk_action_bar_get_preferred_height_for_width (GtkWidget *widget,
                                               gint       width,
                                               gint      *minimum_height,
                                               gint      *natural_height)
{
  gtk_action_bar_compute_size_for_opposing_orientation (widget, width, minimum_height, natural_height);
}

static void
gtk_action_bar_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GtkActionBar *bar = GTK_ACTION_BAR (widget);
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);
  GtkRequestedSize *sizes;
  gint width, height;
  gint nvis_children;
  gint start_width, end_width;
  gint side[2];
  GList *l;
  gint i;
  Child *child;
  GtkPackType packing;
  GtkAllocation child_allocation;
  gint x;
  gint child_size;
  GtkTextDirection direction;
  GtkBorder css_borders;
  gint center_minimum_size;
  gint center_natural_size;

  gtk_widget_set_allocation (widget, allocation);

  direction = gtk_widget_get_direction (widget);
  nvis_children = count_visible_children (bar);
  sizes = g_newa (GtkRequestedSize, nvis_children);

  get_css_padding_and_border (widget, &css_borders);
  width = allocation->width - nvis_children * priv->spacing - css_borders.left - css_borders.right;
  height = allocation->height - css_borders.top - css_borders.bottom;

  i = 0;
  for (l = priv->children; l; l = l->next)
    {
      child = l->data;
      if (!gtk_widget_get_visible (child->widget))
        continue;

      gtk_widget_get_preferred_width_for_height (child->widget,
                                                 height,
                                                 &sizes[i].minimum_size,
                                                 &sizes[i].natural_size);
      width -= sizes[i].minimum_size;
      i++;
    }

  if (priv->center_widget != NULL &&
      gtk_widget_get_visible (priv->center_widget))
    {
      gtk_widget_get_preferred_width_for_height (priv->center_widget,
                                                 height,
                                                 &center_minimum_size,
                                                 &center_natural_size);
      width -= center_natural_size;
    }
  else
    {
      center_minimum_size = 0;
      center_natural_size = 0;
    }

  start_width = 0;
  end_width = 0;

  width = gtk_distribute_natural_allocation (MAX (0, width), nvis_children, sizes);

  side[0] = side[1] = 0;
  for (packing = GTK_PACK_START; packing <= GTK_PACK_END; packing++)
    {
      child_allocation.y = allocation->y + css_borders.top;
      child_allocation.height = height;
      if (packing == GTK_PACK_START)
        x = allocation->x + css_borders.left + start_width;
      else
        x = allocation->x + allocation->width - end_width - css_borders.right;

      i = 0;

      for (l = priv->children; l != NULL; l = l->next)
        {
          child = l->data;
          if (!gtk_widget_get_visible (child->widget))
            continue;

          if (child->pack_type != packing)
            goto next;

          child_size = sizes[i].minimum_size;

          child_allocation.width = child_size;

          if (packing == GTK_PACK_START)
            {
              child_allocation.x = x;
              x += child_size;
              x += priv->spacing;
            }
          else
            {
              x -= child_size;
              child_allocation.x = x;
              x -= priv->spacing;
            }

          side[packing] += child_size + priv->spacing;

          if (direction == GTK_TEXT_DIR_RTL)
            child_allocation.x = allocation->x + allocation->width - (child_allocation.x - allocation->x) - child_allocation.width;

          gtk_widget_size_allocate (child->widget, &child_allocation);

        next:
          i++;
        }
    }

  side[GTK_PACK_START] += start_width;
  side[GTK_PACK_END] += end_width;

  child_allocation.y = allocation->y + css_borders.top;
  child_allocation.height = height;

  width = MAX (side[0], side[1]);

  if (allocation->width - 2 * width >= center_natural_size)
    child_size = MIN (center_natural_size, allocation->width - 2 * width);
  else if (allocation->width - side[0] - side[1] >= center_natural_size)
    child_size = MIN (center_natural_size, allocation->width - side[0] - side[1]);
  else
    child_size = allocation->width - side[0] - side[1];

  child_allocation.x = allocation->x + (allocation->width - child_size) / 2;
  child_allocation.width = child_size;

  if (allocation->x + side[0] > child_allocation.x)
    child_allocation.x = allocation->x + side[0];
  else if (allocation->x + allocation->width - side[1] < child_allocation.x + child_allocation.width)
    child_allocation.x = allocation->x + allocation->width - side[1] - child_allocation.width;

  if (direction == GTK_TEXT_DIR_RTL)
    child_allocation.x = allocation->x + allocation->width - (child_allocation.x - allocation->x) - child_allocation.width;

  if (priv->center_widget &&
      gtk_widget_get_visible (priv->center_widget))
    gtk_widget_size_allocate (priv->center_widget, &child_allocation);
}

/**
 * gtk_action_bar_set_center_widget:
 * @bar: a #GtkActionBar
 * @center_widget: (allow-none): a widget to use for the center
 *
 * Sets the center widget for the #GtkActionBar.
 *
 * Since: 3.12
 */
void
gtk_action_bar_set_center_widget (GtkActionBar *bar,
                                  GtkWidget    *center_widget)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);

  g_return_if_fail (GTK_IS_ACTION_BAR (bar));
  if (center_widget)
    g_return_if_fail (GTK_IS_WIDGET (center_widget));

  /* No need to do anything if the center widget stays the same */
  if (priv->center_widget == center_widget)
    return;

  if (priv->center_widget)
    {
      GtkWidget *center = priv->center_widget;

      priv->center_widget = NULL;
      gtk_widget_unparent (center);
    }

  if (center_widget != NULL)
    {
      priv->center_widget = center_widget;

      gtk_widget_set_parent (priv->center_widget, GTK_WIDGET (bar));
      gtk_widget_set_valign (priv->center_widget, GTK_ALIGN_CENTER);
    }

  gtk_widget_queue_resize (GTK_WIDGET (bar));

  g_object_notify (G_OBJECT (bar), "center-widget");
}

/**
 * gtk_action_bar_get_center_widget:
 * @bar: a #GtkActionBar
 *
 * Retrieves the center box widget of the bar.
 *
 * Return value: (transfer none): the center #GtkBox.
 *
 * Since: 3.12
 */
GtkWidget *
gtk_action_bar_get_center_widget (GtkActionBar *bar)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);

  g_return_val_if_fail (GTK_IS_ACTION_BAR (bar), NULL);

  return priv->center_widget;
}

static void
gtk_action_bar_finalize (GObject *object)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (GTK_ACTION_BAR (object));

  g_list_free (priv->children);

  G_OBJECT_CLASS (gtk_action_bar_parent_class)->finalize (object);
}

static void
gtk_action_bar_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkActionBar *bar = GTK_ACTION_BAR (object);
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);

  switch (prop_id)
    {
    case PROP_SPACING:
      g_value_set_int (value, priv->spacing);
      break;

    case PROP_CENTER_WIDGET:
      g_value_set_object (value, priv->center_widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_action_bar_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkActionBar *bar = GTK_ACTION_BAR (object);
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);

  switch (prop_id)
    {
    case PROP_SPACING:
      priv->spacing = g_value_get_int (value);
      gtk_widget_queue_resize (GTK_WIDGET (bar));
      break;

    case PROP_CENTER_WIDGET:
      gtk_action_bar_set_center_widget (bar, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_action_bar_pack (GtkActionBar *bar,
                     GtkWidget    *widget,
                     GtkPackType   pack_type)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);
  Child *child;

  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  child = g_new (Child, 1);
  child->widget = widget;
  child->pack_type = pack_type;

  priv->children = g_list_append (priv->children, child);

  gtk_widget_freeze_child_notify (widget);
  gtk_widget_set_parent (widget, GTK_WIDGET (bar));
  gtk_widget_child_notify (widget, "pack-type");
  gtk_widget_child_notify (widget, "position");
  gtk_widget_thaw_child_notify (widget);
}

static void
gtk_action_bar_add (GtkContainer *container,
                    GtkWidget    *child)
{
  gtk_action_bar_pack (GTK_ACTION_BAR (container), child, GTK_PACK_START);
}

static GList *
find_child_link (GtkActionBar *bar,
                 GtkWidget    *widget)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);
  GList *l;
  Child *child;

  for (l = priv->children; l; l = l->next)
    {
      child = l->data;
      if (child->widget == widget)
        return l;
    }

  return NULL;
}

static void
gtk_action_bar_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
  GtkActionBar *bar = GTK_ACTION_BAR (container);
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);
  GList *l;
  Child *child;

  l = find_child_link (bar, widget);
  if (l)
    {
      child = l->data;
      gtk_widget_unparent (child->widget);
      priv->children = g_list_delete_link (priv->children, l);
      g_free (child);
      gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
gtk_action_bar_forall (GtkContainer *container,
                       gboolean      include_internals,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  GtkActionBar *bar = GTK_ACTION_BAR (container);
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);
  Child *child;
  GList *children;

  children = priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      if (child->pack_type == GTK_PACK_START)
        (* callback) (child->widget, callback_data);
    }

  if (priv->center_widget != NULL)
    (* callback) (priv->center_widget, callback_data);

  children = priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      if (child->pack_type == GTK_PACK_END)
        (* callback) (child->widget, callback_data);
    }
}

static GType
gtk_action_bar_child_type (GtkContainer *container)
{
  return GTK_TYPE_WIDGET;
}

static void
gtk_action_bar_get_child_property (GtkContainer *container,
                                   GtkWidget    *widget,
                                   guint         property_id,
                                   GValue       *value,
                                   GParamSpec   *pspec)
{
  GtkActionBar *bar = GTK_ACTION_BAR (container);
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (bar);
  GList *l;
  Child *child;

  l = find_child_link (bar, widget);
  if (l == NULL)
    {
      g_param_value_set_default (pspec, value);
      return;
    }

  child = l->data;

  switch (property_id)
    {
    case CHILD_PROP_PACK_TYPE:
      g_value_set_enum (value, child->pack_type);
      break;

    case CHILD_PROP_POSITION:
      g_value_set_int (value, g_list_position (priv->children, l));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_action_bar_set_child_property (GtkContainer *container,
                                   GtkWidget    *widget,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GList *l;
  Child *child;

  l = find_child_link (GTK_ACTION_BAR (container), widget);
  child = l->data;

  switch (property_id)
    {
    case CHILD_PROP_PACK_TYPE:
      child->pack_type = g_value_get_enum (value);
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static GtkWidgetPath *
gtk_action_bar_get_path_for_child (GtkContainer *container,
                                   GtkWidget    *child)
{
  GtkActionBar *bar = GTK_ACTION_BAR (container);
  GtkWidgetPath *path, *sibling_path;
  GList *list, *children;

  path = _gtk_widget_create_path (GTK_WIDGET (container));

  if (gtk_widget_get_visible (child))
    {
      gint i, position;

      sibling_path = gtk_widget_path_new ();

      /* get_all_children works in reverse (!) visible order */
      children = _gtk_container_get_all_children (container);
      if (gtk_widget_get_direction (GTK_WIDGET (bar)) == GTK_TEXT_DIR_LTR)
        children = g_list_reverse (children);

      position = -1;
      i = 0;
      for (list = children; list; list = list->next)
        {
          if (!gtk_widget_get_visible (list->data))
            continue;

          gtk_widget_path_append_for_widget (sibling_path, list->data);

          if (list->data == child)
            position = i;
          i++;
        }
      g_list_free (children);

      if (position >= 0)
        gtk_widget_path_append_with_siblings (path, sibling_path, position);
      else
        gtk_widget_path_append_for_widget (path, child);

      gtk_widget_path_unref (sibling_path);
    }
  else
    gtk_widget_path_append_for_widget (path, child);

  return path;
}

static gint
gtk_action_bar_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_render_background (context, cr, 0, 0,
                         gtk_widget_get_allocated_width (widget),
                         gtk_widget_get_allocated_height (widget));
  gtk_render_frame (context, cr, 0, 0,
                    gtk_widget_get_allocated_width (widget),
                    gtk_widget_get_allocated_height (widget));


  GTK_WIDGET_CLASS (gtk_action_bar_parent_class)->draw (widget, cr);

  return TRUE;
}

static void
gtk_action_bar_class_init (GtkActionBarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->finalize = gtk_action_bar_finalize;
  object_class->get_property = gtk_action_bar_get_property;
  object_class->set_property = gtk_action_bar_set_property;

  widget_class->size_allocate = gtk_action_bar_size_allocate;
  widget_class->get_preferred_width = gtk_action_bar_get_preferred_width;
  widget_class->get_preferred_height = gtk_action_bar_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_action_bar_get_preferred_height_for_width;
  widget_class->get_preferred_width_for_height = gtk_action_bar_get_preferred_width_for_height;
  widget_class->draw = gtk_action_bar_draw;

  container_class->add = gtk_action_bar_add;
  container_class->remove = gtk_action_bar_remove;
  container_class->forall = gtk_action_bar_forall;
  container_class->child_type = gtk_action_bar_child_type;
  container_class->set_child_property = gtk_action_bar_set_child_property;
  container_class->get_child_property = gtk_action_bar_get_child_property;
  container_class->get_path_for_child = gtk_action_bar_get_path_for_child;
  gtk_container_class_handle_border_width (container_class);

  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_PACK_TYPE,
                                              g_param_spec_enum ("pack-type",
                                                                 P_("Pack type"),
                                                                 P_("A GtkPackType indicating whether the child is packed with reference to the start or end of the parent"),
                                                                 GTK_TYPE_PACK_TYPE, GTK_PACK_START,
                                                                 GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_POSITION,
                                              g_param_spec_int ("position",
                                                                P_("Position"),
                                                                P_("The index of the child in the parent"),
                                                                -1, G_MAXINT, 0,
                                                                GTK_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_SPACING,
                                   g_param_spec_int ("spacing",
                                                     P_("Spacing"),
                                                     P_("The amount of space between children"),
                                                     0, G_MAXINT,
                                                     DEFAULT_SPACING,
                                                     GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_CENTER_WIDGET,
                                   g_param_spec_object ("center-widget",
                                                        P_("Center Widget"),
                                                        P_("Widget to display in center"),
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_STRINGS));

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_PANEL);
}

static void
gtk_action_bar_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const gchar  *type)
{
  if (type && strcmp (type, "center") == 0)
    gtk_action_bar_set_center_widget (GTK_ACTION_BAR (buildable), GTK_WIDGET (child));
  else if (!type)
    gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (GTK_ACTION_BAR (buildable), type);
}

static void
gtk_action_bar_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = gtk_action_bar_buildable_add_child;
}

/**
 * gtk_action_bar_pack_start:
 * @bar: A #GtkActionBar
 * @child: the #GtkWidget to be added to @bar
 *
 * Adds @child to @box, packed with reference to the
 * start of the @box.
 *
 * Since: 3.12
 */
void
gtk_action_bar_pack_start (GtkActionBar *bar,
                           GtkWidget    *child)
{
  gtk_action_bar_pack (bar, child, GTK_PACK_START);
}

/**
 * gtk_action_bar_pack_end:
 * @bar: A #GtkActionBar
 * @child: the #GtkWidget to be added to @bar
 *
 * Adds @child to @box, packed with reference to the
 * end of the @box.
 *
 * Since: 3.12
 */
void
gtk_action_bar_pack_end (GtkActionBar *bar,
                         GtkWidget    *child)
{
  gtk_action_bar_pack (bar, child, GTK_PACK_END);
}

/**
 * gtk_action_bar_new:
 *
 * Creates a new #GtkActionBar widget.
 *
 * Returns: a new #GtkActionBar
 *
 * Since: 3.12
 */
GtkWidget *
gtk_action_bar_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_ACTION_BAR, NULL));
}
