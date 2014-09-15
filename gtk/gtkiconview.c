/* gtkiconview.c
 * Copyright (C) 2002, 2004  Anders Carlsson <andersca@gnu.org>
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

#include <string.h>

#include "gtkiconview.h"
#include "gtkiconviewprivate.h"

#include "gtkadjustmentprivate.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderer.h"
#include "gtkcellareabox.h"
#include "gtkcellareacontext.h"
#include "gtkcellrenderertext.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkorientable.h"
#include "gtkmarshalers.h"
#include "gtkbindings.h"
#include "gtkdnd.h"
#include "gtkmain.h"
#include "gtkintl.h"
#include "gtkaccessible.h"
#include "gtkwindow.h"
#include "gtkentry.h"
#include "gtkcombobox.h"
#include "gtkscrollable.h"
#include "gtksizerequest.h"
#include "gtktreednd.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "a11y/gtkiconviewaccessibleprivate.h"

/**
 * SECTION:gtkiconview
 * @title: GtkIconView
 * @short_description: A widget which displays a list of icons in a grid
 *
 * #GtkIconView provides an alternative view on a #GtkTreeModel.
 * It displays the model as a grid of icons with labels. Like
 * #GtkTreeView, it allows to select one or multiple items
 * (depending on the selection mode, see gtk_icon_view_set_selection_mode()).
 * In addition to selection with the arrow keys, #GtkIconView supports
 * rubberband selection, which is controlled by dragging the pointer.
 *
 * Note that if the tree model is backed by an actual tree store (as
 * opposed to a flat list where the mapping to icons is obvious),
 * #GtkIconView will only display the first level of the tree and
 * ignore the tree’s branches.
 */

#define SCROLL_EDGE_SIZE 15

typedef struct _GtkIconViewChild GtkIconViewChild;
struct _GtkIconViewChild
{
  GtkWidget    *widget;
  GdkRectangle  area;
};

/* Signals */
enum
{
  ITEM_ACTIVATED,
  SELECTION_CHANGED,
  SELECT_ALL,
  UNSELECT_ALL,
  SELECT_CURSOR_ITEM,
  TOGGLE_CURSOR_ITEM,
  MOVE_CURSOR,
  ACTIVATE_CURSOR_ITEM,
  LAST_SIGNAL
};

/* Properties */
enum
{
  PROP_0,
  PROP_PIXBUF_COLUMN,
  PROP_TEXT_COLUMN,
  PROP_MARKUP_COLUMN,
  PROP_SELECTION_MODE,
  PROP_ITEM_ORIENTATION,
  PROP_MODEL,
  PROP_COLUMNS,
  PROP_ITEM_WIDTH,
  PROP_SPACING,
  PROP_ROW_SPACING,
  PROP_COLUMN_SPACING,
  PROP_MARGIN,
  PROP_REORDERABLE,
  PROP_TOOLTIP_COLUMN,
  PROP_ITEM_PADDING,
  PROP_CELL_AREA,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
  PROP_ACTIVATE_ON_SINGLE_CLICK
};

/* GObject vfuncs */
static void             gtk_icon_view_cell_layout_init          (GtkCellLayoutIface *iface);
static void             gtk_icon_view_dispose                   (GObject            *object);
static void             gtk_icon_view_constructed               (GObject            *object);
static void             gtk_icon_view_set_property              (GObject            *object,
								 guint               prop_id,
								 const GValue       *value,
								 GParamSpec         *pspec);
static void             gtk_icon_view_get_property              (GObject            *object,
								 guint               prop_id,
								 GValue             *value,
								 GParamSpec         *pspec);
/* GtkWidget vfuncs */
static void             gtk_icon_view_destroy                   (GtkWidget          *widget);
static void             gtk_icon_view_realize                   (GtkWidget          *widget);
static void             gtk_icon_view_unrealize                 (GtkWidget          *widget);
static void             gtk_icon_view_style_updated             (GtkWidget          *widget);
static void             gtk_icon_view_state_flags_changed       (GtkWidget          *widget,
			                                         GtkStateFlags       previous_state);
static GtkSizeRequestMode gtk_icon_view_get_request_mode        (GtkWidget          *widget);
static void             gtk_icon_view_get_preferred_width       (GtkWidget          *widget,
								 gint               *minimum,
								 gint               *natural);
static void             gtk_icon_view_get_preferred_width_for_height
                                                                (GtkWidget          *widget,
                                                                 gint                height,
								 gint               *minimum,
								 gint               *natural);
static void             gtk_icon_view_get_preferred_height      (GtkWidget          *widget,
								 gint               *minimum,
								 gint               *natural);
static void             gtk_icon_view_get_preferred_height_for_width
                                                                (GtkWidget          *widget,
                                                                 gint                width,
								 gint               *minimum,
								 gint               *natural);
static void             gtk_icon_view_size_allocate             (GtkWidget          *widget,
								 GtkAllocation      *allocation);
static gboolean         gtk_icon_view_draw                      (GtkWidget          *widget,
                                                                 cairo_t            *cr);
static gboolean         gtk_icon_view_motion                    (GtkWidget          *widget,
								 GdkEventMotion     *event);
static gboolean         gtk_icon_view_button_press              (GtkWidget          *widget,
								 GdkEventButton     *event);
static gboolean         gtk_icon_view_button_release            (GtkWidget          *widget,
								 GdkEventButton     *event);
static gboolean         gtk_icon_view_key_press                 (GtkWidget          *widget,
								 GdkEventKey        *event);
static gboolean         gtk_icon_view_key_release               (GtkWidget          *widget,
								 GdkEventKey        *event);


/* GtkContainer vfuncs */
static void             gtk_icon_view_remove                    (GtkContainer       *container,
								 GtkWidget          *widget);
static void             gtk_icon_view_forall                    (GtkContainer       *container,
								 gboolean            include_internals,
								 GtkCallback         callback,
								 gpointer            callback_data);

/* GtkIconView vfuncs */
static void             gtk_icon_view_real_select_all           (GtkIconView        *icon_view);
static void             gtk_icon_view_real_unselect_all         (GtkIconView        *icon_view);
static void             gtk_icon_view_real_select_cursor_item   (GtkIconView        *icon_view);
static void             gtk_icon_view_real_toggle_cursor_item   (GtkIconView        *icon_view);
static gboolean         gtk_icon_view_real_activate_cursor_item (GtkIconView        *icon_view);

 /* Internal functions */
static void                 gtk_icon_view_set_hadjustment_values         (GtkIconView            *icon_view);
static void                 gtk_icon_view_set_vadjustment_values         (GtkIconView            *icon_view);
static void                 gtk_icon_view_set_hadjustment                (GtkIconView            *icon_view,
                                                                          GtkAdjustment          *adjustment);
static void                 gtk_icon_view_set_vadjustment                (GtkIconView            *icon_view,
                                                                          GtkAdjustment          *adjustment);
static void                 gtk_icon_view_adjustment_changed             (GtkAdjustment          *adjustment,
									  GtkIconView            *icon_view);
static void                 gtk_icon_view_layout                         (GtkIconView            *icon_view);
static void                 gtk_icon_view_paint_item                     (GtkIconView            *icon_view,
									  cairo_t                *cr,
									  GtkIconViewItem        *item,
									  gint                    x,
									  gint                    y,
									  gboolean                draw_focus);
static void                 gtk_icon_view_paint_rubberband               (GtkIconView            *icon_view,
								          cairo_t                *cr);
static void                 gtk_icon_view_queue_draw_path                (GtkIconView *icon_view,
									  GtkTreePath *path);
static void                 gtk_icon_view_queue_draw_item                (GtkIconView            *icon_view,
									  GtkIconViewItem        *item);
static void                 gtk_icon_view_start_rubberbanding            (GtkIconView            *icon_view,
                                                                          GdkDevice              *device,
									  gint                    x,
									  gint                    y);
static void                 gtk_icon_view_stop_rubberbanding             (GtkIconView            *icon_view);
static void                 gtk_icon_view_update_rubberband_selection    (GtkIconView            *icon_view);
static gboolean             gtk_icon_view_item_hit_test                  (GtkIconView            *icon_view,
									  GtkIconViewItem        *item,
									  gint                    x,
									  gint                    y,
									  gint                    width,
									  gint                    height);
static gboolean             gtk_icon_view_unselect_all_internal          (GtkIconView            *icon_view);
static void                 gtk_icon_view_update_rubberband              (gpointer                data);
static void                 gtk_icon_view_item_invalidate_size           (GtkIconViewItem        *item);
static void                 gtk_icon_view_invalidate_sizes               (GtkIconView            *icon_view);
static void                 gtk_icon_view_add_move_binding               (GtkBindingSet          *binding_set,
									  guint                   keyval,
									  guint                   modmask,
									  GtkMovementStep         step,
									  gint                    count);
static gboolean             gtk_icon_view_real_move_cursor               (GtkIconView            *icon_view,
									  GtkMovementStep         step,
									  gint                    count);
static void                 gtk_icon_view_move_cursor_up_down            (GtkIconView            *icon_view,
									  gint                    count);
static void                 gtk_icon_view_move_cursor_page_up_down       (GtkIconView            *icon_view,
									  gint                    count);
static void                 gtk_icon_view_move_cursor_left_right         (GtkIconView            *icon_view,
									  gint                    count);
static void                 gtk_icon_view_move_cursor_start_end          (GtkIconView            *icon_view,
									  gint                    count);
static void                 gtk_icon_view_scroll_to_item                 (GtkIconView            *icon_view,
									  GtkIconViewItem        *item);
static gboolean             gtk_icon_view_select_all_between             (GtkIconView            *icon_view,
									  GtkIconViewItem        *anchor,
									  GtkIconViewItem        *cursor);

static void                 gtk_icon_view_ensure_cell_area               (GtkIconView            *icon_view,
                                                                          GtkCellArea            *cell_area);

static GtkCellArea         *gtk_icon_view_cell_layout_get_area           (GtkCellLayout          *layout);

static void                 gtk_icon_view_item_selected_changed          (GtkIconView            *icon_view,
		                                                          GtkIconViewItem        *item);

static void                 gtk_icon_view_add_editable                   (GtkCellArea            *area,
									  GtkCellRenderer        *renderer,
									  GtkCellEditable        *editable,
									  GdkRectangle           *cell_area,
									  const gchar            *path,
									  GtkIconView            *icon_view);
static void                 gtk_icon_view_remove_editable                (GtkCellArea            *area,
									  GtkCellRenderer        *renderer,
									  GtkCellEditable        *editable,
									  GtkIconView            *icon_view);
static void                 update_text_cell                             (GtkIconView            *icon_view);
static void                 update_pixbuf_cell                           (GtkIconView            *icon_view);

/* Source side drag signals */
static void gtk_icon_view_drag_begin       (GtkWidget        *widget,
                                            GdkDragContext   *context);
static void gtk_icon_view_drag_end         (GtkWidget        *widget,
                                            GdkDragContext   *context);
static void gtk_icon_view_drag_data_get    (GtkWidget        *widget,
                                            GdkDragContext   *context,
                                            GtkSelectionData *selection_data,
                                            guint             info,
                                            guint             time);
static void gtk_icon_view_drag_data_delete (GtkWidget        *widget,
                                            GdkDragContext   *context);

/* Target side drag signals */
static void     gtk_icon_view_drag_leave         (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  guint             time);
static gboolean gtk_icon_view_drag_motion        (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  guint             time);
static gboolean gtk_icon_view_drag_drop          (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  guint             time);
static void     gtk_icon_view_drag_data_received (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  GtkSelectionData *selection_data,
                                                  guint             info,
                                                  guint             time);
static gboolean gtk_icon_view_maybe_begin_drag   (GtkIconView             *icon_view,
					   	  GdkEventMotion          *event);

static void     remove_scroll_timeout            (GtkIconView *icon_view);

/* GtkBuildable */
static GtkBuildableIface *parent_buildable_iface;
static void     gtk_icon_view_buildable_init             (GtkBuildableIface *iface);
static gboolean gtk_icon_view_buildable_custom_tag_start (GtkBuildable  *buildable,
							  GtkBuilder    *builder,
							  GObject       *child,
							  const gchar   *tagname,
							  GMarkupParser *parser,
							  gpointer      *data);
static void     gtk_icon_view_buildable_custom_tag_end   (GtkBuildable  *buildable,
							  GtkBuilder    *builder,
							  GObject       *child,
							  const gchar   *tagname,
							  gpointer      *data);

static guint icon_view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkIconView, gtk_icon_view, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkIconView)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
						gtk_icon_view_cell_layout_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_icon_view_buildable_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static void
gtk_icon_view_class_init (GtkIconViewClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkBindingSet *binding_set;
  
  binding_set = gtk_binding_set_by_class (klass);

  gobject_class = (GObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  container_class = (GtkContainerClass *) klass;

  gobject_class->constructed = gtk_icon_view_constructed;
  gobject_class->dispose = gtk_icon_view_dispose;
  gobject_class->set_property = gtk_icon_view_set_property;
  gobject_class->get_property = gtk_icon_view_get_property;

  widget_class->destroy = gtk_icon_view_destroy;
  widget_class->realize = gtk_icon_view_realize;
  widget_class->unrealize = gtk_icon_view_unrealize;
  widget_class->style_updated = gtk_icon_view_style_updated;
  widget_class->get_request_mode = gtk_icon_view_get_request_mode;
  widget_class->get_preferred_width = gtk_icon_view_get_preferred_width;
  widget_class->get_preferred_height = gtk_icon_view_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_icon_view_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_icon_view_get_preferred_height_for_width;
  widget_class->size_allocate = gtk_icon_view_size_allocate;
  widget_class->draw = gtk_icon_view_draw;
  widget_class->motion_notify_event = gtk_icon_view_motion;
  widget_class->button_press_event = gtk_icon_view_button_press;
  widget_class->button_release_event = gtk_icon_view_button_release;
  widget_class->key_press_event = gtk_icon_view_key_press;
  widget_class->key_release_event = gtk_icon_view_key_release;
  widget_class->drag_begin = gtk_icon_view_drag_begin;
  widget_class->drag_end = gtk_icon_view_drag_end;
  widget_class->drag_data_get = gtk_icon_view_drag_data_get;
  widget_class->drag_data_delete = gtk_icon_view_drag_data_delete;
  widget_class->drag_leave = gtk_icon_view_drag_leave;
  widget_class->drag_motion = gtk_icon_view_drag_motion;
  widget_class->drag_drop = gtk_icon_view_drag_drop;
  widget_class->drag_data_received = gtk_icon_view_drag_data_received;
  widget_class->state_flags_changed = gtk_icon_view_state_flags_changed;

  container_class->remove = gtk_icon_view_remove;
  container_class->forall = gtk_icon_view_forall;

  klass->select_all = gtk_icon_view_real_select_all;
  klass->unselect_all = gtk_icon_view_real_unselect_all;
  klass->select_cursor_item = gtk_icon_view_real_select_cursor_item;
  klass->toggle_cursor_item = gtk_icon_view_real_toggle_cursor_item;
  klass->activate_cursor_item = gtk_icon_view_real_activate_cursor_item;  
  klass->move_cursor = gtk_icon_view_real_move_cursor;
  
  /* Properties */
  /**
   * GtkIconView:selection-mode:
   * 
   * The ::selection-mode property specifies the selection mode of
   * icon view. If the mode is #GTK_SELECTION_MULTIPLE, rubberband selection
   * is enabled, for the other modes, only keyboard selection is possible.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_SELECTION_MODE,
				   g_param_spec_enum ("selection-mode",
						      P_("Selection mode"),
						      P_("The selection mode"),
						      GTK_TYPE_SELECTION_MODE,
						      GTK_SELECTION_SINGLE,
						      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkIconView:pixbuf-column:
   *
   * The ::pixbuf-column property contains the number of the model column
   * containing the pixbufs which are displayed. The pixbuf column must be 
   * of type #GDK_TYPE_PIXBUF. Setting this property to -1 turns off the
   * display of pixbufs.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_PIXBUF_COLUMN,
				   g_param_spec_int ("pixbuf-column",
						     P_("Pixbuf column"),
						     P_("Model column used to retrieve the icon pixbuf from"),
						     -1, G_MAXINT, -1,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkIconView:text-column:
   *
   * The ::text-column property contains the number of the model column
   * containing the texts which are displayed. The text column must be 
   * of type #G_TYPE_STRING. If this property and the :markup-column 
   * property are both set to -1, no texts are displayed.   
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_TEXT_COLUMN,
				   g_param_spec_int ("text-column",
						     P_("Text column"),
						     P_("Model column used to retrieve the text from"),
						     -1, G_MAXINT, -1,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  
  /**
   * GtkIconView:markup-column:
   *
   * The ::markup-column property contains the number of the model column
   * containing markup information to be displayed. The markup column must be 
   * of type #G_TYPE_STRING. If this property and the :text-column property 
   * are both set to column numbers, it overrides the text column.
   * If both are set to -1, no texts are displayed.   
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_MARKUP_COLUMN,
				   g_param_spec_int ("markup-column",
						     P_("Markup column"),
						     P_("Model column used to retrieve the text if using Pango markup"),
						     -1, G_MAXINT, -1,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  g_object_class_install_property (gobject_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
							P_("Icon View Model"),
							P_("The model for the icon view"),
							GTK_TYPE_TREE_MODEL,
							GTK_PARAM_READWRITE));
  
  /**
   * GtkIconView:columns:
   *
   * The columns property contains the number of the columns in which the
   * items should be displayed. If it is -1, the number of columns will
   * be chosen automatically to fill the available area.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_COLUMNS,
				   g_param_spec_int ("columns",
						     P_("Number of columns"),
						     P_("Number of columns to display"),
						     -1, G_MAXINT, -1,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  

  /**
   * GtkIconView:item-width:
   *
   * The item-width property specifies the width to use for each item. 
   * If it is set to -1, the icon view will automatically determine a 
   * suitable item size.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_ITEM_WIDTH,
				   g_param_spec_int ("item-width",
						     P_("Width for each item"),
						     P_("The width used for each item"),
						     -1, G_MAXINT, -1,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkIconView:spacing:
   *
   * The spacing property specifies the space which is inserted between
   * the cells (i.e. the icon and the text) of an item.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SPACING,
                                   g_param_spec_int ("spacing",
						     P_("Spacing"),
						     P_("Space which is inserted between cells of an item"),
						     0, G_MAXINT, 0,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkIconView:row-spacing:
   *
   * The row-spacing property specifies the space which is inserted between
   * the rows of the icon view.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ROW_SPACING,
                                   g_param_spec_int ("row-spacing",
						     P_("Row Spacing"),
						     P_("Space which is inserted between grid rows"),
						     0, G_MAXINT, 6,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkIconView:column-spacing:
   *
   * The column-spacing property specifies the space which is inserted between
   * the columns of the icon view.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_COLUMN_SPACING,
                                   g_param_spec_int ("column-spacing",
						     P_("Column Spacing"),
						     P_("Space which is inserted between grid columns"),
						     0, G_MAXINT, 6,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkIconView:margin:
   *
   * The margin property specifies the space which is inserted 
   * at the edges of the icon view.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN,
                                   g_param_spec_int ("margin",
						     P_("Margin"),
						     P_("Space which is inserted at the edges of the icon view"),
						     0, G_MAXINT, 6,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkIconView:item-orientation:
   *
   * The item-orientation property specifies how the cells (i.e. the icon and
   * the text) of the item are positioned relative to each other.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_ITEM_ORIENTATION,
				   g_param_spec_enum ("item-orientation",
						      P_("Item Orientation"),
						      P_("How the text and icon of each item are positioned relative to each other"),
						      GTK_TYPE_ORIENTATION,
						      GTK_ORIENTATION_VERTICAL,
						      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkIconView:reorderable:
   *
   * The reorderable property specifies if the items can be reordered
   * by DND.
   *
   * Since: 2.8
   */
  g_object_class_install_property (gobject_class,
                                   PROP_REORDERABLE,
                                   g_param_spec_boolean ("reorderable",
							 P_("Reorderable"),
							 P_("View is reorderable"),
							 FALSE,
							 G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

    g_object_class_install_property (gobject_class,
                                     PROP_TOOLTIP_COLUMN,
                                     g_param_spec_int ("tooltip-column",
                                                       P_("Tooltip Column"),
                                                       P_("The column in the model containing the tooltip texts for the items"),
                                                       -1,
                                                       G_MAXINT,
                                                       -1,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkIconView:item-padding:
   *
   * The item-padding property specifies the padding around each
   * of the icon view's item.
   *
   * Since: 2.18
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ITEM_PADDING,
                                   g_param_spec_int ("item-padding",
						     P_("Item Padding"),
						     P_("Padding around icon view items"),
						     0, G_MAXINT, 6,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkIconView:cell-area:
   *
   * The #GtkCellArea used to layout cell renderers for this view.
   *
   * If no area is specified when creating the icon view with gtk_icon_view_new_with_area() 
   * a #GtkCellAreaBox will be used.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
				   PROP_CELL_AREA,
				   g_param_spec_object ("cell-area",
							P_("Cell Area"),
							P_("The GtkCellArea used to layout cells"),
							GTK_TYPE_CELL_AREA,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtkIconView:activate-on-single-click:
   *
   * The activate-on-single-click property specifies whether the "item-activated" signal
   * will be emitted after a single click.
   *
   * Since: 3.8
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVATE_ON_SINGLE_CLICK,
                                   g_param_spec_boolean ("activate-on-single-click",
							 P_("Activate on Single Click"),
							 P_("Activate row on a single click"),
							 FALSE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /* Scrollable interface properties */
  g_object_class_override_property (gobject_class, PROP_HADJUSTMENT,    "hadjustment");
  g_object_class_override_property (gobject_class, PROP_VADJUSTMENT,    "vadjustment");
  g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  /* Style properties */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boxed ("selection-box-color",
                                                               P_("Selection Box Color"),
                                                               P_("Color of the selection box"),
                                                               g_type_from_name ("GdkColor"),
                                                               GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_uchar ("selection-box-alpha",
                                                               P_("Selection Box Alpha"),
                                                               P_("Opacity of the selection box"),
                                                               0, 0xff,
                                                               0x40,
                                                               GTK_PARAM_READABLE));

  /* Signals */
  /**
   * GtkIconView::item-activated:
   * @iconview: the object on which the signal is emitted
   * @path: the #GtkTreePath for the activated item
   *
   * The ::item-activated signal is emitted when the method
   * gtk_icon_view_item_activated() is called, when the user double
   * clicks an item with the "activate-on-single-click" property set
   * to %FALSE, or when the user single clicks an item when the
   * "activate-on-single-click" property set to %TRUE. It is also
   * emitted when a non-editable item is selected and one of the keys:
   * Space, Return or Enter is pressed.
   */
  icon_view_signals[ITEM_ACTIVATED] =
    g_signal_new (I_("item-activated"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkIconViewClass, item_activated),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_TREE_PATH);

  /**
   * GtkIconView::selection-changed:
   * @iconview: the object on which the signal is emitted
   *
   * The ::selection-changed signal is emitted when the selection
   * (i.e. the set of selected items) changes.
   */
  icon_view_signals[SELECTION_CHANGED] =
    g_signal_new (I_("selection-changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkIconViewClass, selection_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  /**
   * GtkIconView::select-all:
   * @iconview: the object on which the signal is emitted
   *
   * A [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user selects all items.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control selection
   * programmatically.
   * 
   * The default binding for this signal is Ctrl-a.
   */
  icon_view_signals[SELECT_ALL] =
    g_signal_new (I_("select-all"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkIconViewClass, select_all),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  /**
   * GtkIconView::unselect-all:
   * @iconview: the object on which the signal is emitted
   *
   * A [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user unselects all items.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control selection
   * programmatically.
   * 
   * The default binding for this signal is Ctrl-Shift-a. 
   */
  icon_view_signals[UNSELECT_ALL] =
    g_signal_new (I_("unselect-all"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkIconViewClass, unselect_all),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkIconView::select-cursor-item:
   * @iconview: the object on which the signal is emitted
   *
   * A [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user selects the item that is currently
   * focused.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control selection
   * programmatically.
   * 
   * There is no default binding for this signal.
   */
  icon_view_signals[SELECT_CURSOR_ITEM] =
    g_signal_new (I_("select-cursor-item"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkIconViewClass, select_cursor_item),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkIconView::toggle-cursor-item:
   * @iconview: the object on which the signal is emitted
   *
   * A [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user toggles whether the currently
   * focused item is selected or not. The exact effect of this 
   * depend on the selection mode.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control selection
   * programmatically.
   * 
   * There is no default binding for this signal is Ctrl-Space.
   */
  icon_view_signals[TOGGLE_CURSOR_ITEM] =
    g_signal_new (I_("toggle-cursor-item"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkIconViewClass, toggle_cursor_item),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkIconView::activate-cursor-item:
   * @iconview: the object on which the signal is emitted
   *
   * A [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user activates the currently 
   * focused item. 
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control activation
   * programmatically.
   * 
   * The default bindings for this signal are Space, Return and Enter.
   */
  icon_view_signals[ACTIVATE_CURSOR_ITEM] =
    g_signal_new (I_("activate-cursor-item"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkIconViewClass, activate_cursor_item),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);
  
  /**
   * GtkIconView::move-cursor:
   * @iconview: the object which received the signal
   * @step: the granularity of the move, as a #GtkMovementStep
   * @count: the number of @step units to move
   *
   * The ::move-cursor signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user initiates a cursor movement.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control the cursor
   * programmatically.
   *
   * The default bindings for this signal include
   * - Arrow keys which move by individual steps
   * - Home/End keys which move to the first/last item
   * - PageUp/PageDown which move by "pages"
   * All of these will extend the selection when combined with
   * the Shift modifier.
   */
  icon_view_signals[MOVE_CURSOR] =
    g_signal_new (I_("move-cursor"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkIconViewClass, move_cursor),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__ENUM_INT,
		  G_TYPE_BOOLEAN, 2,
		  GTK_TYPE_MOVEMENT_STEP,
		  G_TYPE_INT);

  /* Key bindings */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK, 
				"select-all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK | GDK_SHIFT_MASK, 
				"unselect-all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, GDK_CONTROL_MASK, 
				"toggle-cursor-item", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, GDK_CONTROL_MASK,
				"toggle-cursor-item", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, 0, 
				"activate-cursor-item", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, 0,
				"activate-cursor-item", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, 0, 
				"activate-cursor-item", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_ISO_Enter, 0, 
				"activate-cursor-item", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter, 0, 
				"activate-cursor-item", 0);

  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_Up, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);
  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_KP_Up, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);

  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_Down, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);
  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_KP_Down, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);

  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_p, GDK_CONTROL_MASK,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);

  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_n, GDK_CONTROL_MASK,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);

  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_Home, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, -1);
  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_KP_Home, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, -1);

  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_End, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, 1);
  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_KP_End, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, 1);

  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_Page_Up, 0,
				  GTK_MOVEMENT_PAGES, -1);
  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_KP_Page_Up, 0,
				  GTK_MOVEMENT_PAGES, -1);

  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_Page_Down, 0,
				  GTK_MOVEMENT_PAGES, 1);
  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_KP_Page_Down, 0,
				  GTK_MOVEMENT_PAGES, 1);

  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_Right, 0, 
				  GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_Left, 0, 
				  GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_KP_Right, 0, 
				  GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  gtk_icon_view_add_move_binding (binding_set, GDK_KEY_KP_Left, 0, 
				  GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_ICON_VIEW_ACCESSIBLE);
}

static void
gtk_icon_view_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = _gtk_cell_layout_buildable_add_child;
  iface->custom_tag_start = gtk_icon_view_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_icon_view_buildable_custom_tag_end;
}

static void
gtk_icon_view_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->get_area = gtk_icon_view_cell_layout_get_area;
}

static void
gtk_icon_view_init (GtkIconView *icon_view)
{
  icon_view->priv = gtk_icon_view_get_instance_private (icon_view);

  icon_view->priv->width = 0;
  icon_view->priv->height = 0;
  icon_view->priv->selection_mode = GTK_SELECTION_SINGLE;
  icon_view->priv->pressed_button = -1;
  icon_view->priv->press_start_x = -1;
  icon_view->priv->press_start_y = -1;
  icon_view->priv->text_column = -1;
  icon_view->priv->markup_column = -1;  
  icon_view->priv->pixbuf_column = -1;
  icon_view->priv->text_cell = NULL;
  icon_view->priv->pixbuf_cell = NULL;  
  icon_view->priv->tooltip_column = -1;  

  gtk_widget_set_can_focus (GTK_WIDGET (icon_view), TRUE);

  icon_view->priv->item_orientation = GTK_ORIENTATION_VERTICAL;

  icon_view->priv->columns = -1;
  icon_view->priv->item_width = -1;
  icon_view->priv->spacing = 0;
  icon_view->priv->row_spacing = 6;
  icon_view->priv->column_spacing = 6;
  icon_view->priv->margin = 6;
  icon_view->priv->item_padding = 6;
  icon_view->priv->activate_on_single_click = FALSE;

  icon_view->priv->draw_focus = TRUE;

  icon_view->priv->row_contexts = 
    g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (icon_view)),
                               GTK_STYLE_CLASS_VIEW);
}

