/* eggiconlist.h
 * Copyright (C) 2002  Anders Carlsson <andersca@gnu.org>
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

#include "eggiconlist.h"

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkbindings.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include <glib/gi18n.h>

#include "eggmarshalers.h"

#define MINIMUM_ICON_ITEM_WIDTH 100
#define ICON_TEXT_PADDING 3

#define EGG_ICON_LIST_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EGG_TYPE_ICON_LIST, EggIconListPrivate))
#define VALID_MODEL_AND_COLUMNS(obj) ((obj)->priv->model != NULL)

struct _EggIconListItem
{
  gint ref_count;

  GtkTreeIter iter;
  int index;
  
  gint row, col;

  /* Bounding boxes */
  gint x, y;
  gint width, height;

  gint pixbuf_x, pixbuf_y;
  gint pixbuf_height, pixbuf_width;

  gint layout_x, layout_y;
  gint layout_width, layout_height;

  guint selected : 1;
  guint selected_before_rubberbanding : 1;
};

struct _EggIconListPrivate
{
  gint width, height;

  gint text_column;
  gint markup_column;
  gint pixbuf_column;
  
  GtkSelectionMode selection_mode;

  GdkWindow *bin_window;

  GtkTreeModel *model;
  
  GList *items;
  
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;

  guint layout_idle_id;
  
  gboolean rubberbanding;
  gint rubberband_x1, rubberband_y1;
  gint rubberband_x2, rubberband_y2;

  guint scroll_timeout_id;
  gint scroll_value_diff;
  gint event_last_x, event_last_y;

  EggIconListItem *anchor_item;
  EggIconListItem *cursor_item;

  guint ctrl_pressed : 1;
  guint shift_pressed : 1;
  
  EggIconListItem *last_single_clicked;
  
  /* Drag-and-drop. */
  gint pressed_button;
  gint press_start_x;
  gint press_start_y;

  /* Layout used to draw icon text */
  PangoLayout *layout;
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
  PROP_MODEL,
};

static void egg_icon_list_class_init      (EggIconListClass *klass);
static void egg_icon_list_init            (EggIconList      *icon_list);

/* GObject signals */
static void egg_icon_list_finalize     (GObject      *object);
static void egg_icon_list_set_property (GObject      *object,
					guint         prop_id,
					const GValue *value,
					GParamSpec   *pspec);
static void egg_icon_list_get_property (GObject      *object,
					guint         prop_id,
					GValue       *value,
					GParamSpec   *pspec);


/* GtkObject signals */
static void egg_icon_list_destroy (GtkObject *object);

/* GtkWidget signals */
static void     egg_icon_list_realize        (GtkWidget      *widget);
static void     egg_icon_list_unrealize      (GtkWidget      *widget);
static void     egg_icon_list_map            (GtkWidget      *widget);
static void     egg_icon_list_size_request   (GtkWidget      *widget,
					      GtkRequisition *requisition);
static void     egg_icon_list_size_allocate  (GtkWidget      *widget,
					      GtkAllocation  *allocation);
static gboolean egg_icon_list_expose         (GtkWidget      *widget,
					      GdkEventExpose *expose);
static gboolean egg_icon_list_motion         (GtkWidget      *widget,
					      GdkEventMotion *event);
static gboolean egg_icon_list_button_press   (GtkWidget      *widget,
					      GdkEventButton *event);
static gboolean egg_icon_list_button_release (GtkWidget      *widget,
					      GdkEventButton *event);

/* EggIconList signals */
static void     egg_icon_list_set_adjustments             (EggIconList             *icon_list,
							   GtkAdjustment           *hadj,
							   GtkAdjustment           *vadj);
static void     egg_icon_list_real_select_all             (EggIconList             *icon_list);
static void     egg_icon_list_real_unselect_all           (EggIconList             *icon_list);
static void     egg_icon_list_real_select_cursor_item     (EggIconList             *icon_list);
static void     egg_icon_list_real_toggle_cursor_item     (EggIconList             *icon_list);
static void     egg_icon_list_select_all_between          (EggIconList             *icon_list,
							   EggIconListItem         *anchor,
							   EggIconListItem         *cursor,
							   gboolean                 emit);

/* Internal functions */
static void       egg_icon_list_adjustment_changed          (GtkAdjustment   *adjustment,
							     EggIconList     *icon_list);
static void       egg_icon_list_layout                      (EggIconList     *icon_list);
static void       egg_icon_list_paint_item                  (EggIconList     *icon_list,
							     EggIconListItem *item,
							     GdkRectangle    *area);
static void       egg_icon_list_paint_rubberband            (EggIconList     *icon_list,
							     GdkRectangle    *area);
static void       egg_icon_list_queue_draw_item             (EggIconList     *icon_list,
							     EggIconListItem *item);
static void       egg_icon_list_queue_layout                (EggIconList     *icon_list);
static void       egg_icon_list_set_cursor_item             (EggIconList     *icon_list,
							     EggIconListItem *item);
static void       egg_icon_list_start_rubberbanding         (EggIconList     *icon_list,
							     gint             x,
							     gint             y);
static void       egg_icon_list_stop_rubberbanding          (EggIconList     *icon_list);
static void       egg_icon_list_update_rubberband_selection (EggIconList     *icon_list);
static gboolean   egg_icon_list_item_hit_test               (EggIconListItem *item,
							     gint             x,
							     gint             y,
							     gint             width,
							     gint             height);
static gboolean   egg_icon_list_maybe_begin_dragging_items  (EggIconList     *icon_list,
							     GdkEventMotion  *event);
static gboolean   egg_icon_list_unselect_all_internal       (EggIconList     *icon_list,
							     gboolean         emit);
static void       egg_icon_list_calculate_item_size         (EggIconList     *icon_list,
							     EggIconListItem *item);
static void       rubberbanding                             (gpointer         data);
static void       egg_icon_list_item_invalidate_size        (EggIconListItem *item);
static void       egg_icon_list_invalidate_sizes            (EggIconList     *icon_list);
static void       egg_icon_list_add_move_binding            (GtkBindingSet   *binding_set,
							     guint            keyval,
							     guint            modmask,
							     GtkMovementStep  step,
							     gint             count);
static gboolean   egg_icon_list_real_move_cursor            (EggIconList     *icon_list,
							     GtkMovementStep  step,
							     gint             count);
static void       egg_icon_list_move_cursor_up_down         (EggIconList     *icon_list,
							     gint             count);
static void       egg_icon_list_move_cursor_page_up_down    (EggIconList     *icon_list,
							     gint             count);
static void       egg_icon_list_move_cursor_left_right      (EggIconList     *icon_list,
							     gint             count);
static void       egg_icon_list_move_cursor_start_end       (EggIconList     *icon_list,
							     gint             count);
static void       egg_icon_list_scroll_to_item              (EggIconList     *icon_list,
							     EggIconListItem *item);
static GdkPixbuf *egg_icon_list_get_item_icon               (EggIconList     *icon_list,
							     EggIconListItem *item);
static void       egg_icon_list_update_item_text            (EggIconList     *icon_list,
							     EggIconListItem *item);
static void       egg_icon_list_select_item                 (EggIconList     *icon_list,
							     EggIconListItem *item);
static void       egg_icon_list_unselect_item               (EggIconList     *icon_list,
							     EggIconListItem *item);

static EggIconListItem *
egg_icon_list_get_item_at_pos (EggIconList *icon_list,
			       gint         x,
			       gint         y);





static GtkContainerClass *parent_class = NULL;
static guint icon_list_signals[LAST_SIGNAL] = { 0 };

GType
egg_icon_list_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
	{	  
	  sizeof (EggIconListClass),
	  NULL,		/* base_init */
	  NULL,		/* base_finalize */
	  (GClassInitFunc) egg_icon_list_class_init,
	  NULL,		/* class_finalize */
	  NULL,		/* class_data */
	  sizeof (EggIconList),
	  0,              /* n_preallocs */
	  (GInstanceInitFunc) egg_icon_list_init
	};

      object_type = g_type_register_static (GTK_TYPE_CONTAINER, "EggIconList", &object_info, 0);
    }

  return object_type;
}

