/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkToolbar copyright (C) Federico Mena
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@codefactory.se>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2003 Soeren Sandmann <sandmann@daimi.au.dk>
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

#undef GTK_DISABLE_DEPRECATED

#include "gtkarrow.h"
#include "gtktoolbar.h"
#include "gtkradiotoolbutton.h"
#include "gtkseparatortoolitem.h"
#include "gtkmenu.h"
#include "gtkradiobutton.h"
#include "gtktoolbar.h"
#include "gtkbindings.h"
#include <gdk/gdkkeysyms.h>
#include "gtkmarshalers.h"
#include "gtkmain.h"
#include "gtkstock.h"
#include "gtklabel.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include <string.h>

#define DEFAULT_IPADDING 0

/* note: keep in sync with DEFAULT_SPACE_SIZE and DEFAULT_SPACE_STYLE in gtkseparatortoolitem.c */
#define DEFAULT_SPACE_SIZE  4
#define DEFAULT_SPACE_STYLE GTK_TOOLBAR_SPACE_LINE

#define DEFAULT_ICON_SIZE GTK_ICON_SIZE_LARGE_TOOLBAR
#define DEFAULT_TOOLBAR_STYLE GTK_TOOLBAR_BOTH

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_TOOLBAR_STYLE,
  PROP_SHOW_ARROW
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_EXPAND,
  CHILD_PROP_HOMOGENEOUS,
  CHILD_PROP_PACK_END,
};

enum {
  ORIENTATION_CHANGED,
  STYLE_CHANGED,
  POPUP_CONTEXT_MENU,
  MOVE_FOCUS,
  FOCUS_HOME_OR_END,
  LAST_SIGNAL
};

static void gtk_toolbar_init       (GtkToolbar      *toolbar);
static void gtk_toolbar_class_init (GtkToolbarClass *klass);

static void gtk_toolbar_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec);
static void gtk_toolbar_get_property (GObject      *object,
				      guint         prop_id,
				      GValue       *value,
				      GParamSpec   *pspec);

static gint     gtk_toolbar_expose         (GtkWidget        *widget,
					    GdkEventExpose   *event);
static void     gtk_toolbar_realize        (GtkWidget        *widget);
static void     gtk_toolbar_unrealize      (GtkWidget        *widget);
static void     gtk_toolbar_size_request   (GtkWidget        *widget,
					    GtkRequisition   *requisition);
static void     gtk_toolbar_size_allocate  (GtkWidget        *widget,
					    GtkAllocation    *allocation);
static void     gtk_toolbar_style_set      (GtkWidget        *widget,
					    GtkStyle         *prev_style);
static void     gtk_toolbar_direction_changed (GtkWidget        *widget,
                                               GtkTextDirection  previous_direction);
static gboolean gtk_toolbar_focus          (GtkWidget        *widget,
					    GtkDirectionType  dir);
static void     gtk_toolbar_screen_changed (GtkWidget        *widget,
					    GdkScreen        *previous_screen);
static void     gtk_toolbar_map            (GtkWidget        *widget);
static void     gtk_toolbar_unmap          (GtkWidget        *widget);

static void     gtk_toolbar_drag_leave  (GtkWidget      *widget,
					 GdkDragContext *context,
					 guint           time_);
static gboolean gtk_toolbar_drag_motion (GtkWidget      *widget,
					 GdkDragContext *context,
					 gint            x,
					 gint            y,
					 guint           time_);
static void     gtk_toolbar_set_child_property (GtkContainer    *container,
						GtkWidget       *child,
						guint            property_id,
						const GValue    *value,
						GParamSpec      *pspec);
static void    gtk_toolbar_get_child_property (GtkContainer    *container,
					       GtkWidget       *child,
					       guint            property_id,
					       GValue          *value,
					       GParamSpec      *pspec);
static void gtk_toolbar_finalize (GObject *object);


static void  gtk_toolbar_add        (GtkContainer *container,
				     GtkWidget    *widget);
static void  gtk_toolbar_remove     (GtkContainer *container,
				     GtkWidget    *widget);
static void  gtk_toolbar_forall     (GtkContainer *container,
				     gboolean      include_internals,
				     GtkCallback   callback,
				     gpointer      callback_data);
static GType gtk_toolbar_child_type (GtkContainer *container);

static void gtk_toolbar_real_orientation_changed (GtkToolbar      *toolbar,
						  GtkOrientation   orientation);
static void gtk_toolbar_real_style_changed       (GtkToolbar      *toolbar,
						  GtkToolbarStyle  style);

static gboolean gtk_toolbar_move_focus        (GtkToolbar       *toolbar,
					       GtkDirectionType  dir);
static gboolean gtk_toolbar_focus_home_or_end (GtkToolbar       *toolbar,
					       gboolean          focus_home);

static gboolean             gtk_toolbar_button_press         (GtkWidget      *toolbar,
							      GdkEventButton *event);
static gboolean             gtk_toolbar_arrow_button_press (GtkWidget      *button,
							      GdkEventButton *event,
							      GtkToolbar     *toolbar);
static void                 gtk_toolbar_arrow_button_clicked (GtkWidget      *button,
							       GtkToolbar     *toolbar);
static void                 gtk_toolbar_update_button_relief (GtkToolbar     *toolbar);
static GtkReliefStyle       get_button_relief                (GtkToolbar     *toolbar);
static gint                 get_internal_padding             (GtkToolbar     *toolbar);
static GtkShadowType        get_shadow_type                  (GtkToolbar     *toolbar);
static void                 gtk_toolbar_remove_tool_item     (GtkToolbar     *toolbar,
							      GtkToolItem    *item);
static gboolean		    gtk_toolbar_popup_menu	     (GtkWidget      *toolbar);

static GtkWidget *gtk_toolbar_internal_insert_element (GtkToolbar          *toolbar,
						       GtkToolbarChildType  type,
						       GtkWidget           *widget,
						       const char          *text,
						       const char          *tooltip_text,
						       const char          *tooltip_private_text,
						       GtkWidget           *icon,
						       GtkSignalFunc        callback,
						       gpointer             user_data,
						       gint                 position,
						       gboolean             use_stock);


typedef enum {
  DONT_KNOW,
  OLD_API,
  NEW_API
} ApiMode;

struct _GtkToolbarPrivate
{
  GList     *items;
  
  GtkWidget *arrow;
  GtkWidget *arrow_button;
  
  gboolean   show_arrow;

  gint       drop_index;
  GdkWindow *drag_highlight;
  GtkMenu   *menu;

  GdkWindow *event_window;
  ApiMode    api_mode;
  GtkSettings *settings;
};

static GtkContainerClass *parent_class = NULL;
static guint toolbar_signals [LAST_SIGNAL] = { 0 };

GType
gtk_toolbar_get_type (void)
{
  static GtkType type = 0;

  if (!type)
    {
      static const GTypeInfo type_info =
	{
	  sizeof (GtkToolbarClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) gtk_toolbar_class_init,
	  (GClassFinalizeFunc) NULL,
	  NULL,
	  sizeof (GtkToolbar),
	  0, /* n_preallocs */
	  (GInstanceInitFunc) gtk_toolbar_init,
	};

      type = g_type_register_static (GTK_TYPE_CONTAINER,
				     "GtkToolbar",
				     &type_info, 0);
    }
  
  return type;
}

static void
add_arrow_bindings (GtkBindingSet   *binding_set,
		    guint            keysym,
		    GtkDirectionType dir)
{
  guint keypad_keysym = keysym - GDK_Left + GDK_KP_Left;
  
  gtk_binding_entry_add_signal (binding_set, keysym, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, dir);
  gtk_binding_entry_add_signal (binding_set, keypad_keysym, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, dir);
}

