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
#include "gtkmarshalers.h"
#include "gtkbutton.h"
#include "gtkalignment.h"
#include "gtklabel.h"
#include "gtkhbox.h"
#include "gtkarrow.h"
#include "gtkintl.h"
#include "gtkbindings.h"
#include "gtkcontainer.h"
#include "gtkentry.h"
#include "gtktreemodelsort.h"

#include <string.h>
#include <gdk/gdkkeysyms.h>

#define GTK_TREE_VIEW_SEARCH_DIALOG_KEY "gtk-tree-view-search-dialog"
#define GTK_TREE_VIEW_PRIORITY_VALIDATE (GDK_PRIORITY_REDRAW + 5)
#define GTK_TREE_VIEW_NUM_ROWS_PER_IDLE 50
#define SCROLL_EDGE_SIZE 15
#define EXPANDER_EXTRA_PADDING 4

/* The "background" areas of all rows/cells add up to cover the entire tree.
 * The background includes all inter-row and inter-cell spacing.
 * The "cell" areas are the cell_area passed in to gtk_cell_renderer_render(),
 * i.e. just the cells, no spacing.
 */

#define BACKGROUND_HEIGHT(node) (GTK_RBNODE_GET_HEIGHT (node))
#define CELL_HEIGHT(node, separator) (BACKGROUND_HEIGHT (node) - separator);

#define TREE_WINDOW_Y_TO_RBTREE_Y(tree_view,y) ((y) + tree_view->priv->dy)
#define RBTREE_Y_TO_TREE_WINDOW_Y(tree_view,y) ((y) - tree_view->priv->dy)

/* This is in Window coordinates */
#define BACKGROUND_FIRST_PIXEL(tree_view,tree,node) (RBTREE_Y_TO_TREE_WINDOW_Y (tree_view, _gtk_rbtree_node_find_offset ((tree), (node))))
#define CELL_FIRST_PIXEL(tree_view,tree,node,separator) (BACKGROUND_FIRST_PIXEL (tree_view,tree,node) + separator/2)


typedef struct _GtkTreeViewChild GtkTreeViewChild;
struct _GtkTreeViewChild
{
  GtkWidget *widget;
  gint x;
  gint y;
  gint width;
  gint height;
};


typedef struct _TreeViewDragInfo TreeViewDragInfo;
struct _TreeViewDragInfo
{
  GdkModifierType start_button_mask;
  GtkTargetList *source_target_list;
  GdkDragAction source_actions;

  GtkTargetList *dest_target_list;

  guint source_set : 1;
  guint dest_set : 1;
};


/* Signals */
enum
{
  ROW_ACTIVATED,
  TEST_EXPAND_ROW,
  TEST_COLLAPSE_ROW,
  ROW_EXPANDED,
  ROW_COLLAPSED,
  COLUMNS_CHANGED,
  CURSOR_CHANGED,
  MOVE_CURSOR,
  SELECT_ALL,
  SELECT_CURSOR_ROW,
  TOGGLE_CURSOR_ROW,
  EXPAND_COLLAPSE_CURSOR_ROW,
  SELECT_CURSOR_PARENT,
  START_INTERACTIVE_SEARCH,
  LAST_SIGNAL
};

/* Properties */
enum {
  PROP_0,
  PROP_MODEL,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HEADERS_VISIBLE,
  PROP_HEADERS_CLICKABLE,
  PROP_EXPANDER_COLUMN,
  PROP_REORDERABLE,
  PROP_RULES_HINT,
  PROP_ENABLE_SEARCH,
  PROP_SEARCH_COLUMN,
};

static void     gtk_tree_view_class_init           (GtkTreeViewClass *klass);
static void     gtk_tree_view_init                 (GtkTreeView      *tree_view);

/* object signals */
static void     gtk_tree_view_finalize             (GObject          *object);
static void     gtk_tree_view_set_property         (GObject         *object,
						    guint            prop_id,
						    const GValue    *value,
						    GParamSpec      *pspec);
static void     gtk_tree_view_get_property         (GObject         *object,
						    guint            prop_id,
						    GValue          *value,
						    GParamSpec      *pspec);

/* gtkobject signals */
static void     gtk_tree_view_destroy              (GtkObject        *object);

/* gtkwidget signals */
static void     gtk_tree_view_realize              (GtkWidget        *widget);
static void     gtk_tree_view_unrealize            (GtkWidget        *widget);
static void     gtk_tree_view_map                  (GtkWidget        *widget);
static void     gtk_tree_view_size_request         (GtkWidget        *widget,
						    GtkRequisition   *requisition);
static void     gtk_tree_view_size_allocate        (GtkWidget        *widget,
						    GtkAllocation    *allocation);
static gboolean gtk_tree_view_expose               (GtkWidget        *widget,
						    GdkEventExpose   *event);
static gboolean gtk_tree_view_key_press            (GtkWidget        *widget,
						    GdkEventKey      *event);
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
static void     gtk_tree_view_set_focus_child      (GtkContainer     *container,
						    GtkWidget        *child);
static gint     gtk_tree_view_focus_in             (GtkWidget        *widget,
						    GdkEventFocus    *event);
static gint     gtk_tree_view_focus_out            (GtkWidget        *widget,
						    GdkEventFocus    *event);
static gint     gtk_tree_view_focus                (GtkWidget        *widget,
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
static void gtk_tree_view_set_adjustments                 (GtkTreeView     *tree_view,
							   GtkAdjustment   *hadj,
							   GtkAdjustment   *vadj);
static void gtk_tree_view_real_move_cursor                (GtkTreeView     *tree_view,
							   GtkMovementStep  step,
							   gint             count);
static void gtk_tree_view_real_select_all                 (GtkTreeView     *tree_view);
static void gtk_tree_view_real_select_cursor_row          (GtkTreeView     *tree_view,
							   gboolean         start_editing);
static void gtk_tree_view_real_toggle_cursor_row          (GtkTreeView     *tree_view);
static void gtk_tree_view_real_expand_collapse_cursor_row (GtkTreeView     *tree_view,
							   gboolean         logical,
							   gboolean         expand,
							   gboolean         open_all);
static void gtk_tree_view_real_select_cursor_parent       (GtkTreeView     *tree_view);
static void gtk_tree_view_row_changed                     (GtkTreeModel    *model,
							   GtkTreePath     *path,
							   GtkTreeIter     *iter,
							   gpointer         data);
static void gtk_tree_view_row_inserted                    (GtkTreeModel    *model,
							   GtkTreePath     *path,
							   GtkTreeIter     *iter,
							   gpointer         data);
static void gtk_tree_view_row_has_child_toggled           (GtkTreeModel    *model,
							   GtkTreePath     *path,
							   GtkTreeIter     *iter,
							   gpointer         data);
static void gtk_tree_view_row_deleted                     (GtkTreeModel    *model,
							   GtkTreePath     *path,
							   gpointer         data);
static void gtk_tree_view_rows_reordered                  (GtkTreeModel    *model,
							   GtkTreePath     *parent,
							   GtkTreeIter     *iter,
							   gint            *new_order,
							   gpointer         data);

/* Incremental reflow */
static gboolean validate_row             (GtkTreeView *tree_view,
					  GtkRBTree   *tree,
					  GtkRBNode   *node,
					  GtkTreeIter *iter,
					  GtkTreePath *path);
static void     validate_visible_area    (GtkTreeView *tree_view);
static gboolean validate_rows_handler    (GtkTreeView *tree_view);
static gboolean presize_handler_callback (gpointer     data);
static void     install_presize_handler  (GtkTreeView *tree_view);
static void	gtk_tree_view_dy_to_top_row (GtkTreeView *tree_view);
static void	gtk_tree_view_top_row_to_dy (GtkTreeView *tree_view);


/* Internal functions */
static gboolean gtk_tree_view_is_expander_column             (GtkTreeView       *tree_view,
							      GtkTreeViewColumn *column);
static void     gtk_tree_view_add_move_binding               (GtkBindingSet     *binding_set,
							      guint              keyval,
							      guint              modmask,
							      GtkMovementStep    step,
							      gint               count);
static gint     gtk_tree_view_unref_and_check_selection_tree (GtkTreeView       *tree_view,
							      GtkRBTree         *tree);
static void     gtk_tree_view_queue_draw_path                (GtkTreeView       *tree_view,
							      GtkTreePath       *path,
							      GdkRectangle      *clip_rect);
static void     gtk_tree_view_queue_draw_arrow               (GtkTreeView       *tree_view,
							      GtkRBTree         *tree,
							      GtkRBNode         *node,
							      GdkRectangle      *clip_rect);
static void     gtk_tree_view_draw_arrow                     (GtkTreeView       *tree_view,
							      GtkRBTree         *tree,
							      GtkRBNode         *node,
							      gint               x,
							      gint               y);
static void     gtk_tree_view_get_arrow_xrange               (GtkTreeView       *tree_view,
							      GtkRBTree         *tree,
							      gint              *x1,
							      gint              *x2);
static gint     gtk_tree_view_new_column_width               (GtkTreeView       *tree_view,
							      gint               i,
							      gint              *x);
static void     gtk_tree_view_adjustment_changed             (GtkAdjustment     *adjustment,
							      GtkTreeView       *tree_view);
static void     gtk_tree_view_build_tree                     (GtkTreeView       *tree_view,
							      GtkRBTree         *tree,
							      GtkTreeIter       *iter,
							      gint               depth,
							      gboolean           recurse);
static gboolean gtk_tree_view_discover_dirty_iter            (GtkTreeView       *tree_view,
							      GtkTreeIter       *iter,
							      gint               depth,
							      gint              *height,
							      GtkRBNode         *node);
static void     gtk_tree_view_discover_dirty                 (GtkTreeView       *tree_view,
							      GtkRBTree         *tree,
							      GtkTreeIter       *iter,
							      gint               depth);
static void     gtk_tree_view_clamp_node_visible             (GtkTreeView       *tree_view,
							      GtkRBTree         *tree,
							      GtkRBNode         *node);
static void     gtk_tree_view_clamp_column_visible           (GtkTreeView       *tree_view,
							      GtkTreeViewColumn *column);
static gboolean gtk_tree_view_maybe_begin_dragging_row       (GtkTreeView       *tree_view,
							      GdkEventMotion    *event);
static void     gtk_tree_view_focus_to_cursor                (GtkTreeView       *tree_view);
static void     gtk_tree_view_move_cursor_up_down            (GtkTreeView       *tree_view,
							      gint               count);
static void     gtk_tree_view_move_cursor_page_up_down       (GtkTreeView       *tree_view,
							      gint               count);
static void     gtk_tree_view_move_cursor_left_right         (GtkTreeView       *tree_view,
							      gint               count);
static void     gtk_tree_view_move_cursor_start_end          (GtkTreeView       *tree_view,
							      gint               count);
static gboolean gtk_tree_view_real_collapse_row              (GtkTreeView       *tree_view,
							      GtkTreePath       *path,
							      GtkRBTree         *tree,
							      GtkRBNode         *node,
							      gboolean           animate);
static gboolean gtk_tree_view_real_expand_row                (GtkTreeView       *tree_view,
							      GtkTreePath       *path,
							      GtkRBTree         *tree,
							      GtkRBNode         *node,
							      gboolean           open_all,
							      gboolean           animate);
static void     gtk_tree_view_real_set_cursor                (GtkTreeView       *tree_view,
							      GtkTreePath       *path,
							      gboolean           clear_and_select);

/* interactive search */
static void     gtk_tree_view_search_dialog_destroy     (GtkWidget        *search_dialog,
							 GtkTreeView      *tree_view);
static void     gtk_tree_view_search_position_func      (GtkTreeView      *tree_view,
							 GtkWidget        *search_dialog);
static gboolean gtk_tree_view_search_delete_event       (GtkWidget        *widget,
							 GdkEventAny      *event,
							 GtkTreeView      *tree_view);
static gboolean gtk_tree_view_search_button_press_event (GtkWidget        *widget,
							 GdkEventButton   *event,
							 GtkTreeView      *tree_view);
static gboolean gtk_tree_view_search_key_press_event    (GtkWidget        *entry,
							 GdkEventKey      *event,
							 GtkTreeView      *tree_view);
static void     gtk_tree_view_search_move               (GtkWidget        *window,
							 GtkTreeView      *tree_view,
							 gboolean          up);
static gboolean gtk_tree_view_search_equal_func         (GtkTreeModel     *model,
							 gint              column,
							 const gchar      *key,
							 GtkTreeIter      *iter,
							 gpointer          search_data);
static gboolean gtk_tree_view_search_iter               (GtkTreeModel     *model,
							 GtkTreeSelection *selection,
							 GtkTreeIter      *iter,
							 const gchar      *text,
							 gint             *count,
							 gint              n);
static void     gtk_tree_view_search_init               (GtkWidget        *entry,
							 GtkTreeView      *tree_view);
static void     gtk_tree_view_put                       (GtkTreeView      *tree_view,
							 GtkWidget        *child_widget,
							 gint              x,
							 gint              y,
							 gint              width,
							 gint              height);
static gboolean gtk_tree_view_start_editing             (GtkTreeView      *tree_view,
							 GtkTreePath      *cursor_path);
static void gtk_tree_view_real_start_editing (GtkTreeView       *tree_view,
					      GtkTreeViewColumn *column,
					      GtkTreePath       *path,
					      GtkCellEditable   *cell_editable,
					      GdkRectangle      *cell_area,
					      GdkEvent          *event,
					      guint              flags);
static void gtk_tree_view_stop_editing                  (GtkTreeView *tree_view,
							 gboolean     cancel_editing);
static void gtk_tree_view_real_start_interactive_search (GtkTreeView *tree_view);


static GtkContainerClass *parent_class = NULL;
static guint tree_view_signals [LAST_SIGNAL] = { 0 };



/* GType Methods
 */

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
  GtkBindingSet *binding_set;

  parent_class = g_type_class_peek_parent (class);
  binding_set = gtk_binding_set_by_class (class);

  o_class = (GObjectClass *) class;
  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  /* GObject signals */
  o_class->set_property = gtk_tree_view_set_property;
  o_class->get_property = gtk_tree_view_get_property;
  o_class->finalize = gtk_tree_view_finalize;

  /* GtkObject signals */
  object_class->destroy = gtk_tree_view_destroy;

  /* GtkWidget signals */
  widget_class->map = gtk_tree_view_map;
  widget_class->realize = gtk_tree_view_realize;
  widget_class->unrealize = gtk_tree_view_unrealize;
  widget_class->size_request = gtk_tree_view_size_request;
  widget_class->size_allocate = gtk_tree_view_size_allocate;
  widget_class->button_press_event = gtk_tree_view_button_press;
  widget_class->button_release_event = gtk_tree_view_button_release;
  widget_class->motion_notify_event = gtk_tree_view_motion;
  widget_class->expose_event = gtk_tree_view_expose;
  widget_class->key_press_event = gtk_tree_view_key_press;
  widget_class->enter_notify_event = gtk_tree_view_enter_notify;
  widget_class->leave_notify_event = gtk_tree_view_leave_notify;
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
  widget_class->focus = gtk_tree_view_focus;

  /* GtkContainer signals */
  container_class->remove = gtk_tree_view_remove;
  container_class->forall = gtk_tree_view_forall;
  container_class->set_focus_child = gtk_tree_view_set_focus_child;

  class->set_scroll_adjustments = gtk_tree_view_set_adjustments;
  class->move_cursor = gtk_tree_view_real_move_cursor;
  class->select_all = gtk_tree_view_real_select_all;
  class->select_cursor_row = gtk_tree_view_real_select_cursor_row;
  class->toggle_cursor_row = gtk_tree_view_real_toggle_cursor_row;
  class->expand_collapse_cursor_row = gtk_tree_view_real_expand_collapse_cursor_row;
  class->select_cursor_parent = gtk_tree_view_real_select_cursor_parent;
  class->start_interactive_search = gtk_tree_view_real_start_interactive_search;

  /* Properties */

  g_object_class_install_property (o_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
							_("TreeView Model"),
							_("The model for the tree view"),
							GTK_TYPE_TREE_MODEL,
							G_PARAM_READWRITE));

  g_object_class_install_property (o_class,
                                   PROP_HADJUSTMENT,
                                   g_param_spec_object ("hadjustment",
							_("Horizontal Adjustment"),
                                                        _("Horizontal Adjustment for the widget"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (o_class,
                                   PROP_VADJUSTMENT,
                                   g_param_spec_object ("vadjustment",
							_("Vertical Adjustment"),
                                                        _("Vertical Adjustment for the widget"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (o_class,
                                   PROP_HEADERS_VISIBLE,
                                   g_param_spec_boolean ("headers_visible",
							 _("Visible"),
							 _("Show the column header buttons"),
							 FALSE,
							 G_PARAM_READWRITE));

  g_object_class_install_property (o_class,
                                   PROP_HEADERS_CLICKABLE,
                                   g_param_spec_boolean ("headers_clickable",
							 _("Headers Clickable"),
							 _("Column headers respond to click events"),
							 FALSE,
							 G_PARAM_WRITABLE));

  g_object_class_install_property (o_class,
                                   PROP_EXPANDER_COLUMN,
                                   g_param_spec_object ("expander_column",
							_("Expander Column"),
							_("Set the column for the expander column"),
							GTK_TYPE_TREE_VIEW_COLUMN,
							G_PARAM_READWRITE));

  g_object_class_install_property (o_class,
                                   PROP_REORDERABLE,
                                   g_param_spec_boolean ("reorderable",
							 _("Reorderable"),
							 _("View is reorderable"),
							 FALSE,
							 G_PARAM_READWRITE));

  g_object_class_install_property (o_class,
                                   PROP_RULES_HINT,
                                   g_param_spec_boolean ("rules_hint",
							 _("Rules Hint"),
							 _("Set a hint to the theme engine to draw rows in alternating colors"),
							 FALSE,
							 G_PARAM_READWRITE));

    g_object_class_install_property (o_class,
				     PROP_ENABLE_SEARCH,
				     g_param_spec_boolean ("enable_search",
							   _("Enable Search"),
							   _("View allows user to search through columns interactively"),
							   TRUE,
							   G_PARAM_READWRITE));

    g_object_class_install_property (o_class,
				     PROP_SEARCH_COLUMN,
				     g_param_spec_int ("search_column",
						       _("Search Column"),
						       _("Model column to search through when searching through code"),
						       -1,
						       G_MAXINT,
						       0,
						       G_PARAM_READWRITE));

  /* Style properties */
#define _TREE_VIEW_EXPANDER_SIZE 10
#define _TREE_VIEW_VERTICAL_SEPARATOR 2
#define _TREE_VIEW_HORIZONTAL_SEPARATOR 2
    
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("expander_size",
							     _("Expander Size"),
							     _("Size of the expander arrow."),
							     0,
							     G_MAXINT,
							     _TREE_VIEW_EXPANDER_SIZE,
							     G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("vertical_separator",
							     _("Vertical Separator Width"),
							     _("Vertical space between cells.  Must be an even number."),
							     0,
							     G_MAXINT,
							     _TREE_VIEW_VERTICAL_SEPARATOR,
							     G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("horizontal_separator",
							     _("Horizontal Separator Width"),
							     _("Horizontal space between cells.  Must be an even number."),
							     0,
							     G_MAXINT,
							     _TREE_VIEW_HORIZONTAL_SEPARATOR,
							     G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("allow_rules",
								 _("Allow Rules"),
								 _("Allow drawing of alternating color rows."),
								 TRUE,
								 G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("indent_expanders",
								 _("Indent Expanders"),
								 _("Make the expanders indented."),
								 TRUE,
								 G_PARAM_READABLE));
  /* Signals */
  widget_class->set_scroll_adjustments_signal =
    gtk_signal_new ("set_scroll_adjustments",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTreeViewClass, set_scroll_adjustments),
		    _gtk_marshal_VOID__OBJECT_OBJECT,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);

  tree_view_signals[ROW_ACTIVATED] =
    gtk_signal_new ("row_activated",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTreeViewClass, row_activated),
		    _gtk_marshal_VOID__BOXED_OBJECT,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_TREE_PATH,
		    GTK_TYPE_TREE_VIEW_COLUMN);

  tree_view_signals[TEST_EXPAND_ROW] =
    g_signal_new ("test_expand_row",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTreeViewClass, test_expand_row),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__BOXED_BOXED,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_TREE_ITER,
                  GTK_TYPE_TREE_PATH);

  tree_view_signals[TEST_COLLAPSE_ROW] =
    g_signal_new ("test_collapse_row",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTreeViewClass, test_collapse_row),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__BOXED_BOXED,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_TREE_ITER,
                  GTK_TYPE_TREE_PATH);

  tree_view_signals[ROW_EXPANDED] =
    g_signal_new ("row_expanded",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTreeViewClass, row_expanded),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOXED_BOXED,
                  GTK_TYPE_NONE, 2,
                  GTK_TYPE_TREE_ITER,
                  GTK_TYPE_TREE_PATH);

  tree_view_signals[ROW_COLLAPSED] =
    g_signal_new ("row_collapsed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTreeViewClass, row_collapsed),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOXED_BOXED,
                  GTK_TYPE_NONE, 2,
                  GTK_TYPE_TREE_ITER,
                  GTK_TYPE_TREE_PATH);

  tree_view_signals[COLUMNS_CHANGED] =
    g_signal_new ("columns_changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTreeViewClass, columns_changed),
                  NULL, NULL,
                  _gtk_marshal_NONE__NONE,
                  G_TYPE_NONE, 0);

  tree_view_signals[CURSOR_CHANGED] =
    g_signal_new ("cursor_changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTreeViewClass, cursor_changed),
                  NULL, NULL,
                  _gtk_marshal_NONE__NONE,
                  G_TYPE_NONE, 0);

  tree_view_signals[MOVE_CURSOR] =
    g_signal_new ("move_cursor",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | GTK_RUN_ACTION,
                  G_STRUCT_OFFSET (GtkTreeViewClass, move_cursor),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM_INT,
                  GTK_TYPE_NONE, 2, GTK_TYPE_MOVEMENT_STEP, GTK_TYPE_INT);

  tree_view_signals[SELECT_ALL] =
    g_signal_new ("select_all",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | GTK_RUN_ACTION,
                  G_STRUCT_OFFSET (GtkTreeViewClass, select_all),
                  NULL, NULL,
                  _gtk_marshal_NONE__NONE,
                  GTK_TYPE_NONE, 0);

  tree_view_signals[SELECT_CURSOR_ROW] =
    g_signal_new ("select_cursor_row",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | GTK_RUN_ACTION,
                  G_STRUCT_OFFSET (GtkTreeViewClass, select_cursor_row),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOOLEAN,
                  GTK_TYPE_NONE, 1,
		  G_TYPE_BOOLEAN);

  tree_view_signals[TOGGLE_CURSOR_ROW] =
    g_signal_new ("toggle_cursor_row",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | GTK_RUN_ACTION,
                  G_STRUCT_OFFSET (GtkTreeViewClass, toggle_cursor_row),
                  NULL, NULL,
                  _gtk_marshal_NONE__NONE,
                  GTK_TYPE_NONE, 0);

  tree_view_signals[EXPAND_COLLAPSE_CURSOR_ROW] =
    g_signal_new ("expand_collapse_cursor_row",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | GTK_RUN_ACTION,
                  G_STRUCT_OFFSET (GtkTreeViewClass, expand_collapse_cursor_row),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOOLEAN_BOOLEAN_BOOLEAN,
                  GTK_TYPE_NONE, 3, GTK_TYPE_BOOL, GTK_TYPE_BOOL, GTK_TYPE_BOOL);

  tree_view_signals[SELECT_CURSOR_PARENT] =
    g_signal_new ("select_cursor_parent",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | GTK_RUN_ACTION,
                  G_STRUCT_OFFSET (GtkTreeViewClass, select_cursor_parent),
                  NULL, NULL,
                  _gtk_marshal_NONE__NONE,
                  GTK_TYPE_NONE, 0);

  tree_view_signals[START_INTERACTIVE_SEARCH] =
    g_signal_new ("start_interactive_search",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | GTK_RUN_ACTION,
                  G_STRUCT_OFFSET (GtkTreeViewClass, start_interactive_search),
                  NULL, NULL,
                  _gtk_marshal_NONE__NONE,
                  GTK_TYPE_NONE, 0);

  /* Key bindings */
  gtk_tree_view_add_move_binding (binding_set, GDK_Up, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);

  gtk_tree_view_add_move_binding (binding_set, GDK_Down, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);

  gtk_tree_view_add_move_binding (binding_set, GDK_p, GDK_CONTROL_MASK,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);

  gtk_tree_view_add_move_binding (binding_set, GDK_n, GDK_CONTROL_MASK,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);

  gtk_tree_view_add_move_binding (binding_set, GDK_Home, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, -1);

  gtk_tree_view_add_move_binding (binding_set, GDK_End, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, 1);

  gtk_tree_view_add_move_binding (binding_set, GDK_Page_Up, 0,
				  GTK_MOVEMENT_PAGES, -1);

  gtk_tree_view_add_move_binding (binding_set, GDK_Page_Down, 0,
				  GTK_MOVEMENT_PAGES, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_Right, 0, "move_cursor", 2,
				GTK_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_Left, 0, "move_cursor", 2,
				GTK_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				GTK_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_Right, GDK_CONTROL_MASK, "move_cursor", 2,
				GTK_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_Left, GDK_CONTROL_MASK, "move_cursor", 2,
				GTK_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				GTK_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_Right, GDK_CONTROL_MASK|GDK_SHIFT_MASK, "move_cursor", 2,
				GTK_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_Left, GDK_CONTROL_MASK|GDK_SHIFT_MASK, "move_cursor", 2,
				GTK_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				GTK_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_f, GDK_CONTROL_MASK, "move_cursor", 2,
				GTK_TYPE_ENUM, GTK_MOVEMENT_LOGICAL_POSITIONS,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_b, GDK_CONTROL_MASK, "move_cursor", 2,
				GTK_TYPE_ENUM, GTK_MOVEMENT_LOGICAL_POSITIONS,
				GTK_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_CONTROL_MASK, "toggle_cursor_row", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_a, GDK_CONTROL_MASK, "select_all", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_SHIFT_MASK, "select_cursor_row", 1,
				GTK_TYPE_BOOL, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_space, 0, "select_cursor_row", 1,
				GTK_TYPE_BOOL, TRUE);

  /* expand and collapse rows */
  gtk_binding_entry_add_signal (binding_set, GDK_plus, 0, "expand_collapse_cursor_row", 3,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, TRUE,
				GTK_TYPE_BOOL, FALSE);
  /* Not doable on US keyboards */
  gtk_binding_entry_add_signal (binding_set, GDK_plus, GDK_SHIFT_MASK, "expand_collapse_cursor_row", 3,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, TRUE,
				GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Add, 0, "expand_collapse_cursor_row", 3,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, TRUE,
				GTK_TYPE_BOOL, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Add, GDK_SHIFT_MASK, "expand_collapse_cursor_row", 3,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, TRUE,
				GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Add, GDK_SHIFT_MASK, "expand_collapse_cursor_row", 3,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, TRUE,
				GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Right, GDK_SHIFT_MASK, "expand_collapse_cursor_row", 3,
				GTK_TYPE_BOOL, TRUE,
				GTK_TYPE_BOOL, TRUE,
				GTK_TYPE_BOOL, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_minus, 0, "expand_collapse_cursor_row", 3,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_minus, GDK_SHIFT_MASK, "expand_collapse_cursor_row", 3,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Subtract, 0, "expand_collapse_cursor_row", 3,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Subtract, GDK_SHIFT_MASK, "expand_collapse_cursor_row", 3,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Left, GDK_SHIFT_MASK, "expand_collapse_cursor_row", 3,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, FALSE,
				GTK_TYPE_BOOL, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, 0, "select_cursor_parent", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_s, GDK_CONTROL_MASK, "start_interactive_search", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_S, GDK_CONTROL_MASK, "start_interactive_search", 0);
}

static void
gtk_tree_view_init (GtkTreeView *tree_view)
{
  tree_view->priv = g_new0 (GtkTreeViewPrivate, 1);
  GTK_WIDGET_SET_FLAGS (tree_view, GTK_CAN_FOCUS);

  tree_view->priv->flags = GTK_TREE_VIEW_IS_LIST | GTK_TREE_VIEW_SHOW_EXPANDERS | GTK_TREE_VIEW_DRAW_KEYFOCUS | GTK_TREE_VIEW_HEADERS_VISIBLE;
  gtk_widget_style_get (GTK_WIDGET (tree_view), "expander_size", &tree_view->priv->tab_offset, NULL);

  /* We need some padding */
  tree_view->priv->tab_offset += EXPANDER_EXTRA_PADDING;
  tree_view->priv->dy = 0;
  tree_view->priv->n_columns = 0;
  tree_view->priv->header_height = 1;
  tree_view->priv->x_drag = 0;
  tree_view->priv->drag_pos = -1;
  tree_view->priv->header_has_focus = FALSE;
  tree_view->priv->pressed_button = -1;
  tree_view->priv->press_start_x = -1;
  tree_view->priv->press_start_y = -1;
  tree_view->priv->reorderable = FALSE;
  tree_view->priv->presize_handler_timer = 0;
  gtk_tree_view_set_adjustments (tree_view, NULL, NULL);
  tree_view->priv->selection = _gtk_tree_selection_new_with_tree_view (tree_view);
  tree_view->priv->enable_search = TRUE;
  tree_view->priv->search_column = -1;
  tree_view->priv->search_dialog_position_func = gtk_tree_view_search_position_func;
  tree_view->priv->search_equal_func = gtk_tree_view_search_equal_func;
}



/* GObject Methods
 */

static void
gtk_tree_view_set_property (GObject         *object,
			    guint            prop_id,
			    const GValue    *value,
			    GParamSpec      *pspec)
{
  GtkTreeView *tree_view;

  tree_view = GTK_TREE_VIEW (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_tree_view_set_model (tree_view, g_value_get_object (value));
      break;
    case PROP_HADJUSTMENT:
      gtk_tree_view_set_hadjustment (tree_view, g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      gtk_tree_view_set_vadjustment (tree_view, g_value_get_object (value));
      break;
    case PROP_HEADERS_VISIBLE:
      gtk_tree_view_set_headers_visible (tree_view, g_value_get_boolean (value));
      break;
    case PROP_HEADERS_CLICKABLE:
      gtk_tree_view_set_headers_clickable (tree_view, g_value_get_boolean (value));
      break;
    case PROP_EXPANDER_COLUMN:
      gtk_tree_view_set_expander_column (tree_view, g_value_get_object (value));
      break;
    case PROP_REORDERABLE:
      gtk_tree_view_set_reorderable (tree_view, g_value_get_boolean (value));
      break;
    case PROP_RULES_HINT:
      gtk_tree_view_set_rules_hint (tree_view, g_value_get_boolean (value));
    case PROP_ENABLE_SEARCH:
      gtk_tree_view_set_enable_search (tree_view, g_value_get_boolean (value));
      break;
    case PROP_SEARCH_COLUMN:
      gtk_tree_view_set_search_column (tree_view, g_value_get_int (value));
      break;
    default:
      break;
    }
}

static void
gtk_tree_view_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  GtkTreeView *tree_view;

  tree_view = GTK_TREE_VIEW (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, tree_view->priv->model);
      break;
    case PROP_HADJUSTMENT:
      g_value_set_object (value, tree_view->priv->hadjustment);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, tree_view->priv->vadjustment);
      break;
    case PROP_HEADERS_VISIBLE:
      g_value_set_boolean (value, gtk_tree_view_get_headers_visible (tree_view));
      break;
    case PROP_EXPANDER_COLUMN:
      g_value_set_object (value, tree_view->priv->expander_column);
      break;
    case PROP_REORDERABLE:
      g_value_set_boolean (value, tree_view->priv->reorderable);
      break;
    case PROP_RULES_HINT:
      g_value_set_boolean (value, tree_view->priv->has_rules);
      break;
    case PROP_ENABLE_SEARCH:
      g_value_set_boolean (value, tree_view->priv->enable_search);
      break;
    case PROP_SEARCH_COLUMN:
      g_value_set_int (value, tree_view->priv->search_column);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_view_finalize (GObject *object)
{
  GtkTreeView *tree_view = (GtkTreeView *) object;

  g_free (tree_view->priv);

  if (G_OBJECT_CLASS (parent_class)->finalize)
    (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}



/* GtkObject Methods
 */

static void
gtk_tree_view_destroy (GtkObject *object)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (object);
  GtkWidget *search_dialog;
  GList *list;

  gtk_tree_view_stop_editing (tree_view, TRUE);

  if (tree_view->priv->columns != NULL)
    {
      list = tree_view->priv->columns;
      while (list)
	{
	  GtkTreeViewColumn *column;
	  column = GTK_TREE_VIEW_COLUMN (list->data);
	  list = list->next;
	  gtk_tree_view_remove_column (tree_view, column);
	}
      tree_view->priv->columns = NULL;
    }

  if (tree_view->priv->tree != NULL)
    {
      gtk_tree_view_unref_and_check_selection_tree (tree_view, tree_view->priv->tree);
      _gtk_rbtree_free (tree_view->priv->tree);
      tree_view->priv->tree = NULL;
    }

  if (tree_view->priv->selection != NULL)
    {
      _gtk_tree_selection_set_tree_view (tree_view->priv->selection, NULL);
      g_object_unref (tree_view->priv->selection);
      tree_view->priv->selection = NULL;
    }

  if (tree_view->priv->scroll_to_path != NULL)
    {
      gtk_tree_path_free (tree_view->priv->scroll_to_path);
      tree_view->priv->scroll_to_path = NULL;
    }

  if (tree_view->priv->drag_dest_row != NULL)
    {
      gtk_tree_row_reference_free (tree_view->priv->drag_dest_row);
      tree_view->priv->drag_dest_row = NULL;
    }

  if (tree_view->priv->top_row != NULL)
    {
      gtk_tree_row_reference_free (tree_view->priv->top_row);
      tree_view->priv->top_row = NULL;
    }

  if (tree_view->priv->column_drop_func_data &&
      tree_view->priv->column_drop_func_data_destroy)
    {
      (* tree_view->priv->column_drop_func_data_destroy) (tree_view->priv->column_drop_func_data);
      tree_view->priv->column_drop_func_data = NULL;
    }

  if (tree_view->priv->destroy_count_destroy &&
      tree_view->priv->destroy_count_data)
    {
      (* tree_view->priv->destroy_count_destroy) (tree_view->priv->destroy_count_data);
      tree_view->priv->destroy_count_data = NULL;
    }

  gtk_tree_row_reference_free (tree_view->priv->cursor);
  tree_view->priv->cursor = NULL;

  gtk_tree_row_reference_free (tree_view->priv->anchor);
  tree_view->priv->anchor = NULL;

  /* destroy interactive search dialog */
  search_dialog = gtk_object_get_data (GTK_OBJECT (tree_view),
				       GTK_TREE_VIEW_SEARCH_DIALOG_KEY);
  if (search_dialog)
    gtk_tree_view_search_dialog_destroy (search_dialog,
					 tree_view);

  if (tree_view->priv->search_user_data)
    {
      (* tree_view->priv->search_destroy) (tree_view->priv->search_user_data);
      tree_view->priv->search_user_data = NULL;
    }

  gtk_tree_view_set_model (tree_view, NULL);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* GtkWidget Methods
 */

/* GtkWidget::map helper */
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
	  if (column->resizable)
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
gtk_tree_view_realize (GtkWidget *widget)
{
  GList *tmp_list;
  GtkTreeView *tree_view;
  GdkGCValues values;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (GTK_IS_TREE_VIEW (widget));

  tree_view = GTK_TREE_VIEW (widget);

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
  attributes.y = TREE_VIEW_HEADER_HEIGHT (tree_view);
  attributes.width = tree_view->priv->width;
  attributes.height = widget->allocation.height;
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

  for (tmp_list = tree_view->priv->columns; tmp_list; tmp_list = tmp_list->next)
    _gtk_tree_view_column_realize_button (GTK_TREE_VIEW_COLUMN (tmp_list->data));

  install_presize_handler (tree_view); 
}

static void
gtk_tree_view_unrealize (GtkWidget *widget)
{
  GtkTreeView *tree_view;
  GList *list;

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

  if (tree_view->priv->expand_collapse_timeout != 0)
    {
      gtk_timeout_remove (tree_view->priv->expand_collapse_timeout);
      tree_view->priv->expand_collapse_timeout = 0;
    }
  
  if (tree_view->priv->presize_handler_timer != 0)
    {
      gtk_timeout_remove (tree_view->priv->presize_handler_timer);
      tree_view->priv->presize_handler_timer = 0;
    }

  if (tree_view->priv->validate_rows_timer != 0)
    {
      gtk_timeout_remove (tree_view->priv->validate_rows_timer);
      tree_view->priv->validate_rows_timer = 0;
    }

  for (list = tree_view->priv->columns; list; list = list->next)
    _gtk_tree_view_column_unrealize_button (GTK_TREE_VIEW_COLUMN (list->data));

  gdk_window_set_user_data (tree_view->priv->bin_window, NULL);
  gdk_window_destroy (tree_view->priv->bin_window);
  tree_view->priv->bin_window = NULL;

  gdk_window_set_user_data (tree_view->priv->header_window, NULL);
  gdk_window_destroy (tree_view->priv->header_window);
  tree_view->priv->header_window = NULL;

  if (tree_view->priv->drag_window)
    {
      gdk_window_set_user_data (tree_view->priv->drag_window, NULL);
      gdk_window_destroy (tree_view->priv->drag_window);
      tree_view->priv->drag_window = NULL;
    }

  if (tree_view->priv->drag_highlight_window)
    {
      gdk_window_set_user_data (tree_view->priv->drag_highlight_window, NULL);
      gdk_window_destroy (tree_view->priv->drag_highlight_window);
      tree_view->priv->drag_highlight_window = NULL;
    }

  /* GtkWidget::unrealize destroys children and widget->window */
  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

/* GtkWidget::size_request helper */
static void
gtk_tree_view_size_request_columns (GtkTreeView *tree_view)
{
  GList *list;

  tree_view->priv->header_height = 0;

  if (tree_view->priv->model)
    {
      for (list = tree_view->priv->columns; list; list = list->next)
        {
          GtkRequisition requisition;
          GtkTreeViewColumn *column = list->data;

	  if (column->button == NULL)
	    continue;

          column = list->data;
	  
          gtk_widget_size_request (column->button, &requisition);
	  column->button_request = requisition.width;
          tree_view->priv->header_height = MAX (tree_view->priv->header_height, requisition.height);
        }
    }
}


static void
gtk_tree_view_update_size (GtkTreeView *tree_view)
{
  GList *list;
  GtkTreeViewColumn *column;
  gint i;

  if (tree_view->priv->model == NULL)
    {
      tree_view->priv->width = 0;
      tree_view->priv->height = 0;
      return;
    }

  tree_view->priv->width = 0;
  /* keep this in sync with size_allocate below */
  for (list = tree_view->priv->columns, i = 0; list; list = list->next, i++)
    {
      gint real_requested_width = 0;
      column = list->data;
      if (!column->visible)
	continue;

      if (column->use_resized_width)
	{
	  real_requested_width = column->resized_width;
	}
      else if (column->column_type == GTK_TREE_VIEW_COLUMN_FIXED)
	{
	  real_requested_width = column->fixed_width;
	}
      else if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE))
	{
	  real_requested_width = MAX (column->requested_width, column->button_request);
	}
      else
	{
	  real_requested_width = column->requested_width;
	}

      if (column->min_width != -1)
	real_requested_width = MAX (real_requested_width, column->min_width);
      if (column->max_width != -1)
	real_requested_width = MIN (real_requested_width, column->max_width);

      tree_view->priv->width += real_requested_width;
    }

  if (tree_view->priv->tree == NULL)
    tree_view->priv->height = 0;
  else
    tree_view->priv->height = tree_view->priv->tree->root->offset;
}

static void
gtk_tree_view_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkTreeView *tree_view;
  GList *tmp_list;

  g_return_if_fail (GTK_IS_TREE_VIEW (widget));

  tree_view = GTK_TREE_VIEW (widget);

  /* we validate 50 rows initially just to make sure we have some size */
  /* in practice, with a lot of static lists, this should get a good width */
  validate_rows_handler (tree_view);
  gtk_tree_view_size_request_columns (tree_view);
  gtk_tree_view_update_size (GTK_TREE_VIEW (widget));

  requisition->width = tree_view->priv->width;
  requisition->height = tree_view->priv->height + TREE_VIEW_HEADER_HEIGHT (tree_view);

  tmp_list = tree_view->priv->children;

  while (tmp_list)
    {
      GtkTreeViewChild *child = tmp_list->data;
      GtkRequisition child_requisition;

      tmp_list = tmp_list->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
        gtk_widget_size_request (child->widget, &child_requisition);
    }
}

/* GtkWidget::size_allocate helper */
static void
gtk_tree_view_size_allocate_columns (GtkWidget *widget)
{
  GtkTreeView *tree_view;
  GList *list, *last_column;
  GtkTreeViewColumn *column;
  GtkAllocation allocation;
  gint width = 0;

  tree_view = GTK_TREE_VIEW (widget);

  for (last_column = g_list_last (tree_view->priv->columns);
       last_column && !(GTK_TREE_VIEW_COLUMN (last_column->data)->visible);
       last_column = last_column->prev)
    ;
  if (last_column == NULL)
    return;

  allocation.y = 0;
  allocation.height = tree_view->priv->header_height;

  for (list = tree_view->priv->columns; list != last_column->next; list = list->next)
    {
      gint real_requested_width = 0;
      column = list->data;
      if (!column->visible)
	continue;

      /* We need to handle the dragged button specially.
       */
      if (column == tree_view->priv->drag_column)
	{
	  GtkAllocation drag_allocation;
	  gdk_window_get_size (tree_view->priv->drag_window,
			       &(drag_allocation.width), &(drag_allocation.height));
	  drag_allocation.x = 0;
	  drag_allocation.y = 0;
	  gtk_widget_size_allocate (tree_view->priv->drag_column->button, &drag_allocation);
	  width += drag_allocation.width;
	  continue;
	}

      if (column->use_resized_width)
	{
	  real_requested_width = column->resized_width;
	}
      else if (column->column_type == GTK_TREE_VIEW_COLUMN_FIXED)
	{
	  real_requested_width = column->fixed_width;
	}
      else if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE))
	{
	  real_requested_width = MAX (column->requested_width, column->button_request);
	}
      else
	{
	  real_requested_width = column->requested_width;
	  if (real_requested_width < 0)
	    real_requested_width = 0;
	}

      if (column->min_width != -1)
	real_requested_width = MAX (real_requested_width, column->min_width);
      if (column->max_width != -1)
	real_requested_width = MIN (real_requested_width, column->max_width);

      allocation.x = width;
      column->width = real_requested_width;
      if (list == last_column &&
	  width + real_requested_width < widget->allocation.width)
	{
	  column->width += (widget->allocation.width - column->width - width);
	}
      g_object_notify (G_OBJECT (column), "width");
      allocation.width = column->width;
      width += column->width;
      gtk_widget_size_allocate (column->button, &allocation);
      if (column->window)
	gdk_window_move_resize (column->window,
                                allocation.x + allocation.width - TREE_VIEW_DRAG_WIDTH/2,
				allocation.y,
                                TREE_VIEW_DRAG_WIDTH, allocation.height);
    }
}

