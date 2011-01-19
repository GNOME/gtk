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

/* GtkWidgetClass */
static void      cell_area_scaffold_realize                        (GtkWidget       *widget);
static void      cell_area_scaffold_unrealize                      (GtkWidget       *widget);
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
static void      cell_area_scaffold_map                            (GtkWidget       *widget);
static void      cell_area_scaffold_unmap                          (GtkWidget       *widget);
static gint      cell_area_scaffold_focus                          (GtkWidget       *widget,
								    GtkDirectionType direction);
static gboolean  cell_area_scaffold_button_press                   (GtkWidget       *widget,
								    GdkEventButton  *event);

/* GtkContainerClass */
static void      cell_area_scaffold_forall                         (GtkContainer    *container,
								    gboolean         include_internals,
								    GtkCallback      callback,
								    gpointer         callback_data);
static void      cell_area_scaffold_remove                         (GtkContainer    *container,
								    GtkWidget       *child);
static void      cell_area_scaffold_put_edit_widget                (CellAreaScaffold *scaffold,
								    GtkWidget        *edit_widget,
								    gint              x,
								    gint              y,
								    gint              width,
								    gint              height);

/* CellAreaScaffoldClass */
static void      cell_area_scaffold_activate                       (CellAreaScaffold *scaffold);

/* CellArea/GtkTreeModel callbacks */
static void      size_changed_cb                                   (GtkCellAreaContext *context,
								    GParamSpec       *pspec,
								    CellAreaScaffold *scaffold);
static void      focus_changed_cb                                  (GtkCellArea      *area,
								    GtkCellRenderer  *renderer,
								    const gchar      *path,
								    CellAreaScaffold *scaffold);
static void      add_editable_cb                                   (GtkCellArea      *area,
								    GtkCellRenderer  *renderer,
								    GtkCellEditable  *edit_widget,
								    GdkRectangle     *cell_area,
								    const gchar      *path,
								    CellAreaScaffold *scaffold);
static void      remove_editable_cb                                (GtkCellArea      *area,
								    GtkCellRenderer  *renderer,
								    GtkCellEditable  *edit_widget,
								    CellAreaScaffold *scaffold);
static void      row_changed_cb                                    (GtkTreeModel     *model,
								    GtkTreePath      *path,
								    GtkTreeIter      *iter,
								    CellAreaScaffold *scaffold);
static void      row_inserted_cb                                   (GtkTreeModel     *model,
								    GtkTreePath      *path,
								    GtkTreeIter      *iter,
								    CellAreaScaffold *scaffold);
static void      row_deleted_cb                                    (GtkTreeModel     *model,
								    GtkTreePath      *path,
								    CellAreaScaffold *scaffold);
static void      rows_reordered_cb                                 (GtkTreeModel     *model,
								    GtkTreePath      *parent,
								    GtkTreeIter      *iter,
								    gint             *new_order,
								    CellAreaScaffold *scaffold);

typedef struct {
  gint    size; /* The height of rows in the scaffold's */
} RowData;

struct _CellAreaScaffoldPrivate {

  /* Window for catching events and dispatching them to the cell area */
  GdkWindow       *event_window;

  /* The model we're showing data for */
  GtkTreeModel    *model;
  gulong           row_changed_id;
  gulong           row_inserted_id;
  gulong           row_deleted_id;
  gulong           rows_reordered_id;

  /* The area rendering the data and a global context */
  GtkCellArea        *area;
  GtkCellAreaContext *context;

  /* Cache some info about rows (hieghts etc) */
  GArray          *row_data;

  /* Focus handling */
  gint             focus_row;
  gulong           focus_changed_id;

  /* Check when the underlying area changes the size and
   * we need to queue a redraw */
  gulong           size_changed_id;

  /* Currently edited widget */
  GtkWidget       *edit_widget;
  GdkRectangle     edit_rect;
  gulong           add_editable_id;
  gulong           remove_editable_id;


  gint             row_spacing;
  gint             indent;
};

enum {
  ACTIVATE,
  N_SIGNALS
};

static guint scaffold_signals[N_SIGNALS] = { 0 };

