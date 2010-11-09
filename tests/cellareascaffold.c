/* cellareascaffold.c
 *
 * Copyright (C) 2010 Openismus GmbH
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

#include <string.h>
#include "cellareascaffold.h"

/* GObjectClass */
static void      cell_area_scaffold_finalize                       (GObject              *object);
static void      cell_area_scaffold_dispose                        (GObject              *object);
static void      cell_area_scaffold_set_property                   (GObject              *object,
								    guint                 prop_id,
								    const GValue         *value,
								    GParamSpec           *pspec);
static void      cell_area_scaffold_get_property                   (GObject              *object,
								    guint                 prop_id,
								    GValue               *value,
								    GParamSpec           *pspec);

/* GtkWidgetClass */
static gboolean  cell_area_scaffold_draw                           (GtkWidget       *widget,
								    cairo_t         *cr);
static void      cell_area_scaffold_size_allocate                  (GtkWidget       *widget,
								    GtkAllocation   *allocation);
static void      cell_area_scaffold_get_preferred_width            (GtkWidget       *widget,
								    gint            *minimum_size,
								    gint            *natural_size);
static void      cell_area_scaffold_get_preferred_height_for_width (GtkWidget       *widget,
								    gint             for_size,
								    gint            *minimum_size,
								    gint            *natural_size);
static void      cell_area_scaffold_get_preferred_height           (GtkWidget       *widget,
								    gint            *minimum_size,
								    gint            *natural_size);
static void      cell_area_scaffold_get_preferred_width_for_height (GtkWidget       *widget,
								    gint             for_size,
								    gint            *minimum_size,
								    gint            *natural_size);



typedef struct {
  gint    size; /* The size of the row in the scaffold's opposing orientation */
} RowData;

struct _CellAreaScaffoldPrivate {

  /* The model we're showing data for */
  GtkTreeModel    *model;

  /* The area rendering the data and a global iter */
  GtkCellArea     *area;
  GtkCellAreaIter *iter;

  /* Cache some info about rows (hieghts etc) */
  GArray          *row_data;
};


#define ROW_SPACING  2

enum {
  PROP_0,
  PROP_ORIENTATION
};

G_DEFINE_TYPE_WITH_CODE (CellAreaScaffold, cell_area_scaffold, GTK_TYPE_WIDGET,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL));


static void
cell_area_scaffold_init (CellAreaScaffold *scaffold)
{
  CellAreaScaffoldPrivate *priv;

  scaffold->priv = G_TYPE_INSTANCE_GET_PRIVATE (scaffold,
						TYPE_CELL_AREA_SCAFFOLD,
						CellAreaScaffoldPrivate);
  priv = scaffold->priv;

  priv->area = gtk_cell_area_box_new ();
  priv->iter = gtk_cell_area_create_iter (priv->area);

  priv->row_data = g_array_new (FALSE, FALSE, sizeof (RowData));

  gtk_widget_set_has_window (GTK_WIDGET (scaffold), FALSE);
}

static void
cell_area_scaffold_class_init (CellAreaScaffoldClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS(class);
  gobject_class->dispose = cell_area_scaffold_dispose;
  gobject_class->finalize = cell_area_scaffold_finalize;
  gobject_class->get_property = cell_area_scaffold_get_property;
  gobject_class->set_property = cell_area_scaffold_set_property;

  widget_class = GTK_WIDGET_CLASS(class);
  widget_class->draw = cell_area_scaffold_draw;
  widget_class->size_allocate = cell_area_scaffold_size_allocate;
  widget_class->get_preferred_width = cell_area_scaffold_get_preferred_width;
  widget_class->get_preferred_height_for_width = cell_area_scaffold_get_preferred_height_for_width;
  widget_class->get_preferred_height = cell_area_scaffold_get_preferred_height;
  widget_class->get_preferred_width_for_height = cell_area_scaffold_get_preferred_width_for_height;

  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");

  g_type_class_add_private (gobject_class, sizeof (CellAreaScaffoldPrivate));
}

/*********************************************************
 *                    GObjectClass                       *
 *********************************************************/
static void
cell_area_scaffold_finalize (GObject *object)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (object);
  CellAreaScaffoldPrivate *priv;

  priv = scaffold->priv;

  g_array_free (priv->row_data, TRUE);

  G_OBJECT_CLASS (cell_area_scaffold_parent_class)->finalize (object);
}

static void
cell_area_scaffold_dispose (GObject *object)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (object);
  CellAreaScaffoldPrivate *priv;

  priv = scaffold->priv;

  cell_area_scaffold_set_model (scaffold, NULL);

  if (priv->iter)
    {
      g_object_unref (priv->iter);
      priv->iter = NULL;
    }

  if (priv->area)
    {
      g_object_unref (priv->area);
      priv->area = NULL;
    }

  G_OBJECT_CLASS (cell_area_scaffold_parent_class)->dispose (object);  
}

static void
cell_area_scaffold_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (object);
  CellAreaScaffoldPrivate *priv;

  priv = scaffold->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->area), 
				      g_value_get_enum (value));
      gtk_widget_queue_resize (GTK_WIDGET (scaffold));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void      