static void
add_ctrl_tab_bindings (GtkBindingSet    *binding_set,
		       GdkModifierType   modifiers,
		       GtkDirectionType  direction)
{
  gtk_binding_entry_add_signal (binding_set,
				GDK_Tab, GDK_CONTROL_MASK | modifiers,
				"move_focus", 1,
				GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Tab, GDK_CONTROL_MASK | modifiers,
				"move_focus", 1,
				GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
gtk_toolbar_class_init (GtkToolbarClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkBindingSet *binding_set;

  parent_class = g_type_class_peek_parent (klass);
  
  gobject_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;
  container_class = (GtkContainerClass *)klass;
  
  gobject_class->set_property = gtk_toolbar_set_property;
  gobject_class->get_property = gtk_toolbar_get_property;
  gobject_class->finalize = gtk_toolbar_finalize;

  widget_class->button_press_event = gtk_toolbar_button_press;
  widget_class->expose_event = gtk_toolbar_expose;
  widget_class->size_request = gtk_toolbar_size_request;
  widget_class->size_allocate = gtk_toolbar_size_allocate;
  widget_class->style_set = gtk_toolbar_style_set;
  widget_class->direction_changed = gtk_toolbar_direction_changed;
  widget_class->focus = gtk_toolbar_focus;
  widget_class->screen_changed = gtk_toolbar_screen_changed;
  widget_class->realize = gtk_toolbar_realize;
  widget_class->unrealize = gtk_toolbar_unrealize;
  widget_class->map = gtk_toolbar_map;
  widget_class->unmap = gtk_toolbar_unmap;
  widget_class->popup_menu = gtk_toolbar_popup_menu;
  
  widget_class->drag_leave = gtk_toolbar_drag_leave;
  widget_class->drag_motion = gtk_toolbar_drag_motion;
  
  container_class->add    = gtk_toolbar_add;
  container_class->remove = gtk_toolbar_remove;
  container_class->forall = gtk_toolbar_forall;
  container_class->child_type = gtk_toolbar_child_type;
  container_class->get_child_property = gtk_toolbar_get_child_property;
  container_class->set_child_property = gtk_toolbar_set_child_property;
  
  klass->orientation_changed = gtk_toolbar_real_orientation_changed;
  klass->style_changed = gtk_toolbar_real_style_changed;
  
  toolbar_signals[ORIENTATION_CHANGED] =
    g_signal_new ("orientation_changed",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkToolbarClass, orientation_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_ORIENTATION);
  toolbar_signals[STYLE_CHANGED] =
    g_signal_new ("style_changed",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkToolbarClass, style_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_TOOLBAR_STYLE);
  toolbar_signals[POPUP_CONTEXT_MENU] =
    g_signal_new ("popup_context_menu",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkToolbarClass, popup_context_menu),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__INT_INT_INT,
		  G_TYPE_BOOLEAN, 3,
		  G_TYPE_INT, G_TYPE_INT,
		  G_TYPE_INT);
  toolbar_signals[MOVE_FOCUS] =
    _gtk_binding_signal_new ("move_focus",
			     G_TYPE_FROM_CLASS (klass),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			     G_CALLBACK (gtk_toolbar_move_focus),
			     NULL, NULL,
			     _gtk_marshal_BOOLEAN__ENUM,
			     G_TYPE_BOOLEAN, 1,
			     GTK_TYPE_DIRECTION_TYPE);
  toolbar_signals[FOCUS_HOME_OR_END] =
    _gtk_binding_signal_new ("focus_home_or_end",
			     G_OBJECT_CLASS_TYPE (klass),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			     G_CALLBACK (gtk_toolbar_focus_home_or_end),
			     NULL, NULL,
			     _gtk_marshal_BOOLEAN__BOOLEAN,
			     G_TYPE_BOOLEAN, 1,
			     G_TYPE_BOOLEAN);

  /* properties */
  g_object_class_install_property (gobject_class,
				   PROP_ORIENTATION,
				   g_param_spec_enum ("orientation",
 						      _("Orientation"),
 						      _("The orientation of the toolbar"),
 						      GTK_TYPE_ORIENTATION,
 						      GTK_ORIENTATION_HORIZONTAL,
 						      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_TOOLBAR_STYLE,
				   g_param_spec_enum ("toolbar_style",
 						      _("Toolbar Style"),
 						      _("How to draw the toolbar"),
 						      GTK_TYPE_TOOLBAR_STYLE,
 						      GTK_TOOLBAR_ICONS,
 						      G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SHOW_ARROW,
				   g_param_spec_boolean ("show_arrow",
							 _("Show Arrow"),
							 _("If an arrow should be shown if the toolbar doesn't fit"),
							 FALSE,
							 G_PARAM_READWRITE));

  /* child properties */
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_EXPAND,
					      g_param_spec_boolean ("expand", 
								    _("Expand"), 
								    _("Whether the item should receive extra space when the toolbar grows"),
								    TRUE,
								    G_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_HOMOGENEOUS,
					      g_param_spec_boolean ("homogeneous", 
								    _("Homogeneous"), 
								    _("Whether the item should be the same size as other homogeneous items"),
								    TRUE,
								    G_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PACK_END,
					      g_param_spec_uint ("pack_end", 
								 _("Pack End"), 
								 _("Whether the item is positioned at the end of the toolbar"),
								 0, G_MAXINT, 0,
								 G_PARAM_READWRITE));

  /* style properties */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("space_size",
							     _("Spacer size"),
							     _("Size of spacers"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_SPACE_SIZE,
							     G_PARAM_READABLE));
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("internal_padding",
							     _("Internal padding"),
							     _("Amount of border space between the toolbar shadow and the buttons"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_IPADDING,
                                                             G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_enum ("space_style",
							     _("Space style"),
							     _("Whether spacers are vertical lines or just blank"),
                                                              GTK_TYPE_TOOLBAR_SPACE_STYLE,
                                                              DEFAULT_SPACE_STYLE,
                                                              G_PARAM_READABLE));
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_enum ("button_relief",
							      _("Button relief"),
							      _("Type of bevel around toolbar buttons"),
                                                              GTK_TYPE_RELIEF_STYLE,
                                                              GTK_RELIEF_NONE,
                                                              G_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("shadow_type",
                                                              _("Shadow type"),
                                                              _("Style of bevel around the toolbar"),
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_OUT,
                                                              G_PARAM_READABLE));

  gtk_settings_install_property (g_param_spec_enum ("gtk-toolbar-style",
                                                    _("Toolbar style"),
                                                    _("Whether default toolbars have text only, text and icons, icons only, etc."),
                                                    GTK_TYPE_TOOLBAR_STYLE,
                                                    DEFAULT_TOOLBAR_STYLE,
                                                    G_PARAM_READWRITE));

  gtk_settings_install_property (g_param_spec_enum ("gtk-toolbar-icon-size",
                                                    _("Toolbar icon size"),
                                                    _("Size of icons in default toolbars"),
                                                    GTK_TYPE_ICON_SIZE,
                                                    DEFAULT_ICON_SIZE,
                                                    G_PARAM_READWRITE));  

  binding_set = gtk_binding_set_by_class (klass);

  add_arrow_bindings (binding_set, GDK_Left, GTK_DIR_LEFT);
  add_arrow_bindings (binding_set, GDK_Right, GTK_DIR_RIGHT);
  add_arrow_bindings (binding_set, GDK_Up, GTK_DIR_UP);
  add_arrow_bindings (binding_set, GDK_Down, GTK_DIR_DOWN);

  gtk_binding_entry_add_signal (binding_set, GDK_KP_Home, 0,
                                "focus_home_or_end", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Home, 0,
                                "focus_home_or_end", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_End, 0,
                                "focus_home_or_end", 1,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_End, 0,
                                "focus_home_or_end", 1,
				G_TYPE_BOOLEAN, FALSE);

  add_ctrl_tab_bindings (binding_set, 0, GTK_DIR_TAB_FORWARD);
  add_ctrl_tab_bindings (binding_set, GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);

  g_type_class_add_private (gobject_class, sizeof (GtkToolbarPrivate));  
}

static void
gtk_toolbar_init (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv;
  
  GTK_WIDGET_UNSET_FLAGS (toolbar, GTK_CAN_FOCUS);
  GTK_WIDGET_SET_FLAGS (toolbar, GTK_NO_WINDOW);

  priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  
  toolbar->orientation = GTK_ORIENTATION_HORIZONTAL;
  toolbar->style = DEFAULT_TOOLBAR_STYLE;
  toolbar->icon_size = DEFAULT_ICON_SIZE;
  toolbar->tooltips = gtk_tooltips_new ();
  g_object_ref (toolbar->tooltips);
  gtk_object_sink (GTK_OBJECT (toolbar->tooltips));
  
  priv->arrow_button = gtk_toggle_button_new ();
  g_signal_connect (priv->arrow_button, "button_press_event",
		    G_CALLBACK (gtk_toolbar_arrow_button_press), toolbar);
  g_signal_connect (priv->arrow_button, "clicked",
		    G_CALLBACK (gtk_toolbar_arrow_button_clicked), toolbar);
  gtk_button_set_relief (GTK_BUTTON (priv->arrow_button),
			 get_button_relief (toolbar));

  priv->api_mode = DONT_KNOW;
  
  gtk_button_set_focus_on_click (GTK_BUTTON (priv->arrow_button), FALSE);
  
  priv->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
  gtk_widget_show (priv->arrow);
  gtk_container_add (GTK_CONTAINER (priv->arrow_button), priv->arrow);
  
  gtk_widget_set_parent (priv->arrow_button, GTK_WIDGET (toolbar));

  /* which child position a drop will occur at */
  priv->drop_index = -1;
  priv->drag_highlight = NULL;

  priv->menu = NULL;

  priv->settings = NULL;
}

static gboolean
toolbar_item_visible (GtkToolbar  *toolbar,
		      GtkToolItem *item)
{
  if (GTK_WIDGET_VISIBLE (item) &&
      ((toolbar->orientation == GTK_ORIENTATION_HORIZONTAL &&
	gtk_tool_item_get_visible_horizontal (item)) ||
       (toolbar->orientation == GTK_ORIENTATION_VERTICAL &&
	gtk_tool_item_get_visible_vertical (item))))
    {
      GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
      
      /* With the old toolbar you could hide a button by calling gtk_widget_hide()
       * on it. This doesn't work with the new API because the GtkToolItem will not be
       * hidden.
       */
      if (priv->api_mode == OLD_API)
	{
	  GtkWidget *bin_child = GTK_BIN (item)->child;
	  
	  if (bin_child && !GTK_WIDGET_VISIBLE (bin_child))
	    return FALSE;
	}
      
      return TRUE;
    }
  
  return FALSE;
}

static gboolean
toolbar_item_is_homogeneous (GtkToolItem *item)
{
  return (gtk_tool_item_get_homogeneous (item) && !GTK_IS_SEPARATOR_TOOL_ITEM (item));
}

static void
toolbar_item_set_is_overflow (GtkToolItem *item,
			      gboolean     is_overflow)
{
  g_object_set_data (G_OBJECT (item), "gtk-toolbar-item-is-overflow", GINT_TO_POINTER (is_overflow));
}

static gboolean
toolbar_item_get_is_overflow (GtkToolItem *item)
{
  gpointer result;

  result = g_object_get_data (G_OBJECT (item), "gtk-toolbar-item-is-overflow");

  return GPOINTER_TO_INT (result);
}

static void
gtk_toolbar_set_property (GObject     *object,
			  guint        prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_toolbar_set_orientation (toolbar, g_value_get_enum (value));
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
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  
  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, toolbar->orientation);
      break;
    case PROP_TOOLBAR_STYLE:
      g_value_set_enum (value, toolbar->style);
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
gtk_toolbar_map (GtkWidget *widget)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (widget);

  GTK_WIDGET_CLASS (parent_class)->map (widget);

  if (priv->event_window)
    gdk_window_show_unraised (priv->event_window);
}

static void
gtk_toolbar_unmap (GtkWidget *widget)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (widget);

  if (priv->event_window)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
gtk_toolbar_realize (GtkWidget *widget)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);

  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  border_width = GTK_CONTAINER (widget)->border_width;

  attributes.wclass = GDK_INPUT_ONLY;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  widget->window = gtk_widget_get_parent_window (widget);
  g_object_ref (widget->window);
  widget->style = gtk_style_attach (widget->style, widget->window);
  
  priv->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
				       &attributes, attributes_mask);
  gdk_window_set_user_data (priv->event_window, toolbar);
}

static void
gtk_toolbar_unrealize (GtkWidget *widget)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (widget);

  if (priv->drag_highlight)
    {
      gdk_window_set_user_data (priv->drag_highlight, NULL);
      gdk_window_destroy (priv->drag_highlight);
      priv->drag_highlight = NULL;
    }

  if (priv->event_window)
    {
      gdk_window_set_user_data (priv->event_window, NULL);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static gint
gtk_toolbar_expose (GtkWidget      *widget,
		    GdkEventExpose *event)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  
  GList *items;
  gint border_width;
  
  border_width = GTK_CONTAINER (widget)->border_width;
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_paint_box (widget->style,
		     widget->window,
                     GTK_WIDGET_STATE (widget),
                     get_shadow_type (toolbar),
		     &event->area, widget, "toolbar",
		     border_width + widget->allocation.x,
                     border_width + widget->allocation.y,
		     widget->allocation.width - 2 * border_width,
                     widget->allocation.height - 2 * border_width);
    }

  items = priv->items;
  while (items)
    {
      GtkToolItem *item = GTK_TOOL_ITEM (items->data);

      gtk_container_propagate_expose (GTK_CONTAINER (widget),
				      GTK_WIDGET (item),
				      event);
      
      items = items->next;
    }

  gtk_container_propagate_expose (GTK_CONTAINER (widget),
				  priv->arrow_button,
				  event);

  return FALSE;
}