#define DIRECTION_STR(dir)				\
  ((dir) == GTK_DIR_TAB_FORWARD  ? "tab forward" :	\
   (dir) == GTK_DIR_TAB_BACKWARD ? "tab backward" :	\
   (dir) == GTK_DIR_UP           ? "up" :		\
   (dir) == GTK_DIR_DOWN         ? "down" :		\
   (dir) == GTK_DIR_LEFT         ? "left" :		\
   (dir) == GTK_DIR_RIGHT        ? "right" : "invalid")

G_DEFINE_TYPE_WITH_CODE (CellAreaScaffold, cell_area_scaffold, GTK_TYPE_CONTAINER,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL));


static void
cell_area_scaffold_init (CellAreaScaffold *scaffold)
{
  CellAreaScaffoldPrivate *priv;

  scaffold->priv = G_TYPE_INSTANCE_GET_PRIVATE (scaffold,
						TYPE_CELL_AREA_SCAFFOLD,
						CellAreaScaffoldPrivate);
  priv = scaffold->priv;

  priv->area    = gtk_cell_area_box_new ();
  priv->context = gtk_cell_area_create_context (priv->area);

  priv->row_data = g_array_new (FALSE, FALSE, sizeof (RowData));

  gtk_widget_set_has_window (GTK_WIDGET (scaffold), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (scaffold), TRUE);

  priv->size_changed_id = 
    g_signal_connect (priv->context, "notify",
		      G_CALLBACK (size_changed_cb), scaffold);

  priv->focus_changed_id =
    g_signal_connect (priv->area, "focus-changed",
		      G_CALLBACK (focus_changed_cb), scaffold);

  priv->add_editable_id =
    g_signal_connect (priv->area, "add-editable",
		      G_CALLBACK (add_editable_cb), scaffold);

  priv->remove_editable_id =
    g_signal_connect (priv->area, "remove-editable",
		      G_CALLBACK (remove_editable_cb), scaffold);
}

static void
cell_area_scaffold_class_init (CellAreaScaffoldClass *class)
{
  GObjectClass      *gobject_class;
  GtkWidgetClass    *widget_class;
  GtkContainerClass *container_class;

  gobject_class = G_OBJECT_CLASS (class);
  gobject_class->dispose = cell_area_scaffold_dispose;
  gobject_class->finalize = cell_area_scaffold_finalize;

  widget_class = GTK_WIDGET_CLASS (class);
  widget_class->realize = cell_area_scaffold_realize;
  widget_class->unrealize = cell_area_scaffold_unrealize;
  widget_class->draw = cell_area_scaffold_draw;
  widget_class->size_allocate = cell_area_scaffold_size_allocate;
  widget_class->get_preferred_width = cell_area_scaffold_get_preferred_width;
  widget_class->get_preferred_height_for_width = cell_area_scaffold_get_preferred_height_for_width;
  widget_class->get_preferred_height = cell_area_scaffold_get_preferred_height;
  widget_class->get_preferred_width_for_height = cell_area_scaffold_get_preferred_width_for_height;
  widget_class->map = cell_area_scaffold_map;
  widget_class->unmap = cell_area_scaffold_unmap;
  widget_class->focus = cell_area_scaffold_focus;
  widget_class->button_press_event = cell_area_scaffold_button_press;

  container_class = GTK_CONTAINER_CLASS (class);
  container_class->forall = cell_area_scaffold_forall;
  container_class->remove = cell_area_scaffold_remove;

  class->activate = cell_area_scaffold_activate;

  scaffold_signals[ACTIVATE] =
    g_signal_new ("activate",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (CellAreaScaffoldClass, activate),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  widget_class->activate_signal = scaffold_signals[ACTIVATE];


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

  if (priv->context)
    {
      /* Disconnect signals */
      g_signal_handler_disconnect (priv->context, priv->size_changed_id);

      g_object_unref (priv->context);
      priv->context = NULL;
      priv->size_changed_id = 0;
    }

  if (priv->area)
    {
      /* Disconnect signals */
      g_signal_handler_disconnect (priv->area, priv->focus_changed_id);
      g_signal_handler_disconnect (priv->area, priv->add_editable_id);
      g_signal_handler_disconnect (priv->area, priv->remove_editable_id);

      g_object_unref (priv->area);
      priv->area = NULL;
      priv->focus_changed_id = 0;
    }

  G_OBJECT_CLASS (cell_area_scaffold_parent_class)->dispose (object);  
}

/*********************************************************
 *                    GtkWidgetClass                     *
 *********************************************************/
static void
cell_area_scaffold_realize (GtkWidget *widget)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  GtkAllocation            allocation;
  GdkWindow               *window;
  GdkWindowAttr            attributes;
  gint                     attributes_mask;

  gtk_widget_get_allocation (widget, &allocation);

  gtk_widget_set_realized (widget, TRUE);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_KEY_PRESS_MASK |
			    GDK_KEY_RELEASE_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  priv->event_window = gdk_window_new (window, &attributes, attributes_mask);
  gdk_window_set_user_data (priv->event_window, widget);
}

