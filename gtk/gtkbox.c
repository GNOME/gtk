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
 *
 * In horizontal orientation, the nodes of the children are always arranged
 * from left to right. So :first-child will always select the leftmost child,
 * regardless of text direction.
 */

#include "config.h"

#include "gtkbox.h"
#include "gtkboxprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkcsscustomgadgetprivate.h"
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
  GtkCssGadget   *gadget;

  GtkOrientation  orientation;
  gint16          spacing;

  guint           homogeneous    : 1;
  guint           baseline_pos   : 2;
};

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
                                           GtkAllocation          *allocation);

static void gtk_box_direction_changed  (GtkWidget        *widget,
                                        GtkTextDirection  previous_direction);

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
static void gtk_box_snapshot (GtkWidget   *widget,
                              GtkSnapshot *snapshot);

G_DEFINE_TYPE_WITH_CODE (GtkBox, gtk_box, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkBox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

static void
gtk_box_dispose (GObject *object)
{
  GtkBox *box = GTK_BOX (object);
  GtkBoxPrivate *priv = box->priv;

  g_clear_object (&priv->gadget);

  G_OBJECT_CLASS (gtk_box_parent_class)->dispose (object);
}

static void
gtk_box_class_init (GtkBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->set_property = gtk_box_set_property;
  object_class->get_property = gtk_box_get_property;
  object_class->dispose = gtk_box_dispose;

  widget_class->snapshot = gtk_box_snapshot;
  widget_class->size_allocate = gtk_box_size_allocate;
  widget_class->measure = gtk_box_measure;
  widget_class->direction_changed = gtk_box_direction_changed;

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
  gtk_widget_class_set_css_name (widget_class, "box");
}

static void
gtk_box_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  GtkBox *box = GTK_BOX (object);
  GtkBoxPrivate *private = box->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);
        if (private->orientation != orientation)
          {
            private->orientation = orientation;
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
  GtkBoxPrivate *private = box->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, private->orientation);
      break;
    case PROP_SPACING:
      g_value_set_int (value, private->spacing);
      break;
    case PROP_BASELINE_POSITION:
      g_value_set_enum (value, private->baseline_pos);
      break;
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, private->homogeneous);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_box_snapshot_contents (GtkCssGadget *gadget,
                           GtkSnapshot  *snapshot,
                           int           x,
                           int           y,
                           int           width,
                           int           height,
                           gpointer      data)
{
  GTK_WIDGET_CLASS (gtk_box_parent_class)->snapshot (gtk_css_gadget_get_owner (gadget), snapshot);

  return FALSE;
}

static void
gtk_box_snapshot (GtkWidget   *widget,
                  GtkSnapshot *snapshot)
{
  gtk_css_gadget_snapshot (GTK_BOX (widget)->priv->gadget, snapshot);
}

static void
count_expand_children (GtkBox *box,
                       gint *visible_children,
                       gint *expand_children)
{
  GtkBoxPrivate  *private = box->priv;
  GList       *children;
  GtkBoxChild *child;

  *visible_children = *expand_children = 0;

  for (children = private->children; children; children = children->next)
    {
      child = children->data;

      if (_gtk_widget_get_visible (child->widget))
	{
	  *visible_children += 1;
          if (gtk_widget_compute_expand (child->widget, private->orientation))
	    *expand_children += 1;
	}
    }
}

