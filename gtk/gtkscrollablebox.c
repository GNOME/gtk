/* gtkscrollablebox.c: A box that handles scrollable children
 *
 * SPDX-FileCopyrightText: 2026 Sergey Bugaev
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtkscrollablebox.h"

#include "gtkbuildable.h"
#include "gtkscrollable.h"
#include "gtkorientable.h"
#include "gtkadjustment.h"
#include "gtksizerequest.h"
#include "gtkwidgetprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkadjustmentprivate.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"

/**
 * GtkScrollableBox:
 *
 * Arranges child widgets in a column or a row and scrolls them as
 * a single unit, while handling natively [iface@Scrollable] children
 * such as [class@ListView].
 *
 * `Gtk.ScrollableBox` can be used to add a header or a footer to a
 * list, or to glue several lists together.
 *
 * Similarly to [class@Viewport], `Gtk.ScrollableBox` can move
 * scrolling-unaware child widgets (such as [class@Label]) relative to
 * the viewport. Similarly to [class@Box], `Gtk.ScrollableBox` arranges
 * children either in a row or in a column, depending on its
 * [property@Orientable:orientation].
 *
 * `Gtk.ScrollableBox` itself implements the [iface@Scrollable]
 * interface. You should place it directly into a [class@ScrolledWindow].
 * Note that the default value of [property@Scrollable:vscroll-policy] is
 * [enum@Gtk.ScrollablePolicy.MINIMUM]; depending on your use case,
 * consider setting the box's policy to
 * [enum@Gtk.ScrollablePolicy.NATURAL].
 *
 * `Gtk.ScrollableBox` can handle both natively scrollable children (ones
 * that implement [iface@Scrollable]) and scrolling-unaware ones.
 *
 * The following represents a sample arrangement of widgets to implement
 * a list view with a header scrolling as a single unit:
 *
 * ```
 * Gtk.ScrolledWindow
 * ╰── Gtk.ScrollableBox
 *     ├── Gtk.Label
 *     ├── Gtk.Separator
 *     ╰── Gtk.ListView
 * ```
 *
 * # CSS nodes
 *
 * `Gtk.ScrollableBox` has a single CSS node with the name `box` (same
 * as [class@Box]).
 *
 * # Accessibility
 *
 * `Gtk.ScrollableBox` uses the [enum@Gtk.AccessibleRole.generic] role.
 *
 * Since: 4.24
 */

struct _GtkScrollableBox
{
  GtkWidget parent_instance;

  GtkAdjustment *adjustments[2];
  GtkWidget *anchor_child;

  unsigned baseline_position : 2;
  unsigned orientation : 1;
  unsigned hscroll_policy : 1;
  unsigned vscroll_policy : 1;
  unsigned ignore_child_value_changes : 1;
};

static void gtk_scrollable_box_buildable_iface_init (GtkBuildableIface *iface);

enum
{
  PROP_0,

  PROP_BASELINE_POSITION,

  N_PROPS,

  /* GtkOrientable */
  PROP_ORIENTATION = N_PROPS,

  /* GtkScrollable */
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
};

static GParamSpec *props[N_PROPS];

G_DEFINE_FINAL_TYPE_WITH_CODE (GtkScrollableBox, gtk_scrollable_box, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                      gtk_scrollable_box_buildable_iface_init)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static void
gtk_scrollable_box_init (GtkScrollableBox *box)
{
  box->baseline_position = GTK_BASELINE_POSITION_CENTER;
  box->orientation = GTK_ORIENTATION_VERTICAL;
  box->hscroll_policy = GTK_SCROLL_MINIMUM;
  box->vscroll_policy = GTK_SCROLL_MINIMUM;

  gtk_widget_set_overflow (GTK_WIDGET (box), GTK_OVERFLOW_HIDDEN);
}

static void
gtk_scrollable_box_dispose (GObject *object)
{
  GtkScrollableBox *box = GTK_SCROLLABLE_BOX (object);
  GtkWidget *widget = GTK_WIDGET (box);
  GtkWidget *child;

  g_clear_object (&box->adjustments[GTK_ORIENTATION_HORIZONTAL]);
  g_clear_object (&box->adjustments[GTK_ORIENTATION_VERTICAL]);

  box->anchor_child = NULL;

  while ((child = gtk_widget_get_first_child (widget)))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (gtk_scrollable_box_parent_class)->dispose (object);
}

static void
gtk_scrollable_box_adjustment_value_changed (GtkScrollableBox *box,
                                             GtkAdjustment    *adjustment)
{
  gtk_widget_queue_allocate (GTK_WIDGET (box));
}