static void
cell_area_scaffold_unrealize (GtkWidget *widget)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;

  if (priv->event_window)
    {
      gdk_window_set_user_data (priv->event_window, NULL);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }
  
  GTK_WIDGET_CLASS (cell_area_scaffold_parent_class)->unrealize (widget);
}

static gboolean
cell_area_scaffold_draw (GtkWidget       *widget,
			 cairo_t         *cr)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  GtkTreeIter              iter;
  gboolean                 valid;
  GdkRectangle             background_area;
  GdkRectangle             render_area;
  GtkAllocation            allocation;
  gint                     i = 0;
  gboolean                 have_focus;
  GtkCellRendererState     flags;

  if (!priv->model)
    return FALSE;

  have_focus  = gtk_widget_has_focus (widget);

  gtk_widget_get_allocation (widget, &allocation);

  render_area.x      = 0;
  render_area.y      = 0;
  render_area.width  = allocation.width;
  render_area.height = allocation.height;

  background_area = render_area;

  render_area.x      = priv->indent;
  render_area.width -= priv->indent;

  valid = gtk_tree_model_get_iter_first (priv->model, &iter);
  while (valid)
    {
      RowData *data = &g_array_index (priv->row_data, RowData, i);

      if (have_focus && i == priv->focus_row)
	flags = GTK_CELL_RENDERER_FOCUSED;
      else
	flags = 0;

      render_area.height     = data->size;

      background_area.height = render_area.height;
      background_area.y      = render_area.y;

      if (i == 0)
	{
	  background_area.height += priv->row_spacing / 2;
	  background_area.height += priv->row_spacing % 2;
	}
      else if (i == priv->row_data->len - 1)
	{
	  background_area.y      -= priv->row_spacing / 2;
	  background_area.height += priv->row_spacing / 2;
	}
      else
	{
	  background_area.y      -= priv->row_spacing / 2;
	  background_area.height += priv->row_spacing;
	}

      gtk_cell_area_apply_attributes (priv->area, priv->model, &iter, FALSE, FALSE);
      gtk_cell_area_render (priv->area, priv->context, widget, cr, 
			    &background_area, &render_area, flags,
			    (have_focus && i == priv->focus_row));

      render_area.y += data->size;
      render_area.y += priv->row_spacing;

      i++;
      valid = gtk_tree_model_iter_next (priv->model, &iter);
    }

  /* Draw the edit widget after drawing everything else */
  GTK_WIDGET_CLASS (cell_area_scaffold_parent_class)->draw (widget, cr);

  return FALSE;
}

static void 
request_all_base (CellAreaScaffold *scaffold)
{
  CellAreaScaffoldPrivate *priv = scaffold->priv;
  GtkWidget               *widget = GTK_WIDGET (scaffold);
  GtkTreeIter              iter;
  gboolean                 valid;

  if (!priv->model)
    return;

  g_signal_handler_block (priv->context, priv->size_changed_id);

  valid = gtk_tree_model_get_iter_first (priv->model, &iter);
  while (valid)
    {
      gint min, nat;

      gtk_cell_area_apply_attributes (priv->area, priv->model, &iter, FALSE, FALSE);
      gtk_cell_area_get_preferred_width (priv->area, priv->context, widget, &min, &nat);

      valid = gtk_tree_model_iter_next (priv->model, &iter);
    }

  g_signal_handler_unblock (priv->context, priv->size_changed_id);
}