static void
gtk_tree_view_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GList *tmp_list;
  GtkTreeView *tree_view;

  g_return_if_fail (GTK_IS_TREE_VIEW (widget));

  widget->allocation = *allocation;

  tree_view = GTK_TREE_VIEW (widget);

  tmp_list = tree_view->priv->children;

  while (tmp_list)
    {
      GtkAllocation allocation;

      GtkTreeViewChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      /* totally ignore our childs requisition */
      allocation.x = child->x;
      allocation.y = child->y;
      allocation.width = child->width;
      allocation.height = child->height;
      gtk_widget_size_allocate (child->widget, &allocation);
    }


  tree_view->priv->hadjustment->page_size = allocation->width;
  tree_view->priv->hadjustment->page_increment = allocation->width;
  tree_view->priv->hadjustment->step_increment = allocation->width / 10;
  tree_view->priv->hadjustment->lower = 0;
  tree_view->priv->hadjustment->upper = tree_view->priv->width;

  if (tree_view->priv->hadjustment->value + allocation->width > tree_view->priv->width)
    tree_view->priv->hadjustment->value = MAX (tree_view->priv->width - allocation->width, 0);
  gtk_adjustment_changed (tree_view->priv->hadjustment);

  tree_view->priv->vadjustment->page_size = allocation->height - TREE_VIEW_HEADER_HEIGHT (tree_view);
  tree_view->priv->vadjustment->step_increment = (tree_view->priv->vadjustment->page_size) / 10;
  tree_view->priv->vadjustment->page_increment = (allocation->height - TREE_VIEW_HEADER_HEIGHT (tree_view)) / 2;
  tree_view->priv->vadjustment->lower = 0;
  tree_view->priv->vadjustment->upper = tree_view->priv->height;

  if (tree_view->priv->vadjustment->value + allocation->height > tree_view->priv->height)
    gtk_adjustment_set_value (tree_view->priv->vadjustment,
			      MAX (tree_view->priv->height - allocation->height, 0));
  gtk_adjustment_changed (tree_view->priv->vadjustment);
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      gdk_window_move_resize (tree_view->priv->header_window,
			      - (gint) tree_view->priv->hadjustment->value,
			      0,
			      MAX (tree_view->priv->width, allocation->width),
			      tree_view->priv->header_height);
      gdk_window_move_resize (tree_view->priv->bin_window,
			      - (gint) tree_view->priv->hadjustment->value,
			      TREE_VIEW_HEADER_HEIGHT (tree_view),
			      MAX (tree_view->priv->width, allocation->width),
			      allocation->height - TREE_VIEW_HEADER_HEIGHT (tree_view));
    }

  gtk_tree_view_size_allocate_columns (widget);
  
  if (tree_view->priv->scroll_to_path != NULL ||
      tree_view->priv->scroll_to_column != NULL)
    {
      gtk_tree_view_scroll_to_cell (tree_view,
				    tree_view->priv->scroll_to_path,
				    tree_view->priv->scroll_to_column,
				    tree_view->priv->scroll_to_use_align,
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

static gboolean
gtk_tree_view_button_press (GtkWidget      *widget,
			    GdkEventButton *event)
{
  GtkTreeView *tree_view;
  GList *list;
  GtkTreeViewColumn *column = NULL;
  gint i;
  GdkRectangle background_area;
  GdkRectangle cell_area;
  gint vertical_separator;
  gint horizontal_separator;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  tree_view = GTK_TREE_VIEW (widget);
  gtk_tree_view_stop_editing (tree_view, FALSE);
  gtk_widget_style_get (widget,
			"vertical_separator", &vertical_separator,
			"horizontal_separator", &horizontal_separator,
			NULL);

  if (event->window == tree_view->priv->bin_window &&
      tree_view->priv->tree != NULL)
    {
      GtkRBNode *node;
      GtkRBTree *tree;
      GtkTreePath *path;
      gchar *path_string;
      gint depth;
      gint new_y;
      gint y_offset;
      GtkTreeViewColumn *column = NULL;
      gint column_handled_click = FALSE;

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
      new_y = TREE_WINDOW_Y_TO_RBTREE_Y(tree_view, event->y);
      if (new_y < 0)
	new_y = 0;
      y_offset = -_gtk_rbtree_find_offset (tree_view->priv->tree, new_y, &tree, &node);

      if (node == NULL)
	/* We clicked in dead space */
	return TRUE;

      /* Get the path and the node */
      path = _gtk_tree_view_find_path (tree_view, tree, node);
      depth = gtk_tree_path_get_depth (path);
      background_area.y = y_offset + event->y;
      background_area.height = GTK_RBNODE_GET_HEIGHT (node);
      background_area.x = 0;

      /* Let the column have a chance at selecting it. */
      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  column = list->data;

	  if (!column->visible)
	    continue;

	  background_area.width = column->width;
	  if ((background_area.x > (gint) event->x) ||
	      (background_area.x + background_area.width <= (gint) event->x))
	    {
	      background_area.x += background_area.width;
	      continue;
	    }

	  /* we found the focus column */
	  cell_area = background_area;
	  cell_area.width -= horizontal_separator;
	  cell_area.height -= vertical_separator;
	  cell_area.x += horizontal_separator/2;
	  cell_area.y += vertical_separator/2;
	  if (gtk_tree_view_is_expander_column (tree_view, column) &&
              TREE_VIEW_DRAW_EXPANDERS(tree_view))
	    {
	      cell_area.x += depth*tree_view->priv->tab_offset;
	      cell_area.width -= depth*tree_view->priv->tab_offset;
	    }
	  break;
	}

      if (column == NULL)
	return FALSE;

      tree_view->priv->focus_column = column;
      if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_tree_view_real_set_cursor (tree_view, path, FALSE);
	  gtk_tree_view_real_toggle_cursor_row (tree_view);
	}
      else if (event->state & GDK_SHIFT_MASK)
	{
	  gtk_tree_view_real_set_cursor (tree_view, path, FALSE);
	  gtk_tree_view_real_select_cursor_row (tree_view, FALSE);
	}
      else
	{
	  gtk_tree_view_real_set_cursor (tree_view, path, TRUE);
	}

      if (event->type == GDK_BUTTON_PRESS &&
	  !(event->state & gtk_accelerator_get_default_mod_mask ()))
	{
	  GtkCellEditable *cell_editable = NULL;
	  /* FIXME: get the right flags */
	  guint flags = 0;
	  GtkTreeIter iter;

	  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
	  gtk_tree_view_column_cell_set_cell_data (column,
						   tree_view->priv->model,
						   &iter,
						   GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT),
						   node->children?TRUE:FALSE);

	  path_string = gtk_tree_path_to_string (path);

	  if (_gtk_tree_view_column_cell_event (column,
						&cell_editable,
						(GdkEvent *)event,
						path_string,
						&background_area,
						&cell_area, flags))
	    {
	      if (cell_editable != NULL)
		{
		  gtk_tree_view_real_start_editing (tree_view,
						    column,
						    path,
						    cell_editable,
						    &cell_area,
						    (GdkEvent *)event,
						    flags);

		}
	      column_handled_click = TRUE;
	    }
	  g_free (path_string);
	}

      /* Save press to possibly begin a drag
       */
      if (!column_handled_click &&
	  tree_view->priv->pressed_button < 0)
        {
          tree_view->priv->pressed_button = event->button;
          tree_view->priv->press_start_x = event->x;
          tree_view->priv->press_start_y = event->y;
        }

      if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
	{
	  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT))
	    {
	      if (node->children == NULL)
		gtk_tree_view_real_expand_row (tree_view, path,
					       tree, node, FALSE, TRUE);
	      else
		gtk_tree_view_real_collapse_row (GTK_TREE_VIEW (widget), path,
						 tree, node, TRUE);
	    }

	  gtk_tree_view_row_activated (tree_view, path, column);
	}
      GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS);
      gtk_tree_path_free (path);
      return TRUE;
    }

  /* We didn't click in the window.  Let's check to see if we clicked on a column resize window.
   */
  for (i = 0, list = tree_view->priv->columns; list; list = list->next, i++)
    {
      column = list->data;
      if (event->window == column->window &&
	  column->resizable &&
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
	  column->resized_width = column->width;
	  column->use_resized_width = TRUE;

	  /* block attached dnd signal handler */
	  drag_data = gtk_object_get_data (GTK_OBJECT (widget), "gtk-site-data");
	  if (drag_data)
	    gtk_signal_handler_block_by_data (GTK_OBJECT (widget), drag_data);

	  if (!GTK_WIDGET_HAS_FOCUS (widget))
	    gtk_widget_grab_focus (widget);

	  tree_view->priv->drag_pos = i;
	  tree_view->priv->x_drag = (column->button->allocation.x + column->button->allocation.width);
	  break;
	}
    }
  return TRUE;
}

/* GtkWidget::button_release_event helper */
static gboolean
gtk_tree_view_button_release_drag_column (GtkWidget      *widget,
					  GdkEventButton *event)
{
  GtkTreeView *tree_view;

  tree_view = GTK_TREE_VIEW (widget);

  gdk_pointer_ungrab (GDK_CURRENT_TIME);
  gdk_keyboard_ungrab (GDK_CURRENT_TIME);

  /* Move the button back */
  g_object_ref (tree_view->priv->drag_column->button);
  gtk_container_remove (GTK_CONTAINER (tree_view), tree_view->priv->drag_column->button);
  gtk_widget_set_parent_window (tree_view->priv->drag_column->button, tree_view->priv->header_window);
  gtk_widget_set_parent (tree_view->priv->drag_column->button, GTK_WIDGET (tree_view));
  g_object_unref (tree_view->priv->drag_column->button);
  gtk_widget_queue_resize (widget);

  gtk_widget_grab_focus (tree_view->priv->drag_column->button);

  if (tree_view->priv->cur_reorder &&
      tree_view->priv->cur_reorder->left_column != tree_view->priv->drag_column)
    gtk_tree_view_move_column_after (tree_view, tree_view->priv->drag_column,
				     tree_view->priv->cur_reorder->left_column);
  tree_view->priv->drag_column = NULL;
  gdk_window_hide (tree_view->priv->drag_window);

  g_list_foreach (tree_view->priv->column_drag_info, (GFunc) g_free, NULL);
  g_list_free (tree_view->priv->column_drag_info);
  tree_view->priv->column_drag_info = NULL;

  gdk_window_hide (tree_view->priv->drag_highlight_window);

  /* Reset our flags */
  tree_view->priv->drag_column_window_state = DRAG_COLUMN_WINDOW_STATE_UNSET;
  GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_IN_COLUMN_DRAG);

  return TRUE;
}

/* GtkWidget::button_release_event helper */
static gboolean
gtk_tree_view_button_release_column_resize (GtkWidget      *widget,
					    GdkEventButton *event)
{
  GtkTreeView *tree_view;
  gpointer drag_data;
  gint x;
  gint i;

  tree_view = GTK_TREE_VIEW (widget);

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

  return TRUE;
}

