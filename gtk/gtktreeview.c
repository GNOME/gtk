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
#include "gtktreeprivate.h"
#include "gtkcellrenderer.h"
#include "gtksignal.h"
#include "gtkmain.h"
#include "gtkbutton.h"
#include "gtkalignment.h"
#include "gtklabel.h"

#include <gdk/gdkkeysyms.h>


/* the width of the column resize windows */
#define TREE_VIEW_DRAG_WIDTH 6
#define TREE_VIEW_EXPANDER_WIDTH 14
#define TREE_VIEW_EXPANDER_HEIGHT 14
#define TREE_VIEW_VERTICAL_SEPERATOR 2
#define TREE_VIEW_HORIZONTAL_SEPERATOR 0


typedef struct _GtkTreeViewChild GtkTreeViewChild;

struct _GtkTreeViewChild
{
  GtkWidget *widget;
  gint x;
  gint y;
};


static void     gtk_tree_view_init                 (GtkTreeView      *tree_view);
static void     gtk_tree_view_class_init           (GtkTreeViewClass *klass);

/* widget signals */
static void     gtk_tree_view_set_model_realized   (GtkTreeView      *tree_view);
static void     gtk_tree_view_realize              (GtkWidget        *widget);
static void     gtk_tree_view_unrealize            (GtkWidget        *widget);
static void     gtk_tree_view_map                  (GtkWidget        *widget);
static void     gtk_tree_view_size_request         (GtkWidget        *widget,
						    GtkRequisition   *requisition);
static void     gtk_tree_view_size_allocate        (GtkWidget        *widget,
						    GtkAllocation    *allocation);
static void     gtk_tree_view_draw                 (GtkWidget        *widget,
						    GdkRectangle     *area);
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

/* tree_model signals */
static void     gtk_tree_view_set_adjustments      (GtkTreeView      *tree_view,
						    GtkAdjustment    *hadj,
						    GtkAdjustment    *vadj);
static void     gtk_tree_view_node_changed         (GtkTreeModel     *model,
						    GtkTreePath      *path,
						    GtkTreeNode      *tree_node,
						    gpointer          data);
static void     gtk_tree_view_node_inserted        (GtkTreeModel     *model,
						    GtkTreePath      *path,
						    GtkTreeNode      *tree_node,
						    gpointer          data);
static void     gtk_tree_view_node_child_toggled   (GtkTreeModel     *model,
						    GtkTreePath      *path,
						    GtkTreeNode      *tree_node,
						    gpointer          data);
static void     gtk_tree_view_node_deleted         (GtkTreeModel     *model,
						    GtkTreePath      *path,
						    gpointer          data);

/* Internal functions */
static void     gtk_tree_view_draw_arrow           (GtkTreeView      *tree_view,
						    GtkRBNode        *node,
						    gint              offset,
						    gint              x,
						    gint              y);
static gint     gtk_tree_view_new_column_width     (GtkTreeView      *tree_view,
						    gint              i,
						    gint             *x);
static void     gtk_tree_view_adjustment_changed   (GtkAdjustment    *adjustment,
						    GtkTreeView      *tree_view);
static gint     gtk_tree_view_insert_node_height   (GtkTreeView      *tree_view,
						    GtkRBTree        *tree,
						    GtkTreeNode       node,
						    gint              depth);
static void     gtk_tree_view_build_tree           (GtkTreeView      *tree_view,
						    GtkRBTree        *tree,
						    GtkTreeNode       node,
						    gint              depth,
						    gboolean          recurse,
						    gboolean          calc_bounds);
static void     gtk_tree_view_calc_size            (GtkTreeView      *priv,
						    GtkRBTree        *tree,
						    GtkTreeNode       node,
						    gint              depth);
static gboolean gtk_tree_view_discover_dirty_node  (GtkTreeView      *tree_view,
						    GtkTreeNode       node,
						    gint              depth,
						    gint             *height);
static void     gtk_tree_view_discover_dirty       (GtkTreeView      *tree_view,
						    GtkRBTree        *tree,
						    GtkTreeNode       node,
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

      tree_view_type = g_type_register_static (GTK_TYPE_CONTAINER, "GtkTreeView", &tree_view_info);
    }

  return tree_view_type;
}

static void
gtk_tree_view_class_init (GtkTreeViewClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;
  parent_class = g_type_class_peek_parent (class);

  widget_class->realize = gtk_tree_view_realize;
  widget_class->unrealize = gtk_tree_view_unrealize;
  widget_class->map = gtk_tree_view_map;
  widget_class->size_request = gtk_tree_view_size_request;
  widget_class->size_allocate = gtk_tree_view_size_allocate;
  widget_class->draw = gtk_tree_view_draw;
  widget_class->expose_event = gtk_tree_view_expose;
  //  widget_class->key_press_event = gtk_tree_view_key_press;
  widget_class->motion_notify_event = gtk_tree_view_motion;
  widget_class->enter_notify_event = gtk_tree_view_enter_notify;
  widget_class->leave_notify_event = gtk_tree_view_leave_notify;
  widget_class->button_press_event = gtk_tree_view_button_press;
  widget_class->button_release_event = gtk_tree_view_button_release;
  widget_class->draw_focus = gtk_tree_view_draw_focus;
  widget_class->focus_in_event = gtk_tree_view_focus_in;
  widget_class->focus_out_event = gtk_tree_view_focus_out;

  container_class->forall = gtk_tree_view_forall;
  container_class->remove = gtk_tree_view_remove;
  container_class->focus = gtk_tree_view_focus;

  class->set_scroll_adjustments = gtk_tree_view_set_adjustments;

  widget_class->set_scroll_adjustments_signal =
    gtk_signal_new ("set_scroll_adjustments",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTreeViewClass, set_scroll_adjustments),
		    gtk_marshal_NONE__POINTER_POINTER,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER, GTK_TYPE_POINTER);
}

static void
gtk_tree_view_init (GtkTreeView *tree_view)
{
  tree_view->priv = g_new0 (GtkTreeViewPrivate, 1);

  GTK_WIDGET_UNSET_FLAGS (tree_view, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (tree_view, GTK_CAN_FOCUS);

  tree_view->priv->flags = GTK_TREE_VIEW_IS_LIST | GTK_TREE_VIEW_SHOW_EXPANDERS | GTK_TREE_VIEW_DRAW_KEYFOCUS | GTK_TREE_VIEW_HEADERS_VISIBLE;
  tree_view->priv->tab_offset = TREE_VIEW_EXPANDER_WIDTH;
  tree_view->priv->columns = 0;
  tree_view->priv->column = NULL;
  tree_view->priv->button_pressed_node = NULL;
  tree_view->priv->button_pressed_tree = NULL;
  tree_view->priv->prelight_node = NULL;
  tree_view->priv->prelight_offset = 0;
  tree_view->priv->header_height = 1;
  tree_view->priv->x_drag = 0;
  tree_view->priv->drag_pos = -1;
  tree_view->priv->selection = NULL;
  tree_view->priv->anchor = NULL;
  tree_view->priv->cursor = NULL;
  gtk_tree_view_set_adjustments (tree_view, NULL, NULL);
  _gtk_tree_view_set_size (tree_view, 0, 0);
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

  if (!GTK_WIDGET_REALIZED (tree_view) || tree_view->priv->header_window == NULL)
    return;

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

  for (list = tree_view->priv->column; list; list = list->next)
    {
      column = list->data;
      if (column->button)
	{
	  if (column->visible == FALSE)
	    continue;
	  gtk_widget_set_parent_window (column->button,
					tree_view->priv->header_window);
	  gtk_widget_show (column->button);

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

  if (!GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_MODEL_SETUP) &&
      tree_view->priv->model)
    gtk_tree_view_set_model_realized (tree_view);

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
}

static void
gtk_tree_view_unrealize (GtkWidget *widget)
{
  GtkTreeView *tree_view;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (widget));

  tree_view = GTK_TREE_VIEW (widget);

  gdk_window_set_user_data (tree_view->priv->bin_window, NULL);
  gdk_window_destroy (tree_view->priv->bin_window);
  tree_view->priv->bin_window = NULL;

  gdk_window_set_user_data (tree_view->priv->header_window, NULL);
  gdk_window_destroy (tree_view->priv->header_window);
  tree_view->priv->header_window = NULL;

  gdk_gc_destroy (tree_view->priv->xor_gc);
  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_tree_view_map (GtkWidget *widget)
{
  GList *tmp_list;
  GtkTreeView *tree_view;
  GList *list;
  GtkTreeViewColumn *column;

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
  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE))
    {
      for (list = tree_view->priv->column; list; list = list->next)
	{
	  column = list->data;
	  gtk_widget_map (column->button);
	}
      for (list = tree_view->priv->column; list; list = list->next)
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
  gdk_window_show (widget->window);
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

      gtk_widget_size_request (child->widget, &child_requisition);
    }
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

  for (last_column = g_list_last (tree_view->priv->column);
       last_column && !(GTK_TREE_VIEW_COLUMN (last_column->data)->visible);
       last_column = last_column->prev)
    ;

  if (last_column == NULL)
    return;

  for (list = tree_view->priv->column; list != last_column; list = list->next)
    {
      column = list->data;

      if (!column->visible)
	continue;

      allocation.x = width;
      allocation.width = column->size;
      width += column->size;
      gtk_widget_size_allocate (column->button, &allocation);

      if (column->window)
	gdk_window_move (column->window, width - TREE_VIEW_DRAG_WIDTH/2, 0);
    }
  column = list->data;
  allocation.x = width;
  allocation.width = MAX (widget->allocation.width, tree_view->priv->width) - width;
  gtk_widget_size_allocate (column->button, &allocation);
  if (column->window)
    gdk_window_move (column->window,
		     allocation.x +allocation.width - TREE_VIEW_DRAG_WIDTH/2,
		     0);
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

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_MODEL_SETUP))
    gtk_tree_view_size_allocate_buttons (widget);
}