static void 
get_row_sizes (CellAreaScaffold *scaffold, 
	       GArray           *array,
	       gint              for_size)
{
  CellAreaScaffoldPrivate *priv = scaffold->priv;
  GtkWidget               *widget = GTK_WIDGET (scaffold);
  GtkTreeIter              iter;
  gboolean                 valid;
  gint                     i = 0;

  if (!priv->model)
    return;

  valid = gtk_tree_model_get_iter_first (priv->model, &iter);
  while (valid)
    {
      RowData *data = &g_array_index (array, RowData, i);

      gtk_cell_area_apply_attributes (priv->area, priv->model, &iter, FALSE, FALSE);
      gtk_cell_area_get_preferred_height_for_width (priv->area, priv->context, widget, 
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

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (priv->event_window,
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);

  /* Allocate the child GtkCellEditable widget if one is currently editing a row */
  if (priv->edit_widget)
    gtk_widget_size_allocate (priv->edit_widget, &priv->edit_rect);

  if (!priv->model)
    return;

  /* Cache the per-row sizes and allocate the context */
  gtk_cell_area_context_allocate (priv->context, allocation->width - priv->indent, -1);
  get_row_sizes (scaffold, priv->row_data, allocation->width - priv->indent);
}


static void
cell_area_scaffold_get_preferred_width (GtkWidget       *widget,
					gint            *minimum_size,
					gint            *natural_size)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;

  if (!priv->model)
    return;

  request_all_base (scaffold);
  gtk_cell_area_context_get_preferred_width (priv->context, minimum_size, natural_size);
  
  *minimum_size += priv->indent;
  *natural_size += priv->indent;
}

static void
cell_area_scaffold_get_preferred_height_for_width (GtkWidget       *widget,
						   gint             for_size,
						   gint            *minimum_size,
						   gint            *natural_size)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  GArray                  *request_array;
  gint                     n_rows, i, full_size = 0;

  if (!priv->model)
    return;

  n_rows = gtk_tree_model_iter_n_children (priv->model, NULL);

  /* Get an array for the contextual request */
  request_array = g_array_new (FALSE, FALSE, sizeof (RowData));
  g_array_set_size (request_array, n_rows);
  memset (request_array->data, 0x0, n_rows * sizeof (RowData));

  /* Gather each contextual size into the request array */
  get_row_sizes (scaffold, request_array, for_size - priv->indent);

  /* Sum up the size and add some row spacing */
  for (i = 0; i < n_rows; i++)
    {
      RowData *data = &g_array_index (request_array, RowData, i);
      
      full_size += data->size;
    }
  
  full_size += MAX (0, n_rows -1) * priv->row_spacing;
  
  g_array_free (request_array, TRUE);
  
  *minimum_size = full_size;
  *natural_size = full_size;
}

static void
cell_area_scaffold_get_preferred_height (GtkWidget       *widget,
					 gint            *minimum_size,
					 gint            *natural_size)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  gint                     min_size, nat_size;

  if (!priv->model)
    return;

  GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, &min_size, &nat_size);
  GTK_WIDGET_GET_CLASS (widget)->get_preferred_height_for_width (widget, min_size, 
								 minimum_size, natural_size);
}

static void
cell_area_scaffold_get_preferred_width_for_height (GtkWidget       *widget,
						   gint             for_size,
						   gint            *minimum_size,
						   gint            *natural_size)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;

  if (!priv->model)
    return;

  GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, minimum_size, natural_size);
}

static void
cell_area_scaffold_map (GtkWidget       *widget)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  
  GTK_WIDGET_CLASS (cell_area_scaffold_parent_class)->map (widget);

  if (priv->event_window)
    gdk_window_show (priv->event_window);
}

static void
cell_area_scaffold_unmap (GtkWidget       *widget)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  
  GTK_WIDGET_CLASS (cell_area_scaffold_parent_class)->unmap (widget);

  if (priv->event_window)
    gdk_window_hide (priv->event_window);
}


