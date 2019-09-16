/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkToolbar copyright (C) Federico Mena
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2003, 2004 Soeren Sandmann <sandmann@daimi.au.dk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */


#include "config.h"

#include <math.h>
#include <string.h>

#include "gtktoolbar.h"
#include "gtktoolbarprivate.h"

#include "gtkbindings.h"
#include "gtkbox.h"
#include "gtkcontainerprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkorientable.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtkpopovermenu.h"
#include "gtkmodelbutton.h"
#include "gtkseparator.h"
#include "gtkradiomenuitem.h"
#include "gtkcheckmenuitem.h"
#include "gtkradiobutton.h"
#include "gtkradiotoolbutton.h"
#include "gtkseparatormenuitem.h"
#include "gtkseparatortoolitem.h"
#include "gtktoolshell.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetpath.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtkgestureclick.h"
#include "gtkbuttonprivate.h"


/**
 * SECTION:gtktoolbar
 * @Short_description: Create bars of buttons and other widgets
 * @Title: GtkToolbar
 * @See_also: #GtkToolItem
 *
 * A toolbar is created with a call to gtk_toolbar_new().
 *
 * A toolbar can contain instances of a subclass of #GtkToolItem. To add
 * a #GtkToolItem to the a toolbar, use gtk_toolbar_insert(). To remove
 * an item from the toolbar use gtk_container_remove(). To add a button
 * to the toolbar, add an instance of #GtkToolButton.
 *
 * Toolbar items can be visually grouped by adding instances of
 * #GtkSeparatorToolItem to the toolbar. If the GtkToolItem property
 * “expand” is #TRUE and the property #GtkSeparatorToolItem:draw is set to
 * #FALSE, the effect is to force all following items to the end of the toolbar.
 *
 * By default, a toolbar can be shrunk, upon which it will add an arrow button
 * to show an overflow menu offering access to any #GtkToolItem child that has
 * a proxy menu item. To disable this and request enough size for all children,
 * call gtk_toolbar_set_show_arrow() to set #GtkToolbar:show-arrow to %FALSE.
 *
 * Creating a context menu for the toolbar can be done by connecting to
 * the #GtkToolbar::popup-context-menu signal.
 *
 * # CSS nodes
 *
 * GtkToolbar has a single CSS node with name toolbar.
 */


typedef struct _ToolbarContent ToolbarContent;

#define DEFAULT_TOOLBAR_STYLE GTK_TOOLBAR_BOTH_HORIZ
#define DEFAULT_ANIMATION_STATE TRUE

#define MAX_HOMOGENEOUS_N_CHARS 13 /* Items that are wider than this do not participate
				    * in the homogeneous game. In units of
				    * pango_font_get_estimated_char_width().
				    */
#define SLIDE_SPEED 600.0	   /* How fast the items slide, in pixels per second */
#define ACCEL_THRESHOLD 0.18	   /* After how much time in seconds will items start speeding up */

typedef struct _GtkToolbarPrivate       GtkToolbarPrivate;
typedef struct _GtkToolbarClass         GtkToolbarClass;

struct _GtkToolbar
{
  GtkContainer container;

  GtkToolbarPrivate *priv;
};

struct _GtkToolbarClass
{
  GtkContainerClass parent_class;

  void     (* orientation_changed) (GtkToolbar       *toolbar,
                                    GtkOrientation    orientation);
  void     (* style_changed)       (GtkToolbar       *toolbar,
                                    GtkToolbarStyle   style);
  gboolean (* popup_context_menu)  (GtkToolbar       *toolbar,
                                    gint              x,
                                    gint              y,
                                    gint              button_number);
};

struct _GtkToolbarPrivate
{
  GtkWidget       *menu;
  GtkWidget       *menu_box;
  GtkSettings     *settings;

  GtkToolbarStyle  style;

  GtkToolItem     *highlight_tool_item;
  GtkWidget       *arrow;
  GtkWidget       *arrow_button;

  GtkAllocation    prev_allocation;

  GList           *content;

  GTimer          *timer;

  gulong           settings_connection;

  gint             idle_id;
  gint             button_maxw;         /* maximum width of homogeneous children */
  gint             button_maxh;         /* maximum height of homogeneous children */
  gint             max_homogeneous_pixels;
  gint             num_children;

  GtkOrientation   orientation;

  guint            animation : 1;
  guint            is_sliding : 1;
  guint            need_rebuild : 1;  /* whether the overflow menu should be regenerated */
  guint            show_arrow : 1;
  guint            style_set     : 1;
};

/* Properties */
enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_TOOLBAR_STYLE,
  PROP_SHOW_ARROW,
  PROP_TOOLTIPS,
};

/* Signals */
enum {
  ORIENTATION_CHANGED,
  STYLE_CHANGED,
  POPUP_CONTEXT_MENU,
  FOCUS_HOME_OR_END,
  LAST_SIGNAL
};

typedef enum {
  NOT_ALLOCATED,
  NORMAL,
  HIDDEN,
  OVERFLOWN
} ItemState;


static void       gtk_toolbar_set_property         (GObject             *object,
						    guint                prop_id,
						    const GValue        *value,
						    GParamSpec          *pspec);
static void       gtk_toolbar_get_property         (GObject             *object,
						    guint                prop_id,
						    GValue              *value,
						    GParamSpec          *pspec);
static void       gtk_toolbar_snapshot             (GtkWidget           *widget,
                                                    GtkSnapshot         *snapshot);
static void       gtk_toolbar_size_allocate        (GtkWidget           *widget,
                                                    int                  width,
                                                    int                  height,
                                                    int                  baseline);
static void       gtk_toolbar_style_updated        (GtkWidget           *widget);
static gboolean   gtk_toolbar_focus                (GtkWidget           *widget,
						    GtkDirectionType     dir);
static void       gtk_toolbar_move_focus           (GtkWidget           *widget,
						    GtkDirectionType     dir);
static void       gtk_toolbar_root                 (GtkWidget           *widget);
static void       gtk_toolbar_unroot               (GtkWidget           *widget);
static void       gtk_toolbar_finalize             (GObject             *object);
static void       gtk_toolbar_dispose              (GObject             *object);
static void       gtk_toolbar_add                  (GtkContainer        *container,
						    GtkWidget           *widget);
static void       gtk_toolbar_remove               (GtkContainer        *container,
						    GtkWidget           *widget);
static void       gtk_toolbar_forall               (GtkContainer        *container,
						    GtkCallback          callback,
						    gpointer             callback_data);
static GType      gtk_toolbar_child_type           (GtkContainer        *container);

static void       gtk_toolbar_orientation_changed  (GtkToolbar          *toolbar,
						    GtkOrientation       orientation);
static void       gtk_toolbar_real_style_changed   (GtkToolbar          *toolbar,
						    GtkToolbarStyle      style);
static gboolean   gtk_toolbar_focus_home_or_end    (GtkToolbar          *toolbar,
						    gboolean             focus_home);
static void       gtk_toolbar_arrow_button_press   (GtkGesture          *gesture,
                                                    int                  n_press,
                                                    double               x,
                                                    double               y,
						    GtkToolbar          *toolbar);
static void       gtk_toolbar_arrow_button_clicked (GtkWidget           *button,
						    GtkToolbar          *toolbar);
static gboolean   gtk_toolbar_popup_menu           (GtkWidget           *toolbar);
static void       gtk_toolbar_reconfigured         (GtkToolbar          *toolbar);

static void       gtk_toolbar_measure              (GtkWidget      *widget,
                                                    GtkOrientation  orientation,
                                                    int             for_size,
                                                    int            *minimum,
                                                    int            *natural,
                                                    int            *minimum_baseline,
                                                    int            *natural_baseline);
static void       gtk_toolbar_pressed_cb           (GtkGestureClick *gesture,
                                                    int                   n_press,
                                                    double                x,
                                                    double                y,
                                                    gpointer              user_data);


/* methods on ToolbarContent 'class' */
static ToolbarContent *toolbar_content_new_tool_item        (GtkToolbar          *toolbar,
							     GtkToolItem         *item,
							     gboolean             is_placeholder,
							     gint                 pos);
static void            toolbar_content_remove               (ToolbarContent      *content,
							     GtkToolbar          *toolbar);
static void            toolbar_content_free                 (ToolbarContent      *content);
static void            toolbar_content_snapshot             (ToolbarContent      *content,
							     GtkContainer        *container,
                                                             GtkSnapshot         *snapshot);
static gboolean        toolbar_content_visible              (ToolbarContent      *content,
							     GtkToolbar          *toolbar);
static void            toolbar_content_size_request         (ToolbarContent      *content,
							     GtkToolbar          *toolbar,
							     GtkRequisition      *requisition);
static gboolean        toolbar_content_is_homogeneous       (ToolbarContent      *content,
							     GtkToolbar          *toolbar);
static gboolean        toolbar_content_is_placeholder       (ToolbarContent      *content);
static gboolean        toolbar_content_disappearing         (ToolbarContent      *content);
static ItemState       toolbar_content_get_state            (ToolbarContent      *content);
static gboolean        toolbar_content_child_visible        (ToolbarContent      *content);
static void            toolbar_content_get_goal_allocation  (ToolbarContent      *content,
							     GtkAllocation       *allocation);
static void            toolbar_content_get_allocation       (ToolbarContent      *content,
							     GtkAllocation       *allocation);
static void            toolbar_content_set_start_allocation (ToolbarContent      *content,
							     GtkAllocation       *new_start_allocation);
static void            toolbar_content_get_start_allocation (ToolbarContent      *content,
							     GtkAllocation       *start_allocation);
static gboolean        toolbar_content_get_expand           (ToolbarContent      *content,
                                                             GtkOrientation       orientation);
static void            toolbar_content_set_goal_allocation  (ToolbarContent      *content,
							     GtkAllocation       *allocation);
static void            toolbar_content_set_child_visible    (ToolbarContent      *content,
							     GtkToolbar          *toolbar,
							     gboolean             visible);
static void            toolbar_content_size_allocate        (ToolbarContent      *content,
							     GtkAllocation       *allocation);
static void            toolbar_content_set_state            (ToolbarContent      *content,
							     ItemState            new_state);
