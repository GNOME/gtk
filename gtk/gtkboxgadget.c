/*
 * Copyright © 2015 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkboxgadgetprivate.h"

#include "gtkcssnodeprivate.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtkwidgetprivate.h"

/* GtkBoxGadget is a container gadget implementation that arranges its
 * children in a row, either horizontally or vertically. Children can
 * be either widgets or gadgets, and can be set to expand horizontally
 * or vertically, or both.
 */

typedef struct _GtkBoxGadgetPrivate GtkBoxGadgetPrivate;
struct _GtkBoxGadgetPrivate {
  GtkOrientation orientation;
  GArray *children;
};

typedef gboolean (* ComputeExpandFunc) (GObject *object, GtkOrientation orientation);

typedef struct _GtkBoxGadgetChild GtkBoxGadgetChild;
struct _GtkBoxGadgetChild {
  GObject *object;
  ComputeExpandFunc compute_expand;
  GtkAlign align;
};

G_DEFINE_TYPE_WITH_CODE (GtkBoxGadget, gtk_box_gadget, GTK_TYPE_CSS_GADGET,
                         G_ADD_PRIVATE (GtkBoxGadget))

static gboolean
gtk_box_gadget_child_is_visible (GObject *child)
{
  if (GTK_IS_WIDGET (child))
    return gtk_widget_get_visible (GTK_WIDGET (child));
  else
    return gtk_css_gadget_get_visible (GTK_CSS_GADGET (child));
}

static GtkAlign
gtk_box_gadget_child_get_align (GtkBoxGadget      *gadget,
                                GtkBoxGadgetChild *child)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (GTK_BOX_GADGET (gadget));
  GtkAlign align;

  if (GTK_IS_WIDGET (child->object))
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        g_object_get (child->object, "valign", &align, NULL);
      else
        g_object_get (child->object, "halign", &align, NULL);
    }
  else
    align = child->align;

  return align;
}

static void
gtk_box_gadget_measure_child (GObject        *child,
                              GtkOrientation  orientation,
                              gint            for_size,
                              gint           *minimum,
                              gint           *natural,
                              gint           *minimum_baseline,
                              gint           *natural_baseline)
{
  if (GTK_IS_WIDGET (child))
    {
      _gtk_widget_get_preferred_size_for_size (GTK_WIDGET (child),
                                               orientation,
                                               for_size,
                                               minimum, natural,
                                               minimum_baseline, natural_baseline);
    }
  else
    {
      gtk_css_gadget_get_preferred_size (GTK_CSS_GADGET (child),
                                         orientation,
                                         for_size,
                                         minimum, natural,
                                         minimum_baseline, natural_baseline);
    }
}

static void
gtk_box_gadget_distribute (GtkBoxGadget     *gadget,
                           gint              size,
                           GtkRequestedSize *sizes)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (GTK_BOX_GADGET (gadget));
  guint i, n_expand;

  n_expand = 0;

  for (i = 0 ; i < priv->children->len; i++)
    {
      GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, i);

      gtk_box_gadget_measure_child (child->object,
                                    priv->orientation,
                                    -1, 
                                    &sizes[i].minimum_size, &sizes[i].natural_size,
                                    NULL, NULL);
      if (gtk_box_gadget_child_is_visible (child->object) &&
          child->compute_expand (child->object, priv->orientation))
        n_expand++;
      size -= sizes[i].minimum_size;
    }

  g_return_if_fail (size >= 0);

  size = gtk_distribute_natural_allocation (size, priv->children->len, sizes);

  if (size <= 0 || n_expand == 0)
    return;

  for (i = 0 ; i < priv->children->len; i++)
    {
      GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, i);

      if (!gtk_box_gadget_child_is_visible (child->object) ||
          !child->compute_expand (child->object, priv->orientation))
        continue;

      sizes[i].minimum_size += size / n_expand;
      /* distribute all pixels, even if there's a remainder */
      size -= size / n_expand;
      n_expand--;
    }
}

static void
gtk_box_gadget_measure_orientation (GtkCssGadget   *gadget,
                                    GtkOrientation  orientation,
                                    gint            for_size,
                                    gint           *minimum,
                                    gint           *natural,
                                    gint           *minimum_baseline,
                                    gint           *natural_baseline)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (GTK_BOX_GADGET (gadget));
  gint child_min, child_nat;
  guint i;

  *minimum = 0;
  *natural = 0;

  for (i = 0 ; i < priv->children->len; i++)
    {
      GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, i);

      gtk_box_gadget_measure_child (child->object,
                                    orientation,
                                    for_size,
                                    &child_min, &child_nat,
                                    NULL, NULL);

      *minimum += child_min;
      *natural += child_nat;
    }
}