static gint
cell_area_scaffold_focus (GtkWidget       *widget,
			  GtkDirectionType direction)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  GtkTreeIter              iter;
  gboolean                 valid;
  gint                     focus_row;
  gboolean                 changed = FALSE;

  /* Grab focus on ourself if we dont already have focus */
  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  /* Move focus from cell to cell and row to row */
  focus_row = priv->focus_row;

  g_signal_handler_block (priv->area, priv->focus_changed_id);
  
  valid = gtk_tree_model_iter_nth_child (priv->model, &iter, NULL, priv->focus_row);
  while (valid)
    {
      gtk_cell_area_apply_attributes (priv->area, priv->model, &iter, FALSE, FALSE);
      
      /* If focus stays in the area we dont need to do any more */
      if (gtk_cell_area_focus (priv->area, direction))
	{
	  priv->focus_row = focus_row;

	  /* XXX A smarter implementation would only invalidate the rectangles where
	   * focus was removed from and new focus was placed */
	  gtk_widget_queue_draw (widget);
	  changed = TRUE;
	  break;
	}
      else
	{
	  if (direction == GTK_DIR_RIGHT ||
	      direction == GTK_DIR_LEFT)
	    break;
	  else if (direction == GTK_DIR_UP ||
		   direction == GTK_DIR_TAB_BACKWARD)
	    {
	      if (focus_row == 0)
		break;
	      else
		{
		  /* XXX A real implementation should check if the
		   * previous row can focus with it's attributes setup */
		  focus_row--;
		  valid = gtk_tree_model_iter_nth_child (priv->model, &iter, NULL, focus_row);
		}
	    }
	  else /* direction == GTK_DIR_DOWN || GTK_DIR_TAB_FORWARD */
	    {
	      if (focus_row == priv->row_data->len - 1)
		break;
	      else
		{
		  /* XXX A real implementation should check if the
		   * previous row can focus with it's attributes setup */
		  focus_row++;
		  valid = gtk_tree_model_iter_next (priv->model, &iter);
		}
	    }
	}
    }

  g_signal_handler_unblock (priv->area, priv->focus_changed_id);

  /* XXX A smarter implementation would only invalidate the rectangles where
   * focus was removed from and new focus was placed */
  gtk_widget_queue_draw (widget);

  return changed;
}

static gboolean
cell_area_scaffold_button_press (GtkWidget       *widget,
				 GdkEventButton  *event)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (widget);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  GtkTreeIter              iter;
  gboolean                 valid;
  gint                     i = 0;
  GdkRectangle             event_area;
  GtkAllocation            allocation;
  gboolean                 handled = FALSE;

  gtk_widget_get_allocation (widget, &allocation);

  event_area.x      = 0;
  event_area.y      = 0;
  event_area.width  = allocation.width;
  event_area.height = allocation.height;

  event_area.x      = priv->indent;
  event_area.width -= priv->indent;

  valid = gtk_tree_model_get_iter_first (priv->model, &iter);
  while (valid)
    {
      RowData *data = &g_array_index (priv->row_data, RowData, i);

      event_area.height = data->size;

      if (event->y >= event_area.y && 
	  event->y <= event_area.y + event_area.height)
	{
	  /* XXX A real implementation would assemble GtkCellRendererState flags here */
	  gtk_cell_area_apply_attributes (priv->area, priv->model, &iter, FALSE, FALSE);
	  handled = gtk_cell_area_event (priv->area, priv->context, GTK_WIDGET (scaffold),
					 (GdkEvent *)event, &event_area, 0);
	  break;
	}
      
      event_area.y += data->size;
      event_area.y += priv->row_spacing;
      
      i++;
      valid = gtk_tree_model_iter_next (priv->model, &iter);
    }

  return handled;
}


/*********************************************************
 *                   GtkContainerClass                   *
 *********************************************************/
static void
cell_area_scaffold_put_edit_widget (CellAreaScaffold *scaffold,
				    GtkWidget        *edit_widget,
				    gint              x,
				    gint              y,
				    gint              width,
				    gint              height)
{
  CellAreaScaffoldPrivate *priv = scaffold->priv;

  priv->edit_rect.x      = x;
  priv->edit_rect.y      = y;
  priv->edit_rect.width  = width;
  priv->edit_rect.height = height;
  priv->edit_widget      = edit_widget;

  gtk_widget_set_parent (edit_widget, GTK_WIDGET (scaffold));
}

static void
cell_area_scaffold_forall (GtkContainer    *container,
			   gboolean         include_internals,
			   GtkCallback      callback,
			   gpointer         callback_data)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (container);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;

  if (priv->edit_widget)
    (* callback) (priv->edit_widget, callback_data);
}