static gboolean
gtk_tree_view_button_release (GtkWidget      *widget,
			      GdkEventButton *event)
{
  GtkTreeView *tree_view;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  tree_view = GTK_TREE_VIEW (widget);

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_DRAG))
    return gtk_tree_view_button_release_drag_column (widget, event);

  if (tree_view->priv->pressed_button == event->button)
    tree_view->priv->pressed_button = -1;

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_RESIZE))
    return gtk_tree_view_button_release_column_resize (widget, event);

  if (tree_view->priv->button_pressed_node == NULL)
    return FALSE;

  if (event->button == 1)
    {
      gtk_grab_remove (widget);
      if (tree_view->priv->button_pressed_node == tree_view->priv->prelight_node &&
          GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT))
	{
	  GtkTreePath *path = NULL;

	  path = _gtk_tree_view_find_path (tree_view,
					   tree_view->priv->button_pressed_tree,
					   tree_view->priv->button_pressed_node);
	  /* Actually activate the node */
	  if (tree_view->priv->button_pressed_node->children == NULL)
	    gtk_tree_view_real_expand_row (tree_view, path,
					   tree_view->priv->button_pressed_tree,
					   tree_view->priv->button_pressed_node,
					   FALSE, TRUE);
	  else
	    gtk_tree_view_real_collapse_row (GTK_TREE_VIEW (widget), path,
					     tree_view->priv->button_pressed_tree,
					     tree_view->priv->button_pressed_node, TRUE);
	  gtk_tree_path_free (path);
	}

      tree_view->priv->button_pressed_tree = NULL;
      tree_view->priv->button_pressed_node = NULL;
    }

  return TRUE;
}


/* GtkWidget::motion_event function set.
 */

static gboolean
coords_are_over_arrow (GtkTreeView *tree_view,
                       GtkRBTree   *tree,
                       GtkRBNode   *node,
                       /* these are in window coords */
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

  gtk_tree_view_get_arrow_xrange (tree_view, tree, &arrow.x, &x2);

  arrow.width = x2 - arrow.x;

  return (x >= arrow.x &&
          x < (arrow.x + arrow.width) &&
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
	     /* these are in tree_window coords */
             gint         x,
             gint         y)
{
  if (coords_are_over_arrow (tree_view, tree, node, x, y))
    {
      GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_ARROW_PRELIT);
    }

  tree_view->priv->prelight_node = node;
  tree_view->priv->prelight_tree = tree;

  GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_PRELIT);
}

static void
ensure_unprelighted (GtkTreeView *tree_view)
{
  do_unprelight (tree_view, -1000, -1000); /* coords not possibly over an arrow */
  g_assert (tree_view->priv->prelight_node == NULL);
}




/* Our motion arrow is either a box (in the case of the original spot)
 * or an arrow.  It is expander_size wide.
 */
/*
 * 11111111111111
 * 01111111111110
 * 00111111111100
 * 00011111111000
 * 00001111110000
 * 00000111100000
 * 00000111100000
 * 00000111100000
 * ~ ~ ~ ~ ~ ~ ~
 * 00000111100000
 * 00000111100000
 * 00000111100000
 * 00001111110000
 * 00011111111000
 * 00111111111100
 * 01111111111110
 * 11111111111111
 */

static void
gtk_tree_view_motion_draw_column_motion_arrow (GtkTreeView *tree_view)
{
  GtkTreeViewColumnReorder *reorder = tree_view->priv->cur_reorder;
  GtkWidget *widget = GTK_WIDGET (tree_view);
  GdkBitmap *mask = NULL;
  gint x;
  gint y;
  gint width;
  gint height;
  gint arrow_type = DRAG_COLUMN_WINDOW_STATE_UNSET;
  GdkWindowAttr attributes;
  guint attributes_mask;

  if (!reorder ||
      reorder->left_column == tree_view->priv->drag_column ||
      reorder->right_column == tree_view->priv->drag_column)
    arrow_type = DRAG_COLUMN_WINDOW_STATE_ORIGINAL;
  else if (reorder->left_column || reorder->right_column)
    {
      GdkRectangle visible_rect;
      gtk_tree_view_get_visible_rect (tree_view, &visible_rect);
      if (reorder->left_column)
	x = reorder->left_column->button->allocation.x + reorder->left_column->button->allocation.width;
      else
	x = reorder->right_column->button->allocation.x;

      if (x < visible_rect.x)
	arrow_type = DRAG_COLUMN_WINDOW_STATE_ARROW_LEFT;
      else if (x > visible_rect.x + visible_rect.width)
	arrow_type = DRAG_COLUMN_WINDOW_STATE_ARROW_RIGHT;
      else
        arrow_type = DRAG_COLUMN_WINDOW_STATE_ARROW;
    }

  /* We want to draw the rectangle over the initial location. */
  if (arrow_type == DRAG_COLUMN_WINDOW_STATE_ORIGINAL)
    {
      GdkGC *gc;
      GdkColor col;

      if (tree_view->priv->drag_column_window_state != DRAG_COLUMN_WINDOW_STATE_ORIGINAL)
	{

	  if (tree_view->priv->drag_highlight_window)
	    gdk_window_destroy (tree_view->priv->drag_highlight_window);

	  attributes.window_type = GDK_WINDOW_CHILD;
	  attributes.wclass = GDK_INPUT_OUTPUT;
	  attributes.visual = gtk_widget_get_visual (GTK_WIDGET (tree_view));
	  attributes.colormap = gtk_widget_get_colormap (GTK_WIDGET (tree_view));
	  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK;
	  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
	  tree_view->priv->drag_highlight_window = gdk_window_new (tree_view->priv->header_window, &attributes, attributes_mask);
	  gdk_window_set_user_data (tree_view->priv->drag_highlight_window, GTK_WIDGET (tree_view));

	  width = tree_view->priv->drag_column->button->allocation.width;
	  height = tree_view->priv->drag_column->button->allocation.height;
	  gdk_window_move_resize (tree_view->priv->drag_highlight_window,
				  tree_view->priv->drag_column_x, 0, width, height);

	  mask = gdk_pixmap_new (tree_view->priv->drag_highlight_window, width, height, 1);
	  gc = gdk_gc_new (mask);
	  col.pixel = 1;
	  gdk_gc_set_foreground (gc, &col);
	  gdk_draw_rectangle (mask, gc, TRUE, 0, 0, width, height);
	  col.pixel = 0;
	  gdk_gc_set_foreground(gc, &col);
	  gdk_draw_rectangle (mask, gc, TRUE, 2, 2, width - 4, height - 4);
	  gdk_gc_destroy (gc);

	  gdk_window_shape_combine_mask (tree_view->priv->drag_highlight_window,
					 mask, 0, 0);
	  if (mask) gdk_pixmap_unref (mask);
	  tree_view->priv->drag_column_window_state = DRAG_COLUMN_WINDOW_STATE_ORIGINAL;
	}
    }
  else if (arrow_type == DRAG_COLUMN_WINDOW_STATE_ARROW)
    {
      gint i, j = 1;
      GdkGC *gc;
      GdkColor col;
      gint expander_size;

      gtk_widget_style_get (widget,
			    "expander_size", &expander_size,
			    NULL);

      width = expander_size;

      /* Get x, y, width, height of arrow */
      gdk_window_get_origin (tree_view->priv->header_window, &x, &y);
      if (reorder->left_column)
	{
	  x += reorder->left_column->button->allocation.x + reorder->left_column->button->allocation.width - width/2;
	  height = reorder->left_column->button->allocation.height;
	}
      else
	{
	  x += reorder->right_column->button->allocation.x - width/2;
	  height = reorder->right_column->button->allocation.height;
	}
      y -= expander_size/2; /* The arrow takes up only half the space */
      height += expander_size;

      /* Create the new window */
      if (tree_view->priv->drag_column_window_state != DRAG_COLUMN_WINDOW_STATE_ARROW)
	{
	  if (tree_view->priv->drag_highlight_window)
	    gdk_window_destroy (tree_view->priv->drag_highlight_window);

	  attributes.window_type = GDK_WINDOW_TEMP;
	  attributes.wclass = GDK_INPUT_OUTPUT;
	  attributes.visual = gtk_widget_get_visual (GTK_WIDGET (tree_view));
	  attributes.colormap = gtk_widget_get_colormap (GTK_WIDGET (tree_view));
	  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK;
	  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
	  attributes.width = width;
	  attributes.height = height;
	  tree_view->priv->drag_highlight_window = gdk_window_new (NULL, &attributes, attributes_mask);
	  gdk_window_set_user_data (tree_view->priv->drag_highlight_window, GTK_WIDGET (tree_view));

	  mask = gdk_pixmap_new (tree_view->priv->drag_highlight_window, width, height, 1);
	  gc = gdk_gc_new (mask);
	  col.pixel = 1;
	  gdk_gc_set_foreground (gc, &col);
	  gdk_draw_rectangle (mask, gc, TRUE, 0, 0, width, height);

	  /* Draw the 2 arrows as per above */
	  col.pixel = 0;
	  gdk_gc_set_foreground (gc, &col);
	  for (i = 0; i < width; i ++)
	    {
	      if (i == (width/2 - 1))
		continue;
	      gdk_draw_line (mask, gc, i, j, i, height - j);
	      if (i < (width/2 - 1))
		j++;
	      else
		j--;
	    }
	  gdk_gc_destroy (gc);
	  gdk_window_shape_combine_mask (tree_view->priv->drag_highlight_window,
					 mask, 0, 0);
	  if (mask) gdk_pixmap_unref (mask);
	}

      tree_view->priv->drag_column_window_state = DRAG_COLUMN_WINDOW_STATE_ARROW;
      gdk_window_move (tree_view->priv->drag_highlight_window, x, y);
    }
  else if (arrow_type == DRAG_COLUMN_WINDOW_STATE_ARROW_LEFT ||
	   arrow_type == DRAG_COLUMN_WINDOW_STATE_ARROW_RIGHT)
    {
      gint i, j = 1;
      GdkGC *gc;
      GdkColor col;
      gint expander_size;

      gtk_widget_style_get (widget,
			    "expander_size", &expander_size,
			    NULL);

      width = expander_size;

      /* Get x, y, width, height of arrow */
      width = width/2; /* remember, the arrow only takes half the available width */
      gdk_window_get_origin (widget->window, &x, &y);
      if (arrow_type == DRAG_COLUMN_WINDOW_STATE_ARROW_RIGHT)
	x += widget->allocation.width - width;

      if (reorder->left_column)
	height = reorder->left_column->button->allocation.height;
      else
	height = reorder->right_column->button->allocation.height;

      y -= expander_size;
      height += 2*expander_size;

      /* Create the new window */
      if (tree_view->priv->drag_column_window_state != DRAG_COLUMN_WINDOW_STATE_ARROW_LEFT &&
	  tree_view->priv->drag_column_window_state != DRAG_COLUMN_WINDOW_STATE_ARROW_RIGHT)
	{
	  if (tree_view->priv->drag_highlight_window)
	    gdk_window_destroy (tree_view->priv->drag_highlight_window);

	  attributes.window_type = GDK_WINDOW_TEMP;
	  attributes.wclass = GDK_INPUT_OUTPUT;
	  attributes.visual = gtk_widget_get_visual (GTK_WIDGET (tree_view));
	  attributes.colormap = gtk_widget_get_colormap (GTK_WIDGET (tree_view));
	  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK;
	  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
	  attributes.width = width;
	  attributes.height = height;
	  tree_view->priv->drag_highlight_window = gdk_window_new (NULL, &attributes, attributes_mask);
	  gdk_window_set_user_data (tree_view->priv->drag_highlight_window, GTK_WIDGET (tree_view));

	  mask = gdk_pixmap_new (tree_view->priv->drag_highlight_window, width, height, 1);
	  gc = gdk_gc_new (mask);
	  col.pixel = 1;
	  gdk_gc_set_foreground (gc, &col);
	  gdk_draw_rectangle (mask, gc, TRUE, 0, 0, width, height);

	  /* Draw the 2 arrows as per above */
	  col.pixel = 0;
	  gdk_gc_set_foreground (gc, &col);
	  j = expander_size;
	  for (i = 0; i < width; i ++)
	    {
	      gint k;
	      if (arrow_type == DRAG_COLUMN_WINDOW_STATE_ARROW_LEFT)
		k = width - i - 1;
	      else
		k = i;
	      gdk_draw_line (mask, gc, k, j, k, height - j);
	      gdk_draw_line (mask, gc, k, 0, k, expander_size - j);
	      gdk_draw_line (mask, gc, k, height, k, height - expander_size + j);
	      j--;
	    }
	  gdk_gc_destroy (gc);
	  gdk_window_shape_combine_mask (tree_view->priv->drag_highlight_window,
					 mask, 0, 0);
	  if (mask) gdk_pixmap_unref (mask);
	}

      tree_view->priv->drag_column_window_state = arrow_type;
      gdk_window_move (tree_view->priv->drag_highlight_window, x, y);
   }
  else
    {
      g_warning (G_STRLOC"Invalid GtkTreeViewColumnReorder struct");
      gdk_window_hide (tree_view->priv->drag_highlight_window);
      return;
    }

  gdk_window_show (tree_view->priv->drag_highlight_window);
  gdk_window_raise (tree_view->priv->drag_highlight_window);
}

static gboolean
gtk_tree_view_motion_resize_column (GtkWidget      *widget,
				    GdkEventMotion *event)
{
  gint x;
  gint new_width;
  GtkTreeViewColumn *column;
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

  column = gtk_tree_view_get_column (tree_view, tree_view->priv->drag_pos);

  if (event->is_hint || event->window != widget->window)
    gtk_widget_get_pointer (widget, &x, NULL);
  else
    x = event->x;

  if (tree_view->priv->hadjustment)
    x += tree_view->priv->hadjustment->value;

  new_width = gtk_tree_view_new_column_width (tree_view,
					      tree_view->priv->drag_pos, &x);
  if (x != tree_view->priv->x_drag &&
      (new_width != column->fixed_width));
    {
      column->resized_width = new_width;
      gtk_widget_queue_resize (widget);
    }

  return FALSE;
}


static void
gtk_tree_view_update_current_reorder (GtkTreeView *tree_view)
{
  GtkTreeViewColumnReorder *reorder = NULL;
  GList *list;
  gint mouse_x;

  gdk_window_get_pointer (tree_view->priv->header_window, &mouse_x, NULL, NULL);

  for (list = tree_view->priv->column_drag_info; list; list = list->next)
    {
      reorder = (GtkTreeViewColumnReorder *) list->data;
      if (mouse_x >= reorder->left_align && mouse_x < reorder->right_align)
	break;
      reorder = NULL;
    }

  /*  if (reorder && reorder == tree_view->priv->cur_reorder)
      return;*/

  tree_view->priv->cur_reorder = reorder;
  gtk_tree_view_motion_draw_column_motion_arrow (tree_view);
}

static gboolean
gtk_tree_view_horizontal_autoscroll (GtkTreeView *tree_view)
{
  GdkRectangle visible_rect;
  gint x;
  gint offset;
  gfloat value;

  gdk_window_get_pointer (tree_view->priv->bin_window, &x, NULL, NULL);

  gtk_tree_view_get_visible_rect (tree_view, &visible_rect);

  /* See if we are near the edge. */
  offset = x - (visible_rect.x + SCROLL_EDGE_SIZE);
  if (offset > 0)
    {
      offset = x - (visible_rect.x + visible_rect.width - SCROLL_EDGE_SIZE);
      if (offset < 0)
	return TRUE;
    }
  offset = offset/3;

  value = CLAMP (tree_view->priv->hadjustment->value + offset,
		 0.0, tree_view->priv->hadjustment->upper - tree_view->priv->hadjustment->page_size);
  gtk_adjustment_set_value (tree_view->priv->hadjustment, value);

  return TRUE;

}

static gboolean
gtk_tree_view_motion_drag_column (GtkWidget      *widget,
				  GdkEventMotion *event)
{
  GtkTreeView *tree_view = (GtkTreeView *) widget;
  GtkTreeViewColumn *column = tree_view->priv->drag_column;
  gint x, y;

  /* Sanity Check */
  if ((column == NULL) ||
      (event->window != tree_view->priv->drag_window))
    return FALSE;

  /* Handle moving the header */
  gdk_window_get_position (tree_view->priv->drag_window, &x, &y);
  x = CLAMP (x + (gint)event->x - column->drag_x, 0,
	     MAX (tree_view->priv->width, GTK_WIDGET (tree_view)->allocation.width) - column->button->allocation.width);
  gdk_window_move (tree_view->priv->drag_window, x, y);

  /* autoscroll, if needed */
  gtk_tree_view_horizontal_autoscroll (tree_view);
  /* Update the current reorder position and arrow; */
  gtk_tree_view_update_current_reorder (tree_view);

  return TRUE;
}

static gboolean
gtk_tree_view_motion_bin_window (GtkWidget      *widget,
				 GdkEventMotion *event)
{
  GtkTreeView *tree_view;
  GtkRBTree *tree;
  GtkRBNode *node;
  gint new_y;
  GtkRBTree *old_prelight_tree;
  GtkRBNode *old_prelight_node;
  gboolean old_arrow_prelit;

  tree_view = (GtkTreeView *) widget;

  if (tree_view->priv->tree == NULL)
    return FALSE;

  gtk_tree_view_maybe_begin_dragging_row (tree_view, event);

  old_prelight_tree = tree_view->priv->prelight_tree;
  old_prelight_node = tree_view->priv->prelight_node;
  old_arrow_prelit = GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT);

  new_y = TREE_WINDOW_Y_TO_RBTREE_Y(tree_view, event->y);
  if (new_y < 0)
    new_y = 0;
  do_unprelight (tree_view, event->x, event->y);
  _gtk_rbtree_find_offset (tree_view->priv->tree, new_y, &tree, &node);

  if (tree == NULL)
    return TRUE;

  /* If we are currently pressing down a button, we don't want to prelight anything else. */
  if ((tree_view->priv->button_pressed_node != NULL) &&
      (tree_view->priv->button_pressed_node != node))
    return TRUE;


  do_prelight (tree_view, tree, node, event->x, event->y);

  if (old_prelight_node != tree_view->priv->prelight_node)
    {
      if (old_prelight_node)
	{
	  _gtk_tree_view_queue_draw_node (tree_view,
					  old_prelight_tree,
					  old_prelight_node,
					  NULL);
	}
      if (tree_view->priv->prelight_node)
	{
	  _gtk_tree_view_queue_draw_node (tree_view,
					  tree_view->priv->prelight_tree,
					  tree_view->priv->prelight_node,
					  NULL);
	}
    }
  else if (old_arrow_prelit != GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT))
    {
      if (tree_view->priv->prelight_node)
	{
	  _gtk_tree_view_queue_draw_node (tree_view,
					 tree_view->priv->prelight_tree,
					 tree_view->priv->prelight_node,
					 NULL);
	}
    }
  return TRUE;
}

static gboolean
gtk_tree_view_motion (GtkWidget      *widget,
		      GdkEventMotion *event)
{
  GtkTreeView *tree_view;

  tree_view = (GtkTreeView *) widget;

  /* Resizing a column */
  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_RESIZE))
    return gtk_tree_view_motion_resize_column (widget, event);

  /* Drag column */
  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_DRAG))
    return gtk_tree_view_motion_drag_column (widget, event);

  /* Sanity check it */
  if (event->window == tree_view->priv->bin_window)
    return gtk_tree_view_motion_bin_window (widget, event);

  return FALSE;
}

/* Warning: Very scary function.
 * Modify at your own risk
 *
 * KEEP IN SYNC WITH gtk_tree_view_create_row_drag_icon()!
 * FIXME: It's not...
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
  gint new_y;
  gint y_offset, x_offset, cell_offset;
  gint max_height;
  gint depth;
  GdkRectangle background_area;
  GdkRectangle cell_area;
  guint flags;
  gint highlight_x;
  gint bin_window_width;
  GtkTreePath *cursor_path;
  GtkTreePath *drag_dest_path;
  GList *last_column;
  gint vertical_separator;
  gint horizontal_separator;
  gboolean allow_rules;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);

  tree_view = GTK_TREE_VIEW (widget);

  gtk_widget_style_get (widget,
			"horizontal_separator", &horizontal_separator,
			"vertical_separator", &vertical_separator,
			"allow_rules", &allow_rules,
			NULL);

  if (tree_view->priv->tree == NULL)
    return TRUE;

  /* clip event->area to the visible area */
  if (event->area.height < 0)
    return TRUE;

  validate_visible_area (tree_view);

  new_y = TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, event->area.y);

  if (new_y < 0)
    new_y = 0;
  y_offset = -_gtk_rbtree_find_offset (tree_view->priv->tree, new_y, &tree, &node);

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

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
        flags |= GTK_CELL_RENDERER_SELECTED;

      parity = _gtk_rbtree_node_find_parity (tree, node);

      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  GtkTreeViewColumn *column = list->data;
	  const gchar *detail = NULL;
	  GtkStateType state;

	  if (!column->visible)
            continue;

	  if (cell_offset > event->area.x + event->area.width ||
	      cell_offset + column->width < event->area.x)
	    {
	      cell_offset += column->width;
	      continue;
	    }

          if (column->show_sort_indicator)
            flags |= GTK_CELL_RENDERER_SORTED;
          else
            flags &= ~GTK_CELL_RENDERER_SORTED;

	  gtk_tree_view_column_cell_set_cell_data (column,
						   tree_view->priv->model,
						   &iter,
						   GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT),
						   node->children?TRUE:FALSE);


	  background_area.x = cell_offset;
	  background_area.width = column->width;

          cell_area = background_area;
          cell_area.y += vertical_separator / 2;
          cell_area.x += horizontal_separator / 2;
          cell_area.height -= vertical_separator;
	  cell_area.width -= horizontal_separator;

          /* Select the detail for drawing the cell.  relevant
           * factors are parity, sortedness, and whether to
           * display rules.
           */
          if (allow_rules && tree_view->priv->has_rules)
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

	  if (flags & GTK_CELL_RENDERER_SELECTED)
	    state = GTK_STATE_SELECTED;
	  else
	    state = GTK_STATE_NORMAL;

	  /* Draw background */
          gtk_paint_flat_box (widget->style,
                              event->window,
			      state,
                              GTK_SHADOW_NONE,
                              &event->area,
                              widget,
                              detail,
                              background_area.x,
                              background_area.y,
                              background_area.width,
                              background_area.height);

	  if (gtk_tree_view_is_expander_column (tree_view, column) &&
              TREE_VIEW_DRAW_EXPANDERS(tree_view))
	    {
	      cell_area.x += depth*tree_view->priv->tab_offset;
	      cell_area.width -= depth*tree_view->priv->tab_offset;

              /* If we have an expander column, the highlight underline
               * starts with that column, so that it indicates which
               * level of the tree we're dropping at.
               */
              highlight_x = cell_area.x;
	      gtk_tree_view_column_cell_render (column,
						event->window,
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
	      gtk_tree_view_column_cell_render (column,
						event->window,
						&background_area,
						&cell_area,
						&event->area,
						flags);
	    }
	  if (node == cursor &&
	      ((column == tree_view->priv->focus_column &&
		GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS) &&
		GTK_WIDGET_HAS_FOCUS (widget)) ||
	       (column == tree_view->priv->edited_column)))
	    {
	      gtk_tree_view_column_cell_draw_focus (column,
						    event->window,
						    &background_area,
						    &cell_area,
						    &event->area,
						    flags);
	    }
	  cell_offset += column->width;
	}


      if (node == drag_highlight)
        {
          /* Draw indicator for the drop
           */
          gint highlight_y = -1;
	  GtkRBTree *tree = NULL;
	  GtkRBNode *node = NULL;
	  gint width;

          switch (tree_view->priv->drag_dest_pos)
            {
            case GTK_TREE_VIEW_DROP_BEFORE:
              highlight_y = background_area.y - vertical_separator/2;
              break;

            case GTK_TREE_VIEW_DROP_AFTER:
              highlight_y = background_area.y + background_area.height + vertical_separator/2;
              break;

            case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
            case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
	      _gtk_tree_view_find_node (tree_view, drag_dest_path, &tree, &node);

	      if (tree == NULL)
		break;
	      gdk_drawable_get_size (tree_view->priv->bin_window,
				     &width, NULL);
	      gtk_paint_focus (widget->style,
			       tree_view->priv->bin_window,
			       GTK_WIDGET_STATE (widget),
			       NULL,
			       widget,
			       "treeview-drop-indicator",
			       0, BACKGROUND_FIRST_PIXEL (tree_view, tree, node),
			       width, BACKGROUND_HEIGHT (node));

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
		    /* we should go to done to free some memory */
		    goto done;
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

 done:
  if (cursor_path)
    gtk_tree_path_free (cursor_path);

  if (drag_dest_path)
    gtk_tree_path_free (drag_dest_path);

  return FALSE;
}

static gboolean
gtk_tree_view_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  GtkTreeView *tree_view;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);

  tree_view = GTK_TREE_VIEW (widget);

  if (event->window == tree_view->priv->bin_window)
    return gtk_tree_view_bin_expose (widget, event);
  else if (event->window == tree_view->priv->header_window)
    {
      GList *list;
      
      for (list = tree_view->priv->columns; list != NULL; list = list->next)
	{
	  GtkTreeViewColumn *column = list->data;

	  if (column == tree_view->priv->drag_column)
	    continue;

	  if (column->visible)
	    gtk_container_propagate_expose (GTK_CONTAINER (tree_view),
					    column->button,
					    event);
	}
    }
  else if (event->window == tree_view->priv->drag_window)
    {
      gtk_container_propagate_expose (GTK_CONTAINER (tree_view),
				      tree_view->priv->drag_column->button,
				      event);
    }
  return TRUE;
}

static gboolean
gtk_tree_view_key_press (GtkWidget   *widget,
			 GdkEventKey *event)
{
  GtkTreeView *tree_view = (GtkTreeView *) widget;

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_DRAG))
    {
      if (event->keyval == GDK_Escape)
	{
	  tree_view->priv->cur_reorder = NULL;
	  gtk_tree_view_button_release_drag_column (widget, NULL);
	}
      return TRUE;
    }

  return (* GTK_WIDGET_CLASS (parent_class)->key_press_event) (widget, event);
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
  new_y = TREE_WINDOW_Y_TO_RBTREE_Y(tree_view, event->y);
  if (new_y < 0)
    new_y = 0;
  _gtk_rbtree_find_offset (tree_view->priv->tree, new_y, &tree, &node);

  if (node == NULL)
    return FALSE;

  do_prelight (tree_view, tree, node, event->x, event->y);

  if (tree_view->priv->prelight_node)
    _gtk_tree_view_queue_draw_node (tree_view,
                                   tree_view->priv->prelight_tree,
                                   tree_view->priv->prelight_node,
                                   NULL);

  return TRUE;
}

