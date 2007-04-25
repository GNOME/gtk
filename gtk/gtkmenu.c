/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#define GTK_MENU_INTERNALS
#include <config.h>
#include <string.h> /* memset */
#include "gdk/gdkkeysyms.h"
#include "gtkaccellabel.h"
#include "gtkaccelmap.h"
#include "gtkbindings.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtktearoffmenuitem.h"
#include "gtkwindow.h"
#include "gtkhbox.h"
#include "gtkvscrollbar.h"
#include "gtksettings.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"


#define MENU_ITEM_CLASS(w)   GTK_MENU_ITEM_GET_CLASS (w)

#define DEFAULT_POPUP_DELAY    225
#define DEFAULT_POPDOWN_DELAY  1000

#define NAVIGATION_REGION_OVERSHOOT 50  /* How much the navigation region
					 * extends below the submenu
					 */

#define MENU_SCROLL_STEP1 8
#define MENU_SCROLL_STEP2 15
#define MENU_SCROLL_FAST_ZONE 8
#define MENU_SCROLL_TIMEOUT1 50
#define MENU_SCROLL_TIMEOUT2 20

#define ATTACH_INFO_KEY "gtk-menu-child-attach-info-key"
#define ATTACHED_MENUS "gtk-attached-menus"

typedef struct _GtkMenuAttachData	GtkMenuAttachData;
typedef struct _GtkMenuPrivate  	GtkMenuPrivate;

struct _GtkMenuAttachData
{
  GtkWidget *attach_widget;
  GtkMenuDetachFunc detacher;
};

struct _GtkMenuPrivate 
{
  gboolean seen_item_enter;

  gboolean have_position;
  gint x;
  gint y;

  /* info used for the table */
  guint *heights;
  gint heights_length;

  gint monitor_num;

  /* Cached layout information */
  gboolean have_layout;
  gint n_rows;
  gint n_columns;

  gchar *title;

  /* Arrow states */
  GtkStateType lower_arrow_state;
  GtkStateType upper_arrow_state;

  gboolean ignore_button_release;
  gboolean initially_pushed_in;
};

typedef struct
{
  gint left_attach;
  gint right_attach;
  gint top_attach;
  gint bottom_attach;
  gint effective_left_attach;
  gint effective_right_attach;
  gint effective_top_attach;
  gint effective_bottom_attach;
} AttachInfo;

enum {
  MOVE_SCROLL,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_TEAROFF_STATE,
  PROP_TEAROFF_TITLE
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_LEFT_ATTACH,
  CHILD_PROP_RIGHT_ATTACH,
  CHILD_PROP_TOP_ATTACH,
  CHILD_PROP_BOTTOM_ATTACH
};

static void     gtk_menu_set_property      (GObject          *object,
					    guint             prop_id,
					    const GValue     *value,
					    GParamSpec       *pspec);
static void     gtk_menu_get_property      (GObject          *object,
					    guint             prop_id,
					    GValue           *value,
					    GParamSpec       *pspec);
static void     gtk_menu_set_child_property(GtkContainer     *container,
                                            GtkWidget        *child,
                                            guint             property_id,
                                            const GValue     *value,
                                            GParamSpec       *pspec);
static void     gtk_menu_get_child_property(GtkContainer     *container,
                                            GtkWidget        *child,
                                            guint             property_id,
                                            GValue           *value,
                                            GParamSpec       *pspec);
static void     gtk_menu_destroy           (GtkObject        *object);
static void     gtk_menu_finalize          (GObject          *object);
static void     gtk_menu_realize           (GtkWidget        *widget);
static void     gtk_menu_unrealize         (GtkWidget        *widget);
static void     gtk_menu_size_request      (GtkWidget        *widget,
					    GtkRequisition   *requisition);
static void     gtk_menu_size_allocate     (GtkWidget        *widget,
					    GtkAllocation    *allocation);
static void     gtk_menu_paint             (GtkWidget        *widget,
					    GdkEventExpose   *expose);
static void     gtk_menu_show              (GtkWidget        *widget);
static gboolean gtk_menu_expose            (GtkWidget        *widget,
					    GdkEventExpose   *event);
static gboolean gtk_menu_key_press         (GtkWidget        *widget,
					    GdkEventKey      *event);
static gboolean gtk_menu_scroll            (GtkWidget        *widget,
					    GdkEventScroll   *event);
static gboolean gtk_menu_button_press      (GtkWidget        *widget,
					    GdkEventButton   *event);
static gboolean gtk_menu_button_release    (GtkWidget        *widget,
					    GdkEventButton   *event);
static gboolean gtk_menu_motion_notify     (GtkWidget        *widget,
					    GdkEventMotion   *event);
static gboolean gtk_menu_enter_notify      (GtkWidget        *widget,
					    GdkEventCrossing *event);
static gboolean gtk_menu_leave_notify      (GtkWidget        *widget,
					    GdkEventCrossing *event);
static void     gtk_menu_scroll_to         (GtkMenu          *menu,
					    gint              offset);
static void     gtk_menu_grab_notify       (GtkWidget        *widget,
					    gboolean          was_grabbed);

static void     gtk_menu_stop_scrolling         (GtkMenu  *menu);
static void     gtk_menu_remove_scroll_timeout  (GtkMenu  *menu);
static gboolean gtk_menu_scroll_timeout         (gpointer  data);
static gboolean gtk_menu_scroll_timeout_initial (gpointer  data);
static void     gtk_menu_start_scrolling        (GtkMenu  *menu);

static void     gtk_menu_scroll_item_visible (GtkMenuShell    *menu_shell,
					      GtkWidget       *menu_item);
static void     gtk_menu_select_item       (GtkMenuShell     *menu_shell,
					    GtkWidget        *menu_item);
static void     gtk_menu_real_insert       (GtkMenuShell     *menu_shell,
					    GtkWidget        *child,
					    gint              position);
static void     gtk_menu_scrollbar_changed (GtkAdjustment    *adjustment,
					    GtkMenu          *menu);
static void     gtk_menu_handle_scrolling  (GtkMenu          *menu,
					    gint	      event_x,
					    gint	      event_y,
					    gboolean          enter,
                                            gboolean          motion);
static void     gtk_menu_set_tearoff_hints (GtkMenu          *menu,
					    gint             width);
static void     gtk_menu_style_set         (GtkWidget        *widget,
					    GtkStyle         *previous_style);
static gboolean gtk_menu_focus             (GtkWidget        *widget,
					    GtkDirectionType direction);
static gint     gtk_menu_get_popup_delay   (GtkMenuShell     *menu_shell);
static void     gtk_menu_move_current      (GtkMenuShell     *menu_shell,
                                            GtkMenuDirectionType direction);
static void     gtk_menu_real_move_scroll  (GtkMenu          *menu,
					    GtkScrollType     type);

static void     gtk_menu_stop_navigating_submenu       (GtkMenu          *menu);
static gboolean gtk_menu_stop_navigating_submenu_cb    (gpointer          user_data);
static gboolean gtk_menu_navigating_submenu            (GtkMenu          *menu,
							gint              event_x,
							gint              event_y);
static void     gtk_menu_set_submenu_navigation_region (GtkMenu          *menu,
							GtkMenuItem      *menu_item,
							GdkEventCrossing *event);
 
static void gtk_menu_deactivate	    (GtkMenuShell      *menu_shell);
static void gtk_menu_show_all       (GtkWidget         *widget);
static void gtk_menu_hide_all       (GtkWidget         *widget);
static void gtk_menu_position       (GtkMenu           *menu);
static void gtk_menu_reparent       (GtkMenu           *menu, 
				     GtkWidget         *new_parent, 
				     gboolean           unrealize);
static void gtk_menu_remove         (GtkContainer      *menu,
				     GtkWidget         *widget);

static void gtk_menu_update_title   (GtkMenu           *menu);

static void       menu_grab_transfer_window_destroy (GtkMenu *menu);
static GdkWindow *menu_grab_transfer_window_get     (GtkMenu *menu);

static gboolean gtk_menu_real_can_activate_accel (GtkWidget *widget,
                                                  guint      signal_id);
static void _gtk_menu_refresh_accel_paths (GtkMenu *menu,
					   gboolean group_changed);

static const gchar	  attach_data_key[] = "gtk-menu-attach-data";

static guint menu_signals[LAST_SIGNAL] = { 0 };

static GtkMenuPrivate *
gtk_menu_get_private (GtkMenu *menu)
{
  return G_TYPE_INSTANCE_GET_PRIVATE (menu, GTK_TYPE_MENU, GtkMenuPrivate);
}

G_DEFINE_TYPE (GtkMenu, gtk_menu, GTK_TYPE_MENU_SHELL)

static void
menu_queue_resize (GtkMenu *menu)
{
  GtkMenuPrivate *priv = gtk_menu_get_private (menu);

  priv->have_layout = FALSE;
  gtk_widget_queue_resize (GTK_WIDGET (menu));
}

static AttachInfo *
get_attach_info (GtkWidget *child)
{
  GObject *object = G_OBJECT (child);
  AttachInfo *ai = g_object_get_data (object, ATTACH_INFO_KEY);

  if (!ai)
    {
      ai = g_new0 (AttachInfo, 1);
      g_object_set_data_full (object, I_(ATTACH_INFO_KEY), ai, g_free);
    }

  return ai;
}

static gboolean
is_grid_attached (AttachInfo *ai)
{
  return (ai->left_attach >= 0 &&
	  ai->right_attach >= 0 &&
	  ai->top_attach >= 0 &&
	  ai->bottom_attach >= 0);
}

static void
menu_ensure_layout (GtkMenu *menu)
{
  GtkMenuPrivate *priv = gtk_menu_get_private (menu);

  if (!priv->have_layout)
    {
      GtkMenuShell *menu_shell = GTK_MENU_SHELL (menu);
      GList *l;
      gchar *row_occupied;
      gint current_row;
      gint max_right_attach;      
      gint max_bottom_attach;

      /* Find extents of gridded portion
       */
      max_right_attach = 1;
      max_bottom_attach = 0;

      for (l = menu_shell->children; l; l = l->next)
	{
	  GtkWidget *child = l->data;
	  AttachInfo *ai = get_attach_info (child);

	  if (is_grid_attached (ai))
	    {
	      max_bottom_attach = MAX (max_bottom_attach, ai->bottom_attach);
	      max_right_attach = MAX (max_right_attach, ai->right_attach);
	    }
	}
	 
      /* Find empty rows
       */
      row_occupied = g_malloc0 (max_bottom_attach);

      for (l = menu_shell->children; l; l = l->next)
	{
	  GtkWidget *child = l->data;
	  AttachInfo *ai = get_attach_info (child);

	  if (is_grid_attached (ai))
	    {
	      gint i;

	      for (i = ai->top_attach; i < ai->bottom_attach; i++)
		row_occupied[i] = TRUE;
	    }
	}

      /* Lay non-grid-items out in those rows
       */
      current_row = 0;
      for (l = menu_shell->children; l; l = l->next)
	{
	  GtkWidget *child = l->data;
	  AttachInfo *ai = get_attach_info (child);

	  if (!is_grid_attached (ai))
	    {
	      while (current_row < max_bottom_attach && row_occupied[current_row])
		current_row++;
		
	      ai->effective_left_attach = 0;
	      ai->effective_right_attach = max_right_attach;
	      ai->effective_top_attach = current_row;
	      ai->effective_bottom_attach = current_row + 1;

	      current_row++;
	    }
	  else
	    {
	      ai->effective_left_attach = ai->left_attach;
	      ai->effective_right_attach = ai->right_attach;
	      ai->effective_top_attach = ai->top_attach;
	      ai->effective_bottom_attach = ai->bottom_attach;
	    }
	}

      g_free (row_occupied);

      priv->n_rows = MAX (current_row, max_bottom_attach);
      priv->n_columns = max_right_attach;
      priv->have_layout = TRUE;
    }
}


static gint
gtk_menu_get_n_columns (GtkMenu *menu)
{
  GtkMenuPrivate *priv = gtk_menu_get_private (menu);

  menu_ensure_layout (menu);

  return priv->n_columns;
}

static gint
gtk_menu_get_n_rows (GtkMenu *menu)
{
  GtkMenuPrivate *priv = gtk_menu_get_private (menu);

  menu_ensure_layout (menu);

  return priv->n_rows;
}

static void
get_effective_child_attach (GtkWidget *child,
			    int       *l,
			    int       *r,
			    int       *t,
			    int       *b)
{
  GtkMenu *menu = GTK_MENU (child->parent);
  AttachInfo *ai;
  
  menu_ensure_layout (menu);

  ai = get_attach_info (child);

  if (l)
    *l = ai->effective_left_attach;
  if (r)
    *r = ai->effective_right_attach;
  if (t)
    *t = ai->effective_top_attach;
  if (b)
    *b = ai->effective_bottom_attach;

}

static void
gtk_menu_class_init (GtkMenuClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);
  GtkMenuShellClass *menu_shell_class = GTK_MENU_SHELL_CLASS (class);
  GtkBindingSet *binding_set;
  
  gobject_class->finalize = gtk_menu_finalize;
  gobject_class->set_property = gtk_menu_set_property;
  gobject_class->get_property = gtk_menu_get_property;

  object_class->destroy = gtk_menu_destroy;
  
  widget_class->realize = gtk_menu_realize;
  widget_class->unrealize = gtk_menu_unrealize;
  widget_class->size_request = gtk_menu_size_request;
  widget_class->size_allocate = gtk_menu_size_allocate;
  widget_class->show = gtk_menu_show;
  widget_class->expose_event = gtk_menu_expose;
  widget_class->scroll_event = gtk_menu_scroll;
  widget_class->key_press_event = gtk_menu_key_press;
  widget_class->button_press_event = gtk_menu_button_press;
  widget_class->button_release_event = gtk_menu_button_release;
  widget_class->motion_notify_event = gtk_menu_motion_notify;
  widget_class->show_all = gtk_menu_show_all;
  widget_class->hide_all = gtk_menu_hide_all;
  widget_class->enter_notify_event = gtk_menu_enter_notify;
  widget_class->leave_notify_event = gtk_menu_leave_notify;
  widget_class->style_set = gtk_menu_style_set;
  widget_class->focus = gtk_menu_focus;
  widget_class->can_activate_accel = gtk_menu_real_can_activate_accel;
  widget_class->grab_notify = gtk_menu_grab_notify;

  container_class->remove = gtk_menu_remove;
  container_class->get_child_property = gtk_menu_get_child_property;
  container_class->set_child_property = gtk_menu_set_child_property;
  
  menu_shell_class->submenu_placement = GTK_LEFT_RIGHT;
  menu_shell_class->deactivate = gtk_menu_deactivate;
  menu_shell_class->select_item = gtk_menu_select_item;
  menu_shell_class->insert = gtk_menu_real_insert;
  menu_shell_class->get_popup_delay = gtk_menu_get_popup_delay;
  menu_shell_class->move_current = gtk_menu_move_current;

  menu_signals[MOVE_SCROLL] =
    _gtk_binding_signal_new (I_("move_scroll"),
			     G_OBJECT_CLASS_TYPE (object_class),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			     G_CALLBACK (gtk_menu_real_move_scroll),
			     NULL, NULL,
			     _gtk_marshal_VOID__ENUM,
			     G_TYPE_NONE, 1,
			     GTK_TYPE_SCROLL_TYPE);
  
  g_object_class_install_property (gobject_class,
                                   PROP_TEAROFF_TITLE,
                                   g_param_spec_string ("tearoff-title",
                                                        P_("Tearoff Title"),
                                                        P_("A title that may be displayed by the window manager when this menu is torn-off"),
                                                        "",
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkMenu:tearoff-state:
   *
   * A boolean that indicates whether the menu is torn-off.
   *
   * Since: 2.6
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TEAROFF_STATE,
                                   g_param_spec_boolean ("tearoff-state",
							 P_("Tearoff State"),
							 P_("A boolean that indicates whether the menu is torn-off"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("vertical-padding",
							     P_("Vertical Padding"),
							     P_("Extra space at the top and bottom of the menu"),
							     0,
							     G_MAXINT,
							     1,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("horizontal-padding",
                                                             P_("Horizontal Padding"),
                                                             P_("Extra space at the left and right edges of the menu"),
                                                             0,
                                                             G_MAXINT,
                                                             0,
                                                             GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("vertical-offset",
							     P_("Vertical Offset"),
							     P_("When the menu is a submenu, position it this number of pixels offset vertically"),
							     G_MININT,
							     G_MAXINT,
							     0,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("horizontal-offset",
							     P_("Horizontal Offset"),
							     P_("When the menu is a submenu, position it this number of pixels offset horizontally"),
							     G_MININT,
							     G_MAXINT,
							     -2,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("double-arrows",
                                                                 P_("Double Arrows"),
                                                                 P_("When scrolling, always show both arrows."),
                                                                 TRUE,
                                                                 GTK_PARAM_READABLE));


 gtk_container_class_install_child_property (container_class,
                                             CHILD_PROP_LEFT_ATTACH,
					      g_param_spec_int ("left-attach",
                                                               P_("Left Attach"),
                                                               P_("The column number to attach the left side of the child to"),
								-1, INT_MAX, -1,
                                                               GTK_PARAM_READWRITE));

 gtk_container_class_install_child_property (container_class,
                                             CHILD_PROP_RIGHT_ATTACH,
					      g_param_spec_int ("right-attach",
                                                               P_("Right Attach"),
                                                               P_("The column number to attach the right side of the child to"),
								-1, INT_MAX, -1,
                                                               GTK_PARAM_READWRITE));

 gtk_container_class_install_child_property (container_class,
                                             CHILD_PROP_TOP_ATTACH,
					      g_param_spec_int ("top-attach",
                                                               P_("Top Attach"),
                                                               P_("The row number to attach the top of the child to"),
								-1, INT_MAX, -1,
                                                               GTK_PARAM_READWRITE));

 gtk_container_class_install_child_property (container_class,
                                             CHILD_PROP_BOTTOM_ATTACH,
					      g_param_spec_int ("bottom-attach",
                                                               P_("Bottom Attach"),
                                                               P_("The row number to attach the bottom of the child to"),
								-1, INT_MAX, -1,
                                                               GTK_PARAM_READWRITE));

  binding_set = gtk_binding_set_by_class (class);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Up, 0,
				I_("move_current"), 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PREV);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Up, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PREV);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Down, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_NEXT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Down, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_NEXT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Left, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PARENT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Left, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PARENT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Right, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_CHILD);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Right, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_CHILD);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Home, 0,
				"move_scroll", 1,
				GTK_TYPE_SCROLL_TYPE,
				GTK_SCROLL_START);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Home, 0,
				"move_scroll", 1,
				GTK_TYPE_SCROLL_TYPE,
				GTK_SCROLL_START);
  gtk_binding_entry_add_signal (binding_set,
				GDK_End, 0,
				"move_scroll", 1,
				GTK_TYPE_SCROLL_TYPE,
				GTK_SCROLL_END);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_End, 0,
				"move_scroll", 1,
				GTK_TYPE_SCROLL_TYPE,
				GTK_SCROLL_END);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Page_Up, 0,
				"move_scroll", 1,
				GTK_TYPE_SCROLL_TYPE,
				GTK_SCROLL_PAGE_UP);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Page_Up, 0,
				"move_scroll", 1,
				GTK_TYPE_SCROLL_TYPE,
				GTK_SCROLL_PAGE_UP);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Page_Down, 0,
				"move_scroll", 1,
				GTK_TYPE_SCROLL_TYPE,
				GTK_SCROLL_PAGE_DOWN);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Page_Down, 0,
				"move_scroll", 1,
				GTK_TYPE_SCROLL_TYPE,
				GTK_SCROLL_PAGE_DOWN);

  gtk_settings_install_property (g_param_spec_boolean ("gtk-can-change-accels",
						       P_("Can change accelerators"),
						       P_("Whether menu accelerators can be changed by pressing a key over the menu item"),
						       FALSE,
						       GTK_PARAM_READWRITE));

  gtk_settings_install_property (g_param_spec_int ("gtk-menu-popup-delay",
						   P_("Delay before submenus appear"),
						   P_("Minimum time the pointer must stay over a menu item before the submenu appear"),
						   0,
						   G_MAXINT,
						   DEFAULT_POPUP_DELAY,
						   GTK_PARAM_READWRITE));

  gtk_settings_install_property (g_param_spec_int ("gtk-menu-popdown-delay",
						   P_("Delay before hiding a submenu"),
						   P_("The time before hiding a submenu when the pointer is moving towards the submenu"),
						   0,
						   G_MAXINT,
						   DEFAULT_POPDOWN_DELAY,
						   GTK_PARAM_READWRITE));

  g_type_class_add_private (gobject_class, sizeof (GtkMenuPrivate));
}