static void
gtk_box_gadget_measure_opposite (GtkCssGadget   *gadget,
                                 GtkOrientation  orientation,
                                 gint            for_size,
                                 gint           *minimum,
                                 gint           *natural,
                                 gint           *minimum_baseline,
                                 gint           *natural_baseline)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (GTK_BOX_GADGET (gadget));
  int child_min, child_nat, child_min_baseline, child_nat_baseline;
  int total_min, above_min, below_min, total_nat, above_nat, below_nat;
  GtkRequestedSize *sizes;
  guint i;

  if (for_size >= 0)
    {
      sizes = g_newa (GtkRequestedSize, priv->children->len);
      gtk_box_gadget_distribute (GTK_BOX_GADGET (gadget), for_size, sizes);
    }

  above_min = below_min = above_nat = below_nat = -1;
  total_min = total_nat = 0;

  for (i = 0 ; i < priv->children->len; i++)
    {
      GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, i);

      gtk_box_gadget_measure_child (child->object,
                                    orientation,
                                    for_size >= 0 ? sizes[i].minimum_size : for_size,
                                    &child_min, &child_nat,
                                    &child_min_baseline, &child_nat_baseline);

      if (child_min_baseline >= 0)
        {
          above_min = MAX (above_min, child_min - child_min_baseline);
          below_min = MAX (below_min, child_min_baseline);
          above_nat = MAX (above_nat, child_nat - child_nat_baseline);
          below_nat = MAX (below_nat, child_nat_baseline);
        }
      else
        {
          total_min = MAX (total_min, child_min);
          total_nat = MAX (total_nat, child_nat);
        }
    }

  if (above_min >= 0)
    {
      total_min = MAX (total_min, above_min + below_min);
      total_nat = MAX (total_nat, above_nat + below_nat);
      /* assume GTK_BASELINE_POSITION_CENTER for now */
      if (minimum_baseline)
        *minimum_baseline = above_min + (total_min - (above_min + below_min)) / 2;
      if (natural_baseline)
        *natural_baseline = above_nat + (total_nat - (above_nat + below_nat)) / 2;
    }

  *minimum = total_min;
  *natural = total_nat;
}

