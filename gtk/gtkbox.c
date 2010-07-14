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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/**
 * SECTION:gtkbox
 * @Short_description: Base class for box containers
 * @Title: GtkBox
 * @See_also:i #GtkHBox, #GtkVBox, #GtkFrame, #GtkTable, #GtkLayout
 *
 * GtkBox is an widget which encapsulates functionality for a
 * particular kind of container, one that organizes a variable number of
 * widgets into a rectangular area.  GtkBox has a number of derived
 * classes, e.g. #GtkHBox and #GtkVBox.
 *
 * The rectangular area of a GtkBox is organized into either a single row
 * or a single column of child widgets depending upon whether the box is
 * of type #GtkHBox or #GtkVBox, respectively.  Thus, all children of a
 * GtkBox are allocated one dimension in common, which is the height of a
 * row, or the width of a column.
 *
 * GtkBox uses a notion of <emphasis>packing</emphasis>.  Packing
 * refers to adding widgets with reference to a particular position in a
 * #GtkContainer.  For a GtkBox, there are two reference positions: the
 * <emphasis>start</emphasis> and the <emphasis>end</emphasis> of the box.
 * For a #GtkVBox, the start is defined as the top of the box and the end is
 * defined as the bottom.  For a #GtkHBox the start is defined as the
 * left side and the end is defined as the right side.
 *
 * Use repeated calls to gtk_box_pack_start() to pack widgets into a
 * GtkBox from start to end.  Use gtk_box_pack_end() to add widgets from
 * end to start.  You may intersperse these calls and add widgets from
 * both ends of the same GtkBox.
 *
 * Because GtkBox is a #GtkContainer, you may also use
 * gtk_container_add() to insert widgets into the box, and they will be
 * packed with the default values for #GtkBox:expand and #GtkBox:fill.
 * Use gtk_container_remove() to remove widgets from the GtkBox.
 *
 * Use gtk_box_set_homogeneous() to specify whether or not all children
 * of the GtkBox are forced to get the same amount of space.
 *
 * Use gtk_box_set_spacing() to determine how much space will be
 * minimally placed between all children in the GtkBox.
 *
 * Use gtk_box_reorder_child() to move a GtkBox child to a different
 * place in the box.
 *
 * Use gtk_box_set_child_packing() to reset the #GtkBox:expand,
 * #GtkBox:fill and #GtkBox:padding child properties.
 * Use gtk_box_query_child_packing() to query these fields.
 */

#include "config.h"

#include "gtkbox.h"
#include "gtkorientable.h"
#include "gtksizerequest.h"
#include "gtkprivate.h"
#include "gtkintl.h"


enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_SPACING,
  PROP_HOMOGENEOUS
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_EXPAND,
  CHILD_PROP_FILL,
  CHILD_PROP_PADDING,
  CHILD_PROP_PACK_TYPE,
  CHILD_PROP_POSITION
};


struct _GtkBoxPriv
{
  GtkOrientation  orientation;

  GList          *children;

  gint16          spacing;

  guint           default_expand : 1;
  guint           homogeneous    : 1;
  guint           spacing_set    : 1;
};


typedef struct _GtkBoxDesiredSizes GtkBoxDesiredSizes;
typedef struct _GtkBoxSpreading    GtkBoxSpreading;
typedef struct _GtkBoxChild        GtkBoxChild;

/*
 * GtkBoxChild:
 * @widget: the child widget, packed into the GtkBox.
 * @padding: the number of extra pixels to put between this child and its
 *  neighbors, set when packed, zero by default.
 * @expand: flag indicates whether extra space should be given to this child.
 *  Any extra space given to the parent GtkBox is divided up among all children
 *  with this attribute set to %TRUE; set when packed, %TRUE by default.
 * @fill: flag indicates whether any extra space given to this child due to its
 *  @expand attribute being set is actually allocated to the child, rather than
 *  being used as padding around the widget; set when packed, %TRUE by default.
 * @pack: one of #GtkPackType indicating whether the child is packed with
 *  reference to the start (top/left) or end (bottom/right) of the GtkBox.
 */
struct _GtkBoxChild
{
  GtkWidget *widget;

  guint16    padding;

  guint      expand : 1;
  guint      fill   : 1;
  guint      pack   : 1;
};

struct _GtkBoxDesiredSizes
{
  gint minimum_size;
  gint natural_size;
};

struct _GtkBoxSpreading
{
  GtkBoxChild *child;
  gint index;
};

static void gtk_box_size_allocate         (GtkWidget              *widget,
                                           GtkAllocation          *allocation);

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
                                        gboolean        include_internals,
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


static void               gtk_box_size_request_init    (GtkSizeRequestIface *iface);
static GtkSizeRequestMode gtk_box_get_request_mode     (GtkSizeRequest      *widget);
static void               gtk_box_get_width            (GtkSizeRequest      *widget,
							gint                *minimum_size,
							gint                *natural_size);
static void               gtk_box_get_height           (GtkSizeRequest      *widget,
							gint                *minimum_size,
							gint                *natural_size);
static void               gtk_box_get_width_for_height (GtkSizeRequest      *widget,
							gint                 height,
							gint                *minimum_width,
							gint                *natural_width);