static void 
gtk_menu_set_property (GObject      *object,
		       guint         prop_id,
		       const GValue *value,
		       GParamSpec   *pspec)
{
  GtkMenu *menu;
  
  menu = GTK_MENU (object);
  
  switch (prop_id)
    {
    case PROP_TEAROFF_STATE:
      gtk_menu_set_tearoff_state (menu, g_value_get_boolean (value));
      break;
    case PROP_TEAROFF_TITLE:
      gtk_menu_set_title (menu, g_value_get_string (value));
      break;	  
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_menu_get_property (GObject     *object,
		       guint        prop_id,
		       GValue      *value,
		       GParamSpec  *pspec)
{
  GtkMenu *menu;
  
  menu = GTK_MENU (object);
  
  switch (prop_id)
    {
    case PROP_TEAROFF_STATE:
      g_value_set_boolean (value, gtk_menu_get_tearoff_state (menu));
      break;
    case PROP_TEAROFF_TITLE:
      g_value_set_string (value, gtk_menu_get_title (menu));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_set_child_property (GtkContainer *container,
                             GtkWidget    *child,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkMenu *menu = GTK_MENU (container);
  AttachInfo *ai = get_attach_info (child);

  switch (property_id)
    {
    case CHILD_PROP_LEFT_ATTACH:
      ai->left_attach = g_value_get_int (value);
      break;
    case CHILD_PROP_RIGHT_ATTACH:
      ai->right_attach = g_value_get_int (value);
      break;
    case CHILD_PROP_TOP_ATTACH:
      ai->top_attach = g_value_get_int (value);	
      break;
    case CHILD_PROP_BOTTOM_ATTACH:
      ai->bottom_attach = g_value_get_int (value);
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      return;
    }

  menu_queue_resize (menu);
}

static void
gtk_menu_get_child_property (GtkContainer *container,
                             GtkWidget    *child,
                             guint         property_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  AttachInfo *ai = get_attach_info (child);

  switch (property_id)
    {
    case CHILD_PROP_LEFT_ATTACH:
      g_value_set_int (value, ai->left_attach);
      break;
    case CHILD_PROP_RIGHT_ATTACH:
      g_value_set_int (value, ai->right_attach);
      break;
    case CHILD_PROP_TOP_ATTACH:
      g_value_set_int (value, ai->top_attach);
      break;
    case CHILD_PROP_BOTTOM_ATTACH:
      g_value_set_int (value, ai->bottom_attach);
      break;
      
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      return;
    }
}

static gboolean
gtk_menu_window_event (GtkWidget *window,
		       GdkEvent  *event,
		       GtkWidget *menu)
{
  gboolean handled = FALSE;

  g_object_ref (window);
  g_object_ref (menu);

  switch (event->type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      handled = gtk_widget_event (menu, event);
      break;
    default:
      break;
    }

  g_object_unref (window);
  g_object_unref (menu);

  return handled;
}

static void
gtk_menu_window_size_request (GtkWidget      *window,
			      GtkRequisition *requisition,
			      GtkMenu        *menu)
{
  GtkMenuPrivate *private = gtk_menu_get_private (menu);

  if (private->have_position)
    {
      GdkScreen *screen = gtk_widget_get_screen (window);
      GdkRectangle monitor;
      
      gdk_screen_get_monitor_geometry (screen, private->monitor_num, &monitor);

      if (private->y + requisition->height > monitor.y + monitor.height)
	requisition->height = monitor.y + monitor.height - private->y;

      if (private->y < monitor.y)
	requisition->height -= monitor.y - private->y;
    }
}

static void
gtk_menu_init (GtkMenu *menu)
{
  GtkMenuPrivate *priv = gtk_menu_get_private (menu);

  menu->parent_menu_item = NULL;
  menu->old_active_menu_item = NULL;
  menu->accel_group = NULL;
  menu->position_func = NULL;
  menu->position_func_data = NULL;
  menu->toggle_size = 0;

  menu->toplevel = g_object_connect (g_object_new (GTK_TYPE_WINDOW,
						   "type", GTK_WINDOW_POPUP,
						   "child", menu,
						   NULL),
				     "signal::event", gtk_menu_window_event, menu,
				     "signal::size_request", gtk_menu_window_size_request, menu,
				     "signal::destroy", gtk_widget_destroyed, &menu->toplevel,
				     NULL);
  gtk_window_set_resizable (GTK_WINDOW (menu->toplevel), FALSE);
  gtk_window_set_mnemonic_modifier (GTK_WINDOW (menu->toplevel), 0);

  /* Refloat the menu, so that reference counting for the menu isn't
   * affected by it being a child of the toplevel
   */
  g_object_force_floating (G_OBJECT (menu));
  menu->needs_destruction_ref_count = TRUE;

  menu->view_window = NULL;
  menu->bin_window = NULL;

  menu->scroll_offset = 0;
  menu->scroll_step  = 0;
  menu->timeout_id = 0;
  menu->scroll_fast = FALSE;
  
  menu->tearoff_window = NULL;
  menu->tearoff_hbox = NULL;
  menu->torn_off = FALSE;
  menu->tearoff_active = FALSE;
  menu->tearoff_adjustment = NULL;
  menu->tearoff_scrollbar = NULL;

  menu->upper_arrow_visible = FALSE;
  menu->lower_arrow_visible = FALSE;
  menu->upper_arrow_prelight = FALSE;
  menu->lower_arrow_prelight = FALSE;

  priv->upper_arrow_state = GTK_STATE_NORMAL;
  priv->lower_arrow_state = GTK_STATE_NORMAL;

  priv->have_layout = FALSE;
}

static void
gtk_menu_destroy (GtkObject *object)
{
  GtkMenu *menu;
  GtkMenuAttachData *data;
  GtkMenuPrivate *priv; 

  g_return_if_fail (GTK_IS_MENU (object));

  menu = GTK_MENU (object);

  gtk_menu_remove_scroll_timeout (menu);
  
  data = g_object_get_data (G_OBJECT (object), attach_data_key);
  if (data)
    gtk_menu_detach (menu);
  
  gtk_menu_stop_navigating_submenu (menu);

  if (menu->old_active_menu_item)
    {
      g_object_unref (menu->old_active_menu_item);
      menu->old_active_menu_item = NULL;
    }

  /* Add back the reference count for being a child */
  if (menu->needs_destruction_ref_count)
    {
      menu->needs_destruction_ref_count = FALSE;
      g_object_ref (object);
    }
  
  if (menu->accel_group)
    {
      g_object_unref (menu->accel_group);
      menu->accel_group = NULL;
    }

  if (menu->toplevel)
    gtk_widget_destroy (menu->toplevel);

  if (menu->tearoff_window)
    gtk_widget_destroy (menu->tearoff_window);

  priv = gtk_menu_get_private (menu);

  if (priv->heights)
    {
      g_free (priv->heights);
      priv->heights = NULL;
    }

  if (priv->title)
    {
      g_free (priv->title);
      priv->title = NULL;
    }

  GTK_OBJECT_CLASS (gtk_menu_parent_class)->destroy (object);
}

static void
gtk_menu_finalize (GObject *object)
{
  GtkMenu *menu = GTK_MENU (object);

  g_free (menu->accel_path);
  
  G_OBJECT_CLASS (gtk_menu_parent_class)->finalize (object);
}

static void
menu_change_screen (GtkMenu   *menu,
		    GdkScreen *new_screen)
{
  GtkMenuPrivate *private = gtk_menu_get_private (menu);

  if (gtk_widget_has_screen (GTK_WIDGET (menu)))
    {
      if (new_screen == gtk_widget_get_screen (GTK_WIDGET (menu)))
	return;
    }

  if (menu->torn_off)
    {
      gtk_window_set_screen (GTK_WINDOW (menu->tearoff_window), new_screen);
      gtk_menu_position (menu);
    }

  gtk_window_set_screen (GTK_WINDOW (menu->toplevel), new_screen);
  private->monitor_num = -1;
}

static void
attach_widget_screen_changed (GtkWidget *attach_widget,
			      GdkScreen *previous_screen,
			      GtkMenu   *menu)
{
  if (gtk_widget_has_screen (attach_widget) &&
      !g_object_get_data (G_OBJECT (menu), "gtk-menu-explicit-screen"))
    {
      menu_change_screen (menu, gtk_widget_get_screen (attach_widget));
    }
}

void
gtk_menu_attach_to_widget (GtkMenu	       *menu,
			   GtkWidget	       *attach_widget,
			   GtkMenuDetachFunc	detacher)
{
  GtkMenuAttachData *data;
  GList *list;
  
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GTK_IS_WIDGET (attach_widget));
  
  /* keep this function in sync with gtk_widget_set_parent()
   */
  
  data = g_object_get_data (G_OBJECT (menu), attach_data_key);
  if (data)
    {
      g_warning ("gtk_menu_attach_to_widget(): menu already attached to %s",
		 g_type_name (G_TYPE_FROM_INSTANCE (data->attach_widget)));
     return;
    }
  
  g_object_ref_sink (menu);
  
  data = g_new (GtkMenuAttachData, 1);
  data->attach_widget = attach_widget;
  
  g_signal_connect (attach_widget, "screen_changed",
		    G_CALLBACK (attach_widget_screen_changed), menu);
  attach_widget_screen_changed (attach_widget, NULL, menu);
  
  data->detacher = detacher;
  g_object_set_data (G_OBJECT (menu), I_(attach_data_key), data);
  list = g_object_steal_data (G_OBJECT (attach_widget), ATTACHED_MENUS);
  if (!g_list_find (list, menu))
    {
      list = g_list_prepend (list, menu);
    }
  g_object_set_data_full (G_OBJECT (attach_widget), I_(ATTACHED_MENUS), list, (GtkDestroyNotify) g_list_free);
  
  if (GTK_WIDGET_STATE (menu) != GTK_STATE_NORMAL)
    gtk_widget_set_state (GTK_WIDGET (menu), GTK_STATE_NORMAL);
  
  /* we don't need to set the style here, since
   * we are a toplevel widget.
   */

  /* Fallback title for menu comes from attach widget */
  gtk_menu_update_title (menu);
}

GtkWidget*
gtk_menu_get_attach_widget (GtkMenu *menu)
{
  GtkMenuAttachData *data;
  
  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);
  
  data = g_object_get_data (G_OBJECT (menu), attach_data_key);
  if (data)
    return data->attach_widget;
  return NULL;
}

void
gtk_menu_detach (GtkMenu *menu)
{
  GtkMenuAttachData *data;
  GList *list;
  
  g_return_if_fail (GTK_IS_MENU (menu));
  
  /* keep this function in sync with gtk_widget_unparent()
   */
  data = g_object_get_data (G_OBJECT (menu), attach_data_key);
  if (!data)
    {
      g_warning ("gtk_menu_detach(): menu is not attached");
      return;
    }
  g_object_set_data (G_OBJECT (menu), I_(attach_data_key), NULL);
  
  g_signal_handlers_disconnect_by_func (data->attach_widget,
					(gpointer) attach_widget_screen_changed,
					menu);

  if (data->detacher)
    data->detacher (data->attach_widget, menu);
  list = g_object_steal_data (G_OBJECT (data->attach_widget), ATTACHED_MENUS);
  list = g_list_remove (list, menu);
  if (list)
    g_object_set_data_full (G_OBJECT (data->attach_widget), I_(ATTACHED_MENUS), list, (GtkDestroyNotify) g_list_free);
  else
    g_object_set_data (G_OBJECT (data->attach_widget), I_(ATTACHED_MENUS), NULL);
  
  if (GTK_WIDGET_REALIZED (menu))
    gtk_widget_unrealize (GTK_WIDGET (menu));
  
  g_free (data);
  
  /* Fallback title for menu comes from attach widget */
  gtk_menu_update_title (menu);

  g_object_unref (menu);
}

static void
gtk_menu_remove (GtkContainer *container,
		 GtkWidget    *widget)
{
  GtkMenu *menu;

  g_return_if_fail (GTK_IS_MENU (container));
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));

  menu = GTK_MENU (container);

  /* Clear out old_active_menu_item if it matches the item we are removing
   */
  if (menu->old_active_menu_item == widget)
    {
      g_object_unref (menu->old_active_menu_item);
      menu->old_active_menu_item = NULL;
    }

  GTK_CONTAINER_CLASS (gtk_menu_parent_class)->remove (container, widget);
  g_object_set_data (G_OBJECT (widget), I_(ATTACH_INFO_KEY), NULL);

  menu_queue_resize (menu);
}

GtkWidget*
gtk_menu_new (void)
{
  return g_object_new (GTK_TYPE_MENU, NULL);
}

static void
gtk_menu_real_insert (GtkMenuShell *menu_shell,
		      GtkWidget    *child,
		      gint          position)
{
  GtkMenu *menu = GTK_MENU (menu_shell);
  AttachInfo *ai = get_attach_info (child);

  ai->left_attach = -1;
  ai->right_attach = -1;
  ai->top_attach = -1;
  ai->bottom_attach = -1;

  if (GTK_WIDGET_REALIZED (menu_shell))
    gtk_widget_set_parent_window (child, menu->bin_window);

  GTK_MENU_SHELL_CLASS (gtk_menu_parent_class)->insert (menu_shell, child, position);

  menu_queue_resize (menu);
}

static void
gtk_menu_tearoff_bg_copy (GtkMenu *menu)
{
  GtkWidget *widget;
  gint width, height;

  widget = GTK_WIDGET (menu);

  if (menu->torn_off)
    {
      GdkPixmap *pixmap;
      GdkGC *gc;
      GdkGCValues gc_values;

      menu->tearoff_active = FALSE;
      menu->saved_scroll_offset = menu->scroll_offset;
      
      gc_values.subwindow_mode = GDK_INCLUDE_INFERIORS;
      gc = gdk_gc_new_with_values (widget->window,
				   &gc_values, GDK_GC_SUBWINDOW);
      
      gdk_drawable_get_size (menu->tearoff_window->window, &width, &height);
      
      pixmap = gdk_pixmap_new (menu->tearoff_window->window,
			       width,
			       height,
			       -1);

      gdk_draw_drawable (pixmap, gc,
			 menu->tearoff_window->window,
			 0, 0, 0, 0, -1, -1);
      g_object_unref (gc);

      gtk_widget_set_size_request (menu->tearoff_window,
				   width,
				   height);

      gdk_window_set_back_pixmap (menu->tearoff_window->window, pixmap, FALSE);
      g_object_unref (pixmap);
    }
}

static gboolean
popup_grab_on_window (GdkWindow *window,
		      guint32    activate_time,
		      gboolean   grab_keyboard)
{
  if ((gdk_pointer_grab (window, TRUE,
			 GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			 GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
			 GDK_POINTER_MOTION_MASK,
			 NULL, NULL, activate_time) == 0))
    {
      if (!grab_keyboard ||
	  gdk_keyboard_grab (window, TRUE,
			     activate_time) == 0)
	return TRUE;
      else
	{
	  gdk_display_pointer_ungrab (gdk_drawable_get_display (window),
				      activate_time);
	  return FALSE;
	}
    }

  return FALSE;
}