static void
gtk_toolbar_size_request (GtkWidget      *widget,
			  GtkRequisition *requisition)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  GList *list;
  gint max_child_height;
  gint max_child_width;
  gint max_homogeneous_child_width;
  gint max_homogeneous_child_height;
  gint homogeneous_size;
  gint long_req;
  gint pack_end_size;
  gint pack_front_size;
  gint ipadding;
  GtkRequisition arrow_requisition;

  max_homogeneous_child_width = 0;
  max_homogeneous_child_height = 0;
  max_child_width = 0;
  max_child_height = 0;
  for (list = priv->items; list != NULL; list = list->next)
    {
      GtkRequisition requisition;
      GtkToolItem *item = list->data;
      
      if (!toolbar_item_visible (toolbar, item))
	continue;

      gtk_widget_size_request (GTK_WIDGET (item), &requisition);
      
      max_child_width = MAX (max_child_width, requisition.width);
      max_child_height = MAX (max_child_height, requisition.height);

      if (toolbar_item_is_homogeneous (item))
	{
	  max_homogeneous_child_width = MAX (max_homogeneous_child_width, requisition.width);
	  max_homogeneous_child_height = MAX (max_homogeneous_child_height, requisition.height);
	}
    }
  
  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    homogeneous_size = max_homogeneous_child_width;
  else
    homogeneous_size = max_homogeneous_child_height;
  
  pack_end_size = 0;
  pack_front_size = 0;
  for (list = priv->items; list != NULL; list = list->next)
    {
      GtkToolItem *item = list->data;
      guint size;
      
      if (!toolbar_item_visible (toolbar, item))
	continue;
      
      if (toolbar_item_is_homogeneous (item))
	{
	  size = homogeneous_size;
	}
      else
	{
	  GtkRequisition requisition;
	  
	  gtk_widget_size_request (GTK_WIDGET (item), &requisition);

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    size = requisition.width;
	  else
	    size = requisition.height;
	}
      
      if (gtk_tool_item_get_pack_end (item))
	pack_end_size += size;
      else
	pack_front_size += size;
    }
  
  if (priv->show_arrow)
    {
      gtk_widget_size_request (priv->arrow_button, &arrow_requisition);
      
      if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	long_req = arrow_requisition.width;
      else
	long_req = arrow_requisition.height;

      /* There is no point requesting space for the arrow if that would take
       * up more space than all the items combined
       */
      long_req = MIN (long_req, pack_front_size + pack_end_size);
    }
  else
    {
      arrow_requisition.height = 0;
      arrow_requisition.width = 0;
      
      long_req = pack_end_size + pack_front_size;
    }
  
  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      requisition->width = long_req;
      requisition->height = MAX (max_child_height, arrow_requisition.height);
    }
  else
    {
      requisition->height = long_req;
      requisition->width = MAX (max_child_width, arrow_requisition.width);
    }
  
  /* Extra spacing */
  ipadding = get_internal_padding (toolbar);

  requisition->width += 2 * (ipadding + GTK_CONTAINER (toolbar)->border_width);
  requisition->height += 2 * (ipadding + GTK_CONTAINER (toolbar)->border_width);

  if (get_shadow_type (toolbar) != GTK_SHADOW_NONE)
    {
      requisition->width += 2 * widget->style->xthickness;
      requisition->height += 2 * widget->style->ythickness;
    }
  
  toolbar->button_maxw = max_homogeneous_child_width;
  toolbar->button_maxh = max_homogeneous_child_height;
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
get_item_size (GtkToolbar *toolbar,
	       GtkWidget  *child)
{
  GtkRequisition requisition;
  GtkToolItem *item = GTK_TOOL_ITEM (child);

  gtk_widget_get_child_requisition (child, &requisition);
  
  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (toolbar_item_is_homogeneous (item))
	return toolbar->button_maxw;
      else
	return requisition.width;
    }
  else
    {
      if (toolbar_item_is_homogeneous (item))
	return toolbar->button_maxh;
      else
	return requisition.height;
    }
}

static void
gtk_toolbar_size_allocate (GtkWidget     *widget,
			   GtkAllocation *allocation)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  GtkAllocation *allocations;
  GtkAllocation arrow_allocation;
  gint arrow_size;
  gint size, pos, short_size;
  GList *list;
  gint i;
  gboolean need_arrow;
  gint n_expand_items;
  gint border_width;
  gint available_size;
  gint n_items;
  gint needed_size;
  GList *items;
  GtkRequisition arrow_requisition;

  widget->allocation = *allocation;

  border_width = GTK_CONTAINER (toolbar)->border_width;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (priv->event_window,
                              allocation->x + border_width,
                              allocation->y + border_width,
                              allocation->width - border_width * 2,
                              allocation->height - border_width * 2);
    }
  
  border_width += get_internal_padding (toolbar);
  
  gtk_widget_get_child_requisition (GTK_WIDGET (priv->arrow_button),
				    &arrow_requisition);
  
  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      available_size = size = allocation->width - 2 * border_width;
      short_size = allocation->height - 2 * border_width;
      arrow_size = arrow_requisition.width;

      if (get_shadow_type (toolbar) != GTK_SHADOW_NONE)
	{
	  available_size -= 2 * widget->style->xthickness;
	  short_size -= 2 * widget->style->ythickness;
	}
    }
  else
    {
      available_size = size = allocation->height - 2 * border_width;
      short_size = allocation->width - 2 * border_width;
      arrow_size = arrow_requisition.height;

      if (get_shadow_type (toolbar) != GTK_SHADOW_NONE)
	{
	  available_size -= 2 * widget->style->ythickness;
	  short_size -= 2 * widget->style->xthickness;
	}
    }

  n_items = g_list_length (priv->items);
  allocations = g_new0 (GtkAllocation, n_items);

  needed_size = 0;
  for (list = priv->items; list != NULL; list = list->next)
    {
      GtkToolItem *item = list->data;
      
      if (toolbar_item_visible (toolbar, item))
	needed_size += get_item_size (toolbar, GTK_WIDGET (item));
    }

  need_arrow = (needed_size > available_size) && priv->show_arrow;

  if (need_arrow)
    size = available_size - arrow_size;
  else
    size = available_size;

  items = g_list_copy (priv->items);

  /* calculate widths of pack end items */
  for (list = g_list_last (items), i = 0; list != NULL; list = list->prev, ++i)
    {
      GtkToolItem *item = list->data;
      GtkAllocation *allocation = &(allocations[n_items - i - 1]);
      gint item_size;
      
      if (!gtk_tool_item_get_pack_end (item) ||
	  !toolbar_item_visible (toolbar, item))
	continue;

      item_size = get_item_size (toolbar, GTK_WIDGET (item));
      if (item_size <= size)
	{
	  size -= item_size;
	  allocation->width = item_size;
	  toolbar_item_set_is_overflow (item, FALSE);
	}
      else
	{
	  while (list)
	    {
	      item = list->data;
	      if (gtk_tool_item_get_pack_end (item))
		toolbar_item_set_is_overflow (item, TRUE);
	      
	      list = list->prev;
	    }
	  break;
	}
    }

  /* calculate widths of pack front items */
  for (list = items, i = 0; list != NULL; list = list->next, ++i)
    {
      GtkToolItem *item = list->data;
      gint item_size;

      if (gtk_tool_item_get_pack_end (item) || !toolbar_item_visible (toolbar, item))
	continue;

      item_size = get_item_size (toolbar, GTK_WIDGET (item));
      if (item_size <= size)
	{
	  size -= item_size;
	  allocations[i].width = item_size;
	  toolbar_item_set_is_overflow (item, FALSE);
	}
      else
	{
	  while (list)
	    {
	      item = list->data;
	      if (!gtk_tool_item_get_pack_end (item))
		toolbar_item_set_is_overflow (item, TRUE);
	      list = list->next;
	    }
	  break;
	}
    }

  if (need_arrow)
    {
      arrow_allocation.width = arrow_size;
      arrow_allocation.height = short_size;
    }
  
  /* expand expandable items */
  n_expand_items = 0;
  for (list = priv->items; list != NULL; list = list->next)
    {
      GtkToolItem *item = list->data;
      
      if (toolbar_item_visible (toolbar, item) &&
	  gtk_tool_item_get_expand (item) &&
	  !toolbar_item_get_is_overflow (item) &&
	  !GTK_IS_SEPARATOR_TOOL_ITEM (item))
	{
	  n_expand_items++;
	}
    }
  
  for (list = items, i = 0; list != NULL; list = list->next, ++i)
    {
      GtkToolItem *item = list->data;
      
      if (toolbar_item_visible (toolbar, item) && gtk_tool_item_get_expand (item) &&
	  !toolbar_item_get_is_overflow (item) &&
	  !GTK_IS_SEPARATOR_TOOL_ITEM (item))
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
  
  /* position pack front items */
  pos = border_width;
  for (list = items, i = 0; list != NULL; list = list->next, ++i)
    {
      GtkToolItem *item = list->data;
      
      if (toolbar_item_visible (toolbar, item) && !toolbar_item_get_is_overflow (item) && !gtk_tool_item_get_pack_end (item))
	{
	  allocations[i].x = pos;
	  allocations[i].y = border_width;
	  allocations[i].height = short_size;

	  pos += allocations[i].width;
	}
    }

  /* position pack end items */
  pos = available_size + border_width;
  for (list = g_list_last (items), i = 0; list != NULL; list = list->prev, ++i)
    {
      GtkToolItem *item = list->data;
      
      if (toolbar_item_visible (toolbar, item) && !toolbar_item_get_is_overflow (item) && gtk_tool_item_get_pack_end (item))
	{
	  GtkAllocation *allocation = &(allocations[n_items - i - 1]);

	  allocation->x = pos - allocation->width;
	  allocation->y = border_width;
	  allocation->height = short_size;
	  
	  pos -= allocation->width;
	}
    }

  /* position arrow */
  if (need_arrow)
    {
      arrow_allocation.x = pos - arrow_allocation.width;
      arrow_allocation.y = border_width;
    }
  
  /* fix up allocations in the vertical or RTL cases */
  if (toolbar->orientation == GTK_ORIENTATION_VERTICAL)
    {
      for (i = 0; i < n_items; ++i)
	fixup_allocation_for_vertical (&(allocations[i]));
      
      if (need_arrow)
	fixup_allocation_for_vertical (&arrow_allocation);
    }
  else if (gtk_widget_get_direction (GTK_WIDGET (toolbar)) == GTK_TEXT_DIR_RTL)
    {
      for (i = 0; i < n_items; ++i)
	fixup_allocation_for_rtl (available_size, &(allocations[i]));

      if (need_arrow)
	fixup_allocation_for_rtl (available_size, &arrow_allocation);
    }
  
  /* translate the items by allocation->(x,y) */
  for (i = 0; i < n_items; ++i)
    {
      allocations[i].x += allocation->x;
      allocations[i].y += allocation->y;

      if (get_shadow_type (toolbar) != GTK_SHADOW_NONE)
	{
	  allocations[i].x += widget->style->xthickness;
	  allocations[i].y += widget->style->ythickness;
	}
    }

  if (need_arrow)
    {
      arrow_allocation.x += allocation->x;
      arrow_allocation.y += allocation->y;

      if (get_shadow_type (toolbar) != GTK_SHADOW_NONE)
	{
	  arrow_allocation.x += widget->style->xthickness;
	  arrow_allocation.y += widget->style->ythickness;
	}
    }
  
  /* finally allocate the items */
  for (list = items, i = 0; list != NULL; list = list->next, i++)
    {
      GtkToolItem *item = list->data;
      
      if (toolbar_item_visible (toolbar, item) && !toolbar_item_get_is_overflow (item))
	{
	  gtk_widget_size_allocate (GTK_WIDGET (item), &(allocations[i]));
	  gtk_widget_set_child_visible (GTK_WIDGET (item), TRUE);
	}
      else
	{
	  gtk_widget_set_child_visible (GTK_WIDGET (item), FALSE);
	}
    }

  if (need_arrow)
    {
      gtk_widget_size_allocate (GTK_WIDGET (priv->arrow_button),
				&arrow_allocation);
      gtk_widget_show (GTK_WIDGET (priv->arrow_button));
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (priv->arrow_button));
    }
  
  g_free (allocations);
  g_list_free (items);
}