static void
gtk_tree_view_draw (GtkWidget    *widget,
		    GdkRectangle *area)
{
  GList *tmp_list;
  GtkTreeView *tree_view;
  GtkTreeViewColumn *column;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (widget));

  tree_view = GTK_TREE_VIEW (widget);

  /* We don't have any way of telling themes about this properly,
   * so we just assume a background pixmap
   */
  if (!GTK_WIDGET_APP_PAINTABLE (widget))
    {
      gdk_window_clear_area (tree_view->priv->bin_window,
			     area->x, area->y, area->width, area->height);
      gdk_window_clear_area (tree_view->priv->header_window,
			     area->x, area->y, area->width, area->height);
    }

  tmp_list = tree_view->priv->children;
  while (tmp_list)
    {
      GtkTreeViewChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      if (gtk_widget_intersect (child->widget, area, &child_area))
	gtk_widget_draw (child->widget, &child_area);
    }
  for (tmp_list = tree_view->priv->column; tmp_list; tmp_list = tmp_list->next)
    {
      column = tmp_list->data;
      if (!column->visible)
	continue;
      if (column->button &&
	  gtk_widget_intersect(column->button, area, &child_area))
	gtk_widget_draw (column->button, &child_area);
    }
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
  GtkRBNode *node, *last_node = NULL;
  GtkRBNode *cursor = NULL;
  GtkRBTree *cursor_tree = NULL, *last_tree = NULL;
  GtkTreeNode tree_node;
  GtkCellRenderer *cell;
  gint new_y;
  gint y_offset, x_offset, cell_offset;
  gint i, max_height;
  gint depth;
  GdkRectangle background_area;
  GdkRectangle cell_area;
  guint flags;
  gboolean last_selected;

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
  y_offset = -_gtk_rbtree_find_offset (tree_view->priv->tree,
				       new_y - TREE_VIEW_HEADER_HEIGHT (tree_view),
				       &tree,
				       &node) + new_y - event->area.y;
  if (node == NULL)
    return TRUE;

  /* See if the last node was selected */
  _gtk_rbtree_prev_full (tree, node, &last_tree, &last_node);
  last_selected = (last_node && GTK_RBNODE_FLAG_SET (last_node, GTK_RBNODE_IS_SELECTED));

  /* find the path for the node */
  path = _gtk_tree_view_find_path ((GtkTreeView *)widget,
				   tree,
				   node);
  tree_node = gtk_tree_model_get_node (tree_view->priv->model, path);
  depth = gtk_tree_path_get_depth (path);
  gtk_tree_path_free (path);

  if (tree_view->priv->cursor)
    _gtk_tree_view_find_node (tree_view, tree_view->priv->cursor, &cursor_tree, &cursor);

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
      max_height = GTK_RBNODE_GET_HEIGHT (node);

      x_offset = -event->area.x;
      cell_offset = 0;

      background_area.y = y_offset + event->area.y + TREE_VIEW_VERTICAL_SEPERATOR;
      background_area.height = max_height - TREE_VIEW_VERTICAL_SEPERATOR;
      flags = 0;

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PRELIT))
	flags |= GTK_CELL_RENDERER_PRELIT;

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
	{
	  flags |= GTK_CELL_RENDERER_SELECTED;

	  /* Draw the selection */
	  gdk_draw_rectangle (event->window,
			      GTK_WIDGET (tree_view)->style->bg_gc [GTK_STATE_SELECTED],
			      TRUE,
			      event->area.x,
			      background_area.y - (last_selected?TREE_VIEW_VERTICAL_SEPERATOR:0),
			      event->area.width,
			      background_area.height + (last_selected?TREE_VIEW_VERTICAL_SEPERATOR:0));
	  last_selected = TRUE;
	}
      else
	{
	  last_selected = FALSE;
	}

      for (i = 0, list = tree_view->priv->column; i < tree_view->priv->columns; i++, list = list->next)
	{
	  GtkTreeViewColumn *column = list->data;

	  if (!column->visible)
	    continue;

	  cell = column->cell;
	  gtk_tree_view_column_set_cell_data (column,
					      tree_view->priv->model,
					      tree_node);

	  background_area.x = cell_offset;
	  background_area.width = TREE_VIEW_COLUMN_SIZE (column);
	  if (i == 0 && TREE_VIEW_DRAW_EXPANDERS(tree_view))
	    {
	      cell_area = background_area;
	      cell_area.x += depth*tree_view->priv->tab_offset;
	      cell_area.width -= depth*tree_view->priv->tab_offset;
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
					    node,
					    event->area.y + y_offset,
					    x, y);
		}
	    }
	  else
	    {
	      cell_area = background_area;
	      gtk_cell_renderer_render (cell,
					event->window,
					widget,
					&background_area,
					&cell_area,
					&event->area,
					flags);
	    }
	  cell_offset += TREE_VIEW_COLUMN_SIZE (column);
	}

      if (node == cursor &&
	  GTK_WIDGET_HAS_FOCUS (widget))
	gtk_tree_view_draw_focus (widget);

      y_offset += max_height;
      if (node->children)
	{
	  tree = node->children;
	  node = tree->root;
	  while (node->left != tree->nil)
	    node = node->left;
	  tree_node = gtk_tree_model_node_children (tree_view->priv->model, tree_node);
	  cell = gtk_tree_view_get_column (tree_view, 0)->cell;
	  depth++;

	  /* Sanity Check! */
	  TREE_VIEW_INTERNAL_ASSERT (tree_node != NULL, FALSE);
	}
      else
	{
	  gboolean done = FALSE;
	  do
	    {
	      node = _gtk_rbtree_next (tree, node);
	      if (node != NULL)
		{
		  gtk_tree_model_node_next (tree_view->priv->model, &tree_node);
		  cell = gtk_tree_view_get_column (tree_view, 0)->cell;
		  done = TRUE;

		  /* Sanity Check! */
		  TREE_VIEW_INTERNAL_ASSERT (tree_node != NULL, FALSE);
		}
	      else
		{
		  node = tree->parent_node;
		  tree = tree->parent_tree;
		  if (tree == NULL)
		    /* we've run out of tree.  It's okay though, as we'd only break
		     * out of the while loop below. */
		    return TRUE;
		  tree_node = gtk_tree_model_node_parent (tree_view->priv->model, tree_node);
		  depth--;

		  /* Sanity check */
		  TREE_VIEW_INTERNAL_ASSERT (tree_node != NULL, FALSE);
		}
	    }
	  while (!done);
	}
    }
  while (y_offset < event->area.height);

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
gtk_tree_view_motion (GtkWidget *widget,
		      GdkEventMotion  *event)
{
  GtkTreeView *tree_view;
  GtkRBTree *tree;
  GtkRBNode *node;
  gint new_y;
  gint y_offset;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);

  tree_view = GTK_TREE_VIEW (widget);

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
	  gtk_tree_view_column_set_size (gtk_tree_view_get_column (GTK_TREE_VIEW (widget), tree_view->priv->drag_pos), new_width);
	}

      /* FIXME: We need to scroll */
      _gtk_tree_view_set_size (GTK_TREE_VIEW (widget), -1, tree_view->priv->height);
      return FALSE;
    }

  /* Sanity check it */
  if (event->window != tree_view->priv->bin_window)
    return FALSE;
  if (tree_view->priv->tree == NULL)
    return FALSE;

  if (tree_view->priv->prelight_node != NULL)
    {
      if ((((gint) event->y - TREE_VIEW_HEADER_HEIGHT (tree_view) < tree_view->priv->prelight_offset) ||
	   ((gint) event->y - TREE_VIEW_HEADER_HEIGHT (tree_view) >=
	    (tree_view->priv->prelight_offset + GTK_RBNODE_GET_HEIGHT (tree_view->priv->prelight_node))) ||
	   ((gint) event->x > tree_view->priv->tab_offset)))
	/* We need to unprelight the old one. */
	{
	  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT))
	    {
	      GTK_RBNODE_UNSET_FLAG (tree_view->priv->prelight_node, GTK_RBNODE_IS_PRELIT);
	      gtk_tree_view_draw_arrow (GTK_TREE_VIEW (widget),
					tree_view->priv->prelight_node,
					tree_view->priv->prelight_offset,
					event->x,
					event->y);
	      GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_ARROW_PRELIT);
	    }

	  GTK_RBNODE_UNSET_FLAG (tree_view->priv->prelight_node, GTK_RBNODE_IS_PRELIT);
	  tree_view->priv->prelight_node = NULL;
	  tree_view->priv->prelight_tree = NULL;
	  tree_view->priv->prelight_offset = 0;
	}
    }

  new_y = ((gint)event->y<TREE_VIEW_HEADER_HEIGHT (tree_view))?TREE_VIEW_HEADER_HEIGHT (tree_view):(gint)event->y;
  y_offset = -_gtk_rbtree_find_offset (tree_view->priv->tree, new_y - TREE_VIEW_HEADER_HEIGHT (tree_view),
				       &tree,
				       &node) + new_y - (gint)event->y;

  if (node == NULL)
    return TRUE;

  /* If we are currently pressing down a button, we don't want to prelight anything else. */
  if ((tree_view->priv->button_pressed_node != NULL) &&
      (tree_view->priv->button_pressed_node != node))
    return TRUE;

  /* Do we want to prelight a tab? */
  GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_ARROW_PRELIT);
  if (event->x <= tree_view->priv->tab_offset &&
      event->x >= 0 &&
      ((node->flags & GTK_RBNODE_IS_PARENT) == GTK_RBNODE_IS_PARENT))
    {
      tree_view->priv->prelight_offset = event->y+y_offset;
      GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_ARROW_PRELIT);
    }

  tree_view->priv->prelight_node = node;
  tree_view->priv->prelight_tree = tree;
  tree_view->priv->prelight_offset = event->y+y_offset;

  GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_PRELIT);
  gtk_widget_queue_draw (widget);

  return TRUE;
}

