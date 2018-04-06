/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Red Hat, Inc.
 * Author: Matthias Clasen
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

#include <string.h>

#include "gtkgrid.h"

#include "gtkorientableprivate.h"
#include "gtksizerequest.h"
#include "gtkwidgetprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"


/**
 * SECTION:gtkgrid
 * @Short_description: Pack widgets in rows and columns
 * @Title: GtkGrid
 * @See_also: #GtkBox
 *
 * GtkGrid is a container which arranges its child widgets in
 * rows and columns. It is a very similar to #GtkBox,
 * but it consistently uses #GtkWidget’s #GtkWidget:margin and #GtkWidget:expand
 * properties instead of custom child properties, and it fully supports
 * [height-for-width geometry management][geometry-management].
 *
 * Children are added using gtk_grid_attach(). They can span multiple
 * rows or columns. It is also possible to add a child next to an
 * existing child, using gtk_grid_attach_next_to(). The behaviour of
 * GtkGrid when several children occupy the same grid cell is undefined.
 *
 * GtkGrid can be used like a #GtkBox by just using gtk_container_add(),
 * which will place children next to each other in the direction determined
 * by the #GtkOrientable:orientation property.
 *
 * # CSS nodes
 *
 * GtkGrid uses a single CSS node with name grid.
 */

typedef struct _GtkGridChild GtkGridChild;
typedef struct _GtkGridChildAttach GtkGridChildAttach;
typedef struct _GtkGridRowProperties GtkGridRowProperties;
typedef struct _GtkGridLine GtkGridLine;
typedef struct _GtkGridLines GtkGridLines;
typedef struct _GtkGridLineData GtkGridLineData;
typedef struct _GtkGridRequest GtkGridRequest;

struct _GtkGridChildAttach
{
  gint pos;
  gint span;
};

struct _GtkGridRowProperties
{
  gint row;
  GtkBaselinePosition baseline_position;
};

static const GtkGridRowProperties gtk_grid_row_properties_default = {
  0,
  GTK_BASELINE_POSITION_CENTER
};

struct _GtkGridChild
{
  GtkGridChildAttach attach[2];
};

#define CHILD_LEFT(child)    ((child)->attach[GTK_ORIENTATION_HORIZONTAL].pos)
#define CHILD_WIDTH(child)   ((child)->attach[GTK_ORIENTATION_HORIZONTAL].span)
#define CHILD_TOP(child)     ((child)->attach[GTK_ORIENTATION_VERTICAL].pos)
#define CHILD_HEIGHT(child)  ((child)->attach[GTK_ORIENTATION_VERTICAL].span)

/* A GtkGridLineData struct contains row/column specific parts
 * of the grid.
 */
struct _GtkGridLineData
{
  gint16 spacing;
  guint homogeneous : 1;
};

struct _GtkGridPrivate
{
  GList *row_properties;

  GtkOrientation orientation;
  gint baseline_row;

  GtkGridLineData linedata[2];
};
typedef struct _GtkGridPrivate GtkGridPrivate;

#define ROWS(priv)    (&(priv)->linedata[GTK_ORIENTATION_HORIZONTAL])
#define COLUMNS(priv) (&(priv)->linedata[GTK_ORIENTATION_VERTICAL])

static GQuark child_data_quark = 0;

/* A GtkGridLine struct represents a single row or column
 * during size requests
 */
struct _GtkGridLine
{
  gint minimum;
  gint natural;
  gint minimum_above;
  gint minimum_below;
  gint natural_above;
  gint natural_below;

  gint position;
  gint allocation;
  gint allocated_baseline;

  guint need_expand : 1;
  guint expand      : 1;
  guint empty       : 1;
};

struct _GtkGridLines
{
  GtkGridLine *lines;
  gint min, max;
};

struct _GtkGridRequest
{
  GtkGrid *grid;
  GtkGridLines lines[2];
};


enum
{
  PROP_0,
  PROP_ROW_SPACING,
  PROP_COLUMN_SPACING,
  PROP_ROW_HOMOGENEOUS,
  PROP_COLUMN_HOMOGENEOUS,
  PROP_BASELINE_ROW,
  N_PROPERTIES,
  PROP_ORIENTATION
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

enum
{
  CHILD_PROP_0,
  CHILD_PROP_LEFT_ATTACH,
  CHILD_PROP_TOP_ATTACH,
  CHILD_PROP_WIDTH,
  CHILD_PROP_HEIGHT,
  N_CHILD_PROPERTIES
};

static GParamSpec *child_properties[N_CHILD_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_CODE (GtkGrid, gtk_grid, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkGrid)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))


static void gtk_grid_row_properties_free (GtkGridRowProperties *props);

static void
gtk_grid_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GtkGrid *grid = GTK_GRID (object);
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;

    case PROP_ROW_SPACING:
      g_value_set_int (value, COLUMNS (priv)->spacing);
      break;

    case PROP_COLUMN_SPACING:
      g_value_set_int (value, ROWS (priv)->spacing);
      break;

    case PROP_ROW_HOMOGENEOUS:
      g_value_set_boolean (value, COLUMNS (priv)->homogeneous);
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      g_value_set_boolean (value, ROWS (priv)->homogeneous);
      break;

    case PROP_BASELINE_ROW:
      g_value_set_int (value, priv->baseline_row);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_set_orientation (GtkGrid        *grid,
                          GtkOrientation  orientation)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  if (priv->orientation != orientation)
    {
      priv->orientation = orientation;
      _gtk_orientable_set_style_classes (GTK_ORIENTABLE (grid));

      g_object_notify (G_OBJECT (grid), "orientation");
    }
}

