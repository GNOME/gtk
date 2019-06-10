/* gtkboxlayout.c: Box layout manager
 *
 * Copyright 2019  GNOME Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.         See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkboxlayout.h"

#include "gtkcsspositionvalueprivate.h"
#include "gtkintl.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"

/**
 * SECTION:gtkboxlayout
 * @Title: GtkBoxLayout
 * @Short_description: Layout manager for placing all children in a single row or column
 *
 * A GtkBoxLayout is a layout manager that arranges the children of any
 * widget using it into a single row or column, depending on the value
 * of its #GtkOrientable:orientation property. Within the other dimension
 * all children all allocated the same size. The GtkBoxLayout will respect
 * the #GtkWidget:halign and #GtkWidget:valign properties of each child
 * widget.
 *
 * If you want all children to be assigned the same size, you can use
 * the #GtkBoxLayout:homogeneous property.
 *
 * If you want to specify the amount of space placed between each child,
 * you can use the #GtkBoxLayout:spacing property.
 */

struct _GtkBoxLayout
{
  GtkLayoutManager parent_instance;

  gboolean homogeneous;
  guint spacing;
  GtkOrientation orientation;
  GtkBaselinePosition baseline_position;
};

G_DEFINE_TYPE_WITH_CODE (GtkBoxLayout, gtk_box_layout, GTK_TYPE_LAYOUT_MANAGER,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

enum {
  PROP_HOMOGENEOUS = 1,
  PROP_SPACING,
  PROP_BASELINE_POSITION,

  /* From GtkOrientable */
  PROP_ORIENTATION,

  N_PROPS = PROP_ORIENTATION
};

static GParamSpec *box_layout_props[N_PROPS];

static void
gtk_box_layout_set_orientation (GtkBoxLayout   *self,
                                GtkOrientation  orientation)
{
  GtkLayoutManager *layout_manager = GTK_LAYOUT_MANAGER (self);
  GtkWidget *widget;

  if (self->orientation == orientation)
    return;

  self->orientation = orientation;

  widget = gtk_layout_manager_get_widget (layout_manager);
  if (widget != NULL && GTK_IS_ORIENTABLE (widget))
    _gtk_orientable_set_style_classes (GTK_ORIENTABLE (widget));

  gtk_layout_manager_layout_changed (layout_manager);

  g_object_notify (G_OBJECT (self), "orientation");
}

static void
gtk_box_layout_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkBoxLayout *self = GTK_BOX_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_HOMOGENEOUS:
      gtk_box_layout_set_homogeneous (self, g_value_get_boolean (value));
      break;

    case PROP_SPACING:
      gtk_box_layout_set_spacing (self, g_value_get_int (value));
      break;

    case PROP_BASELINE_POSITION:
      gtk_box_layout_set_baseline_position (self, g_value_get_enum (value));
      break;

    case PROP_ORIENTATION:
      gtk_box_layout_set_orientation (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_box_layout_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkBoxLayout *box_layout = GTK_BOX_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, box_layout->homogeneous);
      break;

    case PROP_SPACING:
      g_value_set_int (value, box_layout->spacing);
      break;

    case PROP_BASELINE_POSITION:
      g_value_set_enum (value, box_layout->baseline_position);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, box_layout->orientation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
count_expand_children (GtkWidget *widget,
                       GtkOrientation orientation,
                       gint *visible_children,
                       gint *expand_children)
{
  GtkWidget *child;

  *visible_children = *expand_children = 0;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      *visible_children += 1;

      if (gtk_widget_compute_expand (child, orientation))
        *expand_children += 1;
    }
}

static gint
get_spacing (GtkBoxLayout *self,
             GtkStyleContext *style_context)
{
  GtkCssValue *border_spacing;
  gint css_spacing;

  border_spacing = _gtk_style_context_peek_property (style_context, GTK_CSS_PROPERTY_BORDER_SPACING);
  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    css_spacing = _gtk_css_position_value_get_x (border_spacing, 100);
  else
    css_spacing = _gtk_css_position_value_get_y (border_spacing, 100);

  return css_spacing + self->spacing;
}