static GtkWidget *     toolbar_content_get_widget           (ToolbarContent      *content);
static void            toolbar_content_set_disappearing     (ToolbarContent      *content,
							     gboolean             disappearing);
static void            toolbar_content_set_size_request     (ToolbarContent      *content,
							     gint                 width,
							     gint                 height);
static void            toolbar_content_toolbar_reconfigured (ToolbarContent      *content,
							     GtkToolbar          *toolbar);
static GtkWidget *     toolbar_content_retrieve_menu_item   (ToolbarContent      *content);
static gboolean        toolbar_content_has_proxy_menu_item  (ToolbarContent	 *content);
static gboolean        toolbar_content_is_separator         (ToolbarContent      *content);
static void	       toolbar_content_set_expand	    (ToolbarContent      *content,
							     gboolean		  expand);

static void            toolbar_tool_shell_iface_init        (GtkToolShellIface   *iface);
static GtkOrientation  toolbar_get_orientation              (GtkToolShell        *shell);
static GtkToolbarStyle toolbar_get_style                    (GtkToolShell        *shell);
static void            toolbar_rebuild_menu                 (GtkToolShell        *shell);


G_DEFINE_TYPE_WITH_CODE (GtkToolbar, gtk_toolbar, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkToolbar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TOOL_SHELL,
                                                toolbar_tool_shell_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE,
                                                NULL))

static guint toolbar_signals[LAST_SIGNAL] = { 0 };


static void
add_arrow_bindings (GtkBindingSet   *binding_set,
		    guint            keysym,
		    GtkDirectionType dir)
{
  guint keypad_keysym = keysym - GDK_KEY_Left + GDK_KEY_KP_Left;
  
  gtk_binding_entry_add_signal (binding_set, keysym, 0,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, dir);
  gtk_binding_entry_add_signal (binding_set, keypad_keysym, 0,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, dir);
}

static void
add_ctrl_tab_bindings (GtkBindingSet    *binding_set,
		       GdkModifierType   modifiers,
		       GtkDirectionType  direction)
{
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_Tab, GDK_CONTROL_MASK | modifiers,
				"move-focus", 1,
				GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_KP_Tab, GDK_CONTROL_MASK | modifiers,
				"move-focus", 1,
				GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
gtk_toolbar_class_init (GtkToolbarClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkBindingSet *binding_set;
  
  gobject_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;
  container_class = (GtkContainerClass *)klass;
  
  gobject_class->set_property = gtk_toolbar_set_property;
  gobject_class->get_property = gtk_toolbar_get_property;
  gobject_class->finalize = gtk_toolbar_finalize;
  gobject_class->dispose = gtk_toolbar_dispose;
  
  widget_class->snapshot = gtk_toolbar_snapshot;
  widget_class->measure = gtk_toolbar_measure;
  widget_class->size_allocate = gtk_toolbar_size_allocate;
  widget_class->style_updated = gtk_toolbar_style_updated;
  widget_class->focus = gtk_toolbar_focus;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_TOOL_BAR);

  /* need to override the base class function via override_class_handler,
   * because the signal slot is not available in GtkWidgetClass
   */
  g_signal_override_class_handler ("move-focus",
                                   GTK_TYPE_TOOLBAR,
                                   G_CALLBACK (gtk_toolbar_move_focus));

  widget_class->root = gtk_toolbar_root;
  widget_class->unroot = gtk_toolbar_unroot;
  widget_class->popup_menu = gtk_toolbar_popup_menu;

  container_class->add    = gtk_toolbar_add;
  container_class->remove = gtk_toolbar_remove;
  container_class->forall = gtk_toolbar_forall;
  container_class->child_type = gtk_toolbar_child_type;

  klass->orientation_changed = gtk_toolbar_orientation_changed;
  klass->style_changed = gtk_toolbar_real_style_changed;
  
  /**
   * GtkToolbar::orientation-changed:
   * @toolbar: the object which emitted the signal
   * @orientation: the new #GtkOrientation of the toolbar
   *
   * Emitted when the orientation of the toolbar changes.
   */
  toolbar_signals[ORIENTATION_CHANGED] =
    g_signal_new (I_("orientation-changed"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkToolbarClass, orientation_changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_ORIENTATION);
  /**
   * GtkToolbar::style-changed:
   * @toolbar: The #GtkToolbar which emitted the signal
   * @style: the new #GtkToolbarStyle of the toolbar
   *
   * Emitted when the style of the toolbar changes. 
   */
  toolbar_signals[STYLE_CHANGED] =
    g_signal_new (I_("style-changed"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkToolbarClass, style_changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_TOOLBAR_STYLE);

  /**
   * GtkToolbar::popup-context-menu:
   * @toolbar: the #GtkToolbar which emitted the signal
   * @x: the x coordinate of the point where the menu should appear
   * @y: the y coordinate of the point where the menu should appear
   * @button: the mouse button the user pressed, or -1
   *
   * Emitted when the user right-clicks the toolbar or uses the
   * keybinding to display a popup menu.
   *
   * Application developers should handle this signal if they want
   * to display a context menu on the toolbar. The context-menu should
   * appear at the coordinates given by @x and @y. The mouse button
   * number is given by the @button parameter. If the menu was popped
   * up using the keyboard, @button is -1.
   *
   * Returns: return %TRUE if the signal was handled, %FALSE if not
   */
  toolbar_signals[POPUP_CONTEXT_MENU] =
    g_signal_new (I_("popup-context-menu"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkToolbarClass, popup_context_menu),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__INT_INT_INT,
		  G_TYPE_BOOLEAN, 3,
		  G_TYPE_INT, G_TYPE_INT,
		  G_TYPE_INT);

  /**
   * GtkToolbar::focus-home-or-end:
   * @toolbar: the #GtkToolbar which emitted the signal
   * @focus_home: %TRUE if the first item should be focused
   *
   * A keybinding signal used internally by GTK+. This signal can't
   * be used in application code
   *
   * Returns: %TRUE if the signal was handled, %FALSE if not
   */
  toolbar_signals[FOCUS_HOME_OR_END] =
    g_signal_new_class_handler (I_("focus-home-or-end"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_toolbar_focus_home_or_end),
                                NULL, NULL,
                                _gtk_marshal_BOOLEAN__BOOLEAN,
                                G_TYPE_BOOLEAN, 1,
                                G_TYPE_BOOLEAN);

  /* properties */
  g_object_class_override_property (gobject_class,
                                    PROP_ORIENTATION,
                                    "orientation");

  g_object_class_install_property (gobject_class,
				   PROP_TOOLBAR_STYLE,
				   g_param_spec_enum ("toolbar-style",
 						      P_("Toolbar Style"),
 						      P_("How to draw the toolbar"),
 						      GTK_TYPE_TOOLBAR_STYLE,
 						      DEFAULT_TOOLBAR_STYLE,
 						      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
				   PROP_SHOW_ARROW,
				   g_param_spec_boolean ("show-arrow",
							 P_("Show Arrow"),
							 P_("If an arrow should be shown if the toolbar doesn’t fit"),
							 TRUE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  binding_set = gtk_binding_set_by_class (klass);
  
  add_arrow_bindings (binding_set, GDK_KEY_Left, GTK_DIR_LEFT);
  add_arrow_bindings (binding_set, GDK_KEY_Right, GTK_DIR_RIGHT);
  add_arrow_bindings (binding_set, GDK_KEY_Up, GTK_DIR_UP);
  add_arrow_bindings (binding_set, GDK_KEY_Down, GTK_DIR_DOWN);
  
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Home, 0,
                                "focus-home-or-end", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Home, 0,
                                "focus-home-or-end", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_End, 0,
                                "focus-home-or-end", 1,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_End, 0,
                                "focus-home-or-end", 1,
				G_TYPE_BOOLEAN, FALSE);
  
  add_ctrl_tab_bindings (binding_set, 0, GTK_DIR_TAB_FORWARD);
  add_ctrl_tab_bindings (binding_set, GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);

  gtk_widget_class_set_css_name (widget_class, I_("toolbar"));
}

static void
toolbar_tool_shell_iface_init (GtkToolShellIface *iface)
{
  iface->get_orientation  = toolbar_get_orientation;
  iface->get_style        = toolbar_get_style;
  iface->rebuild_menu     = toolbar_rebuild_menu;
}

static void
gtk_toolbar_init (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv;
  GtkWidget *widget;
  GtkGesture *gesture;

  widget = GTK_WIDGET (toolbar);
  toolbar->priv = gtk_toolbar_get_instance_private (toolbar);
  priv = toolbar->priv;

  gtk_widget_set_can_focus (widget, FALSE);

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  priv->style = DEFAULT_TOOLBAR_STYLE;
  priv->animation = DEFAULT_ANIMATION_STATE;

  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (toolbar));

  priv->arrow_button = gtk_toggle_button_new ();
  g_signal_connect (gtk_button_get_gesture (GTK_BUTTON (priv->arrow_button)), "pressed",
		    G_CALLBACK (gtk_toolbar_arrow_button_press), toolbar);
  g_signal_connect (priv->arrow_button, "clicked",
		    G_CALLBACK (gtk_toolbar_arrow_button_clicked), toolbar);

  gtk_widget_set_focus_on_click (priv->arrow_button, FALSE);

  priv->arrow = gtk_image_new_from_icon_name ("pan-down-symbolic");
  gtk_widget_set_name (priv->arrow, "gtk-toolbar-arrow");
  gtk_container_add (GTK_CONTAINER (priv->arrow_button), priv->arrow);
  
  gtk_widget_set_parent (priv->arrow_button, widget);
  
  /* which child position a drop will occur at */
  priv->menu = NULL;
  priv->show_arrow = TRUE;
  priv->settings = NULL;
  
  priv->max_homogeneous_pixels = -1;
  
  priv->timer = g_timer_new ();

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_BUBBLE);
  g_signal_connect (gesture, "pressed", G_CALLBACK (gtk_toolbar_pressed_cb), toolbar);
  gtk_widget_add_controller (GTK_WIDGET (toolbar), GTK_EVENT_CONTROLLER (gesture));
}

static void
gtk_toolbar_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_signal_emit (toolbar, toolbar_signals[ORIENTATION_CHANGED], 0,
                     g_value_get_enum (value));
      break;
    case PROP_TOOLBAR_STYLE:
      gtk_toolbar_set_style (toolbar, g_value_get_enum (value));
      break;
    case PROP_SHOW_ARROW:
      gtk_toolbar_set_show_arrow (toolbar, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_toolbar_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (object);
  GtkToolbarPrivate *priv = toolbar->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_TOOLBAR_STYLE:
      g_value_set_enum (value, priv->style);
      break;
    case PROP_SHOW_ARROW:
      g_value_set_boolean (value, priv->show_arrow);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_toolbar_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolbarPrivate *priv = toolbar->priv;
  GList *list;

  for (list = priv->content; list != NULL; list = list->next)
    {
      ToolbarContent *content = list->data;

      toolbar_content_snapshot (content, GTK_CONTAINER (widget), snapshot);
    }

  gtk_widget_snapshot_child (widget,
                             priv->arrow_button,
                             snapshot);
}

static void
gtk_toolbar_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolbarPrivate *priv = toolbar->priv;
  GList *list;
  gint max_child_height;
  gint max_child_width;
  gint max_homogeneous_child_width;
  gint max_homogeneous_child_height;
  gint homogeneous_size;
  gint pack_front_size;
  GtkRequisition arrow_requisition, min_requisition, nat_requisition;

  max_homogeneous_child_width = 0;
  max_homogeneous_child_height = 0;
  max_child_width = 0;
  max_child_height = 0;
  for (list = priv->content; list != NULL; list = list->next)
    {
      GtkRequisition requisition;
      ToolbarContent *content = list->data;
      
      if (!toolbar_content_visible (content, toolbar))
	continue;
      
      toolbar_content_size_request (content, toolbar, &requisition);

      max_child_width = MAX (max_child_width, requisition.width);
      max_child_height = MAX (max_child_height, requisition.height);
      
      if (toolbar_content_is_homogeneous (content, toolbar))
	{
	  max_homogeneous_child_width = MAX (max_homogeneous_child_width, requisition.width);
	  max_homogeneous_child_height = MAX (max_homogeneous_child_height, requisition.height);
	}
    }
  
  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    homogeneous_size = max_homogeneous_child_width;
  else
    homogeneous_size = max_homogeneous_child_height;
  
  pack_front_size = 0;
  for (list = priv->content; list != NULL; list = list->next)
    {
      ToolbarContent *content = list->data;
      guint size;
      
      if (!toolbar_content_visible (content, toolbar))
	continue;

      if (toolbar_content_is_homogeneous (content, toolbar))
	{
	  size = homogeneous_size;
	}
      else
	{
	  GtkRequisition requisition;
	  
	  toolbar_content_size_request (content, toolbar, &requisition);
	  
	  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	    size = requisition.width;
	  else
	    size = requisition.height;
	}

      pack_front_size += size;
    }

  arrow_requisition.height = 0;
  arrow_requisition.width = 0;
  
  if (priv->show_arrow)
    gtk_widget_get_preferred_size (priv->arrow_button,
                                     &arrow_requisition, NULL);
  
  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      nat_requisition.width = pack_front_size;
      nat_requisition.height = MAX (max_child_height, arrow_requisition.height);

      min_requisition.width = priv->show_arrow ? arrow_requisition.width : nat_requisition.width;
      min_requisition.height = nat_requisition.height;
    }
  else
    {
      nat_requisition.width = MAX (max_child_width, arrow_requisition.width);
      nat_requisition.height = pack_front_size;

      min_requisition.width = nat_requisition.width;
      min_requisition.height = priv->show_arrow ? arrow_requisition.height : nat_requisition.height;
    }

  priv->button_maxw = max_homogeneous_child_width;
  priv->button_maxh = max_homogeneous_child_height;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = min_requisition.width;
      *natural = nat_requisition.width;
    }
  else
    {
      *minimum = min_requisition.height;
      *natural = nat_requisition.height;
    }
}

