/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/**
 * SECTION:gtkbox
 * @Short_description: A container for packing widgets in a single row or column
 * @Title: GtkBox
 * @See_also: #GtkGrid
 *
 * The GtkBox widget arranges child widgets into a single row or column,
 * depending upon the value of its #GtkOrientable:orientation property. Within
 * the other dimension, all children are allocated the same size. Of course,
 * the #GtkWidget:halign and #GtkWidget:valign properties can be used on
 * the children to influence their allocation.
 *
 * Use repeated calls to gtk_container_add() to pack widgets into a
 * GtkBox from start to end. Use gtk_container_remove() to remove widgets
 * from the GtkBox. gtk_box_insert_child_after() can be used to add a child
 * at a particular position.
 *
 * Use gtk_box_set_homogeneous() to specify whether or not all children
 * of the GtkBox are forced to get the same amount of space.
 *
 * Use gtk_box_set_spacing() to determine how much space will be
 * minimally placed between all children in the GtkBox. Note that
 * spacing is added between the children.
 *
 * Use gtk_box_reorder_child_after() to move a child to a different
 * place in the box.
 *
 * # CSS nodes
 *
 * GtkBox uses a single CSS node with name box.
 */

#include "config.h"

#include "gtkbox.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkintl.h"
#include "gtkorientable.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetpath.h"
#include "gtkwidgetprivate.h"
#include "a11y/gtkcontaineraccessible.h"


enum {
  PROP_0,
  PROP_SPACING,
  PROP_HOMOGENEOUS,
  PROP_BASELINE_POSITION,

  /* orientable */
  PROP_ORIENTATION,
  LAST_PROP = PROP_ORIENTATION
};

typedef struct
{
  GtkOrientation  orientation;
  gint16          spacing;

  guint           homogeneous    : 1;
  guint           baseline_pos   : 2;
} GtkBoxPrivate;

static GParamSpec *props[LAST_PROP] = { NULL, };

static void gtk_box_size_allocate         (GtkWidget *widget,
                                           int        width,
                                           int        height,
                                           int        baseline);

static void gtk_box_set_property       (GObject        *object,
                                        guint           prop_id,
                                        const GValue   *value,
                                        GParamSpec     *pspec);
static void gtk_box_get_property       (GObject        *object,
                                        guint           prop_id,
                                        GValue         *value,
                                        GParamSpec     *pspec);
static void gtk_box_add                (GtkContainer   *container,
                                        GtkWidget      *widget);
static void gtk_box_remove             (GtkContainer   *container,
                                        GtkWidget      *widget);
static void gtk_box_forall             (GtkContainer   *container,
                                        GtkCallback     callback,
                                        gpointer        callback_data);
static GType gtk_box_child_type        (GtkContainer   *container);
static GtkWidgetPath * gtk_box_get_path_for_child
                                       (GtkContainer   *container,
                                        GtkWidget      *child);
static void gtk_box_measure (GtkWidget      *widget,
                             GtkOrientation  orientation,
                             int             for_size,
                             int            *minimum,
                             int            *natural,
                             int            *minimum_baseline,
                             int            *natural_baseline);