static void
gtk_box_layout_compute_size (GtkBoxLayout *self,
                             GtkWidget    *widget,
                             int           for_size,
                             int          *minimum,
                             int          *natural)
{
  GtkWidget *child;
  int n_visible_children = 0;
  int required_min = 0, required_nat = 0;
  int largest_min = 0, largest_nat = 0;
  int spacing = get_spacing (self, _gtk_widget_get_style_context (widget));

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      int child_min = 0;
      int child_nat = 0;

      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child, self->orientation,
                          for_size,
                          &child_min, &child_nat,
                          NULL, NULL);

      largest_min = MAX (largest_min, child_min);
      largest_nat = MAX (largest_nat, child_nat);

      required_min += child_min;
      required_nat += child_nat;

      n_visible_children += 1;
    }

  if (n_visible_children > 0)
    {
      if (self->homogeneous)
        {
          required_min = largest_min * n_visible_children;
          required_nat = largest_nat * n_visible_children;
        }

      required_min += (n_visible_children - 1) * spacing;
      required_nat += (n_visible_children - 1) * spacing;
    }

  if (minimum != NULL)
    *minimum = required_min;
  if (natural != NULL)
    *natural = required_nat;
}

static void
gtk_box_layout_compute_opposite_size (GtkBoxLayout *self,
                                      GtkWidget    *widget,
                                      int           for_size,
                                      int          *minimum,
                                      int          *natural,
                                      int          *min_baseline,
                                      int          *nat_baseline)
{
  GtkWidget *child;
  int nvis_children;
  int nexpand_children;
  int computed_minimum = 0, computed_natural = 0;
  int computed_minimum_above = 0, computed_natural_above = 0;
  int computed_minimum_below = 0, computed_natural_below = 0;
  int computed_minimum_baseline = -1, computed_natural_baseline = -1;
  GtkRequestedSize *sizes;
  int extra_space, size_given_to_child, i;
  int children_minimum_size = 0;
  int child_size, child_minimum, child_natural;
  int child_minimum_baseline, child_natural_baseline;
  int n_extra_widgets = 0;
  int spacing;
  gboolean have_baseline;

  count_expand_children (widget, self->orientation, &nvis_children, &nexpand_children);

  if (nvis_children <= 0)
    return;

  spacing = get_spacing (self, _gtk_widget_get_style_context (widget));
  sizes = g_newa (GtkRequestedSize, nvis_children);
  extra_space = MAX (0, for_size - (nvis_children - 1) * spacing);

  /* Retrieve desired size for visible children */
  for (i = 0, child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child,
                          self->orientation,
                          -1,
                          &sizes[i].minimum_size, &sizes[i].natural_size,
                          NULL, NULL);

      children_minimum_size += sizes[i].minimum_size;
      i += 1;
    }

  if (self->homogeneous)
    {
      /* We still need to run the above loop to populate the minimum sizes for
       * children that aren't going to fill.
       */

      size_given_to_child = extra_space / nvis_children;
      n_extra_widgets = extra_space % nvis_children;
    }
  else
    {
      /* Bring children up to size first */
      extra_space -= children_minimum_size;
      extra_space = MAX (0, extra_space);
      extra_space = gtk_distribute_natural_allocation (extra_space, nvis_children, sizes);

      /* Calculate space which hasn't distributed yet,
       * and is available for expanding children.
       */
      if (nexpand_children > 0)
        {
          size_given_to_child = extra_space / nexpand_children;
          n_extra_widgets = extra_space % nexpand_children;
        }
      else
        {
          size_given_to_child = 0;
        }
    }

  have_baseline = FALSE;
  for (i = 0, child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      /* Assign the child's size. */
      if (self->homogeneous)
        {
          child_size = size_given_to_child;

          if (n_extra_widgets > 0)
            {
              child_size++;
              n_extra_widgets--;
            }
        }
      else
        {
          child_size = sizes[i].minimum_size;

          if (gtk_widget_compute_expand (child, self->orientation))
            {
              child_size += size_given_to_child;

              if (n_extra_widgets > 0)
                {
                  child_size++;
                  n_extra_widgets--;
                }
            }
        }

      child_minimum_baseline = child_natural_baseline = -1;
      /* Assign the child's position. */
      gtk_widget_measure (child,
                          OPPOSITE_ORIENTATION (self->orientation),
                          child_size,
                          &child_minimum, &child_natural,
                          &child_minimum_baseline, &child_natural_baseline);

      if (child_minimum_baseline >= 0)
        {
          have_baseline = TRUE;
          computed_minimum_below = MAX (computed_minimum_below, child_minimum - child_minimum_baseline);
          computed_natural_below = MAX (computed_natural_below, child_natural - child_natural_baseline);
          computed_minimum_above = MAX (computed_minimum_above, child_minimum_baseline);
          computed_natural_above = MAX (computed_natural_above, child_natural_baseline);
        }
      else
        {
          computed_minimum = MAX (computed_minimum, child_minimum);
          computed_natural = MAX (computed_natural, child_natural);
        }
      i += 1;
    }

  if (have_baseline)
    {
      computed_minimum = MAX (computed_minimum, computed_minimum_below + computed_minimum_above);
      computed_natural = MAX (computed_natural, computed_natural_below + computed_natural_above);
      switch (self->baseline_position)
        {
        case GTK_BASELINE_POSITION_TOP:
          computed_minimum_baseline = computed_minimum_above;
          computed_natural_baseline = computed_natural_above;
          break;
        case GTK_BASELINE_POSITION_CENTER:
          computed_minimum_baseline = computed_minimum_above + MAX((computed_minimum - (computed_minimum_above + computed_minimum_below)) / 2, 0);
          computed_natural_baseline = computed_natural_above + MAX((computed_natural - (computed_natural_above + computed_natural_below)) / 2, 0);
          break;
        case GTK_BASELINE_POSITION_BOTTOM:
          computed_minimum_baseline = computed_minimum - computed_minimum_below;
          computed_natural_baseline = computed_natural - computed_natural_below;
          break;
        default:
          break;
        }
    }

  if (minimum != NULL)
    *minimum = computed_minimum;
  if (natural != NULL)
    *natural = MAX (computed_natural, computed_natural_below + computed_natural_above);

  if (min_baseline != NULL)
    *min_baseline = computed_minimum_baseline;
  if (nat_baseline != NULL)
    *nat_baseline = computed_natural_baseline;
}