/**
 * gtk_menu_popup:
 * @menu: a #GtkMenu.
 * @parent_menu_shell: the menu shell containing the triggering menu item, or %NULL
 * @parent_menu_item: the menu item whose activation triggered the popup, or %NULL
 * @func: a user supplied function used to position the menu, or %NULL
 * @data: user supplied data to be passed to @func.
 * @button: the mouse button which was pressed to initiate the event.
 * @activate_time: the time at which the activation event occurred.
 *
 * Displays a menu and makes it available for selection.  Applications can use
 * this function to display context-sensitive menus, and will typically supply
 * %NULL for the @parent_menu_shell, @parent_menu_item, @func and @data 
 * parameters. The default menu positioning function will position the menu
 * at the current mouse cursor position.
 *
 * The @button parameter should be the mouse button pressed to initiate
 * the menu popup. If the menu popup was initiated by something other than
 * a mouse button press, such as a mouse button release or a keypress,
 * @button should be 0.
 *
 * The @activate_time parameter should be the time stamp of the event that
 * initiated the popup. If such an event is not available, use
 * gtk_get_current_event_time() instead.
 *
 */
void
gtk_menu_popup (GtkMenu		    *menu,
		GtkWidget	    *parent_menu_shell,
		GtkWidget	    *parent_menu_item,
		GtkMenuPositionFunc  func,
		gpointer	     data,
		guint		     button,
		guint32		     activate_time)
{
  GtkWidget *widget;
  GtkWidget *xgrab_shell;
  GtkWidget *parent;
  GdkEvent *current_event;
  GtkMenuShell *menu_shell;
  gboolean grab_keyboard;
  GtkMenuPrivate *priv;
  GtkWidget *parent_toplevel;

  g_return_if_fail (GTK_IS_MENU (menu));

  widget = GTK_WIDGET (menu);
  menu_shell = GTK_MENU_SHELL (menu);
  priv = gtk_menu_get_private (menu);

  menu_shell->parent_menu_shell = parent_menu_shell;

  priv->seen_item_enter = FALSE;
  
  /* Find the last viewable ancestor, and make an X grab on it
   */
  parent = GTK_WIDGET (menu);
  xgrab_shell = NULL;
  while (parent)
    {
      gboolean viewable = TRUE;
      GtkWidget *tmp = parent;
      
      while (tmp)
	{
	  if (!GTK_WIDGET_MAPPED (tmp))
	    {
	      viewable = FALSE;
	      break;
	    }
	  tmp = tmp->parent;
	}
      
      if (viewable)
	xgrab_shell = parent;
      
      parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
    }

  /* We want to receive events generated when we map the menu; unfortunately,
   * since there is probably already an implicit grab in place from the
   * button that the user used to pop up the menu, we won't receive then --
   * in particular, the EnterNotify when the menu pops up under the pointer.
   *
   * If we are grabbing on a parent menu shell, no problem; just grab on
   * that menu shell first before popping up the window with owner_events = TRUE.
   *
   * When grabbing on the menu itself, things get more convuluted - we
   * we do an explicit grab on a specially created window with
   * owner_events = TRUE, which we override further down with a grab
   * on the menu. (We can't grab on the menu until it is mapped; we
   * probably could just leave the grab on the other window, with a
   * little reorganization of the code in gtkmenu*).
   */
  grab_keyboard = gtk_menu_shell_get_take_focus (menu_shell);
  gtk_window_set_accept_focus (GTK_WINDOW (menu->toplevel), grab_keyboard);

  if (xgrab_shell && xgrab_shell != widget)
    {
      if (popup_grab_on_window (xgrab_shell->window, activate_time, grab_keyboard))
	GTK_MENU_SHELL (xgrab_shell)->have_xgrab = TRUE;
    }
  else
    {
      GdkWindow *transfer_window;

      xgrab_shell = widget;
      transfer_window = menu_grab_transfer_window_get (menu);
      if (popup_grab_on_window (transfer_window, activate_time, grab_keyboard))
	GTK_MENU_SHELL (xgrab_shell)->have_xgrab = TRUE;
    }

  if (!GTK_MENU_SHELL (xgrab_shell)->have_xgrab)
    {
      /* We failed to make our pointer/keyboard grab. Rather than leaving the user
       * with a stuck up window, we just abort here. Presumably the user will
       * try again.
       */
      menu_shell->parent_menu_shell = NULL;
      menu_grab_transfer_window_destroy (menu);
      return;
    }

  menu_shell->active = TRUE;
  menu_shell->button = button;

  /* If we are popping up the menu from something other than, a button
   * press then, as a heuristic, we ignore enter events for the menu
   * until we get a MOTION_NOTIFY.  
   */

  current_event = gtk_get_current_event ();
  if (current_event)
    {
      if ((current_event->type != GDK_BUTTON_PRESS) &&
	  (current_event->type != GDK_ENTER_NOTIFY))
	menu_shell->ignore_enter = TRUE;

      gdk_event_free (current_event);
    }
  else
    menu_shell->ignore_enter = TRUE;

  if (menu->torn_off)
    {
      gtk_menu_tearoff_bg_copy (menu);

      gtk_menu_reparent (menu, menu->toplevel, FALSE);
    }

  parent_toplevel = NULL;
  if (parent_menu_shell) 
    parent_toplevel = gtk_widget_get_toplevel (parent_menu_shell);
  else if (!g_object_get_data (G_OBJECT (menu), "gtk-menu-explicit-screen"))
    {
      GtkWidget *attach_widget = gtk_menu_get_attach_widget (menu);
      if (attach_widget)
	parent_toplevel = gtk_widget_get_toplevel (attach_widget);
    }

  /* Set transient for to get the right window group and parent relationship */
  if (parent_toplevel && GTK_IS_WINDOW (parent_toplevel))
    gtk_window_set_transient_for (GTK_WINDOW (menu->toplevel),
				  GTK_WINDOW (parent_toplevel));
  
  menu->parent_menu_item = parent_menu_item;
  menu->position_func = func;
  menu->position_func_data = data;
  menu_shell->activate_time = activate_time;

  /* We need to show the menu here rather in the init function because
   * code expects to be able to tell if the menu is onscreen by
   * looking at the GTK_WIDGET_VISIBLE (menu)
   */
  gtk_widget_show (GTK_WIDGET (menu));

  /* Position the menu, possibly changing the size request
   */
  gtk_menu_position (menu);

  /* Compute the size of the toplevel and realize it so we
   * can scroll correctly.
   */
  {
    GtkRequisition tmp_request;
    GtkAllocation tmp_allocation = { 0, };

    gtk_widget_size_request (menu->toplevel, &tmp_request);
    
    tmp_allocation.width = tmp_request.width;
    tmp_allocation.height = tmp_request.height;

    gtk_widget_size_allocate (menu->toplevel, &tmp_allocation);
    
    gtk_widget_realize (GTK_WIDGET (menu));
  }

  gtk_menu_scroll_to (menu, menu->scroll_offset);

  /* Once everything is set up correctly, map the toplevel window on
     the screen.
   */
  gtk_widget_show (menu->toplevel);

  if (xgrab_shell == widget)
    popup_grab_on_window (widget->window, activate_time, grab_keyboard); /* Should always succeed */
  gtk_grab_add (GTK_WIDGET (menu));
}

void
gtk_menu_popdown (GtkMenu *menu)
{
  GtkMenuPrivate *private;
  GtkMenuShell *menu_shell;

  g_return_if_fail (GTK_IS_MENU (menu));
  
  menu_shell = GTK_MENU_SHELL (menu);
  private = gtk_menu_get_private (menu);

  menu_shell->parent_menu_shell = NULL;
  menu_shell->active = FALSE;
  menu_shell->ignore_enter = FALSE;

  private->have_position = FALSE;

  gtk_menu_stop_scrolling (menu);
  
  gtk_menu_stop_navigating_submenu (menu);
  
  if (menu_shell->active_menu_item)
    {
      if (menu->old_active_menu_item)
	g_object_unref (menu->old_active_menu_item);
      menu->old_active_menu_item = menu_shell->active_menu_item;
      g_object_ref (menu->old_active_menu_item);
    }

  gtk_menu_shell_deselect (menu_shell);
  
  /* The X Grab, if present, will automatically be removed when we hide
   * the window */
  gtk_widget_hide (menu->toplevel);
  gtk_window_set_transient_for (GTK_WINDOW (menu->toplevel), NULL);

  if (menu->torn_off)
    {
      gtk_widget_set_size_request (menu->tearoff_window, -1, -1);
      
      if (GTK_BIN (menu->toplevel)->child) 
	{
	  gtk_menu_reparent (menu, menu->tearoff_hbox, TRUE);
	} 
      else
	{
	  /* We popped up the menu from the tearoff, so we need to 
	   * release the grab - we aren't actually hiding the menu.
	   */
	  if (menu_shell->have_xgrab)
	    {
	      GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (menu));
	      
	      gdk_display_pointer_ungrab (display, GDK_CURRENT_TIME);
	      gdk_display_keyboard_ungrab (display, GDK_CURRENT_TIME);
	    }
	}

      /* gtk_menu_popdown is called each time a menu item is selected from
       * a torn off menu. Only scroll back to the saved position if the
       * non-tearoff menu was popped down.
       */
      if (!menu->tearoff_active)
	gtk_menu_scroll_to (menu, menu->saved_scroll_offset);
      menu->tearoff_active = TRUE;
    }
  else
    gtk_widget_hide (GTK_WIDGET (menu));

  menu_shell->have_xgrab = FALSE;
  gtk_grab_remove (GTK_WIDGET (menu));

  menu_grab_transfer_window_destroy (menu);
}

GtkWidget*
gtk_menu_get_active (GtkMenu *menu)
{
  GtkWidget *child;
  GList *children;
  
  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);
  
  if (!menu->old_active_menu_item)
    {
      child = NULL;
      children = GTK_MENU_SHELL (menu)->children;
      
      while (children)
	{
	  child = children->data;
	  children = children->next;
	  
	  if (GTK_BIN (child)->child)
	    break;
	  child = NULL;
	}
      
      menu->old_active_menu_item = child;
      if (menu->old_active_menu_item)
	g_object_ref (menu->old_active_menu_item);
    }
  
  return menu->old_active_menu_item;
}

void
gtk_menu_set_active (GtkMenu *menu,
		     guint    index)
{
  GtkWidget *child;
  GList *tmp_list;
  
  g_return_if_fail (GTK_IS_MENU (menu));
  
  tmp_list = g_list_nth (GTK_MENU_SHELL (menu)->children, index);
  if (tmp_list)
    {
      child = tmp_list->data;
      if (GTK_BIN (child)->child)
	{
	  if (menu->old_active_menu_item)
	    g_object_unref (menu->old_active_menu_item);
	  menu->old_active_menu_item = child;
	  g_object_ref (menu->old_active_menu_item);
	}
    }
}

void
gtk_menu_set_accel_group (GtkMenu	*menu,
			  GtkAccelGroup *accel_group)
{
  g_return_if_fail (GTK_IS_MENU (menu));
  
  if (menu->accel_group != accel_group)
    {
      if (menu->accel_group)
	g_object_unref (menu->accel_group);
      menu->accel_group = accel_group;
      if (menu->accel_group)
	g_object_ref (menu->accel_group);
      _gtk_menu_refresh_accel_paths (menu, TRUE);
    }
}

GtkAccelGroup*
gtk_menu_get_accel_group (GtkMenu *menu)
{
  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

  return menu->accel_group;
}

static gboolean
gtk_menu_real_can_activate_accel (GtkWidget *widget,
                                  guint      signal_id)
{
  /* Menu items chain here to figure whether they can activate their
   * accelerators.  Unlike ordinary widgets, menus allow accel
   * activation even if invisible since that's the usual case for
   * submenus/popup-menus. however, the state of the attach widget
   * affects the "activeness" of the menu.
   */
  GtkWidget *awidget = gtk_menu_get_attach_widget (GTK_MENU (widget));

  if (awidget)
    return gtk_widget_can_activate_accel (awidget, signal_id);
  else
    return GTK_WIDGET_IS_SENSITIVE (widget);
}

/**
 * gtk_menu_set_accel_path
 * @menu:       a valid #GtkMenu
 * @accel_path: a valid accelerator path
 *
 * Sets an accelerator path for this menu from which accelerator paths
 * for its immediate children, its menu items, can be constructed.
 * The main purpose of this function is to spare the programmer the
 * inconvenience of having to call gtk_menu_item_set_accel_path() on
 * each menu item that should support runtime user changable accelerators.
 * Instead, by just calling gtk_menu_set_accel_path() on their parent,
 * each menu item of this menu, that contains a label describing its purpose,
 * automatically gets an accel path assigned. For example, a menu containing
 * menu items "New" and "Exit", will, after 
 * <literal>gtk_menu_set_accel_path (menu, "&lt;Gnumeric-Sheet&gt;/File");</literal>
 * has been called, assign its items the accel paths:
 * <literal>"&lt;Gnumeric-Sheet&gt;/File/New"</literal> and <literal>"&lt;Gnumeric-Sheet&gt;/File/Exit"</literal>.
 * Assigning accel paths to menu items then enables the user to change
 * their accelerators at runtime. More details about accelerator paths
 * and their default setups can be found at gtk_accel_map_add_entry().
 */
void
gtk_menu_set_accel_path (GtkMenu     *menu,
			 const gchar *accel_path)
{
  g_return_if_fail (GTK_IS_MENU (menu));
  if (accel_path)
    g_return_if_fail (accel_path[0] == '<' && strchr (accel_path, '/')); /* simplistic check */

  g_free (menu->accel_path);
  menu->accel_path = g_strdup (accel_path);
  if (menu->accel_path)
    _gtk_menu_refresh_accel_paths (menu, FALSE);
}

typedef struct {
  GtkMenu *menu;
  gboolean group_changed;
} AccelPropagation;

static void
refresh_accel_paths_foreach (GtkWidget *widget,
			     gpointer   data)
{
  AccelPropagation *prop = data;

  if (GTK_IS_MENU_ITEM (widget))	/* should always be true */
    _gtk_menu_item_refresh_accel_path (GTK_MENU_ITEM (widget),
				       prop->menu->accel_path,
				       prop->menu->accel_group,
				       prop->group_changed);
}

static void
_gtk_menu_refresh_accel_paths (GtkMenu *menu,
			       gboolean group_changed)
{
  g_return_if_fail (GTK_IS_MENU (menu));
      
  if (menu->accel_path && menu->accel_group)
    {
      AccelPropagation prop;

      prop.menu = menu;
      prop.group_changed = group_changed;
      gtk_container_foreach (GTK_CONTAINER (menu),
			     refresh_accel_paths_foreach,
			     &prop);
    }
}

void
gtk_menu_reposition (GtkMenu *menu)
{
  g_return_if_fail (GTK_IS_MENU (menu));

  if (GTK_WIDGET_DRAWABLE (menu) && !menu->torn_off)
    gtk_menu_position (menu);
}

static void
gtk_menu_scrollbar_changed (GtkAdjustment *adjustment,
			    GtkMenu       *menu)
{
  g_return_if_fail (GTK_IS_MENU (menu));

  if (adjustment->value != menu->scroll_offset)
    gtk_menu_scroll_to (menu, adjustment->value);
}

static void
gtk_menu_set_tearoff_hints (GtkMenu *menu,
			    gint     width)
{
  GdkGeometry geometry_hints;
  
  if (!menu->tearoff_window)
    return;

  if (GTK_WIDGET_VISIBLE (menu->tearoff_scrollbar))
    {
      gtk_widget_size_request (menu->tearoff_scrollbar, NULL);
      width += menu->tearoff_scrollbar->requisition.width;
    }

  geometry_hints.min_width = width;
  geometry_hints.max_width = width;
    
  geometry_hints.min_height = 0;
  geometry_hints.max_height = GTK_WIDGET (menu)->requisition.height;

  gtk_window_set_geometry_hints (GTK_WINDOW (menu->tearoff_window),
				 NULL,
				 &geometry_hints,
				 GDK_HINT_MAX_SIZE|GDK_HINT_MIN_SIZE);
}

static void
gtk_menu_update_title (GtkMenu *menu)
{
  if (menu->tearoff_window)
    {
      const gchar *title;
      GtkWidget *attach_widget;

      title = gtk_menu_get_title (menu);
      if (!title)
	{
	  attach_widget = gtk_menu_get_attach_widget (menu);
	  if (GTK_IS_MENU_ITEM (attach_widget))
	    {
	      GtkWidget *child = GTK_BIN (attach_widget)->child;
	      if (GTK_IS_LABEL (child))
		title = gtk_label_get_text (GTK_LABEL (child));
	    }
	}
      
      if (title)
	gtk_window_set_title (GTK_WINDOW (menu->tearoff_window), title);
    }
}

static GtkWidget*
gtk_menu_get_toplevel (GtkWidget *menu)
{
  GtkWidget *attach, *toplevel;

  attach = gtk_menu_get_attach_widget (GTK_MENU (menu));

  if (GTK_IS_MENU_ITEM (attach))
    attach = attach->parent;

  if (GTK_IS_MENU (attach))
    return gtk_menu_get_toplevel (attach);
  else if (GTK_IS_WIDGET (attach))
    {
      toplevel = gtk_widget_get_toplevel (attach);
      if (GTK_WIDGET_TOPLEVEL (toplevel)) 
	return toplevel;
    }

  return NULL;
}

static void
tearoff_window_destroyed (GtkWidget *widget,
			  GtkMenu   *menu)
{
  gtk_menu_set_tearoff_state (menu, FALSE);
}

