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

  guint draw_focus       : 1;
  guint draw_reverse     : 1;
  guint allocate_reverse : 1;
  guint align_reverse    : 1;
};

typedef gboolean (* ComputeExpandFunc) (GObject *object, GtkOrientation orientation);

typedef struct _GtkBoxGadgetChild GtkBoxGadgetChild;
struct _GtkBoxGadgetChild {
  GObject *object;
  gboolean expand;
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

static gboolean
gtk_box_gadget_child_compute_expand (GtkBoxGadget      *gadget,
                                     GtkBoxGadgetChild *child)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (GTK_BOX_GADGET (gadget));

  if (child->expand)
    return TRUE;

  if (GTK_IS_WIDGET (child->object))
    return gtk_widget_compute_expand (GTK_WIDGET (child->object), priv->orientation);

  return FALSE;
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

static GtkAlign
effective_align (GtkAlign align,
                 gboolean reverse)
{
  switch (align)
    {
    case GTK_ALIGN_START:
      return reverse ? GTK_ALIGN_END : GTK_ALIGN_START;
    case GTK_ALIGN_END:
      return reverse ? GTK_ALIGN_START : GTK_ALIGN_END;
    default:
      return align;
    }
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
                           gint              for_size,
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
                                    for_size,
                                    &sizes[i].minimum_size, &sizes[i].natural_size,
                                    NULL, NULL);
      if (gtk_box_gadget_child_is_visible (child->object) &&
          gtk_box_gadget_child_compute_expand (gadget, child))
        n_expand++;
      size -= sizes[i].minimum_size;
    }

  if G_UNLIKELY (size < 0)
    {
      return;
    }

  size = gtk_distribute_natural_allocation (size, priv->children->len, sizes);

  if (size <= 0 || n_expand == 0)
    return;

  for (i = 0 ; i < priv->children->len; i++)
    {
      GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, i);

      if (!gtk_box_gadget_child_is_visible (child->object) ||
          !gtk_box_gadget_child_compute_expand (gadget, child))
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
      gtk_box_gadget_distribute (GTK_BOX_GADGET (gadget), -1, for_size, sizes);
    }

  above_min = below_min = above_nat = below_nat = -1;
  total_min = total_nat = 0;

  for (i = 0 ; i < priv->children->len; i++)
    {
      GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, i);

      gtk_box_gadget_measure_child (child->object,
                                    orientation,
                                    for_size >= 0 ? sizes[i].minimum_size : -1,
                                    &child_min, &child_nat,
                                    &child_min_baseline, &child_nat_baseline);

      if (child_min_baseline >= 0)
        {
          below_min = MAX (below_min, child_min - child_min_baseline);
          above_min = MAX (above_min, child_min_baseline);
          below_nat = MAX (below_nat, child_nat - child_nat_baseline);
          above_nat = MAX (above_nat, child_nat_baseline);
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
                               GtkOrientation  box_orientation,
                               GtkAlign        child_align,
                               GtkAllocation  *allocation,
                               int             baseline,
                               GtkAllocation  *out_clip)
{
  if (GTK_IS_WIDGET (child))
    {
      gtk_widget_size_allocate_with_baseline (GTK_WIDGET (child), allocation, baseline);
      gtk_widget_get_clip (GTK_WIDGET (child), out_clip);
    }
  else
    {
      GtkAllocation child_allocation;
      int minimum, natural;
      int minimum_baseline, natural_baseline;

      if (box_orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          child_allocation.width = allocation->width;
          child_allocation.x = allocation->x;

          gtk_css_gadget_get_preferred_size (GTK_CSS_GADGET (child),
                                             GTK_ORIENTATION_VERTICAL,
                                             allocation->width,
                                             &minimum, &natural,
                                             &minimum_baseline, &natural_baseline);

          switch (child_align)
            {
            case GTK_ALIGN_FILL:
              child_allocation.height = allocation->height;
              child_allocation.y = allocation->y;
              break;
            case GTK_ALIGN_START:
              child_allocation.height = MIN(natural, allocation->height);
              child_allocation.y = allocation->y;
              break;
            case GTK_ALIGN_END:
              child_allocation.height = MIN(natural, allocation->height);
              child_allocation.y = allocation->y + allocation->height - child_allocation.height;
              break;
            case GTK_ALIGN_BASELINE:
              if (minimum_baseline >= 0 && baseline >= 0)
                {
                  child_allocation.height = MIN(natural, allocation->height);
                  child_allocation.y = allocation->y + MAX(0, baseline - minimum_baseline);
                  break;
                }
            case GTK_ALIGN_CENTER:
              child_allocation.height = MIN(natural, allocation->height);
              child_allocation.y = allocation->y + (allocation->height - child_allocation.height) / 2;
              break;
            default:
              g_assert_not_reached ();
            }
        }
      else
        {
          child_allocation.height = allocation->height;
          child_allocation.y = allocation->y;

          gtk_css_gadget_get_preferred_size (GTK_CSS_GADGET (child),
                                             GTK_ORIENTATION_HORIZONTAL,
                                             allocation->height,
                                             &minimum, &natural,
                                             NULL, NULL);

          switch (child_align)
            {
            case GTK_ALIGN_FILL:
              child_allocation.width = allocation->width;
              child_allocation.x = allocation->x;
              break;
            case GTK_ALIGN_START:
              child_allocation.width = MIN(natural, allocation->width);
              child_allocation.x = allocation->x;
              break;
            case GTK_ALIGN_END:
              child_allocation.width = MIN(natural, allocation->width);
              child_allocation.x = allocation->x + allocation->width - child_allocation.width;
              break;
            case GTK_ALIGN_BASELINE:
            case GTK_ALIGN_CENTER:
              child_allocation.width = MIN(natural, allocation->width);
              child_allocation.x = allocation->x + (allocation->width - child_allocation.width) / 2;
              break;
            default:
              g_assert_not_reached ();
            }
        }

      gtk_css_gadget_allocate (GTK_CSS_GADGET (child), &child_allocation, baseline, out_clip);
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
  GtkAlign child_align;
  guint i;

  child_allocation = *allocation;
  sizes = g_newa (GtkRequestedSize, priv->children->len);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_box_gadget_distribute (GTK_BOX_GADGET (gadget), allocation->height, allocation->width, sizes);

      if (priv->allocate_reverse)
        child_allocation.x = allocation->x + allocation->width;

      for (i = 0; i < priv->children->len; i++)
        {
          guint idx = priv->allocate_reverse ? priv->children->len - 1 - i : i;
          GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, idx);
          child_allocation.width = sizes[idx].minimum_size;
          child_allocation.height = allocation->height;
          child_allocation.y = allocation->y;
          if (priv->allocate_reverse)
            child_allocation.x -= child_allocation.width;

          child_align = gtk_box_gadget_child_get_align (GTK_BOX_GADGET (gadget), child);
          gtk_box_gadget_allocate_child (child->object,
                                         priv->orientation,
                                         effective_align (child_align, priv->align_reverse),
                                         &child_allocation,
                                         baseline,
                                         &child_clip);

          if (i == 0)
            *out_clip = child_clip;
          else
            gdk_rectangle_union (out_clip, &child_clip, out_clip);

          if (!priv->allocate_reverse)
            child_allocation.x += sizes[idx].minimum_size;
        }
    }
  else
    {
      gtk_box_gadget_distribute (GTK_BOX_GADGET (gadget), allocation->width, allocation->height, sizes);

      if (priv->allocate_reverse)
        child_allocation.y = allocation->y + allocation->height;

      for (i = 0 ; i < priv->children->len; i++)
        {
          guint idx = priv->allocate_reverse ? priv->children->len - 1 - i : i;
          GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, idx);
          child_allocation.height = sizes[idx].minimum_size;
          child_allocation.width = allocation->width;
          child_allocation.x = allocation->x;
          if (priv->allocate_reverse)
            child_allocation.y -= child_allocation.height;

          child_align = gtk_box_gadget_child_get_align (GTK_BOX_GADGET (gadget), child);
          gtk_box_gadget_allocate_child (child->object,
                                         priv->orientation,
                                         effective_align (child_align, priv->align_reverse),
                                         &child_allocation,
                                         -1,
                                         &child_clip);

          if (i == 0)
            *out_clip = child_clip;
          else
            gdk_rectangle_union (out_clip, &child_clip, out_clip);

          if (!priv->allocate_reverse)
            child_allocation.y += sizes[idx].minimum_size;
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
  GtkWidget *owner = gtk_css_gadget_get_owner (gadget);
  guint i;

  for (i = 0; i < priv->children->len; i++)
    {
      guint draw_index = priv->draw_reverse ? priv->children->len - 1 - i : i;
      GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, draw_index);

      if (GTK_IS_WIDGET (child->object))
        gtk_container_propagate_draw (GTK_CONTAINER (owner), GTK_WIDGET (child->object), cr);
      else
        gtk_css_gadget_draw (GTK_CSS_GADGET (child->object), cr);
    }

  if (priv->draw_focus && gtk_widget_has_visible_focus (owner))
    return TRUE;

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