static void
gtk_box_layout_measure (GtkLayoutManager *layout_manager,
                        GtkWidget        *widget,
                        GtkOrientation    orientation,
                        int               for_size,
                        int              *minimum,
                        int              *natural,
                        int              *min_baseline,
                        int              *nat_baseline)
{
  GtkBoxLayout *self = GTK_BOX_LAYOUT (layout_manager);

  if (self->orientation != orientation)
    {
      gtk_box_layout_compute_opposite_size (self, widget, for_size,
                                            minimum, natural,
                                            min_baseline, nat_baseline);
    }
  else
    {
      gtk_box_layout_compute_size (self, widget, for_size,
                                   minimum, natural);
    }
}

static void
gtk_box_layout_allocate (GtkLayoutManager *layout_manager,
                         GtkWidget        *widget,
                         int               width,
                         int               height,
                         int               baseline)
{
  GtkBoxLayout *self = GTK_BOX_LAYOUT (layout_manager);
  GtkWidget *child;
  gint nvis_children;
  gint nexpand_children;
  GtkTextDirection direction;
  GtkAllocation child_allocation;
  GtkRequestedSize *sizes;
  gint child_minimum_baseline, child_natural_baseline;
  gint minimum_above, natural_above;
  gint minimum_below, natural_below;
  gboolean have_baseline;
  gint extra_space;
  gint children_minimum_size = 0;
  gint size_given_to_child;
  gint n_extra_widgets = 0; /* Number of widgets that receive 1 extra px */
  gint x = 0, y = 0, i;
  gint child_size;
  gint spacing;

  count_expand_children (widget, self->orientation, &nvis_children, &nexpand_children);

  /* If there is no visible child, simply return. */
  if (nvis_children <= 0)
    return;

  direction = _gtk_widget_get_direction (widget);
  sizes = g_newa (GtkRequestedSize, nvis_children);
  spacing = get_spacing (self, _gtk_widget_get_style_context (widget));

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    extra_space = width - (nvis_children - 1) * spacing;
  else
    extra_space = height - (nvis_children - 1) * spacing;

  have_baseline = FALSE;
  minimum_above = natural_above = 0;
  minimum_below = natural_below = 0;

  /* Retrieve desired size for visible children. */
  for (i = 0, child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child,
                          self->orientation,
                          self->orientation == GTK_ORIENTATION_HORIZONTAL ? height : width,
                          &sizes[i].minimum_size, &sizes[i].natural_size,
                          NULL, NULL);

      children_minimum_size += sizes[i].minimum_size;

      sizes[i].data = child;

      i++;
    }

  if (self->homogeneous)
    {
      /* We still need to run the above loop to populate the minimum sizes for
       * children that aren't going to fill.
       */

      size_given_to_child = extra_space / nvis_children;
      n_extra_widgets = extra_space % nvis_children;
    }
  else
    {
      /* Bring children up to size first */
      extra_space -= children_minimum_size;
      extra_space = MAX (0, extra_space);
      extra_space = gtk_distribute_natural_allocation (extra_space, nvis_children, sizes);

      /* Calculate space which hasn't distributed yet,
       * and is available for expanding children.
       */
      if (nexpand_children > 0)
        {
          size_given_to_child = extra_space / nexpand_children;
          n_extra_widgets = extra_space % nexpand_children;
        }
      else
        {
          size_given_to_child = 0;
        }
    }

  /* Allocate child sizes. */
  for (i = 0, child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      /* Assign the child's size. */
      if (self->homogeneous)
        {
          child_size = size_given_to_child;

          if (n_extra_widgets > 0)
            {
              child_size++;
              n_extra_widgets--;
            }
        }
      else
        {
          child_size = sizes[i].minimum_size;

          if (gtk_widget_compute_expand (child, self->orientation))
            {
              child_size += size_given_to_child;

              if (n_extra_widgets > 0)
                {
                  child_size++;
                  n_extra_widgets--;
                }
            }
        }

      sizes[i].natural_size = child_size;

      if (self->orientation == GTK_ORIENTATION_HORIZONTAL &&
          gtk_widget_get_valign (child) == GTK_ALIGN_BASELINE)
        {
          int child_allocation_width;
          int child_minimum_height, child_natural_height;

          child_allocation_width = child_size;

          child_minimum_baseline = -1;
          child_natural_baseline = -1;
          gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL,
                              child_allocation_width,
                              &child_minimum_height, &child_natural_height,
                              &child_minimum_baseline, &child_natural_baseline);

          if (child_minimum_baseline >= 0)
            {
              have_baseline = TRUE;
              minimum_below = MAX (minimum_below, child_minimum_height - child_minimum_baseline);
              natural_below = MAX (natural_below, child_natural_height - child_natural_baseline);
              minimum_above = MAX (minimum_above, child_minimum_baseline);
              natural_above = MAX (natural_above, child_natural_baseline);
            }
        }

      i++;
    }

  if (self->orientation == GTK_ORIENTATION_VERTICAL)
    baseline = -1;

  /* we only calculate our own baseline if we don't get one passed from the parent
   * and any of the child widgets explicitly request one */
  if (baseline == -1 && have_baseline)
    {
      /* TODO: This is purely based on the minimum baseline, when things fit we should
         use the natural one? */

      switch (self->baseline_position)
        {
        case GTK_BASELINE_POSITION_TOP:
          baseline = minimum_above;
          break;
        case GTK_BASELINE_POSITION_CENTER:
          baseline = minimum_above + (height - (minimum_above + minimum_below)) / 2;
          break;
        case GTK_BASELINE_POSITION_BOTTOM:
          baseline = height - minimum_below;
          break;
        default:
          break;
        }
    }

  /* Allocate child positions. */
  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      child_allocation.y = 0;
      child_allocation.height = height;
      x = 0;
    }
  else
    {
      child_allocation.x = 0;
      child_allocation.width = width;
      y = 0;
    }

  for (i = 0, child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      child_size = sizes[i].natural_size;

      /* Assign the child's position. */
      if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          child_allocation.width = child_size;
          child_allocation.x = x;

          x += child_size + spacing;

          if (direction == GTK_TEXT_DIR_RTL)
            child_allocation.x = width - child_allocation.x - child_allocation.width;

        }
      else /* (self->orientation == GTK_ORIENTATION_VERTICAL) */
        {
          child_allocation.height = child_size;
          child_allocation.y = y;

          y += child_size + spacing;
        }

      gtk_widget_size_allocate (child, &child_allocation, baseline);

      i++;
    }
}