/* Is this function necessary? Can I get an enter_notify event w/o either
 * an expose event or a mouse motion event?
 */
static gboolean
gtk_tree_view_enter_notify (GtkWidget        *widget,
			    GdkEventCrossing *event)
{
  GtkTreeView *tree_view;
  GtkRBTree *tree;
  GtkRBNode *node;
  gint new_y;
  gint y_offset;

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
  y_offset = -_gtk_rbtree_find_offset (tree_view->priv->tree,
				       new_y - TREE_VIEW_HEADER_HEIGHT (tree_view),
				       &tree,
				       &node) + new_y - (gint)event->y;

  if (node == NULL)
    return FALSE;

  /* Do we want to prelight a tab? */
  GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_ARROW_PRELIT);
  if (event->x <= tree_view->priv->tab_offset &&
      event->x >= 0 &&
      ((node->flags & GTK_RBNODE_IS_PARENT) == GTK_RBNODE_IS_PARENT))
    {
      tree_view->priv->prelight_offset = event->y+y_offset;
      GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_ARROW_PRELIT);
    }

  tree_view->priv->prelight_node = node;
  tree_view->priv->prelight_tree = tree;
  tree_view->priv->prelight_offset = event->y+y_offset;

  GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_PRELIT);
  gtk_widget_queue_draw (widget);

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

  if (tree_view->priv->prelight_node != NULL)
    {
      GTK_RBNODE_UNSET_FLAG (tree_view->priv->prelight_node, GTK_RBNODE_IS_PRELIT);
      tree_view->priv->prelight_node = NULL;
      tree_view->priv->prelight_tree = NULL;
      tree_view->priv->prelight_offset = 0;
      GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_ARROW_PRELIT);
      gtk_widget_queue_draw (widget);
    }
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
      if (tree_view->priv->prelight_node != FALSE && GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT))
	{
	  if (event->button == 1)
	    {
	      gtk_grab_add (widget);
	      tree_view->priv->button_pressed_node = tree_view->priv->prelight_node;
	      tree_view->priv->button_pressed_tree = tree_view->priv->prelight_tree;
	      gtk_tree_view_draw_arrow (GTK_TREE_VIEW (widget),
					tree_view->priv->prelight_node,
					tree_view->priv->prelight_offset,
					event->x,
					event->y);
	    }
	  return TRUE;
	}

      /* find the node that was clicked */
      new_y = ((gint)event->y<TREE_VIEW_HEADER_HEIGHT (tree_view))?TREE_VIEW_HEADER_HEIGHT (tree_view):(gint)event->y;
      y_offset = -_gtk_rbtree_find_offset (tree_view->priv->tree,
					   new_y - TREE_VIEW_HEADER_HEIGHT (tree_view),
					   &tree,
					   &node) + new_y - (gint)event->y;

      if (node == NULL)
	/* We clicked in dead space */
	return TRUE;

      /* Get the path and the node */
      path = _gtk_tree_view_find_path (tree_view, tree, node);
      depth = gtk_tree_path_get_depth (path);
      background_area.y = y_offset + event->y + TREE_VIEW_VERTICAL_SEPERATOR;
      background_area.height = GTK_RBNODE_GET_HEIGHT (node) - TREE_VIEW_VERTICAL_SEPERATOR;
      background_area.x = 0;
      /* Let the cell have a chance at selecting it. */

      for (i = 0, list = tree_view->priv->column; i < tree_view->priv->columns; i++, list = list->next)
	{
	  GtkTreeViewColumn *column = list->data;
	  GtkCellRenderer *cell;
	  GtkTreeNode *tree_node;

	  if (!column->visible)
	    continue;

	  background_area.width = TREE_VIEW_COLUMN_SIZE (column);
	  if (i == 0 && TREE_VIEW_DRAW_EXPANDERS(tree_view))
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

	  tree_node = gtk_tree_model_get_node (tree_view->priv->model,
					       path);
	  gtk_tree_view_column_set_cell_data (column,
					      tree_view->priv->model,
					      tree_node);

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
      /* Handle the selection */
      if (tree_view->priv->selection == NULL)
	gtk_tree_selection_new_with_tree_view (tree_view);

      _gtk_tree_selection_internal_select_node (tree_view->priv->selection,
						node,
						tree,
						path,
						event->state);
      gtk_tree_path_free (path);
      return TRUE;
    }

  for (i = 0, list = tree_view->priv->column; list; list = list->next, i++)
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
      gtk_tree_view_column_set_size (gtk_tree_view_get_column (GTK_TREE_VIEW (widget), i), width);
      return FALSE;
    }

  if (tree_view->priv->button_pressed_node == NULL)
    return FALSE;

  if (event->button == 1)
    {
      gtk_grab_remove (widget);
      if (tree_view->priv->button_pressed_node == tree_view->priv->prelight_node && GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT))
	{
	  GtkTreePath *path;
	  GtkTreeNode *tree_node;

	  /* Actually activate the node */
	  if (tree_view->priv->button_pressed_node->children == NULL)
	    {
	      path = _gtk_tree_view_find_path (GTK_TREE_VIEW (widget),
					       tree_view->priv->button_pressed_tree,
					       tree_view->priv->button_pressed_node);
	      tree_view->priv->button_pressed_node->children = _gtk_rbtree_new ();
	      tree_view->priv->button_pressed_node->children->parent_tree = tree_view->priv->button_pressed_tree;
	      tree_view->priv->button_pressed_node->children->parent_node = tree_view->priv->button_pressed_node;
	      tree_node = gtk_tree_model_get_node (tree_view->priv->model, path);
	      tree_node = gtk_tree_model_node_children (tree_view->priv->model, tree_node);

	      gtk_tree_view_build_tree (tree_view,
					tree_view->priv->button_pressed_node->children,
					tree_node,
					gtk_tree_path_get_depth (path) + 1,
					FALSE,
					GTK_WIDGET_REALIZED (widget));
	    }
	  else
	    {
	      path = _gtk_tree_view_find_path (GTK_TREE_VIEW (widget),
					       tree_view->priv->button_pressed_node->children,
					       tree_view->priv->button_pressed_node->children->root);
	      tree_node = gtk_tree_model_get_node (tree_view->priv->model, path);

	      gtk_tree_view_discover_dirty (GTK_TREE_VIEW (widget),
					    tree_view->priv->button_pressed_node->children,
					    tree_node,
					    gtk_tree_path_get_depth (path));
	      _gtk_rbtree_remove (tree_view->priv->button_pressed_node->children);
	    }
	  gtk_tree_path_free (path);

	  _gtk_tree_view_set_size (GTK_TREE_VIEW (widget), -1, -1);
	  gtk_widget_queue_resize (widget);
	}

      tree_view->priv->button_pressed_node = NULL;
    }

  return TRUE;
}