static gboolean
gtk_tree_view_leave_notify (GtkWidget        *widget,
			    GdkEventCrossing *event)
{
  GtkWidget *search_dialog;
  GtkTreeView *tree_view;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);

  if (event->mode == GDK_CROSSING_GRAB)
    return TRUE;
  tree_view = GTK_TREE_VIEW (widget);

  if (tree_view->priv->prelight_node)
    _gtk_tree_view_queue_draw_node (tree_view,
                                   tree_view->priv->prelight_tree,
                                   tree_view->priv->prelight_node,
                                   NULL);

  ensure_unprelighted (tree_view);

  /* destroy interactive search dialog */
  search_dialog = gtk_object_get_data (GTK_OBJECT (widget),
				       GTK_TREE_VIEW_SEARCH_DIALOG_KEY);
  if (search_dialog)
    gtk_tree_view_search_dialog_destroy (search_dialog, GTK_TREE_VIEW (widget));

return TRUE;
}


static gint
gtk_tree_view_focus_in (GtkWidget     *widget,
			GdkEventFocus *event)
{
  GtkTreeView *tree_view;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  tree_view = GTK_TREE_VIEW (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);

  gtk_widget_queue_draw (widget);

  return FALSE;
}


static gint
gtk_tree_view_focus_out (GtkWidget     *widget,
			 GdkEventFocus *event)
{
  GtkWidget   *search_dialog;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);

  gtk_widget_queue_draw (widget);

  /* destroy interactive search dialog */
  search_dialog = gtk_object_get_data (GTK_OBJECT (widget),
				       GTK_TREE_VIEW_SEARCH_DIALOG_KEY);
  if (search_dialog)
    gtk_tree_view_search_dialog_destroy (search_dialog, GTK_TREE_VIEW (widget));

  return FALSE;
}


/* Incremental Reflow
 */

/* Returns TRUE if it updated the size
 */
static gboolean
validate_row (GtkTreeView *tree_view,
	      GtkRBTree   *tree,
	      GtkRBNode   *node,
	      GtkTreeIter *iter,
	      GtkTreePath *path)
{
  GtkTreeViewColumn *column;
  GList *list;
  gint height = 0;
  gint horizontal_separator;
  gint depth = gtk_tree_path_get_depth (path);
  gboolean retval = FALSE;


  /* double check the row needs validating */
  if (! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID) &&
      ! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID))
    return FALSE;

  gtk_widget_style_get (GTK_WIDGET (tree_view),
			"horizontal_separator", &horizontal_separator,
			NULL);

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      gint tmp_width;
      gint tmp_height;

      column = list->data;

      if (! column->visible)
	continue;

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID) && !column->dirty)
	continue;

      gtk_tree_view_column_cell_set_cell_data (column, tree_view->priv->model, iter,
					       GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT),
					       node->children?TRUE:FALSE);
      gtk_tree_view_column_cell_get_size (column,
					  NULL, NULL, NULL,
					  &tmp_width, &tmp_height);
      height = MAX (height, tmp_height);

      if (gtk_tree_view_is_expander_column (tree_view, column) && TREE_VIEW_DRAW_EXPANDERS (tree_view))
	tmp_width = tmp_width + horizontal_separator + depth * tree_view->priv->tab_offset;
      else
	tmp_width = tmp_width + horizontal_separator;

      if (tmp_width > column->requested_width)
	{
	  retval = TRUE;
	  column->requested_width = tmp_width;
	}
    }

  if (height != GTK_RBNODE_GET_HEIGHT (node))
    {
      retval = TRUE;
      _gtk_rbtree_node_set_height (tree, node, height);
    }
  _gtk_rbtree_node_mark_valid (tree, node);

  return retval;
}


static void
validate_visible_area (GtkTreeView *tree_view)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkRBTree *tree;
  GtkRBNode *node;
  gint y, height, offset;
  gboolean validated_area = FALSE;
  gboolean size_changed = FALSE;
  gint height_above;
  gint height_below;
  
  if (tree_view->priv->tree == NULL)
    return;
  
  if (! GTK_RBNODE_FLAG_SET (tree_view->priv->tree->root, GTK_RBNODE_DESCENDANTS_INVALID))
    return;
  
  height = GTK_WIDGET (tree_view)->allocation.height - TREE_VIEW_HEADER_HEIGHT (tree_view);

  y = TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, 0);

  offset = _gtk_rbtree_find_offset (tree_view->priv->tree, y,
				    &tree, &node);
  if (node == NULL)
    {
      path = gtk_tree_path_new_root ();
      _gtk_tree_view_find_node (tree_view, path, &tree, &node);
    }
  else
    {
      path = _gtk_tree_view_find_path (tree_view, tree, node);
      height += offset;
    }

  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
  do
    {
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID) ||
	  GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID))
	{
	  validated_area = TRUE;
	  if (validate_row (tree_view, tree, node, &iter, path))
	    size_changed = TRUE;
	}
      height -= GTK_RBNODE_GET_HEIGHT (node);

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
	  TREE_VIEW_INTERNAL_ASSERT_VOID (has_child);
	  gtk_tree_path_append_index (path, 0);
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
		  done = TRUE;

		  /* Sanity Check! */
		  TREE_VIEW_INTERNAL_ASSERT_VOID (has_next);
		}
	      else
		{
		  GtkTreeIter parent_iter = iter;
		  gboolean has_parent;

		  node = tree->parent_node;
		  tree = tree->parent_tree;
		  if (tree == NULL)
		    break;
		  has_parent = gtk_tree_model_iter_parent (tree_view->priv->model,
							   &iter,
							   &parent_iter);

		  /* Sanity check */
		  TREE_VIEW_INTERNAL_ASSERT_VOID (has_parent);
		}
	    }
	  while (!done);
	}
    }
  while (node && height > 0);

  if (size_changed)
    gtk_widget_queue_resize (GTK_WIDGET (tree_view));
  if (validated_area)
    gtk_widget_queue_draw (GTK_WIDGET (tree_view));
  if (path)
    gtk_tree_path_free (path);
}

/* Our strategy for finding nodes to validate is a little convoluted.  We find
 * the left-most uninvalidated node.  We then try walking right, validating
 * nodes.  Once we find a valid node, we repeat the previous process of finding
 * the first invalid node.
 */

static gboolean
validate_rows_handler (GtkTreeView *tree_view)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;
  gboolean validated_area = FALSE;
  gint retval = TRUE;
  GtkTreePath *path = NULL;
  GtkTreeIter iter;
  gint i = 0;
  g_assert (tree_view);

  GDK_THREADS_ENTER ();

  if (tree_view->priv->tree == NULL)
    {
      tree_view->priv->validate_rows_timer = 0;
      return FALSE;
    }

  do
    {

      if (! GTK_RBNODE_FLAG_SET (tree_view->priv->tree->root, GTK_RBNODE_DESCENDANTS_INVALID))
	{
	  retval = FALSE;
	  goto done;
	}

      if (path != NULL)
	{
	  node = _gtk_rbtree_next (tree, node);
	  if (node != NULL)
	    {
	      TREE_VIEW_INTERNAL_ASSERT (gtk_tree_model_iter_next (tree_view->priv->model, &iter), FALSE);
	      gtk_tree_path_next (path);
	    }
	  else
	    {
	      gtk_tree_path_free (path);
	      path = NULL;
	    }
	}

      if (path == NULL)
	{
	  tree = tree_view->priv->tree;
	  node = tree_view->priv->tree->root;

	  g_assert (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_DESCENDANTS_INVALID));

	  do
	    {
	      if (node->left != tree->nil &&
		  GTK_RBNODE_FLAG_SET (node->left, GTK_RBNODE_DESCENDANTS_INVALID))
		{
		  node = node->left;
		}
	      else if (node->right != tree->nil &&
		       GTK_RBNODE_FLAG_SET (node->right, GTK_RBNODE_DESCENDANTS_INVALID))
		{
		  node = node->right;
		}
	      else if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID) ||
		       GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID))
		{
		  break;
		}
	      else if (node->children != NULL)
		{
		  tree = node->children;
		  node = tree->root;
		}
	      else
		/* RBTree corruption!  All bad */
		g_assert_not_reached ();
	    }
	  while (TRUE);
	  path = _gtk_tree_view_find_path (tree_view, tree, node);
	  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
	}
      validated_area = validate_row (tree_view, tree, node, &iter, path) | validated_area;
      i++;
    }
  while (i < GTK_TREE_VIEW_NUM_ROWS_PER_IDLE);
  
 done:
  if (path) gtk_tree_path_free (path);
  if (validated_area)
    gtk_widget_queue_resize (GTK_WIDGET (tree_view));
  if (! retval)
    tree_view->priv->validate_rows_timer = 0;

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean
presize_handler_callback (gpointer data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (data);

  GDK_THREADS_ENTER ();

  if (tree_view->priv->mark_rows_col_dirty)
    {
      if (tree_view->priv->tree)
	_gtk_rbtree_column_invalid (tree_view->priv->tree);
      tree_view->priv->mark_rows_col_dirty = FALSE;
    }
  validate_visible_area (tree_view);
  tree_view->priv->presize_handler_timer = 0;
		   
  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
install_presize_handler (GtkTreeView *tree_view)
{
  if (! GTK_WIDGET_REALIZED (tree_view))
    return;

  if (! tree_view->priv->presize_handler_timer)
    {
      tree_view->priv->presize_handler_timer =
	g_idle_add_full (GTK_PRIORITY_RESIZE - 2, presize_handler_callback, tree_view, NULL);
    }
  if (! tree_view->priv->validate_rows_timer)
    {
      tree_view->priv->validate_rows_timer =
	g_idle_add_full (GTK_TREE_VIEW_PRIORITY_VALIDATE, (GSourceFunc) validate_rows_handler, tree_view, NULL);
    }
}

static void
gtk_tree_view_dy_to_top_row (GtkTreeView *tree_view)
{
  GtkTreePath *path;
  GtkRBTree *tree;
  GtkRBNode *node;

  gtk_tree_row_reference_free (tree_view->priv->top_row);
  tree_view->priv->top_row_dy = _gtk_rbtree_find_offset (tree_view->priv->tree,
							 tree_view->priv->dy,
							 &tree, &node);
  g_assert (tree != NULL);
      
  path = _gtk_tree_view_find_path (tree_view, tree, node);
  tree_view->priv->top_row = gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view), tree_view->priv->model, path);
  gtk_tree_path_free (path);
}

static void
gtk_tree_view_top_row_to_dy (GtkTreeView *tree_view)
{
  GtkTreePath *path;
  GtkRBTree *tree;
  GtkRBNode *node;

  path = gtk_tree_row_reference_get_path (tree_view->priv->top_row);
  if (_gtk_tree_view_find_node (tree_view, path, &tree, &node) &&
      tree != NULL)
    {
      
    }
}

void
_gtk_tree_view_install_mark_rows_col_dirty (GtkTreeView *tree_view)
{
  tree_view->priv->mark_rows_col_dirty = TRUE;

  install_presize_handler (tree_view);
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

  di->source_target_list = NULL;
}

static void
clear_dest_info (TreeViewDragInfo *di)
{
  if (di->dest_target_list)
    gtk_target_list_unref (di->dest_target_list);

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

#if 0
static gint
drag_scan_timeout (gpointer data)
{
  GtkTreeView *tree_view;
  gint x, y;
  GdkModifierType state;
  GtkTreePath *path = NULL;
  GtkTreeViewColumn *column = NULL;
  GdkRectangle visible_rect;

  GDK_THREADS_ENTER ();

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
					TRUE,
                                        0.5, 0.5);

          gtk_tree_path_free (path);
        }
    }

  GDK_THREADS_LEAVE ();

  return TRUE;
}
#endif /* 0 */

static void
remove_scroll_timeout (GtkTreeView *tree_view)
{
  if (tree_view->priv->scroll_timeout != 0)
    {
      gtk_timeout_remove (tree_view->priv->scroll_timeout);
      tree_view->priv->scroll_timeout = 0;
    }
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
remove_open_timeout (GtkTreeView *tree_view)
{
  if (tree_view->priv->open_dest_timeout != 0)
    {
      gtk_timeout_remove (tree_view->priv->open_dest_timeout);
      tree_view->priv->open_dest_timeout = 0;
    }
}


static gint
open_row_timeout (gpointer data)
{
  GtkTreeView *tree_view = data;
  GtkTreePath *dest_path = NULL;
  GtkTreeViewDropPosition pos;
  gboolean result = FALSE;

  GDK_THREADS_ENTER ();

  gtk_tree_view_get_drag_dest_row (tree_view,
                                   &dest_path,
                                   &pos);

  if (dest_path &&
      (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER ||
       pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE))
    {
      gtk_tree_view_expand_row (tree_view, dest_path, FALSE);
      tree_view->priv->open_dest_timeout = 0;

      gtk_tree_path_free (dest_path);
    }
  else
    {
      if (dest_path)
        gtk_tree_path_free (dest_path);

      result = TRUE;
    }

  GDK_THREADS_LEAVE ();

  return result;
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
gtk_tree_view_maybe_begin_dragging_row (GtkTreeView      *tree_view,
                                        GdkEventMotion   *event)
{
  GdkDragContext *context;
  TreeViewDragInfo *di;
  GtkTreePath *path = NULL;
  gint button;
  gint cell_x, cell_y;
  GtkTreeModel *model;
  gboolean retval = FALSE;

  di = get_info (tree_view);

  if (di == NULL)
    goto out;

  if (tree_view->priv->pressed_button < 0)
    goto out;

  if (!gtk_drag_check_threshold (GTK_WIDGET (tree_view),
                                 tree_view->priv->press_start_x,
                                 tree_view->priv->press_start_y,
                                 event->x, event->y))
    goto out;

  model = gtk_tree_view_get_model (tree_view);

  if (model == NULL)
    goto out;

  button = tree_view->priv->pressed_button;
  tree_view->priv->pressed_button = -1;

  gtk_tree_view_get_path_at_pos (tree_view,
                                 tree_view->priv->press_start_x,
                                 tree_view->priv->press_start_y,
                                 &path,
                                 NULL,
                                 &cell_x,
                                 &cell_y);

  if (path == NULL)
    goto out;

  if (!GTK_IS_TREE_DRAG_SOURCE (model) ||
      !gtk_tree_drag_source_row_draggable (GTK_TREE_DRAG_SOURCE (model),
					   path))
    goto out;

  /* FIXME Check whether we're a start button, if not return FALSE and
   * free path
   */

  /* Now we can begin the drag */

  retval = TRUE;

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

 out:
  if (path)
    gtk_tree_path_free (path);

  return retval;
}


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

/* Default signal implementations for the drag signals */
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
      gtk_tree_set_row_drag_data (selection_data,
				  model,
				  source_row);
    }

 done:
  gtk_tree_path_free (source_row);
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
	  if (!gtk_tree_drag_dest_row_drop_possible (GTK_TREE_DRAG_DEST (model),
						     path,
						     selection_data))
	    suggested_action = 0;
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



/* GtkContainer Methods
 */


static void
gtk_tree_view_remove (GtkContainer *container,
		      GtkWidget    *widget)
{
  GtkTreeView *tree_view;
  GtkTreeViewChild *child = NULL;
  GList *tmp_list;

  g_return_if_fail (GTK_IS_TREE_VIEW (container));

  tree_view = GTK_TREE_VIEW (container);

  tmp_list = tree_view->priv->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      if (child->widget == widget)
	{
	  gtk_widget_unparent (widget);

	  tree_view->priv->children = g_list_remove_link (tree_view->priv->children, tmp_list);
	  g_list_free_1 (tmp_list);
	  g_free (child);
	  return;
	}

      tmp_list = tmp_list->next;
    }

  tmp_list = tree_view->priv->columns;

  while (tmp_list)
    {
      GtkTreeViewColumn *column;
      column = tmp_list->data;
      if (column->button == widget)
	{
	  gtk_widget_unparent (widget);
	  return;
	}
      tmp_list = tmp_list->next;
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

/* Returns TRUE if the focus is within the headers, after the focus operation is
 * done
 */
static gboolean
gtk_tree_view_header_focus (GtkTreeView      *tree_view,
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

  last_column = g_list_last (tree_view->priv->columns);
  while (last_column)
    {
      if (GTK_WIDGET_CAN_FOCUS (GTK_TREE_VIEW_COLUMN (last_column->data)->button) &&
	  GTK_TREE_VIEW_COLUMN (last_column->data)->clickable &&
	  GTK_TREE_VIEW_COLUMN (last_column->data)->reorderable &&
	  GTK_TREE_VIEW_COLUMN (last_column->data)->visible)
	break;
      last_column = last_column->prev;
    }

  /* No headers are visible, or are focusable.  We can't focus in or out.
   */
  if (last_column == NULL)
    return FALSE;

  first_column = tree_view->priv->columns;
  while (first_column)
    {
      if (GTK_WIDGET_CAN_FOCUS (GTK_TREE_VIEW_COLUMN (first_column->data)->button) &&
	  GTK_TREE_VIEW_COLUMN (first_column->data)->clickable &&
	  GTK_TREE_VIEW_COLUMN (last_column->data)->reorderable &&
	  GTK_TREE_VIEW_COLUMN (first_column->data)->visible)
	break;
      first_column = first_column->next;
    }

  switch (dir)
    {
    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_UP:
    case GTK_DIR_DOWN:
      if (focus_child == NULL)
	{
	  if (tree_view->priv->focus_column != NULL)
	    focus_child = tree_view->priv->focus_column->button;
	  else
	    focus_child = GTK_TREE_VIEW_COLUMN (first_column->data)->button;
	  gtk_widget_grab_focus (focus_child);
	  break;
	}
      return FALSE;

    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
      if (focus_child == NULL)
	{
	  if (tree_view->priv->focus_column != NULL)
	    focus_child = tree_view->priv->focus_column->button;
	  else if (dir == GTK_DIR_LEFT)
	    focus_child = GTK_TREE_VIEW_COLUMN (last_column->data)->button;
	  else
	    focus_child = GTK_TREE_VIEW_COLUMN (first_column->data)->button;
	  gtk_widget_grab_focus (focus_child);
	  break;
	}

      if (gtk_widget_child_focus (focus_child, dir))
	{
	  /* The focus moves inside the button. */
	  /* This is probably a great example of bad UI */
	  break;
	}

      /* We need to move the focus among the row of buttons. */
      for (tmp_list = tree_view->priv->columns; tmp_list; tmp_list = tmp_list->next)
	if (GTK_TREE_VIEW_COLUMN (tmp_list->data)->button == focus_child)
	  break;

      if (tmp_list == first_column && dir == GTK_DIR_LEFT)
	{
	  focus_child = GTK_TREE_VIEW_COLUMN (last_column->data)->button;
	  gtk_widget_grab_focus (focus_child);
	  break;
	}
      else if (tmp_list == last_column && dir == GTK_DIR_RIGHT)
	{
	  focus_child = GTK_TREE_VIEW_COLUMN (first_column->data)->button;
	  gtk_widget_grab_focus (focus_child);
	  break;
	}

      while (tmp_list)
	{
	  GtkTreeViewColumn *column;

	  if (dir == GTK_DIR_RIGHT)
	    tmp_list = tmp_list->next;
	  else
	    tmp_list = tmp_list->prev;

	  if (tmp_list == NULL)
	    {
	      g_warning ("Internal button not found");
	      break;
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
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  /* if focus child is non-null, we assume it's been set to the current focus child
   */
  if (focus_child)
    {
      for (tmp_list = tree_view->priv->columns; tmp_list; tmp_list = tmp_list->next)
	if (GTK_TREE_VIEW_COLUMN (tmp_list->data)->button == focus_child)
	  break;

      tree_view->priv->focus_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);

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

static gint
gtk_tree_view_focus (GtkWidget        *widget,
		     GtkDirectionType  direction)
{
  GtkTreeView *tree_view;
  GtkWidget *focus_child;
  GtkContainer *container;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);
  g_return_val_if_fail (GTK_WIDGET_VISIBLE (widget), FALSE);

  container = GTK_CONTAINER (widget);
  tree_view = GTK_TREE_VIEW (widget);

  if (!GTK_WIDGET_IS_SENSITIVE (container))
    return FALSE;

  focus_child = container->focus_child;

  gtk_tree_view_stop_editing (GTK_TREE_VIEW (widget), FALSE);
  /* Case 1.  Headers currently have focus. */
  if (focus_child)
    {
      switch (direction)
	{
	case GTK_DIR_LEFT:
	case GTK_DIR_RIGHT:
	  gtk_tree_view_header_focus (tree_view, direction);
	  return TRUE;
	case GTK_DIR_TAB_BACKWARD:
	case GTK_DIR_UP:
	  return FALSE;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_DOWN:
	  if (tree_view->priv->tree == NULL)
	    return FALSE;
	  gtk_tree_view_focus_to_cursor (tree_view);
	  return TRUE;
	}
    }

  /* Case 2. We don't have focus at all. */
  if (!GTK_WIDGET_HAS_FOCUS (container))
    {
      if (tree_view->priv->tree == NULL &&
	  (direction == GTK_DIR_TAB_BACKWARD ||
	   direction == GTK_DIR_UP))
	return gtk_tree_view_header_focus (tree_view, direction);
      if (((direction == GTK_DIR_TAB_FORWARD) ||
	   (direction == GTK_DIR_RIGHT) ||
	   (direction == GTK_DIR_DOWN) ||
	   (direction == GTK_DIR_LEFT)) &&
	  gtk_tree_view_header_focus (tree_view, direction))
	return TRUE;

      if (tree_view->priv->tree == NULL)
	return FALSE;
      gtk_tree_view_focus_to_cursor (tree_view);
      return TRUE;
    }

  /* Case 3. We have focus already. */
  if (tree_view->priv->tree == NULL)
    return gtk_tree_view_header_focus (tree_view, direction);

  if (direction == GTK_DIR_TAB_BACKWARD)
    return (gtk_tree_view_header_focus (tree_view, direction));
  else if (direction == GTK_DIR_TAB_FORWARD)
    return FALSE;

  /* Other directions caught by the keybindings */
  gtk_tree_view_focus_to_cursor (tree_view);
  return TRUE;
}


static void
gtk_tree_view_set_focus_child (GtkContainer *container,
			       GtkWidget    *child)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (container);
  GList *list;

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      if (GTK_TREE_VIEW_COLUMN (list->data)->button == child)
	{
	  tree_view->priv->focus_column = GTK_TREE_VIEW_COLUMN (list->data);
	  break;
	}
    }

  (* parent_class->set_focus_child) (container, child);
}

static void
gtk_tree_view_set_adjustments (GtkTreeView   *tree_view,
			       GtkAdjustment *hadj,
			       GtkAdjustment *vadj)
{
  gboolean need_adjust = FALSE;

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


static void
gtk_tree_view_real_move_cursor (GtkTreeView       *tree_view,
				GtkMovementStep    step,
				gint               count)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (step == GTK_MOVEMENT_LOGICAL_POSITIONS ||
		    step == GTK_MOVEMENT_VISUAL_POSITIONS ||
		    step == GTK_MOVEMENT_DISPLAY_LINES ||
		    step == GTK_MOVEMENT_PAGES ||
		    step == GTK_MOVEMENT_BUFFER_ENDS);

  if (tree_view->priv->tree == NULL)
    return;
  gtk_tree_view_stop_editing (tree_view, FALSE);
  GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS);
  gtk_widget_grab_focus (GTK_WIDGET (tree_view));

  switch (step)
    {
      /* currently we make no distinction.  When we go bi-di, we need to */
    case GTK_MOVEMENT_LOGICAL_POSITIONS:
    case GTK_MOVEMENT_VISUAL_POSITIONS:
      gtk_tree_view_move_cursor_left_right (tree_view, count);
      break;
    case GTK_MOVEMENT_DISPLAY_LINES:
      gtk_tree_view_move_cursor_up_down (tree_view, count);
      break;
    case GTK_MOVEMENT_PAGES:
      gtk_tree_view_move_cursor_page_up_down (tree_view, count);
      break;
    case GTK_MOVEMENT_BUFFER_ENDS:
      gtk_tree_view_move_cursor_start_end (tree_view, count);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
gtk_tree_view_put (GtkTreeView *tree_view,
		   GtkWidget   *child_widget,
		   gint         x,
		   gint         y,
		   gint         width,
		   gint         height)
{
  GtkTreeViewChild *child;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_IS_WIDGET (child_widget));

  child = g_new (GtkTreeViewChild, 1);

  child->widget = child_widget;
  child->x = x;
  child->y = y;
  child->width = width;
  child->height = height;

  tree_view->priv->children = g_list_append (tree_view->priv->children, child);

  if (GTK_WIDGET_REALIZED (tree_view))
    gtk_widget_set_parent_window (child->widget, tree_view->priv->bin_window);

  gtk_widget_set_parent (child_widget, GTK_WIDGET (tree_view));
}

void
_gtk_tree_view_child_move_resize (GtkTreeView *tree_view,
				  GtkWidget   *widget,
				  gint         x,
				  gint         y,
				  gint         width,
				  gint         height)
{
  GtkTreeViewChild *child = NULL;
  GList *list;
  GdkRectangle allocation;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  for (list = tree_view->priv->children; list; list = list->next)
    {
      if (((GtkTreeViewChild *)list->data)->widget == widget)
	{
	  child = list->data;
	  break;
	}
    }
  if (child == NULL)
    return;

  allocation.x = child->x = x;
  allocation.y = child->y = y;
  allocation.width = child->width = width;
  allocation.height = child->height = height;

  if (GTK_WIDGET_REALIZED (widget))
    gtk_widget_size_allocate (widget, &allocation);
}


/* TreeModel Callbacks
 */

static void
gtk_tree_view_row_changed (GtkTreeModel *model,
			   GtkTreePath  *path,
			   GtkTreeIter  *iter,
			   gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *)data;
  GtkRBTree *tree;
  GtkRBNode *node;
  gboolean free_path = FALSE;
  gint vertical_separator;
  GList *list;

  g_return_if_fail (path != NULL || iter != NULL);

  if (!GTK_WIDGET_REALIZED (tree_view))
    /* We can just ignore ::changed signals if we aren't realized, as we don't care about sizes
     */
    return;

  gtk_widget_style_get (GTK_WIDGET (data), "vertical_separator", &vertical_separator, NULL);

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

  _gtk_rbtree_node_mark_invalid (tree, node);
  for (list = tree_view->priv->columns; list; list = list->next)
    {
      GtkTreeViewColumn *column;

      column = list->data;
      if (! column->visible)
	continue;

      if (column->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
	{
	  gtk_tree_view_column_cell_set_dirty (column);
	}
    }

 done:
  install_presize_handler (tree_view);
  if (free_path)
    gtk_tree_path_free (path);
}

static void
gtk_tree_view_row_inserted (GtkTreeModel *model,
			    GtkTreePath  *path,
			    GtkTreeIter  *iter,
			    gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *) data;
  gint *indices;
  GtkRBTree *tmptree, *tree;
  GtkRBNode *tmpnode = NULL;
  gint depth;
  gint i = 0;
  gboolean free_path = FALSE;

  g_return_if_fail (path != NULL || iter != NULL);

  if (path == NULL)
    {
      path = gtk_tree_model_get_path (model, iter);
      free_path = TRUE;
    }
  else if (iter == NULL)
    gtk_tree_model_get_iter (model, iter, path);

  if (tree_view->priv->tree == NULL)
    tree_view->priv->tree = _gtk_rbtree_new ();

  tmptree = tree = tree_view->priv->tree;

  /* Update all row-references */
  gtk_tree_row_reference_inserted (G_OBJECT (data), path);
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
	  /* In theory, the model should have emitted has_child_toggled here.  We
	   * try to catch it anyway, just to be safe, in case the model hasn't.
	   */
	  GtkTreePath *tmppath = _gtk_tree_view_find_path (tree_view,
							   tree,
							   tmpnode);
	  gtk_tree_view_row_has_child_toggled (model, tmppath, NULL, data);
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
  gtk_tree_model_ref_node (tree_view->priv->model, iter);
  if (indices[depth - 1] == 0)
    {
      tmpnode = _gtk_rbtree_find_count (tree, 1);
      _gtk_rbtree_insert_before (tree, tmpnode, 0, FALSE);
    }
  else
    {
      tmpnode = _gtk_rbtree_find_count (tree, indices[depth - 1]);
      _gtk_rbtree_insert_after (tree, tmpnode, 0, FALSE);
    }

 done:
  install_presize_handler (tree_view);
  if (free_path)
    gtk_tree_path_free (path);
}

static void
gtk_tree_view_row_has_child_toggled (GtkTreeModel *model,
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
		gtk_tree_view_column_cell_set_dirty (GTK_TREE_VIEW_COLUMN (list->data));
		break;
	      }
	}
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }
  else
    {
      _gtk_tree_view_queue_draw_node (tree_view, tree, node, NULL);
    }

 done:
  if (free_path)
    gtk_tree_path_free (path);
}