static void
gtk_toolbar_style_set (GtkWidget *widget,
		       GtkStyle  *prev_style)
{
  if (GTK_WIDGET_REALIZED (widget))
    gtk_style_set_background (widget->style, widget->window, widget->state);

  if (prev_style)
    gtk_toolbar_update_button_relief (GTK_TOOLBAR (widget));
}

static void 
gtk_toolbar_direction_changed (GtkWidget        *widget,
		   	       GtkTextDirection  previous_dir)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  
  if (toolbar->orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
      else 
	gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_LEFT, GTK_SHADOW_NONE);
    }

  GTK_WIDGET_CLASS (parent_class)->direction_changed (widget, previous_dir);
}

static GList *
gtk_toolbar_list_children_in_focus_order (GtkToolbar       *toolbar,
					  GtkDirectionType  dir)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  GList *result = NULL;
  GList *list;
  gboolean rtl;

  /* generate list of children in reverse logical order */
  
  for (list = priv->items; list != NULL; list = list->next)
    {
      GtkToolItem *item = list->data;
      if (!gtk_tool_item_get_pack_end (item))
	result = g_list_prepend (result, item);
    }

  for (list = priv->items; list != NULL; list = list->next)
    {
      GtkToolItem *item = list->data;

      if (gtk_tool_item_get_pack_end (item))
	result = g_list_prepend (result, item);
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
      
      if (GTK_CONTAINER (toolbar)->focus_child == child)
	break;

      if (GTK_WIDGET_MAPPED (child) && gtk_widget_child_focus (child, dir))
	break;
    }

  g_list_free (children);
  
  return TRUE;
}   

/* Keybinding handler. This function is called when the user presses
 * Ctrl TAB or an arrow key.
 */
static gboolean
gtk_toolbar_move_focus (GtkToolbar       *toolbar,
			GtkDirectionType  dir)
{
  GList *list;
  gboolean try_focus = FALSE;
  GList *children;
  GtkContainer *container = GTK_CONTAINER (toolbar);

  if (container->focus_child &&
      gtk_widget_child_focus (container->focus_child, dir))
    {
      return TRUE;
    }
  
  children = gtk_toolbar_list_children_in_focus_order (toolbar, dir);

  for (list = children; list != NULL; list = list->next)
    {
      GtkWidget *child = list->data;
      
      if (try_focus && GTK_WIDGET_MAPPED (child) && gtk_widget_child_focus (child, dir))
	break;

      if (child == GTK_CONTAINER (toolbar)->focus_child)
	try_focus = TRUE;
    }

  g_list_free (children);

  return TRUE;
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
 
  /* if focus is already somewhere inside the toolbar then return FALSE.
   * The only way focus can stay inside the toolbar is when the user presses
   * arrow keys or Ctrl TAB (both of which are handled by the
   * gtk_toolbar_move_focus() keybinding function.
   */
  if (GTK_CONTAINER (widget)->focus_child)
    return FALSE;

  children = gtk_toolbar_list_children_in_focus_order (toolbar, dir);

  for (list = children; list != NULL; list = list->next)
    {
      GtkWidget *child = list->data;
      
      if (GTK_WIDGET_MAPPED (child) && gtk_widget_child_focus (child, dir))
	return TRUE;
    }

  g_list_free (children);
  
  return FALSE;
}

static void
style_change_notify (GtkToolbar *toolbar)
{
  if (!toolbar->style_set)
    {
      /* pretend it was set, then unset, thus reverting to new default */
      toolbar->style_set = TRUE; 
      gtk_toolbar_unset_style (toolbar);
    }
}

static void
icon_size_change_notify (GtkToolbar *toolbar)
{ 
  if (!toolbar->icon_size_set)
    {
      /* pretend it was set, then unset, thus reverting to new default */
      toolbar->icon_size_set = TRUE; 
      gtk_toolbar_unset_icon_size (toolbar);
    }
}

static GtkSettings *
toolbar_get_settings (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  return priv->settings;
}

static void
gtk_toolbar_screen_changed (GtkWidget *widget,
			    GdkScreen *previous_screen)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (widget);
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkSettings *old_settings = toolbar_get_settings (toolbar);
  GtkSettings *settings;

  if (gtk_widget_has_screen (GTK_WIDGET (toolbar)))
    settings = gtk_widget_get_settings (GTK_WIDGET (toolbar));
  else
    settings = NULL;

  if (settings == old_settings)
    return;

  if (old_settings)
    {
      g_signal_handler_disconnect (old_settings, toolbar->style_set_connection);
      g_signal_handler_disconnect (old_settings, toolbar->icon_size_connection);

      g_object_unref (old_settings);
    }

  if (settings)
    {
      toolbar->style_set_connection =
	g_signal_connect_swapped (settings,
				  "notify::gtk-toolbar-style",
				  G_CALLBACK (style_change_notify),
				  toolbar);
      toolbar->icon_size_connection =
	g_signal_connect_swapped (settings,
				  "notify::gtk-toolbar-icon-size",
				  G_CALLBACK (icon_size_change_notify),
				  toolbar);

      g_object_ref (settings);
      priv->settings = settings;
    }
  else
    priv->settings = NULL;

  style_change_notify (toolbar);
  icon_size_change_notify (toolbar);
}