static void
egg_icon_list_class_init (EggIconListClass *klass)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;
  
  parent_class = g_type_class_peek_parent (klass);
  binding_set = gtk_binding_set_by_class (klass);

  g_type_class_add_private (klass, sizeof (EggIconListPrivate));

  gobject_class = (GObjectClass *) klass;
  object_class = (GtkObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;

  gobject_class->finalize = egg_icon_list_finalize;
  gobject_class->set_property = egg_icon_list_set_property;
  gobject_class->get_property = egg_icon_list_get_property;

  object_class->destroy = egg_icon_list_destroy;
  
  widget_class->realize = egg_icon_list_realize;
  widget_class->unrealize = egg_icon_list_unrealize;
  widget_class->map = egg_icon_list_map;
  widget_class->size_request = egg_icon_list_size_request;
  widget_class->size_allocate = egg_icon_list_size_allocate;
  widget_class->expose_event = egg_icon_list_expose;
  widget_class->motion_notify_event = egg_icon_list_motion;
  widget_class->button_press_event = egg_icon_list_button_press;
  widget_class->button_release_event = egg_icon_list_button_release;
  
  klass->set_scroll_adjustments = egg_icon_list_set_adjustments;
  klass->select_all = egg_icon_list_real_select_all;
  klass->unselect_all = egg_icon_list_real_unselect_all;
  klass->select_cursor_item = egg_icon_list_real_select_cursor_item;
  klass->toggle_cursor_item = egg_icon_list_real_toggle_cursor_item;
  klass->move_cursor = egg_icon_list_real_move_cursor;
  
  /* Properties */
  g_object_class_install_property (gobject_class,
				   PROP_SELECTION_MODE,
				   g_param_spec_enum ("selection_mode",
						      _("Selection mode"),
						      _("The selection mode"),
						      GTK_TYPE_SELECTION_MODE,
						      GTK_SELECTION_SINGLE,
						      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_PIXBUF_COLUMN,
				   g_param_spec_int ("pixbuf_column",
						      _("Pixbuf column"),
						      _("Model column used to retrieve the icon pixbuf from"),
						     -1, G_MAXINT, -1,
						     G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_TEXT_COLUMN,
				   g_param_spec_int ("text_column",
						      _("Text column"),
						      _("Model column used to retrieve the text from"),
						     -1, G_MAXINT, -1,
						     G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_MARKUP_COLUMN,
				   g_param_spec_int ("markup_column",
						      _("Markup column"),
						      _("Model column used to retrieve the text if using pango markup"),
						     -1, G_MAXINT, -1,
						     G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
							_("Icon List Model"),
							_("The model for the icon list"),
							GTK_TYPE_TREE_MODEL,
							G_PARAM_READWRITE));
  
  /* Style properties */
#define _ICON_LIST_TOP_MARGIN 6
#define _ICON_LIST_BOTTOM_MARGIN 6
#define _ICON_LIST_LEFT_MARGIN 6
#define _ICON_LIST_RIGHT_MARGIN 6
#define _ICON_LIST_ICON_PADDING 6

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("icon_padding",
							     _("Icon padding"),
							     _("Number of pixels between icons"),
							     0,
							     G_MAXINT,
							     _ICON_LIST_ICON_PADDING,
							     G_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("top_margin",
							     _("Top margin"),
							     _("Number of pixels in top margin"),
							     0,
							     G_MAXINT,
							     _ICON_LIST_TOP_MARGIN,
							     G_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("bottom_margin",
							     _("Bottom margin"),
							     _("Number of pixels in bottom margin"),
							     0,
							     G_MAXINT,
							     _ICON_LIST_BOTTOM_MARGIN,
							     G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("left_margin",
							     _("Left margin"),
							     _("Number of pixels in left margin"),
							     0,
							     G_MAXINT,
							     _ICON_LIST_LEFT_MARGIN,
							     G_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("right_margin",
							     _("Right margin"),
							     _("Number of pixels in right margin"),
							     0,
							     G_MAXINT,
							     _ICON_LIST_RIGHT_MARGIN,
							     G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boxed ("selection_box_color",
                                                               _("Selection Box Color"),
                                                               _("Color of the selection box"),
                                                               GDK_TYPE_COLOR,
                                                               G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_uchar ("selection_box_alpha",
                                                               _("Selection Box Alpha"),
                                                               _("Opacity of the selection box"),
                                                               0, 0xff,
                                                               0x40,
                                                               G_PARAM_READABLE));

  /* Signals */
  widget_class->set_scroll_adjustments_signal =
    g_signal_new ("set_scroll_adjustments",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggIconListClass, set_scroll_adjustments),
		  NULL, NULL, 
		  _egg_marshal_VOID__OBJECT_OBJECT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);

  icon_list_signals[ITEM_ACTIVATED] =
    g_signal_new ("item_activated",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggIconListClass, item_activated),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_TREE_PATH);

  icon_list_signals[SELECTION_CHANGED] =
    g_signal_new ("selection_changed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (EggIconListClass, selection_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  icon_list_signals[SELECT_ALL] =
    g_signal_new ("select_all",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (EggIconListClass, select_all),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  icon_list_signals[UNSELECT_ALL] =
    g_signal_new ("unselect_all",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (EggIconListClass, unselect_all),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  icon_list_signals[SELECT_CURSOR_ITEM] =
    g_signal_new ("select_cursor_item",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (EggIconListClass, select_cursor_item),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  icon_list_signals[SELECT_CURSOR_ITEM] =
    g_signal_new ("toggle_cursor_item",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (EggIconListClass, toggle_cursor_item),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  icon_list_signals[MOVE_CURSOR] =
    g_signal_new ("move_cursor",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (EggIconListClass, move_cursor),
		  NULL, NULL,
		  _egg_marshal_BOOLEAN__ENUM_INT,
		  G_TYPE_BOOLEAN, 2,
		  GTK_TYPE_MOVEMENT_STEP,
		  G_TYPE_INT);

  /* Key bindings */
  gtk_binding_entry_add_signal (binding_set, GDK_a, GDK_CONTROL_MASK, "select_all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_a, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "unselect_all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_space, 0, "select_cursor_item", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_CONTROL_MASK, "toggle_cursor_item", 0);

  egg_icon_list_add_move_binding (binding_set, GDK_Up, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);
  egg_icon_list_add_move_binding (binding_set, GDK_KP_Up, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);

  egg_icon_list_add_move_binding (binding_set, GDK_Down, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);
  egg_icon_list_add_move_binding (binding_set, GDK_KP_Down, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);

  egg_icon_list_add_move_binding (binding_set, GDK_p, GDK_CONTROL_MASK,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);

  egg_icon_list_add_move_binding (binding_set, GDK_n, GDK_CONTROL_MASK,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);

  egg_icon_list_add_move_binding (binding_set, GDK_Home, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, -1);
  egg_icon_list_add_move_binding (binding_set, GDK_KP_Home, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, -1);

  egg_icon_list_add_move_binding (binding_set, GDK_End, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, 1);
  egg_icon_list_add_move_binding (binding_set, GDK_KP_End, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, 1);

  egg_icon_list_add_move_binding (binding_set, GDK_Page_Up, 0,
				  GTK_MOVEMENT_PAGES, -1);
  egg_icon_list_add_move_binding (binding_set, GDK_KP_Page_Up, 0,
				  GTK_MOVEMENT_PAGES, -1);

  egg_icon_list_add_move_binding (binding_set, GDK_Page_Down, 0,
				  GTK_MOVEMENT_PAGES, 1);
  egg_icon_list_add_move_binding (binding_set, GDK_KP_Page_Down, 0,
				  GTK_MOVEMENT_PAGES, 1);

  egg_icon_list_add_move_binding (binding_set, GDK_Right, 0, 
				  GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  egg_icon_list_add_move_binding (binding_set, GDK_Left, 0, 
				  GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  egg_icon_list_add_move_binding (binding_set, GDK_KP_Right, 0, 
				  GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  egg_icon_list_add_move_binding (binding_set, GDK_KP_Left, 0, 
				  GTK_MOVEMENT_VISUAL_POSITIONS, -1);
}

static void
egg_icon_list_init (EggIconList *icon_list)
{
  icon_list->priv = EGG_ICON_LIST_GET_PRIVATE (icon_list);
  
  icon_list->priv->width = 0;
  icon_list->priv->height = 0;
  icon_list->priv->selection_mode = GTK_SELECTION_SINGLE;
  icon_list->priv->pressed_button = -1;
  icon_list->priv->press_start_x = -1;
  icon_list->priv->press_start_y = -1;
  icon_list->priv->text_column = -1;
  icon_list->priv->markup_column = -1;  
  icon_list->priv->pixbuf_column = -1;
  
  icon_list->priv->layout = gtk_widget_create_pango_layout (GTK_WIDGET (icon_list), NULL);

  pango_layout_set_wrap (icon_list->priv->layout, PANGO_WRAP_WORD_CHAR);

  GTK_WIDGET_SET_FLAGS (icon_list, GTK_CAN_FOCUS);
  
  egg_icon_list_set_adjustments (icon_list, NULL, NULL);
}

static void
egg_icon_list_destroy (GtkObject *object)
{
  EggIconList *icon_list;

  icon_list = EGG_ICON_LIST (object);
  
  egg_icon_list_set_model (icon_list, NULL);
  
  if (icon_list->priv->layout_idle_id != 0)
    g_source_remove (icon_list->priv->layout_idle_id);

  if (icon_list->priv->scroll_timeout_id != 0)
    g_source_remove (icon_list->priv->scroll_timeout_id);
  
  (GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* GObject methods */
static void
egg_icon_list_finalize (GObject *object)
{
  EggIconList *icon_list;

  icon_list = EGG_ICON_LIST (object);

  g_object_unref (icon_list->priv->layout);
  
  (G_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
egg_icon_list_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  EggIconList *icon_list;

  icon_list = EGG_ICON_LIST (object);

  switch (prop_id)
    {
    case PROP_SELECTION_MODE:
      egg_icon_list_set_selection_mode (icon_list, g_value_get_enum (value));
      break;
    case PROP_PIXBUF_COLUMN:
      egg_icon_list_set_pixbuf_column (icon_list, g_value_get_int (value));
      break;
    case PROP_TEXT_COLUMN:
      egg_icon_list_set_text_column (icon_list, g_value_get_int (value));
      break;
    case PROP_MARKUP_COLUMN:
      egg_icon_list_set_markup_column (icon_list, g_value_get_int (value));
      break;
    case PROP_MODEL:
      egg_icon_list_set_model (icon_list, g_value_get_object (value));
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_icon_list_get_property (GObject      *object,
			    guint         prop_id,
			    GValue       *value,
			    GParamSpec   *pspec)
{
  EggIconList *icon_list;

  icon_list = EGG_ICON_LIST (object);

  switch (prop_id)
    {
    case PROP_SELECTION_MODE:
      g_value_set_enum (value, icon_list->priv->selection_mode);
      break;
    case PROP_PIXBUF_COLUMN:
      g_value_set_int (value, icon_list->priv->pixbuf_column);
      break;
    case PROP_TEXT_COLUMN:
      g_value_set_int (value, icon_list->priv->text_column);
      break;
    case PROP_MARKUP_COLUMN:
      g_value_set_int (value, icon_list->priv->markup_column);
      break;
    case PROP_MODEL:
      g_value_set_object (value, icon_list->priv->model);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* GtkWidget signals */
static void
egg_icon_list_realize (GtkWidget *widget)
{
  EggIconList *icon_list;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  icon_list = EGG_ICON_LIST (widget);

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

  /* Make the window for the icon list */
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = MAX (icon_list->priv->width, widget->allocation.width);
  attributes.height = MAX (icon_list->priv->height, widget->allocation.height);
  attributes.event_mask = (GDK_EXPOSURE_MASK |
			   GDK_SCROLL_MASK |
			   GDK_POINTER_MOTION_MASK |
			   GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK |
			   GDK_KEY_PRESS_MASK |
			   GDK_KEY_RELEASE_MASK) |
    gtk_widget_get_events (widget);
  
  icon_list->priv->bin_window = gdk_window_new (widget->window,
					  &attributes, attributes_mask);
  gdk_window_set_user_data (icon_list->priv->bin_window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gdk_window_set_background (icon_list->priv->bin_window, &widget->style->base[widget->state]);
  gdk_window_set_background (widget->window, &widget->style->base[widget->state]);

  
}

static void
egg_icon_list_unrealize (GtkWidget *widget)
{
  EggIconList *icon_list;

  icon_list = EGG_ICON_LIST (widget);

  gdk_window_set_user_data (icon_list->priv->bin_window, NULL);
  gdk_window_destroy (icon_list->priv->bin_window);
  icon_list->priv->bin_window = NULL;

  /* GtkWidget::unrealize destroys children and widget->window */
  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
egg_icon_list_map (GtkWidget *widget)
{
  EggIconList *icon_list;

  icon_list = EGG_ICON_LIST (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  gdk_window_show (icon_list->priv->bin_window);
  gdk_window_show (widget->window);
}

static void
egg_icon_list_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  EggIconList *icon_list;

  icon_list = EGG_ICON_LIST (widget);

  requisition->width = icon_list->priv->width;
  requisition->height = icon_list->priv->height;
}

static void
egg_icon_list_size_allocate (GtkWidget      *widget,
			     GtkAllocation  *allocation)
{
  EggIconList *icon_list;

  widget->allocation = *allocation;
  
  icon_list = EGG_ICON_LIST (widget);
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      gdk_window_resize (icon_list->priv->bin_window,
			 MAX (icon_list->priv->width, allocation->width),
			 MAX (icon_list->priv->height, allocation->height));
    }

  icon_list->priv->hadjustment->page_size = allocation->width;
  icon_list->priv->hadjustment->page_increment = allocation->width * 0.9;
  icon_list->priv->hadjustment->step_increment = allocation->width * 0.1;
  icon_list->priv->hadjustment->lower = 0;
  icon_list->priv->hadjustment->upper = MAX (allocation->width, icon_list->priv->width);
  gtk_adjustment_changed (icon_list->priv->hadjustment);

  icon_list->priv->vadjustment->page_size = allocation->height;
  icon_list->priv->vadjustment->page_increment = allocation->height * 0.9;
  icon_list->priv->vadjustment->step_increment = allocation->width * 0.1;
  icon_list->priv->vadjustment->lower = 0;
  icon_list->priv->vadjustment->upper = MAX (allocation->height, icon_list->priv->height);
  gtk_adjustment_changed (icon_list->priv->vadjustment);

  egg_icon_list_layout (icon_list);
}

static gboolean
egg_icon_list_expose (GtkWidget *widget,
		      GdkEventExpose *expose)
{
  EggIconList *icon_list;
  GList *icons;

  icon_list = EGG_ICON_LIST (widget);

  if (expose->window != icon_list->priv->bin_window)
    return FALSE;

  for (icons = icon_list->priv->items; icons; icons = icons->next) {
    EggIconListItem *item = icons->data;
    GdkRectangle item_rectangle;

    item_rectangle.x = item->x;
    item_rectangle.y = item->y;
    item_rectangle.width = item->width;
    item_rectangle.height = item->height;

    if (gdk_region_rect_in (expose->region, &item_rectangle) == GDK_OVERLAP_RECTANGLE_OUT)
      continue;

    egg_icon_list_paint_item (icon_list, item, &expose->area);
  }

  if (icon_list->priv->rubberbanding)
    {
      GdkRectangle *rectangles;
      gint n_rectangles;
      
      gdk_region_get_rectangles (expose->region,
				 &rectangles,
				 &n_rectangles);
      
      while (n_rectangles--)
	egg_icon_list_paint_rubberband (icon_list, &rectangles[n_rectangles]);

      g_free (rectangles);
    }

  return TRUE;
}

static gboolean
scroll_timeout (gpointer data)
{
  EggIconList *icon_list;
  gdouble value;
  
  icon_list = data;

  value = MIN (icon_list->priv->vadjustment->value +
	       icon_list->priv->scroll_value_diff,
	       icon_list->priv->vadjustment->upper -
	       icon_list->priv->vadjustment->page_size);

  gtk_adjustment_set_value (icon_list->priv->vadjustment,
			    value);

  rubberbanding (icon_list);
  
  return TRUE;
}

static gboolean
egg_icon_list_motion (GtkWidget      *widget,
		      GdkEventMotion *event)
{
  EggIconList *icon_list;
  gint abs_y;
  
  icon_list = EGG_ICON_LIST (widget);

  egg_icon_list_maybe_begin_dragging_items (icon_list, event);

  if (icon_list->priv->rubberbanding)
    {
      rubberbanding (widget);

      abs_y = event->y - icon_list->priv->height *
	(icon_list->priv->vadjustment->value /
	 (icon_list->priv->vadjustment->upper -
	  icon_list->priv->vadjustment->lower));

      if (abs_y < 0 || abs_y > widget->allocation.height)
	{
	  if (icon_list->priv->scroll_timeout_id == 0)
	    icon_list->priv->scroll_timeout_id = g_timeout_add (30, scroll_timeout, icon_list);

	  if (abs_y < 0)
	    icon_list->priv->scroll_value_diff = abs_y;
	  else
	    icon_list->priv->scroll_value_diff = abs_y - widget->allocation.height;

	  icon_list->priv->event_last_x = event->x;
	  icon_list->priv->event_last_y = event->y;
	}
      else if (icon_list->priv->scroll_timeout_id != 0)
	{
	  g_source_remove (icon_list->priv->scroll_timeout_id);

	  icon_list->priv->scroll_timeout_id = 0;
	}
    }
  
  return TRUE;
}

static gboolean
egg_icon_list_button_press (GtkWidget      *widget,
			    GdkEventButton *event)
{
  EggIconList *icon_list;
  EggIconListItem *item;
  gboolean dirty = FALSE;

  icon_list = EGG_ICON_LIST (widget);

  if (event->window != icon_list->priv->bin_window)
    return FALSE;

  if (!GTK_WIDGET_HAS_FOCUS (widget))
    gtk_widget_grab_focus (widget);

  if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
    {

      item = egg_icon_list_get_item_at_pos (icon_list,
					    event->x, event->y);
      
      if (item != NULL)
	{
	  egg_icon_list_scroll_to_item (icon_list, item);
	  
	  if (icon_list->priv->selection_mode == GTK_SELECTION_NONE)
	    {
	      egg_icon_list_set_cursor_item (icon_list, item);
	    }
	  else if (icon_list->priv->selection_mode == GTK_SELECTION_MULTIPLE &&
		   (event->state & GDK_SHIFT_MASK))
	    {
	      egg_icon_list_unselect_all_internal (icon_list, FALSE);

	      egg_icon_list_set_cursor_item (icon_list, item);
	      if (!icon_list->priv->anchor_item)
		icon_list->priv->anchor_item = item;
	      else 
		egg_icon_list_select_all_between (icon_list,
						  icon_list->priv->anchor_item,
						  item, FALSE);
	      dirty = TRUE;
	    }
	  else 
	    {
	      if (icon_list->priv->selection_mode == GTK_SELECTION_MULTIPLE &&
		  (event->state & GDK_CONTROL_MASK))
		{
		  item->selected = !item->selected;
		  egg_icon_list_queue_draw_item (icon_list, item);
		  dirty = TRUE;
		}
	      else
		{
		  if (!item->selected)
		    {
		      egg_icon_list_unselect_all_internal (icon_list, FALSE);
		      
		      item->selected = TRUE;
		      egg_icon_list_queue_draw_item (icon_list, item);
		      dirty = TRUE;
		    }
		}
	      egg_icon_list_set_cursor_item (icon_list, item);
	      icon_list->priv->anchor_item = item;
	    }
	    
	  /* Save press to possibly begin a drag */
	  if (icon_list->priv->pressed_button < 0)
	    {
	      icon_list->priv->pressed_button = event->button;
	      icon_list->priv->press_start_x = event->x;
	      icon_list->priv->press_start_y = event->y;
	    }

	  if (!icon_list->priv->last_single_clicked)
	    icon_list->priv->last_single_clicked = item;
	}
      else
	{
	  if (icon_list->priv->selection_mode != GTK_SELECTION_BROWSE &&
	      !(event->state & GDK_CONTROL_MASK))
	    {
	      dirty = egg_icon_list_unselect_all_internal (icon_list, FALSE);
	    }
	  
	  if (icon_list->priv->selection_mode == GTK_SELECTION_MULTIPLE)
	    egg_icon_list_start_rubberbanding (icon_list, event->x, event->y);
	}

    }

  if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
    {
      item = egg_icon_list_get_item_at_pos (icon_list,
					    event->x, event->y);

      if (item && item == icon_list->priv->last_single_clicked)
	{
	  GtkTreePath *path;

	  path = gtk_tree_path_new_from_indices (item->index, -1);
	  egg_icon_list_item_activated (icon_list, path);
	  gtk_tree_path_free (path);
	}

      icon_list->priv->last_single_clicked = NULL;
    }
  
  if (dirty)
    g_signal_emit (icon_list, icon_list_signals[SELECTION_CHANGED], 0);

  return TRUE;
}

static gboolean
egg_icon_list_button_release (GtkWidget      *widget,
			      GdkEventButton *event)
{
  EggIconList *icon_list;

  icon_list = EGG_ICON_LIST (widget);

  if (icon_list->priv->pressed_button == event->button)
    icon_list->priv->pressed_button = -1;

  egg_icon_list_stop_rubberbanding (icon_list);

  if (icon_list->priv->scroll_timeout_id != 0)
    {
      g_source_remove (icon_list->priv->scroll_timeout_id);
      icon_list->priv->scroll_timeout_id = 0;
    }

  return TRUE;
}


static void
rubberbanding (gpointer data)
{
  EggIconList *icon_list;
  gint x, y;
  GdkRectangle old_area;
  GdkRectangle new_area;
  GdkRectangle common;
  GdkRegion *invalid_region;
  
  icon_list = EGG_ICON_LIST (data);

  gdk_window_get_pointer (icon_list->priv->bin_window, &x, &y, NULL);

  x = MAX (x, 0);
  y = MAX (y, 0);

  old_area.x = MIN (icon_list->priv->rubberband_x1,
		    icon_list->priv->rubberband_x2);
  old_area.y = MIN (icon_list->priv->rubberband_y1,
		    icon_list->priv->rubberband_y2);
  old_area.width = ABS (icon_list->priv->rubberband_x2 -
			icon_list->priv->rubberband_x1) + 1;
  old_area.height = ABS (icon_list->priv->rubberband_y2 -
			 icon_list->priv->rubberband_y1) + 1;
  
  new_area.x = MIN (icon_list->priv->rubberband_x1, x);
  new_area.y = MIN (icon_list->priv->rubberband_y1, y);
  new_area.width = ABS (x - icon_list->priv->rubberband_x1) + 1;
  new_area.height = ABS (y - icon_list->priv->rubberband_y1) + 1;

  invalid_region = gdk_region_rectangle (&old_area);
  gdk_region_union_with_rect (invalid_region, &new_area);

  gdk_rectangle_intersect (&old_area, &new_area, &common);
  if (common.width > 2 && common.height > 2)
    {
      GdkRegion *common_region;

      /* make sure the border is invalidated */
      common.x += 1;
      common.y += 1;
      common.width -= 2;
      common.height -= 2;
      
      common_region = gdk_region_rectangle (&common);

      gdk_region_subtract (invalid_region, common_region);
      gdk_region_destroy (common_region);
    }
  
  gdk_window_invalidate_region (icon_list->priv->bin_window, invalid_region, TRUE);
    
  gdk_region_destroy (invalid_region);

  icon_list->priv->rubberband_x2 = x;
  icon_list->priv->rubberband_y2 = y;  

  egg_icon_list_update_rubberband_selection (icon_list);
}

static void
egg_icon_list_start_rubberbanding (EggIconList  *icon_list,
				   gint          x,
				   gint          y)
{
  GList *items;

  g_assert (!icon_list->priv->rubberbanding);

  for (items = icon_list->priv->items; items; items = items->next)
    {
      EggIconListItem *item = items->data;

      item->selected_before_rubberbanding = item->selected;
    }
  
  icon_list->priv->rubberband_x1 = x;
  icon_list->priv->rubberband_y1 = y;
  icon_list->priv->rubberband_x2 = x;
  icon_list->priv->rubberband_y2 = y;

  icon_list->priv->rubberbanding = TRUE;

  gtk_grab_add (GTK_WIDGET (icon_list));
}

static void
egg_icon_list_stop_rubberbanding (EggIconList *icon_list)
{
  if (!icon_list->priv->rubberbanding)
    return;

  icon_list->priv->rubberbanding = FALSE;

  gtk_grab_remove (GTK_WIDGET (icon_list));
  
  gtk_widget_queue_draw (GTK_WIDGET (icon_list));
}

static void
egg_icon_list_update_rubberband_selection (EggIconList *icon_list)
{
  GList *items;
  gint x, y, width, height;
  gboolean dirty = FALSE;
  
  x = MIN (icon_list->priv->rubberband_x1,
	   icon_list->priv->rubberband_x2);
  y = MIN (icon_list->priv->rubberband_y1,
	   icon_list->priv->rubberband_y2);
  width = ABS (icon_list->priv->rubberband_x1 - 
	       icon_list->priv->rubberband_x2);
  height = ABS (icon_list->priv->rubberband_y1 - 
		icon_list->priv->rubberband_y2);
  
  for (items = icon_list->priv->items; items; items = items->next)
    {
      EggIconListItem *item = items->data;
      gboolean is_in;
      gboolean selected;
      
      is_in = egg_icon_list_item_hit_test (item, x, y, width, height);

      selected = is_in ^ item->selected_before_rubberbanding;

      if (item->selected != selected)
	{
	  item->selected = selected;
	  dirty = TRUE;
	  egg_icon_list_queue_draw_item (icon_list, item);
	}
    }

  if (dirty)
    g_signal_emit (icon_list, icon_list_signals[SELECTION_CHANGED], 0);
}

static gboolean
egg_icon_list_item_hit_test (EggIconListItem  *item,
			     gint              x,
			     gint              y,
			     gint              width,
			     gint              height)
{
  /* First try the pixbuf */
  if (MIN (x + width, item->pixbuf_x + item->pixbuf_width) - MAX (x, item->pixbuf_x) > 0 &&
      MIN (y + height, item->pixbuf_y + item->pixbuf_height) - MAX (y, item->pixbuf_y) > 0)
    return TRUE;

  /* Then try the text */
  if (MIN (x + width, item->layout_x + item->layout_width) - MAX (x, item->layout_x) > 0 &&
      MIN (y + height, item->layout_y + item->layout_height) - MAX (y, item->layout_y) > 0)
    return TRUE;
  
  return FALSE;
}

static gboolean
egg_icon_list_maybe_begin_dragging_items (EggIconList     *icon_list,
					  GdkEventMotion  *event)
{
  gboolean retval = FALSE;
  gint button;
  if (icon_list->priv->pressed_button < 0)
    return retval;

  if (!gtk_drag_check_threshold (GTK_WIDGET (icon_list),
				 icon_list->priv->press_start_x,
				 icon_list->priv->press_start_y,
				 event->x, event->y))
    return retval;

  button = icon_list->priv->pressed_button;
  icon_list->priv->pressed_button = -1;
  
  {
    static GtkTargetEntry row_targets[] = {
      { "EGG_ICON_LIST_ITEMS", GTK_TARGET_SAME_APP, 0 }
    };
    GtkTargetList *target_list;
    GdkDragContext *context;
    EggIconListItem *item;
    
    retval = TRUE;
    
    target_list = gtk_target_list_new (row_targets, G_N_ELEMENTS (row_targets));

    context = gtk_drag_begin (GTK_WIDGET (icon_list),
			      target_list, GDK_ACTION_MOVE,
			      button,
			      (GdkEvent *)event);

    item = egg_icon_list_get_item_at_pos (icon_list,
					  icon_list->priv->press_start_x,
					  icon_list->priv->press_start_y);
    g_assert (item != NULL);
    gtk_drag_set_icon_pixbuf (context, egg_icon_list_get_item_icon (icon_list, item),
			      event->x - item->x,
			      event->y - item->y);
  }
  
  return retval;
}


static gboolean
egg_icon_list_unselect_all_internal (EggIconList  *icon_list,
				     gboolean      emit)
{
  gboolean dirty = FALSE;
  GList *items;
  
  for (items = icon_list->priv->items; items; items = items->next)
    {
      EggIconListItem *item = items->data;

      if (item->selected)
	{
	  item->selected = FALSE;
	  dirty = TRUE;
	  egg_icon_list_queue_draw_item (icon_list, item);
	}
    }

  if (emit && dirty)
    g_signal_emit (icon_list, icon_list_signals[SELECTION_CHANGED], 0);

  return dirty;
}


/* EggIconList signals */
static void
egg_icon_list_set_adjustments (EggIconList   *icon_list,
			       GtkAdjustment *hadj,
			       GtkAdjustment *vadj)
{
  gboolean need_adjust = FALSE;

  if (hadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
  else
    hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  if (vadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
  else
    vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

  if (icon_list->priv->hadjustment && (icon_list->priv->hadjustment != hadj))
    {
      g_signal_handlers_disconnect_matched (icon_list->priv->hadjustment, G_SIGNAL_MATCH_DATA,
					   0, 0, NULL, NULL, icon_list);
      g_object_unref (icon_list->priv->hadjustment);
    }

  if (icon_list->priv->vadjustment && (icon_list->priv->vadjustment != vadj))
    {
      g_signal_handlers_disconnect_matched (icon_list->priv->vadjustment, G_SIGNAL_MATCH_DATA,
					    0, 0, NULL, NULL, icon_list);
      g_object_unref (icon_list->priv->vadjustment);
    }

  if (icon_list->priv->hadjustment != hadj)
    {
      icon_list->priv->hadjustment = hadj;
      g_object_ref (icon_list->priv->hadjustment);
      gtk_object_sink (GTK_OBJECT (icon_list->priv->hadjustment));

      g_signal_connect (icon_list->priv->hadjustment, "value_changed",
			G_CALLBACK (egg_icon_list_adjustment_changed),
			icon_list);
      need_adjust = TRUE;
    }

  if (icon_list->priv->vadjustment != vadj)
    {
      icon_list->priv->vadjustment = vadj;
      g_object_ref (icon_list->priv->vadjustment);
      gtk_object_sink (GTK_OBJECT (icon_list->priv->vadjustment));

      g_signal_connect (icon_list->priv->vadjustment, "value_changed",
			G_CALLBACK (egg_icon_list_adjustment_changed),
			icon_list);
      need_adjust = TRUE;
    }

  if (need_adjust)
    egg_icon_list_adjustment_changed (NULL, icon_list);
}

static void
egg_icon_list_real_select_all (EggIconList *icon_list)
{
  if (icon_list->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    return;

  egg_icon_list_select_all (icon_list);
}

static void
egg_icon_list_real_unselect_all (EggIconList *icon_list)
{
  if (icon_list->priv->selection_mode == GTK_SELECTION_BROWSE)
    return;

  egg_icon_list_unselect_all (icon_list);
}

static void
egg_icon_list_real_select_cursor_item (EggIconList *icon_list)
{
  egg_icon_list_unselect_all (icon_list);
  
  if (icon_list->priv->cursor_item != NULL)
    egg_icon_list_select_item (icon_list, icon_list->priv->cursor_item);
}

static void
egg_icon_list_real_toggle_cursor_item (EggIconList *icon_list)
{
  if (!icon_list->priv->cursor_item)
    return;

  switch (icon_list->priv->selection_mode)
    {
    case GTK_SELECTION_NONE:
      break;
    case GTK_SELECTION_BROWSE:
      egg_icon_list_select_item (icon_list, icon_list->priv->cursor_item);
      break;
    case GTK_SELECTION_SINGLE:
      if (icon_list->priv->cursor_item->selected)
	egg_icon_list_unselect_item (icon_list, icon_list->priv->cursor_item);
      else
	egg_icon_list_select_item (icon_list, icon_list->priv->cursor_item);
      break;
    case GTK_SELECTION_MULTIPLE:
      icon_list->priv->cursor_item->selected = !icon_list->priv->cursor_item->selected;
      g_signal_emit (icon_list, icon_list_signals[SELECTION_CHANGED], 0); 
      
      egg_icon_list_queue_draw_item (icon_list, icon_list->priv->cursor_item);
      break;
    }
}

/* Internal functions */
static void
egg_icon_list_adjustment_changed (GtkAdjustment *adjustment,
				  EggIconList   *icon_list)
{
  if (GTK_WIDGET_REALIZED (icon_list))
    {
      gdk_window_move (icon_list->priv->bin_window,
		       - icon_list->priv->hadjustment->value,
		       - icon_list->priv->vadjustment->value);

      if (icon_list->priv->rubberbanding)
	rubberbanding (GTK_WIDGET (icon_list));

      gdk_window_process_updates (icon_list->priv->bin_window, TRUE);
    }
}

static GList *
egg_icon_list_layout_single_row (EggIconList *icon_list, GList *first_item, gint *y, gint *maximum_width, gint row)
{
  gint x, current_width, max_height, max_pixbuf_height;
  GList *items, *last_item;
  gint icon_padding;
  gint left_margin, right_margin;
  gint maximum_layout_width;
  gint col;
  gboolean rtl = gtk_widget_get_direction (GTK_WIDGET (icon_list)) == GTK_TEXT_DIR_RTL;

  x = 0;
  col = 0;
  max_height = 0;
  max_pixbuf_height = 0;
  items = first_item;
  current_width = 0;

  gtk_widget_style_get (GTK_WIDGET (icon_list),
			"icon_padding", &icon_padding,
			"left_margin", &left_margin,
			"right_margin", &right_margin,
			NULL);
  
  x += left_margin;
  current_width += left_margin + right_margin;
  items = first_item;

  while (items)
    {
      EggIconListItem *item = items->data;

      egg_icon_list_calculate_item_size (icon_list, item);

      current_width += MAX (item->width, MINIMUM_ICON_ITEM_WIDTH);

      /* Don't add padding to the first or last icon */
      
      if (current_width > GTK_WIDGET (icon_list)->allocation.width &&
	  items != first_item)
	break;

      maximum_layout_width = MAX (item->pixbuf_width, MINIMUM_ICON_ITEM_WIDTH);

      item->y = *y;
      item->x = rtl ? GTK_WIDGET (icon_list)->allocation.width - item->width - x : x;
      if (item->width < MINIMUM_ICON_ITEM_WIDTH) {
	if (rtl)
	  item->x -= (MINIMUM_ICON_ITEM_WIDTH - item->width) / 2;
	else
	  item->x += (MINIMUM_ICON_ITEM_WIDTH - item->width) / 2;
	x += (MINIMUM_ICON_ITEM_WIDTH - item->width);
      }

      item->pixbuf_x = item->x + (item->width - item->pixbuf_width) / 2;
      item->layout_x = item->x + (item->width - item->layout_width) / 2;

      x += item->width;

      max_height = MAX (max_height, item->height);
      max_pixbuf_height = MAX (max_pixbuf_height, item->pixbuf_height);
      
      if (current_width > *maximum_width)
	*maximum_width = current_width;

      item->row = row;
      item->col = col;

      col++;
      items = items->next;
    }

  last_item = items;

  *y += max_height + icon_padding;

  /* Now go through the row again and align the icons */
  for (items = first_item; items != last_item; items = items->next)
    {
      EggIconListItem *item = items->data;

      item->pixbuf_y = item->y + (max_pixbuf_height - item->pixbuf_height);
      item->layout_y = item->pixbuf_y + item->pixbuf_height + ICON_TEXT_PADDING;
      /* Update the bounding box */
      item->y = item->pixbuf_y;

      /* We may want to readjust the new y coordinate. */
      if (item->y + item->height > *y)
	*y = item->y + item->height;

      if (rtl)
	item->col = col - 1 - item->col;
    }
  
  return last_item;
}

static void
egg_icon_list_set_adjustment_upper (GtkAdjustment *adj,
				    gdouble        upper)
{
  if (upper != adj->upper)
    {
      gdouble min = MAX (0.0, upper - adj->page_size);
      gboolean value_changed = FALSE;
      
      adj->upper = upper;

      if (adj->value > min)
	{
	  adj->value = min;
	  value_changed = TRUE;
	}
      
      gtk_adjustment_changed (adj);
      
      if (value_changed)
	gtk_adjustment_value_changed (adj);
    }
}

static void
egg_icon_list_layout (EggIconList *icon_list)
{
  gint y = 0, maximum_width = 0;
  GList *icons;
  GtkWidget *widget;
  gint top_margin, bottom_margin;
  gint row;
  
  widget = GTK_WIDGET (icon_list);
  icons = icon_list->priv->items;

  gtk_widget_style_get (widget,
			"top_margin", &top_margin,
			"bottom_margin", &bottom_margin,
			NULL);
  y += top_margin;
  row = 0;

  do
    {
      icons = egg_icon_list_layout_single_row (icon_list, icons, &y, &maximum_width, row);
      row++;
    }
  while (icons != NULL);

  if (maximum_width != icon_list->priv->width)
    {
      icon_list->priv->width = maximum_width;
    }
  y += bottom_margin;
  
  if (y != icon_list->priv->height)
    {
      icon_list->priv->height = y;
    }

  egg_icon_list_set_adjustment_upper (icon_list->priv->hadjustment, icon_list->priv->width);
  egg_icon_list_set_adjustment_upper (icon_list->priv->vadjustment, icon_list->priv->height);

  if (GTK_WIDGET_REALIZED (icon_list))
    {
      gdk_window_resize (icon_list->priv->bin_window,
			 MAX (icon_list->priv->width, widget->allocation.width),
			 MAX (icon_list->priv->height, widget->allocation.height));
    }

  if (icon_list->priv->layout_idle_id != 0)
    {
      g_source_remove (icon_list->priv->layout_idle_id);
      icon_list->priv->layout_idle_id = 0;
    }

  gtk_widget_queue_draw (GTK_WIDGET (icon_list));
}

/* Updates the pango layout and calculates the size */
static void
egg_icon_list_calculate_item_size (EggIconList *icon_list,
				   EggIconListItem *item)
{
  int layout_width, layout_height;
  int maximum_layout_width;
  GdkPixbuf *pixbuf;
  
  if (item->width != -1 && item->width != -1) 
    return;

  if (icon_list->priv->pixbuf_column != -1)
    {
      pixbuf = egg_icon_list_get_item_icon (icon_list, item);
      item->pixbuf_width = gdk_pixbuf_get_width (pixbuf);
      item->pixbuf_height = gdk_pixbuf_get_height (pixbuf);
      g_object_unref (pixbuf);
    }
  else
    {
      item->pixbuf_width = 0;
      item->pixbuf_height = 0;
    }
  
  maximum_layout_width = MAX (item->pixbuf_width, MINIMUM_ICON_ITEM_WIDTH);

  if (icon_list->priv->markup_column != 1 ||
      icon_list->priv->text_column != -1)
    {
      egg_icon_list_update_item_text (icon_list, item);

      pango_layout_set_alignment (icon_list->priv->layout, PANGO_ALIGN_CENTER);
      pango_layout_set_width (icon_list->priv->layout, maximum_layout_width * PANGO_SCALE);
      
      pango_layout_get_pixel_size (icon_list->priv->layout, &layout_width, &layout_height);
      
      item->width = MAX ((layout_width + 2 * ICON_TEXT_PADDING), item->pixbuf_width);
      item->height = layout_height + 2 * ICON_TEXT_PADDING + item->pixbuf_height;
      item->layout_width = layout_width;
      item->layout_height = layout_height;
    }
  else
    {
      item->layout_width = 0;
      item->layout_height = 0;
    }
}

static void
egg_icon_list_invalidate_sizes (EggIconList *icon_list)
{
  g_list_foreach (icon_list->priv->items,
		  (GFunc)egg_icon_list_item_invalidate_size, NULL);
}

static void
egg_icon_list_item_invalidate_size (EggIconListItem *item)
{
  item->width = -1;
  item->height = -1;
}

static GdkPixbuf *
create_colorized_pixbuf (GdkPixbuf *src, GdkColor *new_color)
{
	gint i, j;
	gint width, height, has_alpha, src_row_stride, dst_row_stride;
	gint red_value, green_value, blue_value;
	guchar *target_pixels;
	guchar *original_pixels;
	guchar *pixsrc;
	guchar *pixdest;
	GdkPixbuf *dest;

	red_value = new_color->red / 255.0;
	green_value = new_color->green / 255.0;
	blue_value = new_color->blue / 255.0;

	dest = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
			       gdk_pixbuf_get_has_alpha (src),
			       gdk_pixbuf_get_bits_per_sample (src),
			       gdk_pixbuf_get_width (src),
			       gdk_pixbuf_get_height (src));
	
	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	src_row_stride = gdk_pixbuf_get_rowstride (src);
	dst_row_stride = gdk_pixbuf_get_rowstride (dest);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i*dst_row_stride;
		pixsrc = original_pixels + i*src_row_stride;
		for (j = 0; j < width; j++) {		
			*pixdest++ = (*pixsrc++ * red_value) >> 8;
			*pixdest++ = (*pixsrc++ * green_value) >> 8;
			*pixdest++ = (*pixsrc++ * blue_value) >> 8;
			if (has_alpha) {
				*pixdest++ = *pixsrc++;
			}
		}
	}
	return dest;
}

static void
egg_icon_list_paint_item (EggIconList     *icon_list,
			  EggIconListItem *item,
			  GdkRectangle    *area)
{
  GdkPixbuf *pixbuf, *tmp;
  GtkStateType state;
  
  if (!VALID_MODEL_AND_COLUMNS (icon_list))
    return;
  
  if (GTK_WIDGET_HAS_FOCUS (icon_list))
    state = GTK_STATE_SELECTED;
  else
    state = GTK_STATE_ACTIVE;

  if (icon_list->priv->pixbuf_column != -1)
    {
      tmp = egg_icon_list_get_item_icon (icon_list, item);
      if (item->selected)
	{
	  pixbuf = create_colorized_pixbuf (tmp,
					    &GTK_WIDGET (icon_list)->style->base[state]);
	  g_object_unref (tmp);
	}
      else
	pixbuf = tmp;
      
      gdk_draw_pixbuf (icon_list->priv->bin_window, NULL, pixbuf,
		       0, 0,
		       item->pixbuf_x, item->pixbuf_y,
		       item->pixbuf_width, item->pixbuf_height,
		       GDK_RGB_DITHER_NORMAL,
		       item->pixbuf_width, item->pixbuf_height);
      g_object_unref (pixbuf);
    }

  if (icon_list->priv->text_column != -1)
    {
      if (item->selected)
	{
	  gdk_draw_rectangle (icon_list->priv->bin_window,
			      GTK_WIDGET (icon_list)->style->base_gc[state],
			      TRUE,
			      item->layout_x - ICON_TEXT_PADDING,
			      item->layout_y - ICON_TEXT_PADDING,
			      item->layout_width + 2 * ICON_TEXT_PADDING,
			      item->layout_height + 2 * ICON_TEXT_PADDING);
	}

      egg_icon_list_update_item_text (icon_list, item);
      gtk_paint_layout (GTK_WIDGET (icon_list)->style,
			icon_list->priv->bin_window,
			item->selected ? state : GTK_STATE_NORMAL,
			TRUE, area, GTK_WIDGET (icon_list), "icon_list",
			item->layout_x - ((item->width - item->layout_width) / 2) - (MAX (item->pixbuf_width, MINIMUM_ICON_ITEM_WIDTH) - item->width) / 2,
			item->layout_y,
			icon_list->priv->layout);

      if (GTK_WIDGET_HAS_FOCUS (icon_list) &&
	  item == icon_list->priv->cursor_item)
	gtk_paint_focus (GTK_WIDGET (icon_list)->style,
			 icon_list->priv->bin_window,
			 item->selected ? GTK_STATE_SELECTED : GTK_STATE_NORMAL,
			 area,
			 GTK_WIDGET (icon_list),
			 "icon_list",
			 item->layout_x - ICON_TEXT_PADDING,
			 item->layout_y - ICON_TEXT_PADDING,
			 item->layout_width + 2 * ICON_TEXT_PADDING,
			 item->layout_height + 2 * ICON_TEXT_PADDING);
    }
}

static guint32
egg_gdk_color_to_rgb (const GdkColor *color)
{
  guint32 result;
  result = (0xff0000 | (color->red & 0xff00));
  result <<= 8;
  result |= ((color->green & 0xff00) | (color->blue >> 8));
  return result;
}

static void
egg_icon_list_paint_rubberband (EggIconList     *icon_list,
				GdkRectangle    *area)
{
  GdkRectangle rect;
  GdkPixbuf *pixbuf;
  GdkGC *gc;
  GdkRectangle rubber_rect;
  GdkColor *fill_color_gdk;
  guint fill_color;
  guchar fill_color_alpha;

  rubber_rect.x = MIN (icon_list->priv->rubberband_x1, icon_list->priv->rubberband_x2);
  rubber_rect.y = MIN (icon_list->priv->rubberband_y1, icon_list->priv->rubberband_y2);
  rubber_rect.width = ABS (icon_list->priv->rubberband_x1 - icon_list->priv->rubberband_x2) + 1;
  rubber_rect.height = ABS (icon_list->priv->rubberband_y1 - icon_list->priv->rubberband_y2) + 1;

  if (!gdk_rectangle_intersect (&rubber_rect, area, &rect))
    return;

  gtk_widget_style_get (GTK_WIDGET (icon_list),
                        "selection_box_color", &fill_color_gdk,
                        "selection_box_alpha", &fill_color_alpha,
                        NULL);

  if (!fill_color_gdk) {
    fill_color_gdk = gdk_color_copy (&GTK_WIDGET (icon_list)->style->base[GTK_STATE_SELECTED]);
  }

  fill_color = egg_gdk_color_to_rgb (fill_color_gdk) << 8 | fill_color_alpha;

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, rect.width, rect.height);
  gdk_pixbuf_fill (pixbuf, fill_color);

  gdk_draw_pixbuf (icon_list->priv->bin_window, NULL, pixbuf,
		   0, 0, 
		   rect.x,rect.y,
		   rect.width, rect.height,
		   GDK_RGB_DITHER_NONE,
		   0, 0);
  g_object_unref (pixbuf);
  gc = gdk_gc_new (icon_list->priv->bin_window);
  gdk_gc_set_rgb_fg_color (gc, fill_color_gdk);
  gdk_gc_set_clip_rectangle (gc, &rect);
  gdk_draw_rectangle (icon_list->priv->bin_window,
		      gc, FALSE,
		      rubber_rect.x, rubber_rect.y,
		      rubber_rect.width - 1, rubber_rect.height - 1);
  gdk_color_free (fill_color_gdk);
  g_object_unref (gc);
}

static void
egg_icon_list_queue_draw_item (EggIconList     *icon_list,
			       EggIconListItem *item)
{
  GdkRectangle rect;

  rect.x = item->x;
  rect.y = item->y;
  rect.width = item->width;
  rect.height = item->height;

  if (icon_list->priv->bin_window)
    gdk_window_invalidate_rect (icon_list->priv->bin_window, &rect, TRUE);
}

static gboolean
layout_callback (gpointer user_data)
{
  EggIconList *icon_list;

  icon_list = EGG_ICON_LIST (user_data);
  
  icon_list->priv->layout_idle_id = 0;

  egg_icon_list_layout (icon_list);
  
  return FALSE;
}

static void
egg_icon_list_queue_layout (EggIconList *icon_list)
{
  if (icon_list->priv->layout_idle_id != 0)
    return;

  icon_list->priv->layout_idle_id = g_idle_add (layout_callback, icon_list);
}

static void
egg_icon_list_set_cursor_item (EggIconList     *icon_list,
			       EggIconListItem *item)
{
  if (icon_list->priv->cursor_item == item)
    return;

  if (icon_list->priv->cursor_item != NULL)
    egg_icon_list_queue_draw_item (icon_list, icon_list->priv->cursor_item);
  
  icon_list->priv->cursor_item = item;
  egg_icon_list_queue_draw_item (icon_list, item);
}


static EggIconListItem *
egg_icon_list_item_new (void)
{
  EggIconListItem *item;

  item = g_new0 (EggIconListItem, 1);

  item->ref_count = 1;
  item->width = -1;
  item->height = -1;
  
  return item;
}

static void
egg_icon_list_item_ref (EggIconListItem *item)
{
  g_return_if_fail (item != NULL);

  item->ref_count += 1;
}

static void
egg_icon_list_item_unref (EggIconListItem *item)
{
  g_return_if_fail (item != NULL);

  item->ref_count -= 1;

  if (item->ref_count == 0)
    {
      g_free (item);
    }
  
}

static void
egg_icon_list_update_item_text (EggIconList     *icon_list,
				EggIconListItem *item)
{
  gboolean iters_persist;
  GtkTreeIter iter;
  GtkTreePath *path;
  gchar *text;
  
  iters_persist = gtk_tree_model_get_flags (icon_list->priv->model) & GTK_TREE_MODEL_ITERS_PERSIST;
  
  if (!iters_persist)
    {
      path = gtk_tree_path_new_from_indices (item->index, -1);
      gtk_tree_model_get_iter (icon_list->priv->model, &iter, path);
      gtk_tree_path_free (path);
    }
  else
    iter = item->iter;

  if (icon_list->priv->markup_column != -1)
    {
      gtk_tree_model_get (icon_list->priv->model, &iter,
			  icon_list->priv->markup_column, &text,
			  -1);
      pango_layout_set_markup (icon_list->priv->layout, text, -1);
    }
  else
    {
      gtk_tree_model_get (icon_list->priv->model, &iter,
			  icon_list->priv->text_column, &text,
			  -1);
      pango_layout_set_text (icon_list->priv->layout, text, -1);
    }

  g_free (text);        
}

static GdkPixbuf *
egg_icon_list_get_item_icon (EggIconList      *icon_list,
			     EggIconListItem  *item)
{
  gboolean iters_persist;
  GtkTreeIter iter;
  GtkTreePath *path;
  GdkPixbuf *pixbuf;
  
  g_return_val_if_fail (item != NULL, NULL);

  iters_persist = gtk_tree_model_get_flags (icon_list->priv->model) & GTK_TREE_MODEL_ITERS_PERSIST;
  
  if (!iters_persist)
    {
      path = gtk_tree_path_new_from_indices (item->index, -1);
      gtk_tree_model_get_iter (icon_list->priv->model, &iter, path);
      gtk_tree_path_free (path);
    }
  else
    iter = item->iter;
  
  gtk_tree_model_get (icon_list->priv->model, &iter,
		      icon_list->priv->pixbuf_column, &pixbuf,
		      -1);

  return pixbuf;
}


static EggIconListItem *
egg_icon_list_get_item_at_pos (EggIconList *icon_list,
			       gint         x,
			       gint         y)
{
  GList *items;
  
  for (items = icon_list->priv->items; items; items = items->next)
    {
      EggIconListItem *item = items->data;
      
      if (x > item->x && x < item->x + item->width &&
	  y > item->y && y < item->y + item->height)
	{
	  gint layout_x = item->x + (item->width - item->layout_width) / 2;
	  /* Check if the mouse is inside the icon or the label */
	  if ((x > item->pixbuf_x && x < item->pixbuf_x + item->pixbuf_width &&
	       y > item->pixbuf_y && y < item->pixbuf_y + item->pixbuf_height) ||
	      (x > layout_x - ICON_TEXT_PADDING &&
	       x < layout_x + item->layout_width + ICON_TEXT_PADDING * 2 &&
	       y > item->layout_y - ICON_TEXT_PADDING
	       && y < item->layout_y + item->layout_height + ICON_TEXT_PADDING * 2))
	    return item;
	}
    }

  return NULL;
}




static void
egg_icon_list_select_item (EggIconList      *icon_list,
			   EggIconListItem  *item)
{
  g_return_if_fail (EGG_IS_ICON_LIST (icon_list));
  g_return_if_fail (item != NULL);

  if (item->selected)
    return;
  
  if (icon_list->priv->selection_mode == GTK_SELECTION_NONE)
    return;
  else if (icon_list->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    egg_icon_list_unselect_all_internal (icon_list, FALSE);

  item->selected = TRUE;

  g_signal_emit (icon_list, icon_list_signals[SELECTION_CHANGED], 0);
  
  egg_icon_list_queue_draw_item (icon_list, item);
}


static void
egg_icon_list_unselect_item (EggIconList      *icon_list,
			     EggIconListItem  *item)
{
  g_return_if_fail (EGG_IS_ICON_LIST (icon_list));
  g_return_if_fail (item != NULL);

  if (!item->selected)
    return;
  
  if (icon_list->priv->selection_mode == GTK_SELECTION_NONE ||
      icon_list->priv->selection_mode == GTK_SELECTION_BROWSE)
    return;
  
  item->selected = FALSE;

  g_signal_emit (icon_list, icon_list_signals[SELECTION_CHANGED], 0);

  egg_icon_list_queue_draw_item (icon_list, item);
}

static void
egg_icon_list_row_changed (GtkTreeModel *model,
			   GtkTreePath  *path,
			   GtkTreeIter  *iter,
			   gpointer      data)
{
  EggIconListItem *item;
  gint index;
  EggIconList *icon_list;

  icon_list = EGG_ICON_LIST (data);
  
  index = gtk_tree_path_get_indices(path)[0];
  item = g_list_nth (icon_list->priv->items, index)->data;

  egg_icon_list_item_invalidate_size (item);
  egg_icon_list_queue_layout (icon_list);
}

static void
egg_icon_list_row_inserted (GtkTreeModel *model,
			    GtkTreePath  *path,
			    GtkTreeIter  *iter,
			    gpointer      data)
{
  gint length, index;
  EggIconListItem *item;
  gboolean iters_persist;
  EggIconList *icon_list;

  icon_list = EGG_ICON_LIST (data);
  iters_persist = gtk_tree_model_get_flags (icon_list->priv->model) & GTK_TREE_MODEL_ITERS_PERSIST;
  
  length = gtk_tree_model_iter_n_children (model, NULL);
  index = gtk_tree_path_get_indices(path)[0];

  item = egg_icon_list_item_new ();

  if (iters_persist)
    item->iter = *iter;

  item->index = index;

  /* FIXME: We can be more efficient here,
     we can store a tail pointer and use that when
     appending (which is a rather common operation)
  */
  icon_list->priv->items = g_list_insert (icon_list->priv->items,
					 item, index);
  
}

static void
egg_icon_list_row_deleted (GtkTreeModel *model,
			   GtkTreePath  *path,
			   gpointer      data)
{
  gint index;
  EggIconList *icon_list;
  EggIconListItem *item;
  GList *list;
  
  icon_list = EGG_ICON_LIST (data);

  index = gtk_tree_path_get_indices(path)[0];

  list = g_list_nth (icon_list->priv->items, index);
  item = list->data;
  item->index = -1;
  egg_icon_list_item_unref (item);  
  icon_list->priv->items = g_list_delete_link (icon_list->priv->items, list);

}

static void
egg_icon_list_rows_reordered (GtkTreeModel *model,
			      GtkTreePath  *parent,
			      GtkTreeIter  *iter,
			      gint         *new_order,
			      gpointer      data)
{
  int i;
  int length;
  EggIconList *icon_list;
  GList *items = NULL, *list;
  gint *inverted_order;
  EggIconListItem **item_array;
  
  icon_list = EGG_ICON_LIST (data);

  length = gtk_tree_model_iter_n_children (model, NULL);
  inverted_order = g_new (gint, length);

  /* Invert the array */
  for (i = 0; i < length; i++)
    inverted_order[new_order[i]] = i;

  item_array = g_new (EggIconListItem *, length);
  for (i = 0, list = icon_list->priv->items; list != NULL; list = list->next, i++)
    item_array[inverted_order[i]] = list->data;

  g_free (inverted_order);
  for (i = 0; i < length; i++)
    items = g_list_prepend (items, item_array[i]);
  
  g_free (item_array);
  g_list_free (icon_list->priv->items);
  icon_list->priv->items = g_list_reverse (items);
}

static void
egg_icon_list_build_items (EggIconList *icon_list)
{
  GtkTreeIter iter;
  int i;
  gboolean iters_persist;
  GList *items = NULL;

  iters_persist = gtk_tree_model_get_flags (icon_list->priv->model) & GTK_TREE_MODEL_ITERS_PERSIST;
  
  if (!gtk_tree_model_get_iter_first (icon_list->priv->model,
				      &iter))
    return;

  i = 0;
  
  do
    {
      EggIconListItem *item = egg_icon_list_item_new ();

      if (iters_persist)
	item->iter = iter;

      item->index = i;
      
      i++;

      items = g_list_prepend (items, item);
      
    } while (gtk_tree_model_iter_next (icon_list->priv->model, &iter));

  icon_list->priv->items = g_list_reverse (items);
}

static void
egg_icon_list_add_move_binding (GtkBindingSet  *binding_set,
				guint           keyval,
				guint           modmask,
				GtkMovementStep step,
				gint            count)
{
  
  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
                                "move_cursor", 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_SHIFT_MASK,
                                "move_cursor", 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);

  if ((modmask & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
   return;

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "move_cursor", 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_CONTROL_MASK,
                                "move_cursor", 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);
}

static gboolean
egg_icon_list_real_move_cursor (EggIconList     *icon_list,
				GtkMovementStep  step,
				gint             count)
{
  GdkModifierType state;

  g_return_val_if_fail (EGG_ICON_LIST (icon_list), FALSE);
  g_return_val_if_fail (step == GTK_MOVEMENT_LOGICAL_POSITIONS ||
			step == GTK_MOVEMENT_VISUAL_POSITIONS ||
			step == GTK_MOVEMENT_DISPLAY_LINES ||
			step == GTK_MOVEMENT_PAGES ||
			step == GTK_MOVEMENT_BUFFER_ENDS, FALSE);

  if (!GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (icon_list)))
    return FALSE;

  gtk_widget_grab_focus (GTK_WIDGET (icon_list));

  if (gtk_get_current_event_state (&state))
    {
      if ((state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
        icon_list->priv->ctrl_pressed = TRUE;
      if ((state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK)
        icon_list->priv->shift_pressed = TRUE;
    }
  /* else we assume not pressed */

  switch (step)
    {
    case GTK_MOVEMENT_LOGICAL_POSITIONS:
    case GTK_MOVEMENT_VISUAL_POSITIONS:
      egg_icon_list_move_cursor_left_right (icon_list, count);
      break;
    case GTK_MOVEMENT_DISPLAY_LINES:
      egg_icon_list_move_cursor_up_down (icon_list, count);
      break;
    case GTK_MOVEMENT_PAGES:
      egg_icon_list_move_cursor_page_up_down (icon_list, count);
      break;
    case GTK_MOVEMENT_BUFFER_ENDS:
      egg_icon_list_move_cursor_start_end (icon_list, count);
      break;
    default:
      g_assert_not_reached ();
    }

  icon_list->priv->ctrl_pressed = FALSE;
  icon_list->priv->shift_pressed = FALSE;

  return TRUE;
}

static EggIconListItem *
find_item (EggIconList     *icon_list,
	   EggIconListItem *current,
	   gint             row_ofs,
	   gint             col_ofs)
{
  gint row, col;
  GList *items;
  EggIconListItem *item;

  /* FIXME: this could be more efficient 
   */
  row = current->row + row_ofs;
  col = current->col + col_ofs;

  for (items = icon_list->priv->items; items; items = items->next)
    {
      item = items->data;
      if (item->row == row && item->col == col)
	return item;
    }
  
  return NULL;
}


static EggIconListItem *
find_item_page_up_down (EggIconList     *icon_list,
			EggIconListItem *current,
			gint             count)
{
  GList *item, *next;
  gint y, col;
  
  col = current->col;
  y = current->y + count * icon_list->priv->vadjustment->page_size;

  item = g_list_find (icon_list->priv->items, current);
  if (count > 0)
    {
      while (item)
	{
	  for (next = item->next; next; next = next->next)
	    {
	      if (((EggIconListItem *)next->data)->col == col)
		break;
	    }
	  if (!next || ((EggIconListItem *)next->data)->y > y)
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
	      if (((EggIconListItem *)next->data)->col == col)
		break;
	    }
	  if (!next || ((EggIconListItem *)next->data)->y < y)
	    break;

	  item = next;
	}
    }

  if (item)
    return item->data;

  return NULL;
}

static void 
egg_icon_list_select_all_between (EggIconList     *icon_list,
				  EggIconListItem *anchor,
				  EggIconListItem *cursor,
				  gboolean         emit)
{
  GList *items;
  EggIconListItem *item;
  gint row1, row2, col1, col2;

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

  for (items = icon_list->priv->items; items; items = items->next)
    {
      item = items->data;

      if (row1 <= item->row && item->row <= row2 &&
	  col1 <= item->col && item->col <= col2)
	{
	  item->selected = TRUE;
	  
	  egg_icon_list_queue_draw_item (icon_list, item);
	}
    }

  if (emit)
    g_signal_emit (icon_list, icon_list_signals[SELECTION_CHANGED], 0);
}

static void 
egg_icon_list_move_cursor_up_down (EggIconList *icon_list,
				   gint         count)
{
  EggIconListItem *item;

  if (!GTK_WIDGET_HAS_FOCUS (icon_list))
    return;
  
  if (!icon_list->priv->cursor_item)
    {
      GList *list;

      if (count > 0)
	list = icon_list->priv->items;
      else
	list = g_list_last (icon_list->priv->items);

      item = list->data;
    }
  else
    item = find_item (icon_list, 
		      icon_list->priv->cursor_item,
		      count, 0);

  if (!item)
    return;

  if (icon_list->priv->ctrl_pressed ||
      !icon_list->priv->shift_pressed ||
      !icon_list->priv->anchor_item ||
      icon_list->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_list->priv->anchor_item = item;

  egg_icon_list_set_cursor_item (icon_list, item);

  if (!icon_list->priv->ctrl_pressed &&
      icon_list->priv->selection_mode != GTK_SELECTION_NONE)
    {
      egg_icon_list_unselect_all (icon_list);
      egg_icon_list_select_all_between (icon_list, 
					icon_list->priv->anchor_item,
					item, TRUE);
    }

  egg_icon_list_scroll_to_item (icon_list, item);
}

static void 
egg_icon_list_move_cursor_page_up_down (EggIconList *icon_list,
					gint         count)
{
  EggIconListItem *item;

  if (!GTK_WIDGET_HAS_FOCUS (icon_list))
    return;
  
  if (!icon_list->priv->cursor_item)
    {
      GList *list;

      if (count > 0)
	list = icon_list->priv->items;
      else
	list = g_list_last (icon_list->priv->items);

      item = list->data;
    }
  else
    item = find_item_page_up_down (icon_list, 
				   icon_list->priv->cursor_item,
				   count);

  if (!item)
    return;

  if (icon_list->priv->ctrl_pressed ||
      !icon_list->priv->shift_pressed ||
      !icon_list->priv->anchor_item ||
      icon_list->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_list->priv->anchor_item = item;

  egg_icon_list_set_cursor_item (icon_list, item);

  if (!icon_list->priv->ctrl_pressed &&
      icon_list->priv->selection_mode != GTK_SELECTION_NONE)
    {
      egg_icon_list_unselect_all (icon_list);
      egg_icon_list_select_all_between (icon_list, 
					icon_list->priv->anchor_item,
					item, TRUE);
    }

  egg_icon_list_scroll_to_item (icon_list, item);
}

static void 
egg_icon_list_move_cursor_left_right (EggIconList *icon_list,
				      gint         count)
{
  EggIconListItem *item;

  if (!GTK_WIDGET_HAS_FOCUS (icon_list))
    return;
  
  if (!icon_list->priv->cursor_item)
    {
      GList *list;

      if (count > 0)
	list = icon_list->priv->items;
      else
	list = g_list_last (icon_list->priv->items);

      item = list->data;
    }
  else
    item = find_item (icon_list, 
		      icon_list->priv->cursor_item,
		      0, count);

  if (!item)
    return;

  if (icon_list->priv->ctrl_pressed ||
      !icon_list->priv->shift_pressed ||
      !icon_list->priv->anchor_item ||
      icon_list->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_list->priv->anchor_item = item;

  egg_icon_list_set_cursor_item (icon_list, item);

  if (!icon_list->priv->ctrl_pressed &&
      icon_list->priv->selection_mode != GTK_SELECTION_NONE)
    {
      egg_icon_list_unselect_all (icon_list);
      egg_icon_list_select_all_between (icon_list, 
					icon_list->priv->anchor_item,
					item, TRUE);
    }

  egg_icon_list_scroll_to_item (icon_list, item);
}

static void 
egg_icon_list_move_cursor_start_end (EggIconList *icon_list,
				     gint         count)
{
  EggIconListItem *item;
  GList *list;
  
  if (!GTK_WIDGET_HAS_FOCUS (icon_list))
    return;
  
  if (count < 0)
    list = icon_list->priv->items;
  else
    list = g_list_last (icon_list->priv->items);
  
  item = list->data;

  if (!item)
    return;

  if (icon_list->priv->ctrl_pressed ||
      !icon_list->priv->shift_pressed ||
      !icon_list->priv->anchor_item ||
      icon_list->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_list->priv->anchor_item = item;

  egg_icon_list_set_cursor_item (icon_list, item);

  if (!icon_list->priv->ctrl_pressed &&
      icon_list->priv->selection_mode != GTK_SELECTION_NONE)
    {
      egg_icon_list_unselect_all (icon_list);
      egg_icon_list_select_all_between (icon_list, 
					icon_list->priv->anchor_item,
					item, TRUE);
    }

  egg_icon_list_scroll_to_item (icon_list, item);
}

static void     
egg_icon_list_scroll_to_item (EggIconList     *icon_list, 
			      EggIconListItem *item)
{
  gint y, height;
  gdouble value;

  gdk_window_get_geometry (icon_list->priv->bin_window, NULL, &y, NULL, &height, NULL);

  if (y + item->y < 0)
    {
      value = icon_list->priv->vadjustment->value + y + item->y;
      gtk_adjustment_set_value (icon_list->priv->vadjustment, value);
    }
  else if (y + item->y + item->height > GTK_WIDGET (icon_list)->allocation.height)
    {
      value = icon_list->priv->vadjustment->value + y + item->y + item->height 
	- GTK_WIDGET (icon_list)->allocation.height;
      gtk_adjustment_set_value (icon_list->priv->vadjustment, value);
    }
}

/* Public API */


/**
 * egg_icon_list_new:
 * 
 * Creates a new #EggIconList widget
 * 
 * Return value: A newly created #EggIconList widget
 **/
GtkWidget *
egg_icon_list_new (void)
{
  return g_object_new (EGG_TYPE_ICON_LIST, NULL);
}

/**
 * egg_icon_list_new_with_model:
 * @model: The model.
 * 
 * Creates a new #EggIconList widget with the model initialized @model.
 * 
 * Return value: A newly created #EggIconList widget.
 *
 * Since: 2.6 
 **/
GtkWidget *
egg_icon_list_new_with_model (GtkTreeModel *model)
{
  return g_object_new (EGG_TYPE_ICON_LIST, "model", model, NULL);
}


/**
 * egg_icon_list_get_path_at_pos:
 * @icon_list: A #EggIconList.
 * @x: The x position to be identified.
 * @y: The y position to be identified
 * 
 * Finds the path at the point (@x, @y), relative to widget coordinates.
 * 
 * Return value: The #GtkTreePath corresponding to the icon or %NULL
 * if no icon exists at that coordinate.
 *
 * Since: 2.6 
 **/
GtkTreePath *
egg_icon_list_get_path_at_pos (EggIconList *icon_list,
			       gint         x,
			       gint         y)
{
  EggIconListItem *item;
  GtkTreePath *path;
  
  g_return_val_if_fail (EGG_IS_ICON_LIST (icon_list), NULL);

  item = egg_icon_list_get_item_at_pos (icon_list, x, y);

  if (!item)
    return NULL;

  path = gtk_tree_path_new_from_indices (item->index, -1);

  return path;
}

/**
 * egg_icon_list_selected_foreach:
 * @icon_list: A #EggIconList.
 * @func: The funcion to call for each selected icon.
 * @data: User data to pass to the function.
 * 
 * Calls a function for each selected icon. Note that the tree or
 * selection cannot be modified from within this function.
 *
 * Since: 2.6 
 **/
void
egg_icon_list_selected_foreach (EggIconList           *icon_list,
				EggIconListForeachFunc func,
				gpointer               data)
{
  GList *list;
  
  for (list = icon_list->priv->items; list; list = list->next)
    {
      EggIconListItem *item = list->data;
      GtkTreePath *path = gtk_tree_path_new_from_indices (item->index, -1);

      if (item->selected)
	(* func) (icon_list, path, data);

      gtk_tree_path_free (path);
    }
}

/**
 * egg_icon_list_set_selection_mode:
 * @icon_list: A #EggIconList.
 * @mode: The selection mode
 * 
 * Sets the selection mode of the @icon_list.
 *
 * Since: 2.6 
 **/
void
egg_icon_list_set_selection_mode (EggIconList      *icon_list,
				  GtkSelectionMode  mode)
{
  g_return_if_fail (EGG_IS_ICON_LIST (icon_list));

  if (mode == icon_list->priv->selection_mode)
    return;
  
  if (mode == GTK_SELECTION_NONE ||
      icon_list->priv->selection_mode == GTK_SELECTION_MULTIPLE)
    egg_icon_list_unselect_all (icon_list);
  
  icon_list->priv->selection_mode = mode;

  g_object_notify (G_OBJECT (icon_list), "selection_mode");
}

/**
 * egg_icon_list_get_selection_mode:
 * @icon_list: A #EggIconList.
 * 
 * Sets the selection mode of the @icon_list.
 *
 * Return value: the current selection mode
 *
 * Since: 2.6 
 **/
GtkSelectionMode
egg_icon_list_get_selection_mode (EggIconList *icon_list)
{
  g_return_val_if_fail (EGG_IS_ICON_LIST (icon_list), GTK_SELECTION_SINGLE);

  return icon_list->priv->selection_mode;
}

/**
 * egg_icon_list_set_model:
 * @icon_list: A #EggIconList.
 * @model: The model.
 *
 * Sets the model for a #EggIconList.  If the @icon_list already has a model
 * set, it will remove it before setting the new model.  If @model is %NULL, then
 * it will unset the old model.
 *
 * Since: 2.6 
 **/
void
egg_icon_list_set_model (EggIconList *icon_list,
			 GtkTreeModel *model)
{
  g_return_if_fail (EGG_IS_ICON_LIST (icon_list));

  if (model != NULL)
    g_return_if_fail (GTK_IS_TREE_MODEL (model));
  
  if (icon_list->priv->model == model)
    return;

  if (model)
    {
      GType pixbuf_column_type, text_column_type;
      
      g_return_if_fail (gtk_tree_model_get_flags (model) & GTK_TREE_MODEL_LIST_ONLY);

      if (icon_list->priv->pixbuf_column != -1)
	{
	  pixbuf_column_type = gtk_tree_model_get_column_type (icon_list->priv->model,
							       icon_list->priv->pixbuf_column);	  

	  g_return_if_fail (pixbuf_column_type == GDK_TYPE_PIXBUF);
	}

      if (icon_list->priv->text_column != -1)
	{
	  text_column_type = gtk_tree_model_get_column_type (icon_list->priv->model,
							     icon_list->priv->pixbuf_column);	  

	  g_return_if_fail (text_column_type == G_TYPE_STRING);
	}
      
    }
  
  if (icon_list->priv->model)
    {
      g_signal_handlers_disconnect_by_func (icon_list->priv->model,
					    egg_icon_list_row_changed,
					    icon_list);
      g_signal_handlers_disconnect_by_func (icon_list->priv->model,
					    egg_icon_list_row_inserted,
					    icon_list);
      g_signal_handlers_disconnect_by_func (icon_list->priv->model,
					    egg_icon_list_row_deleted,
					    icon_list);
      g_signal_handlers_disconnect_by_func (icon_list->priv->model,
					    egg_icon_list_rows_reordered,
					    icon_list);

      g_object_unref (icon_list->priv->model);
      
      g_list_foreach (icon_list->priv->items, (GFunc)egg_icon_list_item_unref, NULL);
      g_list_free (icon_list->priv->items);
      icon_list->priv->items = NULL;
    }

  icon_list->priv->model = model;

  if (icon_list->priv->model)
    {
      g_object_ref (icon_list->priv->model);
      g_signal_connect (icon_list->priv->model,
			"row_changed",
			G_CALLBACK (egg_icon_list_row_changed),
			icon_list);
      g_signal_connect (icon_list->priv->model,
			"row_inserted",
			G_CALLBACK (egg_icon_list_row_inserted),
			icon_list);
      g_signal_connect (icon_list->priv->model,
			"row_deleted",
			G_CALLBACK (egg_icon_list_row_deleted),
			icon_list);
      g_signal_connect (icon_list->priv->model,
			"rows_reordered",
			G_CALLBACK (egg_icon_list_rows_reordered),
			icon_list);

      egg_icon_list_build_items (icon_list);
    }

  g_object_notify (G_OBJECT (icon_list), "model");  
}

/**
 * egg_icon_list_get_model:
 * @icon_list: a #EggIconList
 *
 * Returns the model the #EggIconList is based on.  Returns %NULL if the
 * model is unset.
 *
 * Return value: A #GtkTreeModel, or %NULL if none is currently being used.
 *
 * Since: 2.6 
 **/
GtkTreeModel *
egg_icon_list_get_model (EggIconList *icon_list)
{
  g_return_val_if_fail (EGG_IS_ICON_LIST (icon_list), NULL);

  return icon_list->priv->model;
}

/**
 * egg_icon_list_set_text_column:
 * @icon_list: A #EggIconList.
 * @column: A column in the currently used model.
 * 
 * Sets the column with text for @icon_list to be @column. The text
 * column must be of type #G_TYPE_STRING.
 *
 * Since: 2.6 
 **/
void
egg_icon_list_set_text_column (EggIconList *icon_list,
			       int          column)
{
  if (column == icon_list->priv->text_column)
    return;
  
  if (column == -1)
    icon_list->priv->text_column = -1;
  else
    {
      if (icon_list->priv->model != NULL)
	{
	  GType column_type;
	  
	  column_type = gtk_tree_model_get_column_type (icon_list->priv->model, column);

	  g_return_if_fail (column_type == G_TYPE_STRING);
	}
      
      icon_list->priv->text_column = column;
    }

  egg_icon_list_invalidate_sizes (icon_list);
  egg_icon_list_queue_layout (icon_list);
  
  g_object_notify (G_OBJECT (icon_list), "text_column");
}

/**
 * egg_icon_list_get_text_column:
 * @icon_list: A #EggIconList.
 *
 * Returns the column with text for @icon_list.
 *
 * Returns: the text column, or -1 if it's unset.
 *
 * Since: 2.6
 */
gint
egg_icon_list_get_text_column (EggIconList  *icon_list)
{
  g_return_val_if_fail (EGG_IS_ICON_LIST (icon_list), -1);

  return icon_list->priv->text_column;
}

/**
 * egg_icon_list_set_markup_column:
 * @icon_list: A #EggIconList.
 * @column: A column in the currently used model.
 * 
 * Sets the column with markup information for @icon_list to be
 * @column. The markup column must be of type #G_TYPE_STRING.
 * If the markup column is set to something, it overrides
 * the text column set by #egg_icon_list_set_text_column.
 *
 * Since: 2.6
 **/
void
egg_icon_list_set_markup_column (EggIconList *icon_list,
				 int          column)
{
  if (column == icon_list->priv->markup_column)
    return;
  
  if (column == -1)
    icon_list->priv->markup_column = -1;
  else
    {
      if (icon_list->priv->model != NULL)
	{
	  GType column_type;
	  
	  column_type = gtk_tree_model_get_column_type (icon_list->priv->model, column);

	  g_return_if_fail (column_type == G_TYPE_STRING);
	}
      
      icon_list->priv->markup_column = column;
    }

  egg_icon_list_invalidate_sizes (icon_list);
  egg_icon_list_queue_layout (icon_list);
  
  g_object_notify (G_OBJECT (icon_list), "markup_column");
}

/**
 * egg_icon_list_get_markup_column:
 * @icon_list: A #EggIconList.
 *
 * Returns the column with markup text for @icon_list.
 *
 * Returns: the markup column, or -1 if it's unset.
 *
 * Since: 2.6
 */
gint
egg_icon_list_get_markup_column (EggIconList  *icon_list)
{
  g_return_val_if_fail (EGG_IS_ICON_LIST (icon_list), -1);

  return icon_list->priv->markup_column;
}

/**
 * egg_icon_list_set_pixbuf_column:
 * @icon_list: A #EggIconList.
 * @column: A column in the currently used model.
 * 
 * Sets the column with pixbufs for @icon_list to be @column. The pixbuf
 * column must be of type #GDK_TYPE_PIXBUF
 *
 * Since: 2.6 
 **/
void
egg_icon_list_set_pixbuf_column (EggIconList *icon_list,
				 int          column)
{
  if (column == icon_list->priv->pixbuf_column)
    return;
  
  if (column == -1)
    icon_list->priv->pixbuf_column = -1;
  else
    {
      if (icon_list->priv->model != NULL)
	{
	  GType column_type;
	  
	  column_type = gtk_tree_model_get_column_type (icon_list->priv->model, column);

	  g_return_if_fail (column_type == GDK_TYPE_PIXBUF);
	}
      
      icon_list->priv->pixbuf_column = column;
    }

  egg_icon_list_invalidate_sizes (icon_list);
  egg_icon_list_queue_layout (icon_list);
  
  g_object_notify (G_OBJECT (icon_list), "pixbuf_column");
  
}

/**
 * egg_icon_list_get_pixbuf_column:
 * @icon_list: A #EggIconList.
 *
 * Returns the column with pixbufs for @icon_list.
 *
 * Returns: the pixbuf column, or -1 if it's unset.
 *
 * Since: 2.6
 */
gint
egg_icon_list_get_pixbuf_column (EggIconList  *icon_list)
{
  g_return_val_if_fail (EGG_IS_ICON_LIST (icon_list), -1);

  return icon_list->priv->pixbuf_column;
}

/**
 * egg_icon_list_select_path:
 * @icon_list: A #EggIconList.
 * @path: The #GtkTreePath to be selected.
 * 
 * Selects the row at @path.
 **/
void
egg_icon_list_select_path (EggIconList *icon_list,
			   GtkTreePath *path)
{
  EggIconListItem *item;
  
  g_return_if_fail (EGG_IS_ICON_LIST (icon_list));
  g_return_if_fail (icon_list->priv->model != NULL);
  g_return_if_fail (path != NULL);

  item = g_list_nth (icon_list->priv->items,
		     gtk_tree_path_get_indices(path)[0])->data;

  if (!item)
    return;
  
  egg_icon_list_select_item (icon_list, item);
}

/**
 * egg_icon_list_unselect_path:
 * @icon_list: A #EggIconList.
 * @path: The #GtkTreePath to be unselected.
 * 
 * Unselects the row at @path.
 **/
void
egg_icon_list_unselect_path (EggIconList *icon_list,
			     GtkTreePath *path)
{
  EggIconListItem *item;
  
  g_return_if_fail (EGG_IS_ICON_LIST (icon_list));
  g_return_if_fail (icon_list->priv->model != NULL);
  g_return_if_fail (path != NULL);

  item = g_list_nth (icon_list->priv->items,
		     gtk_tree_path_get_indices(path)[0])->data;

  if (!item)
    return;
  
  egg_icon_list_unselect_item (icon_list, item);
}

/**
 * egg_icon_list_select_all:
 * @icon_list: A #EggIconList.
 * 
 * Selects all the icons. @icon_list must has its selection mode set
 * to #GTK_SELECTION_MULTIPLE.
 **/
void
egg_icon_list_select_all (EggIconList *icon_list)
{
  GList *items;
  gboolean dirty = FALSE;
  
  g_return_if_fail (EGG_IS_ICON_LIST (icon_list));

  for (items = icon_list->priv->items; items; items = items->next)
    {
      EggIconListItem *item = items->data;
      
      if (!item->selected)
	{
	  dirty = TRUE;
	  item->selected = TRUE;
	  egg_icon_list_queue_draw_item (icon_list, item);
	}
    }

  if (dirty)
    g_signal_emit (icon_list, icon_list_signals[SELECTION_CHANGED], 0);
}

/**
 * egg_icon_list_unselect_all:
 * @icon_list: A #EggIconList.
 * 
 * Unselects all the icons.
 **/
void
egg_icon_list_unselect_all (EggIconList *icon_list)
{
  g_return_if_fail (EGG_IS_ICON_LIST (icon_list));

  egg_icon_list_unselect_all_internal (icon_list, TRUE);
}

/**
 * egg_icon_list_path_is_selected:
 * @icon_list: A #EggIconList.
 * @path: A #GtkTreePath to check selection on.
 * 
 * Returns %TRUE if the icon pointed to by @path is currently
 * selected. If @icon does not point to a valid location, %FALSE is returned.
 * 
 * Return value: %TRUE if @path is selected.
 **/
gboolean
egg_icon_list_path_is_selected (EggIconList *icon_list,
				GtkTreePath *path)
{
  EggIconListItem *item;
  
  g_return_val_if_fail (EGG_IS_ICON_LIST (icon_list), FALSE);
  g_return_val_if_fail (icon_list->priv->model != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  
  item = g_list_nth (icon_list->priv->items,
		     gtk_tree_path_get_indices(path)[0])->data;

  if (!item)
    return FALSE;
  
  return item->selected;
}

/**
 * egg_icon_list_item_activated:
 * @icon_list: A #EggIconLis
 * @path: The #GtkTreePath to be activated
 * 
 * Activates the item determined by @path.
 **/
void
egg_icon_list_item_activated (EggIconList      *icon_list,
			      GtkTreePath      *path)
{
  g_signal_emit (G_OBJECT (icon_list), icon_list_signals[ITEM_ACTIVATED], 0, path);
}