G_DEFINE_TYPE_WITH_CODE (GtkBox, gtk_box, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkBox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

static void
gtk_box_class_init (GtkBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->set_property = gtk_box_set_property;
  object_class->get_property = gtk_box_get_property;

  widget_class->size_allocate = gtk_box_size_allocate;
  widget_class->measure = gtk_box_measure;

  container_class->add = gtk_box_add;
  container_class->remove = gtk_box_remove;
  container_class->forall = gtk_box_forall;
  container_class->child_type = gtk_box_child_type;
  container_class->get_path_for_child = gtk_box_get_path_for_child;

  g_object_class_override_property (object_class,
                                    PROP_ORIENTATION,
                                    "orientation");

  props[PROP_SPACING] =
    g_param_spec_int ("spacing",
                      P_("Spacing"),
                      P_("The amount of space between children"),
                      0, G_MAXINT, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_HOMOGENEOUS] =
    g_param_spec_boolean ("homogeneous",
                          P_("Homogeneous"),
                          P_("Whether the children should all be the same size"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_BASELINE_POSITION] =
    g_param_spec_enum ("baseline-position",
                       P_("Baseline position"),
                       P_("The position of the baseline aligned widgets if extra space is available"),
                       GTK_TYPE_BASELINE_POSITION,
                       GTK_BASELINE_POSITION_CENTER,
                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_FILLER);
  gtk_widget_class_set_css_name (widget_class, I_("box"));
}

static void
gtk_box_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  GtkBox *box = GTK_BOX (object);
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);
        if (priv->orientation != orientation)
          {
            priv->orientation = orientation;
            _gtk_orientable_set_style_classes (GTK_ORIENTABLE (box));
            gtk_widget_queue_resize (GTK_WIDGET (box));
            g_object_notify (object, "orientation");
          }
      }
      break;
    case PROP_SPACING:
      gtk_box_set_spacing (box, g_value_get_int (value));
      break;
    case PROP_BASELINE_POSITION:
      gtk_box_set_baseline_position (box, g_value_get_enum (value));
      break;
    case PROP_HOMOGENEOUS:
      gtk_box_set_homogeneous (box, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_box_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  GtkBox *box = GTK_BOX (object);
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_SPACING:
      g_value_set_int (value, priv->spacing);
      break;
    case PROP_BASELINE_POSITION:
      g_value_set_enum (value, priv->baseline_pos);
      break;
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, priv->homogeneous);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
count_expand_children (GtkBox *box,
                       gint *visible_children,
                       gint *expand_children)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GtkWidget *child;

  *visible_children = *expand_children = 0;

  for (child = _gtk_widget_get_first_child (GTK_WIDGET (box));
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (_gtk_widget_get_visible (child))
        {
          *visible_children += 1;

          if (gtk_widget_compute_expand (child, priv->orientation))
            *expand_children += 1;
        }
    }
}

static gint
get_spacing (GtkBox *box)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GtkCssValue *border_spacing;
  gint css_spacing;

  border_spacing = _gtk_style_context_peek_property (gtk_widget_get_style_context (GTK_WIDGET (box)), GTK_CSS_PROPERTY_BORDER_SPACING);
  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    css_spacing = _gtk_css_position_value_get_x (border_spacing, 100);
  else
    css_spacing = _gtk_css_position_value_get_y (border_spacing, 100);

  return css_spacing + priv->spacing;
}

static void
gtk_box_size_allocate (GtkWidget *widget,
                       int        width,
                       int        height,
                       int        baseline)
{
  GtkBox *box = GTK_BOX (widget);
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
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


  count_expand_children (box, &nvis_children, &nexpand_children);

  /* If there is no visible child, simply return. */
  if (nvis_children <= 0)
    return;

  direction = _gtk_widget_get_direction (widget);
  sizes = g_newa (GtkRequestedSize, nvis_children);
  spacing = get_spacing (box);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
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
      if (!_gtk_widget_get_visible (child))
	continue;

      gtk_widget_measure (child,
                          priv->orientation,
                          priv->orientation == GTK_ORIENTATION_HORIZONTAL ? height : width,
                          &sizes[i].minimum_size, &sizes[i].natural_size,
                          NULL, NULL);

      children_minimum_size += sizes[i].minimum_size;

      sizes[i].data = child;

      i++;
    }

  if (priv->homogeneous)
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
      /* If widget is not visible, skip it. */
      if (!_gtk_widget_get_visible (child))
        continue;

      /* Assign the child's size. */
      if (priv->homogeneous)
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

          if (gtk_widget_compute_expand (child, priv->orientation))
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

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
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

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    baseline = -1;

  /* we only calculate our own baseline if we don't get one passed from the parent
   * and any of the child widgets explicitly request one */
  if (baseline == -1 && have_baseline)
    {
      /* TODO: This is purely based on the minimum baseline, when things fit we should
	 use the natural one? */

      switch (priv->baseline_pos)
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
  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
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
      /* If widget is not visible, skip it. */
      if (!_gtk_widget_get_visible (child))
        continue;

      child_size = sizes[i].natural_size;

      /* Assign the child's position. */
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          child_allocation.width = child_size;
          child_allocation.x = x;

          x += child_size + spacing;

          if (direction == GTK_TEXT_DIR_RTL)
            child_allocation.x = width - child_allocation.x - child_allocation.width;

        }
      else /* (priv->orientation == GTK_ORIENTATION_VERTICAL) */
        {
          child_allocation.height = child_size;
          child_allocation.y = y;

          y += child_size + spacing;
        }

      gtk_widget_size_allocate (child, &child_allocation, baseline);

      i++;
    }
}