void       
gtk_menu_set_tearoff_state (GtkMenu  *menu,
			    gboolean  torn_off)
{
  gint width, height;
  
  g_return_if_fail (GTK_IS_MENU (menu));

  if (menu->torn_off != torn_off)
    {
      menu->torn_off = torn_off;
      menu->tearoff_active = torn_off;
      
      if (menu->torn_off)
	{
	  if (GTK_WIDGET_VISIBLE (menu))
	    gtk_menu_popdown (menu);

	  if (!menu->tearoff_window)
	    {
	      GtkWidget *toplevel;

	      menu->tearoff_window = gtk_widget_new (GTK_TYPE_WINDOW,
						     "type", GTK_WINDOW_TOPLEVEL,
						     "screen", gtk_widget_get_screen (menu->toplevel),
						     "app-paintable", TRUE,
						     NULL);

	      gtk_window_set_type_hint (GTK_WINDOW (menu->tearoff_window),
					GDK_WINDOW_TYPE_HINT_MENU);
	      gtk_window_set_mnemonic_modifier (GTK_WINDOW (menu->tearoff_window), 0);
	      g_signal_connect (menu->tearoff_window, "destroy",
				G_CALLBACK (tearoff_window_destroyed), menu);
	      g_signal_connect (menu->tearoff_window, "event",
				G_CALLBACK (gtk_menu_window_event), menu);

	      gtk_menu_update_title (menu);

	      gtk_widget_realize (menu->tearoff_window);

	      toplevel = gtk_menu_get_toplevel (GTK_WIDGET (menu));
	      if (toplevel != NULL)
		gtk_window_set_transient_for (GTK_WINDOW (menu->tearoff_window),
					      GTK_WINDOW (toplevel));
	      
	      menu->tearoff_hbox = gtk_hbox_new (FALSE, FALSE);
	      gtk_container_add (GTK_CONTAINER (menu->tearoff_window), menu->tearoff_hbox);

	      gdk_drawable_get_size (GTK_WIDGET (menu)->window, &width, &height);
	      menu->tearoff_adjustment =
		GTK_ADJUSTMENT (gtk_adjustment_new (0,
						    0,
						    GTK_WIDGET (menu)->requisition.height,
						    MENU_SCROLL_STEP2,
						    height/2,
						    height));
	      g_object_connect (menu->tearoff_adjustment,
				"signal::value_changed", gtk_menu_scrollbar_changed, menu,
				NULL);
	      menu->tearoff_scrollbar = gtk_vscrollbar_new (menu->tearoff_adjustment);

	      gtk_box_pack_end (GTK_BOX (menu->tearoff_hbox),
				menu->tearoff_scrollbar,
				FALSE, FALSE, 0);
	      
	      if (menu->tearoff_adjustment->upper > height)
		gtk_widget_show (menu->tearoff_scrollbar);
	      
	      gtk_widget_show (menu->tearoff_hbox);
	    }
	  
	  gtk_menu_reparent (menu, menu->tearoff_hbox, FALSE);

	  gdk_drawable_get_size (GTK_WIDGET (menu)->window, &width, NULL);

	  /* Update menu->requisition
	   */
	  gtk_widget_size_request (GTK_WIDGET (menu), NULL);
  
	  gtk_menu_set_tearoff_hints (menu, width);
	    
	  gtk_widget_realize (menu->tearoff_window);
	  gtk_menu_position (menu);
	  
	  gtk_widget_show (GTK_WIDGET (menu));
	  gtk_widget_show (menu->tearoff_window);

	  gtk_menu_scroll_to (menu, 0);

	}
      else
	{
	  gtk_widget_hide (GTK_WIDGET (menu));
	  gtk_widget_hide (menu->tearoff_window);
	  if (GTK_IS_CONTAINER (menu->toplevel))
	    gtk_menu_reparent (menu, menu->toplevel, FALSE);
	  gtk_widget_destroy (menu->tearoff_window);
	  
	  menu->tearoff_window = NULL;
	  menu->tearoff_hbox = NULL;
	  menu->tearoff_scrollbar = NULL;
	  menu->tearoff_adjustment = NULL;
	}

      g_object_notify (G_OBJECT (menu), "tearoff-state");
    }
}

/**
 * gtk_menu_get_tearoff_state:
 * @menu: a #GtkMenu
 *
 * Returns whether the menu is torn off. See
 * gtk_menu_set_tearoff_state ().
 *
 * Return value: %TRUE if the menu is currently torn off.
 **/
gboolean
gtk_menu_get_tearoff_state (GtkMenu *menu)
{
  g_return_val_if_fail (GTK_IS_MENU (menu), FALSE);

  return menu->torn_off;
}

/**
 * gtk_menu_set_title:
 * @menu: a #GtkMenu
 * @title: a string containing the title for the menu.
 * 
 * Sets the title string for the menu.  The title is displayed when the menu
 * is shown as a tearoff menu.  If @title is %NULL, the menu will see if it is
 * attached to a parent menu item, and if so it will try to use the same text as
 * that menu item's label.
 **/
void
gtk_menu_set_title (GtkMenu     *menu,
		    const gchar *title)
{
  GtkMenuPrivate *priv;
  char *old_title;

  g_return_if_fail (GTK_IS_MENU (menu));

  priv = gtk_menu_get_private (menu);

  old_title = priv->title;
  priv->title = g_strdup (title);
  g_free (old_title);
       
  gtk_menu_update_title (menu);
  g_object_notify (G_OBJECT (menu), "tearoff-title");
}

/**
 * gtk_menu_get_title:
 * @menu: a #GtkMenu
 *
 * Returns the title of the menu. See gtk_menu_set_title().
 *
 * Return value: the title of the menu, or %NULL if the menu has no
 * title set on it. This string is owned by the widget and should
 * not be modified or freed.
 **/
G_CONST_RETURN gchar *
gtk_menu_get_title (GtkMenu *menu)
{
  GtkMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

  priv = gtk_menu_get_private (menu);

  return priv->title;
}

void
gtk_menu_reorder_child (GtkMenu   *menu,
                        GtkWidget *child,
                        gint       position)
{
  GtkMenuShell *menu_shell;

  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GTK_IS_MENU_ITEM (child));

  menu_shell = GTK_MENU_SHELL (menu);

  if (g_list_find (menu_shell->children, child))
    {   
      menu_shell->children = g_list_remove (menu_shell->children, child);
      menu_shell->children = g_list_insert (menu_shell->children, child, position);

      menu_queue_resize (menu);
    }   
}

static void
gtk_menu_style_set (GtkWidget *widget,
		    GtkStyle  *previous_style)
{
  if (GTK_WIDGET_REALIZED (widget))
    {
      GtkMenu *menu = GTK_MENU (widget);
      
      gtk_style_set_background (widget->style, menu->bin_window, GTK_STATE_NORMAL);
      gtk_style_set_background (widget->style, menu->view_window, GTK_STATE_NORMAL);
      gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
    }
}

static void
gtk_menu_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;
  GtkMenu *menu;
  GtkWidget *child;
  GList *children;
  guint vertical_padding;
  guint horizontal_padding;
  guint scroll_arrow_height;

  g_return_if_fail (GTK_IS_MENU (widget));

  menu = GTK_MENU (widget);
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  
  attributes.event_mask = gtk_widget_get_events (widget);

  attributes.event_mask |= (GDK_EXPOSURE_MASK | GDK_KEY_PRESS_MASK |
			    GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK );
  
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);
  
  border_width = GTK_CONTAINER (widget)->border_width;

  gtk_widget_style_get (GTK_WIDGET (menu),
			"vertical-padding", &vertical_padding,
                        "horizontal-padding", &horizontal_padding,
                        "scroll-arrow-vlength", &scroll_arrow_height,
			NULL);

  attributes.x = border_width + widget->style->xthickness + horizontal_padding;
  attributes.y = border_width + widget->style->ythickness + vertical_padding;
  attributes.width = MAX (1, widget->allocation.width - attributes.x * 2);
  attributes.height = MAX (1, widget->allocation.height - attributes.y * 2);

  if (menu->upper_arrow_visible)
    {
      attributes.y += scroll_arrow_height;
      attributes.height -= scroll_arrow_height;
    }

  if (menu->lower_arrow_visible)
    attributes.height -= scroll_arrow_height;

  menu->view_window = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (menu->view_window, menu);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = MAX (1, widget->allocation.width - (border_width + widget->style->xthickness + horizontal_padding) * 2);
  attributes.height = MAX (1, widget->requisition.height - (border_width + widget->style->ythickness + vertical_padding) * 2);
  
  menu->bin_window = gdk_window_new (menu->view_window, &attributes, attributes_mask);
  gdk_window_set_user_data (menu->bin_window, menu);

  children = GTK_MENU_SHELL (menu)->children;
  while (children)
    {
      child = children->data;
      children = children->next;
	  
      gtk_widget_set_parent_window (child, menu->bin_window);
    }
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, menu->bin_window, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, menu->view_window, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  if (GTK_MENU_SHELL (widget)->active_menu_item)
    gtk_menu_scroll_item_visible (GTK_MENU_SHELL (widget),
				  GTK_MENU_SHELL (widget)->active_menu_item);

  gdk_window_show (menu->bin_window);
  gdk_window_show (menu->view_window);
}

static gboolean 
gtk_menu_focus (GtkWidget       *widget,
                GtkDirectionType direction)
{
  /*
   * A menu or its menu items cannot have focus
   */
  return FALSE;
}

/* See notes in gtk_menu_popup() for information about the "grab transfer window"
 */
static GdkWindow *
menu_grab_transfer_window_get (GtkMenu *menu)
{
  GdkWindow *window = g_object_get_data (G_OBJECT (menu), "gtk-menu-transfer-window");
  if (!window)
    {
      GdkWindowAttr attributes;
      gint attributes_mask;
      
      attributes.x = -100;
      attributes.y = -100;
      attributes.width = 10;
      attributes.height = 10;
      attributes.window_type = GDK_WINDOW_TEMP;
      attributes.wclass = GDK_INPUT_ONLY;
      attributes.override_redirect = TRUE;
      attributes.event_mask = 0;

      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;
      
      window = gdk_window_new (gtk_widget_get_root_window (GTK_WIDGET (menu)),
			       &attributes, attributes_mask);
      gdk_window_set_user_data (window, menu);

      gdk_window_show (window);

      g_object_set_data (G_OBJECT (menu), I_("gtk-menu-transfer-window"), window);
    }

  return window;
}

static void
menu_grab_transfer_window_destroy (GtkMenu *menu)
{
  GdkWindow *window = g_object_get_data (G_OBJECT (menu), "gtk-menu-transfer-window");
  if (window)
    {
      gdk_window_set_user_data (window, NULL);
      gdk_window_destroy (window);
      g_object_set_data (G_OBJECT (menu), I_("gtk-menu-transfer-window"), NULL);
    }
}

static void
gtk_menu_unrealize (GtkWidget *widget)
{
  GtkMenu *menu;

  g_return_if_fail (GTK_IS_MENU (widget));

  menu = GTK_MENU (widget);

  menu_grab_transfer_window_destroy (menu);

  gdk_window_set_user_data (menu->view_window, NULL);
  gdk_window_destroy (menu->view_window);
  menu->view_window = NULL;

  gdk_window_set_user_data (menu->bin_window, NULL);
  gdk_window_destroy (menu->bin_window);
  menu->bin_window = NULL;

  (* GTK_WIDGET_CLASS (gtk_menu_parent_class)->unrealize) (widget);
}

static void
gtk_menu_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  gint i;
  GtkMenu *menu;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;
  guint max_toggle_size;
  guint max_accel_width;
  guint vertical_padding;
  guint horizontal_padding;
  GtkRequisition child_requisition;
  GtkMenuPrivate *priv;
  
  g_return_if_fail (GTK_IS_MENU (widget));
  g_return_if_fail (requisition != NULL);
  
  menu = GTK_MENU (widget);
  menu_shell = GTK_MENU_SHELL (widget);
  priv = gtk_menu_get_private (menu);
  
  requisition->width = 0;
  requisition->height = 0;
  
  max_toggle_size = 0;
  max_accel_width = 0;
  
  g_free (priv->heights);
  priv->heights = g_new0 (guint, gtk_menu_get_n_rows (menu));
  priv->heights_length = gtk_menu_get_n_rows (menu);

  children = menu_shell->children;
  while (children)
    {
      gint part;
      gint toggle_size;
      gint l, r, t, b;

      child = children->data;
      children = children->next;
      
      if (! GTK_WIDGET_VISIBLE (child))
        continue;

      get_effective_child_attach (child, &l, &r, &t, &b);

      /* It's important to size_request the child
       * before doing the toggle size request, in
       * case the toggle size request depends on the size
       * request of a child of the child (e.g. for ImageMenuItem)
       */

       GTK_MENU_ITEM (child)->show_submenu_indicator = TRUE;
       gtk_widget_size_request (child, &child_requisition);

       gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (child), &toggle_size);
       max_toggle_size = MAX (max_toggle_size, toggle_size);
       max_accel_width = MAX (max_accel_width,
                              GTK_MENU_ITEM (child)->accelerator_width);

       part = child_requisition.width / (r - l);
       requisition->width = MAX (requisition->width, part);

       part = MAX (child_requisition.height, toggle_size) / (b - t);
       priv->heights[t] = MAX (priv->heights[t], part);
    }

  for (i = 0; i < gtk_menu_get_n_rows (menu); i++)
    requisition->height += priv->heights[i];

  requisition->width += max_toggle_size + max_accel_width;
  requisition->width *= gtk_menu_get_n_columns (menu);

  gtk_widget_style_get (GTK_WIDGET (menu),
			"vertical-padding", &vertical_padding,
                        "horizontal-padding", &horizontal_padding,
			NULL);

  requisition->width += (GTK_CONTAINER (menu)->border_width + horizontal_padding +
			 widget->style->xthickness) * 2;
  requisition->height += (GTK_CONTAINER (menu)->border_width + vertical_padding +
			  widget->style->ythickness) * 2;
  
  menu->toggle_size = max_toggle_size;

  /* Don't resize the tearoff if it is not active, because it won't redraw (it is only a background pixmap).
   */
  if (menu->tearoff_active)
    gtk_menu_set_tearoff_hints (menu, requisition->width);
}

static void
gtk_menu_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GtkMenu *menu;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GtkAllocation child_allocation;
  GtkRequisition child_requisition;
  GtkMenuPrivate *priv;
  GList *children;
  gint x, y;
  gint width, height;
  guint vertical_padding;
  guint horizontal_padding;
  gint scroll_arrow_height;
  
  g_return_if_fail (GTK_IS_MENU (widget));
  g_return_if_fail (allocation != NULL);
  
  menu = GTK_MENU (widget);
  menu_shell = GTK_MENU_SHELL (widget);
  priv = gtk_menu_get_private (menu);

  widget->allocation = *allocation;
  gtk_widget_get_child_requisition (GTK_WIDGET (menu), &child_requisition);

  gtk_widget_style_get (GTK_WIDGET (menu),
			"vertical-padding", &vertical_padding,
                        "horizontal-padding", &horizontal_padding,
                        "scroll-arrow-vlength", &scroll_arrow_height,
			NULL);

  x = GTK_CONTAINER (menu)->border_width + widget->style->xthickness + horizontal_padding;
  y = GTK_CONTAINER (menu)->border_width + widget->style->ythickness + vertical_padding;

  width = MAX (1, allocation->width - x * 2);
  height = MAX (1, allocation->height - y * 2);

  child_requisition.width -= x * 2;
  child_requisition.height -= y * 2;

  if (menu_shell->active)
    gtk_menu_scroll_to (menu, menu->scroll_offset);

  if (menu->upper_arrow_visible && !menu->tearoff_active)
    {
      y += scroll_arrow_height;
      height -= scroll_arrow_height;
    }

  if (menu->lower_arrow_visible && !menu->tearoff_active)
    height -= scroll_arrow_height;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gdk_window_move_resize (menu->view_window,
			      x,
			      y,
			      width,
			      height);
    }

  if (menu_shell->children)
    {
      gint base_width = width / gtk_menu_get_n_columns (menu);

      children = menu_shell->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_WIDGET_VISIBLE (child))
	    {
              gint i;
	      gint l, r, t, b;

	      get_effective_child_attach (child, &l, &r, &t, &b);

              if (gtk_widget_get_direction (GTK_WIDGET (menu)) == GTK_TEXT_DIR_RTL)
                {
                  guint tmp;
		  tmp = gtk_menu_get_n_columns (menu) - l;
		  l = gtk_menu_get_n_columns (menu) - r;
                  r = tmp;
                }

              child_allocation.width = (r - l) * base_width;
              child_allocation.height = 0;
              child_allocation.x = l * base_width;
              child_allocation.y = 0;

              for (i = 0; i < b; i++)
                {
                  if (i < t)
                    child_allocation.y += priv->heights[i];
                  else
                    child_allocation.height += priv->heights[i];
                }

	      gtk_menu_item_toggle_size_allocate (GTK_MENU_ITEM (child),
						  menu->toggle_size);

	      gtk_widget_size_allocate (child, &child_allocation);
	      gtk_widget_queue_draw (child);
	    }
	}
      
      /* Resize the item window */
      if (GTK_WIDGET_REALIZED (widget))
	{
          gint i;
          gint width, height;

          height = 0;
	  for (i = 0; i < gtk_menu_get_n_rows (menu); i++)
            height += priv->heights[i];

	  width = gtk_menu_get_n_columns (menu) * base_width;
	  gdk_window_resize (menu->bin_window, width, height);
	}

      if (menu->tearoff_active)
	{
	  if (allocation->height >= widget->requisition.height)
	    {
	      if (GTK_WIDGET_VISIBLE (menu->tearoff_scrollbar))
		{
		  gtk_widget_hide (menu->tearoff_scrollbar);
		  gtk_menu_set_tearoff_hints (menu, allocation->width);

		  gtk_menu_scroll_to (menu, 0);
		}
	    }
	  else
	    {
	      menu->tearoff_adjustment->upper = widget->requisition.height;
	      menu->tearoff_adjustment->page_size = allocation->height;
	      
	      if (menu->tearoff_adjustment->value + menu->tearoff_adjustment->page_size >
		  menu->tearoff_adjustment->upper)
		{
		  gint value;
		  value = menu->tearoff_adjustment->upper - menu->tearoff_adjustment->page_size;
		  if (value < 0)
		    value = 0;
		  gtk_menu_scroll_to (menu, value);
		}
	      
	      gtk_adjustment_changed (menu->tearoff_adjustment);
	      
	      if (!GTK_WIDGET_VISIBLE (menu->tearoff_scrollbar))
		{
		  gtk_widget_show (menu->tearoff_scrollbar);
		  gtk_menu_set_tearoff_hints (menu, allocation->width);
		}
	    }
	}
    }
}