static void
count_children_helper (GtkRBTree *tree,
		       GtkRBNode *node,
		       gpointer   data)
{
  if (node->children)
    _gtk_rbtree_traverse (node->children, node->children->root, G_POST_ORDER, count_children_helper, data);
  (*((gint *)data))++;
}

static void
gtk_tree_view_row_deleted (GtkTreeModel *model,
			   GtkTreePath  *path,
			   gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *)data;
  GtkRBTree *tree;
  GtkRBNode *node;
  GList *list;
  gint selection_changed;

  g_return_if_fail (path != NULL);

  gtk_tree_row_reference_deleted (G_OBJECT (data), path);

  if (_gtk_tree_view_find_node (tree_view, path, &tree, &node))
    return;

  if (tree == NULL)
    return;

  /* Change the selection */
  selection_changed = GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED);

  for (list = tree_view->priv->columns; list; list = list->next)
    if (((GtkTreeViewColumn *)list->data)->visible &&
	((GtkTreeViewColumn *)list->data)->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
      gtk_tree_view_column_cell_set_dirty ((GtkTreeViewColumn *)list->data);

  /* Ensure we don't have a dangling pointer to a dead node */
  ensure_unprelighted (tree_view);

  /* Cancel editting if we've started */
  gtk_tree_view_stop_editing (tree_view, TRUE);

  /* If we have a node expanded/collapsed timeout, remove it */
  if (tree_view->priv->expand_collapse_timeout != 0)
    {
      gtk_timeout_remove (tree_view->priv->expand_collapse_timeout);
      tree_view->priv->expand_collapse_timeout = 0;

      /* Reset node */
      GTK_RBNODE_UNSET_FLAG (tree_view->priv->expanded_collapsed_node, GTK_RBNODE_IS_SEMI_COLLAPSED);
      GTK_RBNODE_UNSET_FLAG (tree_view->priv->expanded_collapsed_node, GTK_RBNODE_IS_SEMI_EXPANDED);
      tree_view->priv->expanded_collapsed_node = NULL;
    }

  if (tree_view->priv->destroy_count_func)
    {
      gint child_count = 0;
      if (node->children)
	_gtk_rbtree_traverse (node->children, node->children->root, G_POST_ORDER, count_children_helper, &child_count);
      (* tree_view->priv->destroy_count_func) (tree_view, path, child_count, tree_view->priv->destroy_count_data);
    }

  if (tree->root->count == 1)
    {
      if (tree_view->priv->tree == tree)
	tree_view->priv->tree = NULL;

      _gtk_rbtree_remove (tree);
    }
  else
    {
      _gtk_rbtree_remove_node (tree, node);
    }

  gtk_widget_queue_resize (GTK_WIDGET (tree_view));

  if (selection_changed)
    g_signal_emit_by_name (G_OBJECT (tree_view->priv->selection), "changed");
}


static void
gtk_tree_view_rows_reordered (GtkTreeModel *model,
			      GtkTreePath  *parent,
			      GtkTreeIter  *iter,
			      gint         *new_order,
			      gpointer      data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (data);
  GtkRBTree *tree;
  GtkRBNode *node;
  gint len;

  len = gtk_tree_model_iter_n_children (model, iter);

  if (len < 2)
    return;

  gtk_tree_row_reference_reordered (G_OBJECT (data),
				    parent,
				    iter,
				    new_order);

  if (_gtk_tree_view_find_node (tree_view,
				parent,
				&tree,
				&node))
    return;

  /* We need to special case the parent path */
  if (tree == NULL)
    tree = tree_view->priv->tree;
  else
    tree = node->children;

  if (tree == NULL)
    return;

  /* we need to be unprelighted */
  ensure_unprelighted (tree_view);
  
  _gtk_rbtree_reorder (tree, new_order, len);

  gtk_widget_queue_draw (GTK_WIDGET (tree_view));
}


/* Internal tree functions
 */


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
gtk_tree_view_get_arrow_xrange (GtkTreeView *tree_view,
				GtkRBTree   *tree,
                                gint        *x1,
                                gint        *x2)
{
  gint x_offset = 0;
  GList *list;
  GtkTreeViewColumn *tmp_column = NULL;
  gint total_width;
  gboolean indent_expanders;

  total_width = 0;
  for (list = tree_view->priv->columns; list; list = list->next)
    {
      tmp_column = list->data;

      if (gtk_tree_view_is_expander_column (tree_view, tmp_column))
        {
          x_offset = total_width;
          break;
        }

      if (tmp_column->visible)
        total_width += tmp_column->width;
    }

  gtk_widget_style_get (GTK_WIDGET (tree_view),
			"indent_expanders", &indent_expanders,
			NULL);

  if (indent_expanders)
    x_offset += tree_view->priv->tab_offset * _gtk_rbtree_get_depth (tree);

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

static void
gtk_tree_view_build_tree (GtkTreeView *tree_view,
			  GtkRBTree   *tree,
			  GtkTreeIter *iter,
			  gint         depth,
			  gboolean     recurse)
{
  GtkRBNode *temp = NULL;

  do
    {
      gtk_tree_model_ref_node (tree_view->priv->model, iter);
      temp = _gtk_rbtree_insert_after (tree, temp, 0, FALSE);
      if (recurse)
	{
	  GtkTreeIter child;

	  if (gtk_tree_model_iter_children (tree_view->priv->model, &child, iter))
	    {
	      temp->children = _gtk_rbtree_new ();
	      temp->children->parent_tree = tree;
	      temp->children->parent_node = temp;
	      gtk_tree_view_build_tree (tree_view, temp->children, &child, depth + 1, recurse);
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

/* If height is non-NULL, then we set it to be the new height.  if it's all
 * dirty, then height is -1.  We know we'll remeasure dirty rows, anyways.
 */
static gboolean
gtk_tree_view_discover_dirty_iter (GtkTreeView *tree_view,
				   GtkTreeIter *iter,
				   gint         depth,
				   gint        *height,
				   GtkRBNode   *node)
{
  GtkTreeViewColumn *column;
  GList *list;
  gboolean retval = FALSE;
  gint tmpheight;
  gint horizontal_separator;

  gtk_widget_style_get (GTK_WIDGET (tree_view),
			"horizontal_separator", &horizontal_separator,
			NULL);


  if (height)
    *height = -1;

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      gint width;
      column = list->data;
      if (column->dirty == TRUE)
	continue;
      if (height == NULL && column->column_type == GTK_TREE_VIEW_COLUMN_FIXED)
	continue;
      if (!column->visible)
	continue;

      gtk_tree_view_column_cell_set_cell_data (column, tree_view->priv->model, iter,
					       GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT),
					       node->children?TRUE:FALSE);

      if (height)
	{
	  gtk_tree_view_column_cell_get_size (column,
					      NULL, NULL, NULL,
					      &width, &tmpheight);
	  *height = MAX (*height, tmpheight);
	}
      else
	{
	  gtk_tree_view_column_cell_get_size (column,
					      NULL, NULL, NULL,
					      &width, NULL);
	}

      if (gtk_tree_view_is_expander_column (tree_view, column) &&
          TREE_VIEW_DRAW_EXPANDERS (tree_view))
	{
	  if (depth * tree_view->priv->tab_offset + horizontal_separator + width > column->requested_width)
	    {
	      gtk_tree_view_column_cell_set_dirty (column);
	      retval = TRUE;
	    }
	}
      else
	{
	  if (horizontal_separator + width > column->requested_width)
	    {
	      gtk_tree_view_column_cell_set_dirty (column);
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
      TREE_VIEW_INTERNAL_ASSERT_VOID (temp != NULL);
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
					 NULL,
					 temp);
      if (gtk_tree_model_iter_children (tree_view->priv->model, &child, iter) &&
	  temp->children != NULL)
	gtk_tree_view_discover_dirty (tree_view, temp->children, &child, depth + 1);
      temp = _gtk_rbtree_next (tree, temp);
    }
  while (gtk_tree_model_iter_next (tree_view->priv->model, iter));
}


/* Make sure the node is visible vertically */
static void
gtk_tree_view_clamp_node_visible (GtkTreeView *tree_view,
				  GtkRBTree   *tree,
				  GtkRBNode   *node)
{
  gint offset;

  /* We process updates because we want to clear old selected items when we scroll.
   * if this is removed, we get a "selection streak" at the bottom. */
  if (GTK_WIDGET_REALIZED (tree_view))
    gdk_window_process_updates (tree_view->priv->bin_window, TRUE);

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

static void
gtk_tree_view_clamp_column_visible (GtkTreeView       *tree_view,
				    GtkTreeViewColumn *column)
{
  if (column == NULL)
    return;
  if ((tree_view->priv->hadjustment->value + tree_view->priv->hadjustment->page_size) <
      (column->button->allocation.x + column->button->allocation.width))
    gtk_adjustment_set_value (tree_view->priv->hadjustment,
			      column->button->allocation.x + column->button->allocation.width -
			      tree_view->priv->hadjustment->page_size);
  else if (tree_view->priv->hadjustment->value > column->button->allocation.x)
    gtk_adjustment_set_value (tree_view->priv->hadjustment,
			      column->button->allocation.x);
}

/* This function could be more efficient.  I'll optimize it if profiling seems
 * to imply that it is important */
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

/* Returns TRUE if we ran out of tree before finding the path.  If the path is
 * invalid (ie. points to a node that's not in the tree), *tree and *node are
 * both set to NULL.
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

  if (depth == 0 || tmptree == NULL)
    return FALSE;
  do
    {
      tmpnode = _gtk_rbtree_find_count (tmptree, indices[i] + 1);
      ++i;
      if (tmpnode == NULL)
	{
	  *tree = NULL;
	  *node = NULL;
	  return FALSE;
	}
      if (i >= depth)
	{
	  *tree = tmptree;
	  *node = tmpnode;
	  return FALSE;
	}
      *tree = tmptree;
      *node = tmpnode;
      tmptree = tmpnode->children;
      if (tmptree == NULL)
	return TRUE;
    }
  while (1);
}

static gboolean
gtk_tree_view_is_expander_column (GtkTreeView       *tree_view,
				  GtkTreeViewColumn *column)
{
  GList *list;

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IS_LIST))
    return FALSE;

  if (tree_view->priv->expander_column != NULL)
    {
      if (tree_view->priv->expander_column == column)
	return TRUE;
      return FALSE;
    }
  else
    {
      for (list = tree_view->priv->columns; list; list = list->next)
	if (((GtkTreeViewColumn *)list->data)->visible)
	  break;
      if (list && list->data == column)
	return TRUE;
    }
  return FALSE;
}

static void
gtk_tree_view_add_move_binding (GtkBindingSet  *binding_set,
				guint           keyval,
				guint           modmask,
				GtkMovementStep step,
				gint            count)
{

  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
                                "move_cursor", 2,
                                GTK_TYPE_ENUM, step,
                                GTK_TYPE_INT, count);

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_SHIFT_MASK,
                                "move_cursor", 2,
                                GTK_TYPE_ENUM, step,
                                GTK_TYPE_INT, count);

  if ((modmask & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
   return;

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "move_cursor", 2,
                                GTK_TYPE_ENUM, step,
                                GTK_TYPE_INT, count);

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_CONTROL_MASK,
                                "move_cursor", 2,
                                GTK_TYPE_ENUM, step,
                                GTK_TYPE_INT, count);
}

static gint
gtk_tree_view_unref_tree_helper (GtkTreeModel *model,
				 GtkTreeIter  *iter,
				 GtkRBTree    *tree,
				 GtkRBNode    *node)
{
  gint retval = FALSE;
  do
    {
      g_return_val_if_fail (node != NULL, FALSE);

      if (node->children)
	{
	  GtkTreeIter child;
	  GtkRBTree *new_tree;
	  GtkRBNode *new_node;

	  new_tree = node->children;
	  new_node = new_tree->root;

	  while (new_node && new_node->left != new_tree->nil)
	    new_node = new_node->left;

	  g_return_val_if_fail (gtk_tree_model_iter_children (model, &child, iter), FALSE);
	  retval = retval || gtk_tree_view_unref_tree_helper (model, &child, new_tree, new_node);
	}

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
	retval = TRUE;
      gtk_tree_model_unref_node (model, iter);
      node = _gtk_rbtree_next (tree, node);
    }
  while (gtk_tree_model_iter_next (model, iter));

  return retval;
}

static gint
gtk_tree_view_unref_and_check_selection_tree (GtkTreeView *tree_view,
					      GtkRBTree   *tree)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkRBNode *node;
  gint retval;

  if (!tree)
    return FALSE;

  node = tree->root;
  while (node && node->left != tree->nil)
    node = node->left;

  g_return_val_if_fail (node != NULL, FALSE);
  path = _gtk_tree_view_find_path (tree_view, tree, node);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_view->priv->model),
			   &iter, path);
  retval = gtk_tree_view_unref_tree_helper (GTK_TREE_MODEL (tree_view->priv->model), &iter, tree, node);
  gtk_tree_path_free (path);

  return retval;
}

static void
gtk_tree_view_set_column_drag_info (GtkTreeView       *tree_view,
				    GtkTreeViewColumn *column)
{
  GtkTreeViewColumn *left_column;
  GtkTreeViewColumn *cur_column = NULL;
  GtkTreeViewColumnReorder *reorder;

  GList *tmp_list;
  gint left;

  /* We want to precalculate the motion list such that we know what column slots
   * are available.
   */
  left_column = NULL;

  /* First, identify all possible drop spots */
  tmp_list = tree_view->priv->columns;

  while (tmp_list)
    {
      g_assert (tmp_list);

      cur_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);
      tmp_list = tmp_list->next;

      if (cur_column->visible == FALSE)
	continue;

      /* If it's not the column moving and func tells us to skip over the column, we continue. */
      if (left_column != column && cur_column != column &&
	  tree_view->priv->column_drop_func &&
	  ! (* tree_view->priv->column_drop_func) (tree_view, column, left_column, cur_column, tree_view->priv->column_drop_func_data))
	{
	  left_column = cur_column;
	  continue;
	}
      reorder = g_new (GtkTreeViewColumnReorder, 1);
      reorder->left_column = left_column;
      left_column = reorder->right_column = cur_column;

      tree_view->priv->column_drag_info = g_list_append (tree_view->priv->column_drag_info, reorder);
    }

  /* Add the last one */
  if (tree_view->priv->column_drop_func == NULL ||
      ((left_column != column) &&
       (* tree_view->priv->column_drop_func) (tree_view, column, left_column, cur_column, tree_view->priv->column_drop_func_data)))
    {
      reorder = g_new (GtkTreeViewColumnReorder, 1);
      reorder->left_column = left_column;
      reorder->right_column = NULL;
      tree_view->priv->column_drag_info = g_list_append (tree_view->priv->column_drag_info, reorder);
    }

  /* We quickly check to see if it even makes sense to reorder columns. */
  /* If there is nothing that can be moved, then we return */

  if (tree_view->priv->column_drag_info == NULL)
    return;

  /* We know there are always 2 slots possbile, as you can always return column. */
  /* If that's all there is, return */
  if (tree_view->priv->column_drag_info->next->next == NULL &&
      ((GtkTreeViewColumnReorder *)tree_view->priv->column_drag_info->data)->right_column == column &&
      ((GtkTreeViewColumnReorder *)tree_view->priv->column_drag_info->next->data)->left_column == column)
    {
      for (tmp_list = tree_view->priv->column_drag_info; tmp_list; tmp_list = tmp_list->next)
	g_free (tmp_list->data);
      g_list_free (tree_view->priv->column_drag_info);
      tree_view->priv->column_drag_info = NULL;
      return;
    }
  /* We fill in the ranges for the columns, now that we've isolated them */
  left = - TREE_VIEW_COLUMN_DRAG_DEAD_MULTIPLIER (tree_view);

  for (tmp_list = tree_view->priv->column_drag_info; tmp_list; tmp_list = tmp_list->next)
    {
      reorder = (GtkTreeViewColumnReorder *) tmp_list->data;

      reorder->left_align = left;
      if (tmp_list->next != NULL)
	{
	  g_assert (tmp_list->next->data);
	  left = reorder->right_align = (reorder->right_column->button->allocation.x +
					 reorder->right_column->button->allocation.width +
					 ((GtkTreeViewColumnReorder *)tmp_list->next->data)->left_column->button->allocation.x)/2;
	}
      else
	{
	  gint width;

	  gdk_window_get_size (tree_view->priv->header_window, &width, NULL);
	  reorder->right_align = width + TREE_VIEW_COLUMN_DRAG_DEAD_MULTIPLIER (tree_view);
	}
    }
}

void
_gtk_tree_view_column_start_drag (GtkTreeView       *tree_view,
				  GtkTreeViewColumn *column)
{
  GdkEvent send_event;
  GtkAllocation allocation;
  gint x, y, width, height;

  g_return_if_fail (tree_view->priv->column_drag_info == NULL);

  gtk_tree_view_set_column_drag_info (tree_view, column);

  if (tree_view->priv->column_drag_info == NULL)
    return;

  if (tree_view->priv->drag_window == NULL)
    {
      GdkWindowAttr attributes;
      guint attributes_mask;

      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (GTK_WIDGET (tree_view));
      attributes.colormap = gtk_widget_get_colormap (GTK_WIDGET (tree_view));
      attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

      tree_view->priv->drag_window = gdk_window_new (tree_view->priv->bin_window,
						     &attributes,
						     attributes_mask);
      gdk_window_set_user_data (tree_view->priv->drag_window, GTK_WIDGET (tree_view));
    }

  gdk_pointer_ungrab (GDK_CURRENT_TIME);
  gdk_keyboard_ungrab (GDK_CURRENT_TIME);

  gtk_grab_remove (column->button);

  send_event.crossing.type = GDK_LEAVE_NOTIFY;
  send_event.crossing.send_event = TRUE;
  send_event.crossing.window = column->button->window;
  send_event.crossing.subwindow = NULL;
  send_event.crossing.detail = GDK_NOTIFY_ANCESTOR;
  send_event.crossing.time = GDK_CURRENT_TIME;

  gtk_propagate_event (column->button, &send_event);

  send_event.button.type = GDK_BUTTON_RELEASE;
  send_event.button.window = GDK_ROOT_PARENT ();
  send_event.button.send_event = TRUE;
  send_event.button.time = GDK_CURRENT_TIME;
  send_event.button.x = -1;
  send_event.button.y = -1;
  send_event.button.axes = NULL;
  send_event.button.state = 0;
  send_event.button.button = 1;
  send_event.button.device = gdk_device_get_core_pointer ();
  send_event.button.x_root = 0;
  send_event.button.y_root = 0;

  gtk_propagate_event (column->button, &send_event);

  gdk_window_move_resize (tree_view->priv->drag_window,
			  column->button->allocation.x,
			  0,
			  column->button->allocation.width,
			  column->button->allocation.height);

  /* Kids, don't try this at home */
  g_object_ref (column->button);
  gtk_container_remove (GTK_CONTAINER (tree_view), column->button);
  gtk_widget_set_parent_window (column->button, tree_view->priv->drag_window);
  gtk_widget_set_parent (column->button, GTK_WIDGET (tree_view));
  g_object_unref (column->button);

  tree_view->priv->drag_column_x = column->button->allocation.x;
  allocation = column->button->allocation;
  allocation.x = 0;
  gtk_widget_size_allocate (column->button, &allocation);
  gtk_widget_set_parent_window (column->button, tree_view->priv->drag_window);

  tree_view->priv->drag_column = column;
  gdk_window_show (tree_view->priv->drag_window);

  gdk_window_get_origin (tree_view->priv->header_window, &x, &y);
  gdk_window_get_size (tree_view->priv->header_window, &width, &height);

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));
  while (gtk_events_pending ())
    gtk_main_iteration ();

  GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_IN_COLUMN_DRAG);
  gdk_pointer_grab (tree_view->priv->drag_window,
		    FALSE,
		    GDK_POINTER_MOTION_MASK|GDK_BUTTON_RELEASE_MASK,
		    NULL, NULL, GDK_CURRENT_TIME);
  gdk_keyboard_grab (tree_view->priv->drag_window,
		     FALSE,
		     GDK_CURRENT_TIME);

}

static void
gtk_tree_view_queue_draw_arrow (GtkTreeView      *tree_view,
				GtkRBTree        *tree,
				GtkRBNode        *node,
				GdkRectangle     *clip_rect)
{
  GdkRectangle rect;

  if (!GTK_WIDGET_REALIZED (tree_view))
    return;

  rect.x = 0;
  rect.width = MAX (tree_view->priv->tab_offset, GTK_WIDGET (tree_view)->allocation.width);

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

void
_gtk_tree_view_queue_draw_node (GtkTreeView  *tree_view,
				GtkRBTree    *tree,
				GtkRBNode    *node,
				GdkRectangle *clip_rect)
{
  GdkRectangle rect;

  if (!GTK_WIDGET_REALIZED (tree_view))
    return;

  rect.x = 0;
  rect.width = MAX (tree_view->priv->width, GTK_WIDGET (tree_view)->allocation.width);

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
    _gtk_tree_view_queue_draw_node (tree_view, tree, node, clip_rect);
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
  gint vertical_separator;
  gint expander_size;
  GtkExpanderStyle expander_style;

  gtk_widget_style_get (GTK_WIDGET (tree_view),
			"vertical_separator", &vertical_separator,
			"expander_size", &expander_size,
			NULL);

  if (! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT))
    return;

  widget = GTK_WIDGET (tree_view);

  gtk_tree_view_get_arrow_xrange (tree_view, tree, &x_offset, NULL);

  area.x = x_offset;
  area.y = CELL_FIRST_PIXEL (tree_view, tree, node, vertical_separator);
  area.width = expander_size + 2;
  area.height = CELL_HEIGHT (node, vertical_separator);

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

  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SEMI_EXPANDED))
    expander_style = GTK_EXPANDER_SEMI_EXPANDED;
  else if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SEMI_COLLAPSED))
    expander_style = GTK_EXPANDER_SEMI_COLLAPSED;
  else if (node->children != NULL)
    expander_style = GTK_EXPANDER_EXPANDED;
  else
    expander_style = GTK_EXPANDER_COLLAPSED;

  gtk_paint_expander (widget->style,
                      tree_view->priv->bin_window,
                      state,
                      &area,
                      widget,
                      "treeview",
		      area.x + area.width / 2,
		      area.y + area.height / 2,
		      expander_style);
}

static void
gtk_tree_view_focus_to_cursor (GtkTreeView *tree_view)

{
  GtkTreePath *cursor_path;

  if ((tree_view->priv->tree == NULL) ||
      (! GTK_WIDGET_REALIZED (tree_view)))
    return;

  GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS);
  gtk_widget_grab_focus (GTK_WIDGET (tree_view));

  cursor_path = NULL;
  if (tree_view->priv->cursor)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

  if (cursor_path == NULL)
    {
      cursor_path = gtk_tree_path_new_root ();
      gtk_tree_row_reference_free (tree_view->priv->cursor);
      tree_view->priv->cursor = NULL;

      if (tree_view->priv->selection->type == GTK_SELECTION_MULTIPLE)
	gtk_tree_view_real_set_cursor (tree_view, cursor_path, FALSE);
      else
	gtk_tree_view_real_set_cursor (tree_view, cursor_path, TRUE);
    }
  gtk_tree_path_free (cursor_path);
  if (tree_view->priv->focus_column == NULL)
    {
      GList *list;
      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  if (GTK_TREE_VIEW_COLUMN (list->data)->visible)
	    {
	      tree_view->priv->focus_column = GTK_TREE_VIEW_COLUMN (list->data);
	      break;
	    }
	}
    }
}

static void
gtk_tree_view_move_cursor_up_down (GtkTreeView *tree_view,
				   gint         count)
{
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *cursor_node = NULL;
  GtkRBTree *new_cursor_tree = NULL;
  GtkRBNode *new_cursor_node = NULL;
  GtkTreePath *cursor_path = NULL;

  cursor_path = NULL;
  if (!gtk_tree_row_reference_valid (tree_view->priv->cursor))
    /* FIXME: we lost the cursor; should we get the first? */
    return;

  cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
  _gtk_tree_view_find_node (tree_view, cursor_path,
			    &cursor_tree, &cursor_node);
  gtk_tree_path_free (cursor_path);

  if (cursor_tree == NULL)
    /* FIXME: we lost the cursor; should we get the first? */
    return;
  if (count == -1)
    _gtk_rbtree_prev_full (cursor_tree, cursor_node,
			   &new_cursor_tree, &new_cursor_node);
  else
    _gtk_rbtree_next_full (cursor_tree, cursor_node,
			   &new_cursor_tree, &new_cursor_node);

  if (new_cursor_node)
    {
      cursor_path = _gtk_tree_view_find_path (tree_view, new_cursor_tree, new_cursor_node);
      gtk_tree_view_real_set_cursor (tree_view, cursor_path, TRUE);
      gtk_tree_path_free (cursor_path);
    }
  else
    {
      gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);
    }

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));
}