cell_area_scaffold_get_property (GObject    *object,
				 guint       prop_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (object);
  CellAreaScaffoldPrivate *priv;

  priv = scaffold->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, 
			gtk_orientable_get_orientation (GTK_ORIENTABLE (priv->area)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


/*********************************************************
 *                    GtkWidgetClass                     *
 *********************************************************/
static gboolean
cell_area_scaffold_draw (GtkWidget       *widget,
			 cairo_t         *cr)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  GtkOrientation           orientation;
  GtkTreeIter              iter;
  gboolean                 valid;
  GdkRectangle             render_area;
  GtkAllocation            allocation;
  gint                     i = 0;

  if (!priv->model)
    return FALSE;

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (priv->area));

  gtk_widget_get_allocation (widget, &allocation);

  render_area.x      = 0;
  render_area.y      = 0;
  render_area.width  = allocation.width;
  render_area.height = allocation.height;

  valid = gtk_tree_model_get_iter_first (priv->model, &iter);
  while (valid)
    {
      RowData *data = &g_array_index (priv->row_data, RowData, i);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  render_area.height = data->size;
	}
      else
	{
	  render_area.width = data->size;
	}

      gtk_cell_area_apply_attributes (priv->area, priv->model, &iter, FALSE, FALSE);
      gtk_cell_area_render (priv->area, priv->iter, widget, cr, &render_area, 0);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  render_area.y += data->size;
	  render_area.y += ROW_SPACING;
	}
      else
	{
	  render_area.x += data->size;
	  render_area.x += ROW_SPACING;
	}

      i++;
      valid = gtk_tree_model_iter_next (priv->model, &iter);
    }

  return FALSE;
}

static void 
request_all_base (CellAreaScaffold *scaffold)
{
  CellAreaScaffoldPrivate *priv = scaffold->priv;
  GtkWidget               *widget = GTK_WIDGET (scaffold);
  GtkOrientation           orientation;
  GtkTreeIter              iter;
  gboolean                 valid;

  if (!priv->model)
    return;

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (priv->area));

  valid = gtk_tree_model_get_iter_first (priv->model, &iter);
  while (valid)
    {
      gint min, nat;

      gtk_cell_area_apply_attributes (priv->area, priv->model, &iter, FALSE, FALSE);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	gtk_cell_area_get_preferred_width (priv->area, priv->iter, widget, &min, &nat);
      else
	gtk_cell_area_get_preferred_height (priv->area, priv->iter, widget, &min, &nat);

      valid = gtk_tree_model_iter_next (priv->model, &iter);
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_cell_area_iter_sum_preferred_width (priv->iter);
  else
    gtk_cell_area_iter_sum_preferred_height (priv->iter);
}

static void 
get_row_sizes (CellAreaScaffold *scaffold, 
	       GArray           *array,
	       gint              for_size)
{
  CellAreaScaffoldPrivate *priv = scaffold->priv;
  GtkWidget               *widget = GTK_WIDGET (scaffold);
  GtkOrientation           orientation;
  GtkTreeIter              iter;
  gboolean                 valid;
  gint                     i = 0;

  if (!priv->model)
    return;

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (priv->area));

  valid = gtk_tree_model_get_iter_first (priv->model, &iter);
  while (valid)
    {
      RowData *data = &g_array_index (array, RowData, i);

      gtk_cell_area_apply_attributes (priv->area, priv->model, &iter, FALSE, FALSE);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	gtk_cell_area_get_preferred_height_for_width (priv->area, priv->iter, widget, 
						      for_size, &data->size, NULL);
      else
	gtk_cell_area_get_preferred_width_for_height (priv->area, priv->iter, widget, 
						      for_size, &data->size, NULL);

      i++;
      valid = gtk_tree_model_iter_next (priv->model, &iter);
    }
}

static void
cell_area_scaffold_size_allocate (GtkWidget           *widget,
				  GtkAllocation       *allocation)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  GtkOrientation           orientation;

  if (!priv->model)
    return;

  gtk_widget_set_allocation (widget, allocation);

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (priv->area));

  /* Cache the per-row sizes and allocate the iter */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      get_row_sizes (scaffold, priv->row_data, allocation->width);
      gtk_cell_area_iter_allocate_width (priv->iter, allocation->width);
    }
  else
    {
      get_row_sizes (scaffold, priv->row_data, allocation->height);
      gtk_cell_area_iter_allocate_height (priv->iter, allocation->height);
    }
}


static void
cell_area_scaffold_get_preferred_width (GtkWidget       *widget,
					gint            *minimum_size,
					gint            *natural_size)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  GtkOrientation           orientation;

  if (!priv->model)
    return;

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (priv->area));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      request_all_base (scaffold);

      gtk_cell_area_iter_get_preferred_width (priv->iter, minimum_size, natural_size);
    }
  else
    {
      gint min_size, nat_size;

      GTK_WIDGET_GET_CLASS (widget)->get_preferred_height (widget, &min_size, &nat_size);
      GTK_WIDGET_GET_CLASS (widget)->get_preferred_width_for_height (widget, min_size, 
								     minimum_size, natural_size);
    }
}