static void
find_drop_pos (GtkToolbar *toolbar,
	       gint        x,
	       gint        y,
	       gint       *drop_index,
	       gint       *drop_pos)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  GtkOrientation orientation;
  GtkTextDirection direction;
  GList *items;
  GtkToolItem *item;
  gint border_width;
  gint best_distance, best_pos, best_index, index;

  orientation = toolbar->orientation;
  direction = gtk_widget_get_direction (GTK_WIDGET (toolbar));
  border_width = GTK_CONTAINER (toolbar)->border_width + get_internal_padding (toolbar);

  items = priv->items;
  if (!items)
    {
      *drop_index = 0;
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  if (direction == GTK_TEXT_DIR_LTR) 
	    *drop_pos = border_width;
	  else
	    *drop_pos = GTK_WIDGET (toolbar)->allocation.width - border_width;
	}
      else
	{
	  *drop_pos = border_width;
	}
      return;
    }

  /* initial conditions */
  item = GTK_TOOL_ITEM (items->data);
  best_index = 0;
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (direction == GTK_TEXT_DIR_LTR)
	best_pos = GTK_WIDGET (item)->allocation.x;
      else
	best_pos = GTK_WIDGET (item)->allocation.x +
	  GTK_WIDGET (item)->allocation.width;
      best_distance = ABS (best_pos - x);
    }
  else
    {
      best_pos = GTK_WIDGET (item)->allocation.y;
      best_distance = ABS (best_pos - y);
    }

  index = 0;
  while (items)
    {
      item = GTK_TOOL_ITEM (items->data);
      index++;
      if (GTK_WIDGET_DRAWABLE (item) && !gtk_tool_item_get_pack_end (item))
	{
	  gint pos, distance;

	  if (orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      if (direction == GTK_TEXT_DIR_LTR)
		pos = GTK_WIDGET (item)->allocation.x +
		  GTK_WIDGET (item)->allocation.width;
	      else
		pos = GTK_WIDGET (item)->allocation.x;
	      distance = ABS (pos - x);
	    }
	  else
	    {
	      pos = GTK_WIDGET (item)->allocation.y +
		GTK_WIDGET (item)->allocation.height;
	      distance = ABS (pos - y);
	    }
	  if (distance < best_distance)
	    {
	      best_index = index;
	      best_pos = pos;
	      best_distance = distance;
	    }
	}
      items = items->next;
    }
  *drop_index = best_index;
  *drop_pos = best_pos;
}

static void
gtk_toolbar_drag_leave (GtkWidget      *widget,
			GdkDragContext *context,
			guint           time_)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);

  if (priv->drag_highlight)
    {
      gdk_window_set_user_data (priv->drag_highlight, NULL);
      gdk_window_destroy (priv->drag_highlight);
      priv->drag_highlight = NULL;
    }

  priv->drop_index = -1;
}

static gboolean
gtk_toolbar_drag_motion (GtkWidget      *widget,
			 GdkDragContext *context,
			 gint            x,
			 gint            y,
			 guint           time_)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  gint new_index, new_pos;

  find_drop_pos(toolbar, x, y, &new_index, &new_pos);

  if (!priv->drag_highlight)
    {
      GdkWindowAttr attributes;
      guint attributes_mask;

      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK;
      attributes.width = 1;
      attributes.height = 1;
      attributes_mask = GDK_WA_VISUAL | GDK_WA_COLORMAP;
      priv->drag_highlight = gdk_window_new (widget->window,
					     &attributes, attributes_mask);
      gdk_window_set_user_data (priv->drag_highlight, widget);
      gdk_window_set_background (priv->drag_highlight,
      				 &widget->style->fg[widget->state]);
    }

  if (priv->drop_index < 0 ||
      priv->drop_index != new_index)
    {
      gint border_width = GTK_CONTAINER (toolbar)->border_width;
      priv->drop_index = new_index;
      if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  gdk_window_move_resize (priv->drag_highlight,
				  widget->allocation.x + new_pos - 1,
				  widget->allocation.y + border_width,
				  2, widget->allocation.height-border_width*2);
	}
      else
	{
	  gdk_window_move_resize (priv->drag_highlight,
				  widget->allocation.x + border_width,
				  widget->allocation.y + new_pos - 1,
				  widget->allocation.width-border_width*2, 2);
	}
    }

  gdk_window_show (priv->drag_highlight);

  gdk_drag_status (context, context->suggested_action, time_);

  return TRUE;
}

static void
gtk_toolbar_get_child_property (GtkContainer *container,
				GtkWidget    *child,
				guint         property_id,
				GValue       *value,
				GParamSpec   *pspec)
{
  GtkToolItem *item = GTK_TOOL_ITEM (child);
  
  switch (property_id)
    {
    case CHILD_PROP_PACK_END:
      g_value_set_boolean (value, gtk_tool_item_get_pack_end (item));
      break;

    case CHILD_PROP_HOMOGENEOUS:
      g_value_set_boolean (value, gtk_tool_item_get_homogeneous (item));
      break;

    case CHILD_PROP_EXPAND:
      g_value_set_boolean (value, gtk_tool_item_get_expand (item));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_toolbar_set_child_property (GtkContainer *container,
				GtkWidget    *child,
				guint         property_id,
				const GValue *value,
				GParamSpec   *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_PACK_END:
      gtk_tool_item_set_pack_end (GTK_TOOL_ITEM (child), g_value_get_boolean (value));
      break;

    case CHILD_PROP_HOMOGENEOUS:
      gtk_tool_item_set_homogeneous (GTK_TOOL_ITEM (child), g_value_get_boolean (value));
      break;

    case CHILD_PROP_EXPAND:
      gtk_tool_item_set_homogeneous (GTK_TOOL_ITEM (child), g_value_get_boolean (value));
      break;
      
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_toolbar_add (GtkContainer *container,
		 GtkWidget    *widget)
{
  GtkToolbar *toolbar;
  
  g_return_if_fail (GTK_IS_TOOLBAR (container));
  g_return_if_fail (widget != NULL);

  toolbar = GTK_TOOLBAR (container);
  
  if (GTK_IS_TOOL_ITEM (widget))
    gtk_toolbar_insert (toolbar, GTK_TOOL_ITEM (widget), 0);
  else
    gtk_toolbar_append_widget (toolbar, widget, NULL, NULL);
}

static void
gtk_toolbar_remove (GtkContainer *container,
		    GtkWidget    *widget)
{
  GtkToolbar *toolbar;
  GtkToolItem *item = NULL;
  
  g_return_if_fail (GTK_IS_TOOLBAR (container));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  toolbar = GTK_TOOLBAR (container);

  if (GTK_IS_TOOL_ITEM (widget))
    {
      item = GTK_TOOL_ITEM (widget);
    }
  else
    {
      GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
      GList *list;
      
      for (list = priv->items; list != NULL; list = list->next)
	{
	  if (GTK_BIN (list->data)->child == widget)
	    {
	      item = list->data;
	      break;
	    }
	}
    }

  g_return_if_fail (item != NULL);

  gtk_toolbar_remove_tool_item (GTK_TOOLBAR (container), item);
}

static void
gtk_toolbar_forall (GtkContainer *container,
		    gboolean	  include_internals,
		    GtkCallback   callback,
		    gpointer      callback_data)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (container);
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  GList *items;

  g_return_if_fail (callback != NULL);

  items = priv->items;
      
  while (items)
    {
      GtkToolItem *item = GTK_TOOL_ITEM (items->data);
      
      items = items->next;
      
      (*callback) (GTK_WIDGET (item), callback_data);
    }
  
  if (include_internals)
    (* callback) (priv->arrow_button, callback_data);
}

static GType
gtk_toolbar_child_type (GtkContainer *container)
{
  return GTK_TYPE_TOOL_ITEM;
}

static void
gtk_toolbar_reconfigured (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  GList *items;

  items = priv->items;
  while (items)
    {
      GtkToolItem *item = GTK_TOOL_ITEM (items->data);
      
      gtk_tool_item_toolbar_reconfigured (item);
      
      items = items->next;
    }
}

static void
gtk_toolbar_real_orientation_changed (GtkToolbar    *toolbar,
				      GtkOrientation orientation)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  if (toolbar->orientation != orientation)
    {
      toolbar->orientation = orientation;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_DOWN, GTK_SHADOW_NONE);
      else if (gtk_widget_get_direction (GTK_WIDGET (toolbar)) == GTK_TEXT_DIR_LTR)
	gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
      else 
	gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_LEFT, GTK_SHADOW_NONE);

      gtk_toolbar_reconfigured (toolbar);

      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
      g_object_notify (G_OBJECT (toolbar), "orientation");
    }
}

static void
gtk_toolbar_real_style_changed (GtkToolbar     *toolbar,
				GtkToolbarStyle style)
{
  if (toolbar->style != style)
    {
      toolbar->style = style;

      gtk_toolbar_reconfigured (toolbar);

      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
      g_object_notify (G_OBJECT (toolbar), "toolbar_style");
    }
}

static void
menu_position_func (GtkMenu  *menu,
		    gint     *x,
		    gint     *y,
		    gboolean *push_in,
		    gpointer  user_data)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (user_data);
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  GtkRequisition req;
  GtkRequisition menu_req;
  
  gdk_window_get_origin (GTK_BUTTON (priv->arrow_button)->event_window, x, y);
  gtk_widget_size_request (priv->arrow_button, &req);
  gtk_widget_size_request (GTK_WIDGET (menu), &menu_req);
  
  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *y += priv->arrow_button->allocation.height;
      if (gtk_widget_get_direction (GTK_WIDGET (toolbar)) == GTK_TEXT_DIR_LTR) 
	*x += priv->arrow_button->allocation.width - req.width;
      else 
	*x += req.width - menu_req.width;
    }
  else 
    {
      if (gtk_widget_get_direction (GTK_WIDGET (toolbar)) == GTK_TEXT_DIR_LTR) 
	*x += priv->arrow_button->allocation.width;
      else 
	*x -= menu_req.width;
      *y += priv->arrow_button->allocation.height - req.height;      
    }
  
  *push_in = TRUE;
}

static void
menu_deactivated (GtkWidget  *menu,
		  GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->arrow_button), FALSE);
}

static void
remove_item (GtkWidget *menu_item,
	     gpointer   data)
{
  gtk_container_remove (GTK_CONTAINER (menu_item->parent), menu_item);
}

