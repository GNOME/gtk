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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>

#include <atk/atk.h>

#include "gtkiconview.h"
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
#include "gtktextbuffer.h"
#include "gtkscrollable.h"
#include "gtksizerequest.h"
#include "gtktreednd.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"

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
 * ignore the tree's branches.
 */

#define SCROLL_EDGE_SIZE 15

#define GTK_ICON_VIEW_PRIORITY_LAYOUT (GDK_PRIORITY_REDRAW + 5)

typedef struct _GtkIconViewItem GtkIconViewItem;
struct _GtkIconViewItem
{
  /* First member is always the rectangle so it 
   * can be cast to a rectangle. */
  GdkRectangle cell_area;

  GtkTreeIter iter;
  gint index;
  
  gint row, col;

  guint selected : 1;
  guint selected_before_rubberbanding : 1;

};

typedef struct _GtkIconViewChild GtkIconViewChild;
struct _GtkIconViewChild
{
  GtkWidget    *widget;
  GdkRectangle  area;
};

struct _GtkIconViewPrivate
{
  GtkCellArea        *cell_area;
  GtkCellAreaContext *cell_area_context;

  gulong              add_editable_id;
  gulong              remove_editable_id;
  gulong              context_changed_id;

  GPtrArray          *row_contexts;

  gint width, height;

  GtkSelectionMode selection_mode;

  GdkWindow *bin_window;

  GList *children;

  GtkTreeModel *model;
  
  GList *items;
  
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;

  guint layout_idle_id;
  
  gint rubberband_x1, rubberband_y1;
  gint rubberband_x2, rubberband_y2;
  GdkDevice *rubberband_device;

  guint scroll_timeout_id;
  gint scroll_value_diff;
  gint event_last_x, event_last_y;

  GtkIconViewItem *anchor_item;
  GtkIconViewItem *cursor_item;

  GtkIconViewItem *last_single_clicked;

  GtkOrientation item_orientation;

  gint columns;
  gint item_width;
  gint spacing;
  gint row_spacing;
  gint column_spacing;
  gint margin;
  gint item_padding;

  gint text_column;
  gint markup_column;
  gint pixbuf_column;

  GtkCellRenderer *pixbuf_cell;
  GtkCellRenderer *text_cell;

  gint tooltip_column;

  /* Drag-and-drop. */
  GdkModifierType start_button_mask;
  gint pressed_button;
  gint press_start_x;
  gint press_start_y;

  GdkDragAction source_actions;
  GdkDragAction dest_actions;

  GtkTreeRowReference *dest_item;
  GtkIconViewDropPosition dest_pos;

  /* scroll to */
  GtkTreeRowReference *scroll_to_path;
  gfloat scroll_to_row_align;
  gfloat scroll_to_col_align;
  guint scroll_to_use_align : 1;

  guint source_set : 1;
  guint dest_set : 1;
  guint reorderable : 1;
  guint empty_view_drop :1;

  guint ctrl_pressed : 1;
  guint shift_pressed : 1;

  guint draw_focus : 1;

  /* GtkScrollablePolicy needs to be checked when
   * driving the scrollable adjustment values */
  guint hscroll_policy : 1;
  guint vscroll_policy : 1;

  guint doing_rubberband : 1;

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

  /* For scrollable interface */
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY
};

/* GObject vfuncs */
static void             gtk_icon_view_cell_layout_init          (GtkCellLayoutIface *iface);
static void             gtk_icon_view_dispose                   (GObject            *object);
static GObject         *gtk_icon_view_constructor               (GType               type,
								 guint               n_construct_properties,
								 GObjectConstructParam *construct_properties);
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
static void             gtk_icon_view_get_preferred_width       (GtkWidget          *widget,
								 gint               *minimum,
								 gint               *natural);
static void             gtk_icon_view_get_preferred_height      (GtkWidget          *widget,
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
static AtkObject       *gtk_icon_view_get_accessible            (GtkWidget          *widget);


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
static void                 gtk_icon_view_accessible_set_adjustment      (AtkObject              *accessible,
                                                                          GtkOrientation          orientation,
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
static void                 gtk_icon_view_queue_layout                   (GtkIconView            *icon_view);
static void                 gtk_icon_view_set_cursor_item                (GtkIconView            *icon_view,
									  GtkIconViewItem        *item,
									  GtkCellRenderer        *cursor_cell);
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
static void                 gtk_icon_view_cache_widths                   (GtkIconView            *icon_view);
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
static void                 gtk_icon_view_select_item                    (GtkIconView            *icon_view,
									  GtkIconViewItem        *item);
static void                 gtk_icon_view_unselect_item                  (GtkIconView            *icon_view,
									  GtkIconViewItem        *item);
static gboolean             gtk_icon_view_select_all_between             (GtkIconView            *icon_view,
									  GtkIconViewItem        *anchor,
									  GtkIconViewItem        *cursor);
static GtkIconViewItem *    gtk_icon_view_get_item_at_coords             (GtkIconView            *icon_view,
									  gint                    x,
									  gint                    y,
									  gboolean                only_in_cell,
									  GtkCellRenderer       **cell_at_pos);
static void                 gtk_icon_view_set_cell_data                  (GtkIconView            *icon_view,
									  GtkIconViewItem        *item);

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
static void                 gtk_icon_view_context_changed                (GtkCellAreaContext     *context,
									  GParamSpec             *pspec,
									  GtkIconView            *icon_view);

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

  g_type_class_add_private (klass, sizeof (GtkIconViewPrivate));

  gobject_class = (GObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  container_class = (GtkContainerClass *) klass;

  gobject_class->constructor = gtk_icon_view_constructor;
  gobject_class->dispose = gtk_icon_view_dispose;
  gobject_class->set_property = gtk_icon_view_set_property;
  gobject_class->get_property = gtk_icon_view_get_property;

  widget_class->destroy = gtk_icon_view_destroy;
  widget_class->realize = gtk_icon_view_realize;
  widget_class->unrealize = gtk_icon_view_unrealize;
  widget_class->style_updated = gtk_icon_view_style_updated;
  widget_class->get_accessible = gtk_icon_view_get_accessible;
  widget_class->get_preferred_width = gtk_icon_view_get_preferred_width;
  widget_class->get_preferred_height = gtk_icon_view_get_preferred_height;
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
						      GTK_PARAM_READWRITE));

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
						     GTK_PARAM_READWRITE));

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
						     GTK_PARAM_READWRITE));

  
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
						     GTK_PARAM_READWRITE));
  
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
						     GTK_PARAM_READWRITE));
  

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
						     GTK_PARAM_READWRITE));  

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
						     GTK_PARAM_READWRITE));

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
						     GTK_PARAM_READWRITE));

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
						     GTK_PARAM_READWRITE));

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
						     GTK_PARAM_READWRITE));

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
						      GTK_PARAM_READWRITE));

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
							 G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_TOOLTIP_COLUMN,
                                     g_param_spec_int ("tooltip-column",
                                                       P_("Tooltip Column"),
                                                       P_("The column in the model containing the tooltip texts for the items"),
                                                       -1,
                                                       G_MAXINT,
                                                       -1,
                                                       GTK_PARAM_READWRITE));

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
						     GTK_PARAM_READWRITE));

  /**
   * GtkIconView:cell-area:
   *
   * The #GtkCellArea used to layout cell renderers for this view.
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
                                                               GDK_TYPE_COLOR,
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
   * gtk_icon_view_item_activated() is called or the user double 
   * clicks an item. It is also emitted when a non-editable item
   * is selected and one of the keys: Space, Return or Enter is
   * pressed.
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
   * A <link linkend="keybinding-signals">keybinding signal</link>
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
   * A <link linkend="keybinding-signals">keybinding signal</link>
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
   * A <link linkend="keybinding-signals">keybinding signal</link>
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
   * A <link linkend="keybinding-signals">keybinding signal</link>
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
   * A <link linkend="keybinding-signals">keybinding signal</link>
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
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted when the user initiates a cursor movement.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control the cursor
   * programmatically.
   *
   * The default bindings for this signal include
   * <itemizedlist>
   * <listitem>Arrow keys which move by individual steps</listitem>
   * <listitem>Home/End keys which move to the first/last item</listitem>
   * <listitem>PageUp/PageDown which move by "pages"</listitem>
   * </itemizedlist>
   *
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
  icon_view->priv = G_TYPE_INSTANCE_GET_PRIVATE (icon_view,
                                                 GTK_TYPE_ICON_VIEW,
                                                 GtkIconViewPrivate);

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

  icon_view->priv->draw_focus = TRUE;

  icon_view->priv->row_contexts = 
    g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);
}

/* GObject methods */
static GObject *
gtk_icon_view_constructor (GType               type,
			   guint               n_construct_properties,
			   GObjectConstructParam *construct_properties)
{
  GtkIconView        *icon_view;
  GObject            *object;

  object = G_OBJECT_CLASS (gtk_icon_view_parent_class)->constructor
    (type, n_construct_properties, construct_properties);

  icon_view = (GtkIconView *) object;

  gtk_icon_view_ensure_cell_area (icon_view, NULL);

  return object;
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
      g_signal_handler_disconnect (priv->cell_area_context, priv->context_changed_id);
      priv->context_changed_id = 0;

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
      icon_view->priv->hscroll_policy = g_value_get_enum (value);
      gtk_widget_queue_resize (GTK_WIDGET (icon_view));
      break;
    case PROP_VSCROLL_POLICY:
      icon_view->priv->vscroll_policy = g_value_get_enum (value);
      gtk_widget_queue_resize (GTK_WIDGET (icon_view));
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

  if (icon_view->priv->layout_idle_id != 0)
    {
      g_source_remove (icon_view->priv->layout_idle_id);
      icon_view->priv->layout_idle_id = 0;
    }

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
  gdk_window_set_user_data (window, widget);

  gtk_widget_get_allocation (widget, &allocation);

  /* Make the window for the icon view */
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = MAX (icon_view->priv->width, allocation.width);
  attributes.height = MAX (icon_view->priv->height, allocation.height);
  attributes.event_mask = (GDK_EXPOSURE_MASK |
			   GDK_SCROLL_MASK |
			   GDK_POINTER_MOTION_MASK |
			   GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK |
			   GDK_KEY_PRESS_MASK |
			   GDK_KEY_RELEASE_MASK) |
    gtk_widget_get_events (widget);
  
  icon_view->priv->bin_window = gdk_window_new (window,
						&attributes, attributes_mask);
  gdk_window_set_user_data (icon_view->priv->bin_window, widget);

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
  gtk_style_context_set_background (context, icon_view->priv->bin_window);
  gtk_style_context_restore (context);

  gdk_window_show (icon_view->priv->bin_window);
}

static void
gtk_icon_view_unrealize (GtkWidget *widget)
{
  GtkIconView *icon_view;

  icon_view = GTK_ICON_VIEW (widget);

  gdk_window_set_user_data (icon_view->priv->bin_window, NULL);
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

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);

      gtk_style_context_set_background (context, gtk_widget_get_window (widget));
      gtk_style_context_set_background (context, icon_view->priv->bin_window);

      gtk_style_context_restore (context);
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

static void
gtk_icon_view_get_preferred_width (GtkWidget      *widget,
				   gint           *minimum,
				   gint           *natural)
{
  *minimum = *natural = GTK_ICON_VIEW (widget)->priv->width;
}

static void
gtk_icon_view_get_preferred_height (GtkWidget      *widget,
				   gint           *minimum,
				   gint           *natural)
{
  *minimum = *natural = GTK_ICON_VIEW (widget)->priv->height;
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

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (gtk_widget_get_window (widget),
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      gdk_window_resize (icon_view->priv->bin_window,
			 MAX (icon_view->priv->width, allocation->width),
			 MAX (icon_view->priv->height, allocation->height));
    }

  gtk_icon_view_layout (icon_view);
  
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

  icon_view = GTK_ICON_VIEW (widget);

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

      paint_area.x      = ((GdkRectangle *)item)->x      - icon_view->priv->item_padding;
      paint_area.y      = ((GdkRectangle *)item)->y      - icon_view->priv->item_padding;
      paint_area.width  = ((GdkRectangle *)item)->width  + icon_view->priv->item_padding * 2;
      paint_area.height = ((GdkRectangle *)item)->height + icon_view->priv->item_padding * 2;
      
      cairo_save (cr);

      cairo_rectangle (cr, paint_area.x, paint_area.y, paint_area.width, paint_area.height);
      cairo_clip (cr);

      if (gdk_cairo_get_clip_rectangle (cr, NULL))
        {
          gtk_icon_view_paint_item (icon_view, cr, item,
                                    ((GdkRectangle *)item)->x, ((GdkRectangle *)item)->y,
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
      GtkStateFlags state;
      GdkRectangle rect = { 0 };

      context = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);

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

      gtk_style_context_set_state (context, state);
      gtk_render_focus (context, cr,
                        rect.x, rect.y,
                        rect.width, rect.height);
    }
  
  if (icon_view->priv->doing_rubberband)
    gtk_icon_view_paint_rubberband (icon_view, cr);

  cairo_restore (cr);

  GTK_WIDGET_CLASS (gtk_icon_view_parent_class)->draw (widget, cr);

  return TRUE;
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

	  if (icon_view->priv->scroll_timeout_id == 0)
	    icon_view->priv->scroll_timeout_id = gdk_threads_add_timeout (30, rubberband_scroll_timeout, 
								icon_view);
 	}
      else 
	remove_scroll_timeout (icon_view);
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

static void
gtk_icon_view_context_changed (GtkCellAreaContext     *context,
			       GParamSpec             *pspec,
			       GtkIconView            *icon_view)
{
  if (!strcmp (pspec->name, "minimum-width") ||
      !strcmp (pspec->name, "natural-width") ||
      !strcmp (pspec->name, "minimum-height") ||
      !strcmp (pspec->name, "natural-height"))
    gtk_icon_view_invalidate_sizes (icon_view);
}