void
gtk_box_gadget_set_draw_focus (GtkBoxGadget *gadget,
                               gboolean      draw_focus)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (gadget);

  priv->draw_focus = draw_focus;
}

void
gtk_box_gadget_set_draw_reverse (GtkBoxGadget *gadget,
                                 gboolean      draw_reverse)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (gadget);

  priv->draw_reverse = draw_reverse;
}

void
gtk_box_gadget_set_allocate_reverse (GtkBoxGadget *gadget,
                                     gboolean      allocate_reverse)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (gadget);

  priv->allocate_reverse = allocate_reverse;
}

void
gtk_box_gadget_set_align_reverse (GtkBoxGadget *gadget,
                                  gboolean      align_reverse)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (gadget);

  priv->align_reverse = align_reverse;
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
gtk_box_gadget_insert_object (GtkBoxGadget *gadget,
                              int           pos,
                              GObject      *object,
                              gboolean      expand,
                              GtkAlign      align)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (gadget);
  GtkBoxGadgetChild child;

  child.object = g_object_ref (object);
  child.expand = expand;
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
gtk_box_gadget_insert_widget (GtkBoxGadget *gadget,
                              int           pos,
                              GtkWidget    *widget)
{
  gtk_box_gadget_insert_object (gadget, pos, G_OBJECT (widget), FALSE, GTK_ALIGN_FILL);
}