static void
cell_area_scaffold_remove (GtkContainer    *container,
			   GtkWidget       *child)
{
  CellAreaScaffold        *scaffold = CELL_AREA_SCAFFOLD (container);
  CellAreaScaffoldPrivate *priv     = scaffold->priv;

  g_return_if_fail (child == priv->edit_widget);

  gtk_widget_unparent (priv->edit_widget);
  priv->edit_widget = NULL;
}

/*********************************************************
 *                CellAreaScaffoldClass                  *
 *********************************************************/
static void
cell_area_scaffold_activate (CellAreaScaffold *scaffold)
{
  CellAreaScaffoldPrivate *priv     = scaffold->priv;
  GtkWidget               *widget   = GTK_WIDGET (scaffold);
  GtkAllocation            allocation;
  GdkRectangle             cell_area;
  GtkTreeIter              iter;
  gboolean                 valid;
  gint                     i = 0;

  gtk_widget_get_allocation (widget, &allocation);

  cell_area.x = 0;
  cell_area.y = 0;
  cell_area.width  = allocation.width;
  cell_area.height = allocation.height;

  cell_area.x      = priv->indent;
  cell_area.width -= priv->indent;

  valid = gtk_tree_model_get_iter_first (priv->model, &iter);
  while (valid)
    {
      RowData *data = &g_array_index (priv->row_data, RowData, i);

      if (i == priv->focus_row)
	{
	  cell_area.height = data->size;

	  gtk_cell_area_apply_attributes (priv->area, priv->model, &iter, FALSE, FALSE);
	  gtk_cell_area_activate (priv->area, priv->context, widget, &cell_area, 
				  GTK_CELL_RENDERER_FOCUSED, FALSE);

	  break;
	}

      cell_area.y += data->size + priv->row_spacing;

      i++;
      valid = gtk_tree_model_iter_next (priv->model, &iter);
    }
}

/*********************************************************
 *           CellArea/GtkTreeModel callbacks             *
 *********************************************************/
static void
size_changed_cb (GtkCellAreaContext  *context,
		 GParamSpec          *pspec,
		 CellAreaScaffold    *scaffold)
{
  if (!strcmp (pspec->name, "minimum-width") ||
      !strcmp (pspec->name, "natural-width") ||
      !strcmp (pspec->name, "minimum-height") ||
      !strcmp (pspec->name, "natural-height"))
    gtk_widget_queue_resize (GTK_WIDGET (scaffold));
}

static void
focus_changed_cb (GtkCellArea      *area,
		  GtkCellRenderer  *renderer,
		  const gchar      *path,
		  CellAreaScaffold *scaffold)
{
  CellAreaScaffoldPrivate *priv = scaffold->priv;
  GtkWidget               *widget = GTK_WIDGET (scaffold);
  GtkTreePath             *treepath;
  gint                    *indices;

  if (!priv->model)
    return;

  /* We can be signaled that a renderer lost focus, here
   * we dont care */
  if (!renderer)
    return;
  
  treepath = gtk_tree_path_new_from_string (path);
  indices = gtk_tree_path_get_indices (treepath);

  priv->focus_row = indices[0];

  gtk_tree_path_free (treepath);

  /* Make sure we have focus now */
  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  gtk_widget_queue_draw (widget);
}

static void
add_editable_cb (GtkCellArea      *area,
		 GtkCellRenderer  *renderer,
		 GtkCellEditable  *edit_widget,
		 GdkRectangle     *cell_area,
		 const gchar      *path,
		 CellAreaScaffold *scaffold)
{
  GtkAllocation allocation;

  gtk_widget_get_allocation (GTK_WIDGET (scaffold), &allocation);

  cell_area_scaffold_put_edit_widget (scaffold, GTK_WIDGET (edit_widget),
				      allocation.x + cell_area->x, 
				      allocation.y + cell_area->y, 
				      cell_area->width, cell_area->height);
}

static void
remove_editable_cb (GtkCellArea      *area,
		    GtkCellRenderer  *renderer,
		    GtkCellEditable  *edit_widget,
		    CellAreaScaffold *scaffold)
{
  gtk_container_remove (GTK_CONTAINER (scaffold), GTK_WIDGET (edit_widget));

  gtk_widget_grab_focus (GTK_WIDGET (scaffold));
}