static void
gtk_scrollable_box_child_adjustment_value_changed (GtkWidget     *child,
                                                   GtkAdjustment *adjustment)
{
  GtkScrollableBox *box = GTK_SCROLLABLE_BOX (gtk_widget_get_parent (child));

  if (box->ignore_child_value_changes)
    return;

  box->anchor_child = child;
  gtk_widget_queue_allocate (GTK_WIDGET (box));
}

static void
gtk_scrollable_box_set_adjustment (GtkScrollableBox *box,
                                   GtkAdjustment    *adjustment,
                                   GtkOrientation    orientation,
                                   GParamSpec       *pspec)
{
  GtkAdjustment *old_adjustment = box->adjustments[orientation];

  if (adjustment && old_adjustment == adjustment)
    return;

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  if (old_adjustment)
    {
      g_signal_handlers_disconnect_by_func (old_adjustment,
                                            G_CALLBACK (gtk_scrollable_box_adjustment_value_changed),
                                            box);
      g_object_unref (old_adjustment);
    }
  box->adjustments[orientation] = g_object_ref_sink (adjustment);
  g_signal_connect_object (adjustment, "value-changed",
                           G_CALLBACK (gtk_scrollable_box_adjustment_value_changed),
                           box, G_CONNECT_SWAPPED);
  gtk_scrollable_box_adjustment_value_changed (box, adjustment);
  g_object_notify_by_pspec (G_OBJECT (box), pspec);
}

static void
gtk_scrollable_box_set_policy (GtkScrollableBox   *box,
                               GtkScrollablePolicy policy,
                               GtkOrientation      orientation,
                               GParamSpec         *pspec)
{
  GtkWidget *child;

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (box->vscroll_policy == policy)
        return;
      box->vscroll_policy = policy;
    }
  else
    {
      if (box->hscroll_policy == policy)
        return;
      box->hscroll_policy = policy;
    }

  /* Propagate policy to our scrollable children */
  for (child = gtk_widget_get_first_child (GTK_WIDGET (box));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    if (GTK_IS_SCROLLABLE (child))
      g_object_set (child, pspec->name, policy, NULL);

  gtk_widget_queue_allocate (GTK_WIDGET (box));
  g_object_notify_by_pspec (G_OBJECT (box), pspec);
}