static void
gtk_menu_paint (GtkWidget      *widget,
		GdkEventExpose *event)
{
  GtkMenu *menu;
  GtkMenuPrivate *priv;
  gint width, height;
  gint border_x, border_y;
  guint vertical_padding;
  guint horizontal_padding;
  gint scroll_arrow_height;
  
  g_return_if_fail (GTK_IS_MENU (widget));

  menu = GTK_MENU (widget);
  priv = gtk_menu_get_private (menu);

  gtk_widget_style_get (GTK_WIDGET (menu),
			"vertical-padding", &vertical_padding,
                        "horizontal-padding", &horizontal_padding,
                        "scroll-arrow-vlength", &scroll_arrow_height,
                        NULL);

  border_x = GTK_CONTAINER (widget)->border_width + widget->style->xthickness + horizontal_padding;
  border_y = GTK_CONTAINER (widget)->border_width + widget->style->ythickness + vertical_padding;
  gdk_drawable_get_size (widget->window, &width, &height);

  if (event->window == widget->window)
    {
      gint arrow_space = scroll_arrow_height - 2 * widget->style->ythickness;
      gint arrow_size = 0.7 * arrow_space;

      gtk_paint_box (widget->style,
		     widget->window,
		     GTK_STATE_NORMAL,
		     GTK_SHADOW_OUT,
		     NULL, widget, "menu",
		     0, 0, -1, -1);

      if (menu->upper_arrow_visible && !menu->tearoff_active)
	{
	  gtk_paint_box (widget->style,
			 widget->window,
			 priv->upper_arrow_state,
			 GTK_SHADOW_OUT,
			 NULL, widget, "menu",
			 border_x,
			 border_y,
			 width - 2 * border_x,
			 scroll_arrow_height);

	  gtk_paint_arrow (widget->style,
			   widget->window,
			   priv->upper_arrow_state,
			   GTK_SHADOW_OUT,
			   NULL, widget, "menu_scroll_arrow_up",
			   GTK_ARROW_UP,
			   TRUE,
			   (width - arrow_size ) / 2,
			   border_y + widget->style->ythickness + (arrow_space - arrow_size)/2,
			   arrow_size, arrow_size);
	}

      if (menu->lower_arrow_visible && !menu->tearoff_active)
	{
	  gtk_paint_box (widget->style,
			 widget->window,
			 priv->lower_arrow_state,
			 GTK_SHADOW_OUT,
			 NULL, widget, "menu",
			 border_x,
			 height - border_y - scroll_arrow_height,
			 width - 2*border_x,
			 scroll_arrow_height);

	  gtk_paint_arrow (widget->style,
			   widget->window,
			   priv->lower_arrow_state,
			   GTK_SHADOW_OUT,
			   NULL, widget, "menu_scroll_arrow_down",
			   GTK_ARROW_DOWN,
			   TRUE,
			   (width - arrow_size) / 2,
			   height - border_y - scroll_arrow_height +
			      widget->style->ythickness + (arrow_space - arrow_size)/2,
			   arrow_size, arrow_size);
	}
    }
  else if (event->window == menu->bin_window)
    {
      gint y = -border_y + menu->scroll_offset;

      if (menu->upper_arrow_visible && !menu->tearoff_active)
        y -= scroll_arrow_height;

      gtk_paint_box (widget->style,
		     menu->bin_window,
		     GTK_STATE_NORMAL,
		     GTK_SHADOW_OUT,
		     NULL, widget, "menu",
		     - border_x, y,
		     width, height);
    }
}

static gboolean
gtk_menu_expose (GtkWidget	*widget,
		 GdkEventExpose *event)
{
  g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_menu_paint (widget, event);
      
      (* GTK_WIDGET_CLASS (gtk_menu_parent_class)->expose_event) (widget, event);
    }
  
  return FALSE;
}

static void
gtk_menu_show (GtkWidget *widget)
{
  GtkMenu *menu = GTK_MENU (widget);

  _gtk_menu_refresh_accel_paths (menu, FALSE);

  GTK_WIDGET_CLASS (gtk_menu_parent_class)->show (widget);
}

