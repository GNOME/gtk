/* gtkspreadtable.c
 * Copyright (C) 2007-2010 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gtkspreadtable
 * @Short_Description: A container that distributes its children evenly across rows/columns.
 * @Title: GtkSpreadTable
 *
 * #GtkSpreadTable positions its children by distributing them as
 * evenly as possible across a fixed number of rows or columns.
 *
 * When oriented vertically the #GtkSpreadTable will list its
 * children in order from top to bottom in columns and request
 * the smallest height as possible regardless of differences in
 * child sizes.
 */

#include "config.h"
#include "gtksizerequest.h"
#include "gtkorientable.h"
#include "gtkspreadtable.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtktypeutils.h"

#define DEFAULT_LINES 2

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_HORIZONTAL_SPACING,
  PROP_VERTICAL_SPACING,
  PROP_LINES
};

struct _GtkSpreadTablePrivate {
  GList         *children;

  GtkOrientation orientation;

  guint16        lines;
  guint16        horizontal_spacing;
  guint16        vertical_spacing;
};

/* Our column equalizing algorithm splits up
 * children into 'lines' segments and maintains
 * the integrity of the array of line segments while
 * itterating over columns (or 'segments').
 */
typedef struct {
  GList *children; /* List of children in this segment */
  gint   size;     /* Overall size of segment (or 'height of column') */
} LineSegment;


/* GObjectClass */
static void gtk_spread_table_get_property         (GObject             *object,
						   guint                prop_id,
						   GValue              *value,
						   GParamSpec          *pspec);
static void gtk_spread_table_set_property         (GObject             *object,
						   guint                prop_id,
						   const GValue        *value,
						   GParamSpec          *pspec);

/* GtkWidgetClass */

static GtkSizeRequestMode gtk_spread_table_get_request_mode (GtkWidget *widget);
static void gtk_spread_table_get_width            (GtkWidget           *widget,
						   gint                *minimum_size,
						   gint                *natural_size);
static void gtk_spread_table_get_height           (GtkWidget           *widget,
						   gint                *minimum_size,
						   gint                *natural_size);
static void gtk_spread_table_get_height_for_width (GtkWidget           *widget,
						   gint                 width,
						   gint                *minimum_height,
						   gint                *natural_height);
static void gtk_spread_table_get_width_for_height (GtkWidget           *widget,
						   gint                 width,
						   gint                *minimum_height,
						   gint                *natural_height);
static void gtk_spread_table_size_allocate        (GtkWidget           *widget,
						   GtkAllocation       *allocation);

/* GtkContainerClass */
static void gtk_spread_table_add                  (GtkContainer        *container,
						   GtkWidget           *widget);
static void gtk_spread_table_remove               (GtkContainer        *container,
						   GtkWidget           *widget);


static void gtk_spread_table_forall               (GtkContainer        *container,
						   gboolean             include_internals,
						   GtkCallback          callback,
						   gpointer             callback_data); 
static GType gtk_spread_table_child_type          (GtkContainer        *container);