static void
gtk_scrollable_box_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkScrollableBox *box = GTK_SCROLLABLE_BOX (object);

  switch (prop_id)
    {
    case PROP_BASELINE_POSITION:
      g_value_set_enum (value, box->baseline_position);
      break;
    case PROP_ORIENTATION:
      g_value_set_enum (value, box->orientation);
      break;
    case PROP_HADJUSTMENT:
      g_value_set_object (value, box->adjustments[GTK_ORIENTATION_HORIZONTAL]);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, box->adjustments[GTK_ORIENTATION_VERTICAL]);
      break;
    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, box->hscroll_policy);
      break;
    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, box->vscroll_policy);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scrollable_box_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkScrollableBox *box = GTK_SCROLLABLE_BOX (object);
  GtkWidget *widget = GTK_WIDGET (box);
  gint enum_value;

  switch (prop_id)
    {
    case PROP_BASELINE_POSITION:
      gtk_scrollable_box_set_baseline_position (box, g_value_get_enum (value));
      break;
    case PROP_ORIENTATION:
      enum_value = g_value_get_enum (value);
      if (box->orientation != enum_value)
        {
          box->orientation = enum_value;
          gtk_widget_queue_resize (widget);
          gtk_widget_update_orientation (widget, enum_value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_HADJUSTMENT:
      gtk_scrollable_box_set_adjustment (box, g_value_get_object (value),
                                         GTK_ORIENTATION_HORIZONTAL, pspec);
      break;
    case PROP_VADJUSTMENT:
      gtk_scrollable_box_set_adjustment (box, g_value_get_object (value),
                                         GTK_ORIENTATION_VERTICAL, pspec);
      break;
    case PROP_HSCROLL_POLICY:
      gtk_scrollable_box_set_policy (box, g_value_get_enum (value),
                                     GTK_ORIENTATION_HORIZONTAL, pspec);
      break;
    case PROP_VSCROLL_POLICY:
      gtk_scrollable_box_set_policy (box, g_value_get_enum (value),
                                     GTK_ORIENTATION_VERTICAL, pspec);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_scrollable_box_buildable_add_child (GtkBuildable *buildable,
                                        GtkBuilder   *builder,
                                        GObject      *child,
                                        const char   *type)
{
  if (GTK_IS_WIDGET (child))
    gtk_scrollable_box_append (GTK_SCROLLABLE_BOX (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_scrollable_box_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_scrollable_box_buildable_add_child;
}

static void
gtk_scrollable_box_compute_expand (GtkWidget *widget,
                                   gboolean  *hexpand_p,
                                   gboolean  *vexpand_p)
{
  GtkWidget *child;
  gboolean hexpand = FALSE;
  gboolean vexpand = FALSE;

  for (child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;
      hexpand = hexpand || gtk_widget_compute_expand (child, GTK_ORIENTATION_HORIZONTAL);
      vexpand = vexpand || gtk_widget_compute_expand (child, GTK_ORIENTATION_VERTICAL);
    }

  *hexpand_p = hexpand;
  *vexpand_p = vexpand;
}

static GtkSizeRequestMode
gtk_scrollable_box_get_request_mode (GtkWidget *widget)
{
  GtkScrollableBox *box = GTK_SCROLLABLE_BOX (widget);
  GtkSizeRequestMode child_mode;
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;
      /* If any child is not constant-size, we prefer to be measured
       * along our orientation.
       */
      child_mode = gtk_widget_get_request_mode (child);
      if (child_mode != GTK_SIZE_REQUEST_CONSTANT_SIZE)
        {
          if (box->orientation == GTK_ORIENTATION_VERTICAL)
            return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
          else
            return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
        }
    }

  /* If all our visible children are constant-size, so are we */
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static int
gtk_scrollable_box_get_spacing (GtkScrollableBox *box)
{
  GtkCssNode *node;
  GtkCssStyle *style;
  GtkCssValue *border_spacing;

  node = gtk_widget_get_css_node (GTK_WIDGET (box));
  style = gtk_css_node_get_style (node);
  border_spacing = style->size->border_spacing;
  if (box->orientation == GTK_ORIENTATION_VERTICAL)
    return _gtk_css_position_value_get_y (border_spacing, 100);
  else
    return _gtk_css_position_value_get_x (border_spacing, 100);
}

static void
gtk_scrollable_box_measure (GtkWidget     *widget,
                            GtkOrientation orientation,
                            int            for_size,
                            int           *minimum,
                            int           *natural,
                            int           *minimum_baseline,
                            int           *natural_baseline)
{
  GtkScrollableBox *box = GTK_SCROLLABLE_BOX (widget);
  int spacing;
  GtkWidget *child;
  gboolean first_child;
  int child_min, child_nat, child_min_baseline, child_nat_baseline;
  gboolean have_baseline, align_baseline;
  int min_above, min_below, nat_above, nat_below;

  /* TODO: do all the things gtk_box_layout_compute_opposite_size_for_size does */
  if (orientation != box->orientation)
    for_size = -1;

  *minimum = *natural = 0;
  min_above = min_below = nat_above = nat_below = 0;

  spacing = gtk_scrollable_box_get_spacing (box);
  first_child = TRUE;
  have_baseline = align_baseline = FALSE;

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child, orientation, for_size,
                          &child_min, &child_nat,
                          &child_min_baseline, &child_nat_baseline);

      if (orientation == box->orientation)
        {
          *minimum += child_min;
          *natural += child_nat;
          if (!first_child)
            {
              *minimum += spacing;
              *natural += spacing;
            }
        }
      else
        {
          *minimum = MAX (*minimum, child_min);
          *natural = MAX (*natural, child_nat);

          if (box->orientation == GTK_ORIENTATION_HORIZONTAL && child_min_baseline != -1)
            {
              GtkAlign child_valign;

              have_baseline = TRUE;

              child_valign = gtk_widget_get_valign (child);
              if (child_valign == GTK_ALIGN_BASELINE_FILL ||
                  child_valign == GTK_ALIGN_BASELINE_CENTER)
                align_baseline = TRUE;

              min_above = MAX (min_above, child_min_baseline);
              nat_above = MAX (nat_above, child_nat_baseline);
              min_below = MAX (min_below, child_min - child_min_baseline);
              nat_below = MAX (nat_below, child_nat - child_nat_baseline);
            }
        }

      first_child = FALSE;
    }

  if (have_baseline)
    {
      g_assert (orientation == GTK_ORIENTATION_VERTICAL);
      g_assert (box->orientation == GTK_ORIENTATION_HORIZONTAL);

      if (align_baseline)
        {
          *minimum = MAX (*minimum, min_above + min_below);
          *natural = MAX (*natural, nat_above + nat_below);
        }

      switch (box->baseline_position)
        {
        case GTK_BASELINE_POSITION_TOP:
          *minimum_baseline = min_above;
          *natural_baseline = nat_above;
          break;
        case GTK_BASELINE_POSITION_BOTTOM:
          *minimum_baseline = *minimum - min_below;
          *natural_baseline = *natural - nat_below;
          break;
        case GTK_BASELINE_POSITION_CENTER:
          *minimum_baseline = min_above + (*minimum - min_above - min_below) / 2;
          *natural_baseline = nat_above + (*natural - nat_above - nat_below) / 2;
          break;
        default:
          g_assert_not_reached ();
          break;
        }
    }
  else
    {
      *minimum_baseline = -1;
      *natural_baseline = -1;
    }
}

static void
gtk_scrollable_box_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  GtkScrollableBox *box = GTK_SCROLLABLE_BOX (widget);
  GtkWidget *child, *fully_covering_child = NULL;
  GtkTextDirection direction;
  GtkScrollablePolicy policy, opposite_policy;
  int size_along, size_across;
  unsigned i, n_visible_children, n_expand_children;
  gboolean align_baseline = FALSE;
  int opposite_below, opposite_above;
  int full_size_across;
  GtkRequestedSize *sizes;
  int spacing, extra_space;
  double position;
  GtkAllocation child_allocation;
  int child_allocation_begin, child_allocation_end, child_allocation_size;
  GtkBorder child_border;
  int child_border_size;
  GtkAdjustment *child_adjustment;
  double child_lower, child_bounded_upper, child_value;
  double child_adjustment_scale, fully_covering_child_adjustment_scale = 0.0;
  double adjustment_value, opposite_adjustment_value, step_increment, page_increment;

  box->ignore_child_value_changes = TRUE;
  direction = _gtk_widget_get_direction (widget);
  spacing = gtk_scrollable_box_get_spacing (box);

  if (box->orientation == GTK_ORIENTATION_VERTICAL)
    {
      policy = box->vscroll_policy;
      opposite_policy = box->hscroll_policy;
      size_along = height;
      size_across = width;
      /* We don't even attempt to handle baselines when vertical */
      baseline = -1;
    }
  else
    {
      policy = box->hscroll_policy;
      opposite_policy = box->vscroll_policy;
      size_along = width;
      size_across = height;
    }

  /* Count visible and expand children, also measure size requirements
   * in the opposite orientation.
   */
  n_visible_children = 0;
  n_expand_children = 0;
  full_size_across = size_across;
  opposite_below = 0;
  opposite_above = 0;
  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      int child_min, child_nat, child_min_baseline, child_nat_baseline;

      if (!gtk_widget_should_layout (child))
        continue;

      n_visible_children++;
      if (gtk_widget_compute_expand (child, box->orientation))
        n_expand_children++;

      gtk_widget_measure (child,
                          OPPOSITE_ORIENTATION (box->orientation), -1,
                          &child_min, &child_nat,
                          &child_min_baseline, &child_nat_baseline);

      if (opposite_policy == GTK_SCROLL_MINIMUM)
        full_size_across = MAX (full_size_across, child_min);
      else
        full_size_across = MAX (full_size_across, child_nat);

      if (box->orientation == GTK_ORIENTATION_HORIZONTAL &&
          child_min_baseline != -1 &&
          (gtk_widget_get_valign (child) == GTK_ALIGN_BASELINE_CENTER ||
           gtk_widget_get_valign (child) == GTK_ALIGN_BASELINE_FILL))
        {
          align_baseline = TRUE;

          if (opposite_policy == GTK_SCROLL_MINIMUM)
            {
              opposite_above = MAX (opposite_above, child_min_baseline);
              opposite_below = MAX (opposite_below, child_min - child_min_baseline);
            }
          else
            {
              opposite_above = MAX (opposite_above, child_nat_baseline);
              opposite_below = MAX (opposite_below, child_nat - child_nat_baseline);
            }
        }
    }

  if (n_visible_children == 0)
    return;

  if (align_baseline)
    full_size_across = MAX (full_size_across, opposite_above + opposite_below);

  /* Collect size requests */
  extra_space = size_along - spacing * (n_visible_children - 1);
  sizes = g_newa (GtkRequestedSize, n_visible_children);
  for (i = 0, child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child, box->orientation, full_size_across,
                          &sizes[i].minimum_size, &sizes[i].natural_size,
                          NULL, NULL);

      /* For GTK_SCROLL_NATURAL, we always allocate at least natural sizes,
       * and scroll if that doesn't fit.
       */
      if (policy == GTK_SCROLL_NATURAL)
        sizes[i].minimum_size = sizes[i].natural_size;

      extra_space -= sizes[i].minimum_size;

      i++;
    }

  /* Distribute available space */
  if (extra_space > 0)
    {
      extra_space = gtk_distribute_natural_allocation (extra_space, n_visible_children,
                                                       sizes);
      if (n_expand_children > 0 && extra_space > 0)
        {
          for (i = 0, child = gtk_widget_get_first_child (widget);
               child != NULL;
               child = gtk_widget_get_next_sibling (child))
            {
              if (!gtk_widget_should_layout (child))
                continue;

              if (gtk_widget_compute_expand (child, box->orientation))
                {
                  sizes[i].minimum_size += extra_space / n_expand_children;
                  if (i < extra_space % n_expand_children)
                    sizes[i].minimum_size++;
                }

              i++;
            }
        }
    }

  /* At this point, sizes[i].minimum_size holds the virtual size allocations */

  adjustment_value = 0.0; /* Shut up the warning */
  opposite_adjustment_value =
    gtk_adjustment_get_value (box->adjustments[OPPOSITE_ORIENTATION (box->orientation)]);
  /* TODO: make this work properly, once we decide how to */
  if (align_baseline && baseline != -1)
    baseline -= opposite_adjustment_value;

  /* Decide on the offset of the virtual axis vs actual allocations */
  if (extra_space >= 0)
    {
      /* Everything fits without any scrolling */
      adjustment_value = 0.0;
    }
  else if (box->anchor_child == NULL)
    {
    keep_existing_adjustment_value:
      adjustment_value = gtk_adjustment_get_value (box->adjustments[box->orientation]);
    }
  else
    {
      position = 0.0;

      for (i = 0, child = gtk_widget_get_first_child (widget);
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        {
          if (!gtk_widget_should_layout (child))
            continue;

          if (i > 0)
            position += spacing;

          if (child == box->anchor_child)
            {
              g_assert (GTK_IS_SCROLLABLE (child));

              if (box->orientation == GTK_ORIENTATION_VERTICAL)
                {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                  child_allocation_size = gtk_widget_get_allocated_height (child);
G_GNUC_END_IGNORE_DEPRECATIONS
                  child_adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (child));
                }
              else
                {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                  child_allocation_size = gtk_widget_get_allocated_width (child);
G_GNUC_END_IGNORE_DEPRECATIONS
                  child_adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (child));
                }
              /* This child has configured its adjustment to a specific value,
               * convert that into our allocation offset.
               */
              child_lower = gtk_adjustment_get_lower (child_adjustment);
              child_bounded_upper = gtk_adjustment_get_bounded_upper (child_adjustment);
              child_value = gtk_adjustment_get_value (child_adjustment);

              if (child_bounded_upper == child_lower)
                /* It doesn't know what it wants, leave things be */
                goto keep_existing_adjustment_value;
              /* Should normally be 1.0 */
              child_adjustment_scale = (sizes[i].minimum_size - child_allocation_size) / (child_bounded_upper - child_lower);
              adjustment_value = (child_value - child_lower) * child_adjustment_scale + position;

              break;
            }

          position += sizes[i].minimum_size;
          i++;
        }
      /* We should have encountered the anchor child */
      g_assert (child == box->anchor_child);
    }

  /* Allocate children */
  position = -adjustment_value;

  for (i = 0, child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      if (i > 0)
        position += spacing;

      /* Allocate in integer coordinates */
      child_allocation_begin = ceil (position);
      child_allocation_end = child_allocation_begin + sizes[i].minimum_size;

      if (GTK_IS_SCROLLABLE (child))
        {
          /* Clamp the child's allocation to the viewport (the range from 0 to size_along),
           * but make sure to at least allocate enough size for the child's border.
           */
          memset (&child_border, 0, sizeof (child_border));
          (void) gtk_scrollable_get_border (GTK_SCROLLABLE (child), &child_border);
          if (box->orientation == GTK_ORIENTATION_VERTICAL)
            child_border_size = child_border.top + child_border.bottom;
          else
            child_border_size = child_border.left + child_border.right;

          if (child_allocation_end < 0)
            /* Fully scrolled away. Allocate at least one pixel on top of the border,
             * so it doesn't go crazy with adjustments.
             */
            child_allocation_begin = child_allocation_end - child_border_size - 1;
          else if (child_allocation_begin > size_along)
            /* Ditto */
            child_allocation_end = child_allocation_begin + child_border_size + 1;
          else if (child_allocation_end < size_along)
            child_allocation_begin = MIN (child_allocation_end - child_border_size,
                                          MAX (child_allocation_begin, 0));
          else if (child_allocation_begin >= 0)
            child_allocation_end = MAX (child_allocation_begin + child_border_size,
                                        size_along);
          else
            {
              /* This child fully covers the viewport. The choice of which way to push
               * the border in case it doesn't fit the viewport is essentially
               * arbitrary; given that in practice borders are used for table headers,
               * prefer to align it to the start.
               */
              fully_covering_child = child;
              child_allocation_begin = 0;
              child_allocation_end = MAX (child_border_size, size_along);
            }
        }
      child_allocation_size = child_allocation_end - child_allocation_begin;

      /* TODO: We currently always allocate full_size_across in the opposite
       * orientation. For scrollable children, we should instead do the same
       * thing as we do along our orientation: clip the allocation and communicate
       * this via adjustments.
       */
      if (box->orientation == GTK_ORIENTATION_VERTICAL)
        {
          child_allocation.x = -opposite_adjustment_value;
          child_allocation.y = child_allocation_begin;
          child_allocation.width = full_size_across;
          child_allocation.height = child_allocation_size;
        }
      else if (direction == GTK_TEXT_DIR_LTR)
        {
          child_allocation.x = child_allocation_begin;
          child_allocation.y = -opposite_adjustment_value;
          child_allocation.width = child_allocation_size;
          child_allocation.height = full_size_across;
        }
      else
        {
          child_allocation.x = width - child_allocation_end;
          child_allocation.y = -opposite_adjustment_value;
          child_allocation.width = child_allocation_size;
          child_allocation.height = full_size_across;
        }

      gtk_widget_size_allocate (child, &child_allocation, baseline);

      if (GTK_IS_SCROLLABLE (child))
        {
          if (box->orientation == GTK_ORIENTATION_VERTICAL)
            child_adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (child));
          else
            child_adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (child));

          child_lower = gtk_adjustment_get_lower (child_adjustment);
          child_bounded_upper = gtk_adjustment_get_bounded_upper (child_adjustment);

          if (child_allocation_begin == position)
            child_adjustment_scale = 0.0; /* Does not matter, we multiply it by 0.0 */
          else if (child_lower == child_bounded_upper)
            child_adjustment_scale = 0.0; /* We don't have much choice what to set */
          else if (child_allocation_size == sizes[i].minimum_size)
            /* We allocated to the child its full size, and should have really hit one
             * of the above branches. If somehow we have not, we have no idea what the
             * child even wants its adjustment to mean. Avoid touching it.
             */
            goto next_child;
          else
            {
              /* Should normally be 1.0 */
              child_adjustment_scale = (child_bounded_upper - child_lower) / (sizes[i].minimum_size - child_allocation_size);
              if (child == fully_covering_child)
                fully_covering_child_adjustment_scale = child_adjustment_scale;
            }

          child_value = child_lower + child_adjustment_scale * (child_allocation_begin - position);

          /* If the scales differ, allow for some numerical instability.
           * If we don't do this, we end up continuously tweaking the adjustment
           * value just a tiny bit. Not enough for the user to notice, but
           * enough to cause layout cycles.
           */
          if (child_adjustment_scale != 1.0 && child_adjustment_scale != 0.0)
            {
              double current_value = gtk_adjustment_get_value (child_adjustment);
              if (G_APPROX_VALUE (current_value, child_value, 0.001))
                goto next_child;
            }

          gtk_adjustment_set_value (child_adjustment, child_value);
          /* Changing the value has likely caused the child to queue another
           * allocation. Perform it synchronously here. If the child changes the
           * value again during allocation, we end up ignoring that change; if we
           * don't we risk getting into a layout loop.
           */
          if (_gtk_widget_get_alloc_needed (child))
            gtk_widget_size_allocate (child, &child_allocation, baseline);
        }

    next_child:
      position += sizes[i].minimum_size;
      i++;
    }

  box->ignore_child_value_changes = FALSE;
  box->anchor_child = NULL;

  if (fully_covering_child != NULL && fully_covering_child_adjustment_scale != 0.0)
    {
      if (box->orientation == GTK_ORIENTATION_VERTICAL)
        child_adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (fully_covering_child));
      else
        child_adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (fully_covering_child));

      step_increment = gtk_adjustment_get_step_increment (child_adjustment) / fully_covering_child_adjustment_scale;
      page_increment = gtk_adjustment_get_page_increment (child_adjustment) / fully_covering_child_adjustment_scale;
    }
  else
    {
      step_increment = size_along * 0.1;
      page_increment = size_along * 0.9;
    }

  gtk_adjustment_configure (box->adjustments[box->orientation],
                            adjustment_value,
                            0.0,
                            MAX (size_along, adjustment_value + position),
                            step_increment,
                            page_increment,
                            size_along);
  gtk_adjustment_configure (box->adjustments[OPPOSITE_ORIENTATION(box->orientation)],
                            opposite_adjustment_value,
                            0.0,
                            full_size_across,
                            size_across * 0.1,
                            size_across * 0.9,
                            size_across);
}

