/* gtktreeview.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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


#include "gtktreeview.h"
#include "gtkrbtree.h"
#include "gtktreednd.h"
#include "gtktreeprivate.h"
#include "gtkcellrenderer.h"
#include "gtksignal.h"
#include "gtkmain.h"
#include "gtkbutton.h"
#include "gtkalignment.h"
#include "gtklabel.h"
#include "gtkhbox.h"
#include "gtkarrow.h"

#include <gdk/gdkkeysyms.h>


/* the width of the column resize windows */
#define TREE_VIEW_DRAG_WIDTH 6
#define TREE_VIEW_EXPANDER_WIDTH 14
#define TREE_VIEW_EXPANDER_HEIGHT 14

/* The TREE_VIEW_VERTICAL_SEPARATOR is the space between rows. The
 * GTK_RBNODE_GET_HEIGHT() value includes the separators; the offset
 * of each row points to the center of the inter-row space. For an odd
 * separator, the extra pixel by convention goes _below_ the row.  So
 * a node in the rbtree owns TREE_VIEW_VERTICAL_SEPARATOR/2 pixels
 * above the row, and TREE_VIEW_VERTICAL_SEPARATOR/2 +
 * TREE_VIEW_VERTICAL_SEPARATOR%2 pixels below the row. 
 */

#define TREE_VIEW_VERTICAL_SEPARATOR 2
#define TREE_VIEW_HORIZONTAL_SEPARATOR 0

/* The "background" areas of all rows/cells add up to cover the entire tree.
 * The background includes all inter-row and inter-cell spacing.
 * The "cell" areas are the cell_area passed in to gtk_cell_renderer_render(),
 * i.e. just the cells, no spacing.
 */

#define BACKGROUND_FIRST_PIXEL(tree_view,tree,node) (_gtk_rbtree_node_find_offset ((tree), (node)) + TREE_VIEW_HEADER_HEIGHT ((tree_view)))
#define CELL_FIRST_PIXEL(tree_view,tree,node) (BACKGROUND_FIRST_PIXEL (tree_view,tree,node) + TREE_VIEW_VERTICAL_SEPARATOR/2)

#define BACKGROUND_HEIGHT(node) (GTK_RBNODE_GET_HEIGHT (node))
#define CELL_HEIGHT(node) (BACKGROUND_HEIGHT (node) - TREE_VIEW_VERTICAL_SEPARATOR);

#define TREE_WINDOW_Y_TO_RBTREE_Y(tree_view,y) ((y) - TREE_VIEW_HEADER_HEIGHT (tree_view))
#define RBTREE_Y_TO_TREE_WINDOW_Y(tree_view,y) ((y) + TREE_VIEW_HEADER_HEIGHT (tree_view))

typedef struct _GtkTreeViewChild GtkTreeViewChild;

struct _GtkTreeViewChild
{
  GtkWidget *widget;
  gint x;
  gint y;
};


static void     gtk_tree_view_init                 (GtkTreeView      *tree_view);
static void     gtk_tree_view_class_init           (GtkTreeViewClass *klass);

/* object signals */
static void     gtk_tree_view_finalize             (GObject          *object);

/* widget signals */
static void     gtk_tree_view_setup_model          (GtkTreeView      *tree_view);
static void     gtk_tree_view_realize              (GtkWidget        *widget);
static void     gtk_tree_view_unrealize            (GtkWidget        *widget);
static void     gtk_tree_view_map                  (GtkWidget        *widget);
static void     gtk_tree_view_size_request         (GtkWidget        *widget,
						    GtkRequisition   *requisition);
static void     gtk_tree_view_size_allocate        (GtkWidget        *widget,
						    GtkAllocation    *allocation);
static gboolean gtk_tree_view_expose               (GtkWidget        *widget,
						    GdkEventExpose   *event);
static gboolean gtk_tree_view_motion               (GtkWidget        *widget,
						    GdkEventMotion   *event);
static gboolean gtk_tree_view_enter_notify         (GtkWidget        *widget,
						    GdkEventCrossing *event);
static gboolean gtk_tree_view_leave_notify         (GtkWidget        *widget,
						    GdkEventCrossing *event);
static gboolean gtk_tree_view_button_press         (GtkWidget        *widget,
						    GdkEventButton   *event);
static gboolean gtk_tree_view_button_release       (GtkWidget        *widget,
						    GdkEventButton   *event);
static void     gtk_tree_view_draw_focus           (GtkWidget        *widget);
static gint     gtk_tree_view_focus_in             (GtkWidget        *widget,
						    GdkEventFocus    *event);
static gint     gtk_tree_view_focus_out            (GtkWidget        *widget,
						    GdkEventFocus    *event);
static gint     gtk_tree_view_focus                (GtkContainer     *container,
						    GtkDirectionType  direction);

/* container signals */
static void     gtk_tree_view_remove               (GtkContainer     *container,
						    GtkWidget        *widget);
static void     gtk_tree_view_forall               (GtkContainer     *container,
						    gboolean          include_internals,
						    GtkCallback       callback,
						    gpointer          callback_data);

/* Source side drag signals */
static void gtk_tree_view_drag_begin       (GtkWidget        *widget,
                                            GdkDragContext   *context);
static void gtk_tree_view_drag_end         (GtkWidget        *widget,
                                            GdkDragContext   *context);
static void gtk_tree_view_drag_data_get    (GtkWidget        *widget,
                                            GdkDragContext   *context,
                                            GtkSelectionData *selection_data,
                                            guint             info,
                                            guint             time);
static void gtk_tree_view_drag_data_delete (GtkWidget        *widget,
                                            GdkDragContext   *context);

/* Target side drag signals */
static void     gtk_tree_view_drag_leave         (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  guint             time);
static gboolean gtk_tree_view_drag_motion        (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  guint             time);
static gboolean gtk_tree_view_drag_drop          (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  guint             time);
static void     gtk_tree_view_drag_data_received (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  GtkSelectionData *selection_data,
                                                  guint             info,
                                                  guint             time);

/* tree_model signals */
static void     gtk_tree_view_set_adjustments      (GtkTreeView      *tree_view,
						    GtkAdjustment    *hadj,
						    GtkAdjustment    *vadj);
static void     gtk_tree_view_changed              (GtkTreeModel     *model,
						    GtkTreePath      *path,
						    GtkTreeIter      *iter,
						    gpointer          data);
static void     gtk_tree_view_inserted             (GtkTreeModel     *model,
						    GtkTreePath      *path,
						    GtkTreeIter      *iter,
						    gpointer          data);
static void     gtk_tree_view_child_toggled        (GtkTreeModel     *model,
						    GtkTreePath      *path,
						    GtkTreeIter      *iter,
						    gpointer          data);
static void     gtk_tree_view_deleted              (GtkTreeModel     *model,
						    GtkTreePath      *path,
						    gpointer          data);

/* Internal functions */
static void     gtk_tree_view_queue_draw_node      (GtkTreeView      *tree_view,
						    GtkRBTree        *tree,
						    GtkRBNode        *node,
						    GdkRectangle     *clip_rect);
static void     gtk_tree_view_queue_draw_path      (GtkTreeView      *tree_view,
                                                    GtkTreePath      *path,
                                                    GdkRectangle     *clip_rect);
static void     gtk_tree_view_draw_arrow           (GtkTreeView      *tree_view,
                                                    GtkRBTree        *tree,
						    GtkRBNode        *node,
						    gint              x,
						    gint              y);
static void     gtk_tree_view_get_arrow_xrange     (GtkTreeView      *tree_view,
                                                    gint              *x1,
                                                    gint              *x2);
static gint     gtk_tree_view_new_column_width     (GtkTreeView      *tree_view,
						    gint              i,
						    gint             *x);
static void     gtk_tree_view_adjustment_changed   (GtkAdjustment    *adjustment,
						    GtkTreeView      *tree_view);
static gint     gtk_tree_view_insert_iter_height   (GtkTreeView      *tree_view,
						    GtkRBTree        *tree,
						    GtkTreeIter      *iter,
						    gint              depth);
static void     gtk_tree_view_build_tree           (GtkTreeView      *tree_view,
						    GtkRBTree        *tree,
						    GtkTreeIter      *iter,
						    gint              depth,
						    gboolean          recurse,
						    gboolean          calc_bounds);
static void     gtk_tree_view_calc_size            (GtkTreeView      *priv,
						    GtkRBTree        *tree,
						    GtkTreeIter      *iter,
						    gint              depth);
static gboolean gtk_tree_view_discover_dirty_iter  (GtkTreeView      *tree_view,
						    GtkTreeIter      *iter,
						    gint              depth,
						    gint             *height);
static void     gtk_tree_view_discover_dirty       (GtkTreeView      *tree_view,
						    GtkRBTree        *tree,
						    GtkTreeIter      *iter,
						    gint              depth);
static void     gtk_tree_view_check_dirty          (GtkTreeView      *tree_view);
static void     gtk_tree_view_create_button        (GtkTreeView      *tree_view,
						    gint              i);
static void     gtk_tree_view_create_buttons       (GtkTreeView      *tree_view);
static void     gtk_tree_view_button_clicked       (GtkWidget        *widget,
						    gpointer          data);
static void     gtk_tree_view_clamp_node_visible   (GtkTreeView      *tree_view,
						    GtkRBTree        *tree,
						    GtkRBNode        *node);
static gboolean gtk_tree_view_maybe_begin_dragging_row (GtkTreeView      *tree_view,
                                                        GdkEventMotion   *event);

static GtkContainerClass *parent_class = NULL;


/* Class Functions */
GtkType
gtk_tree_view_get_type (void)
{
  static GtkType tree_view_type = 0;

  if (!tree_view_type)
    {
      static const GTypeInfo tree_view_info =
      {
        sizeof (GtkTreeViewClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_tree_view_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkTreeView),
	0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_tree_view_init
      };

      tree_view_type = g_type_register_static (GTK_TYPE_CONTAINER, "GtkTreeView", &tree_view_info, 0);
    }

  return tree_view_type;
}

static void
gtk_tree_view_class_init (GtkTreeViewClass *class)
{
  GObjectClass *o_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  o_class = (GObjectClass *) class;
  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  parent_class = g_type_class_peek_parent (class);

  o_class->finalize = gtk_tree_view_finalize;

  widget_class->realize = gtk_tree_view_realize;
  widget_class->unrealize = gtk_tree_view_unrealize;
  widget_class->map = gtk_tree_view_map;
  widget_class->size_request = gtk_tree_view_size_request;
  widget_class->size_allocate = gtk_tree_view_size_allocate;
  widget_class->expose_event = gtk_tree_view_expose;
  widget_class->motion_notify_event = gtk_tree_view_motion;
  widget_class->enter_notify_event = gtk_tree_view_enter_notify;
  widget_class->leave_notify_event = gtk_tree_view_leave_notify;
  widget_class->button_press_event = gtk_tree_view_button_press;
  widget_class->button_release_event = gtk_tree_view_button_release;
  widget_class->draw_focus = gtk_tree_view_draw_focus;
  widget_class->focus_in_event = gtk_tree_view_focus_in;
  widget_class->focus_out_event = gtk_tree_view_focus_out;

  widget_class->drag_begin = gtk_tree_view_drag_begin;
  widget_class->drag_end = gtk_tree_view_drag_end;
  widget_class->drag_data_get = gtk_tree_view_drag_data_get;
  widget_class->drag_data_delete = gtk_tree_view_drag_data_delete;

  widget_class->drag_leave = gtk_tree_view_drag_leave;
  widget_class->drag_motion = gtk_tree_view_drag_motion;
  widget_class->drag_drop = gtk_tree_view_drag_drop;
  widget_class->drag_data_received = gtk_tree_view_drag_data_received;
  
  container_class->forall = gtk_tree_view_forall;
  container_class->remove = gtk_tree_view_remove;
  container_class->focus = gtk_tree_view_focus;

  class->set_scroll_adjustments = gtk_tree_view_set_adjustments;

  widget_class->set_scroll_adjustments_signal =
    gtk_signal_new ("set_scroll_adjustments",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTreeViewClass, set_scroll_adjustments),
		    gtk_marshal_VOID__POINTER_POINTER,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);
}

static void
gtk_tree_view_init (GtkTreeView *tree_view)
{
  tree_view->priv = g_new0 (GtkTreeViewPrivate, 1);

  GTK_WIDGET_SET_FLAGS (tree_view, GTK_CAN_FOCUS);

  tree_view->priv->flags = GTK_TREE_VIEW_IS_LIST | GTK_TREE_VIEW_SHOW_EXPANDERS | GTK_TREE_VIEW_DRAW_KEYFOCUS | GTK_TREE_VIEW_HEADERS_VISIBLE;
  tree_view->priv->tab_offset = TREE_VIEW_EXPANDER_WIDTH;
  tree_view->priv->n_columns = 0;
  tree_view->priv->columns = NULL;
  tree_view->priv->button_pressed_node = NULL;
  tree_view->priv->button_pressed_tree = NULL;
  tree_view->priv->prelight_node = NULL;
  tree_view->priv->header_height = 1;
  tree_view->priv->x_drag = 0;
  tree_view->priv->drag_pos = -1;
  tree_view->priv->selection = NULL;
  tree_view->priv->anchor = NULL;
  tree_view->priv->cursor = NULL;

  tree_view->priv->pressed_button = -1;
  tree_view->priv->press_start_x = -1;
  tree_view->priv->press_start_y = -1;
  
  gtk_tree_view_set_adjustments (tree_view, NULL, NULL);
  _gtk_tree_view_set_size (tree_view, 0, 0);
}


/* Object methods
 */

static void
gtk_tree_view_finalize (GObject *object)
{
  GtkTreeView *tree_view = (GtkTreeView *) object;

  if (tree_view->priv->tree)
    _gtk_rbtree_free (tree_view->priv->tree);

  if (tree_view->priv->scroll_to_path != NULL)
    gtk_tree_path_free (tree_view->priv->scroll_to_path);

  if (tree_view->priv->drag_dest_row)
    gtk_tree_row_reference_free (tree_view->priv->drag_dest_row);
  
  g_free (tree_view->priv);
  if (G_OBJECT_CLASS (parent_class)->finalize)
    (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Widget methods
 */

static void
gtk_tree_view_realize_buttons (GtkTreeView *tree_view)
{
  GList *list;
  GtkTreeViewColumn *column;
  GdkWindowAttr attr;
  guint attributes_mask;

  g_return_if_fail (GTK_WIDGET_REALIZED (tree_view));
  g_return_if_fail (tree_view->priv->header_window != NULL);

  attr.window_type = GDK_WINDOW_CHILD;
  attr.wclass = GDK_INPUT_ONLY;
  attr.visual = gtk_widget_get_visual (GTK_WIDGET (tree_view));
  attr.colormap = gtk_widget_get_colormap (GTK_WIDGET (tree_view));
  attr.event_mask = gtk_widget_get_events (GTK_WIDGET (tree_view));
  attr.event_mask = (GDK_BUTTON_PRESS_MASK |
		     GDK_BUTTON_RELEASE_MASK |
		     GDK_POINTER_MOTION_MASK |
		     GDK_POINTER_MOTION_HINT_MASK |
		     GDK_KEY_PRESS_MASK);
  attributes_mask = GDK_WA_CURSOR | GDK_WA_X | GDK_WA_Y;
  attr.cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
  tree_view->priv->cursor_drag = attr.cursor;

  attr.y = 0;
  attr.width = TREE_VIEW_DRAG_WIDTH;
  attr.height = tree_view->priv->header_height;

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      column = list->data;
      if (column->button)
	{
	  if (column->visible == FALSE)
	    continue;
	  if (column->window != NULL)
	    continue;

	  gtk_widget_set_parent_window (column->button,
					tree_view->priv->header_window);

	  attr.x = (column->button->allocation.x + column->button->allocation.width) - 3;
          
	  column->window = gdk_window_new (tree_view->priv->header_window,
					   &attr, attributes_mask);
	  gdk_window_set_user_data (column->window, tree_view);
	}
    }
}

static void
gtk_tree_view_realize (GtkWidget *widget)
{
  GList *tmp_list;
  GtkTreeView *tree_view;
  GdkGCValues values;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (widget));

  tree_view = GTK_TREE_VIEW (widget);

  gtk_tree_view_check_dirty (GTK_TREE_VIEW (widget));
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  /* Make the main, clipping window */
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  /* Make the window for the tree */
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = tree_view->priv->width;
  attributes.height = tree_view->priv->height + TREE_VIEW_HEADER_HEIGHT (tree_view);
  attributes.event_mask = GDK_EXPOSURE_MASK |
    GDK_SCROLL_MASK |
    GDK_POINTER_MOTION_MASK |
    GDK_ENTER_NOTIFY_MASK |
    GDK_LEAVE_NOTIFY_MASK |
    GDK_BUTTON_PRESS_MASK |
    GDK_BUTTON_RELEASE_MASK |
    gtk_widget_get_events (widget);

  tree_view->priv->bin_window = gdk_window_new (widget->window,
						&attributes, attributes_mask);
  gdk_window_set_user_data (tree_view->priv->bin_window, widget);

  /* Make the column header window */
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = MAX (tree_view->priv->width, widget->allocation.width);
  attributes.height = tree_view->priv->header_height;
  attributes.event_mask = (GDK_EXPOSURE_MASK |
			   GDK_SCROLL_MASK |
			   GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK |
			   GDK_KEY_PRESS_MASK |
			   GDK_KEY_RELEASE_MASK) |
    gtk_widget_get_events (widget);

  tree_view->priv->header_window = gdk_window_new (widget->window,
						   &attributes, attributes_mask);
  gdk_window_set_user_data (tree_view->priv->header_window, widget);


  values.foreground = (widget->style->white.pixel==0 ?
		       widget->style->black:widget->style->white);
  values.function = GDK_XOR;
  values.subwindow_mode = GDK_INCLUDE_INFERIORS;
  tree_view->priv->xor_gc = gdk_gc_new_with_values (widget->window,
						    &values,
						    GDK_GC_FOREGROUND |
						    GDK_GC_FUNCTION |
						    GDK_GC_SUBWINDOW);
  /* Add them all up. */
  widget->style = gtk_style_attach (widget->style, widget->window);
  gdk_window_set_background (widget->window, &widget->style->base[widget->state]);
  gdk_window_set_background (tree_view->priv->bin_window, &widget->style->base[widget->state]);
  gtk_style_set_background (widget->style, tree_view->priv->header_window, GTK_STATE_NORMAL);

  tmp_list = tree_view->priv->children;
  while (tmp_list)
    {
      GtkTreeViewChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      gtk_widget_set_parent_window (child->widget, tree_view->priv->bin_window);
    }
  gtk_tree_view_realize_buttons (GTK_TREE_VIEW (widget));
  _gtk_tree_view_set_size (GTK_TREE_VIEW (widget), -1, -1);

  if (tree_view->priv->scroll_to_path != NULL ||
      tree_view->priv->scroll_to_column != NULL)
    {
      gtk_tree_view_scroll_to_cell (tree_view,
				    tree_view->priv->scroll_to_path,
				    tree_view->priv->scroll_to_column,
				    tree_view->priv->scroll_to_row_align,
				    tree_view->priv->scroll_to_col_align);
      if (tree_view->priv->scroll_to_path)
	{
	  gtk_tree_path_free (tree_view->priv->scroll_to_path);
	  tree_view->priv->scroll_to_path = NULL;
	}
      tree_view->priv->scroll_to_column = NULL;
    }
}

static void
gtk_tree_view_unrealize (GtkWidget *widget)
{
  GtkTreeView *tree_view;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (widget));

  tree_view = GTK_TREE_VIEW (widget);

  if (tree_view->priv->scroll_timeout != 0)
    {
      gtk_timeout_remove (tree_view->priv->scroll_timeout);
      tree_view->priv->scroll_timeout = 0;
    }

  if (tree_view->priv->open_dest_timeout != 0)
    {
      gtk_timeout_remove (tree_view->priv->open_dest_timeout);
      tree_view->priv->open_dest_timeout = 0;
    }
  
  /* FIXME where do we clear column->window for each column? */
  
  gdk_window_set_user_data (tree_view->priv->bin_window, NULL);
  gdk_window_destroy (tree_view->priv->bin_window);
  tree_view->priv->bin_window = NULL;

  gdk_window_set_user_data (tree_view->priv->header_window, NULL);
  gdk_window_destroy (tree_view->priv->header_window);
  tree_view->priv->header_window = NULL;

  gdk_cursor_destroy (tree_view->priv->cursor_drag);
  gdk_gc_destroy (tree_view->priv->xor_gc);  
  
  /* GtkWidget::unrealize destroys children and widget->window */
  
  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_tree_view_map_buttons (GtkTreeView *tree_view)
{
  GList *list;

  g_return_if_fail (GTK_WIDGET_MAPPED (tree_view));
  
  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE))
    {
      GtkTreeViewColumn *column;
      
      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  column = list->data;
          if (GTK_WIDGET_VISIBLE (column->button) &&
              !GTK_WIDGET_MAPPED (column->button))
            gtk_widget_map (column->button);
	}
      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  column = list->data;
	  if (column->visible == FALSE)
	    continue;
	  if (column->column_type == GTK_TREE_VIEW_COLUMN_RESIZEABLE)
	    {
	      gdk_window_raise (column->window);
	      gdk_window_show (column->window);
	    }
	  else
	    gdk_window_hide (column->window);
	}
      gdk_window_show (tree_view->priv->header_window);
    }
}