G_DEFINE_TYPE_WITH_CODE (GtkSpreadTable, gtk_spread_table, GTK_TYPE_CONTAINER,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

#define ITEM_SPACING(table)						\
  (((GtkSpreadTable *)(table))->priv->orientation == GTK_ORIENTATION_HORIZONTAL ? \
   ((GtkSpreadTable *)(table))->priv->horizontal_spacing :		\
   ((GtkSpreadTable *)(table))->priv->vertical_spacing)

#define LINE_SPACING(table)						\
  (((GtkSpreadTable *)(table))->priv->orientation == GTK_ORIENTATION_HORIZONTAL ? \
   ((GtkSpreadTable *)(table))->priv->vertical_spacing :		\
   ((GtkSpreadTable *)(table))->priv->horizontal_spacing)

#define OPPOSITE_ORIENTATION(table)					\
  (((GtkSpreadTable *)(table))->priv->orientation == GTK_ORIENTATION_HORIZONTAL ? \
   GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL)

static void
gtk_spread_table_class_init (GtkSpreadTableClass *class)
{
  GObjectClass      *gobject_class    = G_OBJECT_CLASS (class);
  GtkWidgetClass    *widget_class     = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class  = GTK_CONTAINER_CLASS (class);
  
  gobject_class->get_property         = gtk_spread_table_get_property;
  gobject_class->set_property         = gtk_spread_table_set_property;

  widget_class->get_request_mode               = gtk_spread_table_get_request_mode;
  widget_class->get_preferred_width            = gtk_spread_table_get_width;
  widget_class->get_preferred_height           = gtk_spread_table_get_height;
  widget_class->get_preferred_height_for_width = gtk_spread_table_get_height_for_width;
  widget_class->get_preferred_width_for_height = gtk_spread_table_get_width_for_height;
  widget_class->size_allocate                  = gtk_spread_table_size_allocate;
  
  container_class->add                = gtk_spread_table_add;
  container_class->remove             = gtk_spread_table_remove;
  container_class->forall             = gtk_spread_table_forall;
  container_class->child_type         = gtk_spread_table_child_type;

  gtk_container_class_handle_border_width (container_class);

  /* GObjectClass properties */
  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");

  /**
   * GtkSpreadTable:lines:
   *
   * The number of lines (rows/columns) to evenly distribute children to.
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_LINES,
                                   g_param_spec_uint ("lines",
						      P_("Lines"),
						      P_("The number of lines (rows/columns) to "
							 "evenly distribute children to."),
						      DEFAULT_LINES,
						      65535,
						      DEFAULT_LINES,
						      G_PARAM_READABLE | G_PARAM_WRITABLE));


  /**
   * GtkSpreadTable:vertical-spacing:
   *
   * The amount of vertical space between two children.
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_VERTICAL_SPACING,
                                   g_param_spec_uint ("vertical-spacing",
						      P_("Vertical spacing"),
						      P_("The amount of vertical space between two children"),
						      0,
						      65535,
						      0,
						      G_PARAM_READABLE | G_PARAM_WRITABLE));

  /**
   * GtkSpreadTable:horizontal-spacing:
   *
   * The amount of horizontal space between two children.
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_HORIZONTAL_SPACING,
                                   g_param_spec_uint ("horizontal-spacing",
						      P_("Horizontal spacing"),
						      P_("The amount of horizontal space between two children"),
						      0,
						      65535,
						      0,
						      G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_type_class_add_private (class, sizeof (GtkSpreadTablePrivate));
}

static void
gtk_spread_table_init (GtkSpreadTable *spread_table)
{
  GtkSpreadTablePrivate *priv; 
  
  spread_table->priv = priv = 
    G_TYPE_INSTANCE_GET_PRIVATE (spread_table, GTK_TYPE_SPREAD_TABLE, GtkSpreadTablePrivate);

  /* We are vertical by default */
  priv->orientation = GTK_ORIENTATION_VERTICAL;
  priv->lines       = DEFAULT_LINES;

  gtk_widget_set_has_window (GTK_WIDGET (spread_table), FALSE);
}

/*****************************************************
 *                  GObectClass                      * 
 *****************************************************/
static void
gtk_spread_table_get_property (GObject      *object,
			       guint         prop_id,
			       GValue       *value,
			       GParamSpec   *pspec)
{
  GtkSpreadTable        *table = GTK_SPREAD_TABLE (object);
  GtkSpreadTablePrivate *priv  = table->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_LINES:
      g_value_set_uint (value, gtk_spread_table_get_lines (table));
      break;
    case PROP_VERTICAL_SPACING:
      g_value_set_uint (value, gtk_spread_table_get_vertical_spacing (table));
      break;
    case PROP_HORIZONTAL_SPACING:
      g_value_set_uint (value, gtk_spread_table_get_horizontal_spacing (table));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_spread_table_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
  GtkSpreadTable        *table = GTK_SPREAD_TABLE (object);
  GtkSpreadTablePrivate *priv  = table->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      priv->orientation = g_value_get_enum (value);

      /* Re-spread_table the children in the new orientation */
      gtk_widget_queue_resize (GTK_WIDGET (table));
      break;
    case PROP_LINES:
      gtk_spread_table_set_lines (table, g_value_get_uint (value));
      break;
    case PROP_HORIZONTAL_SPACING:
      gtk_spread_table_set_horizontal_spacing (table, g_value_get_uint (value));
      break;
    case PROP_VERTICAL_SPACING:
      gtk_spread_table_set_vertical_spacing (table, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/*****************************************************
 *  Geometric helper functions for request/allocate  * 
 *****************************************************/
static void 
get_widget_size (GtkWidget      *widget,
		 GtkOrientation  orientation,
		 gint            for_size,
		 gint           *min_size,
		 gint           *nat_size)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (for_size < 0)
	gtk_widget_get_preferred_width (widget, min_size, nat_size);
      else
	gtk_widget_get_preferred_width_for_height (widget, for_size, min_size, nat_size);
    }
  else
    {
      if (for_size < 0)
	gtk_widget_get_preferred_height (widget, min_size, nat_size);
      else
	gtk_widget_get_preferred_height_for_width (widget, for_size, min_size, nat_size);
    }
}


static GList *
get_visible_children (GtkSpreadTable *table)
{
  GtkSpreadTablePrivate  *priv = table->priv;
  GtkWidget              *child;
  GList                  *list, *visible = NULL;

  for (list = priv->children; list; list = list->next)
    {
      child = list->data;

      if (!gtk_widget_get_visible (child))
        continue;

      visible = g_list_prepend (visible, child);
    }

  return g_list_reverse (visible);
}

/* This gets the widest child, it is used to reserve 
 * enough space for (columns * widest_child) 
 */
static void
get_largest_line_thickness (GtkSpreadTable *table, 
			    gint           *min_thickness,
			    gint           *nat_thickness)
{
  GtkSpreadTablePrivate  *priv = table->priv;
  GList                  *list;
  gint                    min_size = 0, nat_size = 0;
  GtkOrientation          opposite_orientation;

  opposite_orientation = OPPOSITE_ORIENTATION (table);

  for (list = priv->children; list; list = list->next)
    {
      GtkWidget *child = list->data;
      gint       child_min, child_nat;

      if (!gtk_widget_get_visible (child))
        continue;

      get_widget_size (child, opposite_orientation, -1, &child_min, &child_nat);

      min_size = MAX (min_size, child_min);
      nat_size = MAX (nat_size, child_nat);
    }

  *min_thickness = min_size;
  *nat_thickness = nat_size;
}

/* Gets the column width for a given width */
static gint 
get_line_thickness (GtkSpreadTable *table,
		    gint            for_thickness)
{
  GtkSpreadTablePrivate  *priv = table->priv;
  gint                    line_thickness;

  /* Get the available size per line (when vertical, we are getting the column width here) */
  line_thickness = for_thickness - (priv->lines -1) * LINE_SPACING (table);
  line_thickness = line_thickness / priv->lines;

  return line_thickness;
}

/* Gets the overall height of a column (length of a line segment) */
static gint
get_segment_length (GtkSpreadTable *table,
		    gint            line_thickness,
		    GList          *seg_children)
{
  GtkSpreadTablePrivate  *priv = table->priv;
  GList                  *list;
  gint                    size = 0, i = 0;
  gint                    spacing;

  spacing = ITEM_SPACING (table);

  for (i = 0, list = seg_children; list; list = list->next)
    {
      GtkWidget *child = list->data;
      gint       child_nat;

      if (!gtk_widget_get_visible (child))
        continue;

      get_widget_size (child, priv->orientation, line_thickness, NULL, &child_nat);

      size += child_nat;

      if (i != 0)
	size += spacing;

      i++;
    }

  return size;
}

/* Moves a child from the previous line segment (column) into a given column
 * and keeps the LineSegments array updated */
static gboolean
child_forward (GtkSpreadTable *table, 
	       LineSegment    *line_segments, 
	       gint            line_thickness,
	       gint            line_index)
{
  GtkSpreadTablePrivate  *priv = table->priv;
  GtkWidget              *child;
  GList                  *link;
  gint                    child_size;

  g_assert (line_index > 0);
 
  /* Get the last child from the previous line */
  link = g_list_last (line_segments[line_index - 1].children);

  if (!link)
    return FALSE;

  child = link->data;
  get_widget_size (child, priv->orientation, line_thickness, NULL, &child_size);

  /* Adjust target column size */
  if (line_segments[line_index].children)
    line_segments[line_index].size += ITEM_SPACING (table);
  line_segments[line_index].size += child_size;

  /* Remove it from the previous line */
  line_segments[line_index - 1].children = 
    g_list_remove_link (line_segments[line_index - 1].children, link);

  /* Prepend child to this line */
  line_segments[line_index].children = 
    g_list_concat (link, line_segments[line_index].children);

  /* Adjust previous column size */
  if (line_segments[line_index - 1].children)
    line_segments[line_index - 1].size -= ITEM_SPACING (table);
  line_segments[line_index - 1].size -= child_size;
  
  return TRUE;
}

/* Gets the largest size of line segments between two inclusive indexes*/
static gint 
count_size (LineSegment  *line_segments,
	    gint          index_from,
	    gint          index_to)
{
  gint total_size = 0;
  gint i;

  g_assert (index_from < index_to);

  for (i = index_from; i < index_to + 1; i++)
    {
      /* Ignore the size of columns which have only one child,
       * if they are huge widgets then we want to ignore them and
       * even out the remaining columns, if they are small then we
       * still want to put at least one widget on every line.
       */
      if (line_segments[i].children && 
	  line_segments[i].children->next)
	total_size = MAX (total_size, line_segments[i].size);
    }

  return total_size;
}

/* Creates a copy of the LineSegments array */
static LineSegment *
copy_line_segments (GtkSpreadTable *table,
		    LineSegment    *segments)
{
  GtkSpreadTablePrivate *priv = table->priv;
  LineSegment           *copy = g_new0 (LineSegment, priv->lines);
  gint                   i;

  for (i = 0; i < priv->lines; i++)
    {
      copy[i].children = g_list_copy (segments[i].children);
      copy[i].size     = segments[i].size;
    }

  return copy;
}

/* Frees the LineSegments array */
static void
free_line_segments (GtkSpreadTable *table, 
		    LineSegment    *segments)
{
  GtkSpreadTablePrivate *priv = table->priv;
  gint                 i;

  for (i = 0; i < priv->lines; i++)
    g_list_free (segments[i].children);

  g_free (segments);
}

static void
merge_line_segments (LineSegment  *dest,
		     LineSegment  *src,
		     gint          index)
{
  gint i;

  for (i = 0; i < index + 1; i++)
    {
      g_list_free (dest[i].children);
      dest[i].children = g_list_copy (src[i].children);
      dest[i].size     = src[i].size;
    }
}

/* This algorithm equalizes 2 columns but recurses into previously
 * equalized columns when widgets are moved over from previous columns */
static void
equalize_line_segments (GtkSpreadTable *table, 
			LineSegment    *line_segments,
			gint            line_thickness,
			gint            line_index)
{
  gint         guess_size, best_size;
  gboolean     child_moved;
  LineSegment *segments;

  g_assert (line_index > 0);

  /* Use a local copy to check for a best size of this index and all
   * previous indexes recursively (this allows us to 'try' our array
   * and compare it before updating the return value)  */
  segments = copy_line_segments (table, line_segments);

  /* Get the total best size for all preceeding columns */
  guess_size = best_size = count_size (segments, 0, line_index);

  /* Loop shifting one widget from the previous column over at a time
   * until the new size is worse than the last size (pull as many widgets
   * across into the new segment without getting a larger size)
   */
  while (guess_size <= best_size)
    {
      child_moved = child_forward (table, segments, line_thickness, line_index);

      /* No more children to pull into this column, break out
       * (never decide to leave a preceeding column empty, stick
       * with the last result instead)
       */
      if (!child_moved)
	break;

      /* Now that we've pulled a widget over, recurse backwards through the list and see
       * if we need to shift any widgets in previous lines (columns) 
       */
      if (line_index > 1)
	equalize_line_segments (table, segments, line_thickness, line_index - 1);
      
      /* Get the total new best size for columns iterated over so far */
      guess_size = count_size (segments, 0, line_index);

      /* keep pulling widgets along even if the whole thing stays 
       * the same size (untill the new guess is actually worse) */
      if (guess_size <= best_size)
	{
 	  best_size = guess_size;

	  /* This one's a keeper, override our portion of 'line_segments' */
	  merge_line_segments (line_segments, segments, line_index);
	}
    }

  free_line_segments (table, segments);
}

/* All purpose algorithm entry point, this function takes an allocated size
 * to fit the columns (or rows) and then splits up the child list into 
 * 'n' children per 'segment' in a way that it takes the least space as possible.
 *
 * If 'segments' is specified, it will be allocated the array of integers representing
 * how many children are to be fit per line segment (and must be freed afterwards with g_free()).
 *
 * The function returns the required space (the required height for all columns).
 */
static gint
segment_lines_for_size (GtkSpreadTable *table, 
			gint            for_size,
			gint          **segments)
{
  GtkSpreadTablePrivate  *priv = table->priv;
  LineSegment            *line_segments;
  gint                    line_thickness;
  gint                    max_segment_length = 0;
  gint                   *segment_counts = NULL;
  gint                    i;

  line_segments    = g_new0 (LineSegment, priv->lines);
  if (segments)
    segment_counts = g_new0 (gint, priv->lines);

  line_thickness = get_line_thickness (table, for_size);

  /* Start by lining up all widgets in the first segment */
  line_segments[0].children = get_visible_children (table);
  line_segments[0].size     = get_segment_length (table, line_thickness, line_segments[0].children);

  /* For every segment (column), equalize it and pull widgets from previous segments 
   * until we've pulled widgets into all columns */
  for (i = 1; i < priv->lines; i++)
    equalize_line_segments (table, line_segments, line_thickness, i);

  /* Free the line segments and tally up the 'segment_counts' for size_allocate */
  for (i = 0; i < priv->lines; i++)
    {
      max_segment_length = MAX (max_segment_length, line_segments[i].size);

      if (segment_counts)
	segment_counts[i] = g_list_length (line_segments[i].children);

      g_list_free (line_segments[i].children);
    }
  g_free (line_segments);

  if (segments)
    *segments = segment_counts;

  return max_segment_length;
}


/*****************************************************
 *                 GtkWidgetClass                    * 
 *****************************************************/
static GtkSizeRequestMode 
gtk_spread_table_get_request_mode (GtkWidget *widget)
{
  GtkSpreadTable         *table = GTK_SPREAD_TABLE (widget);
  GtkSpreadTablePrivate  *priv  = table->priv;
  
  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
  else
    return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void 
gtk_spread_table_get_width (GtkWidget           *widget,
			    gint                *minimum_size,
			    gint                *natural_size)
{
  GtkSpreadTable         *table = GTK_SPREAD_TABLE (widget);
  GtkSpreadTablePrivate  *priv  = table->priv;
  gint                    min_width, nat_width;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      /* Get the width for minimum height */      
      gint min_height;

      GTK_WIDGET_GET_CLASS (widget)->get_preferred_height (widget, &min_height, NULL);
      GTK_WIDGET_GET_CLASS (widget)->get_preferred_width_for_height (widget, min_height, 
								     &min_width, &nat_width);
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      gint min_thickness, nat_thickness;

      get_largest_line_thickness (table, &min_thickness, &nat_thickness);

      min_width = min_thickness * priv->lines + LINE_SPACING (table) * (priv->lines - 1);
      nat_width = nat_thickness * priv->lines + LINE_SPACING (table) * (priv->lines - 1);
    }

#if 0
  g_print ("get_width() called; returning min %d and nat %d\n",
	   min_width, nat_width);
#endif

  if (minimum_size)
    *minimum_size = min_width;

  if (natural_size)
    *natural_size = nat_width;
}

static void 
gtk_spread_table_get_height (GtkWidget           *widget,
			     gint                *minimum_size,
			     gint                *natural_size)
{
  GtkSpreadTable         *table = GTK_SPREAD_TABLE (widget);
  GtkSpreadTablePrivate  *priv  = table->priv;
  gint                    min_height, nat_height;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gint min_thickness, nat_thickness;

      get_largest_line_thickness (table, &min_thickness, &nat_thickness);

      min_height = min_thickness * priv->lines + LINE_SPACING (table) * (priv->lines - 1);
      nat_height = nat_thickness * priv->lines + LINE_SPACING (table) * (priv->lines - 1);
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      /* Return the height for the minimum width */
      gint min_width;

      GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, &min_width, NULL);
      GTK_WIDGET_GET_CLASS (widget)->get_preferred_height_for_width (widget, min_width, 
								     &min_height, &nat_height);
    }