/**
 * gtk_icon_view_set_cursor:
 * @icon_view: A #GtkIconView
 * @path: A #GtkTreePath
 * @cell: (allow-none): One of the cell renderers of @icon_view, or %NULL
 * @start_editing: %TRUE if the specified cell should start being edited.
 *
 * Sets the current keyboard focus to be at @path, and selects it.  This is
 * useful when you want to focus the user's attention on a particular item.
 * If @cell is not %NULL, then focus is given to the cell specified by 
 * it. Additionally, if @start_editing is %TRUE, then editing should be 
 * started in the specified cell.  
 *
 * This function is often followed by <literal>gtk_widget_grab_focus 
 * (icon_view)</literal> in order to give keyboard focus to the widget.  
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

  gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

  if (gtk_tree_path_get_depth (path) == 1)
    item = g_list_nth_data (icon_view->priv->items,
			    gtk_tree_path_get_indices(path)[0]);
  
  if (!item)
    return;
  
  gtk_icon_view_set_cursor_item (icon_view, item, cell);
  gtk_icon_view_scroll_to_path (icon_view, path, FALSE, 0.0, 0.0);

  if (start_editing)
    {
      GtkCellAreaContext *context;

      context = g_ptr_array_index (icon_view->priv->row_contexts, item->row);
      gtk_icon_view_set_cell_data (icon_view, item);
      gtk_cell_area_activate (icon_view->priv->cell_area, context, 
			      GTK_WIDGET (icon_view), (GdkRectangle *)item, 
			      0 /* XXX flags */, TRUE);
    }
}

/**
 * gtk_icon_view_get_cursor:
 * @icon_view: A #GtkIconView
 * @path: (out) (allow-none): Return location for the current cursor path,
 *        or %NULL
 * @cell: (out) (allow-none): Return location the current focus cell, or %NULL
 *
 * Fills in @path and @cell with the current cursor path and cell. 
 * If the cursor isn't currently set, then *@path will be %NULL.  
 * If no cell currently has focus, then *@cell will be %NULL.
 *
 * The returned #GtkTreePath must be freed with gtk_tree_path_free().
 *
 * Return value: %TRUE if the cursor is set.
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

  if (cell != NULL && item != NULL)
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

  if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
    {
      item = gtk_icon_view_get_item_at_coords (icon_view, 
					       event->x, event->y,
					       FALSE,
					       &cell);

      /*
       * We consider only the the cells' area as the item area if the
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
	      gtk_icon_view_set_cursor_item (icon_view, item, cursor_cell);
	    }
	  else if (icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE &&
		   (event->state & GDK_SHIFT_MASK))
	    {
	      gtk_icon_view_unselect_all_internal (icon_view);

	      gtk_icon_view_set_cursor_item (icon_view, item, cursor_cell);
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
		  (event->state & GDK_CONTROL_MASK))
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
	      gtk_icon_view_set_cursor_item (icon_view, item, cursor_cell);
	      icon_view->priv->anchor_item = item;
	    }

	  /* Save press to possibly begin a drag */
	  if (icon_view->priv->pressed_button < 0)
	    {
	      icon_view->priv->pressed_button = event->button;
	      icon_view->priv->press_start_x = event->x;
	      icon_view->priv->press_start_y = event->y;
	    }

	  if (!icon_view->priv->last_single_clicked)
	    icon_view->priv->last_single_clicked = item;

	  /* cancel the current editing, if it exists */
	  gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

	  if (cell != NULL && gtk_cell_renderer_is_activatable (cell))
	    {
	      GtkCellAreaContext *context;

	      context = g_ptr_array_index (icon_view->priv->row_contexts, item->row);

	      gtk_icon_view_set_cell_data (icon_view, item);
	      gtk_cell_area_activate (icon_view->priv->cell_area, context,
				      GTK_WIDGET (icon_view),
				      (GdkRectangle *)item, 0/* XXX flags */, FALSE);
	    }
	}
      else
	{
	  if (icon_view->priv->selection_mode != GTK_SELECTION_BROWSE &&
	      !(event->state & GDK_CONTROL_MASK))
	    {
	      dirty = gtk_icon_view_unselect_all_internal (icon_view);
	    }
	  
	  if (icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE)
            gtk_icon_view_start_rubberbanding (icon_view, event->device, event->x, event->y);
	}

      /* don't draw keyboard focus around an clicked-on item */
      icon_view->priv->draw_focus = FALSE;
    }

  if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
    {
      item = gtk_icon_view_get_item_at_coords (icon_view,
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

  return event->button == 1;
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

  return GTK_WIDGET_CLASS (gtk_icon_view_parent_class)->key_press_event (widget, event);
}

static void
gtk_icon_view_update_rubberband (gpointer data)
{
  GtkIconView *icon_view;
  gint x, y;
  GdkRectangle old_area;
  GdkRectangle new_area;
  GdkRectangle common;
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

  gdk_rectangle_intersect (&old_area, &new_area, &common);
  if (common.width > 2 && common.height > 2)
    {
      cairo_region_t *common_region;

      /* make sure the border is invalidated */
      common.x += 1;
      common.y += 1;
      common.width -= 2;
      common.height -= 2;
      
      common_region = cairo_region_create_rectangle (&common);

      cairo_region_subtract (invalid_region, common_region);
      cairo_region_destroy (common_region);
    }
  
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

  gtk_device_grab_add (GTK_WIDGET (icon_view), device, TRUE);
}

static void
gtk_icon_view_stop_rubberbanding (GtkIconView *icon_view)
{
  if (!icon_view->priv->doing_rubberband)
    return;

  gtk_device_grab_remove (GTK_WIDGET (icon_view),
                          icon_view->priv->rubberband_device);

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
  GdkRectangle *item_area = (GdkRectangle *)item;
   
  if (MIN (x + width, item_area->x + item_area->width) - MAX (x, item_area->x) <= 0 ||
      MIN (y + height, item_area->y + item_area->height) - MAX (y, item_area->y) <= 0)
    return FALSE;

  context = g_ptr_array_index (icon_view->priv->row_contexts, item->row);

  gtk_icon_view_set_cell_data (icon_view, item);
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
    gtk_icon_view_select_item (icon_view, icon_view->priv->cursor_item);
}

static gboolean
gtk_icon_view_real_activate_cursor_item (GtkIconView *icon_view)
{
  GtkTreePath *path;
  GtkCellAreaContext *context;

  if (!icon_view->priv->cursor_item)
    return FALSE;

  context = g_ptr_array_index (icon_view->priv->row_contexts, icon_view->priv->cursor_item->row);

  gtk_icon_view_set_cell_data (icon_view, icon_view->priv->cursor_item);
  gtk_cell_area_activate (icon_view->priv->cell_area, context,
                          GTK_WIDGET (icon_view),
                          (GdkRectangle *)icon_view->priv->cursor_item,
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
      gtk_icon_view_select_item (icon_view, icon_view->priv->cursor_item);
      break;
    case GTK_SELECTION_SINGLE:
      if (icon_view->priv->cursor_item->selected)
	gtk_icon_view_unselect_item (icon_view, icon_view->priv->cursor_item);
      else
	gtk_icon_view_select_item (icon_view, icon_view->priv->cursor_item);
      break;
    case GTK_SELECTION_MULTIPLE:
      icon_view->priv->cursor_item->selected = !icon_view->priv->cursor_item->selected;
      g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0); 
      
      gtk_icon_view_item_selected_changed (icon_view, icon_view->priv->cursor_item);      
      gtk_icon_view_queue_draw_item (icon_view, icon_view->priv->cursor_item);
      break;
    }
}

/* Internal functions */
static void
gtk_icon_view_process_updates (GtkIconView *icon_view)
{
  /* Prior to drawing, we check if a layout has been scheduled.  If so,
   * do it now that all cell view items have valid sizes before we proceeed
   * (and resize the bin_window if required).
   */
  if (icon_view->priv->layout_idle_id != 0)
    gtk_icon_view_layout (icon_view);

  gdk_window_process_updates (icon_view->priv->bin_window, TRUE);
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
  AtkObject *atk_obj;

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

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (icon_view));
  gtk_icon_view_accessible_set_adjustment (atk_obj,
                                           GTK_ORIENTATION_HORIZONTAL,
                                           adjustment);

  g_object_notify (G_OBJECT (icon_view), "hadjustment");
}

static void
gtk_icon_view_set_vadjustment (GtkIconView   *icon_view,
                               GtkAdjustment *adjustment)
{
  GtkIconViewPrivate *priv = icon_view->priv;
  AtkObject *atk_obj;

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

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (icon_view));
  gtk_icon_view_accessible_set_adjustment (atk_obj,
                                           GTK_ORIENTATION_VERTICAL,
                                           adjustment);

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

      gtk_icon_view_process_updates (icon_view);
    }
}

static GList *
gtk_icon_view_layout_single_row (GtkIconView *icon_view, 
				 GList       *first_item, 
				 gint         item_width,
				 gint         row,
				 gint        *y, 
				 gint        *maximum_width)
{
  GtkAllocation allocation;
  GtkCellAreaContext *context;
  GtkIconViewPrivate *priv = icon_view->priv;
  GtkWidget *widget = GTK_WIDGET (icon_view);
  gint x, current_width;
  GList *items, *last_item;
  gint col;
  gint max_height = 0;
  gboolean rtl;

  rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  x = 0;
  col = 0;
  items = first_item;
  current_width = 0;

  x += priv->margin;
  current_width += 2 * priv->margin;

  gtk_widget_get_allocation (widget, &allocation);

  context = gtk_cell_area_copy_context (priv->cell_area, priv->cell_area_context);
  g_ptr_array_add (priv->row_contexts, context);

  /* In the first loop we iterate horizontally until we hit allocation width
   * and collect the aligned height-for-width */
  items = first_item;
  while (items)
    {
      GtkIconViewItem *item = items->data;
      GdkRectangle    *item_area = (GdkRectangle *)item;

      item_area->width = item_width;

      current_width += item_area->width + icon_view->priv->item_padding * 2;

      if (items != first_item)
	{
	  if ((icon_view->priv->columns <= 0 && current_width > allocation.width) ||
	      (icon_view->priv->columns > 0 && col >= icon_view->priv->columns))
	    break;
	}

      /* Get this item's particular width & height (all alignments are cached by now) */
      gtk_icon_view_set_cell_data (icon_view, item);
      gtk_cell_area_get_preferred_height_for_width (priv->cell_area,
						    context,
						    widget, item_width, 
						    NULL, NULL);

      current_width += icon_view->priv->column_spacing;

      item_area->y = *y + icon_view->priv->item_padding;
      item_area->x = x  + icon_view->priv->item_padding;

      x = current_width - icon_view->priv->margin; 
	      
      if (current_width > *maximum_width)
	*maximum_width = current_width;

      item->row = row;
      item->col = col;

      col ++;
      items = items->next;
    }

  last_item = items;

  gtk_cell_area_context_get_preferred_height_for_width (context, item_width, &max_height, NULL);
  gtk_cell_area_context_allocate (context, item_width, max_height);

  /* In the second loop the item height has been aligned and derived and
   * we just set the height and handle rtl layout */
  for (items = first_item; items != last_item; items = items->next)
    {
      GtkIconViewItem *item = items->data;
      GdkRectangle    *item_area = (GdkRectangle *)item;

      if (rtl)
	{
	  item_area->x = *maximum_width - item_area->width - item_area->x;
	  item->col = col - 1 - item->col;
	}

      /* All items in the same row get the same height */
      item_area->height = max_height;
    }

  /* Adjust the new y coordinate. */
  *y += max_height + icon_view->priv->row_spacing + icon_view->priv->item_padding * 2;
  
  return last_item;
}

static void
adjust_wrap_width (GtkIconView *icon_view)
{
  if (icon_view->priv->text_cell)
    {
      gint wrap_width = 50;

      /* Here we go with the same old guess, try the icon size and set double
       * the size of the first icon found in the list, naive but works much
       * of the time */
      if (icon_view->priv->items && icon_view->priv->pixbuf_cell)
	{
	  gtk_icon_view_set_cell_data (icon_view, icon_view->priv->items->data);
	  gtk_cell_renderer_get_preferred_width (icon_view->priv->pixbuf_cell,
						 GTK_WIDGET (icon_view),
						 &wrap_width, NULL);
	  
	  wrap_width = MAX (wrap_width * 2, 50);
	}
      
      g_object_set (icon_view->priv->text_cell, "wrap-width", wrap_width, NULL);
      g_object_set (icon_view->priv->text_cell, "width", wrap_width, NULL);
    }
}

static void
gtk_icon_view_layout (GtkIconView *icon_view)
{
  GtkAllocation allocation;
  GtkWidget *widget;
  GList *icons;
  gint y = 0, maximum_width = 0;
  gint row;
  gint item_width;
  gboolean size_changed = FALSE;

  if (icon_view->priv->layout_idle_id != 0)
    {
      g_source_remove (icon_view->priv->layout_idle_id);
      icon_view->priv->layout_idle_id = 0;
    }
  
  if (icon_view->priv->model == NULL)
    return;

  widget = GTK_WIDGET (icon_view);

  item_width = icon_view->priv->item_width;

  /* Update the wrap width for the text cell before going and requesting sizes */
  adjust_wrap_width (icon_view);

  /* Update the context widths for any invalidated items */
  gtk_icon_view_cache_widths (icon_view);

  /* Fetch the new item width if needed */
  if (item_width < 0)
    gtk_cell_area_context_get_preferred_width (icon_view->priv->cell_area_context, 
					       &item_width, NULL);

  gtk_cell_area_context_allocate (icon_view->priv->cell_area_context, item_width, -1);

  icons = icon_view->priv->items;
  y += icon_view->priv->margin;
  row = 0;

  /* Clear the per row contexts */
  g_ptr_array_set_size (icon_view->priv->row_contexts, 0);
  
  do
    {
      icons = gtk_icon_view_layout_single_row (icon_view, icons, 
					       item_width, row,
					       &y, &maximum_width);
      row++;
    }
  while (icons != NULL);

  if (maximum_width != icon_view->priv->width)
    {
      icon_view->priv->width = maximum_width;
      size_changed = TRUE;
    }

  y += icon_view->priv->margin;
  
  if (y != icon_view->priv->height)
    {
      icon_view->priv->height = y;
      size_changed = TRUE;
    }

  gtk_icon_view_set_hadjustment_values (icon_view);
  gtk_icon_view_set_vadjustment_values (icon_view);

  if (size_changed)
    gtk_widget_queue_resize_no_redraw (widget);

  gtk_widget_get_allocation (widget, &allocation);
  if (gtk_widget_get_realized (GTK_WIDGET (icon_view)))
    gdk_window_resize (icon_view->priv->bin_window,
                       MAX (icon_view->priv->width, allocation.width),
                       MAX (icon_view->priv->height, allocation.height));

  if (icon_view->priv->scroll_to_path)
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
  
  gtk_widget_queue_draw (widget);
}