static gint
position (GtkToolbar *toolbar,
          gint        from,
          gint        to,
          gdouble     elapsed)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  gint n_pixels;

  if (!priv->animation)
    return to;

  if (elapsed <= ACCEL_THRESHOLD)
    {
      n_pixels = SLIDE_SPEED * elapsed;
    }
  else
    {
      /* The formula is a second degree polynomial in
       * @elapsed that has the line SLIDE_SPEED * @elapsed
       * as tangent for @elapsed == ACCEL_THRESHOLD.
       * This makes @n_pixels a smooth function of elapsed time.
       */
      n_pixels = (SLIDE_SPEED / ACCEL_THRESHOLD) * elapsed * elapsed -
	SLIDE_SPEED * elapsed + SLIDE_SPEED * ACCEL_THRESHOLD;
    }

  if (to > from)
    return MIN (from + n_pixels, to);
  else
    return MAX (from - n_pixels, to);
}

static void
compute_intermediate_allocation (GtkToolbar          *toolbar,
				 const GtkAllocation *start,
				 const GtkAllocation *goal,
				 GtkAllocation       *intermediate)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  gdouble elapsed = g_timer_elapsed (priv->timer, NULL);

  intermediate->x      = position (toolbar, start->x, goal->x, elapsed);
  intermediate->y      = position (toolbar, start->y, goal->y, elapsed);
  intermediate->width  = position (toolbar, start->x + start->width,
                                   goal->x + goal->width,
                                   elapsed) - intermediate->x;
  intermediate->height = position (toolbar, start->y + start->height,
                                   goal->y + goal->height,
                                   elapsed) - intermediate->y;
}

static void
fixup_allocation_for_rtl (gint           total_size,
			  GtkAllocation *allocation)
{
  allocation->x += (total_size - (2 * allocation->x + allocation->width));
}

static void
fixup_allocation_for_vertical (GtkAllocation *allocation)
{
  gint tmp;
  
  tmp = allocation->x;
  allocation->x = allocation->y;
  allocation->y = tmp;
  
  tmp = allocation->width;
  allocation->width = allocation->height;
  allocation->height = tmp;
}

static gint
get_item_size (GtkToolbar     *toolbar,
	       ToolbarContent *content)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  GtkRequisition requisition;
  
  toolbar_content_size_request (content, toolbar, &requisition);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (toolbar_content_is_homogeneous (content, toolbar))
	return priv->button_maxw;
      else
	return requisition.width;
    }
  else
    {
      if (toolbar_content_is_homogeneous (content, toolbar))
	return priv->button_maxh;
      else
	return requisition.height;
    }
}

static gboolean
slide_idle_handler (gpointer data)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (data);
  GtkToolbarPrivate *priv = toolbar->priv;
  GList *list;

  for (list = priv->content; list != NULL; list = list->next)
    {
      ToolbarContent *content = list->data;
      ItemState state;
      GtkAllocation goal_allocation;
      GtkAllocation allocation;
      gboolean cont;

      state = toolbar_content_get_state (content);
      toolbar_content_get_goal_allocation (content, &goal_allocation);
      toolbar_content_get_allocation (content, &allocation);
      
      cont = FALSE;
      
      if (state == NOT_ALLOCATED)
	{
	  /* an unallocated item means that size allocate has to
	   * called at least once more
	   */
	  cont = TRUE;
	}

      /* An invisible item with a goal allocation of
       * 0 is already at its goal.
       */
      if ((state == NORMAL || state == OVERFLOWN) &&
	  ((goal_allocation.width != 0 &&
	    goal_allocation.height != 0) ||
	   toolbar_content_child_visible (content)))
	{
	  if ((goal_allocation.x != allocation.x ||
	       goal_allocation.y != allocation.y ||
	       goal_allocation.width != allocation.width ||
	       goal_allocation.height != allocation.height))
	    {
	      /* An item is not in its right position yet. Note
	       * that OVERFLOWN items do get an allocation in
	       * gtk_toolbar_size_allocate(). This way you can see
	       * them slide back in when you drag an item off the
	       * toolbar.
	       */
	      cont = TRUE;
	    }
	}

      if (toolbar_content_is_placeholder (content) &&
	  toolbar_content_disappearing (content) &&
	  toolbar_content_child_visible (content))
	{
	  /* A disappearing placeholder is still visible.
	   */
	     
	  cont = TRUE;
	}
      
      if (cont)
	{
	  gtk_widget_queue_resize (GTK_WIDGET (toolbar));
	  
	  return TRUE;
	}
    }
  
  gtk_widget_queue_resize (GTK_WIDGET (toolbar));

  priv->is_sliding = FALSE;
  priv->idle_id = 0;

  return FALSE;
}

static gboolean
rect_within (GtkAllocation *a1,
	     GtkAllocation *a2)
{
  return (a1->x >= a2->x                         &&
	  a1->x + a1->width <= a2->x + a2->width &&
	  a1->y >= a2->y			 &&
	  a1->y + a1->height <= a2->y + a2->height);
}