static GType
gtk_box_child_type (GtkContainer   *container)
{
  return GTK_TYPE_WIDGET;
}

typedef struct _CountingData CountingData;
struct _CountingData {
  GtkWidget *widget;
  gboolean found;
  guint before;
  guint after;
};

static void
count_widget_position (GtkWidget *widget,
                       gpointer   data)
{
  CountingData *count = data;

  if (!_gtk_widget_get_visible (widget))
    return;

  if (count->widget == widget)
    count->found = TRUE;
  else if (count->found)
    count->after++;
  else
    count->before++;
}

static gint
gtk_box_get_visible_position (GtkBox    *box,
                              GtkWidget *child)
{
  CountingData count = { child, FALSE, 0, 0 };
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);

  /* foreach iterates in visible order */
  gtk_container_foreach (GTK_CONTAINER (box),
                         count_widget_position,
                         &count);

  /* the child wasn't found, it's likely an internal child of some
   * subclass, return -1 to indicate that there is no sibling relation
   * to the regular box children
   */
  if (!count.found)
    return -1;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (GTK_WIDGET (box)) == GTK_TEXT_DIR_RTL)
    return count.after;
  else
    return count.before;
}

static GtkWidgetPath *
gtk_box_get_path_for_child (GtkContainer *container,
                            GtkWidget    *child)
{
  GtkWidgetPath *path, *sibling_path;
  GtkBox *box = GTK_BOX (container);
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GList *list, *children;

  path = _gtk_widget_create_path (GTK_WIDGET (container));

  if (_gtk_widget_get_visible (child))
    {
      gint position;

      sibling_path = gtk_widget_path_new ();

      /* get_children works in visible order */
      children = gtk_container_get_children (container);
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
          _gtk_widget_get_direction (GTK_WIDGET (box)) == GTK_TEXT_DIR_RTL)
        children = g_list_reverse (children);

      for (list = children; list; list = list->next)
        {
          if (!_gtk_widget_get_visible (list->data))
            continue;

          gtk_widget_path_append_for_widget (sibling_path, list->data);
        }

      g_list_free (children);

      position = gtk_box_get_visible_position (box, child);

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

static void
gtk_box_compute_size_for_opposing_orientation (GtkBox *box,
                                               int     for_size,
					       gint   *minimum_size,
					       gint   *natural_size,
					       gint   *minimum_baseline,
					       gint   *natural_baseline)
{
  GtkBoxPrivate    *priv = gtk_box_get_instance_private (box);
  GtkWidget        *widget = GTK_WIDGET (box);
  GtkWidget        *child;
  gint              nvis_children;
  gint              nexpand_children;
  gint              computed_minimum = 0, computed_natural = 0;
  gint              computed_minimum_above = 0, computed_natural_above = 0;
  gint              computed_minimum_below = 0, computed_natural_below = 0;
  gint              computed_minimum_baseline = -1, computed_natural_baseline = -1;
  GtkRequestedSize *sizes;
  gint              extra_space, size_given_to_child, i;
  gint              children_minimum_size = 0;
  gint              child_size, child_minimum, child_natural;
  gint              child_minimum_baseline, child_natural_baseline;
  gint              n_extra_widgets = 0;
  gint              spacing;
  gboolean          have_baseline;

  count_expand_children (box, &nvis_children, &nexpand_children);

  if (nvis_children <= 0)
    return;

  spacing = get_spacing (box);
  sizes = g_newa (GtkRequestedSize, nvis_children);
  extra_space = MAX (0, for_size - (nvis_children - 1) * spacing);

  /* Retrieve desired size for visible children */
  for (i = 0, child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (_gtk_widget_get_visible (child))
	{
          gtk_widget_measure (child,
                              priv->orientation,
                              -1,
                              &sizes[i].minimum_size, &sizes[i].natural_size,
                              NULL, NULL);

          children_minimum_size += sizes[i].minimum_size;
	  i += 1;
	}
    }

  if (priv->homogeneous)
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
      /* If widget is not visible, skip it. */
      if (!_gtk_widget_get_visible (child))
        continue;

      /* Assign the child's size. */
      if (priv->homogeneous)
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

          if (gtk_widget_compute_expand (child, priv->orientation))
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
                          OPPOSITE_ORIENTATION (priv->orientation),
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
      switch (priv->baseline_pos)
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

    *minimum_size = computed_minimum;
    *natural_size = MAX (computed_natural, computed_natural_below + computed_natural_above);

    *minimum_baseline = computed_minimum_baseline;
    *natural_baseline = computed_natural_baseline;
}

static void
gtk_box_compute_size_for_orientation (GtkBox *box,
                                      int     for_size,
                                      int    *minimum_size,
                                      int    *natural_size)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GtkWidget *child;
  const int spacing = get_spacing (box);
  int nvis_children = 0;
  int required_size = 0, required_natural = 0;
  int largest_child = 0, largest_natural = 0;

  for (child = _gtk_widget_get_first_child (GTK_WIDGET (box));
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (_gtk_widget_get_visible (child))
        {
          int child_size, child_natural;

          gtk_widget_measure (child,
                              priv->orientation,
                              for_size,
                              &child_size, &child_natural,
                              NULL, NULL);

          largest_child = MAX (largest_child, child_size);
          largest_natural = MAX (largest_natural, child_natural);

	  required_size    += child_size;
	  required_natural += child_natural;

          nvis_children += 1;
        }
    }

  if (nvis_children > 0)
    {
      if (priv->homogeneous)
	{
	  required_size    = largest_child   * nvis_children;
	  required_natural = largest_natural * nvis_children;
	}

      required_size     += (nvis_children - 1) * spacing;
      required_natural  += (nvis_children - 1) * spacing;
    }

  *minimum_size = required_size;
  *natural_size = required_natural;
}