static GtkBoxGadgetChild *
gtk_box_gadget_find_object (GtkBoxGadget *gadget,
                            GObject      *object,
                            int          *position)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (gadget);
  guint i;

  for (i = 0; i < priv->children->len; i++)
    {
      GtkBoxGadgetChild *child = &g_array_index (priv->children, GtkBoxGadgetChild, i);

      if (child->object == object)
        {
          if (position)
            *position = i;
          return child;
        }
    }

  return NULL;
}

static void
gtk_box_gadget_remove_object (GtkBoxGadget *gadget,
                              GObject      *object)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (gadget);
  GtkBoxGadgetChild *child;
  int position;

  child = gtk_box_gadget_find_object (gadget, object, &position);
  if (child)
    {
      gtk_css_node_set_parent (get_css_node (child->object), NULL);
      g_array_remove_index (priv->children, position);
    }
}

void
gtk_box_gadget_remove_widget (GtkBoxGadget *gadget,
                              GtkWidget    *widget)
{
  gtk_box_gadget_remove_object (gadget, G_OBJECT (widget));
}

void
gtk_box_gadget_insert_gadget_before (GtkBoxGadget *gadget,
                                     GtkCssGadget *sibling,
                                     GtkCssGadget *cssgadget,
                                     gboolean      expand,
                                     GtkAlign      align)
{
  /* Insert at the end if no sibling specified */
  int pos = -1;

  if (sibling)
    gtk_box_gadget_find_object (gadget, G_OBJECT (sibling), &pos);

  gtk_box_gadget_insert_gadget (gadget, pos, cssgadget, expand, align);
}

void
gtk_box_gadget_insert_gadget_after (GtkBoxGadget *gadget,
                                    GtkCssGadget *sibling,
                                    GtkCssGadget *cssgadget,
                                    gboolean      expand,
                                    GtkAlign      align)
{
  /* Insert at the beginning if no sibling specified */
  int pos = 0;

  if (sibling && gtk_box_gadget_find_object (gadget, G_OBJECT (sibling), &pos))
    pos++;

  gtk_box_gadget_insert_gadget (gadget, pos, cssgadget, expand, align);
}

void
gtk_box_gadget_insert_gadget (GtkBoxGadget *gadget,
                              int           pos,
                              GtkCssGadget *cssgadget,
                              gboolean      expand,
                              GtkAlign      align)
{
  gtk_box_gadget_insert_object (gadget, pos, G_OBJECT (cssgadget), expand, align);
}

void
gtk_box_gadget_remove_gadget (GtkBoxGadget *gadget,
                              GtkCssGadget *cssgadget)
{
  gtk_box_gadget_remove_object (gadget, G_OBJECT (cssgadget));
}

void
gtk_box_gadget_reverse_children (GtkBoxGadget *gadget)
{
  GtkBoxGadgetPrivate *priv = gtk_box_gadget_get_instance_private (GTK_BOX_GADGET (gadget));
  int i, j;

  gtk_css_node_reverse_children (gtk_css_gadget_get_node (GTK_CSS_GADGET (gadget)));

  for (i = 0, j = priv->children->len - 1; i < j; i++, j--)
    {
      GtkBoxGadgetChild *child1 = &g_array_index (priv->children, GtkBoxGadgetChild, i);
      GtkBoxGadgetChild *child2 = &g_array_index (priv->children, GtkBoxGadgetChild, j);
      GtkBoxGadgetChild tmp;

      tmp = *child1;
      *child1 = *child2;
      *child2 = tmp;
    }
}

void
gtk_box_gadget_set_gadget_expand (GtkBoxGadget *gadget,
                                  GObject      *object,
                                  gboolean      expand)
{
  GtkBoxGadgetChild *child;

  child = gtk_box_gadget_find_object (gadget, object, NULL);

  if (!child)
    return;

  if (child->expand == expand)
    return;

  child->expand = expand;
  gtk_css_gadget_queue_resize (GTK_CSS_GADGET (gadget));
}

void
gtk_box_gadget_set_gadget_align (GtkBoxGadget *gadget,
                                 GObject      *object,
                                 GtkAlign      align)
{
  GtkBoxGadgetChild *child;

  child = gtk_box_gadget_find_object (gadget, object, NULL);

  if (!child)
    return;

  if (child->align == align)
    return;

  child->align = align;
  gtk_css_gadget_queue_resize (GTK_CSS_GADGET (gadget));
}
