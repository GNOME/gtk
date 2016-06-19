/*
 * Copyright 2016 Red Hat, Inc.
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

#include "gtktabsprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkcsscustomgadgetprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkcsscustomgadgetprivate.h"
#include "a11y/gtkcontaineraccessible.h"
#include "gtksettingsprivate.h"

typedef struct {
  GArray *children;
  GtkCssGadget *gadget;
  guint tick_id;
} GtkTabsPrivate;

typedef struct {
  GtkWidget *child;
  gboolean animating;
  guint64 starttime;
  gdouble factor;
  gint width;
} GtkTabsChild;

G_DEFINE_TYPE_WITH_PRIVATE (GtkTabs, gtk_tabs, GTK_TYPE_CONTAINER)

static void
gtk_tabs_finalize (GObject *object)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (object));

  g_clear_object (&priv->gadget);
  g_array_free (priv->children, TRUE);

  G_OBJECT_CLASS (gtk_tabs_parent_class)->finalize (object);
}

static gboolean
gtk_tabs_draw (GtkWidget *widget,
               cairo_t   *cr)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (widget));

  gtk_css_gadget_draw (priv->gadget, cr);

  return FALSE;
}

static void
gtk_tabs_size_allocate (GtkWidget     *widget,
                        GtkAllocation *allocation)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (widget));
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);

  gtk_css_gadget_allocate (priv->gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static void
gtk_tabs_get_preferred_width (GtkWidget *widget,
                              gint      *minimum_size,
                              gint      *natural_size)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (widget));

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_tabs_get_preferred_height (GtkWidget *widget,
                               gint      *minimum_size,
                               gint      *natural_size)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (widget));

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_tabs_get_preferred_width_for_height (GtkWidget *widget,
                                         gint       height,
                                         gint      *minimum_size,
                                         gint      *natural_size)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (widget));

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     height,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_tabs_get_preferred_height_for_width (GtkWidget *widget,
                                         gint       width,
                                         gint      *minimum_size,
                                         gint      *natural_size)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (widget));

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     width,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_tabs_direction_changed (GtkWidget        *widget,
                            GtkTextDirection  previous_direction)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (widget));
  int i, j;

  gtk_css_node_reverse_children (gtk_widget_get_css_node (widget));

  for (i = 0, j = priv->children->len - 1; i < j; i++, j--)
    {
      GtkTabsChild *child1 = &g_array_index (priv->children, GtkTabsChild, i);
      GtkTabsChild *child2 = &g_array_index (priv->children, GtkTabsChild, j);
      GtkTabsChild tmp;

      tmp = *child1;
      *child1 = *child2;
      *child2 = tmp;
    }

  GTK_WIDGET_CLASS (gtk_tabs_parent_class)->direction_changed (widget, previous_direction);
}

#define DURATION (0.25 * 1e6)

static gdouble
ease_out_cubic (gdouble t)
{
  gdouble p = t - 1;
  return p * p * p + 1;
}

static gboolean
gtk_tabs_animate_cb (GtkWidget     *widget,
                     GdkFrameClock *frame_clock,
                     gpointer       user_data)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (widget));
  int i;
  guint64 time;
  int animating;

  animating = 0;
  time = gdk_frame_clock_get_frame_time (frame_clock);

  for (i = 0; i < priv->children->len; i++)
    {
      GtkTabsChild *child = &g_array_index (priv->children, GtkTabsChild, i);

      if (child->animating)
        {
          child->factor = ease_out_cubic ((time - child->starttime) / DURATION);
          if (child->factor >= 1.0)
            child->animating = FALSE;
          else
            animating += 1;
        }
    }

  for (i = priv->children->len - 1; i >= 0; i--)
    {
      GtkTabsChild *child = &g_array_index (priv->children, GtkTabsChild, i);

      if (child->child == NULL && !child->animating)
        g_array_remove_index (priv->children, i);
    }

  gtk_widget_queue_allocate (widget);

  if (animating == 0)
    {
      gtk_widget_remove_tick_callback (widget, priv->tick_id);
      priv->tick_id = 0;
    }

  return G_SOURCE_CONTINUE;
}

static void
ensure_tick_callback (GtkTabs *tabs)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (tabs);

  if (priv->tick_id == 0)
    priv->tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (tabs),
                                                  gtk_tabs_animate_cb, NULL, NULL);
}

void
gtk_tabs_insert (GtkTabs   *tabs,
                 int        pos,
                 GtkWidget *widget)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (tabs);
  GtkTabsChild child;

  child.child = widget;
  if (gtk_widget_get_mapped (GTK_WIDGET (tabs)) &&
      gtk_settings_get_enable_animations (gtk_widget_get_settings (GTK_WIDGET (tabs))))
    {
      child.animating = TRUE;
      child.starttime = gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (GTK_WIDGET (tabs)));
      child.factor = 0.0;
      ensure_tick_callback (tabs);
    }
  else
    {
      child.animating = FALSE;
      child.starttime = 0;
      child.factor = 1.0;
    }

  if (pos < 0 || pos >= priv->children->len)
    {
      g_array_append_val (priv->children, child);
      gtk_css_node_insert_before (gtk_widget_get_css_node (GTK_WIDGET (tabs)),
                                  gtk_widget_get_css_node (widget),
                                  NULL);
    }
  else
    {
      g_array_insert_val (priv->children, pos, child);
      gtk_css_node_insert_before (gtk_widget_get_css_node (GTK_WIDGET (tabs)),
                                  gtk_widget_get_css_node (widget),
                                  gtk_widget_get_css_node (g_array_index (priv->children,
                                                                          GtkTabsChild,
                                                                          pos + 1).child));
    }

  gtk_widget_set_parent (widget, GTK_WIDGET (tabs));
}

static void
gtk_tabs_add (GtkContainer *container,
              GtkWidget    *widget)
{
  gtk_tabs_insert (GTK_TABS (container), -1, widget);
}

static GtkTabsChild *
gtk_tabs_find_child (GtkTabs   *tabs,
                     GtkWidget *widget,
                     int       *position)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (tabs);
  int i;

  for (i = 0; i < priv->children->len; i++)
    {
      GtkTabsChild *child = &g_array_index (priv->children, GtkTabsChild, i);
      if (child->child == widget)
        {
          if (position)
            *position = i;
          return child;
        }
    }

  return NULL;
}

static void
gtk_tabs_remove (GtkContainer *container,
                 GtkWidget    *widget)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (container));
  GtkTabsChild *child;
  int position;

  child = gtk_tabs_find_child (GTK_TABS (container), widget, &position);
  if (child)
    {
      gtk_widget_unparent (child->child);
      if (gtk_widget_get_mapped (GTK_WIDGET (container)) &&
          gtk_settings_get_enable_animations (gtk_widget_get_settings (GTK_WIDGET (container))))
        {
          child->child = NULL;
          child->animating = TRUE;
          child->starttime = gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (GTK_WIDGET (container)));

          ensure_tick_callback (GTK_TABS (container));
        }
      else
        {
          g_array_remove_index (priv->children, position);
        }
    }
}

static void
gtk_tabs_forall (GtkContainer *container,
                 gboolean      include_internals,
                 GtkCallback   callback,
                 gpointer      callback_data)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (container));
  int i;

  for (i = 0; i < priv->children->len; i++)
    {
      GtkTabsChild *child = &g_array_index (priv->children, GtkTabsChild, i);

      if (child->child)
        callback (child->child, callback_data);
    }
}

static void
gtk_tabs_class_init (GtkTabsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->finalize = gtk_tabs_finalize;

  widget_class->draw = gtk_tabs_draw;
  widget_class->size_allocate = gtk_tabs_size_allocate;
  widget_class->get_preferred_width = gtk_tabs_get_preferred_width;
  widget_class->get_preferred_height = gtk_tabs_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_tabs_get_preferred_height_for_width;
  widget_class->get_preferred_width_for_height = gtk_tabs_get_preferred_width_for_height;
  widget_class->direction_changed = gtk_tabs_direction_changed;

  container_class->add = gtk_tabs_add;
  container_class->remove = gtk_tabs_remove;
  container_class->forall = gtk_tabs_forall;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_FILLER);
}

static void
measure_child (GtkTabsChild   *child,
               GtkOrientation  orientation,
               int             for_size,
               int            *minimum,
               int            *natural)
{
  int child_min, child_nat;

  if (child->child)
    {
      _gtk_widget_get_preferred_size_for_size (child->child,
                                               orientation,
                                               for_size,
                                               &child_min,
                                               &child_nat,
                                               NULL, NULL);
      if (orientation == GTK_ORIENTATION_HORIZONTAL && child->animating)
        {
          child_min = MAX ((int) (child->factor * child_min), 1);
          child_nat = MAX ((int) (child->factor * child_nat), 1);
        }
    }
  else if (child->animating)
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        child_min = child_nat = MAX ((1.0 - child->factor) * child->width, 1);
      else
        child_min = child_nat = 1;
    }

  *minimum = child_min;
  *natural = child_nat;
}

static void
gtk_tabs_measure_orientation (GtkCssGadget   *gadget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (widget));
  int child_min, child_nat;
  int i;

  *minimum = 0;
  *natural = 0;

  for (i = 0; i < priv->children->len; i++)
    {
      GtkTabsChild *child = &g_array_index (priv->children, GtkTabsChild, i);

      measure_child (child, orientation, for_size, &child_min, &child_nat);

      *minimum += child_min;
      *natural += child_nat;
    }
}

static void
gtk_tabs_distribute (GtkTabs          *tabs,
                     int               for_size,
                     int               size,
                     GtkRequestedSize *sizes)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (tabs);
  int i;

  for (i = 0; i < priv->children->len; i++)
    {
      GtkTabsChild *child = &g_array_index (priv->children, GtkTabsChild, i);

      measure_child (child,
                     GTK_ORIENTATION_HORIZONTAL,
                     for_size,
                     &sizes[i].minimum_size,
                     &sizes[i].natural_size);

      size -= sizes[i].minimum_size;
    }

  size = gtk_distribute_natural_allocation (size, priv->children->len, sizes);
}

static void
gtk_tabs_measure_opposite (GtkCssGadget   *gadget,
                           GtkOrientation  orientation,
                           int             for_size,
                           int            *minimum,
                           int            *natural,
                           int            *minimum_baseline,
                           int            *natural_baseline)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (widget));
  int child_min, child_nat;
  GtkRequestedSize *sizes;
  int i;

  *minimum = 0;
  *natural = 0;

  if (for_size >= 0)
    {
      sizes = g_newa (GtkRequestedSize, priv->children->len);
      gtk_tabs_distribute (GTK_TABS (widget), -1, for_size, sizes);
    }

  for (i = 0; i < priv->children->len; i++)
    {
      GtkTabsChild *child = &g_array_index (priv->children, GtkTabsChild, i);

      measure_child (child,
                     orientation,
                     for_size >= 0 ? sizes[i].minimum_size : -1,
                     &child_min, &child_nat);

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);
    }
}

static void
gtk_tabs_measure (GtkCssGadget   *gadget,
                  GtkOrientation  orientation,
                  int             for_size,
                  int            *minimum,
                  int            *natural,
                  int            *minimum_baseline,
                  int            *natural_baseline,
                  gpointer        unused)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_tabs_measure_orientation (gadget, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
  else
    gtk_tabs_measure_opposite (gadget, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
}

static void
gtk_tabs_allocate (GtkCssGadget        *gadget,
                   const GtkAllocation *allocation,
                   int                  baseline,
                   GtkAllocation       *out_clip,
                   gpointer             unused)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (widget));
  GtkRequestedSize *sizes;
  GtkAllocation child_allocation, child_clip;
  int i;

  child_allocation = *allocation;
  sizes = g_newa (GtkRequestedSize, priv->children->len);

  gtk_tabs_distribute (GTK_TABS (widget), allocation->height, allocation->width, sizes);
  for (i = 0; i < priv->children->len; i++)
    {
      GtkTabsChild *child = &g_array_index (priv->children, GtkTabsChild, i);
      child_allocation.width = sizes[i].minimum_size;
      child_allocation.height = allocation->height;
      if (!child->animating)
        {
          child->width = child_allocation.width;
          gtk_widget_size_allocate_with_baseline (child->child, &child_allocation, baseline);
          gtk_widget_get_clip (child->child, &child_clip);
          if (i == 0)
            *out_clip = child_clip;
          else
            gdk_rectangle_union (out_clip, &child_clip, out_clip);
        }
      child_allocation.x += sizes[i].minimum_size;
    }
}

static gboolean
gtk_tabs_render (GtkCssGadget *gadget,
                 cairo_t      *cr,
                 int           x,
                 int           y,
                 int           width,
                 int           height,
                 gpointer      data)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (GTK_TABS (widget));
  int i;

  for (i = 0; i < priv->children->len; i++)
    {
      GtkTabsChild *child = &g_array_index (priv->children, GtkTabsChild, i);
      if (!child->animating)
        gtk_container_propagate_draw (GTK_CONTAINER (widget), child->child, cr);
    }

  return FALSE;
}

static void
gtk_tabs_init (GtkTabs *tabs)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (tabs);
  GtkCssNode *widget_node;

  gtk_widget_set_has_window (GTK_WIDGET (tabs), FALSE);

  priv->children = g_array_new (FALSE, FALSE, sizeof (GtkTabsChild));

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (tabs));
  priv->gadget = gtk_css_custom_gadget_new_for_node (widget_node,
                                                     GTK_WIDGET (tabs),
                                                     gtk_tabs_measure,
                                                     gtk_tabs_allocate,
                                                     gtk_tabs_render,
                                                     NULL, NULL);
}

void
gtk_tabs_reorder_child (GtkTabs   *tabs,
                        GtkWidget *widget,
                        int        position)
{
  GtkTabsPrivate *priv = gtk_tabs_get_instance_private (tabs);
  GtkTabsChild *child;
  GtkTabsChild tmp;
  int old_pos;

  child = gtk_tabs_find_child (tabs, widget, &old_pos);
  if (!child || position == old_pos)
    return;

  tmp = *child;
  g_array_remove_index (priv->children, old_pos);
  g_array_insert_val (priv->children, position, tmp);

  gtk_widget_queue_allocate (GTK_WIDGET (tabs));
}