static gboolean
gtk_menu_button_scroll (GtkWidget      *widget,
                        GdkEventButton *event)
{
  if (GTK_IS_MENU (widget))
    {
      GtkMenu *menu = GTK_MENU (widget);

      if (menu->upper_arrow_prelight || menu->lower_arrow_prelight)
        {
          GtkSettings *settings = gtk_widget_get_settings (widget);
          gboolean     touchscreen_mode;

          g_object_get (G_OBJECT (settings),
                        "gtk-touchscreen-mode", &touchscreen_mode,
                        NULL);

          if (touchscreen_mode)
            gtk_menu_handle_scrolling (menu,
                                       event->x_root, event->y_root,
                                       event->type == GDK_BUTTON_PRESS,
                                       FALSE);

          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gtk_menu_button_press (GtkWidget      *widget,
                       GdkEventButton *event)
{
  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  /* Don't pop down the menu for presses over scroll arrows
   */
  if (gtk_menu_button_scroll (widget, event))
    return TRUE;

  return GTK_WIDGET_CLASS (gtk_menu_parent_class)->button_press_event (widget, event);
}

static gboolean
gtk_menu_button_release (GtkWidget      *widget,
			 GdkEventButton *event)
{
  if (GTK_IS_MENU (widget))
    {
      GtkMenuPrivate *priv = gtk_menu_get_private (GTK_MENU (widget));

      if (priv->ignore_button_release)
        {
          priv->ignore_button_release = FALSE;
          return FALSE;
        }
    }

  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

  /* Don't pop down the menu for releases over scroll arrows
   */
  if (gtk_menu_button_scroll (widget, event))
    return TRUE;

  return GTK_WIDGET_CLASS (gtk_menu_parent_class)->button_release_event (widget, event);
}

static const gchar *
get_accel_path (GtkWidget *menu_item,
		gboolean  *locked)
{
  const gchar *path;
  GtkWidget *label;
  GClosure *accel_closure;
  GtkAccelGroup *accel_group;    

  path = _gtk_widget_get_accel_path (menu_item, locked);
  if (!path)
    {
      path = GTK_MENU_ITEM (menu_item)->accel_path;
      
      if (locked)
	{
	  *locked = TRUE;

	  label = GTK_BIN (menu_item)->child;
	  
	  if (GTK_IS_ACCEL_LABEL (label))
	    {
	      g_object_get (label, 
			    "accel-closure", &accel_closure, 
			    NULL);
	      if (accel_closure)
		{
		  accel_group = gtk_accel_group_from_accel_closure (accel_closure);
		  
		  *locked = accel_group->lock_count > 0;
		}
	    }
	}
    }

  return path;
}

static gboolean
gtk_menu_key_press (GtkWidget	*widget,
		    GdkEventKey *event)
{
  GtkMenuShell *menu_shell;
  GtkMenu *menu;
  gboolean delete = FALSE;
  gboolean can_change_accels;
  gchar *accel = NULL;
  guint accel_key, accel_mods;
  GdkModifierType consumed_modifiers;
  GdkDisplay *display;
  
  g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
      
  menu_shell = GTK_MENU_SHELL (widget);
  menu = GTK_MENU (widget);
  
  gtk_menu_stop_navigating_submenu (menu);

  if (GTK_WIDGET_CLASS (gtk_menu_parent_class)->key_press_event (widget, event))
    return TRUE;

  display = gtk_widget_get_display (widget);
    
  g_object_get (gtk_widget_get_settings (widget),
                "gtk-menu-bar-accel", &accel,
		"gtk-can-change-accels", &can_change_accels,
                NULL);

  if (accel && *accel)
    {
      guint keyval = 0;
      GdkModifierType mods = 0;
      gboolean handled = FALSE;
      
      gtk_accelerator_parse (accel, &keyval, &mods);

      if (keyval == 0)
        g_warning ("Failed to parse menu bar accelerator '%s'\n", accel);

      /* FIXME this is wrong, needs to be in the global accel resolution
       * thing, to properly consider i18n etc., but that probably requires
       * AccelGroup changes etc.
       */
      if (event->keyval == keyval &&
          (mods & event->state) == mods)
	gtk_menu_shell_cancel (menu_shell);

      g_free (accel);

      if (handled)
        return TRUE;
    }
  
  switch (event->keyval)
    {
    case GDK_Delete:
    case GDK_KP_Delete:
    case GDK_BackSpace:
      delete = TRUE;
      break;
    default:
      break;
    }

  /* Figure out what modifiers went into determining the key symbol */
  gdk_keymap_translate_keyboard_state (gdk_keymap_get_for_display (display),
				       event->hardware_keycode, event->state, event->group,
				       NULL, NULL, NULL, &consumed_modifiers);

  accel_key = gdk_keyval_to_lower (event->keyval);
  accel_mods = event->state & gtk_accelerator_get_default_mod_mask () & ~consumed_modifiers;

  /* If lowercasing affects the keysym, then we need to include SHIFT in the modifiers,
   * We re-upper case when we match against the keyval, but display and save in caseless form.
   */
  if (accel_key != event->keyval)
    accel_mods |= GDK_SHIFT_MASK;
  
  /* Modify the accelerators */
  if (can_change_accels &&
      menu_shell->active_menu_item &&
      GTK_BIN (menu_shell->active_menu_item)->child &&			/* no separators */
      GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu == NULL &&	/* no submenus */
      (delete || gtk_accelerator_valid (accel_key, accel_mods)))
    {
      GtkWidget *menu_item = menu_shell->active_menu_item;
      gboolean locked, replace_accels = TRUE;
      const gchar *path;

      path = get_accel_path (menu_item, &locked);
      if (!path || locked)
	{
	  /* can't change accelerators on menu_items without paths
	   * (basically, those items are accelerator-locked).
	   */
	  /* g_print("item has no path or is locked, menu prefix: %s\n", menu->accel_path); */
	  gdk_display_beep (display);
	}
      else
	{
	  gboolean changed;

	  /* For the keys that act to delete the current setting, we delete
	   * the current setting if there is one, otherwise, we set the
	   * key as the accelerator.
	   */
	  if (delete)
	    {
	      GtkAccelKey key;
	      
	      if (gtk_accel_map_lookup_entry (path, &key) &&
		  (key.accel_key || key.accel_mods))
		{
		  accel_key = 0;
		  accel_mods = 0;
		}
	    }
	  changed = gtk_accel_map_change_entry (path, accel_key, accel_mods, replace_accels);

	  if (!changed)
	    {
	      /* we failed, probably because this key is in use and
	       * locked already
	       */
	      /* g_print("failed to change\n"); */
	      gdk_display_beep (display);
	    }
	}
    }
  
  return TRUE;
}

static gboolean
check_threshold (GtkWidget *widget,
		 int start_x, int start_y,
		 int x, int y)
{
#define THRESHOLD 8
  
  return
    ABS (start_x - x) > THRESHOLD  ||
    ABS (start_y - y) > THRESHOLD;
}

static gboolean
definitely_within_item (GtkWidget *widget, 
			int x,
			int y)
{
  GdkWindow *window = GTK_MENU_ITEM (widget)->event_window;
  int w, h;

  gdk_drawable_get_size (window, &w, &h);
  
  return
    check_threshold (widget, 0, 0, x, y) &&
    check_threshold (widget, w - 1, 0, x, y) &&
    check_threshold (widget, w - 1, h - 1, x, y) &&
    check_threshold (widget, 0, h - 1, x, y);
}

static gboolean
gtk_menu_motion_notify  (GtkWidget	   *widget,
			 GdkEventMotion    *event)
{
  GtkWidget *menu_item;
  GtkMenu *menu;
  GtkMenuShell *menu_shell;

  gboolean need_enter;

  if (GTK_IS_MENU (widget))
    {
      GtkMenuPrivate *priv = gtk_menu_get_private (GTK_MENU (widget));

      if (priv->ignore_button_release)
        priv->ignore_button_release = FALSE;

      gtk_menu_handle_scrolling (GTK_MENU (widget), event->x_root, event->y_root,
                                 TRUE, TRUE);
    }

  /* We received the event for one of two reasons:
   *
   * a) We are the active menu, and did gtk_grab_add()
   * b) The widget is a child of ours, and the event was propagated
   *
   * Since for computation of navigation regions, we want the menu which
   * is the parent of the menu item, for a), we need to find that menu,
   * which may be different from 'widget'.
   */
  menu_item = gtk_get_event_widget ((GdkEvent*) event);
  if (!GTK_IS_MENU_ITEM (menu_item) ||
      !GTK_IS_MENU (menu_item->parent))
    return FALSE;

  menu_shell = GTK_MENU_SHELL (menu_item->parent);
  menu = GTK_MENU (menu_shell);

  if (definitely_within_item (menu_item, event->x, event->y))
    menu_shell->activate_time = 0;

  need_enter = (menu->navigation_region != NULL || menu_shell->ignore_enter);

  /* Check to see if we are within an active submenu's navigation region
   */
  if (gtk_menu_navigating_submenu (menu, event->x_root, event->y_root))
    return TRUE; 

  /* Make sure we pop down if we enter a non-selectable menu item, so we
   * don't show a submenu when the cursor is outside the stay-up triangle.
   */
  if (!_gtk_menu_item_is_selectable (menu_item))
    {
      gtk_menu_shell_deselect (menu_shell);
      return FALSE;
    }

  if (need_enter)
    {
      /* The menu is now sensitive to enter events on its items, but
       * was previously sensitive.  So we fake an enter event.
       */
      gint width, height;
      
      menu_shell->ignore_enter = FALSE; 
      
      gdk_drawable_get_size (event->window, &width, &height);
      if (event->x >= 0 && event->x < width &&
	  event->y >= 0 && event->y < height)
	{
	  GdkEvent *send_event = gdk_event_new (GDK_ENTER_NOTIFY);
	  gboolean result;

	  send_event->crossing.window = g_object_ref (event->window);
	  send_event->crossing.time = event->time;
	  send_event->crossing.send_event = TRUE;
	  send_event->crossing.x_root = event->x_root;
	  send_event->crossing.y_root = event->y_root;
	  send_event->crossing.x = event->x;
	  send_event->crossing.y = event->y;

	  /* We send the event to 'widget', the currently active menu,
	   * instead of 'menu', the menu that the pointer is in. This
	   * will ensure that the event will be ignored unless the
	   * menuitem is a child of the active menu or some parent
	   * menu of the active menu.
	   */
	  result = gtk_widget_event (widget, send_event);
	  gdk_event_free (send_event);

	  return result;
	}
    }

  return FALSE;
}

static gboolean
get_double_arrows (GtkMenu *menu)
{
  GtkMenuPrivate *priv = gtk_menu_get_private (menu);
  gboolean        double_arrows;

  gtk_widget_style_get (GTK_WIDGET (menu),
                        "double-arrows", &double_arrows,
                        NULL);

  return double_arrows || (priv->initially_pushed_in &&
                           menu->scroll_offset != 0);
}

static void
gtk_menu_scroll_by (GtkMenu *menu, 
		    gint     step)
{
  GtkWidget *widget;
  gint offset;
  gint view_width, view_height;
  gboolean double_arrows;
  gint scroll_arrow_height;
  
  widget = GTK_WIDGET (menu);
  offset = menu->scroll_offset + step;

  gtk_widget_style_get (GTK_WIDGET (menu),
                        "scroll-arrow-vlength", &scroll_arrow_height,
                        NULL);

  double_arrows = get_double_arrows (menu);

  /* If we scroll upward and the non-visible top part
   * is smaller than the scroll arrow it would be
   * pretty stupid to show the arrow and taking more
   * screen space than just scrolling to the top.
   */
  if (!double_arrows)
    if ((step < 0) && (offset < scroll_arrow_height))
      offset = 0;

  /* Don't scroll over the top if we weren't before: */
  if ((menu->scroll_offset >= 0) && (offset < 0))
    offset = 0;

  gdk_drawable_get_size (widget->window, &view_width, &view_height);

  if (menu->scroll_offset == 0 &&
      view_height >= widget->requisition.height)
    return;

  /* Don't scroll past the bottom if we weren't before: */
  if (menu->scroll_offset > 0)
    view_height -= scroll_arrow_height;

  /* When both arrows are always shown, reduce
   * view height even more.
   */
  if (double_arrows)
    view_height -= scroll_arrow_height;

  if ((menu->scroll_offset + view_height <= widget->requisition.height) &&
      (offset + view_height > widget->requisition.height))
    offset = widget->requisition.height - view_height;

  if (offset != menu->scroll_offset)
    gtk_menu_scroll_to (menu, offset);
}

static void
gtk_menu_do_timeout_scroll (GtkMenu  *menu,
                            gboolean  touchscreen_mode)
{
  gboolean upper_visible;
  gboolean lower_visible;

  upper_visible = menu->upper_arrow_visible;
  lower_visible = menu->lower_arrow_visible;

  gtk_menu_scroll_by (menu, menu->scroll_step);

  if (touchscreen_mode &&
      (upper_visible != menu->upper_arrow_visible ||
       lower_visible != menu->lower_arrow_visible))
    {
      /* We are about to hide a scroll arrow while the mouse is pressed,
       * this would cause the uncovered menu item to be activated on button
       * release. Therefore we need to ignore button release here
       */
      GTK_MENU_SHELL (menu)->ignore_enter = TRUE;
      gtk_menu_get_private (menu)->ignore_button_release = TRUE;
    }
}

static gboolean
gtk_menu_scroll_timeout (gpointer  data)
{
  GtkMenu     *menu;
  GtkSettings *settings;
  gboolean     touchscreen_mode;

  GDK_THREADS_ENTER ();

  menu = GTK_MENU (data);

  settings = gtk_widget_get_settings (GTK_WIDGET (menu));
  g_object_get (settings,
                "gtk-touchscreen-mode", &touchscreen_mode,
                NULL);

  gtk_menu_do_timeout_scroll (menu, touchscreen_mode);

  GDK_THREADS_LEAVE ();

  return TRUE;
}

static gboolean
gtk_menu_scroll_timeout_initial (gpointer data)
{
  GtkMenu     *menu;
  GtkSettings *settings;
  guint        timeout;
  gboolean     touchscreen_mode;

  GDK_THREADS_ENTER ();

  menu = GTK_MENU (data);

  settings = gtk_widget_get_settings (GTK_WIDGET (menu));
  g_object_get (settings,
                "gtk-timeout-repeat", &timeout,
                "gtk-touchscreen-mode", &touchscreen_mode,
                NULL);

  gtk_menu_do_timeout_scroll (menu, touchscreen_mode);

  gtk_menu_remove_scroll_timeout (menu);

  menu->timeout_id = g_timeout_add (timeout, gtk_menu_scroll_timeout, menu);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
gtk_menu_start_scrolling (GtkMenu *menu)
{
  GtkSettings *settings;
  guint        timeout;
  gboolean     touchscreen_mode;

  settings = gtk_widget_get_settings (GTK_WIDGET (menu));
  g_object_get (settings,
                "gtk-timeout-repeat", &timeout,
                "gtk-touchscreen-mode", &touchscreen_mode,
                NULL);

  gtk_menu_do_timeout_scroll (menu, touchscreen_mode);

  menu->timeout_id = g_timeout_add (timeout, gtk_menu_scroll_timeout_initial,
                                    menu);
}

static gboolean
gtk_menu_scroll (GtkWidget	*widget,
		 GdkEventScroll *event)
{
  GtkMenu *menu = GTK_MENU (widget);

  switch (event->direction)
    {
    case GDK_SCROLL_RIGHT:
    case GDK_SCROLL_DOWN:
      gtk_menu_scroll_by (menu, MENU_SCROLL_STEP2);
      break;
    case GDK_SCROLL_LEFT:
    case GDK_SCROLL_UP:
      gtk_menu_scroll_by (menu, - MENU_SCROLL_STEP2);
      break;
    }

  return TRUE;
}

static void
gtk_menu_handle_scrolling (GtkMenu *menu,
			   gint     x,
			   gint     y,
			   gboolean enter,
                           gboolean motion)
{
  GtkMenuShell *menu_shell;
  GtkMenuPrivate *priv;
  gint width, height;
  gint border;
  GdkRectangle rect;
  gboolean in_arrow;
  gboolean scroll_fast = FALSE;
  guint vertical_padding;
  gint top_x, top_y;
  gint win_x, win_y;
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (menu));
  gboolean touchscreen_mode;
  gint scroll_arrow_height;
  
  priv = gtk_menu_get_private (menu);

  menu_shell = GTK_MENU_SHELL (menu);

  gdk_drawable_get_size (GTK_WIDGET (menu)->window, &width, &height);

  g_object_get (G_OBJECT (settings),
                "gtk-touchscreen-mode", &touchscreen_mode,
                NULL);

  gtk_widget_style_get (GTK_WIDGET (menu),
			"vertical-padding", &vertical_padding,
                        "scroll-arrow-vlength", &scroll_arrow_height,
			NULL);

  border = GTK_CONTAINER (menu)->border_width +
    GTK_WIDGET (menu)->style->ythickness + vertical_padding;

  gdk_window_get_position (menu->toplevel->window, &top_x, &top_y);
  x -= top_x;
  y -= top_y;

  gdk_window_get_position (GTK_WIDGET (menu)->window, &win_x, &win_y);

  /*  upper arrow handling  */

  rect.x = win_x;
  rect.y = win_y;
  rect.width = width;
  rect.height = scroll_arrow_height + border;

  in_arrow = FALSE;
  if (menu->upper_arrow_visible && !menu->tearoff_active &&
      (x >= rect.x) && (x < rect.x + rect.width) &&
      (y >= rect.y) && (y < rect.y + rect.height))
    {
      in_arrow = TRUE;
    }

  if (touchscreen_mode)
    menu->upper_arrow_prelight = in_arrow;

  if (priv->upper_arrow_state != GTK_STATE_INSENSITIVE)
    {
      gboolean     arrow_pressed = FALSE;
      GtkStateType arrow_state   = GTK_STATE_NORMAL;

      if (menu->upper_arrow_visible && !menu->tearoff_active)
        {
          if (touchscreen_mode)
            {
              if (enter && menu->upper_arrow_prelight)
                {
                  if (menu->timeout_id == 0)
                    {
                      /* Deselect the active item so that
                       * any submenus are popped down
                       */
                      gtk_menu_shell_deselect (menu_shell);

                      gtk_menu_remove_scroll_timeout (menu);
                      menu->scroll_step = -MENU_SCROLL_STEP2; /* always fast */

                      if (!motion)
                        {
                          /* Only do stuff on click. */
                          gtk_menu_start_scrolling (menu);
                          arrow_pressed = TRUE;
                        }
                    }
                  else
                    {
                      arrow_pressed = TRUE;
                    }
                }
              else if (!enter)
                {
                  gtk_menu_stop_scrolling (menu);
                }
            }
          else /* !touchscreen_mode */
            {
              scroll_fast = (y < rect.y + MENU_SCROLL_FAST_ZONE);

              if (enter && in_arrow &&
                  (!menu->upper_arrow_prelight ||
                   menu->scroll_fast != scroll_fast))
                {
                  menu->upper_arrow_prelight = TRUE;
                  menu->scroll_fast = scroll_fast;

                  /* Deselect the active item so that
                   * any submenus are popped down
                   */
                  gtk_menu_shell_deselect (menu_shell);

                  gtk_menu_remove_scroll_timeout (menu);
                  menu->scroll_step = scroll_fast ?
                    -MENU_SCROLL_STEP2 : -MENU_SCROLL_STEP1;

                  menu->timeout_id =
                    g_timeout_add (scroll_fast ?
                                   MENU_SCROLL_TIMEOUT2 : MENU_SCROLL_TIMEOUT1,
                                   gtk_menu_scroll_timeout, menu);
                }
              else if (!enter && !in_arrow && menu->upper_arrow_prelight)
                {
                  gtk_menu_stop_scrolling (menu);
                }
            }
        }

      if (arrow_pressed)
        arrow_state = GTK_STATE_ACTIVE;
      else if (menu->upper_arrow_prelight)
        arrow_state = GTK_STATE_PRELIGHT;

      if (arrow_state != priv->upper_arrow_state)
        {
          priv->upper_arrow_state = arrow_state;

          gdk_window_invalidate_rect (GTK_WIDGET (menu)->window,
                                      &rect, FALSE);
        }
    }

  /*  lower arrow handling  */

  rect.x = win_x;
  rect.y = win_y + height - border - scroll_arrow_height;
  rect.width = width;
  rect.height = scroll_arrow_height + border;

  in_arrow = FALSE;
  if (menu->lower_arrow_visible && !menu->tearoff_active &&
      (x >= rect.x) && (x < rect.x + rect.width) &&
      (y >= rect.y) && (y < rect.y + rect.height))
    {
      in_arrow = TRUE;
    }

  if (touchscreen_mode)
    menu->lower_arrow_prelight = in_arrow;

  if (priv->lower_arrow_state != GTK_STATE_INSENSITIVE)
    {
      gboolean     arrow_pressed = FALSE;
      GtkStateType arrow_state   = GTK_STATE_NORMAL;

      if (menu->lower_arrow_visible && !menu->tearoff_active)
        {
          if (touchscreen_mode)
            {
              if (enter && menu->lower_arrow_prelight)
                {
                  if (menu->timeout_id == 0)
                    {
                      /* Deselect the active item so that
                       * any submenus are popped down
                       */
                      gtk_menu_shell_deselect (menu_shell);

                      gtk_menu_remove_scroll_timeout (menu);
                      menu->scroll_step = MENU_SCROLL_STEP2; /* always fast */

                      if (!motion)
                        {
                          /* Only do stuff on click. */
                          gtk_menu_start_scrolling (menu);
                          arrow_pressed = TRUE;
                        }
                    }
                  else
                    {
                      arrow_pressed = TRUE;
                    }
                }
              else if (!enter)
                {
                  gtk_menu_stop_scrolling (menu);
                }
            }
          else /* !touchscreen_mode */
            {
              scroll_fast = (y > rect.y + rect.height - MENU_SCROLL_FAST_ZONE);

              if (enter && in_arrow &&
                  (!menu->lower_arrow_prelight ||
                   menu->scroll_fast != scroll_fast))
                {
                  menu->lower_arrow_prelight = TRUE;
                  menu->scroll_fast = scroll_fast;

                  /* Deselect the active item so that
                   * any submenus are popped down
                   */
                  gtk_menu_shell_deselect (menu_shell);

                  gtk_menu_remove_scroll_timeout (menu);
                  menu->scroll_step = scroll_fast ?
                    MENU_SCROLL_STEP2 : MENU_SCROLL_STEP1;

                  menu->timeout_id =
                    g_timeout_add (scroll_fast ?
                                   MENU_SCROLL_TIMEOUT2 : MENU_SCROLL_TIMEOUT1,
                                   gtk_menu_scroll_timeout, menu);
                }
              else if (!enter && !in_arrow && menu->lower_arrow_prelight)
                {
                  gtk_menu_stop_scrolling (menu);
                }
            }
        }

      if (arrow_pressed)
        arrow_state = GTK_STATE_ACTIVE;
      else if (menu->lower_arrow_prelight)
        arrow_state = GTK_STATE_PRELIGHT;

      if (arrow_state != priv->lower_arrow_state)
        {
          priv->lower_arrow_state = arrow_state;

          gdk_window_invalidate_rect (GTK_WIDGET (menu)->window,
                                      &rect, FALSE);
        }
    }
}

static gboolean
gtk_menu_enter_notify (GtkWidget        *widget,
		       GdkEventCrossing *event)
{
  GtkSettings *settings = gtk_widget_get_settings (widget);
  GtkWidget *menu_item;
  gboolean touchscreen_mode;

  g_object_get (G_OBJECT (settings),
                "gtk-touchscreen-mode", &touchscreen_mode,
                NULL);

  menu_item = gtk_get_event_widget ((GdkEvent*) event);
  if (GTK_IS_MENU (widget))
    {
      GtkMenuShell *menu_shell = GTK_MENU_SHELL (widget);

      if (!menu_shell->ignore_enter)
	gtk_menu_handle_scrolling (GTK_MENU (widget),
                                   event->x_root, event->y_root, TRUE, TRUE);
    }

  if (!touchscreen_mode && GTK_IS_MENU_ITEM (menu_item))
    {
      GtkWidget *menu = menu_item->parent;
      
      if (GTK_IS_MENU (menu))
	{
	  GtkMenuPrivate *priv = gtk_menu_get_private (GTK_MENU (menu));
	  GtkMenuShell *menu_shell = GTK_MENU_SHELL (menu);

	  if (priv->seen_item_enter)
	    {
	      /* This is the second enter we see for an item
	       * on this menu. This means a release should always
	       * mean activate.
	       */
	      menu_shell->activate_time = 0;
	    }
	  else if ((event->detail != GDK_NOTIFY_NONLINEAR &&
		    event->detail != GDK_NOTIFY_NONLINEAR_VIRTUAL))
	    {
	      if (definitely_within_item (menu_item, event->x, event->y))
		{
		  /* This is an actual user-enter (ie. not a pop-under)
		   * In this case, the user must either have entered
		   * sufficiently far enough into the item, or he must move
		   * far enough away from the enter point. (see
		   * gtk_menu_motion_notify())
		   */
		  menu_shell->activate_time = 0;
		}
	    }
	    
	  priv->seen_item_enter = TRUE;
	}
    }
  
  /* If this is a faked enter (see gtk_menu_motion_notify), 'widget'
   * will not correspond to the event widget's parent.  Check to see
   * if we are in the parent's navigation region.
   */
  if (GTK_IS_MENU_ITEM (menu_item) && GTK_IS_MENU (menu_item->parent) &&
      gtk_menu_navigating_submenu (GTK_MENU (menu_item->parent),
                                   event->x_root, event->y_root))
    return TRUE;

  return GTK_WIDGET_CLASS (gtk_menu_parent_class)->enter_notify_event (widget, event); 
}

static gboolean
gtk_menu_leave_notify (GtkWidget        *widget,
		       GdkEventCrossing *event)
{
  GtkMenuShell *menu_shell;
  GtkMenu *menu;
  GtkMenuItem *menu_item;
  GtkWidget *event_widget;

  menu = GTK_MENU (widget);
  menu_shell = GTK_MENU_SHELL (widget); 
  
  if (gtk_menu_navigating_submenu (menu, event->x_root, event->y_root))
    return TRUE; 

  gtk_menu_handle_scrolling (menu, event->x_root, event->y_root, FALSE, TRUE);

  event_widget = gtk_get_event_widget ((GdkEvent*) event);
  
  if (!GTK_IS_MENU_ITEM (event_widget))
    return TRUE;
  
  menu_item = GTK_MENU_ITEM (event_widget); 
  
  /* Here we check to see if we're leaving an active menu item with a submenu, 
   * in which case we enter submenu navigation mode. 
   */
  if (menu_shell->active_menu_item != NULL
      && menu_item->submenu != NULL
      && menu_item->submenu_placement == GTK_LEFT_RIGHT)
    {
      if (GTK_MENU_SHELL (menu_item->submenu)->active)
	{
	  gtk_menu_set_submenu_navigation_region (menu, menu_item, event);
	  return TRUE;
	}
      else if (menu_item == GTK_MENU_ITEM (menu_shell->active_menu_item))
	{
	  /* We are leaving an active menu item with nonactive submenu.
	   * Deselect it so we don't surprise the user with by popping
	   * up a submenu _after_ he left the item.
	   */
	  gtk_menu_shell_deselect (menu_shell);
	  return TRUE;
	}
    }
  
  return GTK_WIDGET_CLASS (gtk_menu_parent_class)->leave_notify_event (widget, event); 
}

static void 
gtk_menu_stop_navigating_submenu (GtkMenu *menu)
{
  if (menu->navigation_region) 
    {
      gdk_region_destroy (menu->navigation_region);
      menu->navigation_region = NULL;
    }  
  if (menu->navigation_timeout)
    {
      g_source_remove (menu->navigation_timeout);
      menu->navigation_timeout = 0;
    }
}

/* When the timeout is elapsed, the navigation region is destroyed
 * and the menuitem under the pointer (if any) is selected.
 */
static gboolean
gtk_menu_stop_navigating_submenu_cb (gpointer user_data)
{
  GtkMenu *menu = user_data;
  GdkWindow *child_window;

  GDK_THREADS_ENTER ();

  gtk_menu_stop_navigating_submenu (menu);
  
  if (GTK_WIDGET_REALIZED (menu))
    {
      child_window = gdk_window_get_pointer (menu->bin_window, NULL, NULL, NULL);

      if (child_window)
	{
	  GdkEvent *send_event = gdk_event_new (GDK_ENTER_NOTIFY);

	  send_event->crossing.window = g_object_ref (child_window);
	  send_event->crossing.time = GDK_CURRENT_TIME; /* Bogus */
	  send_event->crossing.send_event = TRUE;

	  GTK_WIDGET_CLASS (gtk_menu_parent_class)->enter_notify_event (GTK_WIDGET (menu), (GdkEventCrossing *)send_event);

	  gdk_event_free (send_event);
	}
    }

  GDK_THREADS_LEAVE ();

  return FALSE; 
}

static gboolean
gtk_menu_navigating_submenu (GtkMenu *menu,
			     gint     event_x,
			     gint     event_y)
{
  if (menu->navigation_region)
    {
      if (gdk_region_point_in (menu->navigation_region, event_x, event_y))
	return TRUE;
      else
	{
	  gtk_menu_stop_navigating_submenu (menu);
	  return FALSE;
	}
    }
  return FALSE;
}

#undef DRAW_STAY_UP_TRIANGLE

#ifdef DRAW_STAY_UP_TRIANGLE

static void
draw_stay_up_triangle (GdkWindow *window,
		       GdkRegion *region)
{
  /* Draw ugly color all over the stay-up triangle */
  GdkColor ugly_color = { 0, 50000, 10000, 10000 };
  GdkGCValues gc_values;
  GdkGC *ugly_gc;
  GdkRectangle clipbox;

  gc_values.subwindow_mode = GDK_INCLUDE_INFERIORS;
  ugly_gc = gdk_gc_new_with_values (window, &gc_values, 0 | GDK_GC_SUBWINDOW);
  gdk_gc_set_rgb_fg_color (ugly_gc, &ugly_color);
  gdk_gc_set_clip_region (ugly_gc, region);

  gdk_region_get_clipbox (region, &clipbox);
  
  gdk_draw_rectangle (window,
                     ugly_gc,
                     TRUE,
                     clipbox.x, clipbox.y,
                     clipbox.width, clipbox.height);
  
  g_object_unref (ugly_gc);
}
#endif

static GdkRegion *
flip_region (GdkRegion *region,
	     gboolean   flip_x,
	     gboolean   flip_y)
{
  gint n_rectangles;
  GdkRectangle *rectangles;
  GdkRectangle clipbox;
  GdkRegion *new_region;
  gint i;

  new_region = gdk_region_new ();
  
  gdk_region_get_rectangles (region, &rectangles, &n_rectangles);
  gdk_region_get_clipbox (region, &clipbox);

  for (i = 0; i < n_rectangles; ++i)
    {
      GdkRectangle rect = rectangles[i];

      if (flip_y)
	rect.y -= 2 * (rect.y - clipbox.y) + rect.height;

      if (flip_x)
	rect.x -= 2 * (rect.x - clipbox.x) + rect.width;

      gdk_region_union_with_rect (new_region, &rect);
    }

  g_free (rectangles);

  return new_region;
}

static void
gtk_menu_set_submenu_navigation_region (GtkMenu          *menu,
					GtkMenuItem      *menu_item,
					GdkEventCrossing *event)
{
  gint submenu_left = 0;
  gint submenu_right = 0;
  gint submenu_top = 0;
  gint submenu_bottom = 0;
  gint width = 0;
  gint height = 0;
  GdkPoint point[3];
  GtkWidget *event_widget;

  g_return_if_fail (menu_item->submenu != NULL);
  g_return_if_fail (event != NULL);
  
  event_widget = gtk_get_event_widget ((GdkEvent*) event);
  
  gdk_window_get_origin (menu_item->submenu->window, &submenu_left, &submenu_top);
  gdk_drawable_get_size (menu_item->submenu->window, &width, &height);
  
  submenu_right = submenu_left + width;
  submenu_bottom = submenu_top + height;
  
  gdk_drawable_get_size (event_widget->window, &width, &height);
  
  if (event->x >= 0 && event->x < width)
    {
      gint popdown_delay;
      gboolean flip_y = FALSE;
      gboolean flip_x = FALSE;
      
      gtk_menu_stop_navigating_submenu (menu);

      if (menu_item->submenu_direction == GTK_DIRECTION_RIGHT)
	{
	  /* right */
	  point[0].x = event->x_root;
	  point[1].x = submenu_left;
	}
      else
	{
	  /* left */
	  point[0].x = event->x_root + 1;
	  point[1].x = 2 * (event->x_root + 1) - submenu_right;

	  flip_x = TRUE;
	}

      if (event->y < 0)
	{
	  /* top */
	  point[0].y = event->y_root + 1;
	  point[1].y = 2 * (event->y_root + 1) - submenu_top + NAVIGATION_REGION_OVERSHOOT;

	  if (point[0].y >= point[1].y - NAVIGATION_REGION_OVERSHOOT)
	    return;

	  flip_y = TRUE;
	}
      else
	{
	  /* bottom */
	  point[0].y = event->y_root;
	  point[1].y = submenu_bottom + NAVIGATION_REGION_OVERSHOOT;

	  if (point[0].y >= submenu_bottom)
	    return;
	}

      point[2].x = point[1].x;
      point[2].y = point[0].y;

      menu->navigation_region = gdk_region_polygon (point, 3, GDK_WINDING_RULE);

      if (flip_x || flip_y)
	{
	  GdkRegion *new_region = flip_region (menu->navigation_region, flip_x, flip_y);
	  gdk_region_destroy (menu->navigation_region);
	  menu->navigation_region = new_region;
	}

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (menu)),
		    "gtk-menu-popdown-delay", &popdown_delay,
		    NULL);

      menu->navigation_timeout = g_timeout_add (popdown_delay,
						gtk_menu_stop_navigating_submenu_cb, menu);

#ifdef DRAW_STAY_UP_TRIANGLE
      draw_stay_up_triangle (gdk_get_default_root_window(),
			     menu->navigation_region);
#endif
    }
}

static void
gtk_menu_deactivate (GtkMenuShell *menu_shell)
{
  GtkWidget *parent;
  
  g_return_if_fail (GTK_IS_MENU (menu_shell));
  
  parent = menu_shell->parent_menu_shell;
  
  menu_shell->activate_time = 0;
  gtk_menu_popdown (GTK_MENU (menu_shell));
  
  if (parent)
    gtk_menu_shell_deactivate (GTK_MENU_SHELL (parent));
}

static void
gtk_menu_position (GtkMenu *menu)
{
  GtkWidget *widget;
  GtkRequisition requisition;
  GtkMenuPrivate *private;
  gint x, y;
  gint scroll_offset;
  gint menu_height;
  GdkScreen *screen;
  GdkScreen *pointer_screen;
  GdkRectangle monitor;
  gint scroll_arrow_height;
  
  g_return_if_fail (GTK_IS_MENU (menu));

  widget = GTK_WIDGET (menu);

  screen = gtk_widget_get_screen (widget);
  gdk_display_get_pointer (gdk_screen_get_display (screen),
			   &pointer_screen, &x, &y, NULL);

  gtk_widget_style_get (GTK_WIDGET (menu),
                        "scroll-arrow-vlength", &scroll_arrow_height,
                        NULL);
  
  /* We need the requisition to figure out the right place to
   * popup the menu. In fact, we always need to ask here, since
   * if a size_request was queued while we weren't popped up,
   * the requisition won't have been recomputed yet.
   */
  gtk_widget_size_request (widget, &requisition);

  if (pointer_screen != screen)
    {
      /* Pointer is on a different screen; roughly center the
       * menu on the screen. If someone was using multiscreen
       * + Xinerama together they'd probably want something
       * fancier; but that is likely to be vanishingly rare.
       */
      x = MAX (0, (gdk_screen_get_width (screen) - requisition.width) / 2);
      y = MAX (0, (gdk_screen_get_height (screen) - requisition.height) / 2);
    }

  private = gtk_menu_get_private (menu);
  private->monitor_num = gdk_screen_get_monitor_at_point (screen, x, y);

  private->initially_pushed_in = FALSE;

  /* Set the type hint here to allow custom position functions to set a different hint */
  if (!GTK_WIDGET_VISIBLE (menu->toplevel))
    gtk_window_set_type_hint (GTK_WINDOW (menu->toplevel), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
  
  if (menu->position_func)
    {
      (* menu->position_func) (menu, &x, &y, &private->initially_pushed_in,
                               menu->position_func_data);
      if (private->monitor_num < 0) 
	private->monitor_num = gdk_screen_get_monitor_at_point (screen, x, y);

      gdk_screen_get_monitor_geometry (screen, private->monitor_num, &monitor);
    }
  else
    {
      gint space_left, space_right, space_above, space_below;
      gint needed_width;
      gint needed_height;
      gint xthickness = widget->style->xthickness;
      gint ythickness = widget->style->ythickness;
      gboolean rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

      /* The placement of popup menus horizontally works like this (with
       * RTL in parentheses)
       *
       * - If there is enough room to the right (left) of the mouse cursor,
       *   position the menu there.
       * 
       * - Otherwise, if if there is enough room to the left (right) of the 
       *   mouse cursor, position the menu there.
       * 
       * - Otherwise if the menu is smaller than the monitor, position it
       *   on the side of the mouse cursor that has the most space available
       *
       * - Otherwise (if there is simply not enough room for the menu on the
       *   monitor), position it as far left (right) as possible.
       *
       * Positioning in the vertical direction is similar: first try below
       * mouse cursor, then above.
       */
      gdk_screen_get_monitor_geometry (screen, private->monitor_num, &monitor);

      space_left = x - monitor.x;
      space_right = monitor.x + monitor.width - x - 1;
      space_above = y - monitor.y;
      space_below = monitor.y + monitor.height - y - 1;

      /* position horizontally */

      /* the amount of space we need to position the menu. Note the
       * menu is offset "xthickness" pixels 
       */
      needed_width = requisition.width - xthickness;

      if (needed_width <= space_left ||
	  needed_width <= space_right)
	{
	  if ((rtl  && needed_width <= space_left) ||
	      (!rtl && needed_width >  space_right))
	    {
	      /* position left */
	      x = x + xthickness - requisition.width + 1;
	    }
	  else
	    {
	      /* position right */
	      x = x - xthickness;
	    }

	  /* x is clamped on-screen further down */
	}
      else if (requisition.width <= monitor.width)
	{
	  /* the menu is too big to fit on either side of the mouse
	   * cursor, but smaller than the monitor. Position it on
	   * the side that has the most space
	   */
	  if (space_left > space_right)
	    {
	      /* left justify */
	      x = monitor.x;
	    }
	  else
	    {
	      /* right justify */
	      x = monitor.x + monitor.width - requisition.width;
	    }
	}
      else /* menu is simply too big for the monitor */
	{
	  if (rtl)
	    {
	      /* right justify */
	      x = monitor.x + monitor.width - requisition.width;
	    }
	  else
	    {
	      /* left justify */
	      x = monitor.x;
	    }
	}

      /* Position vertically. The algorithm is the same as above, but
       * simpler because we don't have to take RTL into account.
       */
      needed_height = requisition.height - ythickness;

      if (needed_height <= space_above ||
	  needed_height <= space_below)
	{
	  if (needed_height <= space_below)
	    y = y - ythickness;
	  else
	    y = y + ythickness - requisition.height + 1;
	  
	  y = CLAMP (y, monitor.y,
		     monitor.y + monitor.height - requisition.height);
	}
      else if (needed_height > space_below && needed_height > space_above)
	{
	  if (space_below >= space_above)
	    y = monitor.y + monitor.height - requisition.height;
	  else
	    y = monitor.y;
	}
      else
	{
	  y = monitor.y;
	}
    }

  scroll_offset = 0;

  if (private->initially_pushed_in)
    {
      menu_height = GTK_WIDGET (menu)->requisition.height;

      if (y + menu_height > monitor.y + monitor.height)
	{
	  scroll_offset -= y + menu_height - (monitor.y + monitor.height);
	  y = (monitor.y + monitor.height) - menu_height;
	}
  
      if (y < monitor.y)
	{
	  scroll_offset += monitor.y - y;
	  y = monitor.y;
	}
    }

  /* FIXME: should this be done in the various position_funcs ? */
  x = CLAMP (x, monitor.x, MAX (monitor.x, monitor.x + monitor.width - requisition.width));
 
  if (GTK_MENU_SHELL (menu)->active)
    {
      private->have_position = TRUE;
      private->x = x;
      private->y = y;
    }
  
  if (y + requisition.height > monitor.y + monitor.height)
    requisition.height = (monitor.y + monitor.height) - y;
  
  if (y < monitor.y)
    {
      scroll_offset += monitor.y - y;
      requisition.height -= monitor.y - y;
      y = monitor.y;
    }

  if (scroll_offset > 0)
    scroll_offset += scroll_arrow_height;
  
  gtk_window_move (GTK_WINDOW (GTK_MENU_SHELL (menu)->active ? menu->toplevel : menu->tearoff_window), 
		   x, y);

  if (!GTK_MENU_SHELL (menu)->active)
    {
      gtk_window_resize (GTK_WINDOW (menu->tearoff_window),
			 requisition.width, requisition.height);
    }
  
  menu->scroll_offset = scroll_offset;
}

static void
gtk_menu_remove_scroll_timeout (GtkMenu *menu)
{
  if (menu->timeout_id)
    {
      g_source_remove (menu->timeout_id);
      menu->timeout_id = 0;
    }
}

static void
gtk_menu_stop_scrolling (GtkMenu *menu)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (menu));
  gboolean touchscreen_mode;
  gboolean was_scrolling;

  gtk_menu_remove_scroll_timeout (menu);
  
  g_object_get (G_OBJECT (settings),
		"gtk-touchscreen-mode", &touchscreen_mode,
		NULL);
  
  if (!touchscreen_mode)
    {
      menu->upper_arrow_prelight = FALSE;
      menu->lower_arrow_prelight = FALSE;
    }
}