static void
gtk_tree_view_draw_focus (GtkWidget *widget)
{
  GtkTreeView *tree_view;
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *cursor = NULL;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (widget));

  tree_view = GTK_TREE_VIEW (widget);

  if (! GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS))
    return;
  if (tree_view->priv->cursor == NULL)
    return;

  _gtk_tree_view_find_node (tree_view, tree_view->priv->cursor, &cursor_tree, &cursor);
  if (cursor == NULL)
    return;

  gdk_draw_rectangle (tree_view->priv->bin_window,
		      widget->style->fg_gc[GTK_STATE_NORMAL],
		      FALSE,
		      0,
		      _gtk_rbtree_node_find_offset (cursor_tree, cursor) + TREE_VIEW_HEADER_HEIGHT (tree_view),
		      (gint) MAX (tree_view->priv->width, tree_view->priv->hadjustment->upper),
		      GTK_RBNODE_GET_HEIGHT (cursor));
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

  gtk_widget_draw_focus (widget);

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

  gtk_widget_queue_draw (widget);

  return FALSE;
}

/* FIXME: It would be neat to someday make the headers a seperate widget that
 * can be shared between various apps
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

  for (last_column = g_list_last (tree_view->priv->column);
       last_column &&
	 !(GTK_TREE_VIEW_COLUMN (last_column->data)->visible) &&
	 GTK_WIDGET_CAN_FOCUS (GTK_TREE_VIEW_COLUMN (last_column->data)->button);
       last_column = last_column->prev)
    ;

  for (first_column = tree_view->priv->column;
       first_column &&
	 !(GTK_TREE_VIEW_COLUMN (first_column->data)->visible) &&
	 GTK_WIDGET_CAN_FOCUS (GTK_TREE_VIEW_COLUMN (first_column->data)->button);
       first_column = first_column->next)
    ;

  /* no headers are visible, or are focussable.  We can't focus in or out.
   * I wonder if focussable is a real word...
   */
  if (last_column == NULL)
    {
      gtk_container_set_focus_child (container, NULL);
      return FALSE;
    }

  /* First thing we want to handle is entering and leaving the headers.
   */
  switch (dir)
    {
    case GTK_DIR_TAB_BACKWARD:
      if (!focus_child)
	{
	  focus_child = GTK_TREE_VIEW_COLUMN (last_column->data)->button;
	  gtk_container_set_focus_child (container, focus_child);
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
	  gtk_container_set_focus_child (container, focus_child);
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
	  gtk_container_set_focus_child (container, focus_child);
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
	  gtk_container_set_focus_child (container, focus_child);
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
	  gtk_container_set_focus_child (container, focus_child);
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
	  gtk_container_set_focus_child (container, focus_child);
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
      for (tmp_list = tree_view->priv->column; tmp_list; tmp_list = tmp_list->next)
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
	      gtk_container_set_focus_child (container, column->button);
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
  else
    {
      gtk_container_set_focus_child (container, NULL);
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
	  gtk_container_set_focus_child (container, NULL);
	  return FALSE;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_RIGHT:
	case GTK_DIR_DOWN:
	  if (direction == GTK_DIR_DOWN)
	    {
	      gtk_container_set_focus_child (container, NULL);
	    }
	  else
	    {
	      if (gtk_tree_view_header_focus (tree_view, direction))
		return TRUE;
	    }
	  GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS);
	  gtk_widget_grab_focus (GTK_WIDGET (container));

	  if (tree_view->priv->selection == NULL)
	    gtk_tree_selection_new_with_tree_view (tree_view);

	  /* if there is no keyboard focus yet, we select the first node
	   */
	  if (tree_view->priv->cursor == NULL)
	    tree_view->priv->cursor = gtk_tree_path_new_root ();
	  if (tree_view->priv->cursor)
	    gtk_tree_selection_select_path (tree_view->priv->selection,
					    tree_view->priv->cursor);
	  gtk_widget_queue_draw (GTK_WIDGET (tree_view));
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
	gtk_tree_selection_new_with_tree_view (tree_view);

      if (tree_view->priv->cursor == NULL)
	tree_view->priv->cursor = gtk_tree_path_new_root ();

      if (tree_view->priv->cursor)
	gtk_tree_selection_select_path (tree_view->priv->selection,
					tree_view->priv->cursor);
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
      return TRUE;
    }

  /* Case 3. We have focus already, but no cursor.  We pick the first one
   * and run with it. */
  if (tree_view->priv->cursor == NULL)
    {
      /* We lost our cursor somehow.  Arbitrarily select the first node, and
       * return
       */
      tree_view->priv->cursor = gtk_tree_path_new_root ();

      if (tree_view->priv->cursor)
	gtk_tree_selection_select_path (tree_view->priv->selection,
					tree_view->priv->cursor);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->vadjustment),
				0.0);
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
      return TRUE;
    }


  /* Case 3. We have focus already.  Move the cursor. */
  if (direction == GTK_DIR_LEFT)
    {
      gfloat val;
      val = tree_view->priv->hadjustment->value - tree_view->priv->hadjustment->page_size/2;
      val = MAX (val, 0.0);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->hadjustment), val);
      gtk_widget_grab_focus (GTK_WIDGET (tree_view));
      return TRUE;
    }
  if (direction == GTK_DIR_RIGHT)
    {
      gfloat val;
      val = tree_view->priv->hadjustment->value + tree_view->priv->hadjustment->page_size/2;
      val = MIN (tree_view->priv->hadjustment->upper - tree_view->priv->hadjustment->page_size, val);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->hadjustment), val);
      gtk_widget_grab_focus (GTK_WIDGET (tree_view));
      return TRUE;
    }
  cursor_tree = NULL;
  cursor_node = NULL;

  _gtk_tree_view_find_node (tree_view, tree_view->priv->cursor,
			    &cursor_tree,
			    &cursor_node);
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

      event = gdk_event_peek ();
      if (event && event->type == GDK_KEY_PRESS)
	/* FIXME: This doesn't seem to work. )-:
	 * I fear the event may already have been gotten */
	state = ((GdkEventKey *)event)->state;

      if (event)
	gdk_event_free (event);
      gtk_tree_path_free (tree_view->priv->cursor);

      tree_view->priv->cursor = _gtk_tree_view_find_path (tree_view,
							  cursor_tree,
							  cursor_node);
      if (tree_view->priv->cursor)
	_gtk_tree_selection_internal_select_node (tree_view->priv->selection,
						  cursor_node,
						  cursor_tree,
						  tree_view->priv->cursor,
						  state);
      gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);
      gtk_widget_grab_focus (GTK_WIDGET (tree_view));
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
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

  for (tmp_list = tree_view->priv->column; tmp_list; tmp_list = tmp_list->next)
    {
      column = tmp_list->data;
      if (column->button)
	(* callback) (column->button, callback_data);
    }
}