#if 0
  g_print ("get_height() called; returning min %d and nat %d\n",
	   min_height, nat_height);
#endif

  if (minimum_size)
    *minimum_size = min_height;

  if (natural_size)
    *natural_size = nat_height;
}

static void 
gtk_spread_table_get_height_for_width (GtkWidget           *widget,
				       gint                 width,
				       gint                *minimum_height,
				       gint                *natural_height)
{
  GtkSpreadTable         *table = GTK_SPREAD_TABLE (widget);
  GtkSpreadTablePrivate  *priv  = table->priv;
  gint                    min_height = 0, nat_height = 0;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      /* Just return the minimum/natural height */
      GTK_WIDGET_GET_CLASS (widget)->get_preferred_height (widget, &min_height, &nat_height);
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      gint min_width;

      /* Make sure its no smaller than the minimum */
      GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, &min_width, NULL);

      /* This will segment the lines evenly and return the overall 
       * lengths of the split segments */
      nat_height = min_height = segment_lines_for_size (table, MAX (width, min_width), NULL);
    }

#if 0
  g_print ("get_height_for_width() called for width %d; returning min %d and nat %d\n",
	   width, min_height, nat_height);
#endif

  if (minimum_height)
    *minimum_height = min_height;

  if (natural_height)
    *natural_height = nat_height;
}