static gint
get_spacing (GtkBox *box)
{
  GtkBoxPrivate *priv = box->priv;
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
gtk_box_size_allocate_no_center (GtkWidget           *widget,
                                 const GtkAllocation *allocation,
                                 GdkRectangle        *out_clip)
{
  GtkBox *box = GTK_BOX (widget);
  GtkBoxPrivate *private = box->priv;
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
  gint baseline;

  GtkPackType packing;

  gint extra_space;
  gint children_minimum_size = 0;
  gint size_given_to_child;
  gint n_extra_widgets = 0; /* Number of widgets that receive 1 extra px */
  gint x = 0, y = 0, i;
  gint child_size;
  gint spacing;
  GdkRectangle clip;


  count_expand_children (box, &nvis_children, &nexpand_children);

  /* If there is no visible child, simply return. */
  if (nvis_children <= 0)
    return;

  direction = _gtk_widget_get_direction (widget);
  sizes = g_newa (GtkRequestedSize, nvis_children);
  spacing = get_spacing (box);

  if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
    extra_space = allocation->width - (nvis_children - 1) * spacing;
  else
    extra_space = allocation->height - (nvis_children - 1) * spacing;

  have_baseline = FALSE;
  minimum_above = natural_above = 0;
  minimum_below = natural_below = 0;

  /* Retrieve desired size for visible children. */
  for (i = 0, children = private->children; children; children = children->next)
    {
      child = children->data;

      if (!_gtk_widget_get_visible (child->widget))
	continue;

      gtk_widget_measure (child->widget,
                          private->orientation,
                          private->orientation == GTK_ORIENTATION_HORIZONTAL ?
                                                  allocation->height : allocation->width,
                          &sizes[i].minimum_size, &sizes[i].natural_size,
                          NULL, NULL);

      /* Assert the api is working properly */
      if (sizes[i].minimum_size < 0)
	g_error ("GtkBox child %s minimum %s: %d < 0 for %s %d",
		 gtk_widget_get_name (GTK_WIDGET (child->widget)),
		 (private->orientation == GTK_ORIENTATION_HORIZONTAL) ? "width" : "height",
		 sizes[i].minimum_size,
		 (private->orientation == GTK_ORIENTATION_HORIZONTAL) ? "height" : "width",
		 (private->orientation == GTK_ORIENTATION_HORIZONTAL) ? allocation->height : allocation->width);

      if (sizes[i].natural_size < sizes[i].minimum_size)
	g_error ("GtkBox child %s natural %s: %d < minimum %d for %s %d",
		 gtk_widget_get_name (GTK_WIDGET (child->widget)),
		 (private->orientation == GTK_ORIENTATION_HORIZONTAL) ? "width" : "height",
		 sizes[i].natural_size,
		 sizes[i].minimum_size,
		 (private->orientation == GTK_ORIENTATION_HORIZONTAL) ? "height" : "width",
		 (private->orientation == GTK_ORIENTATION_HORIZONTAL) ? allocation->height : allocation->width);

      children_minimum_size += sizes[i].minimum_size;

      sizes[i].data = child;

      i++;
    }

  if (private->homogeneous)
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
      for (i = 0, children = private->children;
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
	  if (private->homogeneous)
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

              if (gtk_widget_compute_expand (child->widget, private->orientation))
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

	  if (private->orientation == GTK_ORIENTATION_HORIZONTAL &&
	      gtk_widget_get_valign (child->widget) == GTK_ALIGN_BASELINE)
	    {
	      int child_allocation_width;
	      int child_minimum_height, child_natural_height;

              child_allocation_width = MAX (1, child_size);

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

  baseline = gtk_widget_get_allocated_baseline (widget);
  if (baseline == -1 && have_baseline)
    {
      gint height = MAX (1, allocation->height);

      /* TODO: This is purely based on the minimum baseline, when things fit we should
	 use the natural one? */

      switch (private->baseline_pos)
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
	}
    }

  /* Allocate child positions. */
  for (packing = GTK_PACK_START; packing <= GTK_PACK_END; ++packing)
    {
      if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  child_allocation.y = allocation->y;
	  child_allocation.height = MAX (1, allocation->height);
	  if (packing == GTK_PACK_START)
	    x = allocation->x;
	  else
	    x = allocation->x + allocation->width;
	}
      else
	{
	  child_allocation.x = allocation->x;
	  child_allocation.width = MAX (1, allocation->width);
	  if (packing == GTK_PACK_START)
	    y = allocation->y;
	  else
	    y = allocation->y + allocation->height;
	}

      for (i = 0, children = private->children;
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
	  if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
              child_allocation.width = MAX (1, child_size);
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
		child_allocation.x = allocation->x + allocation->width - (child_allocation.x - allocation->x) - child_allocation.width;

	    }
	  else /* (private->orientation == GTK_ORIENTATION_VERTICAL) */
	    {
              child_allocation.height = MAX (1, child_size);
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
	  gtk_widget_size_allocate_with_baseline (child->widget, &child_allocation, baseline);
          gtk_widget_get_clip (child->widget, &clip);
          gdk_rectangle_union (&clip, out_clip, out_clip);

	  i++;
	}
    }
}

static void
gtk_box_allocate_contents (GtkCssGadget        *gadget,
                           const GtkAllocation *allocation,
                           int                  baseline,
                           GtkAllocation       *out_clip,
                           gpointer             unused)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);

  *out_clip = *allocation;
  gtk_box_size_allocate_no_center (widget, allocation, out_clip);
}