/* TreeModel Methods
 */

static void
gtk_tree_view_node_changed (GtkTreeModel *model,
			    GtkTreePath  *path,
			    GtkTreeNode  *tree_node,
			    gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *)data;
  GtkRBTree *tree;
  GtkRBNode *node;
  gint height;
  gboolean dirty_marked;

  g_return_if_fail (path != NULL || node != NULL);

  if (path == NULL)
    path = gtk_tree_model_get_path (model, tree_node);
  else if (tree_node == NULL)
    tree_node = gtk_tree_model_get_node (model, path);

  if (_gtk_tree_view_find_node (tree_view,
				path,
				&tree,
				&node))
    /* We aren't actually showing the node */
    return;

  dirty_marked = gtk_tree_view_discover_dirty_node (tree_view,
						    tree_node,
						    gtk_tree_path_get_depth (path),
						    &height);

  if (GTK_RBNODE_GET_HEIGHT (node) != height + TREE_VIEW_VERTICAL_SEPERATOR)
    {
      _gtk_rbtree_node_set_height (tree, node, height + TREE_VIEW_VERTICAL_SEPERATOR);
      gtk_widget_queue_resize (GTK_WIDGET (data));
      return;
    }
  if (dirty_marked)
    gtk_widget_queue_resize (GTK_WIDGET (data));
  else
    {
      /* FIXME: just redraw the node */
      gtk_widget_queue_resize (GTK_WIDGET (data));
    }
}

static void
gtk_tree_view_node_inserted (GtkTreeModel *model,
			     GtkTreePath  *path,
			     GtkTreeNode  *tree_node,
			     gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *) data;
  gint *indices;
  GtkRBTree *tmptree, *tree;
  GtkRBNode *tmpnode = NULL;
  gint max_height;
  gint depth;
  gint i = 0;

  tmptree = tree = tree_view->priv->tree;
  g_return_if_fail (path != NULL || tree_node != NULL);

  if (path == NULL)
    path = gtk_tree_model_get_path (model, tree_node);
  else if (tree_node == NULL)
    tree_node = gtk_tree_model_get_node (model, path);

  depth = gtk_tree_path_get_depth (path);
  indices = gtk_tree_path_get_indices (path);

  /* First, find the parent tree */
  while (i < depth - 1)
    {
      if (tmptree == NULL)
	{
	  /* We aren't showing the node */
	  return;
	}

      tmpnode = _gtk_rbtree_find_count (tmptree, indices[i] + 1);
      if (tmpnode == NULL)
	{
	  g_warning ("A node was inserted with a parent that's not in the tree.\n" \
		     "This possibly means that a GtkTreeModel inserted a child node\n" \
		     "before the parent was inserted.");
	  return;
	}
      else if (!GTK_RBNODE_FLAG_SET (tmpnode, GTK_RBNODE_IS_PARENT))
	{
	  /* In theory, the model should have emitted child_toggled here.  We
	   * try to catch it anyway, just to be safe, in case the model hasn't.
	   */
	  GtkTreePath *tmppath = _gtk_tree_view_find_path (tree_view,
							   tree,
							   tmpnode);
	  gtk_tree_view_node_child_toggled (model, tmppath, NULL, data);
	  gtk_tree_path_free (tmppath);
	  return;
	}

      tmptree = tmpnode->children;
      tree = tmptree;
      i++;
    }

  if (tree == NULL)
    return;

  /* next, update the selection */
  if (tree_view->priv->anchor)
    {
      gint *select_indices = gtk_tree_path_get_indices (tree_view->priv->anchor);
      gint select_depth = gtk_tree_path_get_depth (tree_view->priv->anchor);

      for (i = 0; i < depth && i < select_depth; i++)
	{
	  if (indices[i] < select_indices[i])
	    {
	      select_indices[i]++;
	      break;
	    }
	  else if (indices[i] > select_indices[i])
	    break;
	  else if (i == depth - 1)
	    {
	      select_indices[i]++;
	      break;
	    }
	}
    }

  max_height = gtk_tree_view_insert_node_height (tree_view,
						 tree,
						 tree_node,
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
}

static void
gtk_tree_view_node_child_toggled (GtkTreeModel *model,
				  GtkTreePath  *path,
				  GtkTreeNode  *tree_node,
				  gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *)data;
  gboolean has_child;
  GtkRBTree *tree;
  GtkRBNode *node;

  g_return_if_fail (path != NULL || node != NULL);

  if (path == NULL)
    path = gtk_tree_model_get_path (model, tree_node);
  else if (tree_node == NULL)
    tree_node = gtk_tree_model_get_node (model, path);

  if (_gtk_tree_view_find_node (tree_view,
				path,
				&tree,
				&node))
    /* We aren't actually showing the node */
    return;

  has_child = gtk_tree_model_node_has_child (model, tree_node);
  /* Sanity check.
   */
  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT) == has_child)
    return;

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
	  for (list = tree_view->priv->column; list; list = list->next)
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
}

static void
gtk_tree_view_node_deleted (GtkTreeModel *model,
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

  /* next, update the selection */
#if 0
  if (tree_view->priv->anchor)
    {
      gint i;
      gint *select_indices = gtk_tree_path_get_indices (tree_view->priv->anchor);
      gint select_depth = gtk_tree_path_get_depth (tree_view->priv->anchor);

      if (gtk_tree_path_compare (path, tree_view->priv->anchor) == 0)
	{

	}
      else
	{
	  for (i = 0; i < depth && i < select_depth; i++)
	    {
	      if (indices[i] < select_indices[i])
		{
		  select_indices[i] = MAX (select_indices[i], 0);
		  break;
		}
	      else if (indices[i] > select_indices[i])
		break;
	      else if (i == depth - 1)
		{
		  select_indices[i] = MAX (select_indices[i], 0);
		  break;
		}
	    }
	}
    }
#endif

  for (list = tree_view->priv->column; list; list = list->next)
    if (((GtkTreeViewColumn *)list->data)->visible &&
	((GtkTreeViewColumn *)list->data)->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
      ((GtkTreeViewColumn *)list->data)->dirty = TRUE;

  if (tree->root->count == 1)
    _gtk_rbtree_remove (tree);
  else
    _gtk_rbtree_remove_node (tree, node);

  _gtk_tree_view_set_size (GTK_TREE_VIEW (data), -1, -1);
  gtk_widget_queue_resize (data);
}

/* Internal tree functions */
static gint
gtk_tree_view_insert_node_height (GtkTreeView *tree_view,
				  GtkRBTree   *tree,
				  GtkTreeNode  node,
				  gint         depth)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  gboolean first = TRUE;
  GList *list;
  gint max_height = 0;

  /* do stuff with node */
  for (list = tree_view->priv->column; list; list = list->next)
    {
      gint height = 0, width = 0;
      column = list->data;

      if (!column->visible)
	continue;
      if (column->column_type == GTK_TREE_VIEW_COLUMN_FIXED)
	{
	  first = FALSE;;
	  continue;
	}

      cell = column->cell;
      gtk_tree_view_column_set_cell_data (column, tree_view->priv->model, node);

      gtk_cell_renderer_get_size (cell, GTK_WIDGET (tree_view), &width, &height);
      max_height = MAX (max_height, TREE_VIEW_VERTICAL_SEPERATOR + height);

      if (first == TRUE && TREE_VIEW_DRAW_EXPANDERS (tree_view))
	column->size = MAX (column->size, depth * tree_view->priv->tab_offset + width);
      else
	column->size = MAX (column->size, width);

      first = FALSE;
    }
  return max_height;

}

