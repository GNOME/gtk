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

#include "eggintl.h"
#include "eggmarshalers.h"


#define MINIMUM_ICON_ITEM_WIDTH 100
#define ICON_TEXT_PADDING 3

#define ICON_LIST_ITEM_DATA "egg-icon-list-item-data"

struct _EggIconListItem
{
  GObject parent;
  
  EggIconList *icon_list;
  char *label;
  GdkPixbuf *icon;

  GList *list;
  
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

  GtkSelectionMode selection_mode;

  GdkWindow *bin_window;

  GList *items;
  GList *last_item;
  gint item_count;
  
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;

  guint layout_idle_id;
  
  gboolean rubberbanding;
  gint rubberband_x1, rubberband_y1;
  gint rubberband_x2, rubberband_y2;
  
  EggIconListItem *cursor_item;
  
  char *typeahead_string;

  /* Sorting */
  gboolean sorted;
  GtkSortType sort_order;
  
  EggIconListItemCompareFunc sort_func;
  gpointer sort_data;
  GDestroyNotify sort_destroy_notify;

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
  ITEM_ADDED,
  ITEM_REMOVED,
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
  PROP_SELECTION_MODE,
  PROP_SORTED,
  PROP_SORT_ORDER,
};

/* Icon List Item properties */
enum
{
  PROP_ITEM_0,
  PROP_LABEL,
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
static gboolean egg_icon_list_key_press      (GtkWidget      *widget,
					      GdkEventKey    *event);


/* EggIconList signals */
static void     egg_icon_list_set_adjustments             (EggIconList             *icon_list,
							   GtkAdjustment           *hadj,
							   GtkAdjustment           *vadj);
static void     egg_icon_list_real_select_all             (EggIconList             *icon_list);
static void     egg_icon_list_real_unselect_all           (EggIconList             *icon_list);
static void     egg_icon_list_real_select_cursor_item     (EggIconList             *icon_list);
static void     egg_icon_list_real_toggle_cursor_item     (EggIconList             *icon_list);

/* Internal functions */
static void     egg_icon_list_adjustment_changed          (GtkAdjustment           *adjustment,
							   EggIconList             *icon_list);
static void     egg_icon_list_layout                      (EggIconList             *icon_list);
static void     egg_icon_list_paint_item                  (EggIconList             *icon_list,
							   EggIconListItem         *item,
							   GdkRectangle            *area);
static void     egg_icon_list_paint_rubberband            (EggIconList             *icon_list,
							   GdkRectangle            *area);
static void     egg_icon_list_queue_draw_item             (EggIconList             *icon_list,
							   EggIconListItem         *item);
static void     egg_icon_list_queue_layout                (EggIconList             *icon_list);
static void     egg_icon_list_set_cursor_item             (EggIconList             *icon_list,
							   EggIconListItem         *item);
static void     egg_icon_list_append_typeahead_string     (EggIconList             *icon_list,
							   const gchar             *string);
static void     egg_icon_list_select_first_matching_item  (EggIconList             *icon_list,
							   const char              *pattern);
static void     egg_icon_list_start_rubberbanding         (EggIconList             *icon_list,
							   gint                     x,
							   gint                     y);
static void     egg_icon_list_stop_rubberbanding          (EggIconList             *icon_list);
static void     egg_icon_list_sort                        (EggIconList             *icon_list);
static gint     egg_icon_list_sort_func                   (EggIconListItem         *a,
							   EggIconListItem         *b,
							   EggIconList             *icon_list);
static void     egg_icon_list_insert_item_sorted          (EggIconList             *icon_list,
							   EggIconListItem         *item);
static void     egg_icon_list_validate                    (EggIconList             *icon_list);
static void     egg_icon_list_update_rubberband_selection (EggIconList             *icon_list);
static gboolean egg_icon_list_item_hit_test               (EggIconListItem         *item,
							   gint                     x,
							   gint                     y,
							   gint                     width,
							   gint                     height);
static gboolean egg_icon_list_maybe_begin_dragging_items  (EggIconList             *icon_list,
							   GdkEventMotion          *event);
static gboolean egg_icon_list_unselect_all_internal       (EggIconList             *icon_list,
							   gboolean                 emit);
static void     egg_icon_list_calculate_item_size         (EggIconList *icon_list, EggIconListItem         *item);
static void     rubberbanding                             (gpointer                 data);


/* EggIconListItem functions */
static void egg_icon_list_item_class_init   (GObjectClass    *klass);
static void egg_icon_list_item_init         (EggIconListItem *item);
static void egg_icon_list_item_finalize     (GObject         *object);
static void egg_icon_list_item_set_property (GObject         *object,
					     guint            prop_id,
					     const GValue    *value,
					     GParamSpec      *pspec);
static void egg_icon_list_item_get_property (GObject         *object,
					     guint            prop_id,
					     GValue          *value,
					     GParamSpec      *pspec);




static void     egg_icon_list_item_invalidate_size        (EggIconListItem         *item);

static GtkContainerClass *parent_class = NULL;
static GObjectClass *item_parent_class = NULL;
static guint icon_list_signals[LAST_SIGNAL] = { 0 };

GType
egg_icon_list_item_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
	{
	  sizeof (GObjectClass),
	  NULL,		/* base_init */
	  NULL,		/* base_finalize */
	  (GClassInitFunc) egg_icon_list_item_class_init,
	  NULL,		/* class_finalize */
	  NULL,		/* class_data */
	  sizeof (EggIconListItem),
	  0,              /* n_preallocs */
	  (GInstanceInitFunc) egg_icon_list_item_init
	};