/* GObject methods */

static void
gtk_icon_view_constructed (GObject *object)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (object);

  G_OBJECT_CLASS (gtk_icon_view_parent_class)->constructed (object);

  gtk_icon_view_ensure_cell_area (icon_view, NULL);
}

static void
gtk_icon_view_dispose (GObject *object)
{
  GtkIconView *icon_view;
  GtkIconViewPrivate *priv;

  icon_view = GTK_ICON_VIEW (object);
  priv      = icon_view->priv;

  if (priv->cell_area_context)
    {
      g_object_unref (priv->cell_area_context);
      priv->cell_area_context = NULL;
    }

  if (priv->row_contexts)
    {
      g_ptr_array_free (priv->row_contexts, TRUE);
      priv->row_contexts = NULL;
    }

  if (priv->cell_area)
    {
      gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

      g_signal_handler_disconnect (priv->cell_area, priv->add_editable_id);
      g_signal_handler_disconnect (priv->cell_area, priv->remove_editable_id);
      priv->add_editable_id = 0;
      priv->remove_editable_id = 0;

      g_object_unref (priv->cell_area);
      priv->cell_area = NULL;
    }

  G_OBJECT_CLASS (gtk_icon_view_parent_class)->dispose (object);
}

static void
gtk_icon_view_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  GtkIconView *icon_view;
  GtkCellArea *area;

  icon_view = GTK_ICON_VIEW (object);

  switch (prop_id)
    {
    case PROP_SELECTION_MODE:
      gtk_icon_view_set_selection_mode (icon_view, g_value_get_enum (value));
      break;
    case PROP_PIXBUF_COLUMN:
      gtk_icon_view_set_pixbuf_column (icon_view, g_value_get_int (value));
      break;
    case PROP_TEXT_COLUMN:
      gtk_icon_view_set_text_column (icon_view, g_value_get_int (value));
      break;
    case PROP_MARKUP_COLUMN:
      gtk_icon_view_set_markup_column (icon_view, g_value_get_int (value));
      break;
    case PROP_MODEL:
      gtk_icon_view_set_model (icon_view, g_value_get_object (value));
      break;
    case PROP_ITEM_ORIENTATION:
      gtk_icon_view_set_item_orientation (icon_view, g_value_get_enum (value));
      break;
    case PROP_COLUMNS:
      gtk_icon_view_set_columns (icon_view, g_value_get_int (value));
      break;
    case PROP_ITEM_WIDTH:
      gtk_icon_view_set_item_width (icon_view, g_value_get_int (value));
      break;
    case PROP_SPACING:
      gtk_icon_view_set_spacing (icon_view, g_value_get_int (value));
      break;
    case PROP_ROW_SPACING:
      gtk_icon_view_set_row_spacing (icon_view, g_value_get_int (value));
      break;
    case PROP_COLUMN_SPACING:
      gtk_icon_view_set_column_spacing (icon_view, g_value_get_int (value));
      break;
    case PROP_MARGIN:
      gtk_icon_view_set_margin (icon_view, g_value_get_int (value));
      break;
    case PROP_REORDERABLE:
      gtk_icon_view_set_reorderable (icon_view, g_value_get_boolean (value));
      break;
      
    case PROP_TOOLTIP_COLUMN:
      gtk_icon_view_set_tooltip_column (icon_view, g_value_get_int (value));
      break;

    case PROP_ITEM_PADDING:
      gtk_icon_view_set_item_padding (icon_view, g_value_get_int (value));
      break;

    case PROP_ACTIVATE_ON_SINGLE_CLICK:
      gtk_icon_view_set_activate_on_single_click (icon_view, g_value_get_boolean (value));
      break;

    case PROP_CELL_AREA:
      /* Construct-only, can only be assigned once */
      area = g_value_get_object (value);
      if (area)
        {
          if (icon_view->priv->cell_area != NULL)
            {
              g_warning ("cell-area has already been set, ignoring construct property");
              g_object_ref_sink (area);
              g_object_unref (area);
            }
          else
            gtk_icon_view_ensure_cell_area (icon_view, area);
        }
      break;

    case PROP_HADJUSTMENT:
      gtk_icon_view_set_hadjustment (icon_view, g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      gtk_icon_view_set_vadjustment (icon_view, g_value_get_object (value));
      break;
    case PROP_HSCROLL_POLICY:
      if (icon_view->priv->hscroll_policy != g_value_get_enum (value))
        {
          icon_view->priv->hscroll_policy = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (icon_view));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_VSCROLL_POLICY:
      if (icon_view->priv->vscroll_policy != g_value_get_enum (value))
        {
          icon_view->priv->vscroll_policy = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (icon_view));
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_icon_view_get_property (GObject      *object,
			    guint         prop_id,
			    GValue       *value,
			    GParamSpec   *pspec)
{
  GtkIconView *icon_view;

  icon_view = GTK_ICON_VIEW (object);

  switch (prop_id)
    {
    case PROP_SELECTION_MODE:
      g_value_set_enum (value, icon_view->priv->selection_mode);
      break;
    case PROP_PIXBUF_COLUMN:
      g_value_set_int (value, icon_view->priv->pixbuf_column);
      break;
    case PROP_TEXT_COLUMN:
      g_value_set_int (value, icon_view->priv->text_column);
      break;
    case PROP_MARKUP_COLUMN:
      g_value_set_int (value, icon_view->priv->markup_column);
      break;
    case PROP_MODEL:
      g_value_set_object (value, icon_view->priv->model);
      break;
    case PROP_ITEM_ORIENTATION:
      g_value_set_enum (value, icon_view->priv->item_orientation);
      break;
    case PROP_COLUMNS:
      g_value_set_int (value, icon_view->priv->columns);
      break;
    case PROP_ITEM_WIDTH:
      g_value_set_int (value, icon_view->priv->item_width);
      break;
    case PROP_SPACING:
      g_value_set_int (value, icon_view->priv->spacing);
      break;
    case PROP_ROW_SPACING:
      g_value_set_int (value, icon_view->priv->row_spacing);
      break;
    case PROP_COLUMN_SPACING:
      g_value_set_int (value, icon_view->priv->column_spacing);
      break;
    case PROP_MARGIN:
      g_value_set_int (value, icon_view->priv->margin);
      break;
    case PROP_REORDERABLE:
      g_value_set_boolean (value, icon_view->priv->reorderable);
      break;
    case PROP_TOOLTIP_COLUMN:
      g_value_set_int (value, icon_view->priv->tooltip_column);
      break;

    case PROP_ITEM_PADDING:
      g_value_set_int (value, icon_view->priv->item_padding);
      break;

    case PROP_ACTIVATE_ON_SINGLE_CLICK:
      g_value_set_boolean (value, icon_view->priv->activate_on_single_click);
      break;

    case PROP_CELL_AREA:
      g_value_set_object (value, icon_view->priv->cell_area);
      break;

    case PROP_HADJUSTMENT:
      g_value_set_object (value, icon_view->priv->hadjustment);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, icon_view->priv->vadjustment);
      break;
    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, icon_view->priv->hscroll_policy);
      break;
    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, icon_view->priv->vscroll_policy);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* GtkWidget methods */
static void
gtk_icon_view_destroy (GtkWidget *widget)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (widget);

  gtk_icon_view_set_model (icon_view, NULL);

  if (icon_view->priv->scroll_to_path != NULL)
    {
      gtk_tree_row_reference_free (icon_view->priv->scroll_to_path);
      icon_view->priv->scroll_to_path = NULL;
    }

  remove_scroll_timeout (icon_view);

  if (icon_view->priv->hadjustment != NULL)
    {
      g_object_unref (icon_view->priv->hadjustment);
      icon_view->priv->hadjustment = NULL;
    }

  if (icon_view->priv->vadjustment != NULL)
    {
      g_object_unref (icon_view->priv->vadjustment);
      icon_view->priv->vadjustment = NULL;
    }

  GTK_WIDGET_CLASS (gtk_icon_view_parent_class)->destroy (widget);
}

static void
gtk_icon_view_realize (GtkWidget *widget)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (widget);
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkStyleContext *context;

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  /* Make the main, clipping window */
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gtk_widget_register_window (widget, window);

  gtk_widget_get_allocation (widget, &allocation);

  /* Make the window for the icon view */
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = MAX (icon_view->priv->width, allocation.width);
  attributes.height = MAX (icon_view->priv->height, allocation.height);
  attributes.event_mask = (GDK_EXPOSURE_MASK |
                           GDK_SCROLL_MASK |
                           GDK_SMOOTH_SCROLL_MASK |
                           GDK_POINTER_MOTION_MASK |
                           GDK_BUTTON_PRESS_MASK |
                           GDK_BUTTON_RELEASE_MASK |
                           GDK_KEY_PRESS_MASK |
                           GDK_KEY_RELEASE_MASK) |
    gtk_widget_get_events (widget);
  
  icon_view->priv->bin_window = gdk_window_new (window,
						&attributes, attributes_mask);
  gtk_widget_register_window (widget, icon_view->priv->bin_window);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_background (context, icon_view->priv->bin_window);
  gtk_style_context_set_background (context, window);

  gdk_window_show (icon_view->priv->bin_window);
}

static void
gtk_icon_view_unrealize (GtkWidget *widget)
{
  GtkIconView *icon_view;

  icon_view = GTK_ICON_VIEW (widget);

  gtk_widget_unregister_window (widget, icon_view->priv->bin_window);
  gdk_window_destroy (icon_view->priv->bin_window);
  icon_view->priv->bin_window = NULL;

  GTK_WIDGET_CLASS (gtk_icon_view_parent_class)->unrealize (widget);
}

static void
_gtk_icon_view_update_background (GtkIconView *icon_view)
{
  GtkWidget *widget = GTK_WIDGET (icon_view);

  if (gtk_widget_get_realized (widget))
    {
      GtkStyleContext *context;

      context = gtk_widget_get_style_context (widget);
      gtk_style_context_set_background (context, gtk_widget_get_window (widget));
      gtk_style_context_set_background (context, icon_view->priv->bin_window);
    }
}

static void
gtk_icon_view_state_flags_changed (GtkWidget     *widget,
                                   GtkStateFlags  previous_state)
{
  _gtk_icon_view_update_background (GTK_ICON_VIEW (widget));
  gtk_widget_queue_draw (widget);
}

static void
gtk_icon_view_style_updated (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_icon_view_parent_class)->style_updated (widget);

  _gtk_icon_view_update_background (GTK_ICON_VIEW (widget));
  gtk_widget_queue_resize (widget);
}

static gint
gtk_icon_view_get_n_items (GtkIconView *icon_view)
{
  GtkIconViewPrivate *priv = icon_view->priv;

  if (priv->model == NULL)
    return 0;

  return gtk_tree_model_iter_n_children (priv->model, NULL);
}

static void
adjust_wrap_width (GtkIconView *icon_view)
{
  if (icon_view->priv->text_cell)
    {
      gint pixbuf_width, wrap_width;

      if (icon_view->priv->items && icon_view->priv->pixbuf_cell)
        {
          gtk_cell_renderer_get_preferred_width (icon_view->priv->pixbuf_cell,
                                                 GTK_WIDGET (icon_view),
                                                 &pixbuf_width, NULL);
        }
      else
        {
          pixbuf_width = 0;
        }

      if (icon_view->priv->item_width >= 0)
        {
          if (icon_view->priv->item_orientation == GTK_ORIENTATION_VERTICAL)
            {
              wrap_width = icon_view->priv->item_width;
            }
          else
            { 
              wrap_width = icon_view->priv->item_width - pixbuf_width;
            }

          wrap_width -= 2 * icon_view->priv->item_padding * 2;
        }
      else
        {
          wrap_width = MAX (pixbuf_width * 2, 50);
        }

      if (icon_view->priv->items && icon_view->priv->pixbuf_cell)
	{
          /* Here we go with the same old guess, try the icon size and set double
           * the size of the first icon found in the list, naive but works much
           * of the time */
	  
	  wrap_width = MAX (wrap_width * 2, 50);
	}
      
      g_object_set (icon_view->priv->text_cell, "wrap-width", wrap_width, NULL);
      g_object_set (icon_view->priv->text_cell, "width", wrap_width, NULL);
    }
}

/* General notes about layout
 *
 * The icon view is layouted like this:
 *
 * +----------+  s  +----------+
 * | padding  |  p  | padding  |
 * | +------+ |  a  | +------+ |
 * | | cell | |  c  | | cell | |
 * | +------+ |  i  | +------+ |
 * |          |  n  |          |
 * +----------+  g  +----------+
 *
 * In size request and allocation code, there are 3 sizes that are used:
 * * cell size
 *   This is the size returned by gtk_cell_area_get_preferred_foo(). In places
 *   where code is interacting with the cell area and renderers this is useful.
 * * padded size
 *   This is the cell size plus the item padding on each side.
 * * spaced size
 *   This is the padded size plus the spacing. This is what’s used for most
 *   calculations because it can (ab)use the following formula:
 *   iconview_size = 2 * margin + n_items * spaced_size - spacing
 * So when reading this code and fixing my bugs where I confuse these two, be
 * aware of this distinction.
 */
static void
cell_area_get_preferred_size (GtkIconView        *icon_view,
                              GtkCellAreaContext *context,
                              GtkOrientation      orientation,
                              gint                for_size,
                              gint               *minimum,
                              gint               *natural)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (for_size > 0)
        gtk_cell_area_get_preferred_width_for_height (icon_view->priv->cell_area,
                                                      context,
                                                      GTK_WIDGET (icon_view),
                                                      for_size,
                                                      minimum, natural);
      else
        gtk_cell_area_get_preferred_width (icon_view->priv->cell_area,
                                           context,
                                           GTK_WIDGET (icon_view),
                                           minimum, natural);
    }
  else
    {
      if (for_size > 0)
        gtk_cell_area_get_preferred_height_for_width (icon_view->priv->cell_area,
                                                      context,
                                                      GTK_WIDGET (icon_view),
                                                      for_size,
                                                      minimum, natural);
      else
        gtk_cell_area_get_preferred_height (icon_view->priv->cell_area,
                                            context,
                                            GTK_WIDGET (icon_view),
                                            minimum, natural);
    }
}

static gboolean
gtk_icon_view_is_empty (GtkIconView *icon_view)
{
  return icon_view->priv->items == NULL;
}

static void
gtk_icon_view_get_preferred_item_size (GtkIconView    *icon_view,
                                       GtkOrientation  orientation,
                                       gint            for_size,
                                       gint           *minimum,
                                       gint           *natural)
{
  GtkIconViewPrivate *priv = icon_view->priv;
  GtkCellAreaContext *context;
  GList *items;

  g_assert (!gtk_icon_view_is_empty (icon_view));

  context = gtk_cell_area_create_context (priv->cell_area);

  for_size -= 2 * priv->item_padding;

  if (for_size > 0)
    {
      /* This is necessary for the context to work properly */
      for (items = priv->items; items; items = items->next)
        {
          GtkIconViewItem *item = items->data;

          _gtk_icon_view_set_cell_data (icon_view, item);
          cell_area_get_preferred_size (icon_view, context, 1 - orientation, -1, NULL, NULL);
        }
    }

  for (items = priv->items; items; items = items->next)
    {
      GtkIconViewItem *item = items->data;

      _gtk_icon_view_set_cell_data (icon_view, item);
      if (items == priv->items)
        adjust_wrap_width (icon_view);
      cell_area_get_preferred_size (icon_view, context, orientation, for_size, NULL, NULL);
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (for_size > 0)
        gtk_cell_area_context_get_preferred_width_for_height (context,
                                                              for_size,
                                                              minimum, natural);
      else
        gtk_cell_area_context_get_preferred_width (context,
                                                   minimum, natural);
    }
  else
    {
      if (for_size > 0)
        gtk_cell_area_context_get_preferred_height_for_width (context,
                                                              for_size,
                                                              minimum, natural);
      else
        gtk_cell_area_context_get_preferred_height (context,
                                                    minimum, natural);
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL && priv->item_width >= 0)
    {
      if (minimum)
        *minimum = MAX (*minimum, priv->item_width);
      if (natural)
        *natural = *minimum;
    }

  if (minimum)
    *minimum = MAX (1, *minimum + 2 * priv->item_padding);
  if (natural)
    *natural = MAX (1, *natural + 2 * priv->item_padding);

  g_object_unref (context);
}

static void
gtk_icon_view_compute_n_items_for_size (GtkIconView    *icon_view,
                                        GtkOrientation  orientation,
                                        gint            size,
                                        gint           *min_items,
                                        gint           *min_item_size,
                                        gint           *max_items,
                                        gint           *max_item_size)
{
  GtkIconViewPrivate *priv = icon_view->priv;
  int minimum, natural, spacing;

  g_return_if_fail (min_item_size == NULL || min_items != NULL);
  g_return_if_fail (max_item_size == NULL || max_items != NULL);
  g_return_if_fail (!gtk_icon_view_is_empty (icon_view));

  gtk_icon_view_get_preferred_item_size (icon_view, orientation, -1, &minimum, &natural);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    spacing = priv->column_spacing;
  else
    spacing = priv->row_spacing;
  
  size -= 2 * priv->margin;
  size += spacing;
  minimum += spacing;
  natural += spacing;

  if (priv->columns > 0)
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (min_items)
            *min_items = priv->columns;
          if (max_items)
            *max_items = priv->columns;
        }
      else
        {
          int n_items = gtk_icon_view_get_n_items (icon_view);

          if (min_items)
            *min_items = (n_items + priv->columns - 1) / priv->columns;
          if (max_items)
            *max_items = (n_items + priv->columns - 1) / priv->columns;
        }
    }
  else
    {
      if (max_items)
        {
          if (size <= minimum)
            *max_items = 1;
          else
            *max_items = size / minimum;
        }

      if (min_items)
        {
          if (size <= natural)
            *min_items = 1;
          else
            *min_items = size / natural;
        }
    }

  if (min_item_size)
    {
      *min_item_size = size / *min_items;
      *min_item_size = CLAMP (*min_item_size, minimum, natural);
      *min_item_size -= spacing;
      *min_item_size -= 2 * priv->item_padding;
    }

  if (max_item_size)
    {
      *max_item_size = size / *max_items;
      *max_item_size = CLAMP (*max_item_size, minimum, natural);
      *max_item_size -= spacing;
      *max_item_size -= 2 * priv->item_padding;
    }
}

static GtkSizeRequestMode
gtk_icon_view_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_icon_view_get_preferred_width (GtkWidget *widget,
				   gint      *minimum,
				   gint      *natural)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (widget);
  GtkIconViewPrivate *priv = icon_view->priv;
  int item_min, item_nat;

  if (gtk_icon_view_is_empty (icon_view))
    {
      *minimum = *natural = 2 * priv->margin;
      return;
    }

  gtk_icon_view_get_preferred_item_size (icon_view, GTK_ORIENTATION_HORIZONTAL, -1, &item_min, &item_nat);

  if (priv->columns > 0)
    {
      *minimum = item_min * priv->columns + priv->column_spacing * (priv->columns - 1);
      *natural = item_nat * priv->columns + priv->column_spacing * (priv->columns - 1);
    }
  else
    {
      int n_items = gtk_icon_view_get_n_items (icon_view);

      *minimum = item_min;
      *natural = item_nat * n_items + priv->column_spacing * (n_items - 1);
    }

  *minimum += 2 * priv->margin;
  *natural += 2 * priv->margin;
}

static void
gtk_icon_view_get_preferred_width_for_height (GtkWidget *widget,
                                              gint       height,
                                              gint      *minimum,
                                              gint      *natural)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (widget);
  GtkIconViewPrivate *priv = icon_view->priv;
  int item_min, item_nat, rows, row_height, n_items;

  if (gtk_icon_view_is_empty (icon_view))
    {
      *minimum = *natural = 2 * priv->margin;
      return;
    }

  gtk_icon_view_compute_n_items_for_size (icon_view, GTK_ORIENTATION_VERTICAL, height, &rows, &row_height, NULL, NULL);
  n_items = gtk_icon_view_get_n_items (icon_view);

  gtk_icon_view_get_preferred_item_size (icon_view, GTK_ORIENTATION_HORIZONTAL, row_height, &item_min, &item_nat);
  *minimum = (item_min + priv->column_spacing) * ((n_items + rows - 1) / rows) - priv->column_spacing;
  *natural = (item_nat + priv->column_spacing) * ((n_items + rows - 1) / rows) - priv->column_spacing;

  *minimum += 2 * priv->margin;
  *natural += 2 * priv->margin;
}

static void
gtk_icon_view_get_preferred_height (GtkWidget *widget,
				    gint      *minimum,
				    gint      *natural)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (widget);
  GtkIconViewPrivate *priv = icon_view->priv;
  int item_min, item_nat, n_items;

  if (gtk_icon_view_is_empty (icon_view))
    {
      *minimum = *natural = 2 * priv->margin;
      return;
    }

  gtk_icon_view_get_preferred_item_size (icon_view, GTK_ORIENTATION_VERTICAL, -1, &item_min, &item_nat);
  n_items = gtk_icon_view_get_n_items (icon_view);

  if (priv->columns > 0)
    {
      int n_rows = (n_items + priv->columns - 1) / priv->columns;

      *minimum = item_min * n_rows + priv->row_spacing * (n_rows - 1);
      *natural = item_nat * n_rows + priv->row_spacing * (n_rows - 1);
    }
  else
    {
      *minimum = item_min;
      *natural = item_nat * n_items + priv->row_spacing * (n_items - 1);
    }

  *minimum += 2 * priv->margin;
  *natural += 2 * priv->margin;
}

static void
gtk_icon_view_get_preferred_height_for_width (GtkWidget *widget,
                                              gint       width,
                                              gint      *minimum,
                                              gint      *natural)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (widget);
  GtkIconViewPrivate *priv = icon_view->priv;
  int item_min, item_nat, columns, column_width, n_items;

  if (gtk_icon_view_is_empty (icon_view))
    {
      *minimum = *natural = 2 * priv->margin;
      return;
    }

  gtk_icon_view_compute_n_items_for_size (icon_view, GTK_ORIENTATION_HORIZONTAL, width, NULL, NULL, &columns, &column_width);
  n_items = gtk_icon_view_get_n_items (icon_view);

  gtk_icon_view_get_preferred_item_size (icon_view, GTK_ORIENTATION_VERTICAL, column_width, &item_min, &item_nat);
  *minimum = (item_min + priv->row_spacing) * ((n_items + columns - 1) / columns) - priv->row_spacing;
  *natural = (item_nat + priv->row_spacing) * ((n_items + columns - 1) / columns) - priv->row_spacing;

  *minimum += 2 * priv->margin;
  *natural += 2 * priv->margin;
}