static void
gtk_spread_table_get_width_for_height (GtkWidget           *widget,
				       gint                 height,
				       gint                *minimum_width,
				       gint                *natural_width)
{
  GtkSpreadTable         *table = GTK_SPREAD_TABLE (widget);
  GtkSpreadTablePrivate  *priv      = table->priv;
  gint                    min_width = 0, nat_width = 0;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gint min_height;

      /* Make sure its no smaller than the minimum */
      GTK_WIDGET_GET_CLASS (widget)->get_preferred_height (widget, &min_height, NULL);

      /* This will segment the lines evenly and return the overall 
       * lengths of the split segments */
      nat_width = min_width = segment_lines_for_size (table, MAX (height, min_height), NULL);
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      /* Just return the minimum/natural height */
      GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, &min_width, &nat_width);
    }

#if 0
  g_print ("get_width_for_height() called for height %d; returning min %d and nat %d\n",
	   height, min_width, nat_width);
#endif

  if (minimum_width)
    *minimum_width = min_width;

  if (natural_width)
    *natural_width = nat_width;
}

static void
allocate_child (GtkSpreadTable *table,
                GtkWidget      *child,
                gint            item_offset,
                gint            line_offset,
                gint            item_size,
                gint            line_size)
{
  GtkSpreadTablePrivate  *priv = table->priv;
  GtkAllocation           widget_allocation;
  GtkAllocation           child_allocation;

  gtk_widget_get_allocation (GTK_WIDGET (table), &widget_allocation);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      child_allocation.x      = widget_allocation.x + item_offset;
      child_allocation.y      = widget_allocation.y + line_offset;
      child_allocation.width  = item_size;
      child_allocation.height = line_size;
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      child_allocation.x      = widget_allocation.x + line_offset;
      child_allocation.y      = widget_allocation.y + item_offset;
      child_allocation.width  = line_size;
      child_allocation.height = item_size;
    }

  gtk_widget_size_allocate (child, &child_allocation);
}