static void
gtk_box_measure (GtkWidget      *widget,
                 GtkOrientation  orientation,
                 int             for_size,
                 int            *minimum,
                 int            *natural,
                 int            *minimum_baseline,
                 int            *natural_baseline)
{
  GtkBox *box = GTK_BOX (widget);
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);

  if (priv->orientation != orientation)
    gtk_box_compute_size_for_opposing_orientation (box, for_size, minimum, natural, minimum_baseline, natural_baseline);
  else
    gtk_box_compute_size_for_orientation (box, for_size, minimum, natural);
}

static void
gtk_box_init (GtkBox *box)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);

  gtk_widget_set_has_surface (GTK_WIDGET (box), FALSE);

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  priv->homogeneous = FALSE;
  priv->spacing = 0;
  priv->baseline_pos = GTK_BASELINE_POSITION_CENTER;

  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (box));
}

/**
 * gtk_box_new:
 * @orientation: the box’s orientation.
 * @spacing: the number of pixels to place by default between children.
 *
 * Creates a new #GtkBox.
 *
 * Returns: a new #GtkBox.
 **/
GtkWidget*
gtk_box_new (GtkOrientation orientation,
             gint           spacing)
{
  return g_object_new (GTK_TYPE_BOX,
                       "orientation", orientation,
                       "spacing",     spacing,
                       NULL);
}

/**
 * gtk_box_set_homogeneous:
 * @box: a #GtkBox
 * @homogeneous: a boolean value, %TRUE to create equal allotments,
 *   %FALSE for variable allotments
 *
 * Sets the #GtkBox:homogeneous property of @box, controlling
 * whether or not all children of @box are given equal space
 * in the box.
 */
void
gtk_box_set_homogeneous (GtkBox  *box,
			 gboolean homogeneous)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);

  g_return_if_fail (GTK_IS_BOX (box));

  homogeneous = homogeneous != FALSE;

  if (priv->homogeneous != homogeneous)
    {
      priv->homogeneous = homogeneous;
      g_object_notify_by_pspec (G_OBJECT (box), props[PROP_HOMOGENEOUS]);
      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}

/**
 * gtk_box_get_homogeneous:
 * @box: a #GtkBox
 *
 * Returns whether the box is homogeneous (all children are the
 * same size). See gtk_box_set_homogeneous().
 *
 * Returns: %TRUE if the box is homogeneous.
 **/
gboolean
gtk_box_get_homogeneous (GtkBox *box)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);

  g_return_val_if_fail (GTK_IS_BOX (box), FALSE);

  return priv->homogeneous;
}

/**
 * gtk_box_set_spacing:
 * @box: a #GtkBox
 * @spacing: the number of pixels to put between children
 *
 * Sets the #GtkBox:spacing property of @box, which is the
 * number of pixels to place between children of @box.
 */
void
gtk_box_set_spacing (GtkBox *box,
		     gint    spacing)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);

  g_return_if_fail (GTK_IS_BOX (box));

  if (priv->spacing != spacing)
    {
      priv->spacing = spacing;

      g_object_notify_by_pspec (G_OBJECT (box), props[PROP_SPACING]);

      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}