static void
gtk_tree_view_map (GtkWidget *widget)
{
  GList *tmp_list;
  GtkTreeView *tree_view;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (widget));

  tree_view = GTK_TREE_VIEW (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  tmp_list = tree_view->priv->children;
  while (tmp_list)
    {
      GtkTreeViewChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
	{
	  if (!GTK_WIDGET_MAPPED (child->widget))
	    gtk_widget_map (child->widget);
	}
    }
  gdk_window_show (tree_view->priv->bin_window);

  gtk_tree_view_map_buttons (tree_view);
  
  gdk_window_show (widget->window);
}

static void
gtk_tree_view_size_request_buttons (GtkTreeView *tree_view)
{
  GList *list;
  
  tree_view->priv->header_height = 1;

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_MODEL_SETUP))
    {
      for (list = tree_view->priv->columns; list; list = list->next)
        {
          GtkRequisition requisition;
          GtkTreeViewColumn *column;
          
          column = list->data;
          
          gtk_widget_size_request (column->button, &requisition);
          
          gtk_tree_view_column_set_width (column,
                                          MAX (column->width, requisition.width));
          tree_view->priv->header_height = MAX (tree_view->priv->header_height, requisition.height);
        }
    }
}

static void
gtk_tree_view_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkTreeView *tree_view;
  GList *tmp_list;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (widget));

  tree_view = GTK_TREE_VIEW (widget);

  requisition->width = 200;
  requisition->height = 200;

  tmp_list = tree_view->priv->children;

  while (tmp_list)
    {
      GtkTreeViewChild *child = tmp_list->data;
      GtkRequisition child_requisition;

      tmp_list = tmp_list->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
        gtk_widget_size_request (child->widget, &child_requisition);
    }

  gtk_tree_view_size_request_buttons (tree_view);
}

static void
gtk_tree_view_size_allocate_buttons (GtkWidget *widget)
{
  GtkTreeView *tree_view;
  GList *list;
  GList *last_column;
  GtkTreeViewColumn *column;
  GtkAllocation allocation;
  gint width = 0;

  tree_view = GTK_TREE_VIEW (widget);

  allocation.y = 0;
  allocation.height = tree_view->priv->header_height;

  for (last_column = g_list_last (tree_view->priv->columns);
       last_column && !(GTK_TREE_VIEW_COLUMN (last_column->data)->visible);
       last_column = last_column->prev)
    ;

  if (last_column == NULL)
    return;

  for (list = tree_view->priv->columns; list != last_column; list = list->next)
    {
      column = list->data;

      if (!column->visible)
	continue;

      allocation.x = width;
      allocation.width = column->width;
      width += column->width;
      gtk_widget_size_allocate (column->button, &allocation);

      if (column->window)
	gdk_window_move_resize (column->window,
                                width - TREE_VIEW_DRAG_WIDTH/2, allocation.y,
                                TREE_VIEW_DRAG_WIDTH, allocation.height);
    }
  column = list->data;
  allocation.x = width;
  allocation.width = MAX (widget->allocation.width, tree_view->priv->width) - width;
  gtk_widget_size_allocate (column->button, &allocation);
  if (column->window)
    gdk_window_move_resize (column->window,
                            allocation.x + allocation.width - TREE_VIEW_DRAG_WIDTH/2,
                            0,
                            TREE_VIEW_DRAG_WIDTH, allocation.height);
}

static void
gtk_tree_view_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GList *tmp_list;
  GtkTreeView *tree_view;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (widget));

  widget->allocation = *allocation;

  tree_view = GTK_TREE_VIEW (widget);
  
  gtk_tree_view_check_dirty (tree_view);
  
  tmp_list = tree_view->priv->children;

  while (tmp_list)
    {
      GtkAllocation allocation;
      GtkRequisition requisition;

      GtkTreeViewChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      allocation.x = child->x;
      allocation.y = child->y;
      gtk_widget_get_child_requisition (child->widget, &requisition);
      allocation.width = requisition.width;
      allocation.height = requisition.height;

      gtk_widget_size_allocate (child->widget, &allocation);
    }

  gtk_tree_view_size_allocate_buttons (widget);
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gdk_window_move_resize (tree_view->priv->header_window,
			      0, 0,
			      MAX (tree_view->priv->width, allocation->width),
			      tree_view->priv->header_height);
    }
  
  tree_view->priv->hadjustment->page_size = allocation->width;
  tree_view->priv->hadjustment->page_increment = allocation->width / 2;
  tree_view->priv->hadjustment->lower = 0;
  tree_view->priv->hadjustment->upper = tree_view->priv->width;

  if (tree_view->priv->hadjustment->value + allocation->width > tree_view->priv->width)
    tree_view->priv->hadjustment->value = MAX (tree_view->priv->width - allocation->width, 0);
  gtk_signal_emit_by_name (GTK_OBJECT (tree_view->priv->hadjustment), "changed");

  tree_view->priv->vadjustment->page_size = allocation->height - TREE_VIEW_HEADER_HEIGHT (tree_view);
  tree_view->priv->vadjustment->page_increment = (allocation->height - TREE_VIEW_HEADER_HEIGHT (tree_view)) / 2;
  tree_view->priv->vadjustment->lower = 0;
  tree_view->priv->vadjustment->upper = tree_view->priv->height;

  if (tree_view->priv->vadjustment->value + allocation->height > tree_view->priv->height)
    gtk_adjustment_set_value (tree_view->priv->vadjustment,
			      (gfloat) MAX (tree_view->priv->height - allocation->height, 0));

  gtk_signal_emit_by_name (GTK_OBJECT (tree_view->priv->vadjustment), "changed");

}

static void
gtk_tree_view_draw_node_focus_rect (GtkWidget   *widget,
                                    GtkTreePath *path)
{
  GtkTreeView *tree_view;
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;
  gint bin_window_width = 0;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (widget));

  tree_view = GTK_TREE_VIEW (widget);

  _gtk_tree_view_find_node (tree_view, path, &tree, &node);

  if (tree == NULL)
    return;

  gdk_drawable_get_size (tree_view->priv->bin_window,
                         &bin_window_width, NULL);
  
  /* FIXME need a style function appropriate for this */
  gdk_draw_rectangle (tree_view->priv->bin_window,
		      widget->style->fg_gc[GTK_STATE_NORMAL],
		      FALSE,
		      0,
                      BACKGROUND_FIRST_PIXEL (tree_view, tree, node),
                      bin_window_width - 2,
		      BACKGROUND_HEIGHT (node) - 1);
}

GdkPixmap*
gtk_tree_view_create_row_drag_icon (GtkTreeView  *tree_view,
                                    GtkTreePath  *path)
{
  GtkTreeIter   iter;
  GtkRBTree    *tree;
  GtkRBNode    *node;
  GtkCellRenderer *cell;
  gint i;
  gint cell_offset;
  GList *list;
  GdkRectangle background_area;
  GtkWidget *widget;
  gint depth;
  /* start drawing inside the black outline */
  gint x = 1, y = 1;
  GdkDrawable *drawable;
  gint bin_window_width;
  
  widget = GTK_WIDGET (tree_view);

  depth = gtk_tree_path_get_depth (path);

  _gtk_tree_view_find_node (tree_view,
                            path,
                            &tree,
                            &node);

  if (tree == NULL)
    return NULL;
  
  if (!gtk_tree_model_get_iter (tree_view->priv->model,
                                &iter,
                                path))
    return NULL;
    
  cell_offset = x;

  background_area.y = y;
  background_area.height = BACKGROUND_HEIGHT (node);

  gdk_drawable_get_size (tree_view->priv->bin_window,
                         &bin_window_width, NULL);
  
  drawable = gdk_pixmap_new (tree_view->priv->bin_window,
                             bin_window_width + 2,
                             background_area.height + 2,
                             -1);

  gdk_draw_rectangle (drawable,
                      widget->style->base_gc[GTK_WIDGET_STATE (widget)],
                      TRUE,
                      0, 0,
                      bin_window_width + 2,
                      background_area.height + 2);

  gdk_draw_rectangle (drawable,
                      widget->style->black_gc,
                      FALSE,
                      0, 0,
                      bin_window_width + 1,
                      background_area.height + 1);
  
  for (i = 0, list = tree_view->priv->columns; i < tree_view->priv->n_columns; i++, list = list->next)
    {
      GtkTreeViewColumn *column = list->data;
      GdkRectangle cell_area;
      
      if (!column->visible)
        continue;

      cell = column->cell;
      gtk_tree_view_column_set_cell_data (column,
                                          tree_view->priv->model,
                                          &iter);

      background_area.x = cell_offset;
      background_area.width = TREE_VIEW_COLUMN_WIDTH (column);

      cell_area = background_area;

      cell_area.y += TREE_VIEW_VERTICAL_SEPARATOR / 2;
      cell_area.height -= TREE_VIEW_VERTICAL_SEPARATOR;
      
      if (i == tree_view->priv->expander_column &&
          TREE_VIEW_DRAW_EXPANDERS(tree_view))
        {
          cell_area.x += depth * tree_view->priv->tab_offset;
          cell_area.width -= depth * tree_view->priv->tab_offset;
        }
      
      gtk_cell_renderer_render (cell,
                                drawable,
                                widget,
                                &background_area,
                                &cell_area,
                                NULL,
                                0);
      
      cell_offset += TREE_VIEW_COLUMN_WIDTH (column);
    }

  return drawable;
}

/* Warning: Very scary function.
 * Modify at your own risk
 */
static gboolean
gtk_tree_view_bin_expose (GtkWidget      *widget,
			  GdkEventExpose *event)
{
  GtkTreeView *tree_view;
  GtkTreePath *path;
  GtkRBTree *tree;
  GList *list;
  GtkRBNode *node;
  GtkRBNode *cursor = NULL;
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *drag_highlight = NULL;
  GtkRBTree *drag_highlight_tree = NULL;
  GtkTreeIter iter;
  GtkCellRenderer *cell;
  gint new_y;
  gint y_offset, x_offset, cell_offset;
  gint i, max_height;
  gint depth;
  GdkRectangle background_area;
  GdkRectangle cell_area;
  guint flags;
  gint highlight_x;
  gint bin_window_width;
  GtkTreePath *cursor_path;
  GtkTreePath *drag_dest_path;
  GList *last_column;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);

  tree_view = GTK_TREE_VIEW (widget);

  if (tree_view->priv->tree == NULL)
    return TRUE;
  
  gtk_tree_view_check_dirty (GTK_TREE_VIEW (widget));
  /* we want to account for a potential HEADER offset.
   * That is, if the header exists, we want to offset our event by its
   * height to find the right node.
   */
  new_y = (event->area.y<TREE_VIEW_HEADER_HEIGHT (tree_view))?TREE_VIEW_HEADER_HEIGHT (tree_view):event->area.y;

  /* y_offset is the */
  
  y_offset = -_gtk_rbtree_find_offset (tree_view->priv->tree,
                                       TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, new_y),
				       &tree,
				       &node) + new_y - event->area.y;
  if (node == NULL)
    return TRUE;

  /* find the path for the node */
  path = _gtk_tree_view_find_path ((GtkTreeView *)widget,
				   tree,
				   node);
  gtk_tree_model_get_iter (tree_view->priv->model,
			   &iter,
			   path);
  depth = gtk_tree_path_get_depth (path);
  gtk_tree_path_free (path);

  cursor_path = NULL;
  drag_dest_path = NULL;
  
  if (tree_view->priv->cursor)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

  if (cursor_path)
    _gtk_tree_view_find_node (tree_view, cursor_path,
                              &cursor_tree, &cursor);

  if (tree_view->priv->drag_dest_row)
    drag_dest_path = gtk_tree_row_reference_get_path (tree_view->priv->drag_dest_row);

  if (drag_dest_path)
    _gtk_tree_view_find_node (tree_view, drag_dest_path,
                              &drag_highlight_tree, &drag_highlight);

  gdk_drawable_get_size (tree_view->priv->bin_window,
                         &bin_window_width, NULL);

  for (last_column = g_list_last (tree_view->priv->columns);
       last_column &&
	 !(GTK_TREE_VIEW_COLUMN (last_column->data)->visible) &&
	 GTK_WIDGET_CAN_FOCUS (GTK_TREE_VIEW_COLUMN (last_column->data)->button);
       last_column = last_column->prev)
    ;

  /* Actually process the expose event.  To do this, we want to
   * start at the first node of the event, and walk the tree in
   * order, drawing each successive node.
   */

  do
    {
      /* Need to think about this more.
	 if (tree_view->priv->show_expanders)
	 max_height = MAX (TREE_VIEW_EXPANDER_MIN_HEIGHT, GTK_RBNODE_GET_HEIGHT (node));
	 else
      */
      gboolean parity;
      
      max_height = BACKGROUND_HEIGHT (node);

      x_offset = -event->area.x;
      cell_offset = 0;
      highlight_x = 0; /* should match x coord of first cell */
      
      background_area.y = y_offset + event->area.y;
      background_area.height = max_height;
      flags = 0;

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PRELIT))
	flags |= GTK_CELL_RENDERER_PRELIT;

      parity = _gtk_rbtree_node_find_parity (tree, node);
      
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
        flags |= GTK_CELL_RENDERER_SELECTED;
      
      for (i = 0, list = tree_view->priv->columns; i < tree_view->priv->n_columns; i++, list = list->next)
	{
	  GtkTreeViewColumn *column = list->data;
          const gchar *detail = NULL;
          
	  if (!column->visible)
            continue;

          if (column->show_sort_indicator)
            flags |= GTK_CELL_RENDERER_SORTED;
          else
            flags &= ~GTK_CELL_RENDERER_SORTED;
          
	  cell = column->cell;
	  gtk_tree_view_column_set_cell_data (column,
					      tree_view->priv->model,
					      &iter);

	  background_area.x = cell_offset;
	  background_area.width = TREE_VIEW_COLUMN_WIDTH (column);

          cell_area = background_area;
          cell_area.y += TREE_VIEW_VERTICAL_SEPARATOR / 2;
          cell_area.height -= TREE_VIEW_VERTICAL_SEPARATOR;

          /* Select the detail for drawing the cell.  relevant
           * factors are parity, sortedness, and whether to
           * display rules.
           */

          /* FIXME when we have style properties, clean this up.
           */

          if (tree_view->priv->has_rules)
            {
              if (flags & GTK_CELL_RENDERER_SORTED)
                {
                  if (parity)
                    detail = "cell_odd_ruled_sorted";
                  else
                    detail = "cell_even_ruled_sorted";
                }
              else
                {
                  if (parity)
                    detail = "cell_odd_ruled";
                  else
                    detail = "cell_even_ruled";
                }
            }
          else
            {
              if (flags & GTK_CELL_RENDERER_SORTED)
                {
                  if (parity)
                    detail = "cell_odd_sorted";
                  else
                    detail = "cell_even_sorted";
                }
              else
                {
                  if (parity)
                    detail = "cell_odd";
                  else
                    detail = "cell_even";
                }
            }

          g_assert (detail);
          
          /* Draw background */
          gtk_paint_flat_box (widget->style,
                              event->window,
                              (flags & GTK_CELL_RENDERER_SELECTED) ?
                              GTK_STATE_SELECTED : GTK_STATE_NORMAL,
                              GTK_SHADOW_NONE,
                              &event->area,
                              widget,
                              detail,
                              background_area.x,
                              background_area.y,
                              background_area.width,
                              background_area.height);
          
	  if (i == tree_view->priv->expander_column &&
              TREE_VIEW_DRAW_EXPANDERS(tree_view))
	    {
	      cell_area.x += depth*tree_view->priv->tab_offset;
	      cell_area.width -= depth*tree_view->priv->tab_offset;

              /* If we have an expander column, the highlight underline
               * starts with that column, so that it indicates which
               * level of the tree we're dropping at.
               */
              highlight_x = cell_area.x;
                
	      gtk_cell_renderer_render (cell,
					event->window,
					widget,
					&background_area,
					&cell_area,
					&event->area,
					flags);

	      if ((node->flags & GTK_RBNODE_IS_PARENT) == GTK_RBNODE_IS_PARENT)
		{
		  gint x, y;
		  gdk_window_get_pointer (tree_view->priv->bin_window, &x, &y, 0);
		  gtk_tree_view_draw_arrow (GTK_TREE_VIEW (widget),
                                            tree,
					    node,
					    x, y);
		}
	    }
	  else
	    {              
	      gtk_cell_renderer_render (cell,
					event->window,
					widget,
					&background_area,
					&cell_area,
					&event->area,
					flags);
	    }
	  cell_offset += TREE_VIEW_COLUMN_WIDTH (column);
	}

      if (node == cursor &&
	  GTK_WIDGET_HAS_FOCUS (widget))
	gtk_tree_view_draw_focus (widget);

      if (node == drag_highlight)
        {
          /* Draw indicator for the drop
           */
          gint highlight_y = -1;

          switch (tree_view->priv->drag_dest_pos)
            {
            case GTK_TREE_VIEW_DROP_BEFORE:
              highlight_y = background_area.y - TREE_VIEW_VERTICAL_SEPARATOR/2;
              break;
              
            case GTK_TREE_VIEW_DROP_AFTER:
              highlight_y = background_area.y + background_area.height + TREE_VIEW_VERTICAL_SEPARATOR/2;
              break;
              
            case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
            case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
              gtk_tree_view_draw_node_focus_rect (widget,
                                                  drag_dest_path);
              break;
            }

          if (highlight_y >= 0)
            {
              gdk_draw_line (event->window,
                             widget->style->black_gc,
                             highlight_x,
                             highlight_y,
                             bin_window_width - highlight_x,
                             highlight_y);
            }
        }
      
      y_offset += max_height;
      if (node->children)
	{
	  GtkTreeIter parent = iter;
	  gboolean has_child;

	  tree = node->children;
	  node = tree->root;

          g_assert (node != tree->nil);
          
	  while (node->left != tree->nil)
	    node = node->left;
	  has_child = gtk_tree_model_iter_children (tree_view->priv->model,
						    &iter,
						    &parent);
	  cell = gtk_tree_view_get_column (tree_view, 0)->cell;
	  depth++;

	  /* Sanity Check! */
	  TREE_VIEW_INTERNAL_ASSERT (has_child, FALSE);
	}
      else
	{
	  gboolean done = FALSE;
	  do
	    {
	      node = _gtk_rbtree_next (tree, node);
	      if (node != NULL)
		{
		  gboolean has_next = gtk_tree_model_iter_next (tree_view->priv->model, &iter);
		  cell = gtk_tree_view_get_column (tree_view, 0)->cell;
		  done = TRUE;

		  /* Sanity Check! */
		  TREE_VIEW_INTERNAL_ASSERT (has_next, FALSE);
		}
	      else
		{
		  GtkTreeIter parent_iter = iter;
		  gboolean has_parent;

		  node = tree->parent_node;
		  tree = tree->parent_tree;
		  if (tree == NULL)
		    /* we've run out of tree.  It's okay to return though, as
		     * we'd only break out of the while loop below. */
		    return TRUE;
		  has_parent = gtk_tree_model_iter_parent (tree_view->priv->model,
							   &iter,
							   &parent_iter);
		  depth--;

		  /* Sanity check */
		  TREE_VIEW_INTERNAL_ASSERT (has_parent, FALSE);
		}
	    }
	  while (!done);
	}
    }
  while (y_offset < event->area.height);
  
  if (cursor_path)
    gtk_tree_path_free (cursor_path);

  if (drag_dest_path)
    gtk_tree_path_free (drag_dest_path);
  
  return TRUE;
}

static gboolean
gtk_tree_view_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  GtkTreeView *tree_view;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);

  tree_view = GTK_TREE_VIEW (widget);

  if (event->window == tree_view->priv->bin_window)
    return gtk_tree_view_bin_expose (widget, event);

  return TRUE;
}

static gboolean
coords_are_over_arrow (GtkTreeView *tree_view,
                       GtkRBTree   *tree,
                       GtkRBNode   *node,
                       /* these are in tree window coords */
                       gint         x,
                       gint         y)
{
  GdkRectangle arrow;
  gint x2;

  if (!GTK_WIDGET_REALIZED (tree_view))
    return FALSE;
  
  if ((node->flags & GTK_RBNODE_IS_PARENT) == 0)
    return FALSE;
  
  arrow.y = BACKGROUND_FIRST_PIXEL (tree_view, tree, node);
      
  arrow.height = BACKGROUND_HEIGHT (node);

  gtk_tree_view_get_arrow_xrange (tree_view, &arrow.x, &x2);

  arrow.width = x2 - arrow.x;

  return (x >= arrow.x &&
          x < (arrow.x + arrow.height) &&
          y >= arrow.y &&
          y < (arrow.y + arrow.height));  
}

static void
do_unprelight (GtkTreeView *tree_view,
               /* these are in tree window coords */
               gint x,
               gint y)
{
  if (tree_view->priv->prelight_node == NULL)
    return;
  
  GTK_RBNODE_UNSET_FLAG (tree_view->priv->prelight_node, GTK_RBNODE_IS_PRELIT);
  
  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT) &&
      !coords_are_over_arrow (tree_view,
                              tree_view->priv->prelight_tree,
                              tree_view->priv->prelight_node,
                              x,
                              y))
    /* We need to unprelight the old arrow. */
    {
      GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_ARROW_PRELIT);
      
      gtk_tree_view_draw_arrow (tree_view,
                                tree_view->priv->prelight_tree,
                                tree_view->priv->prelight_node,
                                x,
                                y);  

    }
  
  tree_view->priv->prelight_node = NULL;
  tree_view->priv->prelight_tree = NULL;
}