static void               gtk_box_get_height_for_width (GtkSizeRequest      *widget,
							gint                 width,
							gint                *minimum_height,
							gint                *natural_height);

static GtkSizeRequestIface *parent_size_request_iface;

G_DEFINE_TYPE_WITH_CODE (GtkBox, gtk_box, GTK_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE,
                                                NULL)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SIZE_REQUEST,
                                                gtk_box_size_request_init));

static void
gtk_box_class_init (GtkBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->set_property = gtk_box_set_property;
  object_class->get_property = gtk_box_get_property;

  widget_class->size_allocate = gtk_box_size_allocate;

  container_class->add = gtk_box_add;
  container_class->remove = gtk_box_remove;
  container_class->forall = gtk_box_forall;
  container_class->child_type = gtk_box_child_type;
  container_class->set_child_property = gtk_box_set_child_property;
  container_class->get_child_property = gtk_box_get_child_property;

  g_object_class_override_property (object_class,
                                    PROP_ORIENTATION,
                                    "orientation");

  g_object_class_install_property (object_class,
                                   PROP_SPACING,
                                   g_param_spec_int ("spacing",
                                                     P_("Spacing"),
                                                     P_("The amount of space between children"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_HOMOGENEOUS,
                                   g_param_spec_boolean ("homogeneous",
							 P_("Homogeneous"),
							 P_("Whether the children should all be the same size"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  /**
   * GtkBox:expand:
   *
   * Whether the child should receive extra space when the parent grows.
   *
   * Note that the default value for this property is %FALSE for GtkBox,
   * but #GtkHBox, #GtkVBox and other subclasses use the old default
   * of %TRUE.
   */
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_EXPAND,
					      g_param_spec_boolean ("expand",
								    P_("Expand"),
								    P_("Whether the child should receive extra space when the parent grows"),
								    FALSE,
								    GTK_PARAM_READWRITE));

  /**
   * GtkBox:fill:
   *
   * Whether the child should receive extra space when the parent grows.
   *
   * Note that the default value for this property is %FALSE for GtkBox,
   * but #GtkHBox, #GtkVBox and other subclasses use the old default
   * of %TRUE.
   */
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_FILL,
					      g_param_spec_boolean ("fill",
								    P_("Fill"),
								    P_("Whether extra space given to the child should be allocated to the child or used as padding"),
								    FALSE,
								    GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PADDING,
					      g_param_spec_uint ("padding",
								 P_("Padding"),
								 P_("Extra space to put between the child and its neighbors, in pixels"),
								 0, G_MAXINT, 0,
								 GTK_PARAM_READWRITE));
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
								GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkBoxPriv));
}

static void
gtk_box_init (GtkBox *box)
{
  GtkBoxPriv *private;

  box->priv = G_TYPE_INSTANCE_GET_PRIVATE (box,
                                           GTK_TYPE_BOX,
                                           GtkBoxPriv);
  private = box->priv;

  gtk_widget_set_has_window (GTK_WIDGET (box), FALSE);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (box), FALSE);

  private->orientation = GTK_ORIENTATION_HORIZONTAL;
  private->children = NULL;

  private->default_expand = FALSE;
  private->homogeneous = FALSE;
  private->spacing = 0;
  private->spacing_set = FALSE;
}

static void
gtk_box_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  GtkBox *box = GTK_BOX (object);
  GtkBoxPriv *private = box->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      private->orientation = g_value_get_enum (value);
      gtk_widget_queue_resize (GTK_WIDGET (box));
      break;
    case PROP_SPACING:
      gtk_box_set_spacing (box, g_value_get_int (value));
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
  GtkBoxPriv *private = box->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, private->orientation);
      break;
    case PROP_SPACING:
      g_value_set_int (value, private->spacing);
      break;
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, private->homogeneous);
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
  GtkBoxPriv  *private = box->priv;
  GList       *children;
  GtkBoxChild *child;

  *visible_children = *expand_children = 0;

  for (children = private->children; children; children = children->next)
    {
      child = children->data;

      if (gtk_widget_get_visible (child->widget))
	{
	  *visible_children += 1;
	  if (child->expand)
	    *expand_children += 1;
	}
    }
}

static gint
gtk_box_compare_gap (gconstpointer p1,
                      gconstpointer p2,
                      gpointer      data)
{
  GtkBoxDesiredSizes *sizes = data;
  const GtkBoxSpreading *c1 = p1;
  const GtkBoxSpreading *c2 = p2;

  const gint d1 = MAX (sizes[c1->index].natural_size -
                       sizes[c1->index].minimum_size,
                       0);
  const gint d2 = MAX (sizes[c2->index].natural_size -
                       sizes[c2->index].minimum_size,
                       0);

  gint delta = (d2 - d1);

  if (0 == delta)
    delta = (c2->index - c1->index);

  return delta;
}

