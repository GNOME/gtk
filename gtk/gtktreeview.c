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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */


#include "config.h"

#include "gtktreeview.h"

#include "gtkadjustmentprivate.h"
#include "gtkbox.h"
#include "gtkbindings.h"
#include "gtkbuildable.h"
#include "gtkbutton.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderer.h"
#include "gtkcontainer.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkdragdest.h"
#include "gtkdragsource.h"
#include "gtkentryprivate.h"
#include "gtksearchentryprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtkeventcontrollermotion.h"
#include "gtkeventcontrollerscroll.h"
#include "gtkframe.h"
#include "gtkgesturedrag.h"
#include "gtkgestureclick.h"
#include "gtkgesturesingle.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtktext.h"
#include "gtktreerbtreeprivate.h"
#include "gtkrendericonprivate.h"
#include "gtkscrollable.h"
#include "gtksettingsprivate.h"
#include "gtksnapshot.h"
#include "gtkstylecontextprivate.h"
#include "gtktooltip.h"
#include "gtktreednd.h"
#include "gtktreemodelsort.h"
#include "gtktreeprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetpath.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowgroup.h"
#include "gtknative.h"

#include "a11y/gtktreeviewaccessibleprivate.h"

#include "gdk/gdkeventsprivate.h"
#include "gdk/gdktextureprivate.h"

#include <math.h>
#include <string.h>

/**
 * SECTION:gtktreeview
 * @Short_description: A widget for displaying both trees and lists
 * @Title: GtkTreeView
 * @See_also: #GtkTreeViewColumn, #GtkTreeSelection, #GtkTreeModel,
 *   [GtkTreeView drag-and-drop][gtk3-GtkTreeView-drag-and-drop],
 *   #GtkTreeSortable, #GtkTreeModelSort, #GtkListStore, #GtkTreeStore,
 *   #GtkCellRenderer, #GtkCellEditable, #GtkCellRendererPixbuf,
 *   #GtkCellRendererText, #GtkCellRendererToggle
 *
 * Widget that displays any object that implements the #GtkTreeModel interface.
 *
 * Please refer to the
 * [tree widget conceptual overview][TreeWidget]
 * for an overview of all the objects and data types related
 * to the tree widget and how they work together.
 *
 * Several different coordinate systems are exposed in the GtkTreeView API.
 * These are:
 *
 * ![](tree-view-coordinates.png)
 *
 * Coordinate systems in GtkTreeView API:
 *
 * - Widget coordinates: Coordinates relative to the widget (usually `widget->window`).
 *
 * - Bin window coordinates: Coordinates relative to the window that GtkTreeView renders to.
 *
 * - Tree coordinates: Coordinates relative to the entire scrollable area of GtkTreeView. These
 *   coordinates start at (0, 0) for row 0 of the tree.
 *
 * Several functions are available for converting between the different
 * coordinate systems.  The most common translations are between widget and bin
 * window coordinates and between bin window and tree coordinates. For the
 * former you can use gtk_tree_view_convert_widget_to_bin_window_coords()
 * (and vice versa), for the latter gtk_tree_view_convert_bin_window_to_tree_coords()
 * (and vice versa).
 *
 * # GtkTreeView as GtkBuildable
 *
 * The GtkTreeView implementation of the GtkBuildable interface accepts
 * #GtkTreeViewColumn objects as <child> elements and exposes the internal
 * #GtkTreeSelection in UI definitions.
 *
 * An example of a UI definition fragment with GtkTreeView:
 * |[
 * <object class="GtkTreeView" id="treeview">
 *   <property name="model">liststore1</property>
 *   <child>
 *     <object class="GtkTreeViewColumn" id="test-column">
 *       <property name="title">Test</property>
 *       <child>
 *         <object class="GtkCellRendererText" id="test-renderer"/>
 *         <attributes>
 *           <attribute name="text">1</attribute>
 *         </attributes>
 *       </child>
 *     </object>
 *   </child>
 *   <child internal-child="selection">
 *     <object class="GtkTreeSelection" id="selection">
 *       <signal name="changed" handler="on_treeview_selection_changed"/>
 *     </object>
 *   </child>
 * </object>
 * ]|
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * treeview.view
 * ├── header
 * │   ├── <column header>
 * ┊   ┊
 * │   ╰── <column header>
 * │
 * ╰── [rubberband]
 * ]|
 *
 * GtkTreeView has a main CSS node with name treeview and style class .view.
 * It has a subnode with name header, which is the parent for all the column
 * header widgets' CSS nodes.
 * For rubberband selection, a subnode with name rubberband is used.
 */

enum
{
  DRAG_COLUMN_WINDOW_STATE_UNSET = 0,
  DRAG_COLUMN_WINDOW_STATE_ORIGINAL = 1,
  DRAG_COLUMN_WINDOW_STATE_ARROW = 2,
  DRAG_COLUMN_WINDOW_STATE_ARROW_LEFT = 3,
  DRAG_COLUMN_WINDOW_STATE_ARROW_RIGHT = 4
};

enum
{
  RUBBER_BAND_OFF = 0,
  RUBBER_BAND_MAYBE_START = 1,
  RUBBER_BAND_ACTIVE = 2
};

typedef enum {
  CLEAR_AND_SELECT = (1 << 0),
  CLAMP_NODE       = (1 << 1),
  CURSOR_INVALID   = (1 << 2)
} SetCursorFlags;

 /* This lovely little value is used to determine how far away from the title bar
  * you can move the mouse and still have a column drag work.
  */
#define TREE_VIEW_COLUMN_DRAG_DEAD_MULTIPLIER(tree_view) (10*gtk_tree_view_get_effective_header_height(tree_view))

#ifdef __GNUC__

#define TREE_VIEW_INTERNAL_ASSERT(expr, ret)     G_STMT_START{          \
     if (!(expr))                                                       \
       {                                                                \
         g_log (G_LOG_DOMAIN,                                           \
                G_LOG_LEVEL_CRITICAL,                                   \
		"%s (%s): assertion `%s' failed.\n"                     \
	        "There is a disparity between the internal view of the GtkTreeView,\n"    \
		"and the GtkTreeModel.  This generally means that the model has changed\n"\
		"without letting the view know.  Any display from now on is likely to\n"  \
		"be incorrect.\n",                                                        \
                G_STRLOC,                                               \
                G_STRFUNC,                                              \
                #expr);                                                 \
         return ret;                                                    \
       };                               }G_STMT_END

#define TREE_VIEW_INTERNAL_ASSERT_VOID(expr)     G_STMT_START{          \
     if (!(expr))                                                       \
       {                                                                \
         g_log (G_LOG_DOMAIN,                                           \
                G_LOG_LEVEL_CRITICAL,                                   \
		"%s (%s): assertion `%s' failed.\n"                     \
	        "There is a disparity between the internal view of the GtkTreeView,\n"    \
		"and the GtkTreeModel.  This generally means that the model has changed\n"\
		"without letting the view know.  Any display from now on is likely to\n"  \
		"be incorrect.\n",                                                        \
                G_STRLOC,                                               \
                G_STRFUNC,                                              \
                #expr);                                                 \
         return;                                                        \
       };                               }G_STMT_END

#else

#define TREE_VIEW_INTERNAL_ASSERT(expr, ret)     G_STMT_START{          \
     if (!(expr))                                                       \
       {                                                                \
         g_log (G_LOG_DOMAIN,                                           \
                G_LOG_LEVEL_CRITICAL,                                   \
		"file %s: line %d: assertion `%s' failed.\n"       \
	        "There is a disparity between the internal view of the GtkTreeView,\n"    \
		"and the GtkTreeModel.  This generally means that the model has changed\n"\
		"without letting the view know.  Any display from now on is likely to\n"  \
		"be incorrect.\n",                                                        \
                __FILE__,                                               \
                __LINE__,                                               \
                #expr);                                                 \
         return ret;                                                    \
       };                               }G_STMT_END

#define TREE_VIEW_INTERNAL_ASSERT_VOID(expr)     G_STMT_START{          \
     if (!(expr))                                                       \
       {                                                                \
         g_log (G_LOG_DOMAIN,                                           \
                G_LOG_LEVEL_CRITICAL,                                   \
		"file %s: line %d: assertion '%s' failed.\n"            \
	        "There is a disparity between the internal view of the GtkTreeView,\n"    \
		"and the GtkTreeModel.  This generally means that the model has changed\n"\
		"without letting the view know.  Any display from now on is likely to\n"  \
		"be incorrect.\n",                                                        \
                __FILE__,                                               \
                __LINE__,                                               \
                #expr);                                                 \
         return;                                                        \
       };                               }G_STMT_END
#endif

#define GTK_TREE_VIEW_PRIORITY_VALIDATE (GDK_PRIORITY_REDRAW + 5)
#define GTK_TREE_VIEW_PRIORITY_SCROLL_SYNC (GTK_TREE_VIEW_PRIORITY_VALIDATE + 2)
/* 3/5 of gdkframeclockidle.c's FRAME_INTERVAL (16667 microsecs) */
#define GTK_TREE_VIEW_TIME_MS_PER_IDLE 10
#define SCROLL_EDGE_SIZE 15
#define GTK_TREE_VIEW_SEARCH_DIALOG_TIMEOUT 5000
#define AUTO_EXPAND_TIMEOUT 500

/* Translate from bin_window coordinates to rbtree (tree coordinates) and
 * vice versa.
 */
#define TREE_WINDOW_Y_TO_RBTREE_Y(tree_view,y) ((y) + tree_view->priv->dy)
#define RBTREE_Y_TO_TREE_WINDOW_Y(tree_view,y) ((y) - tree_view->priv->dy)

/* Vertical separator width. Must be an even number. */
#define _TREE_VIEW_VERTICAL_SEPARATOR 2
/* Horizontal separator width. Must be an even number.  */
#define _TREE_VIEW_HORIZONTAL_SEPARATOR 4
/* Tree view grid line width, in pixels */
#define _TREE_VIEW_GRID_LINE_WIDTH 1
/* Tree view line width, in pixels */
#define _TREE_VIEW_TREE_LINE_WIDTH 1

typedef struct _GtkTreeViewColumnReorder GtkTreeViewColumnReorder;
struct _GtkTreeViewColumnReorder
{
  gint left_align;
  gint right_align;
  GtkTreeViewColumn *left_column;
  GtkTreeViewColumn *right_column;
};

typedef struct _GtkTreeViewChild GtkTreeViewChild;
struct _GtkTreeViewChild
{
  GtkWidget *widget;
  GtkTreeRBNode *node;
  GtkTreeRBTree *tree;
  GtkTreeViewColumn *column;
  GtkBorder border;
};


typedef struct _TreeViewDragInfo TreeViewDragInfo;
struct _TreeViewDragInfo
{
  GdkModifierType start_button_mask;
  GdkDragAction source_actions;

  guint source_set : 1;
  guint dest_set : 1;
};

typedef struct _GtkTreeViewClass      GtkTreeViewClass;
typedef struct _GtkTreeViewPrivate    GtkTreeViewPrivate;

struct _GtkTreeView
{
  GtkContainer parent;

  GtkTreeViewPrivate *priv;
};

struct _GtkTreeViewClass
{
  GtkContainerClass parent_class;

  void     (* row_activated)              (GtkTreeView       *tree_view,
                                           GtkTreePath       *path,
                                           GtkTreeViewColumn *column);
  gboolean (* test_expand_row)            (GtkTreeView       *tree_view,
                                           GtkTreeIter       *iter,
                                           GtkTreePath       *path);
  gboolean (* test_collapse_row)          (GtkTreeView       *tree_view,
                                           GtkTreeIter       *iter,
                                           GtkTreePath       *path);
  void     (* row_expanded)               (GtkTreeView       *tree_view,
                                           GtkTreeIter       *iter,
                                           GtkTreePath       *path);
  void     (* row_collapsed)              (GtkTreeView       *tree_view,
                                           GtkTreeIter       *iter,
                                           GtkTreePath       *path);
  void     (* columns_changed)            (GtkTreeView       *tree_view);
  void     (* cursor_changed)             (GtkTreeView       *tree_view);

  /* Key Binding signals */
  gboolean (* move_cursor)                (GtkTreeView       *tree_view,
                                           GtkMovementStep    step,
                                           gint               count);
  gboolean (* select_all)                 (GtkTreeView       *tree_view);
  gboolean (* unselect_all)               (GtkTreeView       *tree_view);
  gboolean (* select_cursor_row)          (GtkTreeView       *tree_view,
                                           gboolean           start_editing);
  gboolean (* toggle_cursor_row)          (GtkTreeView       *tree_view);
  gboolean (* expand_collapse_cursor_row) (GtkTreeView       *tree_view,
                                           gboolean           logical,
                                           gboolean           expand,
                                           gboolean           open_all);
  gboolean (* select_cursor_parent)       (GtkTreeView       *tree_view);
  gboolean (* start_interactive_search)   (GtkTreeView       *tree_view);
};


struct _GtkTreeViewPrivate
{
  GtkTreeModel *model;

  /* tree information */
  GtkTreeRBTree *tree;

  /* Container info */
  GList *children;
  gint width;

  guint presize_handler_tick_cb;

  /* Adjustments */
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
  gint           min_display_width;
  gint           min_display_height;

  /* CSS nodes */
  GtkCssNode *header_node;

  /* Scroll position state keeping */
  GtkTreeRowReference *top_row;
  gint top_row_dy;
  /* dy == y pos of top_row + top_row_dy */
  /* we cache it for simplicity of the code */
  gint dy;

  guint validate_rows_timer;
  guint scroll_sync_timer;

  /* Indentation and expander layout */
  GtkTreeViewColumn *expander_column;

  gint level_indentation;

  /* Key navigation (focus), selection */
  gint cursor_offset;

  GtkTreeRowReference *anchor;
  GtkTreeRBNode *cursor_node;
  GtkTreeRBTree *cursor_tree;

  GtkTreeViewColumn *focus_column;

  /* Current pressed node, previously pressed, prelight */
  GtkTreeRBNode *button_pressed_node;
  GtkTreeRBTree *button_pressed_tree;

  gint press_start_x;
  gint press_start_y;

  gint event_last_x;
  gint event_last_y;

  GtkTreeRBNode *prelight_node;
  GtkTreeRBTree *prelight_tree;

  /* Cell Editing */
  GtkTreeViewColumn *edited_column;

  /* Auto expand/collapse timeout in hover mode */
  guint auto_expand_timeout;

  /* Selection information */
  GtkTreeSelection *selection;

  /* Header information */
  gint header_height;
  gint n_columns;
  GList *columns;

  GtkTreeViewColumnDropFunc column_drop_func;
  gpointer column_drop_func_data;
  GDestroyNotify column_drop_func_data_destroy;
  GList *column_drag_info;
  GtkTreeViewColumnReorder *cur_reorder;

  gint prev_width_before_expander;

  /* Scroll timeout (e.g. during dnd, rubber banding) */
  guint scroll_timeout;

  /* Interactive Header reordering */
  GtkTreeViewColumn *drag_column;
  gint drag_column_x;
  gint drag_column_y;

  /* Interactive Header Resizing */
  gint drag_pos;
  gint x_drag;

  /* Row drag-and-drop */
  GtkTreeRowReference *drag_dest_row;
  GtkTreeViewDropPosition drag_dest_pos;
  guint open_dest_timeout;

  /* Rubber banding */
  gint rubber_band_status;
  gint rubber_band_x;
  gint rubber_band_y;
  gint rubber_band_extend;
  gint rubber_band_modify;

  /* fixed height */
  gint fixed_height;

  GtkTreeRBNode *rubber_band_start_node;
  GtkTreeRBTree *rubber_band_start_tree;

  GtkTreeRBNode *rubber_band_end_node;
  GtkTreeRBTree *rubber_band_end_tree;
  GtkCssNode *rubber_band_cssnode;

  /* Scroll-to functionality when unrealized */
  GtkTreeRowReference *scroll_to_path;
  GtkTreeViewColumn *scroll_to_column;
  gfloat scroll_to_row_align;
  gfloat scroll_to_col_align;

  /* Interactive search */
  gint selected_iter;
  gint search_column;
  GtkTreeViewSearchPositionFunc search_position_func;
  GtkTreeViewSearchEqualFunc search_equal_func;
  gpointer search_user_data;
  GDestroyNotify search_destroy;
  gpointer search_position_user_data;
  GDestroyNotify search_position_destroy;
  GtkWidget *search_window;
  GtkWidget *search_entry;
  gulong search_entry_changed_id;
  guint typeselect_flush_timeout;

  /* Grid and tree lines */
  GtkTreeViewGridLines grid_lines;
  gboolean tree_lines_enabled;

  /* Row separators */
  GtkTreeViewRowSeparatorFunc row_separator_func;
  gpointer row_separator_data;
  GDestroyNotify row_separator_destroy;

  /* Gestures */
  GtkGesture *click_gesture;
  GtkGesture *drag_gesture; /* Rubberbanding, row DnD */
  GtkGesture *column_drag_gesture; /* Column reordering, resizing */

  /* Tooltip support */
  gint tooltip_column;

  int expander_size;

  GdkRGBA grid_line_color; /* Color used in the textures */
  GdkTexture *horizontal_grid_line_texture;
  GdkTexture *vertical_grid_line_texture;

  GdkRGBA tree_line_color; /* Color used in the textures */
  GdkTexture *horizontal_tree_line_texture;
  GdkTexture *vertical_tree_line_texture;

  /* Here comes the bitfield */
  guint scroll_to_use_align : 1;

  guint fixed_height_mode : 1;
  guint fixed_height_check : 1;

  guint activate_on_single_click : 1;
  guint reorderable : 1;
  guint header_has_focus : 1;
  guint drag_column_surface_state : 3;
  guint mark_rows_col_dirty : 1;

  /* for DnD */
  guint empty_view_drop : 1;

  guint modify_selection_pressed : 1;
  guint extend_selection_pressed : 1;

  guint in_top_row_to_dy : 1;

  /* interactive search */
  guint enable_search : 1;
  guint disable_popdown : 1;
  guint search_custom_entry_set : 1;
  
  guint hover_selection : 1;
  guint hover_expand : 1;
  guint imcontext_changed : 1;

  guint in_scroll : 1;

  guint rubber_banding_enable : 1;

  guint in_grab : 1;

  /* Whether our key press handler is to avoid sending an unhandled binding to the search entry */
  guint search_entry_avoid_unhandled_binding : 1;

  /* GtkScrollablePolicy needs to be checked when
   * driving the scrollable adjustment values */
  guint hscroll_policy : 1;
  guint vscroll_policy : 1;

  /* GtkTreeView flags */
  guint is_list : 1;
  guint show_expanders : 1;
  guint in_column_resize : 1;
  guint arrow_prelit : 1;
  guint headers_visible : 1;
  guint draw_keyfocus : 1;
  guint model_setup : 1;
  guint in_column_drag : 1;
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
  UNSELECT_ALL,
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
  PROP_HEADERS_VISIBLE,
  PROP_HEADERS_CLICKABLE,
  PROP_EXPANDER_COLUMN,
  PROP_REORDERABLE,
  PROP_ENABLE_SEARCH,
  PROP_SEARCH_COLUMN,
  PROP_FIXED_HEIGHT_MODE,
  PROP_HOVER_SELECTION,
  PROP_HOVER_EXPAND,
  PROP_SHOW_EXPANDERS,
  PROP_LEVEL_INDENTATION,
  PROP_RUBBER_BANDING,
  PROP_ENABLE_GRID_LINES,
  PROP_ENABLE_TREE_LINES,
  PROP_TOOLTIP_COLUMN,
  PROP_ACTIVATE_ON_SINGLE_CLICK,
  LAST_PROP,
  /* overridden */
  PROP_HADJUSTMENT = LAST_PROP,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
};

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

/* gtkwidget signals */
static void     gtk_tree_view_destroy              (GtkWidget        *widget);
static void     gtk_tree_view_realize              (GtkWidget        *widget);
static void     gtk_tree_view_unrealize            (GtkWidget        *widget);
static void     gtk_tree_view_map                  (GtkWidget        *widget);
static void     gtk_tree_view_measure              (GtkWidget        *widget,
                                                    GtkOrientation  orientation,
                                                    int             for_size,
                                                    int            *minimum,
                                                    int            *natural,
                                                    int            *minimum_baseline,
                                                    int            *natural_baseline);
static void     gtk_tree_view_size_allocate        (GtkWidget      *widget,
                                                    int             width,
                                                    int             height,
                                                    int             baseline);
static void     gtk_tree_view_snapshot             (GtkWidget        *widget,
                                                    GtkSnapshot      *snapshot);

static gboolean gtk_tree_view_key_controller_key_pressed  (GtkEventControllerKey *key,
                                                           guint                  keyval,
                                                           guint                  keycode,
                                                           GdkModifierType        state,
                                                           GtkTreeView           *tree_view);
static void     gtk_tree_view_key_controller_key_released (GtkEventControllerKey *key,
                                                           guint                  keyval,
                                                           guint                  keycode,
                                                           GdkModifierType        state,
                                                           GtkTreeView           *tree_view);
static void     gtk_tree_view_key_controller_focus_out    (GtkEventControllerKey *key,
                                                           GdkCrossingMode        mode,
                                                           GdkNotifyType          detail,
                                                           GtkTreeView           *tree_view);

static gint     gtk_tree_view_focus                (GtkWidget        *widget,
						    GtkDirectionType  direction);
static gboolean gtk_tree_view_grab_focus           (GtkWidget        *widget);
static void     gtk_tree_view_style_updated        (GtkWidget        *widget);

/* container signals */
static void     gtk_tree_view_remove               (GtkContainer     *container,
						    GtkWidget        *widget);
static void     gtk_tree_view_forall               (GtkContainer     *container,
						    GtkCallback       callback,
						    gpointer          callback_data);

/* Source side drag signals */
static void gtk_tree_view_drag_begin       (GtkWidget        *widget,
                                            GdkDrag          *drag);
static void gtk_tree_view_drag_end         (GtkWidget        *widget,
                                            GdkDrag          *drag);
static void gtk_tree_view_drag_data_get    (GtkWidget        *widget,
                                            GdkDrag          *drag,
                                            GtkSelectionData *selection_data);
static void gtk_tree_view_drag_data_delete (GtkWidget        *widget,
                                            GdkDrag          *drag);

/* Target side drag signals */
static void     gtk_tree_view_drag_leave         (GtkWidget        *widget,
                                                  GdkDrop          *drop);
static gboolean gtk_tree_view_drag_motion        (GtkWidget        *widget,
                                                  GdkDrop          *drop,
                                                  gint              x,
                                                  gint              y);
static gboolean gtk_tree_view_drag_drop          (GtkWidget        *widget,
                                                  GdkDrop          *drop,
                                                  gint              x,
                                                  gint              y);
static void     gtk_tree_view_drag_data_received (GtkWidget        *widget,
                                                  GdkDrop          *drop,
                                                  GtkSelectionData *selection_data);

/* tree_model signals */
static gboolean gtk_tree_view_real_move_cursor            (GtkTreeView     *tree_view,
							   GtkMovementStep  step,
							   gint             count);
static gboolean gtk_tree_view_real_select_all             (GtkTreeView     *tree_view);
static gboolean gtk_tree_view_real_unselect_all           (GtkTreeView     *tree_view);
static gboolean gtk_tree_view_real_select_cursor_row      (GtkTreeView     *tree_view,
							   gboolean         start_editing);
static gboolean gtk_tree_view_real_toggle_cursor_row      (GtkTreeView     *tree_view);
static gboolean gtk_tree_view_real_expand_collapse_cursor_row (GtkTreeView     *tree_view,
							       gboolean         logical,
							       gboolean         expand,
							       gboolean         open_all);
static gboolean gtk_tree_view_real_select_cursor_parent   (GtkTreeView     *tree_view);
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
static gboolean validate_row                              (GtkTreeView     *tree_view,
                                                           GtkTreeRBTree   *tree,
                                                           GtkTreeRBNode   *node,
                                                           GtkTreeIter     *iter,
                                                           GtkTreePath     *path);
static void     validate_visible_area    (GtkTreeView *tree_view);
static gboolean do_validate_rows         (GtkTreeView *tree_view,
					  gboolean     queue_resize);
static gboolean validate_rows            (GtkTreeView *tree_view);
static void     install_presize_handler  (GtkTreeView *tree_view);
static void     install_scroll_sync_handler (GtkTreeView *tree_view);
static void     gtk_tree_view_set_top_row   (GtkTreeView *tree_view,
					     GtkTreePath *path,
					     gint         offset);
static void	gtk_tree_view_dy_to_top_row (GtkTreeView *tree_view);
static void     gtk_tree_view_top_row_to_dy (GtkTreeView *tree_view);
static void     invalidate_empty_focus      (GtkTreeView *tree_view);

/* Internal functions */
static gboolean gtk_tree_view_is_expander_column             (GtkTreeView        *tree_view,
							      GtkTreeViewColumn  *column);
static inline gboolean gtk_tree_view_draw_expanders          (GtkTreeView        *tree_view);
static void     gtk_tree_view_add_move_binding               (GtkBindingSet      *binding_set,
							      guint               keyval,
							      guint               modmask,
							      gboolean            add_shifted_binding,
							      GtkMovementStep     step,
							      gint                count);
static gint     gtk_tree_view_unref_and_check_selection_tree (GtkTreeView        *tree_view,
							      GtkTreeRBTree      *tree);
static void     gtk_tree_view_snapshot_arrow                 (GtkTreeView        *tree_view,
                                                              GtkSnapshot        *snapshot,
							      GtkTreeRBTree      *tree,
							      GtkTreeRBNode      *node);
static void     gtk_tree_view_get_arrow_xrange               (GtkTreeView        *tree_view,
							      GtkTreeRBTree      *tree,
							      gint               *x1,
							      gint               *x2);
static void     gtk_tree_view_adjustment_changed             (GtkAdjustment      *adjustment,
							      GtkTreeView        *tree_view);
static void     gtk_tree_view_build_tree                     (GtkTreeView        *tree_view,
							      GtkTreeRBTree          *tree,
							      GtkTreeIter        *iter,
							      gint                depth,
							      gboolean            recurse);
static void     gtk_tree_view_clamp_node_visible             (GtkTreeView        *tree_view,
							      GtkTreeRBTree      *tree,
							      GtkTreeRBNode      *node);
static void     gtk_tree_view_clamp_column_visible           (GtkTreeView        *tree_view,
							      GtkTreeViewColumn  *column,
							      gboolean            focus_to_cell);
static gboolean gtk_tree_view_maybe_begin_dragging_row       (GtkTreeView        *tree_view);
static void     gtk_tree_view_focus_to_cursor                (GtkTreeView        *tree_view);
static void     gtk_tree_view_move_cursor_up_down            (GtkTreeView        *tree_view,
							      gint                count);
static void     gtk_tree_view_move_cursor_page_up_down       (GtkTreeView        *tree_view,
							      gint                count);
static void     gtk_tree_view_move_cursor_left_right         (GtkTreeView        *tree_view,
							      gint                count);
static void     gtk_tree_view_move_cursor_start_end          (GtkTreeView        *tree_view,
							      gint                count);
static gboolean gtk_tree_view_real_collapse_row              (GtkTreeView        *tree_view,
							      GtkTreePath        *path,
							      GtkTreeRBTree      *tree,
							      GtkTreeRBNode      *node,
							      gboolean            animate);
static gboolean gtk_tree_view_real_expand_row                (GtkTreeView        *tree_view,
							      GtkTreePath        *path,
							      GtkTreeRBTree      *tree,
							      GtkTreeRBNode      *node,
							      gboolean            open_all,
							      gboolean            animate);
static void     gtk_tree_view_real_set_cursor                (GtkTreeView        *tree_view,
							      GtkTreePath        *path,
                                                              SetCursorFlags      flags);
static gboolean gtk_tree_view_has_can_focus_cell             (GtkTreeView        *tree_view);
static void     column_sizing_notify                         (GObject            *object,
                                                              GParamSpec         *pspec,
                                                              gpointer            data);
static void     gtk_tree_view_stop_rubber_band               (GtkTreeView        *tree_view);
static void     update_prelight                              (GtkTreeView        *tree_view,
                                                              int                 x,
                                                              int                 y);

static inline gint gtk_tree_view_get_effective_header_height (GtkTreeView        *tree_view);

static inline gint gtk_tree_view_get_cell_area_y_offset      (GtkTreeView        *tree_view,
                                                              GtkTreeRBTree      *tree,
                                                              GtkTreeRBNode      *node);
static inline gint gtk_tree_view_get_cell_area_height        (GtkTreeView        *tree_view,
                                                              GtkTreeRBNode      *node);

static inline gint gtk_tree_view_get_row_y_offset            (GtkTreeView        *tree_view,
                                                              GtkTreeRBTree      *tree,
                                                              GtkTreeRBNode      *node);
static inline gint gtk_tree_view_get_row_height              (GtkTreeView        *tree_view,
                                                              GtkTreeRBNode      *node);

/* interactive search */
static void     gtk_tree_view_ensure_interactive_directory (GtkTreeView *tree_view);
static void     gtk_tree_view_search_window_hide        (GtkWidget        *search_window,
                                                         GtkTreeView      *tree_view,
                                                         GdkDevice        *device);
static void     gtk_tree_view_search_position_func      (GtkTreeView      *tree_view,
							 GtkWidget        *search_window,
							 gpointer          user_data);
static void     gtk_tree_view_search_preedit_changed    (GtkText          *text,
                                                         const char       *preedit,
							 GtkTreeView      *tree_view);
static void     gtk_tree_view_search_changed            (GtkEditable      *editable,
                                                         GtkTreeView      *tree_view);
static void     gtk_tree_view_search_activate           (GtkEntry         *entry,
							 GtkTreeView      *tree_view);
static void     gtk_tree_view_search_pressed_cb         (GtkGesture       *gesture,
                                                         int               n_press,
                                                         double            x,
                                                         double            y,
							 GtkTreeView      *tree_view);
static gboolean gtk_tree_view_search_scroll_event       (GtkWidget        *entry,
							 gdouble           dx,
                                                         gdouble           dy,
							 GtkTreeView      *tree_view);
static gboolean gtk_tree_view_search_key_pressed        (GtkEventControllerKey *key,
                                                         guint                  keyval,
                                                         guint                  keycode,
                                                         GdkModifierType        state,
                                                         GtkTreeView           *tree_view);
static gboolean gtk_tree_view_search_move               (GtkWidget        *window,
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
                                                         GtkTreePath      *path,
                                                         GtkTreeViewColumn*column,
                                                         const GtkBorder  *border);
static gboolean gtk_tree_view_start_editing             (GtkTreeView      *tree_view,
							 GtkTreePath      *cursor_path,
							 gboolean          edit_only);
static void gtk_tree_view_stop_editing                  (GtkTreeView *tree_view,
							 gboolean     cancel_editing);
static gboolean gtk_tree_view_real_start_interactive_search (GtkTreeView *tree_view,
                                                             GdkDevice   *device,
							     gboolean     keybinding);
static gboolean gtk_tree_view_start_interactive_search      (GtkTreeView *tree_view);
static GtkTreeViewColumn *gtk_tree_view_get_drop_column (GtkTreeView       *tree_view,
							 GtkTreeViewColumn *column,
							 gint               drop_position);

/* GtkBuildable */
static void     gtk_tree_view_buildable_add_child          (GtkBuildable      *tree_view,
							    GtkBuilder        *builder,
							    GObject           *child,
							    const gchar       *type);
static GObject *gtk_tree_view_buildable_get_internal_child (GtkBuildable      *buildable,
							    GtkBuilder        *builder,
							    const gchar       *childname);
static void     gtk_tree_view_buildable_init               (GtkBuildableIface *iface);

/* GtkScrollable */
static void     gtk_tree_view_scrollable_init              (GtkScrollableInterface *iface);

static void           gtk_tree_view_do_set_hadjustment (GtkTreeView   *tree_view,
                                                        GtkAdjustment *adjustment);
static void           gtk_tree_view_do_set_vadjustment (GtkTreeView   *tree_view,
                                                        GtkAdjustment *adjustment);

static gboolean scroll_row_timeout                   (gpointer     data);
static void     add_scroll_timeout                   (GtkTreeView *tree_view);
static void     remove_scroll_timeout                (GtkTreeView *tree_view);

static void     grab_focus_and_unset_draw_keyfocus   (GtkTreeView *tree_view);

/* Gestures */
static void gtk_tree_view_column_click_gesture_pressed (GtkGestureClick *gesture,
                                                             gint                  n_press,
                                                             gdouble               x,
                                                             gdouble               y,
                                                             GtkTreeView          *tree_view);

static void gtk_tree_view_click_gesture_pressed        (GtkGestureClick *gesture,
                                                             gint                  n_press,
                                                             gdouble               x,
                                                             gdouble               y,
                                                             GtkTreeView          *tree_view);
static void gtk_tree_view_click_gesture_released       (GtkGestureClick *gesture,
                                                             gint                  n_press,
                                                             gdouble               x,
                                                             gdouble               y,
                                                             GtkTreeView          *tree_view);

static void gtk_tree_view_column_drag_gesture_begin         (GtkGestureDrag *gesture,
                                                             gdouble         start_x,
                                                             gdouble         start_y,
                                                             GtkTreeView    *tree_view);
static void gtk_tree_view_column_drag_gesture_update        (GtkGestureDrag *gesture,
                                                             gdouble         offset_x,
                                                             gdouble         offset_y,
                                                             GtkTreeView    *tree_view);
static void gtk_tree_view_column_drag_gesture_end           (GtkGestureDrag *gesture,
                                                             gdouble         offset_x,
                                                             gdouble         offset_y,
                                                             GtkTreeView    *tree_view);

static void gtk_tree_view_drag_gesture_begin                (GtkGestureDrag *gesture,
                                                             gdouble         start_x,
                                                             gdouble         start_y,
                                                             GtkTreeView    *tree_view);
static void gtk_tree_view_drag_gesture_update               (GtkGestureDrag *gesture,
                                                             gdouble         offset_x,
                                                             gdouble         offset_y,
                                                             GtkTreeView    *tree_view);
static void gtk_tree_view_drag_gesture_end                  (GtkGestureDrag *gesture,
                                                             gdouble         offset_x,
                                                             gdouble         offset_y,
                                                             GtkTreeView    *tree_view);
static void gtk_tree_view_motion_controller_enter           (GtkEventControllerMotion *controller,
                                                             double                    x,
                                                             double                    y,
                                                             GdkCrossingMode           mode,
                                                             GdkNotifyType             detail,
                                                             GtkTreeView              *tree_view);
static void gtk_tree_view_motion_controller_leave           (GtkEventControllerMotion *controller,
                                                             GdkCrossingMode           mode,
                                                             GdkNotifyType             detail,
                                                             GtkTreeView              *tree_view);
static void gtk_tree_view_motion_controller_motion          (GtkEventControllerMotion *controller,
                                                             double                    x,
                                                             double                    y,
                                                             GtkTreeView              *tree_view);

static guint tree_view_signals [LAST_SIGNAL] = { 0 };
static GParamSpec *tree_view_props [LAST_PROP] = { NULL };



/* GType Methods
 */

G_DEFINE_TYPE_WITH_CODE (GtkTreeView, gtk_tree_view, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkTreeView)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_tree_view_buildable_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE,
                                                gtk_tree_view_scrollable_init))

static void
gtk_tree_view_class_init (GtkTreeViewClass *class)
{
  GObjectClass *o_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkBindingSet *binding_set;

  binding_set = gtk_binding_set_by_class (class);

  o_class = (GObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  /* GObject signals */
  o_class->set_property = gtk_tree_view_set_property;
  o_class->get_property = gtk_tree_view_get_property;
  o_class->finalize = gtk_tree_view_finalize;

  /* GtkWidget signals */
  widget_class->destroy = gtk_tree_view_destroy;
  widget_class->map = gtk_tree_view_map;
  widget_class->realize = gtk_tree_view_realize;
  widget_class->unrealize = gtk_tree_view_unrealize;
  widget_class->measure = gtk_tree_view_measure;
  widget_class->size_allocate = gtk_tree_view_size_allocate;
  widget_class->snapshot = gtk_tree_view_snapshot;
  widget_class->drag_begin = gtk_tree_view_drag_begin;
  widget_class->drag_end = gtk_tree_view_drag_end;
  widget_class->drag_data_get = gtk_tree_view_drag_data_get;
  widget_class->drag_data_delete = gtk_tree_view_drag_data_delete;
  widget_class->drag_leave = gtk_tree_view_drag_leave;
  widget_class->drag_motion = gtk_tree_view_drag_motion;
  widget_class->drag_drop = gtk_tree_view_drag_drop;
  widget_class->drag_data_received = gtk_tree_view_drag_data_received;
  widget_class->focus = gtk_tree_view_focus;
  widget_class->grab_focus = gtk_tree_view_grab_focus;
  widget_class->style_updated = gtk_tree_view_style_updated;

  /* GtkContainer signals */
  container_class->remove = gtk_tree_view_remove;
  container_class->forall = gtk_tree_view_forall;

  class->move_cursor = gtk_tree_view_real_move_cursor;
  class->select_all = gtk_tree_view_real_select_all;
  class->unselect_all = gtk_tree_view_real_unselect_all;
  class->select_cursor_row = gtk_tree_view_real_select_cursor_row;
  class->toggle_cursor_row = gtk_tree_view_real_toggle_cursor_row;
  class->expand_collapse_cursor_row = gtk_tree_view_real_expand_collapse_cursor_row;
  class->select_cursor_parent = gtk_tree_view_real_select_cursor_parent;
  class->start_interactive_search = gtk_tree_view_start_interactive_search;

  /* Properties */

  g_object_class_override_property (o_class, PROP_HADJUSTMENT,    "hadjustment");
  g_object_class_override_property (o_class, PROP_VADJUSTMENT,    "vadjustment");
  g_object_class_override_property (o_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (o_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  tree_view_props[PROP_MODEL] =
      g_param_spec_object ("model",
                           P_("TreeView Model"),
                           P_("The model for the tree view"),
                           GTK_TYPE_TREE_MODEL,
                           GTK_PARAM_READWRITE);

  tree_view_props[PROP_HEADERS_VISIBLE] =
      g_param_spec_boolean ("headers-visible",
                            P_("Headers Visible"),
                            P_("Show the column header buttons"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  tree_view_props[PROP_HEADERS_CLICKABLE] =
      g_param_spec_boolean ("headers-clickable",
                            P_("Headers Clickable"),
                            P_("Column headers respond to click events"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  tree_view_props[PROP_EXPANDER_COLUMN] =
      g_param_spec_object ("expander-column",
                           P_("Expander Column"),
                           P_("Set the column for the expander column"),
                           GTK_TYPE_TREE_VIEW_COLUMN,
                           GTK_PARAM_READWRITE);

  tree_view_props[PROP_REORDERABLE] =
      g_param_spec_boolean ("reorderable",
                            P_("Reorderable"),
                            P_("View is reorderable"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  tree_view_props[PROP_ENABLE_SEARCH] =
      g_param_spec_boolean ("enable-search",
                            P_("Enable Search"),
                            P_("View allows user to search through columns interactively"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  tree_view_props[PROP_SEARCH_COLUMN] =
      g_param_spec_int ("search-column",
                        P_("Search Column"),
                        P_("Model column to search through during interactive search"),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkTreeView:fixed-height-mode:
   *
   * Setting the ::fixed-height-mode property to %TRUE speeds up 
   * #GtkTreeView by assuming that all rows have the same height. 
   * Only enable this option if all rows are the same height.  
   * Please see gtk_tree_view_set_fixed_height_mode() for more 
   * information on this option.
   */
  tree_view_props[PROP_FIXED_HEIGHT_MODE] =
      g_param_spec_boolean ("fixed-height-mode",
                            P_("Fixed Height Mode"),
                            P_("Speeds up GtkTreeView by assuming that all rows have the same height"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkTreeView:hover-selection:
   * 
   * Enables or disables the hover selection mode of @tree_view.
   * Hover selection makes the selected row follow the pointer.
   * Currently, this works only for the selection modes 
   * %GTK_SELECTION_SINGLE and %GTK_SELECTION_BROWSE.
   *
   * This mode is primarily intended for treeviews in popups, e.g.
   * in #GtkComboBox or #GtkEntryCompletion.
   */
  tree_view_props[PROP_HOVER_SELECTION] =
      g_param_spec_boolean ("hover-selection",
                            P_("Hover Selection"),
                            P_("Whether the selection should follow the pointer"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkTreeView:hover-expand:
   * 
   * Enables or disables the hover expansion mode of @tree_view.
   * Hover expansion makes rows expand or collapse if the pointer moves 
   * over them.
   *
   * This mode is primarily intended for treeviews in popups, e.g.
   * in #GtkComboBox or #GtkEntryCompletion.
   */
  tree_view_props[PROP_HOVER_EXPAND] =
      g_param_spec_boolean ("hover-expand",
                            P_("Hover Expand"),
                            P_("Whether rows should be expanded/collapsed when the pointer moves over them"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkTreeView:show-expanders:
   *
   * %TRUE if the view has expanders.
   */
  tree_view_props[PROP_SHOW_EXPANDERS] =
      g_param_spec_boolean ("show-expanders",
                            P_("Show Expanders"),
                            P_("View has expanders"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkTreeView:level-indentation:
   *
   * Extra indentation for each level.
   */
  tree_view_props[PROP_LEVEL_INDENTATION] =
      g_param_spec_int ("level-indentation",
                        P_("Level Indentation"),
                        P_("Extra indentation for each level"),
                        0, G_MAXINT,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  tree_view_props[PROP_RUBBER_BANDING] =
      g_param_spec_boolean ("rubber-banding",
                            P_("Rubber Banding"),
                            P_("Whether to enable selection of multiple items by dragging the mouse pointer"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  tree_view_props[PROP_ENABLE_GRID_LINES] =
      g_param_spec_enum ("enable-grid-lines",
                         P_("Enable Grid Lines"),
                         P_("Whether grid lines should be drawn in the tree view"),
                         GTK_TYPE_TREE_VIEW_GRID_LINES,
                         GTK_TREE_VIEW_GRID_LINES_NONE,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  tree_view_props[PROP_ENABLE_TREE_LINES] =
      g_param_spec_boolean ("enable-tree-lines",
                            P_("Enable Tree Lines"),
                            P_("Whether tree lines should be drawn in the tree view"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  tree_view_props[PROP_TOOLTIP_COLUMN] =
      g_param_spec_int ("tooltip-column",
                        P_("Tooltip Column"),
                        P_("The column in the model containing the tooltip texts for the rows"),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkTreeView:activate-on-single-click:
   *
   * The activate-on-single-click property specifies whether the "row-activated" signal
   * will be emitted after a single click.
   */
  tree_view_props[PROP_ACTIVATE_ON_SINGLE_CLICK] =
      g_param_spec_boolean ("activate-on-single-click",
                            P_("Activate on Single Click"),
                            P_("Activate row on a single click"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (o_class, LAST_PROP, tree_view_props);

  /* Signals */
  /**
   * GtkTreeView::row-activated:
   * @tree_view: the object on which the signal is emitted
   * @path: the #GtkTreePath for the activated row
   * @column: the #GtkTreeViewColumn in which the activation occurred
   *
   * The "row-activated" signal is emitted when the method
   * gtk_tree_view_row_activated() is called, when the user double
   * clicks a treeview row with the "activate-on-single-click"
   * property set to %FALSE, or when the user single clicks a row when
   * the "activate-on-single-click" property set to %TRUE. It is also
   * emitted when a non-editable row is selected and one of the keys:
   * Space, Shift+Space, Return or Enter is pressed.
   *
   * For selection handling refer to the
   * [tree widget conceptual overview][TreeWidget]
   * as well as #GtkTreeSelection.
   */
  tree_view_signals[ROW_ACTIVATED] =
    g_signal_new (I_("row-activated"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, row_activated),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED_OBJECT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_TREE_PATH,
		  GTK_TYPE_TREE_VIEW_COLUMN);
  g_signal_set_va_marshaller (tree_view_signals[ROW_ACTIVATED],
                              G_TYPE_FROM_CLASS (o_class),
                              _gtk_marshal_VOID__BOXED_OBJECTv);

  /**
   * GtkTreeView::test-expand-row:
   * @tree_view: the object on which the signal is emitted
   * @iter: the tree iter of the row to expand
   * @path: a tree path that points to the row 
   * 
   * The given row is about to be expanded (show its children nodes). Use this
   * signal if you need to control the expandability of individual rows.
   *
   * Returns: %FALSE to allow expansion, %TRUE to reject
   */
  tree_view_signals[TEST_EXPAND_ROW] =
    g_signal_new (I_("test-expand-row"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTreeViewClass, test_expand_row),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED_BOXED,
		  G_TYPE_BOOLEAN, 2,
		  GTK_TYPE_TREE_ITER,
		  GTK_TYPE_TREE_PATH);
  g_signal_set_va_marshaller (tree_view_signals[TEST_EXPAND_ROW],
                              G_TYPE_FROM_CLASS (o_class),
                              _gtk_marshal_BOOLEAN__BOXED_BOXEDv);

  /**
   * GtkTreeView::test-collapse-row:
   * @tree_view: the object on which the signal is emitted
   * @iter: the tree iter of the row to collapse
   * @path: a tree path that points to the row 
   * 
   * The given row is about to be collapsed (hide its children nodes). Use this
   * signal if you need to control the collapsibility of individual rows.
   *
   * Returns: %FALSE to allow collapsing, %TRUE to reject
   */
  tree_view_signals[TEST_COLLAPSE_ROW] =
    g_signal_new (I_("test-collapse-row"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTreeViewClass, test_collapse_row),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED_BOXED,
		  G_TYPE_BOOLEAN, 2,
		  GTK_TYPE_TREE_ITER,
		  GTK_TYPE_TREE_PATH);
  g_signal_set_va_marshaller (tree_view_signals[TEST_COLLAPSE_ROW],
                              G_TYPE_FROM_CLASS (o_class),
                              _gtk_marshal_BOOLEAN__BOXED_BOXEDv);

  /**
   * GtkTreeView::row-expanded:
   * @tree_view: the object on which the signal is emitted
   * @iter: the tree iter of the expanded row
   * @path: a tree path that points to the row 
   * 
   * The given row has been expanded (child nodes are shown).
   */
  tree_view_signals[ROW_EXPANDED] =
    g_signal_new (I_("row-expanded"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTreeViewClass, row_expanded),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED_BOXED,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_TREE_ITER,
		  GTK_TYPE_TREE_PATH);
  g_signal_set_va_marshaller (tree_view_signals[ROW_EXPANDED],
                              G_TYPE_FROM_CLASS (o_class),
                              _gtk_marshal_VOID__BOXED_BOXEDv);

  /**
   * GtkTreeView::row-collapsed:
   * @tree_view: the object on which the signal is emitted
   * @iter: the tree iter of the collapsed row
   * @path: a tree path that points to the row 
   * 
   * The given row has been collapsed (child nodes are hidden).
   */
  tree_view_signals[ROW_COLLAPSED] =
    g_signal_new (I_("row-collapsed"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTreeViewClass, row_collapsed),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED_BOXED,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_TREE_ITER,
		  GTK_TYPE_TREE_PATH);
  g_signal_set_va_marshaller (tree_view_signals[ROW_COLLAPSED],
                              G_TYPE_FROM_CLASS (o_class),
                              _gtk_marshal_VOID__BOXED_BOXEDv);

  /**
   * GtkTreeView::columns-changed:
   * @tree_view: the object on which the signal is emitted 
   * 
   * The number of columns of the treeview has changed.
   */
  tree_view_signals[COLUMNS_CHANGED] =
    g_signal_new (I_("columns-changed"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTreeViewClass, columns_changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkTreeView::cursor-changed:
   * @tree_view: the object on which the signal is emitted
   * 
   * The position of the cursor (focused cell) has changed.
   */
  tree_view_signals[CURSOR_CHANGED] =
    g_signal_new (I_("cursor-changed"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTreeViewClass, cursor_changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkTreeView::move-cursor:
   * @tree_view: the object on which the signal is emitted.
   * @step: the granularity of the move, as a
   * #GtkMovementStep. %GTK_MOVEMENT_LOGICAL_POSITIONS,
   * %GTK_MOVEMENT_VISUAL_POSITIONS, %GTK_MOVEMENT_DISPLAY_LINES,
   * %GTK_MOVEMENT_PAGES and %GTK_MOVEMENT_BUFFER_ENDS are
   * supported. %GTK_MOVEMENT_LOGICAL_POSITIONS and
   * %GTK_MOVEMENT_VISUAL_POSITIONS are treated identically.
   * @direction: the direction to move: +1 to move forwards;
   * -1 to move backwards. The resulting movement is
   * undefined for all other values.
   *
   * The #GtkTreeView::move-cursor signal is a [keybinding
   * signal][GtkBindingSignal] which gets emitted when the user
   * presses one of the cursor keys.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control the cursor
   * programmatically. In contrast to gtk_tree_view_set_cursor() and
   * gtk_tree_view_set_cursor_on_cell() when moving horizontally
   * #GtkTreeView::move-cursor does not reset the current selection.
   *
   * Returns: %TRUE if @step is supported, %FALSE otherwise.
   */
  tree_view_signals[MOVE_CURSOR] =
    g_signal_new (I_("move-cursor"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, move_cursor),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__ENUM_INT,
		  G_TYPE_BOOLEAN, 2,
		  GTK_TYPE_MOVEMENT_STEP,
		  G_TYPE_INT);
  g_signal_set_va_marshaller (tree_view_signals[MOVE_CURSOR],
                              G_TYPE_FROM_CLASS (o_class),
                              _gtk_marshal_BOOLEAN__ENUM_INTv);

  tree_view_signals[SELECT_ALL] =
    g_signal_new (I_("select-all"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, select_all),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);
  g_signal_set_va_marshaller (tree_view_signals[SELECT_ALL],
                              G_TYPE_FROM_CLASS (o_class),
                              _gtk_marshal_BOOLEAN__VOIDv);

  tree_view_signals[UNSELECT_ALL] =
    g_signal_new (I_("unselect-all"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, unselect_all),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);
  g_signal_set_va_marshaller (tree_view_signals[UNSELECT_ALL],
                              G_TYPE_FROM_CLASS (o_class),
                              _gtk_marshal_BOOLEAN__VOIDv);

  tree_view_signals[SELECT_CURSOR_ROW] =
    g_signal_new (I_("select-cursor-row"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, select_cursor_row),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__BOOLEAN,
		  G_TYPE_BOOLEAN, 1,
		  G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (tree_view_signals[SELECT_CURSOR_ROW],
                              G_TYPE_FROM_CLASS (o_class),
                              _gtk_marshal_BOOLEAN__BOOLEANv);

  tree_view_signals[TOGGLE_CURSOR_ROW] =
    g_signal_new (I_("toggle-cursor-row"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, toggle_cursor_row),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);
  g_signal_set_va_marshaller (tree_view_signals[TOGGLE_CURSOR_ROW],
                              G_TYPE_FROM_CLASS (o_class),
                              _gtk_marshal_BOOLEAN__VOIDv);

  tree_view_signals[EXPAND_COLLAPSE_CURSOR_ROW] =
    g_signal_new (I_("expand-collapse-cursor-row"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, expand_collapse_cursor_row),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__BOOLEAN_BOOLEAN_BOOLEAN,
		  G_TYPE_BOOLEAN, 3,
		  G_TYPE_BOOLEAN,
		  G_TYPE_BOOLEAN,
		  G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (tree_view_signals[EXPAND_COLLAPSE_CURSOR_ROW],
                              G_TYPE_FROM_CLASS (o_class),
                              _gtk_marshal_BOOLEAN__BOOLEAN_BOOLEAN_BOOLEANv);

  tree_view_signals[SELECT_CURSOR_PARENT] =
    g_signal_new (I_("select-cursor-parent"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, select_cursor_parent),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);
  g_signal_set_va_marshaller (tree_view_signals[SELECT_CURSOR_PARENT],
                              G_TYPE_FROM_CLASS (o_class),
                              _gtk_marshal_BOOLEAN__VOIDv);

  tree_view_signals[START_INTERACTIVE_SEARCH] =
    g_signal_new (I_("start-interactive-search"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, start_interactive_search),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);
  g_signal_set_va_marshaller (tree_view_signals[START_INTERACTIVE_SEARCH],
                              G_TYPE_FROM_CLASS (o_class),
                              _gtk_marshal_BOOLEAN__VOIDv);

  /* Key bindings */
  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_Up, 0, TRUE,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);
  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_KP_Up, 0, TRUE,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);

  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_Down, 0, TRUE,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);
  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_KP_Down, 0, TRUE,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);

  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_p, GDK_CONTROL_MASK, FALSE,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);

  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_n, GDK_CONTROL_MASK, FALSE,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);

  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_Home, 0, TRUE,
				  GTK_MOVEMENT_BUFFER_ENDS, -1);
  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_KP_Home, 0, TRUE,
				  GTK_MOVEMENT_BUFFER_ENDS, -1);

  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_End, 0, TRUE,
				  GTK_MOVEMENT_BUFFER_ENDS, 1);
  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_KP_End, 0, TRUE,
				  GTK_MOVEMENT_BUFFER_ENDS, 1);

  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_Page_Up, 0, TRUE,
				  GTK_MOVEMENT_PAGES, -1);
  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_KP_Page_Up, 0, TRUE,
				  GTK_MOVEMENT_PAGES, -1);

  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_Page_Down, 0, TRUE,
				  GTK_MOVEMENT_PAGES, 1);
  gtk_tree_view_add_move_binding (binding_set, GDK_KEY_KP_Page_Down, 0, TRUE,
				  GTK_MOVEMENT_PAGES, 1);


  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Right, 0, "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Left, 0, "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Right, 0, "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Left, 0, "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Right, GDK_CONTROL_MASK,
                                "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Left, GDK_CONTROL_MASK,
                                "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Right, GDK_CONTROL_MASK,
                                "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Left, GDK_CONTROL_MASK,
                                "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, GDK_CONTROL_MASK, "toggle-cursor-row", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, GDK_CONTROL_MASK, "toggle-cursor-row", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK, "select-all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_slash, GDK_CONTROL_MASK, "select-all", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_A, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "unselect-all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_backslash, GDK_CONTROL_MASK, "unselect-all", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, GDK_SHIFT_MASK, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, GDK_SHIFT_MASK, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, 0, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, 0, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, 0, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_ISO_Enter, 0, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter, 0, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);

  /* expand and collapse rows */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_plus, 0, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, FALSE);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_asterisk, 0,
                                "expand-collapse-cursor-row", 3,
                                G_TYPE_BOOLEAN, TRUE,
                                G_TYPE_BOOLEAN, TRUE,
                                G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Multiply, 0,
                                "expand-collapse-cursor-row", 3,
                                G_TYPE_BOOLEAN, TRUE,
                                G_TYPE_BOOLEAN, TRUE,
                                G_TYPE_BOOLEAN, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_slash, 0,
                                "expand-collapse-cursor-row", 3,
                                G_TYPE_BOOLEAN, TRUE,
                                G_TYPE_BOOLEAN, FALSE,
                                G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Divide, 0,
                                "expand-collapse-cursor-row", 3,
                                G_TYPE_BOOLEAN, TRUE,
                                G_TYPE_BOOLEAN, FALSE,
                                G_TYPE_BOOLEAN, FALSE);

  /* Not doable on US keyboards */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_plus, GDK_SHIFT_MASK, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Add, 0, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Add, GDK_SHIFT_MASK, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Add, GDK_SHIFT_MASK, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Right, GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Right, GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Right,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Right,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_minus, 0, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_minus, GDK_SHIFT_MASK, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Subtract, 0, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Subtract, GDK_SHIFT_MASK, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Left, GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Left, GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Left,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Left,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_BackSpace, 0, "select-cursor-parent", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_BackSpace, GDK_CONTROL_MASK, "select-cursor-parent", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_f, GDK_CONTROL_MASK, "start-interactive-search", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_F, GDK_CONTROL_MASK, "start-interactive-search", 0);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_TREE_VIEW_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("treeview"));
}

static void
gtk_tree_view_init (GtkTreeView *tree_view)
{
  GtkTreeViewPrivate *priv;
  GtkCssNode *widget_node;
  GtkGesture *gesture;
  GtkEventController *controller;

  priv = tree_view->priv = gtk_tree_view_get_instance_private (tree_view);

  gtk_widget_set_can_focus (GTK_WIDGET (tree_view), TRUE);
  gtk_widget_set_overflow (GTK_WIDGET (tree_view), GTK_OVERFLOW_HIDDEN);

  priv->show_expanders = TRUE;
  priv->draw_keyfocus = TRUE;
  priv->headers_visible = TRUE;
  priv->activate_on_single_click = FALSE;

  /* We need some padding */
  priv->dy = 0;
  priv->cursor_offset = 0;
  priv->n_columns = 0;
  priv->header_height = 1;
  priv->x_drag = 0;
  priv->drag_pos = -1;
  priv->header_has_focus = FALSE;
  priv->press_start_x = -1;
  priv->press_start_y = -1;
  priv->reorderable = FALSE;
  priv->presize_handler_tick_cb = 0;
  priv->scroll_sync_timer = 0;
  priv->fixed_height = -1;
  priv->fixed_height_mode = FALSE;
  priv->fixed_height_check = 0;
  priv->selection = _gtk_tree_selection_new_with_tree_view (tree_view);
  priv->enable_search = TRUE;
  priv->search_column = -1;
  priv->search_position_func = gtk_tree_view_search_position_func;
  priv->search_equal_func = gtk_tree_view_search_equal_func;
  priv->search_custom_entry_set = FALSE;
  priv->typeselect_flush_timeout = 0;
  priv->width = 0;
  priv->expander_size = -1;

  priv->hover_selection = FALSE;
  priv->hover_expand = FALSE;

  priv->level_indentation = 0;

  priv->rubber_banding_enable = FALSE;

  priv->grid_lines = GTK_TREE_VIEW_GRID_LINES_NONE;
  priv->tree_lines_enabled = FALSE;

  priv->tooltip_column = -1;

  priv->event_last_x = -10000;
  priv->event_last_y = -10000;

  gtk_tree_view_do_set_vadjustment (tree_view, NULL);
  gtk_tree_view_do_set_hadjustment (tree_view, NULL);

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (tree_view)),
                               GTK_STYLE_CLASS_VIEW);

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (tree_view));
  priv->header_node = gtk_css_node_new ();
  gtk_css_node_set_name (priv->header_node, I_("header"));
  gtk_css_node_set_parent (priv->header_node, widget_node);
  gtk_css_node_set_state (priv->header_node, gtk_css_node_get_state (widget_node));
  g_object_unref (priv->header_node);

  priv->click_gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->click_gesture), 0);
  g_signal_connect (priv->click_gesture, "pressed",
                    G_CALLBACK (gtk_tree_view_click_gesture_pressed), tree_view);
  g_signal_connect (priv->click_gesture, "released",
                    G_CALLBACK (gtk_tree_view_click_gesture_released), tree_view);
  gtk_widget_add_controller (GTK_WIDGET (tree_view), GTK_EVENT_CONTROLLER (priv->click_gesture));

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_tree_view_column_click_gesture_pressed), tree_view);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (tree_view), GTK_EVENT_CONTROLLER (gesture));

  priv->drag_gesture = gtk_gesture_drag_new ();
  g_signal_connect (priv->drag_gesture, "drag-begin",
                    G_CALLBACK (gtk_tree_view_drag_gesture_begin), tree_view);
  g_signal_connect (priv->drag_gesture, "drag-update",
                    G_CALLBACK (gtk_tree_view_drag_gesture_update), tree_view);
  g_signal_connect (priv->drag_gesture, "drag-end",
                    G_CALLBACK (gtk_tree_view_drag_gesture_end), tree_view);
  gtk_widget_add_controller (GTK_WIDGET (tree_view), GTK_EVENT_CONTROLLER (priv->drag_gesture));

  priv->column_drag_gesture = gtk_gesture_drag_new ();
  g_signal_connect (priv->column_drag_gesture, "drag-begin",
                    G_CALLBACK (gtk_tree_view_column_drag_gesture_begin), tree_view);
  g_signal_connect (priv->column_drag_gesture, "drag-update",
                    G_CALLBACK (gtk_tree_view_column_drag_gesture_update), tree_view);
  g_signal_connect (priv->column_drag_gesture, "drag-end",
                    G_CALLBACK (gtk_tree_view_column_drag_gesture_end), tree_view);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->column_drag_gesture),
                                              GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (tree_view), GTK_EVENT_CONTROLLER (priv->column_drag_gesture));

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "enter",
                    G_CALLBACK (gtk_tree_view_motion_controller_enter), tree_view);
  g_signal_connect (controller, "leave",
                    G_CALLBACK (gtk_tree_view_motion_controller_leave), tree_view);
  g_signal_connect (controller, "motion",
                    G_CALLBACK (gtk_tree_view_motion_controller_motion), tree_view);
  gtk_widget_add_controller (GTK_WIDGET (tree_view), controller);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (gtk_tree_view_key_controller_key_pressed), tree_view);
  g_signal_connect (controller, "key-released",
                    G_CALLBACK (gtk_tree_view_key_controller_key_released), tree_view);
  g_signal_connect (controller, "focus-out",
                    G_CALLBACK (gtk_tree_view_key_controller_focus_out), tree_view);
  gtk_widget_add_controller (GTK_WIDGET (tree_view), controller);
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
      gtk_tree_view_do_set_hadjustment (tree_view, g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      gtk_tree_view_do_set_vadjustment (tree_view, g_value_get_object (value));
      break;
    case PROP_HSCROLL_POLICY:
      if (tree_view->priv->hscroll_policy != g_value_get_enum (value))
        {
          tree_view->priv->hscroll_policy = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (tree_view));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_VSCROLL_POLICY:
      if (tree_view->priv->vscroll_policy != g_value_get_enum (value))
        {
          tree_view->priv->vscroll_policy = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (tree_view));
          g_object_notify_by_pspec (object, pspec);
        }
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
    case PROP_ENABLE_SEARCH:
      gtk_tree_view_set_enable_search (tree_view, g_value_get_boolean (value));
      break;
    case PROP_SEARCH_COLUMN:
      gtk_tree_view_set_search_column (tree_view, g_value_get_int (value));
      break;
    case PROP_FIXED_HEIGHT_MODE:
      gtk_tree_view_set_fixed_height_mode (tree_view, g_value_get_boolean (value));
      break;
    case PROP_HOVER_SELECTION:
      if (tree_view->priv->hover_selection != g_value_get_boolean (value))
        {
          tree_view->priv->hover_selection = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_HOVER_EXPAND:
      if (tree_view->priv->hover_expand != g_value_get_boolean (value))
        {
          tree_view->priv->hover_expand = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_SHOW_EXPANDERS:
      gtk_tree_view_set_show_expanders (tree_view, g_value_get_boolean (value));
      break;
    case PROP_LEVEL_INDENTATION:
      if (tree_view->priv->level_indentation != g_value_get_int (value))
        {
          tree_view->priv->level_indentation = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_RUBBER_BANDING:
      if (tree_view->priv->rubber_banding_enable != g_value_get_boolean (value))
        {
          tree_view->priv->rubber_banding_enable = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_ENABLE_GRID_LINES:
      gtk_tree_view_set_grid_lines (tree_view, g_value_get_enum (value));
      break;
    case PROP_ENABLE_TREE_LINES:
      gtk_tree_view_set_enable_tree_lines (tree_view, g_value_get_boolean (value));
      break;
    case PROP_TOOLTIP_COLUMN:
      gtk_tree_view_set_tooltip_column (tree_view, g_value_get_int (value));
      break;
    case PROP_ACTIVATE_ON_SINGLE_CLICK:
      gtk_tree_view_set_activate_on_single_click (tree_view, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
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
    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, tree_view->priv->hscroll_policy);
      break;
    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, tree_view->priv->vscroll_policy);
      break;
    case PROP_HEADERS_VISIBLE:
      g_value_set_boolean (value, gtk_tree_view_get_headers_visible (tree_view));
      break;
    case PROP_HEADERS_CLICKABLE:
      g_value_set_boolean (value, gtk_tree_view_get_headers_clickable (tree_view));
      break;
    case PROP_EXPANDER_COLUMN:
      g_value_set_object (value, tree_view->priv->expander_column);
      break;
    case PROP_REORDERABLE:
      g_value_set_boolean (value, tree_view->priv->reorderable);
      break;
    case PROP_ENABLE_SEARCH:
      g_value_set_boolean (value, tree_view->priv->enable_search);
      break;
    case PROP_SEARCH_COLUMN:
      g_value_set_int (value, tree_view->priv->search_column);
      break;
    case PROP_FIXED_HEIGHT_MODE:
      g_value_set_boolean (value, tree_view->priv->fixed_height_mode);
      break;
    case PROP_HOVER_SELECTION:
      g_value_set_boolean (value, tree_view->priv->hover_selection);
      break;
    case PROP_HOVER_EXPAND:
      g_value_set_boolean (value, tree_view->priv->hover_expand);
      break;
    case PROP_SHOW_EXPANDERS:
      g_value_set_boolean (value, tree_view->priv->show_expanders);
      break;
    case PROP_LEVEL_INDENTATION:
      g_value_set_int (value, tree_view->priv->level_indentation);
      break;
    case PROP_RUBBER_BANDING:
      g_value_set_boolean (value, tree_view->priv->rubber_banding_enable);
      break;
    case PROP_ENABLE_GRID_LINES:
      g_value_set_enum (value, tree_view->priv->grid_lines);
      break;
    case PROP_ENABLE_TREE_LINES:
      g_value_set_boolean (value, tree_view->priv->tree_lines_enabled);
      break;
    case PROP_TOOLTIP_COLUMN:
      g_value_set_int (value, tree_view->priv->tooltip_column);
      break;
    case PROP_ACTIVATE_ON_SINGLE_CLICK:
      g_value_set_boolean (value, tree_view->priv->activate_on_single_click);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_view_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_tree_view_parent_class)->finalize (object);
}


static GtkBuildableIface *parent_buildable_iface;

static void
gtk_tree_view_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_tree_view_buildable_add_child;
  iface->get_internal_child = gtk_tree_view_buildable_get_internal_child;
}

static void
gtk_tree_view_buildable_add_child (GtkBuildable *tree_view,
				   GtkBuilder  *builder,
				   GObject     *child,
				   const gchar *type)
{
  if (GTK_IS_TREE_VIEW_COLUMN (child))
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (child));
  else
    parent_buildable_iface->add_child (tree_view, builder, child, type);
}

static GObject *
gtk_tree_view_buildable_get_internal_child (GtkBuildable      *buildable,
					    GtkBuilder        *builder,
					    const gchar       *childname)
{
    if (strcmp (childname, "selection") == 0)
      return G_OBJECT (GTK_TREE_VIEW (buildable)->priv->selection);
    
    return parent_buildable_iface->get_internal_child (buildable,
						       builder,
						       childname);
}

/* GtkWidget Methods
 */

static void
gtk_tree_view_free_rbtree (GtkTreeView *tree_view)
{
  gtk_tree_rbtree_free (tree_view->priv->tree);

  tree_view->priv->tree = NULL;
  tree_view->priv->button_pressed_node = NULL;
  tree_view->priv->button_pressed_tree = NULL;
  tree_view->priv->prelight_tree = NULL;
  tree_view->priv->prelight_node = NULL;
}

static void
gtk_tree_view_destroy_search_window (GtkTreeView *tree_view)
{
  gtk_widget_destroy (tree_view->priv->search_window);

  tree_view->priv->search_window = NULL;
  tree_view->priv->search_entry = NULL;
  tree_view->priv->search_entry_changed_id = 0;
}

static void
gtk_tree_view_destroy (GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkTreeViewPrivate *priv = gtk_tree_view_get_instance_private (tree_view);
  GList *list;

  gtk_tree_view_stop_editing (tree_view, TRUE);
  gtk_tree_view_stop_rubber_band (tree_view);

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

      gtk_tree_view_free_rbtree (tree_view);
    }

  if (tree_view->priv->selection != NULL)
    {
      _gtk_tree_selection_set_tree_view (tree_view->priv->selection, NULL);
      g_object_unref (tree_view->priv->selection);
      tree_view->priv->selection = NULL;
    }

  g_clear_pointer (&priv->scroll_to_path, gtk_tree_row_reference_free);
  g_clear_pointer (&priv->drag_dest_row, gtk_tree_row_reference_free);
  g_clear_pointer (&priv->top_row, gtk_tree_row_reference_free);

  if (tree_view->priv->column_drop_func_data &&
      tree_view->priv->column_drop_func_data_destroy)
    {
      tree_view->priv->column_drop_func_data_destroy (tree_view->priv->column_drop_func_data);
      tree_view->priv->column_drop_func_data = NULL;
    }

  gtk_tree_row_reference_free (tree_view->priv->anchor);
  tree_view->priv->anchor = NULL;

  /* destroy interactive search dialog */
  if (tree_view->priv->search_window)
    {
      gtk_tree_view_destroy_search_window (tree_view);
      if (tree_view->priv->typeselect_flush_timeout)
	{
	  g_source_remove (tree_view->priv->typeselect_flush_timeout);
	  tree_view->priv->typeselect_flush_timeout = 0;
	}
    }

  if (tree_view->priv->search_custom_entry_set)
    {
      GtkEventController *controller;

      g_signal_handlers_disconnect_by_func (tree_view->priv->search_entry,
                                            G_CALLBACK (gtk_tree_view_search_init),
                                            tree_view);

      if (GTK_IS_ENTRY (tree_view->priv->search_entry))
        controller = gtk_entry_get_key_controller (GTK_ENTRY (tree_view->priv->search_entry));
      else
        controller = gtk_search_entry_get_key_controller (GTK_SEARCH_ENTRY (tree_view->priv->search_entry));
      g_signal_handlers_disconnect_by_func (controller,
                                            G_CALLBACK (gtk_tree_view_search_key_pressed),
                                            tree_view);

      g_object_unref (tree_view->priv->search_entry);

      tree_view->priv->search_entry = NULL;
      tree_view->priv->search_custom_entry_set = FALSE;
    }

  if (tree_view->priv->search_destroy && tree_view->priv->search_user_data)
    {
      tree_view->priv->search_destroy (tree_view->priv->search_user_data);
      tree_view->priv->search_user_data = NULL;
    }

  if (tree_view->priv->search_position_destroy && tree_view->priv->search_position_user_data)
    {
      tree_view->priv->search_position_destroy (tree_view->priv->search_position_user_data);
      tree_view->priv->search_position_user_data = NULL;
    }

  if (tree_view->priv->row_separator_destroy && tree_view->priv->row_separator_data)
    {
      tree_view->priv->row_separator_destroy (tree_view->priv->row_separator_data);
      tree_view->priv->row_separator_data = NULL;
    }

  gtk_tree_view_set_model (tree_view, NULL);

  g_clear_object (&priv->hadjustment);
  g_clear_object (&priv->vadjustment);
  g_clear_object (&priv->horizontal_grid_line_texture);
  g_clear_object (&priv->vertical_grid_line_texture);
  g_clear_object (&priv->horizontal_tree_line_texture);
  g_clear_object (&priv->vertical_tree_line_texture);

  GTK_WIDGET_CLASS (gtk_tree_view_parent_class)->destroy (widget);
}

/* GtkWidget::map helper */
static void
gtk_tree_view_map_buttons (GtkTreeView *tree_view)
{
  GList *list;

  g_return_if_fail (gtk_widget_get_mapped (GTK_WIDGET (tree_view)));

  if (tree_view->priv->headers_visible)
    {
      GtkTreeViewColumn *column;
      GtkWidget         *button;

      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  column = list->data;
	  button = gtk_tree_view_column_get_button (column);

          if (gtk_tree_view_column_get_visible (column) && button)
            gtk_widget_show (button);

          if (gtk_widget_get_visible (button) &&
              !gtk_widget_get_mapped (button))
            gtk_widget_map (button);
	}
    }
}

static void
gtk_tree_view_map (GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GList *tmp_list;

  GTK_WIDGET_CLASS (gtk_tree_view_parent_class)->map (widget);

  tmp_list = tree_view->priv->children;
  while (tmp_list)
    {
      GtkTreeViewChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      if (gtk_widget_get_visible (child->widget))
	{
	  if (!gtk_widget_get_mapped (child->widget))
	    gtk_widget_map (child->widget);
	}
    }

  gtk_tree_view_map_buttons (tree_view);
}

static void
gtk_tree_view_realize (GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GList *tmp_list;

  GTK_WIDGET_CLASS (gtk_tree_view_parent_class)->realize (widget);

  for (tmp_list = tree_view->priv->columns; tmp_list; tmp_list = tmp_list->next)
    _gtk_tree_view_column_realize_button (GTK_TREE_VIEW_COLUMN (tmp_list->data));

  /* Need to call those here, since they create GCs */
  gtk_tree_view_set_grid_lines (tree_view, tree_view->priv->grid_lines);
  gtk_tree_view_set_enable_tree_lines (tree_view, tree_view->priv->tree_lines_enabled);

  install_presize_handler (tree_view); 
}

static void
gtk_tree_view_unrealize (GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkTreeViewPrivate *priv = tree_view->priv;

  if (priv->scroll_timeout != 0)
    {
      g_source_remove (priv->scroll_timeout);
      priv->scroll_timeout = 0;
    }

  if (priv->auto_expand_timeout != 0)
    {
      g_source_remove (priv->auto_expand_timeout);
      priv->auto_expand_timeout = 0;
    }

  if (priv->open_dest_timeout != 0)
    {
      g_source_remove (priv->open_dest_timeout);
      priv->open_dest_timeout = 0;
    }

  if (priv->presize_handler_tick_cb != 0)
    {
      gtk_widget_remove_tick_callback (widget, priv->presize_handler_tick_cb);
      priv->presize_handler_tick_cb = 0;
    }

  if (priv->validate_rows_timer != 0)
    {
      g_source_remove (priv->validate_rows_timer);
      priv->validate_rows_timer = 0;
    }

  if (priv->scroll_sync_timer != 0)
    {
      g_source_remove (priv->scroll_sync_timer);
      priv->scroll_sync_timer = 0;
    }

  if (priv->typeselect_flush_timeout)
    {
      g_source_remove (priv->typeselect_flush_timeout);
      priv->typeselect_flush_timeout = 0;
    }

  GTK_WIDGET_CLASS (gtk_tree_view_parent_class)->unrealize (widget);
}

/* GtkWidget::get_preferred_height helper */
static void
gtk_tree_view_update_height (GtkTreeView *tree_view)
{
  GList *list;

  tree_view->priv->header_height = 0;

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      GtkRequisition     requisition;
      GtkTreeViewColumn *column = list->data;
      GtkWidget         *button = gtk_tree_view_column_get_button (column);

      if (button == NULL)
        continue;

      gtk_widget_get_preferred_size (button, &requisition, NULL);
      tree_view->priv->header_height = MAX (tree_view->priv->header_height, requisition.height);
    }
}

static gint
gtk_tree_view_get_height (GtkTreeView *tree_view)
{
  if (tree_view->priv->tree == NULL)
    return 0;
  else
    return tree_view->priv->tree->root->offset;
}

static void
gtk_tree_view_measure (GtkWidget        *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      GList *list;
      GtkTreeViewColumn *column;
      gint width = 0;

      /* we validate some rows initially just to make sure we have some size.
       * In practice, with a lot of static lists, this should get a good width.
       */
      do_validate_rows (tree_view, FALSE);

      /* keep this in sync with size_allocate below */
      for (list = tree_view->priv->columns; list; list = list->next)
        {
          column = list->data;
          if (!gtk_tree_view_column_get_visible (column) || column == tree_view->priv->drag_column)
            continue;

          width += _gtk_tree_view_column_request_width (column);
        }

      *minimum = *natural = width;
    }
  else /* VERTICAL */
    {
      int height;

      gtk_tree_view_update_height (tree_view);
      height = gtk_tree_view_get_height (tree_view) + gtk_tree_view_get_effective_header_height (tree_view);

      *minimum = *natural = height;
    }
}

static int
gtk_tree_view_calculate_width_before_expander (GtkTreeView *tree_view)
{
  int width = 0;
  GList *list;
  gboolean rtl;

  rtl = (_gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);
  for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
       list->data != tree_view->priv->expander_column;
       list = (rtl ? list->prev : list->next))
    {
      GtkTreeViewColumn *column = list->data;

      width += gtk_tree_view_column_get_width (column);
    }

  return width;
}

/* GtkWidget::size_allocate helper */
static void
gtk_tree_view_size_allocate_columns (GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkTreeViewPrivate *priv = gtk_tree_view_get_instance_private (tree_view);
  const int x_offset = - gtk_adjustment_get_value (priv->hadjustment);
  GList *list, *first_column, *last_column;
  GtkTreeViewColumn *column;
  gint widget_width, width = 0;
  gint extra, extra_per_column;
  gint full_requested_width = 0;
  gint number_of_expand_columns = 0;
  gboolean rtl;

  for (last_column = g_list_last (tree_view->priv->columns);
       last_column &&
       !(gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (last_column->data)));
       last_column = last_column->prev)
    ;
  if (last_column == NULL)
    return;

  for (first_column = g_list_first (tree_view->priv->columns);
       first_column &&
       !(gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (first_column->data)));
       first_column = first_column->next)
    ;

  if (first_column == NULL)
    return;

  rtl = (_gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

  /* find out how many extra space and expandable columns we have */
  for (list = tree_view->priv->columns; list != last_column->next; list = list->next)
    {
      column = (GtkTreeViewColumn *)list->data;

      if (!gtk_tree_view_column_get_visible (column) || column == tree_view->priv->drag_column)
	continue;

      full_requested_width += _gtk_tree_view_column_request_width (column);

      if (gtk_tree_view_column_get_expand (column))
	number_of_expand_columns++;
    }

  widget_width = gtk_widget_get_width (widget);
  extra = MAX (widget_width - full_requested_width, 0);

  if (number_of_expand_columns > 0)
    extra_per_column = extra/number_of_expand_columns;
  else
    extra_per_column = 0;

  for (list = first_column;
       list != last_column->next;
       list = list->next)
    {
      gint column_width;

      column = list->data;
      column_width = _gtk_tree_view_column_request_width (column);

      if (!gtk_tree_view_column_get_visible (column))
	continue;

      if (column == tree_view->priv->drag_column)
        goto next;

      if (gtk_tree_view_column_get_expand (column))
	{
	  if (number_of_expand_columns == 1)
	    {
	      /* We add the remander to the last column as
	       * */
	      column_width += extra;
	    }
	  else
	    {
	      column_width += extra_per_column;
	      extra -= extra_per_column;
	      number_of_expand_columns --;
	    }
	}
      else if (number_of_expand_columns == 0 &&
	       list == last_column)
	{
	  column_width += extra;
	}

      if (rtl)
        _gtk_tree_view_column_allocate (column, widget_width - width - column_width + x_offset,
                                        column_width, tree_view->priv->header_height);
      else
        _gtk_tree_view_column_allocate (column, width + x_offset,
                                        column_width, tree_view->priv->header_height);
  next:
      width += column_width;
    }

  /* We change the width here.  The user might have been resizing columns,
   * which changes the total width of the tree view.  This is of
   * importance for getting the horizontal scroll bar right.
   */
  tree_view->priv->width = width;
}

/* GtkWidget::size_allocate helper */
static void
gtk_tree_view_size_allocate_drag_column (GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkAllocation allocation;
  int baseline;
  GtkWidget *button;

  if (tree_view->priv->drag_column == NULL)
    return;

  button = gtk_tree_view_column_get_button (tree_view->priv->drag_column);

  allocation.x = tree_view->priv->drag_column_x;
  allocation.y = tree_view->priv->drag_column_y;
  allocation.width = gtk_widget_get_allocated_width (button);
  allocation.height = gtk_widget_get_allocated_height (button);
  baseline = gtk_widget_get_allocated_baseline (button);

  gtk_widget_size_allocate (button, &allocation, baseline);
}

static void
gtk_tree_view_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GList *tmp_list;
  double page_size;

  /* We size-allocate the columns first because the width of the
   * tree view (used in updating the adjustments below) might change.
   */
  gtk_tree_view_size_allocate_columns (widget);
  gtk_tree_view_size_allocate_drag_column (widget);

  page_size = gtk_adjustment_get_page_size (tree_view->priv->vadjustment);
  gtk_adjustment_configure (tree_view->priv->hadjustment,
                            gtk_adjustment_get_value (tree_view->priv->hadjustment) +
                            (_gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL ? width - page_size : 0),
                            0,
                            MAX (width, tree_view->priv->width),
                            width * 0.1,
                            width * 0.9,
                            width);

  page_size = height - gtk_tree_view_get_effective_header_height (tree_view);
  gtk_adjustment_configure (tree_view->priv->vadjustment,
                            gtk_adjustment_get_value (tree_view->priv->vadjustment),
                            0,
                            MAX (page_size, gtk_tree_view_get_height (tree_view)),
                            page_size * 0.1,
                            page_size * 0.9,
                            page_size);
 
  /* now the adjustments and window sizes are in sync, we can sync toprow/dy again */
  if (gtk_tree_row_reference_valid (tree_view->priv->top_row))
    gtk_tree_view_top_row_to_dy (tree_view);
  else
    gtk_tree_view_dy_to_top_row (tree_view);
  
  if (gtk_widget_get_realized (widget))
    {
      if (tree_view->priv->tree == NULL)
        invalidate_empty_focus (tree_view);

      if (tree_view->priv->expander_column)
        {
          /* Might seem awkward, but is the best heuristic I could come up
           * with.  Only if the width of the columns before the expander
           * changes, we will update the prelight status.  It is this
           * width that makes the expander move vertically.  Always updating
           * prelight status causes trouble with hover selections.
           */
          gint width_before_expander;

          width_before_expander = gtk_tree_view_calculate_width_before_expander (tree_view);

          if (tree_view->priv->prev_width_before_expander
              != width_before_expander)
              update_prelight (tree_view,
                               tree_view->priv->event_last_x,
                               tree_view->priv->event_last_y);

          tree_view->priv->prev_width_before_expander = width_before_expander;
        }
    }

  for (tmp_list = tree_view->priv->children; tmp_list; tmp_list = tmp_list->next)
    {
      GtkTreeViewChild *child = tmp_list->data;
      GtkTreePath *path;
      GdkRectangle child_rect;
      int min_x, max_x, min_y, max_y;
      int size;
      GtkTextDirection direction;

      direction = _gtk_widget_get_direction (child->widget);
      path = _gtk_tree_path_new_from_rbtree (child->tree, child->node);
      gtk_tree_view_get_cell_area (tree_view, path, child->column, &child_rect);
      child_rect.x += child->border.left;
      child_rect.y += child->border.top;
      child_rect.width -= child->border.left + child->border.right;
      child_rect.height -= child->border.top + child->border.bottom;

      gtk_widget_measure (GTK_WIDGET (child->widget), GTK_ORIENTATION_HORIZONTAL, -1,
                          &size, NULL, NULL, NULL);

      if (size > child_rect.width)
        {
          /* Enlarge the child, extending it to the left (RTL) */
          if (direction == GTK_TEXT_DIR_RTL)
            child_rect.x -= (size - child_rect.width);
          /* or to the right (LTR) */
          else
            child_rect.x += 0;

          child_rect.width = size;
        }

      gtk_widget_measure (GTK_WIDGET (child->widget), GTK_ORIENTATION_VERTICAL,
                          child_rect.width,
                          &size, NULL,
                          NULL, NULL);
      if (size > child_rect.height)
        {
          /* Enlarge the child, extending in both directions equally */
          child_rect.y -= (size - child_rect.height) / 2;
          child_rect.height = size;
        }

      /* push the rect back in the visible area if needed,
       * preferring the top left corner (for RTL)
       * or top right corner (for LTR)
       */
      min_x = gtk_adjustment_get_value (tree_view->priv->hadjustment);
      max_x = min_x + width - child_rect.width;
      min_y = 0;
        max_y = min_y + height - gtk_tree_view_get_effective_header_height (tree_view) - child_rect.height;

      if (direction == GTK_TEXT_DIR_LTR)
        /* Ensure that child's right edge is not sticking to the right
         * (if (child_rect.x > max_x) child_rect.x = max_x),
         * then ensure that child's left edge is visible and is not sticking to the left
         * (if (child_rect.x < min_x) child_rect.x = min_x).
         */
        child_rect.x = MAX (min_x, MIN (max_x, child_rect.x));
      else
        /* Ensure that child's left edge is not sticking to the left
         * (if (child_rect.x < min_x) child_rect.x = min_x),
         * then ensure that child's right edge is visible and is not sticking to the right
         * (if (child_rect.x > max_x) child_rect.x = max_x).
         */
        child_rect.x = MIN (max_x, MAX (min_x, child_rect.x));

      child_rect.y = MAX (min_y, MIN (max_y, child_rect.y));

      gtk_tree_path_free (path);
      gtk_widget_size_allocate (child->widget, &child_rect, -1);
    }
}

/* Grabs the focus and unsets the GTK_TREE_VIEW_DRAW_KEYFOCUS flag */
static void
grab_focus_and_unset_draw_keyfocus (GtkTreeView *tree_view)
{
  GtkWidget *widget = GTK_WIDGET (tree_view);

  if (gtk_widget_get_can_focus (widget) &&
      !gtk_widget_has_focus (widget) &&
      !_gtk_widget_get_shadowed (widget))
    gtk_widget_grab_focus (widget);

  tree_view->priv->draw_keyfocus = 0;
}

static inline gboolean
row_is_separator (GtkTreeView *tree_view,
		  GtkTreeIter *iter,
		  GtkTreePath *path)
{
  gboolean is_separator = FALSE;

  if (tree_view->priv->row_separator_func)
    {
      GtkTreeIter tmpiter;

      if (iter)
        tmpiter = *iter;
      else
        {
          if (!gtk_tree_model_get_iter (tree_view->priv->model, &tmpiter, path))
            return FALSE;
        }

      is_separator = tree_view->priv->row_separator_func (tree_view->priv->model,
                                                          &tmpiter,
                                                          tree_view->priv->row_separator_data);
    }

  return is_separator;
}

static int
gtk_tree_view_get_expander_size (GtkTreeView *tree_view)
{
  GtkTreeViewPrivate *priv = gtk_tree_view_get_instance_private (tree_view);
  GtkStyleContext *context;
  GtkCssStyle *style;
  int min_width;
  int min_height;
  int expander_size;

  if (priv->expander_size != -1)
    return priv->expander_size;

  context = gtk_widget_get_style_context (GTK_WIDGET (tree_view));
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_EXPANDER);

  style = gtk_style_context_lookup_style (context);
  min_width = _gtk_css_number_value_get
    (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MIN_WIDTH), 100);
  min_height = _gtk_css_number_value_get
    (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MIN_HEIGHT), 100);

  gtk_style_context_restore (context);

  expander_size = MAX (min_width, min_height);

  priv->expander_size = expander_size + (_TREE_VIEW_HORIZONTAL_SEPARATOR / 2);

  return priv->expander_size;
}

static void
get_current_selection_modifiers (GtkWidget *widget,
                                 gboolean  *modify,
                                 gboolean  *extend)
{
  GdkModifierType state = 0;
  GdkModifierType mask;

  *modify = FALSE;
  *extend = FALSE;

  if (gtk_get_current_event_state (&state))
    {
      mask = gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_MODIFY_SELECTION);
      if ((state & mask) == mask)
        *modify = TRUE;
      mask = gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_EXTEND_SELECTION);
      if ((state & mask) == mask)
        *extend = TRUE;
    }
}

static void
gtk_tree_view_click_gesture_pressed (GtkGestureClick *gesture,
                                          gint                  n_press,
                                          gdouble               x,
                                          gdouble               y,
                                          GtkTreeView          *tree_view)
{
  GtkWidget *widget = GTK_WIDGET (tree_view);
  GdkRectangle background_area, cell_area;
  GtkTreeViewColumn *column = NULL;
  GdkEventSequence *sequence;
  GdkModifierType modifiers;
  const GdkEvent *event;
  gint new_y, y_offset;
  gint bin_x, bin_y;
  GtkTreePath *path;
  GtkTreeRBNode *node;
  GtkTreeRBTree *tree;
  gint depth;
  guint button;
  GList *list;
  gboolean rtl;

  rtl = (_gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
  gtk_tree_view_stop_editing (tree_view, FALSE);
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button > 3)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  /* Because grab_focus can cause reentrancy, we delay grab_focus until after
   * we're done handling the button press.
   */
  gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, x, y,
                                                     &bin_x, &bin_y);
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  if (n_press > 1)
    gtk_gesture_set_state (tree_view->priv->drag_gesture,
                           GTK_EVENT_SEQUENCE_DENIED);

  /* Empty tree? */
  if (tree_view->priv->tree == NULL)
    {
      grab_focus_and_unset_draw_keyfocus (tree_view);
      return;
    }

  /* are we in an arrow? */
  if (tree_view->priv->prelight_node &&
      tree_view->priv->arrow_prelit &&
      gtk_tree_view_draw_expanders (tree_view))
    {
      if (button == GDK_BUTTON_PRIMARY)
        {
          tree_view->priv->button_pressed_node = tree_view->priv->prelight_node;
          tree_view->priv->button_pressed_tree = tree_view->priv->prelight_tree;
          gtk_widget_queue_draw (widget);
        }

      grab_focus_and_unset_draw_keyfocus (tree_view);
      return;
    }

  /* find the node that was clicked */
  new_y = TREE_WINDOW_Y_TO_RBTREE_Y(tree_view, bin_y);
  if (new_y < 0)
    new_y = 0;
  y_offset = -gtk_tree_rbtree_find_offset (tree_view->priv->tree, new_y, &tree, &node);

  if (node == NULL)
    {
      /* We clicked in dead space */
      grab_focus_and_unset_draw_keyfocus (tree_view);
      return;
    }

  /* Get the path and the node */
  path = _gtk_tree_path_new_from_rbtree (tree, node);

  if (row_is_separator (tree_view, NULL, path))
    {
      gtk_tree_path_free (path);
      grab_focus_and_unset_draw_keyfocus (tree_view);
      return;
    }

  depth = gtk_tree_path_get_depth (path);
  background_area.y = y_offset + bin_y;
  background_area.height = gtk_tree_view_get_row_height (tree_view, node);
  background_area.x = 0;

  gtk_tree_view_convert_bin_window_to_widget_coords (tree_view,
                                                     background_area.x,
                                                     background_area.y,
                                                     &background_area.x,
                                                     &background_area.y);

  /* Let the column have a chance at selecting it. */
  rtl = (_gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
  for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
       list; list = (rtl ? list->prev : list->next))
    {
      GtkTreeViewColumn *candidate = list->data;

      if (!gtk_tree_view_column_get_visible (candidate))
        continue;

      background_area.width = gtk_tree_view_column_get_width (candidate);
      if ((background_area.x > bin_x) ||
          (background_area.x + background_area.width <= bin_x))
        {
          background_area.x += background_area.width;
          continue;
        }

      /* we found the focus column */
      column = candidate;
      cell_area = background_area;
      cell_area.width -= _TREE_VIEW_HORIZONTAL_SEPARATOR;
      cell_area.x += _TREE_VIEW_HORIZONTAL_SEPARATOR / 2;
      if (gtk_tree_view_is_expander_column (tree_view, column))
        {
          if (!rtl)
            cell_area.x += (depth - 1) * tree_view->priv->level_indentation;
          cell_area.width -= (depth - 1) * tree_view->priv->level_indentation;

          if (gtk_tree_view_draw_expanders (tree_view))
            {
              gint expander_size = gtk_tree_view_get_expander_size (tree_view);
              if (!rtl)
                cell_area.x += depth * expander_size;
              cell_area.width -= depth * expander_size;
            }
        }
      break;
    }

  if (column == NULL)
    {
      gtk_tree_path_free (path);
      grab_focus_and_unset_draw_keyfocus (tree_view);
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  _gtk_tree_view_set_focus_column (tree_view, column);

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  gdk_event_get_state (event, &modifiers);

  /* decide if we edit */
  if (button == GDK_BUTTON_PRIMARY &&
      !(modifiers & gtk_accelerator_get_default_mod_mask ()))
    {
      GtkTreePath *anchor;
      GtkTreeIter iter;

      gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
      gtk_tree_view_column_cell_set_cell_data (column,
                                               tree_view->priv->model,
                                               &iter,
                                               GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_PARENT),
                                               node->children?TRUE:FALSE);

      if (tree_view->priv->anchor)
        anchor = gtk_tree_row_reference_get_path (tree_view->priv->anchor);
      else
        anchor = NULL;

      if ((anchor && !gtk_tree_path_compare (anchor, path))
          || !_gtk_tree_view_column_has_editable_cell (column))
        {
          GtkCellEditable *cell_editable = NULL;

          /* FIXME: get the right flags */
          guint flags = 0;

          if (_gtk_tree_view_column_cell_event (column,
                                                (GdkEvent *)event,
                                                &cell_area, flags))
            {
              GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));
              cell_editable = gtk_cell_area_get_edit_widget (area);

              if (cell_editable != NULL)
                {
                  gtk_tree_path_free (path);
                  gtk_tree_path_free (anchor);
                  return;
                }
            }
        }
      if (anchor)
        gtk_tree_path_free (anchor);
    }

  /* we only handle selection modifications on the first button press
   */
  if (n_press == 1)
    {
      GtkCellRenderer *focus_cell;
      gboolean modify, extend;

      get_current_selection_modifiers (widget, &modify, &extend);
      tree_view->priv->modify_selection_pressed = modify;
      tree_view->priv->extend_selection_pressed = extend;

      /* We update the focus cell here, this is also needed if the
       * column does not contain an editable cell.  In this case,
       * GtkCellArea did not receive the event for processing (and
       * could not update the focus cell).
       */
      focus_cell = _gtk_tree_view_column_get_cell_at_pos (column,
                                                          &cell_area,
                                                          &background_area,
                                                          bin_x, bin_y);

      if (focus_cell)
        gtk_tree_view_column_focus_cell (column, focus_cell);

      if (modify)
        {
          gtk_tree_view_real_set_cursor (tree_view, path, CLAMP_NODE);
          gtk_tree_view_real_toggle_cursor_row (tree_view);
        }
      else if (extend)
        {
          gtk_tree_view_real_set_cursor (tree_view, path, CLAMP_NODE);
          gtk_tree_view_real_select_cursor_row (tree_view, FALSE);
        }
      else
        {
          gtk_tree_view_real_set_cursor (tree_view, path, CLEAR_AND_SELECT | CLAMP_NODE);
        }

      tree_view->priv->modify_selection_pressed = FALSE;
      tree_view->priv->extend_selection_pressed = FALSE;
    }

  if (button == GDK_BUTTON_PRIMARY && n_press == 2)
    gtk_tree_view_row_activated (tree_view, path, column);
  else
    {
      if (n_press == 1)
        {
          tree_view->priv->button_pressed_node = tree_view->priv->prelight_node;
          tree_view->priv->button_pressed_tree = tree_view->priv->prelight_tree;
        }

      grab_focus_and_unset_draw_keyfocus (tree_view);
    }

  gtk_tree_path_free (path);

  if (n_press >= 2)
    gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
}

static void
gtk_tree_view_drag_gesture_begin (GtkGestureDrag *gesture,
                                  gdouble         start_x,
                                  gdouble         start_y,
                                  GtkTreeView    *tree_view)
{
  gint bin_x, bin_y;
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;

  if (tree_view->priv->tree == NULL)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, start_x, start_y,
                                                     &bin_x, &bin_y);
  tree_view->priv->press_start_x = tree_view->priv->rubber_band_x = bin_x;
  tree_view->priv->press_start_y = tree_view->priv->rubber_band_y = bin_y;
  gtk_tree_rbtree_find_offset (tree_view->priv->tree, bin_y + tree_view->priv->dy,
                           &tree, &node);

  if (tree_view->priv->rubber_banding_enable
      && !GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_SELECTED)
      && gtk_tree_selection_get_mode (tree_view->priv->selection) == GTK_SELECTION_MULTIPLE)
    {
      gboolean modify, extend;

      tree_view->priv->press_start_y += tree_view->priv->dy;
      tree_view->priv->rubber_band_y += tree_view->priv->dy;
      tree_view->priv->rubber_band_status = RUBBER_BAND_MAYBE_START;

      get_current_selection_modifiers (GTK_WIDGET (tree_view), &modify, &extend);
      tree_view->priv->rubber_band_modify = modify;
      tree_view->priv->rubber_band_extend = extend;
    }
}

static void
gtk_tree_view_column_click_gesture_pressed (GtkGestureClick *gesture,
                                                 gint                  n_press,
                                                 gdouble               x,
                                                 gdouble               y,
                                                 GtkTreeView          *tree_view)
{
  GtkTreeViewColumn *column;
  GList *list;

  if (n_press != 2)
    return;

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      column = list->data;

      if (!_gtk_tree_view_column_coords_in_resize_rect (column, x, y) ||
          !gtk_tree_view_column_get_resizable (column))
        continue;

      if (gtk_tree_view_column_get_sizing (column) != GTK_TREE_VIEW_COLUMN_AUTOSIZE)
        {
          gtk_tree_view_column_set_fixed_width (column, -1);
          gtk_tree_view_column_set_expand (column, FALSE);
          _gtk_tree_view_column_autosize (tree_view, column);
        }

      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      break;
    }
}

static void
gtk_tree_view_column_drag_gesture_begin (GtkGestureDrag *gesture,
                                         gdouble         start_x,
                                         gdouble         start_y,
                                         GtkTreeView    *tree_view)
{
  GtkTreeViewColumn *column;
  gboolean rtl;
  GList *list;
  gint i;

  rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

  for (i = 0, list = tree_view->priv->columns; list; list = list->next, i++)
    {
      gpointer drag_data;
      gint column_width;

      column = list->data;

      if (!_gtk_tree_view_column_coords_in_resize_rect (column, start_x, start_y))
        continue;

      if (!gtk_tree_view_column_get_resizable (column))
        break;

      tree_view->priv->in_column_resize = TRUE;

      /* block attached dnd signal handler */
      drag_data = g_object_get_data (G_OBJECT (tree_view), "gtk-site-data");
      if (drag_data)
        g_signal_handlers_block_matched (tree_view,
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL,
                                         drag_data);

      column_width = gtk_tree_view_column_get_width (column);
      gtk_tree_view_column_set_fixed_width (column, column_width);
      gtk_tree_view_column_set_expand (column, FALSE);

      tree_view->priv->drag_pos = i;
      tree_view->priv->x_drag = start_x + (rtl ? column_width : -column_width);

      if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
        gtk_widget_grab_focus (GTK_WIDGET (tree_view));

      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      return;
    }
}

static void
gtk_tree_view_update_button_position (GtkTreeView       *tree_view,
                                      GtkTreeViewColumn *column)
{
  GtkTreeViewPrivate *priv = tree_view->priv;
  GList *column_el;

  column_el = g_list_find (priv->columns, column);
  g_return_if_fail (column_el != NULL);

  gtk_css_node_insert_after (priv->header_node,
                             gtk_widget_get_css_node (gtk_tree_view_column_get_button (column)),
                             column_el->prev ? gtk_widget_get_css_node (
                                gtk_tree_view_column_get_button (column_el->prev->data)) : NULL);
}

/* column drag gesture helper */
static gboolean
gtk_tree_view_button_release_drag_column (GtkTreeView *tree_view)
{
  GtkWidget *button, *widget = GTK_WIDGET (tree_view);
  GList *l;
  gboolean rtl;
  GtkStyleContext *context;

  rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

  /* Move the button back */
  button = gtk_tree_view_column_get_button (tree_view->priv->drag_column);

  context = gtk_widget_get_style_context (button);
  gtk_style_context_remove_class (context, GTK_STYLE_CLASS_DND);

  gtk_tree_view_update_button_position (tree_view, tree_view->priv->drag_column);
  gtk_widget_queue_allocate (widget);

  gtk_widget_grab_focus (button);

  if (rtl)
    {
      if (tree_view->priv->cur_reorder &&
	  tree_view->priv->cur_reorder->right_column != tree_view->priv->drag_column)
	gtk_tree_view_move_column_after (tree_view, tree_view->priv->drag_column,
					 tree_view->priv->cur_reorder->right_column);
    }
  else
    {
      if (tree_view->priv->cur_reorder &&
	  tree_view->priv->cur_reorder->left_column != tree_view->priv->drag_column)
	gtk_tree_view_move_column_after (tree_view, tree_view->priv->drag_column,
					 tree_view->priv->cur_reorder->left_column);
    }
  tree_view->priv->drag_column = NULL;

  for (l = tree_view->priv->column_drag_info; l != NULL; l = l->next)
    g_slice_free (GtkTreeViewColumnReorder, l->data);
  g_list_free (tree_view->priv->column_drag_info);
  tree_view->priv->column_drag_info = NULL;
  tree_view->priv->cur_reorder = NULL;

  /* Reset our flags */
  tree_view->priv->drag_column_surface_state = DRAG_COLUMN_WINDOW_STATE_UNSET;
  tree_view->priv->in_column_drag = FALSE;

  return TRUE;
}

/* column drag gesture helper */
static gboolean
gtk_tree_view_button_release_column_resize (GtkTreeView *tree_view)
{
  gpointer drag_data;

  tree_view->priv->drag_pos = -1;

  /* unblock attached dnd signal handler */
  drag_data = g_object_get_data (G_OBJECT (tree_view), "gtk-site-data");
  if (drag_data)
    g_signal_handlers_unblock_matched (tree_view,
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL, NULL,
				       drag_data);

  tree_view->priv->in_column_resize = FALSE;
  return TRUE;
}

static void
gtk_tree_view_column_drag_gesture_end (GtkGestureDrag *gesture,
                                       gdouble         offset_x,
                                       gdouble         offset_y,
                                       GtkTreeView    *tree_view)
{
  GdkEventSequence *sequence;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  /* Cancel reorder if the drag got cancelled */
  if (!gtk_gesture_handles_sequence (GTK_GESTURE (gesture), sequence))
    tree_view->priv->cur_reorder = NULL;

  if (tree_view->priv->in_column_drag)
    gtk_tree_view_button_release_drag_column (tree_view);
  else if (tree_view->priv->in_column_resize)
    gtk_tree_view_button_release_column_resize (tree_view);
}

static void
gtk_tree_view_drag_gesture_end (GtkGestureDrag *gesture,
                                gdouble         offset_x,
                                gdouble         offset_y,
                                GtkTreeView    *tree_view)
{
  gtk_tree_view_stop_rubber_band (tree_view);
}

static void
gtk_tree_view_click_gesture_released (GtkGestureClick *gesture,
                                      gint             n_press,
                                      gdouble          x,
                                      gdouble          y,
                                      GtkTreeView     *tree_view)
{
  gboolean modify, extend;
  guint button;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button != GDK_BUTTON_PRIMARY ||
      tree_view->priv->button_pressed_node == NULL ||
      tree_view->priv->button_pressed_node != tree_view->priv->prelight_node)
    return;

  get_current_selection_modifiers (GTK_WIDGET (tree_view), &modify, &extend);

  if (tree_view->priv->arrow_prelit)
    {
      GtkTreePath *path = NULL;

      path = _gtk_tree_path_new_from_rbtree (tree_view->priv->button_pressed_tree,
                                             tree_view->priv->button_pressed_node);
      /* Actually activate the node */
      if (tree_view->priv->button_pressed_node->children == NULL)
        gtk_tree_view_real_expand_row (tree_view, path,
                                       tree_view->priv->button_pressed_tree,
                                       tree_view->priv->button_pressed_node,
                                       FALSE, TRUE);
      else
        gtk_tree_view_real_collapse_row (tree_view, path,
                                         tree_view->priv->button_pressed_tree,
                                         tree_view->priv->button_pressed_node, TRUE);
      gtk_tree_path_free (path);
    }
  else if (tree_view->priv->activate_on_single_click && !modify && !extend)
    {
      GtkTreePath *path = NULL;

      path = _gtk_tree_path_new_from_rbtree (tree_view->priv->button_pressed_tree,
                                             tree_view->priv->button_pressed_node);
      gtk_tree_view_row_activated (tree_view, path, tree_view->priv->focus_column);
      gtk_tree_path_free (path);
    }

  tree_view->priv->button_pressed_tree = NULL;
  tree_view->priv->button_pressed_node = NULL;
}

/* GtkWidget::motion_event function set.
 */

static gboolean
coords_are_over_arrow (GtkTreeView   *tree_view,
                       GtkTreeRBTree *tree,
                       GtkTreeRBNode *node,
                       /* these are in bin window coords */
                       gint           x,
                       gint           y)
{
  GdkRectangle arrow;
  gint x2;

  if (!gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    return FALSE;

  if ((node->flags & GTK_TREE_RBNODE_IS_PARENT) == 0)
    return FALSE;

  arrow.y = gtk_tree_view_get_row_y_offset (tree_view, tree, node);
  arrow.height = gtk_tree_view_get_row_height (tree_view, node);

  gtk_tree_view_get_arrow_xrange (tree_view, tree, &arrow.x, &x2);

  arrow.width = x2 - arrow.x;

  return (x >= arrow.x &&
          x < (arrow.x + arrow.width) &&
	  y >= arrow.y &&
	  y < (arrow.y + arrow.height));
}

static gboolean
auto_expand_timeout (gpointer data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (data);
  GtkTreePath *path;

  if (tree_view->priv->prelight_node)
    {
      path = _gtk_tree_path_new_from_rbtree (tree_view->priv->prelight_tree,
				             tree_view->priv->prelight_node);

      if (tree_view->priv->prelight_node->children)
	gtk_tree_view_collapse_row (tree_view, path);
      else
	gtk_tree_view_expand_row (tree_view, path, FALSE);

      gtk_tree_path_free (path);
    }

  tree_view->priv->auto_expand_timeout = 0;

  return FALSE;
}

static void
remove_auto_expand_timeout (GtkTreeView *tree_view)
{
  if (tree_view->priv->auto_expand_timeout != 0)
    {
      g_source_remove (tree_view->priv->auto_expand_timeout);
      tree_view->priv->auto_expand_timeout = 0;
    }
}

static void
do_prelight (GtkTreeView   *tree_view,
             GtkTreeRBTree *tree,
             GtkTreeRBNode *node,
	     /* these are in bin_window coords */
             gint           x,
             gint           y)
{
  if (tree_view->priv->prelight_tree == tree &&
      tree_view->priv->prelight_node == node)
    {
      /*  We are still on the same node,
	  but we might need to take care of the arrow  */

      if (tree && node && gtk_tree_view_draw_expanders (tree_view))
	{
	  gboolean over_arrow;

	  over_arrow = coords_are_over_arrow (tree_view, tree, node, x, y);

	  if (over_arrow != tree_view->priv->arrow_prelit)
	    {
	      if (over_arrow)
                tree_view->priv->arrow_prelit = TRUE;
	      else
                tree_view->priv->arrow_prelit = FALSE;

	      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
	    }
	}

      return;
    }

  if (tree_view->priv->prelight_tree && tree_view->priv->prelight_node)
    {
      /*  Unprelight the old node and arrow  */

      GTK_TREE_RBNODE_UNSET_FLAG (tree_view->priv->prelight_node,
			     GTK_TREE_RBNODE_IS_PRELIT);

      if (tree_view->priv->arrow_prelit
	  && gtk_tree_view_draw_expanders (tree_view))
	{
          tree_view->priv->arrow_prelit = FALSE;
	  
	  gtk_widget_queue_draw (GTK_WIDGET (tree_view));
	}

      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
    }


  if (tree_view->priv->hover_expand)
    remove_auto_expand_timeout (tree_view);

  /*  Set the new prelight values  */
  tree_view->priv->prelight_node = node;
  tree_view->priv->prelight_tree = tree;

  if (!node || !tree)
    return;

  /*  Prelight the new node and arrow  */

  if (gtk_tree_view_draw_expanders (tree_view)
      && coords_are_over_arrow (tree_view, tree, node, x, y))
    {
      tree_view->priv->arrow_prelit = TRUE;

      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
    }

  GTK_TREE_RBNODE_SET_FLAG (node, GTK_TREE_RBNODE_IS_PRELIT);

  gtk_widget_queue_draw (GTK_WIDGET (tree_view));

  if (tree_view->priv->hover_expand)
    {
      tree_view->priv->auto_expand_timeout = 
	g_timeout_add (AUTO_EXPAND_TIMEOUT, auto_expand_timeout, tree_view);
      g_source_set_name_by_id (tree_view->priv->auto_expand_timeout, "[gtk] auto_expand_timeout");
    }
}

static void
prelight_or_select (GtkTreeView   *tree_view,
		    GtkTreeRBTree *tree,
		    GtkTreeRBNode *node,
		    /* these are in bin_window coords */
		    gint           x,
		    gint           y)
{
  GtkSelectionMode mode = gtk_tree_selection_get_mode (tree_view->priv->selection);
  
  if (tree_view->priv->hover_selection &&
      (mode == GTK_SELECTION_SINGLE || mode == GTK_SELECTION_BROWSE) &&
      !(tree_view->priv->edited_column &&
	gtk_cell_area_get_edit_widget 
	(gtk_cell_layout_get_area (GTK_CELL_LAYOUT (tree_view->priv->edited_column)))))
    {
      if (node)
	{
	  if (!GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_SELECTED))
	    {
	      GtkTreePath *path;
	      
	      path = _gtk_tree_path_new_from_rbtree (tree, node);
	      gtk_tree_selection_select_path (tree_view->priv->selection, path);
	      if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_SELECTED))
		{
                  tree_view->priv->draw_keyfocus = FALSE;
		  gtk_tree_view_real_set_cursor (tree_view, path, 0);
		}
	      gtk_tree_path_free (path);
	    }
	}

      else if (mode == GTK_SELECTION_SINGLE)
	gtk_tree_selection_unselect_all (tree_view->priv->selection);
    }

    do_prelight (tree_view, tree, node, x, y);
}

static void
ensure_unprelighted (GtkTreeView *tree_view)
{
  do_prelight (tree_view,
	       NULL, NULL,
	       -1000, -1000); /* coords not possibly over an arrow */

  g_assert (tree_view->priv->prelight_node == NULL);
}

static void
update_prelight (GtkTreeView *tree_view,
                 gint         x,
                 gint         y)
{
  int new_y;
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;

  if (tree_view->priv->tree == NULL)
    return;

  if (x == -10000)
    {
      ensure_unprelighted (tree_view);
      return;
    }

  new_y = TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, y);
  if (new_y < 0)
    new_y = 0;

  gtk_tree_rbtree_find_offset (tree_view->priv->tree,
                           new_y, &tree, &node);

  if (node)
    prelight_or_select (tree_view, tree, node, x, y);
}

static gboolean
gtk_tree_view_motion_resize_column (GtkTreeView *tree_view,
                                    gdouble      x,
                                    gdouble      y)
{
  gint new_width;
  GtkTreeViewColumn *column;

  column = gtk_tree_view_get_column (tree_view, tree_view->priv->drag_pos);

  if (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL)
    new_width = MAX (tree_view->priv->x_drag - x, 0);
  else
    new_width = MAX (x - tree_view->priv->x_drag, 0);

  if (new_width != gtk_tree_view_column_get_fixed_width (column))
    gtk_tree_view_column_set_fixed_width (column, new_width);

  return FALSE;
}

static void
gtk_tree_view_update_current_reorder (GtkTreeView *tree_view)
{
  GtkTreeViewColumnReorder *reorder = NULL;
  GdkEventSequence *sequence;
  GList *list;
  gdouble x;

  sequence = gtk_gesture_single_get_current_sequence
    (GTK_GESTURE_SINGLE (tree_view->priv->column_drag_gesture));
  gtk_gesture_get_point (tree_view->priv->column_drag_gesture,
                         sequence, &x, NULL);
  x += gtk_adjustment_get_value (tree_view->priv->hadjustment);

  for (list = tree_view->priv->column_drag_info; list; list = list->next)
    {
      reorder = (GtkTreeViewColumnReorder *) list->data;
      if (x >= reorder->left_align && x < reorder->right_align)
	break;
      reorder = NULL;
    }

  tree_view->priv->cur_reorder = reorder;
}

static void
gtk_tree_view_vertical_autoscroll (GtkTreeView *tree_view)
{
  GdkRectangle visible_rect;
  gint y;
  gint offset;

  if (gtk_gesture_is_recognized (tree_view->priv->drag_gesture))
    {
      GdkEventSequence *sequence;
      gdouble py;

      sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (tree_view->priv->drag_gesture));
      gtk_gesture_get_point (tree_view->priv->drag_gesture, sequence, NULL, &py);
      gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, 0, py,
							  NULL, &y);
    }
  else
    {
      y = tree_view->priv->event_last_y;
      gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, 0, y, NULL, &y);
    }

  y += tree_view->priv->dy;
  gtk_tree_view_get_visible_rect (tree_view, &visible_rect);

  /* see if we are near the edge. */
  offset = y - (visible_rect.y + 2 * SCROLL_EDGE_SIZE);
  if (offset > 0)
    {
      offset = y - (visible_rect.y + visible_rect.height - 2 * SCROLL_EDGE_SIZE);
      if (offset < 0)
	return;
    }

  gtk_adjustment_set_value (tree_view->priv->vadjustment,
                            MAX (gtk_adjustment_get_value (tree_view->priv->vadjustment) + offset, 0.0));
}

static void
gtk_tree_view_horizontal_autoscroll (GtkTreeView *tree_view)
{
  GdkEventSequence *sequence;
  GdkRectangle visible_rect;
  gdouble x;
  gint offset;

  sequence = gtk_gesture_single_get_current_sequence
    (GTK_GESTURE_SINGLE (tree_view->priv->column_drag_gesture));
  gtk_gesture_get_point (tree_view->priv->column_drag_gesture,
                         sequence, &x, NULL);
  gtk_tree_view_get_visible_rect (tree_view, &visible_rect);

  x += gtk_adjustment_get_value (tree_view->priv->hadjustment);

  /* See if we are near the edge. */
  offset = x - (visible_rect.x + SCROLL_EDGE_SIZE);
  if (offset > 0)
    {
      offset = x - (visible_rect.x + visible_rect.width - SCROLL_EDGE_SIZE);
      if (offset < 0)
	return;
    }
  offset = offset/3;

  gtk_adjustment_set_value (tree_view->priv->hadjustment,
                            MAX (gtk_adjustment_get_value (tree_view->priv->hadjustment) + offset, 0.0));
}

static void
gtk_tree_view_motion_drag_column (GtkTreeView *tree_view,
                                  gdouble      x,
                                  gdouble      y)
{
  GtkTreeViewColumn *column = tree_view->priv->drag_column;
  GtkWidget *button;
  gint width, button_width;

  button = gtk_tree_view_column_get_button (column);
  x += gtk_adjustment_get_value (tree_view->priv->hadjustment);

  /* Handle moving the header */
  width = gtk_widget_get_allocated_width (GTK_WIDGET (tree_view));
  button_width = gtk_widget_get_allocated_width (button);
  tree_view->priv->drag_column_x = CLAMP (x - _gtk_tree_view_column_get_drag_x (column), 0,
                                          MAX (tree_view->priv->width, width) - button_width);

  /* autoscroll, if needed */
  gtk_tree_view_horizontal_autoscroll (tree_view);
  /* Update the current reorder position and arrow; */
  gtk_tree_view_update_current_reorder (tree_view);
  gtk_widget_queue_allocate (GTK_WIDGET (tree_view));
}

static void
gtk_tree_view_stop_rubber_band (GtkTreeView *tree_view)
{
  remove_scroll_timeout (tree_view);

  if (tree_view->priv->rubber_band_status == RUBBER_BAND_ACTIVE)
    {
      GtkTreePath *tmp_path;

      gtk_widget_queue_draw (GTK_WIDGET (tree_view));

      /* The anchor path should be set to the start path */
      if (tree_view->priv->rubber_band_start_node)
        {
          tmp_path = _gtk_tree_path_new_from_rbtree (tree_view->priv->rubber_band_start_tree,
                                                     tree_view->priv->rubber_band_start_node);

          if (tree_view->priv->anchor)
            gtk_tree_row_reference_free (tree_view->priv->anchor);

          tree_view->priv->anchor =
            gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view),
                                              tree_view->priv->model,
                                              tmp_path);

          gtk_tree_path_free (tmp_path);
        }

      /* ... and the cursor to the end path */
      if (tree_view->priv->rubber_band_end_node)
        {
          tmp_path = _gtk_tree_path_new_from_rbtree (tree_view->priv->rubber_band_end_tree,
                                                     tree_view->priv->rubber_band_end_node);
          gtk_tree_view_real_set_cursor (GTK_TREE_VIEW (tree_view), tmp_path, 0);
          gtk_tree_path_free (tmp_path);
        }

      _gtk_tree_selection_emit_changed (tree_view->priv->selection);

      gtk_css_node_set_parent (tree_view->priv->rubber_band_cssnode, NULL);
      tree_view->priv->rubber_band_cssnode = NULL;
    }

  /* Clear status variables */
  tree_view->priv->rubber_band_status = RUBBER_BAND_OFF;
  tree_view->priv->rubber_band_extend = FALSE;
  tree_view->priv->rubber_band_modify = FALSE;

  tree_view->priv->rubber_band_start_node = NULL;
  tree_view->priv->rubber_band_start_tree = NULL;
  tree_view->priv->rubber_band_end_node = NULL;
  tree_view->priv->rubber_band_end_tree = NULL;
}

static void
gtk_tree_view_update_rubber_band_selection_range (GtkTreeView  *tree_view,
						 GtkTreeRBTree *start_tree,
						 GtkTreeRBNode *start_node,
						 GtkTreeRBTree *end_tree,
						 GtkTreeRBNode *end_node,
						 gboolean       select,
						 gboolean       skip_start,
						 gboolean       skip_end)
{
  if (start_node == end_node)
    return;

  /* We skip the first node and jump inside the loop */
  if (skip_start)
    goto skip_first;

  do
    {
      /* Small optimization by assuming insensitive nodes are never
       * selected.
       */
      if (!GTK_TREE_RBNODE_FLAG_SET (start_node, GTK_TREE_RBNODE_IS_SELECTED))
        {
	  GtkTreePath *path;
	  gboolean selectable;

	  path = _gtk_tree_path_new_from_rbtree (start_tree, start_node);
	  selectable = _gtk_tree_selection_row_is_selectable (tree_view->priv->selection, start_node, path);
	  gtk_tree_path_free (path);

	  if (!selectable)
	    goto node_not_selectable;
	}

      if (select)
        {
	  if (tree_view->priv->rubber_band_extend)
            GTK_TREE_RBNODE_SET_FLAG (start_node, GTK_TREE_RBNODE_IS_SELECTED);
	  else if (tree_view->priv->rubber_band_modify)
	    {
	      /* Toggle the selection state */
	      if (GTK_TREE_RBNODE_FLAG_SET (start_node, GTK_TREE_RBNODE_IS_SELECTED))
		GTK_TREE_RBNODE_UNSET_FLAG (start_node, GTK_TREE_RBNODE_IS_SELECTED);
	      else
		GTK_TREE_RBNODE_SET_FLAG (start_node, GTK_TREE_RBNODE_IS_SELECTED);
	    }
	  else
	    GTK_TREE_RBNODE_SET_FLAG (start_node, GTK_TREE_RBNODE_IS_SELECTED);
	}
      else
        {
	  /* Mirror the above */
	  if (tree_view->priv->rubber_band_extend)
	    GTK_TREE_RBNODE_UNSET_FLAG (start_node, GTK_TREE_RBNODE_IS_SELECTED);
	  else if (tree_view->priv->rubber_band_modify)
	    {
	      /* Toggle the selection state */
	      if (GTK_TREE_RBNODE_FLAG_SET (start_node, GTK_TREE_RBNODE_IS_SELECTED))
		GTK_TREE_RBNODE_UNSET_FLAG (start_node, GTK_TREE_RBNODE_IS_SELECTED);
	      else
		GTK_TREE_RBNODE_SET_FLAG (start_node, GTK_TREE_RBNODE_IS_SELECTED);
	    }
	  else
	    GTK_TREE_RBNODE_UNSET_FLAG (start_node, GTK_TREE_RBNODE_IS_SELECTED);
	}

      gtk_widget_queue_draw (GTK_WIDGET (tree_view));

node_not_selectable:
      if (start_node == end_node)
	break;

skip_first:

      if (start_node->children)
        {
	  start_tree = start_node->children;
          start_node = gtk_tree_rbtree_first (start_tree);
	}
      else
        {
	  gtk_tree_rbtree_next_full (start_tree, start_node, &start_tree, &start_node);

	  if (!start_tree)
	    /* Ran out of tree */
	    break;
	}

      if (skip_end && start_node == end_node)
	break;
    }
  while (TRUE);
}

static void
gtk_tree_view_update_rubber_band_selection (GtkTreeView *tree_view)
{
  GtkTreeRBTree *start_tree, *end_tree;
  GtkTreeRBNode *start_node, *end_node;
  gdouble start_y, offset_y;
  gint bin_y;

  if (!gtk_gesture_is_active (tree_view->priv->drag_gesture))
    return;

  gtk_gesture_drag_get_offset (GTK_GESTURE_DRAG (tree_view->priv->drag_gesture),
                               NULL, &offset_y);
  gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (tree_view->priv->drag_gesture),
                                    NULL, &start_y);
  gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, 0, start_y,
						      NULL, &bin_y);
  bin_y = MAX (0, bin_y + offset_y + tree_view->priv->dy);

  gtk_tree_rbtree_find_offset (tree_view->priv->tree, MIN (tree_view->priv->press_start_y, bin_y), &start_tree, &start_node);
  gtk_tree_rbtree_find_offset (tree_view->priv->tree, MAX (tree_view->priv->press_start_y, bin_y), &end_tree, &end_node);

  /* Handle the start area first */
  if (!start_node && !end_node)
    {
      if (tree_view->priv->rubber_band_start_node)
        {
          GtkTreeRBNode *node = tree_view->priv->rubber_band_start_node;

	  if (tree_view->priv->rubber_band_modify)
	    {
	      /* Toggle the selection state */
	      if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_SELECTED))
		GTK_TREE_RBNODE_UNSET_FLAG (node, GTK_TREE_RBNODE_IS_SELECTED);
	      else
		GTK_TREE_RBNODE_SET_FLAG (node, GTK_TREE_RBNODE_IS_SELECTED);
	    }
          else
            GTK_TREE_RBNODE_UNSET_FLAG (node, GTK_TREE_RBNODE_IS_SELECTED);

          gtk_widget_queue_draw (GTK_WIDGET (tree_view));
        }
    }
  if (!tree_view->priv->rubber_band_start_node || !start_node)
    {
      gtk_tree_view_update_rubber_band_selection_range (tree_view,
						       start_tree,
						       start_node,
						       end_tree,
						       end_node,
						       TRUE,
						       FALSE,
						       FALSE);
    }
  else if (gtk_tree_rbtree_node_find_offset (start_tree, start_node) <
           gtk_tree_rbtree_node_find_offset (tree_view->priv->rubber_band_start_tree, tree_view->priv->rubber_band_start_node))
    {
      /* New node is above the old one; selection became bigger */
      gtk_tree_view_update_rubber_band_selection_range (tree_view,
						       start_tree,
						       start_node,
						       tree_view->priv->rubber_band_start_tree,
						       tree_view->priv->rubber_band_start_node,
						       TRUE,
						       FALSE,
						       TRUE);
    }
  else if (gtk_tree_rbtree_node_find_offset (start_tree, start_node) >
           gtk_tree_rbtree_node_find_offset (tree_view->priv->rubber_band_start_tree, tree_view->priv->rubber_band_start_node))
    {
      /* New node is below the old one; selection became smaller */
      gtk_tree_view_update_rubber_band_selection_range (tree_view,
						       tree_view->priv->rubber_band_start_tree,
						       tree_view->priv->rubber_band_start_node,
						       start_tree,
						       start_node,
						       FALSE,
						       FALSE,
						       TRUE);
    }

  tree_view->priv->rubber_band_start_tree = start_tree;
  tree_view->priv->rubber_band_start_node = start_node;

  /* Next, handle the end area */
  if (!tree_view->priv->rubber_band_end_node)
    {
      /* In the event this happens, start_node was also NULL; this case is
       * handled above.
       */
    }
  else if (!end_node)
    {
      /* Find the last node in the tree */
      gtk_tree_rbtree_find_offset (tree_view->priv->tree, gtk_tree_view_get_height (tree_view) - 1,
			       &end_tree, &end_node);

      /* Selection reached end of the tree */
      gtk_tree_view_update_rubber_band_selection_range (tree_view,
						       tree_view->priv->rubber_band_end_tree,
						       tree_view->priv->rubber_band_end_node,
						       end_tree,
						       end_node,
						       TRUE,
						       TRUE,
						       FALSE);
    }
  else if (gtk_tree_rbtree_node_find_offset (end_tree, end_node) >
           gtk_tree_rbtree_node_find_offset (tree_view->priv->rubber_band_end_tree, tree_view->priv->rubber_band_end_node))
    {
      /* New node is below the old one; selection became bigger */
      gtk_tree_view_update_rubber_band_selection_range (tree_view,
						       tree_view->priv->rubber_band_end_tree,
						       tree_view->priv->rubber_band_end_node,
						       end_tree,
						       end_node,
						       TRUE,
						       TRUE,
						       FALSE);
    }
  else if (gtk_tree_rbtree_node_find_offset (end_tree, end_node) <
           gtk_tree_rbtree_node_find_offset (tree_view->priv->rubber_band_end_tree, tree_view->priv->rubber_band_end_node))
    {
      /* New node is above the old one; selection became smaller */
      gtk_tree_view_update_rubber_band_selection_range (tree_view,
						       end_tree,
						       end_node,
						       tree_view->priv->rubber_band_end_tree,
						       tree_view->priv->rubber_band_end_node,
						       FALSE,
						       TRUE,
						       FALSE);
    }

  tree_view->priv->rubber_band_end_tree = end_tree;
  tree_view->priv->rubber_band_end_node = end_node;
}

static void
gtk_tree_view_update_rubber_band (GtkTreeView *tree_view)
{
  gdouble start_x, start_y, offset_x, offset_y, x, y;
  gint bin_x, bin_y;

  if (!gtk_gesture_is_recognized (tree_view->priv->drag_gesture))
    return;

  gtk_gesture_drag_get_offset (GTK_GESTURE_DRAG (tree_view->priv->drag_gesture),
                               &offset_x, &offset_y);
  gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (tree_view->priv->drag_gesture),
                                    &start_x, &start_y);
  gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, start_x, start_y,
						      &bin_x, &bin_y);
  bin_y += tree_view->priv->dy;

  x = MAX (bin_x + offset_x, 0);
  y = MAX (bin_y + offset_y, 0);

  gtk_widget_queue_draw (GTK_WIDGET (tree_view));

  tree_view->priv->rubber_band_x = x;
  tree_view->priv->rubber_band_y = y;

  gtk_tree_view_update_rubber_band_selection (tree_view);
}

static void
gtk_tree_view_snapshot_rubber_band (GtkTreeView *tree_view,
                                    GtkSnapshot *snapshot)
{
  gdouble start_x, start_y, offset_x, offset_y;
  GdkRectangle rect;
  GtkStyleContext *context;
  gint bin_x, bin_y;

  if (!gtk_gesture_is_recognized (tree_view->priv->drag_gesture))
    return;

  gtk_gesture_drag_get_offset (GTK_GESTURE_DRAG (tree_view->priv->drag_gesture),
                               &offset_x, &offset_y);
  gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (tree_view->priv->drag_gesture),
                                    &start_x, &start_y);
  gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, start_x, start_y,
						      &bin_x, &bin_y);
  bin_x = MAX (0, bin_x + offset_x);
  bin_y = MAX (0, bin_y + offset_y + tree_view->priv->dy);

  context = gtk_widget_get_style_context (GTK_WIDGET (tree_view));

  gtk_style_context_save_to_node (context, tree_view->priv->rubber_band_cssnode);

  rect.x = MIN (tree_view->priv->press_start_x, bin_x);
  rect.y = MIN (tree_view->priv->press_start_y, bin_y) - tree_view->priv->dy;
  rect.width = ABS (tree_view->priv->press_start_x - bin_x) + 1;
  rect.height = ABS (tree_view->priv->press_start_y - bin_y) + 1;

  gtk_snapshot_render_background (snapshot, context,
                                  rect.x, rect.y,
                                  rect.width, rect.height);
  gtk_snapshot_render_frame (snapshot, context,
                             rect.x, rect.y,
                             rect.width, rect.height);

  gtk_style_context_restore (context);
}

static void
gtk_tree_view_column_drag_gesture_update (GtkGestureDrag *gesture,
                                          gdouble         offset_x,
                                          gdouble         offset_y,
                                          GtkTreeView    *tree_view)
{
  gdouble start_x, start_y, x, y;
  GdkEventSequence *sequence;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (gtk_gesture_get_sequence_state (GTK_GESTURE (gesture), sequence) != GTK_EVENT_SEQUENCE_CLAIMED)
    return;

  gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);
  x = start_x + offset_x;
  y = start_y + offset_y;

  if (tree_view->priv->in_column_resize)
    gtk_tree_view_motion_resize_column (tree_view, x, y);
  else if (tree_view->priv->in_column_drag)
    gtk_tree_view_motion_drag_column (tree_view, x, y);
}

static void
gtk_tree_view_drag_gesture_update (GtkGestureDrag *gesture,
                                   gdouble         offset_x,
                                   gdouble         offset_y,
                                   GtkTreeView    *tree_view)
{
  if (tree_view->priv->tree == NULL)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  if (tree_view->priv->rubber_band_status == RUBBER_BAND_MAYBE_START)
    {
      GtkCssNode *widget_node;

      widget_node = gtk_widget_get_css_node (GTK_WIDGET (tree_view));
      tree_view->priv->rubber_band_cssnode = gtk_css_node_new ();
      gtk_css_node_set_name (tree_view->priv->rubber_band_cssnode, I_("rubberband"));
      gtk_css_node_set_parent (tree_view->priv->rubber_band_cssnode, widget_node);
      gtk_css_node_set_state (tree_view->priv->rubber_band_cssnode, gtk_css_node_get_state (widget_node));
      g_object_unref (tree_view->priv->rubber_band_cssnode);

      gtk_tree_view_update_rubber_band (tree_view);

      tree_view->priv->rubber_band_status = RUBBER_BAND_ACTIVE;
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
  else if (tree_view->priv->rubber_band_status == RUBBER_BAND_ACTIVE)
    {
      gtk_tree_view_update_rubber_band (tree_view);

      add_scroll_timeout (tree_view);
    }
  else if (!tree_view->priv->rubber_band_status)
    {
      if (gtk_tree_view_maybe_begin_dragging_row (tree_view))
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
    }
}

static void
gtk_tree_view_motion_controller_motion (GtkEventControllerMotion *controller,
                                        double                    x,
                                        double                    y,
                                        GtkTreeView              *tree_view)
{
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
  gint new_y;
  GList *list;
  gboolean cursor_set = FALSE;

  if (tree_view->priv->tree)
    {
      gint bin_x, bin_y;

      /* If we are currently pressing down a button, we don't want to prelight anything else. */
      if (gtk_gesture_is_active (tree_view->priv->drag_gesture) ||
          gtk_gesture_is_active (tree_view->priv->click_gesture))
        node = NULL;

      gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, x, y,
							  &bin_x, &bin_y);
      new_y = MAX (0, TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, bin_y));

      gtk_tree_rbtree_find_offset (tree_view->priv->tree, new_y, &tree, &node);

      tree_view->priv->event_last_x = bin_x;
      tree_view->priv->event_last_y = bin_y;
      prelight_or_select (tree_view, tree, node, bin_x, bin_y);
    }

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      GtkTreeViewColumn *column = list->data;

      if (_gtk_tree_view_column_coords_in_resize_rect (column, x, y))
        {
          gtk_widget_set_cursor_from_name (GTK_WIDGET (tree_view), "col-resize");
          cursor_set = TRUE;
          break;
        }
    }

  if (!cursor_set)
    gtk_widget_set_cursor (GTK_WIDGET (tree_view), NULL);
}

/* Invalidate the focus rectangle near the edge of the bin_window; used when
 * the tree is empty.
 */
static void
invalidate_empty_focus (GtkTreeView *tree_view)
{
  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return;

  gtk_widget_queue_draw (GTK_WIDGET (tree_view));
}

static void
gtk_tree_view_snapshot_grid_line (GtkTreeView            *tree_view,
                                  GtkSnapshot            *snapshot,
                                  GtkOrientation          orientation,
                                  const graphene_point_t *start,
                                  float                   size)
{
  GtkTreeViewPrivate *priv = gtk_tree_view_get_instance_private (tree_view);
  GtkStyleContext *context;
  const GdkRGBA *grid_line_color;

  context = gtk_widget_get_style_context (GTK_WIDGET (tree_view));
  grid_line_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context,
                                                                                    GTK_CSS_PROPERTY_BORDER_TOP_COLOR));

  if (!gdk_rgba_equal (grid_line_color, &priv->grid_line_color) ||
      (orientation == GTK_ORIENTATION_HORIZONTAL && !priv->horizontal_grid_line_texture) ||
      (orientation == GTK_ORIENTATION_VERTICAL && !priv->vertical_grid_line_texture))
    {
      cairo_surface_t *surface;
      unsigned char *data;

      g_clear_object (&priv->horizontal_grid_line_texture);
      g_clear_object (&priv->vertical_grid_line_texture);
      priv->grid_line_color = *grid_line_color;

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 2, 1);
      data = cairo_image_surface_get_data (surface);
      /* just color the first pixel... */
      data[0] = round (grid_line_color->blue  * 255);
      data[1] = round (grid_line_color->green * 255);
      data[2] = round (grid_line_color->red   * 255);
      data[3] = round (grid_line_color->alpha * 255);

      priv->horizontal_grid_line_texture = gdk_texture_new_for_surface (surface);
      cairo_surface_destroy (surface);

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 2);
      data = cairo_image_surface_get_data (surface);
      data[0] = round (grid_line_color->blue  * 255);
      data[1] = round (grid_line_color->green * 255);
      data[2] = round (grid_line_color->red   * 255);
      data[3] = round (grid_line_color->alpha * 255);

      priv->vertical_grid_line_texture = gdk_texture_new_for_surface (surface);
      cairo_surface_destroy (surface);
    }

  g_assert (priv->horizontal_grid_line_texture);
  g_assert (priv->vertical_grid_line_texture);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_snapshot_push_repeat (snapshot,
                                &GRAPHENE_RECT_INIT (
                                  start->x, start->y,
                                  size, 1),
                                NULL);
      gtk_snapshot_append_texture (snapshot, priv->horizontal_grid_line_texture,
                                   &GRAPHENE_RECT_INIT (0, 0, 2, 1));
      gtk_snapshot_pop (snapshot);
    }
  else /* VERTICAL */
    {
      gtk_snapshot_push_repeat (snapshot,
                                &GRAPHENE_RECT_INIT (
                                  start->x, start->y,
                                  1, size),
                                NULL);
      gtk_snapshot_append_texture (snapshot, priv->vertical_grid_line_texture,
                                   &GRAPHENE_RECT_INIT (0, 0, 1, 2));
      gtk_snapshot_pop (snapshot);
    }
}

static void
gtk_tree_view_snapshot_tree_line (GtkTreeView            *tree_view,
                                  GtkSnapshot            *snapshot,
                                  GtkOrientation          orientation,
                                  const graphene_point_t *start,
                                  float                   size)
{
  GtkTreeViewPrivate *priv = gtk_tree_view_get_instance_private (tree_view);
  GtkStyleContext *context;
  const GdkRGBA *tree_line_color;

  context = gtk_widget_get_style_context (GTK_WIDGET (tree_view));
  tree_line_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context,
                                                                                    GTK_CSS_PROPERTY_BORDER_LEFT_COLOR));

  if (!gdk_rgba_equal (tree_line_color, &priv->tree_line_color) ||
      (orientation == GTK_ORIENTATION_HORIZONTAL && !priv->horizontal_tree_line_texture) ||
      (orientation == GTK_ORIENTATION_VERTICAL && !priv->vertical_tree_line_texture))
    {
      cairo_surface_t *surface;
      unsigned char *data;

      g_clear_object (&priv->horizontal_tree_line_texture);
      g_clear_object (&priv->vertical_tree_line_texture);
      priv->tree_line_color = *tree_line_color;

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 2, 1);
      data = cairo_image_surface_get_data (surface);
      /* just color the first pixel... */
      data[0] = round (tree_line_color->blue  * 255);
      data[1] = round (tree_line_color->green * 255);
      data[2] = round (tree_line_color->red   * 255);
      data[3] = round (tree_line_color->alpha * 255);

      priv->horizontal_tree_line_texture = gdk_texture_new_for_surface (surface);
      cairo_surface_destroy (surface);

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 2);
      data = cairo_image_surface_get_data (surface);
      data[0] = round (tree_line_color->blue  * 255);
      data[1] = round (tree_line_color->green * 255);
      data[2] = round (tree_line_color->red   * 255);
      data[3] = round (tree_line_color->alpha * 255);

      priv->vertical_tree_line_texture = gdk_texture_new_for_surface (surface);
      cairo_surface_destroy (surface);
    }

  g_assert (priv->horizontal_tree_line_texture);
  g_assert (priv->vertical_tree_line_texture);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_snapshot_push_repeat (snapshot,
                                &GRAPHENE_RECT_INIT (
                                  start->x, start->y,
                                  size, 1),
                                NULL);
      gtk_snapshot_append_texture (snapshot, priv->horizontal_tree_line_texture,
                                   &GRAPHENE_RECT_INIT (0, 0, 2, 1));
      gtk_snapshot_pop (snapshot);
    }
  else /* VERTICAL */
    {
      gtk_snapshot_push_repeat (snapshot,
                                &GRAPHENE_RECT_INIT (
                                  start->x, start->y,
                                  1, size),
                                NULL);
      gtk_snapshot_append_texture (snapshot, priv->vertical_tree_line_texture,
                                   &GRAPHENE_RECT_INIT (0, 0, 1, 2));
      gtk_snapshot_pop (snapshot);
    }
}

static void
gtk_tree_view_snapshot_grid_lines (GtkTreeView *tree_view,
                                   GtkSnapshot *snapshot)
{
  GtkTreeViewPrivate *priv = gtk_tree_view_get_instance_private (tree_view);
  GList *list, *first, *last;
  gboolean rtl;
  int current_x = 0;
  int tree_view_height;

  if (priv->grid_lines != GTK_TREE_VIEW_GRID_LINES_VERTICAL &&
      priv->grid_lines != GTK_TREE_VIEW_GRID_LINES_BOTH)
    return;

  rtl = (_gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

  first = g_list_first (priv->columns);
  last = g_list_last (priv->columns);
  tree_view_height = gtk_tree_view_get_height (tree_view);

  for (list = (rtl ? last : first);
       list;
       list = (rtl ? list->prev : list->next))
    {
      GtkTreeViewColumn *column = list->data;

      /* We don't want a line for the last column */
      if (column == (rtl ? first->data : last->data))
        break;

      if (!gtk_tree_view_column_get_visible (column))
        continue;

      current_x += gtk_tree_view_column_get_width (column);

      gtk_tree_view_snapshot_grid_line (tree_view,
                                        snapshot,
                                        GTK_ORIENTATION_VERTICAL,
                                        &(graphene_point_t) { current_x - 1, 0 },
                                        tree_view_height);
    }
}

/* Warning: Very scary function.
 * Modify at your own risk
 *
 * KEEP IN SYNC WITH gtk_tree_view_create_row_drag_icon()!
 * FIXME: It’s not...
 */
static void
gtk_tree_view_bin_snapshot (GtkWidget   *widget,
			    GtkSnapshot *snapshot)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkTreeViewPrivate *priv = gtk_tree_view_get_instance_private (tree_view);
  const int x_scroll_offset = - gtk_adjustment_get_value (priv->hadjustment);
  GtkTreePath *path;
  GtkTreeRBTree *tree;
  GList *list;
  GtkTreeRBNode *node;
  GtkTreeRBNode *drag_highlight = NULL;
  GtkTreeRBTree *drag_highlight_tree = NULL;
  GtkTreeIter iter;
  gint new_y;
  gint y_offset, cell_offset;
  gint max_height;
  gint depth;
  GdkRectangle background_area;
  GdkRectangle cell_area;
  GdkRectangle clip;
  guint flags;
  gint bin_window_width;
  gint bin_window_height;
  GtkTreePath *drag_dest_path;
  GList *first_column, *last_column;
  gboolean has_can_focus_cell;
  gboolean rtl;
  gint n_visible_columns;
  gint expander_size;
  gboolean draw_vgrid_lines, draw_hgrid_lines;
  GtkStyleContext *context;
  gboolean parity;

  rtl = (_gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
  context = gtk_widget_get_style_context (widget);

  if (tree_view->priv->tree == NULL)
    return;

  bin_window_width = gtk_widget_get_width (GTK_WIDGET (tree_view));
  bin_window_height = gtk_widget_get_height(GTK_WIDGET (tree_view));

  clip = (GdkRectangle) { 0, 0, bin_window_width, bin_window_height };
  new_y = TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, clip.y);
  y_offset = -gtk_tree_rbtree_find_offset (tree_view->priv->tree, new_y, &tree, &node);

  if (gtk_tree_view_get_height (tree_view) < bin_window_height)
    {
      gtk_style_context_save (context);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_CELL);

      gtk_snapshot_render_background (snapshot, context,
                                      0, gtk_tree_view_get_height (tree_view),
                                      bin_window_width,
                                      bin_window_height - gtk_tree_view_get_height (tree_view));

      gtk_style_context_restore (context);
    }

  if (node == NULL)
    return;

  /* find the path for the node */
  path = _gtk_tree_path_new_from_rbtree (tree, node);
  gtk_tree_model_get_iter (tree_view->priv->model,
			   &iter,
			   path);
  depth = gtk_tree_path_get_depth (path);
  gtk_tree_path_free (path);
  
  drag_dest_path = NULL;

  if (tree_view->priv->drag_dest_row)
    drag_dest_path = gtk_tree_row_reference_get_path (tree_view->priv->drag_dest_row);

  if (drag_dest_path)
    _gtk_tree_view_find_node (tree_view, drag_dest_path,
                              &drag_highlight_tree, &drag_highlight);

  draw_vgrid_lines =
    tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_VERTICAL
    || tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_BOTH;
  draw_hgrid_lines =
    tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_HORIZONTAL
    || tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_BOTH;
  expander_size = gtk_tree_view_get_expander_size (tree_view);

  n_visible_columns = 0;
  for (list = tree_view->priv->columns; list; list = list->next)
    {
      if (!gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (list->data)))
	continue;
      n_visible_columns ++;
    }

  /* Find the last column */
  for (last_column = g_list_last (tree_view->priv->columns);
       last_column &&
       !(gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (last_column->data)));
       last_column = last_column->prev)
    ;

  /* and the first */
  for (first_column = g_list_first (tree_view->priv->columns);
       first_column &&
       !(gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (first_column->data)));
       first_column = first_column->next)
    ;

  /* Actually process the expose event.  To do this, we want to
   * start at the first node of the event, and walk the tree in
   * order, drawing each successive node.
   */
  
  parity = !(gtk_tree_rbtree_node_get_index (tree, node) % 2);

  do
    {
      gboolean is_separator = FALSE;
      gint n_col = 0;

      parity = !parity;
      is_separator = row_is_separator (tree_view, &iter, NULL);

      max_height = gtk_tree_view_get_row_height (tree_view, node);

      cell_offset = x_scroll_offset;

      background_area.y = y_offset + clip.y;
      background_area.height = max_height;

      flags = 0;

      if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_PRELIT))
	flags |= GTK_CELL_RENDERER_PRELIT;

      if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_SELECTED))
        flags |= GTK_CELL_RENDERER_SELECTED;

      /* we *need* to set cell data on all cells before the call
       * to _has_can_focus_cell, else _has_can_focus_cell() does not
       * return a correct value.
       */
      for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
	   list;
	   list = (rtl ? list->prev : list->next))
        {
	  GtkTreeViewColumn *column = list->data;
	  gtk_tree_view_column_cell_set_cell_data (column,
						   tree_view->priv->model,
						   &iter,
						   GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_PARENT),
						   node->children?TRUE:FALSE);
        }

      has_can_focus_cell = gtk_tree_view_has_can_focus_cell (tree_view);

      for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
	   list;
	   list = (rtl ? list->prev : list->next))
	{
	  GtkTreeViewColumn *column = list->data;
	  GtkStateFlags state = 0;
          gint width;
          gboolean draw_focus;

	  if (!gtk_tree_view_column_get_visible (column))
            continue;

          n_col++;
          width = gtk_tree_view_column_get_width (column);

	  if (cell_offset > clip.x + clip.width ||
	      cell_offset + width < clip.x)
	    {
	      cell_offset += width;
	      continue;
	    }

          if (gtk_tree_view_column_get_sort_indicator (column))
	    flags |= GTK_CELL_RENDERER_SORTED;
          else
            flags &= ~GTK_CELL_RENDERER_SORTED;

	  if (tree_view->priv->cursor_node == node)
            flags |= GTK_CELL_RENDERER_FOCUSED;
          else
            flags &= ~GTK_CELL_RENDERER_FOCUSED;

          if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_PARENT))
            flags |= GTK_CELL_RENDERER_EXPANDABLE;
          else
            flags &= ~GTK_CELL_RENDERER_EXPANDABLE;

          if (node->children)
            flags |= GTK_CELL_RENDERER_EXPANDED;
          else
            flags &= ~GTK_CELL_RENDERER_EXPANDED;

          background_area.x = cell_offset;
	  background_area.width = width;

          cell_area = background_area;
          cell_area.x += _TREE_VIEW_HORIZONTAL_SEPARATOR /2;
          cell_area.width -= _TREE_VIEW_HORIZONTAL_SEPARATOR;

	  if (draw_vgrid_lines)
	    {
	      if (list == first_column)
	        {
		  cell_area.width -= _TREE_VIEW_GRID_LINE_WIDTH / 2;
		}
	      else if (list == last_column)
	        {
		  cell_area.x += _TREE_VIEW_GRID_LINE_WIDTH / 2;
		  cell_area.width -= _TREE_VIEW_GRID_LINE_WIDTH / 2;
		}
	      else
	        {
	          cell_area.x += _TREE_VIEW_GRID_LINE_WIDTH / 2;
	          cell_area.width -= _TREE_VIEW_GRID_LINE_WIDTH;
		}
	    }

	  if (draw_hgrid_lines)
	    {
	      cell_area.y += _TREE_VIEW_GRID_LINE_WIDTH / 2;
	      cell_area.height -= _TREE_VIEW_GRID_LINE_WIDTH;
	    }

	  if (!gdk_rectangle_intersect (&clip, &background_area, NULL))
	    {
	      cell_offset += gtk_tree_view_column_get_width (column);
	      continue;
	    }

          background_area.x -= x_scroll_offset;
          cell_area.x -= x_scroll_offset;

	  gtk_tree_view_column_cell_set_cell_data (column,
						   tree_view->priv->model,
						   &iter,
						   GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_PARENT),
						   node->children?TRUE:FALSE);

          gtk_style_context_save (context);

          state = gtk_cell_renderer_get_state (NULL, widget, flags);
          gtk_style_context_set_state (context, state);

          gtk_style_context_add_class (context, GTK_STYLE_CLASS_CELL);

	  if (node == tree_view->priv->cursor_node && has_can_focus_cell
              && ((column == tree_view->priv->focus_column
                   && tree_view->priv->draw_keyfocus &&
                   gtk_widget_has_visible_focus (widget))
                  || (column == tree_view->priv->edited_column)))
            draw_focus = TRUE;
          else
            draw_focus = FALSE;

	  /* Draw background */
          gtk_snapshot_render_background (snapshot, context,
                                          background_area.x,
                                          background_area.y,
                                          background_area.width,
                                          background_area.height);

          /* Draw frame */
          gtk_snapshot_render_frame (snapshot, context,
                                     background_area.x,
                                     background_area.y,
                                     background_area.width,
                                     background_area.height);

	  if (gtk_tree_view_is_expander_column (tree_view, column))
	    {
	      if (!rtl)
		cell_area.x += (depth - 1) * tree_view->priv->level_indentation;
	      cell_area.width -= (depth - 1) * tree_view->priv->level_indentation;

              if (gtk_tree_view_draw_expanders (tree_view))
	        {
	          if (!rtl)
		    cell_area.x += depth * expander_size;
		  cell_area.width -= depth * expander_size;
		}

	      if (is_separator)
                {
                  GdkRGBA color;

                  gtk_style_context_save (context);
                  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SEPARATOR);

                  gtk_style_context_get_color (context, &color);
                  gtk_snapshot_append_color (snapshot,
                                             &color, 
                                             &GRAPHENE_RECT_INIT(
                                                 cell_area.x,
                                                 cell_area.y + cell_area.height / 2,
                                                 cell_area.x + cell_area.width,
                                                 1
                                             ));

                  gtk_style_context_restore (context);
                }
	      else
                {
                  gtk_tree_view_column_cell_snapshot (column,
                                                      snapshot,
                                                      &background_area,
                                                      &cell_area,
                                                      flags,
                                                      draw_focus);
                }

	      if (gtk_tree_view_draw_expanders (tree_view)
		  && (node->flags & GTK_TREE_RBNODE_IS_PARENT) == GTK_TREE_RBNODE_IS_PARENT)
		{
		  gtk_tree_view_snapshot_arrow (GTK_TREE_VIEW (widget),
                                                snapshot,
                                                tree,
                                                node);
		}
	    }
	  else
	    {
	      if (is_separator)
                {
                  GdkRGBA color;

                  gtk_style_context_save (context);
                  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SEPARATOR);

                  gtk_style_context_get_color (context, &color);
                  gtk_snapshot_append_color (snapshot,
                                             &color, 
                                             &GRAPHENE_RECT_INIT(
                                                 cell_area.x,
                                                 cell_area.y + cell_area.height / 2,
                                                 cell_area.x + cell_area.width,
                                                 1
                                             ));

                  gtk_style_context_restore (context);
                }
	      else
		gtk_tree_view_column_cell_snapshot (column,
                                                    snapshot,
                                                    &background_area,
                                                    &cell_area,
                                                    flags,
                                                    draw_focus);
	    }

          if (draw_hgrid_lines)
            {
              if (background_area.y >= clip.y)
                gtk_tree_view_snapshot_grid_line (tree_view,
                                                  snapshot,
                                                  GTK_ORIENTATION_HORIZONTAL,
                                                  &(graphene_point_t) {
                                                    background_area.x, background_area.y
                                                  },
                                                  background_area.width);

              if (background_area.y + max_height < clip.y + clip.height)
                gtk_tree_view_snapshot_grid_line (tree_view,
                                                  snapshot,
                                                  GTK_ORIENTATION_HORIZONTAL,
                                                  &(graphene_point_t) {
                                                    background_area.x, background_area.y + max_height
                                                  },
                                                  background_area.width);
            }

	  if (gtk_tree_view_is_expander_column (tree_view, column) &&
	      tree_view->priv->tree_lines_enabled)
	    {
	      gint x = background_area.x;
	      gint mult = rtl ? -1 : 1;
	      gint y0 = background_area.y;
	      gint y1 = background_area.y + background_area.height/2;
	      gint y2 = background_area.y + background_area.height;

              if (rtl)
                x += background_area.width - 1;

              if ((node->flags & GTK_TREE_RBNODE_IS_PARENT) == GTK_TREE_RBNODE_IS_PARENT &&
                  depth > 1)
                {
                  gtk_tree_view_snapshot_tree_line (tree_view,
                                                    snapshot,
                                                    GTK_ORIENTATION_HORIZONTAL,
                                                    &(graphene_point_t) {
                                                      x + expander_size * (depth - 1.5) * mult,
                                                      y1
                                                    },
                                                    mult * expander_size * 0.4);

                }
              else if (depth > 1)
                {
                  gtk_tree_view_snapshot_tree_line (tree_view,
                                                    snapshot,
                                                    GTK_ORIENTATION_HORIZONTAL,
                                                    &(graphene_point_t) {
                                                      x + expander_size * (depth - 1.5) * mult,
                                                      y1
                                                    },
                                                    mult * expander_size);
                }

              if (depth > 1)
                {
                  int i;
                  GtkTreeRBNode *tmp_node;
                  GtkTreeRBTree *tmp_tree;

                  if (!gtk_tree_rbtree_next (tree, node))
                    gtk_tree_view_snapshot_tree_line (tree_view,
                                                      snapshot,
                                                      GTK_ORIENTATION_VERTICAL,
                                                      &(graphene_point_t) {
                                                        x + expander_size * (depth - 1.5) * mult,
                                                        y0
                                                      },
                                                      y1 - y0);
                  else
                    gtk_tree_view_snapshot_tree_line (tree_view,
                                                      snapshot,
                                                      GTK_ORIENTATION_VERTICAL,
                                                      &(graphene_point_t) {
                                                        x + expander_size * (depth - 1.5) * mult,
                                                        y0
                                                      },
                                                      y2 - y0);

                  tmp_node = tree->parent_node;
                  tmp_tree = tree->parent_tree;

                  for (i = depth - 2; i > 0; i--)
                    {
                      if (gtk_tree_rbtree_next (tmp_tree, tmp_node))
                        gtk_tree_view_snapshot_tree_line (tree_view,
                                                          snapshot,
                                                          GTK_ORIENTATION_VERTICAL,
                                                          &(graphene_point_t) {
                                                            x + expander_size * (i - 0.5) * mult,
                                                            y0
                                                          },
                                                          y2 - y0);
                      tmp_node = tmp_tree->parent_node;
                      tmp_tree = tmp_tree->parent_tree;
                    }
                }
	    }

          gtk_style_context_restore (context);
	  cell_offset += gtk_tree_view_column_get_width (column);
	}

      if (node == drag_highlight)
        {
          /* Draw indicator for the drop
           */
	  GtkTreeRBTree *drag_tree = NULL;
	  GtkTreeRBNode *drag_node = NULL;

          gtk_style_context_save (context);
          gtk_style_context_set_state (context, gtk_style_context_get_state (context) | GTK_STATE_FLAG_DROP_ACTIVE);

          switch (tree_view->priv->drag_dest_pos)
            {
            case GTK_TREE_VIEW_DROP_BEFORE:
              gtk_style_context_add_class (context, "before");
              break;

            case GTK_TREE_VIEW_DROP_AFTER:
              gtk_style_context_add_class (context, "after");
              break;

            case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
            case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
              gtk_style_context_add_class (context, "into");
              break;

            default:
              break;
            }

          _gtk_tree_view_find_node (tree_view, drag_dest_path, &drag_tree, &drag_node);
          if (drag_tree != NULL)
             gtk_snapshot_render_frame (snapshot, context,
                                        0, gtk_tree_view_get_row_y_offset (tree_view, drag_tree, drag_node),
                                        bin_window_width,
                                        gtk_tree_view_get_row_height (tree_view, drag_node));

          gtk_style_context_restore (context);
        }

      /* draw the big row-spanning focus rectangle, if needed */
      if (!has_can_focus_cell && node == tree_view->priv->cursor_node &&
          tree_view->priv->draw_keyfocus &&
	  gtk_widget_has_visible_focus (widget))
        {
	  gint tmp_y, tmp_height;
	  GtkStateFlags focus_rect_state = 0;

          gtk_style_context_save (context);

          focus_rect_state = gtk_cell_renderer_get_state (NULL, widget, flags);
          gtk_style_context_set_state (context, focus_rect_state);

	  if (draw_hgrid_lines)
	    {
	      tmp_y = gtk_tree_view_get_row_y_offset (tree_view, tree, node) + _TREE_VIEW_GRID_LINE_WIDTH / 2;
              tmp_height = gtk_tree_view_get_row_height (tree_view, node) - _TREE_VIEW_GRID_LINE_WIDTH;
	    }
	  else
	    {
	      tmp_y = gtk_tree_view_get_row_y_offset (tree_view, tree, node);
              tmp_height = gtk_tree_view_get_row_height (tree_view, node);
	    }

          gtk_snapshot_render_focus (snapshot, context,
                                     0, tmp_y,
                                     bin_window_width,
                                     tmp_height);

          gtk_style_context_restore (context);
        }

      y_offset += max_height;
      if (node->children)
	{
	  GtkTreeIter parent = iter;
	  gboolean has_child;

	  tree = node->children;
          node = gtk_tree_rbtree_first (tree);

	  has_child = gtk_tree_model_iter_children (tree_view->priv->model,
						    &iter,
						    &parent);
	  depth++;

	  /* Sanity Check! */
	  TREE_VIEW_INTERNAL_ASSERT_VOID (has_child);
	}
      else
	{
	  gboolean done = FALSE;

	  do
	    {
	      node = gtk_tree_rbtree_next (tree, node);
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
		    /* we should go to done to free some memory */
		    goto done;
		  has_parent = gtk_tree_model_iter_parent (tree_view->priv->model,
							   &iter,
							   &parent_iter);
		  depth--;

		  /* Sanity check */
		  TREE_VIEW_INTERNAL_ASSERT_VOID (has_parent);
		}
	    }
	  while (!done);
	}
    }
  while (y_offset < clip.height);

done:
  gtk_tree_view_snapshot_grid_lines (tree_view, snapshot);

  if (tree_view->priv->rubber_band_status == RUBBER_BAND_ACTIVE)
    gtk_tree_view_snapshot_rubber_band (tree_view, snapshot);

  if (drag_dest_path)
    gtk_tree_path_free (drag_dest_path);
}

static void
gtk_tree_view_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkWidget *button;
  GtkStyleContext *context;
  GList *list;
  gint width, height;

  context = gtk_widget_get_style_context (widget);
  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT(
                              0, gtk_tree_view_get_effective_header_height (tree_view),
                              width,
                              height - gtk_tree_view_get_effective_header_height (tree_view)
                          ));

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (
                          - (gint) gtk_adjustment_get_value (tree_view->priv->hadjustment),
                          gtk_tree_view_get_effective_header_height (tree_view)));
  gtk_tree_view_bin_snapshot (widget, snapshot);
  gtk_snapshot_restore (snapshot);

  /* We can't just chain up to Container::draw as it will try to send the
   * event to the headers, so we handle propagating it to our children
   * (eg. widgets being edited) ourselves.
   */
  for (list = tree_view->priv->children; list; list = list->next)
    {
      GtkTreeViewChild *child = list->data;

      gtk_widget_snapshot_child (widget, child->widget, snapshot);
    }

  gtk_snapshot_pop (snapshot);
  
  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT(
                              0, 0,
                              width,
                              gtk_tree_view_get_effective_header_height (tree_view)
                          ));

  gtk_style_context_save (context);
  gtk_style_context_remove_class (context, GTK_STYLE_CLASS_VIEW);

  for (list = tree_view->priv->columns; list != NULL; list = list->next)
    {
      GtkTreeViewColumn *column = list->data;

      if (column == tree_view->priv->drag_column)
        continue;

      if (gtk_tree_view_column_get_visible (column))
        {
          button = gtk_tree_view_column_get_button (column);
          gtk_widget_snapshot_child (widget, button, snapshot);
        }
    }

  if (tree_view->priv->drag_column)
    {
      button = gtk_tree_view_column_get_button (tree_view->priv->drag_column);
      gtk_widget_snapshot_child (widget, button, snapshot);
    }

  gtk_style_context_restore (context);

  gtk_snapshot_pop (snapshot);
}

enum
{
  DROP_HOME,
  DROP_RIGHT,
  DROP_LEFT,
  DROP_END
};

/* returns 0x1 when no column has been found -- yes it's hackish */
static GtkTreeViewColumn *
gtk_tree_view_get_drop_column (GtkTreeView       *tree_view,
			       GtkTreeViewColumn *column,
			       gint               drop_position)
{
  GtkTreeViewColumn *left_column = NULL;
  GtkTreeViewColumn *cur_column = NULL;
  GList *tmp_list;

  if (!gtk_tree_view_column_get_reorderable (column))
    return (GtkTreeViewColumn *)0x1;

  switch (drop_position)
    {
      case DROP_HOME:
	/* find first column where we can drop */
	tmp_list = tree_view->priv->columns;
	if (column == GTK_TREE_VIEW_COLUMN (tmp_list->data))
	  return (GtkTreeViewColumn *)0x1;

	while (tmp_list)
	  {
	    g_assert (tmp_list);

	    cur_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);
	    tmp_list = tmp_list->next;

	    if (left_column &&
                gtk_tree_view_column_get_visible (left_column) == FALSE)
	      continue;

	    if (!tree_view->priv->column_drop_func)
	      return left_column;

	    if (!tree_view->priv->column_drop_func (tree_view, column, left_column, cur_column, tree_view->priv->column_drop_func_data))
	      {
		left_column = cur_column;
		continue;
	      }

	    return left_column;
	  }

	if (!tree_view->priv->column_drop_func)
	  return left_column;

	if (tree_view->priv->column_drop_func (tree_view, column, left_column, NULL, tree_view->priv->column_drop_func_data))
	  return left_column;
	else
	  return (GtkTreeViewColumn *)0x1;
	break;

      case DROP_RIGHT:
	/* find first column after column where we can drop */
	tmp_list = tree_view->priv->columns;

	for (; tmp_list; tmp_list = tmp_list->next)
	  if (GTK_TREE_VIEW_COLUMN (tmp_list->data) == column)
	    break;

	if (!tmp_list || !tmp_list->next)
	  return (GtkTreeViewColumn *)0x1;

	tmp_list = tmp_list->next;
	left_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);
	tmp_list = tmp_list->next;

	while (tmp_list)
	  {
	    g_assert (tmp_list);

	    cur_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);
	    tmp_list = tmp_list->next;

	    if (left_column &&
                gtk_tree_view_column_get_visible (left_column) == FALSE)
	      {
		left_column = cur_column;
		if (tmp_list)
		  tmp_list = tmp_list->next;
	        continue;
	      }

	    if (!tree_view->priv->column_drop_func)
	      return left_column;

	    if (!tree_view->priv->column_drop_func (tree_view, column, left_column, cur_column, tree_view->priv->column_drop_func_data))
	      {
		left_column = cur_column;
		continue;
	      }

	    return left_column;
	  }

	if (!tree_view->priv->column_drop_func)
	  return left_column;

	if (tree_view->priv->column_drop_func (tree_view, column, left_column, NULL, tree_view->priv->column_drop_func_data))
	  return left_column;
	else
	  return (GtkTreeViewColumn *)0x1;
	break;

      case DROP_LEFT:
	/* find first column before column where we can drop */
	tmp_list = tree_view->priv->columns;

	for (; tmp_list; tmp_list = tmp_list->next)
	  if (GTK_TREE_VIEW_COLUMN (tmp_list->data) == column)
	    break;

	if (!tmp_list || !tmp_list->prev)
	  return (GtkTreeViewColumn *)0x1;

	tmp_list = tmp_list->prev;
	cur_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);
	tmp_list = tmp_list->prev;

	while (tmp_list)
	  {
	    g_assert (tmp_list);

	    left_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);

	    if (left_column &&
                gtk_tree_view_column_get_visible (left_column) == FALSE)
	      {
		/*if (!tmp_list->prev)
		  return (GtkTreeViewColumn *)0x1;
		  */
/*
		cur_column = GTK_TREE_VIEW_COLUMN (tmp_list->prev->data);
		tmp_list = tmp_list->prev->prev;
		continue;*/

		cur_column = left_column;
		if (tmp_list)
		  tmp_list = tmp_list->prev;
		continue;
	      }

	    if (!tree_view->priv->column_drop_func)
	      return left_column;

	    if (tree_view->priv->column_drop_func (tree_view, column, left_column, cur_column, tree_view->priv->column_drop_func_data))
	      return left_column;

	    cur_column = left_column;
	    tmp_list = tmp_list->prev;
	  }

	if (!tree_view->priv->column_drop_func)
	  return NULL;

	if (tree_view->priv->column_drop_func (tree_view, column, NULL, cur_column, tree_view->priv->column_drop_func_data))
	  return NULL;
	else
	  return (GtkTreeViewColumn *)0x1;
	break;

      case DROP_END:
	/* same as DROP_HOME case, but doing it backwards */
	tmp_list = g_list_last (tree_view->priv->columns);
	cur_column = NULL;

	if (column == GTK_TREE_VIEW_COLUMN (tmp_list->data))
	  return (GtkTreeViewColumn *)0x1;

	while (tmp_list)
	  {
	    g_assert (tmp_list);

	    left_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);

	    if (left_column &&
                gtk_tree_view_column_get_visible (left_column) == FALSE)
	      {
		cur_column = left_column;
		tmp_list = tmp_list->prev;
	      }

	    if (!tree_view->priv->column_drop_func)
	      return left_column;

	    if (tree_view->priv->column_drop_func (tree_view, column, left_column, cur_column, tree_view->priv->column_drop_func_data))
	      return left_column;

	    cur_column = left_column;
	    tmp_list = tmp_list->prev;
	  }

	if (!tree_view->priv->column_drop_func)
	  return NULL;

	if (tree_view->priv->column_drop_func (tree_view, column, NULL, cur_column, tree_view->priv->column_drop_func_data))
	  return NULL;
	else
	  return (GtkTreeViewColumn *)0x1;
	break;

      default:
        return (GtkTreeViewColumn *)0x1;
        break;
    }
}

static gboolean
gtk_tree_view_search_key_cancels_search (guint keyval)
{
  return keyval == GDK_KEY_Escape
      || keyval == GDK_KEY_Tab
      || keyval == GDK_KEY_KP_Tab
      || keyval == GDK_KEY_ISO_Left_Tab;
}

static gboolean
no_popup (void)
{
  return TRUE;
}

static gboolean
gtk_tree_view_key_controller_key_pressed (GtkEventControllerKey *key,
                                          guint                  keyval,
                                          guint                  keycode,
                                          GdkModifierType        state,
                                          GtkTreeView           *tree_view)
{
  GtkWidget *widget = GTK_WIDGET (tree_view);
  GtkWidget *button;
  GdkEvent *event;

  if (tree_view->priv->rubber_band_status)
    {
      if (keyval == GDK_KEY_Escape)
	gtk_tree_view_stop_rubber_band (tree_view);

      return TRUE;
    }

  if (tree_view->priv->in_column_drag)
    {
      if (keyval == GDK_KEY_Escape)
        gtk_gesture_set_state (GTK_GESTURE (tree_view->priv->column_drag_gesture),
                               GTK_EVENT_SEQUENCE_DENIED);
      return TRUE;
    }

  if (tree_view->priv->headers_visible)
    {
      GList *focus_column;
      gboolean rtl;

      rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

      for (focus_column = tree_view->priv->columns;
           focus_column;
           focus_column = focus_column->next)
        {
          GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN (focus_column->data);
	  
	  button = gtk_tree_view_column_get_button (column);
          if (gtk_widget_has_focus (button))
            break;
        }

      if (focus_column &&
          (state & GDK_SHIFT_MASK) && (state & GDK_MOD1_MASK) &&
          (keyval == GDK_KEY_Left || keyval == GDK_KEY_KP_Left
           || keyval == GDK_KEY_Right || keyval == GDK_KEY_KP_Right))
        {
          GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN (focus_column->data);
          gint column_width;

          if (!gtk_tree_view_column_get_resizable (column))
            {
              gtk_widget_error_bell (widget);
              return TRUE;
            }

	  column_width = gtk_tree_view_column_get_width (column);

          if (keyval == (rtl ? GDK_KEY_Right : GDK_KEY_Left)
              || keyval == (rtl ? GDK_KEY_KP_Right : GDK_KEY_KP_Left))
            {
	      column_width = MAX (column_width - 2, 0);
            }
          else if (keyval == (rtl ? GDK_KEY_Left : GDK_KEY_Right)
                   || keyval == (rtl ? GDK_KEY_KP_Left : GDK_KEY_KP_Right))
            {
	      column_width = column_width + 2;
            }

	  gtk_tree_view_column_set_fixed_width (column, column_width);
	  gtk_tree_view_column_set_expand (column, FALSE);
          return TRUE;
        }

      if (focus_column &&
          (state & GDK_MOD1_MASK) &&
          (keyval == GDK_KEY_Left || keyval == GDK_KEY_KP_Left
           || keyval == GDK_KEY_Right || keyval == GDK_KEY_KP_Right
           || keyval == GDK_KEY_Home || keyval == GDK_KEY_KP_Home
           || keyval == GDK_KEY_End || keyval == GDK_KEY_KP_End))
        {
          GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN (focus_column->data);

          if (keyval == (rtl ? GDK_KEY_Right : GDK_KEY_Left)
              || keyval == (rtl ? GDK_KEY_KP_Right : GDK_KEY_KP_Left))
            {
              GtkTreeViewColumn *col;
              col = gtk_tree_view_get_drop_column (tree_view, column, DROP_LEFT);
              if (col != (GtkTreeViewColumn *)0x1)
                gtk_tree_view_move_column_after (tree_view, column, col);
              else
                gtk_widget_error_bell (widget);
            }
          else if (keyval == (rtl ? GDK_KEY_Left : GDK_KEY_Right)
                   || keyval == (rtl ? GDK_KEY_KP_Left : GDK_KEY_KP_Right))
            {
              GtkTreeViewColumn *col;
              col = gtk_tree_view_get_drop_column (tree_view, column, DROP_RIGHT);
              if (col != (GtkTreeViewColumn *)0x1)
                gtk_tree_view_move_column_after (tree_view, column, col);
              else
                gtk_widget_error_bell (widget);
            }
          else if (keyval == GDK_KEY_Home || keyval == GDK_KEY_KP_Home)
            {
              GtkTreeViewColumn *col;
              col = gtk_tree_view_get_drop_column (tree_view, column, DROP_HOME);
              if (col != (GtkTreeViewColumn *)0x1)
                gtk_tree_view_move_column_after (tree_view, column, col);
              else
                gtk_widget_error_bell (widget);
            }
          else if (keyval == GDK_KEY_End || keyval == GDK_KEY_KP_End)
            {
              GtkTreeViewColumn *col;
              col = gtk_tree_view_get_drop_column (tree_view, column, DROP_END);
              if (col != (GtkTreeViewColumn *)0x1)
                gtk_tree_view_move_column_after (tree_view, column, col);
              else
                gtk_widget_error_bell (widget);
            }

          return TRUE;
        }
    }

  /* Handle the keybindings. */
  event = gtk_get_current_event ();
  if (gtk_bindings_activate_event (G_OBJECT (widget), (GdkEventKey *)event))
    {
      g_object_unref (event);
      return TRUE;
    }

  g_object_unref (event);

  if (tree_view->priv->search_entry_avoid_unhandled_binding)
    {
      tree_view->priv->search_entry_avoid_unhandled_binding = FALSE;
      return FALSE;
    }

  /* Initially, before the search window is visible, we pass the event to the
   * IM context of the search entry box. If it triggers a commit or a preedit,
   * we then show the search window without loosing tree view focus.
   * If the seach window is already visible, we forward the events to it,
   * keeping the focus on the tree view.
   */
  if (gtk_widget_has_focus (GTK_WIDGET (tree_view))
      && tree_view->priv->enable_search
      && !tree_view->priv->search_custom_entry_set
      && !gtk_tree_view_search_key_cancels_search (keyval))
    {
      GtkWidget *search_window;

      gtk_tree_view_ensure_interactive_directory (tree_view);

      search_window = tree_view->priv->search_window;
      if (!gtk_widget_is_visible (search_window))
        {
          tree_view->priv->imcontext_changed = FALSE;
          gtk_event_controller_key_forward (key, tree_view->priv->search_entry);

          if (tree_view->priv->imcontext_changed)
            {
              GdkDevice *device;

              device = gtk_get_current_event_device ();
              if (gtk_tree_view_real_start_interactive_search (tree_view,
                                                               device,
                                                               FALSE))
                {
                  gtk_widget_grab_focus (GTK_WIDGET (tree_view));
                  return TRUE;
                }
              else
                {
                  gtk_editable_set_text (GTK_EDITABLE (tree_view->priv->search_entry), "");
                  return FALSE;
                }
            }
        }
      else
        {
          gulong popup_menu_id;

          popup_menu_id = g_signal_connect (tree_view->priv->search_entry,
                                            "popup-menu", G_CALLBACK (no_popup),
                                            NULL);

          /* Because we keep the focus on the treeview, we need to forward the
           * key events to the entry, when it is visible. */
          gtk_event_controller_key_forward (key, search_window);

          g_signal_handler_disconnect (tree_view->priv->search_entry,
                                       popup_menu_id);
        }
    }

  return FALSE;
}

static void
gtk_tree_view_key_controller_key_released (GtkEventControllerKey *key,
                                           guint                  keyval,
                                           guint                  keycode,
                                           GdkModifierType        state,
                                           GtkTreeView           *tree_view)
{
  GdkEvent *event;

  if (tree_view->priv->rubber_band_status)
    return;

  /* Handle the keybindings. */
  event = gtk_get_current_event ();
  gtk_bindings_activate_event (G_OBJECT (tree_view), (GdkEventKey *)event);
  g_object_unref (event);
}

static void
gtk_tree_view_motion_controller_enter (GtkEventControllerMotion *controller,
                                       double                    x,
                                       double                    y,
                                       GdkCrossingMode           mode,
                                       GdkNotifyType             detail,
                                       GtkTreeView              *tree_view)
{
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
  gint new_y;

  if (tree_view->priv->tree == NULL)
    return;

  /* find the node internally */
  new_y = TREE_WINDOW_Y_TO_RBTREE_Y(tree_view, y);
  if (new_y < 0)
    new_y = 0;
  gtk_tree_rbtree_find_offset (tree_view->priv->tree, new_y, &tree, &node);

  tree_view->priv->event_last_x = x;
  tree_view->priv->event_last_y = y;

  if ((tree_view->priv->button_pressed_node == NULL) ||
      (tree_view->priv->button_pressed_node == node))
    prelight_or_select (tree_view, tree, node, x, y);
}

static void
gtk_tree_view_motion_controller_leave (GtkEventControllerMotion *controller,
                                       GdkCrossingMode           mode,
                                       GdkNotifyType             detail,
                                       GtkTreeView              *tree_view)
{
  gboolean is_focus, contains_focus;

  if (tree_view->priv->prelight_node)
    gtk_widget_queue_draw (GTK_WIDGET (tree_view));

  tree_view->priv->event_last_x = -10000;
  tree_view->priv->event_last_y = -10000;

  g_object_get (controller,
                "is-pointer-focus", &is_focus,
                "contains-pointer-focus", &contains_focus,
                NULL);

  if (!is_focus && !contains_focus)
    prelight_or_select (tree_view, NULL, NULL, -1000, -1000); /* not possibly over an arrow */
}

static void
gtk_tree_view_key_controller_focus_out (GtkEventControllerKey *key,
                                        GdkCrossingMode        mode,
                                        GdkNotifyType          detail,
                                        GtkTreeView           *tree_view)
{
  gboolean is_focus, contains_focus;

  gtk_widget_queue_draw (GTK_WIDGET (tree_view));

  g_object_get (key,
                "is-focus", &is_focus,
                "contains-focus", &contains_focus,
                NULL);

  /* destroy interactive search dialog */
  if (tree_view->priv->search_window &&
      !gtk_event_controller_key_contains_focus (key))
    gtk_tree_view_search_window_hide (tree_view->priv->search_window, tree_view,
                                      gtk_get_current_event_device ());
}

/* Incremental Reflow
 */

static gboolean
node_is_visible (GtkTreeView   *tree_view,
		 GtkTreeRBTree *tree,
		 GtkTreeRBNode *node)
{
  int y;
  int height;

  y = gtk_tree_rbtree_node_find_offset (tree, node);
  height = gtk_tree_view_get_row_height (tree_view, node);

  if (y >= gtk_adjustment_get_value (tree_view->priv->vadjustment) &&
      y + height <= (gtk_adjustment_get_value (tree_view->priv->vadjustment)
	             + gtk_adjustment_get_page_size (tree_view->priv->vadjustment)))
    return TRUE;

  return FALSE;
}

static gint
get_separator_height (GtkTreeView *tree_view)
{
  GtkStyleContext *context;
  GtkCssStyle *style;
  gdouble d;
  gint min_size;

  context = gtk_widget_get_style_context (GTK_WIDGET (tree_view));
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SEPARATOR);

  style = gtk_style_context_lookup_style (context);
  d = _gtk_css_number_value_get
    (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MIN_HEIGHT), 100);

  if (d < 1)
    min_size = ceil (d);
  else
    min_size = floor (d);

  gtk_style_context_restore (context);

  return min_size;
}

/* Returns TRUE if it updated the size
 */
static gboolean
validate_row (GtkTreeView   *tree_view,
	      GtkTreeRBTree *tree,
	      GtkTreeRBNode *node,
	      GtkTreeIter   *iter,
	      GtkTreePath *path)
{
  GtkTreeViewColumn *column;
  GtkStyleContext *context;
  GList *list, *first_column, *last_column;
  gint height = 0;
  gint depth = gtk_tree_path_get_depth (path);
  gboolean retval = FALSE;
  gboolean is_separator = FALSE;
  gboolean draw_vgrid_lines, draw_hgrid_lines;
  gint expander_size;
  int separator_height;

  /* double check the row needs validating */
  if (! GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID) &&
      ! GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID))
    return FALSE;

  is_separator = row_is_separator (tree_view, iter, NULL);

  draw_vgrid_lines =
    tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_VERTICAL
    || tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_BOTH;
  draw_hgrid_lines =
    tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_HORIZONTAL
    || tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_BOTH;
  expander_size = gtk_tree_view_get_expander_size (tree_view);

  for (last_column = g_list_last (tree_view->priv->columns);
       last_column &&
       !(gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (last_column->data)));
       last_column = last_column->prev)
    ;

  for (first_column = g_list_first (tree_view->priv->columns);
       first_column &&
       !(gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (first_column->data)));
       first_column = first_column->next)
    ;

  separator_height = get_separator_height (tree_view);

  context = gtk_widget_get_style_context (GTK_WIDGET (tree_view));
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_CELL);

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      gint padding = 0;
      gint original_width;
      gint new_width;
      gint row_height;

      column = list->data;

      if (!gtk_tree_view_column_get_visible (column))
	continue;

      if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID) && 
	  !_gtk_tree_view_column_cell_get_dirty (column))
	continue;

      original_width = _gtk_tree_view_column_get_requested_width (column);

      gtk_tree_view_column_cell_set_cell_data (column, tree_view->priv->model, iter,
					       GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_PARENT),
					       node->children?TRUE:FALSE);
      gtk_tree_view_column_cell_get_size (column,
                                          NULL, NULL,
                                          NULL, &row_height);

      if (is_separator)
        {
          height = separator_height;
          /* gtk_tree_view_get_row_height() assumes separator nodes are > 0 */
          height = MAX (height, 1);
        }
      else
        {
          height = MAX (height, row_height);
          height = MAX (height, expander_size);
        }

      if (gtk_tree_view_is_expander_column (tree_view, column))
        {
	  padding += _TREE_VIEW_HORIZONTAL_SEPARATOR + (depth - 1) * tree_view->priv->level_indentation;

	  if (gtk_tree_view_draw_expanders (tree_view))
	    padding += depth * expander_size;
	}
      else
        padding += _TREE_VIEW_HORIZONTAL_SEPARATOR;

      if (draw_vgrid_lines)
        {
	  if (list->data == first_column || list->data == last_column)
	    padding += _TREE_VIEW_GRID_LINE_WIDTH / 2.0;
	  else
	    padding += _TREE_VIEW_GRID_LINE_WIDTH;
	}

      /* Update the padding for the column */
      _gtk_tree_view_column_push_padding (column, padding);
      new_width = _gtk_tree_view_column_get_requested_width (column);

      if (new_width > original_width)
	retval = TRUE;
    }

  gtk_style_context_restore (context);

  if (draw_hgrid_lines)
    height += _TREE_VIEW_GRID_LINE_WIDTH;

  if (height != GTK_TREE_RBNODE_GET_HEIGHT (node))
    {
      retval = TRUE;
      gtk_tree_rbtree_node_set_height (tree, node, height);
    }
  gtk_tree_rbtree_node_mark_valid (tree, node);

  return retval;
}


static void
validate_visible_area (GtkTreeView *tree_view)
{
  GtkTreePath *path = NULL;
  GtkTreePath *above_path = NULL;
  GtkTreeIter iter;
  GtkTreeRBTree *tree = NULL;
  GtkTreeRBNode *node = NULL;
  gboolean need_redraw = FALSE;
  gboolean size_changed = FALSE;
  gint total_height;
  gint area_above = 0;
  gint area_below = 0;

  if (tree_view->priv->tree == NULL)
    return;

  if (! GTK_TREE_RBNODE_FLAG_SET (tree_view->priv->tree->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID) &&
      tree_view->priv->scroll_to_path == NULL)
    return;

  total_height = gtk_widget_get_height (GTK_WIDGET (tree_view))
                 - gtk_tree_view_get_effective_header_height (tree_view);

  if (total_height == 0)
    return;

  /* First, we check to see if we need to scroll anywhere
   */
  if (tree_view->priv->scroll_to_path)
    {
      path = gtk_tree_row_reference_get_path (tree_view->priv->scroll_to_path);
      if (path && !_gtk_tree_view_find_node (tree_view, path, &tree, &node))
	{
          /* we are going to scroll, and will update dy */
	  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
	  if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID) ||
	      GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID))
	    {
              gtk_widget_queue_draw (GTK_WIDGET (tree_view));
	      if (validate_row (tree_view, tree, node, &iter, path))
		size_changed = TRUE;
	    }

	  if (tree_view->priv->scroll_to_use_align)
	    {
	      gint height = gtk_tree_view_get_row_height (tree_view, node);
	      area_above = (total_height - height) *
		tree_view->priv->scroll_to_row_align;
	      area_below = total_height - area_above - height;
	      area_above = MAX (area_above, 0);
	      area_below = MAX (area_below, 0);
	    }
	  else
	    {
	      /* two cases:
	       * 1) row not visible
	       * 2) row visible
	       */
	      gint dy;
	      gint height = gtk_tree_view_get_row_height (tree_view, node);

	      dy = gtk_tree_rbtree_node_find_offset (tree, node);

	      if (dy >= gtk_adjustment_get_value (tree_view->priv->vadjustment) &&
		  dy + height <= (gtk_adjustment_get_value (tree_view->priv->vadjustment)
		                  + gtk_adjustment_get_page_size (tree_view->priv->vadjustment)))
	        {
		  /* row visible: keep the row at the same position */
		  area_above = dy - gtk_adjustment_get_value (tree_view->priv->vadjustment);
		  area_below = (gtk_adjustment_get_value (tree_view->priv->vadjustment) +
		                gtk_adjustment_get_page_size (tree_view->priv->vadjustment))
		               - dy - height;
		}
	      else
	        {
		  /* row not visible */
		  if (dy >= 0
		      && dy + height <= gtk_adjustment_get_page_size (tree_view->priv->vadjustment))
		    {
		      /* row at the beginning -- fixed */
		      area_above = dy;
		      area_below = gtk_adjustment_get_page_size (tree_view->priv->vadjustment)
				   - area_above - height;
		    }
		  else if (dy >= (gtk_adjustment_get_upper (tree_view->priv->vadjustment) -
			          gtk_adjustment_get_page_size (tree_view->priv->vadjustment)))
		    {
		      /* row at the end -- fixed */
		      area_above = dy - (gtk_adjustment_get_upper (tree_view->priv->vadjustment) -
			           gtk_adjustment_get_page_size (tree_view->priv->vadjustment));
                      area_below = gtk_adjustment_get_page_size (tree_view->priv->vadjustment) -
                                   area_above - height;

                      if (area_below < 0)
                        {
			  area_above = gtk_adjustment_get_page_size (tree_view->priv->vadjustment) - height;
                          area_below = 0;
                        }
		    }
		  else
		    {
		      /* row somewhere in the middle, bring it to the top
		       * of the view
		       */
		      area_above = 0;
		      area_below = total_height - height;
		    }
		}
	    }
	}
      else
	/* the scroll to isn't valid; ignore it.
	 */
	{
	  if (tree_view->priv->scroll_to_path && !path)
	    {
	      gtk_tree_row_reference_free (tree_view->priv->scroll_to_path);
	      tree_view->priv->scroll_to_path = NULL;
	    }
	  if (path)
	    gtk_tree_path_free (path);
	  path = NULL;
	}      
    }

  /* We didn't have a scroll_to set, so we just handle things normally
   */
  if (path == NULL)
    {
      gint offset;

      offset = gtk_tree_rbtree_find_offset (tree_view->priv->tree,
					TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, 0),
					&tree, &node);
      if (node == NULL)
	{
	  /* In this case, nothing has been validated */
	  path = gtk_tree_path_new_first ();
	  _gtk_tree_view_find_node (tree_view, path, &tree, &node);
	}
      else
	{
	  path = _gtk_tree_path_new_from_rbtree (tree, node);
	  total_height += offset;
	}

      gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);

      if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID) ||
	  GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID))
	{
          gtk_widget_queue_draw (GTK_WIDGET (tree_view));
	  if (validate_row (tree_view, tree, node, &iter, path))
	    size_changed = TRUE;
	}
      area_above = 0;
      area_below = total_height - gtk_tree_view_get_row_height (tree_view, node);
    }

  above_path = gtk_tree_path_copy (path);

  /* if we do not validate any row above the new top_row, we will make sure
   * that the row immediately above top_row has been validated. (if we do not
   * do this, gtk_tree_rbtree_find_offset will find the row above top_row, because
   * when invalidated that row's height will be zero. and this will mess up
   * scrolling).
   */
  if (area_above == 0)
    {
      GtkTreeRBTree *tmptree;
      GtkTreeRBNode *tmpnode;

      _gtk_tree_view_find_node (tree_view, above_path, &tmptree, &tmpnode);
      gtk_tree_rbtree_prev_full (tmptree, tmpnode, &tmptree, &tmpnode);

      if (tmpnode)
        {
	  GtkTreePath *tmppath;
	  GtkTreeIter tmpiter;

	  tmppath = _gtk_tree_path_new_from_rbtree (tmptree, tmpnode);
	  gtk_tree_model_get_iter (tree_view->priv->model, &tmpiter, tmppath);

	  if (GTK_TREE_RBNODE_FLAG_SET (tmpnode, GTK_TREE_RBNODE_INVALID) ||
	      GTK_TREE_RBNODE_FLAG_SET (tmpnode, GTK_TREE_RBNODE_COLUMN_INVALID))
	    {
              gtk_widget_queue_draw (GTK_WIDGET (tree_view));
	      if (validate_row (tree_view, tmptree, tmpnode, &tmpiter, tmppath))
		size_changed = TRUE;
	    }

	  gtk_tree_path_free (tmppath);
	}
    }

  /* Now, we walk forwards and backwards, measuring rows. Unfortunately,
   * backwards is much slower then forward, as there is no iter_prev function.
   * We go forwards first in case we run out of tree.  Then we go backwards to
   * fill out the top.
   */
  while (node && area_below > 0)
    {
      if (node->children)
	{
	  GtkTreeIter parent = iter;
	  gboolean has_child;

	  tree = node->children;
          node = gtk_tree_rbtree_first (tree);

	  has_child = gtk_tree_model_iter_children (tree_view->priv->model,
						    &iter,
						    &parent);
	  TREE_VIEW_INTERNAL_ASSERT_VOID (has_child);
	  gtk_tree_path_down (path);
	}
      else
	{
	  gboolean done = FALSE;
	  do
	    {
	      node = gtk_tree_rbtree_next (tree, node);
	      if (node != NULL)
		{
		  gboolean has_next = gtk_tree_model_iter_next (tree_view->priv->model, &iter);
		  done = TRUE;
		  gtk_tree_path_next (path);

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
		  gtk_tree_path_up (path);

		  /* Sanity check */
		  TREE_VIEW_INTERNAL_ASSERT_VOID (has_parent);
		}
	    }
	  while (!done);
	}

      if (!node)
        break;

      if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID) ||
	  GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID))
	{
          gtk_widget_queue_draw (GTK_WIDGET (tree_view));
	  if (validate_row (tree_view, tree, node, &iter, path))
	      size_changed = TRUE;
	}

      area_below -= gtk_tree_view_get_row_height (tree_view, node);
    }
  gtk_tree_path_free (path);

  /* If we ran out of tree, and have extra area_below left, we need to add it
   * to area_above */
  if (area_below > 0)
    area_above += area_below;

  _gtk_tree_view_find_node (tree_view, above_path, &tree, &node);

  /* We walk backwards */
  while (area_above > 0)
    {
      gtk_tree_rbtree_prev_full (tree, node, &tree, &node);

      /* Always find the new path in the tree.  We cannot just assume
       * a gtk_tree_path_prev() is enough here, as there might be children
       * in between this node and the previous sibling node.  If this
       * appears to be a performance hotspot in profiles, we can look into
       * intrigate logic for keeping path, node and iter in sync like
       * we do for forward walks.  (Which will be hard because of the lacking
       * iter_prev).
       */

      if (node == NULL)
	break;

      gtk_tree_path_free (above_path);
      above_path = _gtk_tree_path_new_from_rbtree (tree, node);

      gtk_tree_model_get_iter (tree_view->priv->model, &iter, above_path);

      if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID) ||
	  GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID))
	{
          gtk_widget_queue_draw (GTK_WIDGET (tree_view));
	  if (validate_row (tree_view, tree, node, &iter, above_path))
	    size_changed = TRUE;
	}
      area_above -= gtk_tree_view_get_row_height (tree_view, node);
    }

  /* if we scrolled to a path, we need to set the dy here,
   * and sync the top row accordingly
   */
  if (tree_view->priv->scroll_to_path)
    {
      gtk_tree_view_set_top_row (tree_view, above_path, -area_above);
      gtk_tree_view_top_row_to_dy (tree_view);

      need_redraw = TRUE;
    }
  else if (gtk_tree_view_get_height (tree_view) <= gtk_adjustment_get_page_size (tree_view->priv->vadjustment))
    {
      /* when we are not scrolling, we should never set dy to something
       * else than zero. we update top_row to be in sync with dy = 0.
       */
      gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->vadjustment), 0);
      gtk_tree_view_dy_to_top_row (tree_view);
    }
  else if (gtk_adjustment_get_value (tree_view->priv->vadjustment) + gtk_adjustment_get_page_size (tree_view->priv->vadjustment) > gtk_tree_view_get_height (tree_view))
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->vadjustment), gtk_tree_view_get_height (tree_view) - gtk_adjustment_get_page_size (tree_view->priv->vadjustment));
      gtk_tree_view_dy_to_top_row (tree_view);
    }
  else
    gtk_tree_view_top_row_to_dy (tree_view);

  /* update width/height and queue a resize */
  if (size_changed)
    {
      GtkRequisition requisition;

      /* We temporarily guess a size, under the assumption that it will be the
       * same when we get our next size_allocate.  If we don't do this, we'll be
       * in an inconsistent state if we call top_row_to_dy. */

      gtk_widget_get_preferred_size (GTK_WIDGET (tree_view),
                                     &requisition, NULL);
      gtk_adjustment_set_upper (tree_view->priv->hadjustment,
                                MAX (gtk_adjustment_get_upper (tree_view->priv->hadjustment), requisition.width));
      gtk_adjustment_set_upper (tree_view->priv->vadjustment,
                                MAX (gtk_adjustment_get_upper (tree_view->priv->vadjustment), requisition.height));
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  if (tree_view->priv->scroll_to_path)
    {
      gtk_tree_row_reference_free (tree_view->priv->scroll_to_path);
      tree_view->priv->scroll_to_path = NULL;
    }

  if (above_path)
    gtk_tree_path_free (above_path);

  if (tree_view->priv->scroll_to_column)
    {
      tree_view->priv->scroll_to_column = NULL;
    }
  if (need_redraw)
    gtk_widget_queue_draw (GTK_WIDGET (tree_view));
}

static void
initialize_fixed_height_mode (GtkTreeView *tree_view)
{
  if (!tree_view->priv->tree)
    return;

  if (tree_view->priv->fixed_height < 0)
    {
      GtkTreeIter iter;
      GtkTreePath *path;

      GtkTreeRBTree *tree = NULL;
      GtkTreeRBNode *node = NULL;

      tree = tree_view->priv->tree;
      node = tree->root;

      path = _gtk_tree_path_new_from_rbtree (tree, node);
      gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);

      validate_row (tree_view, tree, node, &iter, path);

      gtk_tree_path_free (path);

      tree_view->priv->fixed_height = gtk_tree_view_get_row_height (tree_view, node);
    }

   gtk_tree_rbtree_set_fixed_height (tree_view->priv->tree,
                                 tree_view->priv->fixed_height, TRUE);
}

/* Our strategy for finding nodes to validate is a little convoluted.  We find
 * the left-most uninvalidated node.  We then try walking right, validating
 * nodes.  Once we find a valid node, we repeat the previous process of finding
 * the first invalid node.
 */

static gboolean
do_validate_rows (GtkTreeView *tree_view, gboolean queue_resize)
{
  static gboolean prevent_recursion_hack = FALSE;

  GtkTreeRBTree *tree = NULL;
  GtkTreeRBNode *node = NULL;
  gboolean validated_area = FALSE;
  gint retval = TRUE;
  GtkTreePath *path = NULL;
  GtkTreeIter iter;
  GTimer *timer;
  gint i = 0;

  gint y = -1;
  gint prev_height = -1;
  gboolean fixed_height = TRUE;

  g_assert (tree_view);

  /* prevent infinite recursion via get_preferred_width() */
  if (prevent_recursion_hack)
    return FALSE;

  if (tree_view->priv->tree == NULL)
      return FALSE;

  if (tree_view->priv->fixed_height_mode)
    {
      if (tree_view->priv->fixed_height < 0)
        initialize_fixed_height_mode (tree_view);

      return FALSE;
    }

  timer = g_timer_new ();
  g_timer_start (timer);

  do
    {
      gboolean changed = FALSE;

      if (! GTK_TREE_RBNODE_FLAG_SET (tree_view->priv->tree->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID))
	{
	  retval = FALSE;
	  goto done;
	}

      if (path != NULL)
	{
	  node = gtk_tree_rbtree_next (tree, node);
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

	  g_assert (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_DESCENDANTS_INVALID));

	  do
	    {
	      if (!gtk_tree_rbtree_is_nil (node->left) &&
		  GTK_TREE_RBNODE_FLAG_SET (node->left, GTK_TREE_RBNODE_DESCENDANTS_INVALID))
		{
		  node = node->left;
		}
              else if (!gtk_tree_rbtree_is_nil (node->right) &&
		       GTK_TREE_RBNODE_FLAG_SET (node->right, GTK_TREE_RBNODE_DESCENDANTS_INVALID))
		{
		  node = node->right;
		}
	      else if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID) ||
		       GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID))
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
	  path = _gtk_tree_path_new_from_rbtree (tree, node);
	  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
	}

      changed = validate_row (tree_view, tree, node, &iter, path);
      validated_area = changed || validated_area;

      if (changed)
        {
          gint offset = gtk_tree_view_get_row_y_offset (tree_view, tree, node);

          if (y == -1 || y > offset)
            y = offset;
        }

      if (!tree_view->priv->fixed_height_check)
        {
	  gint height;

	  height = gtk_tree_view_get_row_height (tree_view, node);
	  if (prev_height < 0)
	    prev_height = height;
	  else if (prev_height != height)
	    fixed_height = FALSE;
	}

      i++;
    }
  while (g_timer_elapsed (timer, NULL) < GTK_TREE_VIEW_TIME_MS_PER_IDLE / 1000.);

  if (!tree_view->priv->fixed_height_check)
   {
     if (fixed_height)
       gtk_tree_rbtree_set_fixed_height (tree_view->priv->tree, prev_height, FALSE);

     tree_view->priv->fixed_height_check = 1;
   }
  
 done:
  if (validated_area)
    {
      GtkRequisition requisition;
      gint dummy;

      /* We temporarily guess a size, under the assumption that it will be the
       * same when we get our next size_allocate.  If we don't do this, we'll be
       * in an inconsistent state when we call top_row_to_dy. */

      /* FIXME: This is called from size_request, for some reason it is not infinitely
       * recursing, we cannot call gtk_widget_get_preferred_size() here because that's
       * not allowed (from inside ->get_preferred_width/height() implementations, one
       * should call the vfuncs directly). However what is desired here is the full
       * size including any margins and limited by any alignment (i.e. after 
       * GtkWidget:adjust_size_request() is called).
       *
       * Currently bypassing this but the real solution is to not update the scroll adjustments
       * untill we've recieved an allocation (never update scroll adjustments from size-requests).
       */
      prevent_recursion_hack = TRUE;
      gtk_tree_view_measure (GTK_WIDGET (tree_view),
                             GTK_ORIENTATION_HORIZONTAL,
                             -1,
                             &requisition.width, &dummy,
                             NULL, NULL);
      gtk_tree_view_measure (GTK_WIDGET (tree_view),
                             GTK_ORIENTATION_VERTICAL,
                             -1,
                             &requisition.height, &dummy,
                             NULL, NULL);
      prevent_recursion_hack = FALSE;

      /* If rows above the current position have changed height, this has
       * affected the current view and thus needs a redraw.
       */
      if (y != -1 && y < gtk_adjustment_get_value (tree_view->priv->vadjustment))
        gtk_widget_queue_draw (GTK_WIDGET (tree_view));

      gtk_adjustment_set_upper (tree_view->priv->hadjustment,
                                MAX (gtk_adjustment_get_upper (tree_view->priv->hadjustment), requisition.width));
      gtk_adjustment_set_upper (tree_view->priv->vadjustment,
                                MAX (gtk_adjustment_get_upper (tree_view->priv->vadjustment), requisition.height));

      if (queue_resize)
        gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  if (path) gtk_tree_path_free (path);
  g_timer_destroy (timer);

  if (!retval && gtk_widget_get_mapped (GTK_WIDGET (tree_view)))
    update_prelight (tree_view,
                     tree_view->priv->event_last_x,
                     tree_view->priv->event_last_y);

  return retval;
}

static void
disable_adjustment_animation (GtkTreeView *tree_view)
{
  gtk_adjustment_enable_animation (tree_view->priv->vadjustment,
                                   NULL,
                                   gtk_adjustment_get_animation_duration (tree_view->priv->vadjustment));
}

static void
maybe_reenable_adjustment_animation (GtkTreeView *tree_view)
{
  if (tree_view->priv->presize_handler_tick_cb != 0 ||
      tree_view->priv->validate_rows_timer != 0)
    return;

  gtk_adjustment_enable_animation (tree_view->priv->vadjustment,
                                   gtk_widget_get_frame_clock (GTK_WIDGET (tree_view)),
                                   gtk_adjustment_get_animation_duration (tree_view->priv->vadjustment));
}

static gboolean
do_presize_handler (GtkTreeView *tree_view)
{
  if (tree_view->priv->mark_rows_col_dirty)
   {
      if (tree_view->priv->tree)
	gtk_tree_rbtree_column_invalid (tree_view->priv->tree);
      tree_view->priv->mark_rows_col_dirty = FALSE;
    }
  validate_visible_area (tree_view);
  if (tree_view->priv->presize_handler_tick_cb != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (tree_view), tree_view->priv->presize_handler_tick_cb);
      tree_view->priv->presize_handler_tick_cb = 0;
    }

  if (tree_view->priv->fixed_height_mode)
    {
      GtkRequisition requisition;

      gtk_widget_get_preferred_size (GTK_WIDGET (tree_view),
                                     &requisition, NULL);

      gtk_adjustment_set_upper (tree_view->priv->hadjustment,
                                MAX (gtk_adjustment_get_upper (tree_view->priv->hadjustment), requisition.width));
      gtk_adjustment_set_upper (tree_view->priv->vadjustment,
                                MAX (gtk_adjustment_get_upper (tree_view->priv->vadjustment), requisition.height));
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  maybe_reenable_adjustment_animation (tree_view);

  return FALSE;
}

static gboolean
presize_handler_callback (GtkWidget     *widget,
                          GdkFrameClock *clock,
                          gpointer       unused)
{
  do_presize_handler (GTK_TREE_VIEW (widget));
		   
  return G_SOURCE_REMOVE;
}

static gboolean
validate_rows (GtkTreeView *tree_view)
{
  gboolean retval;
  
  if (tree_view->priv->presize_handler_tick_cb)
    {
      do_presize_handler (tree_view);
      return G_SOURCE_CONTINUE;
    }

  retval = do_validate_rows (tree_view, TRUE);
  
  if (! retval && tree_view->priv->validate_rows_timer)
    {
      g_source_remove (tree_view->priv->validate_rows_timer);
      tree_view->priv->validate_rows_timer = 0;
      maybe_reenable_adjustment_animation (tree_view);
    }

  return retval;
}

static void
install_presize_handler (GtkTreeView *tree_view)
{
  if (! gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    return;

  disable_adjustment_animation (tree_view);

  if (! tree_view->priv->presize_handler_tick_cb)
    {
      tree_view->priv->presize_handler_tick_cb =
	gtk_widget_add_tick_callback (GTK_WIDGET (tree_view), presize_handler_callback, NULL, NULL);
    }
  if (! tree_view->priv->validate_rows_timer)
    {
      tree_view->priv->validate_rows_timer =
	g_idle_add_full (GTK_TREE_VIEW_PRIORITY_VALIDATE, (GSourceFunc) validate_rows, tree_view, NULL);
      g_source_set_name_by_id (tree_view->priv->validate_rows_timer, "[gtk] validate_rows");
    }
}

static gboolean
scroll_sync_handler (GtkTreeView *tree_view)
{
  if (gtk_tree_view_get_height (tree_view) <= gtk_adjustment_get_page_size (tree_view->priv->vadjustment))
    gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->vadjustment), 0);
  else if (gtk_tree_row_reference_valid (tree_view->priv->top_row))
    gtk_tree_view_top_row_to_dy (tree_view);
  else
    gtk_tree_view_dy_to_top_row (tree_view);

  tree_view->priv->scroll_sync_timer = 0;

  return FALSE;
}

static void
install_scroll_sync_handler (GtkTreeView *tree_view)
{
  if (!gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    return;

  if (!tree_view->priv->scroll_sync_timer)
    {
      tree_view->priv->scroll_sync_timer =
	g_idle_add_full (GTK_TREE_VIEW_PRIORITY_SCROLL_SYNC, (GSourceFunc) scroll_sync_handler, tree_view, NULL);
      g_source_set_name_by_id (tree_view->priv->scroll_sync_timer, "[gtk] scroll_sync_handler");
    }
}

static void
gtk_tree_view_set_top_row (GtkTreeView *tree_view,
			   GtkTreePath *path,
			   gint         offset)
{
  gtk_tree_row_reference_free (tree_view->priv->top_row);

  if (!path)
    {
      tree_view->priv->top_row = NULL;
      tree_view->priv->top_row_dy = 0;
    }
  else
    {
      tree_view->priv->top_row = gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view), tree_view->priv->model, path);
      tree_view->priv->top_row_dy = offset;
    }
}

/* Always call this iff dy is in the visible range.  If the tree is empty, then
 * it’s set to be NULL, and top_row_dy is 0;
 */
static void
gtk_tree_view_dy_to_top_row (GtkTreeView *tree_view)
{
  gint offset;
  GtkTreePath *path;
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;

  if (tree_view->priv->tree == NULL)
    {
      gtk_tree_view_set_top_row (tree_view, NULL, 0);
    }
  else
    {
      offset = gtk_tree_rbtree_find_offset (tree_view->priv->tree,
					tree_view->priv->dy,
					&tree, &node);

      if (tree == NULL)
        {
	  gtk_tree_view_set_top_row (tree_view, NULL, 0);
	}
      else
        {
	  path = _gtk_tree_path_new_from_rbtree (tree, node);
	  gtk_tree_view_set_top_row (tree_view, path, offset);
	  gtk_tree_path_free (path);
	}
    }
}

static void
gtk_tree_view_top_row_to_dy (GtkTreeView *tree_view)
{
  GtkTreePath *path;
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
  int new_dy;

  /* Avoid recursive calls */
  if (tree_view->priv->in_top_row_to_dy)
    return;

  if (tree_view->priv->top_row)
    path = gtk_tree_row_reference_get_path (tree_view->priv->top_row);
  else
    path = NULL;

  if (!path)
    tree = NULL;
  else
    _gtk_tree_view_find_node (tree_view, path, &tree, &node);

  if (path)
    gtk_tree_path_free (path);

  if (tree == NULL)
    {
      /* keep dy and set new toprow */
      gtk_tree_row_reference_free (tree_view->priv->top_row);
      tree_view->priv->top_row = NULL;
      tree_view->priv->top_row_dy = 0;
      /* DO NOT install the idle handler */
      gtk_tree_view_dy_to_top_row (tree_view);
      return;
    }

  if (gtk_tree_view_get_row_height (tree_view, node)
      < tree_view->priv->top_row_dy)
    {
      /* new top row -- do NOT install the idle handler */
      gtk_tree_view_dy_to_top_row (tree_view);
      return;
    }

  new_dy = gtk_tree_rbtree_node_find_offset (tree, node);
  new_dy += tree_view->priv->top_row_dy;

  if (new_dy + gtk_adjustment_get_page_size (tree_view->priv->vadjustment) > gtk_tree_view_get_height (tree_view))
    new_dy = gtk_tree_view_get_height (tree_view) - gtk_adjustment_get_page_size (tree_view->priv->vadjustment);

  new_dy = MAX (0, new_dy);

  tree_view->priv->in_top_row_to_dy = TRUE;
  gtk_adjustment_set_value (tree_view->priv->vadjustment, (gdouble)new_dy);
  tree_view->priv->in_top_row_to_dy = FALSE;
}


void
_gtk_tree_view_install_mark_rows_col_dirty (GtkTreeView *tree_view,
					    gboolean     install_handler)
{
  tree_view->priv->mark_rows_col_dirty = TRUE;

  if (install_handler)
    install_presize_handler (tree_view);
}

/*
 * This function works synchronously (due to the while (validate_rows...)
 * loop).
 *
 * There was a check for column_type != GTK_TREE_VIEW_COLUMN_AUTOSIZE
 * here. You now need to check that yourself.
 */
void
_gtk_tree_view_column_autosize (GtkTreeView *tree_view,
			        GtkTreeViewColumn *column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (column));

  _gtk_tree_view_column_cell_set_dirty (column, FALSE);

  do_presize_handler (tree_view);
  while (validate_rows (tree_view));

  gtk_widget_queue_resize (GTK_WIDGET (tree_view));
}

/* Drag-and-drop */

static void
set_source_row (GdkDrag        *drag,
                GtkTreeModel   *model,
                GtkTreePath    *source_row)
{
  g_object_set_data_full (G_OBJECT (drag),
                          I_("gtk-tree-view-source-row"),
                          source_row ? gtk_tree_row_reference_new (model, source_row) : NULL,
                          (GDestroyNotify) (source_row ? gtk_tree_row_reference_free : NULL));
}

static GtkTreePath*
get_source_row (GdkDrag *drag)
{
  GtkTreeRowReference *ref =
    g_object_get_data (G_OBJECT (drag), "gtk-tree-view-source-row");

  if (ref)
    return gtk_tree_row_reference_get_path (ref);
  else
    return NULL;
}

typedef struct
{
  GtkTreeRowReference *dest_row;
  guint                path_down_mode   : 1;
  guint                empty_view_drop  : 1;
  guint                drop_append_mode : 1;
}
DestRow;

static void
dest_row_free (gpointer data)
{
  DestRow *dr = (DestRow *)data;

  gtk_tree_row_reference_free (dr->dest_row);
  g_slice_free (DestRow, dr);
}

static void
set_dest_row (GdkDrop      *drop,
              GtkTreeModel *model,
              GtkTreePath  *dest_row,
              gboolean      path_down_mode,
              gboolean      empty_view_drop,
              gboolean      drop_append_mode)
{
  DestRow *dr;

  if (!dest_row)
    {
      g_object_set_data_full (G_OBJECT (drop), I_("gtk-tree-view-dest-row"),
                              NULL, NULL);
      return;
    }

  dr = g_slice_new (DestRow);

  dr->dest_row = gtk_tree_row_reference_new (model, dest_row);
  dr->path_down_mode = path_down_mode != FALSE;
  dr->empty_view_drop = empty_view_drop != FALSE;
  dr->drop_append_mode = drop_append_mode != FALSE;

  g_object_set_data_full (G_OBJECT (drop), I_("gtk-tree-view-dest-row"),
                          dr, (GDestroyNotify) dest_row_free);
}

static GtkTreePath*
get_dest_row (GdkDrop  *drop,
              gboolean *path_down_mode)
{
  DestRow *dr =
    g_object_get_data (G_OBJECT (drop), "gtk-tree-view-dest-row");

  if (dr)
    {
      GtkTreePath *path = NULL;

      if (path_down_mode)
        *path_down_mode = dr->path_down_mode;

      if (dr->dest_row)
        path = gtk_tree_row_reference_get_path (dr->dest_row);
      else if (dr->empty_view_drop)
        path = gtk_tree_path_new_from_indices (0, -1);
      else
        path = NULL;

      if (path && dr->drop_append_mode)
        gtk_tree_path_next (path);

      return path;
    }
  else
    return NULL;
}

/* Get/set whether drag_motion requested the drag data and
 * drag_data_received should thus not actually insert the data,
 * since the data doesn’t result from a drop.
 */
static void
set_status_pending (GdkDrop       *drop,
                    GdkDragAction  suggested_action)
{
  g_object_set_data (G_OBJECT (drop),
                     I_("gtk-tree-view-status-pending"),
                     GINT_TO_POINTER (suggested_action));
}

static GdkDragAction
get_status_pending (GdkDrop *drop)
{
  return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (drop),
                                             "gtk-tree-view-status-pending"));
}

static TreeViewDragInfo*
get_info (GtkTreeView *tree_view)
{
  return g_object_get_data (G_OBJECT (tree_view), "gtk-tree-view-drag-info");
}

static void
destroy_info (TreeViewDragInfo *di)
{
  g_slice_free (TreeViewDragInfo, di);
}

static TreeViewDragInfo*
ensure_info (GtkTreeView *tree_view)
{
  TreeViewDragInfo *di;

  di = get_info (tree_view);

  if (di == NULL)
    {
      di = g_slice_new0 (TreeViewDragInfo);

      g_object_set_data_full (G_OBJECT (tree_view),
                              I_("gtk-tree-view-drag-info"),
                              di,
                              (GDestroyNotify) destroy_info);
    }

  return di;
}

static void
remove_info (GtkTreeView *tree_view)
{
  g_object_set_data (G_OBJECT (tree_view), I_("gtk-tree-view-drag-info"), NULL);
}

static void
add_scroll_timeout (GtkTreeView *tree_view)
{
  if (tree_view->priv->scroll_timeout == 0)
    {
      tree_view->priv->scroll_timeout = g_timeout_add (150, scroll_row_timeout, tree_view);
      g_source_set_name_by_id (tree_view->priv->scroll_timeout, "[gtk] scroll_row_timeout");
    }
}

static void
remove_scroll_timeout (GtkTreeView *tree_view)
{
  if (tree_view->priv->scroll_timeout != 0)
    {
      g_source_remove (tree_view->priv->scroll_timeout);
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
                 "g_signal_stop_emission_by_name() in your signal handler to prevent "
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
      g_source_remove (tree_view->priv->open_dest_timeout);
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

  return result;
}

static gboolean
scroll_row_timeout (gpointer data)
{
  GtkTreeView *tree_view = data;

  gtk_tree_view_vertical_autoscroll (tree_view);

  if (tree_view->priv->rubber_band_status == RUBBER_BAND_ACTIVE)
    gtk_tree_view_update_rubber_band (tree_view);

  return TRUE;
}

/* Returns TRUE if event should not be propagated to parent widgets */
static gboolean
set_destination_row (GtkTreeView    *tree_view,
                     GdkDrop        *drop,
                     /* coordinates relative to the widget */
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
  gboolean can_drop = FALSE;

  *suggested_action = 0;
  *target = NULL;

  widget = GTK_WIDGET (tree_view);

  di = get_info (tree_view);

  if (di == NULL || y - gtk_tree_view_get_effective_header_height (tree_view) < 0)
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

  *target = gtk_drag_dest_find_target (widget, drop,
                                       gtk_drag_dest_get_target_list (widget));
  if (*target == NULL)
    {
      return FALSE;
    }

  if (!gtk_tree_view_get_dest_row_at_pos (tree_view,
                                          x, y,
                                          &path,
                                          &pos))
    {
      gint n_children;
      GtkTreeModel *model;

      remove_open_timeout (tree_view);

      /* the row got dropped on empty space, let's setup a special case
       */

      if (path)
	gtk_tree_path_free (path);

      model = gtk_tree_view_get_model (tree_view);

      n_children = gtk_tree_model_iter_n_children (model, NULL);
      if (n_children)
        {
          pos = GTK_TREE_VIEW_DROP_AFTER;
          path = gtk_tree_path_new_from_indices (n_children - 1, -1);
        }
      else
        {
          pos = GTK_TREE_VIEW_DROP_BEFORE;
          path = gtk_tree_path_new_from_indices (0, -1);
        }

      can_drop = TRUE;

      goto out;
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
      can_drop = TRUE;
    }

out:
  if (can_drop)
    {
      *suggested_action = GDK_ACTION_COPY | GDK_ACTION_MOVE;

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

  if (path)
    gtk_tree_path_free (path);

  return TRUE;
}

static GtkTreePath*
get_logical_dest_row (GtkTreeView *tree_view,
                      gboolean    *path_down_mode,
                      gboolean    *drop_append_mode)
{
  /* adjust path to point to the row the drop goes in front of */
  GtkTreePath *path = NULL;
  GtkTreeViewDropPosition pos;

  g_return_val_if_fail (path_down_mode != NULL, NULL);
  g_return_val_if_fail (drop_append_mode != NULL, NULL);

  *path_down_mode = FALSE;
  *drop_append_mode = 0;

  gtk_tree_view_get_drag_dest_row (tree_view, &path, &pos);

  if (path == NULL)
    return NULL;

  if (pos == GTK_TREE_VIEW_DROP_BEFORE)
    ; /* do nothing */
  else if (pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE ||
           pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER)
    *path_down_mode = TRUE;
  else
    {
      GtkTreeIter iter;
      GtkTreeModel *model = gtk_tree_view_get_model (tree_view);

      g_assert (pos == GTK_TREE_VIEW_DROP_AFTER);

      if (!gtk_tree_model_get_iter (model, &iter, path) ||
          !gtk_tree_model_iter_next (model, &iter))
        *drop_append_mode = 1;
      else
        {
          *drop_append_mode = 0;
          gtk_tree_path_next (path);
        }
    }

  return path;
}

static gboolean
gtk_tree_view_maybe_begin_dragging_row (GtkTreeView *tree_view)
{
  GtkWidget *widget = GTK_WIDGET (tree_view);
  gdouble start_x, start_y, offset_x, offset_y;
  GdkDrag *drag;
  TreeViewDragInfo *di;
  GtkTreePath *path = NULL;
  gint button;
  GtkTreeModel *model;
  gboolean retval = FALSE;
  gint bin_x, bin_y;

  di = get_info (tree_view);

  if (di == NULL || !di->source_set)
    goto out;

  if (!gtk_gesture_is_recognized (tree_view->priv->drag_gesture))
    goto out;

  gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (tree_view->priv->drag_gesture),
                                    &start_x, &start_y);
  gtk_gesture_drag_get_offset (GTK_GESTURE_DRAG (tree_view->priv->drag_gesture),
                               &offset_x, &offset_y);

  if (!gtk_drag_check_threshold (widget, 0, 0, offset_x, offset_y))
    goto out;

  model = gtk_tree_view_get_model (tree_view);

  if (model == NULL)
    goto out;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (tree_view->priv->drag_gesture));

  /* Deny the click gesture */
  gtk_gesture_set_state (GTK_GESTURE (tree_view->priv->click_gesture),
                         GTK_EVENT_SEQUENCE_DENIED);

  gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, start_x, start_y,
						      &bin_x, &bin_y);
  gtk_tree_view_get_path_at_pos (tree_view, bin_x, bin_y, &path,
                                 NULL, NULL, NULL);

  if (path == NULL)
    goto out;

  if (!GTK_IS_TREE_DRAG_SOURCE (model) ||
      !gtk_tree_drag_source_row_draggable (GTK_TREE_DRAG_SOURCE (model),
					   path))
    goto out;

  if (!(GDK_BUTTON1_MASK << (button - 1) & di->start_button_mask))
    goto out;

  retval = TRUE;

  /* Now we can begin the drag */
  gtk_gesture_set_state (GTK_GESTURE (tree_view->priv->drag_gesture),
                         GTK_EVENT_SEQUENCE_CLAIMED);

  drag = gtk_drag_begin (widget,
                         gtk_gesture_get_device (GTK_GESTURE (tree_view->priv->drag_gesture)),
                         gtk_drag_source_get_target_list (widget),
                         di->source_actions,
                         start_x, start_y);

  set_source_row (drag, model, path);

 out:
  if (path)
    gtk_tree_path_free (path);

  return retval;
}


static void
gtk_tree_view_drag_begin (GtkWidget      *widget,
                          GdkDrag        *drag)
{
  GtkTreeView *tree_view;
  GtkTreePath *path = NULL;
  gint cell_x, cell_y;
  GdkPaintable *row_pix;
  TreeViewDragInfo *di;

  tree_view = GTK_TREE_VIEW (widget);

  /* if the user uses a custom DND source impl, we don't set the icon here */
  di = get_info (tree_view);

  if (di == NULL || !di->source_set)
    return;

  gtk_tree_view_get_path_at_pos (tree_view,
                                 tree_view->priv->press_start_x,
                                 tree_view->priv->press_start_y,
                                 &path,
                                 NULL,
                                 &cell_x,
                                 &cell_y);

  /* If path is NULL, there's nothing we can drag.  For now, we silently
   * bail out.  Actually, dragging should not be possible from an empty
   * tree view, but there's no way we can cancel that from here.
   * Automatically unsetting the tree view as drag source for empty models
   * is something that would likely break other people's code ...
   */
  if (!path)
    return;

  row_pix = gtk_tree_view_create_row_drag_icon (tree_view, path);

  gtk_drag_set_icon_paintable (drag, row_pix, tree_view->priv->press_start_x + 1, 1);

  g_object_unref (row_pix);
  gtk_tree_path_free (path);
}

static void
gtk_tree_view_drag_end (GtkWidget      *widget,
                        GdkDrag        *drag)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

  tree_view->priv->event_last_x = -10000;
  tree_view->priv->event_last_y = -10000;
}

/* Default signal implementations for the drag signals */
static void
gtk_tree_view_drag_data_get (GtkWidget        *widget,
                             GdkDrag          *drag,
                             GtkSelectionData *selection_data)
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

  source_row = get_source_row (drag);

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
  if (gtk_selection_data_get_target (selection_data) == g_intern_static_string ("GTK_TREE_MODEL_ROW"))
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
                                GdkDrag        *drag)
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

  source_row = get_source_row (drag);

  if (source_row == NULL)
    return;

  gtk_tree_drag_source_drag_data_delete (GTK_TREE_DRAG_SOURCE (model), source_row);

  gtk_tree_path_free (source_row);

  set_source_row (drag, NULL, NULL);
}

static void
gtk_tree_view_drag_leave (GtkWidget *widget,
                          GdkDrop   *drop)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

  /* unset any highlight row */
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                   NULL,
                                   GTK_TREE_VIEW_DROP_BEFORE);

  remove_scroll_timeout (GTK_TREE_VIEW (widget));
  remove_open_timeout (GTK_TREE_VIEW (widget));

  tree_view->priv->event_last_x = -10000;
  tree_view->priv->event_last_y = -10000;
}


static gboolean
gtk_tree_view_drag_motion (GtkWidget *widget,
                           GdkDrop   *drop,
                           gint       x,
                           gint       y)
{
  gboolean empty;
  GtkTreePath *path = NULL;
  GtkTreeViewDropPosition pos;
  GtkTreeView *tree_view;
  GdkDragAction suggested_action = 0;
  GdkAtom target;

  tree_view = GTK_TREE_VIEW (widget);

  if (!set_destination_row (tree_view, drop, x, y, &suggested_action, &target))
    return FALSE;

  tree_view->priv->event_last_x = x;
  tree_view->priv->event_last_y = y;

  gtk_tree_view_get_drag_dest_row (tree_view, &path, &pos);

  /* we only know this *after* set_desination_row */
  empty = tree_view->priv->empty_view_drop;

  if (path == NULL && !empty)
    {
      /* Can't drop here. */
      gdk_drop_status (drop, 0);
    }
  else
    {
      if (tree_view->priv->open_dest_timeout == 0 &&
          (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER ||
           pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE))
        {
          tree_view->priv->open_dest_timeout =
            g_timeout_add (AUTO_EXPAND_TIMEOUT, open_row_timeout, tree_view);
          g_source_set_name_by_id (tree_view->priv->open_dest_timeout, "[gtk] open_row_timeout");
        }
      else
        {
	  add_scroll_timeout (tree_view);
	}

      if (target == g_intern_static_string ("GTK_TREE_MODEL_ROW"))
        {
          /* Request data so we can use the source row when
           * determining whether to accept the drop
           */
          set_status_pending (drop, suggested_action);
          gtk_drag_get_data (widget, drop, target);
        }
      else
        {
          set_status_pending (drop, 0);
          gdk_drop_status (drop, suggested_action);
        }
    }

  if (path)
    gtk_tree_path_free (path);

  return TRUE;
}


static gboolean
gtk_tree_view_drag_drop (GtkWidget *widget,
                         GdkDrop   *drop,
                         gint       x,
                         gint       y)
{
  GtkTreeView *tree_view;
  GtkTreePath *path;
  GdkDragAction suggested_action = 0;
  GdkAtom target = NULL;
  TreeViewDragInfo *di;
  GtkTreeModel *model;
  gboolean path_down_mode;
  gboolean drop_append_mode;

  tree_view = GTK_TREE_VIEW (widget);

  model = gtk_tree_view_get_model (tree_view);

  remove_scroll_timeout (GTK_TREE_VIEW (widget));
  remove_open_timeout (GTK_TREE_VIEW (widget));

  di = get_info (tree_view);

  if (di == NULL)
    return FALSE;

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_DEST, "drag_drop"))
    return FALSE;

  if (!set_destination_row (tree_view, drop, x, y, &suggested_action, &target))
    return FALSE;

  path = get_logical_dest_row (tree_view, &path_down_mode, &drop_append_mode);

  if (target != NULL && path != NULL)
    {
      /* in case a motion had requested drag data, change things so we
       * treat drag data receives as a drop.
       */
      set_status_pending (drop, 0);
      set_dest_row (drop, model, path,
                    path_down_mode, tree_view->priv->empty_view_drop,
                    drop_append_mode);
    }

  if (path)
    gtk_tree_path_free (path);

  /* Unset this thing */
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                   NULL,
                                   GTK_TREE_VIEW_DROP_BEFORE);

  if (target != NULL)
    {
      gtk_drag_get_data (widget, drop, target);
      return TRUE;
    }
  else
    return FALSE;
}

static GdkDragAction
gtk_tree_view_get_action (GtkWidget *treeview,
                          GdkDrop   *drop)
{
  GdkDrag *drag = gdk_drop_get_drag (drop);
  GtkWidget *source_widget = gtk_drag_get_source_widget (drag);
  GdkDragAction actions;

  actions = gdk_drop_get_actions (drop);

  if (source_widget == treeview &&
      actions & GDK_ACTION_MOVE)
    return GDK_ACTION_MOVE;

  if (actions & GDK_ACTION_COPY)
    return GDK_ACTION_COPY;

  if (actions & GDK_ACTION_MOVE)
    return GDK_ACTION_MOVE;

  return 0;
}

static void
gtk_tree_view_drag_data_received (GtkWidget        *widget,
                                  GdkDrop          *drop,
                                  GtkSelectionData *selection_data)
{
  GtkTreePath *path;
  TreeViewDragInfo *di;
  GtkTreeModel *model;
  GtkTreeView *tree_view;
  GtkTreePath *dest_row;
  GdkDragAction suggested_action;
  gboolean path_down_mode;
  gboolean drop_append_mode;

  tree_view = GTK_TREE_VIEW (widget);

  model = gtk_tree_view_get_model (tree_view);

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_DEST, "drag_data_received"))
    return;

  di = get_info (tree_view);

  if (di == NULL)
    return;

  suggested_action = get_status_pending (drop);

  if (suggested_action)
    {
      /* We are getting this data due to a request in drag_motion,
       * rather than due to a request in drag_drop, so we are just
       * supposed to call drag_status, not actually paste in the
       * data.
       */
      path = get_logical_dest_row (tree_view, &path_down_mode,
                                   &drop_append_mode);

      if (path == NULL)
        suggested_action = 0;
      else if (path_down_mode)
        gtk_tree_path_down (path);

      if (suggested_action)
        {
	  if (!gtk_tree_drag_dest_row_drop_possible (GTK_TREE_DRAG_DEST (model),
						     path,
						     selection_data))
            {
              if (path_down_mode)
                {
                  path_down_mode = FALSE;
                  gtk_tree_path_up (path);

                  if (!gtk_tree_drag_dest_row_drop_possible (GTK_TREE_DRAG_DEST (model),
                                                             path,
                                                             selection_data))
                    suggested_action = 0;
                }
              else
	        suggested_action = 0;
            }
        }

      gdk_drop_status (drop, suggested_action);

      if (path)
        gtk_tree_path_free (path);

      /* If you can't drop, remove user drop indicator until the next motion */
      if (suggested_action == 0)
        gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                         NULL,
                                         GTK_TREE_VIEW_DROP_BEFORE);

      return;
    }

  dest_row = get_dest_row (drop, &path_down_mode);

  if (dest_row == NULL)
    return;

  if (gtk_selection_data_get_length (selection_data) >= 0)
    {
      if (path_down_mode)
        {
          gtk_tree_path_down (dest_row);
          if (!gtk_tree_drag_dest_row_drop_possible (GTK_TREE_DRAG_DEST (model),
                                                     dest_row, selection_data))
            gtk_tree_path_up (dest_row);
        }
    }

  if (gtk_selection_data_get_length (selection_data) >= 0)
    {
      suggested_action = gtk_tree_view_get_action (widget, drop);

      if (suggested_action &&
          !gtk_tree_drag_dest_drag_data_received (GTK_TREE_DRAG_DEST (model),
                                                  dest_row,
                                                  selection_data))
        suggested_action = 0;
    }

  gdk_drop_finish (drop, suggested_action);

  if (gtk_tree_path_get_depth (dest_row) == 1 &&
      gtk_tree_path_get_indices (dest_row)[0] == 0 &&
      gtk_tree_model_iter_n_children (tree_view->priv->model, NULL) != 0)
    {
      /* special special case drag to "0", scroll to first item */
      if (!tree_view->priv->scroll_to_path)
        gtk_tree_view_scroll_to_cell (tree_view, dest_row, NULL, FALSE, 0.0, 0.0);
    }

  gtk_tree_path_free (dest_row);

  /* drop dest_row */
  set_dest_row (drop, NULL, NULL, FALSE, FALSE, FALSE);
}


/* GtkContainer Methods
 */


static void
gtk_tree_view_remove (GtkContainer *container,
		      GtkWidget    *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (container);
  GtkTreeViewChild *child = NULL;
  GList *tmp_list;

  tmp_list = tree_view->priv->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      if (child->widget == widget)
	{
	  gtk_widget_unparent (widget);

	  tree_view->priv->children = g_list_remove_link (tree_view->priv->children, tmp_list);
	  g_list_free_1 (tmp_list);
	  g_slice_free (GtkTreeViewChild, child);
	  return;
	}

      tmp_list = tmp_list->next;
    }

  tmp_list = tree_view->priv->columns;

  while (tmp_list)
    {
      GtkTreeViewColumn *column;
      GtkWidget         *button;

      column = tmp_list->data;
      button = gtk_tree_view_column_get_button (column);

      if (button == widget)
	{
	  gtk_widget_unparent (widget);
	  return;
	}
      tmp_list = tmp_list->next;
    }
}

static void
gtk_tree_view_forall (GtkContainer *container,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (container);
  GtkTreeViewChild *child = NULL;
  GList *tmp_list;

  tmp_list = tree_view->priv->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      tmp_list = tmp_list->next;

      (* callback) (child->widget, callback_data);
    }
}

/* Returns TRUE is any of the columns contains a cell that can-focus.
 * If this is not the case, a column-spanning focus rectangle will be
 * drawn.
 */
static gboolean
gtk_tree_view_has_can_focus_cell (GtkTreeView *tree_view)
{
  GList *list;

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      GtkTreeViewColumn *column = list->data;

      if (!gtk_tree_view_column_get_visible (column))
	continue;
      if (gtk_cell_area_is_activatable (gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column))))
	return TRUE;
    }

  return FALSE;
}

static void
column_sizing_notify (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    data)
{
  GtkTreeViewColumn *c = GTK_TREE_VIEW_COLUMN (object);

  if (gtk_tree_view_column_get_sizing (c) != GTK_TREE_VIEW_COLUMN_FIXED)
    /* disable fixed height mode */
    g_object_set (data, "fixed-height-mode", FALSE, NULL);
}

/**
 * gtk_tree_view_set_fixed_height_mode:
 * @tree_view: a #GtkTreeView 
 * @enable: %TRUE to enable fixed height mode
 * 
 * Enables or disables the fixed height mode of @tree_view. 
 * Fixed height mode speeds up #GtkTreeView by assuming that all 
 * rows have the same height. 
 * Only enable this option if all rows are the same height and all
 * columns are of type %GTK_TREE_VIEW_COLUMN_FIXED.
 **/
void
gtk_tree_view_set_fixed_height_mode (GtkTreeView *tree_view,
                                     gboolean     enable)
{
  GList *l;
  
  enable = enable != FALSE;

  if (enable == tree_view->priv->fixed_height_mode)
    return;

  if (!enable)
    {
      tree_view->priv->fixed_height_mode = 0;
      tree_view->priv->fixed_height = -1;
    }
  else 
    {
      /* make sure all columns are of type FIXED */
      for (l = tree_view->priv->columns; l; l = l->next)
	{
	  GtkTreeViewColumn *c = l->data;
	  
	  g_return_if_fail (gtk_tree_view_column_get_sizing (c) == GTK_TREE_VIEW_COLUMN_FIXED);
	}
      
      /* yes, we really have to do this is in a separate loop */
      for (l = tree_view->priv->columns; l; l = l->next)
	g_signal_connect (l->data, "notify::sizing",
			  G_CALLBACK (column_sizing_notify), tree_view);
      
      tree_view->priv->fixed_height_mode = 1;
      tree_view->priv->fixed_height = -1;
    }

  /* force a revalidation */
  install_presize_handler (tree_view);

  g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_FIXED_HEIGHT_MODE]);
}

/**
 * gtk_tree_view_get_fixed_height_mode:
 * @tree_view: a #GtkTreeView
 * 
 * Returns whether fixed height mode is turned on for @tree_view.
 * 
 * Returns: %TRUE if @tree_view is in fixed height mode
 **/
gboolean
gtk_tree_view_get_fixed_height_mode (GtkTreeView *tree_view)
{
  return tree_view->priv->fixed_height_mode;
}

/* Returns TRUE if the focus is within the headers, after the focus operation is
 * done
 */
static gboolean
gtk_tree_view_header_focus (GtkTreeView      *tree_view,
			    GtkDirectionType  dir,
			    gboolean          clamp_column_visible)
{
  GtkTreeViewColumn *column;
  GtkWidget *button;
  GtkWidget *focus_child;
  GList *last_column, *first_column;
  GList *tmp_list;
  gboolean rtl;

  if (! tree_view->priv->headers_visible)
    return FALSE;

  focus_child = gtk_widget_get_focus_child (GTK_WIDGET (tree_view));

  first_column = tree_view->priv->columns;
  while (first_column)
    {
      column = GTK_TREE_VIEW_COLUMN (first_column->data);
      button = gtk_tree_view_column_get_button (column);

      if (gtk_widget_get_can_focus (button) &&
          gtk_tree_view_column_get_visible (column) &&
          (gtk_tree_view_column_get_clickable (column) ||
           gtk_tree_view_column_get_reorderable (column)))
	break;
      first_column = first_column->next;
    }

  /* No headers are visible, or are focusable.  We can't focus in or out.
   */
  if (first_column == NULL)
    return FALSE;

  last_column = g_list_last (tree_view->priv->columns);
  while (last_column)
    {
      column = GTK_TREE_VIEW_COLUMN (last_column->data);
      button = gtk_tree_view_column_get_button (column);

      if (gtk_widget_get_can_focus (button) &&
          gtk_tree_view_column_get_visible (column) &&
          (gtk_tree_view_column_get_clickable (column) ||
           gtk_tree_view_column_get_reorderable (column)))
	break;
      last_column = last_column->prev;
    }


  rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

  switch (dir)
    {
    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_UP:
    case GTK_DIR_DOWN:
      if (focus_child == NULL)
	{
	  if (tree_view->priv->focus_column != NULL)
	    button = gtk_tree_view_column_get_button (tree_view->priv->focus_column);
	  else 
	    button = NULL;

	  if (button && gtk_widget_get_can_focus (button))
	    focus_child = button;
	  else
	    focus_child = gtk_tree_view_column_get_button (GTK_TREE_VIEW_COLUMN (first_column->data));

	  gtk_widget_grab_focus (focus_child);
	  break;
	}
      return FALSE;

    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
      if (focus_child == NULL)
	{
	  if (tree_view->priv->focus_column != NULL)
	    focus_child = gtk_tree_view_column_get_button (tree_view->priv->focus_column);
	  else if (dir == GTK_DIR_LEFT)
	    focus_child = gtk_tree_view_column_get_button (GTK_TREE_VIEW_COLUMN (last_column->data));
	  else
	    focus_child = gtk_tree_view_column_get_button (GTK_TREE_VIEW_COLUMN (first_column->data));

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
	if (gtk_tree_view_column_get_button (GTK_TREE_VIEW_COLUMN (tmp_list->data)) == focus_child)
	  break;

      if ((tmp_list == first_column && dir == (rtl ? GTK_DIR_RIGHT : GTK_DIR_LEFT))
	  || (tmp_list == last_column && dir == (rtl ? GTK_DIR_LEFT : GTK_DIR_RIGHT)))
        {
	  gtk_widget_error_bell (GTK_WIDGET (tree_view));
	  break;
	}

      while (tmp_list)
	{
	  if (dir == (rtl ? GTK_DIR_LEFT : GTK_DIR_RIGHT))
	    tmp_list = tmp_list->next;
	  else
	    tmp_list = tmp_list->prev;

	  if (tmp_list == NULL)
	    {
	      g_warning ("Internal button not found");
	      break;
	    }
	  column = tmp_list->data;
	  button = gtk_tree_view_column_get_button (column);
	  if (button &&
	      gtk_tree_view_column_get_visible (column) &&
	      gtk_widget_get_can_focus (button))
	    {
	      focus_child = button;
	      gtk_widget_grab_focus (button);
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
	if (gtk_tree_view_column_get_button (GTK_TREE_VIEW_COLUMN (tmp_list->data)) == focus_child)
	  {
            _gtk_tree_view_set_focus_column (tree_view, GTK_TREE_VIEW_COLUMN (tmp_list->data));
	    break;
	  }

      if (clamp_column_visible)
        {
	  gtk_tree_view_clamp_column_visible (tree_view,
					      tree_view->priv->focus_column,
					      FALSE);
	}
    }

  return (focus_child != NULL);
}

/* This function returns in 'path' the first focusable path, if the given path
 * is already focusable, it’s the returned one.
 */
static gboolean
search_first_focusable_path (GtkTreeView    *tree_view,
			     GtkTreePath   **path,
			     gboolean        search_forward,
			     GtkTreeRBTree **new_tree,
			     GtkTreeRBNode **new_node)
{
  GtkTreeRBTree *tree = NULL;
  GtkTreeRBNode *node = NULL;

  if (!path || !*path)
    return FALSE;

  _gtk_tree_view_find_node (tree_view, *path, &tree, &node);

  if (!tree || !node)
    return FALSE;

  while (node && row_is_separator (tree_view, NULL, *path))
    {
      if (search_forward)
	gtk_tree_rbtree_next_full (tree, node, &tree, &node);
      else
	gtk_tree_rbtree_prev_full (tree, node, &tree, &node);

      if (*path)
	gtk_tree_path_free (*path);

      if (node)
	*path = _gtk_tree_path_new_from_rbtree (tree, node);
      else
	*path = NULL;
    }

  if (new_tree)
    *new_tree = tree;

  if (new_node)
    *new_node = node;

  return (*path != NULL);
}

static gint
gtk_tree_view_focus (GtkWidget        *widget,
		     GtkDirectionType  direction)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkWidget *focus_child;

  if (!gtk_widget_is_sensitive (widget) || !gtk_widget_get_can_focus (widget))
    return FALSE;

  focus_child = gtk_widget_get_focus_child (widget);

  gtk_tree_view_stop_editing (GTK_TREE_VIEW (widget), FALSE);
  /* Case 1.  Headers currently have focus. */
  if (focus_child)
    {
      switch (direction)
	{
	case GTK_DIR_LEFT:
	case GTK_DIR_RIGHT:
	  gtk_tree_view_header_focus (tree_view, direction, TRUE);
	  return TRUE;
	case GTK_DIR_TAB_BACKWARD:
	case GTK_DIR_UP:
	  return FALSE;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_DOWN:
	  return gtk_widget_grab_focus (widget);
	default:
	  g_assert_not_reached ();
	  return FALSE;
	}
    }

  /* Case 2. We don't have focus at all. */
  if (!gtk_widget_has_focus (widget))
    {
      return gtk_widget_grab_focus (widget);
    }

  /* Case 3. We have focus already. */
  if (direction == GTK_DIR_TAB_BACKWARD)
    return (gtk_tree_view_header_focus (tree_view, direction, FALSE));
  else if (direction == GTK_DIR_TAB_FORWARD)
    return FALSE;

  /* Other directions caught by the keybindings */
  return gtk_widget_grab_focus (widget);
}

static gboolean
gtk_tree_view_grab_focus (GtkWidget *widget)
{
  if (!GTK_WIDGET_CLASS (gtk_tree_view_parent_class)->grab_focus (widget))
    return FALSE;

  gtk_tree_view_focus_to_cursor (GTK_TREE_VIEW (widget));
  return TRUE;
}

static void
gtk_tree_view_style_updated (GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkTreeViewPrivate *priv = gtk_tree_view_get_instance_private (tree_view);
  GList *list;
  GtkTreeViewColumn *column;
  GtkStyleContext *style_context;
  GtkCssStyleChange *change;

  GTK_WIDGET_CLASS (gtk_tree_view_parent_class)->style_updated (widget);

  if (gtk_widget_get_realized (widget))
    {
      gtk_tree_view_set_grid_lines (tree_view, priv->grid_lines);
      gtk_tree_view_set_enable_tree_lines (tree_view, priv->tree_lines_enabled);
    }

  style_context = gtk_widget_get_style_context (widget);
  change = gtk_style_context_get_change (style_context);

  if (change == NULL || gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_SIZE))
    {
      for (list = priv->columns; list; list = list->next)
	{
	  column = list->data;
	  _gtk_tree_view_column_cell_set_dirty (column, TRUE);
	}

      priv->fixed_height = -1;
      gtk_tree_rbtree_mark_invalid (priv->tree);
    }

  /* Invalidate expander size */
  priv->expander_size = -1;
}

static gboolean
gtk_tree_view_real_move_cursor (GtkTreeView       *tree_view,
				GtkMovementStep    step,
				gint               count)
{
  GdkModifierType state;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (step == GTK_MOVEMENT_LOGICAL_POSITIONS ||
			step == GTK_MOVEMENT_VISUAL_POSITIONS ||
			step == GTK_MOVEMENT_DISPLAY_LINES ||
			step == GTK_MOVEMENT_PAGES ||
			step == GTK_MOVEMENT_BUFFER_ENDS, FALSE);

  if (tree_view->priv->tree == NULL)
    return FALSE;
  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return FALSE;

  gtk_tree_view_stop_editing (tree_view, FALSE);
  tree_view->priv->draw_keyfocus = TRUE;
  gtk_widget_grab_focus (GTK_WIDGET (tree_view));

  if (gtk_get_current_event_state (&state))
    {
      GdkModifierType extend_mod_mask;
      GdkModifierType modify_mod_mask;

      extend_mod_mask =
        gtk_widget_get_modifier_mask (GTK_WIDGET (tree_view),
                                      GDK_MODIFIER_INTENT_EXTEND_SELECTION);

      modify_mod_mask =
        gtk_widget_get_modifier_mask (GTK_WIDGET (tree_view),
                                      GDK_MODIFIER_INTENT_MODIFY_SELECTION);

      if ((state & modify_mod_mask) == modify_mod_mask)
        tree_view->priv->modify_selection_pressed = TRUE;
      if ((state & extend_mod_mask) == extend_mod_mask)
        tree_view->priv->extend_selection_pressed = TRUE;
    }
  /* else we assume not pressed */

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
    case GTK_MOVEMENT_WORDS:
    case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
    case GTK_MOVEMENT_PARAGRAPHS:
    case GTK_MOVEMENT_PARAGRAPH_ENDS:
    case GTK_MOVEMENT_HORIZONTAL_PAGES:
    default:
      g_assert_not_reached ();
    }

  tree_view->priv->modify_selection_pressed = FALSE;
  tree_view->priv->extend_selection_pressed = FALSE;

  return TRUE;
}

static void
gtk_tree_view_put (GtkTreeView       *tree_view,
		   GtkWidget         *child_widget,
                   GtkTreePath       *path,
                   GtkTreeViewColumn *column,
                   const GtkBorder   *border)
{
  GtkTreeViewChild *child;
  
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_IS_WIDGET (child_widget));

  child = g_slice_new (GtkTreeViewChild);

  child->widget = child_widget;
  if (_gtk_tree_view_find_node (tree_view,
				path,
				&child->tree,
				&child->node))
    {
      g_assert_not_reached ();
    }
  child->column = column;
  child->border = *border;

  tree_view->priv->children = g_list_append (tree_view->priv->children, child);

  gtk_widget_set_parent (child_widget, GTK_WIDGET (tree_view));
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
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
  gboolean free_path = FALSE;
  GList *list;
  GtkTreePath *cursor_path;

  g_return_if_fail (path != NULL || iter != NULL);

  if (tree_view->priv->cursor_node != NULL)
    cursor_path = _gtk_tree_path_new_from_rbtree (tree_view->priv->cursor_tree,
                                                  tree_view->priv->cursor_node);
  else
    cursor_path = NULL;

  if (tree_view->priv->edited_column &&
      (cursor_path == NULL || gtk_tree_path_compare (cursor_path, path) == 0))
    gtk_tree_view_stop_editing (tree_view, TRUE);

  if (cursor_path != NULL)
    gtk_tree_path_free (cursor_path);

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

  _gtk_tree_view_accessible_changed (tree_view, tree, node);

  if (tree_view->priv->fixed_height_mode
      && tree_view->priv->fixed_height >= 0)
    {
      gtk_tree_rbtree_node_set_height (tree, node, tree_view->priv->fixed_height);
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
    }
  else
    {
      gtk_tree_rbtree_node_mark_invalid (tree, node);
      for (list = tree_view->priv->columns; list; list = list->next)
        {
          GtkTreeViewColumn *column;

          column = list->data;
          if (!gtk_tree_view_column_get_visible (column))
            continue;

          if (gtk_tree_view_column_get_sizing (column) == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
            {
              _gtk_tree_view_column_cell_set_dirty (column, TRUE);
            }
        }
    }

 done:
  if (!tree_view->priv->fixed_height_mode &&
      gtk_widget_get_realized (GTK_WIDGET (tree_view)))
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
  GtkTreeRBTree *tree;
  GtkTreeRBNode *tmpnode = NULL;
  gint depth;
  gint i = 0;
  gint height;
  gboolean free_path = FALSE;
  gboolean node_visible = TRUE;

  g_return_if_fail (path != NULL || iter != NULL);

  if (tree_view->priv->fixed_height_mode
      && tree_view->priv->fixed_height >= 0)
    height = tree_view->priv->fixed_height;
  else
    height = 0;

  if (path == NULL)
    {
      path = gtk_tree_model_get_path (model, iter);
      free_path = TRUE;
    }
  else if (iter == NULL)
    gtk_tree_model_get_iter (model, iter, path);

  if (tree_view->priv->tree == NULL)
    tree_view->priv->tree = gtk_tree_rbtree_new ();

  tree = tree_view->priv->tree;

  /* Update all row-references */
  gtk_tree_row_reference_inserted (G_OBJECT (data), path);
  depth = gtk_tree_path_get_depth (path);
  indices = gtk_tree_path_get_indices (path);

  /* First, find the parent tree */
  while (i < depth - 1)
    {
      if (tree == NULL)
	{
	  /* We aren't showing the node */
	  node_visible = FALSE;
          goto done;
	}

      tmpnode = gtk_tree_rbtree_find_count (tree, indices[i] + 1);
      if (tmpnode == NULL)
	{
	  g_warning ("A node was inserted with a parent that's not in the tree.\n" \
		     "This possibly means that a GtkTreeModel inserted a child node\n" \
		     "before the parent was inserted.");
          goto done;
	}
      else if (!GTK_TREE_RBNODE_FLAG_SET (tmpnode, GTK_TREE_RBNODE_IS_PARENT))
	{
          /* FIXME enforce correct behavior on model, probably */
	  /* In theory, the model should have emitted has_child_toggled here.  We
	   * try to catch it anyway, just to be safe, in case the model hasn't.
	   */
	  GtkTreePath *tmppath = _gtk_tree_path_new_from_rbtree (tree, tmpnode);
	  gtk_tree_view_row_has_child_toggled (model, tmppath, NULL, data);
	  gtk_tree_path_free (tmppath);
          goto done;
	}

      tree = tmpnode->children;
      i++;
    }

  if (tree == NULL)
    {
      node_visible = FALSE;
      goto done;
    }

  /* ref the node */
  gtk_tree_model_ref_node (tree_view->priv->model, iter);
  if (indices[depth - 1] == 0)
    {
      tmpnode = gtk_tree_rbtree_find_count (tree, 1);
      tmpnode = gtk_tree_rbtree_insert_before (tree, tmpnode, height, FALSE);
    }
  else
    {
      tmpnode = gtk_tree_rbtree_find_count (tree, indices[depth - 1]);
      tmpnode = gtk_tree_rbtree_insert_after (tree, tmpnode, height, FALSE);
    }

  _gtk_tree_view_accessible_add (tree_view, tree, tmpnode);

 done:
  if (height > 0)
    {
      if (tree)
        gtk_tree_rbtree_node_mark_valid (tree, tmpnode);

      if (node_visible && node_is_visible (tree_view, tree, tmpnode))
	gtk_widget_queue_resize (GTK_WIDGET (tree_view));
      else
        gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }
  else
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
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
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
  if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_PARENT) == has_child)
    goto done;

  if (has_child)
    {
      GTK_TREE_RBNODE_SET_FLAG (node, GTK_TREE_RBNODE_IS_PARENT);
      _gtk_tree_view_accessible_add_state (tree_view, tree, node, GTK_CELL_RENDERER_EXPANDABLE);
    }
  else
    {
      GTK_TREE_RBNODE_UNSET_FLAG (node, GTK_TREE_RBNODE_IS_PARENT);
      _gtk_tree_view_accessible_remove_state (tree_view, tree, node, GTK_CELL_RENDERER_EXPANDABLE);
    }

  if (has_child && tree_view->priv->is_list)
    {
      tree_view->priv->is_list = FALSE;
      if (tree_view->priv->show_expanders)
	{
	  GList *list;

	  for (list = tree_view->priv->columns; list; list = list->next)
	    if (gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (list->data)))
	      {
		_gtk_tree_view_column_cell_set_dirty (GTK_TREE_VIEW_COLUMN (list->data), TRUE);
		break;
	      }
	}
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }
  else
    {
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
    }

 done:
  if (free_path)
    gtk_tree_path_free (path);
}

static void
check_selection_helper (GtkTreeRBTree *tree,
                        GtkTreeRBNode *node,
                        gpointer       data)
{
  gint *value = (gint *)data;

  *value |= GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_SELECTED);

  if (node->children && !*value)
    gtk_tree_rbtree_traverse (node->children, node->children->root, G_POST_ORDER, check_selection_helper, data);
}

static void
gtk_tree_view_row_deleted (GtkTreeModel *model,
			   GtkTreePath  *path,
			   gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *)data;
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
  GList *list;
  gboolean selection_changed = FALSE, cursor_changed = FALSE;
  GtkTreeRBTree *cursor_tree = NULL;
  GtkTreeRBNode *cursor_node = NULL;

  g_return_if_fail (path != NULL);

  gtk_tree_row_reference_deleted (G_OBJECT (data), path);

  if (_gtk_tree_view_find_node (tree_view, path, &tree, &node))
    return;

  if (tree == NULL)
    return;

  /* check if the selection has been changed */
  gtk_tree_rbtree_traverse (tree, node, G_POST_ORDER,
                        check_selection_helper, &selection_changed);

  for (list = tree_view->priv->columns; list; list = list->next)
    if (gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (list->data)) &&
	gtk_tree_view_column_get_sizing (GTK_TREE_VIEW_COLUMN (list->data)) == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
      _gtk_tree_view_column_cell_set_dirty ((GtkTreeViewColumn *)list->data, TRUE);

  /* Ensure we don't have a dangling pointer to a dead node */
  ensure_unprelighted (tree_view);

  /* Cancel editting if we've started */
  gtk_tree_view_stop_editing (tree_view, TRUE);

  /* If the cursor row got deleted, move the cursor to the next row */
  if (tree_view->priv->cursor_node &&
      (tree_view->priv->cursor_node == node ||
       (node->children && (tree_view->priv->cursor_tree == node->children ||
                           gtk_tree_rbtree_contains (node->children, tree_view->priv->cursor_tree)))))
    {
      GtkTreePath *cursor_path;

      cursor_tree = tree;
      cursor_node = gtk_tree_rbtree_next (tree, node);
      /* find the first node that is not going to be deleted */
      while (cursor_node == NULL && cursor_tree->parent_tree)
        {
          cursor_node = gtk_tree_rbtree_next (cursor_tree->parent_tree,
                                          cursor_tree->parent_node);
          cursor_tree = cursor_tree->parent_tree;
        }

      if (cursor_node != NULL)
        cursor_path = _gtk_tree_path_new_from_rbtree (cursor_tree, cursor_node);
      else
        cursor_path = NULL;

      if (cursor_path == NULL ||
          ! search_first_focusable_path (tree_view, &cursor_path, TRUE, 
                                         &cursor_tree, &cursor_node))
        {
          /* It looks like we reached the end of the view without finding
           * a focusable row.  We will step backwards to find the last
           * focusable row.
           */
          gtk_tree_rbtree_prev_full (tree, node, &cursor_tree, &cursor_node);
          if (cursor_node)
            {
              cursor_path = _gtk_tree_path_new_from_rbtree (cursor_tree, cursor_node);
              if (! search_first_focusable_path (tree_view, &cursor_path, FALSE,
                                                 &cursor_tree, &cursor_node))
                cursor_node = NULL;
              gtk_tree_path_free (cursor_path);
            }
        }
      else if (cursor_path)
        gtk_tree_path_free (cursor_path);

      cursor_changed = TRUE;
    }

  if (tree->root->count == 1)
    {
      if (tree_view->priv->tree == tree)
	tree_view->priv->tree = NULL;

      _gtk_tree_view_accessible_remove_state (tree_view,
                                              tree->parent_tree, tree->parent_node,
                                              GTK_CELL_RENDERER_EXPANDED);
      _gtk_tree_view_accessible_remove (tree_view, tree, NULL);
      gtk_tree_rbtree_remove (tree);
    }
  else
    {
      _gtk_tree_view_accessible_remove (tree_view, tree, node);
      gtk_tree_rbtree_remove_node (tree, node);
    }

  if (! gtk_tree_row_reference_valid (tree_view->priv->top_row))
    {
      gtk_tree_row_reference_free (tree_view->priv->top_row);
      tree_view->priv->top_row = NULL;
    }

  install_scroll_sync_handler (tree_view);

  gtk_widget_queue_resize (GTK_WIDGET (tree_view));

  if (cursor_changed)
    {
      if (cursor_node)
        {
          GtkTreePath *cursor_path = _gtk_tree_path_new_from_rbtree (cursor_tree, cursor_node);
          gtk_tree_view_real_set_cursor (tree_view, cursor_path, CLEAR_AND_SELECT | CURSOR_INVALID);
          gtk_tree_path_free (cursor_path);
        }
      else
        gtk_tree_view_real_set_cursor (tree_view, NULL, CLEAR_AND_SELECT | CURSOR_INVALID);
    }
  if (selection_changed)
    g_signal_emit_by_name (tree_view->priv->selection, "changed");
}

static void
gtk_tree_view_rows_reordered (GtkTreeModel *model,
			      GtkTreePath  *parent,
			      GtkTreeIter  *iter,
			      gint         *new_order,
			      gpointer      data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (data);
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
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

  if (tree_view->priv->edited_column)
    gtk_tree_view_stop_editing (tree_view, TRUE);

  /* we need to be unprelighted */
  ensure_unprelighted (tree_view);

  gtk_tree_rbtree_reorder (tree, new_order, len);

  _gtk_tree_view_accessible_reorder (tree_view);

  gtk_widget_queue_draw (GTK_WIDGET (tree_view));

  gtk_tree_view_dy_to_top_row (tree_view);
}


/* Internal tree functions
 */


static void
gtk_tree_view_get_background_xrange (GtkTreeView       *tree_view,
                                     GtkTreeRBTree     *tree,
                                     GtkTreeViewColumn *column,
                                     gint              *x1,
                                     gint              *x2)
{
  GtkTreeViewColumn *tmp_column = NULL;
  gint total_width;
  GList *list;
  gboolean rtl;

  if (x1)
    *x1 = 0;

  if (x2)
    *x2 = 0;

  rtl = (_gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

  total_width = 0;
  for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
       list;
       list = (rtl ? list->prev : list->next))
    {
      tmp_column = list->data;

      if (tmp_column == column)
        break;

      if (gtk_tree_view_column_get_visible (tmp_column))
        total_width += gtk_tree_view_column_get_width (tmp_column);
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
      if (gtk_tree_view_column_get_visible (column))
        *x2 = total_width + gtk_tree_view_column_get_width (column);
      else
        *x2 = total_width; /* width of 0 */
    }
}

static void
gtk_tree_view_get_arrow_xrange (GtkTreeView   *tree_view,
				GtkTreeRBTree *tree,
                                gint          *x1,
                                gint          *x2)
{
  gint x_offset = 0;
  GList *list;
  GtkTreeViewColumn *tmp_column = NULL;
  gint total_width;
  gint expander_size, expander_render_size;
  gboolean rtl;

  rtl = (_gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);
  expander_size = gtk_tree_view_get_expander_size (tree_view);
  expander_render_size = expander_size - (_TREE_VIEW_HORIZONTAL_SEPARATOR / 2);

  total_width = 0;
  for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
       list;
       list = (rtl ? list->prev : list->next))
    {
      tmp_column = list->data;

      if (gtk_tree_view_is_expander_column (tree_view, tmp_column))
        {
	  if (rtl)
	    x_offset = total_width + gtk_tree_view_column_get_width (tmp_column) - expander_size;
	  else
	    x_offset = total_width;
          break;
        }

      if (gtk_tree_view_column_get_visible (tmp_column))
        total_width += gtk_tree_view_column_get_width (tmp_column);
    }

  x_offset += (expander_size - expander_render_size);

  if (rtl)
    x_offset -= expander_size * gtk_tree_rbtree_get_depth (tree);
  else
    x_offset += expander_size * gtk_tree_rbtree_get_depth (tree);

  *x1 = x_offset;

  if (tmp_column &&
      gtk_tree_view_column_get_visible (tmp_column))
    /* +1 because x2 isn't included in the range. */
    *x2 = *x1 + expander_render_size + 1;
  else
    *x2 = *x1;
}

static void
gtk_tree_view_build_tree (GtkTreeView   *tree_view,
			  GtkTreeRBTree *tree,
			  GtkTreeIter   *iter,
			  gint           depth,
			  gboolean       recurse)
{
  GtkTreeRBNode *temp = NULL;
  GtkTreePath *path = NULL;

  do
    {
      gtk_tree_model_ref_node (tree_view->priv->model, iter);
      temp = gtk_tree_rbtree_insert_after (tree, temp, 0, FALSE);

      if (tree_view->priv->fixed_height > 0)
        {
          if (GTK_TREE_RBNODE_FLAG_SET (temp, GTK_TREE_RBNODE_INVALID))
	    {
              gtk_tree_rbtree_node_set_height (tree, temp, tree_view->priv->fixed_height);
	      gtk_tree_rbtree_node_mark_valid (tree, temp);
	    }
        }

      if (tree_view->priv->is_list)
        continue;

      if (recurse)
	{
	  GtkTreeIter child;

	  if (!path)
	    path = gtk_tree_model_get_path (tree_view->priv->model, iter);
	  else
	    gtk_tree_path_next (path);

	  if (gtk_tree_model_iter_children (tree_view->priv->model, &child, iter))
	    {
	      gboolean expand;

	      g_signal_emit (tree_view, tree_view_signals[TEST_EXPAND_ROW], 0, iter, path, &expand);

	      if (gtk_tree_model_iter_has_child (tree_view->priv->model, iter)
		  && !expand)
	        {
	          temp->children = gtk_tree_rbtree_new ();
	          temp->children->parent_tree = tree;
	          temp->children->parent_node = temp;
	          gtk_tree_view_build_tree (tree_view, temp->children, &child, depth + 1, recurse);
		}
	    }
	}

      if (gtk_tree_model_iter_has_child (tree_view->priv->model, iter))
	{
	  if ((temp->flags&GTK_TREE_RBNODE_IS_PARENT) != GTK_TREE_RBNODE_IS_PARENT)
	    temp->flags ^= GTK_TREE_RBNODE_IS_PARENT;
	}
    }
  while (gtk_tree_model_iter_next (tree_view->priv->model, iter));

  if (path)
    gtk_tree_path_free (path);
}

/* Make sure the node is visible vertically */
static void
gtk_tree_view_clamp_node_visible (GtkTreeView   *tree_view,
				  GtkTreeRBTree *tree,
				  GtkTreeRBNode *node)
{
  gint node_dy, height;
  GtkTreePath *path = NULL;

  if (!gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    return;

  /* just return if the node is visible, avoiding a costly expose */
  node_dy = gtk_tree_rbtree_node_find_offset (tree, node);
  height = gtk_tree_view_get_row_height (tree_view, node);
  if (! GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID)
      && node_dy >= gtk_adjustment_get_value (tree_view->priv->vadjustment)
      && node_dy + height <= (gtk_adjustment_get_value (tree_view->priv->vadjustment)
                              + gtk_adjustment_get_page_size (tree_view->priv->vadjustment)))
    return;

  path = _gtk_tree_path_new_from_rbtree (tree, node);
  if (path)
    {
      gtk_tree_view_scroll_to_cell (tree_view, path, NULL, FALSE, 0.0, 0.0);
      gtk_tree_path_free (path);
    }
}

static void
gtk_tree_view_clamp_column_visible (GtkTreeView       *tree_view,
				    GtkTreeViewColumn *column,
				    gboolean           focus_to_cell)
{
  GtkAllocation allocation;
  gint x, width;

  if (column == NULL)
    return;

  gtk_widget_get_allocation (gtk_tree_view_column_get_button (column), &allocation);
  x = allocation.x;
  width = allocation.width;

  if (width > gtk_adjustment_get_page_size (tree_view->priv->hadjustment))
    {
      /* The column is larger than the horizontal page size.  If the
       * column has cells which can be focused individually, then we make
       * sure the cell which gets focus is fully visible (if even the
       * focus cell is bigger than the page size, we make sure the
       * left-hand side of the cell is visible).
       *
       * If the column does not have an activatable cell, we
       * make sure the left-hand side of the column is visible.
       */

      if (focus_to_cell && gtk_tree_view_has_can_focus_cell (tree_view))
        {
          GtkCellArea *cell_area;
          GtkCellRenderer *focus_cell;

          cell_area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));
          focus_cell = gtk_cell_area_get_focus_cell (cell_area);

          if (gtk_tree_view_column_cell_get_position (column, focus_cell,
                                                      &x, &width))
            {
              if (width < gtk_adjustment_get_page_size (tree_view->priv->hadjustment))
                {
                  if (gtk_adjustment_get_value (tree_view->priv->hadjustment) + gtk_adjustment_get_page_size (tree_view->priv->hadjustment) < x + width)
                    gtk_adjustment_set_value (tree_view->priv->hadjustment,
                                              x + width - gtk_adjustment_get_page_size (tree_view->priv->hadjustment));
                  else if (gtk_adjustment_get_value (tree_view->priv->hadjustment) > x)
                    gtk_adjustment_set_value (tree_view->priv->hadjustment, x);
                }
            }
        }

      gtk_adjustment_set_value (tree_view->priv->hadjustment, x);
    }
  else
    {
      if ((gtk_adjustment_get_value (tree_view->priv->hadjustment) + gtk_adjustment_get_page_size (tree_view->priv->hadjustment)) < (x + width))
	  gtk_adjustment_set_value (tree_view->priv->hadjustment,
				    x + width - gtk_adjustment_get_page_size (tree_view->priv->hadjustment));
      else if (gtk_adjustment_get_value (tree_view->priv->hadjustment) > x)
	gtk_adjustment_set_value (tree_view->priv->hadjustment, x);
  }
}

/* This function could be more efficient.  I'll optimize it if profiling seems
 * to imply that it is important */
GtkTreePath *
_gtk_tree_path_new_from_rbtree (GtkTreeRBTree *tree,
			        GtkTreeRBNode *node)
{
  GtkTreePath *path;
  GtkTreeRBTree *tmp_tree;
  GtkTreeRBNode *tmp_node, *last;
  gint count;

  path = gtk_tree_path_new ();

  g_return_val_if_fail (node != NULL, path);

  count = 1 + node->left->count;

  last = node;
  tmp_node = node->parent;
  tmp_tree = tree;
  while (tmp_tree)
    {
      while (!gtk_tree_rbtree_is_nil (tmp_node))
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
 * invalid (ie. points to a node that’s not in the tree), *tree and *node are
 * both set to NULL.
 */
gboolean
_gtk_tree_view_find_node (GtkTreeView    *tree_view,
			  GtkTreePath    *path,
			  GtkTreeRBTree **tree,
			  GtkTreeRBNode **node)
{
  GtkTreeRBNode *tmpnode = NULL;
  GtkTreeRBTree *tmptree = tree_view->priv->tree;
  gint *indices = gtk_tree_path_get_indices (path);
  gint depth = gtk_tree_path_get_depth (path);
  gint i = 0;

  *node = NULL;
  *tree = NULL;

  if (depth == 0 || tmptree == NULL)
    return FALSE;
  do
    {
      tmpnode = gtk_tree_rbtree_find_count (tmptree, indices[i] + 1);
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

  if (tree_view->priv->is_list)
    return FALSE;

  if (tree_view->priv->expander_column != NULL)
    {
      if (tree_view->priv->expander_column == column)
	return TRUE;
      return FALSE;
    }
  else
    {
      for (list = tree_view->priv->columns;
	   list;
	   list = list->next)
	if (gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (list->data)))
	  break;
      if (list && list->data == column)
	return TRUE;
    }
  return FALSE;
}

static inline gboolean
gtk_tree_view_draw_expanders (GtkTreeView *tree_view)
{
  if (!tree_view->priv->is_list && tree_view->priv->show_expanders)
    return TRUE;
  /* else */
  return FALSE;
}

static void
gtk_tree_view_add_move_binding (GtkBindingSet  *binding_set,
				guint           keyval,
				guint           modmask,
				gboolean        add_shifted_binding,
				GtkMovementStep step,
				gint            count)
{
  
  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
                                "move-cursor", 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);

  if (add_shifted_binding)
    gtk_binding_entry_add_signal (binding_set, keyval, GDK_SHIFT_MASK,
				  "move-cursor", 2,
				  G_TYPE_ENUM, step,
				  G_TYPE_INT, count);

  if ((modmask & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
   return;

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "move-cursor", 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_CONTROL_MASK,
                                "move-cursor", 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);
}

static gint
gtk_tree_view_unref_tree_helper (GtkTreeModel  *model,
				 GtkTreeIter   *iter,
				 GtkTreeRBTree *tree,
				 GtkTreeRBNode *node)
{
  gint retval = FALSE;
  do
    {
      g_return_val_if_fail (node != NULL, FALSE);

      if (node->children)
	{
	  GtkTreeIter child;
	  GtkTreeRBTree *new_tree;
	  GtkTreeRBNode *new_node;

	  new_tree = node->children;
          new_node = gtk_tree_rbtree_first (new_tree);

	  if (!gtk_tree_model_iter_children (model, &child, iter))
	    return FALSE;

	  retval = gtk_tree_view_unref_tree_helper (model, &child, new_tree, new_node) | retval;
	}

      if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_SELECTED))
	retval = TRUE;
      gtk_tree_model_unref_node (model, iter);
      node = gtk_tree_rbtree_next (tree, node);
    }
  while (gtk_tree_model_iter_next (model, iter));

  return retval;
}

static gint
gtk_tree_view_unref_and_check_selection_tree (GtkTreeView   *tree_view,
					      GtkTreeRBTree *tree)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeRBNode *node;
  gint retval;

  if (!tree)
    return FALSE;

  node = gtk_tree_rbtree_first (tree);

  g_return_val_if_fail (node != NULL, FALSE);
  path = _gtk_tree_path_new_from_rbtree (tree, node);
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
  gboolean rtl;
  GList *tmp_list;
  gint left;

  /* We want to precalculate the motion list such that we know what column slots
   * are available.
   */
  left_column = NULL;
  rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

  /* First, identify all possible drop spots */
  if (rtl)
    tmp_list = g_list_last (tree_view->priv->columns);
  else
    tmp_list = g_list_first (tree_view->priv->columns);

  while (tmp_list)
    {
      cur_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);
      tmp_list = rtl ? tmp_list->prev : tmp_list->next;

      if (gtk_tree_view_column_get_visible (cur_column) == FALSE)
	continue;

      /* If it's not the column moving and func tells us to skip over the column, we continue. */
      if (left_column != column && cur_column != column &&
	  tree_view->priv->column_drop_func &&
	  ! tree_view->priv->column_drop_func (tree_view, column, left_column, cur_column, tree_view->priv->column_drop_func_data))
	{
	  left_column = cur_column;
	  continue;
	}
      reorder = g_slice_new0 (GtkTreeViewColumnReorder);
      reorder->left_column = left_column;
      left_column = reorder->right_column = cur_column;

      tree_view->priv->column_drag_info = g_list_append (tree_view->priv->column_drag_info, reorder);
    }

  /* Add the last one */
  if (tree_view->priv->column_drop_func == NULL ||
      ((left_column != column) &&
       tree_view->priv->column_drop_func (tree_view, column, left_column, NULL, tree_view->priv->column_drop_func_data)))
    {
      reorder = g_slice_new0 (GtkTreeViewColumnReorder);
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
  if (tree_view->priv->column_drag_info->next == NULL || 
      (tree_view->priv->column_drag_info->next->next == NULL &&
       ((GtkTreeViewColumnReorder *)tree_view->priv->column_drag_info->data)->right_column == column &&
       ((GtkTreeViewColumnReorder *)tree_view->priv->column_drag_info->next->data)->left_column == column))
    {
      for (tmp_list = tree_view->priv->column_drag_info; tmp_list; tmp_list = tmp_list->next)
	g_slice_free (GtkTreeViewColumnReorder, tmp_list->data);
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
          GtkAllocation right_allocation, left_allocation;
	  GtkWidget    *left_button, *right_button;

	  g_assert (tmp_list->next->data);

	  right_button = gtk_tree_view_column_get_button (reorder->right_column);
	  left_button  = gtk_tree_view_column_get_button
	    (((GtkTreeViewColumnReorder *)tmp_list->next->data)->left_column);

          gtk_widget_get_allocation (right_button, &right_allocation);
          gtk_widget_get_allocation (left_button, &left_allocation);
	  left = reorder->right_align = (right_allocation.x + right_allocation.width + left_allocation.x) / 2;
	}
      else
	{
	  reorder->right_align = gtk_widget_get_allocated_width (GTK_WIDGET (tree_view))
                                 + TREE_VIEW_COLUMN_DRAG_DEAD_MULTIPLIER (tree_view);
	}
    }
}

void
_gtk_tree_view_column_start_drag (GtkTreeView       *tree_view,
				  GtkTreeViewColumn *column,
                                  GdkDevice         *device)
{
  GtkAllocation button_allocation;
  GtkWidget *button;
  GtkStyleContext *context;

  g_return_if_fail (tree_view->priv->column_drag_info == NULL);
  g_return_if_fail (tree_view->priv->cur_reorder == NULL);

  gtk_tree_view_set_column_drag_info (tree_view, column);

  if (tree_view->priv->column_drag_info == NULL)
    return;

  button = gtk_tree_view_column_get_button (column);

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_DND);

  gtk_widget_get_allocation (button, &button_allocation);
  tree_view->priv->drag_column_x = button_allocation.x;
  tree_view->priv->drag_column_y = button_allocation.y;

  tree_view->priv->drag_column = column;

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));

  tree_view->priv->in_column_drag = TRUE;

  gtk_gesture_set_state (tree_view->priv->column_drag_gesture,
                         GTK_EVENT_SEQUENCE_CLAIMED);
}

static inline gint
gtk_tree_view_get_effective_header_height (GtkTreeView *tree_view)
{
  if (tree_view->priv->headers_visible)
    return tree_view->priv->header_height;
  else
    return 0;
}

void
_gtk_tree_view_get_row_separator_func (GtkTreeView                 *tree_view,
				       GtkTreeViewRowSeparatorFunc *func,
				       gpointer                    *data)
{
  *func = tree_view->priv->row_separator_func;
  *data = tree_view->priv->row_separator_data;
}

GtkTreePath *
_gtk_tree_view_get_anchor_path (GtkTreeView *tree_view)
{
  if (tree_view->priv->anchor)
    return gtk_tree_row_reference_get_path (tree_view->priv->anchor);

  return NULL;
}

void
_gtk_tree_view_set_anchor_path (GtkTreeView *tree_view,
				GtkTreePath *anchor_path)
{
  if (tree_view->priv->anchor)
    {
      gtk_tree_row_reference_free (tree_view->priv->anchor);
      tree_view->priv->anchor = NULL;
    }

  if (anchor_path && tree_view->priv->model)
    tree_view->priv->anchor =
      gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view), 
					tree_view->priv->model, anchor_path);
}

GtkTreeRBTree *
_gtk_tree_view_get_rbtree (GtkTreeView *tree_view)
{
  return tree_view->priv->tree;
}

gboolean
_gtk_tree_view_get_cursor_node (GtkTreeView    *tree_view,
                                GtkTreeRBTree **tree,
                                GtkTreeRBNode **node)
{
  GtkTreeViewPrivate *priv;

  priv = tree_view->priv;

  if (priv->cursor_node == NULL)
    return FALSE;

  *tree = priv->cursor_tree;
  *node = priv->cursor_node;

  return TRUE;
}

GtkTreeViewColumn *
_gtk_tree_view_get_focus_column (GtkTreeView *tree_view)
{
  return tree_view->priv->focus_column;
}

void
_gtk_tree_view_set_focus_column (GtkTreeView       *tree_view,
				 GtkTreeViewColumn *column)
{
  GtkTreeViewColumn *old_column = tree_view->priv->focus_column;

  if (old_column == column)
    return;

  tree_view->priv->focus_column = column;

  _gtk_tree_view_accessible_update_focus_column (tree_view, 
                                                 old_column,
                                                 column);
}

/* x and y are the mouse position
 */
static void
gtk_tree_view_snapshot_arrow (GtkTreeView   *tree_view,
                              GtkSnapshot   *snapshot,
                              GtkTreeRBTree *tree,
                              GtkTreeRBNode *node)
{
  GdkRectangle area;
  GtkStateFlags state = 0;
  GtkStyleContext *context;
  GtkWidget *widget;
  gint x_offset = 0;
  gint x2;
  GtkCellRendererState flags = 0;
  GtkCssImageBuiltinType image_type;
  gboolean rtl;

  widget = GTK_WIDGET (tree_view);
  context = gtk_widget_get_style_context (widget);
  rtl = (_gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

  if (! GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_PARENT))
    return;

  gtk_tree_view_get_arrow_xrange (tree_view, tree, &x_offset, &x2);

  area.x = x_offset;
  area.y = gtk_tree_view_get_cell_area_y_offset (tree_view, tree, node);
  area.width = x2 - x_offset;
  area.height = gtk_tree_view_get_cell_area_height (tree_view, node);

  if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_SELECTED))
    flags |= GTK_CELL_RENDERER_SELECTED;

  if (node == tree_view->priv->prelight_node &&
      tree_view->priv->arrow_prelit)
    flags |= GTK_CELL_RENDERER_PRELIT;

  state = gtk_cell_renderer_get_state (NULL, widget, flags);

  if (node->children != NULL)
    {
      state |= GTK_STATE_FLAG_CHECKED;
      image_type = rtl ? GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_RIGHT_EXPANDED 
                       : GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_LEFT_EXPANDED;
    }
  else
    {
      state &= ~(GTK_STATE_FLAG_CHECKED);
      image_type = rtl ? GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_RIGHT
                       : GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_LEFT;
    }

  gtk_style_context_save (context);

  gtk_style_context_set_state (context, state);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_EXPANDER);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (area.x, area.y));
  gtk_css_style_snapshot_icon (gtk_style_context_lookup_style (context), snapshot,
                               area.width, area.height, image_type);
  gtk_snapshot_restore (snapshot);

  gtk_style_context_restore (context);
}

static void
gtk_tree_view_focus_to_cursor (GtkTreeView *tree_view)

{
  GtkTreePath *cursor_path;

  if ((tree_view->priv->tree == NULL) ||
      (! gtk_widget_get_realized (GTK_WIDGET (tree_view))))
    return;

  cursor_path = NULL;
  if (tree_view->priv->cursor_node)
    cursor_path = _gtk_tree_path_new_from_rbtree (tree_view->priv->cursor_tree,
                                                  tree_view->priv->cursor_node);

  if (cursor_path == NULL)
    {
      /* Consult the selection before defaulting to the
       * first focusable element
       */
      GList *selected_rows;
      GtkTreeModel *model;
      GtkTreeSelection *selection;

      selection = gtk_tree_view_get_selection (tree_view);
      selected_rows = gtk_tree_selection_get_selected_rows (selection, &model);

      if (selected_rows)
	{
          cursor_path = gtk_tree_path_copy((const GtkTreePath *)(selected_rows->data));
	  g_list_free_full (selected_rows, (GDestroyNotify) gtk_tree_path_free);
        }
      else
	{
	  cursor_path = gtk_tree_path_new_first ();
	  search_first_focusable_path (tree_view, &cursor_path,
				       TRUE, NULL, NULL);
	}

      if (cursor_path)
	{
	  if (gtk_tree_selection_get_mode (tree_view->priv->selection) == GTK_SELECTION_MULTIPLE)
	    gtk_tree_view_real_set_cursor (tree_view, cursor_path, 0);
	  else
	    gtk_tree_view_real_set_cursor (tree_view, cursor_path, CLEAR_AND_SELECT);
	}
    }

  if (cursor_path)
    {
      tree_view->priv->draw_keyfocus = TRUE;

      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
      gtk_tree_path_free (cursor_path);

      if (tree_view->priv->focus_column == NULL)
	{
	  GList *list;
	  for (list = tree_view->priv->columns; list; list = list->next)
	    {
	      if (gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (list->data)))
		{
		  GtkCellArea *cell_area;

                  _gtk_tree_view_set_focus_column (tree_view, GTK_TREE_VIEW_COLUMN (list->data));

		  /* This happens when the treeview initially grabs focus and there
		   * is no column in focus, here we explicitly focus into the first cell */
		  cell_area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (tree_view->priv->focus_column));
		  if (!gtk_cell_area_get_focus_cell (cell_area))
                    {
                      gboolean rtl;

                      rtl = (_gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);
                      gtk_cell_area_focus (cell_area,
                                           rtl ? GTK_DIR_LEFT : GTK_DIR_RIGHT);
                    }

		  break;
		}
	    }
	}
    }
}

static void
gtk_tree_view_move_cursor_up_down (GtkTreeView *tree_view,
				   gint         count)
{
  gint selection_count;
  GtkTreeRBTree *new_cursor_tree = NULL;
  GtkTreeRBNode *new_cursor_node = NULL;
  GtkTreePath *cursor_path = NULL;
  gboolean selectable;
  GtkDirectionType direction;
  GtkCellArea *cell_area = NULL;
  GtkCellRenderer *last_focus_cell = NULL;
  GtkTreeIter iter;

  if (tree_view->priv->cursor_node == NULL)
    return;

  cursor_path = _gtk_tree_path_new_from_rbtree (tree_view->priv->cursor_tree,
                                                tree_view->priv->cursor_node);

  direction = count < 0 ? GTK_DIR_UP : GTK_DIR_DOWN;

  if (tree_view->priv->focus_column)
    cell_area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (tree_view->priv->focus_column));

  /* If focus stays in the area for this row, then just return for this round */
  if (cell_area && (count == -1 || count == 1) &&
      gtk_tree_model_get_iter (tree_view->priv->model, &iter, cursor_path))
    {
      gtk_tree_view_column_cell_set_cell_data (tree_view->priv->focus_column,
					       tree_view->priv->model,
                                               &iter,
                                               GTK_TREE_RBNODE_FLAG_SET (tree_view->priv->cursor_node, GTK_TREE_RBNODE_IS_PARENT),
					       tree_view->priv->cursor_node->children ? TRUE : FALSE);

      /* Save the last cell that had focus, if we hit the end of the view we'll give
       * focus back to it. */
      last_focus_cell = gtk_cell_area_get_focus_cell (cell_area);

      /* If focus stays in the area, no need to change the cursor row */
      if (gtk_cell_area_focus (cell_area, direction))
	return;
    }

  selection_count = gtk_tree_selection_count_selected_rows (tree_view->priv->selection);
  selectable = _gtk_tree_selection_row_is_selectable (tree_view->priv->selection,
						      tree_view->priv->cursor_node,
						      cursor_path);

  if (selection_count == 0
      && gtk_tree_selection_get_mode (tree_view->priv->selection) != GTK_SELECTION_NONE
      && !tree_view->priv->modify_selection_pressed
      && selectable)
    {
      /* Don't move the cursor, but just select the current node */
      new_cursor_tree = tree_view->priv->cursor_tree;
      new_cursor_node = tree_view->priv->cursor_node;
    }
  else
    {
      if (count == -1)
	gtk_tree_rbtree_prev_full (tree_view->priv->cursor_tree, tree_view->priv->cursor_node,
			       &new_cursor_tree, &new_cursor_node);
      else
	gtk_tree_rbtree_next_full (tree_view->priv->cursor_tree, tree_view->priv->cursor_node,
			       &new_cursor_tree, &new_cursor_node);
    }

  gtk_tree_path_free (cursor_path);

  if (new_cursor_node)
    {
      cursor_path = _gtk_tree_path_new_from_rbtree (new_cursor_tree, new_cursor_node);

      search_first_focusable_path (tree_view, &cursor_path,
				   (count != -1),
				   &new_cursor_tree,
				   &new_cursor_node);

      if (cursor_path)
	gtk_tree_path_free (cursor_path);
    }

  /*
   * If the list has only one item and multi-selection is set then select
   * the row (if not yet selected).
   */
  if (gtk_tree_selection_get_mode (tree_view->priv->selection) == GTK_SELECTION_MULTIPLE &&
      new_cursor_node == NULL)
    {
      if (count == -1)
        gtk_tree_rbtree_next_full (tree_view->priv->cursor_tree, tree_view->priv->cursor_node,
    			       &new_cursor_tree, &new_cursor_node);
      else
        gtk_tree_rbtree_prev_full (tree_view->priv->cursor_tree, tree_view->priv->cursor_node,
			       &new_cursor_tree, &new_cursor_node);

      if (new_cursor_node == NULL
	  && !GTK_TREE_RBNODE_FLAG_SET (tree_view->priv->cursor_node, GTK_TREE_RBNODE_IS_SELECTED))
        {
          new_cursor_node = tree_view->priv->cursor_node;
          new_cursor_tree = tree_view->priv->cursor_tree;
        }
      else
        {
          new_cursor_tree = NULL;
          new_cursor_node = NULL;
        }
    }

  if (new_cursor_node)
    {
      cursor_path = _gtk_tree_path_new_from_rbtree (new_cursor_tree, new_cursor_node);
      gtk_tree_view_real_set_cursor (tree_view, cursor_path, CLEAR_AND_SELECT | CLAMP_NODE);
      gtk_tree_path_free (cursor_path);

      /* Give focus to the area in the new row */
      if (cell_area)
	gtk_cell_area_focus (cell_area, direction);
    }
  else
    {
      gtk_tree_view_clamp_node_visible (tree_view, 
                                        tree_view->priv->cursor_tree,
                                        tree_view->priv->cursor_node);

      if (!tree_view->priv->extend_selection_pressed)
        {
          if (! gtk_widget_keynav_failed (GTK_WIDGET (tree_view),
                                          count < 0 ?
                                          GTK_DIR_UP : GTK_DIR_DOWN))
            {
              GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (tree_view)));

              if (toplevel)
                gtk_widget_child_focus (toplevel,
                                        count < 0 ?
                                        GTK_DIR_TAB_BACKWARD :
                                        GTK_DIR_TAB_FORWARD);
            }
        }
      else
        {
          gtk_widget_error_bell (GTK_WIDGET (tree_view));
        }

      if (cell_area)
	gtk_cell_area_set_focus_cell (cell_area, last_focus_cell);
    }
}

static void
gtk_tree_view_move_cursor_page_up_down (GtkTreeView *tree_view,
					gint         count)
{
  GtkTreePath *old_cursor_path = NULL;
  GtkTreePath *cursor_path = NULL;
  GtkTreeRBTree *start_cursor_tree = NULL;
  GtkTreeRBNode *start_cursor_node = NULL;
  GtkTreeRBTree *cursor_tree;
  GtkTreeRBNode *cursor_node;
  gint y;
  gint window_y;

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return;

  if (tree_view->priv->cursor_node == NULL)
    return;

  old_cursor_path = _gtk_tree_path_new_from_rbtree (tree_view->priv->cursor_tree,
                                                    tree_view->priv->cursor_node);

  y = gtk_tree_rbtree_node_find_offset (tree_view->priv->cursor_tree, tree_view->priv->cursor_node);
  window_y = RBTREE_Y_TO_TREE_WINDOW_Y (tree_view, y);
  y += tree_view->priv->cursor_offset;
  y += count * (int)gtk_adjustment_get_page_increment (tree_view->priv->vadjustment);
  y = CLAMP (y, (gint)gtk_adjustment_get_lower (tree_view->priv->vadjustment),  (gint)gtk_adjustment_get_upper (tree_view->priv->vadjustment));

  if (y >= gtk_tree_view_get_height (tree_view))
    y = gtk_tree_view_get_height (tree_view) - 1;

  tree_view->priv->cursor_offset =
    gtk_tree_rbtree_find_offset (tree_view->priv->tree, y,
			     &cursor_tree, &cursor_node);

  if (cursor_tree == NULL)
    {
      /* FIXME: we lost the cursor.  Should we try to get one? */
      gtk_tree_path_free (old_cursor_path);
      return;
    }

  if (tree_view->priv->cursor_offset
      > gtk_tree_view_get_row_height (tree_view, cursor_node))
    {
      gtk_tree_rbtree_next_full (cursor_tree, cursor_node,
			     &cursor_tree, &cursor_node);
      tree_view->priv->cursor_offset -= gtk_tree_view_get_row_height (tree_view, cursor_node);
    }

  y -= tree_view->priv->cursor_offset;
  cursor_path = _gtk_tree_path_new_from_rbtree (cursor_tree, cursor_node);

  start_cursor_tree = cursor_tree;
  start_cursor_node = cursor_node;

  if (! search_first_focusable_path (tree_view, &cursor_path,
				     (count != -1),
				     &cursor_tree, &cursor_node))
    {
      /* It looks like we reached the end of the view without finding
       * a focusable row.  We will step backwards to find the last
       * focusable row.
       */
      cursor_tree = start_cursor_tree;
      cursor_node = start_cursor_node;
      cursor_path = _gtk_tree_path_new_from_rbtree (cursor_tree, cursor_node);

      search_first_focusable_path (tree_view, &cursor_path,
				   (count == -1),
				   &cursor_tree, &cursor_node);
    }

  if (!cursor_path)
    goto cleanup;

  /* update y */
  y = gtk_tree_rbtree_node_find_offset (cursor_tree, cursor_node);

  gtk_tree_view_real_set_cursor (tree_view, cursor_path, CLEAR_AND_SELECT);

  y -= window_y;
  gtk_tree_view_scroll_to_point (tree_view, -1, y);
  gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);
  gtk_widget_queue_draw (GTK_WIDGET (tree_view));

  if (!gtk_tree_path_compare (old_cursor_path, cursor_path))
    gtk_widget_error_bell (GTK_WIDGET (tree_view));

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));

cleanup:
  gtk_tree_path_free (old_cursor_path);
  gtk_tree_path_free (cursor_path);
}

static void
gtk_tree_view_move_cursor_left_right (GtkTreeView *tree_view,
				      gint         count)
{
  GtkTreePath *cursor_path = NULL;
  GtkTreeViewColumn *column;
  GtkTreeIter iter;
  GList *list;
  gboolean found_column = FALSE;
  gboolean rtl;
  GtkDirectionType direction;
  GtkCellArea     *cell_area;
  GtkCellRenderer *last_focus_cell = NULL;
  GtkCellArea     *last_focus_area = NULL;

  rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return;

  if (tree_view->priv->cursor_node == NULL)
    return;

  cursor_path = _gtk_tree_path_new_from_rbtree (tree_view->priv->cursor_tree,
                                                tree_view->priv->cursor_node);

  if (gtk_tree_model_get_iter (tree_view->priv->model, &iter, cursor_path) == FALSE)
    {
      gtk_tree_path_free (cursor_path);
      return;
    }
  gtk_tree_path_free (cursor_path);

  list = rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns);
  if (tree_view->priv->focus_column)
    {
      /* Save the cell/area we are moving focus from, if moving the cursor
       * by one step hits the end we'll set focus back here */
      last_focus_area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (tree_view->priv->focus_column));
      last_focus_cell = gtk_cell_area_get_focus_cell (last_focus_area);

      for (; list; list = (rtl ? list->prev : list->next))
	{
	  if (list->data == tree_view->priv->focus_column)
	    break;
	}
    }

  direction = count > 0 ? GTK_DIR_RIGHT : GTK_DIR_LEFT;

  while (list)
    {
      column = list->data;
      if (gtk_tree_view_column_get_visible (column) == FALSE)
	goto loop_end;

      gtk_tree_view_column_cell_set_cell_data (column,
					       tree_view->priv->model,
					       &iter,
					       GTK_TREE_RBNODE_FLAG_SET (tree_view->priv->cursor_node, GTK_TREE_RBNODE_IS_PARENT),
					       tree_view->priv->cursor_node->children ? TRUE : FALSE);

      cell_area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));
      if (gtk_cell_area_focus (cell_area, direction))
	{
          _gtk_tree_view_set_focus_column (tree_view, column);
	  found_column = TRUE;
	  break;
	}

    loop_end:
      if (count == 1)
	list = rtl ? list->prev : list->next;
      else
	list = rtl ? list->next : list->prev;
    }

  if (found_column)
    {
      if (!gtk_tree_view_has_can_focus_cell (tree_view))
        gtk_widget_queue_draw (GTK_WIDGET (tree_view));
      g_signal_emit (tree_view, tree_view_signals[CURSOR_CHANGED], 0);
      gtk_widget_grab_focus (GTK_WIDGET (tree_view));
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (tree_view));

      if (last_focus_area)
	gtk_cell_area_set_focus_cell (last_focus_area, last_focus_cell);
    }

  gtk_tree_view_clamp_column_visible (tree_view,
				      tree_view->priv->focus_column, TRUE);
}

static void
gtk_tree_view_move_cursor_start_end (GtkTreeView *tree_view,
				     gint         count)
{
  GtkTreeRBTree *cursor_tree;
  GtkTreeRBNode *cursor_node;
  GtkTreePath *path;
  GtkTreePath *old_path;

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return;

  g_return_if_fail (tree_view->priv->tree != NULL);

  gtk_tree_view_get_cursor (tree_view, &old_path, NULL);

  cursor_tree = tree_view->priv->tree;

  if (count == -1)
    {
      cursor_node = gtk_tree_rbtree_first (cursor_tree);

      /* Now go forward to find the first focusable row. */
      path = _gtk_tree_path_new_from_rbtree (cursor_tree, cursor_node);
      search_first_focusable_path (tree_view, &path,
				   TRUE, &cursor_tree, &cursor_node);
    }
  else
    {
      cursor_node = cursor_tree->root;

      do
	{
	  while (cursor_node && !gtk_tree_rbtree_is_nil (cursor_node->right))
	    cursor_node = cursor_node->right;
	  if (cursor_node->children == NULL)
	    break;

	  cursor_tree = cursor_node->children;
	  cursor_node = cursor_tree->root;
	}
      while (1);

      /* Now go backwards to find last focusable row. */
      path = _gtk_tree_path_new_from_rbtree (cursor_tree, cursor_node);
      search_first_focusable_path (tree_view, &path,
				   FALSE, &cursor_tree, &cursor_node);
    }

  if (!path)
    goto cleanup;

  if (gtk_tree_path_compare (old_path, path))
    {
      gtk_tree_view_real_set_cursor (tree_view, path, CLEAR_AND_SELECT | CLAMP_NODE);
      gtk_widget_grab_focus (GTK_WIDGET (tree_view));
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (tree_view));
    }

cleanup:
  gtk_tree_path_free (old_path);
  gtk_tree_path_free (path);
}

static gboolean
gtk_tree_view_real_select_all (GtkTreeView *tree_view)
{
  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return FALSE;

  if (gtk_tree_selection_get_mode (tree_view->priv->selection) != GTK_SELECTION_MULTIPLE)
    return FALSE;

  gtk_tree_selection_select_all (tree_view->priv->selection);

  return TRUE;
}

static gboolean
gtk_tree_view_real_unselect_all (GtkTreeView *tree_view)
{
  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return FALSE;

  if (gtk_tree_selection_get_mode (tree_view->priv->selection) != GTK_SELECTION_MULTIPLE)
    return FALSE;

  gtk_tree_selection_unselect_all (tree_view->priv->selection);

  return TRUE;
}

static gboolean
gtk_tree_view_real_select_cursor_row (GtkTreeView *tree_view,
				      gboolean     start_editing)
{
  GtkTreeRBTree *new_tree = NULL;
  GtkTreeRBNode *new_node = NULL;
  GtkTreeRBTree *cursor_tree = NULL;
  GtkTreeRBNode *cursor_node = NULL;
  GtkTreePath *cursor_path = NULL;
  GtkTreeSelectMode mode = 0;

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return FALSE;

  if (tree_view->priv->cursor_node == NULL)
    return FALSE;

  cursor_path = _gtk_tree_path_new_from_rbtree (tree_view->priv->cursor_tree,
                                                tree_view->priv->cursor_node);

  _gtk_tree_view_find_node (tree_view, cursor_path,
			    &cursor_tree, &cursor_node);

  if (cursor_tree == NULL)
    {
      gtk_tree_path_free (cursor_path);
      return FALSE;
    }

  if (!tree_view->priv->extend_selection_pressed && start_editing &&
      tree_view->priv->focus_column)
    {
      if (gtk_tree_view_start_editing (tree_view, cursor_path, FALSE))
	{
	  gtk_tree_path_free (cursor_path);
	  return TRUE;
	}
    }

  if (tree_view->priv->modify_selection_pressed)
    mode |= GTK_TREE_SELECT_MODE_TOGGLE;
  if (tree_view->priv->extend_selection_pressed)
    mode |= GTK_TREE_SELECT_MODE_EXTEND;

  _gtk_tree_selection_internal_select_node (tree_view->priv->selection,
					    cursor_node,
					    cursor_tree,
					    cursor_path,
                                            mode,
					    FALSE);

  /* We bail out if the original (tree, node) don't exist anymore after
   * handling the selection-changed callback.  We do return TRUE because
   * the key press has been handled at this point.
   */
  _gtk_tree_view_find_node (tree_view, cursor_path, &new_tree, &new_node);

  if (cursor_tree != new_tree || cursor_node != new_node)
    return FALSE;

  gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));
  gtk_widget_queue_draw (GTK_WIDGET (tree_view));

  if (!tree_view->priv->extend_selection_pressed)
    gtk_tree_view_row_activated (tree_view, cursor_path,
                                 tree_view->priv->focus_column);
    
  gtk_tree_path_free (cursor_path);

  return TRUE;
}

static gboolean
gtk_tree_view_real_toggle_cursor_row (GtkTreeView *tree_view)
{
  GtkTreeRBTree *new_tree = NULL;
  GtkTreeRBNode *new_node = NULL;
  GtkTreePath *cursor_path = NULL;

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return FALSE;

  if (tree_view->priv->cursor_node == NULL)
    return FALSE;

  cursor_path = _gtk_tree_path_new_from_rbtree (tree_view->priv->cursor_tree,
                                                tree_view->priv->cursor_node);

  _gtk_tree_selection_internal_select_node (tree_view->priv->selection,
					    tree_view->priv->cursor_node,
					    tree_view->priv->cursor_tree,
					    cursor_path,
                                            GTK_TREE_SELECT_MODE_TOGGLE,
					    FALSE);

  /* We bail out if the original (tree, node) don't exist anymore after
   * handling the selection-changed callback.  We do return TRUE because
   * the key press has been handled at this point.
   */
  _gtk_tree_view_find_node (tree_view, cursor_path, &new_tree, &new_node);

  if (tree_view->priv->cursor_node != new_node)
    return FALSE;

  gtk_tree_view_clamp_node_visible (tree_view,
                                    tree_view->priv->cursor_tree,
                                    tree_view->priv->cursor_node);

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));
  gtk_widget_queue_draw (GTK_WIDGET (tree_view));
  gtk_tree_path_free (cursor_path);

  return TRUE;
}

static gboolean
gtk_tree_view_real_expand_collapse_cursor_row (GtkTreeView *tree_view,
					       gboolean     logical,
					       gboolean     expand,
					       gboolean     open_all)
{
  GtkTreePath *cursor_path = NULL;

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return FALSE;

  if (tree_view->priv->cursor_node == NULL)
    return FALSE;

  cursor_path = _gtk_tree_path_new_from_rbtree (tree_view->priv->cursor_tree,
                                                tree_view->priv->cursor_node);

  /* Don't handle the event if we aren't an expander */
  if (!GTK_TREE_RBNODE_FLAG_SET (tree_view->priv->cursor_node, GTK_TREE_RBNODE_IS_PARENT))
    return FALSE;

  if (!logical
      && gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL)
    expand = !expand;

  if (expand)
    gtk_tree_view_real_expand_row (tree_view,
                                   cursor_path,
                                   tree_view->priv->cursor_tree,
                                   tree_view->priv->cursor_node,
                                   open_all,
                                   TRUE);
  else
    gtk_tree_view_real_collapse_row (tree_view,
                                     cursor_path,
                                     tree_view->priv->cursor_tree,
                                     tree_view->priv->cursor_node,
                                     TRUE);

  gtk_tree_path_free (cursor_path);

  return TRUE;
}

static gboolean
gtk_tree_view_real_select_cursor_parent (GtkTreeView *tree_view)
{
  GtkTreePath *cursor_path = NULL;
  GdkModifierType state;

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    goto out;

  if (tree_view->priv->cursor_node == NULL)
    goto out;

  cursor_path = _gtk_tree_path_new_from_rbtree (tree_view->priv->cursor_tree,
                                                tree_view->priv->cursor_node);

  if (tree_view->priv->cursor_tree->parent_node)
    {
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));

      gtk_tree_path_up (cursor_path);

      if (gtk_get_current_event_state (&state))
	{
          GdkModifierType modify_mod_mask;

          modify_mod_mask =
            gtk_widget_get_modifier_mask (GTK_WIDGET (tree_view),
                                          GDK_MODIFIER_INTENT_MODIFY_SELECTION);

	  if ((state & modify_mod_mask) == modify_mod_mask)
	    tree_view->priv->modify_selection_pressed = TRUE;
	}

      gtk_tree_view_real_set_cursor (tree_view, cursor_path, CLEAR_AND_SELECT | CLAMP_NODE);
      gtk_tree_path_free (cursor_path);

      gtk_widget_grab_focus (GTK_WIDGET (tree_view));

      tree_view->priv->modify_selection_pressed = FALSE;

      return TRUE;
    }

 out:

  tree_view->priv->search_entry_avoid_unhandled_binding = TRUE;
  return FALSE;
}

static gboolean
gtk_tree_view_search_entry_flush_timeout (GtkTreeView *tree_view)
{
  gtk_tree_view_search_window_hide (tree_view->priv->search_window, tree_view, NULL);
  tree_view->priv->typeselect_flush_timeout = 0;

  return FALSE;
}

/* Cut and paste from gtkwindow.c */
static void
send_focus_change (GtkWidget *widget,
                   GdkDevice *device,
		   gboolean   in)
{
  GdkSeat *seat;
  GList *devices, *d;

  seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
  devices = gdk_seat_get_slaves (seat, GDK_SEAT_CAPABILITY_KEYBOARD);
  devices = g_list_prepend (devices, gdk_seat_get_keyboard (seat));

  for (d = devices; d; d = d->next)
    {
      GdkEvent *fevent;
      GdkSurface *surface;

      surface = gtk_native_get_surface (gtk_widget_get_native (widget));

      fevent = gdk_event_new (GDK_FOCUS_CHANGE);

      fevent->any.type = GDK_FOCUS_CHANGE;
      fevent->any.surface = g_object_ref (surface);
      fevent->focus_change.in = in;
      gdk_event_set_device (fevent, device);

      gtk_widget_set_has_focus (widget, in);
      gtk_widget_event (widget, fevent);

      g_object_unref (fevent);
    }

  g_list_free (devices);
}

static void
gtk_tree_view_ensure_interactive_directory (GtkTreeView *tree_view)
{
  GtkWidget *frame, *vbox, *toplevel;
  GdkDisplay *display;
  GtkEventController *controller;
  GtkGesture *gesture;

  if (tree_view->priv->search_custom_entry_set)
    return;

  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (tree_view)));
  display = gtk_widget_get_display (GTK_WIDGET (tree_view));

   if (tree_view->priv->search_window != NULL)
     {
       if (gtk_window_has_group (GTK_WINDOW (toplevel)))
         gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)),
                                      GTK_WINDOW (tree_view->priv->search_window));
       else if (gtk_window_has_group (GTK_WINDOW (tree_view->priv->search_window)))
         gtk_window_group_remove_window (gtk_window_get_group (GTK_WINDOW (tree_view->priv->search_window)),
                                         GTK_WINDOW (tree_view->priv->search_window));

       gtk_window_set_display (GTK_WINDOW (tree_view->priv->search_window), display);

       return;
     }
   
  tree_view->priv->search_window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_display (GTK_WINDOW (tree_view->priv->search_window), display);

  if (gtk_window_has_group (GTK_WINDOW (toplevel)))
    gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)),
				 GTK_WINDOW (tree_view->priv->search_window));

  gtk_window_set_type_hint (GTK_WINDOW (tree_view->priv->search_window),
                            GDK_SURFACE_TYPE_HINT_UTILITY);
  gtk_window_set_modal (GTK_WINDOW (tree_view->priv->search_window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (tree_view->priv->search_window),
                                GTK_WINDOW (toplevel));

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
		    G_CALLBACK (gtk_tree_view_search_key_pressed),
		    tree_view);
  gtk_widget_add_controller (tree_view->priv->search_window, controller);

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_tree_view_search_pressed_cb), tree_view);
  gtk_widget_add_controller (tree_view->priv->search_window, GTK_EVENT_CONTROLLER (gesture));

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  g_signal_connect (controller, "scroll",
		    G_CALLBACK (gtk_tree_view_search_scroll_event),
		    tree_view);
  gtk_widget_add_controller (tree_view->priv->search_window, controller);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_widget_show (frame);
  gtk_container_add (GTK_CONTAINER (tree_view->priv->search_window), frame);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  /* add entry */
  tree_view->priv->search_entry = gtk_text_new ();
  g_signal_connect (tree_view->priv->search_entry, "activate",
                    G_CALLBACK (gtk_tree_view_search_activate), tree_view);
  g_signal_connect (tree_view->priv->search_entry, "preedit-changed",
		    G_CALLBACK (gtk_tree_view_search_preedit_changed), tree_view);
  g_signal_connect (tree_view->priv->search_entry, "changed",
		    G_CALLBACK (gtk_tree_view_search_changed), tree_view);

  gtk_container_add (GTK_CONTAINER (vbox),
		     tree_view->priv->search_entry);

  gtk_widget_realize (tree_view->priv->search_entry);
}

/* Pops up the interactive search entry.  If keybinding is TRUE then the user
 * started this by typing the start_interactive_search keybinding.  Otherwise, it came from 
 */
static gboolean
gtk_tree_view_real_start_interactive_search (GtkTreeView *tree_view,
                                             GdkDevice   *device,
					     gboolean     keybinding)
{
  /* We only start interactive search if we have focus or the columns
   * have focus.  If one of our children have focus, we don't want to
   * start the search.
   */
  GList *list;
  gboolean found_focus = FALSE;

  if (!tree_view->priv->enable_search && !keybinding)
    return FALSE;

  if (tree_view->priv->search_custom_entry_set)
    return FALSE;

  if (tree_view->priv->search_window != NULL &&
      gtk_widget_get_visible (tree_view->priv->search_window))
    return TRUE;

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      GtkTreeViewColumn *column;
      GtkWidget         *button;

      column = list->data;
      if (!gtk_tree_view_column_get_visible (column))
	continue;

      button = gtk_tree_view_column_get_button (column);
      if (gtk_widget_has_focus (button))
	{
	  found_focus = TRUE;
	  break;
	}
    }
  
  if (gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    found_focus = TRUE;

  if (!found_focus)
    return FALSE;

  if (tree_view->priv->search_column < 0)
    return FALSE;

  gtk_tree_view_ensure_interactive_directory (tree_view);

  if (keybinding)
    gtk_editable_set_text (GTK_EDITABLE (tree_view->priv->search_entry), "");

  /* done, show it */
  tree_view->priv->search_position_func (tree_view, tree_view->priv->search_window, tree_view->priv->search_position_user_data);

  /* Grab focus without selecting all the text. */
  if (GTK_IS_ENTRY (tree_view->priv->search_entry))
    gtk_entry_grab_focus_without_selecting (GTK_ENTRY (tree_view->priv->search_entry));
  else
    gtk_widget_grab_focus (tree_view->priv->search_entry);

  gtk_widget_show (tree_view->priv->search_window);
  if (tree_view->priv->search_entry_changed_id == 0)
    {
      tree_view->priv->search_entry_changed_id =
	g_signal_connect (tree_view->priv->search_entry, "changed",
			  G_CALLBACK (gtk_tree_view_search_init),
			  tree_view);
    }

  tree_view->priv->typeselect_flush_timeout =
    g_timeout_add (GTK_TREE_VIEW_SEARCH_DIALOG_TIMEOUT,
                   (GSourceFunc) gtk_tree_view_search_entry_flush_timeout,
                   tree_view);
  g_source_set_name_by_id (tree_view->priv->typeselect_flush_timeout, "[gtk] gtk_tree_view_search_entry_flush_timeout");

  /* send focus-in event */
  send_focus_change (tree_view->priv->search_entry, device, TRUE);

  /* search first matching iter */
  gtk_tree_view_search_init (tree_view->priv->search_entry, tree_view);

  return TRUE;
}

static gboolean
gtk_tree_view_start_interactive_search (GtkTreeView *tree_view)
{
  return gtk_tree_view_real_start_interactive_search (tree_view,
                                                      gtk_get_current_event_device (),
                                                      TRUE);
}

/* Callbacks */
static void
gtk_tree_view_adjustment_changed (GtkAdjustment *adjustment,
				  GtkTreeView   *tree_view)
{
  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    {
      GtkAllocation allocation;
      gint dy;

      gtk_widget_get_allocation (GTK_WIDGET (tree_view), &allocation);
      dy = tree_view->priv->dy - (int) gtk_adjustment_get_value (tree_view->priv->vadjustment);

      if (dy != 0)
        {
          /* update our dy and top_row */
          tree_view->priv->dy = (int) gtk_adjustment_get_value (tree_view->priv->vadjustment);

          update_prelight (tree_view,
                           tree_view->priv->event_last_x,
                           tree_view->priv->event_last_y);

          if (!tree_view->priv->in_top_row_to_dy)
            gtk_tree_view_dy_to_top_row (tree_view);

        }
    }

  gtk_widget_queue_allocate (GTK_WIDGET (tree_view));
}



/* Public methods
 */

/**
 * gtk_tree_view_new:
 *
 * Creates a new #GtkTreeView widget.
 *
 * Returns: A newly created #GtkTreeView widget.
 **/
GtkWidget *
gtk_tree_view_new (void)
{
  return g_object_new (GTK_TYPE_TREE_VIEW, NULL);
}

/**
 * gtk_tree_view_new_with_model:
 * @model: the model.
 *
 * Creates a new #GtkTreeView widget with the model initialized to @model.
 *
 * Returns: A newly created #GtkTreeView widget.
 **/
GtkWidget *
gtk_tree_view_new_with_model (GtkTreeModel *model)
{
  return g_object_new (GTK_TYPE_TREE_VIEW, "model", model, NULL);
}

/* Public Accessors
 */

/**
 * gtk_tree_view_get_model:
 * @tree_view: a #GtkTreeView
 *
 * Returns the model the #GtkTreeView is based on.  Returns %NULL if the
 * model is unset.
 *
 * Returns: (transfer none) (nullable): A #GtkTreeModel, or %NULL if
 * none is currently being used.
 **/
GtkTreeModel *
gtk_tree_view_get_model (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->model;
}

/**
 * gtk_tree_view_set_model:
 * @tree_view: A #GtkTreeView.
 * @model: (allow-none): The model.
 *
 * Sets the model for a #GtkTreeView.  If the @tree_view already has a model
 * set, it will remove it before setting the new model.  If @model is %NULL,
 * then it will unset the old model.
 **/
void
gtk_tree_view_set_model (GtkTreeView  *tree_view,
			 GtkTreeModel *model)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  if (model == tree_view->priv->model)
    return;

  if (tree_view->priv->scroll_to_path)
    {
      gtk_tree_row_reference_free (tree_view->priv->scroll_to_path);
      tree_view->priv->scroll_to_path = NULL;
    }

  if (tree_view->priv->rubber_band_status)
    gtk_tree_view_stop_rubber_band (tree_view);

  if (tree_view->priv->model)
    {
      GList *tmplist = tree_view->priv->columns;

      gtk_tree_view_unref_and_check_selection_tree (tree_view, tree_view->priv->tree);
      gtk_tree_view_stop_editing (tree_view, TRUE);

      g_signal_handlers_disconnect_by_func (tree_view->priv->model,
					    gtk_tree_view_row_changed,
					    tree_view);
      g_signal_handlers_disconnect_by_func (tree_view->priv->model,
					    gtk_tree_view_row_inserted,
					    tree_view);
      g_signal_handlers_disconnect_by_func (tree_view->priv->model,
					    gtk_tree_view_row_has_child_toggled,
					    tree_view);
      g_signal_handlers_disconnect_by_func (tree_view->priv->model,
					    gtk_tree_view_row_deleted,
					    tree_view);
      g_signal_handlers_disconnect_by_func (tree_view->priv->model,
					    gtk_tree_view_rows_reordered,
					    tree_view);

      for (; tmplist; tmplist = tmplist->next)
	_gtk_tree_view_column_unset_model (tmplist->data,
					   tree_view->priv->model);

      if (tree_view->priv->tree)
	gtk_tree_view_free_rbtree (tree_view);

      gtk_tree_row_reference_free (tree_view->priv->drag_dest_row);
      tree_view->priv->drag_dest_row = NULL;
      gtk_tree_row_reference_free (tree_view->priv->anchor);
      tree_view->priv->anchor = NULL;
      gtk_tree_row_reference_free (tree_view->priv->top_row);
      tree_view->priv->top_row = NULL;
      gtk_tree_row_reference_free (tree_view->priv->scroll_to_path);
      tree_view->priv->scroll_to_path = NULL;

      tree_view->priv->scroll_to_column = NULL;

      g_object_unref (tree_view->priv->model);

      tree_view->priv->search_column = -1;
      tree_view->priv->fixed_height_check = 0;
      tree_view->priv->fixed_height = -1;
      tree_view->priv->dy = tree_view->priv->top_row_dy = 0;
    }

  tree_view->priv->model = model;

  if (tree_view->priv->model)
    {
      gint i;
      GtkTreePath *path;
      GtkTreeIter iter;
      GtkTreeModelFlags flags;

      if (tree_view->priv->search_column == -1)
	{
	  for (i = 0; i < gtk_tree_model_get_n_columns (model); i++)
	    {
	      GType type = gtk_tree_model_get_column_type (model, i);

	      if (g_value_type_transformable (type, G_TYPE_STRING))
		{
		  tree_view->priv->search_column = i;
		  break;
		}
	    }
	}

      g_object_ref (tree_view->priv->model);
      g_signal_connect (tree_view->priv->model,
			"row-changed",
			G_CALLBACK (gtk_tree_view_row_changed),
			tree_view);
      g_signal_connect (tree_view->priv->model,
			"row-inserted",
			G_CALLBACK (gtk_tree_view_row_inserted),
			tree_view);
      g_signal_connect (tree_view->priv->model,
			"row-has-child-toggled",
			G_CALLBACK (gtk_tree_view_row_has_child_toggled),
			tree_view);
      g_signal_connect (tree_view->priv->model,
			"row-deleted",
			G_CALLBACK (gtk_tree_view_row_deleted),
			tree_view);
      g_signal_connect (tree_view->priv->model,
			"rows-reordered",
			G_CALLBACK (gtk_tree_view_rows_reordered),
			tree_view);

      flags = gtk_tree_model_get_flags (tree_view->priv->model);
      if ((flags & GTK_TREE_MODEL_LIST_ONLY) == GTK_TREE_MODEL_LIST_ONLY)
        tree_view->priv->is_list = TRUE;
      else
        tree_view->priv->is_list = FALSE;

      path = gtk_tree_path_new_first ();
      if (gtk_tree_model_get_iter (tree_view->priv->model, &iter, path))
	{
	  tree_view->priv->tree = gtk_tree_rbtree_new ();
	  gtk_tree_view_build_tree (tree_view, tree_view->priv->tree, &iter, 1, FALSE);
          _gtk_tree_view_accessible_add (tree_view, tree_view->priv->tree, NULL);
	}
      gtk_tree_path_free (path);

      /*  FIXME: do I need to do this? gtk_tree_view_create_buttons (tree_view); */
      install_presize_handler (tree_view);
    }

  gtk_tree_view_real_set_cursor (tree_view, NULL, CURSOR_INVALID);

  g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_MODEL]);

  if (tree_view->priv->selection)
  _gtk_tree_selection_emit_changed (tree_view->priv->selection);

  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    gtk_widget_queue_resize (GTK_WIDGET (tree_view));
}

/**
 * gtk_tree_view_get_selection:
 * @tree_view: A #GtkTreeView.
 *
 * Gets the #GtkTreeSelection associated with @tree_view.
 *
 * Returns: (transfer none): A #GtkTreeSelection object.
 **/
GtkTreeSelection *
gtk_tree_view_get_selection (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->selection;
}

static void
gtk_tree_view_do_set_hadjustment (GtkTreeView   *tree_view,
                                  GtkAdjustment *adjustment)
{
  GtkTreeViewPrivate *priv = tree_view->priv;

  if (adjustment && priv->hadjustment == adjustment)
    return;

  if (priv->hadjustment != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->hadjustment,
                                            gtk_tree_view_adjustment_changed,
                                            tree_view);
      g_object_unref (priv->hadjustment);
    }

  if (adjustment == NULL)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gtk_tree_view_adjustment_changed), tree_view);
  priv->hadjustment = g_object_ref_sink (adjustment);
  /* FIXME: Adjustment should probably be populated here with fresh values, but
   * internal details are too complicated for me to decipher right now.
   */
  gtk_tree_view_adjustment_changed (NULL, tree_view);

  g_object_notify (G_OBJECT (tree_view), "hadjustment");
}

static void
gtk_tree_view_do_set_vadjustment (GtkTreeView   *tree_view,
                                  GtkAdjustment *adjustment)
{
  GtkTreeViewPrivate *priv = tree_view->priv;

  if (adjustment && priv->vadjustment == adjustment)
    return;

  if (priv->vadjustment != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->vadjustment,
                                            gtk_tree_view_adjustment_changed,
                                            tree_view);
      g_object_unref (priv->vadjustment);
    }

  if (adjustment == NULL)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gtk_tree_view_adjustment_changed), tree_view);
  priv->vadjustment = g_object_ref_sink (adjustment);
  /* FIXME: Adjustment should probably be populated here with fresh values, but
   * internal details are too complicated for me to decipher right now.
   */
  gtk_tree_view_adjustment_changed (NULL, tree_view);
  g_object_notify (G_OBJECT (tree_view), "vadjustment");
}

/* Column and header operations */

/**
 * gtk_tree_view_get_headers_visible:
 * @tree_view: A #GtkTreeView.
 *
 * Returns %TRUE if the headers on the @tree_view are visible.
 *
 * Returns: Whether the headers are visible or not.
 **/
gboolean
gtk_tree_view_get_headers_visible (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return tree_view->priv->headers_visible;
}

/**
 * gtk_tree_view_set_headers_visible:
 * @tree_view: A #GtkTreeView.
 * @headers_visible: %TRUE if the headers are visible
 *
 * Sets the visibility state of the headers.
 **/
void
gtk_tree_view_set_headers_visible (GtkTreeView *tree_view,
				   gboolean     headers_visible)
{
  GList *list;
  GtkTreeViewColumn *column;
  GtkWidget *button;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  headers_visible = !! headers_visible;

  if (tree_view->priv->headers_visible == headers_visible)
    return;

  tree_view->priv->headers_visible = headers_visible == TRUE;

  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    {
      if (headers_visible)
	{
          if (gtk_widget_get_mapped (GTK_WIDGET (tree_view)))
            gtk_tree_view_map_buttons (tree_view);
 	}
      else
	{

	  for (list = tree_view->priv->columns; list; list = list->next)
	    {
	      column = list->data;
	      button = gtk_tree_view_column_get_button (column);

              gtk_widget_hide (button);
	      gtk_widget_unmap (button);
	    }
	}
    }

  gtk_widget_queue_resize (GTK_WIDGET (tree_view));

  g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_HEADERS_VISIBLE]);
}

/**
 * gtk_tree_view_columns_autosize:
 * @tree_view: A #GtkTreeView.
 *
 * Resizes all columns to their optimal width. Only works after the
 * treeview has been realized.
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
      if (gtk_tree_view_column_get_sizing (column) == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
	continue;
      _gtk_tree_view_column_cell_set_dirty (column, TRUE);
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
  gboolean changed = FALSE;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      if (gtk_tree_view_column_get_clickable (GTK_TREE_VIEW_COLUMN (list->data)) != setting)
        {
          gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (list->data), setting);
          changed = TRUE;
        }
    }

  if (changed)
    g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_HEADERS_CLICKABLE]);
}


/**
 * gtk_tree_view_get_headers_clickable:
 * @tree_view: A #GtkTreeView.
 *
 * Returns whether all header columns are clickable.
 *
 * Returns: %TRUE if all header columns are clickable, otherwise %FALSE
 **/
gboolean 
gtk_tree_view_get_headers_clickable (GtkTreeView *tree_view)
{
  GList *list;
  
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  for (list = tree_view->priv->columns; list; list = list->next)
    if (!gtk_tree_view_column_get_clickable (GTK_TREE_VIEW_COLUMN (list->data)))
      return FALSE;

  return TRUE;
}

/**
 * gtk_tree_view_set_activate_on_single_click:
 * @tree_view: a #GtkTreeView
 * @single: %TRUE to emit row-activated on a single click
 *
 * Cause the #GtkTreeView::row-activated signal to be emitted
 * on a single click instead of a double click.
 **/
void
gtk_tree_view_set_activate_on_single_click (GtkTreeView *tree_view,
                                            gboolean     single)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  single = single != FALSE;

  if (tree_view->priv->activate_on_single_click == single)
    return;

  tree_view->priv->activate_on_single_click = single;
  g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_ACTIVATE_ON_SINGLE_CLICK]);
}

/**
 * gtk_tree_view_get_activate_on_single_click:
 * @tree_view: a #GtkTreeView
 *
 * Gets the setting set by gtk_tree_view_set_activate_on_single_click().
 *
 * Returns: %TRUE if row-activated will be emitted on a single click
 **/
gboolean
gtk_tree_view_get_activate_on_single_click (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return tree_view->priv->activate_on_single_click;
}

/* Public Column functions
 */

/**
 * gtk_tree_view_append_column:
 * @tree_view: A #GtkTreeView.
 * @column: The #GtkTreeViewColumn to add.
 *
 * Appends @column to the list of columns. If @tree_view has “fixed_height”
 * mode enabled, then @column must have its “sizing” property set to be
 * GTK_TREE_VIEW_COLUMN_FIXED.
 *
 * Returns: The number of columns in @tree_view after appending.
 **/
gint
gtk_tree_view_append_column (GtkTreeView       *tree_view,
			     GtkTreeViewColumn *column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), -1);
  g_return_val_if_fail (gtk_tree_view_column_get_tree_view (column) == NULL, -1);

  return gtk_tree_view_insert_column (tree_view, column, -1);
}

/**
 * gtk_tree_view_remove_column:
 * @tree_view: A #GtkTreeView.
 * @column: The #GtkTreeViewColumn to remove.
 *
 * Removes @column from @tree_view.
 *
 * Returns: The number of columns in @tree_view after removing.
 **/
gint
gtk_tree_view_remove_column (GtkTreeView       *tree_view,
                             GtkTreeViewColumn *column)
{
  guint position;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), -1);
  g_return_val_if_fail (gtk_tree_view_column_get_tree_view (column) == GTK_WIDGET (tree_view), -1);

  if (tree_view->priv->focus_column == column)
    _gtk_tree_view_set_focus_column (tree_view, NULL);

  if (tree_view->priv->edited_column == column)
    {
      gtk_tree_view_stop_editing (tree_view, TRUE);

      /* no need to, but just to be sure ... */
      tree_view->priv->edited_column = NULL;
    }

  if (tree_view->priv->expander_column == column)
    tree_view->priv->expander_column = NULL;

  g_signal_handlers_disconnect_by_func (column,
                                        G_CALLBACK (column_sizing_notify),
                                        tree_view);

  position = g_list_index (tree_view->priv->columns, column);

  _gtk_tree_view_column_unset_tree_view (column);

  tree_view->priv->columns = g_list_remove (tree_view->priv->columns, column);
  tree_view->priv->n_columns--;

  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    {
      GList *list;

      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  GtkTreeViewColumn *tmp_column;

	  tmp_column = GTK_TREE_VIEW_COLUMN (list->data);
	  if (gtk_tree_view_column_get_visible (tmp_column))
            _gtk_tree_view_column_cell_set_dirty (tmp_column, TRUE);
	}

      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  _gtk_tree_view_accessible_remove_column (tree_view, column, position);

  g_object_unref (column);
  g_signal_emit (tree_view, tree_view_signals[COLUMNS_CHANGED], 0);

  return tree_view->priv->n_columns;
}

/**
 * gtk_tree_view_insert_column:
 * @tree_view: A #GtkTreeView.
 * @column: The #GtkTreeViewColumn to be inserted.
 * @position: The position to insert @column in.
 *
 * This inserts the @column into the @tree_view at @position.  If @position is
 * -1, then the column is inserted at the end. If @tree_view has
 * “fixed_height” mode enabled, then @column must have its “sizing” property
 * set to be GTK_TREE_VIEW_COLUMN_FIXED.
 *
 * Returns: The number of columns in @tree_view after insertion.
 **/
gint
gtk_tree_view_insert_column (GtkTreeView       *tree_view,
                             GtkTreeViewColumn *column,
                             gint               position)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), -1);
  g_return_val_if_fail (gtk_tree_view_column_get_tree_view (column) == NULL, -1);

  if (tree_view->priv->fixed_height_mode)
    g_return_val_if_fail (gtk_tree_view_column_get_sizing (column)
                          == GTK_TREE_VIEW_COLUMN_FIXED, -1);

  if (position < 0 || position > tree_view->priv->n_columns)
    position = tree_view->priv->n_columns;

  g_object_ref_sink (column);

  g_signal_connect (column, "notify::sizing",
                    G_CALLBACK (column_sizing_notify), tree_view);

  tree_view->priv->columns = g_list_insert (tree_view->priv->columns,
					    column, position);
  tree_view->priv->n_columns++;

  gtk_tree_view_update_button_position (tree_view, column);

  _gtk_tree_view_column_set_tree_view (column, tree_view);

  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    {
      GList *list;

      _gtk_tree_view_column_realize_button (column);

      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  column = GTK_TREE_VIEW_COLUMN (list->data);
	  if (gtk_tree_view_column_get_visible (column))
            _gtk_tree_view_column_cell_set_dirty (column, TRUE);
	}
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  _gtk_tree_view_accessible_add_column (tree_view, column, position);

  g_signal_emit (tree_view, tree_view_signals[COLUMNS_CHANGED], 0);

  return tree_view->priv->n_columns;
}

/**
 * gtk_tree_view_insert_column_with_attributes:
 * @tree_view: A #GtkTreeView
 * @position: The position to insert the new column in
 * @title: The title to set the header to
 * @cell: The #GtkCellRenderer
 * @...: A %NULL-terminated list of attributes
 *
 * Creates a new #GtkTreeViewColumn and inserts it into the @tree_view at
 * @position.  If @position is -1, then the newly created column is inserted at
 * the end.  The column is initialized with the attributes given. If @tree_view
 * has “fixed_height” mode enabled, then the new column will have its sizing
 * property set to be GTK_TREE_VIEW_COLUMN_FIXED.
 *
 * Returns: The number of columns in @tree_view after insertion.
 **/
gint
gtk_tree_view_insert_column_with_attributes (GtkTreeView     *tree_view,
					     gint             position,
					     const gchar     *title,
					     GtkCellRenderer *cell,
					     ...)
{
  GtkTreeViewColumn *column;
  gchar *attribute;
  va_list args;
  gint column_id;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);

  column = gtk_tree_view_column_new ();
  if (tree_view->priv->fixed_height_mode)
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);

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

  return gtk_tree_view_insert_column (tree_view, column, position);
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
 * with the given cell renderer and a #GtkTreeCellDataFunc to set cell renderer
 * attributes (normally using data from the model). See also
 * gtk_tree_view_column_set_cell_data_func(), gtk_tree_view_column_pack_start().
 * If @tree_view has “fixed_height” mode enabled, then the new column will have its
 * “sizing” property set to be GTK_TREE_VIEW_COLUMN_FIXED.
 *
 * Returns: number of columns in the tree view post-insert
 **/
gint
gtk_tree_view_insert_column_with_data_func  (GtkTreeView               *tree_view,
                                             gint                       position,
                                             const gchar               *title,
                                             GtkCellRenderer           *cell,
                                             GtkTreeCellDataFunc        func,
                                             gpointer                   data,
                                             GDestroyNotify             dnotify)
{
  GtkTreeViewColumn *column;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);

  column = gtk_tree_view_column_new ();
  if (tree_view->priv->fixed_height_mode)
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);

  gtk_tree_view_column_set_title (column, title);
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_set_cell_data_func (column, cell, func, data, dnotify);

  return gtk_tree_view_insert_column (tree_view, column, position);
}

/**
 * gtk_tree_view_get_n_columns:
 * @tree_view: a #GtkTreeView
 *
 * Queries the number of columns in the given @tree_view.
 *
 * Returns: The number of columns in the @tree_view
 **/
guint
gtk_tree_view_get_n_columns (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), 0);

  return tree_view->priv->n_columns;
}

/**
 * gtk_tree_view_get_column:
 * @tree_view: A #GtkTreeView.
 * @n: The position of the column, counting from 0.
 *
 * Gets the #GtkTreeViewColumn at the given position in the #tree_view.
 *
 * Returns: (nullable) (transfer none): The #GtkTreeViewColumn, or %NULL if the
 * position is outside the range of columns.
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
 * Returns: (element-type GtkTreeViewColumn) (transfer container): A list of #GtkTreeViewColumn s
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
 * @base_column: (allow-none): The #GtkTreeViewColumn to be moved relative to, or %NULL.
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

  gtk_tree_view_update_button_position (tree_view, column);

  gtk_widget_queue_resize (GTK_WIDGET (tree_view));

  _gtk_tree_view_accessible_reorder_column (tree_view, column);

  g_signal_emit (tree_view, tree_view_signals[COLUMNS_CHANGED], 0);
}

/**
 * gtk_tree_view_set_expander_column:
 * @tree_view: A #GtkTreeView
 * @column: (nullable): %NULL, or the column to draw the expander arrow at.
 *
 * Sets the column to draw the expander arrow at. It must be in @tree_view.  
 * If @column is %NULL, then the expander arrow is always at the first 
 * visible column.
 *
 * If you do not want expander arrow to appear in your tree, set the 
 * expander column to a hidden column.
 **/
void
gtk_tree_view_set_expander_column (GtkTreeView       *tree_view,
                                   GtkTreeViewColumn *column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (column == NULL || GTK_IS_TREE_VIEW_COLUMN (column));
  g_return_if_fail (column == NULL || gtk_tree_view_column_get_tree_view (column) == GTK_WIDGET (tree_view));

  if (tree_view->priv->expander_column != column)
    {
      tree_view->priv->expander_column = column;
      g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_EXPANDER_COLUMN]);
    }
}

/**
 * gtk_tree_view_get_expander_column:
 * @tree_view: A #GtkTreeView
 *
 * Returns the column that is the current expander column.
 * This column has the expander arrow drawn next to it.
 *
 * Returns: (transfer none): The expander column.
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
 * @func: (allow-none): A function to determine which columns are reorderable, or %NULL.
 * @user_data: (allow-none): User data to be passed to @func, or %NULL
 * @destroy: (allow-none): Destroy notifier for @user_data, or %NULL
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
					GDestroyNotify             destroy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->column_drop_func_data_destroy)
    tree_view->priv->column_drop_func_data_destroy (tree_view->priv->column_drop_func_data);

  tree_view->priv->column_drop_func = func;
  tree_view->priv->column_drop_func_data = user_data;
  tree_view->priv->column_drop_func_data_destroy = destroy;
}

/**
 * gtk_tree_view_scroll_to_point:
 * @tree_view: a #GtkTreeView
 * @tree_x: X coordinate of new top-left pixel of visible area, or -1
 * @tree_y: Y coordinate of new top-left pixel of visible area, or -1
 *
 * Scrolls the tree view such that the top-left corner of the visible
 * area is @tree_x, @tree_y, where @tree_x and @tree_y are specified
 * in tree coordinates.  The @tree_view must be realized before
 * this function is called.  If it isn't, you probably want to be
 * using gtk_tree_view_scroll_to_cell().
 *
 * If either @tree_x or @tree_y are -1, then that direction isn’t scrolled.
 **/
void
gtk_tree_view_scroll_to_point (GtkTreeView *tree_view,
                               gint         tree_x,
                               gint         tree_y)
{
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (tree_view)));

  hadj = tree_view->priv->hadjustment;
  vadj = tree_view->priv->vadjustment;

  if (tree_x != -1)
    gtk_adjustment_animate_to_value (hadj, tree_x);
  if (tree_y != -1)
    gtk_adjustment_animate_to_value (vadj, tree_y);
}

/**
 * gtk_tree_view_scroll_to_cell:
 * @tree_view: A #GtkTreeView.
 * @path: (allow-none): The path of the row to move to, or %NULL.
 * @column: (allow-none): The #GtkTreeViewColumn to move horizontally to, or %NULL.
 * @use_align: whether to use alignment arguments, or %FALSE.
 * @row_align: The vertical alignment of the row specified by @path.
 * @col_align: The horizontal alignment of the column specified by @column.
 *
 * Moves the alignments of @tree_view to the position specified by @column and
 * @path.  If @column is %NULL, then no horizontal scrolling occurs.  Likewise,
 * if @path is %NULL no vertical scrolling occurs.  At a minimum, one of @column
 * or @path need to be non-%NULL.  @row_align determines where the row is
 * placed, and @col_align determines where @column is placed.  Both are expected
 * to be between 0.0 and 1.0. 0.0 means left/top alignment, 1.0 means
 * right/bottom alignment, 0.5 means center.
 *
 * If @use_align is %FALSE, then the alignment arguments are ignored, and the
 * tree does the minimum amount of work to scroll the cell onto the screen.
 * This means that the cell will be scrolled to the edge closest to its current
 * position.  If the cell is currently visible on the screen, nothing is done.
 *
 * This function only works if the model is set, and @path is a valid row on the
 * model.  If the model changes before the @tree_view is realized, the centered
 * path will be modified to reflect this change.
 **/
void
gtk_tree_view_scroll_to_cell (GtkTreeView       *tree_view,
                              GtkTreePath       *path,
                              GtkTreeViewColumn *column,
			      gboolean           use_align,
                              gfloat             row_align,
                              gfloat             col_align)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (tree_view->priv->model != NULL);
  g_return_if_fail (tree_view->priv->tree != NULL);
  g_return_if_fail (row_align >= 0.0 && row_align <= 1.0);
  g_return_if_fail (col_align >= 0.0 && col_align <= 1.0);
  g_return_if_fail (path != NULL || column != NULL);

  row_align = CLAMP (row_align, 0.0, 1.0);
  col_align = CLAMP (col_align, 0.0, 1.0);


  /* Note: Despite the benefits that come from having one code path for the
   * scrolling code, we short-circuit validate_visible_area's immplementation as
   * it is much slower than just going to the point.
   */
  if (!gtk_widget_get_visible (GTK_WIDGET (tree_view)) ||
      !gtk_widget_get_realized (GTK_WIDGET (tree_view)) ||
      _gtk_widget_get_alloc_needed (GTK_WIDGET (tree_view)) ||
      GTK_TREE_RBNODE_FLAG_SET (tree_view->priv->tree->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID))
    {
      if (tree_view->priv->scroll_to_path)
	gtk_tree_row_reference_free (tree_view->priv->scroll_to_path);

      tree_view->priv->scroll_to_path = NULL;
      tree_view->priv->scroll_to_column = NULL;

      if (path)
	tree_view->priv->scroll_to_path = gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view), tree_view->priv->model, path);
      if (column)
	tree_view->priv->scroll_to_column = column;
      tree_view->priv->scroll_to_use_align = use_align;
      tree_view->priv->scroll_to_row_align = row_align;
      tree_view->priv->scroll_to_col_align = col_align;

      install_presize_handler (tree_view);
    }
  else
    {
      GdkRectangle cell_rect;
      GdkRectangle vis_rect;
      gint dest_x, dest_y;

      gtk_tree_view_get_background_area (tree_view, path, column, &cell_rect);
      gtk_tree_view_get_visible_rect (tree_view, &vis_rect);

      cell_rect.y = TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, cell_rect.y);

      dest_x = vis_rect.x;
      dest_y = vis_rect.y;

      if (column)
	{
	  if (use_align)
	    {
	      dest_x = cell_rect.x - ((vis_rect.width - cell_rect.width) * col_align);
	    }
	  else
	    {
	      if (cell_rect.x < vis_rect.x)
		dest_x = cell_rect.x;
	      if (cell_rect.x + cell_rect.width > vis_rect.x + vis_rect.width)
		dest_x = cell_rect.x + cell_rect.width - vis_rect.width;
	    }
	}

      if (path)
	{
	  if (use_align)
	    {
	      dest_y = cell_rect.y - ((vis_rect.height - cell_rect.height) * row_align);
	      dest_y = MAX (dest_y, 0);
	    }
	  else
	    {
	      if (cell_rect.y < vis_rect.y)
		dest_y = cell_rect.y;
	      if (cell_rect.y + cell_rect.height > vis_rect.y + vis_rect.height)
		dest_y = cell_rect.y + cell_rect.height - vis_rect.height;
	    }
	}

      gtk_tree_view_scroll_to_point (tree_view, dest_x, dest_y);
    }
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

  g_signal_emit (tree_view, tree_view_signals[ROW_ACTIVATED], 0, path, column);
}


static void
gtk_tree_view_expand_all_emission_helper (GtkTreeRBTree *tree,
                                          GtkTreeRBNode *node,
                                          gpointer       data)
{
  GtkTreeView *tree_view = data;

  if ((node->flags & GTK_TREE_RBNODE_IS_PARENT) == GTK_TREE_RBNODE_IS_PARENT &&
      node->children)
    {
      GtkTreePath *path;
      GtkTreeIter iter;

      path = _gtk_tree_path_new_from_rbtree (tree, node);
      gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);

      g_signal_emit (tree_view, tree_view_signals[ROW_EXPANDED], 0, &iter, path);

      gtk_tree_path_free (path);
    }

  if (node->children)
    gtk_tree_rbtree_traverse (node->children,
                          node->children->root,
                          G_PRE_ORDER,
                          gtk_tree_view_expand_all_emission_helper,
                          tree_view);
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
  GtkTreePath *path;
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->tree == NULL)
    return;

  path = gtk_tree_path_new_first ();
  _gtk_tree_view_find_node (tree_view, path, &tree, &node);

  while (node)
    {
      gtk_tree_view_real_expand_row (tree_view, path, tree, node, TRUE, FALSE);
      node = gtk_tree_rbtree_next (tree, node);
      gtk_tree_path_next (path);
  }

  gtk_tree_path_free (path);
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
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
  GtkTreePath *path;
  gint *indices;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->tree == NULL)
    return;

  path = gtk_tree_path_new ();
  gtk_tree_path_down (path);
  indices = gtk_tree_path_get_indices (path);

  tree = tree_view->priv->tree;
  node = gtk_tree_rbtree_first (tree);

  while (node)
    {
      if (node->children)
	gtk_tree_view_real_collapse_row (tree_view, path, tree, node, FALSE);
      indices[0]++;
      node = gtk_tree_rbtree_next (tree, node);
    }

  gtk_tree_path_free (path);
}

/**
 * gtk_tree_view_expand_to_path:
 * @tree_view: A #GtkTreeView.
 * @path: path to a row.
 *
 * Expands the row at @path. This will also expand all parent rows of
 * @path as necessary.
 **/
void
gtk_tree_view_expand_to_path (GtkTreeView *tree_view,
			      GtkTreePath *path)
{
  gint i, depth;
  gint *indices;
  GtkTreePath *tmp;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (path != NULL);

  depth = gtk_tree_path_get_depth (path);
  indices = gtk_tree_path_get_indices (path);

  tmp = gtk_tree_path_new ();
  g_return_if_fail (tmp != NULL);

  for (i = 0; i < depth; i++)
    {
      gtk_tree_path_append_index (tmp, indices[i]);
      gtk_tree_view_expand_row (tree_view, tmp, FALSE);
    }

  gtk_tree_path_free (tmp);
}

/* FIXME the bool return values for expand_row and collapse_row are
 * not analagous; they should be TRUE if the row had children and
 * was not already in the requested state.
 */


static gboolean
gtk_tree_view_real_expand_row (GtkTreeView   *tree_view,
			       GtkTreePath   *path,
			       GtkTreeRBTree *tree,
			       GtkTreeRBNode *node,
			       gboolean       open_all,
			       gboolean       animate)
{
  GtkTreeIter iter;
  GtkTreeIter temp;
  gboolean expand;

  if (animate)
    animate = gtk_settings_get_enable_animations (gtk_widget_get_settings (GTK_WIDGET (tree_view)));

  remove_auto_expand_timeout (tree_view);

  if (node->children && !open_all)
    return FALSE;

  if (! GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_PARENT))
    return FALSE;

  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
  if (! gtk_tree_model_iter_has_child (tree_view->priv->model, &iter))
    return FALSE;


   if (node->children && open_all)
    {
      gboolean retval = FALSE;
      GtkTreePath *tmp_path = gtk_tree_path_copy (path);

      gtk_tree_path_append_index (tmp_path, 0);
      tree = node->children;
      node = gtk_tree_rbtree_first (tree);
      /* try to expand the children */
      do
        {
         gboolean t;
	 t = gtk_tree_view_real_expand_row (tree_view, tmp_path, tree, node,
					    TRUE, animate);
         if (t)
           retval = TRUE;

         gtk_tree_path_next (tmp_path);
	 node = gtk_tree_rbtree_next (tree, node);
       }
      while (node != NULL);

      gtk_tree_path_free (tmp_path);

      return retval;
    }

  g_signal_emit (tree_view, tree_view_signals[TEST_EXPAND_ROW], 0, &iter, path, &expand);

  if (!gtk_tree_model_iter_has_child (tree_view->priv->model, &iter))
    return FALSE;

  if (expand)
    return FALSE;

  node->children = gtk_tree_rbtree_new ();
  node->children->parent_tree = tree;
  node->children->parent_node = node;

  gtk_tree_model_iter_children (tree_view->priv->model, &temp, &iter);

  gtk_tree_view_build_tree (tree_view,
			    node->children,
			    &temp,
			    gtk_tree_path_get_depth (path) + 1,
			    open_all);

  _gtk_tree_view_accessible_add (tree_view, node->children, NULL);
  _gtk_tree_view_accessible_add_state (tree_view,
                                       tree, node,
                                       GTK_CELL_RENDERER_EXPANDED);

  install_presize_handler (tree_view);

  g_signal_emit (tree_view, tree_view_signals[ROW_EXPANDED], 0, &iter, path);
  if (open_all && node->children)
    {
      gtk_tree_rbtree_traverse (node->children,
                            node->children->root,
                            G_PRE_ORDER,
                            gtk_tree_view_expand_all_emission_helper,
                            tree_view);
    }
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
 * Returns: %TRUE if the row existed and had children
 **/
gboolean
gtk_tree_view_expand_row (GtkTreeView *tree_view,
			  GtkTreePath *path,
			  gboolean     open_all)
{
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;

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
gtk_tree_view_real_collapse_row (GtkTreeView   *tree_view,
				 GtkTreePath   *path,
				 GtkTreeRBTree *tree,
				 GtkTreeRBNode *node,
				 gboolean       animate)
{
  GtkTreeIter iter;
  GtkTreeIter children;
  gboolean collapse;
  GList *list;
  gboolean selection_changed, cursor_changed;

  if (animate)
    animate = gtk_settings_get_enable_animations (gtk_widget_get_settings (GTK_WIDGET (tree_view)));

  remove_auto_expand_timeout (tree_view);

  if (node->children == NULL)
    return FALSE;
  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);

  g_signal_emit (tree_view, tree_view_signals[TEST_COLLAPSE_ROW], 0, &iter, path, &collapse);

  if (collapse)
    return FALSE;

  /* if the prelighted node is a child of us, we want to unprelight it.  We have
   * a chance to prelight the correct node below */

  if (tree_view->priv->prelight_tree)
    {
      GtkTreeRBTree *parent_tree;
      GtkTreeRBNode *parent_node;

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

      if (gtk_tree_view_column_get_visible (column) == FALSE)
	continue;
      if (gtk_tree_view_column_get_sizing (column) == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
	_gtk_tree_view_column_cell_set_dirty (column, TRUE);
    }

  if (tree_view->priv->cursor_node)
    {
      cursor_changed = (node->children == tree_view->priv->cursor_tree)
                       || gtk_tree_rbtree_contains (node->children, tree_view->priv->cursor_tree);
    }
  else
    cursor_changed = FALSE;

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

  selection_changed = gtk_tree_view_unref_and_check_selection_tree (tree_view, node->children);
  
  /* Stop a pending double click */
  gtk_event_controller_reset (GTK_EVENT_CONTROLLER (tree_view->priv->click_gesture));

  _gtk_tree_view_accessible_remove (tree_view, node->children, NULL);
  _gtk_tree_view_accessible_remove_state (tree_view,
                                          tree, node,
                                          GTK_CELL_RENDERER_EXPANDED);

  gtk_tree_rbtree_remove (node->children);

  if (cursor_changed)
    gtk_tree_view_real_set_cursor (tree_view, path, CLEAR_AND_SELECT | CURSOR_INVALID);
  if (selection_changed)
    g_signal_emit_by_name (tree_view->priv->selection, "changed");

  if (gtk_widget_get_mapped (GTK_WIDGET (tree_view)))
    {
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  g_signal_emit (tree_view, tree_view_signals[ROW_COLLAPSED], 0, &iter, path);
  
  if (gtk_widget_get_mapped (GTK_WIDGET (tree_view)))
    update_prelight (tree_view,
                     tree_view->priv->event_last_x,
                     tree_view->priv->event_last_y);

  return TRUE;
}

/**
 * gtk_tree_view_collapse_row:
 * @tree_view: a #GtkTreeView
 * @path: path to a row in the @tree_view
 *
 * Collapses a row (hides its child rows, if they exist).
 *
 * Returns: %TRUE if the row was collapsed.
 **/
gboolean
gtk_tree_view_collapse_row (GtkTreeView *tree_view,
			    GtkTreePath *path)
{
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;

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
					GtkTreeRBTree          *tree,
					GtkTreePath            *path,
					GtkTreeViewMappingFunc  func,
					gpointer                user_data)
{
  GtkTreeRBNode *node;

  if (tree == NULL || tree->root == NULL)
    return;

  node = gtk_tree_rbtree_first (tree);

  while (node)
    {
      if (node->children)
	{
	  (* func) (tree_view, path, user_data);
	  gtk_tree_path_down (path);
	  gtk_tree_view_map_expanded_rows_helper (tree_view, node->children, path, func, user_data);
	  gtk_tree_path_up (path);
	}
      gtk_tree_path_next (path);
      node = gtk_tree_rbtree_next (tree, node);
    }
}

/**
 * gtk_tree_view_map_expanded_rows:
 * @tree_view: A #GtkTreeView
 * @func: (scope call): A function to be called
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

  path = gtk_tree_path_new_first ();

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
 * Returns: %TRUE if #path is expanded.
 **/
gboolean
gtk_tree_view_row_expanded (GtkTreeView *tree_view,
			    GtkTreePath *path)
{
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  _gtk_tree_view_find_node (tree_view, path, &tree, &node);

  if (node == NULL)
    return FALSE;

  return (node->children != NULL);
}

/**
 * gtk_tree_view_get_reorderable:
 * @tree_view: a #GtkTreeView
 *
 * Retrieves whether the user can reorder the tree via drag-and-drop. See
 * gtk_tree_view_set_reorderable().
 *
 * Returns: %TRUE if the tree can be reordered.
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
 * This function is a convenience function to allow you to reorder
 * models that support the #GtkTreeDragSourceIface and the
 * #GtkTreeDragDestIface.  Both #GtkTreeStore and #GtkListStore support
 * these.  If @reorderable is %TRUE, then the user can reorder the
 * model by dragging and dropping rows. The developer can listen to
 * these changes by connecting to the model’s #GtkTreeModel::row-inserted
 * and #GtkTreeModel::row-deleted signals. The reordering is implemented
 * by setting up the tree view as a drag source and destination.
 * Therefore, drag and drop can not be used in a reorderable view for any
 * other purpose.
 *
 * This function does not give you any degree of control over the order -- any
 * reordering is allowed.  If more control is needed, you should probably
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

  if (reorderable)
    {
      const char *row_targets[] = {
        "GTK_TREE_MODEL_ROW"
      };
      GdkContentFormats *formats;

      formats = gdk_content_formats_new (row_targets, G_N_ELEMENTS (row_targets));

      gtk_tree_view_enable_model_drag_source (tree_view,
					      GDK_BUTTON1_MASK,
					      formats,
					      GDK_ACTION_MOVE);
      gtk_tree_view_enable_model_drag_dest (tree_view,
					    formats,
					    GDK_ACTION_MOVE);
      gdk_content_formats_unref (formats);
    }
  else
    {
      gtk_tree_view_unset_rows_drag_source (tree_view);
      gtk_tree_view_unset_rows_drag_dest (tree_view);
    }

  tree_view->priv->reorderable = reorderable;

  g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_REORDERABLE]);
}

static void
gtk_tree_view_real_set_cursor (GtkTreeView     *tree_view,
			       GtkTreePath     *path,
                               SetCursorFlags   flags)
{
  if (!(flags & CURSOR_INVALID) && tree_view->priv->cursor_node)
    {
      _gtk_tree_view_accessible_remove_state (tree_view,
                                              tree_view->priv->cursor_tree,
                                              tree_view->priv->cursor_node,
                                              GTK_CELL_RENDERER_FOCUSED);
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
    }

  /* One cannot set the cursor on a separator.   Also, if
   * _gtk_tree_view_find_node returns TRUE, it ran out of tree
   * before finding the tree and node belonging to path.  The
   * path maps to a non-existing path and we will silently bail out.
   * We unset tree and node to avoid further processing.
   */
  if (path == NULL || 
      row_is_separator (tree_view, NULL, path)
      || _gtk_tree_view_find_node (tree_view,
                                   path,
                                   &tree_view->priv->cursor_tree,
                                   &tree_view->priv->cursor_node))
    {
      tree_view->priv->cursor_tree = NULL;
      tree_view->priv->cursor_node = NULL;
    }

  if (tree_view->priv->cursor_node != NULL)
    {
      GtkTreeRBTree *new_tree = NULL;
      GtkTreeRBNode *new_node = NULL;

      if ((flags & CLEAR_AND_SELECT) && !tree_view->priv->modify_selection_pressed)
        {
          GtkTreeSelectMode mode = 0;

          if (tree_view->priv->extend_selection_pressed)
            mode |= GTK_TREE_SELECT_MODE_EXTEND;

          _gtk_tree_selection_internal_select_node (tree_view->priv->selection,
                                                    tree_view->priv->cursor_node,
                                                    tree_view->priv->cursor_tree,
                                                    path,
                                                    mode,
                                                    FALSE);
        }

      /* We have to re-find tree and node here again, somebody might have
       * cleared the node or the whole tree in the GtkTreeSelection::changed
       * callback. If the nodes differ we bail out here.
       */
      _gtk_tree_view_find_node (tree_view, path, &new_tree, &new_node);

      if (tree_view->priv->cursor_node == NULL ||
          tree_view->priv->cursor_node != new_node)
        return;

      if (flags & CLAMP_NODE)
        {
	  gtk_tree_view_clamp_node_visible (tree_view,
                                            tree_view->priv->cursor_tree,
                                            tree_view->priv->cursor_node);
          gtk_widget_queue_draw (GTK_WIDGET (tree_view));
	}

      _gtk_tree_view_accessible_add_state (tree_view,
                                           tree_view->priv->cursor_tree,
                                           tree_view->priv->cursor_node,
                                           GTK_CELL_RENDERER_FOCUSED);
    }

  if (!gtk_widget_in_destruction (GTK_WIDGET (tree_view)))
    g_signal_emit (tree_view, tree_view_signals[CURSOR_CHANGED], 0);
}

/**
 * gtk_tree_view_get_cursor:
 * @tree_view: A #GtkTreeView
 * @path: (out) (transfer full) (optional) (nullable): A pointer to be
 *   filled with the current cursor path, or %NULL
 * @focus_column: (out) (transfer none) (optional) (nullable): A
 *   pointer to be filled with the current focus column, or %NULL
 *
 * Fills in @path and @focus_column with the current path and focus column.  If
 * the cursor isn’t currently set, then *@path will be %NULL.  If no column
 * currently has focus, then *@focus_column will be %NULL.
 *
 * The returned #GtkTreePath must be freed with gtk_tree_path_free() when
 * you are done with it.
 **/
void
gtk_tree_view_get_cursor (GtkTreeView        *tree_view,
			  GtkTreePath       **path,
			  GtkTreeViewColumn **focus_column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (path)
    {
      if (tree_view->priv->cursor_node)
        *path = _gtk_tree_path_new_from_rbtree (tree_view->priv->cursor_tree,
                                                tree_view->priv->cursor_node);
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
 * @focus_column: (allow-none): A #GtkTreeViewColumn, or %NULL
 * @start_editing: %TRUE if the specified cell should start being edited.
 *
 * Sets the current keyboard focus to be at @path, and selects it.  This is
 * useful when you want to focus the user’s attention on a particular row.  If
 * @focus_column is not %NULL, then focus is given to the column specified by 
 * it. Additionally, if @focus_column is specified, and @start_editing is 
 * %TRUE, then editing should be started in the specified cell.  
 * This function is often followed by @gtk_widget_grab_focus (@tree_view) 
 * in order to give keyboard focus to the widget.  Please note that editing 
 * can only happen when the widget is realized.
 *
 * If @path is invalid for @model, the current cursor (if any) will be unset
 * and the function will return without failing.
 **/
void
gtk_tree_view_set_cursor (GtkTreeView       *tree_view,
			  GtkTreePath       *path,
			  GtkTreeViewColumn *focus_column,
			  gboolean           start_editing)
{
  gtk_tree_view_set_cursor_on_cell (tree_view, path, focus_column,
				    NULL, start_editing);
}

/**
 * gtk_tree_view_set_cursor_on_cell:
 * @tree_view: A #GtkTreeView
 * @path: A #GtkTreePath
 * @focus_column: (allow-none): A #GtkTreeViewColumn, or %NULL
 * @focus_cell: (allow-none): A #GtkCellRenderer, or %NULL
 * @start_editing: %TRUE if the specified cell should start being edited.
 *
 * Sets the current keyboard focus to be at @path, and selects it.  This is
 * useful when you want to focus the user’s attention on a particular row.  If
 * @focus_column is not %NULL, then focus is given to the column specified by
 * it. If @focus_column and @focus_cell are not %NULL, and @focus_column
 * contains 2 or more editable or activatable cells, then focus is given to
 * the cell specified by @focus_cell. Additionally, if @focus_column is
 * specified, and @start_editing is %TRUE, then editing should be started in
 * the specified cell.  This function is often followed by
 * @gtk_widget_grab_focus (@tree_view) in order to give keyboard focus to the
 * widget.  Please note that editing can only happen when the widget is
 * realized.
 *
 * If @path is invalid for @model, the current cursor (if any) will be unset
 * and the function will return without failing.
 **/
void
gtk_tree_view_set_cursor_on_cell (GtkTreeView       *tree_view,
				  GtkTreePath       *path,
				  GtkTreeViewColumn *focus_column,
				  GtkCellRenderer   *focus_cell,
				  gboolean           start_editing)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (path != NULL);
  g_return_if_fail (focus_column == NULL || GTK_IS_TREE_VIEW_COLUMN (focus_column));

  if (!tree_view->priv->model)
    return;

  if (focus_cell)
    {
      g_return_if_fail (focus_column);
      g_return_if_fail (GTK_IS_CELL_RENDERER (focus_cell));
    }

  /* cancel the current editing, if it exists */
  if (tree_view->priv->edited_column &&
      gtk_cell_area_get_edit_widget
      (gtk_cell_layout_get_area (GTK_CELL_LAYOUT (tree_view->priv->edited_column))))
    gtk_tree_view_stop_editing (tree_view, TRUE);

  gtk_tree_view_real_set_cursor (tree_view, path, CLEAR_AND_SELECT | CLAMP_NODE);

  if (focus_column &&
      gtk_tree_view_column_get_visible (focus_column))
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
      _gtk_tree_view_set_focus_column (tree_view, focus_column);
      if (focus_cell)
	gtk_tree_view_column_focus_cell (focus_column, focus_cell);
      if (start_editing)
	gtk_tree_view_start_editing (tree_view, path, TRUE);
    }
}

/**
 * gtk_tree_view_get_path_at_pos:
 * @tree_view: A #GtkTreeView.
 * @x: The x position to be identified (relative to bin_window).
 * @y: The y position to be identified (relative to bin_window).
 * @path: (out) (optional) (nullable): A pointer to a #GtkTreePath
 *   pointer to be filled in, or %NULL
 * @column: (out) (transfer none) (optional) (nullable): A pointer to
 *   a #GtkTreeViewColumn pointer to be filled in, or %NULL
 * @cell_x: (out) (optional): A pointer where the X coordinate
 *   relative to the cell can be placed, or %NULL
 * @cell_y: (out) (optional): A pointer where the Y coordinate
 *   relative to the cell can be placed, or %NULL
 *
 * Finds the path at the point (@x, @y), relative to bin_window coordinates.
 * That is, @x and @y are relative to an events coordinates. Widget-relative
 * coordinates must be converted using
 * gtk_tree_view_convert_widget_to_bin_window_coords(). It is primarily for
 * things like popup menus. If @path is non-%NULL, then it will be filled
 * with the #GtkTreePath at that point.  This path should be freed with
 * gtk_tree_path_free().  If @column is non-%NULL, then it will be filled
 * with the column at that point.  @cell_x and @cell_y return the coordinates
 * relative to the cell background (i.e. the @background_area passed to
 * gtk_cell_renderer_render()).  This function is only meaningful if
 * @tree_view is realized.  Therefore this function will always return %FALSE
 * if @tree_view is not realized or does not have a model.
 *
 * For converting widget coordinates (eg. the ones you get from
 * GtkWidget::query-tooltip), please see
 * gtk_tree_view_convert_widget_to_bin_window_coords().
 *
 * Returns: %TRUE if a row exists at that coordinate.
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
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
  gint y_offset;

  g_return_val_if_fail (tree_view != NULL, FALSE);

  if (path)
    *path = NULL;
  if (column)
    *column = NULL;

  if (tree_view->priv->tree == NULL)
    return FALSE;

  if (x > gtk_adjustment_get_upper (tree_view->priv->hadjustment))
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
      gboolean rtl;
      gint width;

      rtl = (_gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);
      for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
	   list;
	   list = (rtl ? list->prev : list->next))
	{
	  tmp_column = list->data;

	  if (gtk_tree_view_column_get_visible (tmp_column) == FALSE)
	    continue;

	  last_column = tmp_column;
          width = gtk_tree_view_column_get_width (tmp_column);
	  if (remaining_x < width)
	    {
              found = TRUE;

              if (column)
                *column = tmp_column;

              if (cell_x)
                *cell_x = remaining_x;

	      break;
	    }
	  remaining_x -= width;
	}

      /* If found is FALSE and there is a last_column, then it the remainder
       * space is in that area
       */
      if (!found)
        {
	  if (last_column)
	    {
	      if (column)
		*column = last_column;
	      
	      if (cell_x)
		*cell_x = gtk_tree_view_column_get_width (last_column) + remaining_x;
	    }
	  else
	    {
	      return FALSE;
	    }
	}
    }

  y_offset = gtk_tree_rbtree_find_offset (tree_view->priv->tree,
				      TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, y),
				      &tree, &node);

  if (tree == NULL)
    return FALSE;

  if (cell_y)
    *cell_y = y_offset;

  if (path)
    *path = _gtk_tree_path_new_from_rbtree (tree, node);

  return TRUE;
}


static inline gint
gtk_tree_view_get_cell_area_height (GtkTreeView   *tree_view,
                                    GtkTreeRBNode *node)
{
  int expander_size = gtk_tree_view_get_expander_size (tree_view);
  int height;

  /* The "cell" areas are the cell_area passed in to gtk_cell_renderer_render(),
   * i.e. just the cells, no spacing.
   *
   * The cell area height is at least expander_size - vertical_separator.
   * For regular nodes, the height is then at least expander_size. We should
   * be able to enforce the expander_size minimum here, because this
   * function will not be called for irregular (e.g. separator) rows.
   */
  height = gtk_tree_view_get_row_height (tree_view, node);
  if (height < expander_size)
    height = expander_size;

  return height;
}

static inline gint
gtk_tree_view_get_cell_area_y_offset (GtkTreeView   *tree_view,
                                      GtkTreeRBTree *tree,
                                      GtkTreeRBNode *node)
{
  int offset;

  offset = gtk_tree_view_get_row_y_offset (tree_view, tree, node);

  return offset;
}

/**
 * gtk_tree_view_get_cell_area:
 * @tree_view: a #GtkTreeView
 * @path: (allow-none): a #GtkTreePath for the row, or %NULL to get only horizontal coordinates
 * @column: (allow-none): a #GtkTreeViewColumn for the column, or %NULL to get only vertical coordinates
 * @rect: (out): rectangle to fill with cell rect
 *
 * Fills the bounding rectangle in bin_window coordinates for the cell at the
 * row specified by @path and the column specified by @column.  If @path is
 * %NULL, or points to a path not currently displayed, the @y and @height fields
 * of the rectangle will be filled with 0. If @column is %NULL, the @x and @width
 * fields will be filled with 0.  The sum of all cell rects does not cover the
 * entire tree; there are extra pixels in between rows, for example. The
 * returned rectangle is equivalent to the @cell_area passed to
 * gtk_cell_renderer_render().  This function is only valid if @tree_view is
 * realized.
 **/
void
gtk_tree_view_get_cell_area (GtkTreeView        *tree_view,
                             GtkTreePath        *path,
                             GtkTreeViewColumn  *column,
                             GdkRectangle       *rect)
{
  GtkTreeRBTree *tree = NULL;
  GtkTreeRBNode *node = NULL;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (column == NULL || GTK_IS_TREE_VIEW_COLUMN (column));
  g_return_if_fail (rect != NULL);
  g_return_if_fail (!column || gtk_tree_view_column_get_tree_view (column) == (GtkWidget *) tree_view);
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (tree_view)));

  rect->x = 0;
  rect->y = 0;
  rect->width = 0;
  rect->height = 0;

  if (column)
    {
      rect->x = gtk_tree_view_column_get_x_offset (column) + _TREE_VIEW_HORIZONTAL_SEPARATOR / 2;
      rect->width = gtk_tree_view_column_get_width (column) - _TREE_VIEW_HORIZONTAL_SEPARATOR;
    }

  if (path)
    {
      gboolean ret = _gtk_tree_view_find_node (tree_view, path, &tree, &node);

      /* Get vertical coords */
      if ((!ret && tree == NULL) || ret)
	return;

      if (row_is_separator (tree_view, NULL, path))
        {
          /* There isn't really a "cell area" for separator, so we
           * return the y, height values for background area instead.
           */
          rect->y = gtk_tree_view_get_row_y_offset (tree_view, tree, node);
          rect->height = gtk_tree_view_get_row_height (tree_view, node);
        }
      else
        {
          rect->y = gtk_tree_view_get_cell_area_y_offset (tree_view, tree, node);
          rect->height = gtk_tree_view_get_cell_area_height (tree_view, node);
        }

      if (column &&
	  gtk_tree_view_is_expander_column (tree_view, column))
	{
	  gint depth = gtk_tree_path_get_depth (path);
	  gboolean rtl;

	  rtl = _gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL;

	  if (!rtl)
	    rect->x += (depth - 1) * tree_view->priv->level_indentation;
	  rect->width -= (depth - 1) * tree_view->priv->level_indentation;

	  if (gtk_tree_view_draw_expanders (tree_view))
	    {
              int expander_size = gtk_tree_view_get_expander_size (tree_view);
	      if (!rtl)
		rect->x += depth * expander_size;
	      rect->width -= depth * expander_size;
	    }

	  rect->width = MAX (rect->width, 0);
	}
    }
}

static inline gint
gtk_tree_view_get_row_height (GtkTreeView   *tree_view,
                              GtkTreeRBNode *node)
{
  int expander_size = gtk_tree_view_get_expander_size (tree_view);
  int height;

  /* The "background" areas of all rows/cells add up to cover the entire tree.
   * The background includes all inter-row and inter-cell spacing.
   *
   * If the row pointed at by node does not have a height set, we default
   * to expander_size, which is the minimum height for regular nodes.
   * Non-regular nodes (e.g. separators) can have a height set smaller
   * than expander_size and should not be overruled here.
   */
  height = GTK_TREE_RBNODE_GET_HEIGHT (node);
  if (height <= 0)
    height = expander_size;

  return height;
}

static inline gint
gtk_tree_view_get_row_y_offset (GtkTreeView   *tree_view,
                                GtkTreeRBTree *tree,
                                GtkTreeRBNode *node)
{
  int offset;

  offset = gtk_tree_rbtree_node_find_offset (tree, node);

  return RBTREE_Y_TO_TREE_WINDOW_Y (tree_view, offset);
}

/**
 * gtk_tree_view_get_background_area:
 * @tree_view: a #GtkTreeView
 * @path: (allow-none): a #GtkTreePath for the row, or %NULL to get only horizontal coordinates
 * @column: (allow-none): a #GtkTreeViewColumn for the column, or %NULL to get only vertical coordiantes
 * @rect: (out): rectangle to fill with cell background rect
 *
 * Fills the bounding rectangle in bin_window coordinates for the cell at the
 * row specified by @path and the column specified by @column.  If @path is
 * %NULL, or points to a node not found in the tree, the @y and @height fields of
 * the rectangle will be filled with 0. If @column is %NULL, the @x and @width
 * fields will be filled with 0.  The returned rectangle is equivalent to the
 * @background_area passed to gtk_cell_renderer_render().  These background
 * areas tile to cover the entire bin window.  Contrast with the @cell_area,
 * returned by gtk_tree_view_get_cell_area(), which returns only the cell
 * itself, excluding surrounding borders and the tree expander area.
 *
 **/
void
gtk_tree_view_get_background_area (GtkTreeView        *tree_view,
                                   GtkTreePath        *path,
                                   GtkTreeViewColumn  *column,
                                   GdkRectangle       *rect)
{
  GtkTreeRBTree *tree = NULL;
  GtkTreeRBNode *node = NULL;

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

      if (!_gtk_tree_view_find_node (tree_view, path, &tree, &node) &&
	  tree == NULL)
	return;

      rect->y = gtk_tree_view_get_row_y_offset (tree_view, tree, node);
      rect->height = gtk_tree_view_get_row_height (tree_view, node);
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
 * @visible_rect: (out): rectangle to fill
 *
 * Fills @visible_rect with the currently-visible region of the
 * buffer, in tree coordinates. Convert to bin_window coordinates with
 * gtk_tree_view_convert_tree_to_bin_window_coords().
 * Tree coordinates start at 0,0 for row 0 of the tree, and cover the entire
 * scrollable area of the tree.
 **/
void
gtk_tree_view_get_visible_rect (GtkTreeView  *tree_view,
                                GdkRectangle *visible_rect)
{
  GtkAllocation allocation;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  widget = GTK_WIDGET (tree_view);

  if (visible_rect)
    {
      gtk_widget_get_allocation (widget, &allocation);
      visible_rect->x = gtk_adjustment_get_value (tree_view->priv->hadjustment);
      visible_rect->y = gtk_adjustment_get_value (tree_view->priv->vadjustment);
      visible_rect->width = allocation.width;
      visible_rect->height = allocation.height - gtk_tree_view_get_effective_header_height (tree_view);
    }
}

/**
 * gtk_tree_view_convert_widget_to_tree_coords:
 * @tree_view: a #GtkTreeView
 * @wx: X coordinate relative to the widget
 * @wy: Y coordinate relative to the widget
 * @tx: (out): return location for tree X coordinate
 * @ty: (out): return location for tree Y coordinate
 *
 * Converts widget coordinates to coordinates for the
 * tree (the full scrollable area of the tree).
 **/
void
gtk_tree_view_convert_widget_to_tree_coords (GtkTreeView *tree_view,
                                             gint         wx,
                                             gint         wy,
                                             gint        *tx,
                                             gint        *ty)
{
  gint x, y;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_tree_view_convert_widget_to_bin_window_coords (tree_view,
						      wx, wy,
						      &x, &y);
  gtk_tree_view_convert_bin_window_to_tree_coords (tree_view,
						   x, y,
						   tx, ty);
}

/**
 * gtk_tree_view_convert_tree_to_widget_coords:
 * @tree_view: a #GtkTreeView
 * @tx: X coordinate relative to the tree
 * @ty: Y coordinate relative to the tree
 * @wx: (out): return location for widget X coordinate
 * @wy: (out): return location for widget Y coordinate
 *
 * Converts tree coordinates (coordinates in full scrollable area of the tree)
 * to widget coordinates.
 **/
void
gtk_tree_view_convert_tree_to_widget_coords (GtkTreeView *tree_view,
                                             gint         tx,
                                             gint         ty,
                                             gint        *wx,
                                             gint        *wy)
{
  gint x, y;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_tree_view_convert_tree_to_bin_window_coords (tree_view,
						    tx, ty,
						    &x, &y);
  gtk_tree_view_convert_bin_window_to_widget_coords (tree_view,
						     x, y,
						     wx, wy);
}

/**
 * gtk_tree_view_convert_widget_to_bin_window_coords:
 * @tree_view: a #GtkTreeView
 * @wx: X coordinate relative to the widget
 * @wy: Y coordinate relative to the widget
 * @bx: (out): return location for bin_window X coordinate
 * @by: (out): return location for bin_window Y coordinate
 *
 * Converts widget coordinates to coordinates for the bin_window.
 **/
void
gtk_tree_view_convert_widget_to_bin_window_coords (GtkTreeView *tree_view,
                                                   gint         wx,
                                                   gint         wy,
                                                   gint        *bx,
                                                   gint        *by)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (bx)
    *bx = wx + gtk_adjustment_get_value (tree_view->priv->hadjustment);
  if (by)
    *by = wy - gtk_tree_view_get_effective_header_height (tree_view);
}

/**
 * gtk_tree_view_convert_bin_window_to_widget_coords:
 * @tree_view: a #GtkTreeView
 * @bx: bin_window X coordinate
 * @by: bin_window Y coordinate
 * @wx: (out): return location for widget X coordinate
 * @wy: (out): return location for widget Y coordinate
 *
 * Converts bin_window coordinates to widget relative coordinates.
 **/
void
gtk_tree_view_convert_bin_window_to_widget_coords (GtkTreeView *tree_view,
                                                   gint         bx,
                                                   gint         by,
                                                   gint        *wx,
                                                   gint        *wy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (wx)
    *wx = bx - gtk_adjustment_get_value (tree_view->priv->hadjustment);
  if (wy)
    *wy = by + gtk_tree_view_get_effective_header_height (tree_view);
}

/**
 * gtk_tree_view_convert_tree_to_bin_window_coords:
 * @tree_view: a #GtkTreeView
 * @tx: tree X coordinate
 * @ty: tree Y coordinate
 * @bx: (out): return location for X coordinate relative to bin_window
 * @by: (out): return location for Y coordinate relative to bin_window
 *
 * Converts tree coordinates (coordinates in full scrollable area of the tree)
 * to bin_window coordinates.
 **/
void
gtk_tree_view_convert_tree_to_bin_window_coords (GtkTreeView *tree_view,
                                                 gint         tx,
                                                 gint         ty,
                                                 gint        *bx,
                                                 gint        *by)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (bx)
    *bx = tx;
  if (by)
    *by = ty - tree_view->priv->dy;
}

/**
 * gtk_tree_view_convert_bin_window_to_tree_coords:
 * @tree_view: a #GtkTreeView
 * @bx: X coordinate relative to bin_window
 * @by: Y coordinate relative to bin_window
 * @tx: (out): return location for tree X coordinate
 * @ty: (out): return location for tree Y coordinate
 *
 * Converts bin_window coordinates to coordinates for the
 * tree (the full scrollable area of the tree).
 **/
void
gtk_tree_view_convert_bin_window_to_tree_coords (GtkTreeView *tree_view,
                                                 gint         bx,
                                                 gint         by,
                                                 gint        *tx,
                                                 gint        *ty)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tx)
    *tx = bx;
  if (ty)
    *ty = by + tree_view->priv->dy;
}



/**
 * gtk_tree_view_get_visible_range:
 * @tree_view: A #GtkTreeView
 * @start_path: (out) (allow-none): Return location for start of region,
 *              or %NULL.
 * @end_path: (out) (allow-none): Return location for end of region, or %NULL.
 *
 * Sets @start_path and @end_path to be the first and last visible path.
 * Note that there may be invisible paths in between.
 *
 * The paths should be freed with gtk_tree_path_free() after use.
 *
 * Returns: %TRUE, if valid paths were placed in @start_path and @end_path.
 **/
gboolean
gtk_tree_view_get_visible_range (GtkTreeView  *tree_view,
                                 GtkTreePath **start_path,
                                 GtkTreePath **end_path)
{
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
  gboolean retval;
  
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  if (!tree_view->priv->tree)
    return FALSE;

  retval = TRUE;

  if (start_path)
    {
      gtk_tree_rbtree_find_offset (tree_view->priv->tree,
                               TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, 0),
                               &tree, &node);
      if (node)
        *start_path = _gtk_tree_path_new_from_rbtree (tree, node);
      else
        retval = FALSE;
    }

  if (end_path)
    {
      gint y;

      if (gtk_tree_view_get_height (tree_view) < gtk_adjustment_get_page_size (tree_view->priv->vadjustment))
        y = gtk_tree_view_get_height (tree_view) - 1;
      else
        y = TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, gtk_adjustment_get_page_size (tree_view->priv->vadjustment)) - 1;

      gtk_tree_rbtree_find_offset (tree_view->priv->tree, y, &tree, &node);
      if (node)
        *end_path = _gtk_tree_path_new_from_rbtree (tree, node);
      else
        retval = FALSE;
    }

  return retval;
}

/**
 * gtk_tree_view_is_blank_at_pos:
 * @tree_view: A #GtkTreeView
 * @x: The x position to be identified (relative to bin_window)
 * @y: The y position to be identified (relative to bin_window)
 * @path: (out) (optional) (nullable): A pointer to a #GtkTreePath pointer to
 *   be filled in, or %NULL
 * @column: (out) (transfer none) (optional) (nullable): A pointer to a
 *   #GtkTreeViewColumn pointer to be filled in, or %NULL
 * @cell_x: (out) (optional): A pointer where the X coordinate relative to the
 *   cell can be placed, or %NULL
 * @cell_y: (out) (optional): A pointer where the Y coordinate relative to the
 *   cell can be placed, or %NULL
 *
 * Determine whether the point (@x, @y) in @tree_view is blank, that is no
 * cell content nor an expander arrow is drawn at the location. If so, the
 * location can be considered as the background. You might wish to take
 * special action on clicks on the background, such as clearing a current
 * selection, having a custom context menu or starting rubber banding.
 *
 * The @x and @y coordinate that are provided must be relative to bin_window
 * coordinates.  Widget-relative coordinates must be converted using
 * gtk_tree_view_convert_widget_to_bin_window_coords().
 *
 * For converting widget coordinates (eg. the ones you get from
 * GtkWidget::query-tooltip), please see
 * gtk_tree_view_convert_widget_to_bin_window_coords().
 *
 * The @path, @column, @cell_x and @cell_y arguments will be filled in
 * likewise as for gtk_tree_view_get_path_at_pos().  Please see
 * gtk_tree_view_get_path_at_pos() for more information.
 *
 * Returns: %TRUE if the area at the given coordinates is blank,
 * %FALSE otherwise.
 */
gboolean
gtk_tree_view_is_blank_at_pos (GtkTreeView       *tree_view,
                               gint                x,
                               gint                y,
                               GtkTreePath       **path,
                               GtkTreeViewColumn **column,
                               gint               *cell_x,
                               gint               *cell_y)
{
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
  GtkTreeIter iter;
  GtkTreePath *real_path;
  GtkTreeViewColumn *real_column;
  GdkRectangle cell_area, background_area;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  if (!gtk_tree_view_get_path_at_pos (tree_view, x, y,
                                      &real_path, &real_column,
                                      cell_x, cell_y))
    /* If there's no path here, it is blank */
    return TRUE;

  if (path)
    *path = real_path;

  if (column)
    *column = real_column;

  gtk_tree_model_get_iter (tree_view->priv->model, &iter, real_path);
  _gtk_tree_view_find_node (tree_view, real_path, &tree, &node);

  /* Check if there's an expander arrow at (x, y) */
  if (real_column == tree_view->priv->expander_column
      && gtk_tree_view_draw_expanders (tree_view))
    {
      gboolean over_arrow;

      over_arrow = coords_are_over_arrow (tree_view, tree, node, x, y);

      if (over_arrow)
        {
          if (!path)
            gtk_tree_path_free (real_path);
          return FALSE;
        }
    }

  /* Otherwise, have the column see if there's a cell at (x, y) */
  gtk_tree_view_column_cell_set_cell_data (real_column,
                                           tree_view->priv->model,
                                           &iter,
                                           GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_PARENT),
                                           node->children ? TRUE : FALSE);

  gtk_tree_view_get_background_area (tree_view, real_path, real_column,
                                     &background_area);
  gtk_tree_view_get_cell_area (tree_view, real_path, real_column,
                               &cell_area);

  if (!path)
    gtk_tree_path_free (real_path);

  return _gtk_tree_view_column_is_blank_at_pos (real_column,
                                                &cell_area,
                                                &background_area,
                                                x, y);
}

static void
unset_reorderable (GtkTreeView *tree_view)
{
  if (tree_view->priv->reorderable)
    {
      tree_view->priv->reorderable = FALSE;
      g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_REORDERABLE]);
    }
}

/**
 * gtk_tree_view_enable_model_drag_source:
 * @tree_view: a #GtkTreeView
 * @start_button_mask: Mask of allowed buttons to start drag
 * @formats: the target formats that the drag will support
 * @actions: the bitmask of possible actions for a drag from this
 *    widget
 *
 * Turns @tree_view into a drag source for automatic DND. Calling this
 * method sets #GtkTreeView:reorderable to %FALSE.
 **/
void
gtk_tree_view_enable_model_drag_source (GtkTreeView       *tree_view,
					GdkModifierType    start_button_mask,
					GdkContentFormats *formats,
					GdkDragAction      actions)
{
  TreeViewDragInfo *di;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_drag_source_set (GTK_WIDGET (tree_view),
		       0,
		       formats,
		       actions);

  di = ensure_info (tree_view);

  di->start_button_mask = start_button_mask;
  di->source_actions = actions;
  di->source_set = TRUE;

  unset_reorderable (tree_view);
}

/**
 * gtk_tree_view_enable_model_drag_dest:
 * @tree_view: a #GtkTreeView
 * @formats: the target formats that the drag will support
 * @actions: the bitmask of possible actions for a drag from this
 *    widget
 * 
 * Turns @tree_view into a drop destination for automatic DND. Calling
 * this method sets #GtkTreeView:reorderable to %FALSE.
 **/
void
gtk_tree_view_enable_model_drag_dest (GtkTreeView       *tree_view,
				      GdkContentFormats *formats,
				      GdkDragAction      actions)
{
  TreeViewDragInfo *di;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_drag_dest_set (GTK_WIDGET (tree_view),
                     0,
                     formats,
                     actions);

  di = ensure_info (tree_view);
  di->dest_set = TRUE;

  unset_reorderable (tree_view);
}

/**
 * gtk_tree_view_unset_rows_drag_source:
 * @tree_view: a #GtkTreeView
 *
 * Undoes the effect of
 * gtk_tree_view_enable_model_drag_source(). Calling this method sets
 * #GtkTreeView:reorderable to %FALSE.
 **/
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
          gtk_drag_source_unset (GTK_WIDGET (tree_view));
          di->source_set = FALSE;
        }

      if (!di->dest_set && !di->source_set)
        remove_info (tree_view);
    }
  
  unset_reorderable (tree_view);
}

/**
 * gtk_tree_view_unset_rows_drag_dest:
 * @tree_view: a #GtkTreeView
 *
 * Undoes the effect of
 * gtk_tree_view_enable_model_drag_dest(). Calling this method sets
 * #GtkTreeView:reorderable to %FALSE.
 **/
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
          di->dest_set = FALSE;
        }

      if (!di->dest_set && !di->source_set)
        remove_info (tree_view);
    }

  unset_reorderable (tree_view);
}

/**
 * gtk_tree_view_set_drag_dest_row:
 * @tree_view: a #GtkTreeView
 * @path: (allow-none): The path of the row to highlight, or %NULL
 * @pos: Specifies whether to drop before, after or into the row
 *
 * Sets the row that is highlighted for feedback.
 * If @path is %NULL, an existing highlight is removed.
 */
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
    {
      current_dest = gtk_tree_row_reference_get_path (tree_view->priv->drag_dest_row);
      gtk_tree_row_reference_free (tree_view->priv->drag_dest_row);
    }

  /* special case a drop on an empty model */
  tree_view->priv->empty_view_drop = 0;

  if (pos == GTK_TREE_VIEW_DROP_BEFORE && path
      && gtk_tree_path_get_depth (path) == 1
      && gtk_tree_path_get_indices (path)[0] == 0)
    {
      gint n_children;

      n_children = gtk_tree_model_iter_n_children (tree_view->priv->model,
                                                   NULL);

      if (!n_children)
        tree_view->priv->empty_view_drop = 1;
    }

  tree_view->priv->drag_dest_pos = pos;

  if (path)
    {
      tree_view->priv->drag_dest_row =
        gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view), tree_view->priv->model, path);
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
    }
  else
    tree_view->priv->drag_dest_row = NULL;

  if (current_dest)
    {
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));

      gtk_tree_path_free (current_dest);
    }
}

/**
 * gtk_tree_view_get_drag_dest_row:
 * @tree_view: a #GtkTreeView
 * @path: (out) (optional) (nullable): Return location for the path of the highlighted row, or %NULL.
 * @pos: (out) (optional): Return location for the drop position, or %NULL
 * 
 * Gets information about the row that is highlighted for feedback.
 **/
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
        {
          if (tree_view->priv->empty_view_drop)
            *path = gtk_tree_path_new_from_indices (0, -1);
          else
            *path = NULL;
        }
    }

  if (pos)
    *pos = tree_view->priv->drag_dest_pos;
}

/**
 * gtk_tree_view_get_dest_row_at_pos:
 * @tree_view: a #GtkTreeView
 * @drag_x: the position to determine the destination row for
 * @drag_y: the position to determine the destination row for
 * @path: (out) (optional) (nullable): Return location for the path of
 *   the highlighted row, or %NULL.
 * @pos: (out) (optional): Return location for the drop position, or
 *   %NULL
 * 
 * Determines the destination row for a given position.  @drag_x and
 * @drag_y are expected to be in widget coordinates.  This function is only
 * meaningful if @tree_view is realized.  Therefore this function will always
 * return %FALSE if @tree_view is not realized or does not have a model.
 * 
 * Returns: whether there is a row at the given position, %TRUE if this
 * is indeed the case.
 **/
gboolean
gtk_tree_view_get_dest_row_at_pos (GtkTreeView             *tree_view,
                                   gint                     drag_x,
                                   gint                     drag_y,
                                   GtkTreePath            **path,
                                   GtkTreeViewDropPosition *pos)
{
  gint cell_y;
  gint bin_x, bin_y;
  gdouble offset_into_row;
  gdouble fourth;
  GdkRectangle cell;
  GtkTreeViewColumn *column = NULL;
  GtkTreePath *tmp_path = NULL;

  /* Note; this function is exported to allow a custom DND
   * implementation, so it can't touch TreeViewDragInfo
   */

  g_return_val_if_fail (tree_view != NULL, FALSE);
  g_return_val_if_fail (drag_x >= 0, FALSE);
  g_return_val_if_fail (drag_y >= 0, FALSE);

  if (path)
    *path = NULL;

  if (tree_view->priv->tree == NULL)
    return FALSE;

  /* If in the top fourth of a row, we drop before that row; if
   * in the bottom fourth, drop after that row; if in the middle,
   * and the row has children, drop into the row.
   */
  gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, drag_x, drag_y,
						      &bin_x, &bin_y);

  if (!gtk_tree_view_get_path_at_pos (tree_view,
				      bin_x,
				      bin_y,
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

  fourth = cell.height / 4.0;

  if (pos)
    {
      if (offset_into_row < fourth)
        {
          *pos = GTK_TREE_VIEW_DROP_BEFORE;
        }
      else if (offset_into_row < (cell.height / 2.0))
        {
          *pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
        }
      else if (offset_into_row < cell.height - fourth)
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


static void
gtk_treeview_snapshot_border (GtkSnapshot           *snapshot,
                              const graphene_rect_t *rect)
{
  GskRoundedRect rounded;
  GskRenderNode *border_node;

  gsk_rounded_rect_init_from_rect (&rounded, rect, 0);

#define BLACK { 0, 0, 0, 1 }
  border_node = gsk_border_node_new (&rounded,
                                     (float[4]) { 1, 1, 1, 1 },
                                     (GdkRGBA[4]) { BLACK, BLACK, BLACK, BLACK });
#undef BLACK

  gtk_snapshot_append_node (snapshot, border_node);

  gsk_render_node_unref (border_node);
}

/* KEEP IN SYNC WITH GTK_TREE_VIEW_BIN_EXPOSE */
/**
 * gtk_tree_view_create_row_drag_icon:
 * @tree_view: a #GtkTreeView
 * @path: a #GtkTreePath in @tree_view
 *
 * Creates a #cairo_surface_t representation of the row at @path.  
 * This image is used for a drag icon.
 *
 * Returns: (transfer full): a newly-allocated surface of the drag icon.
 **/
GdkPaintable *
gtk_tree_view_create_row_drag_icon (GtkTreeView  *tree_view,
                                    GtkTreePath  *path)
{
  GtkTreeIter iter;
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
  GtkStyleContext *context;
  gint cell_offset;
  GList *list;
  GdkRectangle background_area;
  GtkWidget *widget;
  GtkSnapshot *snapshot;
  GdkPaintable *paintable;
  gint depth;
  /* start drawing inside the black outline */
  gint x = 1, y = 1;
  gint bin_window_width;
  gboolean is_separator = FALSE;
  gboolean rtl;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  widget = GTK_WIDGET (tree_view);

  if (!gtk_widget_get_realized (widget))
    return NULL;

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

  context = gtk_widget_get_style_context (widget);

  is_separator = row_is_separator (tree_view, &iter, NULL);

  cell_offset = x;

  background_area.y = y;
  background_area.height = gtk_tree_view_get_row_height (tree_view, node);

  bin_window_width = gtk_widget_get_width (GTK_WIDGET (tree_view));

  snapshot = gtk_snapshot_new ();

  gtk_snapshot_render_background (snapshot, context,
                                  0, 0,
                                  bin_window_width + 2,
                                  background_area.height + 2);

  rtl = gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL;

  for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
      list;
      list = (rtl ? list->prev : list->next))
    {
      GtkTreeViewColumn *column = list->data;
      GdkRectangle cell_area;

      if (!gtk_tree_view_column_get_visible (column))
        continue;

      gtk_tree_view_column_cell_set_cell_data (column, tree_view->priv->model, &iter,
					       GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_IS_PARENT),
					       node->children?TRUE:FALSE);

      background_area.x = cell_offset;
      background_area.width = gtk_tree_view_column_get_width (column);

      cell_area = background_area;

      if (gtk_tree_view_is_expander_column (tree_view, column))
        {
	  if (!rtl)
	    cell_area.x += (depth - 1) * tree_view->priv->level_indentation;
	  cell_area.width -= (depth - 1) * tree_view->priv->level_indentation;

          if (gtk_tree_view_draw_expanders (tree_view))
	    {
              int expander_size = gtk_tree_view_get_expander_size (tree_view);
	      if (!rtl)
		cell_area.x += depth * expander_size;
	      cell_area.width -= depth * expander_size;
	    }
        }

      if (gtk_tree_view_column_cell_is_visible (column))
	{
	  if (is_separator)
            {
              GdkRGBA color;

              gtk_style_context_save (context);
              gtk_style_context_add_class (context, GTK_STYLE_CLASS_SEPARATOR);

              gtk_style_context_get_color (context, &color);
              gtk_snapshot_append_color (snapshot,
                                         &color, 
                                         &GRAPHENE_RECT_INIT(
                                             cell_area.x,
                                             cell_area.y + cell_area.height / 2,
                                             cell_area.x + cell_area.width,
                                             1
                                         ));

              gtk_style_context_restore (context);
            }
	  else
            {
              gtk_tree_view_column_cell_snapshot (column,
                                                  snapshot,
                                                  &background_area,
                                                  &cell_area,
                                                  0, FALSE);
            }
	}
      cell_offset += gtk_tree_view_column_get_width (column);
    }

  gtk_treeview_snapshot_border (snapshot,
                                &GRAPHENE_RECT_INIT(0, 0, bin_window_width + 2, background_area.height + 2));

  paintable = gtk_snapshot_free_to_paintable (snapshot, NULL);

  return paintable;
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
 * the tree interactively (this is sometimes called "typeahead find").
 * 
 * Note that even if this is %FALSE, the user can still initiate a search 
 * using the “start-interactive-search” key binding.
 */
void
gtk_tree_view_set_enable_search (GtkTreeView *tree_view,
				 gboolean     enable_search)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  enable_search = !!enable_search;
  
  if (tree_view->priv->enable_search != enable_search)
    {
       tree_view->priv->enable_search = enable_search;
       g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_ENABLE_SEARCH]);
    }
}

/**
 * gtk_tree_view_get_enable_search:
 * @tree_view: A #GtkTreeView
 *
 * Returns whether or not the tree allows to start interactive searching 
 * by typing in text.
 *
 * Returns: whether or not to let the user search interactively
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
 * Returns: the column the interactive search code searches in.
 */
gint
gtk_tree_view_get_search_column (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);

  return (tree_view->priv->search_column);
}

/**
 * gtk_tree_view_set_search_column:
 * @tree_view: A #GtkTreeView
 * @column: the column of the model to search in, or -1 to disable searching
 *
 * Sets @column as the column where the interactive search code should
 * search in for the current model. 
 * 
 * If the search column is set, users can use the “start-interactive-search”
 * key binding to bring up search popup. The enable-search property controls
 * whether simply typing text will also start an interactive search.
 *
 * Note that @column refers to a column of the current model. The search 
 * column is reset to -1 when the model is changed.
 */
void
gtk_tree_view_set_search_column (GtkTreeView *tree_view,
				 gint         column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (column >= -1);

  if (tree_view->priv->search_column == column)
    return;

  tree_view->priv->search_column = column;
  g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_SEARCH_COLUMN]);
}

/**
 * gtk_tree_view_get_search_equal_func: (skip)
 * @tree_view: A #GtkTreeView
 *
 * Returns the compare function currently in use.
 *
 * Returns: the currently used compare function for the search code.
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
 * @search_user_data: (allow-none): user data to pass to @search_equal_func, or %NULL
 * @search_destroy: (allow-none): Destroy notifier for @search_user_data, or %NULL
 *
 * Sets the compare function for the interactive search capabilities; note
 * that somewhat like strcmp() returning 0 for equality
 * #GtkTreeViewSearchEqualFunc returns %FALSE on matches.
 **/
void
gtk_tree_view_set_search_equal_func (GtkTreeView                *tree_view,
				     GtkTreeViewSearchEqualFunc  search_equal_func,
				     gpointer                    search_user_data,
				     GDestroyNotify              search_destroy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (search_equal_func != NULL);

  if (tree_view->priv->search_destroy)
    tree_view->priv->search_destroy (tree_view->priv->search_user_data);

  tree_view->priv->search_equal_func = search_equal_func;
  tree_view->priv->search_user_data = search_user_data;
  tree_view->priv->search_destroy = search_destroy;
  if (tree_view->priv->search_equal_func == NULL)
    tree_view->priv->search_equal_func = gtk_tree_view_search_equal_func;
}

/**
 * gtk_tree_view_get_search_entry:
 * @tree_view: A #GtkTreeView
 *
 * Returns the #GtkEntry which is currently in use as interactive search
 * entry for @tree_view.  In case the built-in entry is being used, %NULL
 * will be returned.
 *
 * Returns: (transfer none): the entry currently in use as search entry.
 */
GtkEditable *
gtk_tree_view_get_search_entry (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  if (tree_view->priv->search_custom_entry_set)
    return GTK_EDITABLE (tree_view->priv->search_entry);

  return NULL;
}

/**
 * gtk_tree_view_set_search_entry:
 * @tree_view: A #GtkTreeView
 * @entry: (allow-none): the entry the interactive search code of @tree_view should use or %NULL
 *
 * Sets the entry which the interactive search code will use for this
 * @tree_view.  This is useful when you want to provide a search entry
 * in our interface at all time at a fixed position.  Passing %NULL for
 * @entry will make the interactive search code use the built-in popup
 * entry again.
 */
void
gtk_tree_view_set_search_entry (GtkTreeView *tree_view,
				GtkEditable *entry)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (entry == NULL || GTK_IS_ENTRY (entry) || GTK_IS_SEARCH_ENTRY (entry));

  if (tree_view->priv->search_custom_entry_set)
    {
      if (tree_view->priv->search_entry_changed_id)
        {
	  g_signal_handler_disconnect (tree_view->priv->search_entry,
				       tree_view->priv->search_entry_changed_id);
	  tree_view->priv->search_entry_changed_id = 0;
	}

      g_signal_handlers_disconnect_by_func (gtk_entry_get_key_controller (GTK_ENTRY (tree_view->priv->search_entry)),
					    G_CALLBACK (gtk_tree_view_search_key_pressed),
					    tree_view);

      g_object_unref (tree_view->priv->search_entry);
    }
  else if (tree_view->priv->search_window)
    {
      gtk_tree_view_destroy_search_window (tree_view);
    }

  if (entry)
    {
      GtkEventController *controller;

      tree_view->priv->search_entry = GTK_WIDGET (g_object_ref (entry));
      tree_view->priv->search_custom_entry_set = TRUE;

      if (tree_view->priv->search_entry_changed_id == 0)
        {
          tree_view->priv->search_entry_changed_id =
	    g_signal_connect (tree_view->priv->search_entry, "changed",
			      G_CALLBACK (gtk_tree_view_search_init),
			      tree_view);
	}

      if (GTK_IS_ENTRY (entry))
        controller = gtk_entry_get_key_controller (GTK_ENTRY (entry));
      else
        controller = gtk_search_entry_get_key_controller (GTK_SEARCH_ENTRY (entry));
      g_signal_connect (controller, "key-pressed",
                        G_CALLBACK (gtk_tree_view_search_key_pressed), tree_view);

      gtk_tree_view_search_init (tree_view->priv->search_entry, tree_view);
    }
  else
    {
      tree_view->priv->search_entry = NULL;
      tree_view->priv->search_custom_entry_set = FALSE;
    }
}

/**
 * gtk_tree_view_set_search_position_func:
 * @tree_view: A #GtkTreeView
 * @func: (allow-none): the function to use to position the search dialog, or %NULL
 *    to use the default search position function
 * @data: (allow-none): user data to pass to @func, or %NULL
 * @destroy: (allow-none): Destroy notifier for @data, or %NULL
 *
 * Sets the function to use when positioning the search dialog.
 **/
void
gtk_tree_view_set_search_position_func (GtkTreeView                   *tree_view,
				        GtkTreeViewSearchPositionFunc  func,
				        gpointer                       user_data,
				        GDestroyNotify                 destroy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->search_position_destroy)
    tree_view->priv->search_position_destroy (tree_view->priv->search_position_user_data);

  tree_view->priv->search_position_func = func;
  tree_view->priv->search_position_user_data = user_data;
  tree_view->priv->search_position_destroy = destroy;
  if (tree_view->priv->search_position_func == NULL)
    tree_view->priv->search_position_func = gtk_tree_view_search_position_func;
}

/**
 * gtk_tree_view_get_search_position_func: (skip)
 * @tree_view: A #GtkTreeView
 *
 * Returns the positioning function currently in use.
 *
 * Returns: the currently used function for positioning the search dialog.
 */
GtkTreeViewSearchPositionFunc
gtk_tree_view_get_search_position_func (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->search_position_func;
}


static void
gtk_tree_view_search_window_hide (GtkWidget   *search_window,
                                  GtkTreeView *tree_view,
                                  GdkDevice   *device)
{
  if (tree_view->priv->disable_popdown)
    return;

  if (tree_view->priv->search_entry_changed_id)
    {
      g_signal_handler_disconnect (tree_view->priv->search_entry,
				   tree_view->priv->search_entry_changed_id);
      tree_view->priv->search_entry_changed_id = 0;
    }
  if (tree_view->priv->typeselect_flush_timeout)
    {
      g_source_remove (tree_view->priv->typeselect_flush_timeout);
      tree_view->priv->typeselect_flush_timeout = 0;
    }
	
  if (gtk_widget_get_visible (search_window))
    {
      /* send focus-in event */
      send_focus_change (GTK_WIDGET (tree_view->priv->search_entry), device, FALSE);
      gtk_widget_hide (search_window);
      gtk_editable_set_text (GTK_EDITABLE(tree_view->priv->search_entry), "");
      send_focus_change (GTK_WIDGET (tree_view), device, TRUE);
    }
}

static void
gtk_tree_view_search_position_func (GtkTreeView *tree_view,
				    GtkWidget   *search_window,
				    gpointer     user_data)
{
}

/* Because we're visible but offscreen, we just set a flag in the preedit
 * callback.
 */
static void
gtk_tree_view_search_preedit_changed (GtkText      *text,
                                      const char   *predit,
				      GtkTreeView  *tree_view)
{
  tree_view->priv->imcontext_changed = 1;
  if (tree_view->priv->typeselect_flush_timeout)
    {
      g_source_remove (tree_view->priv->typeselect_flush_timeout);
      tree_view->priv->typeselect_flush_timeout =
	g_timeout_add (GTK_TREE_VIEW_SEARCH_DIALOG_TIMEOUT,
                       (GSourceFunc) gtk_tree_view_search_entry_flush_timeout,
		       tree_view);
      g_source_set_name_by_id (tree_view->priv->typeselect_flush_timeout, "[gtk] gtk_tree_view_search_entry_flush_timeout");
    }

}

static void
gtk_tree_view_search_changed (GtkEditable *editable,
                              GtkTreeView  *tree_view)
{
  tree_view->priv->imcontext_changed = 1;
}

static void
gtk_tree_view_search_activate (GtkEntry    *entry,
			       GtkTreeView *tree_view)
{
  GtkTreePath *path;

  gtk_tree_view_search_window_hide (tree_view->priv->search_window,
                                    tree_view,
                                    gtk_get_current_event_device ());

  /* If we have a row selected and it's the cursor row, we activate
   * the row XXX */
  if (tree_view->priv->cursor_node &&
      GTK_TREE_RBNODE_FLAG_SET (tree_view->priv->cursor_node, GTK_TREE_RBNODE_IS_SELECTED))
    {
      path = _gtk_tree_path_new_from_rbtree (tree_view->priv->cursor_tree,
                                             tree_view->priv->cursor_node);
      
      gtk_tree_view_row_activated (tree_view, path, tree_view->priv->focus_column);
      
      gtk_tree_path_free (path);
    }
}

static void
gtk_tree_view_search_pressed_cb (GtkGesture  *gesture,
                                 int          n_press,
                                 double       x,
                                 double       y,
                                 GtkTreeView *tree_view)
{
  GdkDevice *keyb_device;
  GdkEventSequence *sequence;
  const GdkEvent *event;
  GtkWidget *widget;

  sequence = gtk_gesture_get_last_updated_sequence (gesture);
  event = gtk_gesture_get_last_event (gesture, sequence);
  keyb_device = gdk_device_get_associated_device (gdk_event_get_device (event));
  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  gtk_tree_view_search_window_hide (widget, tree_view, keyb_device);
}

static gboolean
gtk_tree_view_search_scroll_event (GtkWidget   *widget,
                                   gdouble      dx,
                                   gdouble      dy,
				   GtkTreeView *tree_view)
{
  GdkScrollDirection direction;

  direction = dy > 0 ? GDK_SCROLL_DOWN : GDK_SCROLL_UP;

  if (direction == GDK_SCROLL_UP)
    gtk_tree_view_search_move (widget, tree_view, TRUE);
  else if (direction == GDK_SCROLL_DOWN)
    gtk_tree_view_search_move (widget, tree_view, FALSE);

  /* renew the flush timeout */
  if (tree_view->priv->typeselect_flush_timeout &&
      !tree_view->priv->search_custom_entry_set)
    {
      g_source_remove (tree_view->priv->typeselect_flush_timeout);
      tree_view->priv->typeselect_flush_timeout =
	g_timeout_add (GTK_TREE_VIEW_SEARCH_DIALOG_TIMEOUT,
		       (GSourceFunc) gtk_tree_view_search_entry_flush_timeout,
		       tree_view);
      g_source_set_name_by_id (tree_view->priv->typeselect_flush_timeout, "[gtk] gtk_tree_view_search_entry_flush_timeout");
    }

  return GDK_EVENT_STOP;
}

static gboolean
gtk_tree_view_search_key_pressed (GtkEventControllerKey *key,
                                  guint                  keyval,
                                  guint                  keycode,
                                  GdkModifierType        state,
                                  GtkTreeView           *tree_view)
{
  GtkWidget *widget = tree_view->priv->search_entry;
  GdkModifierType default_accel;
  gboolean retval = FALSE;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  /* close window and cancel the search */
  if (!tree_view->priv->search_custom_entry_set
      && gtk_tree_view_search_key_cancels_search (keyval))
    {
      gtk_tree_view_search_window_hide (widget, tree_view,
                                        gtk_get_current_event_device ());
      return TRUE;
    }

  default_accel = gtk_widget_get_modifier_mask (widget,
                                                GDK_MODIFIER_INTENT_PRIMARY_ACCELERATOR);

  /* select previous matching iter */
  if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up)
    {
      if (!gtk_tree_view_search_move (widget, tree_view, TRUE))
        gtk_widget_error_bell (widget);

      retval = TRUE;
    }

  if (((state & (default_accel | GDK_SHIFT_MASK)) == (default_accel | GDK_SHIFT_MASK))
      && (keyval == GDK_KEY_g || keyval == GDK_KEY_G))
    {
      if (!gtk_tree_view_search_move (widget, tree_view, TRUE))
        gtk_widget_error_bell (widget);

      retval = TRUE;
    }

  /* select next matching iter */
  if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
    {
      if (!gtk_tree_view_search_move (widget, tree_view, FALSE))
        gtk_widget_error_bell (widget);

      retval = TRUE;
    }

  if (((state & (default_accel | GDK_SHIFT_MASK)) == default_accel)
      && (keyval == GDK_KEY_g || keyval == GDK_KEY_G))
    {
      if (!gtk_tree_view_search_move (widget, tree_view, FALSE))
        gtk_widget_error_bell (widget);

      retval = TRUE;
    }

  /* renew the flush timeout */
  if (retval && tree_view->priv->typeselect_flush_timeout
      && !tree_view->priv->search_custom_entry_set)
    {
      g_source_remove (tree_view->priv->typeselect_flush_timeout);
      tree_view->priv->typeselect_flush_timeout =
	g_timeout_add (GTK_TREE_VIEW_SEARCH_DIALOG_TIMEOUT,
		       (GSourceFunc) gtk_tree_view_search_entry_flush_timeout,
		       tree_view);
      g_source_set_name_by_id (tree_view->priv->typeselect_flush_timeout, "[gtk] gtk_tree_view_search_entry_flush_timeout");
    }

  if (!retval)
    gtk_event_controller_key_forward (key, tree_view->priv->search_entry);

  return retval;
}

/*  this function returns FALSE if there is a search string but
 *  nothing was found, and TRUE otherwise.
 */
static gboolean
gtk_tree_view_search_move (GtkWidget   *window,
			   GtkTreeView *tree_view,
			   gboolean     up)
{
  gboolean ret;
  gint len;
  gint count = 0;
  const gchar *text;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection;

  text = gtk_editable_get_text (GTK_EDITABLE (tree_view->priv->search_entry));

  g_return_val_if_fail (text != NULL, FALSE);

  len = strlen (text);

  if (up && tree_view->priv->selected_iter == 1)
    return len < 1;

  if (len < 1)
    return TRUE;

  model = gtk_tree_view_get_model (tree_view);
  selection = gtk_tree_view_get_selection (tree_view);

  /* search */
  gtk_tree_selection_unselect_all (selection);
  if (!gtk_tree_model_get_iter_first (model, &iter))
    return TRUE;

  ret = gtk_tree_view_search_iter (model, selection, &iter, text,
				   &count, up?((tree_view->priv->selected_iter) - 1):((tree_view->priv->selected_iter + 1)));

  if (ret)
    {
      /* found */
      tree_view->priv->selected_iter += up?(-1):(1);
      return TRUE;
    }
  else
    {
      /* return to old iter */
      count = 0;
      gtk_tree_model_get_iter_first (model, &iter);
      gtk_tree_view_search_iter (model, selection,
				 &iter, text,
				 &count, tree_view->priv->selected_iter);
      return FALSE;
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
  const gchar *str;
  gchar *normalized_string;
  gchar *normalized_key;
  gchar *case_normalized_string = NULL;
  gchar *case_normalized_key = NULL;
  GValue value = G_VALUE_INIT;
  GValue transformed = G_VALUE_INIT;

  gtk_tree_model_get_value (model, iter, column, &value);

  g_value_init (&transformed, G_TYPE_STRING);

  if (!g_value_transform (&value, &transformed))
    {
      g_value_unset (&value);
      return TRUE;
    }

  g_value_unset (&value);

  str = g_value_get_string (&transformed);
  if (!str)
    {
      g_value_unset (&transformed);
      return TRUE;
    }

  normalized_string = g_utf8_normalize (str, -1, G_NORMALIZE_ALL);
  normalized_key = g_utf8_normalize (key, -1, G_NORMALIZE_ALL);

  if (normalized_string && normalized_key)
    {
      case_normalized_string = g_utf8_casefold (normalized_string, -1);
      case_normalized_key = g_utf8_casefold (normalized_key, -1);

      if (strncmp (case_normalized_key, case_normalized_string, strlen (case_normalized_key)) == 0)
        retval = FALSE;
    }

  g_value_unset (&transformed);
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
  GtkTreeRBTree *tree = NULL;
  GtkTreeRBNode *node = NULL;
  GtkTreePath *path;

  GtkTreeView *tree_view = gtk_tree_selection_get_tree_view (selection);

  path = gtk_tree_model_get_path (model, iter);
  _gtk_tree_view_find_node (tree_view, path, &tree, &node);

  do
    {
      if (! tree_view->priv->search_equal_func (model, tree_view->priv->search_column, text, iter, tree_view->priv->search_user_data))
        {
          (*count)++;
          if (*count == n)
            {
              gtk_tree_view_scroll_to_cell (tree_view, path, NULL,
					    TRUE, 0.5, 0.0);
              gtk_tree_selection_select_iter (selection, iter);
              gtk_tree_view_real_set_cursor (tree_view, path, CLAMP_NODE);

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
          node = gtk_tree_rbtree_first (tree);

	  tmp = *iter;
	  has_child = gtk_tree_model_iter_children (model, iter, &tmp);
	  gtk_tree_path_down (path);

	  /* sanity check */
	  TREE_VIEW_INTERNAL_ASSERT (has_child, FALSE);
	}
      else
	{
	  gboolean done = FALSE;

	  do
	    {
	      node = gtk_tree_rbtree_next (tree, node);

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

  return FALSE;
}

static void
gtk_tree_view_search_init (GtkWidget   *entry,
			   GtkTreeView *tree_view)
{
  gint ret;
  gint count = 0;
  const gchar *text;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  text = gtk_editable_get_text (GTK_EDITABLE (entry));

  model = gtk_tree_view_get_model (tree_view);
  selection = gtk_tree_view_get_selection (tree_view);

  /* search */
  gtk_tree_selection_unselect_all (selection);
  if (tree_view->priv->typeselect_flush_timeout
      && !tree_view->priv->search_custom_entry_set)
    {
      g_source_remove (tree_view->priv->typeselect_flush_timeout);
      tree_view->priv->typeselect_flush_timeout =
	g_timeout_add (GTK_TREE_VIEW_SEARCH_DIALOG_TIMEOUT,
		       (GSourceFunc) gtk_tree_view_search_entry_flush_timeout,
		       tree_view);
      g_source_set_name_by_id (tree_view->priv->typeselect_flush_timeout, "[gtk] gtk_tree_view_search_entry_flush_timeout");
    }

  if (*text == '\0')
    return;

  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;

  ret = gtk_tree_view_search_iter (model, selection,
				   &iter, text,
				   &count, 1);

  if (ret)
    tree_view->priv->selected_iter = 1;
}

void
_gtk_tree_view_remove_editable (GtkTreeView       *tree_view,
                                GtkTreeViewColumn *column,
                                GtkCellEditable   *cell_editable)
{
  if (tree_view->priv->edited_column == NULL)
    return;

  g_return_if_fail (column == tree_view->priv->edited_column);

  tree_view->priv->edited_column = NULL;

  if (gtk_widget_has_focus (GTK_WIDGET (cell_editable)))
    gtk_widget_grab_focus (GTK_WIDGET (tree_view));

  gtk_container_remove (GTK_CONTAINER (tree_view),
                        GTK_WIDGET (cell_editable));

  /* FIXME should only redraw a single node */
  gtk_widget_queue_draw (GTK_WIDGET (tree_view));
}

static gboolean
gtk_tree_view_start_editing (GtkTreeView *tree_view,
			     GtkTreePath *cursor_path,
			     gboolean     edit_only)
{
  GtkTreeIter iter;
  GdkRectangle cell_area;
  GtkTreeViewColumn *focus_column;
  guint flags = 0; /* can be 0, as the flags are primarily for rendering */
  gint retval = FALSE;
  GtkTreeRBTree *cursor_tree;
  GtkTreeRBNode *cursor_node;

  g_assert (tree_view->priv->focus_column);
  focus_column = tree_view->priv->focus_column;

  if (!gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    return FALSE;

  if (_gtk_tree_view_find_node (tree_view, cursor_path, &cursor_tree, &cursor_node) ||
      cursor_node == NULL)
    return FALSE;

  gtk_tree_model_get_iter (tree_view->priv->model, &iter, cursor_path);

  validate_row (tree_view, cursor_tree, cursor_node, &iter, cursor_path);

  gtk_tree_view_column_cell_set_cell_data (focus_column,
                                           tree_view->priv->model,
                                           &iter,
                                           GTK_TREE_RBNODE_FLAG_SET (cursor_node, GTK_TREE_RBNODE_IS_PARENT),
                                           cursor_node->children ? TRUE : FALSE);
  gtk_tree_view_get_cell_area (tree_view,
                               cursor_path,
                               focus_column,
                               &cell_area);

  if (gtk_cell_area_activate (gtk_cell_layout_get_area (GTK_CELL_LAYOUT (focus_column)),
                              _gtk_tree_view_column_get_context (focus_column),
                              GTK_WIDGET (tree_view),
                              &cell_area,
                              flags, edit_only))
    retval = TRUE;

  return retval;
}

void
_gtk_tree_view_add_editable (GtkTreeView       *tree_view,
                             GtkTreeViewColumn *column,
                             GtkTreePath       *path,
                             GtkCellEditable   *cell_editable,
                             GdkRectangle      *cell_area)
{
  GdkRectangle full_area;
  GtkBorder border;

  tree_view->priv->edited_column = column;

  gtk_tree_view_real_set_cursor (tree_view, path, CLAMP_NODE);

  tree_view->priv->draw_keyfocus = TRUE;

  gtk_tree_view_get_cell_area (tree_view, path, column, &full_area);
  border.left = cell_area->x - full_area.x;
  border.top = cell_area->y - full_area.y;
  border.right = (full_area.x + full_area.width) - (cell_area->x + cell_area->width);
  border.bottom = (full_area.y + full_area.height) - (cell_area->y + cell_area->height);

  gtk_tree_view_put (tree_view,
                     GTK_WIDGET (cell_editable),
                     path,
                     column,
                     &border);
}

static void
gtk_tree_view_stop_editing (GtkTreeView *tree_view,
			    gboolean     cancel_editing)
{
  GtkTreeViewColumn *column;

  if (tree_view->priv->edited_column == NULL)
    return;

  /*
   * This is very evil. We need to do this, because
   * gtk_cell_editable_editing_done may trigger gtk_tree_view_row_changed
   * later on. If gtk_tree_view_row_changed notices
   * tree_view->priv->edited_column != NULL, it'll call
   * gtk_tree_view_stop_editing again. Bad things will happen then.
   *
   * Please read that again if you intend to modify anything here.
   */

  column = tree_view->priv->edited_column;
  gtk_cell_area_stop_editing (gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column)), cancel_editing);
  tree_view->priv->edited_column = NULL;
}


/**
 * gtk_tree_view_set_hover_selection:
 * @tree_view: a #GtkTreeView
 * @hover: %TRUE to enable hover selection mode
 *
 * Enables or disables the hover selection mode of @tree_view.
 * Hover selection makes the selected row follow the pointer.
 * Currently, this works only for the selection modes 
 * %GTK_SELECTION_SINGLE and %GTK_SELECTION_BROWSE.
 **/
void     
gtk_tree_view_set_hover_selection (GtkTreeView *tree_view,
				   gboolean     hover)
{
  hover = hover != FALSE;

  if (hover != tree_view->priv->hover_selection)
    {
      tree_view->priv->hover_selection = hover;

      g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_HOVER_SELECTION]);
    }
}

/**
 * gtk_tree_view_get_hover_selection:
 * @tree_view: a #GtkTreeView
 * 
 * Returns whether hover selection mode is turned on for @tree_view.
 * 
 * Returns: %TRUE if @tree_view is in hover selection mode
 **/
gboolean 
gtk_tree_view_get_hover_selection (GtkTreeView *tree_view)
{
  return tree_view->priv->hover_selection;
}

/**
 * gtk_tree_view_set_hover_expand:
 * @tree_view: a #GtkTreeView
 * @expand: %TRUE to enable hover selection mode
 *
 * Enables or disables the hover expansion mode of @tree_view.
 * Hover expansion makes rows expand or collapse if the pointer 
 * moves over them.
 **/
void     
gtk_tree_view_set_hover_expand (GtkTreeView *tree_view,
				gboolean     expand)
{
  expand = expand != FALSE;

  if (expand != tree_view->priv->hover_expand)
    {
      tree_view->priv->hover_expand = expand;

      g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_HOVER_EXPAND]);
    }
}

/**
 * gtk_tree_view_get_hover_expand:
 * @tree_view: a #GtkTreeView
 * 
 * Returns whether hover expansion mode is turned on for @tree_view.
 * 
 * Returns: %TRUE if @tree_view is in hover expansion mode
 **/
gboolean 
gtk_tree_view_get_hover_expand (GtkTreeView *tree_view)
{
  return tree_view->priv->hover_expand;
}

/**
 * gtk_tree_view_set_rubber_banding:
 * @tree_view: a #GtkTreeView
 * @enable: %TRUE to enable rubber banding
 *
 * Enables or disables rubber banding in @tree_view.  If the selection mode
 * is #GTK_SELECTION_MULTIPLE, rubber banding will allow the user to select
 * multiple rows by dragging the mouse.
 **/
void
gtk_tree_view_set_rubber_banding (GtkTreeView *tree_view,
				  gboolean     enable)
{
  enable = enable != FALSE;

  if (enable != tree_view->priv->rubber_banding_enable)
    {
      tree_view->priv->rubber_banding_enable = enable;

      g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_RUBBER_BANDING]);
    }
}

/**
 * gtk_tree_view_get_rubber_banding:
 * @tree_view: a #GtkTreeView
 * 
 * Returns whether rubber banding is turned on for @tree_view.  If the
 * selection mode is #GTK_SELECTION_MULTIPLE, rubber banding will allow the
 * user to select multiple rows by dragging the mouse.
 * 
 * Returns: %TRUE if rubber banding in @tree_view is enabled.
 **/
gboolean
gtk_tree_view_get_rubber_banding (GtkTreeView *tree_view)
{
  return tree_view->priv->rubber_banding_enable;
}

/**
 * gtk_tree_view_is_rubber_banding_active:
 * @tree_view: a #GtkTreeView
 * 
 * Returns whether a rubber banding operation is currently being done
 * in @tree_view.
 *
 * Returns: %TRUE if a rubber banding operation is currently being
 * done in @tree_view.
 **/
gboolean
gtk_tree_view_is_rubber_banding_active (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  if (tree_view->priv->rubber_banding_enable
      && tree_view->priv->rubber_band_status == RUBBER_BAND_ACTIVE)
    return TRUE;

  return FALSE;
}

/**
 * gtk_tree_view_get_row_separator_func: (skip)
 * @tree_view: a #GtkTreeView
 * 
 * Returns the current row separator function.
 * 
 * Returns: the current row separator function.
 **/
GtkTreeViewRowSeparatorFunc 
gtk_tree_view_get_row_separator_func (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->row_separator_func;
}

/**
 * gtk_tree_view_set_row_separator_func:
 * @tree_view: a #GtkTreeView
 * @func: (allow-none): a #GtkTreeViewRowSeparatorFunc
 * @data: (allow-none): user data to pass to @func, or %NULL
 * @destroy: (allow-none): destroy notifier for @data, or %NULL
 * 
 * Sets the row separator function, which is used to determine
 * whether a row should be drawn as a separator. If the row separator
 * function is %NULL, no separators are drawn. This is the default value.
 **/
void
gtk_tree_view_set_row_separator_func (GtkTreeView                 *tree_view,
				      GtkTreeViewRowSeparatorFunc  func,
				      gpointer                     data,
				      GDestroyNotify               destroy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->row_separator_destroy)
    tree_view->priv->row_separator_destroy (tree_view->priv->row_separator_data);

  tree_view->priv->row_separator_func = func;
  tree_view->priv->row_separator_data = data;
  tree_view->priv->row_separator_destroy = destroy;

  /* Have the tree recalculate heights */
  gtk_tree_rbtree_mark_invalid (tree_view->priv->tree);
  gtk_widget_queue_resize (GTK_WIDGET (tree_view));
}

/**
 * gtk_tree_view_get_grid_lines:
 * @tree_view: a #GtkTreeView
 *
 * Returns which grid lines are enabled in @tree_view.
 *
 * Returns: a #GtkTreeViewGridLines value indicating which grid lines
 * are enabled.
 */
GtkTreeViewGridLines
gtk_tree_view_get_grid_lines (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), 0);

  return tree_view->priv->grid_lines;
}

/**
 * gtk_tree_view_set_grid_lines:
 * @tree_view: a #GtkTreeView
 * @grid_lines: a #GtkTreeViewGridLines value indicating which grid lines to
 * enable.
 *
 * Sets which grid lines to draw in @tree_view.
 */
void
gtk_tree_view_set_grid_lines (GtkTreeView           *tree_view,
			      GtkTreeViewGridLines   grid_lines)
{
  GtkTreeViewPrivate *priv;
  GtkTreeViewGridLines old_grid_lines;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  priv = tree_view->priv;

  old_grid_lines = priv->grid_lines;
  priv->grid_lines = grid_lines;

  if (old_grid_lines != grid_lines)
    {
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
      
      g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_ENABLE_GRID_LINES]);
    }
}

/**
 * gtk_tree_view_get_enable_tree_lines:
 * @tree_view: a #GtkTreeView.
 *
 * Returns whether or not tree lines are drawn in @tree_view.
 *
 * Returns: %TRUE if tree lines are drawn in @tree_view, %FALSE
 * otherwise.
 */
gboolean
gtk_tree_view_get_enable_tree_lines (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return tree_view->priv->tree_lines_enabled;
}

/**
 * gtk_tree_view_set_enable_tree_lines:
 * @tree_view: a #GtkTreeView
 * @enabled: %TRUE to enable tree line drawing, %FALSE otherwise.
 *
 * Sets whether to draw lines interconnecting the expanders in @tree_view.
 * This does not have any visible effects for lists.
 */
void
gtk_tree_view_set_enable_tree_lines (GtkTreeView *tree_view,
				     gboolean     enabled)
{
  GtkTreeViewPrivate *priv;
  gboolean was_enabled;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  enabled = enabled != FALSE;

  priv = tree_view->priv;

  was_enabled = priv->tree_lines_enabled;

  priv->tree_lines_enabled = enabled;

  if (was_enabled != enabled)
    {
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));

      g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_ENABLE_TREE_LINES]);
    }
}


/**
 * gtk_tree_view_set_show_expanders:
 * @tree_view: a #GtkTreeView
 * @enabled: %TRUE to enable expander drawing, %FALSE otherwise.
 *
 * Sets whether to draw and enable expanders and indent child rows in
 * @tree_view.  When disabled there will be no expanders visible in trees
 * and there will be no way to expand and collapse rows by default.  Also
 * note that hiding the expanders will disable the default indentation.  You
 * can set a custom indentation in this case using
 * gtk_tree_view_set_level_indentation().
 * This does not have any visible effects for lists.
 */
void
gtk_tree_view_set_show_expanders (GtkTreeView *tree_view,
				  gboolean     enabled)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  enabled = enabled != FALSE;
  if (tree_view->priv->show_expanders != enabled)
    {
      tree_view->priv->show_expanders = enabled;
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
      g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_SHOW_EXPANDERS]);
    }
}

/**
 * gtk_tree_view_get_show_expanders:
 * @tree_view: a #GtkTreeView.
 *
 * Returns whether or not expanders are drawn in @tree_view.
 *
 * Returns: %TRUE if expanders are drawn in @tree_view, %FALSE
 * otherwise.
 */
gboolean
gtk_tree_view_get_show_expanders (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return tree_view->priv->show_expanders;
}

/**
 * gtk_tree_view_set_level_indentation:
 * @tree_view: a #GtkTreeView
 * @indentation: the amount, in pixels, of extra indentation in @tree_view.
 *
 * Sets the amount of extra indentation for child levels to use in @tree_view
 * in addition to the default indentation.  The value should be specified in
 * pixels, a value of 0 disables this feature and in this case only the default
 * indentation will be used.
 * This does not have any visible effects for lists.
 */
void
gtk_tree_view_set_level_indentation (GtkTreeView *tree_view,
				     gint         indentation)
{
  tree_view->priv->level_indentation = indentation;

  gtk_widget_queue_draw (GTK_WIDGET (tree_view));
}

/**
 * gtk_tree_view_get_level_indentation:
 * @tree_view: a #GtkTreeView.
 *
 * Returns the amount, in pixels, of extra indentation for child levels
 * in @tree_view.
 *
 * Returns: the amount of extra indentation for child levels in
 * @tree_view.  A return value of 0 means that this feature is disabled.
 */
gint
gtk_tree_view_get_level_indentation (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), 0);

  return tree_view->priv->level_indentation;
}

/**
 * gtk_tree_view_set_tooltip_row:
 * @tree_view: a #GtkTreeView
 * @tooltip: a #GtkTooltip
 * @path: a #GtkTreePath
 *
 * Sets the tip area of @tooltip to be the area covered by the row at @path.
 * See also gtk_tree_view_set_tooltip_column() for a simpler alternative.
 * See also gtk_tooltip_set_tip_area().
 */
void
gtk_tree_view_set_tooltip_row (GtkTreeView *tree_view,
			       GtkTooltip  *tooltip,
			       GtkTreePath *path)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_tree_view_set_tooltip_cell (tree_view, tooltip, path, NULL, NULL);
}

/**
 * gtk_tree_view_set_tooltip_cell:
 * @tree_view: a #GtkTreeView
 * @tooltip: a #GtkTooltip
 * @path: (allow-none): a #GtkTreePath or %NULL
 * @column: (allow-none): a #GtkTreeViewColumn or %NULL
 * @cell: (allow-none): a #GtkCellRenderer or %NULL
 *
 * Sets the tip area of @tooltip to the area @path, @column and @cell have
 * in common.  For example if @path is %NULL and @column is set, the tip
 * area will be set to the full area covered by @column.  See also
 * gtk_tooltip_set_tip_area().
 *
 * Note that if @path is not specified and @cell is set and part of a column
 * containing the expander, the tooltip might not show and hide at the correct
 * position.  In such cases @path must be set to the current node under the
 * mouse cursor for this function to operate correctly.
 *
 * See also gtk_tree_view_set_tooltip_column() for a simpler alternative.
 */
void
gtk_tree_view_set_tooltip_cell (GtkTreeView       *tree_view,
				GtkTooltip        *tooltip,
				GtkTreePath       *path,
				GtkTreeViewColumn *column,
				GtkCellRenderer   *cell)
{
  GdkRectangle rect;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));
  g_return_if_fail (column == NULL || GTK_IS_TREE_VIEW_COLUMN (column));
  g_return_if_fail (cell == NULL || GTK_IS_CELL_RENDERER (cell));

  /* Determine x values. */
  if (column && cell)
    {
      GdkRectangle tmp;
      gint start, width;

      /* We always pass in path here, whether it is NULL or not.
       * For cells in expander columns path must be specified so that
       * we can correctly account for the indentation.  This also means
       * that the tooltip is constrained vertically by the "Determine y
       * values" code below; this is not a real problem since cells actually
       * don't stretch vertically in constrast to columns.
       */
      gtk_tree_view_get_cell_area (tree_view, path, column, &tmp);
      gtk_tree_view_column_cell_get_position (column, cell, &start, &width);

      gtk_tree_view_convert_bin_window_to_widget_coords (tree_view,
							 tmp.x + start, 0,
							 &rect.x, NULL);
      rect.width = width;
    }
  else if (column)
    {
      GdkRectangle tmp;

      gtk_tree_view_get_background_area (tree_view, NULL, column, &tmp);
      gtk_tree_view_convert_bin_window_to_widget_coords (tree_view,
							 tmp.x, 0,
							 &rect.x, NULL);
      rect.width = tmp.width;
    }
  else
    {
      rect.x = 0;
      rect.width = gtk_widget_get_width (GTK_WIDGET (tree_view));;
    }

  /* Determine y values. */
  if (path)
    {
      GdkRectangle tmp;

      gtk_tree_view_get_background_area (tree_view, path, NULL, &tmp);
      gtk_tree_view_convert_bin_window_to_widget_coords (tree_view,
							 0, tmp.y,
							 NULL, &rect.y);
      rect.height = tmp.height;
    }
  else
    {
      rect.y = 0;
      rect.height = gtk_adjustment_get_page_size (tree_view->priv->vadjustment);
    }

  gtk_tooltip_set_tip_area (tooltip, &rect);
}

/**
 * gtk_tree_view_get_tooltip_context:
 * @tree_view: a #GtkTreeView
 * @x: (inout): the x coordinate (relative to widget coordinates)
 * @y: (inout): the y coordinate (relative to widget coordinates)
 * @keyboard_tip: whether this is a keyboard tooltip or not
 * @model: (out) (optional) (nullable) (transfer none): a pointer to
 *         receive a #GtkTreeModel or %NULL
 * @path: (out) (optional): a pointer to receive a #GtkTreePath or %NULL
 * @iter: (out) (optional): a pointer to receive a #GtkTreeIter or %NULL
 *
 * This function is supposed to be used in a #GtkWidget::query-tooltip
 * signal handler for #GtkTreeView.  The @x, @y and @keyboard_tip values
 * which are received in the signal handler, should be passed to this
 * function without modification.
 *
 * The return value indicates whether there is a tree view row at the given
 * coordinates (%TRUE) or not (%FALSE) for mouse tooltips.  For keyboard
 * tooltips the row returned will be the cursor row.  When %TRUE, then any of
 * @model, @path and @iter which have been provided will be set to point to
 * that row and the corresponding model.  @x and @y will always be converted
 * to be relative to @tree_view’s bin_window if @keyboard_tooltip is %FALSE.
 *
 * Returns: whether or not the given tooltip context points to a row.
 */
gboolean
gtk_tree_view_get_tooltip_context (GtkTreeView   *tree_view,
				   gint          *x,
				   gint          *y,
				   gboolean       keyboard_tip,
				   GtkTreeModel **model,
				   GtkTreePath  **path,
				   GtkTreeIter   *iter)
{
  GtkTreePath *tmppath = NULL;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (x != NULL, FALSE);
  g_return_val_if_fail (y != NULL, FALSE);

  if (keyboard_tip)
    {
      gtk_tree_view_get_cursor (tree_view, &tmppath, NULL);

      if (!tmppath)
	return FALSE;
    }
  else
    {
      gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, *x, *y,
							  x, y);

      if (!gtk_tree_view_get_path_at_pos (tree_view, *x, *y,
					  &tmppath, NULL, NULL, NULL))
	return FALSE;
    }

  if (model)
    *model = gtk_tree_view_get_model (tree_view);

  if (iter)
    gtk_tree_model_get_iter (gtk_tree_view_get_model (tree_view),
			     iter, tmppath);

  if (path)
    *path = tmppath;
  else
    gtk_tree_path_free (tmppath);

  return TRUE;
}

static gboolean
gtk_tree_view_set_tooltip_query_cb (GtkWidget  *widget,
				    gint        x,
				    gint        y,
				    gboolean    keyboard_tip,
				    GtkTooltip *tooltip,
				    gpointer    data)
{
  GValue value = G_VALUE_INIT;
  GValue transformed = G_VALUE_INIT;
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

  if (!gtk_tree_view_get_tooltip_context (GTK_TREE_VIEW (widget),
					  &x, &y,
					  keyboard_tip,
					  &model, &path, &iter))
    return FALSE;

  gtk_tree_model_get_value (model, &iter,
                            tree_view->priv->tooltip_column, &value);

  g_value_init (&transformed, G_TYPE_STRING);

  if (!g_value_transform (&value, &transformed))
    {
      g_value_unset (&value);
      gtk_tree_path_free (path);

      return FALSE;
    }

  g_value_unset (&value);

  if (!g_value_get_string (&transformed))
    {
      g_value_unset (&transformed);
      gtk_tree_path_free (path);

      return FALSE;
    }

  gtk_tooltip_set_markup (tooltip, g_value_get_string (&transformed));
  gtk_tree_view_set_tooltip_row (tree_view, tooltip, path);

  gtk_tree_path_free (path);
  g_value_unset (&transformed);

  return TRUE;
}

/**
 * gtk_tree_view_set_tooltip_column:
 * @tree_view: a #GtkTreeView
 * @column: an integer, which is a valid column number for @tree_view’s model
 *
 * If you only plan to have simple (text-only) tooltips on full rows, you
 * can use this function to have #GtkTreeView handle these automatically
 * for you. @column should be set to the column in @tree_view’s model
 * containing the tooltip texts, or -1 to disable this feature.
 *
 * When enabled, #GtkWidget:has-tooltip will be set to %TRUE and
 * @tree_view will connect a #GtkWidget::query-tooltip signal handler.
 *
 * Note that the signal handler sets the text with gtk_tooltip_set_markup(),
 * so &, <, etc have to be escaped in the text.
 */
void
gtk_tree_view_set_tooltip_column (GtkTreeView *tree_view,
			          gint         column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (column == tree_view->priv->tooltip_column)
    return;

  if (column == -1)
    {
      g_signal_handlers_disconnect_by_func (tree_view,
	  				    gtk_tree_view_set_tooltip_query_cb,
					    NULL);
      gtk_widget_set_has_tooltip (GTK_WIDGET (tree_view), FALSE);
    }
  else
    {
      if (tree_view->priv->tooltip_column == -1)
        {
          g_signal_connect (tree_view, "query-tooltip",
		            G_CALLBACK (gtk_tree_view_set_tooltip_query_cb), NULL);
          gtk_widget_set_has_tooltip (GTK_WIDGET (tree_view), TRUE);
        }
    }

  tree_view->priv->tooltip_column = column;
  g_object_notify_by_pspec (G_OBJECT (tree_view), tree_view_props[PROP_TOOLTIP_COLUMN]);
}

/**
 * gtk_tree_view_get_tooltip_column:
 * @tree_view: a #GtkTreeView
 *
 * Returns the column of @tree_view’s model which is being used for
 * displaying tooltips on @tree_view’s rows.
 *
 * Returns: the index of the tooltip column that is currently being
 * used, or -1 if this is disabled.
 */
gint
gtk_tree_view_get_tooltip_column (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), 0);

  return tree_view->priv->tooltip_column;
}

static gboolean
gtk_tree_view_get_border (GtkScrollable *scrollable,
                          GtkBorder     *border)
{
  border->top = gtk_tree_view_get_effective_header_height (GTK_TREE_VIEW (scrollable));

  return TRUE;
}

static void
gtk_tree_view_scrollable_init (GtkScrollableInterface *iface)
{
  iface->get_border = gtk_tree_view_get_border;
}