static void
gtk_spread_table_size_allocate (GtkWidget     *widget,
				GtkAllocation *allocation)
{
  GtkSpreadTable        *table = GTK_SPREAD_TABLE (widget);
  GtkSpreadTablePrivate *priv = table->priv;
  GList                 *list;
  gint                  *segments = NULL;
  gint                   full_thickness;
  gint                   i, j;
  gint                   line_offset, item_offset;
  gint                   line_thickness;
  gint                   line_spacing;
  gint                   item_spacing;
  GtkOrientation         opposite_orientation;

  gtk_widget_set_allocation (widget, allocation);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    full_thickness = allocation->height;
  else
    full_thickness = allocation->width;

  line_thickness       = get_line_thickness (table, full_thickness);
  line_spacing         = LINE_SPACING (table);
  item_spacing         = ITEM_SPACING (table);
  opposite_orientation = OPPOSITE_ORIENTATION (table);

  segment_lines_for_size (table, full_thickness, &segments);

  for (list = priv->children, line_offset = 0, i = 0; 
       i < priv->lines; 
       line_offset += line_thickness + line_spacing, i++)
    {
      for (j = 0, item_offset = 0; list && j < segments[i]; list = list->next)
	{
	  GtkWidget *child = list->data;
	  gint       child_size;

	  if (!gtk_widget_get_visible (child))
	    continue;

	  get_widget_size (child, priv->orientation,
			   line_thickness, NULL, &child_size);

	  allocate_child (table, child, item_offset, line_offset, child_size, line_thickness);

	  item_offset += child_size + item_spacing;

	  j++;
	}
    }

  g_free (segments);
}