static void
gtk_tree_view_move_cursor_page_up_down (GtkTreeView *tree_view,
					gint         count)
{
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *cursor_node = NULL;
  GtkTreePath *cursor_path = NULL;
  gint y;
  gint vertical_separator;

  if (gtk_tree_row_reference_valid (tree_view->priv->cursor))
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
  else
    /* This is sorta weird.  Focus in should give us a cursor */
    return;

  gtk_widget_style_get (GTK_WIDGET (tree_view), "vertical_separator", &vertical_separator, NULL);
  _gtk_tree_view_find_node (tree_view, cursor_path,
			    &cursor_tree, &cursor_node);

  gtk_tree_path_free (cursor_path);

  if (cursor_tree == NULL)
    /* FIXME: we lost the cursor.  Should we try to get one? */
    return;
  g_return_if_fail (cursor_node != NULL);

  y = CELL_FIRST_PIXEL (tree_view, cursor_tree, cursor_node, vertical_separator);
  y += count * tree_view->priv->vadjustment->page_size;
  y = CLAMP (y, (gint)tree_view->priv->vadjustment->lower,  (gint)tree_view->priv->vadjustment->upper - vertical_separator);

  _gtk_rbtree_find_offset (tree_view->priv->tree, y, &cursor_tree, &cursor_node);
  cursor_path = _gtk_tree_view_find_path (tree_view, cursor_tree, cursor_node);
  g_return_if_fail (cursor_path != NULL);
  gtk_tree_view_real_set_cursor (tree_view, cursor_path, TRUE);
}

static void
gtk_tree_view_move_cursor_left_right (GtkTreeView *tree_view,
				      gint         count)
{
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *cursor_node = NULL;
  GtkTreePath *cursor_path = NULL;
  GtkTreeViewColumn *column;
  GtkTreeIter iter;
  GList *list;
  gboolean found_column = FALSE;

  if (gtk_tree_row_reference_valid (tree_view->priv->cursor))
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
  else
    return;

  _gtk_tree_view_find_node (tree_view, cursor_path, &cursor_tree, &cursor_node);
  if (cursor_tree == NULL)
    return;
  if (gtk_tree_model_get_iter (tree_view->priv->model, &iter, cursor_path) == FALSE)
    {
      gtk_tree_path_free (cursor_path);
      return;
    }
  gtk_tree_path_free (cursor_path);

  list = tree_view->priv->columns;
  if (tree_view->priv->focus_column)
    {
      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  if (list->data == tree_view->priv->focus_column)
	    break;
	}
    }

  while (list)
    {
      column = list->data;
      if (column->visible == FALSE)
	goto loop_end;

      gtk_tree_view_column_cell_set_cell_data (column,
					       tree_view->priv->model,
					       &iter,
					       GTK_RBNODE_FLAG_SET (cursor_node, GTK_RBNODE_IS_PARENT),
					       cursor_node->children?TRUE:FALSE);
      if (gtk_tree_view_column_cell_focus (column, count))
	{
	  tree_view->priv->focus_column = column;
	  found_column = TRUE;
	  break;
	}
    loop_end:
      if (count == 1)
	list = list->next;
      else
	list = list->prev;
    }

  if (found_column)
    {
      _gtk_tree_view_queue_draw_node (tree_view,
				     cursor_tree,
				     cursor_node,
				     NULL);
      g_signal_emit (G_OBJECT (tree_view), tree_view_signals[CURSOR_CHANGED], 0);
    }
  gtk_tree_view_clamp_column_visible (tree_view, tree_view->priv->focus_column);
}

static void
gtk_tree_view_move_cursor_start_end (GtkTreeView *tree_view,
				     gint         count)
{
  GtkRBTree *cursor_tree;
  GtkRBNode *cursor_node;
  GtkTreePath *path;

  g_return_if_fail (tree_view->priv->tree != NULL);

  if (count == -1)
    {
      cursor_tree = tree_view->priv->tree;
      cursor_node = cursor_tree->root;
      while (cursor_node && cursor_node->left != cursor_tree->nil)
	cursor_node = cursor_node->left;
    }
  else
    {
      cursor_tree = tree_view->priv->tree;
      cursor_node = cursor_tree->root;
      do
	{
	  while (cursor_node && cursor_node->right != cursor_tree->nil)
	    cursor_node = cursor_node->right;
	  if (cursor_node->children == NULL)
	    break;

	  cursor_tree = cursor_node->children;
	  cursor_node = cursor_tree->root;
	}
      while (1);
    }

  path = _gtk_tree_view_find_path (tree_view, cursor_tree, cursor_node);
  _gtk_tree_selection_internal_select_node (tree_view->priv->selection,
					    cursor_node,
					    cursor_tree,
					    path,
					    FALSE?GDK_SHIFT_MASK:0);

  gtk_tree_row_reference_free (tree_view->priv->cursor);
  tree_view->priv->cursor = gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view), tree_view->priv->model, path);
  gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);
}

static void
gtk_tree_view_real_select_all (GtkTreeView *tree_view)
{
  if (tree_view->priv->selection->type != GTK_SELECTION_MULTIPLE)
    return;
  gtk_tree_selection_select_all (tree_view->priv->selection);
}

static void
gtk_tree_view_real_select_cursor_row (GtkTreeView *tree_view,
				      gboolean     start_editing)
{
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *cursor_node = NULL;
  GtkTreePath *cursor_path = NULL;
  GdkModifierType state = 0;
  cursor_path = NULL;

  if (tree_view->priv->cursor)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

  if (cursor_path == NULL)
    return;

  _gtk_tree_view_find_node (tree_view, cursor_path,
			    &cursor_tree, &cursor_node);

  if (cursor_tree == NULL)
    {
      gtk_tree_path_free (cursor_path);
      return;
    }

  gtk_get_current_event_state (&state);

  if (start_editing && tree_view->priv->focus_column)
    {
      if (gtk_tree_view_start_editing (tree_view, cursor_path))
	{
	  gtk_tree_path_free (cursor_path);
	  return;
	}
    }
  _gtk_tree_selection_internal_select_node (tree_view->priv->selection,
					    cursor_node,
					    cursor_tree,
					    cursor_path,
					    state);

  gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));
  _gtk_tree_view_queue_draw_node (tree_view, cursor_tree, cursor_node, NULL);

  gtk_tree_path_free (cursor_path);
}

static void
gtk_tree_view_real_toggle_cursor_row (GtkTreeView *tree_view)
{
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *cursor_node = NULL;
  GtkTreePath *cursor_path = NULL;

  cursor_path = NULL;
  if (tree_view->priv->cursor)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

  if (cursor_path == NULL)
    return;

  _gtk_tree_view_find_node (tree_view, cursor_path,
			    &cursor_tree, &cursor_node);
  if (cursor_tree == NULL)
    {
      gtk_tree_path_free (cursor_path);
      return;
    }

  _gtk_tree_selection_internal_select_node (tree_view->priv->selection,
					    cursor_node,
					    cursor_tree,
					    cursor_path,
					    GDK_CONTROL_MASK);

  gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));
  gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);
  gtk_tree_path_free (cursor_path);
}



static void
gtk_tree_view_real_expand_collapse_cursor_row (GtkTreeView *tree_view,
					       gboolean     logical,
					       gboolean     expand,
					       gboolean     open_all)
{
  GtkTreePath *cursor_path = NULL;
  GtkRBTree *tree;
  GtkRBNode *node;

  cursor_path = NULL;
  if (tree_view->priv->cursor)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

  if (cursor_path == NULL)
    return;

  if (_gtk_tree_view_find_node (tree_view, cursor_path, &tree, &node))
    return;
  
  gtk_widget_grab_focus (GTK_WIDGET (tree_view));
  gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);

  if (expand)
    gtk_tree_view_real_expand_row (tree_view, cursor_path, tree, node, open_all, TRUE);
  else
    gtk_tree_view_real_collapse_row (tree_view, cursor_path, tree, node, TRUE);

  gtk_tree_path_free (cursor_path);
}

static void
gtk_tree_view_real_select_cursor_parent (GtkTreeView *tree_view)
{
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *cursor_node = NULL;
  GtkTreePath *cursor_path = NULL;

  cursor_path = NULL;
  if (tree_view->priv->cursor)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

  if (cursor_path == NULL)
    return;

  _gtk_tree_view_find_node (tree_view, cursor_path,
			    &cursor_tree, &cursor_node);
  if (cursor_tree == NULL)
    {
      gtk_tree_path_free (cursor_path);
      return;
    }

  if (cursor_tree->parent_node)
    {
      gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);
      cursor_node = cursor_tree->parent_node;
      cursor_tree = cursor_tree->parent_tree;

      gtk_tree_path_up (cursor_path);
      gtk_tree_row_reference_free (tree_view->priv->cursor);
      tree_view->priv->cursor = gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view), tree_view->priv->model, cursor_path);
      _gtk_tree_selection_internal_select_node (tree_view->priv->selection,
						cursor_node,
						cursor_tree,
						cursor_path,
						0);
    }

  gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));
  gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);
  gtk_tree_path_free (cursor_path);
}

static void
gtk_tree_view_real_start_interactive_search (GtkTreeView *tree_view)
{
  GtkWidget *window;
  GtkWidget *entry;
  GtkWidget *search_dialog;

  if (tree_view->priv->enable_search == FALSE ||
      tree_view->priv->search_column < 0)
    return;

  search_dialog = gtk_object_get_data (GTK_OBJECT (tree_view),
				       GTK_TREE_VIEW_SEARCH_DIALOG_KEY);
  if (search_dialog)
    return;

  /* set up window */
  window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_title (GTK_WINDOW (window), "search dialog");
  gtk_container_set_border_width (GTK_CONTAINER (window), 3);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_signal_connect
    (GTK_OBJECT (window), "delete_event",
     GTK_SIGNAL_FUNC (gtk_tree_view_search_delete_event),
     tree_view);
  gtk_signal_connect
    (GTK_OBJECT (window), "key_press_event",
     GTK_SIGNAL_FUNC (gtk_tree_view_search_key_press_event),
     tree_view);
  gtk_signal_connect
    (GTK_OBJECT (window), "button_press_event",
     GTK_SIGNAL_FUNC (gtk_tree_view_search_button_press_event),
     tree_view);

  /* add entry */
  entry = gtk_entry_new ();
  gtk_widget_show (entry);
  gtk_signal_connect
    (GTK_OBJECT (entry), "changed",
     GTK_SIGNAL_FUNC (gtk_tree_view_search_init),
     tree_view);
  gtk_container_add (GTK_CONTAINER (window), entry);

  /* done, show it */
  tree_view->priv->search_dialog_position_func (tree_view, window);
  gtk_widget_show_all (window);
  gtk_widget_grab_focus (entry);

  /* position window */

  /* yes, we point to the entry's private text thing here, a bit evil */
  gtk_object_set_data (GTK_OBJECT (window), "gtk-tree-view-text",
		       (char *) gtk_entry_get_text (GTK_ENTRY (entry)));
  gtk_object_set_data (GTK_OBJECT (tree_view),
		       GTK_TREE_VIEW_SEARCH_DIALOG_KEY, window);

  /* search first matching iter */
  gtk_tree_view_search_init (entry, tree_view);
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
				  GtkTreeView   *tree_view)
{
  if (GTK_WIDGET_REALIZED (tree_view))
    {
      gint dy;
	
      gdk_window_move (tree_view->priv->bin_window,
		       - tree_view->priv->hadjustment->value,
		       TREE_VIEW_HEADER_HEIGHT (tree_view));
      gdk_window_move (tree_view->priv->header_window,
		       - tree_view->priv->hadjustment->value,
		       0);
      dy = tree_view->priv->dy - (int) tree_view->priv->vadjustment->value;
      gdk_window_scroll (tree_view->priv->bin_window, 0, dy);

      /* update our dy and top_row */
      tree_view->priv->dy = (int) tree_view->priv->vadjustment->value;
      gtk_tree_view_dy_to_top_row (tree_view);
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

/* Public Accessors
 */

/**
 * gtk_tree_view_get_model:
 * @tree_view: a #GtkTreeView
 *
 * Returns the model the the #GtkTreeView is based on.  Returns %NULL if the
 * model is unset.
 *
 * Return value: A #GtkTreeModel, or %NULL if none is currently being used.
 **/
GtkTreeModel *
gtk_tree_view_get_model (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->model;
}

/**
 * gtk_tree_view_set_model:
 * @tree_view: A #GtkTreeNode.
 * @model: The model.
 *
 * Sets the model for a #GtkTreeView.  If the @tree_view already has a model
 * set, it will remove it before setting the new model.  If @model is %NULL, then
 * it will unset the old model.
 **/
void
gtk_tree_view_set_model (GtkTreeView  *tree_view,
			 GtkTreeModel *model)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (model == tree_view->priv->model)
    return;

  if (tree_view->priv->model)
    {
      gtk_tree_view_unref_and_check_selection_tree (tree_view, tree_view->priv->tree);

      g_signal_handlers_disconnect_by_func (G_OBJECT (tree_view->priv->model),
					    gtk_tree_view_row_changed, tree_view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (tree_view->priv->model),
					    gtk_tree_view_row_inserted, tree_view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (tree_view->priv->model),
					    gtk_tree_view_row_has_child_toggled, tree_view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (tree_view->priv->model),
					    gtk_tree_view_row_deleted, tree_view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (tree_view->priv->model),
					    gtk_tree_view_rows_reordered, tree_view);
      if (tree_view->priv->tree)
	{
	  _gtk_rbtree_free (tree_view->priv->tree);
	  tree_view->priv->tree = NULL;
	}
      gtk_tree_row_reference_free (tree_view->priv->drag_dest_row);
      tree_view->priv->drag_dest_row = NULL;
      gtk_tree_row_reference_free (tree_view->priv->cursor);
      tree_view->priv->cursor = NULL;
      gtk_tree_row_reference_free (tree_view->priv->anchor);
      tree_view->priv->anchor = NULL;

      g_object_unref (tree_view->priv->model);
      tree_view->priv->search_column = -1;
    }

  tree_view->priv->model = model;


  if (tree_view->priv->model)
    {
      gint i;
      GtkTreePath *path;
      GtkTreeIter iter;


      if (tree_view->priv->search_column == -1)
	{
	  for (i = 0; i < gtk_tree_model_get_n_columns (model); i++)
	    {
	      if (gtk_tree_model_get_column_type (model, i) == G_TYPE_STRING)
		{
		  tree_view->priv->search_column = i;
		  break;
		}
	    }
	}
      g_object_ref (tree_view->priv->model);
      g_signal_connect (tree_view->priv->model,
			"row_changed",
			G_CALLBACK (gtk_tree_view_row_changed),
			tree_view);
      g_signal_connect (tree_view->priv->model,
			"row_inserted",
			G_CALLBACK (gtk_tree_view_row_inserted),
			tree_view);
      g_signal_connect (tree_view->priv->model,
			"row_has_child_toggled",
			G_CALLBACK (gtk_tree_view_row_has_child_toggled),
			tree_view);
      g_signal_connect (tree_view->priv->model,
			"row_deleted",
			G_CALLBACK (gtk_tree_view_row_deleted),
			tree_view);
      g_signal_connect (tree_view->priv->model,
			"rows_reordered",
			G_CALLBACK (gtk_tree_view_rows_reordered),
			tree_view);

      path = gtk_tree_path_new_root ();
      if (gtk_tree_model_get_iter (tree_view->priv->model, &iter, path))
	{
	  tree_view->priv->tree = _gtk_rbtree_new ();
	  gtk_tree_view_build_tree (tree_view, tree_view->priv->tree, &iter, 1, FALSE);
	}
      gtk_tree_path_free (path);

      /*  FIXME: do I need to do this? gtk_tree_view_create_buttons (tree_view); */
    }

  g_object_notify (G_OBJECT (tree_view), "model");

  if (GTK_WIDGET_REALIZED (tree_view))
    gtk_widget_queue_resize (GTK_WIDGET (tree_view));
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
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->selection;
}

/**
 * gtk_tree_view_get_hadjustment:
 * @tree_view: A #GtkTreeView
 *
 * Gets the #GtkAdjustment currently being used for the horizontal aspect.
 *
 * Return value: A #GtkAdjustment object, or %NULL if none is currently being
 * used.
 **/
GtkAdjustment *
gtk_tree_view_get_hadjustment (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  if (tree_view->priv->hadjustment == NULL)
    gtk_tree_view_set_hadjustment (tree_view, NULL);

  return tree_view->priv->hadjustment;
}

/**
 * gtk_tree_view_set_hadjustment:
 * @tree_view: A #GtkTreeView
 * @adjustment: The #GtkAdjustment to set, or %NULL
 *
 * Sets the #GtkAdjustment for the current horizontal aspect.
 **/
void
gtk_tree_view_set_hadjustment (GtkTreeView   *tree_view,
			       GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_tree_view_set_adjustments (tree_view,
				 adjustment,
				 tree_view->priv->vadjustment);

  g_object_notify (G_OBJECT (tree_view), "hadjustment");
}

/**
 * gtk_tree_view_get_vadjustment:
 * @tree_view: A #GtkTreeView
 *
 * Gets the #GtkAdjustment currently being used for the vertical aspect.
 *
 * Return value: A #GtkAdjustment object, or %NULL if none is currently being
 * used.
 **/
GtkAdjustment *
gtk_tree_view_get_vadjustment (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  if (tree_view->priv->vadjustment == NULL)
    gtk_tree_view_set_vadjustment (tree_view, NULL);

  return tree_view->priv->vadjustment;
}

/**
 * gtk_tree_view_set_vadjustment:
 * @tree_view: A #GtkTreeView
 * @adjustment: The #GtkAdjustment to set, or %NULL
 *
 * Sets the #GtkAdjustment for the current vertical aspect.
 **/
void
gtk_tree_view_set_vadjustment (GtkTreeView   *tree_view,
			       GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_tree_view_set_adjustments (tree_view,
				 tree_view->priv->hadjustment,
				 adjustment);

  g_object_notify (G_OBJECT (tree_view), "vadjustment");
}

/* Column and header operations */

/**
 * gtk_tree_view_get_headers_visible:
 * @tree_view: A #GtkTreeView.
 *
 * Returns %TRUE if the headers on the @tree_view are visible.
 *
 * Return value: Whether the headers are visible or not.
 **/
gboolean
gtk_tree_view_get_headers_visible (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE);
}

/**
 * gtk_tree_view_set_headers_visible:
 * @tree_view: A #GtkTreeView.
 * @headers_visible: %TRUE if the headers are visible
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
	  gdk_window_move_resize (tree_view->priv->bin_window, x + TREE_VIEW_HEADER_HEIGHT (tree_view), y, tree_view->priv->width, GTK_WIDGET (tree_view)->allocation.height -  + TREE_VIEW_HEADER_HEIGHT (tree_view));

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

  g_object_notify (G_OBJECT (tree_view), "headers_visible");
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

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      column = list->data;
      if (column->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
	continue;
      gtk_tree_view_column_cell_set_dirty (column);
      dirty = TRUE;
    }

  if (dirty)
    gtk_widget_queue_resize (GTK_WIDGET (tree_view));
}

/**
 * gtk_tree_view_set_headers_clickable:
 * @tree_view: A #GtkTreeView.
 * @setting: %TRUE if the columns are clickable.
 *
 * Allow the column title buttons to be clicked.
 **/
void
gtk_tree_view_set_headers_clickable (GtkTreeView *tree_view,
				     gboolean   setting)
{
  GList *list;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (tree_view->priv->model != NULL);

  for (list = tree_view->priv->columns; list; list = list->next)
    gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (list->data), setting);

  g_object_notify (G_OBJECT (tree_view), "headers_clickable");
}


/**
 * gtk_tree_view_set_rules_hint
 * @tree_view: a #GtkTreeView
 * @setting: %TRUE if the tree requires reading across rows
 *
 * This function tells GTK+ that the user interface for your
 * application requires users to read across tree rows and associate
 * cells with one another. By default, GTK+ will then render the tree
 * with alternating row colors. Do <emphasis>not</emphasis> use it
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

  g_object_notify (G_OBJECT (tree_view), "rules_hint");
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

/* Public Column functions
 */

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
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), -1);
  g_return_val_if_fail (column->tree_view == NULL, -1);

  return gtk_tree_view_insert_column (tree_view, column, -1);
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
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), -1);
  g_return_val_if_fail (column->tree_view == GTK_WIDGET (tree_view), -1);

  _gtk_tree_view_column_unset_tree_view (column);

  if (tree_view->priv->focus_column == column)
    tree_view->priv->focus_column = NULL;

  tree_view->priv->columns = g_list_remove (tree_view->priv->columns, column);
  tree_view->priv->n_columns--;

  if (GTK_WIDGET_REALIZED (tree_view))
    {
      GList *list;

      _gtk_tree_view_column_unrealize_button (column);
      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  GtkTreeViewColumn *tmp_column;

	  tmp_column = GTK_TREE_VIEW_COLUMN (list->data);
	  if (tmp_column->visible)
	    gtk_tree_view_column_cell_set_dirty (tmp_column);
	}

      if (tree_view->priv->n_columns == 0 &&
	  gtk_tree_view_get_headers_visible (tree_view))
	gdk_window_hide (tree_view->priv->header_window);

      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  g_object_unref (G_OBJECT (column));
  g_signal_emit (G_OBJECT (tree_view), tree_view_signals[COLUMNS_CHANGED], 0);

  return tree_view->priv->n_columns;
}

/**
 * gtk_tree_view_insert_column:
 * @tree_view: A #GtkTreeView.
 * @column: The #GtkTreeViewColumn to be inserted.
 * @position: The position to insert @column in.
 *
 * This inserts the @column into the @tree_view at @position.  If @position is
 * -1, then the column is inserted at the end.
 *
 * Return value: The number of columns in @tree_view after insertion.
 **/
gint
gtk_tree_view_insert_column (GtkTreeView       *tree_view,
                             GtkTreeViewColumn *column,
                             gint               position)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), -1);
  g_return_val_if_fail (column->tree_view == NULL, -1);

  g_object_ref (G_OBJECT (column));
  gtk_object_sink (GTK_OBJECT (column));

  if (tree_view->priv->n_columns == 0 &&
      GTK_WIDGET_REALIZED (tree_view) &&
      gtk_tree_view_get_headers_visible (tree_view))
    {
      gdk_window_show (tree_view->priv->header_window);
    }

  tree_view->priv->columns = g_list_insert (tree_view->priv->columns,
					    column, position);
  tree_view->priv->n_columns++;

  _gtk_tree_view_column_set_tree_view (column, tree_view);

  if (GTK_WIDGET_REALIZED (tree_view))
    {
      GList *list;

      _gtk_tree_view_column_realize_button (column);

      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  column = GTK_TREE_VIEW_COLUMN (list->data);
	  if (column->visible)
	    gtk_tree_view_column_cell_set_dirty (column);
	}
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  g_signal_emit (G_OBJECT (tree_view), tree_view_signals[COLUMNS_CHANGED], 0);

  return tree_view->priv->n_columns;
}

/**
 * gtk_tree_view_insert_column_with_attributes:
 * @tree_view: A #GtkTreeView
 * @position: The position to insert the new column in.
 * @title: The title to set the header to.
 * @cell: The #GtkCellRenderer.
 * @Varargs: A %NULL-terminated list of attributes.
 *
 * Creates a new #GtkTreeViewColumn and inserts it into the @tree_view at
 * @position.  If @position is -1, then the newly created column is inserted at
 * the end.  The column is initialized with the attributes given.
 *
 * Return value: The number of columns in @tree_view after insertion.
 **/
gint
gtk_tree_view_insert_column_with_attributes (GtkTreeView     *tree_view,
					     gint             position,
					     gchar           *title,
					     GtkCellRenderer *cell,
					     ...)
{
  GtkTreeViewColumn *column;
  gchar *attribute;
  va_list args;
  gint column_id;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);

  column = gtk_tree_view_column_new ();

  gtk_tree_view_column_set_title (column, title);
  gtk_tree_view_column_pack_start (column, cell, TRUE);

  va_start (args, cell);

  attribute = va_arg (args, gchar *);

  while (attribute != NULL)
    {
      column_id = va_arg (args, gint);
      gtk_tree_view_column_add_attribute (column, cell, attribute, column_id);
      attribute = va_arg (args, gchar *);
    }

  va_end (args);

  gtk_tree_view_insert_column (tree_view, column, position);

  return tree_view->priv->n_columns;
}

/**
 * gtk_tree_view_insert_column_with_data_func:
 * @tree_view: a #GtkTreeView
 * @position: Position to insert, -1 for append
 * @title: column title
 * @cell: cell renderer for column
 * @func: function to set attributes of cell renderer
 * @data: data for @func
 * @dnotify: destroy notifier for @data
 *
 * Convenience function that inserts a new column into the #GtkTreeView
 * with the given cell renderer and a #GtkCellDataFunc to set cell renderer
 * attributes (normally using data from the model). See also
 * gtk_tree_view_column_set_cell_data_func(), gtk_tree_view_column_pack_start().
 *
 * Return value: number of columns in the tree view post-insert
 **/
gint
gtk_tree_view_insert_column_with_data_func  (GtkTreeView               *tree_view,
                                             gint                       position,
                                             gchar                     *title,
                                             GtkCellRenderer           *cell,
                                             GtkTreeCellDataFunc        func,
                                             gpointer                   data,
                                             GDestroyNotify             dnotify)
{
  GtkTreeViewColumn *column;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);

  column = gtk_tree_view_column_new ();

  gtk_tree_view_column_set_title (column, title);
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_set_cell_data_func (column, cell, func, data, dnotify);

  gtk_tree_view_insert_column (tree_view, column, position);

  return tree_view->priv->n_columns;
}

/**
 * gtk_tree_view_get_column:
 * @tree_view: A #GtkTreeView.
 * @n: The position of the column, counting from 0.
 *
 * Gets the #GtkTreeViewColumn at the given position in the #tree_view.
 *
 * Return value: The #GtkTreeViewColumn, or %NULL if the position is outside the
 * range of columns.
 **/
GtkTreeViewColumn *
gtk_tree_view_get_column (GtkTreeView *tree_view,
			  gint         n)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  if (n < 0 || n >= tree_view->priv->n_columns)
    return NULL;

  if (tree_view->priv->columns == NULL)
    return NULL;

  return GTK_TREE_VIEW_COLUMN (g_list_nth (tree_view->priv->columns, n)->data);
}

/**
 * gtk_tree_view_get_columns:
 * @tree_view: A #GtkTreeView
 *
 * Returns a #GList of all the #GtkTreeViewColumn s currently in @tree_view.
 * The returned list must be freed with g_list_free ().
 *
 * Return value: A list of #GtkTreeViewColumn s
 **/
GList *
gtk_tree_view_get_columns (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return g_list_copy (tree_view->priv->columns);
}

/**
 * gtk_tree_view_move_column_after:
 * @tree_view: A #GtkTreeView
 * @column: The #GtkTreeViewColumn to be moved.
 * @base_column: The #GtkTreeViewColumn to be moved relative to, or %NULL.
 *
 * Moves @column to be after to @base_column.  If @base_column is %NULL, then
 * @column is placed in the first position.
 **/
void
gtk_tree_view_move_column_after (GtkTreeView       *tree_view,
				 GtkTreeViewColumn *column,
				 GtkTreeViewColumn *base_column)
{
  GList *column_list_el, *base_el = NULL;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  column_list_el = g_list_find (tree_view->priv->columns, column);
  g_return_if_fail (column_list_el != NULL);

  if (base_column)
    {
      base_el = g_list_find (tree_view->priv->columns, base_column);
      g_return_if_fail (base_el != NULL);
    }

  if (column_list_el->prev == base_el)
    return;

  tree_view->priv->columns = g_list_remove_link (tree_view->priv->columns, column_list_el);
  if (base_el == NULL)
    {
      column_list_el->prev = NULL;
      column_list_el->next = tree_view->priv->columns;
      if (column_list_el->next)
	column_list_el->next->prev = column_list_el;
      tree_view->priv->columns = column_list_el;
    }
  else
    {
      column_list_el->prev = base_el;
      column_list_el->next = base_el->next;
      if (column_list_el->next)
	column_list_el->next->prev = column_list_el;
      base_el->next = column_list_el;
    }

  if (GTK_WIDGET_REALIZED (tree_view))
    {
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
      gtk_tree_view_size_allocate_columns (GTK_WIDGET (tree_view));
    }

  g_signal_emit (G_OBJECT (tree_view), tree_view_signals[COLUMNS_CHANGED], 0);
}

