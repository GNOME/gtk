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
#include "gtkprivate.h"
#include "gtkintl.h"


/**
 * SECTION:gtkgrid
 * @Short_description: Pack widgets in a rows and columns
 * @Title: GtkGrid
 * @See_also: #GtkTable, #GtkHBox, #GtkVBox
 *
 * GtkGrid is a container which arranges its child widgets in
 * rows and columns. It is a very similar to #GtkTable and #GtkBox,
 * but it consistently uses #GtkWidget's #GtkWidget:margin and #GtkWidget:expand
 * properties instead of custom child properties, and it fully supports
 * <link linkend="geometry-management">height-for-width geometry management</link>.
 *
 * Children are added using gtk_grid_attach(). They can span multiple
 * rows or columns. It is also possible to add a child next to an
 * existing child, using gtk_grid_attach_next_to(). The behaviour of
 * GtkGrid when several children occupy the same grid cell is undefined.
 *
 * GtkGrid can be used like a #GtkBox by just using gtk_container_add(),
 * which will place children next to each other in the direction determined
 * by the #GtkOrientable:orientation property.
 */

typedef struct _GtkGridChild GtkGridChild;
typedef struct _GtkGridChildAttach GtkGridChildAttach;
typedef struct _GtkGridLine GtkGridLine;
typedef struct _GtkGridLines GtkGridLines;
typedef struct _GtkGridLineData GtkGridLineData;
typedef struct _GtkGridRequest GtkGridRequest;

struct _GtkGridChildAttach
{
  gint pos;
  gint span;
};

struct _GtkGridChild
{
  GtkWidget *widget;
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
  GList *children;

  GtkOrientation orientation;

  GtkGridLineData linedata[2];
};

#define ROWS(priv)    (&(priv)->linedata[GTK_ORIENTATION_HORIZONTAL])
#define COLUMNS(priv) (&(priv)->linedata[GTK_ORIENTATION_VERTICAL])

/* A GtkGridLine struct represents a single row or column
 * during size requests
 */
struct _GtkGridLine
{
  gint minimum;
  gint natural;
  gint position;
  gint allocation;

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
  PROP_ORIENTATION,
  PROP_ROW_SPACING,
  PROP_COLUMN_SPACING,
  PROP_ROW_HOMOGENEOUS,
  PROP_COLUMN_HOMOGENEOUS
};

enum
{
  CHILD_PROP_0,
  CHILD_PROP_LEFT_ATTACH,
  CHILD_PROP_TOP_ATTACH,
  CHILD_PROP_WIDTH,
  CHILD_PROP_HEIGHT
};

G_DEFINE_TYPE_WITH_CODE (GtkGrid, gtk_grid, GTK_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))


static void
gtk_grid_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GtkGrid *grid = GTK_GRID (object);
  GtkGridPrivate *priv = grid->priv;

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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_set_orientation (GtkGrid        *grid,
                          GtkOrientation  orientation)
{
  GtkGridPrivate *priv = grid->priv;

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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GtkGridChild *
find_grid_child (GtkGrid   *grid,
                 GtkWidget *widget)
{
  GtkGridPrivate *priv = grid->priv;
  GtkGridChild *child;
  GList *list;

  for (list = priv->children; list; list = list->next)
    {
      child = list->data;

      if (child->widget == widget)
        return child;
    }

  return NULL;
}

static void
gtk_grid_get_child_property (GtkContainer *container,
                             GtkWidget    *child,
                             guint         property_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  GtkGrid *grid = GTK_GRID (container);
  GtkGridChild *grid_child;

  grid_child = find_grid_child (grid, child);

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

  grid_child = find_grid_child (grid, child);

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

  if (gtk_widget_get_visible (child) &&
      gtk_widget_get_visible (GTK_WIDGET (grid)))
    gtk_widget_queue_resize (child);
}

static void
gtk_grid_init (GtkGrid *grid)
{
  GtkGridPrivate *priv;

  grid->priv = G_TYPE_INSTANCE_GET_PRIVATE (grid, GTK_TYPE_GRID, GtkGridPrivate);
  priv = grid->priv;

  gtk_widget_set_has_window (GTK_WIDGET (grid), FALSE);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (grid), FALSE);

  priv->children = NULL;
  priv->orientation = GTK_ORIENTATION_HORIZONTAL;

  priv->linedata[0].spacing = 0;
  priv->linedata[1].spacing = 0;

  priv->linedata[0].homogeneous = FALSE;
  priv->linedata[1].homogeneous = FALSE;
}