/*****************************************************
 *                GtkContainerClass                  * 
 *****************************************************/
static void
gtk_spread_table_add (GtkContainer *container,
		      GtkWidget    *widget)
{
  gtk_spread_table_insert_child (GTK_SPREAD_TABLE (container), widget, -1);
}


static void
gtk_spread_table_remove (GtkContainer *container,
			 GtkWidget    *widget)
{
  GtkSpreadTable        *table = GTK_SPREAD_TABLE (container);
  GtkSpreadTablePrivate *priv = table->priv;
  GList                 *list;

  list = g_list_find (priv->children, widget);

  if (list)
    {
      gboolean was_visible = gtk_widget_get_visible (widget);

      gtk_widget_unparent (widget);

      priv->children = g_list_delete_link (priv->children, list);

      if (was_visible && gtk_widget_get_visible (GTK_WIDGET (container)))
        gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}


static void
gtk_spread_table_forall (GtkContainer *container,
			 gboolean      include_internals,
			 GtkCallback   callback,
			 gpointer      callback_data)
{
  GtkSpreadTable        *table = GTK_SPREAD_TABLE (container);
  GtkSpreadTablePrivate *priv = table->priv;
  GList                 *list;

  for (list = priv->children; list; list = list->next)
    (* callback) ((GtkWidget *)list->data, callback_data);
}

static GType
gtk_spread_table_child_type (GtkContainer   *container)
{
  return GTK_TYPE_WIDGET;
}

/*****************************************************
 *                       API                         * 
 *****************************************************/

/**
 * gtk_spread_table_new:
 * @orientation: The #GtkOrientation for the #GtkSpreadTable
 * @lines: The fixed amount of lines to distribute children to.
 *
 * Creates a #GtkSpreadTable.
 *
 * Returns: A new #GtkSpreadTable container
 */
GtkWidget *
gtk_spread_table_new (GtkOrientation orientation,
		      guint          lines)
{
  return (GtkWidget *)g_object_new (GTK_TYPE_SPREAD_TABLE,
				    "orientation", orientation,
				    "lines", lines,
				    NULL);
}

/**
 * gtk_spread_table_insert_child:
 * @spread_table: An #GtkSpreadTable
 * @widget: the child #GtkWidget to add
 * @index: the position in the child list to insert, specify -1 to append to the list.
 *
 * Adds a child to an #GtkSpreadTable with its packing options set
 */
void
gtk_spread_table_insert_child (GtkSpreadTable *table,
			       GtkWidget      *child,
			       gint            index)
{
  GtkSpreadTablePrivate *priv;
  GList               *list;

  g_return_if_fail (GTK_IS_SPREAD_TABLE (table));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = table->priv;

  list = g_list_find (priv->children, child);
  g_return_if_fail (list == NULL);

  priv->children = g_list_insert (priv->children, child, index);

  gtk_widget_set_parent (child, GTK_WIDGET (table));
}


/**
 * gtk_spread_table_set_lines:
 * @table: A #GtkSpreadTable
 * @lines: The amount of evenly allocated child segments.
 *
 * Sets the fixed amount of lines (rows or columns) to
 * distribute children to.
 */
void
gtk_spread_table_set_lines (GtkSpreadTable *table, 
			    guint           lines)
{
  GtkSpreadTablePrivate *priv;

  g_return_if_fail (GTK_IS_SPREAD_TABLE (table));

  priv = table->priv;

  if (priv->lines != lines)
    {
      priv->lines = lines;

      gtk_widget_queue_resize (GTK_WIDGET (table));

      g_object_notify (G_OBJECT (table), "lines");
    }
}

/**
 * gtk_spread_table_get_lines:
 * @table: An #GtkSpreadTable
 *
 * Gets the fixed amount of lines (rows or columns) to
 * distribute children to.
 *
 * Returns: The amount of lines.
 */
guint
gtk_spread_table_get_lines (GtkSpreadTable *table)
{
  g_return_val_if_fail (GTK_IS_SPREAD_TABLE (table), 0);

  return table->priv->lines;
}


/**
 * gtk_spread_table_set_vertical_spacing:
 * @table: An #GtkSpreadTable
 * @spacing: The spacing to use.
 *
 * Sets the vertical space to add between children.
 */
void
gtk_spread_table_set_vertical_spacing  (GtkSpreadTable  *table,
					guint            spacing)
{
  GtkSpreadTablePrivate *priv;

  g_return_if_fail (GTK_IS_SPREAD_TABLE (table));

  priv = table->priv;

  if (priv->vertical_spacing != spacing)
    {
      priv->vertical_spacing = spacing;

      gtk_widget_queue_resize (GTK_WIDGET (table));

      g_object_notify (G_OBJECT (table), "vertical-spacing");
    }
}

/**
 * gtk_spread_table_get_vertical_spacing:
 * @table: An #GtkSpreadTable
 *
 * Gets the vertical spacing.
 *
 * Returns: The vertical spacing.
 */
guint
gtk_spread_table_get_vertical_spacing  (GtkSpreadTable *table)
{
  g_return_val_if_fail (GTK_IS_SPREAD_TABLE (table), 0);

  return table->priv->vertical_spacing;
}

/**
 * gtk_spread_table_set_horizontal_spacing:
 * @table: A #GtkSpreadTable
 * @spacing: The spacing to use.
 *
 * Sets the horizontal space to add between children.
 */
void
gtk_spread_table_set_horizontal_spacing (GtkSpreadTable  *table,
					 guint            spacing)
{
  GtkSpreadTablePrivate *priv;

  g_return_if_fail (GTK_IS_SPREAD_TABLE (table));

  priv = table->priv;

  if (priv->horizontal_spacing != spacing)
    {
      priv->horizontal_spacing = spacing;

      gtk_widget_queue_resize (GTK_WIDGET (table));

      g_object_notify (G_OBJECT (table), "horizontal-spacing");
    }
}

/**
 * gtk_spread_table_get_horizontal_spacing:
 * @table: A #GtkSpreadTable
 *
 * Gets the horizontal spacing.
 *
 * Returns: The horizontal spacing.
 */
guint
gtk_spread_table_get_horizontal_spacing (GtkSpreadTable *table)
{
  g_return_val_if_fail (GTK_IS_SPREAD_TABLE (table), 0);

  return table->priv->horizontal_spacing;
}