static void
gtk_toolbar_begin_sliding (GtkToolbar *toolbar)
{
  GtkAllocation content_allocation;
  GtkWidget *widget = GTK_WIDGET (toolbar);
  GtkToolbarPrivate *priv = toolbar->priv;
  GList *list;
  gint cur_x;
  gint cur_y;
  gboolean rtl;
  gboolean vertical;
  
  /* Start the sliding. This function copies the allocation of every
   * item into content->start_allocation. For items that haven't
   * been allocated yet, we calculate their position and save that
   * in start_allocatino along with zero width and zero height.
   *
   * FIXME: It would be nice if we could share this code with
   * the equivalent in gtk_widget_size_allocate().
   */
  priv->is_sliding = TRUE;
  
  if (!priv->idle_id)
    {
      priv->idle_id = g_idle_add (slide_idle_handler, toolbar);
      g_source_set_name_by_id (priv->idle_id, "[gtk] slide_idle_handler");
    }

  content_allocation.x = 0;
  content_allocation.y = 0;
  content_allocation.width = gtk_widget_get_width (widget);
  content_allocation.height = gtk_widget_get_height (widget);

  rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
  vertical = (priv->orientation == GTK_ORIENTATION_VERTICAL);

  if (rtl)
    {
      cur_x = content_allocation.width;
      cur_y = content_allocation.height;
    }
  else
    {
      cur_x = 0;
      cur_y = 0;
    }

  for (list = priv->content; list != NULL; list = list->next)
    {
      ToolbarContent *content = list->data;
      GtkAllocation new_start_allocation;
      GtkAllocation item_allocation;
      ItemState state;
      
      state = toolbar_content_get_state (content);
      toolbar_content_get_allocation (content, &item_allocation);
      
      if ((state == NORMAL &&
	   rect_within (&item_allocation, &content_allocation)) ||
	  state == OVERFLOWN)
	{
	  new_start_allocation = item_allocation;
	}
      else
	{
	  new_start_allocation.x = cur_x;
	  new_start_allocation.y = cur_y;
	  
	  if (vertical)
	    {
	      new_start_allocation.width = content_allocation.width;
	      new_start_allocation.height = 0;
	    }
	  else
	    {
	      new_start_allocation.width = 0;
	      new_start_allocation.height = content_allocation.height;
	    }
	}
      
      if (vertical)
	cur_y = new_start_allocation.y + new_start_allocation.height;
      else if (rtl)
	cur_x = new_start_allocation.x;
      else
	cur_x = new_start_allocation.x + new_start_allocation.width;
      
      toolbar_content_set_start_allocation (content, &new_start_allocation);
    }

  /* This resize will run before the first idle handler. This
   * will make sure that items get the right goal allocation
   * so that the idle handler will not immediately return
   * FALSE
   */
  gtk_widget_queue_resize (GTK_WIDGET (toolbar));
  g_timer_reset (priv->timer);
}

static void
gtk_toolbar_stop_sliding (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = toolbar->priv;

  if (priv->is_sliding)
    {
      GList *list;
      
      priv->is_sliding = FALSE;
      
      if (priv->idle_id)
	{
	  g_source_remove (priv->idle_id);
	  priv->idle_id = 0;
	}
      
      list = priv->content;
      while (list)
	{
	  ToolbarContent *content = list->data;
	  list = list->next;

	  if (toolbar_content_is_placeholder (content))
	    {
	      toolbar_content_remove (content, toolbar);
	      toolbar_content_free (content);
	    }
	}
      
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
    }
}

static void
remove_item (GtkWidget *menu_item,
	     gpointer   data)
{
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (menu_item)),
                        menu_item);
}

static void
menu_deactivated (GtkWidget  *menu,
		  GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = toolbar->priv;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->arrow_button), FALSE);
}

static void
button_clicked (GtkWidget *button,
                GtkWidget *item)
{
  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
  if (GTK_IS_TOOL_BUTTON (item))
    g_signal_emit_by_name (_gtk_tool_button_get_button (GTK_TOOL_BUTTON (item)), "clicked");
}

static void
rebuild_menu (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  GList *list;

  if (!priv->menu)
    {
      priv->menu = gtk_popover_menu_new (priv->arrow_button);
      priv->menu_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_popover_menu_add_submenu (GTK_POPOVER_MENU (priv->menu), priv->menu_box, "main");

      g_signal_connect (priv->menu, "closed",
                        G_CALLBACK (menu_deactivated), toolbar);
    }

  gtk_container_foreach (GTK_CONTAINER (priv->menu_box), remove_item, NULL);

  for (list = priv->content; list != NULL; list = list->next)
    {
      ToolbarContent *content = list->data;

      if (toolbar_content_get_state (content) == OVERFLOWN &&
	  !toolbar_content_is_placeholder (content))
	{
	  GtkWidget *menu_item = toolbar_content_retrieve_menu_item (content);

	  if (menu_item)
	    {
              GtkWidget *button, *widget;
              const char *text;
              GtkButtonRole role;
              gboolean active;

	      g_assert (GTK_IS_MENU_ITEM (menu_item));
              text = gtk_menu_item_get_label (GTK_MENU_ITEM (menu_item));
              if (text == NULL)
                {
                  GtkWidget *box, *child;
                  box = gtk_bin_get_child (GTK_BIN (menu_item));
                  for (child = gtk_widget_get_first_child (box);
                       child;
                       child = gtk_widget_get_next_sibling (child))
                    {
                      if (GTK_IS_LABEL (child))
                        {
                          text = gtk_label_get_label (GTK_LABEL (child));
                          break;
                        }
                    }
                }

              if (GTK_IS_SEPARATOR_MENU_ITEM (menu_item))
                {
                  gtk_container_add (GTK_CONTAINER (priv->menu_box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
                  continue;
                }
              else if (GTK_IS_RADIO_MENU_ITEM (menu_item))
                role = GTK_BUTTON_ROLE_RADIO;
              else if (GTK_IS_CHECK_MENU_ITEM (menu_item))
                role = GTK_BUTTON_ROLE_CHECK;
              else
                role = GTK_BUTTON_ROLE_NORMAL;

              if (GTK_IS_CHECK_MENU_ITEM (menu_item))
                active = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menu_item));
              else
                active = FALSE;

              button = gtk_model_button_new ();
              g_object_set (button,
                            "text", text,
                            "role", role,
                            "active", active,
                            NULL);
              widget = toolbar_content_get_widget (content);
              g_signal_connect (button, "clicked",
                                G_CALLBACK (button_clicked), widget);
              gtk_container_add (GTK_CONTAINER (priv->menu_box), button);
	    }
	}
    }

  priv->need_rebuild = FALSE;
}

static void
gtk_toolbar_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolbarPrivate *priv = toolbar->priv;
  GtkAllocation arrow_allocation, item_area;
  GtkAllocation *allocations;
  ItemState *new_states;
  gint arrow_size;
  gint size, pos, short_size;
  GList *list;
  gint i;
  gboolean need_arrow;
  gint n_expand_items;
  gint available_size;
  gint n_items;
  gint needed_size;
  GtkRequisition arrow_requisition;
  gboolean overflowing;

  gtk_toolbar_stop_sliding (toolbar);

  gtk_widget_get_preferred_size (priv->arrow_button,
                                 &arrow_requisition, NULL);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      available_size = size = width;
      short_size = height;
      arrow_size = arrow_requisition.width;
    }
  else
    {
      available_size = size = height;
      short_size = width;
      arrow_size = arrow_requisition.height;
    }

  n_items = g_list_length (priv->content);
  allocations = g_new0 (GtkAllocation, n_items);
  new_states = g_new0 (ItemState, n_items);

  needed_size = 0;
  need_arrow = FALSE;
  for (list = priv->content; list != NULL; list = list->next)
    {
      ToolbarContent *content = list->data;

      if (toolbar_content_visible (content, toolbar))
        {
          needed_size += get_item_size (toolbar, content);

          /* Do we need an arrow?
           *
           * Assume we don't, and see if any non-separator item
           * with a proxy menu item is then going to overflow.
           */
          if (needed_size > available_size &&
              !need_arrow &&
              priv->show_arrow &&
              toolbar_content_has_proxy_menu_item (content) &&
              !toolbar_content_is_separator (content))
            {
              need_arrow = TRUE;
            }
        }
    }

  if (need_arrow)
    size = available_size - arrow_size;
  else
    size = available_size;

  /* calculate widths and states of items */
  overflowing = FALSE;
  for (list = priv->content, i = 0; list != NULL; list = list->next, ++i)
    {
      ToolbarContent *content = list->data;
      gint item_size;

      if (!toolbar_content_visible (content, toolbar))
        {
          new_states[i] = HIDDEN;
          continue;
        }

      item_size = get_item_size (toolbar, content);
      if (item_size <= size && !overflowing)
        {
          size -= item_size;
          allocations[i].width = item_size;
          new_states[i] = NORMAL;
        }
      else
        {
          overflowing = TRUE;
          new_states[i] = OVERFLOWN;
          allocations[i].width = item_size;
        }
    }

  /* calculate width of arrow */
  if (need_arrow)
    {
      arrow_allocation.width = arrow_size;
      arrow_allocation.height = MAX (short_size, 1);
    }

  /* expand expandable items */

  /* We don't expand when there is an overflow menu,
   * because that leads to weird jumps when items get
   * moved to the overflow menu and the expanding
   * items suddenly get a lot of extra space
   */
  if (!overflowing)
    {
      n_expand_items = 0;

      for (i = 0, list = priv->content; list != NULL; list = list->next, ++i)
        {
          ToolbarContent *content = list->data;

          if (toolbar_content_get_expand (content, priv->orientation) && new_states[i] == NORMAL)
            n_expand_items++;
        }

      for (list = priv->content, i = 0; list != NULL; list = list->next, ++i)
        {
          ToolbarContent *content = list->data;

          if (toolbar_content_get_expand (content, priv->orientation) && new_states[i] == NORMAL)
            {
              gint extra = size / n_expand_items;
              if (size % n_expand_items != 0)
                extra++;

              allocations[i].width += extra;
              size -= extra;
              n_expand_items--;
            }
        }

      g_assert (n_expand_items == 0);
    }

  /* position items */
  pos = 0;
  for (list = priv->content, i = 0; list != NULL; list = list->next, ++i)
    {
      /* Both NORMAL and OVERFLOWN items get a position.
       * This ensures that sliding will work for OVERFLOWN items too.
       */
      if (new_states[i] == NORMAL || new_states[i] == OVERFLOWN)
        {
          allocations[i].x = pos;
          allocations[i].y = 0;
          allocations[i].height = short_size;

          pos += allocations[i].width;
        }
    }

  /* position arrow */
  if (need_arrow)
    {
      arrow_allocation.x = available_size - arrow_allocation.width;
      arrow_allocation.y = 0;
    }

  item_area.x = 0;
  item_area.y = 0;
  item_area.width = available_size - (need_arrow? arrow_size : 0);
  item_area.height = short_size;

  /* fix up allocations in the vertical or RTL cases */
  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      for (i = 0; i < n_items; ++i)
        fixup_allocation_for_vertical (&(allocations[i]));

      if (need_arrow)
        fixup_allocation_for_vertical (&arrow_allocation);

      fixup_allocation_for_vertical (&item_area);
    }
  else if (gtk_widget_get_direction (GTK_WIDGET (toolbar)) == GTK_TEXT_DIR_RTL)
    {
      for (i = 0; i < n_items; ++i)
        fixup_allocation_for_rtl (available_size, &(allocations[i]));

      if (need_arrow)
        fixup_allocation_for_rtl (available_size, &arrow_allocation);

      fixup_allocation_for_rtl (available_size, &item_area);
    }

  /* did anything change? */
  for (list = priv->content, i = 0; list != NULL; list = list->next, i++)
    {
      ToolbarContent *content = list->data;

      if (toolbar_content_get_state (content) == NORMAL &&
          new_states[i] != NORMAL)
        {
          /* an item disappeared and we didn't change size, so begin sliding */
          gtk_toolbar_begin_sliding (toolbar);
        }
    }

  /* finally allocate the items */
  if (priv->is_sliding)
    {
      for (list = priv->content, i = 0; list != NULL; list = list->next, i++)
        {
          ToolbarContent *content = list->data;

          toolbar_content_set_goal_allocation (content, &(allocations[i]));
        }
    }

  for (list = priv->content, i = 0; list != NULL; list = list->next, ++i)
    {
      ToolbarContent *content = list->data;

      if (new_states[i] == OVERFLOWN || new_states[i] == NORMAL)
        {
          GtkAllocation alloc;
          GtkAllocation start_allocation = { 0, };
          GtkAllocation goal_allocation;

          if (priv->is_sliding)
            {
              toolbar_content_get_start_allocation (content, &start_allocation);
              toolbar_content_get_goal_allocation (content, &goal_allocation);

              compute_intermediate_allocation (toolbar,
                                               &start_allocation,
                                               &goal_allocation,
                                               &alloc);
            }
          else
            {
              alloc = allocations[i];
            }

          if (alloc.width <= 0 || alloc.height <= 0)
            {
              toolbar_content_set_child_visible (content, toolbar, FALSE);
            }
          else
            {
              if (!rect_within (&alloc, &item_area))
                {
                  toolbar_content_set_child_visible (content, toolbar, FALSE);
                  toolbar_content_size_allocate (content, &alloc);
                }
              else
                {
                  toolbar_content_set_child_visible (content, toolbar, TRUE);
                  toolbar_content_size_allocate (content, &alloc);
                }
            }
        }
      else
        {
          toolbar_content_set_child_visible (content, toolbar, FALSE);
        }

      toolbar_content_set_state (content, new_states[i]);
    }

  if (priv->menu && priv->need_rebuild)
    rebuild_menu (toolbar);

  if (need_arrow)
    {
      gtk_widget_size_allocate (GTK_WIDGET (priv->arrow_button), &arrow_allocation, -1);
      gtk_widget_show (GTK_WIDGET (priv->arrow_button));
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (priv->arrow_button));

      if (priv->menu && gtk_widget_get_visible (GTK_WIDGET (priv->menu)))
        gtk_menu_shell_deactivate (GTK_MENU_SHELL (priv->menu));
    }

  g_free (allocations);
  g_free (new_states);
}