static void
show_menu (GtkToolbar     *toolbar,
	   GdkEventButton *event)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  GList *list;
  
  if (priv->menu)
    {
      gtk_container_foreach (GTK_CONTAINER (priv->menu), remove_item, NULL);
      gtk_widget_destroy (GTK_WIDGET (priv->menu));
    }

  priv->menu = GTK_MENU (gtk_menu_new ());
  g_signal_connect (priv->menu, "deactivate", G_CALLBACK (menu_deactivated), toolbar);

  for (list = priv->items; list != NULL; list = list->next)
    {
      GtkToolItem *item = list->data;

      if (toolbar_item_visible (toolbar, item) && toolbar_item_get_is_overflow (item))
	{
	  GtkWidget *menu_item = gtk_tool_item_retrieve_proxy_menu_item (item);

	  if (menu_item)
	    {
	      g_assert (GTK_IS_MENU_ITEM (menu_item));
	      gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), menu_item);
	    }
	}
    }

  gtk_widget_show_all (GTK_WIDGET (priv->menu));

  gtk_menu_popup (GTK_MENU (priv->menu), NULL, NULL,
		  menu_position_func, toolbar,
		  event? event->button : 0, event? event->time : gtk_get_current_event_time());
}

static void
gtk_toolbar_arrow_button_clicked (GtkWidget  *button,
				  GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);  

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->arrow_button)) &&
      (!priv->menu || !GTK_WIDGET_VISIBLE (GTK_WIDGET (priv->menu))))
    {
      /* We only get here when the button is clicked with the keybaord,
       * because mouse button presses result in the menu being shown so
       * that priv->menu would be non-NULL and visible.
       */
      show_menu (toolbar, NULL);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (priv->menu), FALSE);
    }
}

static gboolean
gtk_toolbar_arrow_button_press (GtkWidget      *button,
				GdkEventButton *event,
				GtkToolbar     *toolbar)
{
  show_menu (toolbar, event);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
 
  return TRUE;
}

static gboolean
gtk_toolbar_button_press (GtkWidget      *toolbar,
    			  GdkEventButton *event)
{
  if (event->button == 3)
    {
      gboolean return_value;
    
      g_signal_emit (toolbar, toolbar_signals[POPUP_CONTEXT_MENU], 0,
		     (int)event->x_root, (int)event->y_root, event->button,
		     &return_value);

      return return_value;
    }

  return FALSE;
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

static void
gtk_toolbar_update_button_relief (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);

  gtk_toolbar_reconfigured (toolbar);

  gtk_button_set_relief (GTK_BUTTON (priv->arrow_button), get_button_relief (toolbar));
}

static GtkReliefStyle
get_button_relief (GtkToolbar *toolbar)
{
  GtkReliefStyle button_relief = GTK_RELIEF_NORMAL;

  gtk_widget_ensure_style (GTK_WIDGET (toolbar));
  
  gtk_widget_style_get (GTK_WIDGET (toolbar),
                        "button_relief", &button_relief,
                        NULL);

  return button_relief;
}

static gint
get_internal_padding (GtkToolbar *toolbar)
{
  gint ipadding = 0;

  gtk_widget_style_get (GTK_WIDGET (toolbar),
			"internal_padding", &ipadding,
			NULL);

  return ipadding;
}

static GtkShadowType
get_shadow_type (GtkToolbar *toolbar)
{
  GtkShadowType shadow_type;

  gtk_widget_style_get (GTK_WIDGET (toolbar),
			"shadow_type", &shadow_type,
			NULL);

  return shadow_type;
}

static gboolean
gtk_toolbar_check_old_api (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);

  if (priv->api_mode == NEW_API)
    {
      g_warning ("mixing deprecated and non-deprecated GtkToolbar API is not allowed");
      return FALSE;
    }

  priv->api_mode = OLD_API;
  return TRUE;
}

static gboolean
gtk_toolbar_check_new_api (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);

  if (priv->api_mode == OLD_API)
    {
      g_warning ("mixing deprecated and non-deprecated GtkToolbar API is not allowed");
      return FALSE;
    }
  
  priv->api_mode = NEW_API;
  return TRUE;
}

static void
gtk_toolbar_insert_tool_item (GtkToolbar  *toolbar,
			      GtkToolItem *item,
			      gint         pos)
{
  GtkToolbarPrivate *priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  
  priv->items = g_list_insert (priv->items, item, pos);
  toolbar->num_children++;

  gtk_widget_set_parent (GTK_WIDGET (item), GTK_WIDGET (toolbar));
}

static void
gtk_toolbar_remove_tool_item (GtkToolbar  *toolbar,
			      GtkToolItem *item)
{
  GtkToolbarPrivate *priv;
  GList *tmp;
  gint nth_child;

  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));
  priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  g_return_if_fail (GTK_IS_TOOL_ITEM (item));
  g_return_if_fail (g_list_find (priv->items, item));
  
  nth_child = 0;

  for (tmp = priv->items; tmp != NULL; tmp = tmp->next)
    {
      if (tmp->data == item)
	break;

      nth_child++;
    }

  priv->items = g_list_remove (priv->items, item);

  gtk_widget_unparent (GTK_WIDGET (item));

  if (priv->api_mode == OLD_API)
    {
      GtkToolbarChild *toolbar_child;

      toolbar_child = g_list_nth_data (toolbar->children, nth_child);
      toolbar->children = g_list_remove (toolbar->children, toolbar_child);

      g_free (toolbar_child);
    }

  gtk_widget_queue_resize (GTK_WIDGET (toolbar));
}

/**
 * gtk_toolbar_new:
 * 
 * Creates a new toolbar. 

 * Return Value: the newly-created toolbar.
 **/
GtkWidget *
gtk_toolbar_new (void)
{
  GtkToolbar *toolbar;

  toolbar = g_object_new (GTK_TYPE_TOOLBAR, NULL);

  return GTK_WIDGET (toolbar);
}

void
gtk_toolbar_insert (GtkToolbar  *toolbar,
		    GtkToolItem *item,
		    gint         pos)
{
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));
  g_return_if_fail (GTK_IS_TOOL_ITEM (item));

  if (!gtk_toolbar_check_new_api (toolbar))
    return;
  
  gtk_toolbar_insert_tool_item (toolbar, item, pos);
}

gint
gtk_toolbar_get_item_index (GtkToolbar  *toolbar,
			    GtkToolItem *item)
{
  GtkToolbarPrivate *priv;

  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), -1);
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (item), -1);

  if (!gtk_toolbar_check_new_api (toolbar))
    return -1;
  
  priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  g_return_val_if_fail (g_list_find (priv->items, item) != NULL, -1);

  return g_list_index (priv->items, item);
}

/**
 * gtk_toolbar_set_orientation:
 * @toolbar: a #GtkToolbar.
 * @orientation: a new #GtkOrientation.
 * 
 * Sets whether a toolbar should appear horizontally or vertically.
 **/
void
gtk_toolbar_set_orientation (GtkToolbar     *toolbar,
			     GtkOrientation  orientation)
{
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  g_signal_emit (toolbar, toolbar_signals[ORIENTATION_CHANGED], 0, orientation);
}

/**
 * gtk_toolbar_get_orientation:
 * @toolbar: a #GtkToolbar
 * 
 * Retrieves the current orientation of the toolbar. See
 * gtk_toolbar_set_orientation().
 *
 * Return value: the orientation
 **/
GtkOrientation
gtk_toolbar_get_orientation (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), GTK_ORIENTATION_HORIZONTAL);

  return toolbar->orientation;
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
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  toolbar->style_set = TRUE;  
  g_signal_emit (toolbar, toolbar_signals[STYLE_CHANGED], 0, style);

  
}

/**
 * gtk_toolbar_get_style:
 * @toolbar: a #GtkToolbar
 *
 * Retrieves whether the toolbar has text, icons, or both . See
 * gtk_toolbar_set_style().
 
 * Return value: the current style of @toolbar
 **/
GtkToolbarStyle
gtk_toolbar_get_style (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), DEFAULT_TOOLBAR_STYLE);

  return toolbar->style;
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
  GtkToolbarStyle style;

  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (toolbar->style_set)
    {
      GtkSettings *settings = toolbar_get_settings (toolbar);

      if (settings)
	g_object_get (settings,
		      "gtk-toolbar-style", &style,
		      NULL);
      else
	style = DEFAULT_TOOLBAR_STYLE;

      if (style != toolbar->style)
	g_signal_emit (toolbar, toolbar_signals[STYLE_CHANGED], 0, style);

      toolbar->style_set = FALSE;
    }
}

/**
 * gtk_toolbar_set_tooltips:
 * @toolbar: a #GtkToolbar.
 * @enable: set to %FALSE to disable the tooltips, or %TRUE to enable them.
 * 
 * Sets if the tooltips of a toolbar should be active or not.
 **/
void
gtk_toolbar_set_tooltips (GtkToolbar *toolbar,
			  gboolean    enable)
{
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (enable)
    gtk_tooltips_enable (toolbar->tooltips);
  else
    gtk_tooltips_disable (toolbar->tooltips);
}

/**
 * gtk_toolbar_get_tooltips:
 * @toolbar: a #GtkToolbar
 *
 * Retrieves whether tooltips are enabled. See
 * gtk_toolbar_set_tooltips().
 *
 * Return value: %TRUE if tooltips are enabled
 **/
gboolean
gtk_toolbar_get_tooltips (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), FALSE);

  return toolbar->tooltips->enabled;
}

gint
gtk_toolbar_get_n_items (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv;

  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), -1);

  if (!gtk_toolbar_check_new_api (toolbar))
    return -1;
  
  priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  
  return g_list_length (priv->items);
}

/*
 * returns NULL if n is out of range
 */
GtkToolItem *
gtk_toolbar_get_nth_item (GtkToolbar *toolbar,
			  gint        n)
{
  GtkToolbarPrivate *priv;

  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), NULL);

  if (!gtk_toolbar_check_new_api (toolbar))
    return NULL;
  
  priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  
  return g_list_nth_data (priv->items, n);
}

/**
 * gtk_toolbar_set_icon_size:
 * @toolbar: A #GtkToolbar
 * @icon_size: The #GtkIconSize that stock icons in the toolbar shall have.
 *
 * This function sets the size of stock icons in the toolbar. You
 * can call it both before you add the icons and after they've been
 * added. The size you set will override user preferences for the default
 * icon size.
 **/