      object_type = g_type_register_static (G_TYPE_OBJECT, "EggIconListItem", &object_info, 0);
    }

  return object_type;
}

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
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;
  
  parent_class = g_type_class_peek_parent (klass);
  binding_set = gtk_binding_set_by_class (klass);
  
  gobject_class = (GObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;

  gobject_class->finalize = egg_icon_list_finalize;
  gobject_class->set_property = egg_icon_list_set_property;
  gobject_class->get_property = egg_icon_list_get_property;

  widget_class->realize = egg_icon_list_realize;
  widget_class->unrealize = egg_icon_list_unrealize;
  widget_class->map = egg_icon_list_map;
  widget_class->size_request = egg_icon_list_size_request;
  widget_class->size_allocate = egg_icon_list_size_allocate;
  widget_class->expose_event = egg_icon_list_expose;
  widget_class->motion_notify_event = egg_icon_list_motion;
  widget_class->button_press_event = egg_icon_list_button_press;
  widget_class->button_release_event = egg_icon_list_button_release;
  widget_class->key_press_event = egg_icon_list_key_press;
  
  klass->set_scroll_adjustments = egg_icon_list_set_adjustments;
  klass->select_all = egg_icon_list_real_select_all;
  klass->unselect_all = egg_icon_list_real_unselect_all;
  klass->select_cursor_item = egg_icon_list_real_select_cursor_item;
  klass->toggle_cursor_item = egg_icon_list_real_toggle_cursor_item;
  
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
				   PROP_SORTED,
				   g_param_spec_boolean ("sorted",
							 _("Sorted"),
							 _("Icon list is sorted"),
							 FALSE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_SORT_ORDER,
                                   g_param_spec_enum ("sort_order",
                                                      _("Sort order"),
                                                      _("Sort direction the icon list should use"),
                                                      GTK_TYPE_SORT_TYPE,
                                                      GTK_SORT_ASCENDING,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

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
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  EGG_TYPE_ICON_LIST_ITEM);

  icon_list_signals[SELECTION_CHANGED] =
    g_signal_new ("selection_changed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (EggIconListClass, selection_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  icon_list_signals[ITEM_ADDED] =
    g_signal_new ("item_added",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggIconListClass, item_added),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, EGG_TYPE_ICON_LIST_ITEM);

  icon_list_signals[ITEM_REMOVED] =
    g_signal_new ("item_removed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggIconListClass, item_removed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, EGG_TYPE_ICON_LIST_ITEM);
  
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

  /* Key bindings */
  gtk_binding_entry_add_signal (binding_set, GDK_a, GDK_CONTROL_MASK, "select_all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_a, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "unselect_all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_space, 0, "select_cursor_item", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_CONTROL_MASK, "toggle_cursor_item", 0);
}


static void
egg_icon_list_init (EggIconList *icon_list)
{
  icon_list->priv = g_new0 (EggIconListPrivate, 1);
  GTK_WIDGET_SET_FLAGS (icon_list, GTK_CAN_FOCUS);

  icon_list->priv->width = 0;
  icon_list->priv->height = 0;
  icon_list->priv->selection_mode = GTK_SELECTION_SINGLE;
  icon_list->priv->sort_order = GTK_SORT_ASCENDING;
  icon_list->priv->pressed_button = -1;
  icon_list->priv->press_start_x = -1;
  icon_list->priv->press_start_y = -1;
  icon_list->priv->layout = gtk_widget_create_pango_layout (GTK_WIDGET (icon_list), NULL);
  
  egg_icon_list_set_adjustments (icon_list, NULL, NULL);
}


/* GObject methods */
static void
egg_icon_list_finalize (GObject *object)
{
  EggIconList *icon_list;

  icon_list = EGG_ICON_LIST (object);

  if (icon_list->priv->layout_idle_id != 0)
    g_source_remove (icon_list->priv->layout_idle_id);

  g_free (icon_list->priv);
  
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
    case PROP_SORTED:
      egg_icon_list_set_sorted (icon_list, g_value_get_boolean (value));
      break;
    case PROP_SORT_ORDER:
      egg_icon_list_set_sort_order (icon_list, g_value_get_enum (value));
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
    case PROP_SORTED:
      g_value_set_boolean (value, icon_list->priv->sorted);
      break;
    case PROP_SORT_ORDER:
      g_value_set_enum (value, icon_list->priv->sort_order);
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
  g_signal_emit_by_name (icon_list->priv->hadjustment, "changed");

  icon_list->priv->vadjustment->page_size = allocation->height;
  icon_list->priv->vadjustment->page_increment = allocation->height * 0.9;
  icon_list->priv->vadjustment->step_increment = allocation->width * 0.1;
  icon_list->priv->vadjustment->lower = 0;
  icon_list->priv->vadjustment->upper = MAX (allocation->height, icon_list->priv->height);
  g_signal_emit_by_name (icon_list->priv->hadjustment, "changed");

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
egg_icon_list_motion (GtkWidget      *widget,
		      GdkEventMotion *event)
{
  EggIconList *icon_list;

  icon_list = EGG_ICON_LIST (widget);

  egg_icon_list_maybe_begin_dragging_items (icon_list, event);

  if (icon_list->priv->rubberbanding)
    rubberbanding (widget);
  
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

      if (icon_list->priv->selection_mode == GTK_SELECTION_NONE)
	return TRUE;
      
      item = egg_icon_list_get_item_at_pos (icon_list,
					    event->x, event->y);

      if (item != NULL)
	{
	  if (icon_list->priv->selection_mode == GTK_SELECTION_MULTIPLE &&
	      (event->state & GDK_CONTROL_MASK))
	    {
	      item->selected = !item->selected;
	      dirty = TRUE;
	    }
	  else
	    {
	      if (!item->selected)
		{
		  egg_icon_list_unselect_all_internal (icon_list, FALSE);
	      
		  item->selected = TRUE;
		  dirty = TRUE;
		}
	    }
	  
	  egg_icon_list_set_cursor_item (icon_list, item);
	  egg_icon_list_queue_draw_item (icon_list, item);
	  
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
	  egg_icon_list_item_activated (icon_list, item);
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

  return TRUE;
}


static gboolean
egg_icon_list_key_press (GtkWidget    *widget,
			 GdkEventKey  *event)
{
  if ((* GTK_WIDGET_CLASS (parent_class)->key_press_event) (widget, event))
    return TRUE;

  return FALSE;
  
  if ((event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) == 0)
    egg_icon_list_append_typeahead_string (EGG_ICON_LIST (widget), event->string);

  return TRUE;
}

static void
egg_icon_list_select_first_matching_item (EggIconList  *icon_list,
					  const char   *pattern)
{
  GList *items;

  if (pattern == NULL)
    return;
  
  for (items = icon_list->priv->items; items; items = items->next)
    {
      EggIconListItem *item = items->data;

      if (strncmp (pattern, item->label, strlen (pattern)) == 0)
	{
	  egg_icon_list_select_item (icon_list, item);
	  break;
	}
    }
}

static void
rubberbanding (gpointer data)
{
  EggIconList *icon_list;
  gint x, y;
  GdkRectangle old_area;
  GdkRectangle new_area;
  GdkRectangle common;
  GdkRegion *common_region;
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

  gdk_rectangle_intersect (&old_area, &new_area, &common);
  
  /* always invalidate border */
  if (common.width > 2)
    {
      common.width -= 2;
      common.x += 1;
    }
  
  if (common.height > 2)
    {
      common.y += 1;
      common.height -= 2;
    }

  common_region = gdk_region_rectangle (&common);
  
  invalid_region = gdk_region_rectangle (&old_area);
  gdk_region_union_with_rect (invalid_region, &new_area);
  gdk_region_subtract (invalid_region, common_region);
  
  gdk_window_invalidate_region (icon_list->priv->bin_window, invalid_region, TRUE);
    
  gdk_region_destroy (common_region);
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

static gint
egg_icon_list_sort_func (EggIconListItem  *a,
			 EggIconListItem  *b,
			 EggIconList      *icon_list)
{
  gint result;

  result = (* icon_list->priv->sort_func) (icon_list, a, b,
					   icon_list->priv->sort_data);

  if (icon_list->priv->sort_order == GTK_SORT_DESCENDING)
    result = -result;

  return result;
}

static void
egg_icon_list_insert_item_sorted (EggIconList      *icon_list,
				  EggIconListItem  *item)
{
  GList *list;
  GList *tmp_list = icon_list->priv->items;
  gint cmp;
  
  egg_icon_list_validate (icon_list);

  list = g_list_alloc ();
  item->list = list;
  item->icon_list = icon_list;
  list->data = item;
  
  if (!icon_list->priv->items)
    {
      icon_list->priv->items = list;
      icon_list->priv->last_item = list;
      icon_list->priv->item_count += 1;

      egg_icon_list_validate (icon_list);
      
      return;
    }

  cmp = egg_icon_list_sort_func (item, tmp_list->data, icon_list);

  while ((tmp_list->next) && (cmp > 0))
    {
      tmp_list = tmp_list->next;
      cmp = egg_icon_list_sort_func (item, tmp_list->data, icon_list);
    }

  if ((!tmp_list->next) && (cmp > 0))
    {
      tmp_list->next = list;
      list->prev = tmp_list;
      icon_list->priv->last_item = list;
      icon_list->priv->item_count += 1;
      egg_icon_list_validate (icon_list);
      
      return;
    }

  if (tmp_list->prev)
    {
      tmp_list->prev->next = list;
      list->prev = tmp_list->prev;
    }
  
  list->next = tmp_list;
  tmp_list->prev = list;

  if (tmp_list == icon_list->priv->items)
    icon_list->priv->items = list;
  
  icon_list->priv->item_count += 1;
  egg_icon_list_validate (icon_list);

  egg_icon_list_queue_layout (icon_list);
}


static void
egg_icon_list_sort (EggIconList *icon_list)
{
  egg_icon_list_validate (icon_list);

  /* FIXME: We can optimize this */
  icon_list->priv->items = g_list_sort_with_data (icon_list->priv->items,
						  (GCompareDataFunc)egg_icon_list_sort_func,
						  icon_list);
  icon_list->priv->last_item = g_list_last (icon_list->priv->items);
  
  egg_icon_list_validate (icon_list);
  egg_icon_list_queue_layout (icon_list);
}


static void
egg_icon_list_validate (EggIconList *icon_list)
{
#if 0
  GList *list;

  g_print ("----\n");
  for (list = icon_list->priv->items; list; list = list->next)
    {
      EggIconListItem *item = list->data;

      g_print ("%s\n", egg_icon_list_item_get_label (item));
    }
  g_print ("----\n");
#endif
  
  g_assert (g_list_length (icon_list->priv->items) == icon_list->priv->item_count);
  g_assert (g_list_last (icon_list->priv->items) == icon_list->priv->last_item);
  g_assert (g_list_first (icon_list->priv->last_item) == icon_list->priv->items);
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
    gtk_drag_set_icon_pixbuf (context, egg_icon_list_item_get_icon (item),
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
  if (icon_list->priv->selection_mode == GTK_SELECTION_NONE)
    return;

  /* FIXME: Use another function here */
  if (icon_list->priv->cursor_item != NULL)
    {
      if (icon_list->priv->selection_mode == GTK_SELECTION_BROWSE)
	icon_list->priv->cursor_item->selected = TRUE;
      else
	icon_list->priv->cursor_item->selected = !icon_list->priv->cursor_item->selected;
      
      egg_icon_list_queue_draw_item (icon_list, icon_list->priv->cursor_item);
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
      gdk_window_process_updates (icon_list->priv->bin_window, TRUE);
    }
}

static GList *
egg_icon_list_layout_single_row (EggIconList *icon_list, GList *first_item, gint *y, gint *maximum_width)
{
  gint x, current_width, max_height, max_pixbuf_height;
  GList *items, *last_item;
  gint icon_padding;
  gint left_margin, right_margin;
  gint maximum_layout_width;
  
  x = 0;
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
      item->x = x;
      
      if (item->width < MINIMUM_ICON_ITEM_WIDTH) {
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
    }
  
  return last_item;
}

static void
egg_icon_list_set_adjustment_upper (GtkAdjustment *adj,
				    gdouble        upper)
{
  if (upper != adj->upper)
    {
      gdouble min = MAX (0., upper - adj->page_size);
      gboolean value_changed = FALSE;
      
      adj->upper = upper;
      
      if (adj->value > min)
	{
	  adj->value = min;
	  value_changed = TRUE;
	}
      
      g_signal_emit_by_name (adj, "changed");
      
      if (value_changed)
	g_signal_emit_by_name (adj, "value_changed");
    }
}

static void
egg_icon_list_layout (EggIconList *icon_list)
{
  gint y = 0, maximum_width = 0;
  GList *icons;
  GtkWidget *widget;
  gint top_margin, bottom_margin;
  
  widget = GTK_WIDGET (icon_list);
  icons = icon_list->priv->items;

  gtk_widget_style_get (widget,
			"top_margin", &top_margin,
			"bottom_margin", &bottom_margin,
			NULL);
  y += top_margin;
  
  do
    {
      icons = egg_icon_list_layout_single_row (icon_list, icons, &y, &maximum_width);
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


static void
egg_icon_list_item_init (EggIconListItem *item)
{
  item->width = -1;
  item->height = -1;
}

static void
egg_icon_list_item_class_init (GObjectClass *klass)
{
  item_parent_class = g_type_class_peek_parent (klass);

  klass->finalize = egg_icon_list_item_finalize;;
  klass->set_property = egg_icon_list_item_set_property;
  klass->get_property = egg_icon_list_item_get_property;

  /* Properties */
  g_object_class_install_property (klass,
				   PROP_LABEL,
				   g_param_spec_string ("label",
							_("Icon item label"),
							_("The label of the icon item"),
							NULL,
							G_PARAM_READWRITE));

}

static void
egg_icon_list_item_set_property (GObject         *object,
				 guint            prop_id,
				 const GValue    *value,
				 GParamSpec      *pspec)
{
}

static void
egg_icon_list_item_get_property (GObject         *object,
				 guint            prop_id,
				 GValue          *value,
				 GParamSpec      *pspec)
{
}

static void
egg_icon_list_item_finalize (GObject *object)
{
  EggIconListItem *item;

  item = EGG_ICON_LIST_ITEM (object);
  
  g_free (item->label);
  g_object_unref (item->icon);
  
  (G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Creates or updates the pango layout and calculates the size */
static void
egg_icon_list_calculate_item_size (EggIconList *icon_list, EggIconListItem *item)
{
  int layout_width, layout_height;
  int maximum_layout_width;
  
  if (item->width != -1 && item->width != -1) 
    return;

  item->pixbuf_width = gdk_pixbuf_get_width (item->icon);
  item->pixbuf_height = gdk_pixbuf_get_height (item->icon);

  maximum_layout_width = MAX (item->pixbuf_width, MINIMUM_ICON_ITEM_WIDTH);

  pango_layout_set_text (icon_list->priv->layout, item->label, -1);

  pango_layout_set_alignment (icon_list->priv->layout, PANGO_ALIGN_CENTER);
  pango_layout_set_width (icon_list->priv->layout, maximum_layout_width * PANGO_SCALE);
  
  pango_layout_get_pixel_size (icon_list->priv->layout, &layout_width, &layout_height);

  item->width = MAX ((layout_width + 2 * ICON_TEXT_PADDING), item->pixbuf_width);
  item->height = layout_height + 2 * ICON_TEXT_PADDING + item->pixbuf_height;
  item->layout_width = layout_width;
  item->layout_height = layout_height;
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
  GdkPixbuf *pixbuf;
  GtkStateType state;
  
  if (GTK_WIDGET_HAS_FOCUS (icon_list))
    state = GTK_STATE_SELECTED;
  else
    state = GTK_STATE_ACTIVE;

  if (item->selected)
    pixbuf = create_colorized_pixbuf (item->icon,
				      &GTK_WIDGET (icon_list)->style->base[state]);
  else
    pixbuf = gdk_pixbuf_ref (item->icon);
	
  gdk_pixbuf_render_to_drawable_alpha (pixbuf,
				       icon_list->priv->bin_window,
				       0, 0,
				       item->pixbuf_x,
				       item->pixbuf_y,
				       item->pixbuf_width,
				       item->pixbuf_height,
				       GDK_PIXBUF_ALPHA_FULL,
				       0,
				       GDK_RGB_DITHER_NORMAL,
				       item->pixbuf_width,
				       item->pixbuf_height);
  g_object_unref (pixbuf);

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

  pango_layout_set_text (icon_list->priv->layout, item->label, -1);
  gdk_draw_layout (icon_list->priv->bin_window,
		   GTK_WIDGET (icon_list)->style->text_gc[item->selected ? state : GTK_STATE_NORMAL],
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
		     "iconlist",
		     item->layout_x - ICON_TEXT_PADDING,
		     item->layout_y - ICON_TEXT_PADDING,
		     item->layout_width + 2 * ICON_TEXT_PADDING,
		     item->layout_height + 2 * ICON_TEXT_PADDING);
}

static void
egg_icon_list_paint_rubberband (EggIconList     *icon_list,
				GdkRectangle    *area)
{
  GdkRectangle rect;
  GdkPixbuf *pixbuf;
  GdkGC *gc;
  GdkColor color;
  GdkRectangle rubber_rect;
  
  rubber_rect.x = MIN (icon_list->priv->rubberband_x1, icon_list->priv->rubberband_x2);
  rubber_rect.y = MIN (icon_list->priv->rubberband_y1, icon_list->priv->rubberband_y2);
  rubber_rect.width = ABS (icon_list->priv->rubberband_x1 - icon_list->priv->rubberband_x2) + 1;
  rubber_rect.height = ABS (icon_list->priv->rubberband_y1 - icon_list->priv->rubberband_y2) + 1;

  if (!gdk_rectangle_intersect (&rubber_rect, area, &rect))
    return;

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, rect.width, rect.height);
  gdk_pixbuf_fill (pixbuf, 0x9db8d27F);

  gdk_pixbuf_render_to_drawable_alpha (pixbuf,
				       icon_list->priv->bin_window,
				       0, 0,
				       rect.x,rect.y,
				       rect.width, rect.height,
				       GDK_PIXBUF_ALPHA_FULL,
				       0,
				       GDK_RGB_DITHER_NONE,
				       0, 0);
  g_object_unref (pixbuf);
  gc = gdk_gc_new (icon_list->priv->bin_window);
  color.red = 0x72 * 255;
  color.green = 0x7d * 255;
  color.blue = 0x97 * 255;
  gdk_gc_set_rgb_fg_color (gc, &color);
  gdk_gc_set_clip_rectangle (gc, &rect);
  gdk_draw_rectangle (icon_list->priv->bin_window,
		      gc, FALSE,
		      rubber_rect.x, rubber_rect.y,
		      rubber_rect.width - 1, rubber_rect.height - 1);
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

static void
egg_icon_list_append_typeahead_string (EggIconList     *icon_list,
				       const gchar     *string)
{
  int i;
  char *typeahead_string;

  if (strlen (string) == 0)
    return;

  for (i = 0; i < strlen (string); i++)
    {
      if (!g_ascii_isprint (string[i]))
	return;
    }

  typeahead_string = g_strconcat (icon_list->priv->typeahead_string ?
				  icon_list->priv->typeahead_string : "",
				  string, NULL);
  g_free (icon_list->priv->typeahead_string);
  icon_list->priv->typeahead_string = typeahead_string;

  egg_icon_list_select_first_matching_item (icon_list,
					    icon_list->priv->typeahead_string);
  
  g_print ("wooo: \"%s\"\n", typeahead_string);
}

/* Public API */
GtkWidget *
egg_icon_list_new (void)
{
  EggIconList *icon_list;

  icon_list = g_object_new (EGG_TYPE_ICON_LIST, NULL);

  return GTK_WIDGET (icon_list);
}

EggIconListItem *
egg_icon_list_item_new (GdkPixbuf   *icon,
			const char  *label)
{
  EggIconListItem *item;

  item = g_object_new (EGG_TYPE_ICON_LIST_ITEM, NULL);
  
  item->label = g_strdup (label);
  item->icon = g_object_ref (icon);
  
  return item;
}

void
egg_icon_list_item_set_data (EggIconListItem  *item,
			     gpointer          data)
{
  egg_icon_list_item_set_data_full (item, data, NULL);
}

void
egg_icon_list_item_set_data_full (EggIconListItem  *item,
				  gpointer          data, 
				  GDestroyNotify    destroy_notify)
{
  g_return_if_fail (GTK_IS_ICON_LIST_ITEM (item));

  g_object_set_data_full (G_OBJECT (item), ICON_LIST_ITEM_DATA, data, destroy_notify);
}

gpointer
egg_icon_list_item_get_data (EggIconListItem *item)
{
  g_return_val_if_fail (GTK_IS_ICON_LIST_ITEM (item), NULL);

  return g_object_get_data (G_OBJECT (item), ICON_LIST_ITEM_DATA);
}

void
egg_icon_list_item_set_label (EggIconListItem  *item,
			      const char       *label)
{
  g_return_if_fail (item != NULL);
  g_return_if_fail (label != NULL);

  if (strcmp (item->label, label) == 0)
    return;

  g_free (item->label);
  item->label = g_strdup (label);
  egg_icon_list_item_invalidate_size (item);

  egg_icon_list_queue_layout (item->icon_list);

  g_object_notify (G_OBJECT (item), "label");
}

G_CONST_RETURN gchar *
egg_icon_list_item_get_label (EggIconListItem *item)
{
  g_return_val_if_fail (item != NULL, NULL);

  return item->label;
}

void
egg_icon_list_item_set_icon (EggIconListItem  *item,
			     GdkPixbuf        *icon)
{
  g_return_if_fail (item != NULL);

  if (icon == item->icon)
    return;

  g_object_unref (item->icon);
  item->icon = g_object_ref (icon);

  egg_icon_list_item_invalidate_size (item);

  egg_icon_list_queue_layout (item->icon_list);
}

GdkPixbuf *
egg_icon_list_item_get_icon (EggIconListItem  *item)
{
  g_return_val_if_fail (item != NULL, NULL);

  return item->icon;
}

void
egg_icon_list_append_item (EggIconList     *icon_list,
			   EggIconListItem *item)
{
  GList *list;
  
  g_return_if_fail (GTK_IS_ICON_LIST (icon_list));
  g_return_if_fail (item != NULL);
  g_return_if_fail (item->icon_list == NULL);
  
  if (icon_list->priv->sorted)
    {
      egg_icon_list_insert_item_sorted (icon_list, item);
      return;
    }

  egg_icon_list_validate (icon_list);
  
  list = g_list_alloc ();
  item->list = list;
  item->icon_list = icon_list;
  list->data = item;
  g_object_ref (item);
  
  if (icon_list->priv->last_item)
    {
      icon_list->priv->last_item->next = list;
      list->prev = icon_list->priv->last_item;
    }
  else
      icon_list->priv->items = list;

  icon_list->priv->last_item = list;
  icon_list->priv->item_count += 1;
  
  egg_icon_list_validate (icon_list);

  g_signal_emit (icon_list, icon_list_signals[ITEM_ADDED], 0, item);
  
  egg_icon_list_queue_layout (icon_list);
}

void
egg_icon_list_prepend_item (EggIconList      *icon_list,
			    EggIconListItem  *item)
{
  GList *list;

  g_return_if_fail (GTK_IS_ICON_LIST (icon_list));
  g_return_if_fail (item != NULL);
  g_return_if_fail (item->icon_list == NULL);
  
  egg_icon_list_validate (icon_list);

  list = g_list_alloc ();
  item->list = list;
  item->icon_list = icon_list;
  list->data = item;
  g_object_ref (item);
  
  if (icon_list->priv->last_item == NULL)
    icon_list->priv->last_item = list;
  
  if (icon_list->priv->items)
      icon_list->priv->items->prev = list;
  
  list->next = icon_list->priv->items;
  icon_list->priv->items = list;
  icon_list->priv->item_count += 1;
  
  egg_icon_list_validate (icon_list);

  g_signal_emit (icon_list, icon_list_signals[ITEM_ADDED], 0, item);

  egg_icon_list_queue_layout (icon_list);

}


void
egg_icon_list_insert_item_before (EggIconList      *icon_list,
				  EggIconListItem  *sibling,
				  EggIconListItem  *item)
{
  GList *list;
  
  g_return_if_fail (GTK_IS_ICON_LIST (icon_list));
  g_return_if_fail (item != NULL);
  g_return_if_fail (item->icon_list == NULL);
  
  if (icon_list->priv->sorted)
    {
      egg_icon_list_insert_item_sorted (icon_list, item);
      return;
    }
  
  if (sibling == NULL)
    egg_icon_list_append_item (icon_list, item);
  
  egg_icon_list_validate (icon_list);

  list = g_list_alloc ();
  item->list = list;
  item->icon_list = icon_list;
  list->data = item;
  g_object_ref (item);
  
  list->prev = sibling->list->prev;
  list->next = sibling->list;
  sibling->list->prev->next = list;
  sibling->list->prev = list;

  if (sibling->list == icon_list->priv->items)
    icon_list->priv->items = list;

  icon_list->priv->item_count += 1;
  egg_icon_list_validate (icon_list);

  g_signal_emit (icon_list, icon_list_signals[ITEM_ADDED], 0, item);
  
  egg_icon_list_queue_layout (icon_list);
}

void
egg_icon_list_insert_item_after  (EggIconList      *icon_list,
				  EggIconListItem  *sibling,
				  EggIconListItem  *item)
{
  GList *list;

  g_return_if_fail (GTK_IS_ICON_LIST (icon_list));
  g_return_if_fail (item != NULL);
  g_return_if_fail (item->icon_list == NULL);
  
  if (icon_list->priv->sorted)
    {
      egg_icon_list_insert_item_sorted (icon_list, item);
      return;
    }

  if (sibling == NULL)
    {
      egg_icon_list_prepend_item (icon_list, item);
      return;
    }

  egg_icon_list_validate (icon_list);

  list = g_list_alloc ();
  item->list = list;
  item->icon_list = icon_list;
  list->data = item;
  g_object_ref (item);
  
  list->next = sibling->list->next;
  list->prev = sibling->list;
  sibling->list->next->prev = list;
  sibling->list->next = list;

  if (sibling->list == icon_list->priv->last_item)
    icon_list->priv->last_item = list;

  icon_list->priv->item_count += 1;
  egg_icon_list_validate (icon_list);
  g_signal_emit (icon_list, icon_list_signals[ITEM_ADDED], 0, item);
  
  egg_icon_list_queue_layout (icon_list);
}

void
egg_icon_list_remove_item (EggIconList      *icon_list,
			   EggIconListItem  *item)
{
  g_return_if_fail (GTK_IS_ICON_LIST (icon_list));
  g_return_if_fail (item != NULL);
  g_return_if_fail (item->icon_list == icon_list);
  
  egg_icon_list_validate (icon_list);

  if (item->list->prev)
    item->list->prev->next = item->list->next;
  if (item->list->next)
    item->list->next->prev = item->list->prev;

  if (item->list == icon_list->priv->items)
    icon_list->priv->items = item->list->next;
  if (item->list == icon_list->priv->last_item)
    icon_list->priv->last_item = item->list->prev;

  g_list_free_1 (item->list);
  item->list = NULL;
  item->icon_list = NULL;
  egg_icon_list_item_invalidate_size (item);
  
  icon_list->priv->item_count -= 1;
  egg_icon_list_validate (icon_list);

  g_signal_emit (icon_list, icon_list_signals[ITEM_REMOVED], 0, item);

  if (item->selected)
    {
      item->selected = FALSE;

      g_signal_emit (icon_list, icon_list_signals[SELECTION_CHANGED], 0);
    }

  if (icon_list->priv->cursor_item == item)
    g_error ("FIXME: Move to first focused item");

  if (icon_list->priv->last_single_clicked == item)
    icon_list->priv->last_single_clicked = NULL;
  
  g_object_unref (item);
  
  egg_icon_list_queue_layout (icon_list);
}


EggIconListItem *
egg_icon_list_get_item_at_pos (EggIconList *icon_list,
			       gint         x,
			       gint         y)
{
  GList *items;
  
  g_return_val_if_fail (GTK_IS_ICON_LIST (icon_list), NULL);

  for (items = icon_list->priv->items; items; items = items->next)
    {
      EggIconListItem *item = items->data;
      
      if (x > item->x && x < item->x + item->width &&
	  y > item->y && y < item->y + item->height)
	{
	  gint layout_x = item->x +  (item->width - item->layout_width) / 2;
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

gint
egg_icon_list_get_item_count (EggIconList *icon_list)
{
  g_return_val_if_fail (GTK_IS_ICON_LIST (icon_list), 0);

  return icon_list->priv->item_count;
}

void
egg_icon_list_foreach (EggIconList           *icon_list,
		       EggIconListForeachFunc func,
		       gpointer               data)
{
  GList *list;

  for (list = icon_list->priv->items; list; list = list->next)
    (* func) (icon_list, list->data, data);
}

void
egg_icon_list_selected_foreach (EggIconList           *icon_list,
				EggIconListForeachFunc func,
				gpointer               data)
{
  GList *list;

  for (list = icon_list->priv->items; list; list = list->next)
    {
      EggIconListItem *item = list->data;

      if (item->selected)
	(* func) (icon_list, list->data, data);
    }
}

GList *
egg_icon_list_get_selected (EggIconList  *icon_list)
{
  GList *list, *selected = NULL;

  g_return_val_if_fail (GTK_IS_ICON_LIST (icon_list), NULL);
  
  for (list = icon_list->priv->items; list; list = list->next)
    {
      EggIconListItem *item = list->data;
      
      if (item->selected)
	selected = g_list_prepend (selected, item);
    }

  return g_list_reverse (selected);
}

void
egg_icon_list_set_selection_mode (EggIconList      *icon_list,
				  GtkSelectionMode  mode)
{
  g_return_if_fail (GTK_IS_ICON_LIST (icon_list));

  if (mode == icon_list->priv->selection_mode)
    return;
  
  if (mode == GTK_SELECTION_NONE ||
      icon_list->priv->selection_mode == GTK_SELECTION_MULTIPLE)
    egg_icon_list_unselect_all (icon_list);
  
  icon_list->priv->selection_mode = mode;

  g_object_notify (G_OBJECT (icon_list), "selection_mode");
}

GtkSelectionMode
egg_icon_list_get_selection_mode (EggIconList *icon_list)
{
  g_return_val_if_fail (GTK_IS_ICON_LIST (icon_list), GTK_SELECTION_SINGLE);

  return icon_list->priv->selection_mode;
}

void
egg_icon_list_select_item (EggIconList      *icon_list,
			   EggIconListItem  *item)
{
  g_return_if_fail (GTK_IS_ICON_LIST (icon_list));
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


void
egg_icon_list_unselect_item (EggIconList      *icon_list,
			     EggIconListItem  *item)
{
  g_return_if_fail (GTK_IS_ICON_LIST (icon_list));
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

gboolean
egg_icon_list_item_is_selected (EggIconListItem *item)
{
  g_return_val_if_fail (item != NULL, FALSE);

  return item->selected;
}

void
egg_icon_list_unselect_all (EggIconList *icon_list)
{
  g_return_if_fail (GTK_IS_ICON_LIST (icon_list));

  egg_icon_list_unselect_all_internal (icon_list, TRUE);
}

void
egg_icon_list_select_all (EggIconList *icon_list)
{
  GList *items;
  gboolean dirty = FALSE;
  
  g_return_if_fail (GTK_IS_ICON_LIST (icon_list));

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

void
egg_icon_list_set_sorted (EggIconList *icon_list,
			  gboolean     sorted)
{
  g_return_if_fail (GTK_IS_ICON_LIST (icon_list));
  g_return_if_fail (icon_list->priv->sort_func != NULL);

  if (icon_list->priv->sorted == sorted)
    return;

  icon_list->priv->sorted = sorted;
  g_object_notify (G_OBJECT (icon_list), "sorted");
  
  if (icon_list->priv->sorted)
    egg_icon_list_sort (icon_list);
}

gboolean
egg_icon_list_get_sorted (EggIconList *icon_list)
{
  g_return_val_if_fail (GTK_IS_ICON_LIST (icon_list), FALSE);
  
  return icon_list->priv->sorted;
}

void
egg_icon_list_set_sort_func (EggIconList                *icon_list,
			     EggIconListItemCompareFunc  func,
			     gpointer                    data,
			     GDestroyNotify              destroy_notify)
{
  g_return_if_fail (GTK_IS_ICON_LIST (icon_list));
  g_return_if_fail (func != NULL);

  if (icon_list->priv->sort_destroy_notify &&
      icon_list->priv->sort_data)
    (* icon_list->priv->sort_destroy_notify) (icon_list->priv->sort_data);

  icon_list->priv->sort_func = func;
  icon_list->priv->sort_data = data;
  icon_list->priv->sort_destroy_notify = destroy_notify;
}

void
egg_icon_list_set_sort_order (EggIconList  *icon_list,
			      GtkSortType   order)
{
  g_return_if_fail (GTK_IS_ICON_LIST (icon_list));

  if (icon_list->priv->sort_order == order)
    return;

  icon_list->priv->sort_order = order;

  if (icon_list->priv->sorted)
    egg_icon_list_sort (icon_list);
  
  g_object_notify (G_OBJECT (icon_list), "sort_order");
}

GtkSortType
egg_icon_list_get_sort_order (EggIconList  *icon_list)
{
  g_return_val_if_fail (GTK_IS_ICON_LIST (icon_list), GTK_SORT_ASCENDING);

  return icon_list->priv->sort_order;
}

void
egg_icon_list_item_activated (EggIconList      *icon_list,
			      EggIconListItem  *item)
{
  g_signal_emit (G_OBJECT (icon_list), icon_list_signals[ITEM_ACTIVATED], 0, item);
}

GList *
egg_icon_list_get_items (EggIconList *icon_list)
{
  g_return_val_if_fail (GTK_IS_ICON_LIST (icon_list), NULL);

  return icon_list->priv->items;
}

EggIconList *
egg_icon_list_item_get_icon_list (EggIconListItem *item)
{
  g_return_val_if_fail (GTK_IS_ICON_LIST_ITEM (item), NULL);

  return item->icon_list;
}