/**
 * gtk_tree_view_set_expander_column:
 * @tree_view: A #GtkTreeView
 * @column: %NULL, or the column to draw the expander arrow at.
 *
 * Sets the column to draw the expander arrow at. It must be in @tree_view.  If
 * @column is %NULL, then the expander arrow is always at the first visible
 * column.
 **/
void
gtk_tree_view_set_expander_column (GtkTreeView       *tree_view,
                                   GtkTreeViewColumn *column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  if (column != NULL)
    g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (column));

  if (tree_view->priv->expander_column != column)
    {
      GList *list;

      if (column)
	{
	  /* Confirm that column is in tree_view */
	  for (list = tree_view->priv->columns; list; list = list->next)
	    if (list->data == column)
	      break;
	  g_return_if_fail (list != NULL);
	}

      tree_view->priv->expander_column = column;
      g_object_notify (G_OBJECT (tree_view), "expander_column");
    }
}

/**
 * gtk_tree_view_get_expander_column:
 * @tree_view: A #GtkTreeView
 *
 * Returns the column that is the current expander column.  This
 * column has the expander arrow drawn next to it.
 *
 * Return value: The expander column.
 **/
GtkTreeViewColumn *
gtk_tree_view_get_expander_column (GtkTreeView *tree_view)
{
  GList *list;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  for (list = tree_view->priv->columns; list; list = list->next)
    if (gtk_tree_view_is_expander_column (tree_view, GTK_TREE_VIEW_COLUMN (list->data)))
      return (GtkTreeViewColumn *) list->data;
  return NULL;
}


/**
 * gtk_tree_view_set_column_drag_function:
 * @tree_view: A #GtkTreeView.
 * @func: A function to determine which columns are reorderable, or %NULL.
 * @user_data: User data to be passed to @func, or %NULL
 * @destroy: Destroy notifier for @user_data, or %NULL
 *
 * Sets a user function for determining where a column may be dropped when
 * dragged.  This function is called on every column pair in turn at the
 * beginning of a column drag to determine where a drop can take place.  The
 * arguments passed to @func are: the @tree_view, the #GtkTreeViewColumn being
 * dragged, the two #GtkTreeViewColumn s determining the drop spot, and
 * @user_data.  If either of the #GtkTreeViewColumn arguments for the drop spot
 * are %NULL, then they indicate an edge.  If @func is set to be %NULL, then
 * @tree_view reverts to the default behavior of allowing all columns to be
 * dropped everywhere.
 **/
void
gtk_tree_view_set_column_drag_function (GtkTreeView               *tree_view,
					GtkTreeViewColumnDropFunc  func,
					gpointer                   user_data,
					GtkDestroyNotify           destroy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->column_drop_func_data_destroy)
    (* tree_view->priv->column_drop_func_data_destroy) (tree_view->priv->column_drop_func_data);

  tree_view->priv->column_drop_func = func;
  tree_view->priv->column_drop_func_data = user_data;
  tree_view->priv->column_drop_func_data_destroy = destroy;
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
 * this function is called.  If it isn't, you probably want to be
 * using gtk_tree_view_scroll_to_cell().
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

  gtk_adjustment_set_value (hadj, CLAMP (tree_x, hadj->lower, hadj->upper - hadj->page_size));
  gtk_adjustment_set_value (vadj, CLAMP (tree_y, vadj->lower, vadj->upper - vadj->page_size));
}

/**
 * gtk_tree_view_scroll_to_cell
 * @tree_view: A #GtkTreeView.
 * @path: The path of the row to move to, or %NULL.
 * @column: The #GtkTreeViewColumn to move horizontally to, or %NULL.
 * @use_align: whether to use alignment arguments, or %FALSE.
 * @row_align: The vertical alignment of the row specified by @path.
 * @col_align: The horizontal alignment of the column specified by @column.
 *
 * Moves the alignments of @tree_view to the position specified by @column and
 * @path.  If @column is %NULL, then no horizontal scrolling occurs.  Likewise,
 * if @path is %NULL no vertical scrolling occurs.  @row_align determines where
 * the row is placed, and @col_align determines where @column is placed.  Both
 * are expected to be between 0.0 and 1.0. 0.0 means left/top alignment, 1.0
 * means right/bottom alignment, 0.5 means center.  If @use_align is %FALSE,
 * then the alignment arguments are ignored, and the tree does the minimum
 * amount of work to scroll the cell onto the screen.
 **/
void
gtk_tree_view_scroll_to_cell (GtkTreeView       *tree_view,
                              GtkTreePath       *path,
                              GtkTreeViewColumn *column,
			      gboolean           use_align,
                              gfloat             row_align,
                              gfloat             col_align)
{
  GdkRectangle cell_rect;
  GdkRectangle vis_rect;
  gint dest_x, dest_y;
  gfloat within_margin = 0;

  /* FIXME work on unmapped/unrealized trees? maybe implement when
   * we do incremental reflow for trees
   */

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (row_align >= 0.0 && row_align <= 1.0);
  g_return_if_fail (col_align >= 0.0 && col_align <= 1.0);
  g_return_if_fail (path != NULL || column != NULL);

  row_align = CLAMP (row_align, 0.0, 1.0);
  col_align = CLAMP (col_align, 0.0, 1.0);

  if (! GTK_WIDGET_REALIZED (tree_view))
    {
      if (path)
	tree_view->priv->scroll_to_path = gtk_tree_path_copy (path);
      if (column)
	tree_view->priv->scroll_to_column = column;
      tree_view->priv->scroll_to_use_align = use_align;
      tree_view->priv->scroll_to_row_align = row_align;
      tree_view->priv->scroll_to_col_align = col_align;

      return;
    }

  gtk_tree_view_get_cell_area (tree_view, path, column, &cell_rect);
  gtk_tree_view_get_visible_rect (tree_view, &vis_rect);

  dest_x = vis_rect.x;
  dest_y = vis_rect.y;

  if (column)
    {
      if (use_align)
	{
	  dest_x = cell_rect.x + cell_rect.width * row_align - vis_rect.width * row_align;
	}
      else
	{
	  if (cell_rect.x < vis_rect.x)
	    dest_x = cell_rect.x - vis_rect.width * within_margin;
	  else if (cell_rect.x + cell_rect.width > vis_rect.x + vis_rect.width)
	    dest_x = cell_rect.x + cell_rect.width - vis_rect.width * (1 - within_margin);
	}
    }

  if (path)
    {
      if (use_align)
	{
	  dest_y = cell_rect.y + cell_rect.height * col_align - vis_rect.height * col_align;
	}
      else
	{
	  if (cell_rect.y < vis_rect.y)
	    dest_y = cell_rect.y - vis_rect.height * within_margin;
	  else if (cell_rect.y + cell_rect.height > vis_rect.y + vis_rect.height)
	    dest_y = cell_rect.y + cell_rect.height - vis_rect.height * (1 - within_margin);
	}
    }

  gtk_tree_view_scroll_to_point (tree_view, dest_x, dest_y);
}


/**
 * gtk_tree_view_row_activated:
 * @tree_view: A #GtkTreeView
 * @path: The #GtkTreePath to be activated.
 * @column: The #GtkTreeViewColumn to be activated.
 *
 * Activates the cell determined by @path and @column.
 **/
void
gtk_tree_view_row_activated (GtkTreeView       *tree_view,
			     GtkTreePath       *path,
			     GtkTreeViewColumn *column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  g_signal_emit (G_OBJECT(tree_view), tree_view_signals[ROW_ACTIVATED], 0, path, column);
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
				TRUE);
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
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (tree_view->priv->tree != NULL);

  _gtk_rbtree_traverse (tree_view->priv->tree,
			tree_view->priv->tree->root,
			G_PRE_ORDER,
			gtk_tree_view_expand_all_helper,
			tree_view);
}

/* Timeout to animate the expander during expands and collapses */
static gboolean
expand_collapse_timeout (gpointer data)
{
  GtkTreeView *tree_view = data;
  GtkRBNode *node;
  GtkRBTree *tree;
  gboolean expanding;
  gboolean redraw;

  GDK_THREADS_ENTER ();

  redraw = FALSE;
  expanding = TRUE;

  node = tree_view->priv->expanded_collapsed_node;
  tree = tree_view->priv->expanded_collapsed_tree;

  if (node->children == NULL)
    expanding = FALSE;

  if (expanding)
    {
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SEMI_COLLAPSED))
	{
	  GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_IS_SEMI_COLLAPSED);
	  GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_SEMI_EXPANDED);

	  redraw = TRUE;

	}
      else if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SEMI_EXPANDED))
	{
	  GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_IS_SEMI_EXPANDED);

	  redraw = TRUE;
	}
    }
  else
    {
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SEMI_EXPANDED))
	{
	  GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_IS_SEMI_EXPANDED);
	  GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_SEMI_COLLAPSED);

	  redraw = TRUE;
	}
      else if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SEMI_COLLAPSED))
	{
	  GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_IS_SEMI_COLLAPSED);

	  redraw = TRUE;

	}
    }

  if (redraw)
    {
      gtk_tree_view_queue_draw_arrow (tree_view, tree, node, NULL);

      GDK_THREADS_LEAVE ();

      return TRUE;
    }

  GDK_THREADS_LEAVE ();

  return FALSE;
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
  GtkRBTree *tree;
  GtkRBNode *node;
  GtkTreePath *path;
  guint *indices;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (tree_view->priv->tree != NULL);

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, 0);
  indices = gtk_tree_path_get_indices (path);

  tree = tree_view->priv->tree;
  node = tree->root;
  while (node && node->left != tree->nil)
    node = node->left;

  while (node)
    {
      if (node->children)
	gtk_tree_view_real_collapse_row (tree_view, path, tree, node, FALSE);
      indices[0]++;
      node = _gtk_rbtree_next (tree, node);
    }

  gtk_tree_path_free (path);
}

/* FIXME the bool return values for expand_row and collapse_row are
 * not analagous; they should be TRUE if the row had children and
 * was not already in the requested state.
 */


static gboolean
gtk_tree_view_real_expand_row (GtkTreeView *tree_view,
			       GtkTreePath *path,
			       GtkRBTree   *tree,
			       GtkRBNode   *node,
			       gboolean     open_all,
			       gboolean     animate)
{
  GtkTreeIter iter;
  GtkTreeIter temp;
  gboolean expand;


  if (node->children)
    return TRUE;
  if (! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT))
    return FALSE;

  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
  if (! gtk_tree_model_iter_has_child (tree_view->priv->model, &iter))
    return FALSE;

  g_signal_emit (G_OBJECT (tree_view), tree_view_signals[TEST_EXPAND_ROW], 0, &iter, path, &expand);

  if (expand)
    return FALSE;

  node->children = _gtk_rbtree_new ();
  node->children->parent_tree = tree;
  node->children->parent_node = node;

  gtk_tree_model_iter_children (tree_view->priv->model, &temp, &iter);

  gtk_tree_view_build_tree (tree_view,
			    node->children,
			    &temp,
			    gtk_tree_path_get_depth (path) + 1,
			    open_all);

  if (tree_view->priv->expand_collapse_timeout)
    {
      gtk_timeout_remove (tree_view->priv->expand_collapse_timeout);
      tree_view->priv->expand_collapse_timeout = 0;
    }

  if (tree_view->priv->expanded_collapsed_node != NULL)
    {
      GTK_RBNODE_UNSET_FLAG (tree_view->priv->expanded_collapsed_node, GTK_RBNODE_IS_SEMI_EXPANDED);
      GTK_RBNODE_UNSET_FLAG (tree_view->priv->expanded_collapsed_node, GTK_RBNODE_IS_SEMI_COLLAPSED);

      tree_view->priv->expanded_collapsed_node = NULL;
    }

  if (animate)
    {
      tree_view->priv->expand_collapse_timeout = gtk_timeout_add (50, expand_collapse_timeout, tree_view);
      tree_view->priv->expanded_collapsed_node = node;
      tree_view->priv->expanded_collapsed_tree = tree;

      GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_SEMI_COLLAPSED);
    }

  if (GTK_WIDGET_MAPPED (tree_view))
    install_presize_handler (tree_view);

  g_signal_emit (G_OBJECT (tree_view), tree_view_signals[ROW_EXPANDED], 0, &iter, path);
  return TRUE;
}


/**
 * gtk_tree_view_expand_row:
 * @tree_view: a #GtkTreeView
 * @path: path to a row
 * @open_all: whether to recursively expand, or just expand immediate children
 *
 * Opens the row so its children are visible.
 *
 * Return value: %TRUE if the row existed and had children
 **/
gboolean
gtk_tree_view_expand_row (GtkTreeView *tree_view,
			  GtkTreePath *path,
			  gboolean     open_all)
{
  GtkRBTree *tree;
  GtkRBNode *node;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (tree_view->priv->model != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  if (_gtk_tree_view_find_node (tree_view,
				path,
				&tree,
				&node))
    return FALSE;

  if (tree != NULL)
    return gtk_tree_view_real_expand_row (tree_view, path, tree, node, open_all, FALSE);
  else
    return FALSE;
}

static gboolean
gtk_tree_view_real_collapse_row (GtkTreeView *tree_view,
				 GtkTreePath *path,
				 GtkRBTree   *tree,
				 GtkRBNode   *node,
				 gboolean     animate)
{
  GtkTreeIter iter;
  GtkTreeIter children;
  gboolean collapse;
  gint x, y;
  GList *list;

  if (node->children == NULL)
    return FALSE;

  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);

  g_signal_emit (G_OBJECT (tree_view), tree_view_signals[TEST_COLLAPSE_ROW], 0, &iter, path, &collapse);

  if (collapse)
    return FALSE;

  /* if the prelighted node is a child of us, we want to unprelight it.  We have
   * a chance to prelight the correct node below */

  if (tree_view->priv->prelight_tree)
    {
      GtkRBTree *parent_tree;
      GtkRBNode *parent_node;

      parent_tree = tree_view->priv->prelight_tree->parent_tree;
      parent_node = tree_view->priv->prelight_tree->parent_node;
      while (parent_tree)
	{
	  if (parent_tree == tree && parent_node == node)
	    {
	      ensure_unprelighted (tree_view);
	      break;
	    }
	  parent_node = parent_tree->parent_node;
	  parent_tree = parent_tree->parent_tree;
	}
    }

  TREE_VIEW_INTERNAL_ASSERT (gtk_tree_model_iter_children (tree_view->priv->model, &children, &iter), FALSE);

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      GtkTreeViewColumn *column = list->data;

      if (column->visible == FALSE)
	continue;
      if (gtk_tree_view_column_get_sizing (column) == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
	gtk_tree_view_column_cell_set_dirty (column);
    }

  if (tree_view->priv->destroy_count_func)
    {
      GtkTreePath *child_path;
      gint child_count = 0;
      child_path = gtk_tree_path_copy (path);
      gtk_tree_path_append_index (child_path, 0);
      if (node->children)
	_gtk_rbtree_traverse (node->children, node->children->root, G_POST_ORDER, count_children_helper, &child_count);
      (* tree_view->priv->destroy_count_func) (tree_view, child_path, child_count, tree_view->priv->destroy_count_data);
      gtk_tree_path_free (child_path);
    }

  if (gtk_tree_view_unref_and_check_selection_tree (tree_view, node->children))
    g_signal_emit_by_name (G_OBJECT (tree_view->priv->selection), "changed", 0);
  _gtk_rbtree_remove (node->children);

  if (tree_view->priv->expand_collapse_timeout)
    {
      gtk_timeout_remove (tree_view->priv->expand_collapse_timeout);
      tree_view->priv->expand_collapse_timeout = 0;
    }
  
  if (tree_view->priv->expanded_collapsed_node != NULL)
    {
      GTK_RBNODE_UNSET_FLAG (tree_view->priv->expanded_collapsed_node, GTK_RBNODE_IS_SEMI_EXPANDED);
      GTK_RBNODE_UNSET_FLAG (tree_view->priv->expanded_collapsed_node, GTK_RBNODE_IS_SEMI_COLLAPSED);
      
      tree_view->priv->expanded_collapsed_node = NULL;
    }

  if (animate)
    {
      tree_view->priv->expand_collapse_timeout = gtk_timeout_add (50, expand_collapse_timeout, tree_view);
      tree_view->priv->expanded_collapsed_node = node;
      tree_view->priv->expanded_collapsed_tree = tree;

      GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_SEMI_EXPANDED);
    }
  
  if (GTK_WIDGET_MAPPED (tree_view))
    {
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  if (gtk_tree_row_reference_valid (tree_view->priv->cursor))
    {
      GtkTreePath *cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

      if (gtk_tree_path_is_ancestor (path, cursor_path))
	{
	  gtk_tree_row_reference_free (tree_view->priv->cursor);
	  tree_view->priv->cursor = gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view),
								      tree_view->priv->model,
								      path);
	}
      gtk_tree_path_free (cursor_path);
    }

  if (gtk_tree_row_reference_valid (tree_view->priv->anchor))
      {
      GtkTreePath *anchor_path = gtk_tree_row_reference_get_path (tree_view->priv->anchor);
      if (gtk_tree_path_is_ancestor (path, anchor_path))
	{
	  gtk_tree_row_reference_free (tree_view->priv->anchor);
	  tree_view->priv->anchor = NULL;
	}
      gtk_tree_path_free (anchor_path);

    }

  g_signal_emit (G_OBJECT (tree_view), tree_view_signals[ROW_COLLAPSED], 0, &iter, path);

  /* now that we've collapsed all rows, we want to try to set the prelight
   * again. To do this, we fake a motion event and send it to ourselves. */

  if (gdk_window_at_pointer (&x, &y) == tree_view->priv->bin_window)
    {
      GdkEventMotion event;
      event.window = tree_view->priv->bin_window;
      event.x = x;
      event.y = y;

      /* despite the fact this isn't a real event, I'm almost positive it will
       * never trigger a drag event.  maybe_drag is the only function that uses
       * more than just event.x and event.y. */
      gtk_tree_view_motion_bin_window (GTK_WIDGET (tree_view), &event);
    }
  return TRUE;
}

/**
 * gtk_tree_view_collapse_row:
 * @tree_view: a #GtkTreeView
 * @path: path to a row in the @tree_view
 *
 * Collapses a row (hides its child rows, if they exist).
 *
 * Return value: %TRUE if the row was collapsed.
 **/
gboolean
gtk_tree_view_collapse_row (GtkTreeView *tree_view,
			    GtkTreePath *path)
{
  GtkRBTree *tree;
  GtkRBNode *node;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (tree_view->priv->tree != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  if (_gtk_tree_view_find_node (tree_view,
				path,
				&tree,
				&node))
    return FALSE;

  if (tree == NULL || node->children == NULL)
    return FALSE;

  return gtk_tree_view_real_collapse_row (tree_view, path, tree, node, FALSE);
}

static void
gtk_tree_view_map_expanded_rows_helper (GtkTreeView            *tree_view,
					GtkRBTree              *tree,
					GtkTreePath            *path,
					GtkTreeViewMappingFunc  func,
					gpointer                user_data)
{
  GtkRBNode *node;
  gint *indices;
  gint depth;
  gint i = 0;

  if (tree == NULL || tree->root == NULL)
    return;

  node = tree->root;

  indices = gtk_tree_path_get_indices (path);
  depth = gtk_tree_path_get_depth (path);

  while (node && node->left != tree->nil)
    node = node->left;

  while (node)
    {
      if (node->children)
	{
	  gtk_tree_path_append_index (path, 0);
	  gtk_tree_view_map_expanded_rows_helper (tree_view, node->children, path, func, user_data);
	  gtk_tree_path_up (path);
	  (* func) (tree_view, path, user_data);
	}
      i++;
      indices[depth -1] = i;
      node = _gtk_rbtree_next (tree, node);
    }
}

/**
 * gtk_tree_view_map_expanded_rows:
 * @tree_view: A #GtkTreeView
 * @func: A function to be called
 * @data: User data to be passed to the function.
 *
 * Calls @func on all expanded rows.
 **/
void
gtk_tree_view_map_expanded_rows (GtkTreeView            *tree_view,
				 GtkTreeViewMappingFunc  func,
				 gpointer                user_data)
{
  GtkTreePath *path;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (func != NULL);

  path = gtk_tree_path_new_root ();

  gtk_tree_view_map_expanded_rows_helper (tree_view,
					  tree_view->priv->tree,
					  path, func, user_data);

  gtk_tree_path_free (path);
}

/**
 * gtk_tree_view_row_expanded:
 * @tree_view: A #GtkTreeView.
 * @path: A #GtkTreePath to test expansion state.
 *
 * Returns %TRUE if the node pointed to by @path is expanded in @tree_view.
 *
 * Return value: %TRUE if #path is expanded.
 **/
gboolean
gtk_tree_view_row_expanded (GtkTreeView *tree_view,
			    GtkTreePath *path)
{
  GtkRBTree *tree;
  GtkRBNode *node;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  _gtk_tree_view_find_node (tree_view, path, &tree, &node);

  if (node == NULL)
    return FALSE;

  return (node->children != NULL);
}

static GtkTargetEntry row_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, 0 }
};


/**
 * gtk_tree_view_get_reorderable:
 * @tree_view: a #GtkTreeView
 *
 * Retrieves whether the user can reorder the tree via drag-and-drop. See
 * gtk_tree_view_set_reorderable().
 *
 * Return value: %TRUE if the tree can be reordered.
 **/
gboolean
gtk_tree_view_get_reorderable (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return tree_view->priv->reorderable;
}

/**
 * gtk_tree_view_set_reorderable:
 * @tree_view: A #GtkTreeView.
 * @reorderable: %TRUE, if the tree can be reordered.
 *
 * This function is a convenience function to allow you to reorder models that
 * support the #GtkDragSourceIface and the #GtkDragDestIface.  Both
 * #GtkTreeStore and #GtkListStore support these.  If @reorderable is %TRUE, then
 * the user can reorder the model by dragging and dropping columns.  The
 * developer can listen to these changes by connecting to the model's
 * signals.
 *
 * This function does not give you any degree of control over the order -- any
 * reorderering is allowed.  If more control is needed, you should probably
 * handle drag and drop manually.
 **/
void
gtk_tree_view_set_reorderable (GtkTreeView *tree_view,
			       gboolean     reorderable)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  reorderable = reorderable != FALSE;

  if (tree_view->priv->reorderable == reorderable)
    return;

  tree_view->priv->reorderable = reorderable;

  if (reorderable)
    {
      gtk_tree_view_enable_model_drag_source (tree_view,
					      GDK_BUTTON1_MASK,
					      row_targets,
					      G_N_ELEMENTS (row_targets),
					      GDK_ACTION_MOVE);
      gtk_tree_view_enable_model_drag_dest (tree_view,
					    row_targets,
					    G_N_ELEMENTS (row_targets),
					    GDK_ACTION_MOVE);
    }
  else
    {
      gtk_tree_view_unset_rows_drag_source (tree_view);
      gtk_tree_view_unset_rows_drag_dest (tree_view);
    }

  g_object_notify (G_OBJECT (tree_view), "reorderable");
}

static void
gtk_tree_view_real_set_cursor (GtkTreeView     *tree_view,
			       GtkTreePath     *path,
			       gboolean         clear_and_select)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;
  GdkModifierType state = 0;

  if (gtk_tree_row_reference_valid (tree_view->priv->cursor))
    {
      GtkTreePath *cursor_path;
      cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
      gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);
      gtk_tree_path_free (cursor_path);
    }

  gtk_tree_row_reference_free (tree_view->priv->cursor);
  gtk_get_current_event_state (&state);

  tree_view->priv->cursor = gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view),
							      tree_view->priv->model,
							      path);
  _gtk_tree_view_find_node (tree_view, path, &tree, &node);
  if (tree != NULL)
    {
      if (clear_and_select && !((state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK))
	_gtk_tree_selection_internal_select_node (tree_view->priv->selection,
						  node, tree, path,
						  state);
      gtk_tree_view_clamp_node_visible (tree_view, tree, node);
      _gtk_tree_view_queue_draw_node (tree_view, tree, node, NULL);
    }

  g_signal_emit (G_OBJECT (tree_view), tree_view_signals[CURSOR_CHANGED], 0);
}

/**
 * gtk_tree_view_get_cursor:
 * @tree_view: A #GtkTreeView
 * @path: A pointer to be filled with the current cursor path, or %NULL
 * @focus_column: A pointer to be filled with the current focus column, or %NULL
 *
 * Fills in @path and @focus_column with the current path and focus column.  If
 * the cursor isn't currently set, then *@path will be %NULL.  If no column
 * currently has focus, then *@focus_column will be %NULL.
 **/
void
gtk_tree_view_get_cursor (GtkTreeView        *tree_view,
			  GtkTreePath       **path,
			  GtkTreeViewColumn **focus_column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (path)
    {
      if (gtk_tree_row_reference_valid (tree_view->priv->cursor))
	*path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
      else
	*path = NULL;
    }

  if (focus_column)
    {
      *focus_column = tree_view->priv->focus_column;
    }
}

/**
 * gtk_tree_view_set_cursor:
 * @tree_view: A #GtkTreeView
 * @path: A #GtkTreePath
 * @focus_column: A #GtkTreeViewColumn, or %NULL
 * @start_editing: %TRUE if the specified cell should start being edited.
 *
 * Sets the current keyboard focus to be at @path, and selects it.  This is
 * useful when you want to focus the user's attention on a particular row.  If
 * @column is not %NULL, then focus is given to the column specified by it.
 * Additionally, if @column is specified, and @start_editing is %TRUE, then
 * editing should be started in the specified cell.  Keyboard focus is given to
 * the widget after this is called.  Please note that editing can only happen
 * when the widget is realized.
 **/
void
gtk_tree_view_set_cursor (GtkTreeView       *tree_view,
			  GtkTreePath       *path,
			  GtkTreeViewColumn *focus_column,
			  gboolean           start_editing)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (path != NULL);
  if (focus_column)
    g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (focus_column));

  gtk_tree_view_real_set_cursor (tree_view, path, TRUE);

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));
  if (focus_column && focus_column->visible)
    {
      GList *list;
      gboolean column_in_tree = FALSE;

      for (list = tree_view->priv->columns; list; list = list->next)
	if (list->data == focus_column)
	  {
	    column_in_tree = TRUE;
	    break;
	  }
      g_return_if_fail (column_in_tree);
      tree_view->priv->focus_column = focus_column;
      if (start_editing)
	gtk_tree_view_start_editing (tree_view, path);
    }
}