static void
gtk_icon_view_allocate_children (GtkIconView *icon_view)
{
  GList *list;

  for (list = icon_view->priv->children; list; list = list->next)
    {
      GtkIconViewChild *child = list->data;

      /* totally ignore our child's requisition */
      gtk_widget_size_allocate (child->widget, &child->area);
    }
}

static void
gtk_icon_view_size_allocate (GtkWidget      *widget,
			     GtkAllocation  *allocation)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (widget);

  gtk_widget_set_allocation (widget, allocation);

  gtk_icon_view_layout (icon_view);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (gtk_widget_get_window (widget),
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      gdk_window_resize (icon_view->priv->bin_window,
			 MAX (icon_view->priv->width, allocation->width),
			 MAX (icon_view->priv->height, allocation->height));
    }

  gtk_icon_view_allocate_children (icon_view);

  /* Delay signal emission */
  g_object_freeze_notify (G_OBJECT (icon_view->priv->hadjustment));
  g_object_freeze_notify (G_OBJECT (icon_view->priv->vadjustment));

  gtk_icon_view_set_hadjustment_values (icon_view);
  gtk_icon_view_set_vadjustment_values (icon_view);

  if (gtk_widget_get_realized (widget) &&
      icon_view->priv->scroll_to_path)
    {
      GtkTreePath *path;
      path = gtk_tree_row_reference_get_path (icon_view->priv->scroll_to_path);
      gtk_tree_row_reference_free (icon_view->priv->scroll_to_path);
      icon_view->priv->scroll_to_path = NULL;

      gtk_icon_view_scroll_to_path (icon_view, path,
				    icon_view->priv->scroll_to_use_align,
				    icon_view->priv->scroll_to_row_align,
				    icon_view->priv->scroll_to_col_align);
      gtk_tree_path_free (path);
    }

  /* Emit any pending signals now */
  g_object_thaw_notify (G_OBJECT (icon_view->priv->hadjustment));
  g_object_thaw_notify (G_OBJECT (icon_view->priv->vadjustment));
}

static gboolean
gtk_icon_view_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  GtkIconView *icon_view;
  GList *icons;
  GtkTreePath *path;
  gint dest_index;
  GtkIconViewDropPosition dest_pos;
  GtkIconViewItem *dest_item = NULL;
  GtkStyleContext *context;

  icon_view = GTK_ICON_VIEW (widget);

  context = gtk_widget_get_style_context (widget);
  gtk_render_background (context, cr,
                         0, 0,
                         gtk_widget_get_allocated_width (widget),
                         gtk_widget_get_allocated_height (widget));

  if (!gtk_cairo_should_draw_window (cr, icon_view->priv->bin_window))
    return FALSE;

  cairo_save (cr);

  gtk_cairo_transform_to_window (cr, widget, icon_view->priv->bin_window);

  cairo_set_line_width (cr, 1.);

  gtk_icon_view_get_drag_dest_item (icon_view, &path, &dest_pos);

  if (path)
    {
      dest_index = gtk_tree_path_get_indices (path)[0];
      gtk_tree_path_free (path);
    }
  else
    dest_index = -1;

  for (icons = icon_view->priv->items; icons; icons = icons->next)
    {
      GtkIconViewItem *item = icons->data;
      GdkRectangle paint_area;

      paint_area.x      = item->cell_area.x      - icon_view->priv->item_padding;
      paint_area.y      = item->cell_area.y      - icon_view->priv->item_padding;
      paint_area.width  = item->cell_area.width  + icon_view->priv->item_padding * 2;
      paint_area.height = item->cell_area.height + icon_view->priv->item_padding * 2;
      
      cairo_save (cr);

      cairo_rectangle (cr, paint_area.x, paint_area.y, paint_area.width, paint_area.height);
      cairo_clip (cr);

      if (gdk_cairo_get_clip_rectangle (cr, NULL))
        {
          gtk_icon_view_paint_item (icon_view, cr, item,
                                    item->cell_area.x, item->cell_area.y,
                                    icon_view->priv->draw_focus);

          if (dest_index == item->index)
            dest_item = item;
        }

      cairo_restore (cr);
    }

  if (dest_item &&
      dest_pos != GTK_ICON_VIEW_NO_DROP)
    {
      GtkStyleContext *context;
      GdkRectangle rect = { 0 };

      context = gtk_widget_get_style_context (widget);

      switch (dest_pos)
	{
	case GTK_ICON_VIEW_DROP_INTO:
          rect = dest_item->cell_area;
	  break;
	case GTK_ICON_VIEW_DROP_ABOVE:
          rect.x = dest_item->cell_area.x;
          rect.y = dest_item->cell_area.y - 1;
          rect.width = dest_item->cell_area.width;
          rect.height = 2;
	  break;
	case GTK_ICON_VIEW_DROP_LEFT:
          rect.x = dest_item->cell_area.x - 1;
          rect.y = dest_item->cell_area.y;
          rect.width = 2;
          rect.height = dest_item->cell_area.height;
	  break;
	case GTK_ICON_VIEW_DROP_BELOW:
          rect.x = dest_item->cell_area.x;
          rect.y = dest_item->cell_area.y + dest_item->cell_area.height - 1;
          rect.width = dest_item->cell_area.width;
          rect.height = 2;
	  break;
	case GTK_ICON_VIEW_DROP_RIGHT:
          rect.x = dest_item->cell_area.x + dest_item->cell_area.width - 1;
          rect.y = dest_item->cell_area.y;
          rect.width = 2;
          rect.height = dest_item->cell_area.height;
	case GTK_ICON_VIEW_NO_DROP: ;
	  break;
        }

      gtk_render_focus (context, cr,
                        rect.x, rect.y,
                        rect.width, rect.height);
    }

  if (icon_view->priv->doing_rubberband)
    gtk_icon_view_paint_rubberband (icon_view, cr);

  cairo_restore (cr);

  return GTK_WIDGET_CLASS (gtk_icon_view_parent_class)->draw (widget, cr);
}

static gboolean
rubberband_scroll_timeout (gpointer data)
{
  GtkIconView *icon_view = data;

  gtk_adjustment_set_value (icon_view->priv->vadjustment,
                            gtk_adjustment_get_value (icon_view->priv->vadjustment) +
                            icon_view->priv->scroll_value_diff);

  gtk_icon_view_update_rubberband (icon_view);
  
  return TRUE;
}

static gboolean
gtk_icon_view_motion (GtkWidget      *widget,
		      GdkEventMotion *event)
{
  GtkAllocation allocation;
  GtkIconView *icon_view;
  gint abs_y;
  
  icon_view = GTK_ICON_VIEW (widget);

  gtk_icon_view_maybe_begin_drag (icon_view, event);

  if (icon_view->priv->doing_rubberband)
    {
      gtk_icon_view_update_rubberband (widget);
      
      abs_y = event->y - icon_view->priv->height *
	(gtk_adjustment_get_value (icon_view->priv->vadjustment) /
	 (gtk_adjustment_get_upper (icon_view->priv->vadjustment) -
	  gtk_adjustment_get_lower (icon_view->priv->vadjustment)));

      gtk_widget_get_allocation (widget, &allocation);

      if (abs_y < 0 || abs_y > allocation.height)
	{
	  if (abs_y < 0)
	    icon_view->priv->scroll_value_diff = abs_y;
	  else
	    icon_view->priv->scroll_value_diff = abs_y - allocation.height;

	  icon_view->priv->event_last_x = event->x;
	  icon_view->priv->event_last_y = event->y;

	  if (icon_view->priv->scroll_timeout_id == 0) {
	    icon_view->priv->scroll_timeout_id = gdk_threads_add_timeout (30, rubberband_scroll_timeout, 
								icon_view);
	    g_source_set_name_by_id (icon_view->priv->scroll_timeout_id, "[gtk+] rubberband_scroll_timeout");
	  }
 	}
      else 
	remove_scroll_timeout (icon_view);
    }
  else
    {
      GtkIconViewItem *item, *last_prelight_item;
      GtkCellRenderer *cell = NULL;

      last_prelight_item = icon_view->priv->last_prelight;
      item = _gtk_icon_view_get_item_at_coords (icon_view,
                                               event->x, event->y,
                                               FALSE,
                                               &cell);

      if (item != last_prelight_item)
        {
          if (item != NULL)
            {
              item->prelight = TRUE;
              gtk_icon_view_queue_draw_item (icon_view, item);
            }

          if (last_prelight_item != NULL)
            {
              last_prelight_item->prelight = FALSE;
              gtk_icon_view_queue_draw_item (icon_view,
                                             icon_view->priv->last_prelight);
            }

          icon_view->priv->last_prelight = item;
        }
    }
  
  return TRUE;
}

static void
gtk_icon_view_remove (GtkContainer *container,
		      GtkWidget    *widget)
{
  GtkIconView *icon_view;
  GtkIconViewChild *child = NULL;
  GList *tmp_list;

  icon_view = GTK_ICON_VIEW (container);
  
  tmp_list = icon_view->priv->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      if (child->widget == widget)
	{
	  gtk_widget_unparent (widget);

	  icon_view->priv->children = g_list_remove_link (icon_view->priv->children, tmp_list);
	  g_list_free_1 (tmp_list);
	  g_free (child);
	  return;
	}

      tmp_list = tmp_list->next;
    }
}

static void
gtk_icon_view_forall (GtkContainer *container,
		      gboolean      include_internals,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkIconView *icon_view;
  GtkIconViewChild *child = NULL;
  GList *tmp_list;

  icon_view = GTK_ICON_VIEW (container);

  tmp_list = icon_view->priv->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      tmp_list = tmp_list->next;

      (* callback) (child->widget, callback_data);
    }
}

static void 
gtk_icon_view_item_selected_changed (GtkIconView      *icon_view,
                                     GtkIconViewItem  *item)
{
  AtkObject *obj;
  AtkObject *item_obj;

  obj = gtk_widget_get_accessible (GTK_WIDGET (icon_view));
  if (obj != NULL)
    {
      item_obj = atk_object_ref_accessible_child (obj, item->index);
      if (item_obj != NULL)
        {
          atk_object_notify_state_change (item_obj, ATK_STATE_SELECTED, item->selected);
          g_object_unref (item_obj);
        }
    }
}

static void
gtk_icon_view_add_editable (GtkCellArea            *area,
			    GtkCellRenderer        *renderer,
			    GtkCellEditable        *editable,
			    GdkRectangle           *cell_area,
			    const gchar            *path,
			    GtkIconView            *icon_view)
{
  GtkIconViewChild *child;
  GtkWidget *widget = GTK_WIDGET (editable);
  
  child = g_new (GtkIconViewChild, 1);
  
  child->widget      = widget;
  child->area.x      = cell_area->x;
  child->area.y      = cell_area->y;
  child->area.width  = cell_area->width;
  child->area.height = cell_area->height;

  icon_view->priv->children = g_list_append (icon_view->priv->children, child);

  if (gtk_widget_get_realized (GTK_WIDGET (icon_view)))
    gtk_widget_set_parent_window (child->widget, icon_view->priv->bin_window);
  
  gtk_widget_set_parent (widget, GTK_WIDGET (icon_view));
}

static void
gtk_icon_view_remove_editable (GtkCellArea            *area,
			       GtkCellRenderer        *renderer,
			       GtkCellEditable        *editable,
			       GtkIconView            *icon_view)
{
  GtkTreePath *path;

  if (gtk_widget_has_focus (GTK_WIDGET (editable)))
    gtk_widget_grab_focus (GTK_WIDGET (icon_view));
  
  gtk_container_remove (GTK_CONTAINER (icon_view),
			GTK_WIDGET (editable));  

  path = gtk_tree_path_new_from_string (gtk_cell_area_get_current_path_string (area));
  gtk_icon_view_queue_draw_path (icon_view, path);
  gtk_tree_path_free (path);
}

/**
 * gtk_icon_view_set_cursor:
 * @icon_view: A #GtkIconView
 * @path: A #GtkTreePath
 * @cell: (allow-none): One of the cell renderers of @icon_view, or %NULL
 * @start_editing: %TRUE if the specified cell should start being edited.
 *
 * Sets the current keyboard focus to be at @path, and selects it.  This is
 * useful when you want to focus the user’s attention on a particular item.
 * If @cell is not %NULL, then focus is given to the cell specified by 
 * it. Additionally, if @start_editing is %TRUE, then editing should be 
 * started in the specified cell.  
 *
 * This function is often followed by `gtk_widget_grab_focus 
 * (icon_view)` in order to give keyboard focus to the widget.  
 * Please note that editing can only happen when the widget is realized.
 *
 * Since: 2.8
 **/
void
gtk_icon_view_set_cursor (GtkIconView     *icon_view,
			  GtkTreePath     *path,
			  GtkCellRenderer *cell,
			  gboolean         start_editing)
{
  GtkIconViewItem *item = NULL;

  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  g_return_if_fail (path != NULL);
  g_return_if_fail (cell == NULL || GTK_IS_CELL_RENDERER (cell));

  if (icon_view->priv->cell_area)
    gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

  if (gtk_tree_path_get_depth (path) == 1)
    item = g_list_nth_data (icon_view->priv->items,
			    gtk_tree_path_get_indices(path)[0]);
  
  if (!item)
    return;
  
  _gtk_icon_view_set_cursor_item (icon_view, item, cell);
  gtk_icon_view_scroll_to_path (icon_view, path, FALSE, 0.0, 0.0);

  if (start_editing && 
      icon_view->priv->cell_area)
    {
      GtkCellAreaContext *context;

      context = g_ptr_array_index (icon_view->priv->row_contexts, item->row);
      _gtk_icon_view_set_cell_data (icon_view, item);
      gtk_cell_area_activate (icon_view->priv->cell_area, context, 
			      GTK_WIDGET (icon_view), &item->cell_area, 
			      0 /* XXX flags */, TRUE);
    }
}

/**
 * gtk_icon_view_get_cursor:
 * @icon_view: A #GtkIconView
 * @path: (out) (allow-none) (transfer full): Return location for the current
 *        cursor path, or %NULL
 * @cell: (out) (allow-none) (transfer none): Return location the current
 *        focus cell, or %NULL
 *
 * Fills in @path and @cell with the current cursor path and cell. 
 * If the cursor isn’t currently set, then *@path will be %NULL.  
 * If no cell currently has focus, then *@cell will be %NULL.
 *
 * The returned #GtkTreePath must be freed with gtk_tree_path_free().
 *
 * Returns: %TRUE if the cursor is set.
 *
 * Since: 2.8
 **/
gboolean
gtk_icon_view_get_cursor (GtkIconView      *icon_view,
			  GtkTreePath     **path,
			  GtkCellRenderer **cell)
{
  GtkIconViewItem *item;

  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), FALSE);

  item = icon_view->priv->cursor_item;

  if (path != NULL)
    {
      if (item != NULL)
	*path = gtk_tree_path_new_from_indices (item->index, -1);
      else
	*path = NULL;
    }

  if (cell != NULL && item != NULL && icon_view->priv->cell_area != NULL)
    *cell = gtk_cell_area_get_focus_cell (icon_view->priv->cell_area);

  return (item != NULL);
}

static gboolean
gtk_icon_view_button_press (GtkWidget      *widget,
			    GdkEventButton *event)
{
  GtkIconView *icon_view;
  GtkIconViewItem *item;
  gboolean dirty = FALSE;
  GtkCellRenderer *cell = NULL, *cursor_cell = NULL;

  icon_view = GTK_ICON_VIEW (widget);

  if (event->window != icon_view->priv->bin_window)
    return FALSE;

  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  if (event->button == GDK_BUTTON_PRIMARY && event->type == GDK_BUTTON_PRESS)
    {
      GdkModifierType extend_mod_mask;
      GdkModifierType modify_mod_mask;

      extend_mod_mask =
        gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_EXTEND_SELECTION);

      modify_mod_mask =
        gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_MODIFY_SELECTION);

      item = _gtk_icon_view_get_item_at_coords (icon_view, 
					       event->x, event->y,
					       FALSE,
					       &cell);

      /*
       * We consider only the cells' area as the item area if the
       * item is not selected, but if it *is* selected, the complete
       * selection rectangle is considered to be part of the item.
       */
      if (item != NULL && (cell != NULL || item->selected))
	{
	  if (cell != NULL)
	    {
	      if (gtk_cell_renderer_is_activatable (cell))
		cursor_cell = cell;
	    }

	  gtk_icon_view_scroll_to_item (icon_view, item);
	  
	  if (icon_view->priv->selection_mode == GTK_SELECTION_NONE)
	    {
	      _gtk_icon_view_set_cursor_item (icon_view, item, cursor_cell);
	    }
	  else if (icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE &&
		   (event->state & extend_mod_mask))
	    {
	      gtk_icon_view_unselect_all_internal (icon_view);

	      _gtk_icon_view_set_cursor_item (icon_view, item, cursor_cell);
	      if (!icon_view->priv->anchor_item)
		icon_view->priv->anchor_item = item;
	      else 
		gtk_icon_view_select_all_between (icon_view,
						  icon_view->priv->anchor_item,
						  item);
	      dirty = TRUE;
	    }
	  else 
	    {
	      if ((icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE ||
		  ((icon_view->priv->selection_mode == GTK_SELECTION_SINGLE) && item->selected)) &&
		  (event->state & modify_mod_mask))
		{
		  item->selected = !item->selected;
		  gtk_icon_view_queue_draw_item (icon_view, item);
		  dirty = TRUE;
		}
	      else
		{
		  gtk_icon_view_unselect_all_internal (icon_view);

		  item->selected = TRUE;
		  gtk_icon_view_queue_draw_item (icon_view, item);
		  dirty = TRUE;
		}
	      _gtk_icon_view_set_cursor_item (icon_view, item, cursor_cell);
	      icon_view->priv->anchor_item = item;
	    }

	  /* Save press to possibly begin a drag */
	  if (icon_view->priv->pressed_button < 0)
	    {
	      icon_view->priv->pressed_button = event->button;
	      icon_view->priv->press_start_x = event->x;
	      icon_view->priv->press_start_y = event->y;
	    }

          icon_view->priv->last_single_clicked = item;

	  /* cancel the current editing, if it exists */
	  gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

	  if (cell != NULL && gtk_cell_renderer_is_activatable (cell))
	    {
	      GtkCellAreaContext *context;

	      context = g_ptr_array_index (icon_view->priv->row_contexts, item->row);

	      _gtk_icon_view_set_cell_data (icon_view, item);
	      gtk_cell_area_activate (icon_view->priv->cell_area, context,
				      GTK_WIDGET (icon_view),
				      &item->cell_area, 0/* XXX flags */, FALSE);
	    }
	}
      else
	{
	  if (icon_view->priv->selection_mode != GTK_SELECTION_BROWSE &&
	      !(event->state & modify_mod_mask))
	    {
	      dirty = gtk_icon_view_unselect_all_internal (icon_view);
	    }
	  
	  if (icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE)
            gtk_icon_view_start_rubberbanding (icon_view, event->device, event->x, event->y);
	}

      /* don't draw keyboard focus around an clicked-on item */
      icon_view->priv->draw_focus = FALSE;
    }

  if (!icon_view->priv->activate_on_single_click
      && event->button == GDK_BUTTON_PRIMARY
      && event->type == GDK_2BUTTON_PRESS)
    {
      item = _gtk_icon_view_get_item_at_coords (icon_view,
					       event->x, event->y,
					       FALSE,
					       NULL);

      if (item && item == icon_view->priv->last_single_clicked)
	{
	  GtkTreePath *path;

	  path = gtk_tree_path_new_from_indices (item->index, -1);
	  gtk_icon_view_item_activated (icon_view, path);
	  gtk_tree_path_free (path);
	}

      icon_view->priv->last_single_clicked = NULL;
      icon_view->priv->pressed_button = -1;
    }
  
  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);

  return event->button == GDK_BUTTON_PRIMARY;
}

static gboolean
button_event_modifies_selection (GdkEventButton *event)
{
        return (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) != 0;
}

static gboolean
gtk_icon_view_button_release (GtkWidget      *widget,
			      GdkEventButton *event)
{
  GtkIconView *icon_view;

  icon_view = GTK_ICON_VIEW (widget);
  
  if (icon_view->priv->pressed_button == event->button)
    icon_view->priv->pressed_button = -1;

  gtk_icon_view_stop_rubberbanding (icon_view);

  remove_scroll_timeout (icon_view);

  if (event->button == GDK_BUTTON_PRIMARY
      && icon_view->priv->activate_on_single_click
      && !button_event_modifies_selection (event)
      && icon_view->priv->last_single_clicked != NULL)
    {
      GtkIconViewItem *item;

      item = _gtk_icon_view_get_item_at_coords (icon_view,
                                                event->x, event->y,
                                                FALSE,
                                                NULL);
      if (item == icon_view->priv->last_single_clicked)
        {
          GtkTreePath *path;
          path = gtk_tree_path_new_from_indices (item->index, -1);
          gtk_icon_view_item_activated (icon_view, path);
          gtk_tree_path_free (path);
        }

      icon_view->priv->last_single_clicked = NULL;
    }

  return TRUE;
}

static gboolean
gtk_icon_view_key_press (GtkWidget      *widget,
			 GdkEventKey    *event)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (widget);

  if (icon_view->priv->doing_rubberband)
    {
      if (event->keyval == GDK_KEY_Escape)
	gtk_icon_view_stop_rubberbanding (icon_view);

      return TRUE;
    }

  return GTK_WIDGET_CLASS (gtk_icon_view_parent_class)->key_press_event (widget, event);
}

static gboolean
gtk_icon_view_key_release (GtkWidget      *widget,
			   GdkEventKey    *event)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (widget);

  if (icon_view->priv->doing_rubberband)
    return TRUE;

  return GTK_WIDGET_CLASS (gtk_icon_view_parent_class)->key_release_event (widget, event);
}

static void
gtk_icon_view_update_rubberband (gpointer data)
{
  GtkIconView *icon_view;
  gint x, y;
  GdkRectangle old_area;
  GdkRectangle new_area;
  cairo_region_t *invalid_region;
  
  icon_view = GTK_ICON_VIEW (data);

  gdk_window_get_device_position (icon_view->priv->bin_window,
                                  icon_view->priv->rubberband_device,
                                  &x, &y, NULL);

  x = MAX (x, 0);
  y = MAX (y, 0);

  old_area.x = MIN (icon_view->priv->rubberband_x1,
		    icon_view->priv->rubberband_x2);
  old_area.y = MIN (icon_view->priv->rubberband_y1,
		    icon_view->priv->rubberband_y2);
  old_area.width = ABS (icon_view->priv->rubberband_x2 -
			icon_view->priv->rubberband_x1) + 1;
  old_area.height = ABS (icon_view->priv->rubberband_y2 -
			 icon_view->priv->rubberband_y1) + 1;
  
  new_area.x = MIN (icon_view->priv->rubberband_x1, x);
  new_area.y = MIN (icon_view->priv->rubberband_y1, y);
  new_area.width = ABS (x - icon_view->priv->rubberband_x1) + 1;
  new_area.height = ABS (y - icon_view->priv->rubberband_y1) + 1;

  invalid_region = cairo_region_create_rectangle (&old_area);
  cairo_region_union_rectangle (invalid_region, &new_area);

  gdk_window_invalidate_region (icon_view->priv->bin_window, invalid_region, TRUE);
    
  cairo_region_destroy (invalid_region);

  icon_view->priv->rubberband_x2 = x;
  icon_view->priv->rubberband_y2 = y;  

  gtk_icon_view_update_rubberband_selection (icon_view);
}

static void
gtk_icon_view_start_rubberbanding (GtkIconView  *icon_view,
                                   GdkDevice    *device,
				   gint          x,
				   gint          y)
{
  GList *items;

  if (icon_view->priv->rubberband_device)
    return;

  for (items = icon_view->priv->items; items; items = items->next)
    {
      GtkIconViewItem *item = items->data;

      item->selected_before_rubberbanding = item->selected;
    }
  
  icon_view->priv->rubberband_x1 = x;
  icon_view->priv->rubberband_y1 = y;
  icon_view->priv->rubberband_x2 = x;
  icon_view->priv->rubberband_y2 = y;

  icon_view->priv->doing_rubberband = TRUE;
  icon_view->priv->rubberband_device = device;
}

static void
gtk_icon_view_stop_rubberbanding (GtkIconView *icon_view)
{
  if (!icon_view->priv->doing_rubberband)
    return;

  icon_view->priv->doing_rubberband = FALSE;
  icon_view->priv->rubberband_device = NULL;

  gtk_widget_queue_draw (GTK_WIDGET (icon_view));
}

static void
gtk_icon_view_update_rubberband_selection (GtkIconView *icon_view)
{
  GList *items;
  gint x, y, width, height;
  gboolean dirty = FALSE;
  
  x = MIN (icon_view->priv->rubberband_x1,
	   icon_view->priv->rubberband_x2);
  y = MIN (icon_view->priv->rubberband_y1,
	   icon_view->priv->rubberband_y2);
  width = ABS (icon_view->priv->rubberband_x1 - 
	       icon_view->priv->rubberband_x2);
  height = ABS (icon_view->priv->rubberband_y1 - 
		icon_view->priv->rubberband_y2);
  
  for (items = icon_view->priv->items; items; items = items->next)
    {
      GtkIconViewItem *item = items->data;
      gboolean is_in;
      gboolean selected;
      
      is_in = gtk_icon_view_item_hit_test (icon_view, item, 
					   x, y, width, height);

      selected = is_in ^ item->selected_before_rubberbanding;

      if (item->selected != selected)
	{
	  item->selected = selected;
	  dirty = TRUE;
	  gtk_icon_view_queue_draw_item (icon_view, item);
	}
    }

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}