static void
gtk_scrollable_box_class_init (GtkScrollableBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_scrollable_box_dispose;
  object_class->get_property = gtk_scrollable_box_get_property;
  object_class->set_property = gtk_scrollable_box_set_property;

  widget_class->focus = gtk_widget_focus_child;
  widget_class->compute_expand = gtk_scrollable_box_compute_expand;
  widget_class->get_request_mode = gtk_scrollable_box_get_request_mode;
  widget_class->measure = gtk_scrollable_box_measure;
  widget_class->size_allocate = gtk_scrollable_box_size_allocate;

  /**
   * GtkScrollableBox:baseline-position:
   *
   * How to position baseline-aligned children in a horizontal box if
   * additional vertical space is available.
   *
   * See [property@Gtk.Box:baseline-position].
   *
   * Since: 4.24
   */
  props[PROP_BASELINE_POSITION] =
    g_param_spec_enum ("baseline-position", NULL, NULL,
                       GTK_TYPE_BASELINE_POSITION,
                       GTK_BASELINE_POSITION_CENTER,
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_properties (object_class, N_PROPS, props);

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");
  g_object_class_override_property (object_class, PROP_HADJUSTMENT, "hadjustment");
  g_object_class_override_property (object_class, PROP_VADJUSTMENT, "vadjustment");
  g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  gtk_widget_class_set_css_name (widget_class, I_("box"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
}

/**
 * gtk_scrollable_box_new:
 * @orientation: whether the box should position its children
 *   vertically or horizontally
 *
 * Creates a new scrollable box with the given orientation.
 *
 * Use CSS `border-spacing` to add spacing between children of the box.
 *
 * Returns: (transfer floating): the newly created scrollable box
 *
 * Since: 4.24
 */
GtkWidget *
gtk_scrollable_box_new (GtkOrientation orientation)
{
  return g_object_new (GTK_TYPE_SCROLLABLE_BOX,
                       "orientation", orientation,
                       NULL);
}

static void
gtk_scrollable_box_add_child (GtkScrollableBox *box,
                              GtkScrollable    *child)
{
  GtkAdjustment *adjustment;
  const char *adjustment_prop;

  adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  if (box->orientation == GTK_ORIENTATION_VERTICAL)
    adjustment_prop = "vadjustment";
  else
    adjustment_prop = "hadjustment";

  g_signal_connect_object (adjustment, "value-changed",
                           G_CALLBACK (gtk_scrollable_box_child_adjustment_value_changed),
                           child, G_CONNECT_SWAPPED);

  g_object_set (child,
                "hscroll-policy", box->hscroll_policy,
                "vscroll-policy", box->vscroll_policy,
                adjustment_prop, adjustment,
                NULL);
}

/**
 * gtk_scrollable_box_append:
 * @box: a scrollable box
 * @child: (transfer floating): the widget to append
 *
 * Adds the given child widget at the end of the box.
 *
 * Since: 4.24
 */
void
gtk_scrollable_box_append (GtkScrollableBox *box,
                           GtkWidget        *child)
{
  g_return_if_fail (GTK_IS_SCROLLABLE_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_widget_insert_before (child, GTK_WIDGET (box), NULL);
  if (GTK_IS_SCROLLABLE (child))
    gtk_scrollable_box_add_child (box, GTK_SCROLLABLE (child));
}

/**
 * gtk_scrollable_box_prepend:
 * @box: a scrollable box
 * @child: (transfer floating): the widget to prepend
 *
 * Adds the given child widget at the beginning of the box.
 *
 * Since: 4.24
 */
void
gtk_scrollable_box_prepend (GtkScrollableBox *box,
                            GtkWidget        *child)
{
  g_return_if_fail (GTK_IS_SCROLLABLE_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_widget_insert_after (child, GTK_WIDGET (box), NULL);
  if (GTK_IS_SCROLLABLE (child))
    gtk_scrollable_box_add_child (box, GTK_SCROLLABLE (child));
}

/**
 * gtk_scrollable_box_remove:
 * @box: a scrollable box
 * @child: a child widget to remove
 *
 * Removes a child widget from the box.
 *
 * Since: 4.24
 */
void
gtk_scrollable_box_remove (GtkScrollableBox *box,
                           GtkWidget        *child)
{
  GtkAdjustment *adjustment;

  g_return_if_fail (GTK_IS_SCROLLABLE_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == GTK_WIDGET (box));

  if (box->anchor_child == child)
    box->anchor_child = NULL;

  if (GTK_IS_SCROLLABLE (child))
    {
      if (box->orientation == GTK_ORIENTATION_VERTICAL)
        adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (child));
      else
        adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (child));

      g_signal_handlers_disconnect_by_func (adjustment,
                                            gtk_scrollable_box_child_adjustment_value_changed,
                                            child);
    }

  gtk_widget_unparent (child);
}

/**
 * gtk_scrollable_box_insert_child_after:
 * @box: a scrollable box
 * @child: (transfer floating): the widget to insert
 * @sibling: (nullable): the sibling after which to insert @child
 *
 * Inserts the given child into the box at a specific position.
 *
 * The child is placed after @sibling in the list of @box children.
 * If @sibling is null, the @child is placed at the beginning.
 *
 * Since: 4.24
 */
void
gtk_scrollable_box_insert_child_after (GtkScrollableBox *box,
                                       GtkWidget        *child,
                                       GtkWidget        *sibling)
{
  g_return_if_fail (GTK_IS_SCROLLABLE_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));
  if (sibling)
    {
      g_return_if_fail (GTK_IS_WIDGET (sibling));
      g_return_if_fail (gtk_widget_get_parent (sibling) == GTK_WIDGET (box));
    }

  gtk_widget_insert_after (child, GTK_WIDGET (box), sibling);
  if (GTK_IS_SCROLLABLE (child))
    gtk_scrollable_box_add_child (box, GTK_SCROLLABLE (child));
}

/**
 * gtk_scrollable_box_reorder_child_after:
 * @box: a scrollable box
 * @child: (transfer floating): the widget to move
 * @sibling: (nullable): the sibling after which to place @child
 *
 * Moves an existing child into a given position.
 *
 * The child is placed after @sibling in the list of @box children.
 * If @sibling is null, the @child is placed at the beginning.
 *
 * Since: 4.24
 */
void
gtk_scrollable_box_reorder_child_after (GtkScrollableBox *box,
                                        GtkWidget        *child,
                                        GtkWidget        *sibling)
{
  g_return_if_fail (GTK_IS_SCROLLABLE_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == GTK_WIDGET (box));
  if (sibling)
    {
      g_return_if_fail (GTK_IS_WIDGET (sibling));
      g_return_if_fail (gtk_widget_get_parent (sibling) == GTK_WIDGET (box));
    }

  gtk_widget_insert_after (child, GTK_WIDGET (box), sibling);
}

/**
 * gtk_scrollable_box_get_baseline_position:
 * @box: a scrollable box
 *
 * Gets the baseline position set by [method@Gtk.ScrollableBox.set_baseline_position].
 *
 * Returns: the baseline position
 *
 * Since: 4.24
 */
GtkBaselinePosition
gtk_scrollable_box_get_baseline_position (GtkScrollableBox *box)
{
  g_return_val_if_fail (GTK_IS_SCROLLABLE_BOX (box), GTK_BASELINE_POSITION_CENTER);

  return box->baseline_position;
}

/**
 * gtk_scrollable_box_set_baseline_position:
 * @box: a scrollable box
 * @position: the baseline position value
 *
 * Sets the baseline position of the box.
 *
 * For horizontal boxes with baseline-aligned children, this controls
 * how the children are placed vertically if there is more vertical
 * space available than the minimum required for fitting the children.
 *
 * See [method@Gtk.Box.set_baseline_position].
 *
 * Since: 4.24
 */
void
gtk_scrollable_box_set_baseline_position (GtkScrollableBox   *box,
                                          GtkBaselinePosition position)
{
  g_return_if_fail (GTK_IS_SCROLLABLE_BOX (box));

  if (box->baseline_position == position)
    return;

  box->baseline_position = position;
  gtk_widget_queue_resize (GTK_WIDGET (box));
  g_object_notify_by_pspec (G_OBJECT (box), props[PROP_BASELINE_POSITION]);
}