static void
grid_attach (GtkGrid   *grid,
             GtkWidget *widget,
             gint       left,
             gint       top,
             gint       width,
             gint       height)
{
  GtkGridPrivate *priv = grid->priv;
  GtkGridChild *child;

  child = g_slice_new (GtkGridChild);
  child->widget = widget;
  CHILD_LEFT (child) = left;
  CHILD_TOP (child) = top;
  CHILD_WIDTH (child) = width;
  CHILD_HEIGHT (child) = height;

  priv->children = g_list_prepend (priv->children, child);

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
  GtkGridPrivate *priv = grid->priv;
  GtkGridChild *grid_child;
  GtkGridChildAttach *attach;
  GtkGridChildAttach *opposite;
  GList *list;
  gint pos;
  gboolean hit;

  if (max)
    pos = -G_MAXINT;
  else
    pos = G_MAXINT;

  hit = FALSE;

  for (list = priv->children; list; list = list->next)
    {
      grid_child = list->data;

      attach = &grid_child->attach[orientation];
      opposite = &grid_child->attach[1 - orientation];

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
  GtkGridPrivate *priv = grid->priv;
  gint pos[2] = { 0, 0 };

  pos[priv->orientation] = find_attach_position (grid, priv->orientation, 0, 1, TRUE);
  grid_attach (grid, child, pos[0], pos[1], 1, 1);
}

static void
gtk_grid_remove (GtkContainer *container,
                 GtkWidget    *child)
{
  GtkGrid *grid = GTK_GRID (container);
  GtkGridPrivate *priv = grid->priv;
  GtkGridChild *grid_child;
  GList *list;

  for (list = priv->children; list; list = list->next)
    {
      grid_child = list->data;

      if (grid_child->widget == child)
        {
          gboolean was_visible = gtk_widget_get_visible (child);

          gtk_widget_unparent (child);

          priv->children = g_list_remove (priv->children, grid_child);

          g_slice_free (GtkGridChild, grid_child);

          if (was_visible && gtk_widget_get_visible (GTK_WIDGET (grid)))
            gtk_widget_queue_resize (GTK_WIDGET (grid));

          break;
        }
    }
}

static void
gtk_grid_forall (GtkContainer *container,
                 gboolean      include_internals,
                 GtkCallback   callback,
                 gpointer      callback_data)
{
  GtkGrid *grid = GTK_GRID (container);
  GtkGridPrivate *priv = grid->priv;
  GtkGridChild *child;
  GList *list;

  list = priv->children;
  while (list)
    {
      child = list->data;
      list  = list->next;

      (* callback) (child->widget, callback_data);
    }
}

static GType
gtk_grid_child_type (GtkContainer *container)
{
  return GTK_TYPE_WIDGET;
}

/* Calculates the min and max numbers for both orientations.
 */
static void
gtk_grid_request_count_lines (GtkGridRequest *request)
{
  GtkGridPrivate *priv = request->grid->priv;
  GtkGridChild *child;
  GtkGridChildAttach *attach;
  GList *list;
  gint min[2];
  gint max[2];

  min[0] = min[1] = G_MAXINT;
  max[0] = max[1] = G_MININT;

  for (list = priv->children; list; list = list->next)
    {
      child = list->data;
      attach = child->attach;

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
  GtkGridPrivate *priv = request->grid->priv;
  GtkGridChild *child;
  GtkGridChildAttach *attach;
  GtkGridLines *lines;
  GList *list;
  gint i;

  lines = &request->lines[orientation];

  for (i = 0; i < lines->max - lines->min; i++)
    {
      lines->lines[i].minimum = 0;
      lines->lines[i].natural = 0;
      lines->lines[i].expand = FALSE;
    }

  for (list = priv->children; list; list = list->next)
    {
      child = list->data;

      attach = &child->attach[orientation];
      if (attach->span == 1 && gtk_widget_compute_expand (child->widget, orientation))
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
  GtkGridPrivate *priv = request->grid->priv;
  GtkGridLineData *linedata;
  GtkGridLines *lines;
  GtkGridLine *line;
  GtkGridChildAttach *attach;
  gint size;
  gint i;

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];
  attach = &child->attach[orientation];

  size = (attach->span - 1) * linedata->spacing;
  for (i = 0; i < attach->span; i++)
    {
      line = &lines->lines[attach->pos - lines->min + i];
      size += line->allocation;
    }

  return size;
}

static void
compute_request_for_child (GtkGridRequest *request,
                           GtkGridChild   *child,
                           GtkOrientation  orientation,
                           gboolean        contextual,
                           gint           *minimum,
                           gint           *natural)
{
  if (contextual)
    {
      gint size;

      size = compute_allocation_for_child (request, child, 1 - orientation);
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        gtk_widget_get_preferred_width_for_height (child->widget,
                                                   size,
                                                   minimum, natural);
      else
        gtk_widget_get_preferred_height_for_width (child->widget,
                                                   size,
                                                   minimum, natural);
    }
  else
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        gtk_widget_get_preferred_width (child->widget, minimum, natural);
      else
        gtk_widget_get_preferred_height (child->widget, minimum, natural);
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
  GtkGridPrivate *priv = request->grid->priv;
  GtkGridChild *child;
  GtkGridChildAttach *attach;
  GtkGridLines *lines;
  GtkGridLine *line;
  GList *list;
  gint minimum;
  gint natural;

  lines = &request->lines[orientation];

  for (list = priv->children; list; list = list->next)
    {
      child = list->data;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      attach = &child->attach[orientation];
      if (attach->span != 1)
        continue;

      compute_request_for_child (request, child, orientation, contextual, &minimum, &natural);

      line = &lines->lines[attach->pos - lines->min];
      line->minimum = MAX (line->minimum, minimum);
      line->natural = MAX (line->natural, natural);
    }
}

/* Enforce homogeneous sizes.
 */
static void
gtk_grid_request_homogeneous (GtkGridRequest *request,
                              GtkOrientation  orientation)
{
  GtkGridPrivate *priv = request->grid->priv;
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
  GtkGridPrivate *priv = request->grid->priv;
  GList *list;
  GtkGridChild *child;
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
  gint extra;
  gint expand;
  gint line_extra;
  gint i;

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];

  for (list = priv->children; list; list = list->next)
    {
      child = list->data;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      attach = &child->attach[orientation];
      if (attach->span == 1)
        continue;

      compute_request_for_child (request, child, orientation, contextual, &minimum, &natural);

      span_minimum = (attach->span - 1) * linedata->spacing;
      span_natural = (attach->span - 1) * linedata->spacing;
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

              total = minimum - (attach->span - 1) * linedata->spacing;
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

              total = natural - (attach->span - 1) * linedata->spacing;
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
                                 gint           *nonempty_lines,
                                 gint           *expand_lines)
{
  GtkGridPrivate *priv = request->grid->priv;
  GtkGridChild *child;
  GtkGridChildAttach *attach;
  GList *list;
  gint i;
  GtkGridLines *lines;
  GtkGridLine *line;
  gboolean has_expand;
  gint expand;
  gint empty;

  lines = &request->lines[orientation];

  for (i = 0; i < lines->max - lines->min; i++)
    {
      lines->lines[i].need_expand = FALSE;
      lines->lines[i].expand = FALSE;
      lines->lines[i].empty = TRUE;
    }

  for (list = priv->children; list; list = list->next)
    {
      child = list->data;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      attach = &child->attach[orientation];
      if (attach->span != 1)
        continue;

      line = &lines->lines[attach->pos - lines->min];
      line->empty = FALSE;
      if (gtk_widget_compute_expand (child->widget, orientation))
        line->expand = TRUE;
    }

  for (list = priv->children; list; list = list->next)
    {
      child = list->data;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      attach = &child->attach[orientation];
      if (attach->span == 1)
        continue;

      has_expand = FALSE;
      for (i = 0; i < attach->span; i++)
        {
          line = &lines->lines[attach->pos - lines->min + i];
          line->empty = FALSE;
          if (line->expand)
            has_expand = TRUE;
        }

      if (!has_expand && gtk_widget_compute_expand (child->widget, orientation))
        {
          for (i = 0; i < attach->span; i++)
            {
              line = &lines->lines[attach->pos - lines->min + i];
              line->need_expand = TRUE;
            }
        }
    }

  empty = 0;
  expand = 0;
  for (i = 0; i < lines->max - lines->min; i++)
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
    *nonempty_lines = lines->max - lines->min - empty;

  if (expand_lines)
    *expand_lines = expand;
}

/* Sums the minimum and natural fields of lines and their spacing.
 */
static void
gtk_grid_request_sum (GtkGridRequest *request,
                      GtkOrientation  orientation,
                      gint           *minimum,
                      gint           *natural)
{
  GtkGridPrivate *priv = request->grid->priv;
  GtkGridLineData *linedata;
  GtkGridLines *lines;
  gint i;
  gint min, nat;
  gint nonempty;

  gtk_grid_request_compute_expand (request, orientation, &nonempty, NULL);

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];

  min = 0;
  nat = 0;
  if (nonempty > 0)
    {
      min = (nonempty - 1) * linedata->spacing;
      nat = (nonempty - 1) * linedata->spacing;
    }

  for (i = 0; i < lines->max - lines->min; i++)
    {
      min += lines->lines[i].minimum;
      nat += lines->lines[i].natural;
    }

  if (minimum)
    *minimum = min;

  if (natural)
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

/* Requires that the minimum and natural fields of lines
 * have been set, computes the allocation field of lines
 * by distributing total_size among lines.
 */
static void
gtk_grid_request_allocate (GtkGridRequest *request,
                           GtkOrientation  orientation,
                           gint            total_size)
{
  GtkGridPrivate *priv = request->grid->priv;
  GtkGridLineData *linedata;
  GtkGridLines *lines;
  GtkGridLine *line;
  gint nonempty;
  gint expand;
  gint i, j;
  GtkRequestedSize *sizes;
  gint extra;
  gint rest;
  gint size;

  gtk_grid_request_compute_expand (request, orientation, &nonempty, &expand);

  if (nonempty == 0)
    return;

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];

  size = total_size - (nonempty - 1) * linedata->spacing;

  if (linedata->homogeneous)
    {
      extra = size / nonempty;
      rest = size % nonempty;

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
      sizes = g_newa (GtkRequestedSize, nonempty);

      j = 0;
      for (i = 0; i < lines->max - lines->min; i++)
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
      for (i = 0; i < lines->max - lines->min; i++)
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
}

/* Computes the position fields from allocation and spacing.
 */
static void
gtk_grid_request_position (GtkGridRequest *request,
                           GtkOrientation  orientation)
{
  GtkGridPrivate *priv = request->grid->priv;
  GtkGridLineData *linedata;
  GtkGridLines *lines;
  GtkGridLine *line;
  gint position;
  gint i;

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];

  position = 0;
  for (i = 0; i < lines->max - lines->min; i++)
    {
      line = &lines->lines[i];
      if (!line->empty)
        {
          line->position = position;
          position += line->allocation + linedata->spacing;
        }
    }
}

static void
gtk_grid_get_size (GtkGrid        *grid,
                   GtkOrientation  orientation,
                   gint           *minimum,
                   gint           *natural)
{
  GtkGridRequest request;
  GtkGridLines *lines;

  if (minimum)
    *minimum = 0;

  if (natural)
    *natural = 0;

  if (grid->priv->children == NULL)
    return;

  request.grid = grid;
  gtk_grid_request_count_lines (&request);
  lines = &request.lines[orientation];
  lines->lines = g_newa (GtkGridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (GtkGridLine));

  gtk_grid_request_run (&request, orientation, FALSE);
  gtk_grid_request_sum (&request, orientation, minimum, natural);
}

static void
gtk_grid_get_size_for_size (GtkGrid        *grid,
                            GtkOrientation  orientation,
                            gint            size,
                            gint           *minimum,
                            gint           *natural)
{
  GtkGridRequest request;
  GtkGridLines *lines;
  gint min_size;

  if (minimum)
    *minimum = 0;

  if (natural)
    *natural = 0;

  if (grid->priv->children == NULL)
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
  gtk_grid_request_sum (&request, 1 - orientation, &min_size, NULL);
  gtk_grid_request_allocate (&request, 1 - orientation, MAX (size, min_size));

  gtk_grid_request_run (&request, orientation, TRUE);
  gtk_grid_request_sum (&request, orientation, minimum, natural);
}

static void
gtk_grid_get_preferred_width (GtkWidget *widget,
                              gint      *minimum,
                              gint      *natural)
{
  GtkGrid *grid = GTK_GRID (widget);

  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT)
    gtk_grid_get_size_for_size (grid, GTK_ORIENTATION_HORIZONTAL, 0, minimum, natural);
  else
    gtk_grid_get_size (grid, GTK_ORIENTATION_HORIZONTAL, minimum, natural);
}