typedef struct {
  GdkRectangle hit_rect;
  gboolean     hit;
} HitTestData;

static gboolean 
hit_test (GtkCellRenderer    *renderer,
	  const GdkRectangle *cell_area,
	  const GdkRectangle *cell_background,
	  HitTestData        *data)
{
  if (MIN (data->hit_rect.x + data->hit_rect.width, cell_area->x + cell_area->width) - 
      MAX (data->hit_rect.x, cell_area->x) > 0 &&
      MIN (data->hit_rect.y + data->hit_rect.height, cell_area->y + cell_area->height) - 
      MAX (data->hit_rect.y, cell_area->y) > 0)
    data->hit = TRUE;
  
  return (data->hit != FALSE);
}

static gboolean
gtk_icon_view_item_hit_test (GtkIconView      *icon_view,
			     GtkIconViewItem  *item,
			     gint              x,
			     gint              y,
			     gint              width,
			     gint              height)
{
  HitTestData data = { { x, y, width, height }, FALSE };
  GtkCellAreaContext *context;
  GdkRectangle *item_area = &item->cell_area;
   
  if (MIN (x + width, item_area->x + item_area->width) - MAX (x, item_area->x) <= 0 ||
      MIN (y + height, item_area->y + item_area->height) - MAX (y, item_area->y) <= 0)
    return FALSE;

  context = g_ptr_array_index (icon_view->priv->row_contexts, item->row);

  _gtk_icon_view_set_cell_data (icon_view, item);
  gtk_cell_area_foreach_alloc (icon_view->priv->cell_area, context,
			       GTK_WIDGET (icon_view),
			       item_area, item_area,
			       (GtkCellAllocCallback)hit_test, &data);

  return data.hit;
}

static gboolean
gtk_icon_view_unselect_all_internal (GtkIconView  *icon_view)
{
  gboolean dirty = FALSE;
  GList *items;

  if (icon_view->priv->selection_mode == GTK_SELECTION_NONE)
    return FALSE;

  for (items = icon_view->priv->items; items; items = items->next)
    {
      GtkIconViewItem *item = items->data;

      if (item->selected)
	{
	  item->selected = FALSE;
	  dirty = TRUE;
	  gtk_icon_view_queue_draw_item (icon_view, item);
	  gtk_icon_view_item_selected_changed (icon_view, item);
	}
    }

  return dirty;
}


/* GtkIconView signals */
static void
gtk_icon_view_real_select_all (GtkIconView *icon_view)
{
  gtk_icon_view_select_all (icon_view);
}

static void
gtk_icon_view_real_unselect_all (GtkIconView *icon_view)
{
  gtk_icon_view_unselect_all (icon_view);
}

static void
gtk_icon_view_real_select_cursor_item (GtkIconView *icon_view)
{
  gtk_icon_view_unselect_all (icon_view);

  if (icon_view->priv->cursor_item != NULL)
    _gtk_icon_view_select_item (icon_view, icon_view->priv->cursor_item);
}

static gboolean
gtk_icon_view_real_activate_cursor_item (GtkIconView *icon_view)
{
  GtkTreePath *path;
  GtkCellAreaContext *context;

  if (!icon_view->priv->cursor_item)
    return FALSE;

  context = g_ptr_array_index (icon_view->priv->row_contexts, icon_view->priv->cursor_item->row);

  _gtk_icon_view_set_cell_data (icon_view, icon_view->priv->cursor_item);
  gtk_cell_area_activate (icon_view->priv->cell_area, context,
                          GTK_WIDGET (icon_view),
                          &icon_view->priv->cursor_item->cell_area,
                          0 /* XXX flags */,
                          FALSE);

  path = gtk_tree_path_new_from_indices (icon_view->priv->cursor_item->index, -1);
  gtk_icon_view_item_activated (icon_view, path);
  gtk_tree_path_free (path);

  return TRUE;
}

static void
gtk_icon_view_real_toggle_cursor_item (GtkIconView *icon_view)
{
  if (!icon_view->priv->cursor_item)
    return;

  switch (icon_view->priv->selection_mode)
    {
    case GTK_SELECTION_NONE:
      break;
    case GTK_SELECTION_BROWSE:
      _gtk_icon_view_select_item (icon_view, icon_view->priv->cursor_item);
      break;
    case GTK_SELECTION_SINGLE:
      if (icon_view->priv->cursor_item->selected)
	_gtk_icon_view_unselect_item (icon_view, icon_view->priv->cursor_item);
      else
	_gtk_icon_view_select_item (icon_view, icon_view->priv->cursor_item);
      break;
    case GTK_SELECTION_MULTIPLE:
      icon_view->priv->cursor_item->selected = !icon_view->priv->cursor_item->selected;
      g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0); 
      
      gtk_icon_view_item_selected_changed (icon_view, icon_view->priv->cursor_item);      
      gtk_icon_view_queue_draw_item (icon_view, icon_view->priv->cursor_item);
      break;
    }
}

static void
gtk_icon_view_set_hadjustment_values (GtkIconView *icon_view)
{
  GtkAllocation  allocation;
  GtkAdjustment *adj = icon_view->priv->hadjustment;
  gdouble old_page_size;
  gdouble old_upper;
  gdouble old_value;
  gdouble new_value;
  gdouble new_upper;

  gtk_widget_get_allocation (GTK_WIDGET (icon_view), &allocation);

  old_value = gtk_adjustment_get_value (adj);
  old_upper = gtk_adjustment_get_upper (adj);
  old_page_size = gtk_adjustment_get_page_size (adj);
  new_upper = MAX (allocation.width, icon_view->priv->width);

  if (gtk_widget_get_direction (GTK_WIDGET (icon_view)) == GTK_TEXT_DIR_RTL)
    {
      /* Make sure no scrolling occurs for RTL locales also (if possible) */
      /* Quick explanation:
       *   In LTR locales, leftmost portion of visible rectangle should stay
       *   fixed, which means left edge of scrollbar thumb should remain fixed
       *   and thus adjustment's value should stay the same.
       *
       *   In RTL locales, we want to keep rightmost portion of visible
       *   rectangle fixed. This means right edge of thumb should remain fixed.
       *   In this case, upper - value - page_size should remain constant.
       */
      new_value = (new_upper - allocation.width) -
                  (old_upper - old_value - old_page_size);
      new_value = CLAMP (new_value, 0, new_upper - allocation.width);
    }
  else
    new_value = CLAMP (old_value, 0, new_upper - allocation.width);

  gtk_adjustment_configure (adj,
                            new_value,
                            0.0,
                            new_upper,
                            allocation.width * 0.1,
                            allocation.width * 0.9,
                            allocation.width);
}

static void
gtk_icon_view_set_vadjustment_values (GtkIconView *icon_view)
{
  GtkAllocation  allocation;
  GtkAdjustment *adj = icon_view->priv->vadjustment;

  gtk_widget_get_allocation (GTK_WIDGET (icon_view), &allocation);

  gtk_adjustment_configure (adj,
                            gtk_adjustment_get_value (adj),
                            0.0,
                            MAX (allocation.height, icon_view->priv->height),
                            allocation.height * 0.1,
                            allocation.height * 0.9,
                            allocation.height);
}

static void
gtk_icon_view_set_hadjustment (GtkIconView   *icon_view,
                               GtkAdjustment *adjustment)
{
  GtkIconViewPrivate *priv = icon_view->priv;

  if (adjustment && priv->hadjustment == adjustment)
    return;

  if (priv->hadjustment != NULL)
    {
      g_signal_handlers_disconnect_matched (priv->hadjustment,
                                            G_SIGNAL_MATCH_DATA,
                                            0, 0, NULL, NULL, icon_view);
      g_object_unref (priv->hadjustment);
    }

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gtk_icon_view_adjustment_changed), icon_view);
  priv->hadjustment = g_object_ref_sink (adjustment);
  gtk_icon_view_set_hadjustment_values (icon_view);

  g_object_notify (G_OBJECT (icon_view), "hadjustment");
}

static void
gtk_icon_view_set_vadjustment (GtkIconView   *icon_view,
                               GtkAdjustment *adjustment)
{
  GtkIconViewPrivate *priv = icon_view->priv;

  if (adjustment && priv->vadjustment == adjustment)
    return;

  if (priv->vadjustment != NULL)
    {
      g_signal_handlers_disconnect_matched (priv->vadjustment,
                                            G_SIGNAL_MATCH_DATA,
                                            0, 0, NULL, NULL, icon_view);
      g_object_unref (priv->vadjustment);
    }

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gtk_icon_view_adjustment_changed), icon_view);
  priv->vadjustment = g_object_ref_sink (adjustment);
  gtk_icon_view_set_vadjustment_values (icon_view);

  g_object_notify (G_OBJECT (icon_view), "vadjustment");
}

static void
gtk_icon_view_adjustment_changed (GtkAdjustment *adjustment,
                                  GtkIconView   *icon_view)
{
  GtkIconViewPrivate *priv = icon_view->priv;

  if (gtk_widget_get_realized (GTK_WIDGET (icon_view)))
    {
      gdk_window_move (priv->bin_window,
                       - gtk_adjustment_get_value (priv->hadjustment),
                       - gtk_adjustment_get_value (priv->vadjustment));

      if (icon_view->priv->doing_rubberband)
        gtk_icon_view_update_rubberband (GTK_WIDGET (icon_view));

      _gtk_icon_view_accessible_adjustment_changed (icon_view);
    }
}

static gint
compare_sizes (gconstpointer p1,
	       gconstpointer p2,
               gpointer      unused)
{
  return GPOINTER_TO_INT (((const GtkRequestedSize *) p1)->data)
       - GPOINTER_TO_INT (((const GtkRequestedSize *) p2)->data);
}

static void
gtk_icon_view_layout (GtkIconView *icon_view)
{
  GtkIconViewPrivate *priv = icon_view->priv;
  GtkWidget *widget = GTK_WIDGET (icon_view);
  GList *items;
  gint item_width; /* this doesn't include item_padding */
  gint n_columns, n_rows, n_items;
  gint col, row;
  GtkRequestedSize *sizes;
  gboolean rtl;

  if (gtk_icon_view_is_empty (icon_view))
    return;

  rtl = gtk_widget_get_direction (GTK_WIDGET (icon_view)) == GTK_TEXT_DIR_RTL;
  n_items = gtk_icon_view_get_n_items (icon_view);

  gtk_icon_view_compute_n_items_for_size (icon_view, 
                                          GTK_ORIENTATION_HORIZONTAL,
                                          gtk_widget_get_allocated_width (widget),
                                          NULL, NULL,
                                          &n_columns, &item_width);
  n_rows = (n_items + n_columns - 1) / n_columns;

  priv->width = n_columns * (item_width + 2 * priv->item_padding + priv->column_spacing) - priv->column_spacing;
  priv->width += 2 * priv->margin;
  priv->width = MAX (priv->width, gtk_widget_get_allocated_width (widget));

  /* Clear the per row contexts */
  g_ptr_array_set_size (icon_view->priv->row_contexts, 0);

  gtk_cell_area_context_reset (priv->cell_area_context);
  /* because layouting is complicated. We designed an API
   * that is O(N²) and nonsensical.
   * And we're proud of it. */
  for (items = priv->items; items; items = items->next)
    {
      _gtk_icon_view_set_cell_data (icon_view, items->data);
      gtk_cell_area_get_preferred_width (priv->cell_area,
                                         priv->cell_area_context,
                                         widget,
                                         NULL, NULL);
    }

  sizes = g_newa (GtkRequestedSize, n_rows);
  items = priv->items;
  priv->height = priv->margin;

  /* Collect the heights for all rows */
  for (row = 0; row < n_rows; row++)
    {
      GtkCellAreaContext *context = gtk_cell_area_copy_context (priv->cell_area, priv->cell_area_context);
      g_ptr_array_add (priv->row_contexts, context);

      for (col = 0; col < n_columns && items; col++, items = items->next)
        {
          GtkIconViewItem *item = items->data;

          _gtk_icon_view_set_cell_data (icon_view, item);
          gtk_cell_area_get_preferred_height_for_width (priv->cell_area,
                                                        context,
                                                        widget,
                                                        item_width, 
                                                        NULL, NULL);
        }
      
      sizes[row].data = GINT_TO_POINTER (row);
      gtk_cell_area_context_get_preferred_height_for_width (context,
                                                            item_width,
                                                            &sizes[row].minimum_size,
                                                            &sizes[row].natural_size);
      priv->height += sizes[row].minimum_size + 2 * priv->item_padding + priv->row_spacing;
    }

  priv->height -= priv->row_spacing;
  priv->height += priv->margin;
  priv->height = MIN (priv->height, gtk_widget_get_allocated_height (widget));

  gtk_distribute_natural_allocation (gtk_widget_get_allocated_height (widget) - priv->height,
                                     n_rows,
                                     sizes);

  /* Actually allocate the rows */
  g_qsort_with_data (sizes, n_rows, sizeof (GtkRequestedSize), compare_sizes, NULL);
  
  items = priv->items;
  priv->height = priv->margin;

  for (row = 0; row < n_rows; row++)
    {
      GtkCellAreaContext *context = g_ptr_array_index (priv->row_contexts, row);
      gtk_cell_area_context_allocate (context, item_width, sizes[row].minimum_size);

      priv->height += priv->item_padding;

      for (col = 0; col < n_columns && items; col++, items = items->next)
        {
          GtkIconViewItem *item = items->data;

          item->cell_area.x = priv->margin + (col * 2 + 1) * priv->item_padding + col * (priv->column_spacing + item_width);
          item->cell_area.width = item_width;
          item->cell_area.y = priv->height;
          item->cell_area.height = sizes[row].minimum_size;
          item->row = row;
          item->col = col;
          if (rtl)
            {
              item->cell_area.x = priv->width - item_width - item->cell_area.x;
              item->col = n_columns - 1 - col;
            }
        }

      priv->height += sizes[row].minimum_size + priv->item_padding + priv->row_spacing;
    }

  priv->height -= priv->row_spacing;
  priv->height += priv->margin;
  priv->height = MAX (priv->height, gtk_widget_get_allocated_height (widget));
}

static void
gtk_icon_view_invalidate_sizes (GtkIconView *icon_view)
{
  /* Clear all item sizes */
  g_list_foreach (icon_view->priv->items,
		  (GFunc)gtk_icon_view_item_invalidate_size, NULL);

  /* Re-layout the items */
  gtk_widget_queue_resize (GTK_WIDGET (icon_view));
}

static void
gtk_icon_view_item_invalidate_size (GtkIconViewItem *item)
{
  item->cell_area.width = -1;
  item->cell_area.height = -1;
}

static void
gtk_icon_view_paint_item (GtkIconView     *icon_view,
                          cairo_t         *cr,
                          GtkIconViewItem *item,
                          gint             x,
                          gint             y,
                          gboolean         draw_focus)
{
  GdkRectangle cell_area;
  GtkStateFlags state = 0;
  GtkCellRendererState flags = 0;
  GtkStyleContext *style_context;
  GtkWidget *widget = GTK_WIDGET (icon_view);
  GtkIconViewPrivate *priv = icon_view->priv;
  GtkCellAreaContext *context;

  if (priv->model == NULL || item->cell_area.width <= 0 || item->cell_area.height <= 0)
    return;

  _gtk_icon_view_set_cell_data (icon_view, item);

  style_context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_save (style_context);
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_CELL);

  state &= ~(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_PRELIGHT);

  if ((state & GTK_STATE_FLAG_FOCUSED) &&
      item == icon_view->priv->cursor_item)
    flags |= GTK_CELL_RENDERER_FOCUSED;

  if (item->selected)
    {
      state |= GTK_STATE_FLAG_SELECTED;
      flags |= GTK_CELL_RENDERER_SELECTED;
    }

  if (item->prelight)
    {
      state |= GTK_STATE_FLAG_PRELIGHT;
      flags |= GTK_CELL_RENDERER_PRELIT;
    }

  gtk_style_context_set_state (style_context, state);

  if (item->selected)
    {
      gtk_render_background (style_context, cr,
                             x - icon_view->priv->item_padding,
                             y - icon_view->priv->item_padding,
                             item->cell_area.width  + icon_view->priv->item_padding * 2,
                             item->cell_area.height + icon_view->priv->item_padding * 2);
      gtk_render_frame (style_context, cr,
                        x - icon_view->priv->item_padding,
                        y - icon_view->priv->item_padding,
                        item->cell_area.width  + icon_view->priv->item_padding * 2,
                        item->cell_area.height + icon_view->priv->item_padding * 2);
    }

  cell_area.x      = x;
  cell_area.y      = y;
  cell_area.width  = item->cell_area.width;
  cell_area.height = item->cell_area.height;

  context = g_ptr_array_index (priv->row_contexts, item->row);
  gtk_cell_area_render (priv->cell_area, context,
                        widget, cr, &cell_area, &cell_area, flags,
                        draw_focus);

  gtk_style_context_restore (style_context);
}

static void
gtk_icon_view_paint_rubberband (GtkIconView     *icon_view,
				cairo_t         *cr)
{
  GtkStyleContext *context;
  GdkRectangle rect;

  cairo_save (cr);

  rect.x = MIN (icon_view->priv->rubberband_x1, icon_view->priv->rubberband_x2);
  rect.y = MIN (icon_view->priv->rubberband_y1, icon_view->priv->rubberband_y2);
  rect.width = ABS (icon_view->priv->rubberband_x1 - icon_view->priv->rubberband_x2) + 1;
  rect.height = ABS (icon_view->priv->rubberband_y1 - icon_view->priv->rubberband_y2) + 1;

  context = gtk_widget_get_style_context (GTK_WIDGET (icon_view));

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_RUBBERBAND);

  gdk_cairo_rectangle (cr, &rect);
  cairo_clip (cr);

  gtk_render_background (context, cr,
                         rect.x, rect.y,
                         rect.width, rect.height);
  gtk_render_frame (context, cr,
                    rect.x, rect.y,
                    rect.width, rect.height);

  gtk_style_context_restore (context);
  cairo_restore (cr);
}

static void
gtk_icon_view_queue_draw_path (GtkIconView *icon_view,
			       GtkTreePath *path)
{
  GList *l;
  gint index;

  index = gtk_tree_path_get_indices (path)[0];

  for (l = icon_view->priv->items; l; l = l->next) 
    {
      GtkIconViewItem *item = l->data;

      if (item->index == index)
	{
	  gtk_icon_view_queue_draw_item (icon_view, item);
	  break;
	}
    }
}

static void
gtk_icon_view_queue_draw_item (GtkIconView     *icon_view,
			       GtkIconViewItem *item)
{
  GdkRectangle  rect;
  GdkRectangle *item_area = &item->cell_area;

  rect.x      = item_area->x - icon_view->priv->item_padding;
  rect.y      = item_area->y - icon_view->priv->item_padding;
  rect.width  = item_area->width  + icon_view->priv->item_padding * 2;
  rect.height = item_area->height + icon_view->priv->item_padding * 2;

  if (icon_view->priv->bin_window)
    gdk_window_invalidate_rect (icon_view->priv->bin_window, &rect, TRUE);
}

void
_gtk_icon_view_set_cursor_item (GtkIconView     *icon_view,
                                GtkIconViewItem *item,
                                GtkCellRenderer *cursor_cell)
{
  AtkObject *obj;
  AtkObject *item_obj;
  AtkObject *cursor_item_obj;

  /* When hitting this path from keynav, the focus cell is
   * already set, we dont need to notify the atk object
   * but we still need to queue the draw here (in the case
   * that the focus cell changes but not the cursor item).
   */
  gtk_icon_view_queue_draw_item (icon_view, item);

  if (icon_view->priv->cursor_item == item &&
      (cursor_cell == NULL || cursor_cell == gtk_cell_area_get_focus_cell (icon_view->priv->cell_area)))
    return;

  obj = gtk_widget_get_accessible (GTK_WIDGET (icon_view));
  if (icon_view->priv->cursor_item != NULL)
    {
      gtk_icon_view_queue_draw_item (icon_view, icon_view->priv->cursor_item);
      if (obj != NULL)
        {
          cursor_item_obj = atk_object_ref_accessible_child (obj, icon_view->priv->cursor_item->index);
          if (cursor_item_obj != NULL)
            atk_object_notify_state_change (cursor_item_obj, ATK_STATE_FOCUSED, FALSE);
        }
    }
  icon_view->priv->cursor_item = item;

  if (cursor_cell)
    gtk_cell_area_set_focus_cell (icon_view->priv->cell_area, cursor_cell);
  else
    {
      /* Make sure there is a cell in focus initially */
      if (!gtk_cell_area_get_focus_cell (icon_view->priv->cell_area))
	gtk_cell_area_focus (icon_view->priv->cell_area, GTK_DIR_TAB_FORWARD);
    }
  
  /* Notify that accessible focus object has changed */
  item_obj = atk_object_ref_accessible_child (obj, item->index);

  if (item_obj != NULL)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      atk_focus_tracker_notify (item_obj);
      G_GNUC_END_IGNORE_DEPRECATIONS;
      atk_object_notify_state_change (item_obj, ATK_STATE_FOCUSED, TRUE);
      g_object_unref (item_obj); 
    }
}


static GtkIconViewItem *
gtk_icon_view_item_new (void)
{
  GtkIconViewItem *item;

  item = g_slice_new0 (GtkIconViewItem);

  item->cell_area.width  = -1;
  item->cell_area.height = -1;
  
  return item;
}

static void
gtk_icon_view_item_free (GtkIconViewItem *item)
{
  g_return_if_fail (item != NULL);

  g_slice_free (GtkIconViewItem, item);
}

GtkIconViewItem *
_gtk_icon_view_get_item_at_coords (GtkIconView          *icon_view,
                                   gint                  x,
                                   gint                  y,
                                   gboolean              only_in_cell,
                                   GtkCellRenderer     **cell_at_pos)
{
  GList *items;

  if (cell_at_pos)
    *cell_at_pos = NULL;

  for (items = icon_view->priv->items; items; items = items->next)
    {
      GtkIconViewItem *item = items->data;
      GdkRectangle    *item_area = &item->cell_area;

      if (x >= item_area->x - icon_view->priv->column_spacing/2 && 
	  x <= item_area->x + item_area->width + icon_view->priv->column_spacing/2 &&
	  y >= item_area->y - icon_view->priv->row_spacing/2 && 
	  y <= item_area->y + item_area->height + icon_view->priv->row_spacing/2)
	{
	  if (only_in_cell || cell_at_pos)
	    {
	      GtkCellRenderer *cell = NULL;
	      GtkCellAreaContext *context;

	      context = g_ptr_array_index (icon_view->priv->row_contexts, item->row);
	      _gtk_icon_view_set_cell_data (icon_view, item);

	      if (x >= item_area->x && x <= item_area->x + item_area->width &&
		  y >= item_area->y && y <= item_area->y + item_area->height)
		cell = gtk_cell_area_get_cell_at_position (icon_view->priv->cell_area, context,
							   GTK_WIDGET (icon_view),
							   item_area,
							   x, y, NULL);

	      if (cell_at_pos)
		*cell_at_pos = cell;

	      if (only_in_cell)
		return cell != NULL ? item : NULL;
	      else
		return item;
	    }
	  return item;
	}
    }
  return NULL;
}

void
_gtk_icon_view_select_item (GtkIconView      *icon_view,
                            GtkIconViewItem  *item)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  g_return_if_fail (item != NULL);

  if (item->selected)
    return;
  
  if (icon_view->priv->selection_mode == GTK_SELECTION_NONE)
    return;
  else if (icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    gtk_icon_view_unselect_all_internal (icon_view);

  item->selected = TRUE;

  gtk_icon_view_item_selected_changed (icon_view, item);
  g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);

  gtk_icon_view_queue_draw_item (icon_view, item);
}


void
_gtk_icon_view_unselect_item (GtkIconView      *icon_view,
                              GtkIconViewItem  *item)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  g_return_if_fail (item != NULL);

  if (!item->selected)
    return;
  
  if (icon_view->priv->selection_mode == GTK_SELECTION_NONE ||
      icon_view->priv->selection_mode == GTK_SELECTION_BROWSE)
    return;
  
  item->selected = FALSE;

  gtk_icon_view_item_selected_changed (icon_view, item);
  g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);

  gtk_icon_view_queue_draw_item (icon_view, item);
}

static void
verify_items (GtkIconView *icon_view)
{
  GList *items;
  int i = 0;

  for (items = icon_view->priv->items; items; items = items->next)
    {
      GtkIconViewItem *item = items->data;

      if (item->index != i)
	g_error ("List item does not match its index: "
		 "item index %d and list index %d\n", item->index, i);

      i++;
    }
}

static void
gtk_icon_view_row_changed (GtkTreeModel *model,
                           GtkTreePath  *path,
                           GtkTreeIter  *iter,
                           gpointer      data)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (data);

  /* ignore changes in branches */
  if (gtk_tree_path_get_depth (path) > 1)
    return;

  /* An icon view subclass might add it's own model and populate
   * things at init() time instead of waiting for the constructor() 
   * to be called 
   */
  if (icon_view->priv->cell_area)
    gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

  /* Here we can use a "grow-only" strategy for optimization
   * and only invalidate a single item and queue a relayout
   * instead of invalidating the whole thing.
   *
   * For now GtkIconView still cant deal with huge models
   * so just invalidate the whole thing when the model
   * changes.
   */
  gtk_icon_view_invalidate_sizes (icon_view);

  verify_items (icon_view);
}