static void
cell_area_scaffold_get_preferred_height_for_width (GtkWidget       *widget,
						   gint             for_size,
						   gint            *minimum_size,
						   gint            *natural_size)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  GtkOrientation           orientation;

  if (!priv->model)
    return;

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (priv->area));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      GArray *request_array;
      gint    n_rows, i, full_size = 0;

      n_rows = gtk_tree_model_iter_n_children (priv->model, NULL);

      /* Get an array for the contextual request */
      request_array = g_array_new (FALSE, FALSE, sizeof (RowData));
      g_array_set_size (request_array, n_rows);
      memset (request_array->data, 0x0, n_rows * sizeof (RowData));

      /* Gather each contextual size into the request array */
      get_row_sizes (scaffold, request_array, for_size);

      /* Sum up the size and add some row spacing */
      for (i = 0; i < n_rows; i++)
	{
	  RowData *data = &g_array_index (request_array, RowData, i);

	  full_size += data->size;
	}

      full_size += MAX (0, n_rows -1) * ROW_SPACING;

      g_array_free (request_array, TRUE);

      *minimum_size = full_size;
      *natural_size = full_size;
    }
  else
    {
      GTK_WIDGET_GET_CLASS (widget)->get_preferred_height (widget, minimum_size, natural_size);
    }
}

static void
cell_area_scaffold_get_preferred_height (GtkWidget       *widget,
					 gint            *minimum_size,
					 gint            *natural_size)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  GtkOrientation           orientation;

  if (!priv->model)
    return;

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (priv->area));

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      request_all_base (scaffold);

      gtk_cell_area_iter_get_preferred_height (priv->iter, minimum_size, natural_size);
    }
  else
    {
      gint min_size, nat_size;

      GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, &min_size, &nat_size);
      GTK_WIDGET_GET_CLASS (widget)->get_preferred_height_for_width (widget, min_size, 
								     minimum_size, natural_size);
    }
}

static void
cell_area_scaffold_get_preferred_width_for_height (GtkWidget       *widget,
						   gint             for_size,
						   gint            *minimum_size,
						   gint            *natural_size)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  GtkOrientation           orientation;

  if (!priv->model)
    return;

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (priv->area));

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      GArray *request_array;
      gint    n_rows, i, full_size = 0;

      n_rows = gtk_tree_model_iter_n_children (priv->model, NULL);

      /* Get an array for the contextual request */
      request_array = g_array_new (FALSE, FALSE, sizeof (RowData));
      g_array_set_size (request_array, n_rows);
      memset (request_array->data, 0x0, n_rows * sizeof (RowData));

      /* Gather each contextual size into the request array */
      get_row_sizes (scaffold, request_array, for_size);

      /* Sum up the size and add some row spacing */
      for (i = 0; i < n_rows; i++)
	{
	  RowData *data = &g_array_index (request_array, RowData, i);

	  full_size += data->size;
	}

      full_size += MAX (0, n_rows -1) * ROW_SPACING;

      g_array_free (request_array, TRUE);

      *minimum_size = full_size;
      *natural_size = full_size;
    }
  else
    {
      GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, minimum_size, natural_size);
    }
}



/*********************************************************
 *                         API                           *
 *********************************************************/
GtkWidget *
cell_area_scaffold_new (void)
{
  return (GtkWidget *)g_object_new (TYPE_CELL_AREA_SCAFFOLD, NULL);
}

GtkCellArea  *
cell_area_scaffold_get_area (CellAreaScaffold *scaffold)
{
  CellAreaScaffoldPrivate *priv;
  
  g_return_val_if_fail (IS_CELL_AREA_SCAFFOLD (scaffold), NULL);

  priv = scaffold->priv;

  return priv->area;
}

void
cell_area_scaffold_set_model (CellAreaScaffold *scaffold,
			      GtkTreeModel     *model)
{
  CellAreaScaffoldPrivate *priv;
  
  g_return_if_fail (IS_CELL_AREA_SCAFFOLD (scaffold));

  priv = scaffold->priv;

  if (priv->model != model)
    {
      if (priv->model)
	{
	  /* XXX disconnect signals */
	  g_object_unref (priv->model);
	}

      priv->model = model;

      if (priv->model)
	{
	  gint n_rows;

	  /* XXX connect signals */
	  g_object_ref (priv->model);

	  n_rows = gtk_tree_model_iter_n_children (priv->model, NULL);

	  /* Clear/reset the array */
	  g_array_set_size (priv->row_data, n_rows);
	  memset (priv->row_data->data, 0x0, n_rows * sizeof (RowData));
	}
      else
	{
	  g_array_set_size (priv->row_data, 0);
	}

      gtk_cell_area_iter_flush (priv->iter);

      gtk_widget_queue_resize (GTK_WIDGET (scaffold));
    }
}

GtkTreeModel *
cell_area_scaffold_get_model (CellAreaScaffold *scaffold)
{
  CellAreaScaffoldPrivate *priv;
  
  g_return_val_if_fail (IS_CELL_AREA_SCAFFOLD (scaffold), NULL);

  priv = scaffold->priv;

  return priv->model;
}