static void
gtk_box_size_allocate (GtkWidget     *widget,
                       GtkAllocation *allocation)
{
  GtkBoxPrivate *priv = GTK_BOX (widget)->priv;
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);

  gtk_css_gadget_allocate (priv->gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
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
      for (list = GTK_BOX (container)->priv->children; list; list = list->next)
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

  if (box->priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
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
  GtkBox *box;
  GtkBoxPrivate *private;
  GList *list, *children;

  box = GTK_BOX (container);
  private = box->priv;

  path = _gtk_widget_create_path (GTK_WIDGET (container));

  if (_gtk_widget_get_visible (child))
    {
      gint position;

      sibling_path = gtk_widget_path_new ();

      /* get_children works in visible order */
      children = gtk_container_get_children (container);
      if (private->orientation == GTK_ORIENTATION_HORIZONTAL &&
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
  GtkBoxPrivate *priv = box->priv;
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
  if (box->priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
      _gtk_widget_get_direction (GTK_WIDGET (box)) == GTK_TEXT_DIR_RTL)
    reverse = !reverse;

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
gtk_box_direction_changed (GtkWidget        *widget,
                           GtkTextDirection  previous_direction)
{
  GtkBox *box = GTK_BOX (widget);

  if (box->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_css_node_reverse_children (gtk_widget_get_css_node (widget));
}

static GtkBoxChild *
gtk_box_pack (GtkBox      *box,
              GtkWidget   *child,
              GtkPackType  pack_type)
{
  GtkContainer *container = GTK_CONTAINER (box);
  GtkBoxPrivate *private = box->priv;
  GtkBoxChild *child_info;

  g_return_val_if_fail (GTK_IS_BOX (box), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);
  g_return_val_if_fail (_gtk_widget_get_parent (child) == NULL, NULL);

  child_info = g_new (GtkBoxChild, 1);
  child_info->widget = child;
  child_info->pack = pack_type;

  private->children = g_list_append (private->children, child_info);
  gtk_box_update_child_css_position (box, child_info);

  gtk_widget_freeze_child_notify (child);

  gtk_widget_set_parent (child, GTK_WIDGET (box));

  if (pack_type != GTK_PACK_START)
    gtk_container_child_notify_by_pspec (container, child, child_props[CHILD_PROP_PACK_TYPE]);
  gtk_container_child_notify_by_pspec (container, child, child_props[CHILD_PROP_POSITION]);

  gtk_widget_thaw_child_notify (child);

  return child_info;
}

static void
gtk_box_get_size (GtkWidget      *widget,
		  GtkOrientation  orientation,
		  gint           *minimum_size,
		  gint           *natural_size,
		  gint           *minimum_baseline,
		  gint           *natural_baseline)
{
  GtkBox *box;
  GtkBoxPrivate *private;
  GList *children;
  gint nvis_children;
  gint minimum, natural;
  gint minimum_above, natural_above;
  gint minimum_below, natural_below;
  gboolean have_baseline;
  gint min_baseline, nat_baseline;

  box = GTK_BOX (widget);
  private = box->priv;

  have_baseline = FALSE;
  minimum = natural = 0;
  minimum_above = natural_above = 0;
  minimum_below = natural_below = 0;
  min_baseline = nat_baseline = -1;

  nvis_children = 0;

  for (children = private->children; children; children = children->next)
    {
      GtkBoxChild *child = children->data;

      if (_gtk_widget_get_visible (child->widget))
        {
          gint child_minimum, child_natural;
          gint child_minimum_baseline = -1, child_natural_baseline = -1;

          gtk_widget_measure (child->widget,
                              orientation,
                              -1,
                              &child_minimum, &child_natural,
                              &child_minimum_baseline, &child_natural_baseline);

          if (private->orientation == orientation)
	    {
              if (private->homogeneous)
                {
                  int largest;

                  largest = child_minimum;
                  minimum = MAX (minimum, largest);

                  largest = child_natural;
                  natural = MAX (natural, largest);
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

  if (nvis_children > 0 && private->orientation == orientation)
    {
      gint spacing = get_spacing (box);

      if (private->homogeneous)
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
      switch (private->baseline_pos)
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
	}
    }

  if (minimum_size)
    *minimum_size = minimum;

  if (natural_size)
    *natural_size = natural;

  if (minimum_baseline)
    *minimum_baseline = min_baseline;

  if (natural_baseline)
    *natural_baseline = nat_baseline;
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
  gtk_css_gadget_get_preferred_size (GTK_BOX (widget)->priv->gadget,
                                     orientation,
                                     for_size,
                                     minimum, natural,
                                     minimum_baseline, natural_baseline);
}

static void
gtk_box_compute_size_for_opposing_orientation (GtkBox *box,
					       gint    avail_size,
					       gint   *minimum_size,
					       gint   *natural_size,
					       gint   *minimum_baseline,
					       gint   *natural_baseline)
{
  GtkBoxPrivate    *private = box->priv;
  GtkBoxChild      *child;
  GList            *children;
  gint              nvis_children;
  gint              nexpand_children;
  gint              computed_minimum = 0, computed_natural = 0;
  gint              computed_minimum_above = 0, computed_natural_above = 0;
  gint              computed_minimum_below = 0, computed_natural_below = 0;
  gint              computed_minimum_baseline = -1, computed_natural_baseline = -1;
  GtkRequestedSize *sizes;
  GtkPackType       packing;
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
  for (i = 0, children = private->children; children; children = children->next)
    {
      child = children->data;

      if (_gtk_widget_get_visible (child->widget))
	{
          gtk_widget_measure (child->widget,
                              private->orientation,
                              -1,
                              &sizes[i].minimum_size, &sizes[i].natural_size,
                              NULL, NULL);

	  /* Assert the api is working properly */
	  if (sizes[i].minimum_size < 0)
	    g_error ("GtkBox child %s minimum %s: %d < 0",
		     gtk_widget_get_name (GTK_WIDGET (child->widget)),
		     (private->orientation == GTK_ORIENTATION_HORIZONTAL) ? "width" : "height",
		     sizes[i].minimum_size);

	  if (sizes[i].natural_size < sizes[i].minimum_size)
	    g_error ("GtkBox child %s natural %s: %d < minimum %d",
		     gtk_widget_get_name (GTK_WIDGET (child->widget)),
		     (private->orientation == GTK_ORIENTATION_HORIZONTAL) ? "width" : "height",
		     sizes[i].natural_size,
		     sizes[i].minimum_size);

          children_minimum_size += sizes[i].minimum_size;

	  sizes[i].data = child;

	  i += 1;
	}
    }

  if (private->homogeneous)
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
  /* Allocate child positions. */
  for (packing = GTK_PACK_START; packing <= GTK_PACK_END; ++packing)
    {
      for (i = 0, children = private->children;
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

	  if (child->pack == packing)
	    {
	      /* Assign the child's size. */
	      if (private->homogeneous)
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

                  if (gtk_widget_compute_expand (child->widget, private->orientation))
		    {
                      child_size += size_given_to_child;

		      if (n_extra_widgets > 0)
			{
			  child_size++;
			  n_extra_widgets--;
			}
		    }
		}

              child_size = MAX (1, child_size);


	      child_minimum_baseline = child_natural_baseline = -1;
	      /* Assign the child's position. */
              gtk_widget_measure (child->widget,
                                  OPPOSITE_ORIENTATION (private->orientation),
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
	    }
	  i += 1;
	}
    }

  if (have_baseline)
    {
      computed_minimum = MAX (computed_minimum, computed_minimum_below + computed_minimum_above);
      computed_natural = MAX (computed_natural, computed_natural_below + computed_natural_above);
      switch (private->baseline_pos)
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
	}
    }

  if (minimum_baseline)
    *minimum_baseline = computed_minimum_baseline;
  if (natural_baseline)
    *natural_baseline = computed_natural_baseline;

  if (minimum_size)
    *minimum_size = computed_minimum;
  if (natural_size)
    *natural_size = MAX (computed_natural, computed_natural_below + computed_natural_above);
}

static void
gtk_box_compute_size_for_orientation (GtkBox *box,
				      gint    avail_size,
				      gint   *minimum_size,
				      gint   *natural_size)
{
  GtkBoxPrivate    *private = box->priv;
  GList         *children;
  gint           nvis_children = 0;
  gint           required_size = 0, required_natural = 0, child_size, child_natural;
  gint           largest_child = 0, largest_natural = 0;
  gint           spacing = get_spacing (box);

  for (children = private->children; children != NULL;
       children = children->next)
    {
      GtkBoxChild *child = children->data;

      if (_gtk_widget_get_visible (child->widget))
        {

          gtk_widget_measure (child->widget,
                              private->orientation,
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
      if (private->homogeneous)
	{
	  required_size    = largest_child   * nvis_children;
	  required_natural = largest_natural * nvis_children;
	}

      required_size     += (nvis_children - 1) * spacing;
      required_natural  += (nvis_children - 1) * spacing;
    }

  if (minimum_size)
    *minimum_size = required_size;

  if (natural_size)
    *natural_size = required_natural;
}

static void
gtk_box_get_content_size (GtkCssGadget   *gadget,
                          GtkOrientation  orientation,
                          gint            for_size,
                          gint           *minimum,
                          gint           *natural,
                          gint           *minimum_baseline,
                          gint           *natural_baseline,
                          gpointer        unused)
{
  GtkWidget     *widget  = gtk_css_gadget_get_owner (gadget);
  GtkBox        *box     = GTK_BOX (widget);
  GtkBoxPrivate *private = box->priv;

  if (for_size < 0)
    gtk_box_get_size (widget, orientation, minimum, natural, minimum_baseline, natural_baseline);
  else
    {
      if (private->orientation != orientation)
	gtk_box_compute_size_for_opposing_orientation (box, for_size, minimum, natural, minimum_baseline, natural_baseline);
      else
	{
	  if (minimum_baseline)
	    *minimum_baseline = -1;
	  if (natural_baseline)
	    *natural_baseline = -1;
	  gtk_box_compute_size_for_orientation (box, for_size, minimum, natural);
	}
    }
}

static void
gtk_box_init (GtkBox *box)
{
  GtkBoxPrivate *private;

  box->priv = gtk_box_get_instance_private (box);
  private = box->priv;

  gtk_widget_set_has_window (GTK_WIDGET (box), FALSE);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (box), FALSE);

  private->orientation = GTK_ORIENTATION_HORIZONTAL;
  private->children = NULL;

  private->homogeneous = FALSE;
  private->spacing = 0;
  private->baseline_pos = GTK_BASELINE_POSITION_CENTER;

  private->gadget = gtk_css_custom_gadget_new_for_node (gtk_widget_get_css_node (GTK_WIDGET (box)),
                                                        GTK_WIDGET (box),
                                                        gtk_box_get_content_size,
                                                        gtk_box_allocate_contents,
                                                        gtk_box_snapshot_contents,
                                                        NULL,
                                                        NULL);

  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (box));
}

GtkCssGadget *
gtk_box_get_gadget (GtkBox *box)
{
  return box->priv->gadget;
}

/**
 * gtk_box_new:
 * @orientation: the box’s orientation.
 * @spacing: the number of pixels to place by default between children.
 *
 * Creates a new #GtkBox.
 *
 * Returns: a new #GtkBox.
 *
 * Since: 3.0
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
  GtkBoxPrivate *private;

  g_return_if_fail (GTK_IS_BOX (box));

  private = box->priv;

  homogeneous = homogeneous != FALSE;

  if (private->homogeneous != homogeneous)
    {
      private->homogeneous = homogeneous;
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
  g_return_val_if_fail (GTK_IS_BOX (box), FALSE);

  return box->priv->homogeneous;
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
  GtkBoxPrivate *private;

  g_return_if_fail (GTK_IS_BOX (box));

  private = box->priv;

  if (private->spacing != spacing)
    {
      private->spacing = spacing;

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
  g_return_val_if_fail (GTK_IS_BOX (box), 0);

  return box->priv->spacing;
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
 *
 * Since: 3.10
 */
void
gtk_box_set_baseline_position (GtkBox             *box,
			       GtkBaselinePosition position)
{
  GtkBoxPrivate *private;

  g_return_if_fail (GTK_IS_BOX (box));

  private = box->priv;

  if (private->baseline_pos != position)
    {
      private->baseline_pos = position;

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
 *
 * Since: 3.10
 **/
GtkBaselinePosition
gtk_box_get_baseline_position (GtkBox *box)
{
  g_return_val_if_fail (GTK_IS_BOX (box), GTK_BASELINE_POSITION_CENTER);

  return box->priv->baseline_pos;
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
  GtkBoxPrivate *priv;
  GList *old_link;
  GList *new_link;
  GtkBoxChild *child_info = NULL;
  gint old_position;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = box->priv;

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
  GtkBoxPrivate *private;
  GList *list;
  GtkBoxChild *child_info = NULL;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));

  private = box->priv;

  list = private->children;
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
  GtkBoxPrivate *private;
  GList *list;
  GtkBoxChild *child_info = NULL;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));

  private = box->priv;

  list = private->children;
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
  GtkBoxPrivate *priv = box->priv;
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
  GtkBoxPrivate *priv = box->priv;
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
  GtkBoxPrivate *priv;
  GtkBoxChild *child;
  GList *children;
  GList *retval = NULL;

  g_return_val_if_fail (GTK_IS_BOX (box), NULL);

  priv = box->priv;

  children = priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      retval = g_list_prepend (retval, child->widget);
    }

  return g_list_reverse (retval);
}