void
gtk_toolbar_set_icon_size (GtkToolbar  *toolbar,
			   GtkIconSize  icon_size)
{
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  toolbar->icon_size_set = TRUE;

  if (toolbar->icon_size == icon_size)
    return;

  toolbar->icon_size = icon_size;

  gtk_toolbar_reconfigured (toolbar);

  gtk_widget_queue_resize (GTK_WIDGET (toolbar));
}

/**
 * gtk_toolbar_get_icon_size:
 * @toolbar: a #GtkToolbar
 *
 * Retrieves the icon size fo the toolbar. See gtk_toolbar_set_icon_size().
 *
 * Return value: the current icon size for the icons on the toolbar.
 **/
GtkIconSize
gtk_toolbar_get_icon_size (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), DEFAULT_ICON_SIZE);

  return toolbar->icon_size;
}

GtkReliefStyle
gtk_toolbar_get_relief_style (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), GTK_RELIEF_NONE);

  return get_button_relief (toolbar);
}

/**
 * gtk_toolbar_unset_icon_size:
 * @toolbar: a #GtkToolbar
 * 
 * Unsets toolbar icon size set with gtk_toolbar_set_icon_size(), so that
 * user preferences will be used to determine the icon size.
 **/
void
gtk_toolbar_unset_icon_size (GtkToolbar *toolbar)
{
  GtkIconSize size;

  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));
  
  if (toolbar->icon_size_set)
    {
      GtkSettings *settings = toolbar_get_settings (toolbar);

      if (settings)
	{
	  g_object_get (settings,
			"gtk-toolbar-icon-size", &size,
			NULL);
	}
      else
	size = DEFAULT_ICON_SIZE;

      if (size != toolbar->icon_size)
	gtk_toolbar_set_icon_size (toolbar, size);

      toolbar->icon_size_set = FALSE;
    }
}

void
gtk_toolbar_set_show_arrow (GtkToolbar *toolbar,
			    gboolean    show_arrow)
{
  GtkToolbarPrivate *priv;
  
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  show_arrow = show_arrow != FALSE;

  if (priv->show_arrow != show_arrow)
    {
      priv->show_arrow = show_arrow;
      
      if (!priv->show_arrow)
	gtk_widget_hide (priv->arrow_button);
      
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));      
      g_object_notify (G_OBJECT (toolbar), "show_arrow");
    }
}

gboolean
gtk_toolbar_get_show_arrow (GtkToolbar *toolbar)
{
  GtkToolbarPrivate *priv;

  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), FALSE);

  if (!gtk_toolbar_check_new_api (toolbar))
    return FALSE;
  
  priv = GTK_TOOLBAR_GET_PRIVATE (toolbar);
  
  return priv->show_arrow;
}

gint
gtk_toolbar_get_drop_index (GtkToolbar *toolbar,
			    gint        x,
			    gint        y)
{
  gint drop_index, drop_pos;

  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), FALSE);

  if (!gtk_toolbar_check_new_api (toolbar))
    return -1;
  
  find_drop_pos (toolbar, x, y, &drop_index, &drop_pos);

  return drop_index;
}

/**
 * gtk_toolbar_append_item:
 * @toolbar: a #GtkToolbar.
 * @text: give your toolbar button a label.
 * @tooltip_text: a string that appears when the user holds the mouse over this item.
 * @tooltip_private_text: use with #GtkTipsQuery.
 * @icon: a #GtkWidget that should be used as the button's icon.
 * @callback: the function to be executed when the button is pressed.
 * @user_data: a pointer to any data you wish to be passed to the callback.
 * 
 * Inserts a new item into the toolbar. You must specify the position
 * in the toolbar where it will be inserted.
 * 
 * Return value: the new toolbar item as a #GtkWidget.
 **/
GtkWidget *
gtk_toolbar_append_item (GtkToolbar    *toolbar,
			 const char    *text,
			 const char    *tooltip_text,
			 const char    *tooltip_private_text,
			 GtkWidget     *icon,
			 GtkSignalFunc  callback,
			 gpointer       user_data)
{
  return gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_BUTTON,
				     NULL, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     toolbar->num_children);
}

/**
 * gtk_toolbar_prepend_item:
 * @toolbar: a #GtkToolbar.
 * @text: give your toolbar button a label.
 * @tooltip_text: a string that appears when the user holds the mouse over this item.
 * @tooltip_private_text: use with #GtkTipsQuery.
 * @icon: a #GtkWidget that should be used as the button's icon.
 * @callback: the function to be executed when the button is pressed.
 * @user_data: a pointer to any data you wish to be passed to the callback.
 * 
 * Adds a new button to the beginning (top or left edges) of the given toolbar.
 *
 * Return value: the new toolbar item as a #GtkWidget.
 **/
GtkWidget *
gtk_toolbar_prepend_item (GtkToolbar    *toolbar,
			  const char    *text,
			  const char    *tooltip_text,
			  const char    *tooltip_private_text,
			  GtkWidget     *icon,
			  GtkSignalFunc  callback,
			  gpointer       user_data)
{
  return gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_BUTTON,
				     NULL, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     0);
}

/**
 * gtk_toolbar_insert_item:
 * @toolbar: a #GtkToolbar.
 * @text: give your toolbar button a label.
 * @tooltip_text: a string that appears when the user holds the mouse over this item.
 * @tooltip_private_text: use with #GtkTipsQuery.
 * @icon: a #GtkWidget that should be used as the button's icon.
 * @callback: the function to be executed when the button is pressed.
 * @user_data: a pointer to any data you wish to be passed to the callback.
 * @position: the number of widgets to insert this item after.
 * 
 * Inserts a new item into the toolbar. You must specify the position in the
 * toolbar where it will be inserted.
 *
 * Return value: the new toolbar item as a #GtkWidget.
 **/
GtkWidget *
gtk_toolbar_insert_item (GtkToolbar    *toolbar,
			 const char    *text,
			 const char    *tooltip_text,
			 const char    *tooltip_private_text,
			 GtkWidget     *icon,
			 GtkSignalFunc  callback,
			 gpointer       user_data,
			 gint           position)
{
  return gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_BUTTON,
				     NULL, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     position);
}

/**
 * gtk_toolbar_insert_stock:
 * @toolbar: A #GtkToolbar
 * @stock_id: The id of the stock item you want to insert
 * @tooltip_text: The text in the tooltip of the toolbar button
 * @tooltip_private_text: The private text of the tooltip
 * @callback: The callback called when the toolbar button is clicked.
 * @user_data: user data passed to callback
 * @position: The position the button shall be inserted at.
 *            -1 means at the end.
 *
 * Inserts a stock item at the specified position of the toolbar.  If
 * @stock_id is not a known stock item ID, it's inserted verbatim,
 * except that underscores used to mark mnemonics are removed.
 *
 * Returns: the inserted widget
 */
GtkWidget*
gtk_toolbar_insert_stock (GtkToolbar      *toolbar,
			  const gchar     *stock_id,
			  const char      *tooltip_text,
			  const char      *tooltip_private_text,
			  GtkSignalFunc    callback,
			  gpointer         user_data,
			  gint             position)
{
  return gtk_toolbar_internal_insert_element (toolbar, GTK_TOOLBAR_CHILD_BUTTON,
					      NULL, stock_id,
					      tooltip_text, tooltip_private_text,
					      NULL, callback, user_data,
					      position, TRUE);
}

/**
 * gtk_toolbar_append_space:
 * @toolbar: a #GtkToolbar.
 * 
 * Adds a new space to the end of the toolbar.
 **/
void
gtk_toolbar_append_space (GtkToolbar *toolbar)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      toolbar->num_children);
}

/**
 * gtk_toolbar_prepend_space:
 * @toolbar: a #GtkToolbar.
 * 
 * Adds a new space to the beginning of the toolbar.
 **/
void
gtk_toolbar_prepend_space (GtkToolbar *toolbar)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      0);
}

/**
 * gtk_toolbar_insert_space:
 * @toolbar: a #GtkToolbar
 * @position: the number of widgets after which a space should be inserted.
 * 
 * Inserts a new space in the toolbar at the specified position.
 **/
void
gtk_toolbar_insert_space (GtkToolbar *toolbar,
			  gint        position)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      position);
}

/**
 * gtk_toolbar_remove_space:
 * @toolbar: a #GtkToolbar.
 * @position: the index of the space to remove.
 * 
 * Removes a space from the specified position.
 **/
void
gtk_toolbar_remove_space (GtkToolbar *toolbar,
			  gint        position)
{
  GtkToolItem *item;

  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (!gtk_toolbar_check_old_api (toolbar))
    return;
  
  item = g_list_nth_data (toolbar->children, position);

  if (!item)
    {
      g_warning ("Toolbar position %d doesn't exist", position);
      return;
    }

  if (!GTK_IS_SEPARATOR_TOOL_ITEM (item))
    {
      g_warning ("Toolbar position %d is not a space", position);
      return;
    }

  gtk_toolbar_remove_tool_item (toolbar, item);
}

/**
 * gtk_toolbar_append_widget:
 * @toolbar: a #GtkToolbar.
 * @widget: a #GtkWidget to add to the toolbar. 
 * @tooltip_text: the element's tooltip.
 * @tooltip_private_text: used for context-sensitive help about this toolbar element.
 * 
 * Adds a widget to the end of the given toolbar.
 **/ 
void
gtk_toolbar_append_widget (GtkToolbar  *toolbar,
			   GtkWidget   *widget,
			   const gchar *tooltip_text,
			   const gchar *tooltip_private_text)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      toolbar->num_children);
}

/**
 * gtk_toolbar_prepend_widget:
 * @toolbar: a #GtkToolbar.
 * @widget: a #GtkWidget to add to the toolbar. 
 * @tooltip_text: the element's tooltip.
 * @tooltip_private_text: used for context-sensitive help about this toolbar element.
 * 
 * Adds a widget to the beginning of the given toolbar.
 **/ 