static void
gtk_toolbar_style_updated (GtkWidget *widget)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolbarPrivate *priv = toolbar->priv;

  GTK_WIDGET_CLASS (gtk_toolbar_parent_class)->style_updated (widget);

  priv->max_homogeneous_pixels = -1;
}

static GList *
gtk_toolbar_list_children_in_focus_order (GtkToolbar       *toolbar,
					  GtkDirectionType  dir)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  GList *result = NULL;
  GList *list;
  gboolean rtl;
  
  /* generate list of children in reverse logical order */
  
  for (list = priv->content; list != NULL; list = list->next)
    {
      ToolbarContent *content = list->data;
      GtkWidget *widget;
      
      widget = toolbar_content_get_widget (content);
      
      if (widget)
	result = g_list_prepend (result, widget);
    }
  
  result = g_list_prepend (result, priv->arrow_button);
  
  rtl = (gtk_widget_get_direction (GTK_WIDGET (toolbar)) == GTK_TEXT_DIR_RTL);
  
  /* move in logical order when
   *
   *	- dir is TAB_FORWARD
   *
   *	- in RTL mode and moving left or up
   *
   *    - in LTR mode and moving right or down
   */
  if (dir == GTK_DIR_TAB_FORWARD                                        ||
      (rtl  && (dir == GTK_DIR_UP   || dir == GTK_DIR_LEFT))		||
      (!rtl && (dir == GTK_DIR_DOWN || dir == GTK_DIR_RIGHT)))
    {
      result = g_list_reverse (result);
    }
  
  return result;
}

static gboolean
gtk_toolbar_focus_home_or_end (GtkToolbar *toolbar,
			       gboolean    focus_home)
{
  GList *children, *list;
  GtkDirectionType dir = focus_home? GTK_DIR_RIGHT : GTK_DIR_LEFT;
  
  children = gtk_toolbar_list_children_in_focus_order (toolbar, dir);
  
  if (gtk_widget_get_direction (GTK_WIDGET (toolbar)) == GTK_TEXT_DIR_RTL)
    {
      children = g_list_reverse (children);
      
      dir = (dir == GTK_DIR_RIGHT)? GTK_DIR_LEFT : GTK_DIR_RIGHT;
    }
  
  for (list = children; list != NULL; list = list->next)
    {
      GtkWidget *child = list->data;

      if (gtk_widget_get_focus_child (GTK_WIDGET (toolbar)) == child)
	break;
      
      if (gtk_widget_get_mapped (child) && gtk_widget_child_focus (child, dir))
	break;
    }
  
  g_list_free (children);
  
  return TRUE;
}   

/* Keybinding handler. This function is called when the user presses
 * Ctrl TAB or an arrow key.
 */
static void
gtk_toolbar_move_focus (GtkWidget        *widget,
			GtkDirectionType  dir)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkWidget *focus_child;
  GList *list;
  gboolean try_focus = FALSE;
  GList *children;

  focus_child = gtk_widget_get_focus_child (widget);

  if (focus_child && gtk_widget_child_focus (focus_child, dir))
    return;
  
  children = gtk_toolbar_list_children_in_focus_order (toolbar, dir);
  
  for (list = children; list != NULL; list = list->next)
    {
      GtkWidget *child = list->data;
      
      if (try_focus && gtk_widget_get_mapped (child) && gtk_widget_child_focus (child, dir))
	break;
      
      if (child == focus_child)
	try_focus = TRUE;
    }
  
  g_list_free (children);
}

/* The focus handler for the toolbar. It called when the user presses
 * TAB or otherwise tries to focus the toolbar.
 */
static gboolean
gtk_toolbar_focus (GtkWidget        *widget,
		   GtkDirectionType  dir)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GList *children, *list;
  gboolean result = FALSE;

  /* if focus is already somewhere inside the toolbar then return FALSE.
   * The only way focus can stay inside the toolbar is when the user presses
   * arrow keys or Ctrl TAB (both of which are handled by the
   * gtk_toolbar_move_focus() keybinding function.
   */
  if (gtk_widget_get_focus_child (widget))
    return FALSE;

  children = gtk_toolbar_list_children_in_focus_order (toolbar, dir);

  for (list = children; list != NULL; list = list->next)
    {
      GtkWidget *child = list->data;
      
      if (gtk_widget_get_mapped (child) && gtk_widget_child_focus (child, dir))
	{
	  result = TRUE;
	  break;
	}
    }

  g_list_free (children);

  return result;
}

static GtkSettings *
toolbar_get_settings (GtkToolbar *toolbar)
{
  return toolbar->priv->settings;
}

static void
animation_change_notify (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  GtkSettings *settings = toolbar_get_settings (toolbar);
  gboolean animation;

  if (settings)
    g_object_get (settings,
                  "gtk-enable-animations", &animation,
                  NULL);
  else
    animation = DEFAULT_ANIMATION_STATE;

  priv->animation = animation;
}

static void
settings_change_notify (GtkSettings      *settings,
                        const GParamSpec *pspec,
                        GtkToolbar       *toolbar)
{
  if (! strcmp (pspec->name, "gtk-enable-animations"))
    animation_change_notify (toolbar);
}

static void
gtk_toolbar_root (GtkWidget *widget)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolbarPrivate *priv = toolbar->priv;
  GtkSettings *settings;

  GTK_WIDGET_CLASS (gtk_toolbar_parent_class)->root (widget);

  settings = gtk_widget_get_settings (GTK_WIDGET (toolbar));

  priv->settings_connection =
    g_signal_connect (settings, "notify",
                      G_CALLBACK (settings_change_notify),
                      toolbar);

  priv->settings = g_object_ref (settings);

  animation_change_notify (toolbar);
}

static void
gtk_toolbar_unroot (GtkWidget *widget)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolbarPrivate *priv = toolbar->priv;

  if (priv->settings_connection)
    g_signal_handler_disconnect (priv->settings, priv->settings_connection);
  priv->settings_connection = 0;
  g_clear_object (&priv->settings);

  GTK_WIDGET_CLASS (gtk_toolbar_parent_class)->unroot (widget);
}