static void
do_prelight (GtkTreeView *tree_view,
             GtkRBTree   *tree,
             GtkRBNode   *node,
             /* these are in tree window coords */
             gint         x,
             gint         y)
{
  if (coords_are_over_arrow (tree_view, tree, node, x, y))
    GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_ARROW_PRELIT);

  tree_view->priv->prelight_node = node;
  tree_view->priv->prelight_tree = tree;

  GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_PRELIT);
}

static void
ensure_unprelighted (GtkTreeView *tree_view)
{
  do_unprelight (tree_view, -1000, -1000); /* coords not possibly over an arrow */
}

static gboolean
gtk_tree_view_motion (GtkWidget      *widget,
		      GdkEventMotion *event)
{
  GtkTreeView *tree_view;
  GtkRBTree *tree;
  GtkRBNode *node;
  gint new_y;
  GtkRBTree *old_prelight_tree;
  GtkRBNode *old_prelight_node;
  
  tree_view = (GtkTreeView *) widget;
  
  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_RESIZE))
    {
      gint x;
      gint new_width;

      if (event->is_hint || event->window != widget->window)
	gtk_widget_get_pointer (widget, &x, NULL);
      else
	x = event->x;

      new_width = gtk_tree_view_new_column_width (GTK_TREE_VIEW (widget), tree_view->priv->drag_pos, &x);
      if (x != tree_view->priv->x_drag)
	{
	  gtk_tree_view_column_set_width (gtk_tree_view_get_column (GTK_TREE_VIEW (widget), tree_view->priv->drag_pos), new_width);
	}

      /* FIXME: Do we need to scroll */
      _gtk_tree_view_set_size (GTK_TREE_VIEW (widget), -1, tree_view->priv->height);
      return FALSE;
    }

  /* Sanity check it */
  if (event->window != tree_view->priv->bin_window)
    return FALSE;

  if (tree_view->priv->tree == NULL)
    return FALSE;

  gtk_tree_view_maybe_begin_dragging_row (tree_view, event);

  old_prelight_tree = tree_view->priv->prelight_tree;
  old_prelight_node = tree_view->priv->prelight_node;
  
  do_unprelight (tree_view, event->x, event->y);  
  
  new_y = ((gint)event->y<TREE_VIEW_HEADER_HEIGHT (tree_view))?TREE_VIEW_HEADER_HEIGHT (tree_view):(gint)event->y;

  _gtk_rbtree_find_offset (tree_view->priv->tree,
                           TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, new_y),
                           &tree,
                           &node);

  if (tree == NULL)
    return TRUE;

  /* If we are currently pressing down a button, we don't want to prelight anything else. */
  if ((tree_view->priv->button_pressed_node != NULL) &&
      (tree_view->priv->button_pressed_node != node))
    return TRUE;


  do_prelight (tree_view, tree, node, event->x, new_y);

  if (old_prelight_node != tree_view->priv->prelight_node)
    {
      if (old_prelight_node)
        gtk_tree_view_queue_draw_node (tree_view,
                                       old_prelight_tree,
                                       old_prelight_node,
                                       NULL);

      if (tree_view->priv->prelight_node)
        gtk_tree_view_queue_draw_node (tree_view,
                                       tree_view->priv->prelight_tree,
                                       tree_view->priv->prelight_node,
                                       NULL);
    }
  
  return TRUE;
}

/* FIXME Is this function necessary? Can I get an enter_notify event
 * w/o either an expose event or a mouse motion event?
 */
static gboolean
gtk_tree_view_enter_notify (GtkWidget        *widget,
			    GdkEventCrossing *event)
{
  GtkTreeView *tree_view;
  GtkRBTree *tree;
  GtkRBNode *node;
  gint new_y;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);

  tree_view = GTK_TREE_VIEW (widget);

  /* Sanity check it */
  if (event->window != tree_view->priv->bin_window)
    return FALSE;

  if (tree_view->priv->tree == NULL)
    return FALSE;

  if ((tree_view->priv->button_pressed_node != NULL) &&
      (tree_view->priv->button_pressed_node != node))
    return TRUE;

  /* find the node internally */
  new_y = ((gint)event->y<TREE_VIEW_HEADER_HEIGHT (tree_view))?TREE_VIEW_HEADER_HEIGHT (tree_view):(gint)event->y;

  _gtk_rbtree_find_offset (tree_view->priv->tree,
                           TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, new_y),
                           &tree,
                           &node);
  
  if (node == NULL)
    return FALSE;

  do_prelight (tree_view, tree, node, event->x, new_y);

  if (tree_view->priv->prelight_node)
    gtk_tree_view_queue_draw_node (tree_view,
                                   tree_view->priv->prelight_tree,
                                   tree_view->priv->prelight_node,
                                   NULL);
  
  return TRUE;
}

static gboolean
gtk_tree_view_leave_notify (GtkWidget        *widget,
			    GdkEventCrossing *event)
{
  GtkTreeView *tree_view;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);

  tree_view = GTK_TREE_VIEW (widget);

  if (tree_view->priv->prelight_node)
    gtk_tree_view_queue_draw_node (tree_view,
                                   tree_view->priv->prelight_tree,
                                   tree_view->priv->prelight_node,
                                   NULL);
  
  ensure_unprelighted (tree_view);

  return TRUE;
}

static gboolean
gtk_tree_view_button_press (GtkWidget      *widget,
			    GdkEventButton *event)
{
  GtkTreeView *tree_view;
  GList *list;
  GtkTreeViewColumn *column;
  gint i;
  GdkRectangle background_area;
  GdkRectangle cell_area;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  tree_view = GTK_TREE_VIEW (widget);

  if (event->window == tree_view->priv->bin_window)
    {
      GtkRBNode *node;
      GtkRBTree *tree;
      GtkTreePath *path;
      gchar *path_string;
      gint depth;
      gint new_y;
      gint y_offset;

      if (!GTK_WIDGET_HAS_FOCUS (widget))
	gtk_widget_grab_focus (widget);
      GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS);

      /* are we in an arrow? */
      if (tree_view->priv->prelight_node &&
          GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT))
	{
	  if (event->button == 1)
	    {
	      gtk_grab_add (widget);
	      tree_view->priv->button_pressed_node = tree_view->priv->prelight_node;
	      tree_view->priv->button_pressed_tree = tree_view->priv->prelight_tree;
	      gtk_tree_view_draw_arrow (GTK_TREE_VIEW (widget),
                                        tree_view->priv->prelight_tree,
					tree_view->priv->prelight_node,
					event->x,
					event->y);
	    }
	  return TRUE;
	}

      /* find the node that was clicked */
      new_y = ((gint)event->y<TREE_VIEW_HEADER_HEIGHT (tree_view))?TREE_VIEW_HEADER_HEIGHT (tree_view):(gint)event->y;
      y_offset = -_gtk_rbtree_find_offset (tree_view->priv->tree,
                                           TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, new_y),
					   &tree,
					   &node) + new_y - (gint)event->y;

      if (node == NULL)
	/* We clicked in dead space */
	return TRUE;

      /* Get the path and the node */
      path = _gtk_tree_view_find_path (tree_view, tree, node);
      depth = gtk_tree_path_get_depth (path);
      background_area.y = y_offset + event->y + TREE_VIEW_VERTICAL_SEPARATOR;
      background_area.height = GTK_RBNODE_GET_HEIGHT (node) - TREE_VIEW_VERTICAL_SEPARATOR;
      background_area.x = 0;
      /* Let the cell have a chance at selecting it. */

      for (i = 0, list = tree_view->priv->columns; i < tree_view->priv->n_columns; i++, list = list->next)
	{
	  GtkTreeViewColumn *column = list->data;
	  GtkCellRenderer *cell;
	  GtkTreeIter iter;

	  if (!column->visible)
	    continue;

	  background_area.width = TREE_VIEW_COLUMN_WIDTH (column);
	  if (i == tree_view->priv->expander_column &&
              TREE_VIEW_DRAW_EXPANDERS(tree_view))
	    {
	      cell_area = background_area;
	      cell_area.x += depth*tree_view->priv->tab_offset;
	      cell_area.width -= depth*tree_view->priv->tab_offset;
	    }
	  else
	    {
	      cell_area = background_area;
	    }

	  cell = column->cell;

	  if ((background_area.x > (gint) event->x) ||
	      (background_area.y > (gint) event->y) ||
	      (background_area.x + background_area.width <= (gint) event->x) ||
	      (background_area.y + background_area.height <= (gint) event->y))
	    {
	      background_area.x += background_area.width;
	      continue;
	    }

	  gtk_tree_model_get_iter (tree_view->priv->model,
				   &iter,
				   path);
	  gtk_tree_view_column_set_cell_data (column,
					      tree_view->priv->model,
					      &iter);

	  path_string = gtk_tree_path_to_string (path);
	  if (gtk_cell_renderer_event (cell,
				       (GdkEvent *)event,
				       widget,
				       path_string,
				       &background_area,
				       &cell_area,
				       0))

	    {
	      g_free (path_string);
	      gtk_tree_path_free (path);
	      return TRUE;
	    }
	  else
	    {
	      g_free (path_string);
	      break;
	    }
	}

      /* Save press to possibly begin a drag
       */
      if (tree_view->priv->pressed_button < 0)
        {
          tree_view->priv->pressed_button = event->button;
          tree_view->priv->press_start_x = event->x;
          tree_view->priv->press_start_y = event->y;
        }      
      
      /* Handle the selection */
      if (tree_view->priv->selection == NULL)
	tree_view->priv->selection =
          _gtk_tree_selection_new_with_tree_view (tree_view);

      _gtk_tree_selection_internal_select_node (tree_view->priv->selection,
						node,
						tree,
						path,
						event->state);
      gtk_tree_path_free (path);
      return TRUE;
    }

  for (i = 0, list = tree_view->priv->columns; list; list = list->next, i++)
    {
      column = list->data;
      if (event->window == column->window &&
	  column->column_type == GTK_TREE_VIEW_COLUMN_RESIZEABLE &&
	  column->window)
	{
	  gpointer drag_data;

	  if (gdk_pointer_grab (column->window, FALSE,
				GDK_POINTER_MOTION_HINT_MASK |
				GDK_BUTTON1_MOTION_MASK |
				GDK_BUTTON_RELEASE_MASK,
				NULL, NULL, event->time))
	    return FALSE;

	  gtk_grab_add (widget);
	  GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_IN_COLUMN_RESIZE);

	  /* block attached dnd signal handler */
	  drag_data = gtk_object_get_data (GTK_OBJECT (widget), "gtk-site-data");
	  if (drag_data)
	    gtk_signal_handler_block_by_data (GTK_OBJECT (widget), drag_data);

	  if (!GTK_WIDGET_HAS_FOCUS (widget))
	    gtk_widget_grab_focus (widget);

	  tree_view->priv->drag_pos = i;
	  tree_view->priv->x_drag = (column->button->allocation.x + column->button->allocation.width);
	}
    }
  return TRUE;
}

static gboolean
gtk_tree_view_button_release (GtkWidget      *widget,
			      GdkEventButton *event)
{
  GtkTreeView *tree_view;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  tree_view = GTK_TREE_VIEW (widget);

  if (tree_view->priv->pressed_button == event->button)
    tree_view->priv->pressed_button = -1;
  
  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_RESIZE))
    {
      gpointer drag_data;
      gint width;
      gint x;
      gint i;

      i = tree_view->priv->drag_pos;
      tree_view->priv->drag_pos = -1;

      /* unblock attached dnd signal handler */
      drag_data = gtk_object_get_data (GTK_OBJECT (widget), "gtk-site-data");
      if (drag_data)
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (widget), drag_data);

      GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_IN_COLUMN_RESIZE);
      gtk_widget_get_pointer (widget, &x, NULL);
      gtk_grab_remove (widget);
      gdk_pointer_ungrab (event->time);

      width = gtk_tree_view_new_column_width (GTK_TREE_VIEW (widget), i, &x);
      gtk_tree_view_column_set_width (gtk_tree_view_get_column (GTK_TREE_VIEW (widget), i), width);
      return FALSE;
    }

  if (tree_view->priv->button_pressed_node == NULL)
    return FALSE;

  if (event->button == 1)
    {
      gtk_grab_remove (widget);
      if (tree_view->priv->button_pressed_node == tree_view->priv->prelight_node &&
          GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT))
	{
	  GtkTreePath *path;
	  GtkTreeIter iter;

	  /* Actually activate the node */
	  if (tree_view->priv->button_pressed_node->children == NULL)
	    {
	      GtkTreeIter child;
	      path = _gtk_tree_view_find_path (GTK_TREE_VIEW (widget),
					       tree_view->priv->button_pressed_tree,
					       tree_view->priv->button_pressed_node);
	      tree_view->priv->button_pressed_node->children = _gtk_rbtree_new ();
	      tree_view->priv->button_pressed_node->children->parent_tree = tree_view->priv->button_pressed_tree;
	      tree_view->priv->button_pressed_node->children->parent_node = tree_view->priv->button_pressed_node;
	      gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
              
	      if (gtk_tree_model_iter_children (tree_view->priv->model, &child, &iter))
                gtk_tree_view_build_tree (tree_view,
                                          tree_view->priv->button_pressed_node->children,
                                          &child,
                                          gtk_tree_path_get_depth (path) + 1,
                                          FALSE,
                                          GTK_WIDGET_REALIZED (widget));
	    }
	  else
	    {
	      path = _gtk_tree_view_find_path (GTK_TREE_VIEW (widget),
					       tree_view->priv->button_pressed_node->children,
					       tree_view->priv->button_pressed_node->children->root);
	      gtk_tree_model_get_iter (tree_view->priv->model,
				       &iter,
				       path);

	      gtk_tree_view_discover_dirty (GTK_TREE_VIEW (widget),
					    tree_view->priv->button_pressed_node->children,
					    &iter,
					    gtk_tree_path_get_depth (path));
	      _gtk_rbtree_remove (tree_view->priv->button_pressed_node->children);
	    }
	  gtk_tree_path_free (path);

	  _gtk_tree_view_set_size (GTK_TREE_VIEW (widget), -1, -1);
	}

      tree_view->priv->button_pressed_node = NULL;
    }

  return TRUE;
}


static void
gtk_tree_view_draw_focus (GtkWidget *widget)
{
  GtkTreeView *tree_view;
  GtkTreePath *cursor_path;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (widget));

  tree_view = GTK_TREE_VIEW (widget);

  if (! GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS))
    return;

  if (tree_view->priv->cursor == NULL)
    return;

  cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
  if (cursor_path == NULL)
    return;
  
  gtk_tree_view_draw_node_focus_rect (widget, cursor_path);
}


static gint
gtk_tree_view_focus_in (GtkWidget     *widget,
			GdkEventFocus *event)
{
  GtkTreeView *tree_view;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  tree_view = GTK_TREE_VIEW (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);

  /* FIXME don't redraw so much */
  gtk_widget_queue_draw (widget);

  return FALSE;
}


static gint
gtk_tree_view_focus_out (GtkWidget     *widget,
			 GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);

  /* FIXME don't redraw so much */
  gtk_widget_queue_draw (widget);

  return FALSE;
}

/* FIXME: It would be neat to someday make the headers a seperate widget that
 * can be shared between various apps.  Wishful thinking, though...
 */
/* Returns TRUE if the focus is within the headers, after the focus operation is
 * done
 */
static gboolean
gtk_tree_view_header_focus (GtkTreeView        *tree_view,
			    GtkDirectionType  dir)
{
  GtkWidget *focus_child;
  GtkContainer *container;

  GList *last_column, *first_column;
  GList *tmp_list;

  if (! GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE))
    return FALSE;

  focus_child = GTK_CONTAINER (tree_view)->focus_child;
  container = GTK_CONTAINER (tree_view);

  for (last_column = g_list_last (tree_view->priv->columns);
       last_column &&
	 !(GTK_TREE_VIEW_COLUMN (last_column->data)->visible) &&
	 GTK_WIDGET_CAN_FOCUS (GTK_TREE_VIEW_COLUMN (last_column->data)->button);
       last_column = last_column->prev)
    ;

  for (first_column = tree_view->priv->columns;
       first_column &&
	 !(GTK_TREE_VIEW_COLUMN (first_column->data)->visible) &&
	 GTK_WIDGET_CAN_FOCUS (GTK_TREE_VIEW_COLUMN (first_column->data)->button);
       first_column = first_column->next)
    ;

  /* no headers are visible, or are focussable.  We can't focus in or out.
   * I wonder if focussable is a real word...
   */
  if (last_column == NULL)
    return FALSE;

  /* First thing we want to handle is entering and leaving the headers.
   */
  switch (dir)
    {
    case GTK_DIR_TAB_BACKWARD:
      if (!focus_child)
	{
	  focus_child = GTK_TREE_VIEW_COLUMN (last_column->data)->button;
	  gtk_widget_grab_focus (focus_child);
	  goto cleanup;
	}
      if (focus_child == GTK_TREE_VIEW_COLUMN (first_column->data)->button)
	{
	  focus_child = NULL;
	  goto cleanup;
	}
      break;

    case GTK_DIR_TAB_FORWARD:
      if (!focus_child)
	{
	  focus_child = GTK_TREE_VIEW_COLUMN (first_column->data)->button;
	  gtk_widget_grab_focus (focus_child);
	  goto cleanup;
	}
      if (focus_child == GTK_TREE_VIEW_COLUMN (last_column->data)->button)
	{
	  focus_child = NULL;
	  goto cleanup;
	}
      break;

    case GTK_DIR_LEFT:
      if (!focus_child)
	{
	  focus_child = GTK_TREE_VIEW_COLUMN (last_column->data)->button;
	  gtk_widget_grab_focus (focus_child);
	  goto cleanup;
	}
      if (focus_child == GTK_TREE_VIEW_COLUMN (first_column->data)->button)
	{
	  focus_child = NULL;
	  goto cleanup;
	}
      break;

    case GTK_DIR_RIGHT:
      if (!focus_child)
	{
	  focus_child = GTK_TREE_VIEW_COLUMN (first_column->data)->button;
	  gtk_widget_grab_focus (focus_child);
	  goto cleanup;
	}
      if (focus_child == GTK_TREE_VIEW_COLUMN (last_column->data)->button)
	{
	  focus_child = NULL;
	  goto cleanup;
	}
      break;

    case GTK_DIR_UP:
      if (!focus_child)
	{
	  focus_child = GTK_TREE_VIEW_COLUMN (first_column->data)->button;
	  gtk_widget_grab_focus (focus_child);
	}
      else
	{
	  focus_child = NULL;
	}
      goto cleanup;

    case GTK_DIR_DOWN:
      if (!focus_child)
	{
	  focus_child = GTK_TREE_VIEW_COLUMN (first_column->data)->button;
	  gtk_widget_grab_focus (focus_child);
	}
      else
	{
	  focus_child = NULL;
	}
      goto cleanup;
    }

  /* We need to move the focus to the next button. */
  if (focus_child)
    {
      for (tmp_list = tree_view->priv->columns; tmp_list; tmp_list = tmp_list->next)
	if (GTK_TREE_VIEW_COLUMN (tmp_list->data)->button == focus_child)
	  {
	    if (gtk_container_focus (GTK_CONTAINER (GTK_TREE_VIEW_COLUMN (tmp_list->data)->button), dir))
	      {
		/* The focus moves inside the button. */
		/* This is probably a great example of bad UI */
		goto cleanup;
	      }
	    break;
	  }

      /* We need to move the focus among the row of buttons. */
      while (tmp_list)
	{
	  GtkTreeViewColumn *column;

	  if (dir == GTK_DIR_RIGHT || dir == GTK_DIR_TAB_FORWARD)
	    tmp_list = tmp_list->next;
	  else
	    tmp_list = tmp_list->prev;

	  if (tmp_list == NULL)
	    {
	      g_warning ("Internal button not found");
	      goto cleanup;
	    }
	  column = tmp_list->data;
	  if (column->button &&
	      column->visible &&
	      GTK_WIDGET_CAN_FOCUS (column->button))
	    {
	      focus_child = column->button;
	      gtk_widget_grab_focus (column->button);
	      break;
	    }
	}
    }

 cleanup:
  /* if focus child is non-null, we assume it's been set to the current focus child
   */
  if (focus_child)
    {
      /* If the following isn't true, then the view is smaller then the scrollpane.
       */
      if ((focus_child->allocation.x + focus_child->allocation.width) <=
	  (tree_view->priv->hadjustment->upper))
	{
	  /* Scroll to the button, if needed */
	  if ((tree_view->priv->hadjustment->value + tree_view->priv->hadjustment->page_size) <
	      (focus_child->allocation.x + focus_child->allocation.width))
	    gtk_adjustment_set_value (tree_view->priv->hadjustment,
				      focus_child->allocation.x + focus_child->allocation.width -
				      tree_view->priv->hadjustment->page_size);
	  else if (tree_view->priv->hadjustment->value > focus_child->allocation.x)
	    gtk_adjustment_set_value (tree_view->priv->hadjustment,
				      focus_child->allocation.x);
	}
    }

  return (focus_child != NULL);
}