static void
gtk_tree_view_build_tree (GtkTreeView *tree_view,
			  GtkRBTree   *tree,
			  GtkTreeNode  node,
			  gint         depth,
			  gboolean     recurse,
			  gboolean     calc_bounds)
{
  GtkRBNode *temp = NULL;
  GtkTreeNode child;
  gint max_height;

  if (!node)
    return;
  do
    {
      max_height = 0;
      if (calc_bounds)
	max_height = gtk_tree_view_insert_node_height (tree_view,
						       tree,
						       node,
						       depth);
      temp = _gtk_rbtree_insert_after (tree, temp, max_height);
      if (recurse)
	{
	  child = gtk_tree_model_node_children (tree_view->priv->model, node);
	  if (child != NULL)
	    {
	      temp->children = _gtk_rbtree_new ();
	      temp->children->parent_tree = tree;
	      temp->children->parent_node = temp;
	      gtk_tree_view_build_tree (tree_view, temp->children, child, depth + 1, recurse, calc_bounds);
	    }
	}
      if (gtk_tree_model_node_has_child (tree_view->priv->model, node))
	{
	  if ((temp->flags&GTK_RBNODE_IS_PARENT) != GTK_RBNODE_IS_PARENT)
	    temp->flags ^= GTK_RBNODE_IS_PARENT;
	  GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_IS_LIST);
	}
    }
  while (gtk_tree_model_node_next (tree_view->priv->model, &node));
}

static void
gtk_tree_view_calc_size (GtkTreeView *tree_view,
			 GtkRBTree   *tree,
			 GtkTreeNode  node,
			 gint         depth)
{
  GtkRBNode *temp = tree->root;
  GtkTreeNode child;
  GtkCellRenderer *cell;
  GList *list;
  GtkTreeViewColumn *column;
  gint max_height;
  gint i;

  /* FIXME: Make this function robust against internal inconsistencies! */
  if (!node)
    return;
  TREE_VIEW_INTERNAL_ASSERT_VOID (tree != NULL);

  while (temp->left != tree->nil)
    temp = temp->left;

  do
    {
      max_height = 0;
      /* Do stuff with node */
      for (list = tree_view->priv->column, i = 0; i < tree_view->priv->columns; list = list->next, i++)
	{
	  gint height = 0, width = 0;
	  column = list->data;

	  if (!column->visible)
	    continue;

	  gtk_tree_view_column_set_cell_data (column, tree_view->priv->model, node);
	  cell = column->cell;
	  gtk_cell_renderer_get_size (cell, GTK_WIDGET (tree_view), &width, &height);
	  max_height = MAX (max_height, TREE_VIEW_VERTICAL_SEPERATOR + height);

	  /* FIXME: I'm getting the width of all nodes here. )-: */
	  if (column->dirty == FALSE || column->column_type == GTK_TREE_VIEW_COLUMN_FIXED)
	    continue;

	  if (i == 0 && TREE_VIEW_DRAW_EXPANDERS (tree_view))
	    column->size = MAX (column->size, depth * tree_view->priv->tab_offset + width);
	  else
	    column->size = MAX (column->size, width);
	}
      _gtk_rbtree_node_set_height (tree, temp, max_height);
      child = gtk_tree_model_node_children (tree_view->priv->model, node);
      if (child != NULL && temp->children != NULL)
	gtk_tree_view_calc_size (tree_view, temp->children, child, depth + 1);
      temp = _gtk_rbtree_next (tree, temp);
    }
  while (gtk_tree_model_node_next (tree_view->priv->model, &node));
}

static gboolean
gtk_tree_view_discover_dirty_node (GtkTreeView *tree_view,
				   GtkTreeNode  node,
				   gint         depth,
				   gint        *height)
{
  GtkCellRenderer *cell;
  GtkTreeViewColumn *column;
  GList *list;
  gint i;
  gint retval = FALSE;
  gint tmpheight;

  /* Do stuff with node */
  if (height)
    *height = 0;

  for (i = 0, list = tree_view->priv->column; list; list = list->next, i++)
    {
      gint width;
      column = list->data;
      if (column->dirty == TRUE || column->column_type == GTK_TREE_VIEW_COLUMN_FIXED)
	continue;
      if (!column->visible)
	continue;

      cell = column->cell;
      gtk_tree_view_column_set_cell_data (column, tree_view->priv->model, node);

      if (height)
	{
	  gtk_cell_renderer_get_size (cell, GTK_WIDGET (tree_view), &width, &tmpheight);
	  *height = MAX (*height, tmpheight);
	}
      else
	{
	  gtk_cell_renderer_get_size (cell, GTK_WIDGET (tree_view), &width, NULL);
	}
      if (i == 0 && TREE_VIEW_DRAW_EXPANDERS (tree_view))
	{
	  if (depth * tree_view->priv->tab_offset + width > column->size)
	    {
	      column->dirty = TRUE;
	      retval = TRUE;
	    }
	}
      else
	{
	  if (width > column->size)
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
			      GtkTreeNode  node,
			      gint       depth)
{
  GtkRBNode *temp = tree->root;
  GtkTreeViewColumn *column;
  GList *list;
  GtkTreeNode child;
  gboolean is_all_dirty;

  /* FIXME: Make this function robust against internal inconsistencies! */
  if (!node)
    return;
  TREE_VIEW_INTERNAL_ASSERT_VOID (tree != NULL);

  while (temp->left != tree->nil)
    temp = temp->left;

  do
    {
      is_all_dirty = TRUE;
      for (list = tree_view->priv->column; list; list = list->next)
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

      gtk_tree_view_discover_dirty_node (tree_view,
					 node,
					 depth,
					 FALSE);
      child = gtk_tree_model_node_children (tree_view->priv->model, node);
      if (child != NULL && temp->children != NULL)
	gtk_tree_view_discover_dirty (tree_view, temp->children, child, depth + 1);
      temp = _gtk_rbtree_next (tree, temp);
    }
  while (gtk_tree_model_node_next (tree_view->priv->model, &node));
}


static void
gtk_tree_view_check_dirty (GtkTreeView *tree_view)
{
  GtkTreePath *path;
  GtkTreeNode *tree_node;
  gboolean dirty = FALSE;
  GList *list;
  GtkTreeViewColumn *column;

  for (list = tree_view->priv->column; list; list = list->next)
    {
      column = list->data;
      if (column->dirty)
	{
	  dirty = TRUE;
	  if (column->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
	    {
	      column->size = column->button->requisition.width;
	    }
	}
    }
  if (dirty == FALSE)
    return;

  path = gtk_tree_path_new_root ();
  if (path != NULL)
    {
      tree_node = gtk_tree_model_get_node (tree_view->priv->model, path);
      gtk_tree_path_free (path);
      gtk_tree_view_calc_size (tree_view, tree_view->priv->tree, tree_node, 1);
      _gtk_tree_view_set_size (tree_view, -1, -1);
    }

  for (list = tree_view->priv->column; list; list = list->next)
    {
      column = list->data;
      column->dirty = FALSE;
    }
}

static void
gtk_tree_view_create_button (GtkTreeView *tree_view,
			     gint       i)
{
  GtkWidget *button;
  GtkTreeViewColumn *column;

  column = g_list_nth (tree_view->priv->column, i)->data;
  gtk_widget_push_composite_child ();
  button = column->button = gtk_button_new ();
  gtk_widget_pop_composite_child ();

  gtk_widget_set_parent (button, GTK_WIDGET (tree_view));

  gtk_signal_connect (GTK_OBJECT (button), "clicked",
  		      (GtkSignalFunc) gtk_tree_view_button_clicked,
		      (gpointer) tree_view);
}

static void
gtk_tree_view_create_buttons (GtkTreeView *tree_view)
{
  GtkWidget *alignment;
  GtkWidget *label;
  GtkRequisition requisition;
  GList *list;
  GtkTreeViewColumn *column;
  gint i;

  for (list = tree_view->priv->column, i = 0; list; list = list->next, i++)
    {
      column = list->data;

      gtk_tree_view_create_button (tree_view, i);
      switch (column->justification)
	{
	case GTK_JUSTIFY_LEFT:
	  alignment = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
	  break;
	case GTK_JUSTIFY_RIGHT:
	  alignment = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
	  break;
	case GTK_JUSTIFY_CENTER:
	  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	  break;
	case GTK_JUSTIFY_FILL:
	default:
	  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	  break;
	}
      label = gtk_label_new (column->title);

      gtk_container_add (GTK_CONTAINER (alignment), label);
      gtk_container_add (GTK_CONTAINER (column->button), alignment);

      gtk_widget_show (label);
      gtk_widget_show (alignment);
      gtk_widget_size_request (column->button, &requisition);

      column->size = MAX (column->size, requisition.width);
      tree_view->priv->header_height = MAX (tree_view->priv->header_height, requisition.height);
    }
  if (GTK_WIDGET_REALIZED (tree_view))
    {
      gtk_tree_view_realize_buttons (tree_view);
      if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE))
	{
	  /* We need to do this twice, as we need to map
	   * all the buttons before we map the columns */
	  for (list = tree_view->priv->column; list; list = list->next)
	    {
	      column = list->data;
	      if (column->visible == FALSE)
		continue;
	      gtk_widget_map (column->button);
	    }
	  for (list = tree_view->priv->column; list; list = list->next)
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
	}
    }
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

  /* find the column who's button was pressed */
  for (list = tree_view->priv->column; list; list = list->next)
    if (GTK_TREE_VIEW_COLUMN (list->data)->button == widget)
      break;

  //  gtk_signal_emit (GTK_OBJECT (clist), clist_signals[CLICK_COLUMN], i);
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
 * it's important */
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

/* Returns wether or not it's a parent, or not */
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
      if (++i >= depth)
	{
	  *node = tmpnode;
	  *tree = tmptree;
	  return FALSE;
	}
      tmptree = tmpnode->children;
    }
  while (1);
}