static void
gtk_grid_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GtkGrid *grid = GTK_GRID (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_grid_set_orientation (grid, g_value_get_enum (value));
      break;

    case PROP_ROW_SPACING:
      gtk_grid_set_row_spacing (grid, g_value_get_int (value));
      break;

    case PROP_COLUMN_SPACING:
      gtk_grid_set_column_spacing (grid, g_value_get_int (value));
      break;

    case PROP_ROW_HOMOGENEOUS:
      gtk_grid_set_row_homogeneous (grid, g_value_get_boolean (value));
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      gtk_grid_set_column_homogeneous (grid, g_value_get_boolean (value));
      break;

    case PROP_BASELINE_ROW:
      gtk_grid_set_baseline_row (grid, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GtkGridChild *
get_grid_child (GtkWidget *widget)
{
  return (GtkGridChild *) g_object_get_qdata ((GObject *)widget, child_data_quark);
}

static void
gtk_grid_get_child_property (GtkContainer *container,
                             GtkWidget    *child,
                             guint         property_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  GtkGridChild *grid_child;

  grid_child = get_grid_child (child);

  if (grid_child == NULL)
    {
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      return;
    }

  switch (property_id)
    {
    case CHILD_PROP_LEFT_ATTACH:
      g_value_set_int (value, CHILD_LEFT (grid_child));
      break;

    case CHILD_PROP_TOP_ATTACH:
      g_value_set_int (value, CHILD_TOP (grid_child));
      break;

    case CHILD_PROP_WIDTH:
      g_value_set_int (value, CHILD_WIDTH (grid_child));
      break;

    case CHILD_PROP_HEIGHT:
      g_value_set_int (value, CHILD_HEIGHT (grid_child));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_grid_set_child_property (GtkContainer *container,
                             GtkWidget    *child,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkGrid *grid = GTK_GRID (container);
  GtkGridChild *grid_child;

  grid_child = get_grid_child (child);

  if (grid_child == NULL)
    {
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      return;
    }

  switch (property_id)
    {
    case CHILD_PROP_LEFT_ATTACH:
      CHILD_LEFT (grid_child) = g_value_get_int (value);
      break;

    case CHILD_PROP_TOP_ATTACH:
      CHILD_TOP (grid_child) = g_value_get_int (value);
      break;

   case CHILD_PROP_WIDTH:
      CHILD_WIDTH (grid_child) = g_value_get_int (value);
      break;

    case CHILD_PROP_HEIGHT:
      CHILD_HEIGHT (grid_child) = g_value_get_int (value);
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }

  if (_gtk_widget_get_visible (child) &&
      _gtk_widget_get_visible (GTK_WIDGET (grid)))
    gtk_widget_queue_resize (child);
}

static void
gtk_grid_finalize (GObject *object)
{
  GtkGrid *grid = GTK_GRID (object);
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_list_free_full (priv->row_properties, (GDestroyNotify)gtk_grid_row_properties_free);

  G_OBJECT_CLASS (gtk_grid_parent_class)->finalize (object);
}

static void
grid_attach (GtkGrid   *grid,
             GtkWidget *widget,
             gint       left,
             gint       top,
             gint       width,
             gint       height)
{
  GtkGridChild *child;

  child = g_new (GtkGridChild, 1);
  CHILD_LEFT (child) = left;
  CHILD_TOP (child) = top;
  CHILD_WIDTH (child) = width;
  CHILD_HEIGHT (child) = height;

  g_object_set_qdata_full (G_OBJECT (widget), child_data_quark, child, g_free);

  gtk_widget_set_parent (widget, GTK_WIDGET (grid));
}

/* Find the position 'touching' existing
 * children. @orientation and @max determine
 * from which direction to approach (horizontal
 * + max = right, vertical + !max = top, etc).
 * @op_pos, @op_span determine the rows/columns
 * in which the touching has to happen.
 */
static gint
find_attach_position (GtkGrid         *grid,
                      GtkOrientation   orientation,
                      gint             op_pos,
                      gint             op_span,
                      gboolean         max)
{
  GtkWidget *child;
  gint pos;
  gboolean hit;

  if (max)
    pos = -G_MAXINT;
  else
    pos = G_MAXINT;

  hit = FALSE;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      const GtkGridChild *grid_child = get_grid_child (child);
      const GtkGridChildAttach *attach = &grid_child->attach[orientation];
      const GtkGridChildAttach *opposite = &grid_child->attach[1 - orientation];

      /* check if the ranges overlap */
      if (opposite->pos <= op_pos + op_span && op_pos <= opposite->pos + opposite->span)
        {
          hit = TRUE;

          if (max)
            pos = MAX (pos, attach->pos + attach->span);
          else
            pos = MIN (pos, attach->pos);
        }
     }

  if (!hit)
    pos = 0;

  return pos;
}

static void
gtk_grid_add (GtkContainer *container,
              GtkWidget    *child)
{
  GtkGrid *grid = GTK_GRID (container);
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  gint pos[2] = { 0, 0 };

  pos[priv->orientation] = find_attach_position (grid, priv->orientation, 0, 1, TRUE);
  grid_attach (grid, child, pos[0], pos[1], 1, 1);
}

static void
gtk_grid_remove (GtkContainer *container,
                 GtkWidget    *child)
{
  GtkGrid *grid = GTK_GRID (container);
  gboolean was_visible;

  was_visible = _gtk_widget_get_visible (child);
  gtk_widget_unparent (child);

  if (was_visible && _gtk_widget_get_visible (GTK_WIDGET (grid)))
    gtk_widget_queue_resize (GTK_WIDGET (grid));
}

static void
gtk_grid_forall (GtkContainer *container,
                 GtkCallback   callback,
                 gpointer      callback_data)
{
  GtkWidget *child;

  child = gtk_widget_get_first_child (GTK_WIDGET (container));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      (* callback) (child, callback_data);

      child = next;
    }
}

static GType
gtk_grid_child_type (GtkContainer *container)
{
  return GTK_TYPE_WIDGET;
}

static int
get_spacing (GtkGrid        *grid,
             GtkOrientation  orientation)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkCssValue *border_spacing;
  gint css_spacing;

  border_spacing = _gtk_style_context_peek_property (gtk_widget_get_style_context (GTK_WIDGET (grid)), GTK_CSS_PROPERTY_BORDER_SPACING);
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    css_spacing = _gtk_css_position_value_get_x (border_spacing, 100);
  else
    css_spacing = _gtk_css_position_value_get_y (border_spacing, 100);

  return css_spacing + priv->linedata[orientation].spacing;
}

/* Calculates the min and max numbers for both orientations.
 */
static void
gtk_grid_request_count_lines (GtkGridRequest *request)
{
  GtkWidget *child;
  gint min[2];
  gint max[2];

  min[0] = min[1] = G_MAXINT;
  max[0] = max[1] = G_MININT;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (request->grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridChild *grid_child = get_grid_child (child);
      GtkGridChildAttach *attach = grid_child->attach;

      min[0] = MIN (min[0], attach[0].pos);
      max[0] = MAX (max[0], attach[0].pos + attach[0].span);
      min[1] = MIN (min[1], attach[1].pos);
      max[1] = MAX (max[1], attach[1].pos + attach[1].span);
    }

  request->lines[0].min = min[0];
  request->lines[0].max = max[0];
  request->lines[1].min = min[1];
  request->lines[1].max = max[1];
}

/* Sets line sizes to 0 and marks lines as expand
 * if they have a non-spanning expanding child.
 */
static void
gtk_grid_request_init (GtkGridRequest *request,
                       GtkOrientation  orientation)
{
  GtkWidget *child;
  GtkGridChildAttach *attach;
  GtkGridLines *lines;
  gint i;

  lines = &request->lines[orientation];

  for (i = 0; i < lines->max - lines->min; i++)
    {
      lines->lines[i].minimum = 0;
      lines->lines[i].natural = 0;
      lines->lines[i].minimum_above = -1;
      lines->lines[i].minimum_below = -1;
      lines->lines[i].natural_above = -1;
      lines->lines[i].natural_below = -1;
      lines->lines[i].expand = FALSE;
      lines->lines[i].empty = TRUE;
    }


  for (child = gtk_widget_get_first_child (GTK_WIDGET (request->grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridChild *grid_child = get_grid_child (child);

      attach = &grid_child->attach[orientation];
      if (attach->span == 1 && gtk_widget_compute_expand (child, orientation))
        lines->lines[attach->pos - lines->min].expand = TRUE;
    }
}

/* Sums allocations for lines spanned by child and their spacing.
 */
static gint
compute_allocation_for_child (GtkGridRequest *request,
                              GtkGridChild   *child,
                              GtkOrientation  orientation)
{
  GtkGridLines *lines;
  GtkGridLine *line;
  GtkGridChildAttach *attach;
  gint size;
  gint i;

  lines = &request->lines[orientation];
  attach = &child->attach[orientation];

  size = (attach->span - 1) * get_spacing (request->grid, orientation);
  for (i = 0; i < attach->span; i++)
    {
      line = &lines->lines[attach->pos - lines->min + i];
      size += line->allocation;
    }

  return size;
}

static void
compute_request_for_child (GtkGridRequest *request,
                           GtkWidget      *child,
                           GtkGridChild   *grid_child,
                           GtkOrientation  orientation,
                           gboolean        contextual,
                           gint           *minimum,
                           gint           *natural,
			   gint           *minimum_baseline,
                           gint           *natural_baseline)
{
  if (minimum_baseline)
    *minimum_baseline = -1;
  if (natural_baseline)
    *natural_baseline = -1;

  if (contextual)
    {
      gint size;

      size = compute_allocation_for_child (request, grid_child, 1 - orientation);

      gtk_widget_measure (child,
                          orientation,
                          size,
                          minimum, natural,
                          minimum_baseline, natural_baseline);
    }
  else
    {
      gtk_widget_measure (child,
                          orientation,
                          -1,
                          minimum, natural,
                          minimum_baseline, natural_baseline);
    }
}

/* Sets requisition to max. of non-spanning children.
 * If contextual is TRUE, requires allocations of
 * lines in the opposite orientation to be set.
 */
static void
gtk_grid_request_non_spanning (GtkGridRequest *request,
                               GtkOrientation  orientation,
                               gboolean        contextual)
{
  GtkWidget *child;
  GtkGridChildAttach *attach;
  GtkGridLines *lines;
  GtkGridLine *line;
  gint i;
  GtkBaselinePosition baseline_pos;
  gint minimum, minimum_baseline;
  gint natural, natural_baseline;

  lines = &request->lines[orientation];

  for (child = gtk_widget_get_first_child (GTK_WIDGET (request->grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridChild *grid_child = get_grid_child (child);

      if (!_gtk_widget_get_visible (child))
        continue;

      attach = &grid_child->attach[orientation];
      if (attach->span != 1)
        continue;

      compute_request_for_child (request, child, grid_child, orientation, contextual, &minimum, &natural, &minimum_baseline, &natural_baseline);

      line = &lines->lines[attach->pos - lines->min];

      if (minimum_baseline != -1)
	{
	  line->minimum_above = MAX (line->minimum_above, minimum_baseline);
	  line->minimum_below = MAX (line->minimum_below, minimum - minimum_baseline);
	  line->natural_above = MAX (line->natural_above, natural_baseline);
	  line->natural_below = MAX (line->natural_below, natural - natural_baseline);
	}
      else
	{
	  line->minimum = MAX (line->minimum, minimum);
	  line->natural = MAX (line->natural, natural);
	}
    }

  for (i = 0; i < lines->max - lines->min; i++)
    {
      line = &lines->lines[i];

      if (line->minimum_above != -1)
	{
	  line->minimum = MAX (line->minimum, line->minimum_above + line->minimum_below);
	  line->natural = MAX (line->natural, line->natural_above + line->natural_below);

	  baseline_pos = gtk_grid_get_row_baseline_position (request->grid, i + lines->min);

	  switch (baseline_pos)
	    {
	    case GTK_BASELINE_POSITION_TOP:
	      line->minimum_above += 0;
	      line->minimum_below += line->minimum - (line->minimum_above + line->minimum_below);
	      line->natural_above += 0;
	      line->natural_below += line->natural - (line->natural_above + line->natural_below);
	      break;
	    case GTK_BASELINE_POSITION_CENTER:
	      line->minimum_above += (line->minimum - (line->minimum_above + line->minimum_below))/2;
	      line->minimum_below += (line->minimum - (line->minimum_above + line->minimum_below))/2;
	      line->natural_above += (line->natural - (line->natural_above + line->natural_below))/2;
	      line->natural_below += (line->natural - (line->natural_above + line->natural_below))/2;
	      break;
	    case GTK_BASELINE_POSITION_BOTTOM:
	      line->minimum_above += line->minimum - (line->minimum_above + line->minimum_below);
	      line->minimum_below += 0;
	      line->natural_above += line->natural - (line->natural_above + line->natural_below);
	      line->natural_below += 0;
	      break;
            default:
              break;
	    }
	}
    }
}

/* Enforce homogeneous sizes.
 */
static void
gtk_grid_request_homogeneous (GtkGridRequest *request,
                              GtkOrientation  orientation)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (request->grid);
  GtkGridLineData *linedata;
  GtkGridLines *lines;
  gint minimum, natural;
  gint i;

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];

  if (!linedata->homogeneous)
    return;

  minimum = 0;
  natural = 0;

  for (i = 0; i < lines->max - lines->min; i++)
    {
      minimum = MAX (minimum, lines->lines[i].minimum);
      natural = MAX (natural, lines->lines[i].natural);
    }

  for (i = 0; i < lines->max - lines->min; i++)
    {
      lines->lines[i].minimum = minimum;
      lines->lines[i].natural = natural;
      /* TODO: Do we want to adjust the baseline here too?
       * And if so, also in the homogenous resize.
       */
    }
}

/* Deals with spanning children.
 * Requires expand fields of lines to be set for
 * non-spanning children.
 */
static void
gtk_grid_request_spanning (GtkGridRequest *request,
                           GtkOrientation  orientation,
                           gboolean        contextual)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (request->grid);
  GtkWidget *child;
  GtkGridChildAttach *attach;
  GtkGridLineData *linedata;
  GtkGridLines *lines;
  GtkGridLine *line;
  gint minimum;
  gint natural;
  gint span_minimum;
  gint span_natural;
  gint span_expand;
  gboolean force_expand;
  gint spacing;
  gint extra;
  gint expand;
  gint line_extra;
  gint i;

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];
  spacing = get_spacing (request->grid, orientation);

  for (child = gtk_widget_get_first_child (GTK_WIDGET (request->grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridChild *grid_child = get_grid_child (child);

      if (!_gtk_widget_get_visible (child))
        continue;

      attach = &grid_child->attach[orientation];
      if (attach->span == 1)
        continue;

      /* We ignore baselines for spanning children */
      compute_request_for_child (request, child, grid_child, orientation, contextual, &minimum, &natural, NULL, NULL);

      span_minimum = (attach->span - 1) * spacing;
      span_natural = (attach->span - 1) * spacing;
      span_expand = 0;
      force_expand = FALSE;
      for (i = 0; i < attach->span; i++)
        {
          line = &lines->lines[attach->pos - lines->min + i];
          span_minimum += line->minimum;
          span_natural += line->natural;
          if (line->expand)
            span_expand += 1;
        }
      if (span_expand == 0)
        {
          span_expand = attach->span;
          force_expand = TRUE;
        }

      /* If we need to request more space for this child to fill
       * its requisition, then divide up the needed space amongst the
       * lines it spans, favoring expandable lines if any.
       *
       * When doing homogeneous allocation though, try to keep the
       * line allocations even, since we're going to force them to
       * be the same anyway, and we don't want to introduce unnecessary
       * extra space.
       */
      if (span_minimum < minimum)
        {
          if (linedata->homogeneous)
            {
              gint total, m;

              total = minimum - (attach->span - 1) * spacing;
              m = total / attach->span + (total % attach->span ? 1 : 0);
              for (i = 0; i < attach->span; i++)
                {
                  line = &lines->lines[attach->pos - lines->min + i];
                  line->minimum = MAX(line->minimum, m);
                }
            }
          else
            {
              extra = minimum - span_minimum;
              expand = span_expand;
              for (i = 0; i < attach->span; i++)
                {
                  line = &lines->lines[attach->pos - lines->min + i];
                  if (force_expand || line->expand)
                    {
                      line_extra = extra / expand;
                      line->minimum += line_extra;
                      extra -= line_extra;
                      expand -= 1;
                    }
                }
            }
        }

      if (span_natural < natural)
        {
          if (linedata->homogeneous)
            {
              gint total, n;

              total = natural - (attach->span - 1) * spacing;
              n = total / attach->span + (total % attach->span ? 1 : 0);
              for (i = 0; i < attach->span; i++)
                {
                  line = &lines->lines[attach->pos - lines->min + i];
                  line->natural = MAX(line->natural, n);
                }
            }
          else
            {
              extra = natural - span_natural;
              expand = span_expand;
              for (i = 0; i < attach->span; i++)
                {
                  line = &lines->lines[attach->pos - lines->min + i];
                  if (force_expand || line->expand)
                    {
                      line_extra = extra / expand;
                      line->natural += line_extra;
                      extra -= line_extra;
                      expand -= 1;
                    }
                }
            }
        }
    }
}

/* Marks empty and expanding lines and counts them.
 */
static void
gtk_grid_request_compute_expand (GtkGridRequest *request,
                                 GtkOrientation  orientation,
				 gint            min,
				 gint            max,
                                 gint           *nonempty_lines,
                                 gint           *expand_lines)
{
  GtkWidget *child;
  GtkGridChildAttach *attach;
  gint i;
  GtkGridLines *lines;
  GtkGridLine *line;
  gboolean has_expand;
  gint expand;
  gint empty;

  lines = &request->lines[orientation];

  min = MAX (min, lines->min);
  max = MIN (max, lines->max);

  for (i = min - lines->min; i < max - lines->min; i++)
    {
      lines->lines[i].need_expand = FALSE;
      lines->lines[i].expand = FALSE;
      lines->lines[i].empty = TRUE;
    }

  for (child = gtk_widget_get_first_child (GTK_WIDGET (request->grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridChild *grid_child = get_grid_child (child);

      if (!_gtk_widget_get_visible (child))
        continue;

      attach = &grid_child->attach[orientation];
      if (attach->span != 1)
        continue;

      if (attach->pos >= max || attach->pos < min)
	continue;

      line = &lines->lines[attach->pos - lines->min];
      line->empty = FALSE;
      if (gtk_widget_compute_expand (child, orientation))
        line->expand = TRUE;
    }

  for (child = gtk_widget_get_first_child (GTK_WIDGET (request->grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridChild *grid_child = get_grid_child (child);

      if (!_gtk_widget_get_visible (child))
        continue;

      attach = &grid_child->attach[orientation];
      if (attach->span == 1)
        continue;

      has_expand = FALSE;
      for (i = 0; i < attach->span; i++)
        {
          line = &lines->lines[attach->pos - lines->min + i];

          if (line->expand)
            has_expand = TRUE;

	  if (attach->pos + i >= max || attach->pos + 1 < min)
	    continue;

          line->empty = FALSE;
        }

      if (!has_expand && gtk_widget_compute_expand (child, orientation))
        {
          for (i = 0; i < attach->span; i++)
            {
	      if (attach->pos + i >= max || attach->pos + 1 < min)
		continue;

              line = &lines->lines[attach->pos - lines->min + i];
              line->need_expand = TRUE;
            }
        }
    }

  empty = 0;
  expand = 0;
  for (i = min - lines->min; i < max - lines->min; i++)
    {
      line = &lines->lines[i];

      if (line->need_expand)
        line->expand = TRUE;

      if (line->empty)
        empty += 1;

      if (line->expand)
        expand += 1;
    }

  if (nonempty_lines)
    *nonempty_lines = max - min - empty;

  if (expand_lines)
    *expand_lines = expand;
}

/* Sums the minimum and natural fields of lines and their spacing.
 */
static void
gtk_grid_request_sum (GtkGridRequest *request,
                      GtkOrientation  orientation,
                      gint           *minimum,
                      gint           *natural,
		      gint           *minimum_baseline,
		      gint           *natural_baseline)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (request->grid);
  GtkGridLines *lines;
  gint i;
  gint min, nat;
  gint nonempty;
  gint spacing;

  gtk_grid_request_compute_expand (request, orientation, G_MININT, G_MAXINT, &nonempty, NULL);

  lines = &request->lines[orientation];
  spacing = get_spacing (request->grid, orientation);

  min = 0;
  nat = 0;
  for (i = 0; i < lines->max - lines->min; i++)
    {
      if (orientation == GTK_ORIENTATION_VERTICAL &&
	  lines->min + i == priv->baseline_row &&
	  lines->lines[i].minimum_above != -1)
	{
	  if (minimum_baseline)
	    *minimum_baseline = min + lines->lines[i].minimum_above;
	  if (natural_baseline)
	    *natural_baseline = nat + lines->lines[i].natural_above;
	}

      min += lines->lines[i].minimum;
      nat += lines->lines[i].natural;

      if (!lines->lines[i].empty)
	{
	  min += spacing;
	  nat += spacing;
	}
    }

  /* Remove last spacing, if any was applied */
  if (nonempty > 0)
    {
      min -= spacing;
      nat -= spacing;
    }

  *minimum = min;
  *natural = nat;
}

/* Computes minimum and natural fields of lines.
 * When contextual is TRUE, requires allocation of
 * lines in the opposite orientation to be set.
 */
static void
gtk_grid_request_run (GtkGridRequest *request,
                      GtkOrientation  orientation,
                      gboolean        contextual)
{
  gtk_grid_request_init (request, orientation);
  gtk_grid_request_non_spanning (request, orientation, contextual);
  gtk_grid_request_homogeneous (request, orientation);
  gtk_grid_request_spanning (request, orientation, contextual);
  gtk_grid_request_homogeneous (request, orientation);
}

static void
gtk_grid_distribute_non_homogeneous (GtkGridLines *lines,
				     gint nonempty,
				     gint expand,
				     gint size,
				     gint min,
				     gint max)
{
  GtkRequestedSize *sizes;
  GtkGridLine *line;
  gint extra;
  gint rest;
  int i, j;

  if (nonempty == 0)
    return;

  sizes = g_newa (GtkRequestedSize, nonempty);

  j = 0;
  for (i = min - lines->min; i < max - lines->min; i++)
    {
      line = &lines->lines[i];
      if (line->empty)
	continue;

      size -= line->minimum;

      sizes[j].minimum_size = line->minimum;
      sizes[j].natural_size = line->natural;
      sizes[j].data = line;
      j++;
    }

  size = gtk_distribute_natural_allocation (MAX (0, size), nonempty, sizes);

  if (expand > 0)
    {
      extra = size / expand;
      rest = size % expand;
    }
  else
    {
      extra = 0;
      rest = 0;
    }

  j = 0;
  for (i = min - lines->min; i < max - lines->min; i++)
    {
      line = &lines->lines[i];
      if (line->empty)
	continue;

      g_assert (line == sizes[j].data);

      line->allocation = sizes[j].minimum_size;
      if (line->expand)
	{
	  line->allocation += extra;
	  if (rest > 0)
	    {
	      line->allocation += 1;
	      rest -= 1;
	    }
	}

      j++;
    }
}

/* Requires that the minimum and natural fields of lines
 * have been set, computes the allocation field of lines
 * by distributing total_size among lines.
 */
static void
gtk_grid_request_allocate (GtkGridRequest *request,
                           GtkOrientation  orientation,
                           gint            total_size)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (request->grid);
  GtkGridLineData *linedata;
  GtkGridLines *lines;
  GtkGridLine *line;
  gint nonempty1, nonempty2;
  gint expand1, expand2;
  gint i;
  GtkBaselinePosition baseline_pos;
  gint baseline;
  gint extra, extra2;
  gint rest;
  gint size1, size2;
  gint split, split_pos;
  gint spacing;

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];
  spacing = get_spacing (request->grid, orientation);

  baseline = gtk_widget_get_allocated_baseline (GTK_WIDGET (request->grid));

  if (orientation == GTK_ORIENTATION_VERTICAL && baseline != -1 &&
      priv->baseline_row >= lines->min && priv->baseline_row < lines->max &&
      lines->lines[priv->baseline_row - lines->min].minimum_above != -1)
    {
      split = priv->baseline_row;
      split_pos = baseline - lines->lines[priv->baseline_row - lines->min].minimum_above;
      gtk_grid_request_compute_expand (request, orientation, lines->min, split, &nonempty1, &expand1);
      gtk_grid_request_compute_expand (request, orientation, split, lines->max, &nonempty2, &expand2);

      if (nonempty2 > 0)
	{
	  size1 = split_pos - (nonempty1) * spacing;
	  size2 = (total_size - split_pos) - (nonempty2 - 1) * spacing;
	}
      else
	{
	  size1 = total_size - (nonempty1 - 1) * spacing;
	  size2 = 0;
	}
    }
  else
    {
      gtk_grid_request_compute_expand (request, orientation, lines->min, lines->max, &nonempty1, &expand1);
      nonempty2 = expand2 = 0;
      split = lines->max;

      size1 = total_size - (nonempty1 - 1) * spacing;
      size2 = 0;
    }

  if (nonempty1 == 0 && nonempty2 == 0)
    return;

  if (linedata->homogeneous)
    {
      if (nonempty1 > 0)
	{
	  extra = size1 / nonempty1;
	  rest = size1 % nonempty1;
	}
      else
	{
	  extra = 0;
	  rest = 0;
	}
      if (nonempty2 > 0)
	{
	  extra2 = size2 / nonempty2;
	  if (extra2 < extra || nonempty1 == 0)
	    {
	      extra = extra2;
	      rest = size2 % nonempty2;
	    }
	}

      for (i = 0; i < lines->max - lines->min; i++)
        {
          line = &lines->lines[i];
          if (line->empty)
            continue;

          line->allocation = extra;
          if (rest > 0)
            {
              line->allocation += 1;
              rest -= 1;
            }
        }
    }
  else
    {
      gtk_grid_distribute_non_homogeneous (lines,
					   nonempty1,
					   expand1,
					   size1,
					   lines->min,
					   split);
      gtk_grid_distribute_non_homogeneous (lines,
					   nonempty2,
					   expand2,
					   size2,
					   split,
					   lines->max);
    }

  for (i = 0; i < lines->max - lines->min; i++)
    {
      line = &lines->lines[i];
      if (line->empty)
	continue;

      if (line->minimum_above != -1)
	{
	  /* Note: This is overridden in gtk_grid_request_position for the allocated baseline */
	  baseline_pos = gtk_grid_get_row_baseline_position (request->grid, i + lines->min);

	  switch (baseline_pos)
	    {
	    case GTK_BASELINE_POSITION_TOP:
	      line->allocated_baseline =
		line->minimum_above;
	      break;
	    case GTK_BASELINE_POSITION_CENTER:
	      line->allocated_baseline =
		line->minimum_above +
		(line->allocation - (line->minimum_above + line->minimum_below)) / 2;
	      break;
	    case GTK_BASELINE_POSITION_BOTTOM:
	      line->allocated_baseline =
		line->allocation - line->minimum_below;
	      break;
            default:
              break;
	    }
	}
      else
	line->allocated_baseline = -1;
    }
}

/* Computes the position fields from allocation and spacing.
 */
static void
gtk_grid_request_position (GtkGridRequest *request,
                           GtkOrientation  orientation)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (request->grid);
  GtkGridLines *lines;
  GtkGridLine *line;
  gint position, old_position;
  int allocated_baseline;
  gint spacing;
  gint i, j;

  lines = &request->lines[orientation];
  spacing = get_spacing (request->grid, orientation);

  allocated_baseline = gtk_widget_get_allocated_baseline (GTK_WIDGET(request->grid));

  position = 0;
  for (i = 0; i < lines->max - lines->min; i++)
    {
      line = &lines->lines[i];

      if (orientation == GTK_ORIENTATION_VERTICAL &&
	  i + lines->min == priv->baseline_row &&
	  allocated_baseline != -1 &&
	  lines->lines[i].minimum_above != -1)
	{
	  old_position = position;
	  position = allocated_baseline - line->minimum_above;

	  /* Back-patch previous rows */
	  for (j = 0; j < i; j++)
	    {
	      if (!lines->lines[j].empty)
		lines->lines[j].position += position - old_position;
	    }
	}

      if (!line->empty)
        {
          line->position = position;
          position += line->allocation + spacing;

	  if (orientation == GTK_ORIENTATION_VERTICAL &&
	      i + lines->min == priv->baseline_row &&
	      allocated_baseline != -1 &&
	      lines->lines[i].minimum_above != -1)
	    line->allocated_baseline = allocated_baseline - line->position;
        }
    }
}

static void
gtk_grid_get_size (GtkGrid        *grid,
                   GtkOrientation  orientation,
                   gint           *minimum,
                   gint           *natural,
		   gint           *minimum_baseline,
		   gint           *natural_baseline)
{
  GtkGridRequest request;
  GtkGridLines *lines;

  *minimum = 0;
  *natural = 0;

  if (minimum_baseline)
    *minimum_baseline = -1;

  if (natural_baseline)
    *natural_baseline = -1;

  if (gtk_widget_get_first_child (GTK_WIDGET (grid)) == NULL)
    return;

  request.grid = grid;
  gtk_grid_request_count_lines (&request);
  lines = &request.lines[orientation];
  lines->lines = g_newa (GtkGridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (GtkGridLine));

  gtk_grid_request_run (&request, orientation, FALSE);
  gtk_grid_request_sum (&request, orientation, minimum, natural,
			minimum_baseline, natural_baseline);
}

static void
gtk_grid_get_size_for_size (GtkGrid        *grid,
                            GtkOrientation  orientation,
                            gint            size,
                            gint           *minimum,
                            gint           *natural,
			    gint           *minimum_baseline,
                            gint           *natural_baseline)
{
  GtkGridRequest request;
  GtkGridLines *lines;
  gint min_size, nat_size;

  *minimum = 0;
  *natural = 0;

  if (minimum_baseline)
    *minimum_baseline = -1;

  if (natural_baseline)
    *natural_baseline = -1;

  if (gtk_widget_get_first_child (GTK_WIDGET (grid)) == NULL)
    return;

  request.grid = grid;
  gtk_grid_request_count_lines (&request);
  lines = &request.lines[0];
  lines->lines = g_newa (GtkGridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (GtkGridLine));
  lines = &request.lines[1];
  lines->lines = g_newa (GtkGridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (GtkGridLine));

  gtk_grid_request_run (&request, 1 - orientation, FALSE);
  gtk_grid_request_sum (&request, 1 - orientation, &min_size, &nat_size, NULL, NULL);
  gtk_grid_request_allocate (&request, 1 - orientation, MAX (size, min_size));

  gtk_grid_request_run (&request, orientation, TRUE);
  gtk_grid_request_sum (&request, orientation, minimum, natural, minimum_baseline, natural_baseline);
}

static void
gtk_grid_measure (GtkWidget     *widget,
                  GtkOrientation  orientation,
                  int             for_size,
                  int            *minimum,
                  int            *natural,
                  int            *minimum_baseline,
                  int            *natural_baseline)
{
  GtkGrid *grid = GTK_GRID (widget);

  if ((orientation == GTK_ORIENTATION_HORIZONTAL &&
       gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT) ||
      (orientation == GTK_ORIENTATION_VERTICAL &&
       gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH))
    gtk_grid_get_size_for_size (grid, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
  else
    gtk_grid_get_size (grid, orientation, minimum, natural, minimum_baseline, natural_baseline);
}

static void
allocate_child (GtkGridRequest *request,
                GtkOrientation  orientation,
                GtkWidget      *child,
                GtkGridChild   *grid_child,
                gint           *position,
                gint           *size,
		gint           *baseline)
{
  GtkGridLines *lines;
  GtkGridLine *line;
  GtkGridChildAttach *attach;
  gint i;

  lines = &request->lines[orientation];
  attach = &grid_child->attach[orientation];

  *position = lines->lines[attach->pos - lines->min].position;
  if (attach->span == 1 && gtk_widget_get_valign (child) == GTK_ALIGN_BASELINE)
    *baseline = lines->lines[attach->pos - lines->min].allocated_baseline;
  else
    *baseline = -1;

  *size = (attach->span - 1) * get_spacing (request->grid, orientation);
  for (i = 0; i < attach->span; i++)
    {
      line = &lines->lines[attach->pos - lines->min + i];
      *size += line->allocation;
    }
}

static void
gtk_grid_request_allocate_children (GtkGridRequest      *request,
                                    const GtkAllocation *allocation)
{
  GtkWidget *child;
  GtkAllocation child_allocation;
  gint x, y, width, height, baseline, ignore;


  for (child = gtk_widget_get_first_child (GTK_WIDGET (request->grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridChild *grid_child = get_grid_child (child);

      if (!_gtk_widget_get_visible (child))
        continue;

      allocate_child (request, GTK_ORIENTATION_HORIZONTAL, child, grid_child, &x, &width, &ignore);
      allocate_child (request, GTK_ORIENTATION_VERTICAL, child, grid_child, &y, &height, &baseline);

      child_allocation.x = x;
      child_allocation.y = y;
      child_allocation.width = width;
      child_allocation.height = height;

      if (_gtk_widget_get_direction (GTK_WIDGET (request->grid)) == GTK_TEXT_DIR_RTL)
        child_allocation.x = allocation->x + allocation->width
                             - (child_allocation.x - allocation->x) - child_allocation.width;

      gtk_widget_size_allocate (child, &child_allocation, baseline);
    }
}

#define GET_SIZE(allocation, orientation) (orientation == GTK_ORIENTATION_HORIZONTAL ? allocation->width : allocation->height)

static void
gtk_grid_size_allocate (GtkWidget          *widget,
                        const GtkAllocation *allocation,
                        int                  baseline)
{
  GtkGrid *grid = GTK_GRID (widget);
  GtkGridRequest request;
  GtkGridLines *lines;
  GtkOrientation orientation;

  if (gtk_widget_get_first_child (widget) == NULL)
    return;

  request.grid = grid;

  gtk_grid_request_count_lines (&request);
  lines = &request.lines[0];
  lines->lines = g_newa (GtkGridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (GtkGridLine));
  lines = &request.lines[1];
  lines->lines = g_newa (GtkGridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (GtkGridLine));

  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT)
    orientation = GTK_ORIENTATION_HORIZONTAL;
  else
    orientation = GTK_ORIENTATION_VERTICAL;

  gtk_grid_request_run (&request, 1 - orientation, FALSE);
  gtk_grid_request_allocate (&request, 1 - orientation, GET_SIZE (allocation, 1 - orientation));
  gtk_grid_request_run (&request, orientation, TRUE);

  gtk_grid_request_allocate (&request, orientation, GET_SIZE (allocation, orientation));

  gtk_grid_request_position (&request, 0);
  gtk_grid_request_position (&request, 1);

  gtk_grid_request_allocate_children (&request, allocation);
}

static void
gtk_grid_class_init (GtkGridClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->get_property = gtk_grid_get_property;
  object_class->set_property = gtk_grid_set_property;
  object_class->finalize = gtk_grid_finalize;

  widget_class->size_allocate = gtk_grid_size_allocate;
  widget_class->measure = gtk_grid_measure;

  container_class->add = gtk_grid_add;
  container_class->remove = gtk_grid_remove;
  container_class->forall = gtk_grid_forall;
  container_class->child_type = gtk_grid_child_type;
  container_class->set_child_property = gtk_grid_set_child_property;
  container_class->get_child_property = gtk_grid_get_child_property;

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  obj_properties[PROP_ROW_SPACING] =
    g_param_spec_int ("row-spacing",
                      P_("Row spacing"),
                      P_("The amount of space between two consecutive rows"),
                      0, G_MAXINT16, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  obj_properties[PROP_COLUMN_SPACING] =
    g_param_spec_int ("column-spacing",
                      P_("Column spacing"),
                      P_("The amount of space between two consecutive columns"),
                      0, G_MAXINT16, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  obj_properties[PROP_ROW_HOMOGENEOUS] =
    g_param_spec_boolean ("row-homogeneous",
                          P_("Row Homogeneous"),
                          P_("If TRUE, the rows are all the same height"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  obj_properties[PROP_COLUMN_HOMOGENEOUS] =
    g_param_spec_boolean ("column-homogeneous",
                          P_("Column Homogeneous"),
                          P_("If TRUE, the columns are all the same width"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  obj_properties[PROP_BASELINE_ROW] =
    g_param_spec_int ("baseline-row",
                      P_("Baseline Row"),
                      P_("The row to align the to the baseline when valign is GTK_ALIGN_BASELINE"),
                      0, G_MAXINT, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     obj_properties);

  child_properties[CHILD_PROP_LEFT_ATTACH] =
    g_param_spec_int ("left-attach",
                      P_("Left attachment"),
                      P_("The column number to attach the left side of the child to"),
                      G_MININT, G_MAXINT, 0,
                      GTK_PARAM_READWRITE);

  child_properties[CHILD_PROP_TOP_ATTACH] =
    g_param_spec_int ("top-attach",
                      P_("Top attachment"),
                      P_("The row number to attach the top side of a child widget to"),
                      G_MININT, G_MAXINT, 0,
                      GTK_PARAM_READWRITE);

  child_properties[CHILD_PROP_WIDTH] =
    g_param_spec_int ("width",
                      P_("Width"),
                      P_("The number of columns that a child spans"),
                      1, G_MAXINT, 1,
                      GTK_PARAM_READWRITE);

  child_properties[CHILD_PROP_HEIGHT] =
    g_param_spec_int ("height",
                      P_("Height"),
                      P_("The number of rows that a child spans"),
                      1, G_MAXINT, 1,
                      GTK_PARAM_READWRITE);


  gtk_container_class_install_child_properties (container_class, N_CHILD_PROPERTIES, child_properties);
  child_data_quark = g_quark_from_static_string ("gtk-grid-child-data");
  gtk_widget_class_set_css_name (widget_class, I_("grid"));
}

static void
gtk_grid_init (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  gtk_widget_set_has_surface (GTK_WIDGET (grid), FALSE);

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  priv->baseline_row = 0;

  priv->linedata[0].spacing = 0;
  priv->linedata[1].spacing = 0;

  priv->linedata[0].homogeneous = FALSE;
  priv->linedata[1].homogeneous = FALSE;

  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (grid));
}

/**
 * gtk_grid_new:
 *
 * Creates a new grid widget.
 *
 * Returns: the new #GtkGrid
 */
GtkWidget *
gtk_grid_new (void)
{
  return g_object_new (GTK_TYPE_GRID, NULL);
}

/**
 * gtk_grid_attach:
 * @grid: a #GtkGrid
 * @child: the widget to add
 * @left: the column number to attach the left side of @child to
 * @top: the row number to attach the top side of @child to
 * @width: the number of columns that @child will span
 * @height: the number of rows that @child will span
 *
 * Adds a widget to the grid.
 *
 * The position of @child is determined by @left and @top. The
 * number of “cells” that @child will occupy is determined by
 * @width and @height.
 */
void
gtk_grid_attach (GtkGrid   *grid,
                 GtkWidget *child,
                 gint       left,
                 gint       top,
                 gint       width,
                 gint       height)
{
  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (_gtk_widget_get_parent (child) == NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  grid_attach (grid, child, left, top, width, height);
}

/**
 * gtk_grid_attach_next_to:
 * @grid: a #GtkGrid
 * @child: the widget to add
 * @sibling: (allow-none): the child of @grid that @child will be placed
 *     next to, or %NULL to place @child at the beginning or end
 * @side: the side of @sibling that @child is positioned next to
 * @width: the number of columns that @child will span
 * @height: the number of rows that @child will span
 *
 * Adds a widget to the grid.
 *
 * The widget is placed next to @sibling, on the side determined by
 * @side. When @sibling is %NULL, the widget is placed in row (for
 * left or right placement) or column 0 (for top or bottom placement),
 * at the end indicated by @side.
 *
 * Attaching widgets labeled [1], [2], [3] with @sibling == %NULL and
 * @side == %GTK_POS_LEFT yields a layout of [3][2][1].
 */
void
gtk_grid_attach_next_to (GtkGrid         *grid,
                         GtkWidget       *child,
                         GtkWidget       *sibling,
                         GtkPositionType  side,
                         gint             width,
                         gint             height)
{
  GtkGridChild *grid_sibling;
  gint left, top;

  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (_gtk_widget_get_parent (child) == NULL);
  g_return_if_fail (sibling == NULL || _gtk_widget_get_parent (sibling) == (GtkWidget*)grid);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  if (sibling)
    {
      grid_sibling = get_grid_child (sibling);

      switch (side)
        {
        case GTK_POS_LEFT:
          left = CHILD_LEFT (grid_sibling) - width;
          top = CHILD_TOP (grid_sibling);
          break;
        case GTK_POS_RIGHT:
          left = CHILD_LEFT (grid_sibling) + CHILD_WIDTH (grid_sibling);
          top = CHILD_TOP (grid_sibling);
          break;
        case GTK_POS_TOP:
          left = CHILD_LEFT (grid_sibling);
          top = CHILD_TOP (grid_sibling) - height;
          break;
        case GTK_POS_BOTTOM:
          left = CHILD_LEFT (grid_sibling);
          top = CHILD_TOP (grid_sibling) + CHILD_HEIGHT (grid_sibling);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      switch (side)
        {
        case GTK_POS_LEFT:
          left = find_attach_position (grid, GTK_ORIENTATION_HORIZONTAL, 0, height, FALSE);
          left -= width;
          top = 0;
          break;
        case GTK_POS_RIGHT:
          left = find_attach_position (grid, GTK_ORIENTATION_HORIZONTAL, 0, height, TRUE);
          top = 0;
          break;
        case GTK_POS_TOP:
          left = 0;
          top = find_attach_position (grid, GTK_ORIENTATION_VERTICAL, 0, width, FALSE);
          top -= height;
          break;
        case GTK_POS_BOTTOM:
          left = 0;
          top = find_attach_position (grid, GTK_ORIENTATION_VERTICAL, 0, width, TRUE);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  grid_attach (grid, child, left, top, width, height);
}

/**
 * gtk_grid_get_child_at:
 * @grid: a #GtkGrid
 * @left: the left edge of the cell
 * @top: the top edge of the cell
 *
 * Gets the child of @grid whose area covers the grid
 * cell whose upper left corner is at @left, @top.
 *
 * Returns: (transfer none) (nullable): the child at the given position, or %NULL
 */
GtkWidget *
gtk_grid_get_child_at (GtkGrid *grid,
                       gint     left,
                       gint     top)
{
  GtkWidget *child;

  g_return_val_if_fail (GTK_IS_GRID (grid), NULL);

  for (child = gtk_widget_get_first_child (GTK_WIDGET (grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridChild *grid_child = get_grid_child (child);

      if (CHILD_LEFT (grid_child) <= left &&
          CHILD_LEFT (grid_child) + CHILD_WIDTH (grid_child) > left &&
          CHILD_TOP (grid_child) <= top &&
          CHILD_TOP (grid_child) + CHILD_HEIGHT (grid_child) > top)
        return child;
    }

  return NULL;
}

/**
 * gtk_grid_insert_row:
 * @grid: a #GtkGrid
 * @position: the position to insert the row at
 *
 * Inserts a row at the specified position.
 *
 * Children which are attached at or below this position
 * are moved one row down. Children which span across this
 * position are grown to span the new row.
 */
void
gtk_grid_insert_row (GtkGrid *grid,
                     gint     position)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkWidget *child;
  int top, height;
  GList *list;

  g_return_if_fail (GTK_IS_GRID (grid));

  for (child = gtk_widget_get_first_child (GTK_WIDGET (grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridChild *grid_child = get_grid_child (child);

      top = CHILD_TOP (grid_child);
      height = CHILD_HEIGHT (grid_child);

      if (top >= position)
        {
          CHILD_TOP (grid_child) = top + 1;
          gtk_container_child_notify_by_pspec (GTK_CONTAINER (grid),
                                               child,
                                               child_properties[CHILD_PROP_TOP_ATTACH]);
        }
      else if (top + height > position)
        {
          CHILD_HEIGHT (grid_child) = height + 1;
          gtk_container_child_notify_by_pspec (GTK_CONTAINER (grid),
                                               child,
                                               child_properties[CHILD_PROP_HEIGHT]);
        }
    }

  for (list = priv->row_properties; list != NULL; list = list->next)
    {
      GtkGridRowProperties *prop = list->data;

      if (prop->row >= position)
	prop->row += 1;
    }
}

/**
 * gtk_grid_remove_row:
 * @grid: a #GtkGrid
 * @position: the position of the row to remove
 *
 * Removes a row from the grid.
 *
 * Children that are placed in this row are removed,
 * spanning children that overlap this row have their
 * height reduced by one, and children below the row
 * are moved up.
 */
void
gtk_grid_remove_row (GtkGrid *grid,
                     gint     position)
{
  int top, height;
  GtkWidget *child;

  g_return_if_fail (GTK_IS_GRID (grid));

  child = gtk_widget_get_first_child (GTK_WIDGET (grid));
  while (child)
    {
      GtkGridChild *grid_child = get_grid_child (child);
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      top = CHILD_TOP (grid_child);
      height = CHILD_HEIGHT (grid_child);

      if (top <= position && top + height > position)
        height--;
      if (top > position)
        top--;

      if (height <= 0)
        gtk_container_remove (GTK_CONTAINER (grid), child);
      else
        gtk_container_child_set (GTK_CONTAINER (grid), child,
                                 "height", height,
                                 "top-attach", top,
                                 NULL);
      child = next;
    }
}

/**
 * gtk_grid_insert_column:
 * @grid: a #GtkGrid
 * @position: the position to insert the column at
 *
 * Inserts a column at the specified position.
 *
 * Children which are attached at or to the right of this position
 * are moved one column to the right. Children which span across this
 * position are grown to span the new column.
 */
void
gtk_grid_insert_column (GtkGrid *grid,
                        gint     position)
{
  GtkWidget *child;
  int left, width;

  g_return_if_fail (GTK_IS_GRID (grid));

  for (child = gtk_widget_get_first_child (GTK_WIDGET (grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridChild *grid_child = get_grid_child (child);

      left = CHILD_LEFT (grid_child);
      width = CHILD_WIDTH (grid_child);

      if (left >= position)
        {
          CHILD_LEFT (grid_child) = left + 1;
          gtk_container_child_notify_by_pspec (GTK_CONTAINER (grid),
                                               child,
                                               child_properties[CHILD_PROP_LEFT_ATTACH]);
        }
      else if (left + width > position)
        {
          CHILD_WIDTH (grid_child) = width + 1;
          gtk_container_child_notify_by_pspec (GTK_CONTAINER (grid),
                                               child,
                                               child_properties[CHILD_PROP_WIDTH]);
        }
    }
}

/**
 * gtk_grid_remove_column:
 * @grid: a #GtkGrid
 * @position: the position of the column to remove
 *
 * Removes a column from the grid.
 *
 * Children that are placed in this column are removed,
 * spanning children that overlap this column have their
 * width reduced by one, and children after the column
 * are moved to the left.
 */
void
gtk_grid_remove_column (GtkGrid *grid,
                        gint     position)
{
  GtkWidget *child;
  int left, width;

  g_return_if_fail (GTK_IS_GRID (grid));

  child = gtk_widget_get_first_child (GTK_WIDGET (grid));
  while (child)
    {
      GtkGridChild *grid_child = get_grid_child (child);
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      left = CHILD_LEFT (grid_child);
      width = CHILD_WIDTH (grid_child);

      if (left <= position && left + width > position)
        width--;
      if (left > position)
        left--;

      if (width <= 0)
        gtk_container_remove (GTK_CONTAINER (grid), child);
      else
        gtk_container_child_set (GTK_CONTAINER (grid), child,
                                 "width", width,
                                 "left-attach", left,
                                 NULL);

      child = next;
    }
}

/**
 * gtk_grid_insert_next_to:
 * @grid: a #GtkGrid
 * @sibling: the child of @grid that the new row or column will be
 *     placed next to
 * @side: the side of @sibling that @child is positioned next to
 *
 * Inserts a row or column at the specified position.
 *
 * The new row or column is placed next to @sibling, on the side
 * determined by @side. If @side is %GTK_POS_TOP or %GTK_POS_BOTTOM,
 * a row is inserted. If @side is %GTK_POS_LEFT of %GTK_POS_RIGHT,
 * a column is inserted.
 */
void
gtk_grid_insert_next_to (GtkGrid         *grid,
                         GtkWidget       *sibling,
                         GtkPositionType  side)
{
  GtkGridChild *child;

  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (GTK_IS_WIDGET (sibling));
  g_return_if_fail (_gtk_widget_get_parent (sibling) == (GtkWidget*)grid);

  child = get_grid_child (sibling);

  switch (side)
    {
    case GTK_POS_LEFT:
      gtk_grid_insert_column (grid, CHILD_LEFT (child));
      break;
    case GTK_POS_RIGHT:
      gtk_grid_insert_column (grid, CHILD_LEFT (child) + CHILD_WIDTH (child));
      break;
    case GTK_POS_TOP:
      gtk_grid_insert_row (grid, CHILD_TOP (child));
      break;
    case GTK_POS_BOTTOM:
      gtk_grid_insert_row (grid, CHILD_TOP (child) + CHILD_HEIGHT (child));
      break;
    default:
      g_assert_not_reached ();
    }
}

/**
 * gtk_grid_set_row_homogeneous:
 * @grid: a #GtkGrid
 * @homogeneous: %TRUE to make rows homogeneous
 *
 * Sets whether all rows of @grid will have the same height.
 */
void
gtk_grid_set_row_homogeneous (GtkGrid  *grid,
                              gboolean  homogeneous)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  g_return_if_fail (GTK_IS_GRID (grid));

  /* Yes, homogeneous rows means all the columns have the same size */
  if (COLUMNS (priv)->homogeneous != homogeneous)
    {
      COLUMNS (priv)->homogeneous = homogeneous;

      if (_gtk_widget_get_visible (GTK_WIDGET (grid)))
        gtk_widget_queue_resize (GTK_WIDGET (grid));

      g_object_notify_by_pspec (G_OBJECT (grid), obj_properties [PROP_ROW_HOMOGENEOUS]);
    }
}

/**
 * gtk_grid_get_row_homogeneous:
 * @grid: a #GtkGrid
 *
 * Returns whether all rows of @grid have the same height.
 *
 * Returns: whether all rows of @grid have the same height.
 */
gboolean
gtk_grid_get_row_homogeneous (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  g_return_val_if_fail (GTK_IS_GRID (grid), FALSE);

  return COLUMNS (priv)->homogeneous;
}

/**
 * gtk_grid_set_column_homogeneous:
 * @grid: a #GtkGrid
 * @homogeneous: %TRUE to make columns homogeneous
 *
 * Sets whether all columns of @grid will have the same width.
 */
void
gtk_grid_set_column_homogeneous (GtkGrid  *grid,
                                 gboolean  homogeneous)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  g_return_if_fail (GTK_IS_GRID (grid));

  /* Yes, homogeneous columns means all the rows have the same size */
  if (ROWS (priv)->homogeneous != homogeneous)
    {
      ROWS (priv)->homogeneous = homogeneous;

      if (_gtk_widget_get_visible (GTK_WIDGET (grid)))
        gtk_widget_queue_resize (GTK_WIDGET (grid));

      g_object_notify_by_pspec (G_OBJECT (grid), obj_properties [PROP_COLUMN_HOMOGENEOUS]);
    }
}

/**
 * gtk_grid_get_column_homogeneous:
 * @grid: a #GtkGrid
 *
 * Returns whether all columns of @grid have the same width.
 *
 * Returns: whether all columns of @grid have the same width.
 */
gboolean
gtk_grid_get_column_homogeneous (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  g_return_val_if_fail (GTK_IS_GRID (grid), FALSE);

  return ROWS (priv)->homogeneous;
}

/**
 * gtk_grid_set_row_spacing:
 * @grid: a #GtkGrid
 * @spacing: the amount of space to insert between rows
 *
 * Sets the amount of space between rows of @grid.
 */
void
gtk_grid_set_row_spacing (GtkGrid *grid,
                          guint    spacing)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (spacing <= G_MAXINT16);

  if (COLUMNS (priv)->spacing != spacing)
    {
      COLUMNS (priv)->spacing = spacing;

      if (_gtk_widget_get_visible (GTK_WIDGET (grid)))
        gtk_widget_queue_resize (GTK_WIDGET (grid));

      g_object_notify_by_pspec (G_OBJECT (grid), obj_properties [PROP_ROW_SPACING]);
    }
}

/**
 * gtk_grid_get_row_spacing:
 * @grid: a #GtkGrid
 *
 * Returns the amount of space between the rows of @grid.
 *
 * Returns: the row spacing of @grid
 */
guint
gtk_grid_get_row_spacing (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  g_return_val_if_fail (GTK_IS_GRID (grid), 0);

  return COLUMNS (priv)->spacing;
}

/**
 * gtk_grid_set_column_spacing:
 * @grid: a #GtkGrid
 * @spacing: the amount of space to insert between columns
 *
 * Sets the amount of space between columns of @grid.
 */
void
gtk_grid_set_column_spacing (GtkGrid *grid,
                             guint    spacing)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (spacing <= G_MAXINT16);

  if (ROWS (priv)->spacing != spacing)
    {
      ROWS (priv)->spacing = spacing;

      if (_gtk_widget_get_visible (GTK_WIDGET (grid)))
        gtk_widget_queue_resize (GTK_WIDGET (grid));

      g_object_notify_by_pspec (G_OBJECT (grid), obj_properties [PROP_COLUMN_SPACING]);
    }
}

/**
 * gtk_grid_get_column_spacing:
 * @grid: a #GtkGrid
 *
 * Returns the amount of space between the columns of @grid.
 *
 * Returns: the column spacing of @grid
 */
guint
gtk_grid_get_column_spacing (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), 0);

  return ROWS (priv)->spacing;
}

static GtkGridRowProperties *
find_row_properties (GtkGrid      *grid,
		     gint          row)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GList *l;

  for (l = priv->row_properties; l != NULL; l = l->next)
    {
      GtkGridRowProperties *prop = l->data;
      if (prop->row == row)
	return prop;
    }

  return NULL;
}

static void
gtk_grid_row_properties_free (GtkGridRowProperties *props)
{
  g_slice_free (GtkGridRowProperties, props);
}

static GtkGridRowProperties *
get_row_properties_or_create (GtkGrid      *grid,
			      gint          row)
{
  GtkGridRowProperties *props;
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  props = find_row_properties (grid, row);
  if (props)
    return props;

  props = g_slice_new (GtkGridRowProperties);
  *props = gtk_grid_row_properties_default;
  props->row = row;

  priv->row_properties =
    g_list_prepend (priv->row_properties, props);

  return props;
}

static const GtkGridRowProperties *
get_row_properties_or_default (GtkGrid      *grid,
			       gint          row)
{
  GtkGridRowProperties *props;

  props = find_row_properties (grid, row);
  if (props)
    return props;
  return &gtk_grid_row_properties_default;
}

/**
 * gtk_grid_set_row_baseline_position:
 * @grid: a #GtkGrid
 * @row: a row index
 * @pos: a #GtkBaselinePosition
 *
 * Sets how the baseline should be positioned on @row of the
 * grid, in case that row is assigned more space than is requested.
 */
void
gtk_grid_set_row_baseline_position (GtkGrid            *grid,
				    gint                row,
				    GtkBaselinePosition pos)
{
  GtkGridRowProperties *props;

  g_return_if_fail (GTK_IS_GRID (grid));

  props = get_row_properties_or_create (grid, row);

  if (props->baseline_position != pos)
    {
      props->baseline_position = pos;

      if (_gtk_widget_get_visible (GTK_WIDGET (grid)))
        gtk_widget_queue_resize (GTK_WIDGET (grid));
    }
}

/**
 * gtk_grid_get_row_baseline_position:
 * @grid: a #GtkGrid
 * @row: a row index
 *
 * Returns the baseline position of @row as set
 * by gtk_grid_set_row_baseline_position() or the default value
 * %GTK_BASELINE_POSITION_CENTER.
 *
 * Returns: the baseline position of @row
 */
GtkBaselinePosition
gtk_grid_get_row_baseline_position (GtkGrid      *grid,
				    gint          row)
{
  const GtkGridRowProperties *props;

  g_return_val_if_fail (GTK_IS_GRID (grid), GTK_BASELINE_POSITION_CENTER);

  props = get_row_properties_or_default (grid, row);

  return props->baseline_position;
}

/**
 * gtk_grid_set_baseline_row:
 * @grid: a #GtkGrid
 * @row: the row index
 *
 * Sets which row defines the global baseline for the entire grid.
 * Each row in the grid can have its own local baseline, but only
 * one of those is global, meaning it will be the baseline in the
 * parent of the @grid.
 */
void
gtk_grid_set_baseline_row (GtkGrid *grid,
			   gint     row)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_if_fail (GTK_IS_GRID (grid));

  if (priv->baseline_row != row)
    {
      priv->baseline_row = row;

      if (_gtk_widget_get_visible (GTK_WIDGET (grid)))
	gtk_widget_queue_resize (GTK_WIDGET (grid));
      g_object_notify (G_OBJECT (grid), "baseline-row");
    }
}

/**
 * gtk_grid_get_baseline_row:
 * @grid: a #GtkGrid
 *
 * Returns which row defines the global baseline of @grid.
 *
 * Returns: the row index defining the global baseline
 */
gint
gtk_grid_get_baseline_row (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), 0);

  return priv->baseline_row;
}