static int
find_drop_index (GtkToolbar *toolbar,
		 gint        x,
		 gint        y)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  GList *interesting_content;
  GList *list;
  GtkOrientation orientation;
  GtkTextDirection direction;
  gint best_distance = G_MAXINT;
  gint distance;
  gint cursor;
  gint pos;
  ToolbarContent *best_content;
  GtkAllocation allocation;
  
  /* list items we care about wrt. drag and drop */
  interesting_content = NULL;
  for (list = priv->content; list != NULL; list = list->next)
    {
      ToolbarContent *content = list->data;
      
      if (toolbar_content_get_state (content) == NORMAL)
	interesting_content = g_list_prepend (interesting_content, content);
    }
  interesting_content = g_list_reverse (interesting_content);
  
  if (!interesting_content)
    return 0;
  
  orientation = priv->orientation;
  direction = gtk_widget_get_direction (GTK_WIDGET (toolbar));
  
  /* distance to first interesting item */
  best_content = interesting_content->data;
  toolbar_content_get_allocation (best_content, &allocation);
  
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      cursor = x;
      
      if (direction == GTK_TEXT_DIR_LTR)
	pos = allocation.x;
      else
	pos = allocation.x + allocation.width;
    }
  else
    {
      cursor = y;
      pos = allocation.y;
    }
  
  best_content = NULL;
  best_distance = ABS (pos - cursor);
  
  /* distance to far end of each item */
  for (list = interesting_content; list != NULL; list = list->next)
    {
      ToolbarContent *content = list->data;
      
      toolbar_content_get_allocation (content, &allocation);
      
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  if (direction == GTK_TEXT_DIR_LTR)
	    pos = allocation.x + allocation.width;
	  else
	    pos = allocation.x;
	}
      else
	{
	  pos = allocation.y + allocation.height;
	}
      
      distance = ABS (pos - cursor);
      
      if (distance < best_distance)
	{
	  best_distance = distance;
	  best_content = content;
	}
    }
  
  g_list_free (interesting_content);
  
  if (!best_content)
    return 0;
  else
    return g_list_index (priv->content, best_content) + 1;
}

static void
reset_all_placeholders (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  GList *list;
  
  for (list = priv->content; list != NULL; list = list->next)
    {
      ToolbarContent *content = list->data;
      if (toolbar_content_is_placeholder (content))
	toolbar_content_set_disappearing (content, TRUE);
    }
}

static gint
physical_to_logical (GtkToolbar *toolbar,
		     gint        physical)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  GList *list;
  int logical;
  
  g_assert (physical >= 0);
  
  logical = 0;
  for (list = priv->content; list && physical > 0; list = list->next)
    {
      ToolbarContent *content = list->data;
      
      if (!toolbar_content_is_placeholder (content))
	logical++;
      physical--;
    }
  
  g_assert (physical == 0);
  
  return logical;
}

static gint
logical_to_physical (GtkToolbar *toolbar,
		     gint        logical)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  GList *list;
  gint physical;
  
  g_assert (logical >= 0);
  
  physical = 0;
  for (list = priv->content; list; list = list->next)
    {
      ToolbarContent *content = list->data;
      
      if (!toolbar_content_is_placeholder (content))
	{
	  if (logical == 0)
	    break;
	  logical--;
	}
      
      physical++;
    }
  
  g_assert (logical == 0);
  
  return physical;
}

/**
 * gtk_toolbar_set_drop_highlight_item:
 * @toolbar: a #GtkToolbar
 * @tool_item: (allow-none): a #GtkToolItem, or %NULL to turn of highlighting
 * @index_: a position on @toolbar
 *
 * Highlights @toolbar to give an idea of what it would look like
 * if @item was added to @toolbar at the position indicated by @index_.
 * If @item is %NULL, highlighting is turned off. In that case @index_ 
 * is ignored.
 *
 * The @tool_item passed to this function must not be part of any widget
 * hierarchy. When an item is set as drop highlight item it can not
 * added to any widget hierarchy or used as highlight item for another
 * toolbar.
 **/
void
gtk_toolbar_set_drop_highlight_item (GtkToolbar  *toolbar,
				     GtkToolItem *tool_item,
				     gint         index_)
{
  ToolbarContent *content;
  GtkToolbarPrivate *priv;
  gint n_items;
  GtkRequisition requisition;
  GtkRequisition old_requisition;
  gboolean restart_sliding;
  
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));
  g_return_if_fail (tool_item == NULL || GTK_IS_TOOL_ITEM (tool_item));

  priv = toolbar->priv;

  if (!tool_item)
    {
      if (priv->highlight_tool_item)
	{
	  gtk_widget_unparent (GTK_WIDGET (priv->highlight_tool_item));
	  g_object_unref (priv->highlight_tool_item);
	  priv->highlight_tool_item = NULL;
	}
      
      reset_all_placeholders (toolbar);
      gtk_toolbar_begin_sliding (toolbar);
      return;
    }
  
  n_items = gtk_toolbar_get_n_items (toolbar);
  if (index_ < 0 || index_ > n_items)
    index_ = n_items;
  
  if (tool_item != priv->highlight_tool_item)
    {
      if (priv->highlight_tool_item)
	g_object_unref (priv->highlight_tool_item);
      
      g_object_ref_sink (tool_item);
      
      priv->highlight_tool_item = tool_item;
      
      gtk_widget_set_parent (GTK_WIDGET (priv->highlight_tool_item),
			     GTK_WIDGET (toolbar));
    }
  
  index_ = logical_to_physical (toolbar, index_);
  
  content = g_list_nth_data (priv->content, index_);
  
  if (index_ > 0)
    {
      ToolbarContent *prev_content;
      
      prev_content = g_list_nth_data (priv->content, index_ - 1);
      
      if (prev_content && toolbar_content_is_placeholder (prev_content))
	content = prev_content;
    }
  
  if (!content || !toolbar_content_is_placeholder (content))
    {
      GtkWidget *placeholder;
      
      placeholder = GTK_WIDGET (gtk_separator_tool_item_new ());

      content = toolbar_content_new_tool_item (toolbar,
					       GTK_TOOL_ITEM (placeholder),
					       TRUE, index_);
    }
  
  g_assert (content);
  g_assert (toolbar_content_is_placeholder (content));

  gtk_widget_get_preferred_size (GTK_WIDGET (priv->highlight_tool_item),
                                 &requisition, NULL);

  toolbar_content_set_expand (content, gtk_tool_item_get_expand (tool_item));
  
  restart_sliding = FALSE;
  toolbar_content_size_request (content, toolbar, &old_requisition);
  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      requisition.height = -1;
      if (requisition.width != old_requisition.width)
	restart_sliding = TRUE;
    }
  else
    {
      requisition.width = -1;
      if (requisition.height != old_requisition.height)
	restart_sliding = TRUE;
    }

  if (toolbar_content_disappearing (content))
    restart_sliding = TRUE;
  
  reset_all_placeholders (toolbar);
  toolbar_content_set_disappearing (content, FALSE);
  
  toolbar_content_set_size_request (content,
				    requisition.width, requisition.height);
  
  if (restart_sliding)
    gtk_toolbar_begin_sliding (toolbar);
}

static void
gtk_toolbar_add (GtkContainer *container,
		 GtkWidget    *widget)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (container);

  gtk_toolbar_insert (toolbar, GTK_TOOL_ITEM (widget), -1);
}

static void
gtk_toolbar_remove (GtkContainer *container,
		    GtkWidget    *widget)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (container);
  GtkToolbarPrivate *priv = toolbar->priv;
  ToolbarContent *content_to_remove;
  GList *list;

  content_to_remove = NULL;
  for (list = priv->content; list != NULL; list = list->next)
    {
      ToolbarContent *content = list->data;
      GtkWidget *child;
      
      child = toolbar_content_get_widget (content);
      if (child && child == widget)
	{
	  content_to_remove = content;
	  break;
	}
    }
  
  g_return_if_fail (content_to_remove != NULL);
  
  toolbar_content_remove (content_to_remove, toolbar);
  toolbar_content_free (content_to_remove);
}

static void
gtk_toolbar_forall (GtkContainer *container,
                    GtkCallback   callback,
                    gpointer      callback_data)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (container);
  GtkToolbarPrivate *priv = toolbar->priv;
  GList *list;

  g_return_if_fail (callback != NULL);

  list = priv->content;
  while (list)
    {
      ToolbarContent *content = list->data;
      GList *next = list->next;

      if (!toolbar_content_is_placeholder (content))
        {
          GtkWidget *child = toolbar_content_get_widget (content);

          if (child)
            callback (child, callback_data);
        }

      list = next;
    }
}

static GType
gtk_toolbar_child_type (GtkContainer *container)
{
  return GTK_TYPE_TOOL_ITEM;
}

static void
gtk_toolbar_reconfigured (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  GList *list;
  
  list = priv->content;
  while (list)
    {
      ToolbarContent *content = list->data;
      GList *next = list->next;
      
      toolbar_content_toolbar_reconfigured (content, toolbar);
      
      list = next;
    }
}

static void
gtk_toolbar_orientation_changed (GtkToolbar    *toolbar,
				 GtkOrientation orientation)
{
  GtkToolbarPrivate *priv = toolbar->priv;

  if (priv->orientation != orientation)
    {
      priv->orientation = orientation;
      
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        gtk_image_set_from_icon_name (GTK_IMAGE (priv->arrow), "pan-down-symbolic");
      else
        gtk_image_set_from_icon_name (GTK_IMAGE (priv->arrow), "pan-end-symbolic");
      
      gtk_toolbar_reconfigured (toolbar);
      
      _gtk_orientable_set_style_classes (GTK_ORIENTABLE (toolbar));
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
      g_object_notify (G_OBJECT (toolbar), "orientation");
    }
}

static void
gtk_toolbar_real_style_changed (GtkToolbar     *toolbar,
				GtkToolbarStyle style)
{
  GtkToolbarPrivate *priv = toolbar->priv;

  if (priv->style != style)
    {
      priv->style = style;

      gtk_toolbar_reconfigured (toolbar);
      
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
      g_object_notify (G_OBJECT (toolbar), "toolbar-style");
    }
}

static void
show_menu (GtkToolbar     *toolbar,
	   GdkEventButton *event)
{
  GtkToolbarPrivate *priv = toolbar->priv;

  rebuild_menu (toolbar);

  gtk_popover_popup (GTK_POPOVER (priv->menu));
}

static void
gtk_toolbar_arrow_button_clicked (GtkWidget  *button,
				  GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = toolbar->priv;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->arrow_button)) &&
      (!priv->menu || !gtk_widget_get_visible (GTK_WIDGET (priv->menu))))
    {
      /* We only get here when the button is clicked with the keyboard,
       * because mouse button presses result in the menu being shown so
       * that priv->menu would be non-NULL and visible.
       */
      show_menu (toolbar, NULL);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (priv->menu), FALSE);
    }
}