/* x and y are the mouse position
 */
static void
gtk_tree_view_draw_arrow (GtkTreeView *tree_view,
			  GtkRBNode   *node,
			  gint         offset,
			  gint         x,
			  gint         y)
{
  GdkRectangle area;
  GtkStateType state;
  GtkShadowType shadow;
  GdkPoint points[3];

  area.x = 0;
  area.y = offset + TREE_VIEW_VERTICAL_SEPERATOR;
  area.width = tree_view->priv->tab_offset - 2;
  area.height = GTK_RBNODE_GET_HEIGHT (node) - TREE_VIEW_VERTICAL_SEPERATOR;

  if (node == tree_view->priv->button_pressed_node)
    {
      if (x >= area.x && x <= (area.x + area.width) &&
	  y >= area.y && y <= (area.y + area.height))
	{
	  state = GTK_STATE_ACTIVE;
	  shadow = GTK_SHADOW_IN;
	}
      else
	{
	  state = GTK_STATE_NORMAL;
	  shadow = GTK_SHADOW_OUT;
	}
    }
  else
    {
      state = (node==tree_view->priv->prelight_node&&GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT)?GTK_STATE_PRELIGHT:GTK_STATE_NORMAL);
      shadow = GTK_SHADOW_OUT;
    }

  if (TRUE ||
      (((node->flags & GTK_RBNODE_IS_PARENT) == GTK_RBNODE_IS_PARENT) &&
       node->children))
    {
      points[0].x = area.x + 2;
      points[0].y = area.y + (area.height - TREE_VIEW_EXPANDER_HEIGHT)/2;
      points[1].x = points[0].x + TREE_VIEW_EXPANDER_WIDTH/2;
      points[1].y = points[0].y + TREE_VIEW_EXPANDER_HEIGHT/2;
      points[2].x = points[0].x;
      points[2].y = points[0].y + TREE_VIEW_EXPANDER_HEIGHT;

    }

  gdk_draw_polygon (tree_view->priv->bin_window,
		    GTK_WIDGET (tree_view)->style->base_gc[state],
		    TRUE, points, 3);
  gdk_draw_polygon (tree_view->priv->bin_window,
		    GTK_WIDGET (tree_view)->style->fg_gc[state],
		    FALSE, points, 3);


  /*    gtk_paint_arrow (GTK_WIDGET (tree_view)->style, */
  /*  		   tree_view->priv->bin_window, */
  /*  		   state, */
  /*  		   shadow, */
  /*  		   &area, */
  /*  		   GTK_WIDGET (tree_view), */
  /*  		   "GtkTreeView", */
  /*  		   arrow_dir, */
  /*  		   TRUE, */
  /*  		   area.x, area.y, */
  /*  		   area.width, area.height); */
}

void
_gtk_tree_view_set_size (GtkTreeView     *tree_view,
			 gint           width,
			 gint           height)
{
  GList *list;
  GtkTreeViewColumn *column;
  gint i;

  if (tree_view->priv->model == NULL)
    {
      tree_view->priv->width = 1;
      tree_view->priv->height = 1;
      return;
    }
  if (width == -1)
    {
      width = 0;
      for (list = tree_view->priv->column, i = 0; list; list = list->next, i++)
	{
	  column = list->data;
	  if (!column->visible)
	    continue;
	  width += TREE_VIEW_COLUMN_SIZE (column);
	}
    }
  if (height == -1)
    height = tree_view->priv->tree->root->offset + TREE_VIEW_VERTICAL_SEPERATOR;

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
 * in as a pointer and automagicly corrected if it's beyond min/max limits */
static gint
gtk_tree_view_new_column_width (GtkTreeView *tree_view,
				gint       i,
				gint      *x)
{
  GtkTreeViewColumn *column;
  gint width;

  /* first translate the x position from widget->window
   * to clist->clist_window */

  column = g_list_nth (tree_view->priv->column, i)->data;
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
GtkWidget *
gtk_tree_view_new (void)
{
  GtkTreeView *tree_view;

  tree_view = GTK_TREE_VIEW (gtk_type_new (gtk_tree_view_get_type ()));

  return GTK_WIDGET (tree_view);
}

GtkWidget *
gtk_tree_view_new_with_model (GtkTreeModel *model)
{
  GtkTreeView *tree_view;

  tree_view = GTK_TREE_VIEW (gtk_type_new (gtk_tree_view_get_type ()));
  gtk_tree_view_set_model (tree_view, model);

  return GTK_WIDGET (tree_view);
}

GtkTreeModel *
gtk_tree_view_get_model (GtkTreeView *tree_view)
{
  g_return_val_if_fail (tree_view != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->model;
}

static void
gtk_tree_view_set_model_realized (GtkTreeView *tree_view)
{
  GtkTreePath *path;
  GtkTreeNode  node;

  tree_view->priv->tree = _gtk_rbtree_new ();

  gtk_signal_connect (GTK_OBJECT (tree_view->priv->model),
		      "node_changed",
		      gtk_tree_view_node_changed,
		      tree_view);
  gtk_signal_connect (GTK_OBJECT (tree_view->priv->model),
		      "node_inserted",
		      gtk_tree_view_node_inserted,
		      tree_view);
  gtk_signal_connect (GTK_OBJECT (tree_view->priv->model),
		      "node_child_toggled",
		      gtk_tree_view_node_child_toggled,
		      tree_view);
  gtk_signal_connect (GTK_OBJECT (tree_view->priv->model),
		      "node_deleted",
		      gtk_tree_view_node_deleted,
		      tree_view);

  if (tree_view->priv->column == NULL)
    return;

  path = gtk_tree_path_new_root ();
  if (path == NULL)
    return;

  node = gtk_tree_model_get_node (tree_view->priv->model, path);
  gtk_tree_path_free (path);
  gtk_tree_view_build_tree (tree_view, tree_view->priv->tree, node, 1, FALSE, GTK_WIDGET_REALIZED (tree_view));

  gtk_tree_view_create_buttons (tree_view);
  GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_MODEL_SETUP);
}

void
gtk_tree_view_set_model (GtkTreeView *tree_view, GtkTreeModel *model)
{
  GList *list;
  GtkTreeViewColumn *column;

  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->model != NULL)
    {
      for (list = tree_view->priv->column; list; list = list->next)
	{
	  column = list->data;
	  if (column->button)
	    {
	      gtk_widget_unparent (column->button);
	      gdk_window_set_user_data (column->window, NULL);
	      gdk_window_destroy (column->window);
	    }
	}
      if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_MODEL_SETUP))
	{
	  gtk_signal_disconnect_by_func (GTK_OBJECT (tree_view->priv->model),
					 gtk_tree_view_node_changed,
					 tree_view);
	  gtk_signal_disconnect_by_func (GTK_OBJECT (tree_view->priv->model),
					 gtk_tree_view_node_inserted,
					 tree_view);
	  gtk_signal_disconnect_by_func (GTK_OBJECT (tree_view->priv->model),
					 gtk_tree_view_node_child_toggled,
					 tree_view);
	  gtk_signal_disconnect_by_func (GTK_OBJECT (tree_view->priv->model),
					 gtk_tree_view_node_deleted,
					 tree_view);
	  _gtk_rbtree_free (tree_view->priv->tree);
	}

      g_list_free (tree_view->priv->column);
      tree_view->priv->column = NULL;
      GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_MODEL_SETUP);
    }

  tree_view->priv->model = model;
  if (model == NULL)
    {
      tree_view->priv->tree = NULL;
      tree_view->priv->columns = 0;
      tree_view->priv->column = NULL;
      if (GTK_WIDGET_REALIZED (tree_view))
	_gtk_tree_view_set_size (tree_view, 0, 0);
      return;
    }

  if (GTK_WIDGET_REALIZED (tree_view))
    {
      gtk_tree_view_set_model_realized (tree_view);
      _gtk_tree_view_set_size (tree_view, -1, -1);
    }
}