static void
gtk_grid_get_preferred_height (GtkWidget *widget,
                               gint      *minimum,
                               gint      *natural)
{
  GtkGrid *grid = GTK_GRID (widget);

  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
    gtk_grid_get_size_for_size (grid, GTK_ORIENTATION_VERTICAL, 0, minimum, natural);
  else
    gtk_grid_get_size (grid, GTK_ORIENTATION_VERTICAL, minimum, natural);
}

static void
gtk_grid_get_preferred_width_for_height (GtkWidget *widget,
                                         gint       height,
                                         gint      *minimum,
                                         gint      *natural)
{
  GtkGrid *grid = GTK_GRID (widget);

  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT)
    gtk_grid_get_size_for_size (grid, GTK_ORIENTATION_HORIZONTAL, height, minimum, natural);
  else
    gtk_grid_get_size (grid, GTK_ORIENTATION_HORIZONTAL, minimum, natural);
}

static void
gtk_grid_get_preferred_height_for_width (GtkWidget *widget,
                                         gint       width,
                                         gint      *minimum,
                                         gint      *natural)
{
  GtkGrid *grid = GTK_GRID (widget);

  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
    gtk_grid_get_size_for_size (grid, GTK_ORIENTATION_VERTICAL, width, minimum, natural);
  else
    gtk_grid_get_size (grid, GTK_ORIENTATION_VERTICAL, minimum, natural);
}