static void
gtk_icon_view_row_inserted (GtkTreeModel *model,
			    GtkTreePath  *path,
			    GtkTreeIter  *iter,
			    gpointer      data)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (data);
  gint index;
  GtkIconViewItem *item;
  GList *list;

  /* ignore changes in branches */
  if (gtk_tree_path_get_depth (path) > 1)
    return;

  index = gtk_tree_path_get_indices(path)[0];

  item = gtk_icon_view_item_new ();

  item->index = index;

  /* FIXME: We can be more efficient here,
     we can store a tail pointer and use that when
     appending (which is a rather common operation)
  */
  icon_view->priv->items = g_list_insert (icon_view->priv->items,
					 item, index);
  
  list = g_list_nth (icon_view->priv->items, index + 1);
  for (; list; list = list->next)
    {
      item = list->data;

      item->index++;
    }
    
  verify_items (icon_view);

  gtk_widget_queue_resize (GTK_WIDGET (icon_view));
}

static void
gtk_icon_view_row_deleted (GtkTreeModel *model,
			   GtkTreePath  *path,
			   gpointer      data)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (data);
  gint index;
  GtkIconViewItem *item;
  GList *list, *next;
  gboolean emit = FALSE;

  /* ignore changes in branches */
  if (gtk_tree_path_get_depth (path) > 1)
    return;

  index = gtk_tree_path_get_indices(path)[0];

  list = g_list_nth (icon_view->priv->items, index);
  item = list->data;

  if (icon_view->priv->cell_area)
    gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

  if (item == icon_view->priv->anchor_item)
    icon_view->priv->anchor_item = NULL;

  if (item == icon_view->priv->cursor_item)
    icon_view->priv->cursor_item = NULL;

  if (item == icon_view->priv->last_prelight)
    icon_view->priv->last_prelight = NULL;

  if (item->selected)
    emit = TRUE;
  
  gtk_icon_view_item_free (item);

  for (next = list->next; next; next = next->next)
    {
      item = next->data;

      item->index--;
    }
  
  icon_view->priv->items = g_list_delete_link (icon_view->priv->items, list);

  verify_items (icon_view);  
  
  gtk_widget_queue_resize (GTK_WIDGET (icon_view));

  if (emit)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}

static void
gtk_icon_view_rows_reordered (GtkTreeModel *model,
			      GtkTreePath  *parent,
			      GtkTreeIter  *iter,
			      gint         *new_order,
			      gpointer      data)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (data);
  int i;
  int length;
  GList *items = NULL, *list;
  GtkIconViewItem **item_array;
  gint *order;

  /* ignore changes in branches */
  if (iter != NULL)
    return;

  if (icon_view->priv->cell_area)
    gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

  length = gtk_tree_model_iter_n_children (model, NULL);

  order = g_new (gint, length);
  for (i = 0; i < length; i++)
    order [new_order[i]] = i;

  item_array = g_new (GtkIconViewItem *, length);
  for (i = 0, list = icon_view->priv->items; list != NULL; list = list->next, i++)
    item_array[order[i]] = list->data;
  g_free (order);

  for (i = length - 1; i >= 0; i--)
    {
      item_array[i]->index = i;
      items = g_list_prepend (items, item_array[i]);
    }
  
  g_free (item_array);
  g_list_free (icon_view->priv->items);
  icon_view->priv->items = items;

  gtk_widget_queue_resize (GTK_WIDGET (icon_view));

  verify_items (icon_view);  
}

static void
gtk_icon_view_build_items (GtkIconView *icon_view)
{
  GtkTreeIter iter;
  int i;
  GList *items = NULL;

  if (!gtk_tree_model_get_iter_first (icon_view->priv->model,
				      &iter))
    return;

  i = 0;
  
  do
    {
      GtkIconViewItem *item = gtk_icon_view_item_new ();

      item->index = i;
      
      i++;

      items = g_list_prepend (items, item);
      
    } while (gtk_tree_model_iter_next (icon_view->priv->model, &iter));

  icon_view->priv->items = g_list_reverse (items);
}

static void
gtk_icon_view_add_move_binding (GtkBindingSet  *binding_set,
				guint           keyval,
				guint           modmask,
				GtkMovementStep step,
				gint            count)
{
  
  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
                                I_("move-cursor"), 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);

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

static gboolean
gtk_icon_view_real_move_cursor (GtkIconView     *icon_view,
				GtkMovementStep  step,
				gint             count)
{
  GdkModifierType state;

  g_return_val_if_fail (GTK_ICON_VIEW (icon_view), FALSE);
  g_return_val_if_fail (step == GTK_MOVEMENT_LOGICAL_POSITIONS ||
			step == GTK_MOVEMENT_VISUAL_POSITIONS ||
			step == GTK_MOVEMENT_DISPLAY_LINES ||
			step == GTK_MOVEMENT_PAGES ||
			step == GTK_MOVEMENT_BUFFER_ENDS, FALSE);

  if (!gtk_widget_has_focus (GTK_WIDGET (icon_view)))
    return FALSE;

  gtk_cell_area_stop_editing (icon_view->priv->cell_area, FALSE);
  gtk_widget_grab_focus (GTK_WIDGET (icon_view));

  if (gtk_get_current_event_state (&state))
    {
      GdkModifierType extend_mod_mask;
      GdkModifierType modify_mod_mask;

      extend_mod_mask =
        gtk_widget_get_modifier_mask (GTK_WIDGET (icon_view),
                                      GDK_MODIFIER_INTENT_EXTEND_SELECTION);
      modify_mod_mask =
        gtk_widget_get_modifier_mask (GTK_WIDGET (icon_view),
                                      GDK_MODIFIER_INTENT_MODIFY_SELECTION);

      if ((state & modify_mod_mask) == modify_mod_mask)
        icon_view->priv->modify_selection_pressed = TRUE;
      if ((state & extend_mod_mask) == extend_mod_mask)
        icon_view->priv->extend_selection_pressed = TRUE;
    }
  /* else we assume not pressed */

  switch (step)
    {
    case GTK_MOVEMENT_LOGICAL_POSITIONS:
    case GTK_MOVEMENT_VISUAL_POSITIONS:
      gtk_icon_view_move_cursor_left_right (icon_view, count);
      break;
    case GTK_MOVEMENT_DISPLAY_LINES:
      gtk_icon_view_move_cursor_up_down (icon_view, count);
      break;
    case GTK_MOVEMENT_PAGES:
      gtk_icon_view_move_cursor_page_up_down (icon_view, count);
      break;
    case GTK_MOVEMENT_BUFFER_ENDS:
      gtk_icon_view_move_cursor_start_end (icon_view, count);
      break;
    default:
      g_assert_not_reached ();
    }

  icon_view->priv->modify_selection_pressed = FALSE;
  icon_view->priv->extend_selection_pressed = FALSE;

  icon_view->priv->draw_focus = TRUE;

  return TRUE;
}

static GtkIconViewItem *
find_item (GtkIconView     *icon_view,
	   GtkIconViewItem *current,
	   gint             row_ofs,
	   gint             col_ofs)
{
  gint row, col;
  GList *items;
  GtkIconViewItem *item;

  /* FIXME: this could be more efficient 
   */
  row = current->row + row_ofs;
  col = current->col + col_ofs;

  for (items = icon_view->priv->items; items; items = items->next)
    {
      item = items->data;
      if (item->row == row && item->col == col)
	return item;
    }
  
  return NULL;
}

static GtkIconViewItem *
find_item_page_up_down (GtkIconView     *icon_view,
			GtkIconViewItem *current,
			gint             count)
{
  GList *item, *next;
  gint y, col;
  
  col = current->col;
  y = current->cell_area.y + count * gtk_adjustment_get_page_size (icon_view->priv->vadjustment);

  item = g_list_find (icon_view->priv->items, current);
  if (count > 0)
    {
      while (item)
	{
	  for (next = item->next; next; next = next->next)
	    {
	      if (((GtkIconViewItem *)next->data)->col == col)
		break;
	    }
	  if (!next || ((GtkIconViewItem *)next->data)->cell_area.y > y)
	    break;

	  item = next;
	}
    }
  else 
    {
      while (item)
	{
	  for (next = item->prev; next; next = next->prev)
	    {
	      if (((GtkIconViewItem *)next->data)->col == col)
		break;
	    }
	  if (!next || ((GtkIconViewItem *)next->data)->cell_area.y < y)
	    break;

	  item = next;
	}
    }

  if (item)
    return item->data;

  return NULL;
}

static gboolean
gtk_icon_view_select_all_between (GtkIconView     *icon_view,
				  GtkIconViewItem *anchor,
				  GtkIconViewItem *cursor)
{
  GList *items;
  GtkIconViewItem *item;
  gint row1, row2, col1, col2;
  gboolean dirty = FALSE;
  
  if (anchor->row < cursor->row)
    {
      row1 = anchor->row;
      row2 = cursor->row;
    }
  else
    {
      row1 = cursor->row;
      row2 = anchor->row;
    }

  if (anchor->col < cursor->col)
    {
      col1 = anchor->col;
      col2 = cursor->col;
    }
  else
    {
      col1 = cursor->col;
      col2 = anchor->col;
    }

  for (items = icon_view->priv->items; items; items = items->next)
    {
      item = items->data;

      if (row1 <= item->row && item->row <= row2 &&
	  col1 <= item->col && item->col <= col2)
	{
	  if (!item->selected)
	    {
	      dirty = TRUE;
	      item->selected = TRUE;
	      gtk_icon_view_item_selected_changed (icon_view, item);
	    }
	  gtk_icon_view_queue_draw_item (icon_view, item);
	}
    }

  return dirty;
}

static void 
gtk_icon_view_move_cursor_up_down (GtkIconView *icon_view,
				   gint         count)
{
  GtkIconViewItem *item;
  GtkCellRenderer *cell = NULL;
  gboolean dirty = FALSE;
  gint step;
  GtkDirectionType direction;

  if (!gtk_widget_has_focus (GTK_WIDGET (icon_view)))
    return;

  direction = count < 0 ? GTK_DIR_UP : GTK_DIR_DOWN;

  if (!icon_view->priv->cursor_item)
    {
      GList *list;

      if (count > 0)
	list = icon_view->priv->items;
      else
	list = g_list_last (icon_view->priv->items);

      if (list)
        {
          item = list->data;

          /* Give focus to the first cell initially */
          _gtk_icon_view_set_cell_data (icon_view, item);
          gtk_cell_area_focus (icon_view->priv->cell_area, direction);
        }
      else
        {
          item = NULL;
        }
    }
  else
    {
      item = icon_view->priv->cursor_item;
      step = count > 0 ? 1 : -1;      

      /* Save the current focus cell in case we hit the edge */
      cell = gtk_cell_area_get_focus_cell (icon_view->priv->cell_area);

      while (item)
	{
	  _gtk_icon_view_set_cell_data (icon_view, item);

	  if (gtk_cell_area_focus (icon_view->priv->cell_area, direction))
	    break;

	  item = find_item (icon_view, item, step, 0);
	}
    }

  if (!item)
    {
      if (!gtk_widget_keynav_failed (GTK_WIDGET (icon_view), direction))
        {
          GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (icon_view));
          if (toplevel)
            gtk_widget_child_focus (toplevel,
                                    direction == GTK_DIR_UP ?
                                    GTK_DIR_TAB_BACKWARD :
                                    GTK_DIR_TAB_FORWARD);

        }

      gtk_cell_area_set_focus_cell (icon_view->priv->cell_area, cell);
      return;
    }

  if (icon_view->priv->modify_selection_pressed ||
      !icon_view->priv->extend_selection_pressed ||
      !icon_view->priv->anchor_item ||
      icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_view->priv->anchor_item = item;

  cell = gtk_cell_area_get_focus_cell (icon_view->priv->cell_area);
  _gtk_icon_view_set_cursor_item (icon_view, item, cell);

  if (!icon_view->priv->modify_selection_pressed &&
      icon_view->priv->selection_mode != GTK_SELECTION_NONE)
    {
      dirty = gtk_icon_view_unselect_all_internal (icon_view);
      dirty = gtk_icon_view_select_all_between (icon_view, 
						icon_view->priv->anchor_item,
						item) || dirty;
    }

  gtk_icon_view_scroll_to_item (icon_view, item);

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}

static void 
gtk_icon_view_move_cursor_page_up_down (GtkIconView *icon_view,
					gint         count)
{
  GtkIconViewItem *item;
  gboolean dirty = FALSE;
  
  if (!gtk_widget_has_focus (GTK_WIDGET (icon_view)))
    return;
  
  if (!icon_view->priv->cursor_item)
    {
      GList *list;

      if (count > 0)
	list = icon_view->priv->items;
      else
	list = g_list_last (icon_view->priv->items);

      item = list ? list->data : NULL;
    }
  else
    item = find_item_page_up_down (icon_view, 
				   icon_view->priv->cursor_item,
				   count);

  if (item == icon_view->priv->cursor_item)
    gtk_widget_error_bell (GTK_WIDGET (icon_view));

  if (!item)
    return;

  if (icon_view->priv->modify_selection_pressed ||
      !icon_view->priv->extend_selection_pressed ||
      !icon_view->priv->anchor_item ||
      icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_view->priv->anchor_item = item;

  _gtk_icon_view_set_cursor_item (icon_view, item, NULL);

  if (!icon_view->priv->modify_selection_pressed &&
      icon_view->priv->selection_mode != GTK_SELECTION_NONE)
    {
      dirty = gtk_icon_view_unselect_all_internal (icon_view);
      dirty = gtk_icon_view_select_all_between (icon_view, 
						icon_view->priv->anchor_item,
						item) || dirty;
    }

  gtk_icon_view_scroll_to_item (icon_view, item);

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);  
}

static void 
gtk_icon_view_move_cursor_left_right (GtkIconView *icon_view,
				      gint         count)
{
  GtkIconViewItem *item;
  GtkCellRenderer *cell = NULL;
  gboolean dirty = FALSE;
  gint step;
  GtkDirectionType direction;

  if (!gtk_widget_has_focus (GTK_WIDGET (icon_view)))
    return;

  direction = count < 0 ? GTK_DIR_LEFT : GTK_DIR_RIGHT;

  if (!icon_view->priv->cursor_item)
    {
      GList *list;

      if (count > 0)
	list = icon_view->priv->items;
      else
	list = g_list_last (icon_view->priv->items);

      if (list)
        {
          item = list->data;

          /* Give focus to the first cell initially */
          _gtk_icon_view_set_cell_data (icon_view, item);
          gtk_cell_area_focus (icon_view->priv->cell_area, direction);
        }
      else
        {
          item = NULL;
        }
    }
  else
    {
      item = icon_view->priv->cursor_item;
      step = count > 0 ? 1 : -1;

      /* Save the current focus cell in case we hit the edge */
      cell = gtk_cell_area_get_focus_cell (icon_view->priv->cell_area);

      while (item)
	{
	  _gtk_icon_view_set_cell_data (icon_view, item);

	  if (gtk_cell_area_focus (icon_view->priv->cell_area, direction))
	    break;
	  
	  item = find_item (icon_view, item, 0, step);
	}
    }

  if (!item)
    {
      if (!gtk_widget_keynav_failed (GTK_WIDGET (icon_view), direction))
        {
          GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (icon_view));
          if (toplevel)
            gtk_widget_child_focus (toplevel,
                                    direction == GTK_DIR_LEFT ?
                                    GTK_DIR_TAB_BACKWARD :
                                    GTK_DIR_TAB_FORWARD);

        }

      gtk_cell_area_set_focus_cell (icon_view->priv->cell_area, cell);
      return;
    }

  if (icon_view->priv->modify_selection_pressed ||
      !icon_view->priv->extend_selection_pressed ||
      !icon_view->priv->anchor_item ||
      icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_view->priv->anchor_item = item;

  cell = gtk_cell_area_get_focus_cell (icon_view->priv->cell_area);
  _gtk_icon_view_set_cursor_item (icon_view, item, cell);

  if (!icon_view->priv->modify_selection_pressed &&
      icon_view->priv->selection_mode != GTK_SELECTION_NONE)
    {
      dirty = gtk_icon_view_unselect_all_internal (icon_view);
      dirty = gtk_icon_view_select_all_between (icon_view, 
						icon_view->priv->anchor_item,
						item) || dirty;
    }

  gtk_icon_view_scroll_to_item (icon_view, item);

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}

static void 
gtk_icon_view_move_cursor_start_end (GtkIconView *icon_view,
				     gint         count)
{
  GtkIconViewItem *item;
  GList *list;
  gboolean dirty = FALSE;
  
  if (!gtk_widget_has_focus (GTK_WIDGET (icon_view)))
    return;
  
  if (count < 0)
    list = icon_view->priv->items;
  else
    list = g_list_last (icon_view->priv->items);
  
  item = list ? list->data : NULL;

  if (item == icon_view->priv->cursor_item)
    gtk_widget_error_bell (GTK_WIDGET (icon_view));

  if (!item)
    return;

  if (icon_view->priv->modify_selection_pressed ||
      !icon_view->priv->extend_selection_pressed ||
      !icon_view->priv->anchor_item ||
      icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_view->priv->anchor_item = item;

  _gtk_icon_view_set_cursor_item (icon_view, item, NULL);

  if (!icon_view->priv->modify_selection_pressed &&
      icon_view->priv->selection_mode != GTK_SELECTION_NONE)
    {
      dirty = gtk_icon_view_unselect_all_internal (icon_view);
      dirty = gtk_icon_view_select_all_between (icon_view, 
						icon_view->priv->anchor_item,
						item) || dirty;
    }

  gtk_icon_view_scroll_to_item (icon_view, item);

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}

/**
 * gtk_icon_view_scroll_to_path:
 * @icon_view: A #GtkIconView.
 * @path: The path of the item to move to.
 * @use_align: whether to use alignment arguments, or %FALSE.
 * @row_align: The vertical alignment of the item specified by @path.
 * @col_align: The horizontal alignment of the item specified by @path.
 *
 * Moves the alignments of @icon_view to the position specified by @path.  
 * @row_align determines where the row is placed, and @col_align determines 
 * where @column is placed.  Both are expected to be between 0.0 and 1.0. 
 * 0.0 means left/top alignment, 1.0 means right/bottom alignment, 0.5 means 
 * center.
 *
 * If @use_align is %FALSE, then the alignment arguments are ignored, and the
 * tree does the minimum amount of work to scroll the item onto the screen.
 * This means that the item will be scrolled to the edge closest to its current
 * position.  If the item is currently visible on the screen, nothing is done.
 *
 * This function only works if the model is set, and @path is a valid row on 
 * the model. If the model changes before the @icon_view is realized, the 
 * centered path will be modified to reflect this change.
 *
 * Since: 2.8
 **/
void
gtk_icon_view_scroll_to_path (GtkIconView *icon_view,
			      GtkTreePath *path,
			      gboolean     use_align,
			      gfloat       row_align,
			      gfloat       col_align)
{
  GtkIconViewItem *item = NULL;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  g_return_if_fail (path != NULL);
  g_return_if_fail (row_align >= 0.0 && row_align <= 1.0);
  g_return_if_fail (col_align >= 0.0 && col_align <= 1.0);

  widget = GTK_WIDGET (icon_view);

  if (gtk_tree_path_get_depth (path) > 0)
    item = g_list_nth_data (icon_view->priv->items,
			    gtk_tree_path_get_indices(path)[0]);
  
  if (!item || item->cell_area.width < 0 ||
      !gtk_widget_get_realized (widget))
    {
      if (icon_view->priv->scroll_to_path)
	gtk_tree_row_reference_free (icon_view->priv->scroll_to_path);

      icon_view->priv->scroll_to_path = NULL;

      if (path)
	icon_view->priv->scroll_to_path = gtk_tree_row_reference_new_proxy (G_OBJECT (icon_view), icon_view->priv->model, path);

      icon_view->priv->scroll_to_use_align = use_align;
      icon_view->priv->scroll_to_row_align = row_align;
      icon_view->priv->scroll_to_col_align = col_align;

      return;
    }

  if (use_align)
    {
      GtkAllocation allocation;
      gint x, y;
      gfloat offset;
      GdkRectangle item_area = 
	{ 
	  item->cell_area.x - icon_view->priv->item_padding, 
	  item->cell_area.y - icon_view->priv->item_padding, 
	  item->cell_area.width  + icon_view->priv->item_padding * 2, 
	  item->cell_area.height + icon_view->priv->item_padding * 2 
	};

      gdk_window_get_position (icon_view->priv->bin_window, &x, &y);

      gtk_widget_get_allocation (widget, &allocation);

      offset = y + item_area.y - row_align * (allocation.height - item_area.height);

      gtk_adjustment_set_value (icon_view->priv->vadjustment,
                                gtk_adjustment_get_value (icon_view->priv->vadjustment) + offset);

      offset = x + item_area.x - col_align * (allocation.width - item_area.width);

      gtk_adjustment_set_value (icon_view->priv->hadjustment,
                                gtk_adjustment_get_value (icon_view->priv->hadjustment) + offset);

      gtk_adjustment_changed (icon_view->priv->hadjustment);
      gtk_adjustment_changed (icon_view->priv->vadjustment);
    }
  else
    gtk_icon_view_scroll_to_item (icon_view, item);
}


static void
gtk_icon_view_scroll_to_item (GtkIconView     *icon_view,
                              GtkIconViewItem *item)
{
  GtkIconViewPrivate *priv = icon_view->priv;
  GtkWidget *widget = GTK_WIDGET (icon_view);
  GtkAdjustment *hadj, *vadj;
  GtkAllocation allocation;
  gint x, y;
  GdkRectangle item_area;

  item_area.x = item->cell_area.x - priv->item_padding;
  item_area.y = item->cell_area.y - priv->item_padding;
  item_area.width = item->cell_area.width  + priv->item_padding * 2;
  item_area.height = item->cell_area.height + priv->item_padding * 2;

  gdk_window_get_position (icon_view->priv->bin_window, &x, &y);
  gtk_widget_get_allocation (widget, &allocation);

  hadj = icon_view->priv->hadjustment;
  vadj = icon_view->priv->vadjustment;

  if (y + item_area.y < 0)
    gtk_adjustment_animate_to_value (vadj,
                                     gtk_adjustment_get_value (vadj)
                                     + y + item_area.y);
  else if (y + item_area.y + item_area.height > allocation.height)
    gtk_adjustment_animate_to_value (vadj,
                                     gtk_adjustment_get_value (vadj)
                                     + y + item_area.y + item_area.height - allocation.height);

  if (x + item_area.x < 0)
    gtk_adjustment_animate_to_value (hadj,
                                     gtk_adjustment_get_value (hadj)
                                     + x + item_area.x);
  else if (x + item_area.x + item_area.width > allocation.width)
    gtk_adjustment_animate_to_value (hadj,
                                     gtk_adjustment_get_value (hadj)
                                     + x + item_area.x + item_area.width - allocation.width);

  gtk_adjustment_changed (hadj);
  gtk_adjustment_changed (vadj);
}

/* GtkCellLayout implementation */

static void
gtk_icon_view_ensure_cell_area (GtkIconView *icon_view,
                                GtkCellArea *cell_area)
{
  GtkIconViewPrivate *priv = icon_view->priv;

  if (priv->cell_area)
    return;

  if (cell_area)
    priv->cell_area = cell_area;
  else
    priv->cell_area = gtk_cell_area_box_new ();

  g_object_ref_sink (priv->cell_area);

  if (GTK_IS_ORIENTABLE (priv->cell_area))
    gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->cell_area), priv->item_orientation);

  priv->cell_area_context = gtk_cell_area_create_context (priv->cell_area);

  priv->add_editable_id =
    g_signal_connect (priv->cell_area, "add-editable",
                      G_CALLBACK (gtk_icon_view_add_editable), icon_view);
  priv->remove_editable_id =
    g_signal_connect (priv->cell_area, "remove-editable",
                      G_CALLBACK (gtk_icon_view_remove_editable), icon_view);

  update_text_cell (icon_view);
  update_pixbuf_cell (icon_view);
}

static GtkCellArea *
gtk_icon_view_cell_layout_get_area (GtkCellLayout *cell_layout)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (cell_layout);
  GtkIconViewPrivate *priv = icon_view->priv;

  if (G_UNLIKELY (!priv->cell_area))
    gtk_icon_view_ensure_cell_area (icon_view, NULL);

  return icon_view->priv->cell_area;
}

void
_gtk_icon_view_set_cell_data (GtkIconView     *icon_view,
			      GtkIconViewItem *item)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  path = gtk_tree_path_new_from_indices (item->index, -1);
  if (!gtk_tree_model_get_iter (icon_view->priv->model, &iter, path))
    return;
  gtk_tree_path_free (path);

  gtk_cell_area_apply_attributes (icon_view->priv->cell_area, 
				  icon_view->priv->model,
				  &iter, FALSE, FALSE);
}



/* Public API */


/**
 * gtk_icon_view_new:
 * 
 * Creates a new #GtkIconView widget
 * 
 * Returns: A newly created #GtkIconView widget
 *
 * Since: 2.6
 **/
GtkWidget *
gtk_icon_view_new (void)
{
  return g_object_new (GTK_TYPE_ICON_VIEW, NULL);
}

/**
 * gtk_icon_view_new_with_area:
 * @area: the #GtkCellArea to use to layout cells
 * 
 * Creates a new #GtkIconView widget using the
 * specified @area to layout cells inside the icons.
 * 
 * Returns: A newly created #GtkIconView widget
 *
 * Since: 3.0
 **/
GtkWidget *
gtk_icon_view_new_with_area (GtkCellArea *area)
{
  return g_object_new (GTK_TYPE_ICON_VIEW, "cell-area", area, NULL);
}

/**
 * gtk_icon_view_new_with_model:
 * @model: The model.
 * 
 * Creates a new #GtkIconView widget with the model @model.
 * 
 * Returns: A newly created #GtkIconView widget.
 *
 * Since: 2.6 
 **/
GtkWidget *
gtk_icon_view_new_with_model (GtkTreeModel *model)
{
  return g_object_new (GTK_TYPE_ICON_VIEW, "model", model, NULL);
}