static void
gtk_box_gadget_get_preferred_size (GtkCssGadget   *gadget,
                                   GtkOrientation  orientation,
                                   gint            for_size,
                                   gint           *minimum,
                                   gint           *natural,
                                   gint           *minimum_baseline,
                                   gint           *natural_baseline)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (GTK_BOX_GADGET (gadget));

  if (priv->orientation == orientation)
    gtk_box_gadget_measure_orientation (gadget, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
  else
    gtk_box_gadget_measure_opposite (gadget, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
}

static void
gtk_box_gadget_allocate_child (GObject        *child,
                               GtkAllocation *allocation,
                               int            baseline,
                               GtkAllocation *out_clip)
{
  if (GTK_IS_WIDGET (child))
    {
      gtk_widget_size_allocate_with_baseline (GTK_WIDGET (child),
                                              allocation,
                                              baseline);
      gtk_widget_get_clip (GTK_WIDGET (child), out_clip);
    }
  else
    {
      gtk_css_gadget_allocate (GTK_CSS_GADGET (child),
                               allocation,
                               baseline,
                               out_clip);
    }
}

static void
gtk_box_gadget_allocate (GtkCssGadget        *gadget,
                         const GtkAllocation *allocation,
                         int                  baseline,
                         GtkAllocation       *out_clip)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (GTK_BOX_GADGET (gadget));
  GtkRequestedSize *sizes;
  GtkAllocation child_allocation, child_clip;
  guint i;

  child_allocation = *allocation;
  sizes = g_newa (GtkRequestedSize, priv->children->len);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_box_gadget_distribute (GTK_BOX_GADGET (gadget), allocation->width, sizes);

      if (baseline < 0)
        {
          for (i = 0; i < priv->children->len; i++)
            {
              GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, i);

              if (gtk_box_gadget_child_get_align (GTK_BOX_GADGET (gadget), child) == GTK_ALIGN_BASELINE)
                {
                  gint child_min, child_nat;
                  gint child_baseline_min, child_baseline_nat;

                  gtk_box_gadget_measure_child (child->object,
                                                GTK_ORIENTATION_VERTICAL,
                                                sizes[i].minimum_size,
                                                &child_min, &child_nat,
                                                &child_baseline_min, &child_baseline_nat);
                  baseline = MAX (baseline, child_baseline_min);
                }
            }
        }

      for (i = 0; i < priv->children->len; i++)
        {
          GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, i);
          gint child_min, child_nat;
          gint child_baseline_min, child_baseline_nat;

          child_allocation.width = sizes[i].minimum_size;
          gtk_box_gadget_measure_child (child->object,
                                        GTK_ORIENTATION_VERTICAL,
                                        child_allocation.width,
                                        &child_min, &child_nat,
                                        &child_baseline_min, &child_baseline_nat);
          switch (gtk_box_gadget_child_get_align (GTK_BOX_GADGET (gadget), child))
            {
            case GTK_ALIGN_FILL:
              child_allocation.height = allocation->height;
              child_allocation.y = allocation->y;
              break;
            case GTK_ALIGN_START:
              child_allocation.height = MIN(child_nat, allocation->height);
              child_allocation.y = allocation->y;
              break;
            case GTK_ALIGN_END:
              child_allocation.height = MIN(child_nat, allocation->height);
              child_allocation.y = allocation->y + allocation->height - child_allocation.height;
              break;
            case GTK_ALIGN_BASELINE:
              if (child_baseline_min >= 0 && baseline >= 0)
                {
                  child_allocation.height = MIN(child_nat, allocation->height);
                  child_allocation.y = allocation->y + baseline - child_baseline_min;
                  break;
                }
            case GTK_ALIGN_CENTER:
              child_allocation.height = MIN(child_nat, allocation->height);
              child_allocation.y = allocation->y + (allocation->height - child_allocation.height) / 2;
              break;
            default:
              g_assert_not_reached ();
            }

          gtk_box_gadget_allocate_child (child->object, &child_allocation, baseline, &child_clip);
          if (i == 0)
            *out_clip = child_clip;
          else
            gdk_rectangle_union (out_clip, &child_clip, out_clip);
          child_allocation.x += sizes[i].minimum_size;
        }
    }
  else
    {
      gtk_box_gadget_distribute (GTK_BOX_GADGET (gadget), allocation->height, sizes);

      for (i = 0 ; i < priv->children->len; i++)
        {
          GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, i);
          gint child_min, child_nat;

          child_allocation.height = sizes[i].minimum_size;
          gtk_box_gadget_measure_child (child->object,
                                        GTK_ORIENTATION_HORIZONTAL,
                                        child_allocation.height,
                                        &child_min, &child_nat,
                                        NULL, NULL);

          switch (gtk_box_gadget_child_get_align (GTK_BOX_GADGET (gadget), child))
            {
            case GTK_ALIGN_FILL:
              child_allocation.width = allocation->width;
              child_allocation.x = allocation->x;
              break;
            case GTK_ALIGN_START:
              child_allocation.width = MIN(child_nat, allocation->width);
              child_allocation.x = allocation->x;
              break;
            case GTK_ALIGN_END:
              child_allocation.width = MIN(child_nat, allocation->width);
              child_allocation.x = allocation->x + allocation->width - child_allocation.width;
              break;
            case GTK_ALIGN_BASELINE:
            case GTK_ALIGN_CENTER:
              child_allocation.width = MIN(child_nat, allocation->width);
              child_allocation.x = allocation->x + (allocation->width - child_allocation.width) / 2;
              break;
            default:
              g_assert_not_reached ();
            }

          gtk_box_gadget_allocate_child (child->object, &child_allocation, -1, &child_clip);
          if (i == 0)
            *out_clip = child_clip;
          else
            gdk_rectangle_union (out_clip, &child_clip, out_clip);
          child_allocation.y += sizes[i].minimum_size;
        }
    }
}

static gboolean
gtk_box_gadget_draw (GtkCssGadget *gadget,
                     cairo_t      *cr,
                     int           x,
                     int           y,
                     int           width,
                     int           height)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (GTK_BOX_GADGET (gadget));
  guint i;

  for (i = 0 ; i < priv->children->len; i++)
    {
      GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, i);

      if (GTK_IS_WIDGET (child->object))
        {
          gtk_container_propagate_draw (GTK_CONTAINER (gtk_css_gadget_get_owner (gadget)),
                                        GTK_WIDGET (child->object),
                                        cr);
        }
      else
        {
          gtk_css_gadget_draw (GTK_CSS_GADGET (child->object),
                               cr);
        }
    }

  return FALSE;
}

static void
gtk_box_gadget_finalize (GObject *object)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (GTK_BOX_GADGET (object));

  g_array_free (priv->children, TRUE);

  G_OBJECT_CLASS (gtk_box_gadget_parent_class)->finalize (object);
}

static void
gtk_box_gadget_class_init (GtkBoxGadgetClass *klass)
{
  GtkCssGadgetClass *gadget_class = GTK_CSS_GADGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_box_gadget_finalize;

  gadget_class->get_preferred_size = gtk_box_gadget_get_preferred_size;
  gadget_class->allocate = gtk_box_gadget_allocate;
  gadget_class->draw = gtk_box_gadget_draw;
}

static void
gtk_box_gadget_clear_child (gpointer data)
{
  GtkBoxGadgetChild *child = data;

  g_object_unref (child->object);
}