GtkTreeSelection *
gtk_tree_view_get_selection (GtkTreeView *tree_view)
{
  g_return_val_if_fail (tree_view != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  if (tree_view->priv->selection == NULL)
    gtk_tree_selection_new_with_tree_view (tree_view);

  return tree_view->priv->selection;
}

void
gtk_tree_view_set_selection (GtkTreeView      *tree_view,
			     GtkTreeSelection *selection)
{
  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

  g_object_ref (G_OBJECT (selection));

  if (tree_view->priv->selection != NULL)
    g_object_unref (G_OBJECT (tree_view->priv->selection));

  tree_view->priv->selection = selection;
}

GtkAdjustment *
gtk_tree_view_get_hadjustment (GtkTreeView *tree_view)
{
  g_return_val_if_fail (tree_view != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->hadjustment;
}

void
gtk_tree_view_set_hadjustment (GtkTreeView     *tree_view,
			       GtkAdjustment *adjustment)
{
  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_tree_view_set_adjustments (tree_view,
				 adjustment,
				 tree_view->priv->vadjustment);
}

GtkAdjustment *
gtk_tree_view_get_vadjustment (GtkTreeView *tree_view)
{
  g_return_val_if_fail (tree_view != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->vadjustment;
}

void
gtk_tree_view_set_vadjustment (GtkTreeView     *tree_view,
			       GtkAdjustment *adjustment)
{
  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_tree_view_set_adjustments (tree_view,
				 tree_view->priv->hadjustment,
				 adjustment);
}

static void
gtk_tree_view_set_adjustments (GtkTreeView     *tree_view,
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

gboolean
gtk_tree_view_get_headers_visible (GtkTreeView *tree_view)
{
  g_return_val_if_fail (tree_view != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE);
}

void
gtk_tree_view_set_headers_visible (GtkTreeView *tree_view,
				   gboolean   headers_visible)
{
  gint x, y;
  GList *list;
  GtkTreeViewColumn *column;

  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

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
	  for (list = tree_view->priv->column; list; list = list->next)
	    {
	      column = list->data;
	      gtk_widget_map (column->button);
	    }

	  for (list = tree_view->priv->column; list; list = list->next)
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
      else
	{
	  gdk_window_move_resize (tree_view->priv->bin_window, x, y, tree_view->priv->width, tree_view->priv->height);
	  for (list = tree_view->priv->column; list; list = list->next)
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


void
gtk_tree_view_columns_autosize (GtkTreeView *tree_view)
{
  gboolean dirty = FALSE;
  GList *list;
  GtkTreeViewColumn *column;

  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  for (list = tree_view->priv->column; list; list = list->next)
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

void
gtk_tree_view_set_headers_active (GtkTreeView *tree_view,
				  gboolean   active)
{
  GList *list;

  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (tree_view->priv->model != NULL);

  for (list = tree_view->priv->column; list; list = list->next)
    gtk_tree_view_column_set_header_active (GTK_TREE_VIEW_COLUMN (list->data), active);
}

gint
gtk_tree_view_add_column (GtkTreeView   *tree_view,
			  GtkTreeViewColumn *column)
{
  g_return_val_if_fail (tree_view != NULL, -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);
  g_return_val_if_fail (column != NULL, -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), -1);
  g_return_val_if_fail (column->tree_view == NULL, -1);

  tree_view->priv->column = g_list_append (tree_view->priv->column,
					   column);
  column->tree_view = GTK_WIDGET (tree_view);
  return tree_view->priv->columns++;
}

GtkTreeViewColumn *
gtk_tree_view_get_column (GtkTreeView *tree_view,
			  gint       n)
{
  g_return_val_if_fail (tree_view != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);
  g_return_val_if_fail (tree_view->priv->model != NULL, NULL);
  g_return_val_if_fail (n >= 0 || n < tree_view->priv->columns, NULL);

  if (tree_view->priv->column == NULL)
    return NULL;

  return GTK_TREE_VIEW_COLUMN (g_list_nth (tree_view->priv->column, n)->data);
}

void
gtk_tree_view_move_to (GtkTreeView *tree_view,
		       GtkTreePath *path,
		       gint       column,
		       gfloat     row_align,
		       gfloat     col_align)
{
  GtkRBNode *node = NULL;
  GtkRBTree *tree = NULL;

  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (column < -1 || column > tree_view->priv->columns)
    return;

  row_align = CLAMP (row_align, 0, 1);
  col_align = CLAMP (col_align, 0, 1);

  if (path != NULL)
    {
      _gtk_tree_view_find_node (tree_view, path,
				&tree, &node);
      if (node == NULL)
	return;
    }

  if (tree_view->priv->hadjustment && column >= 0)
    {
      GtkTreeViewColumn *col;

      col = g_list_nth (tree_view->priv->column, column)->data;
      /* FIXME -- write  */
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
      GtkTreeNode tree_node;

      node->children = _gtk_rbtree_new ();
      node->children->parent_tree = tree;
      node->children->parent_node = node;
      path = _gtk_tree_view_find_path (tree_view, tree, node);
      tree_node = gtk_tree_model_get_node (tree_view->priv->model, path);
      tree_node = gtk_tree_model_node_children (tree_view->priv->model, tree_node);
      gtk_tree_view_build_tree (tree_view,
				node->children,
				tree_node,
				gtk_tree_path_get_depth (path) + 1,
				TRUE,
				GTK_WIDGET_REALIZED (tree_view));
      gtk_tree_path_free (path);
    }
}

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
				   gpointer  data)
{
  if (node->children)
    {
      GtkTreePath *path;
      GtkTreeNode *tree_node;

      path = _gtk_tree_view_find_path (GTK_TREE_VIEW (data),
				       node->children,
				       node->children->root);
      tree_node = gtk_tree_model_get_node (GTK_TREE_VIEW (data)->priv->model, path);
      gtk_tree_view_discover_dirty (GTK_TREE_VIEW (data),
				    node->children,
				    tree_node,
				    gtk_tree_path_get_depth (path));
      _gtk_rbtree_remove (node->children);
      gtk_tree_path_free (path);
    }
}

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

  if (GTK_WIDGET_REALIZED (tree_view))
    gtk_widget_queue_draw (GTK_WIDGET (tree_view));
}