/**
 * gtk_icon_view_convert_widget_to_bin_window_coords:
 * @icon_view: a #GtkIconView 
 * @wx: X coordinate relative to the widget
 * @wy: Y coordinate relative to the widget
 * @bx: (out): return location for bin_window X coordinate
 * @by: (out): return location for bin_window Y coordinate
 * 
 * Converts widget coordinates to coordinates for the bin_window,
 * as expected by e.g. gtk_icon_view_get_path_at_pos(). 
 *
 * Since: 2.12
 */
void
gtk_icon_view_convert_widget_to_bin_window_coords (GtkIconView *icon_view,
                                                   gint         wx,
                                                   gint         wy, 
                                                   gint        *bx,
                                                   gint        *by)
{
  gint x, y;

  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  if (icon_view->priv->bin_window) 
    gdk_window_get_position (icon_view->priv->bin_window, &x, &y);
  else
    x = y = 0;
 
  if (bx)
    *bx = wx - x;
  if (by)
    *by = wy - y;
}

/**
 * gtk_icon_view_get_path_at_pos:
 * @icon_view: A #GtkIconView.
 * @x: The x position to be identified
 * @y: The y position to be identified
 * 
 * Finds the path at the point (@x, @y), relative to bin_window coordinates.
 * See gtk_icon_view_get_item_at_pos(), if you are also interested in
 * the cell at the specified position. 
 * See gtk_icon_view_convert_widget_to_bin_window_coords() for converting
 * widget coordinates to bin_window coordinates.
 * 
 * Returns: The #GtkTreePath corresponding to the icon or %NULL
 * if no icon exists at that position.
 *
 * Since: 2.6 
 **/
GtkTreePath *
gtk_icon_view_get_path_at_pos (GtkIconView *icon_view,
			       gint         x,
			       gint         y)
{
  GtkIconViewItem *item;
  GtkTreePath *path;
  
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), NULL);

  item = _gtk_icon_view_get_item_at_coords (icon_view, x, y, TRUE, NULL);

  if (!item)
    return NULL;

  path = gtk_tree_path_new_from_indices (item->index, -1);

  return path;
}

/**
 * gtk_icon_view_get_item_at_pos:
 * @icon_view: A #GtkIconView.
 * @x: The x position to be identified
 * @y: The y position to be identified
 * @path: (out) (allow-none): Return location for the path, or %NULL
 * @cell: (out) (allow-none): Return location for the renderer
 *   responsible for the cell at (@x, @y), or %NULL
 * 
 * Finds the path at the point (@x, @y), relative to bin_window coordinates.
 * In contrast to gtk_icon_view_get_path_at_pos(), this function also 
 * obtains the cell at the specified position. The returned path should
 * be freed with gtk_tree_path_free().
 * See gtk_icon_view_convert_widget_to_bin_window_coords() for converting
 * widget coordinates to bin_window coordinates.
 * 
 * Returns: %TRUE if an item exists at the specified position
 *
 * Since: 2.8
 **/
gboolean 
gtk_icon_view_get_item_at_pos (GtkIconView      *icon_view,
			       gint              x,
			       gint              y,
			       GtkTreePath     **path,
			       GtkCellRenderer **cell)
{
  GtkIconViewItem *item;
  GtkCellRenderer *renderer = NULL;
  
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), FALSE);

  item = _gtk_icon_view_get_item_at_coords (icon_view, x, y, TRUE, &renderer);

  if (path != NULL)
    {
      if (item != NULL)
	*path = gtk_tree_path_new_from_indices (item->index, -1);
      else
	*path = NULL;
    }

  if (cell != NULL)
    *cell = renderer;

  return (item != NULL);
}

/**
 * gtk_icon_view_get_cell_rect:
 * @icon_view: a #GtkIconView
 * @path: a #GtkTreePath
 * @cell: (allow-none): a #GtkCellRenderer or %NULL
 * @rect: (out): rectangle to fill with cell rect
 *
 * Fills the bounding rectangle in widget coordinates for the cell specified by
 * @path and @cell. If @cell is %NULL the main cell area is used.
 *
 * This function is only valid if @icon_view is realized.
 *
 * Returns: %FALSE if there is no such item, %TRUE otherwise
 *
 * Since: 3.6
 */
gboolean
gtk_icon_view_get_cell_rect (GtkIconView     *icon_view,
                             GtkTreePath     *path,
                             GtkCellRenderer *cell,
                             GdkRectangle    *rect)
{
  GtkIconViewItem *item = NULL;
  gint x, y;

  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), FALSE);
  g_return_val_if_fail (cell == NULL || GTK_IS_CELL_RENDERER (cell), FALSE);

  if (gtk_tree_path_get_depth (path) > 0)
    item = g_list_nth_data (icon_view->priv->items,
			    gtk_tree_path_get_indices(path)[0]);

  if (!item)
    return FALSE;

  if (cell)
    {
      GtkCellAreaContext *context;

      context = g_ptr_array_index (icon_view->priv->row_contexts, item->row);
      _gtk_icon_view_set_cell_data (icon_view, item);
      gtk_cell_area_get_cell_allocation (icon_view->priv->cell_area, context,
					 GTK_WIDGET (icon_view),
					 cell, &item->cell_area, rect);
    }
  else
    {
      rect->x = item->cell_area.x - icon_view->priv->item_padding;
      rect->y = item->cell_area.y - icon_view->priv->item_padding;
      rect->width  = item->cell_area.width  + icon_view->priv->item_padding * 2;
      rect->height = item->cell_area.height + icon_view->priv->item_padding * 2;
    }

  if (icon_view->priv->bin_window)
    {
      gdk_window_get_position (icon_view->priv->bin_window, &x, &y);
      rect->x += x;
      rect->y += y;
    }

  return TRUE;
}

/**
 * gtk_icon_view_set_tooltip_item:
 * @icon_view: a #GtkIconView
 * @tooltip: a #GtkTooltip
 * @path: a #GtkTreePath
 * 
 * Sets the tip area of @tooltip to be the area covered by the item at @path.
 * See also gtk_icon_view_set_tooltip_column() for a simpler alternative.
 * See also gtk_tooltip_set_tip_area().
 * 
 * Since: 2.12
 */
void 
gtk_icon_view_set_tooltip_item (GtkIconView     *icon_view,
                                GtkTooltip      *tooltip,
                                GtkTreePath     *path)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_icon_view_set_tooltip_cell (icon_view, tooltip, path, NULL);
}

/**
 * gtk_icon_view_set_tooltip_cell:
 * @icon_view: a #GtkIconView
 * @tooltip: a #GtkTooltip
 * @path: a #GtkTreePath
 * @cell: (allow-none): a #GtkCellRenderer or %NULL
 *
 * Sets the tip area of @tooltip to the area which @cell occupies in
 * the item pointed to by @path. See also gtk_tooltip_set_tip_area().
 *
 * See also gtk_icon_view_set_tooltip_column() for a simpler alternative.
 *
 * Since: 2.12
 */
void
gtk_icon_view_set_tooltip_cell (GtkIconView     *icon_view,
                                GtkTooltip      *tooltip,
                                GtkTreePath     *path,
                                GtkCellRenderer *cell)
{
  GdkRectangle rect;

  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));
  g_return_if_fail (cell == NULL || GTK_IS_CELL_RENDERER (cell));

  if (!gtk_icon_view_get_cell_rect (icon_view, path, cell, &rect))
    return;

  gtk_tooltip_set_tip_area (tooltip, &rect);
}


/**
 * gtk_icon_view_get_tooltip_context:
 * @icon_view: an #GtkIconView
 * @x: (inout): the x coordinate (relative to widget coordinates)
 * @y: (inout): the y coordinate (relative to widget coordinates)
 * @keyboard_tip: whether this is a keyboard tooltip or not
 * @model: (out) (allow-none) (transfer none): a pointer to receive a
 *         #GtkTreeModel or %NULL
 * @path: (out) (allow-none): a pointer to receive a #GtkTreePath or %NULL
 * @iter: (out) (allow-none): a pointer to receive a #GtkTreeIter or %NULL
 *
 * This function is supposed to be used in a #GtkWidget::query-tooltip
 * signal handler for #GtkIconView.  The @x, @y and @keyboard_tip values
 * which are received in the signal handler, should be passed to this
 * function without modification.
 *
 * The return value indicates whether there is an icon view item at the given
 * coordinates (%TRUE) or not (%FALSE) for mouse tooltips. For keyboard
 * tooltips the item returned will be the cursor item. When %TRUE, then any of
 * @model, @path and @iter which have been provided will be set to point to
 * that row and the corresponding model. @x and @y will always be converted
 * to be relative to @icon_view’s bin_window if @keyboard_tooltip is %FALSE.
 *
 * Returns: whether or not the given tooltip context points to a item
 *
 * Since: 2.12
 */
gboolean
gtk_icon_view_get_tooltip_context (GtkIconView   *icon_view,
                                   gint          *x,
                                   gint          *y,
                                   gboolean       keyboard_tip,
                                   GtkTreeModel **model,
                                   GtkTreePath  **path,
                                   GtkTreeIter   *iter)
{
  GtkTreePath *tmppath = NULL;

  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), FALSE);
  g_return_val_if_fail (x != NULL, FALSE);
  g_return_val_if_fail (y != NULL, FALSE);

  if (keyboard_tip)
    {
      gtk_icon_view_get_cursor (icon_view, &tmppath, NULL);

      if (!tmppath)
        return FALSE;
    }
  else
    {
      gtk_icon_view_convert_widget_to_bin_window_coords (icon_view, *x, *y,
                                                         x, y);

      if (!gtk_icon_view_get_item_at_pos (icon_view, *x, *y, &tmppath, NULL))
        return FALSE;
    }

  if (model)
    *model = gtk_icon_view_get_model (icon_view);

  if (iter)
    gtk_tree_model_get_iter (gtk_icon_view_get_model (icon_view),
                             iter, tmppath);

  if (path)
    *path = tmppath;
  else
    gtk_tree_path_free (tmppath);

  return TRUE;
}

static gboolean
gtk_icon_view_set_tooltip_query_cb (GtkWidget  *widget,
                                    gint        x,
                                    gint        y,
                                    gboolean    keyboard_tip,
                                    GtkTooltip *tooltip,
                                    gpointer    data)
{
  gchar *str;
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkIconView *icon_view = GTK_ICON_VIEW (widget);

  if (!gtk_icon_view_get_tooltip_context (GTK_ICON_VIEW (widget),
                                          &x, &y,
                                          keyboard_tip,
                                          &model, &path, &iter))
    return FALSE;

  gtk_tree_model_get (model, &iter, icon_view->priv->tooltip_column, &str, -1);

  if (!str)
    {
      gtk_tree_path_free (path);
      return FALSE;
    }

  gtk_tooltip_set_markup (tooltip, str);
  gtk_icon_view_set_tooltip_item (icon_view, tooltip, path);

  gtk_tree_path_free (path);
  g_free (str);

  return TRUE;
}


/**
 * gtk_icon_view_set_tooltip_column:
 * @icon_view: a #GtkIconView
 * @column: an integer, which is a valid column number for @icon_view’s model
 *
 * If you only plan to have simple (text-only) tooltips on full items, you
 * can use this function to have #GtkIconView handle these automatically
 * for you. @column should be set to the column in @icon_view’s model
 * containing the tooltip texts, or -1 to disable this feature.
 *
 * When enabled, #GtkWidget:has-tooltip will be set to %TRUE and
 * @icon_view will connect a #GtkWidget::query-tooltip signal handler.
 *
 * Note that the signal handler sets the text with gtk_tooltip_set_markup(),
 * so &, <, etc have to be escaped in the text.
 *
 * Since: 2.12
 */
void
gtk_icon_view_set_tooltip_column (GtkIconView *icon_view,
                                  gint         column)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  if (column == icon_view->priv->tooltip_column)
    return;

  if (column == -1)
    {
      g_signal_handlers_disconnect_by_func (icon_view,
                                            gtk_icon_view_set_tooltip_query_cb,
                                            NULL);
      gtk_widget_set_has_tooltip (GTK_WIDGET (icon_view), FALSE);
    }
  else
    {
      if (icon_view->priv->tooltip_column == -1)
        {
          g_signal_connect (icon_view, "query-tooltip",
                            G_CALLBACK (gtk_icon_view_set_tooltip_query_cb), NULL);
          gtk_widget_set_has_tooltip (GTK_WIDGET (icon_view), TRUE);
        }
    }

  icon_view->priv->tooltip_column = column;
  g_object_notify (G_OBJECT (icon_view), "tooltip-column");
}

/**
 * gtk_icon_view_get_tooltip_column:
 * @icon_view: a #GtkIconView
 *
 * Returns the column of @icon_view’s model which is being used for
 * displaying tooltips on @icon_view’s rows.
 *
 * Returns: the index of the tooltip column that is currently being
 * used, or -1 if this is disabled.
 *
 * Since: 2.12
 */
gint
gtk_icon_view_get_tooltip_column (GtkIconView *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), 0);

  return icon_view->priv->tooltip_column;
}

/**
 * gtk_icon_view_get_visible_range:
 * @icon_view: A #GtkIconView
 * @start_path: (out) (allow-none): Return location for start of region,
 *              or %NULL
 * @end_path: (out) (allow-none): Return location for end of region, or %NULL
 * 
 * Sets @start_path and @end_path to be the first and last visible path. 
 * Note that there may be invisible paths in between.
 * 
 * Both paths should be freed with gtk_tree_path_free() after use.
 * 
 * Returns: %TRUE, if valid paths were placed in @start_path and @end_path
 *
 * Since: 2.8
 **/
gboolean
gtk_icon_view_get_visible_range (GtkIconView  *icon_view,
				 GtkTreePath **start_path,
				 GtkTreePath **end_path)
{
  gint start_index = -1;
  gint end_index = -1;
  GList *icons;

  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), FALSE);

  if (icon_view->priv->hadjustment == NULL ||
      icon_view->priv->vadjustment == NULL)
    return FALSE;

  if (start_path == NULL && end_path == NULL)
    return FALSE;
  
  for (icons = icon_view->priv->items; icons; icons = icons->next) 
    {
      GtkIconViewItem *item = icons->data;
      GdkRectangle    *item_area = &item->cell_area;

      if ((item_area->x + item_area->width >= (int)gtk_adjustment_get_value (icon_view->priv->hadjustment)) &&
	  (item_area->y + item_area->height >= (int)gtk_adjustment_get_value (icon_view->priv->vadjustment)) &&
	  (item_area->x <= 
	   (int) (gtk_adjustment_get_value (icon_view->priv->hadjustment) + 
		  gtk_adjustment_get_page_size (icon_view->priv->hadjustment))) &&
	  (item_area->y <= 
	   (int) (gtk_adjustment_get_value (icon_view->priv->vadjustment) + 
		  gtk_adjustment_get_page_size (icon_view->priv->vadjustment))))
	{
	  if (start_index == -1)
	    start_index = item->index;
	  end_index = item->index;
	}
    }

  if (start_path && start_index != -1)
    *start_path = gtk_tree_path_new_from_indices (start_index, -1);
  if (end_path && end_index != -1)
    *end_path = gtk_tree_path_new_from_indices (end_index, -1);
  
  return start_index != -1;
}

/**
 * gtk_icon_view_selected_foreach:
 * @icon_view: A #GtkIconView.
 * @func: (scope call): The function to call for each selected icon.
 * @data: User data to pass to the function.
 * 
 * Calls a function for each selected icon. Note that the model or
 * selection cannot be modified from within this function.
 *
 * Since: 2.6 
 **/
void
gtk_icon_view_selected_foreach (GtkIconView           *icon_view,
				GtkIconViewForeachFunc func,
				gpointer               data)
{
  GList *list;
  
  for (list = icon_view->priv->items; list; list = list->next)
    {
      GtkIconViewItem *item = list->data;
      GtkTreePath *path = gtk_tree_path_new_from_indices (item->index, -1);

      if (item->selected)
	(* func) (icon_view, path, data);

      gtk_tree_path_free (path);
    }
}

/**
 * gtk_icon_view_set_selection_mode:
 * @icon_view: A #GtkIconView.
 * @mode: The selection mode
 * 
 * Sets the selection mode of the @icon_view.
 *
 * Since: 2.6 
 **/
void
gtk_icon_view_set_selection_mode (GtkIconView      *icon_view,
				  GtkSelectionMode  mode)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  if (mode == icon_view->priv->selection_mode)
    return;
  
  if (mode == GTK_SELECTION_NONE ||
      icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE)
    gtk_icon_view_unselect_all (icon_view);
  
  icon_view->priv->selection_mode = mode;

  g_object_notify (G_OBJECT (icon_view), "selection-mode");
}

/**
 * gtk_icon_view_get_selection_mode:
 * @icon_view: A #GtkIconView.
 * 
 * Gets the selection mode of the @icon_view.
 *
 * Returns: the current selection mode
 *
 * Since: 2.6 
 **/
GtkSelectionMode
gtk_icon_view_get_selection_mode (GtkIconView *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), GTK_SELECTION_SINGLE);

  return icon_view->priv->selection_mode;
}

/**
 * gtk_icon_view_set_model:
 * @icon_view: A #GtkIconView.
 * @model: (allow-none): The model.
 *
 * Sets the model for a #GtkIconView.
 * If the @icon_view already has a model set, it will remove
 * it before setting the new model.  If @model is %NULL, then
 * it will unset the old model.
 *
 * Since: 2.6 
 **/
void
gtk_icon_view_set_model (GtkIconView *icon_view,
			 GtkTreeModel *model)
{
  gboolean dirty;

  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));
  
  if (icon_view->priv->model == model)
    return;

  if (icon_view->priv->scroll_to_path)
    {
      gtk_tree_row_reference_free (icon_view->priv->scroll_to_path);
      icon_view->priv->scroll_to_path = NULL;
    }

  /* The area can be NULL while disposing */
  if (icon_view->priv->cell_area)
    gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

  dirty = gtk_icon_view_unselect_all_internal (icon_view);

  if (model)
    {
      GType column_type;

      if (icon_view->priv->pixbuf_column != -1)
	{
	  column_type = gtk_tree_model_get_column_type (model,
							icon_view->priv->pixbuf_column);	  

	  g_return_if_fail (column_type == GDK_TYPE_PIXBUF);
	}

      if (icon_view->priv->text_column != -1)
	{
	  column_type = gtk_tree_model_get_column_type (model,
							icon_view->priv->text_column);	  

	  g_return_if_fail (column_type == G_TYPE_STRING);
	}

      if (icon_view->priv->markup_column != -1)
	{
	  column_type = gtk_tree_model_get_column_type (model,
							icon_view->priv->markup_column);	  

	  g_return_if_fail (column_type == G_TYPE_STRING);
	}
      
    }
  
  if (icon_view->priv->model)
    {
      g_signal_handlers_disconnect_by_func (icon_view->priv->model,
					    gtk_icon_view_row_changed,
					    icon_view);
      g_signal_handlers_disconnect_by_func (icon_view->priv->model,
					    gtk_icon_view_row_inserted,
					    icon_view);
      g_signal_handlers_disconnect_by_func (icon_view->priv->model,
					    gtk_icon_view_row_deleted,
					    icon_view);
      g_signal_handlers_disconnect_by_func (icon_view->priv->model,
					    gtk_icon_view_rows_reordered,
					    icon_view);

      g_object_unref (icon_view->priv->model);
      
      g_list_free_full (icon_view->priv->items, (GDestroyNotify) gtk_icon_view_item_free);
      icon_view->priv->items = NULL;
      icon_view->priv->anchor_item = NULL;
      icon_view->priv->cursor_item = NULL;
      icon_view->priv->last_single_clicked = NULL;
      icon_view->priv->last_prelight = NULL;
      icon_view->priv->width = 0;
      icon_view->priv->height = 0;
    }

  icon_view->priv->model = model;

  if (icon_view->priv->model)
    {
      g_object_ref (icon_view->priv->model);
      g_signal_connect (icon_view->priv->model,
			"row-changed",
			G_CALLBACK (gtk_icon_view_row_changed),
			icon_view);
      g_signal_connect (icon_view->priv->model,
			"row-inserted",
			G_CALLBACK (gtk_icon_view_row_inserted),
			icon_view);
      g_signal_connect (icon_view->priv->model,
			"row-deleted",
			G_CALLBACK (gtk_icon_view_row_deleted),
			icon_view);
      g_signal_connect (icon_view->priv->model,
			"rows-reordered",
			G_CALLBACK (gtk_icon_view_rows_reordered),
			icon_view);

      gtk_icon_view_build_items (icon_view);
    }

  g_object_notify (G_OBJECT (icon_view), "model");  

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);

  gtk_widget_queue_resize (GTK_WIDGET (icon_view));
}

/**
 * gtk_icon_view_get_model:
 * @icon_view: a #GtkIconView
 *
 * Returns the model the #GtkIconView is based on.  Returns %NULL if the
 * model is unset.
 *
 * Returns: (transfer none): A #GtkTreeModel, or %NULL if none is
 *     currently being used.
 *
 * Since: 2.6 
 **/
GtkTreeModel *
gtk_icon_view_get_model (GtkIconView *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), NULL);

  return icon_view->priv->model;
}

static void
update_text_cell (GtkIconView *icon_view)
{
  if (!icon_view->priv->cell_area)
    return;

  if (icon_view->priv->text_column == -1 &&
      icon_view->priv->markup_column == -1)
    {
      if (icon_view->priv->text_cell != NULL)
	{
	  gtk_cell_area_remove (icon_view->priv->cell_area, 
				icon_view->priv->text_cell);
	  icon_view->priv->text_cell = NULL;
	}
    }
  else 
    {
      if (icon_view->priv->text_cell == NULL)
	{
	  icon_view->priv->text_cell = gtk_cell_renderer_text_new ();

	  gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (icon_view), icon_view->priv->text_cell, FALSE);
	}

      if (icon_view->priv->markup_column != -1)
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icon_view),
					icon_view->priv->text_cell, 
					"markup", icon_view->priv->markup_column, 
					NULL);
      else
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icon_view),
					icon_view->priv->text_cell, 
					"text", icon_view->priv->text_column, 
					NULL);

      if (icon_view->priv->item_orientation == GTK_ORIENTATION_VERTICAL)
	g_object_set (icon_view->priv->text_cell,
                      "alignment", PANGO_ALIGN_CENTER,
		      "wrap-mode", PANGO_WRAP_WORD_CHAR,
		      "xalign", 0.5,
		      "yalign", 0.0,
		      NULL);
      else
	g_object_set (icon_view->priv->text_cell,
                      "alignment", PANGO_ALIGN_LEFT,
		      "wrap-mode", PANGO_WRAP_WORD_CHAR,
		      "xalign", 0.0,
		      "yalign", 0.5,
		      NULL);
    }
}

static void
update_pixbuf_cell (GtkIconView *icon_view)
{
  if (!icon_view->priv->cell_area)
    return;

  if (icon_view->priv->pixbuf_column == -1)
    {
      if (icon_view->priv->pixbuf_cell != NULL)
	{
	  gtk_cell_area_remove (icon_view->priv->cell_area, 
				icon_view->priv->pixbuf_cell);

	  icon_view->priv->pixbuf_cell = NULL;
	}
    }
  else 
    {
      if (icon_view->priv->pixbuf_cell == NULL)
	{
	  icon_view->priv->pixbuf_cell = gtk_cell_renderer_pixbuf_new ();
	  
	  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (icon_view), icon_view->priv->pixbuf_cell, FALSE);
	}
      
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icon_view),
				      icon_view->priv->pixbuf_cell, 
				      "pixbuf", icon_view->priv->pixbuf_column, 
				      NULL);

      if (icon_view->priv->item_orientation == GTK_ORIENTATION_VERTICAL)
	g_object_set (icon_view->priv->pixbuf_cell,
		      "xalign", 0.5,
		      "yalign", 1.0,
		      NULL);
      else
	g_object_set (icon_view->priv->pixbuf_cell,
		      "xalign", 0.0,
		      "yalign", 0.0,
		      NULL);
    }
}

/**
 * gtk_icon_view_set_text_column:
 * @icon_view: A #GtkIconView.
 * @column: A column in the currently used model, or -1 to display no text
 * 
 * Sets the column with text for @icon_view to be @column. The text
 * column must be of type #G_TYPE_STRING.
 *
 * Since: 2.6 
 **/
void
gtk_icon_view_set_text_column (GtkIconView *icon_view,
			       gint          column)
{
  if (column == icon_view->priv->text_column)
    return;
  
  if (column == -1)
    icon_view->priv->text_column = -1;
  else
    {
      if (icon_view->priv->model != NULL)
	{
	  GType column_type;
	  
	  column_type = gtk_tree_model_get_column_type (icon_view->priv->model, column);

	  g_return_if_fail (column_type == G_TYPE_STRING);
	}
      
      icon_view->priv->text_column = column;
    }

  if (icon_view->priv->cell_area)
    gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

  update_text_cell (icon_view);

  gtk_icon_view_invalidate_sizes (icon_view);
  
  g_object_notify (G_OBJECT (icon_view), "text-column");
}

/**
 * gtk_icon_view_get_text_column:
 * @icon_view: A #GtkIconView.
 *
 * Returns the column with text for @icon_view.
 *
 * Returns: the text column, or -1 if it’s unset.
 *
 * Since: 2.6
 */
gint
gtk_icon_view_get_text_column (GtkIconView  *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), -1);

  return icon_view->priv->text_column;
}

/**
 * gtk_icon_view_set_markup_column:
 * @icon_view: A #GtkIconView.
 * @column: A column in the currently used model, or -1 to display no text
 * 
 * Sets the column with markup information for @icon_view to be
 * @column. The markup column must be of type #G_TYPE_STRING.
 * If the markup column is set to something, it overrides
 * the text column set by gtk_icon_view_set_text_column().
 *
 * Since: 2.6
 **/