static void
gtk_box_layout_class_init (GtkBoxLayoutClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkLayoutManagerClass *layout_manager_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  gobject_class->set_property = gtk_box_layout_set_property;
  gobject_class->get_property = gtk_box_layout_get_property;

  layout_manager_class->measure = gtk_box_layout_measure;
  layout_manager_class->allocate = gtk_box_layout_allocate;

  /**
   * GtkBoxLayout:homogeneous:
   *
   * Whether the box layout should distribute the available space
   * homogeneously among the children of the widget using it as a
   * layout manager.
   */
  box_layout_props[PROP_HOMOGENEOUS] =
    g_param_spec_boolean ("homogeneous",
                          P_("Homogeneous"),
                          P_("Distribute space homogeneously"),
                          FALSE,
                          GTK_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkBoxLayout:spacing:
   *
   * The space between each child of the widget using the box
   * layout as its layout manager.
   */
  box_layout_props[PROP_SPACING] =
    g_param_spec_int ("spacing",
                      P_("Spacing"),
                      P_("Spacing between widgets"),
                      0, G_MAXINT, 0,
                      GTK_PARAM_READWRITE |
                      G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkBoxLayout:baseline-position:
   *
   * The position of the allocated baseline within the extra space
   * allocated to each child of the widget using a box layout
   * manager.
   *
   * This property is only relevant for horizontal layouts containing
   * at least one child with a baseline alignment.
   */
  box_layout_props[PROP_BASELINE_POSITION] =
    g_param_spec_enum ("baseline-position",
                       P_("Baseline position"),
                       P_("The position of the baseline aligned widgets if extra space is available"),
                       GTK_TYPE_BASELINE_POSITION,
                       GTK_BASELINE_POSITION_CENTER,
                       GTK_PARAM_READWRITE |
                       G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, box_layout_props);
  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");
}

static void
gtk_box_layout_init (GtkBoxLayout *self)
{
  self->homogeneous = FALSE;
  self->spacing = 0;
  self->orientation = GTK_ORIENTATION_HORIZONTAL;
  self->baseline_position = GTK_BASELINE_POSITION_CENTER;
}

/**
 * gtk_box_layout_new:
 * @orientation: the orientation for the new layout
 *
 * Creates a new box layout.
 *
 * Returns: a new box layout
 */
GtkLayoutManager *
gtk_box_layout_new (GtkOrientation orientation)
{
  return g_object_new (GTK_TYPE_BOX_LAYOUT,
                       "orientation", orientation,
                       NULL);
}

/**
 * gtk_box_layout_set_homogeneous:
 * @box_layout: a #GtkBoxLayout
 * @homogeneous: %TRUE to set the box layout as homogeneous
 *
 * Sets whether the box layout will allocate the same
 * size to all children.
 */
void
gtk_box_layout_set_homogeneous (GtkBoxLayout *box_layout,
                                gboolean      homogeneous)
{
  g_return_if_fail (GTK_IS_BOX_LAYOUT (box_layout));

  homogeneous = !!homogeneous;
  if (box_layout->homogeneous == homogeneous)
    return;

  box_layout->homogeneous = homogeneous;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (box_layout));
  g_object_notify_by_pspec (G_OBJECT (box_layout), box_layout_props[PROP_HOMOGENEOUS]);
}

/**
 * gtk_box_layout_get_homogeneous:
 * @box_layout: a #GtkBoxLayout
 *
 * Returns whether the layout is set to be homogeneous.
 *
 * Return: %TRUE if the layout is homogeneous
 */
gboolean
gtk_box_layout_get_homogeneous (GtkBoxLayout *box_layout)
{
  g_return_val_if_fail (GTK_IS_BOX_LAYOUT (box_layout), FALSE);

  return box_layout->homogeneous;
}

/**
 * gtk_box_layout_set_spacing:
 * @box_layout: a #GtkBoxLayout
 * @spacing: the spacing to apply between children
 *
 * Sets how much spacing to put between children.
 */
void
gtk_box_layout_set_spacing (GtkBoxLayout *box_layout,
                            guint         spacing)
{
  g_return_if_fail (GTK_IS_BOX_LAYOUT (box_layout));

  if (box_layout->spacing == spacing)
    return;

  box_layout->spacing = spacing;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (box_layout));
  g_object_notify_by_pspec (G_OBJECT (box_layout), box_layout_props[PROP_SPACING]);
}