static void
gtk_toolbar_arrow_button_press (GtkGesture     *gesture,
                                int             n_press,
                                double          x,
                                double          y,
				GtkToolbar     *toolbar)
{
  GtkWidget *button;

  button = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  show_menu (toolbar, NULL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
}


static void
gtk_toolbar_pressed_cb (GtkGestureClick *gesture,
                        int              n_press,
                        double           x,
                        double           y,
                        gpointer         user_data)
{
  GtkToolbar *toolbar = user_data;
  GdkEventSequence *sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  const GdkEvent *event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  int button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (gdk_event_triggers_context_menu (event))
    {
      gboolean return_value;

      g_signal_emit (toolbar, toolbar_signals[POPUP_CONTEXT_MENU], 0,
                     (int)x, (int)y, button, &return_value);

      if (return_value)
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      else
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
    }
}

static gboolean
gtk_toolbar_popup_menu (GtkWidget *toolbar)
{
  gboolean return_value;
  /* This function is the handler for the "popup menu" keybinding,
   * ie., it is called when the user presses Shift F10
   */
  g_signal_emit (toolbar, toolbar_signals[POPUP_CONTEXT_MENU], 0,
		 -1, -1, -1, &return_value);
  
  return return_value;
}

/**
 * gtk_toolbar_new:
 * 
 * Creates a new toolbar. 
 
 * Returns: the newly-created toolbar.
 **/
GtkWidget *
gtk_toolbar_new (void)
{
  GtkToolbar *toolbar;
  
  toolbar = g_object_new (GTK_TYPE_TOOLBAR, NULL);
  
  return GTK_WIDGET (toolbar);
}

/**
 * gtk_toolbar_insert:
 * @toolbar: a #GtkToolbar
 * @item: a #GtkToolItem
 * @pos: the position of the new item
 *
 * Insert a #GtkToolItem into the toolbar at position @pos. If @pos is
 * 0 the item is prepended to the start of the toolbar. If @pos is
 * negative, the item is appended to the end of the toolbar.
 **/
void
gtk_toolbar_insert (GtkToolbar  *toolbar,
		    GtkToolItem *item,
		    gint         pos)
{
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));
  g_return_if_fail (GTK_IS_TOOL_ITEM (item));

  pos = MIN (pos, (int)g_list_length (toolbar->priv->content));

  if (pos >= 0)
    pos = logical_to_physical (toolbar, pos);

  toolbar_content_new_tool_item (toolbar, item, FALSE, pos);
}

/**
 * gtk_toolbar_get_item_index:
 * @toolbar: a #GtkToolbar
 * @item: a #GtkToolItem that is a child of @toolbar
 * 
 * Returns the position of @item on the toolbar, starting from 0.
 * It is an error if @item is not a child of the toolbar.
 * 
 * Returns: the position of item on the toolbar.
 **/
gint
gtk_toolbar_get_item_index (GtkToolbar  *toolbar,
			    GtkToolItem *item)
{
  GtkToolbarPrivate *priv;
  GList *list;
  int n;
  
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), -1);
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (item), -1);
  g_return_val_if_fail (gtk_widget_get_parent (GTK_WIDGET (item)) == GTK_WIDGET (toolbar), -1);

  priv = toolbar->priv;

  n = 0;
  for (list = priv->content; list != NULL; list = list->next)
    {
      ToolbarContent *content = list->data;
      GtkWidget *widget;
      
      widget = toolbar_content_get_widget (content);
      
      if (item == GTK_TOOL_ITEM (widget))
	break;
      
      ++n;
    }
  
  return physical_to_logical (toolbar, n);
}

/**
 * gtk_toolbar_set_style:
 * @toolbar: a #GtkToolbar.
 * @style: the new style for @toolbar.
 * 
 * Alters the view of @toolbar to display either icons only, text only, or both.
 **/
void
gtk_toolbar_set_style (GtkToolbar      *toolbar,
		       GtkToolbarStyle  style)
{
  GtkToolbarPrivate *priv;

  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  priv = toolbar->priv;

  priv->style_set = TRUE;
  g_signal_emit (toolbar, toolbar_signals[STYLE_CHANGED], 0, style);
}

/**
 * gtk_toolbar_get_style:
 * @toolbar: a #GtkToolbar
 *
 * Retrieves whether the toolbar has text, icons, or both . See
 * gtk_toolbar_set_style().
 
 * Returns: the current style of @toolbar
 **/
GtkToolbarStyle
gtk_toolbar_get_style (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), DEFAULT_TOOLBAR_STYLE);

  return toolbar->priv->style;
}

/**
 * gtk_toolbar_unset_style:
 * @toolbar: a #GtkToolbar
 * 
 * Unsets a toolbar style set with gtk_toolbar_set_style(), so that
 * user preferences will be used to determine the toolbar style.
 **/
void
gtk_toolbar_unset_style (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv;
  GtkToolbarStyle style;
  
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  priv = toolbar->priv;

  if (priv->style_set)
    {
      style = DEFAULT_TOOLBAR_STYLE;

      if (style != priv->style)
	g_signal_emit (toolbar, toolbar_signals[STYLE_CHANGED], 0, style);

      priv->style_set = FALSE;
    }
}

/**
 * gtk_toolbar_get_n_items:
 * @toolbar: a #GtkToolbar
 * 
 * Returns the number of items on the toolbar.
 * 
 * Returns: the number of items on the toolbar
 **/
gint
gtk_toolbar_get_n_items (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv;

  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), -1);

  priv = toolbar->priv;

  return physical_to_logical (toolbar, g_list_length (priv->content));
}

/**
 * gtk_toolbar_get_nth_item:
 * @toolbar: a #GtkToolbar
 * @n: A position on the toolbar
 *
 * Returns the @n'th item on @toolbar, or %NULL if the
 * toolbar does not contain an @n'th item.
 *
 * Returns: (nullable) (transfer none): The @n'th #GtkToolItem on @toolbar,
 *     or %NULL if there isn’t an @n'th item.
 **/
GtkToolItem *
gtk_toolbar_get_nth_item (GtkToolbar *toolbar,
			  gint        n)
{
  GtkToolbarPrivate *priv;
  ToolbarContent *content;
  gint n_items;
  
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), NULL);

  priv = toolbar->priv;

  n_items = gtk_toolbar_get_n_items (toolbar);
  
  if (n < 0 || n >= n_items)
    return NULL;

  content = g_list_nth_data (priv->content, logical_to_physical (toolbar, n));
  
  g_assert (content);
  g_assert (!toolbar_content_is_placeholder (content));
  
  return GTK_TOOL_ITEM (toolbar_content_get_widget (content));
}

/**
 * gtk_toolbar_set_show_arrow:
 * @toolbar: a #GtkToolbar
 * @show_arrow: Whether to show an overflow menu
 * 
 * Sets whether to show an overflow menu when @toolbar isn’t allocated enough
 * size to show all of its items. If %TRUE, items which can’t fit in @toolbar,
 * and which have a proxy menu item set by gtk_tool_item_set_proxy_menu_item()
 * or #GtkToolItem::create-menu-proxy, will be available in an overflow menu,
 * which can be opened by an added arrow button. If %FALSE, @toolbar will
 * request enough size to fit all of its child items without any overflow.
 **/
void
gtk_toolbar_set_show_arrow (GtkToolbar *toolbar,
			    gboolean    show_arrow)
{
  GtkToolbarPrivate *priv;

  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  priv = toolbar->priv;

  show_arrow = show_arrow != FALSE;
  
  if (priv->show_arrow != show_arrow)
    {
      priv->show_arrow = show_arrow;
      
      if (!priv->show_arrow)
	gtk_widget_hide (priv->arrow_button);
      
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));      
      g_object_notify (G_OBJECT (toolbar), "show-arrow");
    }
}

/**
 * gtk_toolbar_get_show_arrow:
 * @toolbar: a #GtkToolbar
 * 
 * Returns whether the toolbar has an overflow menu.
 * See gtk_toolbar_set_show_arrow().
 * 
 * Returns: %TRUE if the toolbar has an overflow menu.
 **/
gboolean
gtk_toolbar_get_show_arrow (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), FALSE);

  return toolbar->priv->show_arrow;
}

/**
 * gtk_toolbar_get_drop_index:
 * @toolbar: a #GtkToolbar
 * @x: x coordinate of a point on the toolbar
 * @y: y coordinate of a point on the toolbar
 *
 * Returns the position corresponding to the indicated point on
 * @toolbar. This is useful when dragging items to the toolbar:
 * this function returns the position a new item should be
 * inserted.
 *
 * @x and @y are in @toolbar coordinates.
 * 
 * Returns: The position corresponding to the point (@x, @y) on the toolbar.
 **/
gint
gtk_toolbar_get_drop_index (GtkToolbar *toolbar,
			    gint        x,
			    gint        y)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), -1);
  
  return physical_to_logical (toolbar, find_drop_index (toolbar, x, y));
}

static void
gtk_toolbar_dispose (GObject *object)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (object);
  GtkToolbarPrivate *priv = toolbar->priv;

  g_clear_pointer (&priv->arrow_button, gtk_widget_unparent);

  if (priv->menu)
    {
      g_signal_handlers_disconnect_by_func (priv->menu,
                                            menu_deactivated, toolbar);
      gtk_widget_destroy (GTK_WIDGET (priv->menu));
      priv->menu = NULL;
    }

  if (priv->settings_connection > 0)
    {
      g_signal_handler_disconnect (priv->settings, priv->settings_connection);
      priv->settings_connection = 0;
    }

  g_clear_object (&priv->settings);

 G_OBJECT_CLASS (gtk_toolbar_parent_class)->dispose (object);
}

static void
gtk_toolbar_finalize (GObject *object)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (object);
  GtkToolbarPrivate *priv = toolbar->priv;

  g_list_free_full (priv->content, (GDestroyNotify)toolbar_content_free);

  g_timer_destroy (priv->timer);

  if (priv->idle_id)
    g_source_remove (priv->idle_id);

  G_OBJECT_CLASS (gtk_toolbar_parent_class)->finalize (object);
}

/*
 * ToolbarContent methods
 */
typedef enum {
  UNKNOWN,
  YES,
  NO
} TriState;

struct _ToolbarContent
{
  ItemState      state;