/* WARNING: Scary function */
static gint
gtk_tree_view_focus (GtkContainer     *container,
		     GtkDirectionType  direction)
{
  GtkTreeView *tree_view;
  GtkWidget *focus_child;
  GdkEvent *event;
  GtkRBTree *cursor_tree;
  GtkRBNode *cursor_node;
  GtkTreePath *cursor_path;
  
  g_return_val_if_fail (container != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (container), FALSE);
  g_return_val_if_fail (GTK_WIDGET_VISIBLE (container), FALSE);

  tree_view = GTK_TREE_VIEW (container);

  if (!GTK_WIDGET_IS_SENSITIVE (container))
    return FALSE;
  if (tree_view->priv->tree == NULL)
    return FALSE;

  focus_child = container->focus_child;

  /* Case 1.  Headers have focus. */
  if (focus_child)
    {
      switch (direction)
	{
	case GTK_DIR_LEFT:
	case GTK_DIR_TAB_BACKWARD:
	  return (gtk_tree_view_header_focus (tree_view, direction));
	case GTK_DIR_UP:
	  return FALSE;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_RIGHT:
	case GTK_DIR_DOWN:
	  if (direction != GTK_DIR_DOWN)
	    {
	      if (gtk_tree_view_header_focus (tree_view, direction))
		return TRUE;
	    }
	  GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS);
	  gtk_widget_grab_focus (GTK_WIDGET (container));

	  if (tree_view->priv->selection == NULL)
	    tree_view->priv->selection =
              _gtk_tree_selection_new_with_tree_view (tree_view);

	  /* if there is no keyboard focus yet, we select the first node
	   */

          cursor_path = NULL;

          if (tree_view->priv->cursor)
            cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
          
	  if (cursor_path == NULL)
            {
              GtkTreePath *tmp_path = gtk_tree_path_new_root ();

              if (tree_view->priv->cursor)
                gtk_tree_row_reference_free (tree_view->priv->cursor);
              
              tree_view->priv->cursor =
                gtk_tree_row_reference_new (tree_view->priv->model,
                                            tmp_path);
              cursor_path = tmp_path;
            }
          
	  gtk_tree_selection_select_path (tree_view->priv->selection,
                                          cursor_path);

          gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);          

          gtk_tree_path_free (cursor_path);
          
	  return TRUE;
	}
    }

  /* Case 2. We don't have focus at all. */
  if (!GTK_WIDGET_HAS_FOCUS (container))
    {
      if ((direction == GTK_DIR_TAB_FORWARD) ||
	  (direction == GTK_DIR_RIGHT) ||
	  (direction == GTK_DIR_DOWN))
	{
	  if (gtk_tree_view_header_focus (tree_view, direction))
	    return TRUE;
	}

      /* The headers didn't want the focus, so we take it. */
      GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS);
      gtk_widget_grab_focus (GTK_WIDGET (container));

      if (tree_view->priv->selection == NULL)
	tree_view->priv->selection =
          _gtk_tree_selection_new_with_tree_view (tree_view);

      cursor_path = NULL;
      if (tree_view->priv->cursor)
        cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
      
      if (cursor_path == NULL)
        {
          GtkTreePath *tmp_path = gtk_tree_path_new_root ();
          
          if (tree_view->priv->cursor)
            gtk_tree_row_reference_free (tree_view->priv->cursor);
          
          tree_view->priv->cursor =
            gtk_tree_row_reference_new (tree_view->priv->model,
                                        tmp_path);
          cursor_path = tmp_path;
        }        

      gtk_tree_selection_select_path (tree_view->priv->selection,
                                      cursor_path);

      gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);

      gtk_tree_path_free (cursor_path);      

      return TRUE;
    }

  cursor_path = NULL;
  if (tree_view->priv->cursor)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

  /* Case 3. We have focus already, but no cursor.  We pick the first one
   * and run with it.
   */
  
  if (cursor_path == NULL)
    {
      GtkTreePath *tmp_path = gtk_tree_path_new_root ();
      
      if (tree_view->priv->cursor)
        gtk_tree_row_reference_free (tree_view->priv->cursor);
      
      tree_view->priv->cursor =
        gtk_tree_row_reference_new (tree_view->priv->model,
                                    tmp_path);
      cursor_path = tmp_path;
      
      gtk_tree_selection_select_path (tree_view->priv->selection,
                                      cursor_path);
      
      gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);
      
      gtk_tree_path_free (cursor_path);      
      
      return TRUE;
    }

  /* Case 4. We have focus already.  Move the cursor. */
  if (direction == GTK_DIR_LEFT)
    {
      gfloat val;
      val = tree_view->priv->hadjustment->value - tree_view->priv->hadjustment->page_size/2;
      val = MAX (val, 0.0);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->hadjustment), val);
      gtk_widget_grab_focus (GTK_WIDGET (tree_view));

      gtk_tree_path_free (cursor_path);
      
      return TRUE;
    }
  if (direction == GTK_DIR_RIGHT)
    {
      gfloat val;
      val = tree_view->priv->hadjustment->value + tree_view->priv->hadjustment->page_size/2;
      val = MIN (tree_view->priv->hadjustment->upper - tree_view->priv->hadjustment->page_size, val);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->hadjustment), val);
      gtk_widget_grab_focus (GTK_WIDGET (tree_view));

      gtk_tree_path_free (cursor_path);
      
      return TRUE;
    }

  cursor_tree = NULL;
  cursor_node = NULL;

  _gtk_tree_view_find_node (tree_view, cursor_path,
			    &cursor_tree,
			    &cursor_node);

  /* undraw the old row */
  gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);
  
  gtk_tree_path_free (cursor_path);
  cursor_path = NULL;

  if (tree_view->priv->cursor)
    {
      gtk_tree_row_reference_free (tree_view->priv->cursor);
      tree_view->priv->cursor = NULL;
    }
  
  switch (direction)
    {
    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_UP:
      _gtk_rbtree_prev_full (cursor_tree,
			     cursor_node,
			     &cursor_tree,
			     &cursor_node);
      break;
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_DOWN:
      _gtk_rbtree_next_full (cursor_tree,
			     cursor_node,
			     &cursor_tree,
			     &cursor_node);
      break;
    default:
      break;
    }

  if (cursor_node)
    {
      GdkModifierType state = 0;

      event = gtk_get_current_event ();
      if (event)
        gdk_event_get_state (event, &state);

      if (event)
	gdk_event_free (event);
      
      cursor_path = _gtk_tree_view_find_path (tree_view,
                                              cursor_tree,
                                              cursor_node);

      if (cursor_path)
        {
          _gtk_tree_selection_internal_select_node (tree_view->priv->selection,
                                                    cursor_node,
                                                    cursor_tree,
                                                    cursor_path,
                                                    state);

          tree_view->priv->cursor = gtk_tree_row_reference_new (tree_view->priv->model,
                                                                cursor_path);


          /* draw the newly-selected row */
          gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);
          
          gtk_tree_path_free (cursor_path);
        }
          
      gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);
      gtk_widget_grab_focus (GTK_WIDGET (tree_view));

      return TRUE;
    }

  /* At this point, we've progressed beyond the edge of the rows. */

  if ((direction == GTK_DIR_LEFT) ||
      (direction == GTK_DIR_TAB_BACKWARD) ||
      (direction == GTK_DIR_UP))
    /* We can't go back anymore.  Try the headers */
    return (gtk_tree_view_header_focus (tree_view, direction));

  /* we've reached the end of the tree.  Go on. */
  return FALSE;
}

/* Container method
 */
static void
gtk_tree_view_remove (GtkContainer *container,
		      GtkWidget    *widget)
{
  GtkTreeView *tree_view;
  GtkTreeViewChild *child = NULL;
  GList *tmp_list;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (container));

  tree_view = GTK_TREE_VIEW (container);

  tmp_list = tree_view->priv->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      if (child->widget == widget)
	break;
      tmp_list = tmp_list->next;
    }

  if (tmp_list)
    {
      gtk_widget_unparent (widget);

      tree_view->priv->children = g_list_remove_link (tree_view->priv->children, tmp_list);
      g_list_free_1 (tmp_list);
      g_free (child);
    }
}

static void
gtk_tree_view_forall (GtkContainer *container,
		      gboolean      include_internals,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkTreeView *tree_view;
  GtkTreeViewChild *child = NULL;
  GtkTreeViewColumn *column;
  GList *tmp_list;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (container));
  g_return_if_fail (callback != NULL);

  tree_view = GTK_TREE_VIEW (container);

  tmp_list = tree_view->priv->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      tmp_list = tmp_list->next;

      (* callback) (child->widget, callback_data);
    }
  if (include_internals == FALSE)
    return;

  for (tmp_list = tree_view->priv->columns; tmp_list; tmp_list = tmp_list->next)
    {
      column = tmp_list->data;      
      
      if (column->button)
	(* callback) (column->button, callback_data);
    }
}

/* TreeModel Callbacks
 */

static void
gtk_tree_view_changed (GtkTreeModel *model,
		       GtkTreePath  *path,
		       GtkTreeIter  *iter,
		       gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *)data;
  GtkRBTree *tree;
  GtkRBNode *node;
  gint height;
  gboolean dirty_marked;
  gboolean free_path = FALSE;

  g_return_if_fail (path != NULL || iter != NULL);

  if (path == NULL)
    {
      path = gtk_tree_model_get_path (model, iter);
      free_path = TRUE;
    }
  else if (iter == NULL)
    gtk_tree_model_get_iter (model, iter, path);

  if (_gtk_tree_view_find_node (tree_view,
				path,
				&tree,
				&node))
    /* We aren't actually showing the node */
    goto done;

  if (tree == NULL)
    goto done;
  
  dirty_marked = gtk_tree_view_discover_dirty_iter (tree_view,
						    iter,
						    gtk_tree_path_get_depth (path),
						    &height);

  if (GTK_RBNODE_GET_HEIGHT (node) != height + TREE_VIEW_VERTICAL_SEPARATOR)
    {
      _gtk_rbtree_node_set_height (tree, node, height + TREE_VIEW_VERTICAL_SEPARATOR);
      gtk_widget_queue_resize (GTK_WIDGET (data));
      goto done;
    }
  if (dirty_marked)
    gtk_widget_queue_resize (GTK_WIDGET (data));
  else
    {
      gtk_tree_view_queue_draw_node (tree_view, tree, node, NULL);
    }

 done:
  if (free_path)
    gtk_tree_path_free (path);
}

static void
gtk_tree_view_inserted (GtkTreeModel *model,
			GtkTreePath  *path,
			GtkTreeIter  *iter,
			gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *) data;
  gint *indices;
  GtkRBTree *tmptree, *tree;
  GtkRBNode *tmpnode = NULL;
  gint max_height;
  gint depth;
  gint i = 0;
  gboolean free_path = FALSE;

  tmptree = tree = tree_view->priv->tree;
  g_return_if_fail (path != NULL || iter != NULL);

  if (path == NULL)
    {
      path = gtk_tree_model_get_path (model, iter);
      free_path = TRUE;
    }
  else if (iter == NULL)
    gtk_tree_model_get_iter (model, iter, path);

  depth = gtk_tree_path_get_depth (path);
  indices = gtk_tree_path_get_indices (path);

  /* First, find the parent tree */
  while (i < depth - 1)
    {
      if (tmptree == NULL)
	{
	  /* We aren't showing the node */
          goto done;
	}

      tmpnode = _gtk_rbtree_find_count (tmptree, indices[i] + 1);
      if (tmpnode == NULL)
	{
	  g_warning ("A node was inserted with a parent that's not in the tree.\n" \
		     "This possibly means that a GtkTreeModel inserted a child node\n" \
		     "before the parent was inserted.");
          goto done;
	}
      else if (!GTK_RBNODE_FLAG_SET (tmpnode, GTK_RBNODE_IS_PARENT))
	{
          /* FIXME enforce correct behavior on model, probably */
	  /* In theory, the model should have emitted child_toggled here.  We
	   * try to catch it anyway, just to be safe, in case the model hasn't.
	   */
	  GtkTreePath *tmppath = _gtk_tree_view_find_path (tree_view,
							   tree,
							   tmpnode);
	  gtk_tree_view_child_toggled (model, tmppath, NULL, data);
	  gtk_tree_path_free (tmppath);
          goto done;
	}

      tmptree = tmpnode->children;
      tree = tmptree;
      i++;
    }

  if (tree == NULL)
    goto done;

  /* ref the node */
  gtk_tree_model_ref_iter (tree_view->priv->model, iter);
  max_height = gtk_tree_view_insert_iter_height (tree_view,
						 tree,
						 iter,
						 depth);
  if (indices[depth - 1] == 0)
    {
      tmpnode = _gtk_rbtree_find_count (tree, 1);
      _gtk_rbtree_insert_before (tree, tmpnode, max_height);
    }
  else
    {
      tmpnode = _gtk_rbtree_find_count (tree, indices[depth - 1]);
      _gtk_rbtree_insert_after (tree, tmpnode, max_height);
    }

  _gtk_tree_view_set_size (tree_view, -1, tree_view->priv->height + max_height);

 done:
  if (free_path)
    gtk_tree_path_free (path);
}

static void
gtk_tree_view_child_toggled (GtkTreeModel *model,
			     GtkTreePath  *path,
			     GtkTreeIter  *iter,
			     gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *)data;
  GtkTreeIter real_iter;
  gboolean has_child;
  GtkRBTree *tree;
  GtkRBNode *node;
  gboolean free_path = FALSE;

  g_return_if_fail (path != NULL || iter != NULL);

  if (iter)
    real_iter = *iter;

  if (path == NULL)
    {
      path = gtk_tree_model_get_path (model, iter);
      free_path = TRUE;
    }
  else if (iter == NULL)
    gtk_tree_model_get_iter (model, &real_iter, path);

  if (_gtk_tree_view_find_node (tree_view,
				path,
				&tree,
				&node))
    /* We aren't actually showing the node */
    goto done;

  if (tree == NULL)
    goto done;
  
  has_child = gtk_tree_model_iter_has_child (model, &real_iter);
  /* Sanity check.
   */
  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT) == has_child)
    goto done;

  if (has_child)
    GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_PARENT);
  else
    GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_IS_PARENT);

  if (has_child && GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IS_LIST))
    {
      GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_IS_LIST);
      if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_SHOW_EXPANDERS))
	{
	  GList *list;
	  for (list = tree_view->priv->columns; list; list = list->next)
	    if (GTK_TREE_VIEW_COLUMN (list->data)->visible)
	      {
		GTK_TREE_VIEW_COLUMN (list->data)->dirty = TRUE;
		break;
	      }
	}
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }
  else
    {
      /* FIXME: Just redraw the node */
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
    }

 done:
  if (free_path)
    gtk_tree_path_free (path);
}

static void
gtk_tree_view_deleted (GtkTreeModel *model,
		       GtkTreePath  *path,
		       gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *)data;
  GtkRBTree *tree;
  GtkRBNode *node;
  GList *list;
  
  g_return_if_fail (path != NULL);

  if (_gtk_tree_view_find_node (tree_view, path, &tree, &node))
    return;

  if (tree == NULL)
    return;
  
  /* next, update the selection */
  if (tree_view->priv->anchor)
    {
      GtkTreePath *anchor_path;

      /* the row reference may not have been updated yet. If it has not,
       * then anchor_path and path being equal indicates that the anchor
       * row was deleted. If it has, then anchor_path == NULL indicates the
       * the anchor row was deleted.
       */
      
      anchor_path = gtk_tree_row_reference_get_path (tree_view->priv->anchor);

      if (anchor_path == NULL ||
          gtk_tree_path_compare (path, anchor_path) == 0)
	{
	  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED) &&
              tree_view->priv->selection)
	    gtk_signal_emit_by_name (GTK_OBJECT (tree_view->priv->selection),
				     "selection_changed");
	}

      if (anchor_path)
        gtk_tree_path_free (anchor_path);
    }

  for (list = tree_view->priv->columns; list; list = list->next)
    if (((GtkTreeViewColumn *)list->data)->visible &&
	((GtkTreeViewColumn *)list->data)->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
      ((GtkTreeViewColumn *)list->data)->dirty = TRUE;

  /* Ensure we don't have a dangling pointer to a dead node */
  ensure_unprelighted (tree_view);

  g_assert (tree_view->priv->prelight_node == NULL);
  
  if (tree->root->count == 1)
    {
      _gtk_rbtree_remove (tree);
    }
  else
    {      
      _gtk_rbtree_remove_node (tree, node);
    }
  
  _gtk_tree_view_set_size (GTK_TREE_VIEW (data), -1, -1);
}

/* Internal tree functions */
static gint
gtk_tree_view_insert_iter_height (GtkTreeView *tree_view,
				  GtkRBTree   *tree,
				  GtkTreeIter *iter,
				  gint         depth)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GList *list;
  gint max_height = 0;
  gint i;

  i = 0;
  
  /* do stuff with node */
  for (list = tree_view->priv->columns; list; list = list->next)
    {
      gint height = 0, width = 0;
      column = list->data;

      if (!column->visible)
	continue;

      if (column->column_type == GTK_TREE_VIEW_COLUMN_FIXED)
        {
          ++i;
          continue;
        }

      cell = column->cell;
      gtk_tree_view_column_set_cell_data (column, tree_view->priv->model, iter);

      gtk_cell_renderer_get_size (cell, GTK_WIDGET (tree_view), &width, &height);
      max_height = MAX (max_height, TREE_VIEW_VERTICAL_SEPARATOR + height);

      if (i == tree_view->priv->expander_column &&
          TREE_VIEW_DRAW_EXPANDERS (tree_view))
	gtk_tree_view_column_set_width (column,
                                        MAX (column->width, depth * tree_view->priv->tab_offset + width));
      else
        gtk_tree_view_column_set_width (column,
                                        MAX (column->width, width));

      ++i;
    }
  return max_height;
}

static void
gtk_tree_view_build_tree (GtkTreeView *tree_view,
			  GtkRBTree   *tree,
			  GtkTreeIter *iter,
			  gint         depth,
			  gboolean     recurse,
			  gboolean     calc_bounds)
{
  GtkRBNode *temp = NULL;
  gint max_height;

  do
    {
      max_height = 0;
      if (calc_bounds)
	max_height = gtk_tree_view_insert_iter_height (tree_view,
						       tree,
						       iter,
						       depth);

      gtk_tree_model_ref_iter (tree_view->priv->model, iter);
      temp = _gtk_rbtree_insert_after (tree, temp, max_height);
      if (recurse)
	{
	  GtkTreeIter child;

	  if (gtk_tree_model_iter_children (tree_view->priv->model, &child, iter))
	    {
	      temp->children = _gtk_rbtree_new ();
	      temp->children->parent_tree = tree;
	      temp->children->parent_node = temp;
	      gtk_tree_view_build_tree (tree_view, temp->children, &child, depth + 1, recurse, calc_bounds);
	    }
	}
      if (gtk_tree_model_iter_has_child (tree_view->priv->model, iter))
	{
	  if ((temp->flags&GTK_RBNODE_IS_PARENT) != GTK_RBNODE_IS_PARENT)
	    temp->flags ^= GTK_RBNODE_IS_PARENT;
	  GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_IS_LIST);
	}
    }
  while (gtk_tree_model_iter_next (tree_view->priv->model, iter));
}

static void
gtk_tree_view_calc_size (GtkTreeView *tree_view,
			 GtkRBTree   *tree,
			 GtkTreeIter *iter,
			 gint         depth)
{
  GtkRBNode *temp;
  GtkTreeIter child;
  GtkCellRenderer *cell;
  GList *list;
  GtkTreeViewColumn *column;
  gint max_height;
  gint i;

  TREE_VIEW_INTERNAL_ASSERT_VOID (tree != NULL);

  temp = tree->root;
  while (temp->left != tree->nil)
    temp = temp->left;

  do
    {
      max_height = 0;
      /* Do stuff with node */
      for (list = tree_view->priv->columns, i = 0; i < tree_view->priv->n_columns; list = list->next, i++)
	{
	  gint height = 0, width = 0;
	  column = list->data;

	  if (!column->visible)
	    continue;

	  gtk_tree_view_column_set_cell_data (column, tree_view->priv->model, iter);
	  cell = column->cell;
	  gtk_cell_renderer_get_size (cell, GTK_WIDGET (tree_view), &width, &height);
	  max_height = MAX (max_height, TREE_VIEW_VERTICAL_SEPARATOR + height);

	  /* FIXME: I'm getting the width of all nodes here. )-: */
	  if (column->dirty == FALSE || column->column_type == GTK_TREE_VIEW_COLUMN_FIXED)
	    continue;

	  if (i == tree_view->priv->expander_column &&
              TREE_VIEW_DRAW_EXPANDERS (tree_view))
            gtk_tree_view_column_set_width (column,
                                            MAX (column->width, depth * tree_view->priv->tab_offset + width));
	  else
            gtk_tree_view_column_set_width (column, MAX (column->width, width));
	}
      _gtk_rbtree_node_set_height (tree, temp, max_height);
      if (temp->children != NULL &&
	  gtk_tree_model_iter_children (tree_view->priv->model, &child, iter))
	gtk_tree_view_calc_size (tree_view, temp->children, &child, depth + 1);
      temp = _gtk_rbtree_next (tree, temp);
    }
  while (gtk_tree_model_iter_next (tree_view->priv->model, iter));
}