/**
 * gtk_box_layout_get_spacing:
 * @box_layout: a #GtkBoxLayout
 *
 * Returns the space that @box_layout puts between children.
 *
 * Returns: the spacing of the layout
 */
guint
gtk_box_layout_get_spacing (GtkBoxLayout *box_layout)
{
  g_return_val_if_fail (GTK_IS_BOX_LAYOUT (box_layout), 0);

  return box_layout->spacing;
}

/**
 * gtk_box_layout_set_baseline_position:
 * @box_layout: a #GtkBoxLayout
 * @position: a #GtkBaselinePosition
 *
 * Sets the baseline position of a box layout.
 *
 * The baseline position affects only horizontal boxes with at least one
 * baseline aligned child. If there is more vertical space available than
 * requested, and the baseline is not allocated by the parent then the
 * given @position is used to allocate the baseline within the extra
 * space available.
 */
void
gtk_box_layout_set_baseline_position (GtkBoxLayout        *box_layout,
                                      GtkBaselinePosition  position)
{
  g_return_if_fail (GTK_IS_BOX_LAYOUT (box_layout));

  if (box_layout->baseline_position != position)
    {
      box_layout->baseline_position = position;

      g_object_notify_by_pspec (G_OBJECT (box_layout), box_layout_props[PROP_BASELINE_POSITION]);

      gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (box_layout));
    }
}

/**
 * gtk_box_layout_get_baseline_position:
 * @box_layout: a #GtkBoxLayout
 *
 * Gets the value set by gtk_box_layout_set_baseline_position().
 *
 * Returns: the baseline position
 */
GtkBaselinePosition
gtk_box_layout_get_baseline_position (GtkBoxLayout *box_layout)
{
  g_return_val_if_fail (GTK_IS_BOX_LAYOUT (box_layout), GTK_BASELINE_POSITION_CENTER);

  return box_layout->baseline_position;
}