static void
gtk_menu_scroll_to (GtkMenu *menu,
		    gint    offset)
{
  GtkWidget *widget;
  gint x, y;
  gint view_width, view_height;
  gint border_width;
  gint menu_height;
  guint vertical_padding;
  guint horizontal_padding;
  gboolean double_arrows;
  gint scroll_arrow_height;
  
  widget = GTK_WIDGET (menu);

  if (menu->tearoff_active &&
      menu->tearoff_adjustment &&
      (menu->tearoff_adjustment->value != offset))
    {
      menu->tearoff_adjustment->value =
	CLAMP (offset,
	       0, menu->tearoff_adjustment->upper - menu->tearoff_adjustment->page_size);
      gtk_adjustment_value_changed (menu->tearoff_adjustment);
    }
  
  /* Move/resize the viewport according to arrows: */
  view_width = widget->allocation.width;
  view_height = widget->allocation.height;

  gtk_widget_style_get (GTK_WIDGET (menu),
                        "vertical-padding", &vertical_padding,
                        "horizontal-padding", &horizontal_padding,
                        "scroll-arrow-vlength", &scroll_arrow_height,
                        NULL);

  double_arrows = get_double_arrows (menu);

  border_width = GTK_CONTAINER (menu)->border_width;
  view_width -= (border_width + widget->style->xthickness + horizontal_padding) * 2;
  view_height -= (border_width + widget->style->ythickness + vertical_padding) * 2;
  menu_height = widget->requisition.height -
    (border_width + widget->style->ythickness + vertical_padding) * 2;

  x = border_width + widget->style->xthickness + horizontal_padding;
  y = border_width + widget->style->ythickness + vertical_padding;

  if (double_arrows && !menu->tearoff_active)
    {
      if (view_height < menu_height               ||
          (offset > 0 && menu->scroll_offset > 0) ||
          (offset < 0 && menu->scroll_offset < 0))
        {
          GtkMenuPrivate *priv = gtk_menu_get_private (menu);
          GtkStateType    upper_arrow_previous_state = priv->upper_arrow_state;
          GtkStateType    lower_arrow_previous_state = priv->lower_arrow_state;

          if (!menu->upper_arrow_visible || !menu->lower_arrow_visible)
            gtk_widget_queue_draw (GTK_WIDGET (menu));

          view_height -= 2 * scroll_arrow_height;
          y += scroll_arrow_height;

          menu->upper_arrow_visible = menu->lower_arrow_visible = TRUE;

          if (offset <= 0)
            priv->upper_arrow_state = GTK_STATE_INSENSITIVE;
          else if (priv->upper_arrow_state == GTK_STATE_INSENSITIVE)
            priv->upper_arrow_state = menu->upper_arrow_prelight ?
              GTK_STATE_PRELIGHT : GTK_STATE_NORMAL;

          if (offset >= menu_height - view_height)
            priv->lower_arrow_state = GTK_STATE_INSENSITIVE;
          else if (priv->lower_arrow_state == GTK_STATE_INSENSITIVE)
            priv->lower_arrow_state = menu->lower_arrow_prelight ?
              GTK_STATE_PRELIGHT : GTK_STATE_NORMAL;

          if ((priv->upper_arrow_state != upper_arrow_previous_state) ||
              (priv->lower_arrow_state != lower_arrow_previous_state))
            gtk_widget_queue_draw (GTK_WIDGET (menu));

          if (upper_arrow_previous_state != GTK_STATE_INSENSITIVE &&
              priv->upper_arrow_state == GTK_STATE_INSENSITIVE)
            {
              /* At the upper border, possibly remove timeout */
              if (menu->scroll_step < 0)
                {
                  gtk_menu_stop_scrolling (menu);
                  gtk_widget_queue_draw (GTK_WIDGET (menu));
                }
            }

          if (lower_arrow_previous_state != GTK_STATE_INSENSITIVE &&
              priv->lower_arrow_state == GTK_STATE_INSENSITIVE)
            {
              /* At the lower border, possibly remove timeout */
              if (menu->scroll_step > 0)
                {
                  gtk_menu_stop_scrolling (menu);
                  gtk_widget_queue_draw (GTK_WIDGET (menu));
                }
            }
        }
      else if (menu->upper_arrow_visible || menu->lower_arrow_visible)
        {
          offset = 0;

          menu->upper_arrow_visible = menu->lower_arrow_visible = FALSE;
          menu->upper_arrow_prelight = menu->lower_arrow_prelight = FALSE;

          gtk_menu_stop_scrolling (menu);
          gtk_widget_queue_draw (GTK_WIDGET (menu));
        }
    }
  else if (!menu->tearoff_active)
    {
      gboolean last_visible;

      last_visible = menu->upper_arrow_visible;
      menu->upper_arrow_visible = offset > 0;
      
      if (menu->upper_arrow_visible)
	view_height -= scroll_arrow_height;
      
      if ((last_visible != menu->upper_arrow_visible) &&
          !menu->upper_arrow_visible)
	{
          menu->upper_arrow_prelight = FALSE;

	  /* If we hid the upper arrow, possibly remove timeout */
	  if (menu->scroll_step < 0)
	    {
	      gtk_menu_stop_scrolling (menu);
	      gtk_widget_queue_draw (GTK_WIDGET (menu));
	    }
	}

      last_visible = menu->lower_arrow_visible;
      menu->lower_arrow_visible = offset < menu_height - view_height;
      
      if (menu->lower_arrow_visible)
	view_height -= scroll_arrow_height;
      
      if ((last_visible != menu->lower_arrow_visible) &&
	   !menu->lower_arrow_visible)
	{
          menu->lower_arrow_prelight = FALSE;

	  /* If we hid the lower arrow, possibly remove timeout */
	  if (menu->scroll_step > 0)
	    {
	      gtk_menu_stop_scrolling (menu);
	      gtk_widget_queue_draw (GTK_WIDGET (menu));
	    }
	}
      
      if (menu->upper_arrow_visible)
	y += scroll_arrow_height;
    }

  /* Scroll the menu: */
  if (GTK_WIDGET_REALIZED (menu))
    gdk_window_move (menu->bin_window, 0, -offset);

  if (GTK_WIDGET_REALIZED (menu))
    gdk_window_move_resize (menu->view_window,
			    x,
			    y,
			    view_width,
			    view_height);

  menu->scroll_offset = offset;
}

static gboolean
compute_child_offset (GtkMenu   *menu,
		      GtkWidget *menu_item,
		      gint      *offset,
		      gint      *height,
		      gboolean  *is_last_child)
{
  GtkMenuPrivate *priv = gtk_menu_get_private (menu);
  gint item_top_attach;
  gint item_bottom_attach;
  gint child_offset = 0;
  gint i;

  get_effective_child_attach (menu_item, NULL, NULL,
			      &item_top_attach, &item_bottom_attach);

  /* there is a possibility that we get called before _size_request, so
   * check the height table for safety.
   */
  if (!priv->heights || priv->heights_length < gtk_menu_get_n_rows (menu))
    return FALSE;

  /* when we have a row with only invisible children, it's height will
   * be zero, so there's no need to check WIDGET_VISIBLE here
   */
  for (i = 0; i < item_top_attach; i++)
    child_offset += priv->heights[i];

  if (is_last_child)
    *is_last_child = (item_bottom_attach == gtk_menu_get_n_rows (menu));
  if (offset)
    *offset = child_offset;
  if (height)
    *height = priv->heights[item_top_attach];

  return TRUE;
}