static gboolean
gtk_tree_view_discover_dirty_iter (GtkTreeView *tree_view,
				   GtkTreeIter *iter,
				   gint         depth,
				   gint        *height)
{
  GtkCellRenderer *cell;
  GtkTreeViewColumn *column;
  GList *list;
  gint i;
  gint retval = FALSE;
  gint tmpheight;

  if (height)
    *height = 0;

  for (i = 0, list = tree_view->priv->columns; list; list = list->next, i++)
    {
      gint width;
      column = list->data;
      if (column->dirty == TRUE || column->column_type == GTK_TREE_VIEW_COLUMN_FIXED)
	continue;
      if (!column->visible)
	continue;

      cell = column->cell;
      gtk_tree_view_column_set_cell_data (column, tree_view->priv->model, iter);

      if (height)
	{
	  gtk_cell_renderer_get_size (cell, GTK_WIDGET (tree_view), &width, &tmpheight);
	  *height = MAX (*height, tmpheight);
	}
      else
	{
	  gtk_cell_renderer_get_size (cell, GTK_WIDGET (tree_view), &width, NULL);
	}
      if (i == tree_view->priv->expander_column &&
          TREE_VIEW_DRAW_EXPANDERS (tree_view))
	{
	  if (depth * tree_view->priv->tab_offset + width > column->width)
	    {
	      column->dirty = TRUE;
	      retval = TRUE;
	    }
	}
      else
	{
	  if (width > column->width)
	    {
	      column->dirty = TRUE;
	      retval = TRUE;
	    }
	}
    }

  return retval;
}

static void
gtk_tree_view_discover_dirty (GtkTreeView *tree_view,
			      GtkRBTree   *tree,
			      GtkTreeIter *iter,
			      gint         depth)
{
  GtkRBNode *temp = tree->root;
  GtkTreeViewColumn *column;
  GList *list;
  GtkTreeIter child;
  gboolean is_all_dirty;

  TREE_VIEW_INTERNAL_ASSERT_VOID (tree != NULL);

  while (temp->left != tree->nil)
    temp = temp->left;

  do
    {
      is_all_dirty = TRUE;
      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  column = list->data;
	  if (column->dirty == FALSE)
	    {
	      is_all_dirty = FALSE;
	      break;
	    }
	}

      if (is_all_dirty)
	return;

      gtk_tree_view_discover_dirty_iter (tree_view,
					 iter,
					 depth,
					 FALSE);
      if (gtk_tree_model_iter_children (tree_view->priv->model, &child, iter) &&
	  temp->children != NULL)
	gtk_tree_view_discover_dirty (tree_view, temp->children, &child, depth + 1);
      temp = _gtk_rbtree_next (tree, temp);
    }
  while (gtk_tree_model_iter_next (tree_view->priv->model, iter));
}


static void
gtk_tree_view_check_dirty (GtkTreeView *tree_view)
{
  GtkTreePath *path;
  gboolean dirty = FALSE;
  GList *list;
  GtkTreeViewColumn *column;
  GtkTreeIter iter;
  
  if (!GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_MODEL_SETUP) &&
      tree_view->priv->model)
    gtk_tree_view_setup_model (tree_view);
  
  for (list = tree_view->priv->columns; list; list = list->next)
    {
      column = list->data;
      if (column->dirty)
	{
	  dirty = TRUE;
	  if (column->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
	    {
              gint w = 1;

              if (column->button)
                w = MAX (w, column->button->requisition.width);
              
              gtk_tree_view_column_set_width (column, w);
	    }
	}
    }

  if (dirty == FALSE)
    return;

  if (tree_view->priv->model == NULL)
    return;
  
  path = gtk_tree_path_new_root ();
  if (gtk_tree_model_get_iter (tree_view->priv->model, &iter, path))
    {
      gtk_tree_view_calc_size (tree_view, tree_view->priv->tree, &iter, 1);
      _gtk_tree_view_set_size (tree_view, -1, -1);
    }
      
  gtk_tree_path_free (path);
  
  for (list = tree_view->priv->columns; list; list = list->next)
    {
      column = list->data;
      column->dirty = FALSE;
    }
}

static void
gtk_tree_view_create_button (GtkTreeView *tree_view,
			     gint         i)
{
  GtkWidget *button;
  GtkTreeViewColumn *column;

  column = g_list_nth (tree_view->priv->columns, i)->data;
  gtk_widget_push_composite_child ();
  button = column->button = gtk_button_new ();
  gtk_widget_pop_composite_child ();

  gtk_widget_set_parent (button, GTK_WIDGET (tree_view));\
  
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
  		      (GtkSignalFunc) gtk_tree_view_button_clicked,
		      (gpointer) tree_view);

  gtk_widget_show (button);
}

static void
gtk_tree_view_create_buttons (GtkTreeView *tree_view)
{
  GtkWidget *alignment;
  GtkWidget *label;
  GList *list;
  GtkTreeViewColumn *column;
  gint i;
  GtkWidget *hbox;
  GtkWidget *arrow;

  /* FIXME this has to be merged with update_button_contents() in
   * gtktreeviewcolumn.c
   */
  
  for (list = tree_view->priv->columns, i = 0; list; list = list->next, i++)
    {
      column = list->data;
      
      if (column->button != NULL)
	continue;

      gtk_tree_view_create_button (tree_view, i);
      alignment = gtk_alignment_new (column->xalign, 0.5, 0.0, 0.0);

      hbox = gtk_hbox_new (FALSE, 2);
      arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_IN);

      column->arrow = arrow;
      column->alignment = alignment;
      
      if (column->child)
        label = column->child;
      else
        {
          label = gtk_label_new (column->title);
          gtk_widget_show (label);
        }

      if (column->xalign <= 0.5)
        gtk_box_pack_end (GTK_BOX (hbox), arrow, FALSE, FALSE, 0);
      else
        gtk_box_pack_start (GTK_BOX (hbox), arrow, FALSE, FALSE, 0);
      
      gtk_box_pack_start (GTK_BOX (hbox), alignment, TRUE, TRUE, 0);
        
      gtk_container_add (GTK_CONTAINER (alignment), label);
      gtk_container_add (GTK_CONTAINER (column->button), hbox);

      gtk_widget_show (hbox);
      gtk_widget_show (alignment);
      /* don't show the arrow yet */
    }

  gtk_tree_view_size_request_buttons (tree_view);
  
  if (GTK_WIDGET_REALIZED (tree_view))
    gtk_tree_view_realize_buttons (tree_view);

  if (GTK_WIDGET_MAPPED (tree_view))
    gtk_tree_view_map_buttons (tree_view);  
}

static void
gtk_tree_view_button_clicked (GtkWidget *widget,
			      gpointer   data)
{
  GList *list;
  GtkTreeView *tree_view;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (data));

  tree_view = GTK_TREE_VIEW (data);

  /* find the column whose button was pressed */
  for (list = tree_view->priv->columns; list; list = list->next)
    if (GTK_TREE_VIEW_COLUMN (list->data)->button == widget)
      break;

  if (list)
    gtk_tree_view_column_clicked (GTK_TREE_VIEW_COLUMN (list->data));
}

/* Make sure the node is visible vertically */
static void
gtk_tree_view_clamp_node_visible (GtkTreeView *tree_view,
				  GtkRBTree   *tree,
				  GtkRBNode   *node)
{
  gint offset;

  offset = _gtk_rbtree_node_find_offset (tree, node);

  /* we reverse the order, b/c in the unusual case of the
   * node's height being taller then the visible area, we'd rather
   * have the node flush to the top
   */
  if (offset + GTK_RBNODE_GET_HEIGHT (node) >
      tree_view->priv->vadjustment->value + tree_view->priv->vadjustment->page_size)
    gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->vadjustment),
			      offset + GTK_RBNODE_GET_HEIGHT (node) -
			      tree_view->priv->vadjustment->page_size);
  if (offset < tree_view->priv->vadjustment->value)
    gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->vadjustment),
			      offset);
}

/* This function could be more efficient.
 * I'll optimize it if profiling seems to imply that
 * it's important
 */
GtkTreePath *
_gtk_tree_view_find_path (GtkTreeView *tree_view,
			  GtkRBTree   *tree,
			  GtkRBNode   *node)
{
  GtkTreePath *path;
  GtkRBTree *tmp_tree;
  GtkRBNode *tmp_node, *last;
  gint count;

  path = gtk_tree_path_new ();

  g_return_val_if_fail (node != NULL, path);
  g_return_val_if_fail (node != tree->nil, path);

  count = 1 + node->left->count;

  last = node;
  tmp_node = node->parent;
  tmp_tree = tree;
  while (tmp_tree)
    {
      while (tmp_node != tmp_tree->nil)
	{
	  if (tmp_node->right == last)
	    count += 1 + tmp_node->left->count;
	  last = tmp_node;
	  tmp_node = tmp_node->parent;
	}
      gtk_tree_path_prepend_index (path, count - 1);
      last = tmp_tree->parent_node;
      tmp_tree = tmp_tree->parent_tree;
      if (last)
	{
	  count = 1 + last->left->count;
	  tmp_node = last->parent;
	}
    }
  return path;
}

/* Returns TRUE if we ran out of tree before finding the node,
 * so the returned node is the last node we saw and the returned
 * tree is NULL
 */
gboolean
_gtk_tree_view_find_node (GtkTreeView  *tree_view,
			  GtkTreePath  *path,
			  GtkRBTree   **tree,
			  GtkRBNode   **node)
{
  GtkRBNode *tmpnode = NULL;
  GtkRBTree *tmptree = tree_view->priv->tree;
  gint *indices = gtk_tree_path_get_indices (path);
  gint depth = gtk_tree_path_get_depth (path);
  gint i = 0;

  *node = NULL;
  *tree = NULL;

  do
    {
      if (tmptree == NULL)
	{
	  *node = tmpnode;
	  *tree = tmptree;
	  return TRUE;
	}
      tmpnode = _gtk_rbtree_find_count (tmptree, indices[i] + 1);
      ++i;
      if (i >= depth)
	{
	  *node = tmpnode;
	  *tree = tmptree;
	  return FALSE;
	}
      tmptree = tmpnode->children;
    }
  while (1);
}

static void
gtk_tree_view_queue_draw_node (GtkTreeView  *tree_view,
			       GtkRBTree    *tree,
			       GtkRBNode    *node,
			       GdkRectangle *clip_rect)
{
  GdkRectangle rect;

  if (!GTK_WIDGET_REALIZED (tree_view))
    return;
  
  rect.x = 0;
  rect.width = tree_view->priv->width;

  rect.y = BACKGROUND_FIRST_PIXEL (tree_view, tree, node);
  rect.height = BACKGROUND_HEIGHT (node);

  if (clip_rect)
    {
      GdkRectangle new_rect;

      gdk_rectangle_intersect (clip_rect, &rect, &new_rect);

      gdk_window_invalidate_rect (tree_view->priv->bin_window, &new_rect, TRUE);
    }
  else
    {
      gdk_window_invalidate_rect (tree_view->priv->bin_window, &rect, TRUE);
    }
}

static void
gtk_tree_view_queue_draw_path (GtkTreeView      *tree_view,
                               GtkTreePath      *path,
                               GdkRectangle     *clip_rect)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;
  
  _gtk_tree_view_find_node (tree_view, path, &tree, &node);

  if (tree)
    gtk_tree_view_queue_draw_node (tree_view, tree, node, clip_rect);
}

/* x and y are the mouse position
 */
static void
gtk_tree_view_draw_arrow (GtkTreeView *tree_view,
                          GtkRBTree   *tree,
			  GtkRBNode   *node,
			  gint         x,
			  gint         y)
{
  GdkRectangle area;
  GtkStateType state;
  GtkWidget *widget;
  gint x_offset = 0;
  
  if (! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT))
    return;

  widget = GTK_WIDGET (tree_view);

  gtk_tree_view_get_arrow_xrange (tree_view, &x_offset, NULL);
  
  area.x = x_offset;
  area.y = CELL_FIRST_PIXEL (tree_view, tree, node);
  area.width = tree_view->priv->tab_offset - 2;
  area.height = CELL_HEIGHT (node);

  if (node == tree_view->priv->button_pressed_node)
    {
      if (x >= area.x && x <= (area.x + area.width) &&
	  y >= area.y && y <= (area.y + area.height))
        state = GTK_STATE_ACTIVE;
      else
        state = GTK_STATE_NORMAL;
    }
  else
    {
      if (node == tree_view->priv->prelight_node &&
	  GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT))
	state = GTK_STATE_PRELIGHT;
      else
	state = GTK_STATE_NORMAL;
    }

  /* FIXME expander size should come from a style property */
#define EXPANDER_SIZE 8
  gtk_paint_expander (widget->style,
                      tree_view->priv->bin_window,
                      state,
                      &area,
                      widget,
                      "treeview",
                      area.x,
                      (area.y + (area.height - EXPANDER_SIZE) / 2 - (area.height + 1) % 2),
                      node->children != NULL);
#undef EXPANDER_SIZE
}

void
_gtk_tree_view_set_size (GtkTreeView     *tree_view,
			 gint             width,
			 gint             height)
{
  GList *list;
  GtkTreeViewColumn *column;
  gint i;

  if (width == tree_view->priv->width &&
      height == tree_view->priv->height)
    return;
  
  if (tree_view->priv->model == NULL)
    {
      tree_view->priv->width = width;
      tree_view->priv->height = height;
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
      return;
    }
  if (width == -1)
    {
      width = 0;
      for (list = tree_view->priv->columns, i = 0; list; list = list->next, i++)
	{
	  column = list->data;
	  if (!column->visible)
	    continue;
	  width += TREE_VIEW_COLUMN_WIDTH (column);
	}
    }

  if (height == -1)
    height = tree_view->priv->tree->root->offset + TREE_VIEW_VERTICAL_SEPARATOR;

  tree_view->priv->width = width;
  tree_view->priv->height = height;

  if (tree_view->priv->hadjustment->upper != tree_view->priv->width)
    {
      tree_view->priv->hadjustment->upper = tree_view->priv->width;
      gtk_signal_emit_by_name (GTK_OBJECT (tree_view->priv->hadjustment), "changed");
    }

  if (tree_view->priv->vadjustment->upper != tree_view->priv->height)
    {
      tree_view->priv->vadjustment->upper = tree_view->priv->height;
      gtk_signal_emit_by_name (GTK_OBJECT (tree_view->priv->vadjustment), "changed");
    }

  if (GTK_WIDGET_REALIZED (tree_view))
    {
      gdk_window_resize (tree_view->priv->bin_window, MAX (width, GTK_WIDGET (tree_view)->allocation.width), height + TREE_VIEW_HEADER_HEIGHT (tree_view));
      gdk_window_resize (tree_view->priv->header_window, MAX (width, GTK_WIDGET (tree_view)->allocation.width), tree_view->priv->header_height);
    }
  
  gtk_widget_queue_resize (GTK_WIDGET (tree_view));
}

/* this function returns the new width of the column being resized given
 * the column and x position of the cursor; the x cursor position is passed
 * in as a pointer and automagicly corrected if it's beyond min/max limits
 */
static gint
gtk_tree_view_new_column_width (GtkTreeView *tree_view,
				gint       i,
				gint      *x)
{
  GtkTreeViewColumn *column;
  gint width;

  /* first translate the x position from widget->window
   * to clist->clist_window
   */

  column = g_list_nth (tree_view->priv->columns, i)->data;
  width = *x - column->button->allocation.x;

  /* Clamp down the value */
  if (column->min_width == -1)
    width = MAX (column->button->requisition.width,
		 width);
  else
    width = MAX (column->min_width,
		 width);
  if (column->max_width != -1)
    width = MIN (width, column->max_width != -1);
  *x = column->button->allocation.x + width;

  return width;
}

/* Callbacks */
static void
gtk_tree_view_adjustment_changed (GtkAdjustment *adjustment,
				  GtkTreeView     *tree_view)
{
  if (GTK_WIDGET_REALIZED (tree_view))
    {
      gdk_window_move (tree_view->priv->bin_window,
		       - tree_view->priv->hadjustment->value,
		       - tree_view->priv->vadjustment->value);
      gdk_window_move (tree_view->priv->header_window,
		       - tree_view->priv->hadjustment->value,
		       0);

      gdk_window_process_updates (tree_view->priv->bin_window, TRUE);
      gdk_window_process_updates (tree_view->priv->header_window, TRUE);
    }
}



/* Public methods
 */

/**
 * gtk_tree_view_new:
 * 
 * Creates a new #GtkTreeView widget.
 * 
 * Return value: A newly created #GtkTreeView widget.
 **/
GtkWidget *
gtk_tree_view_new (void)
{
  GtkTreeView *tree_view;

  tree_view = GTK_TREE_VIEW (gtk_type_new (gtk_tree_view_get_type ()));

  return GTK_WIDGET (tree_view);
}

/**
 * gtk_tree_view_new_with_model:
 * @model: the model.
 * 
 * Creates a new #GtkTreeView widget with the model initialized to @model.
 * 
 * Return value: A newly created #GtkTreeView widget.
 **/
GtkWidget *
gtk_tree_view_new_with_model (GtkTreeModel *model)
{
  GtkTreeView *tree_view;

  tree_view = GTK_TREE_VIEW (gtk_type_new (gtk_tree_view_get_type ()));
  gtk_tree_view_set_model (tree_view, model);

  return GTK_WIDGET (tree_view);
}

/**
 * gtk_tree_view_get_model:
 * @tree_view: a #GtkTreeView
 * 
 * Returns the model the the #GtkTreeView is based on.  Returns NULL if the
 * model is unset.
 * 
 * Return value: A #GtkTreeModel, or NULL if none is currently being used.
 **/
GtkTreeModel *
gtk_tree_view_get_model (GtkTreeView *tree_view)
{
  g_return_val_if_fail (tree_view != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->model;
}

static void
gtk_tree_view_setup_model (GtkTreeView *tree_view)
{
  GtkTreePath *path;
  GtkTreeIter iter;

  tree_view->priv->tree = _gtk_rbtree_new ();

  g_signal_connect (tree_view->priv->model,
		    "changed",
		    gtk_tree_view_changed,
		    tree_view);
  g_signal_connect (tree_view->priv->model,
		    "inserted",
		    gtk_tree_view_inserted,
		    tree_view);
  g_signal_connect (tree_view->priv->model,
		    "child_toggled",
		    gtk_tree_view_child_toggled,
		    tree_view);
  g_signal_connect (tree_view->priv->model,
		    "deleted",
		    gtk_tree_view_deleted,
		    tree_view);

  if (tree_view->priv->columns == NULL)
    return;

  path = gtk_tree_path_new_root ();

  if (gtk_tree_model_get_iter (tree_view->priv->model, &iter, path))
    {
      gtk_tree_view_build_tree (tree_view, tree_view->priv->tree, &iter, 1, FALSE, GTK_WIDGET_REALIZED (tree_view));
    }
  
  gtk_tree_path_free (path);

  gtk_tree_view_create_buttons (tree_view);

  GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_MODEL_SETUP);
}

/**
 * gtk_tree_view_set_model:
 * @tree_view: A #GtkTreeNode.
 * @model: The model.
 * 
 * Sets the model for a #GtkTreeView.  If the @tree_view already has a model
 * set, it will remove it before setting the new model.  If @model is NULL, then
 * it will unset the old model.
 **/
void
gtk_tree_view_set_model (GtkTreeView  *tree_view,
			 GtkTreeModel *model)
{
  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->model != NULL)
    {

      /* No longer do this. */
#if 0
      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  column = list->data;
	  if (column->button)
	    {
	      gtk_widget_unparent (column->button);
	      gdk_window_set_user_data (column->window, NULL);
	      gdk_window_destroy (column->window);
	    }
	}
#endif
      if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_MODEL_SETUP))
	{
	  gtk_signal_disconnect_by_func (GTK_OBJECT (tree_view->priv->model),
					 gtk_tree_view_changed,
					 tree_view);
	  gtk_signal_disconnect_by_func (GTK_OBJECT (tree_view->priv->model),
					 gtk_tree_view_inserted,
					 tree_view);
	  gtk_signal_disconnect_by_func (GTK_OBJECT (tree_view->priv->model),
					 gtk_tree_view_child_toggled,
					 tree_view);
	  gtk_signal_disconnect_by_func (GTK_OBJECT (tree_view->priv->model),
					 gtk_tree_view_deleted,
					 tree_view);
	  _gtk_rbtree_free (tree_view->priv->tree);
	}
#if 0
      g_list_free (tree_view->priv->columns);
      tree_view->priv->columns = NULL;
#endif

      if (tree_view->priv->drag_dest_row)
        gtk_tree_row_reference_free (tree_view->priv->drag_dest_row);
      
      GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_MODEL_SETUP);
    }

  tree_view->priv->model = model;
  if (model == NULL)
    {
      tree_view->priv->tree = NULL;
      if (GTK_WIDGET_REALIZED (tree_view))
	_gtk_tree_view_set_size (tree_view, 0, 0);
      return;
    }
  else if (GTK_WIDGET_REALIZED (tree_view))
    {
      gtk_tree_view_setup_model (tree_view);
      _gtk_tree_view_set_size (tree_view, -1, -1);
    }
}

/**
 * gtk_tree_view_get_selection:
 * @tree_view: A #GtkTreeView.
 * 
 * Gets the #GtkTreeSelection associated with @tree_view.
 * 
 * Return value: A #GtkTreeSelection object.
 **/