/**
 * gtk_tree_view_get_bin_window:
 * @tree_view: A #GtkTreeView
 * 
 * Returns the window that @tree_view renders to.  This is used primarily to
 * compare to <literal>event->window</literal> to confirm that the event on
 * @tree_view is on the right window.
 * 
 * Return value: A #GdkWindow, or %NULL when @tree_view hasn't been realized yet
 **/
GdkWindow *
gtk_tree_view_get_bin_window (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->bin_window;
}

/**
 * gtk_tree_view_get_path_at_pos:
 * @tree_view: A #GtkTreeView.
 * @x: The x position to be identified.
 * @y: The y position to be identified.
 * @path: A pointer to a #GtkTreePath pointer to be filled in, or %NULL
 * @column: A pointer to a #GtkTreeViewColumn pointer to be filled in, or %NULL
 * @cell_x: A pointer where the X coordinate relative to the cell can be placed, or %NULL
 * @cell_y: A pointer where the Y coordinate relative to the cell can be placed, or %NULL
 *
 * Finds the path at the point (@x, @y), relative to widget coordinates.  That
 * is, @x and @y are relative to an events coordinates. @x and @y must come
 * from an event on the @tree_view only where event->window ==
 * gtk_tree_view_get_bin (). It is primarily for things like popup menus.
 * If @path is non-%NULL, then it will be filled with the #GtkTreePath at that
 * point.  This path should be freed with gtk_tree_path_free().  If @column
 * is non-%NULL, then it will be filled with the column at that point.
 * @cell_x and @cell_y return the coordinates relative to the cell background
 * (i.e. the @background_area passed to gtk_cell_renderer_render()).  This
 * function is only meaningful if @tree_view is realized.
 *
 * Return value: %TRUE if a row exists at that coordinate.
 **/
gboolean
gtk_tree_view_get_path_at_pos (GtkTreeView        *tree_view,
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
  g_return_val_if_fail (tree_view->priv->bin_window != NULL, FALSE);

  if (path)
    *path = NULL;
  if (column)
    *column = NULL;

  if (tree_view->priv->tree == NULL)
    return FALSE;

  if (x > tree_view->priv->hadjustment->page_size)
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

  y_offset = _gtk_rbtree_find_offset (tree_view->priv->tree,
				      TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, y),
				      &tree, &node);

  if (tree == NULL)
    return FALSE;

  if (cell_y)
    *cell_y = y_offset;

  if (path)
    *path = _gtk_tree_view_find_path (tree_view, tree, node);

  return TRUE;
}


/**
 * gtk_tree_view_get_cell_area:
 * @tree_view: a #GtkTreeView
 * @path: a #GtkTreePath for the row, or %NULL to get only horizontal coordinates
 * @column: a #GtkTreeViewColumn for the column, or %NULL to get only vertical coordiantes
 * @rect: rectangle to fill with cell rect
 *
 * Fills the bounding rectangle in tree window coordinates for the cell at the
 * row specified by @path and the column specified by @column.  If @path is
 * %NULL, or points to a path not currently displayed, the @y and @height fields
 * of the rectangle will be filled with 0. If @column is %NULL, the @x and @width
 * fields will be filled with 0.  The sum of all cell rects does not cover the
 * entire tree; there are extra pixels in between rows, for example. The
 * returned rectangle is equivalent to the @cell_area passed to
 * gtk_cell_renderer_render().  This function is only valid if #tree_view is
 * realized.
 **/
void
gtk_tree_view_get_cell_area (GtkTreeView        *tree_view,
                             GtkTreePath        *path,
                             GtkTreeViewColumn  *column,
                             GdkRectangle       *rect)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;
  gint vertical_separator;
  gint horizontal_separator;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (column == NULL || GTK_IS_TREE_VIEW_COLUMN (column));
  g_return_if_fail (rect != NULL);
  g_return_if_fail (!column || column->tree_view == (GtkWidget *) tree_view);
  g_return_if_fail (GTK_WIDGET_REALIZED (tree_view));

  gtk_widget_style_get (GTK_WIDGET (tree_view),
			"vertical_separator", &vertical_separator,
			"horizontal_separator", &horizontal_separator,
			NULL);

  rect->x = 0;
  rect->y = 0;
  rect->width = 0;
  rect->height = 0;

  if (column)
    {
      rect->x = column->button->allocation.x + horizontal_separator/2;
      rect->width = column->button->allocation.width - horizontal_separator;
    }

  if (path)
    {
      /* Get vertical coords */
      if (_gtk_tree_view_find_node (tree_view, path, &tree, &node) &&
	  tree != NULL)
	return;

      rect->y = CELL_FIRST_PIXEL (tree_view, tree, node, vertical_separator);
      rect->height = CELL_HEIGHT (node, vertical_separator);

      if (gtk_tree_view_is_expander_column (tree_view, column) &&
	  TREE_VIEW_DRAW_EXPANDERS (tree_view))
	{
	  gint depth = gtk_tree_path_get_depth (path) - 1;
	  rect->x += depth * tree_view->priv->tab_offset;
	  rect->width -= depth * tree_view->priv->tab_offset;
	  rect->width = MAX (rect->width, 0);
	}
    }
}

/**
 * gtk_tree_view_get_background_area:
 * @tree_view: a #GtkTreeView
 * @path: a #GtkTreePath for the row, or %NULL to get only horizontal coordinates
 * @column: a #GtkTreeViewColumn for the column, or %NULL to get only vertical coordiantes
 * @rect: rectangle to fill with cell background rect
 *
 * Fills the bounding rectangle in tree window coordinates for the cell at the
 * row specified by @path and the column specified by @column.  If @path is
 * %NULL, or points to a node not found in the tree, the @y and @height fields of
 * the rectangle will be filled with 0. If @column is %NULL, the @x and @width
 * fields will be filled with 0.  The returned rectangle is equivalent to the
 * @background_area passed to gtk_cell_renderer_render().  These background
 * areas tile to cover the entire tree window (except for the area used for
 * header buttons). Contrast with the @cell_area, returned by
 * gtk_tree_view_get_cell_area(), which returns only the cell itself, excluding
 * surrounding borders and the tree expander area.
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

      if (_gtk_tree_view_find_node (tree_view, path, &tree, &node) &&
	  tree != NULL)
	return;

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
    *tx = wx + tree_view->priv->hadjustment->value;
  if (ty)
    *ty = wy + tree_view->priv->vadjustment->value;
}

/**
 * gtk_tree_view_tree_to_widget_coords:
 * @tree_view: a #GtkTreeView
 * @tx: tree X coordinate
 * @ty: tree Y coordinate
 * @wx: return location for widget X coordinate
 * @wy: return location for widget Y coordinate
 *
 * Converts tree coordinates (coordinates in full scrollable area of the tree)
 * to widget coordinates.
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
    *wx = tx - tree_view->priv->hadjustment->value;
  if (wy)
    *wy = ty - tree_view->priv->vadjustment->value;
}

static void
unset_reorderable (GtkTreeView *tree_view)
{
  if (tree_view->priv->reorderable)
    {
      tree_view->priv->reorderable = FALSE;
      g_object_notify (G_OBJECT (tree_view), "reorderable");
    }
}

void
gtk_tree_view_enable_model_drag_source (GtkTreeView              *tree_view,
					GdkModifierType           start_button_mask,
					const GtkTargetEntry     *targets,
					gint                      n_targets,
					GdkDragAction             actions)
{
  TreeViewDragInfo *di;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  di = ensure_info (tree_view);
  clear_source_info (di);

  di->start_button_mask = start_button_mask;
  di->source_target_list = gtk_target_list_new (targets, n_targets);
  di->source_actions = actions;

  di->source_set = TRUE;

  unset_reorderable (tree_view);
}

void
gtk_tree_view_enable_model_drag_dest (GtkTreeView              *tree_view,
				      const GtkTargetEntry     *targets,
				      gint                      n_targets,
				      GdkDragAction             actions)
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

  di->dest_set = TRUE;

  unset_reorderable (tree_view);
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
  
  unset_reorderable (tree_view);
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

  unset_reorderable (tree_view);
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
        gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view), tree_view->priv->model, path);
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
  g_return_val_if_fail (drag_x >= 0, FALSE);
  g_return_val_if_fail (drag_y >= 0, FALSE);
  g_return_val_if_fail (tree_view->priv->bin_window != NULL, FALSE);


  if (path)
    *path = NULL;

  if (tree_view->priv->tree == NULL)
    return FALSE;

  /* remember that drag_x and drag_y are in widget coords, convert to tree window */

  gtk_tree_view_widget_to_tree_coords (tree_view, drag_x, drag_y,
                                       &x, &y);

  /* If in the top quarter of a row, we drop before that row; if
   * in the bottom quarter, drop after that row; if in the middle,
   * and the row has children, drop into the row.
   */

  if (!gtk_tree_view_get_path_at_pos (tree_view,
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



/* KEEP IN SYNC WITH GTK_TREE_VIEW_BIN_EXPOSE */
/**
 * gtk_tree_view_create_row_drag_icon:
 * @tree_view: a #GtkTreeView
 * @path: a #GtkTreePath in @tree_view
 *
 * Creates a #GdkPixmap representation of the row at @path.  This image is used
 * for a drag icon.
 *
 * Return value: a newly-allocated pixmap of the drag icon.
 **/
GdkPixmap *
gtk_tree_view_create_row_drag_icon (GtkTreeView  *tree_view,
                                    GtkTreePath  *path)
{
  GtkTreeIter   iter;
  GtkRBTree    *tree;
  GtkRBNode    *node;
  gint cell_offset;
  GList *list;
  GdkRectangle background_area;
  GdkRectangle expose_area;
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

  expose_area.x = 0;
  expose_area.y = 0;
  expose_area.width = bin_window_width + 2;
  expose_area.height = background_area.height + 2;

  gdk_draw_rectangle (drawable,
                      widget->style->base_gc [GTK_WIDGET_STATE (widget)],
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

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      GtkTreeViewColumn *column = list->data;
      GdkRectangle cell_area;
      gint vertical_separator;

      if (!column->visible)
        continue;

      gtk_tree_view_column_cell_set_cell_data (column, tree_view->priv->model, &iter,
					       GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT),
					       node->children?TRUE:FALSE);

      background_area.x = cell_offset;
      background_area.width = column->width;

      cell_area = background_area;

      gtk_widget_style_get (widget, "vertical_separator", &vertical_separator, NULL);
      cell_area.y += vertical_separator / 2;
      cell_area.height -= vertical_separator;

      if (gtk_tree_view_is_expander_column (tree_view, column) &&
          TREE_VIEW_DRAW_EXPANDERS(tree_view))
        {
          cell_area.x += depth * tree_view->priv->tab_offset;
          cell_area.width -= depth * tree_view->priv->tab_offset;
        }

      if (gtk_tree_view_column_cell_is_visible (column))
	gtk_tree_view_column_cell_render (column,
					  drawable,
					  &background_area,
					  &cell_area,
					  &expose_area,
					  0);

      cell_offset += column->width;
    }

  return drawable;
}


/**
 * gtk_tree_view_set_destroy_count_func:
 * @tree_view: A #GtkTreeView
 * @func: Function to be called when a view row is destroyed, or %NULL
 * @data: User data to be passed to @func, or %NULL
 * @destroy: Destroy notifier for @data, or %NULL
 *
 * This function should almost never be used.  It is meant for private use by
 * ATK for determining the number of visible children that are removed when the
 * user collapses a row, or a row is deleted.
 **/
void
gtk_tree_view_set_destroy_count_func (GtkTreeView             *tree_view,
				      GtkTreeDestroyCountFunc  func,
				      gpointer                 data,
				      GtkDestroyNotify         destroy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->destroy_count_destroy)
    (* tree_view->priv->destroy_count_destroy) (tree_view->priv->destroy_count_data);

  tree_view->priv->destroy_count_func = func;
  tree_view->priv->destroy_count_data = data;
  tree_view->priv->destroy_count_destroy = destroy;
}


/*
 * Interactive search
 */

/**
 * gtk_tree_view_set_enable_search:
 * @tree_view: A #GtkTreeView
 * @enable_search: %TRUE, if the user can search interactively
 *
 * If @enable_search is set, then the user can type in text to search through
 * the tree interactively.
 */
void
gtk_tree_view_set_enable_search (GtkTreeView *tree_view,
				 gboolean     enable_search)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  tree_view->priv->enable_search = !!enable_search;
}

/**
 * gtk_tree_view_get_enable_search:
 * @tree_view: A #GtkTreeView
 *
 * Returns whether or not the tree allows interactive searching.
 *
 * Return value: whether or not to let the user search interactively
 */
gboolean
gtk_tree_view_get_enable_search (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return tree_view->priv->enable_search;
}


/**
 * gtk_tree_view_get_search_column:
 * @tree_view: A #GtkTreeView
 *
 * Gets the column searched on by the interactive search code.
 *
 * Return value: the column the interactive search code searches in.
 */
gint
gtk_tree_view_get_search_column (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), 0);

  return (tree_view->priv->search_column);
}

/**
 * gtk_tree_view_set_search_column:
 * @tree_view: A #GtkTreeView
 * @column: the column to search in
 *
 * Sets @column as the column where the interactive search code should search
 * in.  Additionally, turns on interactive searching.
 */
void
gtk_tree_view_set_search_column (GtkTreeView *tree_view,
				 gint         column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (column >= 0);

  if (tree_view->priv->search_column == column)
    return;

  tree_view->priv->search_column = column;

}

/**
 * gtk_tree_view_search_get_search_equal_func:
 * @tree_view: A #GtkTreeView
 *
 * Returns the compare function currently in use.
 *
 * Return value: the currently used compare function for the search code.
 */

GtkTreeViewSearchEqualFunc
gtk_tree_view_get_search_equal_func (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->search_equal_func;
}

/**
 * gtk_tree_view_set_search_equal_func:
 * @tree_view: A #GtkTreeView
 * @search_equal_func: the compare function to use during the search
 * @search_user_data: user data to pass to @search_equal_func, or %NULL
 * @search_destroy: Destroy notifier for @search_user_data, or %NULL
 *
 * Sets the compare function for the interactive search capabilities.
 **/
void
gtk_tree_view_set_search_equal_func (GtkTreeView                *tree_view,
				     GtkTreeViewSearchEqualFunc  search_equal_func,
				     gpointer                    search_user_data,
				     GtkDestroyNotify            search_destroy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (search_equal_func !=NULL);

  if (tree_view->priv->search_destroy)
    (* tree_view->priv->search_destroy) (tree_view->priv->search_user_data);

  tree_view->priv->search_equal_func = search_equal_func;
  tree_view->priv->search_user_data = search_user_data;
  tree_view->priv->search_destroy = search_destroy;
  if (tree_view->priv->search_equal_func == NULL)
    tree_view->priv->search_equal_func = gtk_tree_view_search_equal_func;
}

static void
gtk_tree_view_search_dialog_destroy (GtkWidget   *search_dialog,
				     GtkTreeView *tree_view)
{
  /* remove data from tree_view */
  gtk_object_remove_data (GTK_OBJECT (tree_view),
			  GTK_TREE_VIEW_SEARCH_DIALOG_KEY);

  gtk_widget_destroy (search_dialog);
}

static void
gtk_tree_view_search_position_func (GtkTreeView *tree_view,
				    GtkWidget   *search_dialog)
{
  gint tree_x, tree_y;
  gint tree_width, tree_height;
  GdkWindow *tree_window = GTK_WIDGET (tree_view)->window;
  GtkRequisition requisition;

  /* put window in the lower right corner */
  gdk_window_get_origin (tree_window, &tree_x, &tree_y);
  gdk_window_get_size (tree_window,
		       &tree_width,
		       &tree_height);
  gtk_widget_size_request (search_dialog, &requisition);
  gtk_window_move (GTK_WINDOW (search_dialog),
		   tree_x + tree_width - requisition.width,
		   tree_y + tree_height);
}

static gboolean
gtk_tree_view_search_delete_event (GtkWidget *widget,
				   GdkEventAny *event,
				   GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  gtk_tree_view_search_dialog_destroy (widget, tree_view);

  return TRUE;
}

static gboolean
gtk_tree_view_search_button_press_event (GtkWidget *widget,
					 GdkEventButton *event,
					 GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  gtk_tree_view_search_dialog_destroy (widget, tree_view);

  return TRUE;
}

static gboolean
gtk_tree_view_search_key_press_event (GtkWidget *widget,
				      GdkEventKey *event,
				      GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  /* close window */
  if (event->keyval == GDK_Escape ||
      event->keyval == GDK_Return ||
      event->keyval == GDK_Tab)
    {
      gtk_tree_view_search_dialog_destroy (widget, tree_view);
      return TRUE;
    }

  /* select previous matching iter */
  if (event->keyval == GDK_Up)
    {
      gtk_tree_view_search_move (widget, tree_view, TRUE);
      return TRUE;
    }

  /* select next matching iter */
  if (event->keyval == GDK_Down)
    {
      gtk_tree_view_search_move (widget, tree_view, FALSE);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_tree_view_search_move (GtkWidget   *window,
			   GtkTreeView *tree_view,
			   gboolean     up)
{
  gboolean ret;
  gint *selected_iter;
  gint len;
  gint count = 0;
  gchar *text;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection;

  text = gtk_object_get_data (GTK_OBJECT (window), "gtk-tree-view-text");
  selected_iter = gtk_object_get_data (GTK_OBJECT (window), "gtk-tree-view-selected-iter");

  g_return_if_fail (text != NULL);

  if (!selected_iter || (up && *selected_iter == 1))
    return;

  len = strlen (text);

  if (len < 1)
    return;

  model = gtk_tree_view_get_model (tree_view);
  selection = gtk_tree_view_get_selection (tree_view);

  /* search */
  gtk_tree_selection_unselect_all (selection);
  gtk_tree_model_get_iter_root (model, &iter);

  ret = gtk_tree_view_search_iter (model, selection, &iter, text,
				   &count, up?((*selected_iter) - 1):((*selected_iter + 1)));

  if (ret)
    {
      /* found */
      *selected_iter += up?(-1):(1);
    }
  else
    {
      /* return to old iter */
      count = 0;
      gtk_tree_model_get_iter_root (model, &iter);
      gtk_tree_view_search_iter (model, selection,
				 &iter, text,
				 &count, *selected_iter);
    }
}

static gboolean
gtk_tree_view_search_equal_func (GtkTreeModel *model,
				 gint          column,
				 const gchar  *key,
				 GtkTreeIter  *iter,
				 gpointer      search_data)
{
  gboolean retval = TRUE;
  gchar *normalized_string;
  gchar *normalized_key;
  gchar *case_normalized_string;
  gchar *case_normalized_key;
  GValue value = {0,};
  gint key_len;

  gtk_tree_model_get_value (model, iter, column, &value);
  normalized_string = g_utf8_normalize (g_value_get_string (&value), -1, G_NORMALIZE_ALL);
  normalized_key = g_utf8_normalize (key, -1, G_NORMALIZE_ALL);
  case_normalized_string = g_utf8_casefold (normalized_string, -1);
  case_normalized_key = g_utf8_casefold (normalized_key, -1);

  key_len = strlen (case_normalized_key);

  if (!strncmp (case_normalized_key, case_normalized_string, key_len))
    retval = FALSE;

  g_value_unset (&value);
  g_free (normalized_key);
  g_free (normalized_string);
  g_free (case_normalized_key);
  g_free (case_normalized_string);

  return retval;
}

static gboolean
gtk_tree_view_search_iter (GtkTreeModel     *model,
			   GtkTreeSelection *selection,
			   GtkTreeIter      *iter,
			   const gchar      *text,
			   gint             *count,
			   gint              n)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;
  GtkTreePath *path;

  GtkTreeView *tree_view = gtk_tree_selection_get_tree_view (selection);
  GtkTreeViewColumn *column =
    gtk_tree_view_get_column (tree_view, tree_view->priv->search_column);

  path = gtk_tree_model_get_path (model, iter);
  _gtk_tree_view_find_node (tree_view, path, &tree, &node);

  do
    {
      if (! tree_view->priv->search_equal_func (model, tree_view->priv->search_column, text, iter, tree_view->priv->search_user_data))
        {
          (*count)++;
          if (*count == n)
            {
              gtk_tree_selection_select_iter (selection, iter);
              gtk_tree_view_scroll_to_cell (tree_view, path, column,
					    TRUE, 0.5, 0.5);
	      gtk_tree_view_real_set_cursor (tree_view, path, FALSE);

	      if (path)
		gtk_tree_path_free (path);

              return TRUE;
            }
        }

      if (node->children)
	{
	  gboolean has_child;
	  GtkTreeIter tmp;

	  tree = node->children;
	  node = tree->root;

	  while (node->left != tree->nil)
	    node = node->left;

	  tmp = *iter;
	  has_child = gtk_tree_model_iter_children (model, iter, &tmp);
	  gtk_tree_path_append_index (path, 0);

	  /* sanity check */
	  TREE_VIEW_INTERNAL_ASSERT (has_child, FALSE);
	}
      else
	{
	  gboolean done = FALSE;

	  do
	    {
	      node = _gtk_rbtree_next (tree, node);

	      if (node)
		{
		  gboolean has_next;

		  has_next = gtk_tree_model_iter_next (model, iter);

		  done = TRUE;
		  gtk_tree_path_next (path);

		  /* sanity check */
		  TREE_VIEW_INTERNAL_ASSERT (has_next, FALSE);
		}
	      else
		{
		  gboolean has_parent;
		  GtkTreeIter tmp_iter = *iter;

		  node = tree->parent_node;
		  tree = tree->parent_tree;

		  if (!tree)
		    {
		      if (path)
			gtk_tree_path_free (path);

		      /* we've run out of tree, done with this func */
		      return FALSE;
		    }

		  has_parent = gtk_tree_model_iter_parent (model,
							   iter,
							   &tmp_iter);
		  gtk_tree_path_up (path);

		  /* sanity check */
		  TREE_VIEW_INTERNAL_ASSERT (has_parent, FALSE);
		}
	    }
	  while (!done);
	}
    }
  while (1);

  if (path)
    gtk_tree_path_free (path);

  return FALSE;
}

static void
gtk_tree_view_search_init (GtkWidget   *entry,
			   GtkTreeView *tree_view)
{
  gint ret;
  gint *selected_iter;
  gint len;
  gint count = 0;
  const gchar *text;
  GtkWidget *window;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  window = gtk_widget_get_parent (entry);
  text = gtk_entry_get_text (GTK_ENTRY (entry));
  len = strlen (text);
  model = gtk_tree_view_get_model (tree_view);
  selection = gtk_tree_view_get_selection (tree_view);

  /* search */
  gtk_tree_selection_unselect_all (selection);
  selected_iter = gtk_object_get_data (GTK_OBJECT (window), "gtk-tree-view-selected-iter");
  if (selected_iter)
    g_free (selected_iter);
  gtk_object_remove_data (GTK_OBJECT (window), "gtk-tree-view-selected-iter");

  if (len < 1)
    return;

  gtk_tree_model_get_iter_root (model, &iter);

  ret = gtk_tree_view_search_iter (model, selection,
				   &iter, text,
				   &count, 1);

  if (ret)
    {
      selected_iter = g_malloc (sizeof (int));
      *selected_iter = 1;
      gtk_object_set_data (GTK_OBJECT (window), "gtk-tree-view-selected-iter",
                           selected_iter);
    }
}

static void
gtk_tree_view_remove_widget (GtkCellEditable *cell_editable,
			     GtkTreeView     *tree_view)
{
  if (tree_view->priv->edited_column == NULL)
    return;

  _gtk_tree_view_column_stop_editing (tree_view->priv->edited_column);
  tree_view->priv->edited_column = NULL;

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));

  gtk_container_remove (GTK_CONTAINER (tree_view),
			GTK_WIDGET (cell_editable));
}

static gboolean
gtk_tree_view_start_editing (GtkTreeView *tree_view,
			     GtkTreePath *cursor_path)
{
  GtkTreeIter iter;
  GdkRectangle background_area;
  GdkRectangle cell_area;
  GtkCellEditable *editable_widget = NULL;
  gchar *path_string;
  guint flags = 0; /* can be 0, as the flags are primarily for rendering */
  gint retval = FALSE;
  GtkRBTree *cursor_tree;
  GtkRBNode *cursor_node;

  g_assert (tree_view->priv->focus_column);

  if (! GTK_WIDGET_REALIZED (tree_view))
    return FALSE;

  if (_gtk_tree_view_find_node (tree_view, cursor_path, &cursor_tree, &cursor_node) ||
      cursor_node == NULL)
    return FALSE;

  path_string = gtk_tree_path_to_string (cursor_path);
  gtk_tree_model_get_iter (tree_view->priv->model, &iter, cursor_path);
  gtk_tree_view_column_cell_set_cell_data (tree_view->priv->focus_column,
					   tree_view->priv->model,
					   &iter,
					   GTK_RBNODE_FLAG_SET (cursor_node, GTK_RBNODE_IS_PARENT),
					   cursor_node->children?TRUE:FALSE);
  gtk_tree_view_get_background_area (tree_view,
				     cursor_path,
				     tree_view->priv->focus_column,
				     &background_area);
  gtk_tree_view_get_cell_area (tree_view,
			       cursor_path,
			       tree_view->priv->focus_column,
			       &cell_area);
  if (_gtk_tree_view_column_cell_event (tree_view->priv->focus_column,
					&editable_widget,
					NULL,
					path_string,
					&background_area,
					&cell_area,
					flags))
    {
      retval = TRUE;
      if (editable_widget != NULL)
	{
	  gtk_tree_view_real_start_editing (tree_view,
					    tree_view->priv->focus_column,
					    cursor_path,
					    editable_widget,
					    &cell_area,
					    NULL,
					    flags);
	}

    }
  g_free (path_string);
  return retval;
}

static void
gtk_tree_view_real_start_editing (GtkTreeView       *tree_view,
				  GtkTreeViewColumn *column,
				  GtkTreePath       *path,
				  GtkCellEditable   *cell_editable,
				  GdkRectangle      *cell_area,
				  GdkEvent          *event,
				  guint              flags)
{
  tree_view->priv->edited_column = column;
  _gtk_tree_view_column_start_editing (column, GTK_CELL_EDITABLE (cell_editable));
  gtk_tree_view_real_set_cursor (tree_view, path, FALSE);
  GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS);
  gtk_tree_view_put (tree_view,
		     GTK_WIDGET (cell_editable),
		     cell_area->x, cell_area->y, cell_area->width, cell_area->height);
  gtk_cell_editable_start_editing (GTK_CELL_EDITABLE (cell_editable),
				   (GdkEvent *)event);
  gtk_widget_grab_focus (GTK_WIDGET (cell_editable));
  g_signal_connect (cell_editable, "remove_widget", G_CALLBACK (gtk_tree_view_remove_widget), tree_view);
}

static void
gtk_tree_view_stop_editing (GtkTreeView *tree_view,
			    gboolean     cancel_editing)
{
  if (tree_view->priv->edited_column == NULL)
    return;

  if (! cancel_editing)
    gtk_cell_editable_editing_done (tree_view->priv->edited_column->editable_widget);

  gtk_cell_editable_remove_widget (tree_view->priv->edited_column->editable_widget);
}
