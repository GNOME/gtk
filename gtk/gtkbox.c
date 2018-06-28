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
 * @Short_description: A container box
 * @Title: GtkBox
 * @See_also: #GtkFrame, #GtkGrid, #GtkLayout
 *
 * The GtkBox widget organizes child widgets into a rectangular area.
 *
 * The rectangular area of a GtkBox is organized into either a single row
 * or a single column of child widgets depending upon the orientation.
 * Thus, all children of a GtkBox are allocated one dimension in common,
 * which is the height of a row, or the width of a column.
 *
 * GtkBox uses a notion of packing. Packing refers
 * to adding widgets with reference to a particular position in a
 * #GtkContainer. For a GtkBox, there are two reference positions: the
 * start and the end of the box.
 * For a vertical #GtkBox, the start is defined as the top of the box and
 * the end is defined as the bottom. For a horizontal #GtkBox the start
 * is defined as the left side and the end is defined as the right side.
 *
 * Use repeated calls to gtk_box_pack_start() to pack widgets into a
 * GtkBox from start to end. Use gtk_box_pack_end() to add widgets from
 * end to start. You may intersperse these calls and add widgets from
 * both ends of the same GtkBox.
 *
 * Because GtkBox is a #GtkContainer, you may also use gtk_container_add()
 * to insert widgets into the box. Use gtk_container_remove()
 * to remove widgets from the GtkBox.
 *
 * Use gtk_box_set_homogeneous() to specify whether or not all children
 * of the GtkBox are forced to get the same amount of space.
 *
 * Use gtk_box_set_spacing() to determine how much space will be
 * minimally placed between all children in the GtkBox. Note that
 * spacing is added between the children.
 *
 * Use gtk_box_reorder_child() to move a GtkBox child to a different
 * place in the box.
 *
 * Note that a single-row or single-column #GtkGrid provides exactly
 * the same functionality as #GtkBox.
 *
 * # CSS nodes
 *
 * GtkBox uses a single CSS node with name box.
 */

#include "config.h"

#include "gtkbox.h"
#include "gtkboxprivate.h"
#include "gtkcssnodeprivate.h"
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

enum {
  CHILD_PROP_0,
  CHILD_PROP_PACK_TYPE,
  CHILD_PROP_POSITION,
  LAST_CHILD_PROP
};

typedef struct _GtkBoxChild        GtkBoxChild;

struct _GtkBoxPrivate
{
  GList          *children;

  GtkOrientation  orientation;
  gint16          spacing;

  guint           homogeneous    : 1;
  guint           baseline_pos   : 2;
};
typedef struct _GtkBoxPrivate GtkBoxPrivate;

static GParamSpec *props[LAST_PROP] = { NULL, };
static GParamSpec *child_props[LAST_CHILD_PROP] = { NULL, };

/*
 * GtkBoxChild:
 * @widget: the child widget, packed into the GtkBox.
 *  neighbors, set when packed, zero by default.
 * @pack: one of #GtkPackType indicating whether the child is packed with
 *  reference to the start (top/left) or end (bottom/right) of the GtkBox.
 */
struct _GtkBoxChild
{
  GtkWidget *widget;

  guint      pack   : 1;
};

static void gtk_box_size_allocate         (GtkWidget              *widget,
                                           const GtkAllocation    *allocation,
                                           int                     baseline);

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
static void gtk_box_set_child_property (GtkContainer   *container,
                                        GtkWidget      *child,
                                        guint           property_id,
                                        const GValue   *value,
                                        GParamSpec     *pspec);