static void
gtk_box_size_allocate (GtkWidget     *widget,
                       GtkAllocation *allocation)
{
  GtkBox *box = GTK_BOX (widget);
  GtkBoxPriv *private = box->priv;
  GtkBoxChild *child;
  GList *children;
  gint nvis_children;
  gint nexpand_children;

  widget->allocation = *allocation;

  count_expand_children (box, &nvis_children, &nexpand_children);

  if (nvis_children > 0)
    {
      guint border_width = gtk_container_get_border_width (GTK_CONTAINER (box));
      GtkTextDirection direction = gtk_widget_get_direction (widget);
      GtkAllocation child_allocation;
      GtkBoxSpreading *spreading = g_newa (GtkBoxSpreading, nvis_children);
      GtkBoxDesiredSizes *sizes = g_newa (GtkBoxDesiredSizes, nvis_children);

      GtkPackType packing;

      gint size;
      gint extra;
      gint x = 0, y = 0, i;
      gint child_size;

      if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
        size = allocation->width - border_width * 2 - (nvis_children - 1) * private->spacing;
      else
        size = allocation->height - border_width * 2 - (nvis_children - 1) * private->spacing;

      /* Retrieve desired size for visible children */
      i = 0;
      children = private->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (gtk_widget_get_visible (child->widget))
	    {
	      if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
		gtk_size_request_get_width_for_height (GTK_SIZE_REQUEST (child->widget),
							  allocation->height,
							  &sizes[i].minimum_size,
							  &sizes[i].natural_size);
	      else
		gtk_size_request_get_height_for_width (GTK_SIZE_REQUEST (child->widget),
							  allocation->width,
							  &sizes[i].minimum_size,
							  &sizes[i].natural_size);
	      
	      
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
	      
	      size -= sizes[i].minimum_size;
	      size -= child->padding * 2;
	      
	      spreading[i].index = i;
	      spreading[i].child = child;
	      
	      i += 1;
	    }
	}

      if (private->homogeneous)
	{
	  /* If were homogenous we still need to run the above loop to get the minimum sizes
	   * for children that are not going to fill 
	   */
	  if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
	    size = allocation->width - border_width * 2 - (nvis_children - 1) * private->spacing;
	  else
	    size = allocation->height - border_width * 2 - (nvis_children - 1) * private->spacing;
	  
          extra = size / nvis_children;
        }
      else
	{

          /* Distribute the container's extra space c_gap. We want to assign
           * this space such that the sum of extra space assigned to children
           * (c^i_gap) is equal to c_cap. The case that there's not enough
           * space for all children to take their natural size needs some
           * attention. The goals we want to achieve are:
           *
           *   a) Maximize number of children taking their natural size.
           *   b) The allocated size of children should be a continuous
           *   function of c_gap.  That is, increasing the container size by
           *   one pixel should never make drastic changes in the distribution.
           *   c) If child i takes its natural size and child j doesn't,
           *   child j should have received at least as much gap as child i.
           *
           * The following code distributes the additional space by following
           * this rules.
           */

          /* Sort descending by gap and position. */
          g_qsort_with_data (spreading,
                             nvis_children, sizeof (GtkBoxSpreading),
                             gtk_box_compare_gap, sizes);

          /* Distribute available space.
           * This master piece of a loop was conceived by Behdad Esfahbod.
           */
          for (i = nvis_children - 1; size > 0 && i >= 0; --i)
            {
              /* Divide remaining space by number of remaining children.
               * Sort order and reducing remaining space by assigned space
               * ensures that space is distributed equally.
               */
              gint glue = (size + i) / (i + 1);
              gint gap = sizes[spreading[i].index].natural_size
                       - sizes[spreading[i].index].minimum_size;

              extra = MIN (glue, gap);
              sizes[spreading[i].index].minimum_size += extra;

              size -= extra;
            }

          /* Calculate space which hasn't distributed yet,
           * and is available for expanding children.
           */
          if (nexpand_children > 0)
            extra = size / nexpand_children;
          else
            extra = 0;
        }

      /* Allocate child positions. */

      for (packing = GTK_PACK_START; packing <= GTK_PACK_END; ++packing)
        {
          if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              child_allocation.y = allocation->y + border_width;
              child_allocation.height = MAX (1, allocation->height - border_width * 2);
              if (packing == GTK_PACK_START)
                x = allocation->x + border_width;
              else
                x = allocation->x + allocation->width - border_width;
            }
          else
            {
              child_allocation.x = allocation->x + border_width;
              child_allocation.width = MAX (1, allocation->width - border_width * 2);
              if (packing == GTK_PACK_START)
                y = allocation->y + border_width;
              else
                y = allocation->y + allocation->height - border_width;
            }

	  i = 0;
          children = private->children;
          while (children)
	    {
	      child = children->data;
	      children = children->next;

	      if (gtk_widget_get_visible (child->widget))
	        {
                  if (child->pack == packing)
                    {
                      /* Assign the child's size. */
	              if (private->homogeneous)
		        {
		          if (nvis_children == 1)
                            child_size = size;
		          else
                            child_size = extra;

		          nvis_children -= 1;
		          size -= extra;
		        }
	              else
		        {
		          child_size = sizes[i].minimum_size + child->padding * 2;

		          if (child->expand)
		            {
		              if (nexpand_children == 1)
                                child_size += size;
		              else
                                child_size += extra;

		              nexpand_children -= 1;
		              size -= extra;
		            }
		        }

                      /* Assign the child's position. */
                      if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
                        {
	                  if (child->fill)
		            {
                              child_allocation.width = MAX (1, child_size - child->padding * 2);
                              child_allocation.x = x + child->padding;
		            }
	                  else
		            {
			      child_allocation.width = sizes[i].minimum_size;
                              child_allocation.x = x + (child_size - child_allocation.width) / 2;
		            }

                          if (packing == GTK_PACK_START)
			    {
			      x += child_size + private->spacing;
			    }
                          else
			    {
			      x -= child_size + private->spacing;

			      child_allocation.x -= child_size;
			    }

	                  if (direction == GTK_TEXT_DIR_RTL)
                            child_allocation.x = allocation->x + allocation->width - (child_allocation.x - allocation->x) - child_allocation.width;

                        }
                      else /* (private->orientation == GTK_ORIENTATION_VERTICAL) */
                        {
	                  if (child->fill)
		            {
                              child_allocation.height = MAX (1, child_size - child->padding * 2);
                              child_allocation.y = y + child->padding;
		            }
	                  else
		            {
			      child_allocation.height = sizes[i].minimum_size;
                              child_allocation.y = y + (child_size - child_allocation.height) / 2;
		            }

                         if (packing == GTK_PACK_START)
			   {
			     y += child_size + private->spacing;
			   }
                         else
			   {
			     y -= child_size + private->spacing;

			     child_allocation.y -= child_size;
			   }
                        }
	              gtk_widget_size_allocate (child->widget, &child_allocation);
                    }

		  i += 1;
                }
	    }
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
  gboolean expand = 0;
  gboolean fill = 0;
  guint padding = 0;
  GtkPackType pack_type = 0;

  if (property_id != CHILD_PROP_POSITION)
    gtk_box_query_child_packing (GTK_BOX (container),
				 child,
				 &expand,
				 &fill,
				 &padding,
				 &pack_type);
  switch (property_id)
    {
    case CHILD_PROP_EXPAND:
      gtk_box_set_child_packing (GTK_BOX (container),
				 child,
				 g_value_get_boolean (value),
				 fill,
				 padding,
				 pack_type);
      break;
    case CHILD_PROP_FILL:
      gtk_box_set_child_packing (GTK_BOX (container),
				 child,
				 expand,
				 g_value_get_boolean (value),
				 padding,
				 pack_type);
      break;
    case CHILD_PROP_PADDING:
      gtk_box_set_child_packing (GTK_BOX (container),
				 child,
				 expand,
				 fill,
				 g_value_get_uint (value),
				 pack_type);
      break;
    case CHILD_PROP_PACK_TYPE:
      gtk_box_set_child_packing (GTK_BOX (container),
				 child,
				 expand,
				 fill,
				 padding,
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
  gboolean expand = FALSE;
  gboolean fill = FALSE;
  guint padding = 0;
  GtkPackType pack_type = 0;
  GList *list;

  if (property_id != CHILD_PROP_POSITION)
    gtk_box_query_child_packing (GTK_BOX (container),
				 child,
				 &expand,
				 &fill,
				 &padding,
				 &pack_type);
  switch (property_id)
    {
      guint i;
    case CHILD_PROP_EXPAND:
      g_value_set_boolean (value, expand);
      break;
    case CHILD_PROP_FILL:
      g_value_set_boolean (value, fill);
      break;
    case CHILD_PROP_PADDING:
      g_value_set_uint (value, padding);
      break;
    case CHILD_PROP_PACK_TYPE:
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

static void
gtk_box_pack (GtkBox      *box,
              GtkWidget   *child,
              gboolean     expand,
              gboolean     fill,
              guint        padding,
              GtkPackType  pack_type)
{
  GtkBoxPriv *private = box->priv;
  GtkBoxChild *child_info;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == NULL);

  child_info = g_new (GtkBoxChild, 1);
  child_info->widget = child;
  child_info->padding = padding;
  child_info->expand = expand ? TRUE : FALSE;
  child_info->fill = fill ? TRUE : FALSE;
  child_info->pack = pack_type;

  private->children = g_list_append (private->children, child_info);

  gtk_widget_freeze_child_notify (child);

  gtk_widget_set_parent (child, GTK_WIDGET (box));
  
  gtk_widget_child_notify (child, "expand");
  gtk_widget_child_notify (child, "fill");
  gtk_widget_child_notify (child, "padding");
  gtk_widget_child_notify (child, "pack-type");
  gtk_widget_child_notify (child, "position");
  gtk_widget_thaw_child_notify (child);
}


static void
gtk_box_size_request_init (GtkSizeRequestIface *iface)
{
  parent_size_request_iface = g_type_interface_peek_parent (iface);

  iface->get_request_mode     = gtk_box_get_request_mode;
  iface->get_width            = gtk_box_get_width;
  iface->get_height           = gtk_box_get_height;
  iface->get_height_for_width = gtk_box_get_height_for_width;
  iface->get_width_for_height = gtk_box_get_width_for_height;
}

static GtkSizeRequestMode
gtk_box_get_request_mode  (GtkSizeRequest  *widget)
{
  GtkBoxPriv *private = GTK_BOX (widget)->priv;

  return (private->orientation == GTK_ORIENTATION_VERTICAL) ?
    GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH : GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
gtk_box_get_size (GtkSizeRequest      *widget,
		  GtkOrientation       orientation,
		  gint                *minimum_size,
		  gint                *natural_size)
{
  GtkBox *box;
  GtkBoxPriv *private;
  GList *children;
  gint nvis_children;
  gint border_width;
  gint minimum, natural;

  box = GTK_BOX (widget);
  private = box->priv;
  border_width = gtk_container_get_border_width (GTK_CONTAINER (box));

  minimum = natural = 0;

  nvis_children = 0;

  for (children = private->children; children; children = children->next)
    {
      GtkBoxChild *child = children->data;

      if (gtk_widget_get_visible (child->widget))
        {
          gint child_minimum, child_natural;

	  if (orientation == GTK_ORIENTATION_HORIZONTAL)
	    gtk_size_request_get_width (GTK_SIZE_REQUEST (child->widget),
					&child_minimum, &child_natural);
	  else
	    gtk_size_request_get_height (GTK_SIZE_REQUEST (child->widget),
					 &child_minimum, &child_natural);

          if (private->orientation == orientation)
	    {
              if (private->homogeneous)
                {
                  gint largest;

                  largest = child_minimum + child->padding * 2;
                  minimum = MAX (minimum, largest);

                  largest = child_natural + child->padding * 2;
                  natural = MAX (natural, largest);
                }
              else
                {
                  minimum += child_minimum + child->padding * 2;
                  natural += child_natural + child->padding * 2;
                }
	    }
	  else
	    {
	      /* The biggest mins and naturals in the opposing orientation */
              minimum = MAX (minimum, child_minimum);
              natural = MAX (natural, child_natural);
	    }

          nvis_children += 1;
        }
    }

  if (nvis_children > 0 && private->orientation == orientation)
    {
      if (private->homogeneous)
	{
	  minimum *= nvis_children;
	  natural *= nvis_children;
	}
      minimum += (nvis_children - 1) * private->spacing;
      natural += (nvis_children - 1) * private->spacing;
    }

  minimum += border_width * 2;
  natural += border_width * 2;

  if (minimum_size)
    *minimum_size = minimum;

  if (natural_size)
    *natural_size = natural;
}

static void
gtk_box_get_width (GtkSizeRequest      *widget,
		   gint                *minimum_size,
		   gint                *natural_size)
{
  gtk_box_get_size (widget, GTK_ORIENTATION_HORIZONTAL, minimum_size, natural_size);
}

static void
gtk_box_get_height (GtkSizeRequest      *widget,
		    gint                *minimum_size,
		    gint                *natural_size)
{
  gtk_box_get_size (widget, GTK_ORIENTATION_VERTICAL, minimum_size, natural_size);
}

static void 
gtk_box_compute_size_for_opposing_orientation (GtkBox *box,
					       gint    avail_size,
					       gint   *minimum_size,
					       gint   *natural_size)
{
  GtkBoxPriv    *private = box->priv;
  GtkBoxChild   *child;
  GList         *children;
  gint           nvis_children;
  gint           nexpand_children;
  gint           computed_minimum = 0, computed_natural = 0;
  guint          border_width = gtk_container_get_border_width (GTK_CONTAINER (box));

  count_expand_children (box, &nvis_children, &nexpand_children);

  if (nvis_children > 0)
    {
      GtkBoxSpreading     *spreading    = g_newa (GtkBoxSpreading, nvis_children);
      GtkBoxDesiredSizes  *sizes        = g_newa (GtkBoxDesiredSizes, nvis_children);
      GtkPackType          packing;
      gint                 size;
      gint                 extra, i;
      gint                 child_size, child_minimum, child_natural;

      size = avail_size - border_width * 2 - (nvis_children - 1) * private->spacing;

      /* Retrieve desired size for visible children */
      for (i = 0, children = private->children; children; children = children->next)
	{
	  child = children->data;
	  
	  if (gtk_widget_get_visible (child->widget))
	    {
	      if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
		gtk_size_request_get_width (GTK_SIZE_REQUEST (child->widget),
					    &sizes[i].minimum_size,
					    &sizes[i].natural_size);
	      else
		gtk_size_request_get_height (GTK_SIZE_REQUEST (child->widget),
					     &sizes[i].minimum_size,
					     &sizes[i].natural_size);
	      
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

	      size -= sizes[i].minimum_size;
	      size -= child->padding * 2;
	      
	      spreading[i].index = i;
	      spreading[i].child = child;
	      
	      i += 1;
	    }
	}

      if (private->homogeneous)
	{
	  /* If were homogenous we still need to run the above loop to get the minimum sizes
	   * for children that are not going to fill 
	   */
	  size = avail_size - border_width * 2 - (nvis_children - 1) * private->spacing;
          extra = size / nvis_children;
        }
      else
	{

          /* Distribute the container's extra space c_gap. We want to assign
           * this space such that the sum of extra space assigned to children
           * (c^i_gap) is equal to c_cap. The case that there's not enough
           * space for all children to take their natural size needs some
           * attention. The goals we want to achieve are:
           *
           *   a) Maximize number of children taking their natural size.
           *   b) The allocated size of children should be a continuous
           *   function of c_gap.  That is, increasing the container size by
           *   one pixel should never make drastic changes in the distribution.
           *   c) If child i takes its natural size and child j doesn't,
           *   child j should have received at least as much gap as child i.
           *
           * The following code distributes the additional space by following
           * this rules.
           */

          /* Sort descending by gap and position. */

          g_qsort_with_data (spreading,
                             nvis_children, sizeof (GtkBoxSpreading),
                             gtk_box_compare_gap, sizes);

          /* Distribute available space.
           * This master piece of a loop was conceived by Behdad Esfahbod.
           */
          for (i = nvis_children - 1; size > 0 && i >= 0; --i)
            {
              /* Divide remaining space by number of remaining children.
               * Sort order and reducing remaining space by assigned space
               * ensures that space is distributed equally.
               */
              gint glue = (size + i) / (i + 1);
              gint gap = sizes[spreading[i].index].natural_size
                       - sizes[spreading[i].index].minimum_size;

              extra = MIN (glue, gap);
              sizes[spreading[i].index].minimum_size += extra;

              size -= extra;
            }

          /* Calculate space which hasn't distributed yet,
           * and is available for expanding children.
           */
          if (nexpand_children > 0)
            extra = size / nexpand_children;
          else
            extra = 0;
        }

      /* Allocate child positions. */
      for (packing = GTK_PACK_START; packing <= GTK_PACK_END; ++packing)
        {
          for (i = 0, children = private->children; children; children = children->next)
	    {
	      child = children->data;

	      if (gtk_widget_get_visible (child->widget))
	        {
                  if (child->pack == packing)
                    {
                      /* Assign the child's size. */
	              if (private->homogeneous)
		        {
		          if (nvis_children == 1)
                            child_size = size;
		          else
                            child_size = extra;

		          nvis_children -= 1;
		          size -= extra;
		        }
	              else
		        {
		          child_size = sizes[i].minimum_size + child->padding * 2;

		          if (child->expand)
		            {
		              if (nexpand_children == 1)
                                child_size += size;
		              else
                                child_size += extra;

		              nexpand_children -= 1;
		              size -= extra;
		            }
		        }

		      if (child->fill)
			{
			  child_size = MAX (1, child_size - child->padding * 2);
			}
		      else
			{
			  child_size = sizes[i].minimum_size;
			}


                      /* Assign the child's position. */
                      if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
			gtk_size_request_get_height_for_width (GTK_SIZE_REQUEST (child->widget),
								  child_size, &child_minimum, &child_natural);
                      else /* (private->orientation == GTK_ORIENTATION_VERTICAL) */
			gtk_size_request_get_width_for_height (GTK_SIZE_REQUEST (child->widget),
								  child_size, &child_minimum, &child_natural);

		      
		      computed_minimum = MAX (computed_minimum, child_minimum);
		      computed_natural = MAX (computed_natural, child_natural);
                    }
		  i += 1;
                }
	    }
	}
    }

  computed_minimum += border_width * 2;
  computed_natural += border_width * 2;

  if (minimum_size)
    *minimum_size = computed_minimum;
  if (natural_size)
    *natural_size = computed_natural;
}  

static void 
gtk_box_compute_size_for_orientation (GtkBox *box,
				      gint    avail_size,
				      gint   *minimum_size,
				      gint   *natural_size)
{
  GtkBoxPriv    *private = box->priv;
  GList         *children;
  gint           nvis_children = 0;
  gint           required_size = 0, required_natural = 0, child_size, child_natural;
  gint           largest_child = 0, largest_natural = 0;
  guint          border_width;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (box));
  avail_size -= border_width * 2;

  for (children = private->children; children != NULL; 
       children = children->next, nvis_children++)
    {
      GtkBoxChild *child = children->data;

      if (gtk_widget_get_visible (child->widget))
        {

          if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
	    gtk_size_request_get_width_for_height (GTK_SIZE_REQUEST (child->widget),
						      avail_size, &child_size, &child_natural);
	  else
	    gtk_size_request_get_height_for_width (GTK_SIZE_REQUEST (child->widget),
						      avail_size, &child_size, &child_natural);


	  child_size    += child->padding * 2;
	  child_natural += child->padding * 2;

	  if (child_size > largest_child)
	    largest_child = child_size;

	  if (child_natural > largest_natural)
	    largest_natural = child_natural;

	  required_size    += child_size;
	  required_natural += child_natural;
        }
    }

  if (nvis_children > 0)
    {
      if (private->homogeneous)
	{
	  required_size    = largest_child   * nvis_children;
	  required_natural = largest_natural * nvis_children;
	}

      required_size     += (nvis_children - 1) * private->spacing;
      required_natural  += (nvis_children - 1) * private->spacing;
    }

  required_size    += border_width * 2;
  required_natural += border_width * 2;

  if (minimum_size)
    *minimum_size = required_size;

  if (natural_size)
    *natural_size = required_natural;
}

static void 
gtk_box_get_width_for_height (GtkSizeRequest *widget,
			      gint            height,
			      gint           *minimum_width,
			      gint           *natural_width)
{
  GtkBox        *box     = GTK_BOX (widget);
  GtkBoxPriv    *private = box->priv;

  if (private->orientation == GTK_ORIENTATION_VERTICAL)
    gtk_box_compute_size_for_opposing_orientation (box, height, minimum_width, natural_width); 
  else
    gtk_box_compute_size_for_orientation (box, height, minimum_width, natural_width);
}

static void 
gtk_box_get_height_for_width (GtkSizeRequest *widget,
			      gint            width,
			      gint           *minimum_height,
			      gint           *natural_height)
{
  GtkBox        *box     = GTK_BOX (widget);
  GtkBoxPriv    *private = box->priv;

  if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_box_compute_size_for_opposing_orientation (box, width, minimum_height, natural_height);
  else
    gtk_box_compute_size_for_orientation (box, width, minimum_height, natural_height);
}

/**
 * gtk_box_new:
 * @orientation: the box' orientation.
 * @homogeneous: %TRUE if all children are to be given equal space allocations.
 * @spacing: the number of pixels to place by default between children.
 *
 * Creates a new #GtkBox.
 *
 * Return value: a new #GtkBox.
 *
 * Since: 3.0
 **/
GtkWidget*
gtk_box_new (GtkOrientation orientation,
             gboolean       homogeneous,
             gint           spacing)
{
  return g_object_new (GTK_TYPE_BOX,
                       "orientation", orientation,
                       "spacing",     spacing,
                       "homogeneous", homogeneous ? TRUE : FALSE,
                       NULL);
}

/**
 * gtk_box_pack_start:
 * @box: a #GtkBox
 * @child: the #GtkWidget to be added to @box
 * @expand: %TRUE if the new child is to be given extra space allocated to
 * @box.  The extra space will be divided evenly between all children of
 * @box that use this option
 * @fill: %TRUE if space given to @child by the @expand option is
 *   actually allocated to @child, rather than just padding it.  This
 *   parameter has no effect if @expand is set to %FALSE.  A child is
 *   always allocated the full height of a #GtkHBox and the full width 
 *   of a #GtkVBox. This option affects the other dimension
 * @padding: extra space in pixels to put between this child and its
 *   neighbors, over and above the global amount specified by
 *   #GtkBox:spacing property.  If @child is a widget at one of the 
 *   reference ends of @box, then @padding pixels are also put between 
 *   @child and the reference edge of @box
 *
 * Adds @child to @box, packed with reference to the start of @box.
 * The @child is packed after any other child packed with reference 
 * to the start of @box.
 */
void
gtk_box_pack_start (GtkBox    *box,
		    GtkWidget *child,
		    gboolean   expand,
		    gboolean   fill,
		    guint      padding)
{
  gtk_box_pack (box, child, expand, fill, padding, GTK_PACK_START);
}

/**
 * gtk_box_pack_end:
 * @box: a #GtkBox
 * @child: the #GtkWidget to be added to @box
 * @expand: %TRUE if the new child is to be given extra space allocated 
 *   to @box. The extra space will be divided evenly between all children 
 *   of @box that use this option
 * @fill: %TRUE if space given to @child by the @expand option is
 *   actually allocated to @child, rather than just padding it.  This
 *   parameter has no effect if @expand is set to %FALSE.  A child is
 *   always allocated the full height of a #GtkHBox and the full width 
 *   of a #GtkVBox.  This option affects the other dimension
 * @padding: extra space in pixels to put between this child and its
 *   neighbors, over and above the global amount specified by
 *   #GtkBox:spacing property.  If @child is a widget at one of the 
 *   reference ends of @box, then @padding pixels are also put between 
 *   @child and the reference edge of @box
 *
 * Adds @child to @box, packed with reference to the end of @box.  
 * The @child is packed after (away from end of) any other child 
 * packed with reference to the end of @box.
 */
void
gtk_box_pack_end (GtkBox    *box,
		  GtkWidget *child,
		  gboolean   expand,
		  gboolean   fill,
		  guint      padding)
{
  gtk_box_pack (box, child, expand, fill, padding, GTK_PACK_END);
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
  GtkBoxPriv *private;

  g_return_if_fail (GTK_IS_BOX (box));

  private = box->priv;

  if ((homogeneous ? TRUE : FALSE) != private->homogeneous)
    {
      private->homogeneous = homogeneous ? TRUE : FALSE;
      g_object_notify (G_OBJECT (box), "homogeneous");
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
 * Return value: %TRUE if the box is homogeneous.
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
  GtkBoxPriv *private;

  g_return_if_fail (GTK_IS_BOX (box));

  private = box->priv;

  if (spacing != private->spacing)
    {
      private->spacing = spacing;
      _gtk_box_set_spacing_set (box, TRUE);

      g_object_notify (G_OBJECT (box), "spacing");

      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}

/**
 * gtk_box_get_spacing:
 * @box: a #GtkBox
 * 
 * Gets the value set by gtk_box_set_spacing().
 * 
 * Return value: spacing between children
 **/
gint
gtk_box_get_spacing (GtkBox *box)
{
  g_return_val_if_fail (GTK_IS_BOX (box), 0);

  return box->priv->spacing;
}

void
_gtk_box_set_spacing_set (GtkBox  *box,
                          gboolean spacing_set)
{
  GtkBoxPriv *private;

  g_return_if_fail (GTK_IS_BOX (box));

  private = box->priv;

  private->spacing_set = spacing_set ? TRUE : FALSE;
}

gboolean
_gtk_box_get_spacing_set (GtkBox *box)
{
  GtkBoxPriv *private;

  g_return_val_if_fail (GTK_IS_BOX (box), FALSE);

  private = box->priv;

  return private->spacing_set;
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
 * The list is the <structfield>children</structfield> field of
 * #GtkBox-struct, and contains both widgets packed #GTK_PACK_START 
 * as well as widgets packed #GTK_PACK_END, in the order that these 
 * widgets were added to @box.
 * 
 * A widget's position in the @box children list determines where 
 * the widget is packed into @box.  A child widget at some position 
 * in the list will be packed just after all other widgets of the 
 * same packing type that appear earlier in the list.
 */ 
void
gtk_box_reorder_child (GtkBox    *box,
		       GtkWidget *child,
		       gint       position)
{
  GtkBoxPriv *priv;
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

  gtk_widget_child_notify (child, "position");
  if (gtk_widget_get_visible (child)
      && gtk_widget_get_visible (GTK_WIDGET (box)))
    gtk_widget_queue_resize (child);
}

/**
 * gtk_box_query_child_packing:
 * @box: a #GtkBox
 * @child: the #GtkWidget of the child to query
 * @expand: pointer to return location for #GtkBox:expand child property 
 * @fill: pointer to return location for #GtkBox:fill child property 
 * @padding: pointer to return location for #GtkBox:padding child property 
 * @pack_type: pointer to return location for #GtkBox:pack-type child property 
 * 
 * Obtains information about how @child is packed into @box.
 */
void
gtk_box_query_child_packing (GtkBox      *box,
			     GtkWidget   *child,
			     gboolean    *expand,
			     gboolean    *fill,
			     guint       *padding,
			     GtkPackType *pack_type)
{
  GtkBoxPriv *private;
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
      if (expand)
	*expand = child_info->expand;
      if (fill)
	*fill = child_info->fill;
      if (padding)
	*padding = child_info->padding;
      if (pack_type)
	*pack_type = child_info->pack;
    }
}

/**
 * gtk_box_set_child_packing:
 * @box: a #GtkBox
 * @child: the #GtkWidget of the child to set
 * @expand: the new value of the #GtkBox:expand child property 
 * @fill: the new value of the #GtkBox:fill child property
 * @padding: the new value of the #GtkBox:padding child property
 * @pack_type: the new value of the #GtkBox:pack-type child property
 *
 * Sets the way @child is packed into @box.
 */
void
gtk_box_set_child_packing (GtkBox      *box,
			   GtkWidget   *child,
			   gboolean     expand,
			   gboolean     fill,
			   guint        padding,
			   GtkPackType  pack_type)
{
  GtkBoxPriv *private;
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
      child_info->expand = expand != FALSE;
      gtk_widget_child_notify (child, "expand");
      child_info->fill = fill != FALSE;
      gtk_widget_child_notify (child, "fill");
      child_info->padding = padding;
      gtk_widget_child_notify (child, "padding");
      if (pack_type == GTK_PACK_END)
	child_info->pack = GTK_PACK_END;
      else
	child_info->pack = GTK_PACK_START;
      gtk_widget_child_notify (child, "pack-type");

      if (gtk_widget_get_visible (child)
          && gtk_widget_get_visible (GTK_WIDGET (box)))
	gtk_widget_queue_resize (child);
    }
  gtk_widget_thaw_child_notify (child);
}

void
_gtk_box_set_old_defaults (GtkBox *box)
{
  GtkBoxPriv *private;

  g_return_if_fail (GTK_IS_BOX (box));

  private = box->priv;

  private->default_expand = TRUE;
}

static void
gtk_box_add (GtkContainer *container,
	     GtkWidget    *widget)
{
  GtkBoxPriv *priv = GTK_BOX (container)->priv;

  gtk_box_pack_start (GTK_BOX (container), widget,
                      priv->default_expand,
                      priv->default_expand,
                      0);
}

static void
gtk_box_remove (GtkContainer *container,
		GtkWidget    *widget)
{
  GtkBox *box = GTK_BOX (container);
  GtkBoxPriv *priv = box->priv;
  GtkBoxChild *child;
  GList *children;

  children = priv->children;
  while (children)
    {
      child = children->data;

      if (child->widget == widget)
	{
	  gboolean was_visible;

	  was_visible = gtk_widget_get_visible (widget);
	  gtk_widget_unparent (widget);

	  priv->children = g_list_remove_link (priv->children, children);
	  g_list_free (children);
	  g_free (child);

	  /* queue resize regardless of gtk_widget_get_visible (container),
	   * since that's what is needed by toplevels.
	   */
	  if (was_visible)
	    gtk_widget_queue_resize (GTK_WIDGET (container));

	  break;
	}

      children = children->next;
    }
}

static void
gtk_box_forall (GtkContainer *container,
		gboolean      include_internals,
		GtkCallback   callback,
		gpointer      callback_data)
{
  GtkBox *box = GTK_BOX (container);
  GtkBoxPriv *priv = box->priv;
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