/* This ensures that all widths have been cached in the
 * context and we have proper alignments to go on.
 */
static void
gtk_icon_view_cache_widths (GtkIconView *icon_view)
{
  GList *items;

  g_signal_handler_block (icon_view->priv->cell_area_context, 
			  icon_view->priv->context_changed_id);

  for (items = icon_view->priv->items; items; items = items->next)
    {
      GtkIconViewItem *item = items->data;

      /* Only fetch the width of items with invalidated sizes */
      if (item->cell_area.width < 0)
	{
	  gtk_icon_view_set_cell_data (icon_view, item);
	  gtk_cell_area_get_preferred_width (icon_view->priv->cell_area, 
					     icon_view->priv->cell_area_context,
					     GTK_WIDGET (icon_view), NULL, NULL);
	}
    }

  g_signal_handler_unblock (icon_view->priv->cell_area_context, 
			    icon_view->priv->context_changed_id);
}

static void
gtk_icon_view_invalidate_sizes (GtkIconView *icon_view)
{
  /* Clear all item sizes */
  g_list_foreach (icon_view->priv->items,
		  (GFunc)gtk_icon_view_item_invalidate_size, NULL);

  /* Reset the context */
  g_signal_handler_block (icon_view->priv->cell_area_context, 
			  icon_view->priv->context_changed_id);
  gtk_cell_area_context_reset (icon_view->priv->cell_area_context);
  g_signal_handler_unblock (icon_view->priv->cell_area_context, 
			    icon_view->priv->context_changed_id);

  /* Re-layout the items */
  gtk_icon_view_queue_layout (icon_view);
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

  if (priv->model == NULL)
    return;

  gtk_icon_view_set_cell_data (icon_view, item);

  style_context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (style_context);
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_VIEW);
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_CELL);

  if (item->selected)
    {
      if (gtk_widget_has_focus (widget) &&
          item == icon_view->priv->cursor_item)
        {
          state |= GTK_STATE_FLAG_FOCUSED;
          flags |= GTK_CELL_RENDERER_FOCUSED;
        }

      state |= GTK_STATE_FLAG_SELECTED;
      flags |= GTK_CELL_RENDERER_SELECTED;

      gtk_style_context_set_state (style_context, state);
      gtk_render_background (style_context, cr,
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
  GdkRectangle *item_area = (GdkRectangle *)item;

  rect.x      = item_area->x - icon_view->priv->item_padding;
  rect.y      = item_area->y - icon_view->priv->item_padding;
  rect.width  = item_area->width  + icon_view->priv->item_padding * 2;
  rect.height = item_area->height + icon_view->priv->item_padding * 2;

  if (icon_view->priv->bin_window)
    gdk_window_invalidate_rect (icon_view->priv->bin_window, &rect, TRUE);
}

static gboolean
layout_callback (gpointer user_data)
{
  GtkIconView *icon_view;

  icon_view = GTK_ICON_VIEW (user_data);
  
  icon_view->priv->layout_idle_id = 0;

  gtk_icon_view_layout (icon_view);
  
  return FALSE;
}

static void
gtk_icon_view_queue_layout (GtkIconView *icon_view)
{
  if (icon_view->priv->layout_idle_id != 0)
    return;

  icon_view->priv->layout_idle_id =
      gdk_threads_add_idle_full (GTK_ICON_VIEW_PRIORITY_LAYOUT,
                                 layout_callback, icon_view, NULL);
}

static void
gtk_icon_view_set_cursor_item (GtkIconView     *icon_view,
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
      atk_focus_tracker_notify (item_obj);
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

static GtkIconViewItem *
gtk_icon_view_get_item_at_coords (GtkIconView          *icon_view,
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
      GdkRectangle    *item_area = (GdkRectangle *)item;

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
	      gtk_icon_view_set_cell_data (icon_view, item);

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

static void
gtk_icon_view_select_item (GtkIconView      *icon_view,
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


static void
gtk_icon_view_unselect_item (GtkIconView      *icon_view,
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
  gboolean iters_persist;
  GList *list;

  /* ignore changes in branches */
  if (gtk_tree_path_get_depth (path) > 1)
    return;

  iters_persist = gtk_tree_model_get_flags (icon_view->priv->model) & GTK_TREE_MODEL_ITERS_PERSIST;
  
  index = gtk_tree_path_get_indices(path)[0];

  item = gtk_icon_view_item_new ();

  if (iters_persist)
    item->iter = *iter;

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

  gtk_icon_view_queue_layout (icon_view);
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

  gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);

  if (item == icon_view->priv->anchor_item)
    icon_view->priv->anchor_item = NULL;

  if (item == icon_view->priv->cursor_item)
    icon_view->priv->cursor_item = NULL;

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
  
  gtk_icon_view_queue_layout (icon_view);

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

  gtk_icon_view_queue_layout (icon_view);

  verify_items (icon_view);  
}

static void
gtk_icon_view_build_items (GtkIconView *icon_view)
{
  GtkTreeIter iter;
  int i;
  gboolean iters_persist;
  GList *items = NULL;

  iters_persist = gtk_tree_model_get_flags (icon_view->priv->model) & GTK_TREE_MODEL_ITERS_PERSIST;
  
  if (!gtk_tree_model_get_iter_first (icon_view->priv->model,
				      &iter))
    return;

  i = 0;
  
  do
    {
      GtkIconViewItem *item = gtk_icon_view_item_new ();

      if (iters_persist)
	item->iter = iter;

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
      if ((state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
        icon_view->priv->ctrl_pressed = TRUE;
      if ((state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK)
        icon_view->priv->shift_pressed = TRUE;
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

  icon_view->priv->ctrl_pressed = FALSE;
  icon_view->priv->shift_pressed = FALSE;

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
  GtkCellRenderer *cell;
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

      item = list ? list->data : NULL;

      /* Give focus to the first cell initially */
      gtk_icon_view_set_cell_data (icon_view, item);
      gtk_cell_area_focus (icon_view->priv->cell_area, direction);
    }
  else
    {
      item = icon_view->priv->cursor_item;
      step = count > 0 ? 1 : -1;      

      /* Save the current focus cell in case we hit the edge */
      cell = gtk_cell_area_get_focus_cell (icon_view->priv->cell_area);

      while (item)
	{
	  gtk_icon_view_set_cell_data (icon_view, item);

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

  if (icon_view->priv->ctrl_pressed ||
      !icon_view->priv->shift_pressed ||
      !icon_view->priv->anchor_item ||
      icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_view->priv->anchor_item = item;

  cell = gtk_cell_area_get_focus_cell (icon_view->priv->cell_area);
  gtk_icon_view_set_cursor_item (icon_view, item, cell);

  if (!icon_view->priv->ctrl_pressed &&
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

  if (icon_view->priv->ctrl_pressed ||
      !icon_view->priv->shift_pressed ||
      !icon_view->priv->anchor_item ||
      icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_view->priv->anchor_item = item;

  gtk_icon_view_set_cursor_item (icon_view, item, NULL);

  if (!icon_view->priv->ctrl_pressed &&
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

      item = list ? list->data : NULL;

      /* Give focus to the first cell initially */
      gtk_icon_view_set_cell_data (icon_view, item);
      gtk_cell_area_focus (icon_view->priv->cell_area, direction);
    }
  else
    {
      item = icon_view->priv->cursor_item;
      step = count > 0 ? 1 : -1;

      /* Save the current focus cell in case we hit the edge */
      cell = gtk_cell_area_get_focus_cell (icon_view->priv->cell_area);

      while (item)
	{
	  gtk_icon_view_set_cell_data (icon_view, item);

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

  if (icon_view->priv->ctrl_pressed ||
      !icon_view->priv->shift_pressed ||
      !icon_view->priv->anchor_item ||
      icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_view->priv->anchor_item = item;

  cell = gtk_cell_area_get_focus_cell (icon_view->priv->cell_area);
  gtk_icon_view_set_cursor_item (icon_view, item, cell);

  if (!icon_view->priv->ctrl_pressed &&
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

  if (icon_view->priv->ctrl_pressed ||
      !icon_view->priv->shift_pressed ||
      !icon_view->priv->anchor_item ||
      icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_view->priv->anchor_item = item;

  gtk_icon_view_set_cursor_item (icon_view, item, NULL);

  if (!icon_view->priv->ctrl_pressed &&
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
    gtk_adjustment_set_value (vadj,
                              gtk_adjustment_get_value (vadj)
                                + y + item_area.y);
  else if (y + item_area.y + item_area.height > allocation.height)
    gtk_adjustment_set_value (vadj,
                              gtk_adjustment_get_value (vadj)
                                + y + item_area.y + item_area.height - allocation.height);

  if (x + item_area.x < 0)
    gtk_adjustment_set_value (hadj,
                              gtk_adjustment_get_value (hadj)
                                + x + item_area.x);
  else if (x + item_area.x + item_area.width > allocation.width)
    gtk_adjustment_set_value (hadj,
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
  priv->context_changed_id =
    g_signal_connect (priv->cell_area_context, "notify",
                      G_CALLBACK (gtk_icon_view_context_changed), icon_view);
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

static void
gtk_icon_view_set_cell_data (GtkIconView     *icon_view,
			     GtkIconViewItem *item)
{
  gboolean iters_persist;
  GtkTreeIter iter;

  iters_persist = gtk_tree_model_get_flags (icon_view->priv->model) & GTK_TREE_MODEL_ITERS_PERSIST;
  
  if (!iters_persist)
    {
      GtkTreePath *path;

      path = gtk_tree_path_new_from_indices (item->index, -1);
      if (!gtk_tree_model_get_iter (icon_view->priv->model, &iter, path))
        return;
      gtk_tree_path_free (path);
    }
  else
    iter = item->iter;

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
 * Return value: A newly created #GtkIconView widget
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
 * Return value: A newly created #GtkIconView widget
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
 * Return value: A newly created #GtkIconView widget.
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
 * Return value: The #GtkTreePath corresponding to the icon or %NULL
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

  item = gtk_icon_view_get_item_at_coords (icon_view, x, y, TRUE, NULL);

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
 * Return value: %TRUE if an item exists at the specified position
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

  item = gtk_icon_view_get_item_at_coords (icon_view, x, y, TRUE, &renderer);

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
  GtkIconViewItem *item = NULL;
  gint x, y;
 
  g_return_if_fail (GTK_IS_ICON_VIEW (icon_view));
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));
  g_return_if_fail (cell == NULL || GTK_IS_CELL_RENDERER (cell));

  if (gtk_tree_path_get_depth (path) > 0)
    item = g_list_nth_data (icon_view->priv->items,
                            gtk_tree_path_get_indices(path)[0]);
 
  if (!item)
    return;

  if (cell)
    {
      GtkCellAreaContext *context;

      context = g_ptr_array_index (icon_view->priv->row_contexts, item->row);
      gtk_icon_view_set_cell_data (icon_view, item);
      gtk_cell_area_get_cell_allocation (icon_view->priv->cell_area, context,
					 GTK_WIDGET (icon_view),
					 cell, (GdkRectangle *)item, &rect);
    }
  else
    {
      rect.x = item->cell_area.x - icon_view->priv->item_padding;
      rect.y = item->cell_area.y - icon_view->priv->item_padding;
      rect.width  = item->cell_area.width  + icon_view->priv->item_padding * 2;
      rect.height = item->cell_area.height + icon_view->priv->item_padding * 2;
    }
  
  if (icon_view->priv->bin_window)
    {
      gdk_window_get_position (icon_view->priv->bin_window, &x, &y);
      rect.x += x;
      rect.y += y; 
    }

  gtk_tooltip_set_tip_area (tooltip, &rect); 
}


/**
 * gtk_icon_view_get_tooltip_context:
 * @icon_view: an #GtkIconView
 * @x: (inout): the x coordinate (relative to widget coordinates)
 * @y: (inout): the y coordinate (relative to widget coordinates)
 * @keyboard_tip: whether this is a keyboard tooltip or not
 * @model: (out) (allow-none): a pointer to receive a #GtkTreeModel or %NULL
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
 * to be relative to @icon_view's bin_window if @keyboard_tooltip is %FALSE.
 *
 * Return value: whether or not the given tooltip context points to a item
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
 * @column: an integer, which is a valid column number for @icon_view's model
 *
 * If you only plan to have simple (text-only) tooltips on full items, you
 * can use this function to have #GtkIconView handle these automatically
 * for you. @column should be set to the column in @icon_view's model
 * containing the tooltip texts, or -1 to disable this feature.
 *
 * When enabled, #GtkWidget::has-tooltip will be set to %TRUE and
 * @icon_view will connect a #GtkWidget::query-tooltip signal handler.
 *
 * Note that the signal handler sets the text with gtk_tooltip_set_markup(),
 * so &amp;, &lt;, etc have to be escaped in the text.
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
 * Returns the column of @icon_view's model which is being used for
 * displaying tooltips on @icon_view's rows.
 *
 * Return value: the index of the tooltip column that is currently being
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
 * Return value: %TRUE, if valid paths were placed in @start_path and @end_path
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
      GdkRectangle    *item_area = (GdkRectangle *)item;

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
 * Return value: the current selection mode
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
      
      g_list_foreach (icon_view->priv->items, (GFunc)gtk_icon_view_item_free, NULL);
      g_list_free (icon_view->priv->items);
      icon_view->priv->items = NULL;
      icon_view->priv->anchor_item = NULL;
      icon_view->priv->cursor_item = NULL;
      icon_view->priv->last_single_clicked = NULL;
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

      gtk_icon_view_queue_layout (icon_view);
    }

  g_object_notify (G_OBJECT (icon_view), "model");  

  if (gtk_widget_get_realized (GTK_WIDGET (icon_view)))
    gtk_widget_queue_resize (GTK_WIDGET (icon_view));
}

/**
 * gtk_icon_view_get_model:
 * @icon_view: a #GtkIconView
 *
 * Returns the model the #GtkIconView is based on.  Returns %NULL if the
 * model is unset.
 *
 * Return value: (transfer none): A #GtkTreeModel, or %NULL if none is
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
 * Returns: the text column, or -1 if it's unset.
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
 * Returns: the markup column, or -1 if it's unset.
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
 * Returns: the pixbuf column, or -1 if it's unset.
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
    gtk_icon_view_select_item (icon_view, item);
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
  
  gtk_icon_view_unselect_item (icon_view, item);
}

/**
 * gtk_icon_view_get_selected_items:
 * @icon_view: A #GtkIconView.
 *
 * Creates a list of paths of all selected items. Additionally, if you are
 * planning on modifying the model after calling this function, you may
 * want to convert the returned list into a list of #GtkTreeRowReference<!-- -->s.
 * To do this, you can use gtk_tree_row_reference_new().
 *
 * To free the return value, use:
 * |[
 * g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
 * g_list_free (list);
 * ]|
 *
 * Return value: (element-type GtkTreePath) (transfer full): A #GList containing a #GtkTreePath for each selected row.
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
 * Return value: %TRUE if @path is selected.
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

      if (GTK_IS_ORIENTABLE (icon_view->priv->cell_area))
	gtk_orientable_set_orientation (GTK_ORIENTABLE (icon_view->priv->cell_area), 
					icon_view->priv->item_orientation);

      gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);
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
 * Return value: the relative position of texts and icons 
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

      gtk_cell_area_stop_editing (icon_view->priv->cell_area, TRUE);
      gtk_icon_view_queue_layout (icon_view);
      
      g_object_notify (G_OBJECT (icon_view), "columns");
    }  
}

/**
 * gtk_icon_view_get_columns:
 * @icon_view: a #GtkIconView
 * 
 * Returns the value of the ::columns property.
 * 
 * Return value: the number of columns, or -1
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
 * Return value: the width of a single item, or -1
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
 * Return value: the space between cells 
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
 * Return value: the space between rows
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
 * Return value: the space between columns
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
 * Return value: the space at the borders 
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
 * around each of the icon view's items.
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
 * Return value: the padding around items
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
 * since the data doesn't result from a drop.
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
gtk_icon_view_autoscroll (GtkIconView *icon_view)
{
  GdkWindow *window;
  gint px, py, x, y, width, height;
  gint hoffset, voffset;

  window = gtk_widget_get_window (GTK_WIDGET (icon_view));

  gdk_window_get_pointer (window, &px, &py, NULL);
  gdk_window_get_geometry (window, &x, &y, &width, &height);

  /* see if we are near the edge. */
  voffset = py - (y + 2 * SCROLL_EDGE_SIZE);
  if (voffset > 0)
    voffset = MAX (py - (y + height - 2 * SCROLL_EDGE_SIZE), 0);

  hoffset = px - (x + 2 * SCROLL_EDGE_SIZE);
  if (hoffset > 0)
    hoffset = MAX (px - (x + width - 2 * SCROLL_EDGE_SIZE), 0);

  if (voffset != 0)
    gtk_adjustment_set_value (icon_view->priv->vadjustment,
                              gtk_adjustment_get_value (icon_view->priv->vadjustment) + voffset);

  if (hoffset != 0)
    gtk_adjustment_set_value (icon_view->priv->hadjustment,
                              gtk_adjustment_get_value (icon_view->priv->hadjustment) + hoffset);
}


static gboolean
drag_scroll_timeout (gpointer data)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (data);

  gtk_icon_view_autoscroll (icon_view);

  return TRUE;
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

  context = gtk_drag_begin (widget,
                            gtk_drag_source_get_target_list (widget),
                            icon_view->priv->source_actions,
                            button,
                            (GdkEvent*)event);

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

  item = gtk_icon_view_get_item_at_coords (icon_view,
					   icon_view->priv->press_start_x,
					   icon_view->priv->press_start_y,
					   TRUE,
					   NULL);

  g_return_if_fail (item != NULL);

  x = icon_view->priv->press_start_x - item->cell_area.x + 1;
  y = icon_view->priv->press_start_y - item->cell_area.y + 1;
  
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
	  icon_view->priv->scroll_timeout_id =
	    gdk_threads_add_timeout (50, drag_scroll_timeout, icon_view);
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
 * @icon_view: a #GtkIconTreeView
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
 * Return value: whether there is an item at the given position.
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

  item = gtk_icon_view_get_item_at_coords (icon_view, 
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
 * Return value: (transfer full): a newly-allocated surface of the drag icon.
 * 
 * Since: 2.8
 **/
cairo_surface_t *
gtk_icon_view_create_drag_icon (GtkIconView *icon_view,
				GtkTreePath *path)
{
  GtkWidget *widget;
  GtkStyleContext *context;
  cairo_t *cr;
  cairo_surface_t *surface;
  GList *l;
  gint index;

  g_return_val_if_fail (GTK_IS_ICON_VIEW (icon_view), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  widget = GTK_WIDGET (icon_view);
  context = gtk_widget_get_style_context (widget);

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
                                                       CAIRO_CONTENT_COLOR,
                                                       rect.width + 2,
                                                       rect.height + 2);

	  cr = cairo_create (surface);
	  cairo_set_line_width (cr, 1.);

          gtk_render_background (context, cr, 0, 0,
                                 rect.width + 2, rect.height + 2);

          cairo_save (cr);

          cairo_rectangle (cr, 1, 1, rect.width, rect.height);
          cairo_clip (cr);

	  gtk_icon_view_paint_item (icon_view, cr, item, 
				    icon_view->priv->item_padding + 1, 
				    icon_view->priv->item_padding + 1, FALSE);

          cairo_restore (cr);

	  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0); /* black */
	  cairo_rectangle (cr, 0.5, 0.5, rect.width + 1, rect.height + 1);
	  cairo_stroke (cr);

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
 * Return value: %TRUE if the list can be reordered.
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


/* Accessibility Support */

static gpointer accessible_parent_class;
static gpointer accessible_item_parent_class;
static GQuark accessible_private_data_quark = 0;

#define GTK_TYPE_ICON_VIEW_ITEM_ACCESSIBLE      (gtk_icon_view_item_accessible_get_type ())
#define GTK_ICON_VIEW_ITEM_ACCESSIBLE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ICON_VIEW_ITEM_ACCESSIBLE, GtkIconViewItemAccessible))
#define GTK_IS_ICON_VIEW_ITEM_ACCESSIBLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ICON_VIEW_ITEM_ACCESSIBLE))

static GType gtk_icon_view_item_accessible_get_type (void);

enum {
    ACTION_ACTIVATE,
    LAST_ACTION
};

typedef struct
{
  AtkObject parent;

  GtkIconViewItem *item;

  GtkWidget *widget;

  AtkStateSet *state_set;

  gchar *text;

  GtkTextBuffer *text_buffer;

  gchar *action_descriptions[LAST_ACTION];
  gchar *image_description;
  guint action_idle_handler;
} GtkIconViewItemAccessible;

static const gchar *const gtk_icon_view_item_accessible_action_names[] = 
{
  "activate",
  NULL
};

static const gchar *const gtk_icon_view_item_accessible_action_descriptions[] =
{
  "Activate item",
  NULL
};
typedef struct _GtkIconViewItemAccessibleClass
{
  AtkObjectClass parent_class;

} GtkIconViewItemAccessibleClass;

static gboolean gtk_icon_view_item_accessible_is_showing (GtkIconViewItemAccessible *item);

static gboolean
gtk_icon_view_item_accessible_idle_do_action (gpointer data)
{
  GtkIconViewItemAccessible *item;
  GtkIconView *icon_view;
  GtkTreePath *path;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (data);
  item->action_idle_handler = 0;

  if (item->widget != NULL)
    {
      icon_view = GTK_ICON_VIEW (item->widget);
      path = gtk_tree_path_new_from_indices (item->item->index, -1);
      gtk_icon_view_item_activated (icon_view, path);
      gtk_tree_path_free (path);
    }

  return FALSE;
}

static gboolean
gtk_icon_view_item_accessible_action_do_action (AtkAction *action,
                                                gint       i)
{
  GtkIconViewItemAccessible *item;

  if (i < 0 || i >= LAST_ACTION) 
    return FALSE;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (action);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return FALSE;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return FALSE;

  switch (i)
    {
    case ACTION_ACTIVATE:
      if (!item->action_idle_handler)
        item->action_idle_handler = gdk_threads_add_idle (gtk_icon_view_item_accessible_idle_do_action, item);
      break;
    default:
      g_assert_not_reached ();
      return FALSE;

    }        
  return TRUE;
}

static gint
gtk_icon_view_item_accessible_action_get_n_actions (AtkAction *action)
{
        return LAST_ACTION;
}

static const gchar *
gtk_icon_view_item_accessible_action_get_description (AtkAction *action,
                                                      gint       i)
{
  GtkIconViewItemAccessible *item;

  if (i < 0 || i >= LAST_ACTION) 
    return NULL;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (action);

  if (item->action_descriptions[i])
    return item->action_descriptions[i];
  else
    return gtk_icon_view_item_accessible_action_descriptions[i];
}

static const gchar *
gtk_icon_view_item_accessible_action_get_name (AtkAction *action,
                                               gint       i)
{
  if (i < 0 || i >= LAST_ACTION) 
    return NULL;

  return gtk_icon_view_item_accessible_action_names[i];
}

static gboolean
gtk_icon_view_item_accessible_action_set_description (AtkAction   *action,
                                                      gint         i,
                                                      const gchar *description)
{
  GtkIconViewItemAccessible *item;

  if (i < 0 || i >= LAST_ACTION) 
    return FALSE;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (action);

  g_free (item->action_descriptions[i]);

  item->action_descriptions[i] = g_strdup (description);

  return TRUE;
}

static void
atk_action_item_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_icon_view_item_accessible_action_do_action;
  iface->get_n_actions = gtk_icon_view_item_accessible_action_get_n_actions;
  iface->get_description = gtk_icon_view_item_accessible_action_get_description;
  iface->get_name = gtk_icon_view_item_accessible_action_get_name;
  iface->set_description = gtk_icon_view_item_accessible_action_set_description;
}

static const gchar *
gtk_icon_view_item_accessible_image_get_image_description (AtkImage *image)
{
  GtkIconViewItemAccessible *item;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (image);

  return item->image_description;
}

static gboolean
gtk_icon_view_item_accessible_image_set_image_description (AtkImage    *image,
                                                           const gchar *description)
{
  GtkIconViewItemAccessible *item;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (image);

  g_free (item->image_description);
  item->image_description = g_strdup (description);

  return TRUE;
}

typedef struct {
  GdkRectangle box;
  gboolean     pixbuf_found;
} GetPixbufBoxData;

static gboolean 
get_pixbuf_foreach (GtkCellRenderer    *renderer,
		    const GdkRectangle *cell_area,
		    const GdkRectangle *cell_background,
		    GetPixbufBoxData   *data)
{
  if (GTK_IS_CELL_RENDERER_PIXBUF (renderer))
    {
      data->box = *cell_area;
      data->pixbuf_found = TRUE;
    }
  return (data->pixbuf_found != FALSE);
}

static gboolean
get_pixbuf_box (GtkIconView     *icon_view,
		GtkIconViewItem *item,
		GdkRectangle    *box)
{
  GetPixbufBoxData data = { { 0, }, FALSE };
  GtkCellAreaContext *context;

  context = g_ptr_array_index (icon_view->priv->row_contexts, item->row);

  gtk_icon_view_set_cell_data (icon_view, item);
  gtk_cell_area_foreach_alloc (icon_view->priv->cell_area, context,
			       GTK_WIDGET (icon_view),
			       (GdkRectangle *)item, (GdkRectangle *)item,
			       (GtkCellAllocCallback)get_pixbuf_foreach, &data);

  return data.pixbuf_found;
}

static gboolean 
get_text_foreach (GtkCellRenderer    *renderer,
		  gchar             **text)
{
  if (GTK_IS_CELL_RENDERER_TEXT (renderer))
    {
      g_object_get (renderer, "text", text, NULL);

      return TRUE;
    }
  return FALSE;
}

static gchar *
get_text (GtkIconView     *icon_view,
	  GtkIconViewItem *item)
{
  gchar *text = NULL;

  gtk_icon_view_set_cell_data (icon_view, item);
  gtk_cell_area_foreach (icon_view->priv->cell_area,
			 (GtkCellCallback)get_text_foreach, &text);

  return text;
}

static void
gtk_icon_view_item_accessible_image_get_image_size (AtkImage *image,
                                                    gint     *width,
                                                    gint     *height)
{
  GtkIconViewItemAccessible *item;
  GdkRectangle box;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (image);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return;

  if (get_pixbuf_box (GTK_ICON_VIEW (item->widget), item->item, &box))
    {
      *width = box.width;
      *height = box.height;  
    }
}

static void
gtk_icon_view_item_accessible_image_get_image_position (AtkImage    *image,
                                                        gint        *x,
                                                        gint        *y,
                                                        AtkCoordType coord_type)
{
  GtkIconViewItemAccessible *item;
  GdkRectangle box;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (image);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return;

  atk_component_get_position (ATK_COMPONENT (image), x, y, coord_type);

  if (get_pixbuf_box (GTK_ICON_VIEW (item->widget), item->item, &box))
    {
      *x+= box.x - item->item->cell_area.x;
      *y+= box.y - item->item->cell_area.y;
    }

}

static void
atk_image_item_interface_init (AtkImageIface *iface)
{
  iface->get_image_description = gtk_icon_view_item_accessible_image_get_image_description;
  iface->set_image_description = gtk_icon_view_item_accessible_image_set_image_description;
  iface->get_image_size = gtk_icon_view_item_accessible_image_get_image_size;
  iface->get_image_position = gtk_icon_view_item_accessible_image_get_image_position;
}

static gchar *
gtk_icon_view_item_accessible_text_get_text (AtkText *text,
                                             gint     start_pos,
                                             gint     end_pos)
{
  GtkIconViewItemAccessible *item;
  GtkTextIter start, end;
  GtkTextBuffer *buffer;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (text);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return NULL;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return NULL;

  buffer = item->text_buffer;
  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_pos);
  if (end_pos < 0)
    gtk_text_buffer_get_end_iter (buffer, &end);
  else
    gtk_text_buffer_get_iter_at_offset (buffer, &end, end_pos);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static gunichar
gtk_icon_view_item_accessible_text_get_character_at_offset (AtkText *text,
                                                            gint     offset)
{
  GtkIconViewItemAccessible *item;
  GtkTextIter start, end;
  GtkTextBuffer *buffer;
  gchar *string;
  gunichar unichar;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (text);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return '\0';

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return '\0';

  buffer = item->text_buffer;
  if (offset >= gtk_text_buffer_get_char_count (buffer))
    return '\0';

  gtk_text_buffer_get_iter_at_offset (buffer, &start, offset);
  end = start;
  gtk_text_iter_forward_char (&end);
  string = gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
  unichar = g_utf8_get_char (string);
  g_free(string);

  return unichar;
}

#if 0
static void
get_pango_text_offsets (PangoLayout     *layout,
                        GtkTextBuffer   *buffer,
                        gint             function,
                        AtkTextBoundary  boundary_type,
                        gint             offset,
                        gint            *start_offset,
                        gint            *end_offset,
                        GtkTextIter     *start_iter,
                        GtkTextIter     *end_iter)
{
  PangoLayoutIter *iter;
  PangoLayoutLine *line, *prev_line = NULL, *prev_prev_line = NULL;
  gint index, start_index, end_index;
  const gchar *text;
  gboolean found = FALSE;

  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango_layout_get_iter (layout);
  do
    {
      line = pango_layout_iter_get_line_readonly (iter);
      start_index = line->start_index;
      end_index = start_index + line->length;

      if (index >= start_index && index <= end_index)
        {
          /*
           * Found line for offset
           */
          switch (function)
            {
            case 0:
                  /*
                   * We want the previous line
                   */
              if (prev_line)
                {
                  switch (boundary_type)
                    {
                    case ATK_TEXT_BOUNDARY_LINE_START:
                      end_index = start_index;
                      start_index = prev_line->start_index;
                      break;
                    case ATK_TEXT_BOUNDARY_LINE_END:
                      if (prev_prev_line)
                        start_index = prev_prev_line->start_index + 
                                  prev_prev_line->length;
                      end_index = prev_line->start_index + prev_line->length;
                      break;
                    default:
                      g_assert_not_reached();
                    }
                }
              else
                start_index = end_index = 0;
              break;
            case 1:
              switch (boundary_type)
                {
                case ATK_TEXT_BOUNDARY_LINE_START:
                  if (pango_layout_iter_next_line (iter))
                    end_index = pango_layout_iter_get_line_readonly (iter)->start_index;
                  break;
                case ATK_TEXT_BOUNDARY_LINE_END:
                  if (prev_line)
                    start_index = prev_line->start_index + 
                                  prev_line->length;
                  break;
                default:
                  g_assert_not_reached();
                }
              break;
            case 2:
               /*
                * We want the next line
                */
              if (pango_layout_iter_next_line (iter))
                {
                  line = pango_layout_iter_get_line_readonly (iter);
                  switch (boundary_type)
                    {
                    case ATK_TEXT_BOUNDARY_LINE_START:
                      start_index = line->start_index;
                      if (pango_layout_iter_next_line (iter))
                        end_index = pango_layout_iter_get_line_readonly (iter)->start_index;
                      else
                        end_index = start_index + line->length;
                      break;
                    case ATK_TEXT_BOUNDARY_LINE_END:
                      start_index = end_index;
                      end_index = line->start_index + line->length;
                      break;
                    default:
                      g_assert_not_reached();
                    }
                }
              else
                start_index = end_index;
              break;
            }
          found = TRUE;
          break;
        }
      prev_prev_line = prev_line; 
      prev_line = line; 
    }
  while (pango_layout_iter_next_line (iter));

  if (!found)
    {
      start_index = prev_line->start_index + prev_line->length;
      end_index = start_index;
    }
  pango_layout_iter_free (iter);
  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
 
  gtk_text_buffer_get_iter_at_offset (buffer, start_iter, *start_offset);
  gtk_text_buffer_get_iter_at_offset (buffer, end_iter, *end_offset);
}
#endif

static gchar*
gtk_icon_view_item_accessible_text_get_text_before_offset (AtkText         *text,
                                                           gint            offset,
                                                           AtkTextBoundary boundary_type,
                                                           gint            *start_offset,
                                                           gint            *end_offset)
{
  GtkIconViewItemAccessible *item;
  GtkTextIter start, end;
  GtkTextBuffer *buffer;
#if 0
  GtkIconView *icon_view;
#endif

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (text);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return NULL;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return NULL;

  buffer = item->text_buffer;

  if (!gtk_text_buffer_get_char_count (buffer))
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }
  gtk_text_buffer_get_iter_at_offset (buffer, &start, offset);
   
  end = start;

  switch (boundary_type)
    {
    case ATK_TEXT_BOUNDARY_CHAR:
      gtk_text_iter_backward_char(&start);
      break;
    case ATK_TEXT_BOUNDARY_WORD_START:
      if (!gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      end = start;
      gtk_text_iter_backward_word_start(&start);
      break;
    case ATK_TEXT_BOUNDARY_WORD_END:
      if (gtk_text_iter_inside_word (&start) &&
          !gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      while (!gtk_text_iter_ends_word (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      end = start;
      gtk_text_iter_backward_word_start(&start);
      while (!gtk_text_iter_ends_word (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      break;
    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      if (!gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      end = start;
      gtk_text_iter_backward_sentence_start (&start);
      break;
    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      if (gtk_text_iter_inside_sentence (&start) &&
          !gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      while (!gtk_text_iter_ends_sentence (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      end = start;
      gtk_text_iter_backward_sentence_start (&start);
      while (!gtk_text_iter_ends_sentence (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      break;
   case ATK_TEXT_BOUNDARY_LINE_START:
   case ATK_TEXT_BOUNDARY_LINE_END:
#if 0
      icon_view = GTK_ICON_VIEW (item->widget);
      /* FIXME we probably have to use GailTextCell to salvage this */
      gtk_icon_view_update_item_text (icon_view, item->item);
      get_pango_text_offsets (icon_view->priv->layout,
                              buffer,
                              0,
                              boundary_type,
                              offset,
                              start_offset,
                              end_offset,
                              &start,
                              &end);
#endif
      break;
    }

  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static gchar*
gtk_icon_view_item_accessible_text_get_text_at_offset (AtkText         *text,
                                                       gint            offset,
                                                       AtkTextBoundary boundary_type,
                                                       gint            *start_offset,
                                                       gint            *end_offset)
{
  GtkIconViewItemAccessible *item;
  GtkTextIter start, end;
  GtkTextBuffer *buffer;
#if 0
  GtkIconView *icon_view;
#endif

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (text);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return NULL;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return NULL;

  buffer = item->text_buffer;

  if (!gtk_text_buffer_get_char_count (buffer))
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }
  gtk_text_buffer_get_iter_at_offset (buffer, &start, offset);
   
  end = start;

  switch (boundary_type)
    {
    case ATK_TEXT_BOUNDARY_CHAR:
      gtk_text_iter_forward_char (&end);
      break;
    case ATK_TEXT_BOUNDARY_WORD_START:
      if (!gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      if (gtk_text_iter_inside_word (&end))
        gtk_text_iter_forward_word_end (&end);
      while (!gtk_text_iter_starts_word (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            break;
        }
      break;
    case ATK_TEXT_BOUNDARY_WORD_END:
      if (gtk_text_iter_inside_word (&start) &&
          !gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      while (!gtk_text_iter_ends_word (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      gtk_text_iter_forward_word_end (&end);
      break;
    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      if (!gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      if (gtk_text_iter_inside_sentence (&end))
        gtk_text_iter_forward_sentence_end (&end);
      while (!gtk_text_iter_starts_sentence (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            break;
        }
      break;
    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      if (gtk_text_iter_inside_sentence (&start) &&
          !gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      while (!gtk_text_iter_ends_sentence (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      gtk_text_iter_forward_sentence_end (&end);
      break;
   case ATK_TEXT_BOUNDARY_LINE_START:
   case ATK_TEXT_BOUNDARY_LINE_END:
#if 0
      icon_view = GTK_ICON_VIEW (item->widget);
      /* FIXME we probably have to use GailTextCell to salvage this */
      gtk_icon_view_update_item_text (icon_view, item->item);
      get_pango_text_offsets (icon_view->priv->layout,
                              buffer,
                              1,
                              boundary_type,
                              offset,
                              start_offset,
                              end_offset,
                              &start,
                              &end);
#endif
      break;
    }


  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static gchar*
gtk_icon_view_item_accessible_text_get_text_after_offset (AtkText         *text,
                                                          gint            offset,
                                                          AtkTextBoundary boundary_type,
                                                          gint            *start_offset,
                                                          gint            *end_offset)
{
  GtkIconViewItemAccessible *item;
  GtkTextIter start, end;
  GtkTextBuffer *buffer;
#if 0
  GtkIconView *icon_view;
#endif

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (text);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return NULL;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return NULL;

  buffer = item->text_buffer;

  if (!gtk_text_buffer_get_char_count (buffer))
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }
  gtk_text_buffer_get_iter_at_offset (buffer, &start, offset);
   
  end = start;

  switch (boundary_type)
    {
    case ATK_TEXT_BOUNDARY_CHAR:
      gtk_text_iter_forward_char(&start);
      gtk_text_iter_forward_chars(&end, 2);
      break;
    case ATK_TEXT_BOUNDARY_WORD_START:
      if (gtk_text_iter_inside_word (&end))
        gtk_text_iter_forward_word_end (&end);
      while (!gtk_text_iter_starts_word (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            break;
        }
      start = end;
      if (!gtk_text_iter_is_end (&end))
        {
          gtk_text_iter_forward_word_end (&end);
          while (!gtk_text_iter_starts_word (&end))
            {
              if (!gtk_text_iter_forward_char (&end))
                break;
            }
        }
      break;
    case ATK_TEXT_BOUNDARY_WORD_END:
      gtk_text_iter_forward_word_end (&end);
      start = end;
      if (!gtk_text_iter_is_end (&end))
        gtk_text_iter_forward_word_end (&end);
      break;
    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      if (gtk_text_iter_inside_sentence (&end))
        gtk_text_iter_forward_sentence_end (&end);
      while (!gtk_text_iter_starts_sentence (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            break;
        }
      start = end;
      if (!gtk_text_iter_is_end (&end))
        {
          gtk_text_iter_forward_sentence_end (&end);
          while (!gtk_text_iter_starts_sentence (&end))
            {
              if (!gtk_text_iter_forward_char (&end))
                break;
            }
        }
      break;
    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      gtk_text_iter_forward_sentence_end (&end);
      start = end;
      if (!gtk_text_iter_is_end (&end))
        gtk_text_iter_forward_sentence_end (&end);
      break;
   case ATK_TEXT_BOUNDARY_LINE_START:
   case ATK_TEXT_BOUNDARY_LINE_END:
#if 0
      icon_view = GTK_ICON_VIEW (item->widget);
      /* FIXME we probably have to use GailTextCell to salvage this */
      gtk_icon_view_update_item_text (icon_view, item->item);
      get_pango_text_offsets (icon_view->priv->layout,
                              buffer,
                              2,
                              boundary_type,
                              offset,
                              start_offset,
                              end_offset,
                              &start,
                              &end);
#endif
      break;
    }
  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static gint
gtk_icon_view_item_accessible_text_get_character_count (AtkText *text)
{
  GtkIconViewItemAccessible *item;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (text);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return 0;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return 0;

  return gtk_text_buffer_get_char_count (item->text_buffer);
}

static void
gtk_icon_view_item_accessible_text_get_character_extents (AtkText      *text,
                                                          gint         offset,
                                                          gint         *x,
                                                          gint         *y,
                                                          gint         *width,
                                                          gint         *height,
                                                          AtkCoordType coord_type)
{
  GtkIconViewItemAccessible *item;
#if 0
  GtkIconView *icon_view;
  PangoRectangle char_rect;
  const gchar *item_text;
  gint index;
#endif

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (text);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return;

#if 0
  icon_view = GTK_ICON_VIEW (item->widget);
      /* FIXME we probably have to use GailTextCell to salvage this */
  gtk_icon_view_update_item_text (icon_view, item->item);
  item_text = pango_layout_get_text (icon_view->priv->layout);
  index = g_utf8_offset_to_pointer (item_text, offset) - item_text;
  pango_layout_index_to_pos (icon_view->priv->layout, index, &char_rect);

  atk_component_get_position (ATK_COMPONENT (text), x, y, coord_type);
  *x += item->item->layout_x - item->item->x + char_rect.x / PANGO_SCALE;
  /* Look at gtk_icon_view_paint_item() to see where the text is. */
  *x -=  ((item->item->width - item->item->layout_width) / 2) + (MAX (item->item->pixbuf_width, icon_view->priv->item_width) - item->item->width) / 2,
  *y += item->item->layout_y - item->item->y + char_rect.y / PANGO_SCALE;
  *width = char_rect.width / PANGO_SCALE;
  *height = char_rect.height / PANGO_SCALE;
#endif
}

static gint
gtk_icon_view_item_accessible_text_get_offset_at_point (AtkText      *text,
                                                        gint          x,
                                                        gint          y,
                                                        AtkCoordType coord_type)
{
  GtkIconViewItemAccessible *item;
  gint offset = 0;
#if 0
  GtkIconView *icon_view;
  const gchar *item_text;
  gint index;
  gint l_x, l_y;
#endif

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (text);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return -1;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return -1;

#if 0
  icon_view = GTK_ICON_VIEW (item->widget);
      /* FIXME we probably have to use GailTextCell to salvage this */
  gtk_icon_view_update_item_text (icon_view, item->item);
  atk_component_get_position (ATK_COMPONENT (text), &l_x, &l_y, coord_type);
  x -= l_x + item->item->layout_x - item->item->x;
  x +=  ((item->item->width - item->item->layout_width) / 2) + (MAX (item->item->pixbuf_width, icon_view->priv->item_width) - item->item->width) / 2,
  y -= l_y + item->item->layout_y - item->item->y;
  item_text = pango_layout_get_text (icon_view->priv->layout);
  if (!pango_layout_xy_to_index (icon_view->priv->layout, 
                                x * PANGO_SCALE,
                                y * PANGO_SCALE,
                                &index, NULL))
    {
      if (x < 0 || y < 0)
        index = 0;
      else
        index = -1;
    } 
  if (index == -1)
    offset = g_utf8_strlen (item_text, -1);
  else
    offset = g_utf8_pointer_to_offset (item_text, item_text + index);
#endif
  return offset;
}

static void
atk_text_item_interface_init (AtkTextIface *iface)
{
  iface->get_text = gtk_icon_view_item_accessible_text_get_text;
  iface->get_character_at_offset = gtk_icon_view_item_accessible_text_get_character_at_offset;
  iface->get_text_before_offset = gtk_icon_view_item_accessible_text_get_text_before_offset;
  iface->get_text_at_offset = gtk_icon_view_item_accessible_text_get_text_at_offset;
  iface->get_text_after_offset = gtk_icon_view_item_accessible_text_get_text_after_offset;
  iface->get_character_count = gtk_icon_view_item_accessible_text_get_character_count;
  iface->get_character_extents = gtk_icon_view_item_accessible_text_get_character_extents;
  iface->get_offset_at_point = gtk_icon_view_item_accessible_text_get_offset_at_point;
}

static void
gtk_icon_view_item_accessible_get_extents (AtkComponent *component,
                                           gint         *x,
                                           gint         *y,
                                           gint         *width,
                                           gint         *height,
                                           AtkCoordType  coord_type)
{
  GtkIconViewItemAccessible *item;
  AtkObject *parent_obj;
  gint l_x, l_y;

  g_return_if_fail (GTK_IS_ICON_VIEW_ITEM_ACCESSIBLE (component));

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (component);
  if (!GTK_IS_WIDGET (item->widget))
    return;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return;

  *width = item->item->cell_area.width;
  *height = item->item->cell_area.height;
  if (gtk_icon_view_item_accessible_is_showing (item))
    {
      parent_obj = gtk_widget_get_accessible (item->widget);
      atk_component_get_position (ATK_COMPONENT (parent_obj), &l_x, &l_y, coord_type);
      *x = l_x + item->item->cell_area.x;
      *y = l_y + item->item->cell_area.y;
    }
  else
    {
      *x = G_MININT;
      *y = G_MININT;
    }
}

static gboolean
gtk_icon_view_item_accessible_grab_focus (AtkComponent *component)
{
  GtkIconViewItemAccessible *item;
  GtkWidget *toplevel;

  g_return_val_if_fail (GTK_IS_ICON_VIEW_ITEM_ACCESSIBLE (component), FALSE);

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (component);
  if (!GTK_IS_WIDGET (item->widget))
    return FALSE;

  gtk_widget_grab_focus (item->widget);
  gtk_icon_view_set_cursor_item (GTK_ICON_VIEW (item->widget), item->item, NULL);
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (item->widget));
  if (gtk_widget_is_toplevel (toplevel))
    gtk_window_present (GTK_WINDOW (toplevel));

  return TRUE;
}

static void
atk_component_item_interface_init (AtkComponentIface *iface)
{
  iface->get_extents = gtk_icon_view_item_accessible_get_extents;
  iface->grab_focus = gtk_icon_view_item_accessible_grab_focus;
}

static gboolean
gtk_icon_view_item_accessible_add_state (GtkIconViewItemAccessible *item,
                                         AtkStateType               state_type,
                                         gboolean                   emit_signal)
{
  gboolean rc;

  rc = atk_state_set_add_state (item->state_set, state_type);
  /*
   * The signal should only be generated if the value changed,
   * not when the item is set up.  So states that are set
   * initially should pass FALSE as the emit_signal argument.
   */

  if (emit_signal)
    {
      atk_object_notify_state_change (ATK_OBJECT (item), state_type, TRUE);
      /* If state_type is ATK_STATE_VISIBLE, additional notification */
      if (state_type == ATK_STATE_VISIBLE)
        g_signal_emit_by_name (item, "visible-data-changed");
    }

  return rc;
}

static gboolean
gtk_icon_view_item_accessible_remove_state (GtkIconViewItemAccessible *item,
                                            AtkStateType               state_type,
                                            gboolean                   emit_signal)
{
  if (atk_state_set_contains_state (item->state_set, state_type))
    {
      gboolean rc;

      rc = atk_state_set_remove_state (item->state_set, state_type);
      /*
       * The signal should only be generated if the value changed,
       * not when the item is set up.  So states that are set
       * initially should pass FALSE as the emit_signal argument.
       */

      if (emit_signal)
        {
          atk_object_notify_state_change (ATK_OBJECT (item), state_type, FALSE);
          /* If state_type is ATK_STATE_VISIBLE, additional notification */
          if (state_type == ATK_STATE_VISIBLE)
            g_signal_emit_by_name (item, "visible-data-changed");
        }

      return rc;
    }
  else
    return FALSE;
}

static gboolean
gtk_icon_view_item_accessible_is_showing (GtkIconViewItemAccessible *item)
{
  GtkAllocation allocation;
  GtkIconView *icon_view;
  GdkRectangle visible_rect;
  gboolean is_showing;

  /*
   * An item is considered "SHOWING" if any part of the item is in the
   * visible rectangle.
   */

  if (!GTK_IS_ICON_VIEW (item->widget))
    return FALSE;

  if (item->item == NULL)
    return FALSE;

  gtk_widget_get_allocation (item->widget, &allocation);

  icon_view = GTK_ICON_VIEW (item->widget);
  visible_rect.x = 0;
  if (icon_view->priv->hadjustment)
    visible_rect.x += gtk_adjustment_get_value (icon_view->priv->hadjustment);
  visible_rect.y = 0;
  if (icon_view->priv->hadjustment)
    visible_rect.y += gtk_adjustment_get_value (icon_view->priv->vadjustment);
  visible_rect.width = allocation.width;
  visible_rect.height = allocation.height;

  if (((item->item->cell_area.x + item->item->cell_area.width) < visible_rect.x) ||
     ((item->item->cell_area.y + item->item->cell_area.height) < (visible_rect.y)) ||
     (item->item->cell_area.x > (visible_rect.x + visible_rect.width)) ||
     (item->item->cell_area.y > (visible_rect.y + visible_rect.height)))
    is_showing =  FALSE;
  else
    is_showing = TRUE;

  return is_showing;
}

static gboolean
gtk_icon_view_item_accessible_set_visibility (GtkIconViewItemAccessible *item,
                                              gboolean                   emit_signal)
{
  if (gtk_icon_view_item_accessible_is_showing (item))
    return gtk_icon_view_item_accessible_add_state (item, ATK_STATE_SHOWING,
						    emit_signal);
  else
    return gtk_icon_view_item_accessible_remove_state (item, ATK_STATE_SHOWING,
						       emit_signal);
}

static void
gtk_icon_view_item_accessible_object_init (GtkIconViewItemAccessible *item)
{
  gint i;

  item->state_set = atk_state_set_new ();

  atk_state_set_add_state (item->state_set, ATK_STATE_ENABLED);
  atk_state_set_add_state (item->state_set, ATK_STATE_FOCUSABLE);
  atk_state_set_add_state (item->state_set, ATK_STATE_SENSITIVE);
  atk_state_set_add_state (item->state_set, ATK_STATE_SELECTABLE);
  atk_state_set_add_state (item->state_set, ATK_STATE_VISIBLE);

  for (i = 0; i < LAST_ACTION; i++)
    item->action_descriptions[i] = NULL;

  item->image_description = NULL;

  item->action_idle_handler = 0;
}

static void
gtk_icon_view_item_accessible_finalize (GObject *object)
{
  GtkIconViewItemAccessible *item;
  gint i;

  g_return_if_fail (GTK_IS_ICON_VIEW_ITEM_ACCESSIBLE (object));

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (object);

  if (item->widget)
    g_object_remove_weak_pointer (G_OBJECT (item->widget), (gpointer) &item->widget);

  if (item->state_set)
    g_object_unref (item->state_set);

  if (item->text_buffer)
     g_object_unref (item->text_buffer);

  for (i = 0; i < LAST_ACTION; i++)
    g_free (item->action_descriptions[i]);

  g_free (item->image_description);

  if (item->action_idle_handler)
    {
      g_source_remove (item->action_idle_handler);
      item->action_idle_handler = 0;
    }

  G_OBJECT_CLASS (accessible_item_parent_class)->finalize (object);
}

static G_CONST_RETURN gchar*
gtk_icon_view_item_accessible_get_name (AtkObject *obj)
{
  if (obj->name)
    return obj->name;
  else
    {
      GtkIconViewItemAccessible *item;
      GtkTextIter start_iter;
      GtkTextIter end_iter;

      item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (obj);
 
      gtk_text_buffer_get_start_iter (item->text_buffer, &start_iter); 
      gtk_text_buffer_get_end_iter (item->text_buffer, &end_iter); 

      return gtk_text_buffer_get_text (item->text_buffer, &start_iter, &end_iter, FALSE);
    }
}

static AtkObject*
gtk_icon_view_item_accessible_get_parent (AtkObject *obj)
{
  GtkIconViewItemAccessible *item;

  g_return_val_if_fail (GTK_IS_ICON_VIEW_ITEM_ACCESSIBLE (obj), NULL);
  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (obj);

  if (item->widget)
    return gtk_widget_get_accessible (item->widget);
  else
    return NULL;
}

static gint
gtk_icon_view_item_accessible_get_index_in_parent (AtkObject *obj)
{
  GtkIconViewItemAccessible *item;

  g_return_val_if_fail (GTK_IS_ICON_VIEW_ITEM_ACCESSIBLE (obj), 0);
  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (obj);

  return item->item->index; 
}

static AtkStateSet *
gtk_icon_view_item_accessible_ref_state_set (AtkObject *obj)
{
  GtkIconViewItemAccessible *item;
  GtkIconView *icon_view;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (obj);
  g_return_val_if_fail (item->state_set, NULL);

  if (!item->widget)
    return NULL;

  icon_view = GTK_ICON_VIEW (item->widget);
  if (icon_view->priv->cursor_item == item->item)
    atk_state_set_add_state (item->state_set, ATK_STATE_FOCUSED);
  else
    atk_state_set_remove_state (item->state_set, ATK_STATE_FOCUSED);
  if (item->item->selected)
    atk_state_set_add_state (item->state_set, ATK_STATE_SELECTED);
  else
    atk_state_set_remove_state (item->state_set, ATK_STATE_SELECTED);

  return g_object_ref (item->state_set);
}

static void
gtk_icon_view_item_accessible_class_init (AtkObjectClass *klass)
{
  GObjectClass *gobject_class;

  accessible_item_parent_class = g_type_class_peek_parent (klass);

  gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = gtk_icon_view_item_accessible_finalize;

  klass->get_index_in_parent = gtk_icon_view_item_accessible_get_index_in_parent; 
  klass->get_name = gtk_icon_view_item_accessible_get_name; 
  klass->get_parent = gtk_icon_view_item_accessible_get_parent; 
  klass->ref_state_set = gtk_icon_view_item_accessible_ref_state_set; 
}

static GType
gtk_icon_view_item_accessible_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      const GTypeInfo tinfo =
      {
        sizeof (GtkIconViewItemAccessibleClass),
        (GBaseInitFunc) NULL, /* base init */
        (GBaseFinalizeFunc) NULL, /* base finalize */
        (GClassInitFunc) gtk_icon_view_item_accessible_class_init, /* class init */
        (GClassFinalizeFunc) NULL, /* class finalize */
        NULL, /* class data */
        sizeof (GtkIconViewItemAccessible), /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) gtk_icon_view_item_accessible_object_init, /* instance init */
        NULL /* value table */
      };

      const GInterfaceInfo atk_component_info =
      {
        (GInterfaceInitFunc) atk_component_item_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };
      const GInterfaceInfo atk_action_info =
      {
        (GInterfaceInitFunc) atk_action_item_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };
      const GInterfaceInfo atk_image_info =
      {
        (GInterfaceInitFunc) atk_image_item_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };
      const GInterfaceInfo atk_text_info =
      {
        (GInterfaceInitFunc) atk_text_item_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };

      type = g_type_register_static (ATK_TYPE_OBJECT,
                                     I_("GtkIconViewItemAccessible"), &tinfo, 0);
      g_type_add_interface_static (type, ATK_TYPE_COMPONENT,
                                   &atk_component_info);
      g_type_add_interface_static (type, ATK_TYPE_ACTION,
                                   &atk_action_info);
      g_type_add_interface_static (type, ATK_TYPE_IMAGE,
                                   &atk_image_info);
      g_type_add_interface_static (type, ATK_TYPE_TEXT,
                                   &atk_text_info);
    }

  return type;
}

#define GTK_TYPE_ICON_VIEW_ACCESSIBLE      (gtk_icon_view_accessible_get_type ())
#define GTK_ICON_VIEW_ACCESSIBLE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ICON_VIEW_ACCESSIBLE, GtkIconViewAccessible))
#define GTK_IS_ICON_VIEW_ACCESSIBLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ICON_VIEW_ACCESSIBLE))

static GType gtk_icon_view_accessible_get_type (void);

typedef struct
{
   AtkObject parent;
} GtkIconViewAccessible;

typedef struct
{
  AtkObject *item;
  gint       index;
} GtkIconViewItemAccessibleInfo;

typedef struct
{
  GList *items;

  GtkAdjustment *old_hadj;
  GtkAdjustment *old_vadj;

  GtkTreeModel *model;

} GtkIconViewAccessiblePrivate;

static GtkIconViewAccessiblePrivate *
gtk_icon_view_accessible_get_priv (AtkObject *accessible)
{
  return g_object_get_qdata (G_OBJECT (accessible),
                             accessible_private_data_quark);
}

static void
gtk_icon_view_item_accessible_info_new (AtkObject *accessible,
                                        AtkObject *item,
                                        gint       index)
{
  GtkIconViewItemAccessibleInfo *info;
  GtkIconViewItemAccessibleInfo *tmp_info;
  GtkIconViewAccessiblePrivate *priv;
  GList *items;

  info = g_new (GtkIconViewItemAccessibleInfo, 1);
  info->item = item;
  info->index = index;

  priv = gtk_icon_view_accessible_get_priv (accessible);
  items = priv->items;
  while (items)
    {
      tmp_info = items->data;
      if (tmp_info->index > index)
        break;
      items = items->next;
    }
  priv->items = g_list_insert_before (priv->items, items, info);
  priv->old_hadj = NULL;
  priv->old_vadj = NULL;
}

static gint
gtk_icon_view_accessible_get_n_children (AtkObject *accessible)
{
  GtkIconView *icon_view;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (!widget)
      return 0;

  icon_view = GTK_ICON_VIEW (widget);

  return g_list_length (icon_view->priv->items);
}

static AtkObject *
gtk_icon_view_accessible_find_child (AtkObject *accessible,
                                     gint       index)
{
  GtkIconViewAccessiblePrivate *priv;
  GtkIconViewItemAccessibleInfo *info;
  GList *items;

  priv = gtk_icon_view_accessible_get_priv (accessible);
  items = priv->items;

  while (items)
    {
      info = items->data;
      if (info->index == index)
        return info->item;
      items = items->next; 
    }
  return NULL;
}

static AtkObject *
gtk_icon_view_accessible_ref_child (AtkObject *accessible,
                                    gint       index)
{
  GtkIconView *icon_view;
  GtkWidget *widget;
  GList *icons;
  AtkObject *obj;
  GtkIconViewItemAccessible *a11y_item;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (!widget)
    return NULL;

  icon_view = GTK_ICON_VIEW (widget);
  icons = g_list_nth (icon_view->priv->items, index);
  obj = NULL;
  if (icons)
    {
      GtkIconViewItem *item = icons->data;
   
      g_return_val_if_fail (item->index == index, NULL);
      obj = gtk_icon_view_accessible_find_child (accessible, index);
      if (!obj)
        {
          gchar *text;

          obj = g_object_new (gtk_icon_view_item_accessible_get_type (), NULL);
          gtk_icon_view_item_accessible_info_new (accessible,
                                                  obj,
                                                  index);
          obj->role = ATK_ROLE_ICON;
          a11y_item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (obj);
          a11y_item->item = item;
          a11y_item->widget = widget;
          a11y_item->text_buffer = gtk_text_buffer_new (NULL);

          text = get_text (icon_view, item);
          if (text)
            {
              gtk_text_buffer_set_text (a11y_item->text_buffer, text, -1);
              g_free (text);
            } 

          gtk_icon_view_item_accessible_set_visibility (a11y_item, FALSE);
          g_object_add_weak_pointer (G_OBJECT (widget), (gpointer) &(a11y_item->widget));
       }
      g_object_ref (obj);
    }
  return obj;
}

static void
gtk_icon_view_accessible_traverse_items (GtkIconViewAccessible *view,
                                         GList                 *list)
{
  GtkIconViewAccessiblePrivate *priv;
  GtkIconViewItemAccessibleInfo *info;
  GtkIconViewItemAccessible *item;
  GList *items;
  
  priv =  gtk_icon_view_accessible_get_priv (ATK_OBJECT (view));
  if (priv->items)
    {
      GtkWidget *widget;
      gboolean act_on_item;

      widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (view));
      if (widget == NULL)
        return;

      items = priv->items;

      act_on_item = (list == NULL);

      while (items)
        {

          info = (GtkIconViewItemAccessibleInfo *)items->data;
          item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (info->item);

          if (act_on_item == FALSE && list == items)
            act_on_item = TRUE;

          if (act_on_item)
	    gtk_icon_view_item_accessible_set_visibility (item, TRUE);

          items = items->next;
       }
   }
}

static void
gtk_icon_view_accessible_adjustment_changed (GtkAdjustment         *adjustment,
                                             GtkIconViewAccessible *view)
{
  gtk_icon_view_accessible_traverse_items (view, NULL);
}

static void
gtk_icon_view_accessible_set_adjustment (AtkObject      *accessible,
                                         GtkOrientation  orientation,
                                         GtkAdjustment  *adjustment)
{
  GtkIconViewAccessiblePrivate *priv;
  GtkAdjustment **old_adj_ptr;

  priv = gtk_icon_view_accessible_get_priv (accessible);

  /* Adjustments are set for the first time in constructor and priv is not
   * initialized at that time, so skip this first setting. */
  if (!priv)
    return;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (priv->old_hadj == adjustment)
        return;

      old_adj_ptr = &priv->old_hadj;
    }
  else
    {
      if (priv->old_vadj == adjustment)
        return;

      old_adj_ptr = &priv->old_vadj;
    }

  /* Disconnect signal handlers */
  if (*old_adj_ptr)
    {
      g_object_remove_weak_pointer (G_OBJECT (*old_adj_ptr),
                                    (gpointer *)&priv->old_hadj);
      g_signal_handlers_disconnect_by_func (*old_adj_ptr,
                                            gtk_icon_view_accessible_adjustment_changed,
                                            accessible);
    }

  /* Connect signal */
  *old_adj_ptr = adjustment;
  g_object_add_weak_pointer (G_OBJECT (adjustment), (gpointer *)old_adj_ptr);
  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gtk_icon_view_accessible_adjustment_changed),
                    accessible);
}

static void
gtk_icon_view_accessible_model_row_changed (GtkTreeModel *tree_model,
                                            GtkTreePath  *path,
                                            GtkTreeIter  *iter,
                                            gpointer      user_data)
{
  AtkObject *atk_obj;
  gint index;
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;
  GtkIconViewItemAccessible *a11y_item;
  const gchar *name;
  gchar *text;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (user_data));
  index = gtk_tree_path_get_indices(path)[0];
  a11y_item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (
      gtk_icon_view_accessible_find_child (atk_obj, index));

  if (a11y_item)
    {
      widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_obj));
      icon_view = GTK_ICON_VIEW (widget);
      item = a11y_item->item;

      name = gtk_icon_view_item_accessible_get_name (ATK_OBJECT (a11y_item));

      if (!name || strcmp (name, "") == 0)
        {
          text = get_text (icon_view, item);
          if (text)
            {
              gtk_text_buffer_set_text (a11y_item->text_buffer, text, -1);
              g_free (text);
            }
        }
    }

  g_signal_emit_by_name (atk_obj, "visible-data-changed");

  return;
}

static void
gtk_icon_view_accessible_model_row_inserted (GtkTreeModel *tree_model,
                                             GtkTreePath  *path,
                                             GtkTreeIter  *iter,
                                             gpointer     user_data)
{
  GtkIconViewAccessiblePrivate *priv;
  GtkIconViewItemAccessibleInfo *info;
  GtkIconViewAccessible *view;
  GtkIconViewItemAccessible *item;
  GList *items;
  GList *tmp_list;
  AtkObject *atk_obj;
  gint index;

  index = gtk_tree_path_get_indices(path)[0];
  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (user_data));
  view = GTK_ICON_VIEW_ACCESSIBLE (atk_obj);
  priv = gtk_icon_view_accessible_get_priv (atk_obj);

  items = priv->items;
  tmp_list = NULL;
  while (items)
    {
      info = items->data;
      item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (info->item);
      if (info->index != item->item->index)
        {
          if (info->index < index)
            g_warning ("Unexpected index value on insertion %d %d", index, info->index);
 
          if (tmp_list == NULL)
            tmp_list = items;
   
          info->index = item->item->index;
        }

      items = items->next;
    }
  gtk_icon_view_accessible_traverse_items (view, tmp_list);
  g_signal_emit_by_name (atk_obj, "children-changed::add",
                         index, NULL, NULL);
  return;
}

static void
gtk_icon_view_accessible_model_row_deleted (GtkTreeModel *tree_model,
                                            GtkTreePath  *path,
                                            gpointer     user_data)
{
  GtkIconViewAccessiblePrivate *priv;
  GtkIconViewItemAccessibleInfo *info;
  GtkIconViewAccessible *view;
  GtkIconViewItemAccessible *item;
  GList *items;
  GList *tmp_list;
  GList *deleted_item;
  AtkObject *atk_obj;
  gint index;

  index = gtk_tree_path_get_indices(path)[0];
  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (user_data));
  view = GTK_ICON_VIEW_ACCESSIBLE (atk_obj);
  priv = gtk_icon_view_accessible_get_priv (atk_obj);

  items = priv->items;
  tmp_list = NULL;
  deleted_item = NULL;
  info = NULL;
  while (items)
    {
      info = items->data;
      item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (info->item);
      if (info->index == index)
        {
          deleted_item = items;
        }
      if (info->index != item->item->index)
        {
          if (tmp_list == NULL)
            tmp_list = items;
            
          info->index = item->item->index;
        }

      items = items->next;
    }
  gtk_icon_view_accessible_traverse_items (view, tmp_list);
  if (deleted_item)
    {
      info = deleted_item->data;
      gtk_icon_view_item_accessible_add_state (GTK_ICON_VIEW_ITEM_ACCESSIBLE (info->item), ATK_STATE_DEFUNCT, TRUE);
      g_signal_emit_by_name (atk_obj, "children-changed::remove",
                             index, NULL, NULL);
      priv->items = g_list_remove_link (priv->items, deleted_item);
      g_free (info);
    }

  return;
}

static gint
gtk_icon_view_accessible_item_compare (GtkIconViewItemAccessibleInfo *i1,
                                       GtkIconViewItemAccessibleInfo *i2)
{
  return i1->index - i2->index;
}

static void
gtk_icon_view_accessible_model_rows_reordered (GtkTreeModel *tree_model,
                                               GtkTreePath  *path,
                                               GtkTreeIter  *iter,
                                               gint         *new_order,
                                               gpointer     user_data)
{
  GtkIconViewAccessiblePrivate *priv;
  GtkIconViewItemAccessibleInfo *info;
  GtkIconView *icon_view;
  GtkIconViewItemAccessible *item;
  GList *items;
  AtkObject *atk_obj;
  gint *order;
  gint length, i;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (user_data));
  icon_view = GTK_ICON_VIEW (user_data);
  priv = gtk_icon_view_accessible_get_priv (atk_obj);

  length = gtk_tree_model_iter_n_children (tree_model, NULL);

  order = g_new (gint, length);
  for (i = 0; i < length; i++)
    order [new_order[i]] = i;

  items = priv->items;
  while (items)
    {
      info = items->data;
      item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (info->item);
      info->index = order[info->index];
      item->item = g_list_nth_data (icon_view->priv->items, info->index);
      items = items->next;
    }
  g_free (order);
  priv->items = g_list_sort (priv->items, 
                             (GCompareFunc)gtk_icon_view_accessible_item_compare);

  return;
}

static void
gtk_icon_view_accessible_disconnect_model_signals (GtkTreeModel *model,
                                                   GtkWidget *widget)
{
  GObject *obj;

  obj = G_OBJECT (model);
  g_signal_handlers_disconnect_by_func (obj, (gpointer) gtk_icon_view_accessible_model_row_changed, widget);
  g_signal_handlers_disconnect_by_func (obj, (gpointer) gtk_icon_view_accessible_model_row_inserted, widget);
  g_signal_handlers_disconnect_by_func (obj, (gpointer) gtk_icon_view_accessible_model_row_deleted, widget);
  g_signal_handlers_disconnect_by_func (obj, (gpointer) gtk_icon_view_accessible_model_rows_reordered, widget);
}

static void
gtk_icon_view_accessible_connect_model_signals (GtkIconView *icon_view)
{
  GObject *obj;

  obj = G_OBJECT (icon_view->priv->model);
  g_signal_connect_data (obj, "row-changed",
                         (GCallback) gtk_icon_view_accessible_model_row_changed,
                         icon_view, NULL, 0);
  g_signal_connect_data (obj, "row-inserted",
                         (GCallback) gtk_icon_view_accessible_model_row_inserted, 
                         icon_view, NULL, G_CONNECT_AFTER);
  g_signal_connect_data (obj, "row-deleted",
                         (GCallback) gtk_icon_view_accessible_model_row_deleted, 
                         icon_view, NULL, G_CONNECT_AFTER);
  g_signal_connect_data (obj, "rows-reordered",
                         (GCallback) gtk_icon_view_accessible_model_rows_reordered, 
                         icon_view, NULL, G_CONNECT_AFTER);
}

static void
gtk_icon_view_accessible_clear_cache (GtkIconViewAccessiblePrivate *priv)
{
  GtkIconViewItemAccessibleInfo *info;
  GList *items;

  items = priv->items;
  while (items)
    {
      info = (GtkIconViewItemAccessibleInfo *) items->data;
      g_object_unref (info->item);
      g_free (items->data);
      items = items->next;
    }
  g_list_free (priv->items);
  priv->items = NULL;
}

static void
gtk_icon_view_accessible_notify_gtk (GObject *obj,
                                     GParamSpec *pspec)
{
  GtkIconView *icon_view;
  GtkWidget *widget;
  AtkObject *atk_obj;
  GtkIconViewAccessiblePrivate *priv;

  if (strcmp (pspec->name, "model") == 0)
    {
      widget = GTK_WIDGET (obj); 
      atk_obj = gtk_widget_get_accessible (widget);
      priv = gtk_icon_view_accessible_get_priv (atk_obj);
      if (priv->model)
        {
          g_object_remove_weak_pointer (G_OBJECT (priv->model),
                                        (gpointer *)&priv->model);
          gtk_icon_view_accessible_disconnect_model_signals (priv->model, widget);
        }
      gtk_icon_view_accessible_clear_cache (priv);

      icon_view = GTK_ICON_VIEW (obj);
      priv->model = icon_view->priv->model;
      /* If there is no model the GtkIconView is probably being destroyed */
      if (priv->model)
        {
          g_object_add_weak_pointer (G_OBJECT (priv->model), (gpointer *)&priv->model);
          gtk_icon_view_accessible_connect_model_signals (icon_view);
        }
    }

  return;
}

static void
gtk_icon_view_accessible_initialize (AtkObject *accessible,
                                     gpointer   data)
{
  GtkIconViewAccessiblePrivate *priv;
  GtkIconView *icon_view;

  if (ATK_OBJECT_CLASS (accessible_parent_class)->initialize)
    ATK_OBJECT_CLASS (accessible_parent_class)->initialize (accessible, data);

  priv = g_new0 (GtkIconViewAccessiblePrivate, 1);
  g_object_set_qdata (G_OBJECT (accessible),
                      accessible_private_data_quark,
                      priv);

  icon_view = GTK_ICON_VIEW (data);
  if (icon_view->priv->hadjustment)
    gtk_icon_view_accessible_set_adjustment (accessible,
					     GTK_ORIENTATION_HORIZONTAL,
					     icon_view->priv->hadjustment);
  if (icon_view->priv->vadjustment)
    gtk_icon_view_accessible_set_adjustment (accessible,
					     GTK_ORIENTATION_VERTICAL,
					     icon_view->priv->vadjustment);
  g_signal_connect (data,
                    "notify",
                    G_CALLBACK (gtk_icon_view_accessible_notify_gtk),
                    NULL);

  priv->model = icon_view->priv->model;
  if (priv->model)
    {
      g_object_add_weak_pointer (G_OBJECT (priv->model), (gpointer *)&priv->model);
      gtk_icon_view_accessible_connect_model_signals (icon_view);
    }
                          
  accessible->role = ATK_ROLE_LAYERED_PANE;
}

static void
gtk_icon_view_accessible_finalize (GObject *object)
{
  GtkIconViewAccessiblePrivate *priv;

  priv = gtk_icon_view_accessible_get_priv (ATK_OBJECT (object));
  gtk_icon_view_accessible_clear_cache (priv);

  g_free (priv);

  G_OBJECT_CLASS (accessible_parent_class)->finalize (object);
}

static void
gtk_icon_view_accessible_destroyed (GtkWidget *widget,
                                    GtkAccessible *accessible)
{
  AtkObject *atk_obj;
  GtkIconViewAccessiblePrivate *priv;

  atk_obj = ATK_OBJECT (accessible);
  priv = gtk_icon_view_accessible_get_priv (atk_obj);
  if (priv->old_hadj)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->old_hadj),
                                    (gpointer *)&priv->old_hadj);
          
      g_signal_handlers_disconnect_by_func (priv->old_hadj,
                                            (gpointer) gtk_icon_view_accessible_adjustment_changed,
                                            accessible);
      priv->old_hadj = NULL;
    }
  if (priv->old_vadj)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->old_vadj),
                                    (gpointer *)&priv->old_vadj);
          
      g_signal_handlers_disconnect_by_func (priv->old_vadj,
                                            (gpointer) gtk_icon_view_accessible_adjustment_changed,
                                            accessible);
      priv->old_vadj = NULL;
    }
}

static void
gtk_icon_view_accessible_connect_widget_destroyed (GtkAccessible *accessible)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget)
    {
      g_signal_connect_after (widget,
                              "destroy",
                              G_CALLBACK (gtk_icon_view_accessible_destroyed),
                              accessible);
    }
  GTK_ACCESSIBLE_CLASS (accessible_parent_class)->connect_widget_destroyed (accessible);
}

static void
gtk_icon_view_accessible_class_init (AtkObjectClass *klass)
{
  GObjectClass *gobject_class;
  GtkAccessibleClass *accessible_class;

  accessible_parent_class = g_type_class_peek_parent (klass);

  gobject_class = (GObjectClass *)klass;
  accessible_class = (GtkAccessibleClass *)klass;

  gobject_class->finalize = gtk_icon_view_accessible_finalize;

  klass->get_n_children = gtk_icon_view_accessible_get_n_children;
  klass->ref_child = gtk_icon_view_accessible_ref_child;
  klass->initialize = gtk_icon_view_accessible_initialize;

  accessible_class->connect_widget_destroyed = gtk_icon_view_accessible_connect_widget_destroyed;

  accessible_private_data_quark = g_quark_from_static_string ("icon_view-accessible-private-data");
}

static AtkObject*
gtk_icon_view_accessible_ref_accessible_at_point (AtkComponent *component,
                                                  gint          x,
                                                  gint          y,
                                                  AtkCoordType  coord_type)
{
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;
  gint x_pos, y_pos;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (component));
  if (widget == NULL)
  /* State is defunct */
    return NULL;

  icon_view = GTK_ICON_VIEW (widget);
  atk_component_get_extents (component, &x_pos, &y_pos, NULL, NULL, coord_type);
  item = gtk_icon_view_get_item_at_coords (icon_view, x - x_pos, y - y_pos, TRUE, NULL);
  if (item)
    return gtk_icon_view_accessible_ref_child (ATK_OBJECT (component), item->index);

  return NULL;
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
  iface->ref_accessible_at_point = gtk_icon_view_accessible_ref_accessible_at_point;
}

static gboolean
gtk_icon_view_accessible_add_selection (AtkSelection *selection,
                                        gint i)
{
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  icon_view = GTK_ICON_VIEW (widget);

  item = g_list_nth_data (icon_view->priv->items, i);

  if (!item)
    return FALSE;

  gtk_icon_view_select_item (icon_view, item);

  return TRUE;
}

static gboolean
gtk_icon_view_accessible_clear_selection (AtkSelection *selection)
{
  GtkWidget *widget;
  GtkIconView *icon_view;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  icon_view = GTK_ICON_VIEW (widget);
  gtk_icon_view_unselect_all (icon_view);

  return TRUE;
}

static AtkObject*
gtk_icon_view_accessible_ref_selection (AtkSelection *selection,
                                        gint          i)
{
  GList *l;
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return NULL;

  icon_view = GTK_ICON_VIEW (widget);

  l = icon_view->priv->items;
  while (l)
    {
      item = l->data;
      if (item->selected)
        {
          if (i == 0)
	    return atk_object_ref_accessible_child (gtk_widget_get_accessible (widget), item->index);
          else
            i--;
        }
      l = l->next;
    }

  return NULL;
}

static gint
gtk_icon_view_accessible_get_selection_count (AtkSelection *selection)
{
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;
  GList *l;
  gint count;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return 0;

  icon_view = GTK_ICON_VIEW (widget);

  l = icon_view->priv->items;
  count = 0;
  while (l)
    {
      item = l->data;

      if (item->selected)
	count++;

      l = l->next;
    }

  return count;
}

static gboolean
gtk_icon_view_accessible_is_child_selected (AtkSelection *selection,
                                            gint          i)
{
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  icon_view = GTK_ICON_VIEW (widget);

  item = g_list_nth_data (icon_view->priv->items, i);
  if (!item)
    return FALSE;

  return item->selected;
}

static gboolean
gtk_icon_view_accessible_remove_selection (AtkSelection *selection,
                                           gint          i)
{
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;
  GList *l;
  gint count;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  icon_view = GTK_ICON_VIEW (widget);
  l = icon_view->priv->items;
  count = 0;
  while (l)
    {
      item = l->data;
      if (item->selected)
        {
          if (count == i)
            {
              gtk_icon_view_unselect_item (icon_view, item);
              return TRUE;
            }
          count++;
        }
      l = l->next;
    }

  return FALSE;
}
 
static gboolean
gtk_icon_view_accessible_select_all_selection (AtkSelection *selection)
{
  GtkWidget *widget;
  GtkIconView *icon_view;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  icon_view = GTK_ICON_VIEW (widget);
  gtk_icon_view_select_all (icon_view);
  return TRUE;
}

static void
gtk_icon_view_accessible_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gtk_icon_view_accessible_add_selection;
  iface->clear_selection = gtk_icon_view_accessible_clear_selection;
  iface->ref_selection = gtk_icon_view_accessible_ref_selection;
  iface->get_selection_count = gtk_icon_view_accessible_get_selection_count;
  iface->is_child_selected = gtk_icon_view_accessible_is_child_selected;
  iface->remove_selection = gtk_icon_view_accessible_remove_selection;
  iface->select_all_selection = gtk_icon_view_accessible_select_all_selection;
}

static GType
gtk_icon_view_accessible_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      GTypeInfo tinfo =
      {
        0, /* class size */
        (GBaseInitFunc) NULL, /* base init */
        (GBaseFinalizeFunc) NULL, /* base finalize */
        (GClassInitFunc) gtk_icon_view_accessible_class_init,
        (GClassFinalizeFunc) NULL, /* class finalize */
        NULL, /* class data */
        0, /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) NULL, /* instance init */
        NULL /* value table */
      };
      const GInterfaceInfo atk_component_info =
      {
        (GInterfaceInitFunc) atk_component_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };
      const GInterfaceInfo atk_selection_info =
      {
        (GInterfaceInitFunc) gtk_icon_view_accessible_selection_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };

      /*
       * Figure out the size of the class and instance
       * we are deriving from
       */
      AtkObjectFactory *factory;
      GType derived_type;
      GTypeQuery query;
      GType derived_atk_type;

      derived_type = g_type_parent (GTK_TYPE_ICON_VIEW);
      factory = atk_registry_get_factory (atk_get_default_registry (), 
                                          derived_type);
      derived_atk_type = atk_object_factory_get_accessible_type (factory);
      g_type_query (derived_atk_type, &query);
      tinfo.class_size = query.class_size;
      tinfo.instance_size = query.instance_size;
 
      type = g_type_register_static (derived_atk_type, 
                                     I_("GtkIconViewAccessible"), 
                                     &tinfo, 0);
      g_type_add_interface_static (type, ATK_TYPE_COMPONENT,
                                   &atk_component_info);
      g_type_add_interface_static (type, ATK_TYPE_SELECTION,
                                   &atk_selection_info);
    }
  return type;
}

static AtkObject *
gtk_icon_view_accessible_new (GObject *obj)
{
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_WIDGET (obj), NULL);

  accessible = g_object_new (gtk_icon_view_accessible_get_type (), NULL);
  atk_object_initialize (accessible, obj);

  return accessible;
}

static GType
gtk_icon_view_accessible_factory_get_accessible_type (void)
{
  return gtk_icon_view_accessible_get_type ();
}

static AtkObject*
gtk_icon_view_accessible_factory_create_accessible (GObject *obj)
{
  return gtk_icon_view_accessible_new (obj);
}

static void
gtk_icon_view_accessible_factory_class_init (AtkObjectFactoryClass *klass)
{
  klass->create_accessible = gtk_icon_view_accessible_factory_create_accessible;
  klass->get_accessible_type = gtk_icon_view_accessible_factory_get_accessible_type;
}

static GType
gtk_icon_view_accessible_factory_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      const GTypeInfo tinfo =
      {
        sizeof (AtkObjectFactoryClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gtk_icon_view_accessible_factory_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (AtkObjectFactory),
        0,             /* n_preallocs */
        NULL, NULL
      };

      type = g_type_register_static (ATK_TYPE_OBJECT_FACTORY, 
                                    I_("GtkIconViewAccessibleFactory"),
                                    &tinfo, 0);
    }
  return type;
}


static AtkObject *
gtk_icon_view_get_accessible (GtkWidget *widget)
{
  static gboolean first_time = TRUE;

  if (first_time)
    {
      AtkObjectFactory *factory;
      AtkRegistry *registry;
      GType derived_type; 
      GType derived_atk_type; 

      /*
       * Figure out whether accessibility is enabled by looking at the
       * type of the accessible object which would be created for
       * the parent type of GtkIconView.
       */
      derived_type = g_type_parent (GTK_TYPE_ICON_VIEW);

      registry = atk_get_default_registry ();
      factory = atk_registry_get_factory (registry,
                                          derived_type);
      derived_atk_type = atk_object_factory_get_accessible_type (factory);
      if (g_type_is_a (derived_atk_type, GTK_TYPE_ACCESSIBLE)) 
	atk_registry_set_factory_type (registry, 
				       GTK_TYPE_ICON_VIEW,
				       gtk_icon_view_accessible_factory_get_type ());
      first_time = FALSE;
    } 
  return GTK_WIDGET_CLASS (gtk_icon_view_parent_class)->get_accessible (widget);
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
    parent_buildable_iface->custom_tag_end (buildable, builder, child, tagname,
					    data);
}