static void gtk_box_get_child_property (GtkContainer   *container,
                                        GtkWidget      *child,
                                        guint           property_id,
                                        GValue         *value,
                                        GParamSpec     *pspec);
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
  container_class->set_child_property = gtk_box_set_child_property;
  container_class->get_child_property = gtk_box_get_child_property;
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

  child_props[CHILD_PROP_PACK_TYPE] =
      g_param_spec_enum ("pack-type",
                         P_("Pack type"),
                         P_("A GtkPackType indicating whether the child is packed with reference to the start or end of the parent"),
                         GTK_TYPE_PACK_TYPE, GTK_PACK_START,
                         GTK_PARAM_READWRITE);

  child_props[CHILD_PROP_POSITION] =
      g_param_spec_int ("position",
                        P_("Position"),
                        P_("The index of the child in the parent"),
                        -1, G_MAXINT,
                        0,
                        GTK_PARAM_READWRITE);

  gtk_container_class_install_child_properties (container_class, LAST_CHILD_PROP, child_props);

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
gtk_box_size_allocate (GtkWidget           *widget,
                       const GtkAllocation *allocation,
                       int                  baseline)
{
  GtkBox *box = GTK_BOX (widget);
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GtkBoxChild *child;
  GList *children;
  gint nvis_children;
  gint nexpand_children;

  GtkTextDirection direction;
  GtkAllocation child_allocation;
  GtkRequestedSize *sizes;
  gint child_minimum_baseline, child_natural_baseline;
  gint minimum_above, natural_above;
  gint minimum_below, natural_below;
  gboolean have_baseline;

  GtkPackType packing;

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
    extra_space = allocation->width - (nvis_children - 1) * spacing;
  else
    extra_space = allocation->height - (nvis_children - 1) * spacing;

  have_baseline = FALSE;
  minimum_above = natural_above = 0;
  minimum_below = natural_below = 0;

  /* Retrieve desired size for visible children. */
  for (i = 0, children = priv->children; children; children = children->next)
    {
      child = children->data;

      if (!_gtk_widget_get_visible (child->widget))
	continue;

      gtk_widget_measure (child->widget,
                          priv->orientation,
                          priv->orientation == GTK_ORIENTATION_HORIZONTAL ?
                                                  allocation->height : allocation->width,
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
  for (packing = GTK_PACK_START; packing <= GTK_PACK_END; ++packing)
    {
      for (i = 0, children = priv->children;
	   children;
	   children = children->next)
	{
	  child = children->data;

	  /* If widget is not visible, skip it. */
	  if (!_gtk_widget_get_visible (child->widget))
	    continue;

	  /* If widget is packed differently skip it, but still increment i,
	   * since widget is visible and will be handled in next loop iteration.
	   */
	  if (child->pack != packing)
	    {
	      i++;
	      continue;
	    }

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

              if (gtk_widget_compute_expand (child->widget, priv->orientation))
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
              gtk_widget_get_valign (child->widget) == GTK_ALIGN_BASELINE)
	    {
	      int child_allocation_width;
	      int child_minimum_height, child_natural_height;

              child_allocation_width = child_size;

	      child_minimum_baseline = -1;
	      child_natural_baseline = -1;
              gtk_widget_measure (child->widget, GTK_ORIENTATION_VERTICAL,
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
    }

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    baseline = -1;

  /* we only calculate our own baseline if we don't get one passed from the parent
   * and any of the child widgets explicitly request one */
  if (baseline == -1 && have_baseline)
    {
      gint height = allocation->height;

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
  for (packing = GTK_PACK_START; packing <= GTK_PACK_END; ++packing)
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  child_allocation.y = 0;
	  child_allocation.height = allocation->height;
	  if (packing == GTK_PACK_START)
	    x = 0;
	  else
	    x = allocation->width;
	}
      else
	{
	  child_allocation.x = 0;
	  child_allocation.width = allocation->width;
	  if (packing == GTK_PACK_START)
	    y = 0;
	  else
	    y = allocation->height;
	}

      for (i = 0, children = priv->children;
	   children;
	   children = children->next)
	{
	  child = children->data;

	  /* If widget is not visible, skip it. */
	  if (!_gtk_widget_get_visible (child->widget))
	    continue;

	  /* If widget is packed differently skip it, but still increment i,
	   * since widget is visible and will be handled in next loop iteration.
	   */
	  if (child->pack != packing)
	    {
	      i++;
	      continue;
	    }

	  child_size = sizes[i].natural_size;

	  /* Assign the child's position. */
	  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
              child_allocation.width = child_size;
              child_allocation.x = x;

	      if (packing == GTK_PACK_START)
		{
		  x += child_size + spacing;
		}
	      else
		{
		  x -= child_size + spacing;

		  child_allocation.x -= child_size;
		}

	      if (direction == GTK_TEXT_DIR_RTL)
		child_allocation.x = allocation->width - (child_allocation.x - allocation->x) - child_allocation.width;

	    }
	  else /* (priv->orientation == GTK_ORIENTATION_VERTICAL) */
	    {
              child_allocation.height = child_size;
              child_allocation.y = y;

	      if (packing == GTK_PACK_START)
		{
		  y += child_size + spacing;
		}
	      else
		{
		  y -= child_size + spacing;

		  child_allocation.y -= child_size;
		}
	    }
	  gtk_widget_size_allocate (child->widget, &child_allocation, baseline);

	  i++;
	}
    }
}

static GType
gtk_box_child_type (GtkContainer   *container)
{
  return GTK_TYPE_WIDGET;
}

static void
gtk_box_set_child_property (GtkContainer *container,
                            GtkWidget    *child,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_PACK_TYPE:
      gtk_box_set_child_packing (GTK_BOX (container),
				 child,
				 g_value_get_enum (value));
      break;
    case CHILD_PROP_POSITION:
      gtk_box_reorder_child (GTK_BOX (container),
			     child,
			     g_value_get_int (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_box_get_child_property (GtkContainer *container,
			    GtkWidget    *child,
			    guint         property_id,
			    GValue       *value,
			    GParamSpec   *pspec)
{
  GtkPackType pack_type = 0;
  GtkBoxPrivate *priv = gtk_box_get_instance_private (GTK_BOX (container));
  GList *list;

  switch (property_id)
    {
      guint i;
    case CHILD_PROP_PACK_TYPE:
      gtk_box_query_child_packing (GTK_BOX (container),
                                   child,
                                   &pack_type);
      g_value_set_enum (value, pack_type);
      break;
    case CHILD_PROP_POSITION:
      i = 0;
      for (list = priv->children; list; list = list->next)
	{
	  GtkBoxChild *child_entry;

	  child_entry = list->data;
	  if (child_entry->widget == child)
	    break;
	  i++;
	}
      g_value_set_int (value, list ? i : -1);
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
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
gtk_box_update_child_css_position (GtkBox      *box,
                                   GtkBoxChild *child_info)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GtkBoxChild *prev;
  gboolean reverse;
  GList *l;

  prev = NULL;
  for (l = priv->children; l->data != child_info; l = l->next)
    {
      GtkBoxChild *cur = l->data;

      if (cur->pack == child_info->pack)
        prev = cur;
    }

  reverse = child_info->pack == GTK_PACK_END;

  if (reverse)
    gtk_css_node_insert_before (gtk_widget_get_css_node (GTK_WIDGET (box)),
                                gtk_widget_get_css_node (child_info->widget),
                                prev ? gtk_widget_get_css_node (prev->widget) : NULL);
  else
    gtk_css_node_insert_after (gtk_widget_get_css_node (GTK_WIDGET (box)),
                               gtk_widget_get_css_node (child_info->widget),
                               prev ? gtk_widget_get_css_node (prev->widget) : NULL);
}

static void
gtk_box_pack (GtkBox      *box,
              GtkWidget   *child,
              GtkPackType  pack_type)
{
  GtkContainer *container = GTK_CONTAINER (box);
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GtkBoxChild *child_info;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (_gtk_widget_get_parent (child) == NULL);

  child_info = g_new (GtkBoxChild, 1);
  child_info->widget = child;
  child_info->pack = pack_type;

  priv->children = g_list_append (priv->children, child_info);
  gtk_box_update_child_css_position (box, child_info);

  gtk_widget_freeze_child_notify (child);

  gtk_widget_set_parent (child, GTK_WIDGET (box));

  if (pack_type != GTK_PACK_START)
    gtk_container_child_notify_by_pspec (container, child, child_props[CHILD_PROP_PACK_TYPE]);
  gtk_container_child_notify_by_pspec (container, child, child_props[CHILD_PROP_POSITION]);

  gtk_widget_thaw_child_notify (child);
}

static void
gtk_box_get_size (GtkWidget      *widget,
		  GtkOrientation  orientation,
		  gint           *minimum_size,
		  gint           *natural_size,
		  gint           *minimum_baseline,
		  gint           *natural_baseline)
{
  GtkBox *box = GTK_BOX (widget);
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  gint nvis_children;
  gint minimum, natural;
  gint minimum_above, natural_above;
  gint minimum_below, natural_below;
  gboolean have_baseline;
  gint min_baseline, nat_baseline;
  GtkWidget *child;

  have_baseline = FALSE;
  minimum = natural = 0;
  minimum_above = natural_above = 0;
  minimum_below = natural_below = 0;
  min_baseline = nat_baseline = -1;

  nvis_children = 0;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (_gtk_widget_get_visible (child))
        {
          gint child_minimum, child_natural;
          gint child_minimum_baseline = -1, child_natural_baseline = -1;

          gtk_widget_measure (child,
                              orientation,
                              -1,
                              &child_minimum, &child_natural,
                              &child_minimum_baseline, &child_natural_baseline);

          if (priv->orientation == orientation)
	    {
              if (priv->homogeneous)
                {
                  minimum = MAX (minimum, child_minimum);
                  natural = MAX (natural, child_natural);
                }
              else
                {
                  minimum += child_minimum;
                  natural += child_natural;
                }
	    }
	  else
	    {
	      if (child_minimum_baseline >= 0)
		{
		  have_baseline = TRUE;
		  minimum_below = MAX (minimum_below, child_minimum - child_minimum_baseline);
		  natural_below = MAX (natural_below, child_natural - child_natural_baseline);
		  minimum_above = MAX (minimum_above, child_minimum_baseline);
		  natural_above = MAX (natural_above, child_natural_baseline);
		}
	      else
		{
		  /* The biggest mins and naturals in the opposing orientation */
		  minimum = MAX (minimum, child_minimum);
		  natural = MAX (natural, child_natural);
		}
	    }

          nvis_children += 1;
        }
    }

  if (nvis_children > 0 && priv->orientation == orientation)
    {
      gint spacing = get_spacing (box);

      if (priv->homogeneous)
	{
          minimum *= nvis_children;
          natural *= nvis_children;
	}
      minimum += (nvis_children - 1) * spacing;
      natural += (nvis_children - 1) * spacing;
    }

  minimum = MAX (minimum, minimum_below + minimum_above);
  natural = MAX (natural, natural_below + natural_above);

  if (have_baseline)
    {
      switch (priv->baseline_pos)
	{
	case GTK_BASELINE_POSITION_TOP:
	  min_baseline = minimum_above;
	  nat_baseline = natural_above;
	  break;
	case GTK_BASELINE_POSITION_CENTER:
	  min_baseline = minimum_above + (minimum - (minimum_above + minimum_below)) / 2;
	  nat_baseline = natural_above + (natural - (natural_above + natural_below)) / 2;
	  break;
	case GTK_BASELINE_POSITION_BOTTOM:
	  min_baseline = minimum - minimum_below;
	  nat_baseline = natural - natural_below;
	  break;
        default:
          break;
	}
    }

  *minimum_size = minimum;
  *natural_size = natural;
  *minimum_baseline = min_baseline;
  *natural_baseline = nat_baseline;
}

static void
gtk_box_compute_size_for_opposing_orientation (GtkBox *box,
					       gint    avail_size,
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
  extra_space = avail_size - (nvis_children - 1) * spacing;

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
				      gint    avail_size,
				      gint   *minimum_size,
				      gint   *natural_size)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GtkWidget *child;
  gint           nvis_children = 0;
  gint           required_size = 0, required_natural = 0, child_size, child_natural;
  gint           largest_child = 0, largest_natural = 0;
  gint           spacing = get_spacing (box);

  for (child = gtk_widget_get_first_child (GTK_WIDGET (box));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (_gtk_widget_get_visible (child))
        {

          gtk_widget_measure (child,
                              priv->orientation,
                              avail_size,
                              &child_size, &child_natural,
                              NULL, NULL);

	  if (child_size > largest_child)
	    largest_child = child_size;

	  if (child_natural > largest_natural)
	    largest_natural = child_natural;

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
  GtkBox        *box     = GTK_BOX (widget);
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);

  if (for_size < 0)
    gtk_box_get_size (widget, orientation, minimum, natural, minimum_baseline, natural_baseline);
  else
    {
      if (priv->orientation != orientation)
        gtk_box_compute_size_for_opposing_orientation (box, for_size, minimum, natural, minimum_baseline, natural_baseline);
      else
        gtk_box_compute_size_for_orientation (box, for_size, minimum, natural);
    }
}

static void
gtk_box_init (GtkBox *box)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);

  gtk_widget_set_has_surface (GTK_WIDGET (box), FALSE);

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  priv->children = NULL;

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
 * gtk_box_pack_start:
 * @box: a #GtkBox
 * @child: the #GtkWidget to be added to @box
 *
 * Adds @child to @box, packed with reference to the start of @box.
 * The @child is packed after any other child packed with reference
 * to the start of @box.
 */
void
gtk_box_pack_start (GtkBox    *box,
		    GtkWidget *child)
{
  gtk_box_pack (box, child, GTK_PACK_START);
}

/**
 * gtk_box_pack_end:
 * @box: a #GtkBox
 * @child: the #GtkWidget to be added to @box
 *
 * Adds @child to @box, packed with reference to the end of @box.
 * The @child is packed after (away from end of) any other child
 * packed with reference to the end of @box.
 */
void
gtk_box_pack_end (GtkBox    *box,
		  GtkWidget *child)
{
  gtk_box_pack (box, child, GTK_PACK_END);
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

/**
 * gtk_box_reorder_child:
 * @box: a #GtkBox
 * @child: the #GtkWidget to move
 * @position: the new position for @child in the list of children
 *   of @box, starting from 0. If negative, indicates the end of
 *   the list
 *
 * Moves @child to a new @position in the list of @box children.
 * The list contains widgets packed #GTK_PACK_START
 * as well as widgets packed #GTK_PACK_END, in the order that these
 * widgets were added to @box.
 *
 * A widget’s position in the @box children list determines where
 * the widget is packed into @box.  A child widget at some position
 * in the list will be packed just after all other widgets of the
 * same packing type that appear earlier in the list.
 */
void
gtk_box_reorder_child (GtkBox    *box,
		       GtkWidget *child,
		       gint       position)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GList *old_link;
  GList *new_link;
  GtkBoxChild *child_info = NULL;
  gint old_position;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));

  old_link = priv->children;
  old_position = 0;
  while (old_link)
    {
      child_info = old_link->data;
      if (child_info->widget == child)
	break;

      old_link = old_link->next;
      old_position++;
    }

  g_return_if_fail (old_link != NULL);

  if (position == old_position)
    return;

  priv->children = g_list_delete_link (priv->children, old_link);

  if (position < 0)
    new_link = NULL;
  else
    new_link = g_list_nth (priv->children, position);

  priv->children = g_list_insert_before (priv->children, new_link, child_info);
  gtk_box_update_child_css_position (box, child_info);

  gtk_container_child_notify_by_pspec (GTK_CONTAINER (box), child, child_props[CHILD_PROP_POSITION]);
  if (_gtk_widget_get_visible (child) &&
      _gtk_widget_get_visible (GTK_WIDGET (box)))
    {
      gtk_widget_queue_resize (child);
    }
}

/**
 * gtk_box_query_child_packing:
 * @box: a #GtkBox
 * @child: the #GtkWidget of the child to query
 * @pack_type: (out) (optional): pointer to return location for pack-type
 *     child property
 *
 * Obtains information about how @child is packed into @box.
 */
void
gtk_box_query_child_packing (GtkBox      *box,
			     GtkWidget   *child,
			     GtkPackType *pack_type)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GList *list;
  GtkBoxChild *child_info = NULL;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = priv->children;
  while (list)
    {
      child_info = list->data;
      if (child_info->widget == child)
	break;

      list = list->next;
    }

  if (list)
    {
      if (pack_type)
	*pack_type = child_info->pack;
    }
}

/**
 * gtk_box_set_child_packing:
 * @box: a #GtkBox
 * @child: the #GtkWidget of the child to set
 * @pack_type: the new value of the pack-type child property
 *
 * Sets the way @child is packed into @box.
 */
void
gtk_box_set_child_packing (GtkBox      *box,
			   GtkWidget   *child,
			   GtkPackType  pack_type)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GList *list;
  GtkBoxChild *child_info = NULL;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = priv->children;
  while (list)
    {
      child_info = list->data;
      if (child_info->widget == child)
	break;

      list = list->next;
    }

  gtk_widget_freeze_child_notify (child);
  if (list)
    {
      if (pack_type != GTK_PACK_END)
        pack_type = GTK_PACK_START;
      if (child_info->pack != pack_type)
        {
	  child_info->pack = pack_type;
          gtk_box_update_child_css_position (box, child_info);
          gtk_container_child_notify_by_pspec (GTK_CONTAINER (box), child, child_props[CHILD_PROP_PACK_TYPE]);
        }

      if (_gtk_widget_get_visible (child) &&
          _gtk_widget_get_visible (GTK_WIDGET (box)))
	gtk_widget_queue_resize (child);
    }
  gtk_widget_thaw_child_notify (child);
}