static void
gtk_menu_scroll_item_visible (GtkMenuShell    *menu_shell,
			      GtkWidget       *menu_item)
{
  GtkMenu *menu;
  gint child_offset, child_height;
  gint width, height;
  gint y;
  gint arrow_height;
  gboolean last_child = 0;
  
  menu = GTK_MENU (menu_shell);

  /* We need to check if the selected item fully visible.
   * If not we need to scroll the menu so that it becomes fully
   * visible.
   */

  if (compute_child_offset (menu, menu_item,
			    &child_offset, &child_height, &last_child))
    {
      guint vertical_padding;
      gboolean double_arrows;
      gint scroll_arrow_height;
      
      y = menu->scroll_offset;
      gdk_drawable_get_size (GTK_WIDGET (menu)->window, &width, &height);

      gtk_widget_style_get (GTK_WIDGET (menu),
			    "vertical-padding", &vertical_padding,
                            "scroll-arrow-vlength", &scroll_arrow_height,
                            NULL);

      double_arrows = get_double_arrows (menu);

      height -= 2*GTK_CONTAINER (menu)->border_width + 2*GTK_WIDGET (menu)->style->ythickness + 2*vertical_padding;
      
      if (child_offset < y)
	{
	  /* Ignore the enter event we might get if the pointer is on the menu
	   */
	  menu_shell->ignore_enter = TRUE;
	  gtk_menu_scroll_to (menu, child_offset);
	}
      else
	{
	  arrow_height = 0;
	  if (menu->upper_arrow_visible && !menu->tearoff_active)
	    arrow_height += scroll_arrow_height;
	  if (menu->lower_arrow_visible && !menu->tearoff_active)
	    arrow_height += scroll_arrow_height;
	  
	  if (child_offset + child_height > y + height - arrow_height)
	    {
	      arrow_height = 0;
	      if ((!last_child && !menu->tearoff_active) || double_arrows)
		arrow_height += scroll_arrow_height;

	      y = child_offset + child_height - height + arrow_height;
	      if (((y > 0) && !menu->tearoff_active) || double_arrows)
		{
		  /* Need upper arrow */
		  arrow_height += scroll_arrow_height;
		  y = child_offset + child_height - height + arrow_height;
		}
	      /* Ignore the enter event we might get if the pointer is on the menu
	       */
	      menu_shell->ignore_enter = TRUE;
	      gtk_menu_scroll_to (menu, y);
	    }
	}    
      
    }
}

static void
gtk_menu_select_item (GtkMenuShell  *menu_shell,
		      GtkWidget     *menu_item)
{
  GtkMenu *menu = GTK_MENU (menu_shell);

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (menu)))
    gtk_menu_scroll_item_visible (menu_shell, menu_item);

  GTK_MENU_SHELL_CLASS (gtk_menu_parent_class)->select_item (menu_shell, menu_item);
}


/* Reparent the menu, taking care of the refcounting
 *
 * If unrealize is true we force a unrealize while reparenting the parent.
 * This can help eliminate flicker in some cases.
 *
 * What happens is that when the menu is unrealized and then re-realized,
 * the allocations are as follows:
 *
 *  parent - 1x1 at (0,0) 
 *  child1 - 100x20 at (0,0)
 *  child2 - 100x20 at (0,20)
 *  child3 - 100x20 at (0,40)
 *
 * That is, the parent is small but the children are full sized. Then,
 * when the queued_resize gets processed, the parent gets resized to
 * full size. 
 *
 * But in order to eliminate flicker when scrolling, gdkgeometry-x11.c
 * contains the following logic:
 * 
 * - if a move or resize operation on a window would change the clip 
 *   region on the children, then before the window is resized
 *   the background for children is temporarily set to None, the
 *   move/resize done, and the background for the children restored.
 *
 * So, at the point where the parent is resized to final size, the
 * background for the children is temporarily None, and thus they
 * are not cleared to the background color and the previous background
 * (the image of the menu) is left in place.
 */
static void 
gtk_menu_reparent (GtkMenu      *menu, 
		   GtkWidget    *new_parent, 
		   gboolean      unrealize)
{
  GtkObject *object = GTK_OBJECT (menu);
  GtkWidget *widget = GTK_WIDGET (menu);
  gboolean was_floating = g_object_is_floating (object);

  g_object_ref_sink (object);

  if (unrealize)
    {
      g_object_ref (object);
      gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
      gtk_container_add (GTK_CONTAINER (new_parent), widget);
      g_object_unref (object);
    }
  else
    gtk_widget_reparent (GTK_WIDGET (menu), new_parent);
  
  if (was_floating)
    g_object_force_floating (G_OBJECT (object));
  else
    g_object_unref (object);
}

static void
gtk_menu_show_all (GtkWidget *widget)
{
  /* Show children, but not self. */
  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_show_all, NULL);
}


static void
gtk_menu_hide_all (GtkWidget *widget)
{
  /* Hide children, but not self. */
  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_hide_all, NULL);
}

/**
 * gtk_menu_set_screen:
 * @menu: a #GtkMenu.
 * @screen: a #GdkScreen, or %NULL if the screen should be
 *          determined by the widget the menu is attached to.
 *
 * Sets the #GdkScreen on which the menu will be displayed.
 * 
 * Since: 2.2
 **/
void
gtk_menu_set_screen (GtkMenu   *menu, 
		     GdkScreen *screen)
{
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (!screen || GDK_IS_SCREEN (screen));

  g_object_set_data (G_OBJECT (menu), I_("gtk-menu-explicit-screen"), screen);

  if (screen)
    {
      menu_change_screen (menu, screen);
    }
  else
    {
      GtkWidget *attach_widget = gtk_menu_get_attach_widget (menu);
      if (attach_widget)
	attach_widget_screen_changed (attach_widget, NULL, menu);
    }
}

/**
 * gtk_menu_attach:
 * @menu: a #GtkMenu.
 * @child: a #GtkMenuItem.
 * @left_attach: The column number to attach the left side of the item to.
 * @right_attach: The column number to attach the right side of the item to.
 * @top_attach: The row number to attach the top of the item to.
 * @bottom_attach: The row number to attach the bottom of the item to.
 *
 * Adds a new #GtkMenuItem to a (table) menu. The number of 'cells' that
 * an item will occupy is specified by @left_attach, @right_attach,
 * @top_attach and @bottom_attach. These each represent the leftmost,
 * rightmost, uppermost and lower column and row numbers of the table.
 * (Columns and rows are indexed from zero).
 *
 * Note that this function is not related to gtk_menu_detach().
 *
 * Since: 2.4
 **/
void
gtk_menu_attach (GtkMenu   *menu,
                 GtkWidget *child,
                 guint      left_attach,
                 guint      right_attach,
                 guint      top_attach,
                 guint      bottom_attach)
{
  GtkMenuShell *menu_shell;
  
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GTK_IS_MENU_ITEM (child));
  g_return_if_fail (child->parent == NULL || 
		    child->parent == GTK_WIDGET (menu));
  g_return_if_fail (left_attach < right_attach);
  g_return_if_fail (top_attach < bottom_attach);

  menu_shell = GTK_MENU_SHELL (menu);
  
  if (!child->parent)
    {
      AttachInfo *ai = get_attach_info (child);
      
      ai->left_attach = left_attach;
      ai->right_attach = right_attach;
      ai->top_attach = top_attach;
      ai->bottom_attach = bottom_attach;
      
      menu_shell->children = g_list_append (menu_shell->children, child);

      gtk_widget_set_parent (child, GTK_WIDGET (menu));

      menu_queue_resize (menu);
    }
  else
    {
      gtk_container_child_set (GTK_CONTAINER (child->parent), child,
			       "left-attach",   left_attach,
			       "right-attach",  right_attach,
			       "top-attach",    top_attach,
			       "bottom-attach", bottom_attach,
			       NULL);
    }
}

static gint
gtk_menu_get_popup_delay (GtkMenuShell *menu_shell)
{
  gint popup_delay;

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (menu_shell)),
		"gtk-menu-popup-delay", &popup_delay,
		NULL);

  return popup_delay;
}

static GtkWidget *
find_child_containing (GtkMenuShell *menu_shell,
                       int           left,
                       int           right,
                       int           top,
                       int           bottom)
{
  GList *list;

  /* find a child which includes the area given by
   * left, right, top, bottom.
   */

  for (list = menu_shell->children; list; list = list->next)
    {
      gint l, r, t, b;

      if (!_gtk_menu_item_is_selectable (list->data))
        continue;

      get_effective_child_attach (list->data, &l, &r, &t, &b);

      if (l <= left && right <= r
          && t <= top && bottom <= b)
        return GTK_WIDGET (list->data);
    }

  return NULL;
}

static void
gtk_menu_move_current (GtkMenuShell *menu_shell,
                       GtkMenuDirectionType direction)
{
  GtkMenu *menu = GTK_MENU (menu_shell);
  gint i;
  gint l, r, t, b;
  GtkWidget *match = NULL;

  if (gtk_widget_get_direction (GTK_WIDGET (menu_shell)) == GTK_TEXT_DIR_RTL)
    {
      switch (direction)
	{
	case GTK_MENU_DIR_CHILD:
	  direction = GTK_MENU_DIR_PARENT;
	  break;
	case GTK_MENU_DIR_PARENT:
	  direction = GTK_MENU_DIR_CHILD;
	  break;
	default: ;
	}
    }

  /* use special table menu key bindings */
  if (menu_shell->active_menu_item && gtk_menu_get_n_columns (menu) > 1)
    {
      get_effective_child_attach (menu_shell->active_menu_item, &l, &r, &t, &b);

      if (direction == GTK_MENU_DIR_NEXT)
        {
	  for (i = b; i < gtk_menu_get_n_rows (menu); i++)
            {
              match = find_child_containing (menu_shell, l, l + 1, i, i + 1);
              if (match)
                break;
            }

	  if (!match)
	    {
	      /* wrap around */
	      for (i = 0; i < t; i++)
	        {
                  match = find_child_containing (menu_shell,
                                                 l, l + 1, i, i + 1);
                  if (match)
                    break;
		}
	    }
	}
      else if (direction == GTK_MENU_DIR_PREV)
        {
          for (i = t; i > 0; i--)
            {
              match = find_child_containing (menu_shell, l, l + 1, i - 1, i);
              if (match)
                break;
            }

	  if (!match)
	    {
	      /* wrap around */
	      for (i = gtk_menu_get_n_rows (menu); i > b; i--)
	        {
                  match = find_child_containing (menu_shell,
                                                 l, l + 1, i - 1, i);
                  if (match)
		    break;
		}
	    }
	}
      else if (direction == GTK_MENU_DIR_PARENT)
        {
          /* we go one left if possible */
          if (l > 0)
            match = find_child_containing (menu_shell, l - 1, l, t, t + 1);

          if (!match)
            {
              GtkWidget *parent = menu_shell->parent_menu_shell;

              if (!parent
                  || g_list_length (GTK_MENU_SHELL (parent)->children) <= 1)
                match = menu_shell->active_menu_item;
            }
        }
      else if (direction == GTK_MENU_DIR_CHILD)
        {
          /* we go one right if possible */
	  if (r < gtk_menu_get_n_columns (menu))
            match = find_child_containing (menu_shell, r, r + 1, t, t + 1);

          if (!match)
            {
              GtkWidget *parent = menu_shell->parent_menu_shell;

              if (! GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu &&
                  (!parent ||
                   g_list_length (GTK_MENU_SHELL (parent)->children) <= 1))
                match = menu_shell->active_menu_item;
            }
        }

      if (match)
        {
	  gtk_menu_shell_select_item (menu_shell, match);
          return;
        }
    }

  GTK_MENU_SHELL_CLASS (gtk_menu_parent_class)->move_current (menu_shell, direction);
}

static gint
get_visible_size (GtkMenu *menu)
{
  GtkWidget *widget = GTK_WIDGET (menu);
  GtkContainer *container = GTK_CONTAINER (menu);
  gint scroll_arrow_height;
  
  gint menu_height = (widget->allocation.height
		      - 2 * (container->border_width
			     + widget->style->ythickness));
  
  gtk_widget_style_get (GTK_WIDGET (menu),
                        "scroll-arrow-vlength", &scroll_arrow_height,
                        NULL);

  if (menu->upper_arrow_visible && !menu->tearoff_active)
    menu_height -= scroll_arrow_height;
  if (menu->lower_arrow_visible && !menu->tearoff_active)
    menu_height -= scroll_arrow_height;

  return menu_height;
}

/* Find the sensitive on-screen child containing @y, or if none,
 * the nearest selectable onscreen child. (%NULL if none)
 */
static GtkWidget *
child_at (GtkMenu *menu,
	  gint     y)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (menu);
  GtkWidget *child = NULL;
  gint child_offset = 0;
  GList *children;
  gint menu_height;
  gint lower, upper;		/* Onscreen bounds */

  menu_height = get_visible_size (menu);
  lower = menu->scroll_offset;
  upper = menu->scroll_offset + menu_height;
  
  for (children = menu_shell->children; children; children = children->next)
    {
      if (GTK_WIDGET_VISIBLE (children->data))
	{
	  GtkRequisition child_requisition;

	  gtk_widget_size_request (children->data, &child_requisition);

	  if (_gtk_menu_item_is_selectable (children->data) &&
	      child_offset >= lower &&
	      child_offset + child_requisition.height <= upper)
	    {
	      child = children->data;
	      
	      if (child_offset + child_requisition.height > y &&
		  !GTK_IS_TEAROFF_MENU_ITEM (child))
		return child;
	    }
      
	  child_offset += child_requisition.height;
	}
    }

  return child;
}

static gint
get_menu_height (GtkMenu *menu)
{
  gint height;
  GtkWidget *widget = GTK_WIDGET (menu);
  gint scroll_arrow_height;

  gtk_widget_style_get (GTK_WIDGET (menu),
                        "scroll-arrow-vlength", &scroll_arrow_height,
                        NULL);
  
  height = widget->requisition.height;
  height -= (GTK_CONTAINER (widget)->border_width + widget->style->ythickness) * 2;

  if (menu->upper_arrow_visible && !menu->tearoff_active)
    height -= scroll_arrow_height;

  if (menu->lower_arrow_visible && !menu->tearoff_active)
    height -= scroll_arrow_height;

  return height;
}

static void
gtk_menu_real_move_scroll (GtkMenu       *menu,
			   GtkScrollType  type)
{
  gint page_size = get_visible_size (menu);
  gint end_position = get_menu_height (menu);
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (menu);
  gint scroll_arrow_height;

  gtk_widget_style_get (GTK_WIDGET (menu),
                        "scroll-arrow-vlength", &scroll_arrow_height,
                        NULL);
  
  switch (type)
    {
    case GTK_SCROLL_PAGE_UP:
    case GTK_SCROLL_PAGE_DOWN:
      {
	gint old_offset;
        gint new_offset;
	gint child_offset = 0;
	gboolean old_upper_arrow_visible;
	gint step;

	if (type == GTK_SCROLL_PAGE_UP)
	  step = - page_size;
	else
	  step = page_size;

	if (menu_shell->active_menu_item)
	  {
	    gint child_height;
	    
	    compute_child_offset (menu, menu_shell->active_menu_item,
				  &child_offset, &child_height, NULL);
	    child_offset += child_height / 2;
	  }

	menu_shell->ignore_enter = TRUE;
	old_upper_arrow_visible = menu->upper_arrow_visible && !menu->tearoff_active;
	old_offset = menu->scroll_offset;

        new_offset = menu->scroll_offset + step;
        new_offset = CLAMP (new_offset, 0, end_position - page_size);

        gtk_menu_scroll_to (menu, new_offset);
	
	if (menu_shell->active_menu_item)
	  {
	    GtkWidget *new_child;
	    gboolean new_upper_arrow_visible = menu->upper_arrow_visible && !menu->tearoff_active;

	    if (menu->scroll_offset != old_offset)
	      step = menu->scroll_offset - old_offset;

	    step -= (new_upper_arrow_visible - old_upper_arrow_visible) * scroll_arrow_height;

	    new_child = child_at (menu, child_offset + step);
	    if (new_child)
	      gtk_menu_shell_select_item (menu_shell, new_child);
	  }
      }
      break;
    case GTK_SCROLL_START:
      /* Ignore the enter event we might get if the pointer is on the menu
       */
      menu_shell->ignore_enter = TRUE;
      gtk_menu_scroll_to (menu, 0);
      gtk_menu_shell_select_first (menu_shell, TRUE);
      break;
    case GTK_SCROLL_END:
      /* Ignore the enter event we might get if the pointer is on the menu
       */
      menu_shell->ignore_enter = TRUE;
      gtk_menu_scroll_to (menu, end_position - page_size);
      _gtk_menu_shell_select_last (menu_shell, TRUE);
      break;
    default:
      break;
    }
}


/**
 * gtk_menu_set_monitor:
 * @menu: a #GtkMenu
 * @monitor_num: the number of the monitor on which the menu should
 *    be popped up
 * 
 * Informs GTK+ on which monitor a menu should be popped up. 
 * See gdk_screen_get_monitor_geometry().
 *
 * This function should be called from a #GtkMenuPositionFunc if the
 * menu should not appear on the same monitor as the pointer. This 
 * information can't be reliably inferred from the coordinates returned
 * by a #GtkMenuPositionFunc, since, for very long menus, these coordinates 
 * may extend beyond the monitor boundaries or even the screen boundaries. 
 *
 * Since: 2.4
 **/
void
gtk_menu_set_monitor (GtkMenu *menu,
		      gint     monitor_num)
{
  GtkMenuPrivate *priv;
  g_return_if_fail (GTK_IS_MENU (menu));

  priv = gtk_menu_get_private (menu);
  
  priv->monitor_num = monitor_num;
}

/**
 * gtk_menu_get_for_attach_widget:
 * @widget: a #GtkWidget
 *
 * Returns a list of the menus which are attached to this widget.
 * This list is owned by GTK+ and must not be modified.
 *
 * Return value: the list of menus attached to his widget.
 *
 * Since: 2.6
 **/
GList*
gtk_menu_get_for_attach_widget (GtkWidget *widget)
{
  GList *list;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  list = g_object_get_data (G_OBJECT (widget), ATTACHED_MENUS);
  return list;
}

static void
gtk_menu_grab_notify (GtkWidget *widget,
		      gboolean   was_grabbed)
{
  GtkWidget *toplevel;
  GtkWindowGroup *group;
  GtkWidget *grab;

  toplevel = gtk_widget_get_toplevel (widget);
  group = gtk_window_get_group (GTK_WINDOW (toplevel));
  grab = _gtk_window_group_get_current_grab (group); 

  if (!was_grabbed)
    {
      if (GTK_MENU_SHELL (widget)->active && !GTK_IS_MENU_SHELL (grab))
        gtk_menu_shell_cancel (GTK_MENU_SHELL (widget));
    }
}

#define __GTK_MENU_C__
#include "gtkaliasdef.c"