GtkTreeSelection *
gtk_tree_view_get_selection (GtkTreeView *tree_view)
{
  g_return_val_if_fail (tree_view != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  if (tree_view->priv->selection == NULL)
    tree_view->priv->selection =
      _gtk_tree_selection_new_with_tree_view (tree_view);

  return tree_view->priv->selection;
}

/**
 * gtk_tree_view_get_hadjustment:
 * @tree_view: A #GtkTreeView
 * 
 * Gets the #GtkAdjustment currently being used for the horizontal aspect.
 * 
 * Return value: A #GtkAdjustment object, or NULL if none is currently being
 * used.
 **/
GtkAdjustment *
gtk_tree_view_get_hadjustment (GtkTreeView *tree_view)
{
  g_return_val_if_fail (tree_view != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  if (tree_view->priv->hadjustment == NULL)
    gtk_tree_view_set_hadjustment (tree_view, NULL);
  
  return tree_view->priv->hadjustment;
}

/**
 * gtk_tree_view_set_hadjustment:
 * @tree_view: A #GtkTreeView
 * @adjustment: The #GtkAdjustment to set, or NULL
 * 
 * Sets the #GtkAdjustment for the current horizontal aspect.
 **/
void
gtk_tree_view_set_hadjustment (GtkTreeView   *tree_view,
			       GtkAdjustment *adjustment)
{
  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_tree_view_set_adjustments (tree_view,
				 adjustment,
				 tree_view->priv->vadjustment);
}

/**
 * gtk_tree_view_get_vadjustment:
 * @tree_view: A #GtkTreeView
 * 
 * Gets the #GtkAdjustment currently being used for the vertical aspect.
 * 
 * Return value: A #GtkAdjustment object, or NULL if none is currently being
 * used.
 **/
GtkAdjustment *
gtk_tree_view_get_vadjustment (GtkTreeView *tree_view)
{
  g_return_val_if_fail (tree_view != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  if (tree_view->priv->vadjustment == NULL)
    gtk_tree_view_set_vadjustment (tree_view, NULL);
  
  return tree_view->priv->vadjustment;
}

/**
 * gtk_tree_view_set_vadjustment:
 * @tree_view: A #GtkTreeView
 * @adjustment: The #GtkAdjustment to set, or NULL
 * 
 * Sets the #GtkAdjustment for the current vertical aspect.
 **/
void
gtk_tree_view_set_vadjustment (GtkTreeView   *tree_view,
			       GtkAdjustment *adjustment)
{
  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_tree_view_set_adjustments (tree_view,
				 tree_view->priv->hadjustment,
				 adjustment);
}

/**
 * gtk_tree_view_set_adjustments:
 * @tree_view: A #GtkTreeView
 * @hadj: The horizontal #GtkAdjustment to set, or NULL
 * @vadj: The vertical #GtkAdjustment to set, or NULL
 * 
 * Sets the horizonal and or vertical #GtkAdjustment.
 **/
static void
gtk_tree_view_set_adjustments (GtkTreeView   *tree_view,
			       GtkAdjustment *hadj,
			       GtkAdjustment *vadj)
{
  gboolean need_adjust = FALSE;

  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (hadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
  else
    hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  if (vadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
  else
    vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

  if (tree_view->priv->hadjustment && (tree_view->priv->hadjustment != hadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (tree_view->priv->hadjustment), tree_view);
      gtk_object_unref (GTK_OBJECT (tree_view->priv->hadjustment));
    }

  if (tree_view->priv->vadjustment && (tree_view->priv->vadjustment != vadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (tree_view->priv->vadjustment), tree_view);
      gtk_object_unref (GTK_OBJECT (tree_view->priv->vadjustment));
    }

  if (tree_view->priv->hadjustment != hadj)
    {
      tree_view->priv->hadjustment = hadj;
      gtk_object_ref (GTK_OBJECT (tree_view->priv->hadjustment));
      gtk_object_sink (GTK_OBJECT (tree_view->priv->hadjustment));

      gtk_signal_connect (GTK_OBJECT (tree_view->priv->hadjustment), "value_changed",
			  (GtkSignalFunc) gtk_tree_view_adjustment_changed,
			  tree_view);
      need_adjust = TRUE;
    }

  if (tree_view->priv->vadjustment != vadj)
    {
      tree_view->priv->vadjustment = vadj;
      gtk_object_ref (GTK_OBJECT (tree_view->priv->vadjustment));
      gtk_object_sink (GTK_OBJECT (tree_view->priv->vadjustment));

      gtk_signal_connect (GTK_OBJECT (tree_view->priv->vadjustment), "value_changed",
			  (GtkSignalFunc) gtk_tree_view_adjustment_changed,
			  tree_view);
      need_adjust = TRUE;
    }

  if (need_adjust)
    gtk_tree_view_adjustment_changed (NULL, tree_view);
}


/* Column and header operations */

/**
 * gtk_tree_view_get_headers_visible:
 * @tree_view: A #GtkTreeView.
 * 
 * Returns TRUE if the headers on the @tree_view are visible.
 * 
 * Return value: Whether the headers are visible or not.
 **/
gboolean
gtk_tree_view_get_headers_visible (GtkTreeView *tree_view)
{
  g_return_val_if_fail (tree_view != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE);
}

/**
 * gtk_tree_view_set_headers_visible:
 * @tree_view: A #GtkTreeView.
 * @headers_visible: TRUE if the headers are visible
 * 
 * Sets the the visibility state of the headers.
 **/
void
gtk_tree_view_set_headers_visible (GtkTreeView *tree_view,
				   gboolean     headers_visible)
{
  gint x, y;
  GList *list;
  GtkTreeViewColumn *column;

  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  headers_visible = !! headers_visible;
  
  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE) == headers_visible)
    return;

  if (headers_visible)
    GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE);
  else
    GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE);

  if (GTK_WIDGET_REALIZED (tree_view))
    {
      gdk_window_get_position (tree_view->priv->bin_window, &x, &y);
      if (headers_visible)
	{
	  gdk_window_move_resize (tree_view->priv->bin_window, x, y, tree_view->priv->width, tree_view->priv->height + TREE_VIEW_HEADER_HEIGHT (tree_view));
          
          if (GTK_WIDGET_MAPPED (tree_view))
            gtk_tree_view_map_buttons (tree_view);
 	}
      else
	{
	  gdk_window_move_resize (tree_view->priv->bin_window, x, y, tree_view->priv->width, tree_view->priv->height);

	  for (list = tree_view->priv->columns; list; list = list->next)
	    {
	      column = list->data;
	      gtk_widget_unmap (column->button);
	    }
	  gdk_window_hide (tree_view->priv->header_window);
	}
    }

  tree_view->priv->vadjustment->page_size = GTK_WIDGET (tree_view)->allocation.height - TREE_VIEW_HEADER_HEIGHT (tree_view);
  tree_view->priv->vadjustment->page_increment = (GTK_WIDGET (tree_view)->allocation.height - TREE_VIEW_HEADER_HEIGHT (tree_view)) / 2;
  tree_view->priv->vadjustment->lower = 0;
  tree_view->priv->vadjustment->upper = tree_view->priv->height;
  gtk_signal_emit_by_name (GTK_OBJECT (tree_view->priv->vadjustment), "changed");

  gtk_widget_queue_resize (GTK_WIDGET (tree_view));
}


/**
 * gtk_tree_view_columns_autosize:
 * @tree_view: A #GtkTreeView.
 * 
 * Resizes all columns to their optimal width.
 **/
void
gtk_tree_view_columns_autosize (GtkTreeView *tree_view)
{
  gboolean dirty = FALSE;
  GList *list;
  GtkTreeViewColumn *column;

  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      column = list->data;
      if (column->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
	continue;
      column->dirty = TRUE;
      dirty = TRUE;
    }

  if (dirty)
    gtk_widget_queue_resize (GTK_WIDGET (tree_view));
}

/**
 * gtk_tree_view_set_headers_clickable:
 * @tree_view: A #GtkTreeView.
 * @setting: TRUE if the columns are clickable.
 * 
 * Allow the column title buttons to be clicked.
 **/
void
gtk_tree_view_set_headers_clickable (GtkTreeView *tree_view,
				     gboolean   setting)
{
  GList *list;

  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (tree_view->priv->model != NULL);

  for (list = tree_view->priv->columns; list; list = list->next)
    gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (list->data), setting);
}

/**
 * gtk_tree_view_append_column:
 * @tree_view: A #GtkTreeView.
 * @column: The #GtkTreeViewColumn to add.
 * 
 * Appends @column to the list of columns.
 * 
 * Return value: The number of columns in @tree_view after appending.
 **/
gint
gtk_tree_view_append_column (GtkTreeView       *tree_view,
			     GtkTreeViewColumn *column)
{
  g_return_val_if_fail (tree_view != NULL, -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);
  g_return_val_if_fail (column != NULL, -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), -1);
  g_return_val_if_fail (column->tree_view == NULL, -1);

  g_object_ref (G_OBJECT (column));
  tree_view->priv->columns = g_list_append (tree_view->priv->columns, column);
  column->tree_view = GTK_WIDGET (tree_view);

  tree_view->priv->n_columns++;

  /* FIXME create header for the new column! */
  
  return tree_view->priv->n_columns;
}


/**
 * gtk_tree_view_remove_column:
 * @tree_view: A #GtkTreeView.
 * @column: The #GtkTreeViewColumn to remove.
 * 
 * Removes @column from @tree_view.
 * 
 * Return value: The number of columns in @tree_view after removing.
 **/
gint
gtk_tree_view_remove_column (GtkTreeView       *tree_view,
                             GtkTreeViewColumn *column)
{
  g_return_val_if_fail (tree_view != NULL, -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);
  g_return_val_if_fail (column != NULL, -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), -1);
  g_return_val_if_fail (column->tree_view == GTK_WIDGET (tree_view), -1);

  tree_view->priv->columns = g_list_remove (tree_view->priv->columns,
                                           column);
  column->tree_view = NULL;
  g_object_unref (G_OBJECT (column));

  tree_view->priv->n_columns--;

  /* FIXME destroy header for the column! */
  
  return tree_view->priv->n_columns;
}

/**
 * gtk_tree_view_insert_column:
 * @tree_view: A #GtkTreeView.
 * @column: The #GtkTreeViewColumn to be inserted.
 * @position: The position to insert @column in.
 * 
 * This inserts the @column into the @tree_view at @position.
 * 
 * Return value: The number of columns in @tree_view after insertion.
 **/
gint
gtk_tree_view_insert_column (GtkTreeView       *tree_view,
                             GtkTreeViewColumn *column,
                             gint               position)
{
  g_return_val_if_fail (tree_view != NULL, -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);
  g_return_val_if_fail (column != NULL, -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), -1);
  g_return_val_if_fail (column->tree_view == NULL, -1);

  g_object_ref (G_OBJECT (column));

  tree_view->priv->columns = g_list_insert (tree_view->priv->columns,
                                           column, position);
  column->tree_view = GTK_WIDGET (tree_view);

  tree_view->priv->n_columns++;

  /* FIXME create header for the column! */
  
  return tree_view->priv->n_columns;
}

/**
 * gtk_tree_view_get_column:
 * @tree_view: A #GtkTreeView.
 * @n: The position of the column, counting from 0.
 * 
 * Gets the #GtkTreeViewColumn at the given position in the #tree_view.
 * 
 * Return value: The #GtkTreeViewColumn, or NULL if the position is outside the
 * range of columns.
 **/
GtkTreeViewColumn *
gtk_tree_view_get_column (GtkTreeView *tree_view,
			  gint         n)
{
  g_return_val_if_fail (tree_view != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);
  g_return_val_if_fail (tree_view->priv->model != NULL, NULL);

  if (n < 0 || n >= tree_view->priv->n_columns)
    return NULL;
  
  if (tree_view->priv->columns == NULL)
    return NULL;

  return GTK_TREE_VIEW_COLUMN (g_list_nth (tree_view->priv->columns, n)->data);
}

void
gtk_tree_view_set_expander_column (GtkTreeView *tree_view,
                                   gint         col)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->expander_column != col)
    {
      tree_view->priv->expander_column = col;

      /*   g_object_notify (G_OBJECT (tree_view), "expander_column"); */
    }
}

gint
gtk_tree_view_get_expander_column (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);

  return tree_view->priv->expander_column;
}

/**
 * gtk_tree_view_scroll_to_point:
 * @tree_view: a #GtkTreeView
 * @tree_x: X coordinate of new top-left pixel of visible area
 * @tree_y: Y coordinate of new top-left pixel of visible area
 * 
 * Scrolls the tree view such that the top-left corner of the visible
 * area is @tree_x, @tree_y, where @tree_x and @tree_y are specified
 * in tree window coordinates.  The @tree_view must be realized before
 * this function is called.  If it isn't, you probably want ot be
 * using gtk_tree_view_scroll_to_cell.
 **/
void
gtk_tree_view_scroll_to_point (GtkTreeView *tree_view,
                               gint         tree_x,
                               gint         tree_y)
{
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_WIDGET_REALIZED (tree_view));

  hadj = tree_view->priv->hadjustment;
  vadj = tree_view->priv->vadjustment;

  gtk_adjustment_set_value (hadj, CLAMP (tree_x, hadj->lower, hadj->upper));
  gtk_adjustment_set_value (vadj, CLAMP (tree_y, vadj->lower, vadj->upper));  
}

/**
 * gtk_tree_view_scroll_to_cell
 * @tree_view: A #GtkTreeView.
 * @path: The path of the row to move to.
 * @column: The #GtkTreeViewColumn to move horizontally to.
 * @row_align: The vertical alignment of the row specified by @path.
 * @col_align: The horizontal alignment of the column specified by @column.
 * 
 * Moves the alignments of @tree_view to the position specified by
 * @column and @path.  If @column is NULL, then no horizontal
 * scrolling occurs.  Likewise, if @path is NULL no vertical scrolling
 * occurs.  @row_align determines where the row is placed, and
 * @col_align determines where @column is placed.  Both are expected
 * to be between 0.0 and 1.0. 0.0 means left/top alignment, 1.0 means
 * right/bottom alignment, 0.5 means center.
 **/
void
gtk_tree_view_scroll_to_cell (GtkTreeView       *tree_view,
                              GtkTreePath       *path,
                              GtkTreeViewColumn *column,
                              gfloat             row_align,
                              gfloat             col_align)
{
  GdkRectangle cell_rect;
  GdkRectangle vis_rect;
  gint dest_x, dest_y;
  
  /* FIXME work on unmapped/unrealized trees? maybe implement when
   * we do incremental reflow for trees
   */
  
  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (row_align >= 0.0);
  g_return_if_fail (row_align <= 1.0);
  g_return_if_fail (col_align >= 0.0);
  g_return_if_fail (col_align <= 1.0);
  g_return_if_fail (path != NULL || column != NULL);

  row_align = CLAMP (row_align, 0.0, 1.0);
  col_align = CLAMP (col_align, 0.0, 1.0);

  if (! GTK_WIDGET_REALIZED (tree_view))
    {
      if (path)
	tree_view->priv->scroll_to_path = gtk_tree_path_copy (path);
      if (column)
	tree_view->priv->scroll_to_column = column;
      tree_view->priv->scroll_to_row_align = row_align;
      tree_view->priv->scroll_to_col_align = col_align;

      return;
    }

  gtk_tree_view_get_cell_area (tree_view, path, column, &cell_rect);
  gtk_tree_view_get_visible_rect (tree_view, &vis_rect);

  dest_x = vis_rect.x;
  dest_y = vis_rect.y;
  
  if (path)
    {
      dest_x = cell_rect.x +
        cell_rect.width * row_align -
        vis_rect.width * row_align;
    }

  if (column)
    {
      dest_y = cell_rect.y +
        cell_rect.height * col_align -
        vis_rect.height * col_align;
    }

  gtk_tree_view_scroll_to_point (tree_view, dest_x, dest_y);
}

/**
 * gtk_tree_view_get_path_at_pos:
 * @tree_view: A #GtkTreeView.
 * @window: The #GdkWindow to check against.
 * @x: The x position to be identified.
 * @y: The y position to be identified.
 * @path: A pointer to a #GtkTreePath pointer to be filled in, or %NULL
 * @column: A pointer to a #GtkTreeViewColumn pointer to be filled in, or %NULL
 * @cell_x: A pointer where the X coordinate relative to the cell can be placed, or %NULL
 * @cell_y: A pointer where the Y coordinate relative to the cell can be placed, or %NULL
 * 
 * Finds the path at the point (@x, @y) relative to @window.  If
 * @window is NULL, then the point is found relative to the widget
 * coordinates.  This function is expected to be called after an
 * event, with event->window being passed in as @window.  It is
 * primarily for things like popup menus.  If @path is non-NULL, then
 * it will be filled with the #GtkTreePath at that point.  This path
 * should be freed with #gtk_tree_path_free.  If @column is non-NULL,
 * then it will be filled with the column at that point. @cell_x and
 * @cell_y return the coordinates relative to the cell background
 * (i.e. the background_area passed to gtk_cell_renderer_render()).
 * 
 * Return value: TRUE if a row exists at that coordinate.
 **/
gboolean
gtk_tree_view_get_path_at_pos (GtkTreeView        *tree_view,
			       GdkWindow          *window,
			       gint                x,
			       gint                y,
			       GtkTreePath       **path,
			       GtkTreeViewColumn **column,
                               gint               *cell_x,
                               gint               *cell_y)
{
  GtkRBTree *tree;
  GtkRBNode *node;
  gint y_offset;
  
  g_return_val_if_fail (tree_view != NULL, FALSE);
  g_return_val_if_fail (tree_view->priv->tree != NULL, FALSE);
  g_return_val_if_fail (tree_view->priv->bin_window != NULL, FALSE);

  if (window)
    g_return_val_if_fail (window == tree_view->priv->bin_window, FALSE);

  if (path)
    *path = NULL;
  if (column)
    *column = NULL;

  if (x > tree_view->priv->hadjustment->upper)
    return FALSE;

  if (x < 0 || y < 0)
    return FALSE;
  
  if (column || cell_x)
    {
      GtkTreeViewColumn *tmp_column;
      GtkTreeViewColumn *last_column = NULL;
      GList *list;
      gint remaining_x = x;
      gboolean found = FALSE;
      
      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  tmp_column = list->data;

	  if (tmp_column->visible == FALSE)
	    continue;

	  last_column = tmp_column;
	  if (remaining_x <= tmp_column->width)
	    {
              found = TRUE;
              
              if (column)
                *column = tmp_column;

              if (cell_x)
                *cell_x = remaining_x;
              
	      break;
	    }
	  remaining_x -= tmp_column->width;
	}

      if (!found)
        {
          if (column)
            *column = last_column;

          if (cell_x)
            *cell_x = last_column->width + remaining_x;
        }
    }

  if (window)
    {
      y_offset = _gtk_rbtree_find_offset (tree_view->priv->tree,
                                          TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, y),
                                          &tree, &node);
    }
  else
    {
      if (y < TREE_VIEW_HEADER_HEIGHT (tree_view))
	return FALSE;

      y_offset = _gtk_rbtree_find_offset (tree_view->priv->tree,
                                          TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, y + tree_view->priv->vadjustment->value),
                                          &tree, &node);
    }
  
  if (tree == NULL)
    return FALSE;

  if (cell_y)
    *cell_y = y_offset;
  
  if (path)
    *path = _gtk_tree_view_find_path (tree_view, tree, node);

  return TRUE;
}


static void
gtk_tree_view_get_background_xrange (GtkTreeView       *tree_view,
                                     GtkRBTree         *tree,
                                     GtkTreeViewColumn *column,
                                     gint              *x1,
                                     gint              *x2)
{
  GtkTreeViewColumn *tmp_column = NULL;
  gint total_width;
  GList *list;
  
  if (x1)
    *x1 = 0;

  if (x2)
    *x2 = 0;

  total_width = 0;
  for (list = tree_view->priv->columns; list; list = list->next)
    {
      tmp_column = list->data;
      
      if (tmp_column == column)
        break;
      
      if (tmp_column->visible)
        total_width += tmp_column->width;
    }
  
  if (tmp_column != column)
    {
      g_warning (G_STRLOC": passed-in column isn't in the tree");
      return;
    }
  
  if (x1)
    *x1 = total_width;
  
  if (x2)
    {
      if (column->visible)
        *x2 = total_width + column->width;
      else
        *x2 = total_width; /* width of 0 */
    }
}

static void
gtk_tree_view_get_cell_xrange (GtkTreeView       *tree_view,
                               GtkRBTree         *tree,
                               GtkTreeViewColumn *column,
                               gint              *x1,
                               gint              *x2)
{
  GtkTreeViewColumn *tmp_column = NULL;
  gint total_width;
  GList *list;
  gint i;
  
  if (x1)
    *x1 = 0;

  if (x2)
    *x2 = 0;

  i = 0;
  total_width = 0;
  for (list = tree_view->priv->columns; list; list = list->next)
    {
      tmp_column = list->data;
      
      if (tmp_column == column)
        break;
      
      if (tmp_column->visible)
        total_width += tmp_column->width;

      ++i;
    }
  
  if (tmp_column != column)
    {
      g_warning (G_STRLOC": passed-in column isn't in the tree");
      return;
    }

  /* Remember we're getting the cell range, i.e. the cell_area passed
   * to the cell renderer.
   */

  if (i == tree_view->priv->expander_column)
    total_width += tree_view->priv->tab_offset * _gtk_rbtree_get_depth (tree);
  
  if (x1)
    *x1 = total_width;
  
  if (x2)
    {
      if (column->visible)
        *x2 = total_width + column->width;
      else
        *x2 = total_width; /* width of 0 */
    }
}

static void
gtk_tree_view_get_arrow_xrange (GtkTreeView *tree_view,
                                gint        *x1,
                                gint        *x2)
{
  gint x_offset = 0;
  GList *list;
  GtkTreeViewColumn *tmp_column = NULL;
  gint total_width;
  gint i;
  
  i = 0;
  total_width = 0;
  for (list = tree_view->priv->columns; list; list = list->next)
    {
      tmp_column = list->data;

      if (i == tree_view->priv->expander_column)
        {
          x_offset = total_width;
          break;
        }
          
      if (tmp_column->visible)
        total_width += tmp_column->width;

      ++i;
    }

  if (x1)
    *x1 = x_offset;
  
  if (tmp_column && tmp_column->visible)
    {
      /* +1 because x2 isn't included in the range. */
      if (x2)
        *x2 = x_offset + tree_view->priv->tab_offset + 1;
    }
  else
    {
      /* return an empty range, the expander column is hidden */
      if (x2)
        *x2 = x_offset;
    }
}