static void
gtk_box_add (GtkContainer *container,
	     GtkWidget    *widget)
{
  gtk_box_pack_start (GTK_BOX (container), widget);
}

static void
gtk_box_remove (GtkContainer *container,
		GtkWidget    *widget)
{
  GtkBox *box = GTK_BOX (container);
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GtkBoxChild *child;
  GList *children;

  children = priv->children;
  while (children)
    {
      child = children->data;

      if (child->widget == widget)
	{
	  gboolean was_visible;

	  was_visible = _gtk_widget_get_visible (widget);
	  gtk_widget_unparent (widget);

	  priv->children = g_list_remove_link (priv->children, children);
	  g_list_free (children);
	  g_free (child);

	  /* queue resize regardless of gtk_widget_get_visible (container),
	   * since that's what is needed by toplevels.
	   */
	  if (was_visible)
            {
	      gtk_widget_queue_resize (GTK_WIDGET (container));
            }

	  break;
	}

      children = children->next;
    }
}

static void
gtk_box_forall (GtkContainer *container,
		GtkCallback   callback,
		gpointer      callback_data)
{
  GtkBox *box = GTK_BOX (container);
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GtkBoxChild *child;
  GList *children;

  children = priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (child->pack == GTK_PACK_START)
	(* callback) (child->widget, callback_data);
    }


  children = g_list_last (priv->children);
  while (children)
    {
      child = children->data;
      children = children->prev;

      if (child->pack == GTK_PACK_END)
	(* callback) (child->widget, callback_data);
    }
}

GList *
_gtk_box_get_children (GtkBox *box)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GtkBoxChild *child;
  GList *children;
  GList *retval = NULL;

  g_return_val_if_fail (GTK_IS_BOX (box), NULL);

  children = priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      retval = g_list_prepend (retval, child->widget);
    }

  return g_list_reverse (retval);
}