static void 
rebuild_and_reset_internals (CellAreaScaffold *scaffold)
{
  CellAreaScaffoldPrivate *priv = scaffold->priv;
  gint n_rows;

  if (priv->model)
    {
      n_rows = gtk_tree_model_iter_n_children (priv->model, NULL);

      /* Clear/reset the array */
      g_array_set_size (priv->row_data, n_rows);
      memset (priv->row_data->data, 0x0, n_rows * sizeof (RowData));
    }
  else
    g_array_set_size (priv->row_data, 0);

  /* Data changed, lets reset the context and consequently queue resize and
   * start everything over again (note this is definitly far from optimized) */
  gtk_cell_area_context_reset (priv->context);
}

static void
row_changed_cb (GtkTreeModel     *model,
		GtkTreePath      *path,
		GtkTreeIter      *iter,
		CellAreaScaffold *scaffold)
{
  rebuild_and_reset_internals (scaffold);
}

static void
row_inserted_cb (GtkTreeModel     *model,
		 GtkTreePath      *path,
		 GtkTreeIter      *iter,
		 CellAreaScaffold *scaffold)
{
  rebuild_and_reset_internals (scaffold);
}

static void
row_deleted_cb (GtkTreeModel     *model,
		GtkTreePath      *path,
		CellAreaScaffold *scaffold)
{
  rebuild_and_reset_internals (scaffold);
}

static void
rows_reordered_cb (GtkTreeModel     *model,
		   GtkTreePath      *parent,
		   GtkTreeIter      *iter,
		   gint             *new_order,
		   CellAreaScaffold *scaffold)
{
  rebuild_and_reset_internals (scaffold);
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
	  g_signal_handler_disconnect (priv->model, priv->row_changed_id);
	  g_signal_handler_disconnect (priv->model, priv->row_inserted_id);
	  g_signal_handler_disconnect (priv->model, priv->row_deleted_id);
	  g_signal_handler_disconnect (priv->model, priv->rows_reordered_id);

	  g_object_unref (priv->model);
	}

      priv->model = model;

      if (priv->model)
	{
	  g_object_ref (priv->model);

	  priv->row_changed_id = 
	    g_signal_connect (priv->model, "row-changed",
			      G_CALLBACK (row_changed_cb), scaffold);

	  priv->row_inserted_id = 
	    g_signal_connect (priv->model, "row-inserted",
			      G_CALLBACK (row_inserted_cb), scaffold);

	  priv->row_deleted_id = 
	    g_signal_connect (priv->model, "row-deleted",
			      G_CALLBACK (row_deleted_cb), scaffold);

	  priv->rows_reordered_id = 
	    g_signal_connect (priv->model, "rows-reordered",
			      G_CALLBACK (rows_reordered_cb), scaffold);
	}

      rebuild_and_reset_internals (scaffold);
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


void
cell_area_scaffold_set_row_spacing (CellAreaScaffold *scaffold,
				    gint              spacing)
{
  CellAreaScaffoldPrivate *priv;
  
  g_return_if_fail (IS_CELL_AREA_SCAFFOLD (scaffold));

  priv = scaffold->priv;

  if (priv->row_spacing != spacing)
    {
      priv->row_spacing = spacing;
      gtk_widget_queue_resize (GTK_WIDGET (scaffold));
    }
}

gint
cell_area_scaffold_get_row_spacing (CellAreaScaffold *scaffold)
{
  CellAreaScaffoldPrivate *priv;
  
  g_return_val_if_fail (IS_CELL_AREA_SCAFFOLD (scaffold), 0);

  priv = scaffold->priv;

  return priv->row_spacing;
}

void
cell_area_scaffold_set_indentation (CellAreaScaffold *scaffold,
				    gint              indent)
{
  CellAreaScaffoldPrivate *priv;
  
  g_return_if_fail (IS_CELL_AREA_SCAFFOLD (scaffold));

  priv = scaffold->priv;

  if (priv->indent != indent)
    {
      priv->indent = indent;
      gtk_widget_queue_resize (GTK_WIDGET (scaffold));
    }
}

gint
cell_area_scaffold_get_indentation (CellAreaScaffold *scaffold)
{
  CellAreaScaffoldPrivate *priv;
  
  g_return_val_if_fail (IS_CELL_AREA_SCAFFOLD (scaffold), 0);

  priv = scaffold->priv;

  return priv->indent;
}