  GtkToolItem   *item;
  GtkAllocation  allocation;
  GtkAllocation  start_allocation;
  GtkAllocation  goal_allocation;
  guint          is_placeholder : 1;
  guint          disappearing : 1;
  guint          has_menu : 2;
};

static ToolbarContent *
toolbar_content_new_tool_item (GtkToolbar  *toolbar,
			       GtkToolItem *item,
			       gboolean     is_placeholder,
			       gint	    pos)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  ToolbarContent *content, *previous;

  content = g_slice_new0 (ToolbarContent);
  
  content->state = NOT_ALLOCATED;
  content->item = item;
  content->is_placeholder = is_placeholder;

  previous = pos > 0 ? g_list_nth_data (priv->content, -1) : NULL;
  priv->content = g_list_insert (priv->content, content, pos);

  gtk_widget_insert_after (GTK_WIDGET (item), GTK_WIDGET (toolbar),
                           previous ? GTK_WIDGET (previous->item) : NULL);

  if (!is_placeholder)
    {
      priv->num_children++;

      gtk_toolbar_stop_sliding (toolbar);
    }

  gtk_widget_queue_resize (GTK_WIDGET (toolbar));
  priv->need_rebuild = TRUE;
  
  return content;
}

static void
toolbar_content_remove (ToolbarContent *content,
			GtkToolbar     *toolbar)
{
  GtkToolbarPrivate *priv = toolbar->priv;

  gtk_widget_unparent (GTK_WIDGET (content->item));

  priv->content = g_list_remove (priv->content, content);

  if (!toolbar_content_is_placeholder (content))
    priv->num_children--;

  gtk_widget_queue_resize (GTK_WIDGET (toolbar));
  priv->need_rebuild = TRUE;
}

static void
toolbar_content_free (ToolbarContent *content)
{
  g_slice_free (ToolbarContent, content);
}

static gint
calculate_max_homogeneous_pixels (GtkWidget *widget)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  gint char_width;
  
  context = gtk_widget_get_pango_context (widget);

  metrics = pango_context_get_metrics (context,
                                       pango_context_get_font_description (context),
				       pango_context_get_language (context));
  char_width = pango_font_metrics_get_approximate_char_width (metrics);
  pango_font_metrics_unref (metrics);
  
  return PANGO_PIXELS (MAX_HOMOGENEOUS_N_CHARS * char_width);
}

static void
toolbar_content_snapshot (ToolbarContent *content,
                          GtkContainer   *container,
                          GtkSnapshot    *snapshot)
{
  GtkWidget *widget;

  if (content->is_placeholder)
    return;
  
  widget = GTK_WIDGET (content->item);

  if (widget)
    gtk_widget_snapshot_child (GTK_WIDGET (container), widget, snapshot);
}

static gboolean
toolbar_content_visible (ToolbarContent *content,
			 GtkToolbar     *toolbar)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  GtkToolItem *item;

  item = content->item;

  if (!gtk_widget_get_visible (GTK_WIDGET (item)))
    return FALSE;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_tool_item_get_visible_horizontal (item))
    return TRUE;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL &&
      gtk_tool_item_get_visible_vertical (item))
    return TRUE;
      
  return FALSE;
}

static void
toolbar_content_size_request (ToolbarContent *content,
			      GtkToolbar     *toolbar,
			      GtkRequisition *requisition)
{
  gtk_widget_get_preferred_size (GTK_WIDGET (content->item),
                                 requisition, NULL);
  if (content->is_placeholder &&
      content->disappearing)
    {
      requisition->width = 0;
      requisition->height = 0;
    }
}

static gboolean
toolbar_content_is_homogeneous (ToolbarContent *content,
				GtkToolbar     *toolbar)
{
  GtkToolbarPrivate *priv = toolbar->priv;
  GtkRequisition requisition;
  gboolean result;
  
  if (priv->max_homogeneous_pixels < 0)
    {
      priv->max_homogeneous_pixels =
	calculate_max_homogeneous_pixels (GTK_WIDGET (toolbar));
    }
  
  toolbar_content_size_request (content, toolbar, &requisition);
  
  if (requisition.width > priv->max_homogeneous_pixels)
    return FALSE;

  result = gtk_tool_item_get_homogeneous (content->item) &&
           !GTK_IS_SEPARATOR_TOOL_ITEM (content->item);

  if (gtk_tool_item_get_is_important (content->item) &&
      priv->style == GTK_TOOLBAR_BOTH_HORIZ &&
      priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      result = FALSE;
    }

  return result;
}

static gboolean
toolbar_content_is_placeholder (ToolbarContent *content)
{
  if (content->is_placeholder)
    return TRUE;
  
  return FALSE;
}

static gboolean
toolbar_content_disappearing (ToolbarContent *content)
{
  if (content->disappearing)
    return TRUE;
  
  return FALSE;
}

static ItemState
toolbar_content_get_state (ToolbarContent *content)
{
  return content->state;
}

static gboolean
toolbar_content_child_visible (ToolbarContent *content)
{
  return gtk_widget_get_child_visible (GTK_WIDGET (content->item));
}

static void
toolbar_content_get_goal_allocation (ToolbarContent *content,
				     GtkAllocation  *allocation)
{
  *allocation = content->goal_allocation;
}

static void
toolbar_content_get_allocation (ToolbarContent *content,
				GtkAllocation  *allocation)
{
  *allocation = content->allocation;
}

static void
toolbar_content_set_start_allocation (ToolbarContent *content,
				      GtkAllocation  *allocation)
{
  content->start_allocation = *allocation;
}

static gboolean
toolbar_content_get_expand (ToolbarContent *content, GtkOrientation orientation)
{
  if (!content->disappearing &&
      (gtk_tool_item_get_expand (content->item) || gtk_widget_compute_expand (GTK_WIDGET (content->item), orientation)))
    return TRUE;

  return FALSE;
}

static void
toolbar_content_set_goal_allocation (ToolbarContent *content,
				     GtkAllocation  *allocation)
{
  content->goal_allocation = *allocation;
}

static void
toolbar_content_set_child_visible (ToolbarContent *content,
				   GtkToolbar     *toolbar,
				   gboolean        visible)
{
  gtk_widget_set_child_visible (GTK_WIDGET (content->item),
                                visible);
}

static void
toolbar_content_get_start_allocation (ToolbarContent *content,
				      GtkAllocation  *start_allocation)
{
  *start_allocation = content->start_allocation;
}

static void
toolbar_content_size_allocate (ToolbarContent *content,
			       GtkAllocation  *allocation)
{
  content->allocation = *allocation;
  gtk_widget_size_allocate (GTK_WIDGET (content->item), allocation, -1);
}

static void
toolbar_content_set_state (ToolbarContent *content,
			   ItemState       state)
{
  content->state = state;
}

static GtkWidget *
toolbar_content_get_widget (ToolbarContent *content)
{
  return GTK_WIDGET (content->item);
}


static void
toolbar_content_set_disappearing (ToolbarContent *content,
				  gboolean        disappearing)
{
  content->disappearing = disappearing;
}

static void
toolbar_content_set_size_request (ToolbarContent *content,
				  gint            width,
				  gint            height)
{
  gtk_widget_set_size_request (GTK_WIDGET (content->item),
                               width, height);
}

static void
toolbar_content_toolbar_reconfigured (ToolbarContent *content,
				      GtkToolbar     *toolbar)
{
  gtk_tool_item_toolbar_reconfigured (content->item);
}

static GtkWidget *
toolbar_content_retrieve_menu_item (ToolbarContent *content)
{
  return gtk_tool_item_retrieve_proxy_menu_item (content->item);
}

static gboolean
toolbar_content_has_proxy_menu_item (ToolbarContent *content)
{
  GtkWidget *menu_item;

  if (content->has_menu == YES)
    return TRUE;
  else if (content->has_menu == NO)
    return FALSE;

  menu_item = toolbar_content_retrieve_menu_item (content);

  content->has_menu = menu_item? YES : NO;

  return menu_item != NULL;
}

static void
toolbar_content_set_unknown_menu_status (ToolbarContent *content)
{
  content->has_menu = UNKNOWN;
}

static gboolean
toolbar_content_is_separator (ToolbarContent *content)
{
  return GTK_IS_SEPARATOR_TOOL_ITEM (content->item);
}

static void
toolbar_content_set_expand (ToolbarContent *content,
                            gboolean        expand)
{
  gtk_tool_item_set_expand (content->item, expand);
}

/* GTK+ internal methods */
gchar *
_gtk_toolbar_elide_underscores (const gchar *original)
{
  gchar *q, *result;
  const gchar *p, *end;
  gsize len;
  gboolean last_underscore;
  
  if (!original)
    return NULL;

  len = strlen (original);
  q = result = g_malloc (len + 1);
  last_underscore = FALSE;
  
  end = original + len;
  for (p = original; p < end; p++)
    {
      if (!last_underscore && *p == '_')
	last_underscore = TRUE;
      else
	{
	  last_underscore = FALSE;
	  if (original + 2 <= p && p + 1 <= end && 
              p[-2] == '(' && p[-1] == '_' && p[0] != '_' && p[1] == ')')
	    {
	      q--;
	      *q = '\0';
	      p++;
	    }
	  else
	    *q++ = *p;
	}
    }

  if (last_underscore)
    *q++ = '_';
  
  *q = '\0';
  
  return result;
}

static GtkOrientation
toolbar_get_orientation (GtkToolShell *shell)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (shell);
  GtkToolbarPrivate *priv = toolbar->priv;

  return priv->orientation;
}

static GtkToolbarStyle
toolbar_get_style (GtkToolShell *shell)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (shell);
  GtkToolbarPrivate *priv = toolbar->priv;

  return priv->style;
}

static void
toolbar_rebuild_menu (GtkToolShell *shell)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (shell);
  GtkToolbarPrivate *priv = toolbar->priv;
  GList *list;

  priv->need_rebuild = TRUE;

  for (list = priv->content; list != NULL; list = list->next)
    {
      ToolbarContent *content = list->data;

      toolbar_content_set_unknown_menu_status (content);
    }
  
  gtk_widget_queue_resize (GTK_WIDGET (shell));
}