void
gtk_icon_view_set_markup_column (GtkIconView *icon_view,
				 gint         column)
{
  if (column == icon_view->priv->markup_column)
    return;
  
  if (column == -1)
    icon_view->priv->markup_column = -1;
  else
    {
      if (icon_view->priv->model != NULL)
	{
	  GType column_type;
	  
	  column_type = gtk_tree_model_get_column_type (icon_view->priv->model, column);

	  g_return_if_fail (column_type == G_TYPE_STRING);
	}
      
      icon_view->priv->markup_column = column;
    }

  if (icon_view->priv->cell_area)
    gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

  update_text_cell (icon_view);

  gtk_icon_view_invalidate_sizes (icon_view);
  
  g_object_notify (G_OBJECT (icon_view), "markup-column");
}

/**
 * gtk_icon_view_get_markup_column:
 * @icon_view: A #GtkIconView.
 *
 * Returns the column with markup text for @icon_view.
 *
 * Returns: the markup column, or -1 if it’s unset.
 *
 * Since: 2.6
 */
gint
gtk_icon_view_get_markup_column (GtkIconView  *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), -1);

  return icon_view->priv->markup_column;
}

/**
 * gtk_icon_view_set_pixbuf_column:
 * @icon_view: A #GtkIconView.
 * @column: A column in the currently used model, or -1 to disable
 * 
 * Sets the column with pixbufs for @icon_view to be @column. The pixbuf
 * column must be of type #GDK_TYPE_PIXBUF
 *
 * Since: 2.6 
 **/
void
gtk_icon_view_set_pixbuf_column (GtkIconView *icon_view,
				 gint         column)
{
  if (column == icon_view->priv->pixbuf_column)
    return;
  
  if (column == -1)
    icon_view->priv->pixbuf_column = -1;
  else
    {
      if (icon_view->priv->model != NULL)
	{
	  GType column_type;
	  
	  column_type = gtk_tree_model_get_column_type (icon_view->priv->model, column);

	  g_return_if_fail (column_type == GDK_TYPE_PIXBUF);
	}
      
      icon_view->priv->pixbuf_column = column;
    }

  if (icon_view->priv->cell_area)
    gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

  update_pixbuf_cell (icon_view);

  gtk_icon_view_invalidate_sizes (icon_view);
  
  g_object_notify (G_OBJECT (icon_view), "pixbuf-column");
  
}

/**
 * gtk_icon_view_get_pixbuf_column:
 * @icon_view: A #GtkIconView.
 *
 * Returns the column with pixbufs for @icon_view.
 *
 * Returns: the pixbuf column, or -1 if it’s unset.
 *
 * Since: 2.6
 */
gint
gtk_icon_view_get_pixbuf_column (GtkIconView  *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), -1);

  return icon_view->priv->pixbuf_column;
}

/**
 * gtk_icon_view_select_path:
 * @icon_view: A #GtkIconView.
 * @path: The #GtkTreePath to be selected.
 * 
 * Selects the row at @path.
 *
 * Since: 2.6
 **/
void
gtk_icon_view_select_path (GtkIconView *icon_view,
			   GtkTreePath *path)
{
  GtkIconViewItem *item = NULL;

  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  g_return_if_fail (icon_view->priv->model != NULL);
  g_return_if_fail (path != NULL);

  if (gtk_tree_path_get_depth (path) > 0)
    item = g_list_nth_data (icon_view->priv->items,
			    gtk_tree_path_get_indices(path)[0]);

  if (item)
    _gtk_icon_view_select_item (icon_view, item);
}

/**
 * gtk_icon_view_unselect_path:
 * @icon_view: A #GtkIconView.
 * @path: The #GtkTreePath to be unselected.
 * 
 * Unselects the row at @path.
 *
 * Since: 2.6
 **/
void
gtk_icon_view_unselect_path (GtkIconView *icon_view,
			     GtkTreePath *path)
{
  GtkIconViewItem *item;
  
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  g_return_if_fail (icon_view->priv->model != NULL);
  g_return_if_fail (path != NULL);

  item = g_list_nth_data (icon_view->priv->items,
			  gtk_tree_path_get_indices(path)[0]);

  if (!item)
    return;
  
  _gtk_icon_view_unselect_item (icon_view, item);
}

/**
 * gtk_icon_view_get_selected_items:
 * @icon_view: A #GtkIconView.
 *
 * Creates a list of paths of all selected items. Additionally, if you are
 * planning on modifying the model after calling this function, you may
 * want to convert the returned list into a list of #GtkTreeRowReferences.
 * To do this, you can use gtk_tree_row_reference_new().
 *
 * To free the return value, use:
 * |[<!-- language="C" -->
 * g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
 * ]|
 *
 * Returns: (element-type GtkTreePath) (transfer full): A #GList containing a #GtkTreePath for each selected row.
 *
 * Since: 2.6
 **/
GList *
gtk_icon_view_get_selected_items (GtkIconView *icon_view)
{
  GList *list;
  GList *selected = NULL;
  
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), NULL);
  
  for (list = icon_view->priv->items; list != NULL; list = list->next)
    {
      GtkIconViewItem *item = list->data;

      if (item->selected)
	{
	  GtkTreePath *path = gtk_tree_path_new_from_indices (item->index, -1);

	  selected = g_list_prepend (selected, path);
	}
    }

  return selected;
}

/**
 * gtk_icon_view_select_all:
 * @icon_view: A #GtkIconView.
 * 
 * Selects all the icons. @icon_view must has its selection mode set
 * to #GTK_SELECTION_MULTIPLE.
 *
 * Since: 2.6
 **/
void
gtk_icon_view_select_all (GtkIconView *icon_view)
{
  GList *items;
  gboolean dirty = FALSE;
  
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  if (icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    return;

  for (items = icon_view->priv->items; items; items = items->next)
    {
      GtkIconViewItem *item = items->data;
      
      if (!item->selected)
	{
	  dirty = TRUE;
	  item->selected = TRUE;
	  gtk_icon_view_queue_draw_item (icon_view, item);
	}
    }

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}

/**
 * gtk_icon_view_unselect_all:
 * @icon_view: A #GtkIconView.
 * 
 * Unselects all the icons.
 *
 * Since: 2.6
 **/
void
gtk_icon_view_unselect_all (GtkIconView *icon_view)
{
  gboolean dirty = FALSE;
  
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  if (icon_view->priv->selection_mode == GTK_SELECTION_BROWSE)
    return;

  dirty = gtk_icon_view_unselect_all_internal (icon_view);

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}

/**
 * gtk_icon_view_path_is_selected:
 * @icon_view: A #GtkIconView.
 * @path: A #GtkTreePath to check selection on.
 * 
 * Returns %TRUE if the icon pointed to by @path is currently
 * selected. If @path does not point to a valid location, %FALSE is returned.
 * 
 * Returns: %TRUE if @path is selected.
 *
 * Since: 2.6
 **/
gboolean
gtk_icon_view_path_is_selected (GtkIconView *icon_view,
				GtkTreePath *path)
{
  GtkIconViewItem *item;
  
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), FALSE);
  g_return_val_if_fail (icon_view->priv->model != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  
  item = g_list_nth_data (icon_view->priv->items,
			  gtk_tree_path_get_indices(path)[0]);

  if (!item)
    return FALSE;
  
  return item->selected;
}

/**
 * gtk_icon_view_get_item_row:
 * @icon_view: a #GtkIconView
 * @path: the #GtkTreePath of the item
 *
 * Gets the row in which the item @path is currently
 * displayed. Row numbers start at 0.
 *
 * Returns: The row in which the item is displayed
 *
 * Since: 2.22
 */
gint
gtk_icon_view_get_item_row (GtkIconView *icon_view,
                            GtkTreePath *path)
{
  GtkIconViewItem *item;

  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), -1);
  g_return_val_if_fail (icon_view->priv->model != NULL, -1);
  g_return_val_if_fail (path != NULL, -1);

  item = g_list_nth_data (icon_view->priv->items,
                          gtk_tree_path_get_indices(path)[0]);

  if (!item)
    return -1;

  return item->row;
}

/**
 * gtk_icon_view_get_item_column:
 * @icon_view: a #GtkIconView
 * @path: the #GtkTreePath of the item
 *
 * Gets the column in which the item @path is currently
 * displayed. Column numbers start at 0.
 *
 * Returns: The column in which the item is displayed
 *
 * Since: 2.22
 */
gint
gtk_icon_view_get_item_column (GtkIconView *icon_view,
                               GtkTreePath *path)
{
  GtkIconViewItem *item;

  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), -1);
  g_return_val_if_fail (icon_view->priv->model != NULL, -1);
  g_return_val_if_fail (path != NULL, -1);

  item = g_list_nth_data (icon_view->priv->items,
                          gtk_tree_path_get_indices(path)[0]);

  if (!item)
    return -1;

  return item->col;
}

/**
 * gtk_icon_view_item_activated:
 * @icon_view: A #GtkIconView
 * @path: The #GtkTreePath to be activated
 * 
 * Activates the item determined by @path.
 *
 * Since: 2.6
 **/
void
gtk_icon_view_item_activated (GtkIconView      *icon_view,
			      GtkTreePath      *path)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  g_return_if_fail (path != NULL);
  
  g_signal_emit (icon_view, icon_view_signals[ITEM_ACTIVATED], 0, path);
}

/**
 * gtk_icon_view_set_item_orientation:
 * @icon_view: a #GtkIconView
 * @orientation: the relative position of texts and icons 
 * 
 * Sets the ::item-orientation property which determines whether the labels 
 * are drawn beside the icons instead of below.
 *
 * Since: 2.6
 **/
void 
gtk_icon_view_set_item_orientation (GtkIconView    *icon_view,
                                    GtkOrientation  orientation)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  if (icon_view->priv->item_orientation != orientation)
    {
      icon_view->priv->item_orientation = orientation;

      if (icon_view->priv->cell_area)
	{
	  if (GTK_IS_ORIENTABLE (icon_view->priv->cell_area))
	    gtk_orientable_set_orientation (GTK_ORIENTABLE (icon_view->priv->cell_area), 
					    icon_view->priv->item_orientation);

	  gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);
	}

      gtk_icon_view_invalidate_sizes (icon_view);

      update_text_cell (icon_view);
      update_pixbuf_cell (icon_view);
      
      g_object_notify (G_OBJECT (icon_view), "item-orientation");
    }
}

/**
 * gtk_icon_view_get_item_orientation:
 * @icon_view: a #GtkIconView
 * 
 * Returns the value of the ::item-orientation property which determines 
 * whether the labels are drawn beside the icons instead of below. 
 * 
 * Returns: the relative position of texts and icons 
 *
 * Since: 2.6
 **/
GtkOrientation
gtk_icon_view_get_item_orientation (GtkIconView *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), 
			GTK_ORIENTATION_VERTICAL);

  return icon_view->priv->item_orientation;
}

/**
 * gtk_icon_view_set_columns:
 * @icon_view: a #GtkIconView
 * @columns: the number of columns
 * 
 * Sets the ::columns property which determines in how
 * many columns the icons are arranged. If @columns is
 * -1, the number of columns will be chosen automatically 
 * to fill the available area. 
 *
 * Since: 2.6
 */
void 
gtk_icon_view_set_columns (GtkIconView *icon_view,
			   gint         columns)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  
  if (icon_view->priv->columns != columns)
    {
      icon_view->priv->columns = columns;

      if (icon_view->priv->cell_area)
	gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

      gtk_widget_queue_resize (GTK_WIDGET (icon_view));
      
      g_object_notify (G_OBJECT (icon_view), "columns");
    }  
}

/**
 * gtk_icon_view_get_columns:
 * @icon_view: a #GtkIconView
 * 
 * Returns the value of the ::columns property.
 * 
 * Returns: the number of columns, or -1
 *
 * Since: 2.6
 */
gint
gtk_icon_view_get_columns (GtkIconView *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), -1);

  return icon_view->priv->columns;
}

/**
 * gtk_icon_view_set_item_width:
 * @icon_view: a #GtkIconView
 * @item_width: the width for each item
 * 
 * Sets the ::item-width property which specifies the width 
 * to use for each item. If it is set to -1, the icon view will 
 * automatically determine a suitable item size.
 *
 * Since: 2.6
 */
void 
gtk_icon_view_set_item_width (GtkIconView *icon_view,
			      gint         item_width)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  
  if (icon_view->priv->item_width != item_width)
    {
      icon_view->priv->item_width = item_width;
      
      if (icon_view->priv->cell_area)
	gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

      gtk_icon_view_invalidate_sizes (icon_view);
      
      update_text_cell (icon_view);

      g_object_notify (G_OBJECT (icon_view), "item-width");
    }  
}

/**
 * gtk_icon_view_get_item_width:
 * @icon_view: a #GtkIconView
 * 
 * Returns the value of the ::item-width property.
 * 
 * Returns: the width of a single item, or -1
 *
 * Since: 2.6
 */
gint
gtk_icon_view_get_item_width (GtkIconView *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), -1);

  return icon_view->priv->item_width;
}


/**
 * gtk_icon_view_set_spacing:
 * @icon_view: a #GtkIconView
 * @spacing: the spacing
 * 
 * Sets the ::spacing property which specifies the space 
 * which is inserted between the cells (i.e. the icon and 
 * the text) of an item.
 *
 * Since: 2.6
 */
void 
gtk_icon_view_set_spacing (GtkIconView *icon_view,
			   gint         spacing)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  
  if (icon_view->priv->spacing != spacing)
    {
      icon_view->priv->spacing = spacing;

      if (icon_view->priv->cell_area)
	gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

      gtk_icon_view_invalidate_sizes (icon_view);

      g_object_notify (G_OBJECT (icon_view), "spacing");
    }  
}

/**
 * gtk_icon_view_get_spacing:
 * @icon_view: a #GtkIconView
 * 
 * Returns the value of the ::spacing property.
 * 
 * Returns: the space between cells 
 *
 * Since: 2.6
 */
gint
gtk_icon_view_get_spacing (GtkIconView *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), -1);

  return icon_view->priv->spacing;
}

/**
 * gtk_icon_view_set_row_spacing:
 * @icon_view: a #GtkIconView
 * @row_spacing: the row spacing
 * 
 * Sets the ::row-spacing property which specifies the space 
 * which is inserted between the rows of the icon view.
 *
 * Since: 2.6
 */
void 
gtk_icon_view_set_row_spacing (GtkIconView *icon_view,
			       gint         row_spacing)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  
  if (icon_view->priv->row_spacing != row_spacing)
    {
      icon_view->priv->row_spacing = row_spacing;

      if (icon_view->priv->cell_area)
	gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

      gtk_icon_view_invalidate_sizes (icon_view);

      g_object_notify (G_OBJECT (icon_view), "row-spacing");
    }  
}

/**
 * gtk_icon_view_get_row_spacing:
 * @icon_view: a #GtkIconView
 * 
 * Returns the value of the ::row-spacing property.
 * 
 * Returns: the space between rows
 *
 * Since: 2.6
 */
gint
gtk_icon_view_get_row_spacing (GtkIconView *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), -1);

  return icon_view->priv->row_spacing;
}

/**
 * gtk_icon_view_set_column_spacing:
 * @icon_view: a #GtkIconView
 * @column_spacing: the column spacing
 * 
 * Sets the ::column-spacing property which specifies the space 
 * which is inserted between the columns of the icon view.
 *
 * Since: 2.6
 */
void 
gtk_icon_view_set_column_spacing (GtkIconView *icon_view,
				  gint         column_spacing)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  
  if (icon_view->priv->column_spacing != column_spacing)
    {
      icon_view->priv->column_spacing = column_spacing;

      if (icon_view->priv->cell_area)
	gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

      gtk_icon_view_invalidate_sizes (icon_view);

      g_object_notify (G_OBJECT (icon_view), "column-spacing");
    }  
}

/**
 * gtk_icon_view_get_column_spacing:
 * @icon_view: a #GtkIconView
 * 
 * Returns the value of the ::column-spacing property.
 * 
 * Returns: the space between columns
 *
 * Since: 2.6
 */
gint
gtk_icon_view_get_column_spacing (GtkIconView *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), -1);

  return icon_view->priv->column_spacing;
}

/**
 * gtk_icon_view_set_margin:
 * @icon_view: a #GtkIconView
 * @margin: the margin
 * 
 * Sets the ::margin property which specifies the space 
 * which is inserted at the top, bottom, left and right 
 * of the icon view.
 *
 * Since: 2.6
 */
void 
gtk_icon_view_set_margin (GtkIconView *icon_view,
			  gint         margin)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  
  if (icon_view->priv->margin != margin)
    {
      icon_view->priv->margin = margin;

      if (icon_view->priv->cell_area)
	gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

      gtk_icon_view_invalidate_sizes (icon_view);

      g_object_notify (G_OBJECT (icon_view), "margin");
    }  
}

/**
 * gtk_icon_view_get_margin:
 * @icon_view: a #GtkIconView
 * 
 * Returns the value of the ::margin property.
 * 
 * Returns: the space at the borders 
 *
 * Since: 2.6
 */
gint
gtk_icon_view_get_margin (GtkIconView *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), -1);

  return icon_view->priv->margin;
}

/**
 * gtk_icon_view_set_item_padding:
 * @icon_view: a #GtkIconView
 * @item_padding: the item padding
 *
 * Sets the #GtkIconView:item-padding property which specifies the padding
 * around each of the icon view’s items.
 *
 * Since: 2.18
 */
void
gtk_icon_view_set_item_padding (GtkIconView *icon_view,
				gint         item_padding)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  
  if (icon_view->priv->item_padding != item_padding)
    {
      icon_view->priv->item_padding = item_padding;

      if (icon_view->priv->cell_area)
	gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

      gtk_icon_view_invalidate_sizes (icon_view);

      g_object_notify (G_OBJECT (icon_view), "item-padding");
    }  
}

/**
 * gtk_icon_view_get_item_padding:
 * @icon_view: a #GtkIconView
 * 
 * Returns the value of the ::item-padding property.
 * 
 * Returns: the padding around items
 *
 * Since: 2.18
 */
gint
gtk_icon_view_get_item_padding (GtkIconView *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), -1);

  return icon_view->priv->item_padding;
}

/* Get/set whether drag_motion requested the drag data and
 * drag_data_received should thus not actually insert the data,
 * since the data doesn’t result from a drop.
 */
static void
set_status_pending (GdkDragContext *context,
                    GdkDragAction   suggested_action)
{
  g_object_set_data (G_OBJECT (context),
                     I_("gtk-icon-view-status-pending"),
                     GINT_TO_POINTER (suggested_action));
}

static GdkDragAction
get_status_pending (GdkDragContext *context)
{
  return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (context),
                                             "gtk-icon-view-status-pending"));
}

static void
unset_reorderable (GtkIconView *icon_view)
{
  if (icon_view->priv->reorderable)
    {
      icon_view->priv->reorderable = FALSE;
      g_object_notify (G_OBJECT (icon_view), "reorderable");
    }
}

static void
set_source_row (GdkDragContext *context,
                GtkTreeModel   *model,
                GtkTreePath    *source_row)
{
  if (source_row)
    g_object_set_data_full (G_OBJECT (context),
			    I_("gtk-icon-view-source-row"),
			    gtk_tree_row_reference_new (model, source_row),
			    (GDestroyNotify) gtk_tree_row_reference_free);
  else
    g_object_set_data_full (G_OBJECT (context),
			    I_("gtk-icon-view-source-row"),
			    NULL, NULL);
}

static GtkTreePath*
get_source_row (GdkDragContext *context)
{
  GtkTreeRowReference *ref;

  ref = g_object_get_data (G_OBJECT (context), "gtk-icon-view-source-row");

  if (ref)
    return gtk_tree_row_reference_get_path (ref);
  else
    return NULL;
}

typedef struct
{
  GtkTreeRowReference *dest_row;
  gboolean             empty_view_drop;
  gboolean             drop_append_mode;
} DestRow;

static void
dest_row_free (gpointer data)
{
  DestRow *dr = (DestRow *)data;

  gtk_tree_row_reference_free (dr->dest_row);
  g_free (dr);
}

static void
set_dest_row (GdkDragContext *context,
	      GtkTreeModel   *model,
	      GtkTreePath    *dest_row,
	      gboolean        empty_view_drop,
	      gboolean        drop_append_mode)
{
  DestRow *dr;

  if (!dest_row)
    {
      g_object_set_data_full (G_OBJECT (context),
			      I_("gtk-icon-view-dest-row"),
			      NULL, NULL);
      return;
    }
  
  dr = g_new0 (DestRow, 1);
     
  dr->dest_row = gtk_tree_row_reference_new (model, dest_row);
  dr->empty_view_drop = empty_view_drop;
  dr->drop_append_mode = drop_append_mode;
  g_object_set_data_full (G_OBJECT (context),
                          I_("gtk-icon-view-dest-row"),
                          dr, (GDestroyNotify) dest_row_free);
}