/**
 * gtk_tree_view_get_cell_area:
 * @tree_view: a #GtkTreeView
 * @path: a #GtkTreePath for the row, or %NULL to get only horizontal coordinates
 * @column: a #GtkTreeViewColumn for the column, or %NULL to get only vertical coordiantes
 * @rect: rectangle to fill with cell rect
 * 
 * Fills the bounding rectangle in tree window coordinates for the
 * cell at the row specified by @path and the column specified by
 * @column.  If @path is %NULL, the y and height fields of the
 * rectangle will be filled with 0. If @column is %NULL, the x and
 * width fields will be filled with 0.  The sum of all cell rects does
 * not cover the entire tree; there are extra pixels in between rows,
 * for example. The returned rectangle is equivalent to the @cell_area
 * passed to gtk_cell_renderer_render().
 * 
 **/
void
gtk_tree_view_get_cell_area (GtkTreeView        *tree_view,
                             GtkTreePath        *path,
                             GtkTreeViewColumn  *column,
                             GdkRectangle       *rect)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;
  
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (column == NULL || GTK_IS_TREE_VIEW_COLUMN (column));
  g_return_if_fail (rect != NULL);

  rect->x = 0;
  rect->y = 0;
  rect->width = 0;
  rect->height = 0;
  
  if (path)
    {
      /* Get vertical coords */
      
      _gtk_tree_view_find_node (tree_view, path, &tree, &node);
      
      if (tree == NULL)
        {
          g_warning (G_STRLOC": no row corresponding to path");
          return;
        }

      /* Remember that the rbtree stores node height including the vertical
       * separator, see comment at top of file.
       */
      rect->y = CELL_FIRST_PIXEL (tree_view, tree, node);

      rect->height = CELL_HEIGHT (node);
    }

  if (column)
    {
      gint x2 = 0;
      
      gtk_tree_view_get_cell_xrange (tree_view, tree, column, &rect->x, &x2);
      rect->width = x2 - rect->x;
    }
}


/**
 * gtk_tree_view_get_background_area:
 * @tree_view: a #GtkTreeView
 * @path: a #GtkTreePath for the row, or %NULL to get only horizontal coordinates
 * @column: a #GtkTreeViewColumn for the column, or %NULL to get only vertical coordiantes
 * @rect: rectangle to fill with cell background rect
 * 
 * Fills the bounding rectangle in tree window coordinates for the
 * cell at the row specified by @path and the column specified by
 * @column.  If @path is %NULL, the y and height fields of the
 * rectangle will be filled with 0. If @column is %NULL, the x and
 * width fields will be filled with 0.  The returned rectangle is
 * equivalent to the @background_area passed to
 * gtk_cell_renderer_render().  These background areas tile to cover
 * the entire tree window (except for the area used for header
 * buttons). Contrast with the cell_area, returned by
 * gtk_tree_view_get_cell_area(), which returns only the cell itself,
 * excluding surrounding borders and the tree expander area.
 * 
 **/
void
gtk_tree_view_get_background_area (GtkTreeView        *tree_view,
                                   GtkTreePath        *path,
                                   GtkTreeViewColumn  *column,
                                   GdkRectangle       *rect)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;
  
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (column == NULL || GTK_IS_TREE_VIEW_COLUMN (column));
  g_return_if_fail (rect != NULL);

  rect->x = 0;
  rect->y = 0;
  rect->width = 0;
  rect->height = 0;
  
  if (path)
    {
      /* Get vertical coords */
      
      _gtk_tree_view_find_node (tree_view, path, &tree, &node);
      
      if (tree == NULL)
        {
          g_warning (G_STRLOC": no row corresponding to path");
          return;
        }

      rect->y = BACKGROUND_FIRST_PIXEL (tree_view, tree, node);

      rect->height = BACKGROUND_HEIGHT (node);
    }

  if (column)
    {
      gint x2 = 0;
      
      gtk_tree_view_get_background_xrange (tree_view, tree, column, &rect->x, &x2);
      rect->width = x2 - rect->x;
    }
}

static void
gtk_tree_view_expand_all_helper (GtkRBTree  *tree,
				 GtkRBNode  *node,
				 gpointer  data)
{
  GtkTreeView *tree_view = data;

  if (node->children)
    _gtk_rbtree_traverse (node->children,
			  node->children->root,
			  G_PRE_ORDER,
			  gtk_tree_view_expand_all_helper,
			  data);
  else if ((node->flags & GTK_RBNODE_IS_PARENT) == GTK_RBNODE_IS_PARENT && node->children == NULL)
    {
      GtkTreePath *path;
      GtkTreeIter iter;
      GtkTreeIter child;

      node->children = _gtk_rbtree_new ();
      node->children->parent_tree = tree;
      node->children->parent_node = node;
      path = _gtk_tree_view_find_path (tree_view, tree, node);
      gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
      gtk_tree_model_iter_children (tree_view->priv->model, &child, &iter);
      gtk_tree_view_build_tree (tree_view,
				node->children,
				&child,
				gtk_tree_path_get_depth (path) + 1,
				TRUE,
				GTK_WIDGET_REALIZED (tree_view));
      gtk_tree_path_free (path);
    }
}

/**
 * gtk_tree_view_expand_all:
 * @tree_view: A #GtkTreeView.
 * 
 * Recursively expands all nodes in the @tree_view.
 **/
void
gtk_tree_view_expand_all (GtkTreeView *tree_view)
{
  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (tree_view->priv->tree != NULL);

  _gtk_rbtree_traverse (tree_view->priv->tree,
			tree_view->priv->tree->root,
			G_PRE_ORDER,
			gtk_tree_view_expand_all_helper,
			tree_view);

  _gtk_tree_view_set_size (tree_view, -1,-1);
}

static void
gtk_tree_view_collapse_all_helper (GtkRBTree  *tree,
				   GtkRBNode  *node,
				   gpointer    data)
{
  if (node->children)
    {
      GtkTreePath *path;
      GtkTreeIter iter;

      path = _gtk_tree_view_find_path (GTK_TREE_VIEW (data),
				       node->children,
				       node->children->root);
      gtk_tree_model_get_iter (GTK_TREE_VIEW (data)->priv->model,
			       &iter,
			       path);
      gtk_tree_view_discover_dirty (GTK_TREE_VIEW (data),
				    node->children,
				    &iter,
				    gtk_tree_path_get_depth (path));

      /* Ensure we don't have a dangling pointer to a dead node */
      ensure_unprelighted (GTK_TREE_VIEW (data));
      
      _gtk_rbtree_remove (node->children);
      gtk_tree_path_free (path);
    }
}

/**
 * gtk_tree_view_collapse_all:
 * @tree_view: A #GtkTreeView.
 * 
 * Recursively collapses all visible, expanded nodes in @tree_view.
 **/
void
gtk_tree_view_collapse_all (GtkTreeView *tree_view)
{
  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (tree_view->priv->tree != NULL);

  _gtk_rbtree_traverse (tree_view->priv->tree,
			tree_view->priv->tree->root,
			G_PRE_ORDER,
			gtk_tree_view_collapse_all_helper,
			tree_view);

  if (GTK_WIDGET_MAPPED (tree_view))
    gtk_widget_queue_draw (GTK_WIDGET (tree_view));
}

/* FIXME the bool return values for expand_row and collapse_row are
 * not analagous; they should be TRUE if the row had children and
 * was not already in the requested state.
 */

/**
 * gtk_tree_view_expand_row:
 * @tree_view: a #GtkTreeView
 * @path: path to a row
 * @open_all: whether to recursively expand, or just expand immediate children
 * 
 * Opens the row so its children are visible
 * 
 * Return value: %TRUE if the row existed and had children
 **/
gboolean
gtk_tree_view_expand_row (GtkTreeView *tree_view,
			  GtkTreePath *path,
			  gboolean     open_all)
{
  GtkTreeIter iter;
  GtkTreeIter child;
  GtkRBTree *tree;
  GtkRBNode *node;

  g_return_val_if_fail (tree_view != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (tree_view->priv->model != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  if (_gtk_tree_view_find_node (tree_view,
				path,
				&tree,
				&node))
    return FALSE;
  
  if (node->children)
    return TRUE;
  
  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
  if (! gtk_tree_model_iter_has_child (tree_view->priv->model, &iter))
    return FALSE;
  
  node->children = _gtk_rbtree_new ();
  node->children->parent_tree = tree;
  node->children->parent_node = node;

  gtk_tree_model_iter_children (tree_view->priv->model, &child, &iter);
  gtk_tree_view_build_tree (tree_view,
			    node->children,
			    &child,
			    gtk_tree_path_get_depth (path) + 1,
			    open_all,
			    GTK_WIDGET_REALIZED (tree_view));

  if (GTK_WIDGET_MAPPED (tree_view))
    gtk_widget_queue_draw (GTK_WIDGET (tree_view));

  return TRUE;
}

/**
 * gtk_tree_view_collapse_row:
 * @tree_view: a #GtkTreeView
 * @path: path to a row in the @tree_view
 * 
 * Collapses a row (hides its child rows).
 * 
 * Return value: %TRUE if the row was expanded
 **/
gboolean
gtk_tree_view_collapse_row (GtkTreeView *tree_view,
			    GtkTreePath *path)
{
  GtkRBTree *tree;
  GtkRBNode *node;
  GtkTreeIter iter;

  g_return_val_if_fail (tree_view != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (tree_view->priv->tree != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  if (_gtk_tree_view_find_node (tree_view,
				path,
				&tree,
				&node))
    return FALSE;

  if (node->children == NULL)
    return FALSE;

  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
  gtk_tree_view_discover_dirty (tree_view,
				node->children,
				&iter,
				gtk_tree_path_get_depth (path));

  /* Ensure we don't have a dangling pointer to a dead node */
  ensure_unprelighted (tree_view);
  
  g_assert (tree_view->priv->prelight_node == NULL);
  
  _gtk_rbtree_remove (node->children);

  if (GTK_WIDGET_MAPPED (tree_view))
    gtk_widget_queue_draw (GTK_WIDGET (tree_view));

  return TRUE;
}

/**
 * gtk_tree_view_get_visible_rect:
 * @tree_view: a #GtkTreeView
 * @visible_rect: rectangle to fill
 *
 * Fills @visible_rect with the currently-visible region of the
 * buffer, in tree coordinates. Convert to widget coordinates with
 * gtk_tree_view_tree_to_widget_coords(). Tree coordinates start at
 * 0,0 for row 0 of the tree, and cover the entire scrollable area of
 * the tree.
 **/
void
gtk_tree_view_get_visible_rect (GtkTreeView  *tree_view,
                                GdkRectangle *visible_rect)
{
  GtkWidget *widget;

  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  widget = GTK_WIDGET (tree_view);

  if (visible_rect)
    {
      visible_rect->x = tree_view->priv->hadjustment->value;
      visible_rect->y = tree_view->priv->vadjustment->value;
      visible_rect->width = widget->allocation.width;
      visible_rect->height = widget->allocation.height - TREE_VIEW_HEADER_HEIGHT (tree_view);
    }
}

/**
 * gtk_tree_view_widget_to_tree_coords:
 * @tree_view: a #GtkTreeView
 * @wx: widget X coordinate
 * @wy: widget Y coordinate
 * @tx: return location for tree X coordinate
 * @ty: return location for tree Y coordinate
 *
 * Converts widget coordinates to coordinates for the
 * tree window (the full scrollable area of the tree).
 * 
 **/
void
gtk_tree_view_widget_to_tree_coords (GtkTreeView *tree_view,
                                     gint         wx,
                                     gint         wy,
                                     gint        *tx,
                                     gint        *ty)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  
  if (tx)
    {
      *tx = wx + tree_view->priv->hadjustment->value;
    }

  if (ty)
    {
      *ty = wy + tree_view->priv->vadjustment->value;
    }
}

/**
 * gtk_tree_view_tree_to_widget_coords:
 * @tree_view: a #GtkTreeView
 * @tx: tree X coordinate
 * @ty: tree Y coordinate
 * @wx: return location for widget X coordinate
 * @wy: return location for widget Y coordinate
 *
 * Converts tree coordinates (coordinates in full scrollable
 * area of the tree) to widget coordinates.
 * 
 **/
void
gtk_tree_view_tree_to_widget_coords (GtkTreeView *tree_view,
                                     gint         tx,
                                     gint         ty,
                                     gint        *wx,
                                     gint        *wy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (wx)
    {
      *wx = tx - tree_view->priv->hadjustment->value;
    }

  if (wy)
    {
      *wy = ty - tree_view->priv->vadjustment->value;
    }
}

/**
 * gtk_tree_view_set_rules_hint
 * @tree_view: a #GtkTreeView
 * @setting: %TRUE if the tree requires reading across rows
 * 
 * This function tells GTK+ that the user interface for your
 * application requires users to read across tree rows and associate
 * cells with one another. By default, GTK+ will then render the tree
 * with alternating row colors. <emphasis>DO NOT</emphasis> use it
 * just because you prefer the appearance of the ruled tree; that's a
 * question for the theme. Some themes will draw tree rows in
 * alternating colors even when rules are turned off, and users who
 * prefer that appearance all the time can choose those themes. You
 * should call this function only as a <emphasis>semantic</emphasis>
 * hint to the theme engine that your tree makes alternating colors
 * useful from a functional standpoint (since it has lots of columns,
 * generally).
 * 
 **/
void
gtk_tree_view_set_rules_hint (GtkTreeView  *tree_view,
                              gboolean      setting)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  
  setting = setting != FALSE;

  if (tree_view->priv->has_rules != setting)
    {
      tree_view->priv->has_rules = setting;
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
    }
}

/**
 * gtk_tree_view_get_rules_hint
 * @tree_view: a #GtkTreeView
 * 
 * Gets the setting set by gtk_tree_view_set_rules_hint().
 * 
 * Return value: %TRUE if rules are useful for the user of this tree
 **/
gboolean
gtk_tree_view_get_rules_hint (GtkTreeView  *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  
  return tree_view->priv->has_rules;
}

/* Drag-and-drop */

static void
set_source_row (GdkDragContext *context,
                GtkTreeModel   *model,
                GtkTreePath    *source_row)
{
  g_object_set_data_full (G_OBJECT (context),
                          "gtk-tree-view-source-row",
                          source_row ? gtk_tree_row_reference_new (model, source_row) : NULL,
                          (GDestroyNotify) (source_row ? gtk_tree_row_reference_free : NULL));
}

static GtkTreePath*
get_source_row (GdkDragContext *context)
{
  GtkTreeRowReference *ref =
    g_object_get_data (G_OBJECT (context), "gtk-tree-view-source-row");

  if (ref)
    return gtk_tree_row_reference_get_path (ref);
  else
    return NULL;
}


static void
set_dest_row (GdkDragContext *context,
              GtkTreeModel   *model,
              GtkTreePath    *dest_row)
{
  g_object_set_data_full (G_OBJECT (context),
                          "gtk-tree-view-dest-row",
                          dest_row ? gtk_tree_row_reference_new (model, dest_row) : NULL,
                          (GDestroyNotify) (dest_row ? gtk_tree_row_reference_free : NULL));
}

static GtkTreePath*
get_dest_row (GdkDragContext *context)
{
  GtkTreeRowReference *ref =
    g_object_get_data (G_OBJECT (context), "gtk-tree-view-dest-row");

  if (ref)
    return gtk_tree_row_reference_get_path (ref);
  else
    return NULL;
}

/* Get/set whether drag_motion requested the drag data and
 * drag_data_received should thus not actually insert the data,
 * since the data doesn't result from a drop.
 */
static void
set_status_pending (GdkDragContext *context,
                    GdkDragAction   suggested_action)
{
  g_object_set_data (G_OBJECT (context),
                     "gtk-tree-view-status-pending",
                     GINT_TO_POINTER (suggested_action));
}

static GdkDragAction
get_status_pending (GdkDragContext *context)
{
  return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (context),
                                             "gtk-tree-view-status-pending"));
}

typedef struct _TreeViewDragInfo TreeViewDragInfo;

struct _TreeViewDragInfo
{
  GdkModifierType start_button_mask;
  GtkTargetList *source_target_list;
  GdkDragAction source_actions;
  GClosure *row_draggable_closure;

  GtkTargetList *dest_target_list;
  GClosure *location_droppable_closure;
  
  guint source_set : 1;
  guint dest_set : 1;
};

static TreeViewDragInfo*
get_info (GtkTreeView *tree_view)
{
  return g_object_get_data (G_OBJECT (tree_view), "gtk-tree-view-drag-info");
}

static void
clear_source_info (TreeViewDragInfo *di)
{
  if (di->source_target_list)
    gtk_target_list_unref (di->source_target_list);

  if (di->row_draggable_closure)
    g_closure_unref (di->row_draggable_closure);
  
  di->source_target_list = NULL;
  di->row_draggable_closure = NULL;
}

static void
clear_dest_info (TreeViewDragInfo *di)
{
  if (di->location_droppable_closure)
    g_closure_unref (di->location_droppable_closure);

  if (di->dest_target_list)
    gtk_target_list_unref (di->dest_target_list);
  
  di->location_droppable_closure = NULL;
  di->dest_target_list = NULL;
}

static void
destroy_info (TreeViewDragInfo *di)
{
  clear_source_info (di);
  clear_dest_info (di);
  g_free (di);
}

static TreeViewDragInfo*
ensure_info (GtkTreeView *tree_view)
{
  TreeViewDragInfo *di;

  di = get_info (tree_view);

  if (di == NULL)
    {
      di = g_new0 (TreeViewDragInfo, 1);
      
      g_object_set_data_full (G_OBJECT (tree_view),
                              "gtk-tree-view-drag-info",
                              di,
                              (GDestroyNotify) destroy_info);
    }

  return di;
}

static void
remove_info (GtkTreeView *tree_view)
{
  g_object_set_data (G_OBJECT (tree_view), "gtk-tree-view-drag-info", NULL);
}

#define SCROLL_EDGE_SIZE 15

static gint
drag_scan_timeout (gpointer data)
{
  GtkTreeView *tree_view;
  gint x, y;
  GdkModifierType state;
  GtkTreePath *path = NULL;
  GtkTreeViewColumn *column = NULL;
  GdkRectangle visible_rect;
  
  tree_view = GTK_TREE_VIEW (data);

  gdk_window_get_pointer (tree_view->priv->bin_window,
                          &x, &y, &state);  

  gtk_tree_view_get_visible_rect (tree_view, &visible_rect);

  /* See if we are near the edge. */
  if ((x - visible_rect.x) < SCROLL_EDGE_SIZE ||
      (visible_rect.x + visible_rect.width - x) < SCROLL_EDGE_SIZE ||
      (y - visible_rect.y) < SCROLL_EDGE_SIZE ||
      (visible_rect.y + visible_rect.height - y) < SCROLL_EDGE_SIZE)
    {
      gtk_tree_view_get_path_at_pos (tree_view,
                                     tree_view->priv->bin_window,
                                     x, y,
                                     &path,
                                     &column,
                                     NULL,
                                     NULL);
      
      if (path != NULL)
        {
          gtk_tree_view_scroll_to_cell (tree_view,
                                        path,
                                        column,
                                        0.5, 0.5); 
          
          gtk_tree_path_free (path);
        }
    }
  
  return TRUE;
}


static void
ensure_scroll_timeout (GtkTreeView *tree_view)
{
  if (tree_view->priv->scroll_timeout == 0)
    tree_view->priv->scroll_timeout = gtk_timeout_add (50, drag_scan_timeout, tree_view);
}

static void
remove_scroll_timeout (GtkTreeView *tree_view)
{
  if (tree_view->priv->scroll_timeout != 0)
    {
      gtk_timeout_remove (tree_view->priv->scroll_timeout);
      tree_view->priv->scroll_timeout = 0;
    }
}

void
gtk_tree_view_set_rows_drag_source (GtkTreeView              *tree_view,
                                    GdkModifierType           start_button_mask,
                                    const GtkTargetEntry     *targets,
                                    gint                      n_targets,
                                    GdkDragAction             actions,
                                    GtkTreeViewDraggableFunc  row_draggable_func,
                                    gpointer                  user_data)
{
  TreeViewDragInfo *di;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  
  di = ensure_info (tree_view);
  clear_source_info (di);

  di->start_button_mask = start_button_mask;
  di->source_target_list = gtk_target_list_new (targets, n_targets);
  di->source_actions = actions;

  if (row_draggable_func)
    {
      di->row_draggable_closure = g_cclosure_new ((GCallback) row_draggable_func,
                                                  user_data, NULL);
      g_closure_ref (di->row_draggable_closure);
      g_closure_sink (di->row_draggable_closure);
    }

  di->source_set = TRUE;
}