static void
allocate_child (GtkGridRequest *request,
                GtkOrientation  orientation,
                GtkGridChild   *child,
                gint           *position,
                gint           *size)
{
  GtkGridPrivate *priv = request->grid->priv;
  GtkGridLineData *linedata;
  GtkGridLines *lines;
  GtkGridLine *line;
  GtkGridChildAttach *attach;
  gint i;

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];
  attach = &child->attach[orientation];

  *position = lines->lines[attach->pos - lines->min].position;

  *size = (attach->span - 1) * linedata->spacing;
  for (i = 0; i < attach->span; i++)
    {
      line = &lines->lines[attach->pos - lines->min + i];
      *size += line->allocation;
    }
}

static void
gtk_grid_request_allocate_children (GtkGridRequest *request)
{
  GtkGridPrivate *priv = request->grid->priv;
  GList *list;
  GtkGridChild *child;
  GtkAllocation allocation;
  GtkAllocation child_allocation;
  gint x, y, width, height;

  gtk_widget_get_allocation (GTK_WIDGET (request->grid), &allocation);

  for (list = priv->children; list; list = list->next)
    {
      child = list->data;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      allocate_child (request, GTK_ORIENTATION_HORIZONTAL, child, &x, &width);
      allocate_child (request, GTK_ORIENTATION_VERTICAL, child, &y, &height);

      child_allocation.x = allocation.x + x;
      child_allocation.y = allocation.y + y;
      child_allocation.width = MAX (1, width);
      child_allocation.height = MAX (1, height);

      if (gtk_widget_get_direction (GTK_WIDGET (request->grid)) == GTK_TEXT_DIR_RTL)
        child_allocation.x = allocation.x + allocation.width
                             - (child_allocation.x - allocation.x) - child_allocation.width;

      gtk_widget_size_allocate (child->widget, &child_allocation);
    }
}