/**
 * gtk_box_get_spacing:
 * @box: a #GtkBox
 *
 * Gets the value set by gtk_box_set_spacing().
 *
 * Returns: spacing between children
 **/
gint
gtk_box_get_spacing (GtkBox *box)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  g_return_val_if_fail (GTK_IS_BOX (box), 0);

  return priv->spacing;
}

/**
 * gtk_box_set_baseline_position:
 * @box: a #GtkBox
 * @position: a #GtkBaselinePosition
 *
 * Sets the baseline position of a box. This affects
 * only horizontal boxes with at least one baseline aligned
 * child. If there is more vertical space available than requested,
 * and the baseline is not allocated by the parent then
 * @position is used to allocate the baseline wrt the
 * extra space available.
 */
void
gtk_box_set_baseline_position (GtkBox             *box,
			       GtkBaselinePosition position)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);

  g_return_if_fail (GTK_IS_BOX (box));

  if (priv->baseline_pos != position)
    {
      priv->baseline_pos = position;

      g_object_notify_by_pspec (G_OBJECT (box), props[PROP_BASELINE_POSITION]);

      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}

/**
 * gtk_box_get_baseline_position:
 * @box: a #GtkBox
 *
 * Gets the value set by gtk_box_set_baseline_position().
 *
 * Returns: the baseline position
 **/
GtkBaselinePosition
gtk_box_get_baseline_position (GtkBox *box)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);

  g_return_val_if_fail (GTK_IS_BOX (box), GTK_BASELINE_POSITION_CENTER);

  return priv->baseline_pos;
}

static void
gtk_box_add (GtkContainer *container,
             GtkWidget    *child)
{
  gtk_widget_set_parent (child, GTK_WIDGET (container));
}

static void
gtk_box_remove (GtkContainer *container,
		GtkWidget    *widget)
{
  gtk_widget_unparent (widget);
}

static void
gtk_box_forall (GtkContainer *container,
		GtkCallback   callback,
		gpointer      callback_data)
{
  GtkWidget *child;

  child = _gtk_widget_get_first_child (GTK_WIDGET (container));
  while (child)
    {
      GtkWidget *next = _gtk_widget_get_next_sibling (child);

      (* callback) (child, callback_data);

      child = next;
    }

}

/**
 * gtk_box_insert_child_after:
 * @box: a #GtkBox
 * @child: the #GtkWidget to insert
 * @sibling: (nullable): the sibling to move @child after, or %NULL
 *
 * Inserts @child in the position after @sibling in the list
 * of @box children. If @sibling is %NULL, insert @child at 
 * the first position.
 */
void
gtk_box_insert_child_after (GtkBox    *box,
                            GtkWidget *child,
                            GtkWidget *sibling)
{
  GtkWidget *widget = GTK_WIDGET (box);

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);
  if (sibling)
    {
      g_return_if_fail (GTK_IS_WIDGET (sibling));
      g_return_if_fail (gtk_widget_get_parent (sibling) == widget);
    }

  if (child == sibling)
    return;

  gtk_widget_insert_after (child, widget, sibling);
  gtk_css_node_insert_after (gtk_widget_get_css_node (widget),
                             gtk_widget_get_css_node (child),
                             sibling ? gtk_widget_get_css_node (sibling) : NULL);
}

/**
 * gtk_box_reorder_child_after:
 * @box: a #GtkBox
 * @child: the #GtkWidget to move, must be a child of @box
 * @sibling: (nullable): the sibling to move @child after, or %NULL
 *
 * Moves @child to the position after @sibling in the list
 * of @box children. If @sibling is %NULL, move @child to
 * the first position.
 */
void
gtk_box_reorder_child_after (GtkBox    *box,
                             GtkWidget *child,
                             GtkWidget *sibling)
{
  GtkWidget *widget = GTK_WIDGET (box);

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == widget);
  if (sibling)
    {
      g_return_if_fail (GTK_IS_WIDGET (sibling));
      g_return_if_fail (gtk_widget_get_parent (sibling) == widget);
    }

  if (child == sibling)
    return;

  gtk_widget_insert_after (child, widget, sibling);
  gtk_css_node_insert_after (gtk_widget_get_css_node (widget),
                             gtk_widget_get_css_node (child),
                             sibling ? gtk_widget_get_css_node (sibling) : NULL);
}