void
gtk_toolbar_prepend_widget (GtkToolbar  *toolbar,
			    GtkWidget   *widget,
			    const gchar *tooltip_text,
			    const gchar *tooltip_private_text)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      0);
}

/**
 * gtk_toolbar_insert_widget:
 * @toolbar: a #GtkToolbar.
 * @widget: a #GtkWidget to add to the toolbar. 
 * @tooltip_text: the element's tooltip.
 * @tooltip_private_text: used for context-sensitive help about this toolbar element.
 * @position: the number of widgets to insert this widget after.
 * 
 * Inserts a widget in the toolbar at the given position.
 **/ 
void
gtk_toolbar_insert_widget (GtkToolbar *toolbar,
			   GtkWidget  *widget,
			   const char *tooltip_text,
			   const char *tooltip_private_text,
			   gint        position)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      position);
}

/**
 * gtk_toolbar_append_element:
 * @toolbar: a #GtkToolbar.
 * @type: a value of type #GtkToolbarChildType that determines what @widget will be.
 * @widget: a #GtkWidget, or %NULL.
 * @text: the element's label.
 * @tooltip_text: the element's tooltip.
 * @tooltip_private_text: used for context-sensitive help about this toolbar element.
 * @icon: a #GtkWidget that provides pictorial representation of the element's function.
 * @callback: the function to be executed when the button is pressed.
 * @user_data: any data you wish to pass to the callback.
 * 
 * Adds a new element to the end of a toolbar.
 * 
 * If @type == %GTK_TOOLBAR_CHILD_WIDGET, @widget is used as the new element.
 * If @type == %GTK_TOOLBAR_CHILD_RADIOBUTTON, @widget is used to determine
 * the radio group for the new element. In all other cases, @widget must
 * be %NULL.
 * 
 * Return value: the new toolbar element as a #GtkWidget.
 **/
GtkWidget*
gtk_toolbar_append_element (GtkToolbar          *toolbar,
			    GtkToolbarChildType  type,
			    GtkWidget           *widget,
			    const char          *text,
			    const char          *tooltip_text,
			    const char          *tooltip_private_text,
			    GtkWidget           *icon,
			    GtkSignalFunc        callback,
			    gpointer             user_data)
{
  return gtk_toolbar_insert_element (toolbar, type, widget, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     toolbar->num_children);
}

/**
 * gtk_toolbar_prepend_element:
 * 
 * @toolbar: a #GtkToolbar.
 * @type: a value of type #GtkToolbarChildType that determines what @widget will be.
 * @widget: a #GtkWidget, or %NULL
 * @text: the element's label.
 * @tooltip_text: the element's tooltip.
 * @tooltip_private_text: used for context-sensitive help about this toolbar element.
 * @icon: a #GtkWidget that provides pictorial representation of the element's function.
 * @callback: the function to be executed when the button is pressed.
 * @user_data: any data you wish to pass to the callback.
 *  
 * Adds a new element to the beginning of a toolbar.
 * 
 * If @type == %GTK_TOOLBAR_CHILD_WIDGET, @widget is used as the new element.
 * If @type == %GTK_TOOLBAR_CHILD_RADIOBUTTON, @widget is used to determine
 * the radio group for the new element. In all other cases, @widget must
 * *be %NULL.
 * 
 * Return value: the new toolbar element as a #GtkWidget.
 **/
GtkWidget *
gtk_toolbar_prepend_element (GtkToolbar          *toolbar,
			     GtkToolbarChildType  type,
			     GtkWidget           *widget,
			     const char          *text,
			     const char          *tooltip_text,
			     const char          *tooltip_private_text,
			     GtkWidget           *icon,
			     GtkSignalFunc        callback,
			     gpointer             user_data)
{
  return gtk_toolbar_insert_element (toolbar, type, widget, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data, 0);
}

/**
 * gtk_toolbar_insert_element:
 * @toolbar: a #GtkToolbar.
 * @type: a value of type #GtkToolbarChildType that determines what @widget
 *   will be.
 * @widget: a #GtkWidget, or %NULL. 
 * @text: the element's label.
 * @tooltip_text: the element's tooltip.
 * @tooltip_private_text: used for context-sensitive help about this toolbar element.
 * @icon: a #GtkWidget that provides pictorial representation of the element's function.
 * @callback: the function to be executed when the button is pressed.
 * @user_data: any data you wish to pass to the callback.
 * @position: the number of widgets to insert this element after.
 *
 * Inserts a new element in the toolbar at the given position. 
 *
 * If @type == %GTK_TOOLBAR_CHILD_WIDGET, @widget is used as the new element.
 * If @type == %GTK_TOOLBAR_CHILD_RADIOBUTTON, @widget is used to determine
 * the radio group for the new element. In all other cases, @widget must
 * be %NULL.
 *
 * Return value: the new toolbar element as a #GtkWidget.
 **/
GtkWidget *
gtk_toolbar_insert_element (GtkToolbar          *toolbar,
			    GtkToolbarChildType  type,
			    GtkWidget           *widget,
			    const char          *text,
			    const char          *tooltip_text,
			    const char          *tooltip_private_text,
			    GtkWidget           *icon,
			    GtkSignalFunc        callback,
			    gpointer             user_data,
			    gint                 position)
{
  return gtk_toolbar_internal_insert_element (toolbar, type, widget, text,
					      tooltip_text, tooltip_private_text,
					      icon, callback, user_data, position, FALSE);
}

gchar *
_gtk_toolbar_elide_underscores (const gchar *original)
{
  gchar *q, *result;
  const gchar *p;
  gboolean last_underscore;

  q = result = g_malloc (strlen (original) + 1);
  last_underscore = FALSE;
  
  for (p = original; *p; p++)
    {
      if (!last_underscore && *p == '_')
	last_underscore = TRUE;
      else
	{
	  last_underscore = FALSE;
	  *q++ = *p;
	}
    }
  
  *q = '\0';
  
  return result;
}

static GtkWidget *
gtk_toolbar_internal_insert_element (GtkToolbar          *toolbar,
				     GtkToolbarChildType  type,
				     GtkWidget           *widget,
				     const char          *text,
				     const char          *tooltip_text,
				     const char          *tooltip_private_text,
				     GtkWidget           *icon,
				     GtkSignalFunc        callback,
				     gpointer             user_data,
				     gint                 position,
				     gboolean             use_stock)
{
  GtkToolbarChild *child;
  GtkToolItem *item = NULL;
  
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), NULL);
  
  if (!gtk_toolbar_check_old_api (toolbar))
    return NULL;
  
  if (type == GTK_TOOLBAR_CHILD_WIDGET)
    g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  else if (type != GTK_TOOLBAR_CHILD_RADIOBUTTON)
    g_return_val_if_fail (widget == NULL, NULL);

  child = g_new (GtkToolbarChild, 1);

  child->type = type;
  child->icon = NULL;
  child->label = NULL;

  switch (type)
    {
    case GTK_TOOLBAR_CHILD_SPACE:
      item = gtk_separator_tool_item_new ();
      child->widget = NULL;
      break;

    case GTK_TOOLBAR_CHILD_WIDGET:
      item = gtk_tool_item_new ();
      child->widget = widget;
      gtk_container_add (GTK_CONTAINER (item), child->widget);
      break;

    case GTK_TOOLBAR_CHILD_BUTTON:
      item = gtk_tool_button_new (NULL, NULL);
      child->widget = _gtk_tool_button_get_button (GTK_TOOL_BUTTON (item));
      break;
      
    case GTK_TOOLBAR_CHILD_TOGGLEBUTTON:
      item = gtk_toggle_tool_button_new ();
      child->widget = _gtk_tool_button_get_button (GTK_TOOL_BUTTON (item));
      break;

    case GTK_TOOLBAR_CHILD_RADIOBUTTON:
      item = gtk_radio_tool_button_new (widget
					? gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget))
					: NULL);
      child->widget = _gtk_tool_button_get_button (GTK_TOOL_BUTTON (item));
      break;
    }

  gtk_widget_show (GTK_WIDGET (item));
  
  if (type == GTK_TOOLBAR_CHILD_BUTTON ||
      type == GTK_TOOLBAR_CHILD_RADIOBUTTON ||
      type == GTK_TOOLBAR_CHILD_TOGGLEBUTTON)
    {
      if (text)
	{
	  if (use_stock)
	    {
	      GtkStockItem stock_item;
	      gchar *label_text;

	      gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (item), text);

	      gtk_stock_lookup (text, &stock_item);
	      label_text = _gtk_toolbar_elide_underscores (stock_item.label);
	      child->label = GTK_WIDGET (gtk_label_new (label_text));
	      g_free (label_text);
	    }
	  else
	    {
	      child->label = gtk_label_new (text);
	    }
	  gtk_tool_button_set_label_widget (GTK_TOOL_BUTTON (item), child->label);
	  gtk_widget_show (child->label);
	}

      if (icon)
	{
	  child->icon = icon;
	  gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (item), icon);

	  /* Applications depend on the toolbar showing the widget for them */
	  gtk_widget_show (GTK_WIDGET (icon));
	}

      /*
       * We need to connect to the button's clicked callback because some
       * programs may rely on that the widget in the callback is a GtkButton
       */
      if (callback)
	g_signal_connect (child->widget, "clicked",
			  callback, user_data);
    }

  if ((type != GTK_TOOLBAR_CHILD_SPACE) && tooltip_text)
    gtk_tool_item_set_tooltip (item, toolbar->tooltips,
			       tooltip_text, tooltip_private_text);
  
  toolbar->children = g_list_insert (toolbar->children, child, position);

  gtk_toolbar_insert_tool_item (toolbar, item, position);

  return child->widget;
}

static void
gtk_toolbar_finalize (GObject *object)
{
  GList *list;
  GtkToolbar *toolbar = GTK_TOOLBAR (object);

  if (toolbar->tooltips)
    g_object_unref (toolbar->tooltips);

  for (list = toolbar->children; list != NULL; list = list->next)
    g_free (list->data);

  g_list_free (toolbar->children);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}