#define GET_SIZE(allocation, orientation) (orientation == GTK_ORIENTATION_HORIZONTAL ? allocation->width : allocation->height)

static void
gtk_grid_size_allocate (GtkWidget     *widget,
                        GtkAllocation *allocation)
{
  GtkGrid *grid = GTK_GRID (widget);
  GtkGridPrivate *priv = grid->priv;
  GtkGridRequest request;
  GtkGridLines *lines;
  GtkOrientation orientation;

  if (priv->children == NULL)
    {
      gtk_widget_set_allocation (widget, allocation);
      return;
    }

  request.grid = grid;

  gtk_grid_request_count_lines (&request);
  lines = &request.lines[0];
  lines->lines = g_newa (GtkGridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (GtkGridLine));
  lines = &request.lines[1];
  lines->lines = g_newa (GtkGridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (GtkGridLine));

  gtk_widget_set_allocation (widget, allocation);

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

  gtk_grid_request_allocate_children (&request);
}

static void
gtk_grid_class_init (GtkGridClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->get_property = gtk_grid_get_property;
  object_class->set_property = gtk_grid_set_property;

  widget_class->size_allocate = gtk_grid_size_allocate;
  widget_class->get_preferred_width = gtk_grid_get_preferred_width;
  widget_class->get_preferred_height = gtk_grid_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_grid_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_grid_get_preferred_height_for_width;

  container_class->add = gtk_grid_add;
  container_class->remove = gtk_grid_remove;
  container_class->forall = gtk_grid_forall;
  container_class->child_type = gtk_grid_child_type;
  container_class->set_child_property = gtk_grid_set_child_property;
  container_class->get_child_property = gtk_grid_get_child_property;
  gtk_container_class_handle_border_width (container_class);

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  g_object_class_install_property (object_class, PROP_ROW_SPACING,
    g_param_spec_int ("row-spacing",
                      P_("Row spacing"),
                      P_("The amount of space between two consecutive rows"),
                      0, G_MAXINT16, 0,
                      GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_COLUMN_SPACING,
    g_param_spec_int ("column-spacing",
                      P_("Column spacing"),
                      P_("The amount of space between two consecutive columns"),
                      0, G_MAXINT16, 0,
                      GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_ROW_HOMOGENEOUS,
    g_param_spec_boolean ("row-homogeneous",
                          P_("Row Homogeneous"),
                          P_("If TRUE, the rows are all the same height"),
                          FALSE,
                          GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_COLUMN_HOMOGENEOUS,
    g_param_spec_boolean ("column-homogeneous",
                          P_("Column Homogeneous"),
                          P_("If TRUE, the columns are all the same width"),
                          FALSE,
                          GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class, CHILD_PROP_LEFT_ATTACH,
    g_param_spec_int ("left-attach",
                      P_("Left attachment"),
                      P_("The column number to attach the left side of the child to"),
                      G_MININT, G_MAXINT, 0,
                      GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class, CHILD_PROP_TOP_ATTACH,
    g_param_spec_int ("top-attach",
                      P_("Top attachment"),
                      P_("The row number to attach the top side of a child widget to"),
                      G_MININT, G_MAXINT, 0,
                      GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class, CHILD_PROP_WIDTH,
    g_param_spec_int ("width",
                      P_("Width"),
                      P_("The number of columns that a child spans"),
                      1, G_MAXINT, 1,
                      GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class, CHILD_PROP_HEIGHT,
    g_param_spec_int ("height",
                      P_("Height"),
                      P_("The number of rows that a child spans"),
                      1, G_MAXINT, 1,
                      GTK_PARAM_READWRITE));

  g_type_class_add_private (class, sizeof (GtkGridPrivate));
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
 * number of 'cells' that @child will occupy is determined by
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
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);
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
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);
  g_return_if_fail (sibling == NULL || gtk_widget_get_parent (sibling) == (GtkWidget*)grid);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  if (sibling)
    {
      grid_sibling = find_grid_child (grid, sibling);

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
 * Returns: (transfer none): the child at the given position, or %NULL
 *
 * Since: 3.2
 */
GtkWidget *
gtk_grid_get_child_at (GtkGrid *grid,
                       gint     left,
                       gint     top)
{
  GtkGridPrivate *priv;
  GtkGridChild *child;
  GList *list;

  g_return_val_if_fail (GTK_IS_GRID (grid), NULL);

  priv = grid->priv;

  for (list = priv->children; list; list = list->next)
    {
      child = list->data;

      if (CHILD_LEFT (child) <= left &&
          CHILD_LEFT (child) + CHILD_WIDTH (child) > left &&
          CHILD_TOP (child) <= top &&
          CHILD_TOP (child) + CHILD_HEIGHT (child) > top)
        return child->widget;
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
 *
 * Since: 3.2
 */
void
gtk_grid_insert_row (GtkGrid *grid,
                     gint     position)
{
  GtkGridPrivate *priv;
  GtkGridChild *child;
  GList *list;
  gint top, height;

  g_return_if_fail (GTK_IS_GRID (grid));

  priv = grid->priv;

  for (list = priv->children; list; list = list->next)
    {
      child = list->data;

      top = CHILD_TOP (child);
      height = CHILD_HEIGHT (child);

      if (top >= position)
        {
          CHILD_TOP (child) = top + 1;
          gtk_container_child_notify (GTK_CONTAINER (grid), child->widget, "top-attach");
        }
      else if (top + height > position)
        {
          CHILD_HEIGHT (child) = height + 1;
          gtk_container_child_notify (GTK_CONTAINER (grid), child->widget, "height");
        }
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
 *
 * Since: 3.2
 */
void
gtk_grid_insert_column (GtkGrid *grid,
                        gint     position)
{
  GtkGridPrivate *priv;
  GtkGridChild *child;
  GList *list;
  gint left, width;

  g_return_if_fail (GTK_IS_GRID (grid));

  priv = grid->priv;

  for (list = priv->children; list; list = list->next)
    {
      child = list->data;

      left = CHILD_LEFT (child);
      width = CHILD_WIDTH (child);

      if (left >= position)
        {
          CHILD_LEFT (child) = left + 1;
          gtk_container_child_notify (GTK_CONTAINER (grid), child->widget, "left-attach");
        }
      else if (left + width > position)
        {
          CHILD_WIDTH (child) = width + 1;
          gtk_container_child_notify (GTK_CONTAINER (grid), child->widget, "width");
        }
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
 *
 * Since: 3.2
 */
void
gtk_grid_insert_next_to (GtkGrid         *grid,
                         GtkWidget       *sibling,
                         GtkPositionType  side)
{
  GtkGridChild *child;

  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (GTK_IS_WIDGET (sibling));
  g_return_if_fail (gtk_widget_get_parent (sibling) == (GtkWidget*)grid);

  child = find_grid_child (grid, sibling);

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
  GtkGridPrivate *priv;
  g_return_if_fail (GTK_IS_GRID (grid));

  priv = grid->priv;

  /* Yes, homogeneous rows means all the columns have the same size */
  if (COLUMNS (priv)->homogeneous != homogeneous)
    {
      COLUMNS (priv)->homogeneous = homogeneous;

      if (gtk_widget_get_visible (GTK_WIDGET (grid)))
        gtk_widget_queue_resize (GTK_WIDGET (grid));

      g_object_notify (G_OBJECT (grid), "row-homogeneous");
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
  GtkGridPrivate *priv;
  g_return_val_if_fail (GTK_IS_GRID (grid), FALSE);

  priv = grid->priv;

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
  GtkGridPrivate *priv;
  g_return_if_fail (GTK_IS_GRID (grid));

  priv = grid->priv;

  /* Yes, homogeneous columns means all the rows have the same size */
  if (ROWS (priv)->homogeneous != homogeneous)
    {
      ROWS (priv)->homogeneous = homogeneous;

      if (gtk_widget_get_visible (GTK_WIDGET (grid)))
        gtk_widget_queue_resize (GTK_WIDGET (grid));

      g_object_notify (G_OBJECT (grid), "column-homogeneous");
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
  GtkGridPrivate *priv;
  g_return_val_if_fail (GTK_IS_GRID (grid), FALSE);

  priv = grid->priv;

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
  GtkGridPrivate *priv;
  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (spacing <= G_MAXINT16);

  priv = grid->priv;

  if (COLUMNS (priv)->spacing != spacing)
    {
      COLUMNS (priv)->spacing = spacing;

      if (gtk_widget_get_visible (GTK_WIDGET (grid)))
        gtk_widget_queue_resize (GTK_WIDGET (grid));

      g_object_notify (G_OBJECT (grid), "row-spacing");
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
  GtkGridPrivate *priv;
  g_return_val_if_fail (GTK_IS_GRID (grid), 0);

  priv = grid->priv;

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
  GtkGridPrivate *priv;
  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (spacing <= G_MAXINT16);

  priv = grid->priv;

  if (ROWS (priv)->spacing != spacing)
    {
      ROWS (priv)->spacing = spacing;

      if (gtk_widget_get_visible (GTK_WIDGET (grid)))
        gtk_widget_queue_resize (GTK_WIDGET (grid));

      g_object_notify (G_OBJECT (grid), "column-spacing");
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
  GtkGridPrivate *priv;

  g_return_val_if_fail (GTK_IS_GRID (grid), 0);

  priv = grid->priv;

  return ROWS (priv)->spacing;
}