void
gtk_tree_view_set_rows_drag_dest (GtkTreeView              *tree_view,
                                  const GtkTargetEntry     *targets,
                                  gint                      n_targets,
                                  GdkDragAction             actions,
                                  GtkTreeViewDroppableFunc  location_droppable_func,
                                  gpointer                  user_data)
{
  TreeViewDragInfo *di;
  
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_drag_dest_set (GTK_WIDGET (tree_view),
                     0,
                     NULL,
                     0,
                     actions);
  
  di = ensure_info (tree_view);
  clear_dest_info (di);  

  if (targets)
    di->dest_target_list = gtk_target_list_new (targets, n_targets);

  if (location_droppable_func)
    {
      di->location_droppable_closure = g_cclosure_new ((GCallback) location_droppable_func,
                                                       user_data, NULL);
      g_closure_ref (di->location_droppable_closure);
      g_closure_sink (di->location_droppable_closure);
    }
  
  di->dest_set = TRUE;
}

void
gtk_tree_view_unset_rows_drag_source (GtkTreeView *tree_view)
{
  TreeViewDragInfo *di;
  
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  
  di = get_info (tree_view);

  if (di)
    {
      if (di->source_set)
        {
          clear_source_info (di);
          di->source_set = FALSE;
        }

      if (!di->dest_set && !di->source_set)
        remove_info (tree_view);
    }
}

void
gtk_tree_view_unset_rows_drag_dest (GtkTreeView *tree_view)
{
  TreeViewDragInfo *di;
  
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  
  di = get_info (tree_view);

  if (di)
    {
      if (di->dest_set)
        {
          gtk_drag_dest_unset (GTK_WIDGET (tree_view));
          clear_dest_info (di);
          di->dest_set = FALSE;
        }

      if (!di->dest_set && !di->source_set)
        remove_info (tree_view);
    }
}

void
gtk_tree_view_set_drag_dest_row (GtkTreeView            *tree_view,
                                 GtkTreePath            *path,
                                 GtkTreeViewDropPosition pos)
{
  GtkTreePath *current_dest;
  /* Note; this function is exported to allow a custom DND
   * implementation, so it can't touch TreeViewDragInfo
   */
  
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  current_dest = NULL;
  
  if (tree_view->priv->drag_dest_row)
    current_dest = gtk_tree_row_reference_get_path (tree_view->priv->drag_dest_row);

  if (current_dest)
    {
      gtk_tree_view_queue_draw_path (tree_view, current_dest, NULL);
      gtk_tree_path_free (current_dest);
    }

  if (tree_view->priv->drag_dest_row)
    gtk_tree_row_reference_free (tree_view->priv->drag_dest_row);

  tree_view->priv->drag_dest_pos = pos;
  
  if (path)
    {
      tree_view->priv->drag_dest_row =
        gtk_tree_row_reference_new (tree_view->priv->model,
                                    path);
      gtk_tree_view_queue_draw_path (tree_view, path, NULL);
    }
  else
    tree_view->priv->drag_dest_row = NULL;
}

void
gtk_tree_view_get_drag_dest_row (GtkTreeView              *tree_view,
                                 GtkTreePath             **path,
                                 GtkTreeViewDropPosition  *pos)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  
  if (path)
    {
      if (tree_view->priv->drag_dest_row)
        *path = gtk_tree_row_reference_get_path (tree_view->priv->drag_dest_row);
      else
        *path = NULL;
    }

  if (pos)
    *pos = tree_view->priv->drag_dest_pos;
}

gboolean
gtk_tree_view_get_dest_row_at_pos (GtkTreeView             *tree_view,
                                   gint                     drag_x,
                                   gint                     drag_y,
                                   GtkTreePath            **path,
                                   GtkTreeViewDropPosition *pos)
{
  gint cell_y;
  gdouble offset_into_row;
  gdouble quarter;
  gint x, y;
  GdkRectangle cell;
  GtkTreeViewColumn *column = NULL;
  GtkTreePath *tmp_path = NULL;
  
  /* Note; this function is exported to allow a custom DND
   * implementation, so it can't touch TreeViewDragInfo
   */
  
  g_return_val_if_fail (tree_view != NULL, FALSE);
  g_return_val_if_fail (tree_view->priv->tree != NULL, FALSE);
  g_return_val_if_fail (drag_x >= 0, FALSE);
  g_return_val_if_fail (drag_y >= 0, FALSE);
  g_return_val_if_fail (tree_view->priv->bin_window != NULL, FALSE);

  if (path)
    *path = NULL;

  /* remember that drag_x and drag_y are in widget coords, convert to tree window */

  gtk_tree_view_widget_to_tree_coords (tree_view, drag_x, drag_y,
                                       &x, &y);
  
  /* If in the top quarter of a row, we drop before that row; if
   * in the bottom quarter, drop after that row; if in the middle,
   * and the row has children, drop into the row.
   */

  if (!gtk_tree_view_get_path_at_pos (tree_view,
                                      tree_view->priv->bin_window,
                                      x, y,
                                      &tmp_path,
                                      &column,
                                      NULL,
                                      &cell_y))
    return FALSE;  

  gtk_tree_view_get_background_area (tree_view, tmp_path, column,
                                     &cell);

  offset_into_row = cell_y;
  
  if (path)
    *path = tmp_path;
  else
    gtk_tree_path_free (tmp_path);

  tmp_path = NULL;
  
  quarter = cell.height / 4.0;
  
  if (pos)
    {
      if (offset_into_row < quarter)
        {
          *pos = GTK_TREE_VIEW_DROP_BEFORE;          
        }
      else if (offset_into_row < quarter * 2)
        {
          *pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
        }
      else if (offset_into_row < quarter * 3)
        {
          *pos = GTK_TREE_VIEW_DROP_INTO_OR_AFTER;
        }
      else
        {
          *pos = GTK_TREE_VIEW_DROP_AFTER;
        }
    }

  return TRUE;
}

static gboolean
gtk_tree_view_maybe_begin_dragging_row (GtkTreeView      *tree_view,
                                        GdkEventMotion   *event)
{
  GdkDragContext *context;
  TreeViewDragInfo *di;
  GtkTreePath *path = NULL;
  gint button;
  gint cell_x, cell_y;
  GtkTreeModel *model;
  
  di = get_info (tree_view);

  if (di == NULL)
    return FALSE;

  if (tree_view->priv->pressed_button < 0)
    return FALSE;
  
  if (!gtk_drag_check_threshold (GTK_WIDGET (tree_view),
                                 tree_view->priv->press_start_x,
                                 tree_view->priv->press_start_y,
                                 event->x, event->y))
    return FALSE;

  model = gtk_tree_view_get_model (tree_view);

  if (model == NULL)
    return FALSE;
  
  button = tree_view->priv->pressed_button;
  tree_view->priv->pressed_button = -1;

  gtk_tree_view_get_path_at_pos (tree_view,
                                 tree_view->priv->bin_window,
                                 tree_view->priv->press_start_x,
                                 tree_view->priv->press_start_y,
                                 &path,
                                 NULL,
                                 &cell_x,
                                 &cell_y);

  if (path == NULL)
    return FALSE;
  
  /* FIXME if the path doesn't match the row_draggable predicate,
   * return FALSE and free path
   */  

  /* FIXME Check whether we're a start button, if not return FALSE and
   * free path
   */
  
  context = gtk_drag_begin (GTK_WIDGET (tree_view),
                            di->source_target_list,
                            di->source_actions,
                            button,
                            (GdkEvent*)event);

  gtk_drag_set_icon_default (context);

  {
    GdkPixmap *row_pix;

    row_pix = gtk_tree_view_create_row_drag_icon (tree_view,
                                                  path);

    gtk_drag_set_icon_pixmap (context,
                              gdk_drawable_get_colormap (row_pix),
                              row_pix,
                              NULL,
                              /* the + 1 is for the black border in the icon */
                              tree_view->priv->press_start_x + 1,
                              cell_y + 1);

    gdk_pixmap_unref (row_pix);
  }
  
  set_source_row (context, model, path);
  gtk_tree_path_free (path);
  
  return TRUE;
}

/* Default signal implementations for the drag signals */

static void
gtk_tree_view_drag_begin (GtkWidget      *widget,
                          GdkDragContext *context)
{
  /* do nothing */
}

static void
gtk_tree_view_drag_end (GtkWidget      *widget,
                        GdkDragContext *context)
{
  /* do nothing */
}

static void
gtk_tree_view_drag_data_get (GtkWidget        *widget,
                             GdkDragContext   *context,
                             GtkSelectionData *selection_data,
                             guint             info,
                             guint             time)
{
  GtkTreeView *tree_view;
  GtkTreeModel *model;
  TreeViewDragInfo *di;
  GtkTreePath *source_row;
  
  tree_view = GTK_TREE_VIEW (widget);

  model = gtk_tree_view_get_model (tree_view);

  if (model == NULL)
    return;

  di = get_info (GTK_TREE_VIEW (widget));
  
  if (di == NULL)    
    return;

  source_row = get_source_row (context);

  if (source_row == NULL)
    return;
  
  /* We can implement the GTK_TREE_MODEL_ROW target generically for
   * any model; for DragSource models there are some other targets
   * we also support.
   */

  if (GTK_IS_TREE_DRAG_SOURCE (model) &&
      gtk_tree_drag_source_drag_data_get (GTK_TREE_DRAG_SOURCE (model),
                                          source_row,
                                          selection_data))
    goto done;

  /* If drag_data_get does nothing, try providing row data. */
  if (selection_data->target == gdk_atom_intern ("GTK_TREE_MODEL_ROW", FALSE))
    {
      gtk_selection_data_set_tree_row (selection_data,
                                       model,
                                       source_row);
    }

 done:
  gtk_tree_path_free (source_row);
}


static gboolean
check_model_dnd (GtkTreeModel *model,
                 GType         required_iface,
                 const gchar  *signal)
{
  if (model == NULL || !G_TYPE_CHECK_INSTANCE_TYPE ((model), required_iface))
    {
      g_warning ("You must override the default '%s' handler "
                 "on GtkTreeView when using models that don't support "
                 "the %s interface and enabling drag-and-drop. The simplest way to do this "
                 "is to connect to '%s' and call "
                 "gtk_signal_emit_stop_by_name() in your signal handler to prevent "
                 "the default handler from running. Look at the source code "
                 "for the default handler in gtktreeview.c to get an idea what "
                 "your handler should do. (gtktreeview.c is in the GTK source "
                 "code.) If you're using GTK from a language other than C, "
                 "there may be a more natural way to override default handlers, e.g. via derivation.",
                 signal, g_type_name (required_iface), signal);
      return FALSE;
    }
  else
    return TRUE;
}

static void
gtk_tree_view_drag_data_delete (GtkWidget      *widget,
                                GdkDragContext *context)
{
  TreeViewDragInfo *di;
  GtkTreeModel *model;
  GtkTreeView *tree_view;
  GtkTreePath *source_row;
  
  tree_view = GTK_TREE_VIEW (widget);
  model = gtk_tree_view_get_model (tree_view);
  
  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_SOURCE, "drag_data_delete"))
    return;
  
  di = get_info (tree_view);

  if (di == NULL)
    return;

  source_row = get_source_row (context);
  
  if (source_row == NULL)
    return;
  
  gtk_tree_drag_source_drag_data_delete (GTK_TREE_DRAG_SOURCE (model),
                                         source_row);

  gtk_tree_path_free (source_row);
  
  set_source_row (context, NULL, NULL);
}

static void
remove_open_timeout (GtkTreeView *tree_view)
{
  if (tree_view->priv->open_dest_timeout != 0)
    {
      gtk_timeout_remove (tree_view->priv->open_dest_timeout);
      tree_view->priv->open_dest_timeout = 0;
    }
}

static void
gtk_tree_view_drag_leave (GtkWidget      *widget,
                          GdkDragContext *context,
                          guint             time)
{
  TreeViewDragInfo *di;
  
  di = get_info (GTK_TREE_VIEW (widget));
  
  /* unset any highlight row */
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                   NULL,
                                   GTK_TREE_VIEW_DROP_BEFORE);
  
  remove_scroll_timeout (GTK_TREE_VIEW (widget));
  remove_open_timeout (GTK_TREE_VIEW (widget));
}

static gint
open_row_timeout (gpointer data)
{
  GtkTreeView *tree_view = data;
  GtkTreePath *dest_path = NULL;
  GtkTreeViewDropPosition pos;

  gtk_tree_view_get_drag_dest_row (tree_view,
                                   &dest_path,
                                   &pos);
  
  if (dest_path && 
      (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER ||
       pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE))
    {
      gtk_tree_view_expand_row (tree_view,
                                dest_path,
                                FALSE);
      tree_view->priv->open_dest_timeout = 0;

      gtk_tree_path_free (dest_path);
      
      return FALSE;
    }  
  else
    {
      if (dest_path)
        gtk_tree_path_free (dest_path);
      return TRUE;
    }
}

/* Returns TRUE if event should not be propagated to parent widgets */
static gboolean
set_destination_row (GtkTreeView    *tree_view,
                     GdkDragContext *context,
                     gint            x,
                     gint            y,
                     GdkDragAction  *suggested_action,
                     GdkAtom        *target)
{
  GtkTreePath *path = NULL;
  GtkTreeViewDropPosition pos;
  GtkTreeViewDropPosition old_pos;
  TreeViewDragInfo *di;
  GtkWidget *widget;
  GtkTreePath *old_dest_path = NULL;
  
  *suggested_action = 0;
  *target = GDK_NONE;
  
  widget = GTK_WIDGET (tree_view);
  
  di = get_info (tree_view);
  
  if (di == NULL)
    {
      /* someone unset us as a drag dest, note that if
       * we return FALSE drag_leave isn't called
       */

      gtk_tree_view_set_drag_dest_row (tree_view,
                                       NULL,
                                       GTK_TREE_VIEW_DROP_BEFORE);

      remove_scroll_timeout (GTK_TREE_VIEW (widget));
      remove_open_timeout (GTK_TREE_VIEW (widget));
      
      return FALSE; /* no longer a drop site */
    }

  *target = gtk_drag_dest_find_target (widget, context, di->dest_target_list);
  if (*target == GDK_NONE)
    {
      return FALSE;
    }
  
  if (!gtk_tree_view_get_dest_row_at_pos (tree_view,
                                          x, y,
                                          &path,
                                          &pos))
    {
      /* can't drop here */
      remove_open_timeout (tree_view);

      gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                       NULL,
                                       GTK_TREE_VIEW_DROP_BEFORE);

      /* don't propagate to parent though */
      return TRUE;
    }
  
  g_assert (path);

  /* If we left the current row's "open" zone, unset the timeout for
   * opening the row
   */
  gtk_tree_view_get_drag_dest_row (tree_view,
                                   &old_dest_path,
                                   &old_pos);
  
  if (old_dest_path &&
      (gtk_tree_path_compare (path, old_dest_path) != 0 ||
       !(pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER ||
         pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)))
    remove_open_timeout (tree_view);

  if (old_dest_path)
    gtk_tree_path_free (old_dest_path);
  
  if (TRUE /* FIXME if the location droppable predicate */)
    {
      GtkWidget *source_widget;
      
      *suggested_action = context->suggested_action;
      
      source_widget = gtk_drag_get_source_widget (context);
      
      if (source_widget == widget)
        {
          /* Default to MOVE, unless the user has
           * pressed ctrl or alt to affect available actions
           */
          if ((context->actions & GDK_ACTION_MOVE) != 0)
            *suggested_action = GDK_ACTION_MOVE;
        }

      gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                       path, pos);
    }
  else
    {
      /* can't drop here */
      remove_open_timeout (tree_view);
      
      gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                       NULL,
                                       GTK_TREE_VIEW_DROP_BEFORE);
    }

  return TRUE;
}

static gboolean
gtk_tree_view_drag_motion (GtkWidget        *widget,
                           GdkDragContext   *context,
                           gint              x,
                           gint              y,
                           guint             time)
{
  GtkTreePath *path = NULL;
  GtkTreeViewDropPosition pos;
  GtkTreeView *tree_view;
  GdkDragAction suggested_action = 0;
  GdkAtom target;
  
  tree_view = GTK_TREE_VIEW (widget);
  
  if (!set_destination_row (tree_view, context, x, y, &suggested_action, &target))
    return FALSE;

  ensure_scroll_timeout (tree_view);
  
  gtk_tree_view_get_drag_dest_row (tree_view, &path, &pos);

  if (path == NULL)
    {
      /* Can't drop here. */
      gdk_drag_status (context, 0, time);
    }
  else
    {
      if (tree_view->priv->open_dest_timeout == 0 &&
          (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER ||
           pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE))
        {
          tree_view->priv->open_dest_timeout =
            gtk_timeout_add (500, open_row_timeout, tree_view);      
        }

      if (target == gdk_atom_intern ("GTK_TREE_MODEL_ROW", FALSE))
        {
          /* Request data so we can use the source row when
           * determining whether to accept the drop
           */
          set_status_pending (context, suggested_action);
          gtk_drag_get_data (widget, context, target, time);
        }
      else
        {
          set_status_pending (context, 0);
          gdk_drag_status (context, suggested_action, time);
        }
    }

  if (path)
    gtk_tree_path_free (path);

  return TRUE;
}

static GtkTreePath*
get_logical_dest_row (GtkTreeView *tree_view)

{
  /* adjust path to point to the row the drop goes in front of */
  GtkTreePath *path = NULL;
  GtkTreeViewDropPosition pos;

  gtk_tree_view_get_drag_dest_row (tree_view, &path, &pos);

  if (path == NULL)
    return NULL;
  
  if (pos == GTK_TREE_VIEW_DROP_BEFORE)
    ; /* do nothing */
  else if (pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE ||
           pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER)
    {
      /* get first child, drop before it */
      gtk_tree_path_append_index (path, 0);
    }
  else
    {
      g_assert (pos == GTK_TREE_VIEW_DROP_AFTER);
      gtk_tree_path_next (path);
    }

  return path;
}

static gboolean
gtk_tree_view_drag_drop (GtkWidget        *widget,
                         GdkDragContext   *context,
                         gint              x,
                         gint              y,
                         guint             time)
{
  GtkTreeView *tree_view;
  GtkTreePath *path;
  GdkDragAction suggested_action = 0;
  GdkAtom target = GDK_NONE;
  TreeViewDragInfo *di;
  GtkTreeModel *model;
  
  tree_view = GTK_TREE_VIEW (widget);

  model = gtk_tree_view_get_model (tree_view);
  
  remove_scroll_timeout (GTK_TREE_VIEW (widget));
  remove_open_timeout (GTK_TREE_VIEW (widget));
  
  di = get_info (tree_view);  
  
  if (di == NULL)
    return FALSE;
  
  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_DEST, "drag_drop"))
    return FALSE;
  
  if (!set_destination_row (tree_view, context, x, y, &suggested_action, &target))
    return FALSE;

  path = get_logical_dest_row (tree_view);

  if (target != GDK_NONE && path != NULL)
    {
      /* in case a motion had requested drag data, change things so we
       * treat drag data receives as a drop.
       */
      set_status_pending (context, 0);
      
      set_dest_row (context, model, path);
    }

  if (path)
    gtk_tree_path_free (path);
  
  /* Unset this thing */
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                   NULL,
                                   GTK_TREE_VIEW_DROP_BEFORE);
  
  if (target != GDK_NONE)
    {
      gtk_drag_get_data (widget, context, target, time);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_tree_view_drag_data_received (GtkWidget        *widget,
                                  GdkDragContext   *context,
                                  gint              x,
                                  gint              y,
                                  GtkSelectionData *selection_data,
                                  guint             info,
                                  guint             time)
{
  GtkTreePath *path;
  TreeViewDragInfo *di;
  gboolean accepted = FALSE;
  GtkTreeModel *model;
  GtkTreeView *tree_view;
  GtkTreePath *dest_row;
  GdkDragAction suggested_action;
  
  tree_view = GTK_TREE_VIEW (widget);

  model = gtk_tree_view_get_model (tree_view);

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_DEST, "drag_data_received"))
    return;  
  
  di = get_info (tree_view);

  if (di == NULL)
    return;

  suggested_action = get_status_pending (context);

  if (suggested_action)
    {
      /* We are getting this data due to a request in drag_motion,
       * rather than due to a request in drag_drop, so we are just
       * supposed to call drag_status, not actually paste in the
       * data.
       */
      path = get_logical_dest_row (tree_view);

      if (path == NULL)
        suggested_action = 0;

      if (suggested_action)
        {
          GtkTreeModel *src_model = NULL;
          GtkTreePath *src_path = NULL;
          
          if (!gtk_selection_data_get_tree_row (selection_data,
                                                &src_model,
                                                &src_path))
            suggested_action = 0;

          if (suggested_action)
            {
              if (!gtk_tree_drag_dest_row_drop_possible (GTK_TREE_DRAG_DEST (model),
                                                         src_model,
                                                         src_path,
                                                         path))
                suggested_action = 0;

              gtk_tree_path_free (src_path);
            }
        }
      
      gdk_drag_status (context, suggested_action, time);

      if (path)
        gtk_tree_path_free (path);

      /* If you can't drop, remove user drop indicator until the next motion */
      if (suggested_action == 0)
        gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                         NULL,
                                         GTK_TREE_VIEW_DROP_BEFORE);
      
      return;
    }
  
  dest_row = get_dest_row (context);

  if (dest_row == NULL)
    return;
  
  if (selection_data->length >= 0)
    {
      if (gtk_tree_drag_dest_drag_data_received (GTK_TREE_DRAG_DEST (model),
                                                 dest_row,
                                                 selection_data))
        accepted = TRUE;
    }
  
  gtk_drag_finish (context, 
                   accepted,
                   (context->action == GDK_ACTION_MOVE),
                   time);

  gtk_tree_path_free (dest_row);

  /* drop dest_row */
  set_dest_row (context, NULL, NULL);
}