static void
gtk_box_gadget_init (GtkBoxGadget *gadget)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (gadget);

  priv->children = g_array_new (FALSE, FALSE, sizeof (GtkBoxGadgetChild));
  g_array_set_clear_func (priv->children, gtk_box_gadget_clear_child);
}

GtkCssGadget *
gtk_box_gadget_new_for_node (GtkCssNode *node,
                               GtkWidget  *owner)
{
  return g_object_new (GTK_TYPE_BOX_GADGET,
                       "node", node,
                       "owner", owner,
                       NULL);
}

GtkCssGadget *
gtk_box_gadget_new (const char   *name,
                    GtkWidget    *owner,
                    GtkCssGadget *parent,
                    GtkCssGadget *next_sibling)
{
  GtkCssNode *node;
  GtkCssGadget *result;

  node = gtk_css_node_new ();
  gtk_css_node_set_name (node, g_intern_string (name));
  if (parent)
    gtk_css_node_insert_before (gtk_css_gadget_get_node (parent),
                                node,
                                next_sibling ? gtk_css_gadget_get_node (next_sibling) : NULL);

  result = gtk_box_gadget_new_for_node (node, owner);

  g_object_unref (node);

  return result;
}

void
gtk_box_gadget_set_orientation (GtkBoxGadget   *gadget,
                                GtkOrientation  orientation)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (gadget);

  priv->orientation = orientation;
}

static GtkCssNode *
get_css_node (GObject *child)
{
  if (GTK_IS_WIDGET (child))
    return gtk_widget_get_css_node (GTK_WIDGET (child));
  else
    return gtk_css_gadget_get_node (GTK_CSS_GADGET (child));
}

static void
gtk_box_gadget_insert_object (GtkBoxGadget      *gadget,
                              int                pos,
                              GObject           *object,
                              ComputeExpandFunc  compute_expand_func,
                              GtkAlign           align)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (gadget);
  GtkBoxGadgetChild child;

  child.object = g_object_ref (object);
  child.compute_expand = compute_expand_func;
  child.align = align;

  if (pos < 0 || pos >= priv->children->len)
    {
      g_array_append_val (priv->children, child);
      gtk_css_node_insert_before (gtk_css_gadget_get_node (GTK_CSS_GADGET (gadget)),
                                  get_css_node (object),
                                  NULL);
    }
  else
    {
      g_array_insert_val (priv->children, pos, child);
      gtk_css_node_insert_before (gtk_css_gadget_get_node (GTK_CSS_GADGET (gadget)),
                                  get_css_node (object),
                                  get_css_node (g_array_index (priv->children, GtkBoxGadgetChild, pos + 1).object));
    }
}

void
gtk_box_gadget_insert_widget (GtkBoxGadget           *gadget,
                              int                     pos,
                              GtkWidget              *widget)
{
  gtk_box_gadget_insert_object (gadget,
                                pos,
                                G_OBJECT (widget),
                                (ComputeExpandFunc) gtk_widget_compute_expand,
                                GTK_ALIGN_FILL);
}

void
gtk_box_gadget_remove_object (GtkBoxGadget *gadget,
                              GObject      *object)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (gadget);
  guint i;

  for (i = 0; i < priv->children->len; i++)
    {
      GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, i);

      if (child->object == object)
        {
          gtk_css_node_set_parent (get_css_node (object), NULL);
          g_array_remove_index (priv->children, i);
          break;
        }
    }
}

void
gtk_box_gadget_remove_widget (GtkBoxGadget *gadget,
                              GtkWidget    *widget)
{
  gtk_box_gadget_remove_object (gadget, G_OBJECT (widget));
}

static gboolean
only_horizontal (GObject        *object,
                 GtkOrientation  orientation)
{
  return orientation == GTK_ORIENTATION_HORIZONTAL;
}

static gboolean
only_vertical (GObject        *object,
               GtkOrientation  orientation)
{
  return orientation == GTK_ORIENTATION_VERTICAL;
}

void
gtk_box_gadget_insert_gadget (GtkBoxGadget *gadget,
                              int           pos,
                              GtkCssGadget *cssgadget,
                              gboolean      hexpand,
                              gboolean      vexpand,
                              GtkAlign      align)
{
  gtk_box_gadget_insert_object (gadget,
                                pos,
                                G_OBJECT (cssgadget),
                                hexpand ? (vexpand ? (ComputeExpandFunc) gtk_true : only_horizontal)
                                        : (vexpand ? only_vertical : (ComputeExpandFunc) gtk_false),
                                align);
}

void
gtk_box_gadget_remove_gadget (GtkBoxGadget *gadget,
                              GtkCssGadget *cssgadget)
{
  gtk_box_gadget_remove_object (gadget, G_OBJECT (cssgadget));
}