static GtkTreePath*
get_dest_row (GdkDragContext *context)
{
  DestRow *dr;

  dr = g_object_get_data (G_OBJECT (context), "gtk-icon-view-dest-row");

  if (dr)
    {
      GtkTreePath *path = NULL;
      
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

static gboolean
check_model_dnd (GtkTreeModel *model,
                 GType         required_iface,
                 const gchar  *signal)
{
  if (model == NULL || !G_TYPE_CHECK_INSTANCE_TYPE ((model), required_iface))
    {
      g_warning ("You must override the default '%s' handler "
                 "on GtkIconView when using models that don't support "
                 "the %s interface and enabling drag-and-drop. The simplest way to do this "
                 "is to connect to '%s' and call "
                 "g_signal_stop_emission_by_name() in your signal handler to prevent "
                 "the default handler from running. Look at the source code "
                 "for the default handler in gtkiconview.c to get an idea what "
                 "your handler should do. (gtkiconview.c is in the GTK+ source "
                 "code.) If you're using GTK+ from a language other than C, "
                 "there may be a more natural way to override default handlers, e.g. via derivation.",
                 signal, g_type_name (required_iface), signal);
      return FALSE;
    }
  else
    return TRUE;
}

static void
remove_scroll_timeout (GtkIconView *icon_view)
{
  if (icon_view->priv->scroll_timeout_id != 0)
    {
      g_source_remove (icon_view->priv->scroll_timeout_id);

      icon_view->priv->scroll_timeout_id = 0;
    }
}

static void
gtk_icon_view_autoscroll (GtkIconView *icon_view,
                          GdkDevice   *device)
{
  GdkWindow *window;
  gint px, py, width, height;
  gint hoffset, voffset;

  window = gtk_widget_get_window (GTK_WIDGET (icon_view));

  gdk_window_get_device_position (window, device, &px, &py, NULL);
  gdk_window_get_geometry (window, NULL, NULL, &width, &height);

  /* see if we are near the edge. */
  voffset = py - 2 * SCROLL_EDGE_SIZE;
  if (voffset > 0)
    voffset = MAX (py - (height - 2 * SCROLL_EDGE_SIZE), 0);

  hoffset = px - 2 * SCROLL_EDGE_SIZE;
  if (hoffset > 0)
    hoffset = MAX (px - (width - 2 * SCROLL_EDGE_SIZE), 0);

  if (voffset != 0)
    gtk_adjustment_set_value (icon_view->priv->vadjustment,
                              gtk_adjustment_get_value (icon_view->priv->vadjustment) + voffset);

  if (hoffset != 0)
    gtk_adjustment_set_value (icon_view->priv->hadjustment,
                              gtk_adjustment_get_value (icon_view->priv->hadjustment) + hoffset);
}

typedef struct {
  GtkIconView *icon_view;
  GdkDevice   *device;
} DragScrollData;

static gboolean
drag_scroll_timeout (gpointer datap)
{
  DragScrollData *data = datap;

  gtk_icon_view_autoscroll (data->icon_view, data->device);

  return TRUE;
}

static void
drag_scroll_data_free (DragScrollData *data)
{
  g_slice_free (DragScrollData, data);
}

static gboolean
set_destination (GtkIconView    *icon_view,
		 GdkDragContext *context,
		 gint            x,
		 gint            y,
		 GdkDragAction  *suggested_action,
		 GdkAtom        *target)
{
  GtkWidget *widget;
  GtkTreePath *path = NULL;
  GtkIconViewDropPosition pos;
  GtkIconViewDropPosition old_pos;
  GtkTreePath *old_dest_path = NULL;
  gboolean can_drop = FALSE;

  widget = GTK_WIDGET (icon_view);

  *suggested_action = 0;
  *target = GDK_NONE;

  if (!icon_view->priv->dest_set)
    {
      /* someone unset us as a drag dest, note that if
       * we return FALSE drag_leave isn't called
       */

      gtk_icon_view_set_drag_dest_item (icon_view,
					NULL,
					GTK_ICON_VIEW_DROP_LEFT);

      remove_scroll_timeout (GTK_ICON_VIEW (widget));

      return FALSE; /* no longer a drop site */
    }

  *target = gtk_drag_dest_find_target (widget, context,
                                       gtk_drag_dest_get_target_list (widget));
  if (*target == GDK_NONE)
    return FALSE;

  if (!gtk_icon_view_get_dest_item_at_pos (icon_view, x, y, &path, &pos)) 
    {
      gint n_children;
      GtkTreeModel *model;
      
      /* the row got dropped on empty space, let's setup a special case
       */

      if (path)
	gtk_tree_path_free (path);

      model = gtk_icon_view_get_model (icon_view);

      n_children = gtk_tree_model_iter_n_children (model, NULL);
      if (n_children)
        {
          pos = GTK_ICON_VIEW_DROP_BELOW;
          path = gtk_tree_path_new_from_indices (n_children - 1, -1);
        }
      else
        {
          pos = GTK_ICON_VIEW_DROP_ABOVE;
          path = gtk_tree_path_new_from_indices (0, -1);
        }

      can_drop = TRUE;

      goto out;
    }

  g_assert (path);

  gtk_icon_view_get_drag_dest_item (icon_view,
				    &old_dest_path,
				    &old_pos);
  
  if (old_dest_path)
    gtk_tree_path_free (old_dest_path);
  
  if (TRUE /* FIXME if the location droppable predicate */)
    {
      can_drop = TRUE;
    }

out:
  if (can_drop)
    {
      GtkWidget *source_widget;

      *suggested_action = gdk_drag_context_get_suggested_action (context);
      source_widget = gtk_drag_get_source_widget (context);

      if (source_widget == widget)
        {
          /* Default to MOVE, unless the user has
           * pressed ctrl or shift to affect available actions
           */
          if ((gdk_drag_context_get_actions (context) & GDK_ACTION_MOVE) != 0)
            *suggested_action = GDK_ACTION_MOVE;
        }

      gtk_icon_view_set_drag_dest_item (GTK_ICON_VIEW (widget),
					path, pos);
    }
  else
    {
      /* can't drop here */
      gtk_icon_view_set_drag_dest_item (GTK_ICON_VIEW (widget),
					NULL,
					GTK_ICON_VIEW_DROP_LEFT);
    }
  
  if (path)
    gtk_tree_path_free (path);
  
  return TRUE;
}

static GtkTreePath*
get_logical_destination (GtkIconView *icon_view,
			 gboolean    *drop_append_mode)
{
  /* adjust path to point to the row the drop goes in front of */
  GtkTreePath *path = NULL;
  GtkIconViewDropPosition pos;
  
  *drop_append_mode = FALSE;

  gtk_icon_view_get_drag_dest_item (icon_view, &path, &pos);

  if (path == NULL)
    return NULL;

  if (pos == GTK_ICON_VIEW_DROP_RIGHT || 
      pos == GTK_ICON_VIEW_DROP_BELOW)
    {
      GtkTreeIter iter;
      GtkTreeModel *model = icon_view->priv->model;

      if (!gtk_tree_model_get_iter (model, &iter, path) ||
          !gtk_tree_model_iter_next (model, &iter))
        *drop_append_mode = TRUE;
      else
        {
          *drop_append_mode = FALSE;
          gtk_tree_path_next (path);
        }      
    }

  return path;
}

static gboolean
gtk_icon_view_maybe_begin_drag (GtkIconView    *icon_view,
				GdkEventMotion *event)
{
  GtkWidget *widget = GTK_WIDGET (icon_view);
  GdkDragContext *context;
  GtkTreePath *path = NULL;
  gint button;
  GtkTreeModel *model;
  gboolean retval = FALSE;

  if (!icon_view->priv->source_set)
    goto out;

  if (icon_view->priv->pressed_button < 0)
    goto out;

  if (!gtk_drag_check_threshold (GTK_WIDGET (icon_view),
                                 icon_view->priv->press_start_x,
                                 icon_view->priv->press_start_y,
                                 event->x, event->y))
    goto out;

  model = gtk_icon_view_get_model (icon_view);

  if (model == NULL)
    goto out;

  button = icon_view->priv->pressed_button;
  icon_view->priv->pressed_button = -1;

  path = gtk_icon_view_get_path_at_pos (icon_view,
					icon_view->priv->press_start_x,
					icon_view->priv->press_start_y);

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

  context = gtk_drag_begin_with_coordinates (widget,
                                             gtk_drag_source_get_target_list (widget),
                                             icon_view->priv->source_actions,
                                             button,
                                             (GdkEvent*)event,
                                             icon_view->priv->press_start_x,
                                             icon_view->priv->press_start_y);

  set_source_row (context, model, path);
  
 out:
  if (path)
    gtk_tree_path_free (path);

  return retval;
}

/* Source side drag signals */
static void 
gtk_icon_view_drag_begin (GtkWidget      *widget,
			  GdkDragContext *context)
{
  GtkIconView *icon_view;
  GtkIconViewItem *item;
  cairo_surface_t *icon;
  gint x, y;
  GtkTreePath *path;

  icon_view = GTK_ICON_VIEW (widget);

  /* if the user uses a custom DnD impl, we don't set the icon here */
  if (!icon_view->priv->dest_set && !icon_view->priv->source_set)
    return;

  item = _gtk_icon_view_get_item_at_coords (icon_view,
					   icon_view->priv->press_start_x,
					   icon_view->priv->press_start_y,
					   TRUE,
					   NULL);

  g_return_if_fail (item != NULL);

  x = icon_view->priv->press_start_x - item->cell_area.x + icon_view->priv->item_padding;
  y = icon_view->priv->press_start_y - item->cell_area.y + icon_view->priv->item_padding;
  
  path = gtk_tree_path_new_from_indices (item->index, -1);
  icon = gtk_icon_view_create_drag_icon (icon_view, path);
  gtk_tree_path_free (path);

  cairo_surface_set_device_offset (icon, -x, -y);

  gtk_drag_set_icon_surface (context, icon);

  cairo_surface_destroy (icon);
}

static void 
gtk_icon_view_drag_end (GtkWidget      *widget,
			GdkDragContext *context)
{
  /* do nothing */
}

static void 
gtk_icon_view_drag_data_get (GtkWidget        *widget,
			     GdkDragContext   *context,
			     GtkSelectionData *selection_data,
			     guint             info,
			     guint             time)
{
  GtkIconView *icon_view;
  GtkTreeModel *model;
  GtkTreePath *source_row;

  icon_view = GTK_ICON_VIEW (widget);
  model = gtk_icon_view_get_model (icon_view);

  if (model == NULL)
    return;

  if (!icon_view->priv->source_set)
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
  if (gtk_selection_data_get_target (selection_data) == gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
    gtk_tree_set_row_drag_data (selection_data,
				model,
				source_row);

 done:
  gtk_tree_path_free (source_row);
}

static void 
gtk_icon_view_drag_data_delete (GtkWidget      *widget,
				GdkDragContext *context)
{
  GtkTreeModel *model;
  GtkIconView *icon_view;
  GtkTreePath *source_row;

  icon_view = GTK_ICON_VIEW (widget);
  model = gtk_icon_view_get_model (icon_view);

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_SOURCE, "drag-data-delete"))
    return;

  if (!icon_view->priv->source_set)
    return;

  source_row = get_source_row (context);

  if (source_row == NULL)
    return;

  gtk_tree_drag_source_drag_data_delete (GTK_TREE_DRAG_SOURCE (model),
                                         source_row);

  gtk_tree_path_free (source_row);

  set_source_row (context, NULL, NULL);
}

/* Target side drag signals */
static void
gtk_icon_view_drag_leave (GtkWidget      *widget,
			  GdkDragContext *context,
			  guint           time)
{
  GtkIconView *icon_view;

  icon_view = GTK_ICON_VIEW (widget);

  /* unset any highlight row */
  gtk_icon_view_set_drag_dest_item (icon_view,
				    NULL,
				    GTK_ICON_VIEW_DROP_LEFT);

  remove_scroll_timeout (icon_view);
}

static gboolean 
gtk_icon_view_drag_motion (GtkWidget      *widget,
			   GdkDragContext *context,
			   gint            x,
			   gint            y,
			   guint           time)
{
  GtkTreePath *path = NULL;
  GtkIconViewDropPosition pos;
  GtkIconView *icon_view;
  GdkDragAction suggested_action = 0;
  GdkAtom target;
  gboolean empty;

  icon_view = GTK_ICON_VIEW (widget);

  if (!set_destination (icon_view, context, x, y, &suggested_action, &target))
    return FALSE;

  gtk_icon_view_get_drag_dest_item (icon_view, &path, &pos);

  /* we only know this *after* set_desination_row */
  empty = icon_view->priv->empty_view_drop;

  if (path == NULL && !empty)
    {
      /* Can't drop here. */
      gdk_drag_status (context, 0, time);
    }
  else
    {
      if (icon_view->priv->scroll_timeout_id == 0)
	{
          DragScrollData *data = g_slice_new (DragScrollData);
          data->icon_view = icon_view;
          data->device = gdk_drag_context_get_device (context);

	  icon_view->priv->scroll_timeout_id =
	    gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT, 50, drag_scroll_timeout, data, (GDestroyNotify) drag_scroll_data_free);
	  g_source_set_name_by_id (icon_view->priv->scroll_timeout_id, "[gtk+] drag_scroll_timeout");
	}

      if (target == gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
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
gtk_icon_view_drag_drop (GtkWidget      *widget,
			 GdkDragContext *context,
			 gint            x,
			 gint            y,
			 guint           time)
{
  GtkIconView *icon_view;
  GtkTreePath *path;
  GdkDragAction suggested_action = 0;
  GdkAtom target = GDK_NONE;
  GtkTreeModel *model;
  gboolean drop_append_mode;

  icon_view = GTK_ICON_VIEW (widget);
  model = gtk_icon_view_get_model (icon_view);

  remove_scroll_timeout (GTK_ICON_VIEW (widget));

  if (!icon_view->priv->dest_set)
    return FALSE;

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_DEST, "drag-drop"))
    return FALSE;

  if (!set_destination (icon_view, context, x, y, &suggested_action, &target))
    return FALSE;
  
  path = get_logical_destination (icon_view, &drop_append_mode);

  if (target != GDK_NONE && path != NULL)
    {
      /* in case a motion had requested drag data, change things so we
       * treat drag data receives as a drop.
       */
      set_status_pending (context, 0);
      set_dest_row (context, model, path, 
		    icon_view->priv->empty_view_drop, drop_append_mode);
    }

  if (path)
    gtk_tree_path_free (path);

  /* Unset this thing */
  gtk_icon_view_set_drag_dest_item (icon_view, NULL, GTK_ICON_VIEW_DROP_LEFT);

  if (target != GDK_NONE)
    {
      gtk_drag_get_data (widget, context, target, time);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_icon_view_drag_data_received (GtkWidget        *widget,
				  GdkDragContext   *context,
				  gint              x,
				  gint              y,
				  GtkSelectionData *selection_data,
				  guint             info,
				  guint             time)
{
  GtkTreePath *path;
  gboolean accepted = FALSE;
  GtkTreeModel *model;
  GtkIconView *icon_view;
  GtkTreePath *dest_row;
  GdkDragAction suggested_action;
  gboolean drop_append_mode;
  
  icon_view = GTK_ICON_VIEW (widget);  
  model = gtk_icon_view_get_model (icon_view);

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_DEST, "drag-data-received"))
    return;

  if (!icon_view->priv->dest_set)
    return;

  suggested_action = get_status_pending (context);

  if (suggested_action)
    {
      /* We are getting this data due to a request in drag_motion,
       * rather than due to a request in drag_drop, so we are just
       * supposed to call drag_status, not actually paste in the
       * data.
       */
      path = get_logical_destination (icon_view, &drop_append_mode);

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
        gtk_icon_view_set_drag_dest_item (icon_view,
					  NULL,
					  GTK_ICON_VIEW_DROP_LEFT);
      return;
    }
  

  dest_row = get_dest_row (context);

  if (dest_row == NULL)
    return;

  if (gtk_selection_data_get_length (selection_data) >= 0)
    {
      if (gtk_tree_drag_dest_drag_data_received (GTK_TREE_DRAG_DEST (model),
                                                 dest_row,
                                                 selection_data))
        accepted = TRUE;
    }

  gtk_drag_finish (context,
                   accepted,
                   (gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE),
                   time);

  gtk_tree_path_free (dest_row);

  /* drop dest_row */
  set_dest_row (context, NULL, NULL, FALSE, FALSE);
}

/* Drag-and-Drop support */
/**
 * gtk_icon_view_enable_model_drag_source:
 * @icon_view: a #GtkIconView
 * @start_button_mask: Mask of allowed buttons to start drag
 * @targets: (array length=n_targets): the table of targets that the drag will
 *           support
 * @n_targets: the number of items in @targets
 * @actions: the bitmask of possible actions for a drag from this
 *    widget
 *
 * Turns @icon_view into a drag source for automatic DND. Calling this
 * method sets #GtkIconView:reorderable to %FALSE.
 *
 * Since: 2.8
 **/
void
gtk_icon_view_enable_model_drag_source (GtkIconView              *icon_view,
					GdkModifierType           start_button_mask,
					const GtkTargetEntry     *targets,
					gint                      n_targets,
					GdkDragAction             actions)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  gtk_drag_source_set (GTK_WIDGET (icon_view), 0, targets, n_targets, actions);

  icon_view->priv->start_button_mask = start_button_mask;
  icon_view->priv->source_actions = actions;

  icon_view->priv->source_set = TRUE;

  unset_reorderable (icon_view);
}

/**
 * gtk_icon_view_enable_model_drag_dest:
 * @icon_view: a #GtkIconView
 * @targets: (array length=n_targets): the table of targets that the drag will
 *           support
 * @n_targets: the number of items in @targets
 * @actions: the bitmask of possible actions for a drag to this
 *    widget
 *
 * Turns @icon_view into a drop destination for automatic DND. Calling this
 * method sets #GtkIconView:reorderable to %FALSE.
 *
 * Since: 2.8
 **/
void 
gtk_icon_view_enable_model_drag_dest (GtkIconView          *icon_view,
				      const GtkTargetEntry *targets,
				      gint                  n_targets,
				      GdkDragAction         actions)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  gtk_drag_dest_set (GTK_WIDGET (icon_view), 0, targets, n_targets, actions);

  icon_view->priv->dest_actions = actions;

  icon_view->priv->dest_set = TRUE;

  unset_reorderable (icon_view);  
}

/**
 * gtk_icon_view_unset_model_drag_source:
 * @icon_view: a #GtkIconView
 * 
 * Undoes the effect of gtk_icon_view_enable_model_drag_source(). Calling this
 * method sets #GtkIconView:reorderable to %FALSE.
 *
 * Since: 2.8
 **/
void
gtk_icon_view_unset_model_drag_source (GtkIconView *icon_view)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  if (icon_view->priv->source_set)
    {
      gtk_drag_source_unset (GTK_WIDGET (icon_view));
      icon_view->priv->source_set = FALSE;
    }

  unset_reorderable (icon_view);
}

/**
 * gtk_icon_view_unset_model_drag_dest:
 * @icon_view: a #GtkIconView
 * 
 * Undoes the effect of gtk_icon_view_enable_model_drag_dest(). Calling this
 * method sets #GtkIconView:reorderable to %FALSE.
 *
 * Since: 2.8
 **/
void
gtk_icon_view_unset_model_drag_dest (GtkIconView *icon_view)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  if (icon_view->priv->dest_set)
    {
      gtk_drag_dest_unset (GTK_WIDGET (icon_view));
      icon_view->priv->dest_set = FALSE;
    }

  unset_reorderable (icon_view);
}

/* These are useful to implement your own custom stuff. */
/**
 * gtk_icon_view_set_drag_dest_item:
 * @icon_view: a #GtkIconView
 * @path: (allow-none): The path of the item to highlight, or %NULL.
 * @pos: Specifies where to drop, relative to the item
 *
 * Sets the item that is highlighted for feedback.
 *
 * Since: 2.8
 */
void
gtk_icon_view_set_drag_dest_item (GtkIconView              *icon_view,
				  GtkTreePath              *path,
				  GtkIconViewDropPosition   pos)
{
  /* Note; this function is exported to allow a custom DND
   * implementation, so it can't touch TreeViewDragInfo
   */

  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  if (icon_view->priv->dest_item)
    {
      GtkTreePath *current_path;
      current_path = gtk_tree_row_reference_get_path (icon_view->priv->dest_item);
      gtk_tree_row_reference_free (icon_view->priv->dest_item);
      icon_view->priv->dest_item = NULL;      

      gtk_icon_view_queue_draw_path (icon_view, current_path);
      gtk_tree_path_free (current_path);
    }
  
  /* special case a drop on an empty model */
  icon_view->priv->empty_view_drop = FALSE;
  if (pos == GTK_ICON_VIEW_DROP_ABOVE && path
      && gtk_tree_path_get_depth (path) == 1
      && gtk_tree_path_get_indices (path)[0] == 0)
    {
      gint n_children;

      n_children = gtk_tree_model_iter_n_children (icon_view->priv->model,
                                                   NULL);

      if (n_children == 0)
        icon_view->priv->empty_view_drop = TRUE;
    }

  icon_view->priv->dest_pos = pos;

  if (path)
    {
      icon_view->priv->dest_item =
        gtk_tree_row_reference_new_proxy (G_OBJECT (icon_view), 
					  icon_view->priv->model, path);
      
      gtk_icon_view_queue_draw_path (icon_view, path);
    }
}

/**
 * gtk_icon_view_get_drag_dest_item:
 * @icon_view: a #GtkIconView
 * @path: (out) (allow-none): Return location for the path of
 *        the highlighted item, or %NULL.
 * @pos: (out) (allow-none): Return location for the drop position, or %NULL
 * 
 * Gets information about the item that is highlighted for feedback.
 *
 * Since: 2.8
 **/
void
gtk_icon_view_get_drag_dest_item (GtkIconView              *icon_view,
				  GtkTreePath             **path,
				  GtkIconViewDropPosition  *pos)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  if (path)
    {
      if (icon_view->priv->dest_item)
        *path = gtk_tree_row_reference_get_path (icon_view->priv->dest_item);
      else
	*path = NULL;
    }

  if (pos)
    *pos = icon_view->priv->dest_pos;
}

/**
 * gtk_icon_view_get_dest_item_at_pos:
 * @icon_view: a #GtkIconView
 * @drag_x: the position to determine the destination item for
 * @drag_y: the position to determine the destination item for
 * @path: (out) (allow-none): Return location for the path of the item,
 *    or %NULL.
 * @pos: (out) (allow-none): Return location for the drop position, or %NULL
 * 
 * Determines the destination item for a given position.
 * 
 * Returns: whether there is an item at the given position.
 *
 * Since: 2.8
 **/
gboolean
gtk_icon_view_get_dest_item_at_pos (GtkIconView              *icon_view,
				    gint                      drag_x,
				    gint                      drag_y,
				    GtkTreePath             **path,
				    GtkIconViewDropPosition  *pos)
{
  GtkIconViewItem *item;

  /* Note; this function is exported to allow a custom DND
   * implementation, so it can't touch TreeViewDragInfo
   */

  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), FALSE);
  g_return_val_if_fail (drag_x >= 0, FALSE);
  g_return_val_if_fail (drag_y >= 0, FALSE);
  g_return_val_if_fail (icon_view->priv->bin_window != NULL, FALSE);


  if (path)
    *path = NULL;

  item = _gtk_icon_view_get_item_at_coords (icon_view, 
					   drag_x + gtk_adjustment_get_value (icon_view->priv->hadjustment), 
					   drag_y + gtk_adjustment_get_value (icon_view->priv->vadjustment),
					   FALSE, NULL);

  if (item == NULL)
    return FALSE;

  if (path)
    *path = gtk_tree_path_new_from_indices (item->index, -1);

  if (pos)
    {
      if (drag_x < item->cell_area.x + item->cell_area.width / 4)
	*pos = GTK_ICON_VIEW_DROP_LEFT;
      else if (drag_x > item->cell_area.x + item->cell_area.width * 3 / 4)
	*pos = GTK_ICON_VIEW_DROP_RIGHT;
      else if (drag_y < item->cell_area.y + item->cell_area.height / 4)
	*pos = GTK_ICON_VIEW_DROP_ABOVE;
      else if (drag_y > item->cell_area.y + item->cell_area.height * 3 / 4)
	*pos = GTK_ICON_VIEW_DROP_BELOW;
      else
	*pos = GTK_ICON_VIEW_DROP_INTO;
    }

  return TRUE;
}

/**
 * gtk_icon_view_create_drag_icon:
 * @icon_view: a #GtkIconView
 * @path: a #GtkTreePath in @icon_view
 *
 * Creates a #cairo_surface_t representation of the item at @path.  
 * This image is used for a drag icon.
 *
 * Returns: (transfer full): a newly-allocated surface of the drag icon.
 * 
 * Since: 2.8
 **/
cairo_surface_t *
gtk_icon_view_create_drag_icon (GtkIconView *icon_view,
				GtkTreePath *path)
{
  GtkWidget *widget;
  cairo_t *cr;
  cairo_surface_t *surface;
  GList *l;
  gint index;

  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  widget = GTK_WIDGET (icon_view);

  if (!gtk_widget_get_realized (widget))
    return NULL;

  index = gtk_tree_path_get_indices (path)[0];

  for (l = icon_view->priv->items; l; l = l->next) 
    {
      GtkIconViewItem *item = l->data;
      
      if (index == item->index)
	{
	  GdkRectangle rect = { 
	    item->cell_area.x - icon_view->priv->item_padding, 
	    item->cell_area.y - icon_view->priv->item_padding, 
	    item->cell_area.width  + icon_view->priv->item_padding * 2, 
	    item->cell_area.height + icon_view->priv->item_padding * 2 
	  };

	  surface = gdk_window_create_similar_surface (icon_view->priv->bin_window,
                                                       CAIRO_CONTENT_COLOR_ALPHA,
                                                       rect.width,
                                                       rect.height);

	  cr = cairo_create (surface);

	  gtk_icon_view_paint_item (icon_view, cr, item, 
				    icon_view->priv->item_padding,
				    icon_view->priv->item_padding,
                                    FALSE);

	  cairo_destroy (cr);

	  return surface;
	}
    }
  
  return NULL;
}

/**
 * gtk_icon_view_get_reorderable:
 * @icon_view: a #GtkIconView
 *
 * Retrieves whether the user can reorder the list via drag-and-drop. 
 * See gtk_icon_view_set_reorderable().
 *
 * Returns: %TRUE if the list can be reordered.
 *
 * Since: 2.8
 **/
gboolean
gtk_icon_view_get_reorderable (GtkIconView *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), FALSE);

  return icon_view->priv->reorderable;
}

static const GtkTargetEntry item_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, 0 }
};


/**
 * gtk_icon_view_set_reorderable:
 * @icon_view: A #GtkIconView.
 * @reorderable: %TRUE, if the list of items can be reordered.
 *
 * This function is a convenience function to allow you to reorder models that
 * support the #GtkTreeDragSourceIface and the #GtkTreeDragDestIface.  Both
 * #GtkTreeStore and #GtkListStore support these.  If @reorderable is %TRUE, then
 * the user can reorder the model by dragging and dropping rows.  The
 * developer can listen to these changes by connecting to the model's
 * row_inserted and row_deleted signals. The reordering is implemented by setting up
 * the icon view as a drag source and destination. Therefore, drag and
 * drop can not be used in a reorderable view for any other purpose.
 *
 * This function does not give you any degree of control over the order -- any
 * reordering is allowed.  If more control is needed, you should probably
 * handle drag and drop manually.
 *
 * Since: 2.8
 **/
void
gtk_icon_view_set_reorderable (GtkIconView *icon_view,
			       gboolean     reorderable)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  reorderable = reorderable != FALSE;

  if (icon_view->priv->reorderable == reorderable)
    return;

  if (reorderable)
    {
      gtk_icon_view_enable_model_drag_source (icon_view,
					      GDK_BUTTON1_MASK,
					      item_targets,
					      G_N_ELEMENTS (item_targets),
					      GDK_ACTION_MOVE);
      gtk_icon_view_enable_model_drag_dest (icon_view,
					    item_targets,
					    G_N_ELEMENTS (item_targets),
					    GDK_ACTION_MOVE);
    }
  else
    {
      gtk_icon_view_unset_model_drag_source (icon_view);
      gtk_icon_view_unset_model_drag_dest (icon_view);
    }

  icon_view->priv->reorderable = reorderable;

  g_object_notify (G_OBJECT (icon_view), "reorderable");
}

/**
 * gtk_icon_view_set_activate_on_single_click:
 * @icon_view: a #GtkIconView
 * @single: %TRUE to emit item-activated on a single click
 *
 * Causes the #GtkIconView::item-activated signal to be emitted on
 * a single click instead of a double click.
 *
 * Since: 3.8
 **/
void
gtk_icon_view_set_activate_on_single_click (GtkIconView *icon_view,
                                            gboolean     single)
{
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));

  single = single != FALSE;

  if (icon_view->priv->activate_on_single_click == single)
    return;

  icon_view->priv->activate_on_single_click = single;
  g_object_notify (G_OBJECT (icon_view), "activate-on-single-click");
}

/**
 * gtk_icon_view_get_activate_on_single_click:
 * @icon_view: a #GtkIconView
 *
 * Gets the setting set by gtk_icon_view_set_activate_on_single_click().
 *
 * Returns: %TRUE if item-activated will be emitted on a single click
 *
 * Since: 3.8
 **/
gboolean
gtk_icon_view_get_activate_on_single_click (GtkIconView *icon_view)
{
  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), FALSE);

  return icon_view->priv->activate_on_single_click;
}

static gboolean
gtk_icon_view_buildable_custom_tag_start (GtkBuildable  *buildable,
                                          GtkBuilder    *builder,
                                          GObject       *child,
                                          const gchar   *tagname,
                                          GMarkupParser *parser,
                                          gpointer      *data)
{
  if (parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                tagname, parser, data))
    return TRUE;

  return _gtk_cell_layout_buildable_custom_tag_start (buildable, builder, child,
                                                      tagname, parser, data);
}

static void
gtk_icon_view_buildable_custom_tag_end (GtkBuildable *buildable,
                                        GtkBuilder   *builder,
                                        GObject      *child,
                                        const gchar  *tagname,
                                        gpointer     *data)
{
  if (!_gtk_cell_layout_buildable_custom_tag_end (buildable, builder,
                                                  child, tagname, data))
    parent_buildable_iface->custom_tag_end (buildable, builder,
                                            child, tagname, data);
}
