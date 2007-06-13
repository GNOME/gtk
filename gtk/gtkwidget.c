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

#include <config.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include "gtkcontainer.h"
#include "gtkaccelmap.h"
#include "gtkclipboard.h"
#include "gtkiconfactory.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkrc.h"
#include "gtkselection.h"
#include "gtksettings.h"
#include "gtksizegroup.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtkbindings.h"
#include "gtkprivate.h"
#include "gdk/gdk.h"
#include "gdk/gdkprivate.h" /* Used in gtk_reset_shapes_recurse to avoid copy */
#include <gobject/gvaluecollector.h>
#include <gobject/gobjectnotifyqueue.c>
#include "gdk/gdkkeysyms.h"
#include "gtkintl.h"
#include "gtkaccessible.h"
#include "gtktooltips.h"
#include "gtkinvisible.h"
#include "gtkalias.h"

#define WIDGET_CLASS(w)	 GTK_WIDGET_GET_CLASS (w)
#define	INIT_PATH_SIZE	(512)


enum {
  SHOW,
  HIDE,
  MAP,
  UNMAP,
  REALIZE,
  UNREALIZE,
  SIZE_REQUEST,
  SIZE_ALLOCATE,
  STATE_CHANGED,
  PARENT_SET,
  HIERARCHY_CHANGED,
  STYLE_SET,
  DIRECTION_CHANGED,
  GRAB_NOTIFY,
  CHILD_NOTIFY,
  MNEMONIC_ACTIVATE,
  GRAB_FOCUS,
  FOCUS,
  EVENT,
  EVENT_AFTER,
  BUTTON_PRESS_EVENT,
  BUTTON_RELEASE_EVENT,
  SCROLL_EVENT,
  MOTION_NOTIFY_EVENT,
  DELETE_EVENT,
  DESTROY_EVENT,
  EXPOSE_EVENT,
  KEY_PRESS_EVENT,
  KEY_RELEASE_EVENT,
  ENTER_NOTIFY_EVENT,
  LEAVE_NOTIFY_EVENT,
  CONFIGURE_EVENT,
  FOCUS_IN_EVENT,
  FOCUS_OUT_EVENT,
  MAP_EVENT,
  UNMAP_EVENT,
  PROPERTY_NOTIFY_EVENT,
  SELECTION_CLEAR_EVENT,
  SELECTION_REQUEST_EVENT,
  SELECTION_NOTIFY_EVENT,
  SELECTION_GET,
  SELECTION_RECEIVED,
  PROXIMITY_IN_EVENT,
  PROXIMITY_OUT_EVENT,
  DRAG_BEGIN,
  DRAG_END,
  DRAG_DATA_DELETE,
  DRAG_LEAVE,
  DRAG_MOTION,
  DRAG_DROP,
  DRAG_DATA_GET,
  DRAG_DATA_RECEIVED,
  CLIENT_EVENT,
  NO_EXPOSE_EVENT,
  VISIBILITY_NOTIFY_EVENT,
  WINDOW_STATE_EVENT,
  POPUP_MENU,
  SHOW_HELP,
  ACCEL_CLOSURES_CHANGED,
  SCREEN_CHANGED,
  CAN_ACTIVATE_ACCEL,
  GRAB_BROKEN,
  COMPOSITED_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_PARENT,
  PROP_WIDTH_REQUEST,
  PROP_HEIGHT_REQUEST,
  PROP_VISIBLE,
  PROP_SENSITIVE,
  PROP_APP_PAINTABLE,
  PROP_CAN_FOCUS,
  PROP_HAS_FOCUS,
  PROP_IS_FOCUS,
  PROP_CAN_DEFAULT,
  PROP_HAS_DEFAULT,
  PROP_RECEIVES_DEFAULT,
  PROP_COMPOSITE_CHILD,
  PROP_STYLE,
  PROP_EVENTS,
  PROP_EXTENSION_EVENTS,
  PROP_NO_SHOW_ALL
};

typedef	struct	_GtkStateData	 GtkStateData;

struct _GtkStateData
{
  GtkStateType  state;
  guint		state_restoration : 1;
  guint         parent_sensitive : 1;
  guint		use_forall : 1;
};


/* --- prototypes --- */
static void	gtk_widget_class_init		(GtkWidgetClass     *klass);
static void	gtk_widget_base_class_finalize	(GtkWidgetClass     *klass);
static void	gtk_widget_init			(GtkWidget          *widget);
static void	gtk_widget_set_property		 (GObject           *object,
						  guint              prop_id,
						  const GValue      *value,
						  GParamSpec        *pspec);
static void	gtk_widget_get_property		 (GObject           *object,
						  guint              prop_id,
						  GValue            *value,
						  GParamSpec        *pspec);
static void	gtk_widget_dispose		 (GObject	    *object);
static void	gtk_widget_real_destroy		 (GtkObject	    *object);
static void	gtk_widget_finalize		 (GObject	    *object);
static void	gtk_widget_real_show		 (GtkWidget	    *widget);
static void	gtk_widget_real_hide		 (GtkWidget	    *widget);
static void	gtk_widget_real_map		 (GtkWidget	    *widget);
static void	gtk_widget_real_unmap		 (GtkWidget	    *widget);
static void	gtk_widget_real_realize		 (GtkWidget	    *widget);
static void	gtk_widget_real_unrealize	 (GtkWidget	    *widget);
static void	gtk_widget_real_size_request	 (GtkWidget	    *widget,
						  GtkRequisition    *requisition);
static void	gtk_widget_real_size_allocate	 (GtkWidget	    *widget,
						  GtkAllocation	    *allocation);
static void	gtk_widget_style_set		 (GtkWidget	    *widget,
						  GtkStyle          *previous_style);
static void	gtk_widget_direction_changed	 (GtkWidget	    *widget,
						  GtkTextDirection   previous_direction);

static void	gtk_widget_real_grab_focus	 (GtkWidget         *focus_widget);
static gboolean gtk_widget_real_show_help        (GtkWidget         *widget,
                                                  GtkWidgetHelpType  help_type);

static void	gtk_widget_dispatch_child_properties_changed	(GtkWidget        *object,
								 guint             n_pspecs,
								 GParamSpec      **pspecs);
static gboolean		gtk_widget_real_key_press_event   	(GtkWidget        *widget,
								 GdkEventKey      *event);
static gboolean		gtk_widget_real_key_release_event 	(GtkWidget        *widget,
								 GdkEventKey      *event);
static gboolean		gtk_widget_real_focus_in_event   	 (GtkWidget       *widget,
								  GdkEventFocus   *event);
static gboolean		gtk_widget_real_focus_out_event   	(GtkWidget        *widget,
								 GdkEventFocus    *event);
static gboolean		gtk_widget_real_focus			(GtkWidget        *widget,
								 GtkDirectionType  direction);
static PangoContext*	gtk_widget_peek_pango_context		(GtkWidget	  *widget);
static void     	gtk_widget_update_pango_context		(GtkWidget	  *widget);
static void		gtk_widget_propagate_state		(GtkWidget	  *widget,
								 GtkStateData 	  *data);
static void             gtk_widget_reset_rc_style               (GtkWidget        *widget);
static void		gtk_widget_set_style_internal		(GtkWidget	  *widget,
								 GtkStyle	  *style,
								 gboolean	   initial_emission);
static gint		gtk_widget_event_internal		(GtkWidget	  *widget,
								 GdkEvent	  *event);
static gboolean		gtk_widget_real_mnemonic_activate	(GtkWidget	  *widget,
								 gboolean	   group_cycling);
static void		gtk_widget_aux_info_destroy		(GtkWidgetAuxInfo *aux_info);
static AtkObject*	gtk_widget_real_get_accessible		(GtkWidget	  *widget);
static void		gtk_widget_accessible_interface_init	(AtkImplementorIface *iface);
static AtkObject*	gtk_widget_ref_accessible		(AtkImplementor *implementor);
static void             gtk_widget_invalidate_widget_windows    (GtkWidget        *widget,
								 GdkRegion        *region);
static GdkScreen *      gtk_widget_get_screen_unchecked         (GtkWidget        *widget);
static void		gtk_widget_queue_shallow_draw		(GtkWidget        *widget);
static gboolean         gtk_widget_real_can_activate_accel      (GtkWidget *widget,
                                                                 guint      signal_id);
     
static void gtk_widget_set_usize_internal (GtkWidget *widget,
					   gint       width,
					   gint       height);
static void gtk_widget_get_draw_rectangle (GtkWidget    *widget,
					   GdkRectangle *rect);


/* --- variables --- */
static gpointer         gtk_widget_parent_class = NULL;
static guint            widget_signals[LAST_SIGNAL] = { 0 };
static GtkStyle        *gtk_default_style = NULL;
static GSList          *colormap_stack = NULL;
static guint            composite_child_stack = 0;
static GtkTextDirection gtk_default_direction = GTK_TEXT_DIR_LTR;
static GParamSpecPool  *style_property_spec_pool = NULL;

static GQuark		quark_property_parser = 0;
static GQuark		quark_aux_info = 0;
static GQuark		quark_accel_path = 0;
static GQuark		quark_accel_closures = 0;
static GQuark		quark_event_mask = 0;
static GQuark		quark_extension_event_mode = 0;
static GQuark		quark_parent_window = 0;
static GQuark		quark_shape_info = 0;
static GQuark		quark_input_shape_info = 0;
static GQuark		quark_colormap = 0;
static GQuark		quark_pango_context = 0;
static GQuark		quark_rc_style = 0;
static GQuark		quark_accessible_object = 0;
static GQuark		quark_mnemonic_labels = 0;
GParamSpecPool         *_gtk_widget_child_property_pool = NULL;
GObjectNotifyContext   *_gtk_widget_child_property_notify_context = NULL;

/* --- functions --- */
GType
gtk_widget_get_type (void)
{
  static GType widget_type = 0;

  if (G_UNLIKELY (widget_type == 0))
    {
      const GTypeInfo widget_info =
      {
	sizeof (GtkWidgetClass),
	NULL,		/* base_init */
	(GBaseFinalizeFunc) gtk_widget_base_class_finalize,
	(GClassInitFunc) gtk_widget_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_init */
	sizeof (GtkWidget),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_widget_init,
	NULL,		/* value_table */
      };

      const GInterfaceInfo accessibility_info =
      {
	(GInterfaceInitFunc) gtk_widget_accessible_interface_init,
	(GInterfaceFinalizeFunc) NULL,
	NULL /* interface data */
      };

      widget_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkWidget",
                                           &widget_info, G_TYPE_FLAG_ABSTRACT);

      g_type_add_interface_static (widget_type, ATK_TYPE_IMPLEMENTOR,
                                   &accessibility_info) ;

    }

  return widget_type;
}

static void
child_property_notify_dispatcher (GObject     *object,
				  guint        n_pspecs,
				  GParamSpec **pspecs)
{
  GTK_WIDGET_GET_CLASS (object)->dispatch_child_properties_changed (GTK_WIDGET (object), n_pspecs, pspecs);
}

static void
gtk_widget_class_init (GtkWidgetClass *klass)
{
  static GObjectNotifyContext cpn_context = { 0, NULL, NULL };
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set;

  gtk_widget_parent_class = g_type_class_peek_parent (klass);

  quark_property_parser = g_quark_from_static_string ("gtk-rc-property-parser");
  quark_aux_info = g_quark_from_static_string ("gtk-aux-info");
  quark_accel_path = g_quark_from_static_string ("gtk-accel-path");
  quark_accel_closures = g_quark_from_static_string ("gtk-accel-closures");
  quark_event_mask = g_quark_from_static_string ("gtk-event-mask");
  quark_extension_event_mode = g_quark_from_static_string ("gtk-extension-event-mode");
  quark_parent_window = g_quark_from_static_string ("gtk-parent-window");
  quark_shape_info = g_quark_from_static_string ("gtk-shape-info");
  quark_input_shape_info = g_quark_from_static_string ("gtk-input-shape-info");
  quark_colormap = g_quark_from_static_string ("gtk-colormap");
  quark_pango_context = g_quark_from_static_string ("gtk-pango-context");
  quark_rc_style = g_quark_from_static_string ("gtk-rc-style");
  quark_accessible_object = g_quark_from_static_string ("gtk-accessible-object");
  quark_mnemonic_labels = g_quark_from_static_string ("gtk-mnemonic-labels");

  style_property_spec_pool = g_param_spec_pool_new (FALSE);
  _gtk_widget_child_property_pool = g_param_spec_pool_new (TRUE);
  cpn_context.quark_notify_queue = g_quark_from_static_string ("GtkWidget-child-property-notify-queue");
  cpn_context.dispatcher = child_property_notify_dispatcher;
  _gtk_widget_child_property_notify_context = &cpn_context;

  gobject_class->dispose = gtk_widget_dispose;
  gobject_class->finalize = gtk_widget_finalize;
  gobject_class->set_property = gtk_widget_set_property;
  gobject_class->get_property = gtk_widget_get_property;

  object_class->destroy = gtk_widget_real_destroy;
  
  klass->activate_signal = 0;
  klass->set_scroll_adjustments_signal = 0;
  klass->dispatch_child_properties_changed = gtk_widget_dispatch_child_properties_changed;
  klass->show = gtk_widget_real_show;
  klass->show_all = gtk_widget_show;
  klass->hide = gtk_widget_real_hide;
  klass->hide_all = gtk_widget_hide;
  klass->map = gtk_widget_real_map;
  klass->unmap = gtk_widget_real_unmap;
  klass->realize = gtk_widget_real_realize;
  klass->unrealize = gtk_widget_real_unrealize;
  klass->size_request = gtk_widget_real_size_request;
  klass->size_allocate = gtk_widget_real_size_allocate;
  klass->state_changed = NULL;
  klass->parent_set = NULL;
  klass->hierarchy_changed = NULL;
  klass->style_set = gtk_widget_style_set;
  klass->direction_changed = gtk_widget_direction_changed;
  klass->grab_notify = NULL;
  klass->child_notify = NULL;
  klass->mnemonic_activate = gtk_widget_real_mnemonic_activate;
  klass->grab_focus = gtk_widget_real_grab_focus;
  klass->focus = gtk_widget_real_focus;
  klass->event = NULL;
  klass->button_press_event = NULL;
  klass->button_release_event = NULL;
  klass->motion_notify_event = NULL;
  klass->delete_event = NULL;
  klass->destroy_event = NULL;
  klass->expose_event = NULL;
  klass->key_press_event = gtk_widget_real_key_press_event;
  klass->key_release_event = gtk_widget_real_key_release_event;
  klass->enter_notify_event = NULL;
  klass->leave_notify_event = NULL;
  klass->configure_event = NULL;
  klass->focus_in_event = gtk_widget_real_focus_in_event;
  klass->focus_out_event = gtk_widget_real_focus_out_event;
  klass->map_event = NULL;
  klass->unmap_event = NULL;
  klass->window_state_event = NULL;
  klass->property_notify_event = _gtk_selection_property_notify;
  klass->selection_clear_event = gtk_selection_clear;
  klass->selection_request_event = _gtk_selection_request;
  klass->selection_notify_event = _gtk_selection_notify;
  klass->selection_received = NULL;
  klass->proximity_in_event = NULL;
  klass->proximity_out_event = NULL;
  klass->drag_begin = NULL;
  klass->drag_end = NULL;
  klass->drag_data_delete = NULL;
  klass->drag_leave = NULL;
  klass->drag_motion = NULL;
  klass->drag_drop = NULL;
  klass->drag_data_received = NULL;
  klass->screen_changed = NULL;
  klass->can_activate_accel = gtk_widget_real_can_activate_accel;
  klass->grab_broken_event = NULL;

  klass->show_help = gtk_widget_real_show_help;
  
  /* Accessibility support */
  klass->get_accessible = gtk_widget_real_get_accessible;

  klass->no_expose_event = NULL;

  g_object_class_install_property (gobject_class,
				   PROP_NAME,
				   g_param_spec_string ("name",
 							P_("Widget name"),
							P_("The name of the widget"),
							NULL,
							GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_PARENT,
				   g_param_spec_object ("parent",
							P_("Parent widget"), 
							P_("The parent widget of this widget. Must be a Container widget"),
							GTK_TYPE_CONTAINER,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_WIDTH_REQUEST,
				   g_param_spec_int ("width-request",
 						     P_("Width request"),
 						     P_("Override for width request of the widget, or -1 if natural request should be used"),
 						     -1,
 						     G_MAXINT,
 						     -1,
 						     GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_HEIGHT_REQUEST,
				   g_param_spec_int ("height-request",
 						     P_("Height request"),
 						     P_("Override for height request of the widget, or -1 if natural request should be used"),
 						     -1,
 						     G_MAXINT,
 						     -1,
 						     GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_VISIBLE,
				   g_param_spec_boolean ("visible",
 							 P_("Visible"),
 							 P_("Whether the widget is visible"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SENSITIVE,
				   g_param_spec_boolean ("sensitive",
 							 P_("Sensitive"),
 							 P_("Whether the widget responds to input"),
 							 TRUE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_APP_PAINTABLE,
				   g_param_spec_boolean ("app-paintable",
 							 P_("Application paintable"),
 							 P_("Whether the application will paint directly on the widget"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_CAN_FOCUS,
				   g_param_spec_boolean ("can-focus",
 							 P_("Can focus"),
 							 P_("Whether the widget can accept the input focus"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_HAS_FOCUS,
				   g_param_spec_boolean ("has-focus",
 							 P_("Has focus"),
 							 P_("Whether the widget has the input focus"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_IS_FOCUS,
				   g_param_spec_boolean ("is-focus",
 							 P_("Is focus"),
 							 P_("Whether the widget is the focus widget within the toplevel"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_CAN_DEFAULT,
				   g_param_spec_boolean ("can-default",
 							 P_("Can default"),
 							 P_("Whether the widget can be the default widget"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_HAS_DEFAULT,
				   g_param_spec_boolean ("has-default",
 							 P_("Has default"),
 							 P_("Whether the widget is the default widget"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_RECEIVES_DEFAULT,
				   g_param_spec_boolean ("receives-default",
 							 P_("Receives default"),
 							 P_("If TRUE, the widget will receive the default action when it is focused"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_COMPOSITE_CHILD,
				   g_param_spec_boolean ("composite-child",
 							 P_("Composite child"),
 							 P_("Whether the widget is part of a composite widget"),
 							 FALSE,
 							 GTK_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
				   PROP_STYLE,
				   g_param_spec_object ("style",
 							P_("Style"),
 							P_("The style of the widget, which contains information about how it will look (colors etc)"),
 							GTK_TYPE_STYLE,
 							GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_EVENTS,
				   g_param_spec_flags ("events",
 						       P_("Events"),
 						       P_("The event mask that decides what kind of GdkEvents this widget gets"),
 						       GDK_TYPE_EVENT_MASK,
 						       GDK_STRUCTURE_MASK,
 						       GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_EXTENSION_EVENTS,
				   g_param_spec_enum ("extension-events",
 						      P_("Extension events"),
 						      P_("The mask that decides what kind of extension events this widget gets"),
 						      GDK_TYPE_EXTENSION_MODE,
 						      GDK_EXTENSION_EVENTS_NONE,
 						      GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_NO_SHOW_ALL,
				   g_param_spec_boolean ("no-show-all",
 							 P_("No show all"),
 							 P_("Whether gtk_widget_show_all() should not affect this widget"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  widget_signals[SHOW] =
    g_signal_new (I_("show"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, show),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  widget_signals[HIDE] =
    g_signal_new (I_("hide"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, hide),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  widget_signals[MAP] =
    g_signal_new (I_("map"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, map),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  widget_signals[UNMAP] =
    g_signal_new (I_("unmap"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, unmap),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  widget_signals[REALIZE] =
    g_signal_new (I_("realize"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, realize),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  widget_signals[UNREALIZE] =
    g_signal_new (I_("unrealize"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, unrealize),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  widget_signals[SIZE_REQUEST] =
    g_signal_new (I_("size_request"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, size_request),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_REQUISITION | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[SIZE_ALLOCATE] = 
    g_signal_new (I_("size_allocate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, size_allocate),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_RECTANGLE | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[STATE_CHANGED] =
    g_signal_new (I_("state_changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, state_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_STATE_TYPE);

  /**
   * GtkWidget::parent-set:
   * @widget: the object on which the signal is emitted
   * @old_parent: the previous parent, or %NULL if the widget 
   *   just got its initial parent.
   *
   * The parent-set signal is emitted when a new parent has been set 
   * on a widget. 
   */
  widget_signals[PARENT_SET] =
    g_signal_new (I_("parent_set"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, parent_set),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);
  widget_signals[HIERARCHY_CHANGED] =
    g_signal_new (I_("hierarchy_changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, hierarchy_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);
  /**
   * GtkWidget::style-set:
   * @widget: the object on which the signal is emitted
   * @previous_style: the previous style, or %NULL if the widget 
   *   just got its initial style 
   *
   * The style-set signal is emitted when a new style has been set 
   * on a widget. Note that style-modifying functions like 
   * gtk_widget_modify_base() also cause this signal to be emitted.
   */
  widget_signals[STYLE_SET] =
    g_signal_new (I_("style_set"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, style_set),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_STYLE);
  widget_signals[DIRECTION_CHANGED] =
    g_signal_new (I_("direction_changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, direction_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_TEXT_DIRECTION);

  /**
   * GtkWidget::grab-notify:
   * @widget: the object which received the signal
   * @was_grabbed: %FALSE if the widget becomes shadowed, %TRUE
   *               if it becomes unshadowed
   *
   * The ::grab-notify signal is emitted when a widget becomes
   * shadowed by a GTK+ grab (not a pointer or keyboard grab) on 
   * another widget, or when it becomes unshadowed due to a grab 
   * being removed.
   * 
   * A widget is shadowed by a gtk_grab_add() when the topmost 
   * grab widget in the grab stack of its window group is not 
   * its ancestor.
   */
  widget_signals[GRAB_NOTIFY] =
    g_signal_new (I_("grab_notify"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, grab_notify),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE, 1,
		  G_TYPE_BOOLEAN);

/**
 * GtkWidget::child-notify:
 * @widget: the object which received the signal.
 * @pspec: the #GParamSpec of the changed child property.
 *
 * The ::child-notify signal is emitted for each child property that has 
 * changed on an object. The signal's detail holds the property name. 
 */
  widget_signals[CHILD_NOTIFY] =
    g_signal_new (I_("child_notify"),
		   G_TYPE_FROM_CLASS (gobject_class),
		   G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_DETAILED | G_SIGNAL_NO_HOOKS,
		   G_STRUCT_OFFSET (GtkWidgetClass, child_notify),
		   NULL, NULL,
		   g_cclosure_marshal_VOID__PARAM,
		   G_TYPE_NONE, 1,
		   G_TYPE_PARAM);
  widget_signals[MNEMONIC_ACTIVATE] =
    g_signal_new (I_("mnemonic_activate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, mnemonic_activate),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOOLEAN,
		  G_TYPE_BOOLEAN, 1,
		  G_TYPE_BOOLEAN);
  widget_signals[GRAB_FOCUS] =
    g_signal_new (I_("grab_focus"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkWidgetClass, grab_focus),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  widget_signals[FOCUS] =
    g_signal_new (I_("focus"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, focus),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__ENUM,
		  G_TYPE_BOOLEAN, 1,
		  GTK_TYPE_DIRECTION_TYPE);
  widget_signals[EVENT] =
    g_signal_new (I_("event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[EVENT_AFTER] =
    g_signal_new (I_("event_after"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  0,
		  0,
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[BUTTON_PRESS_EVENT] =
    g_signal_new (I_("button_press_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, button_press_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[BUTTON_RELEASE_EVENT] =
    g_signal_new (I_("button_release_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, button_release_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[SCROLL_EVENT] =
    g_signal_new (I_("scroll_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, scroll_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[MOTION_NOTIFY_EVENT] =
    g_signal_new (I_("motion_notify_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, motion_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[COMPOSITED_CHANGED] =
    g_signal_new (I_("composited_changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkWidgetClass, composited_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

/**
 * GtkWidget::delete-event:
 * @widget: the object which received the signal.
 * @event: the event which triggered this signal
 *
 * The ::delete-event signal is emitted if a user requests that
 * a toplevel window is closed. The default handler for this signal
 * destroys the window. Connecting gtk_widget_hide_on_delete() to
 * this signal will cause the window to be hidden instead, so that
 * it can later be shown again without reconstructing it.
 *
 * Returns: %TRUE to stop other handlers from being invoked for the event. 
 *   %FALSE to propagate the event further.
 */
  widget_signals[DELETE_EVENT] =
    g_signal_new (I_("delete_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, delete_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

/**
 * GtkWidget::destroy-event:
 * @widget: the object which received the signal.
 * @event: the event which triggered this signal
 *
 * The ::destroy-event signal is emitted when a #GdkWindow is destroyed.
 * You rarely get this signal, because most widgets disconnect themselves 
 * from their window before they destroy it, so no widget owns the 
 * window at destroy time.
 * 
 * Returns: %TRUE to stop other handlers from being invoked for the event. 
 *   %FALSE to propagate the event further.
 */
  widget_signals[DESTROY_EVENT] =
    g_signal_new (I_("destroy_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, destroy_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[EXPOSE_EVENT] =
    g_signal_new (I_("expose_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, expose_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[KEY_PRESS_EVENT] =
    g_signal_new (I_("key_press_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, key_press_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[KEY_RELEASE_EVENT] =
    g_signal_new (I_("key_release_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, key_release_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[ENTER_NOTIFY_EVENT] =
    g_signal_new (I_("enter_notify_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, enter_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[LEAVE_NOTIFY_EVENT] =
    g_signal_new (I_("leave_notify_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, leave_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[CONFIGURE_EVENT] =
    g_signal_new (I_("configure_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, configure_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[FOCUS_IN_EVENT] =
    g_signal_new (I_("focus_in_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, focus_in_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[FOCUS_OUT_EVENT] =
    g_signal_new (I_("focus_out_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, focus_out_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[MAP_EVENT] =
    g_signal_new (I_("map_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, map_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[UNMAP_EVENT] =
    g_signal_new (I_("unmap_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, unmap_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[PROPERTY_NOTIFY_EVENT] =
    g_signal_new (I_("property_notify_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, property_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[SELECTION_CLEAR_EVENT] =
    g_signal_new (I_("selection_clear_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_clear_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[SELECTION_REQUEST_EVENT] =
    g_signal_new (I_("selection_request_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_request_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[SELECTION_NOTIFY_EVENT] =
    g_signal_new (I_("selection_notify_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[SELECTION_RECEIVED] =
    g_signal_new (I_("selection_received"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_received),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED_UINT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		  G_TYPE_UINT);
  widget_signals[SELECTION_GET] =
    g_signal_new (I_("selection_get"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_get),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED_UINT_UINT,
		  G_TYPE_NONE, 3,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		  G_TYPE_UINT,
		  G_TYPE_UINT);
  widget_signals[PROXIMITY_IN_EVENT] =
    g_signal_new (I_("proximity_in_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, proximity_in_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[PROXIMITY_OUT_EVENT] =
    g_signal_new (I_("proximity_out_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, proximity_out_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::drag-leave:
   * @widget: the object which received the signal.
   * @drag_context: the drag context
   * @time: the timestamp of the motion event
   *
   * The ::drag-leave signal is emitted on the drop site when the cursor 
   * leaves the widget. A typical reason to connect to this signal is to 
   * undo things done in ::drag-motion, e.g. undo highlighting with 
   * gtk_drag_unhighlight()
   */
  widget_signals[DRAG_LEAVE] =
    g_signal_new (I_("drag_leave"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_leave),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_UINT,
		  G_TYPE_NONE, 2,
		  GDK_TYPE_DRAG_CONTEXT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::drag-begin:
   * @widget: the object which received the signal.
   * @drag_context: the drag context
   *
   * The ::drag-begin signal is emitted on the drag source when a drag is started. 
   * A typical reason to connect to this signal is to set up a custom drag icon with
   * gtk_drag_source_set_icon().
   */
  widget_signals[DRAG_BEGIN] =
    g_signal_new (I_("drag_begin"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_begin),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DRAG_CONTEXT);

  /**
   * GtkWidget::drag-end:
   * @widget: the object which received the signal.
   * @drag_context: the drag context
   *
   * The ::drag-end signal is emitted on the drag source when a drag is finished. 
   * A typical reason to connect to this signal is to undo things done in ::drag-begin.
   */
  widget_signals[DRAG_END] =
    g_signal_new (I_("drag_end"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_end),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DRAG_CONTEXT);

  /**
   * GtkWidget::drag-data-delete:
   * @widget: the object which received the signal.
   * @drag_context: the drag context
   *
   * The ::drag-data-delete signal is emitted on the drag source when a drag 
   * with the action %GDK_ACTION_MOVE is successfully completed. The signal 
   * handler is responsible for deleting the data that has been dropped. What 
   * "delete" means, depends on the context of the drag operation. 
   */
  widget_signals[DRAG_DATA_DELETE] =
    g_signal_new (I_("drag_data_delete"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_data_delete),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DRAG_CONTEXT);

  /**
   * GtkWidget::drag-motion:
   * @widget: the object which received the signal.
   * @drag_context: the drag context
   * @x: the x coordinate of the current cursor position
   * @y: the y coordinate of the current cursor position
   * @time: the timestamp of the motion event
   * @returns: whether the cursor position is in a drop zone
   *
   * The ::drag-motion signal is emitted on the drop site when the user 
   * moves the cursor over the widget during a drag. The signal handler 
   * must determine whether the cursor position is in a drop zone or not. 
   * If it is not in a drop zone, it returns %FALSE and no further processing 
   * is necessary. Otherwise, the handler returns %TRUE. In this case, the 
   * handler is responsible for providing the necessary information for 
   * displaying feedback to the user, by calling gdk_drag_status(). If the 
   * decision whether the drop will be accepted or rejected can't be made
   * based solely on the cursor position and the type of the data, the handler 
   * may inspect the dragged data by calling gtk_drag_get_data() and defer the 
   * gdk_drag_status() call to the ::drag-data-received handler. 
   *
   * Note that there is no ::drag-enter signal. The drag receiver has to keep 
   * track of whether he has received any ::drag-motion signals since the last 
   * ::drag-leave and if not, treat the ::drag-motion signal as an "enter" signal. 
   * Upon an "enter", the handler will typically highlight the drop site with 
   * gtk_drag_highlight().
   *
   * <informalexample><programlisting> 
   * static void
   * drag_motion (GtkWidget *widget,
   *       	  GdkDragContext *context,
   *              gint x,
   *              gint y,
   *              guint time)
   * {
   *   GdkAtom target;
   *  
   *   PrivateData *private_data = GET_PRIVATE_DATA (widget);
   *  
   *   if (!private_data->drag_highlight) 
   *    {
   *      private_data->drag_highlight = 1;
   *      gtk_drag_highlight (widget);
   *    }
   *  
   *   target = gtk_drag_dest_find_target (widget, context, NULL);
   *   if (target == GDK_NONE)
   *     gdk_drag_status (context, 0, time);
   *   else 
   *    {
   *      private_data->pending_status = context->suggested_action;
   *      gtk_drag_get_data (widget, context, target, time);
   *    }
   *  
   *   return TRUE;
   * }
   *   
   * static void
   * drag_data_received (GtkWidget        *widget,
   *                     GdkDragContext   *context,
   *                     gint              x,
   *                     gint              y,
   *                     GtkSelectionData *selection_data,
   *                     guint             info,
   *                     guint             time)
   * {
   *   PrivateData *private_data = GET_PRIVATE_DATA (widget);
   *   
   *   if (private_data->suggested_action) 
   *    {
   *      private_data->suggested_action = 0;
   *      
   *     /<!-- -->* We are getting this data due to a request in drag_motion,
   *      * rather than due to a request in drag_drop, so we are just
   *      * supposed to call gdk_drag_status(<!-- -->), not actually paste in 
   *      * the data.
   *      *<!-- -->/
   *      str = gtk_selection_data_get_text (selection_data);
   *      if (!data_is_acceptable (str)) 
   *        gdk_drag_status (context, 0, time);
   *      else
   *        gdk_drag_status (context, private_data->suggested_action, time);
   *    }
   *   else
   *    {
   *      /<!-- -->* accept the drop *<!-- -->/
   *    }
   * }
   * </programlisting></informalexample>
   */
  widget_signals[DRAG_MOTION] =
    g_signal_new (I_("drag_motion"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_motion),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT_INT_INT_UINT,
		  G_TYPE_BOOLEAN, 4,
		  GDK_TYPE_DRAG_CONTEXT,
		  G_TYPE_INT,
		  G_TYPE_INT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::drag-drop:
   * @widget: the object which received the signal.
   * @drag_context: the drag context
   * @x: the x coordinate of the current cursor position
   * @y: the y coordinate of the current cursor position
   * @time: the timestamp of the motion event
   * @returns: whether the cursor position is in a drop zone
   *
   * The ::drag-drop signal is emitted on the drop site when the user drops the 
   * data onto the widget. The signal handler must determine whether the cursor 
   * position is in a drop zone or not. If it is not in a drop zone, it returns 
   * %FALSE and no further processing is necessary. Otherwise, the handler returns 
   * %TRUE. In this case, the handler must ensure that gtk_drag_finish() is called 
   * to let the source know that the drop is done. The call to gtk_drag_finish() 
   * can be done either directly or in a ::drag-data-received handler which gets 
   * triggered by calling gtk_drop_get_data() to receive the data for one or more 
   * of the supported targets.
   */
  widget_signals[DRAG_DROP] =
    g_signal_new (I_("drag_drop"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_drop),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT_INT_INT_UINT,
		  G_TYPE_BOOLEAN, 4,
		  GDK_TYPE_DRAG_CONTEXT,
		  G_TYPE_INT,
		  G_TYPE_INT,
		  G_TYPE_UINT);

  /** 
   * GtkWidget::drag-data-get:
   * @widget: the object which received the signal.
   * @drag_context: the drag context
   * @data: the #GtkSelectionData to be filled with the dragged data
   * @info: the info that has been registered with the target in the #GtkTargetList.
   * @time: the timestamp at which the data was requested
   *
   * The ::drag-data-get signal is emitted on the drag source when the drop site 
   * requests the data which is dragged. It is the responsibility of the signal 
   * handler to fill @data with the data in the format which is indicated by @info. 
   * See gtk_selection_data_set() and gtk_selection_data_set_text().
   */
  widget_signals[DRAG_DATA_GET] =
    g_signal_new (I_("drag_data_get"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_data_get),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_BOXED_UINT_UINT,
		  G_TYPE_NONE, 4,
		  GDK_TYPE_DRAG_CONTEXT,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		  G_TYPE_UINT,
		  G_TYPE_UINT);

/**
 * GtkWidget::drag-data-received:
 * @widget: the object which received the signal.
 * @drag_context: the drag context
 * @x: where the drop happened
 * @y: where the drop happened
 * @data: the received data
 * @info: the info that has been registered with the target in the #GtkTargetList.
 * @time: the timestamp at which the data was received
 *
 * The ::drag-data-received signal is emitted on the drop site when the dragged 
 * data has been received. If the data was received in order to determine whether 
 * the drop will be accepted, the handler is expected to call gdk_drag_status() 
 * and <emphasis>not</emphasis> finish the drag. If the data was received in 
 * response to a ::drag-drop signal (and this is the last target to be received), 
 * the handler for this signal is expected to process the received data and then 
 * call gtk_drag_finish(), setting the @success parameter depending on whether 
 * the data was processed successfully. 
 * 
 * The handler may inspect and modify @drag_context->action before calling 
 * gtk_drag_finish(), e.g. to implement %GDK_ACTION_ASK as shown in the following 
 * example:
 * <informalexample><programlisting>
 * void  
 * drag_data_received (GtkWidget          *widget,
 *                     GdkDragContext     *drag_context,
 *                     gint                x,
 *                     gint                y,
 *                     GtkSelectionData   *data,
 *                     guint               info,
 *                     guint               time)
 * {
 *   if ((data->length >= 0) && (data->format == 8))
 *     {
 *       if (drag_context->action == GDK_ACTION_ASK) 
 *         {
 *           GtkWidget *dialog;
 *           gint response;
 *           
 *           dialog = gtk_message_dialog_new (NULL,
 *                                            GTK_DIALOG_MODAL | 
 *                                            GTK_DIALOG_DESTROY_WITH_PARENT,
 *                                            GTK_MESSAGE_INFO,
 *                                            GTK_BUTTONS_YES_NO,
 *                                            "Move the data ?\n");
 *           response = gtk_dialog_run (GTK_DIALOG (dialog));
 *           gtk_widget_destroy (dialog);
 *             
 *           if (response == GTK_RESPONSE_YES)
 *             drag_context->action = GDK_ACTION_MOVE;
 *           else
 *             drag_context->action = GDK_ACTION_COPY;
 *          }
 *          
 *       gtk_drag_finish (drag_context, TRUE, FALSE, time);
 *       return;
 *     }
 *       
 *    gtk_drag_finish (drag_context, FALSE, FALSE, time);
 *  }
 * </programlisting></informalexample>
 */
  widget_signals[DRAG_DATA_RECEIVED] =
    g_signal_new (I_("drag_data_received"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_data_received),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_INT_INT_BOXED_UINT_UINT,
		  G_TYPE_NONE, 6,
		  GDK_TYPE_DRAG_CONTEXT,
		  G_TYPE_INT,
		  G_TYPE_INT,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		  G_TYPE_UINT,
		  G_TYPE_UINT);
  widget_signals[VISIBILITY_NOTIFY_EVENT] =
    g_signal_new (I_("visibility_notify_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, visibility_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[CLIENT_EVENT] =
    g_signal_new (I_("client_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, client_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[NO_EXPOSE_EVENT] =
    g_signal_new (I_("no_expose_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, no_expose_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  widget_signals[WINDOW_STATE_EVENT] =
    g_signal_new (I_("window_state_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, window_state_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * GtkWidget::grab-broken-event:
   * @widget: the object which received the signal
   * @event: the #GdkEventGrabBroken event
   *
   * Emitted when a pointer or keyboard grab on a window belonging 
   * to @widget gets broken. 
   * 
   * On X11, this happens when the grab window becomes unviewable 
   * (i.e. it or one of its ancestors is unmapped), or if the same 
   * application grabs the pointer or keyboard again.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event. 
   *   %FALSE to propagate the event further.
   *
   * Since: 2.8
   */
  widget_signals[GRAB_BROKEN] =
    g_signal_new (I_("grab_broken_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, grab_broken_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
/**
 * GtkWidget::popup-menu
 * @widget: the object which received the signal
 * @returns: TRUE if a menu was activated
 *
 * This signal gets emitted whenever a widget should pop up a context-sensitive
 * menu.  This usually happens through the standard key binding mechanism; by
 * pressing a certain key while a widget is focused, the user can cause the
 * widget to pop up a menu.  For example, the #GtkEntry widget creates a menu
 * with clipboard commands.  See <xref linkend="checklist-popup-menu"/> for an
 * example of how to use this signal.
 */
  widget_signals[POPUP_MENU] =
    g_signal_new (I_("popup_menu"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkWidgetClass, popup_menu),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);
  widget_signals[SHOW_HELP] =
    g_signal_new (I_("show_help"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkWidgetClass, show_help),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__ENUM,
		  G_TYPE_BOOLEAN, 1,
		  GTK_TYPE_WIDGET_HELP_TYPE);
  widget_signals[ACCEL_CLOSURES_CHANGED] =
    g_signal_new (I_("accel_closures_changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  0,
		  0,
		  NULL, NULL,
		  _gtk_marshal_NONE__NONE,
		  G_TYPE_NONE, 0);
  widget_signals[SCREEN_CHANGED] =
    g_signal_new (I_("screen_changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, screen_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_SCREEN);
/**
 * GtkWidget::can-activate-accel:
 * @widget: the object which received the signal
 * @signal_id: the ID of a signal installed on @widget
 * @returns: %TRUE if the signal can be activated.
 *
 * Determines whether an accelerator that activates the signal
 * identified by @signal_id can currently be activated.
 * This signal is present to allow applications and derived
 * widgets to override the default #GtkWidget handling
 * for determining whether an accelerator can be activated.
 */
  widget_signals[CAN_ACTIVATE_ACCEL] =
     g_signal_new (I_("can_activate_accel"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, can_activate_accel),
                  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__UINT,
                  G_TYPE_BOOLEAN, 1, G_TYPE_UINT);

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_F10, GDK_SHIFT_MASK,
                                "popup_menu", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Menu, 0,
                                "popup_menu", 0);  

  gtk_binding_entry_add_signal (binding_set, GDK_F1, GDK_CONTROL_MASK,
                                "show_help", 1,
                                GTK_TYPE_WIDGET_HELP_TYPE,
                                GTK_WIDGET_HELP_TOOLTIP);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_F1, GDK_CONTROL_MASK,
                                "show_help", 1,
                                GTK_TYPE_WIDGET_HELP_TYPE,
                                GTK_WIDGET_HELP_TOOLTIP);
  gtk_binding_entry_add_signal (binding_set, GDK_F1, GDK_SHIFT_MASK,
                                "show_help", 1,
                                GTK_TYPE_WIDGET_HELP_TYPE,
                                GTK_WIDGET_HELP_WHATS_THIS);  
  gtk_binding_entry_add_signal (binding_set, GDK_KP_F1, GDK_SHIFT_MASK,
                                "show_help", 1,
                                GTK_TYPE_WIDGET_HELP_TYPE,
                                GTK_WIDGET_HELP_WHATS_THIS);

  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boolean ("interior-focus",
								 P_("Interior Focus"),
								 P_("Whether to draw the focus indicator inside widgets"),
								 TRUE,
								 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (klass,
					   g_param_spec_int ("focus-line-width",
							     P_("Focus linewidth"),
							     P_("Width, in pixels, of the focus indicator line"),
							     0, G_MAXINT, 1,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (klass,
					   g_param_spec_string ("focus-line-pattern",
								P_("Focus line dash pattern"),
								P_("Dash pattern used to draw the focus indicator"),
								"\1\1",
								GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_int ("focus-padding",
							     P_("Focus padding"),
							     P_("Width, in pixels, between focus indicator and the widget 'box'"),
							     0, G_MAXINT, 1,
							     GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boxed ("cursor-color",
							       P_("Cursor color"),
							       P_("Color with which to draw insertion cursor"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boxed ("secondary-cursor-color",
							       P_("Secondary cursor color"),
							       P_("Color with which to draw the secondary insertion cursor when editing mixed right-to-left and left-to-right text"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_float ("cursor-aspect-ratio",
							       P_("Cursor line aspect ratio"),
							       P_("Aspect ratio with which to draw insertion cursor"),
							       0.0, 1.0, 0.04,
							       GTK_PARAM_READABLE));

/**
 * GtkWidget:draw-border:
 *
 * The "draw-border" property defines the size of areas outside 
 * the widget's allocation to draw.
 *
 * Since: 2.8
 */
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boxed ("draw-border",
							       P_("Draw Border"),
							       P_("Size of areas outside the widget's allocation to draw"),
							       GTK_TYPE_BORDER,
							       GTK_PARAM_READABLE));

/**
 * GtkWidget:link-color:
 *
 * The "link-color" property defines the color of unvisited links.
 *
 * Since: 2.10
 */
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boxed ("link-color",
							       P_("Unvisited Link Color"),
							       P_("Color of unvisited links"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE));

/**
 * GtkWidget:visited-link-color:
 *
 * The "visited-link-color" property defines the color of visited links.
 *
 * Since: 2.10
 */
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boxed ("visited-link-color",
							       P_("Visited Link Color"),
							       P_("Color of visited links"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE));

/**
 * GtkWidget:wide-separators:
 *
 * The "wide-separators" property defines whether separators have 
 * configurable width and should be drawn using a box instead of a line.
 *
 * Since: 2.10
 */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_boolean ("wide-separators",
                                                                 P_("Wide Separators"),
                                                                 P_("Whether separators have configurable width and should be drawn using a box instead of a line"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

/**
 * GtkWidget:separator-width:
 *
 * The "separator-width" property defines the width of separators.
 * This property only takes effect if "wide-separators" is %TRUE.
 *
 * Since: 2.10
 */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("separator-width",
                                                             P_("Separator Width"),
                                                             P_("The width of separators if wide-separators is TRUE"),
                                                             0, G_MAXINT, 0,
                                                             GTK_PARAM_READABLE));

/**
 * GtkWidget:separator-height:
 *
 * The "separator-height" property defines the height of separators.
 * This property only takes effect if "wide-separators" is %TRUE.
 *
 * Since: 2.10
 */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("separator-height",
                                                             P_("Separator Height"),
                                                             P_("The height of separators if \"wide-separators\" is TRUE"),
                                                             0, G_MAXINT, 0,
                                                             GTK_PARAM_READABLE));

/**
 * GtkWidget:scroll-arrow-hlength:
 *
 * The "scroll-arrow-hlength" property defines the length of 
 * horizontal scroll arrows.
 *
 * Since: 2.10
 */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("scroll-arrow-hlength",
                                                             P_("Horizontal Scroll Arrow Length"),
                                                             P_("The length of horizontal scroll arrows"),
                                                             1, G_MAXINT, 16,
                                                             GTK_PARAM_READABLE));

/**
 * GtkWidget:scroll-arrow-vlength:
 *
 * The "scroll-arrow-vlength" property defines the length of 
 * vertical scroll arrows.
 *
 * Since: 2.10
 */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("scroll-arrow-vlength",
                                                             P_("Vertical Scroll Arrow Length"),
                                                             P_("The length of vertical scroll arrows"),
                                                             1, G_MAXINT, 16,
                                                             GTK_PARAM_READABLE));
}

static void
gtk_widget_base_class_finalize (GtkWidgetClass *klass)
{
  GList *list, *node;

  list = g_param_spec_pool_list_owned (style_property_spec_pool, G_OBJECT_CLASS_TYPE (klass));
  for (node = list; node; node = node->next)
    {
      GParamSpec *pspec = node->data;

      g_param_spec_pool_remove (style_property_spec_pool, pspec);
      g_param_spec_unref (pspec);
    }
  g_list_free (list);
}

static void
gtk_widget_set_property (GObject         *object,
			 guint            prop_id,
			 const GValue    *value,
			 GParamSpec      *pspec)
{
  GtkWidget *widget = GTK_WIDGET (object);

  switch (prop_id)
    {
      guint32 saved_flags;
      
    case PROP_NAME:
      gtk_widget_set_name (widget, g_value_get_string (value));
      break;
    case PROP_PARENT:
      gtk_container_add (GTK_CONTAINER (g_value_get_object (value)), widget);
      break;
    case PROP_WIDTH_REQUEST:
      gtk_widget_set_usize_internal (widget, g_value_get_int (value), -2);
      break;
    case PROP_HEIGHT_REQUEST:
      gtk_widget_set_usize_internal (widget, -2, g_value_get_int (value));
      break;
    case PROP_VISIBLE:
      if (g_value_get_boolean (value))
	gtk_widget_show (widget);
      else
	gtk_widget_hide (widget);
      break;
    case PROP_SENSITIVE:
      gtk_widget_set_sensitive (widget, g_value_get_boolean (value));
      break;
    case PROP_APP_PAINTABLE:
      gtk_widget_set_app_paintable (widget, g_value_get_boolean (value));
      break;
    case PROP_CAN_FOCUS:
      saved_flags = GTK_WIDGET_FLAGS (widget);
      if (g_value_get_boolean (value))
	GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
      else
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_FOCUS);
      if (saved_flags != GTK_WIDGET_FLAGS (widget))
	gtk_widget_queue_resize (widget);
      break;
    case PROP_HAS_FOCUS:
      if (g_value_get_boolean (value))
	gtk_widget_grab_focus (widget);
      break;
    case PROP_IS_FOCUS:
      if (g_value_get_boolean (value))
	gtk_widget_grab_focus (widget);
      break;
    case PROP_CAN_DEFAULT:
      saved_flags = GTK_WIDGET_FLAGS (widget);
      if (g_value_get_boolean (value))
	GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
      else
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_DEFAULT);
      if (saved_flags != GTK_WIDGET_FLAGS (widget))
	gtk_widget_queue_resize (widget);
      break;
    case PROP_HAS_DEFAULT:
      if (g_value_get_boolean (value))
	gtk_widget_grab_default (widget);
      break;
    case PROP_RECEIVES_DEFAULT:
      if (g_value_get_boolean (value))
	GTK_WIDGET_SET_FLAGS (widget, GTK_RECEIVES_DEFAULT);
      else
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_RECEIVES_DEFAULT);
      break;
    case PROP_STYLE:
      gtk_widget_set_style (widget, g_value_get_object (value));
      break;
    case PROP_EVENTS:
      if (!GTK_WIDGET_REALIZED (widget) && !GTK_WIDGET_NO_WINDOW (widget))
	gtk_widget_set_events (widget, g_value_get_flags (value));
      break;
    case PROP_EXTENSION_EVENTS:
      gtk_widget_set_extension_events (widget, g_value_get_enum (value));
      break;
    case PROP_NO_SHOW_ALL:
      gtk_widget_set_no_show_all (widget, g_value_get_boolean (value));
      break;
    default:
      break;
    }
}

static void
gtk_widget_get_property (GObject         *object,
			 guint            prop_id,
			 GValue          *value,
			 GParamSpec      *pspec)
{
  GtkWidget *widget = GTK_WIDGET (object);
  
  switch (prop_id)
    {
      gint *eventp;
      GdkExtensionMode *modep;

    case PROP_NAME:
      if (widget->name)
	g_value_set_string (value, widget->name);
      else
	g_value_set_string (value, "");
      break;
    case PROP_PARENT:
      if (widget->parent)
	g_value_set_object (value, widget->parent);
      else
	g_value_set_object (value, NULL);
      break;
    case PROP_WIDTH_REQUEST:
      {
        int w;
        gtk_widget_get_size_request (widget, &w, NULL);
        g_value_set_int (value, w);
      }
      break;
    case PROP_HEIGHT_REQUEST:
      {
        int h;
        gtk_widget_get_size_request (widget, NULL, &h);
        g_value_set_int (value, h);
      }
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, (GTK_WIDGET_VISIBLE (widget) != FALSE));
      break;
    case PROP_SENSITIVE:
      g_value_set_boolean (value, (GTK_WIDGET_SENSITIVE (widget) != FALSE));
      break;
    case PROP_APP_PAINTABLE:
      g_value_set_boolean (value, (GTK_WIDGET_APP_PAINTABLE (widget) != FALSE));
      break;
    case PROP_CAN_FOCUS:
      g_value_set_boolean (value, (GTK_WIDGET_CAN_FOCUS (widget) != FALSE));
      break;
    case PROP_HAS_FOCUS:
      g_value_set_boolean (value, (GTK_WIDGET_HAS_FOCUS (widget) != FALSE));
      break;
    case PROP_IS_FOCUS:
      g_value_set_boolean (value, (gtk_widget_is_focus (widget)));
      break;
    case PROP_CAN_DEFAULT:
      g_value_set_boolean (value, (GTK_WIDGET_CAN_DEFAULT (widget) != FALSE));
      break;
    case PROP_HAS_DEFAULT:
      g_value_set_boolean (value, (GTK_WIDGET_HAS_DEFAULT (widget) != FALSE));
      break;
    case PROP_RECEIVES_DEFAULT:
      g_value_set_boolean (value, (GTK_WIDGET_RECEIVES_DEFAULT (widget) != FALSE));
      break;
    case PROP_COMPOSITE_CHILD:
      g_value_set_boolean (value, (GTK_WIDGET_COMPOSITE_CHILD (widget) != FALSE));
      break;
    case PROP_STYLE:
      g_value_set_object (value, gtk_widget_get_style (widget));
      break;
    case PROP_EVENTS:
      eventp = g_object_get_qdata (G_OBJECT (widget), quark_event_mask);
      if (!eventp)
	g_value_set_flags (value, 0);
      else
	g_value_set_flags (value, *eventp);
      break;
    case PROP_EXTENSION_EVENTS:
      modep = g_object_get_qdata (G_OBJECT (widget), quark_extension_event_mode);
      if (!modep)
 	g_value_set_enum (value, 0);
      else
 	g_value_set_enum (value, (GdkExtensionMode) *modep);
      break;
    case PROP_NO_SHOW_ALL:
      g_value_set_boolean (value, gtk_widget_get_no_show_all (widget));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_widget_init (GtkWidget *widget)
{
  GTK_PRIVATE_FLAGS (widget) = PRIVATE_GTK_CHILD_VISIBLE;
  widget->state = GTK_STATE_NORMAL;
  widget->saved_state = GTK_STATE_NORMAL;
  widget->name = NULL;
  widget->requisition.width = 0;
  widget->requisition.height = 0;
  widget->allocation.x = -1;
  widget->allocation.y = -1;
  widget->allocation.width = 1;
  widget->allocation.height = 1;
  widget->window = NULL;
  widget->parent = NULL;

  GTK_WIDGET_SET_FLAGS (widget,
			GTK_SENSITIVE |
			GTK_PARENT_SENSITIVE |
			(composite_child_stack ? GTK_COMPOSITE_CHILD : 0) |
			GTK_DOUBLE_BUFFERED);

  GTK_PRIVATE_SET_FLAG (widget, GTK_REDRAW_ON_ALLOC);
  GTK_PRIVATE_SET_FLAG (widget, GTK_REQUEST_NEEDED);
  GTK_PRIVATE_SET_FLAG (widget, GTK_ALLOC_NEEDED);

  widget->style = gtk_widget_get_default_style ();
  g_object_ref (widget->style);
}


static void
gtk_widget_dispatch_child_properties_changed (GtkWidget   *widget,
					      guint        n_pspecs,
					      GParamSpec **pspecs)
{
  GtkWidget *container = widget->parent;
  guint i;

  for (i = 0; widget->parent == container && i < n_pspecs; i++)
    g_signal_emit (widget, widget_signals[CHILD_NOTIFY], g_quark_from_string (pspecs[i]->name), pspecs[i]);
}

/**
 * gtk_widget_freeze_child_notify:
 * @widget: a #GtkWidget
 * 
 * Stops emission of "child-notify" signals on @widget. The signals are
 * queued until gtk_widget_thaw_child_notify() is called on @widget. 
 *
 * This is the analogue of g_object_freeze_notify() for child properties.
 **/
void
gtk_widget_freeze_child_notify (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!G_OBJECT (widget)->ref_count)
    return;

  g_object_ref (widget);
  g_object_notify_queue_freeze (G_OBJECT (widget), _gtk_widget_child_property_notify_context);
  g_object_unref (widget);
}

/**
 * gtk_widget_child_notify:
 * @widget: a #GtkWidget
 * @child_property: the name of a child property installed on the 
 *                  class of @widget<!-- -->'s parent.
 * 
 * Emits a "child-notify" signal for the 
 * <link linkend="child-properties">child property</link> @child_property 
 * on @widget.
 *
 * This is the analogue of g_object_notify() for child properties.
 **/
void
gtk_widget_child_notify (GtkWidget    *widget,
			 const gchar  *child_property)
{
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (child_property != NULL);
  if (!G_OBJECT (widget)->ref_count || !widget->parent)
    return;

  g_object_ref (widget);
  pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool,
				    child_property,
				    G_OBJECT_TYPE (widget->parent),
				    TRUE);
  if (!pspec)
    g_warning ("%s: container class `%s' has no child property named `%s'",
	       G_STRLOC,
	       G_OBJECT_TYPE_NAME (widget->parent),
	       child_property);
  else
    {
      GObjectNotifyQueue *nqueue = g_object_notify_queue_freeze (G_OBJECT (widget), _gtk_widget_child_property_notify_context);

      g_object_notify_queue_add (G_OBJECT (widget), nqueue, pspec);
      g_object_notify_queue_thaw (G_OBJECT (widget), nqueue);
    }
  g_object_unref (widget);
}

/**
 * gtk_widget_thaw_child_notify:
 * @widget: a #GtkWidget
 *
 * Reverts the effect of a previous call to gtk_widget_freeze_child_notify().
 * This causes all queued "child-notify" signals on @widget to be emitted.
 */ 
void
gtk_widget_thaw_child_notify (GtkWidget *widget)
{
  GObjectNotifyQueue *nqueue;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!G_OBJECT (widget)->ref_count)
    return;

  g_object_ref (widget);
  nqueue = g_object_notify_queue_from_object (G_OBJECT (widget), _gtk_widget_child_property_notify_context);
  if (!nqueue || !nqueue->freeze_count)
    g_warning (G_STRLOC ": child-property-changed notification for %s(%p) is not frozen",
	       G_OBJECT_TYPE_NAME (widget), widget);
  else
    g_object_notify_queue_thaw (G_OBJECT (widget), nqueue);
  g_object_unref (widget);
}


/**
 * gtk_widget_new:
 * @type: type ID of the widget to create
 * @first_property_name: name of first property to set
 * @Varargs: value of first property, followed by more properties, %NULL-terminated
 * 
 * This is a convenience function for creating a widget and setting
 * its properties in one go. For example you might write:
 * <literal>gtk_widget_new (GTK_TYPE_LABEL, "label", "Hello World", "xalign",
 * 0.0, NULL)</literal> to create a left-aligned label. Equivalent to
 * g_object_new(), but returns a widget so you don't have to
 * cast the object yourself.
 * 
 * Return value: a new #GtkWidget of type @widget_type
 **/
GtkWidget*
gtk_widget_new (GType        type,
		const gchar *first_property_name,
		...)
{
  GtkWidget *widget;
  va_list var_args;
  
  g_return_val_if_fail (g_type_is_a (type, GTK_TYPE_WIDGET), NULL);
  
  va_start (var_args, first_property_name);
  widget = (GtkWidget *)g_object_new_valist (type, first_property_name, var_args);
  va_end (var_args);

  return widget;
}

/**
 * gtk_widget_set:
 * @widget: a #GtkWidget
 * @first_property_name: name of first property to set
 * @Varargs: value of first property, followed by more properties, %NULL-terminated
 * 
 * Like g_object_set() - there's no reason to use this instead of
 * g_object_set().
 **/
void
gtk_widget_set (GtkWidget   *widget,
		const gchar *first_property_name,
		...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  va_start (var_args, first_property_name);
  g_object_set_valist (G_OBJECT (widget), first_property_name, var_args);
  va_end (var_args);
}

static inline void	   
gtk_widget_queue_draw_child (GtkWidget *widget)
{
  GtkWidget *parent;

  parent = widget->parent;
  if (parent && GTK_WIDGET_DRAWABLE (parent))
    gtk_widget_queue_draw_area (parent,
				widget->allocation.x,
				widget->allocation.y,
				widget->allocation.width,
				widget->allocation.height);
}

/**
 * gtk_widget_unparent:
 * @widget: a #GtkWidget
 * 
 * This function is only for use in widget implementations.
 * Should be called by implementations of the remove method
 * on #GtkContainer, to dissociate a child from the container.
 **/
void
gtk_widget_unparent (GtkWidget *widget)
{
  GObjectNotifyQueue *nqueue;
  GtkWidget *toplevel;
  GtkWidget *old_parent;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  if (widget->parent == NULL)
    return;
  
  /* keep this function in sync with gtk_menu_detach()
   */

  g_object_freeze_notify (G_OBJECT (widget));
  nqueue = g_object_notify_queue_freeze (G_OBJECT (widget), _gtk_widget_child_property_notify_context);

  toplevel = gtk_widget_get_toplevel (widget);
  if (GTK_WIDGET_TOPLEVEL (toplevel))
    _gtk_window_unset_focus_and_default (GTK_WINDOW (toplevel), widget);

  if (GTK_CONTAINER (widget->parent)->focus_child == widget)
    gtk_container_set_focus_child (GTK_CONTAINER (widget->parent), NULL);

  /* If we are unanchoring the child, we save around the toplevel
   * to emit hierarchy changed
   */
  if (GTK_WIDGET_ANCHORED (widget->parent))
    g_object_ref (toplevel);
  else
    toplevel = NULL;

  gtk_widget_queue_draw_child (widget);

  /* Reset the width and height here, to force reallocation if we
   * get added back to a new parent. This won't work if our new
   * allocation is smaller than 1x1 and we actually want a size of 1x1...
   * (would 0x0 be OK here?)
   */
  widget->allocation.width = 1;
  widget->allocation.height = 1;
  
  if (GTK_WIDGET_REALIZED (widget)) 
    {
      if (GTK_WIDGET_IN_REPARENT (widget))
	gtk_widget_unmap (widget);
      else
	gtk_widget_unrealize (widget);
    }

  /* Removing a widget from a container restores the child visible
   * flag to the default state, so it doesn't affect the child
   * in the next parent.
   */
  GTK_PRIVATE_SET_FLAG (widget, GTK_CHILD_VISIBLE);
    
  old_parent = widget->parent;
  widget->parent = NULL;
  gtk_widget_set_parent_window (widget, NULL);
  g_signal_emit (widget, widget_signals[PARENT_SET], 0, old_parent);
  if (toplevel)
    {
      _gtk_widget_propagate_hierarchy_changed (widget, toplevel);
      g_object_unref (toplevel);
    }
      
  g_object_notify (G_OBJECT (widget), "parent");
  g_object_thaw_notify (G_OBJECT (widget));
  if (!widget->parent)
    g_object_notify_queue_clear (G_OBJECT (widget), nqueue);
  g_object_notify_queue_thaw (G_OBJECT (widget), nqueue);
  g_object_unref (widget);
}

/**
 * gtk_widget_destroy:
 * @widget: a #GtkWidget
 *
 * Destroys a widget. Equivalent to gtk_object_destroy(), except that
 * you don't have to cast the widget to #GtkObject. When a widget is
 * destroyed, it will break any references it holds to other objects.
 * If the widget is inside a container, the widget will be removed
 * from the container. If the widget is a toplevel (derived from
 * #GtkWindow), it will be removed from the list of toplevels, and the
 * reference GTK+ holds to it will be removed. Removing a
 * widget from its container or the list of toplevels results in the
 * widget being finalized, unless you've added additional references
 * to the widget with g_object_ref().
 *
 * In most cases, only toplevel widgets (windows) require explicit
 * destruction, because when you destroy a toplevel its children will
 * be destroyed as well.
 * 
 **/
void
gtk_widget_destroy (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_object_destroy ((GtkObject*) widget);
}

/**
 * gtk_widget_destroyed:
 * @widget: a #GtkWidget
 * @widget_pointer: address of a variable that contains @widget
 *
 * This function sets *@widget_pointer to %NULL if @widget_pointer !=
 * %NULL.  It's intended to be used as a callback connected to the
 * "destroy" signal of a widget. You connect gtk_widget_destroyed()
 * as a signal handler, and pass the address of your widget variable
 * as user data. Then when the widget is destroyed, the variable will
 * be set to %NULL. Useful for example to avoid multiple copies
 * of the same dialog.
 * 
 **/
void
gtk_widget_destroyed (GtkWidget      *widget,
		      GtkWidget      **widget_pointer)
{
  /* Don't make any assumptions about the
   *  value of widget!
   *  Even check widget_pointer.
   */
  if (widget_pointer)
    *widget_pointer = NULL;
}

/**
 * gtk_widget_show:
 * @widget: a #GtkWidget
 * 
 * Flags a widget to be displayed. Any widget that isn't shown will
 * not appear on the screen. If you want to show all the widgets in a
 * container, it's easier to call gtk_widget_show_all() on the
 * container, instead of individually showing the widgets.
 *
 * Remember that you have to show the containers containing a widget,
 * in addition to the widget itself, before it will appear onscreen.
 *
 * When a toplevel container is shown, it is immediately realized and
 * mapped; other shown widgets are realized and mapped when their
 * toplevel container is realized and mapped.
 * 
 **/
void
gtk_widget_show (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!GTK_WIDGET_VISIBLE (widget))
    {
      g_object_ref (widget);
      if (!GTK_WIDGET_TOPLEVEL (widget))
	gtk_widget_queue_resize (widget);
      g_signal_emit (widget, widget_signals[SHOW], 0);
      g_object_notify (G_OBJECT (widget), "visible");
      g_object_unref (widget);
    }
}

static void
gtk_widget_real_show (GtkWidget *widget)
{
  if (!GTK_WIDGET_VISIBLE (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);

      if (widget->parent &&
	  GTK_WIDGET_MAPPED (widget->parent) &&
	  GTK_WIDGET_CHILD_VISIBLE (widget) &&
	  !GTK_WIDGET_MAPPED (widget))
	gtk_widget_map (widget);
    }
}

static void
gtk_widget_show_map_callback (GtkWidget *widget, GdkEvent *event, gint *flag)
{
  *flag = TRUE;
  g_signal_handlers_disconnect_by_func (widget,
					gtk_widget_show_map_callback, 
					flag);
}

/**
 * gtk_widget_show_now:
 * @widget: a #GtkWidget
 * 
 * Shows a widget. If the widget is an unmapped toplevel widget
 * (i.e. a #GtkWindow that has not yet been shown), enter the main
 * loop and wait for the window to actually be mapped. Be careful;
 * because the main loop is running, anything can happen during
 * this function.
 **/
void
gtk_widget_show_now (GtkWidget *widget)
{
  gint flag = FALSE;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* make sure we will get event */
  if (!GTK_WIDGET_MAPPED (widget) &&
      GTK_WIDGET_TOPLEVEL (widget))
    {
      gtk_widget_show (widget);

      g_signal_connect (widget, "map_event",
			G_CALLBACK (gtk_widget_show_map_callback), 
			&flag);

      while (!flag)
	gtk_main_iteration ();
    }
  else
    gtk_widget_show (widget);
}

/**
 * gtk_widget_hide:
 * @widget: a #GtkWidget
 * 
 * Reverses the effects of gtk_widget_show(), causing the widget to be
 * hidden (invisible to the user).
 **/
void
gtk_widget_hide (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (GTK_WIDGET_VISIBLE (widget))
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
      
      g_object_ref (widget);
      if (toplevel != widget && GTK_WIDGET_TOPLEVEL (toplevel))
	_gtk_window_unset_focus_and_default (GTK_WINDOW (toplevel), widget);

      g_signal_emit (widget, widget_signals[HIDE], 0);
      if (!GTK_WIDGET_TOPLEVEL (widget))
	gtk_widget_queue_resize (widget);
      g_object_notify (G_OBJECT (widget), "visible");
      g_object_unref (widget);
    }
}

static void
gtk_widget_real_hide (GtkWidget *widget)
{
  if (GTK_WIDGET_VISIBLE (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_VISIBLE);
      
      if (GTK_WIDGET_MAPPED (widget))
	gtk_widget_unmap (widget);
    }
}

/**
 * gtk_widget_hide_on_delete:
 * @widget: a #GtkWidget
 * 
 * Utility function; intended to be connected to the "delete_event"
 * signal on a #GtkWindow. The function calls gtk_widget_hide() on its
 * argument, then returns %TRUE. If connected to "delete_event", the
 * result is that clicking the close button for a window (on the
 * window frame, top right corner usually) will hide but not destroy
 * the window. By default, GTK+ destroys windows when "delete_event"
 * is received.
 * 
 * Return value: %TRUE
 **/
gboolean
gtk_widget_hide_on_delete (GtkWidget      *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  
  gtk_widget_hide (widget);
  
  return TRUE;
}

/**
 * gtk_widget_show_all:
 * @widget: a #GtkWidget
 * 
 * Recursively shows a widget, and any child widgets (if the widget is
 * a container).
 **/
void
gtk_widget_show_all (GtkWidget *widget)
{
  GtkWidgetClass *class;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if ((GTK_WIDGET_FLAGS (widget) & GTK_NO_SHOW_ALL) != 0)
    return;

  class = GTK_WIDGET_GET_CLASS (widget);

  if (class->show_all)
    class->show_all (widget);
}

/**
 * gtk_widget_hide_all:
 * @widget: a #GtkWidget
 * 
 * Recursively hides a widget and any child widgets.
 **/
void
gtk_widget_hide_all (GtkWidget *widget)
{
  GtkWidgetClass *class;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if ((GTK_WIDGET_FLAGS (widget) & GTK_NO_SHOW_ALL) != 0)
    return;

  class = GTK_WIDGET_GET_CLASS (widget);

  if (class->hide_all)
    class->hide_all (widget);
}

/**
 * gtk_widget_map:
 * @widget: a #GtkWidget
 * 
 * This function is only for use in widget implementations. Causes
 * a widget to be mapped if it isn't already.
 * 
 **/
void
gtk_widget_map (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_VISIBLE (widget));
  g_return_if_fail (GTK_WIDGET_CHILD_VISIBLE (widget));
  
  if (!GTK_WIDGET_MAPPED (widget))
    {
      if (!GTK_WIDGET_REALIZED (widget))
	gtk_widget_realize (widget);

      g_signal_emit (widget, widget_signals[MAP], 0);

      if (GTK_WIDGET_NO_WINDOW (widget))
	gdk_window_invalidate_rect (widget->window, &widget->allocation, FALSE);
    }
}

/**
 * gtk_widget_unmap:
 * @widget: a #GtkWidget
 *
 * This function is only for use in widget implementations. Causes
 * a widget to be unmapped if it's currently mapped.
 * 
 **/
void
gtk_widget_unmap (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (GTK_WIDGET_MAPPED (widget))
    {
      if (GTK_WIDGET_NO_WINDOW (widget))
	gdk_window_invalidate_rect (widget->window, &widget->allocation, FALSE);
      g_signal_emit (widget, widget_signals[UNMAP], 0);
    }
}

/**
 * gtk_widget_realize:
 * @widget: a #GtkWidget
 * 
 * Creates the GDK (windowing system) resources associated with a
 * widget.  For example, @widget->window will be created when a widget
 * is realized.  Normally realization happens implicitly; if you show
 * a widget and all its parent containers, then the widget will be
 * realized and mapped automatically.
 * 
 * Realizing a widget requires all
 * the widget's parent widgets to be realized; calling
 * gtk_widget_realize() realizes the widget's parents in addition to
 * @widget itself. If a widget is not yet inside a toplevel window
 * when you realize it, bad things will happen.
 *
 * This function is primarily used in widget implementations, and
 * isn't very useful otherwise. Many times when you think you might
 * need it, a better approach is to connect to a signal that will be
 * called after the widget is realized automatically, such as
 * "expose_event". Or simply g_signal_connect_after() to the
 * "realize" signal.
 *  
 **/
void
gtk_widget_realize (GtkWidget *widget)
{
  gint events;
  GdkExtensionMode mode;
  GtkWidgetShapeInfo *shape_info;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_ANCHORED (widget) ||
		    GTK_IS_INVISIBLE (widget));
  
  if (!GTK_WIDGET_REALIZED (widget))
    {
      /*
	if (GTK_IS_CONTAINER (widget) && !GTK_WIDGET_NO_WINDOW (widget))
	  g_message ("gtk_widget_realize(%s)", g_type_name (GTK_WIDGET_TYPE (widget)));
      */

      if (widget->parent == NULL &&
          !GTK_WIDGET_TOPLEVEL (widget))
        g_warning ("Calling gtk_widget_realize() on a widget that isn't "
                   "inside a toplevel window is not going to work very well. "
                   "Widgets must be inside a toplevel container before realizing them.");
      
      if (widget->parent && !GTK_WIDGET_REALIZED (widget->parent))
	gtk_widget_realize (widget->parent);

      gtk_widget_ensure_style (widget);
      
      g_signal_emit (widget, widget_signals[REALIZE], 0);
      
      if (GTK_WIDGET_HAS_SHAPE_MASK (widget))
	{
	  shape_info = g_object_get_qdata (G_OBJECT (widget), quark_shape_info);
	  gdk_window_shape_combine_mask (widget->window,
					 shape_info->shape_mask,
					 shape_info->offset_x,
					 shape_info->offset_y);
	}
      
      shape_info = g_object_get_qdata (G_OBJECT (widget), quark_input_shape_info);
      if (shape_info)
	gdk_window_input_shape_combine_mask (widget->window,
					     shape_info->shape_mask,
					     shape_info->offset_x,
					     shape_info->offset_y);

      if (!GTK_WIDGET_NO_WINDOW (widget))
	{
	  mode = gtk_widget_get_extension_events (widget);
	  if (mode != GDK_EXTENSION_EVENTS_NONE)
	    {
	      events = gtk_widget_get_events (widget);
	      gdk_input_set_extension_events (widget->window, events, mode);
	    }
	}
      
    }
}

/**
 * gtk_widget_unrealize:
 * @widget: a #GtkWidget
 *
 * This function is only useful in widget implementations.
 * Causes a widget to be unrealized (frees all GDK resources
 * associated with the widget, such as @widget->window).
 * 
 **/
void
gtk_widget_unrealize (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (GTK_WIDGET_HAS_SHAPE_MASK (widget))
    gtk_widget_shape_combine_mask (widget, NULL, 0, 0);

  if (g_object_get_qdata (G_OBJECT (widget), quark_input_shape_info))
    gtk_widget_input_shape_combine_mask (widget, NULL, 0, 0);

  if (GTK_WIDGET_REALIZED (widget))
    {
      g_object_ref (widget);
      g_signal_emit (widget, widget_signals[UNREALIZE], 0);
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED | GTK_MAPPED);
      g_object_unref (widget);
    }
}

/*****************************************
 * Draw queueing.
 *****************************************/

/**
 * gtk_widget_queue_draw_area:
 * @widget: a #GtkWidget
 * @x: x coordinate of upper-left corner of rectangle to redraw
 * @y: y coordinate of upper-left corner of rectangle to redraw
 * @width: width of region to draw
 * @height: height of region to draw
 *
 * Invalidates the rectangular area of @widget defined by @x, @y,
 * @width and @height by calling gdk_window_invalidate_rect() on the
 * widget's window and all its child windows.  Once the main loop
 * becomes idle (after the current batch of events has been processed,
 * roughly), the window will receive expose events for the union of
 * all regions that have been invalidated.
 *
 * Normally you would only use this function in widget
 * implementations. You might also use it, or
 * gdk_window_invalidate_rect() directly, to schedule a redraw of a
 * #GtkDrawingArea or some portion thereof.
 *
 * Frequently you can just call gdk_window_invalidate_rect() or
 * gdk_window_invalidate_region() instead of this function. Those
 * functions will invalidate only a single window, instead of the
 * widget and all its children.
 *
 * The advantage of adding to the invalidated region compared to
 * simply drawing immediately is efficiency; using an invalid region
 * ensures that you only have to redraw one time.
 * 
 **/
void	   
gtk_widget_queue_draw_area (GtkWidget *widget,
			    gint       x,
			    gint       y,
			    gint       width,
 			    gint       height)
{
  GdkRectangle invalid_rect;
  GtkWidget *w;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!GTK_WIDGET_REALIZED (widget))
    return;
  
  /* Just return if the widget or one of its ancestors isn't mapped */
  for (w = widget; w != NULL; w = w->parent)
    if (!GTK_WIDGET_MAPPED (w))
      return;

  /* Find the correct widget */

  if (!GTK_WIDGET_NO_WINDOW (widget))
    {
      if (widget->parent)
	{
	  /* Translate widget relative to window-relative */

	  gint wx, wy, wwidth, wheight;
	  
	  gdk_window_get_position (widget->window, &wx, &wy);
	  x -= wx - widget->allocation.x;
	  y -= wy - widget->allocation.y;
	  
	  gdk_drawable_get_size (widget->window, &wwidth, &wheight);

	  if (x + width <= 0 || y + height <= 0 ||
	      x >= wwidth || y >= wheight)
	    return;
	  
	  if (x < 0)
	    {
	      width += x;  x = 0;
	    }
	  if (y < 0)
	    {
	      height += y; y = 0;
	    }
	  if (x + width > wwidth)
	    width = wwidth - x;
	  if (y + height > wheight)
	    height = wheight - y;
	}
    }

  invalid_rect.x = x;
  invalid_rect.y = y;
  invalid_rect.width = width;
  invalid_rect.height = height;
  
  gdk_window_invalidate_rect (widget->window, &invalid_rect, TRUE);
}

static void
widget_add_child_draw_rectangle (GtkWidget    *widget,
				 GdkRectangle *rect)
{
  GdkRectangle child_rect;
  
  if (!GTK_WIDGET_MAPPED (widget) ||
      widget->window != widget->parent->window)
    return;

  gtk_widget_get_draw_rectangle (widget, &child_rect);
  gdk_rectangle_union (rect, &child_rect, rect);
}

static void
gtk_widget_get_draw_rectangle (GtkWidget    *widget,
			       GdkRectangle *rect)
{
  if (GTK_WIDGET_NO_WINDOW (widget))
    {
      GtkBorder *draw_border = NULL;

      *rect = widget->allocation;

      gtk_widget_style_get (widget, 
			    "draw-border", &draw_border,
			    NULL);
      if (draw_border)
	{
	  rect->x -= draw_border->top;
	  rect->y -= draw_border->left;
	  rect->width += draw_border->left + draw_border->right;
	  rect->height += draw_border->top + draw_border->bottom;
        
          gtk_border_free (draw_border);
	}

      if (GTK_IS_CONTAINER (widget))
	gtk_container_forall (GTK_CONTAINER (widget),
			      (GtkCallback)widget_add_child_draw_rectangle,
			      rect);
    }
  else
    {
      rect->x = 0;
      rect->y = 0;
      rect->width = widget->allocation.width;
      rect->height = widget->allocation.height;
    }
}

/**
 * gtk_widget_queue_draw:
 * @widget: a #GtkWidget
 *
 * Equivalent to calling gtk_widget_queue_draw_area() for the
 * entire area of a widget.
 * 
 **/
void	   
gtk_widget_queue_draw (GtkWidget *widget)
{
  GdkRectangle rect;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_get_draw_rectangle (widget, &rect);

  gtk_widget_queue_draw_area (widget,
			      rect.x, rect.y,
			      rect.width, rect.height);
}

/* Invalidates the given area (allocation-relative-coordinates)
 * in all of the widget's windows
 */
/**
 * gtk_widget_queue_clear_area:
 * @widget: a #GtkWidget
 * @x: x coordinate of upper-left corner of rectangle to redraw
 * @y: y coordinate of upper-left corner of rectangle to redraw
 * @width: width of region to draw
 * @height: height of region to draw
 * 
 * This function is no longer different from
 * gtk_widget_queue_draw_area(), though it once was. Now it just calls
 * gtk_widget_queue_draw_area(). Originally
 * gtk_widget_queue_clear_area() would force a redraw of the
 * background for %GTK_NO_WINDOW widgets, and
 * gtk_widget_queue_draw_area() would not. Now both functions ensure
 * the background will be redrawn.
 * 
 * @Deprecated: Use gtk_widget_queue_draw_area() instead.
 **/
void	   
gtk_widget_queue_clear_area (GtkWidget *widget,
			     gint       x,
			     gint       y,
			     gint       width,
			     gint       height)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_queue_draw_area (widget, x, y, width, height);
}

/**
 * gtk_widget_queue_clear:
 * @widget: a #GtkWidget
 * 
 * This function does the same as gtk_widget_queue_draw().
 *
 * @Deprecated: Use gtk_widget_queue_draw() instead.
 **/
void	   
gtk_widget_queue_clear (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_queue_draw (widget);
}

/**
 * gtk_widget_queue_resize:
 * @widget: a #GtkWidget
 *
 * This function is only for use in widget implementations.
 * Flags a widget to have its size renegotiated; should
 * be called when a widget for some reason has a new size request.
 * For example, when you change the text in a #GtkLabel, #GtkLabel
 * queues a resize to ensure there's enough space for the new text.
 **/
void
gtk_widget_queue_resize (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (GTK_WIDGET_REALIZED (widget))
    gtk_widget_queue_shallow_draw (widget);
      
  _gtk_size_group_queue_resize (widget);
}

/**
 * gtk_widget_queue_resize_no_redraw:
 * @widget: a #GtkWidget
 *
 * This function works like gtk_widget_queue_resize(), except that the
 * widget is not invalidated.
 *
 * Since: 2.4
 **/
void
gtk_widget_queue_resize_no_redraw (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  _gtk_size_group_queue_resize (widget);
}

/**
 * gtk_widget_draw:
 * @widget: a #GtkWidget
 * @area: area to draw
 *
 * In GTK+ 1.2, this function would immediately render the
 * region @area of a widget, by invoking the virtual draw method of a
 * widget. In GTK+ 2.0, the draw method is gone, and instead
 * gtk_widget_draw() simply invalidates the specified region of the
 * widget, then updates the invalid region of the widget immediately.
 * Usually you don't want to update the region immediately for
 * performance reasons, so in general gtk_widget_queue_draw_area() is
 * a better choice if you want to draw a region of a widget.
 * 
 **/
void
gtk_widget_draw (GtkWidget    *widget,
		 GdkRectangle *area)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      if (area)
        gtk_widget_queue_draw_area (widget,
                                    area->x, area->y,
                                    area->width, area->height);
      else
        gtk_widget_queue_draw (widget);

      gdk_window_process_updates (widget->window, TRUE);
    }
}

/**
 * gtk_widget_size_request:
 * @widget: a #GtkWidget
 * @requisition: a #GtkRequisition to be filled in
 * 
 * This function is typically used when implementing a #GtkContainer
 * subclass.  Obtains the preferred size of a widget. The container
 * uses this information to arrange its child widgets and decide what
 * size allocations to give them with gtk_widget_size_allocate().
 *
 * You can also call this function from an application, with some
 * caveats. Most notably, getting a size request requires the widget
 * to be associated with a screen, because font information may be
 * needed. Multihead-aware applications should keep this in mind.
 *
 * Also remember that the size request is not necessarily the size
 * a widget will actually be allocated.
 *
 * See also gtk_widget_get_child_requisition().
 **/
void
gtk_widget_size_request (GtkWidget	*widget,
			 GtkRequisition *requisition)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

#ifdef G_ENABLE_DEBUG
  if (requisition == &widget->requisition)
    g_warning ("gtk_widget_size_request() called on child widget with request equal\n to widget->requisition. gtk_widget_set_usize() may not work properly.");
#endif /* G_ENABLE_DEBUG */

  _gtk_size_group_compute_requisition (widget, requisition);
}

/**
 * gtk_widget_get_child_requisition:
 * @widget: a #GtkWidget
 * @requisition: a #GtkRequisition to be filled in
 * 
 * This function is only for use in widget implementations. Obtains
 * @widget->requisition, unless someone has forced a particular
 * geometry on the widget (e.g. with gtk_widget_set_usize()), in which
 * case it returns that geometry instead of the widget's requisition.
 *
 * This function differs from gtk_widget_size_request() in that
 * it retrieves the last size request value from @widget->requisition,
 * while gtk_widget_size_request() actually calls the "size_request" method
 * on @widget to compute the size request and fill in @widget->requisition,
 * and only then returns @widget->requisition.
 *
 * Because this function does not call the "size_request" method, it
 * can only be used when you know that @widget->requisition is
 * up-to-date, that is, gtk_widget_size_request() has been called
 * since the last time a resize was queued. In general, only container
 * implementations have this information; applications should use
 * gtk_widget_size_request().
 **/
void
gtk_widget_get_child_requisition (GtkWidget	 *widget,
				  GtkRequisition *requisition)
{
  _gtk_size_group_get_child_requisition (widget, requisition);
}

static gboolean
invalidate_predicate (GdkWindow *window,
		      gpointer   data)
{
  gpointer user_data;

  gdk_window_get_user_data (window, &user_data);

  return (user_data == data);
}

/* Invalidate @region in widget->window and all children
 * of widget->window owned by widget. @region is in the
 * same coordinates as widget->allocation and will be
 * modified by this call.
 */
static void
gtk_widget_invalidate_widget_windows (GtkWidget *widget,
				      GdkRegion *region)
{
  if (!GTK_WIDGET_REALIZED (widget))
    return;
  
  if (!GTK_WIDGET_NO_WINDOW (widget) && widget->parent)
    {
      int x, y;
      
      gdk_window_get_position (widget->window, &x, &y);
      gdk_region_offset (region, -x, -y);
    }

  gdk_window_invalidate_maybe_recurse (widget->window, region,
				       invalidate_predicate, widget);
}

/**
 * gtk_widget_queue_shallow_draw:
 * @widget: a #GtkWidget
 *
 * Like gtk_widget_queue_draw(), but only windows owned
 * by @widget are invalidated.
 **/
static void
gtk_widget_queue_shallow_draw (GtkWidget *widget)
{
  GdkRectangle rect;
  GdkRegion *region;
  
  if (!GTK_WIDGET_REALIZED (widget))
    return;

  gtk_widget_get_draw_rectangle (widget, &rect);

  /* get_draw_rectangle() gives us window coordinates, we
   * need to convert to the coordinates that widget->allocation
   * is in.
   */
  if (!GTK_WIDGET_NO_WINDOW (widget) && widget->parent)
    {
      int wx, wy;
      
      gdk_window_get_position (widget->window, &wx, &wy);
      
      rect.x += wx;
      rect.y += wy;
    }
  
  region = gdk_region_rectangle (&rect);
  gtk_widget_invalidate_widget_windows (widget, region);
  gdk_region_destroy (region);
}

/**
 * gtk_widget_size_allocate:
 * @widget: a #GtkWidget
 * @allocation: position and size to be allocated to @widget
 *
 * This function is only used by #GtkContainer subclasses, to assign a size
 * and position to their child widgets. 
 * 
 **/
void
gtk_widget_size_allocate (GtkWidget	*widget,
			  GtkAllocation *allocation)
{
  GtkWidgetAuxInfo *aux_info;
  GdkRectangle real_allocation;
  GdkRectangle old_allocation;
  gboolean alloc_needed;
  gboolean size_changed;
  gboolean position_changed;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
 
#ifdef G_ENABLE_DEBUG
  if (gtk_debug_flags & GTK_DEBUG_GEOMETRY)
    {
      gint depth;
      GtkWidget *parent;
      const gchar *name;

      depth = 0;
      parent = widget;
      while (parent)
	{
	  depth++;
	  parent = gtk_widget_get_parent (parent);
	}
      
      name = g_type_name (G_OBJECT_TYPE (G_OBJECT (widget)));
      g_print ("gtk_widget_size_allocate: %*s%s %d %d\n", 
	       2 * depth, " ", name, 
	       allocation->width, allocation->height);
    }
#endif /* G_ENABLE_DEBUG */
 
  alloc_needed = GTK_WIDGET_ALLOC_NEEDED (widget);
  if (!GTK_WIDGET_REQUEST_NEEDED (widget))      /* Preserve request/allocate ordering */
    GTK_PRIVATE_UNSET_FLAG (widget, GTK_ALLOC_NEEDED);

  old_allocation = widget->allocation;
  real_allocation = *allocation;
  aux_info =_gtk_widget_get_aux_info (widget, FALSE);
  
  if (aux_info)
    {
      if (aux_info->x_set)
	real_allocation.x = aux_info->x;
      if (aux_info->y_set)
	real_allocation.y = aux_info->y;
    }

  if (real_allocation.width < 0 || real_allocation.height < 0)
    {
      g_warning ("gtk_widget_size_allocate(): attempt to allocate widget with width %d and height %d",
		 real_allocation.width,
		 real_allocation.height);
    }
  
  real_allocation.width = MAX (real_allocation.width, 1);
  real_allocation.height = MAX (real_allocation.height, 1);

  size_changed = (old_allocation.width != real_allocation.width ||
		  old_allocation.height != real_allocation.height);
  position_changed = (old_allocation.x != real_allocation.x ||
		      old_allocation.y != real_allocation.y);

  if (!alloc_needed && !size_changed && !position_changed)
    return;
  
  g_signal_emit (widget, widget_signals[SIZE_ALLOCATE], 0, &real_allocation);

  if (GTK_WIDGET_MAPPED (widget))
    {
      if (GTK_WIDGET_NO_WINDOW (widget) && GTK_WIDGET_REDRAW_ON_ALLOC (widget) && position_changed)
	{
	  /* Invalidate union(old_allaction,widget->allocation) in widget->window
	   */
	  GdkRegion *invalidate = gdk_region_rectangle (&widget->allocation);
	  gdk_region_union_with_rect (invalidate, &old_allocation);

	  gdk_window_invalidate_region (widget->window, invalidate, FALSE);
	  gdk_region_destroy (invalidate);
	}
      
      if (size_changed)
	{
	  if (GTK_WIDGET_REDRAW_ON_ALLOC (widget))
	    {
	      /* Invalidate union(old_allaction,widget->allocation) in widget->window and descendents owned by widget
	       */
	      GdkRegion *invalidate = gdk_region_rectangle (&widget->allocation);
	      gdk_region_union_with_rect (invalidate, &old_allocation);

	      gtk_widget_invalidate_widget_windows (widget, invalidate);
	      gdk_region_destroy (invalidate);
	    }
	}
    }

  if ((size_changed || position_changed) && widget->parent &&
      GTK_WIDGET_REALIZED (widget->parent) && GTK_CONTAINER (widget->parent)->reallocate_redraws)
    {
      GdkRegion *invalidate = gdk_region_rectangle (&widget->parent->allocation);
      gtk_widget_invalidate_widget_windows (widget->parent, invalidate);
      gdk_region_destroy (invalidate);
    }
}

/**
 * gtk_widget_common_ancestor:
 * @widget_a: a #GtkWidget
 * @widget_b: a #GtkWidget
 * 
 * Find the common ancestor of @widget_a and @widget_b that
 * is closest to the two widgets.
 * 
 * Return value: the closest common ancestor of @widget_a and
 *   @widget_b or %NULL if @widget_a and @widget_b do not
 *   share a common ancestor.
 **/
static GtkWidget *
gtk_widget_common_ancestor (GtkWidget *widget_a,
			    GtkWidget *widget_b)
{
  GtkWidget *parent_a;
  GtkWidget *parent_b;
  gint depth_a = 0;
  gint depth_b = 0;

  parent_a = widget_a;
  while (parent_a->parent)
    {
      parent_a = parent_a->parent;
      depth_a++;
    }

  parent_b = widget_b;
  while (parent_b->parent)
    {
      parent_b = parent_b->parent;
      depth_b++;
    }

  if (parent_a != parent_b)
    return NULL;

  while (depth_a > depth_b)
    {
      widget_a = widget_a->parent;
      depth_a--;
    }

  while (depth_b > depth_a)
    {
      widget_b = widget_b->parent;
      depth_b--;
    }

  while (widget_a != widget_b)
    {
      widget_a = widget_a->parent;
      widget_b = widget_b->parent;
    }

  return widget_a;
}

/**
 * gtk_widget_translate_coordinates:
 * @src_widget:  a #GtkWidget
 * @dest_widget: a #GtkWidget
 * @src_x: X position relative to @src_widget
 * @src_y: Y position relative to @src_widget
 * @dest_x: location to store X position relative to @dest_widget
 * @dest_y: location to store Y position relative to @dest_widget
 * 
 * Translate coordinates relative to @src_widget's allocation to coordinates
 * relative to @dest_widget's allocations. In order to perform this
 * operation, both widgets must be realized, and must share a common
 * toplevel.
 * 
 * Return value: %FALSE if either widget was not realized, or there
 *   was no common ancestor. In this case, nothing is stored in
 *   *@dest_x and *@dest_y. Otherwise %TRUE.
 **/
gboolean
gtk_widget_translate_coordinates (GtkWidget  *src_widget,
				  GtkWidget  *dest_widget,
				  gint        src_x,
				  gint        src_y,
				  gint       *dest_x,
				  gint       *dest_y)
{
  GtkWidget *ancestor;
  GdkWindow *window;

  g_return_val_if_fail (GTK_IS_WIDGET (src_widget), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (dest_widget), FALSE);

  ancestor = gtk_widget_common_ancestor (src_widget, dest_widget);
  if (!ancestor || !GTK_WIDGET_REALIZED (src_widget) || !GTK_WIDGET_REALIZED (dest_widget))
    return FALSE;

  /* Translate from allocation relative to window relative */
  if (!GTK_WIDGET_NO_WINDOW (src_widget) && src_widget->parent)
    {
      gint wx, wy;
      gdk_window_get_position (src_widget->window, &wx, &wy);

      src_x -= wx - src_widget->allocation.x;
      src_y -= wy - src_widget->allocation.y;
    }
  else
    {
      src_x += src_widget->allocation.x;
      src_y += src_widget->allocation.y;
    }

  /* Translate to the common ancestor */
  window = src_widget->window;
  while (window != ancestor->window)
    {
      gint dx, dy;
      
      gdk_window_get_position (window, &dx, &dy);
      
      src_x += dx;
      src_y += dy;
      
      window = gdk_window_get_parent (window);

      if (!window)		/* Handle GtkHandleBox */
	return FALSE;
    }

  /* And back */
  window = dest_widget->window;
  while (window != ancestor->window)
    {
      gint dx, dy;
      
      gdk_window_get_position (window, &dx, &dy);
      
      src_x -= dx;
      src_y -= dy;
      
      window = gdk_window_get_parent (window);
      
      if (!window)		/* Handle GtkHandleBox */
	return FALSE;
    }

  /* Translate from window relative to allocation relative */
  if (!GTK_WIDGET_NO_WINDOW (dest_widget) && dest_widget->parent)
    {
      gint wx, wy;
      gdk_window_get_position (dest_widget->window, &wx, &wy);

      src_x += wx - dest_widget->allocation.x;
      src_y += wy - dest_widget->allocation.y;
    }
  else
    {
      src_x -= dest_widget->allocation.x;
      src_y -= dest_widget->allocation.y;
    }

  if (dest_x)
    *dest_x = src_x;
  if (dest_y)
    *dest_y = src_y;

  return TRUE;
}

static void
gtk_widget_real_size_allocate (GtkWidget     *widget,
			       GtkAllocation *allocation)
{
  widget->allocation = *allocation;
  
  if (GTK_WIDGET_REALIZED (widget) &&
      !GTK_WIDGET_NO_WINDOW (widget))
     {
	gdk_window_move_resize (widget->window,
				allocation->x, allocation->y,
				allocation->width, allocation->height);
     }
}

static gboolean
gtk_widget_real_can_activate_accel (GtkWidget *widget,
                                    guint      signal_id)
{
  /* widgets must be onscreen for accels to take effect */
  return GTK_WIDGET_IS_SENSITIVE (widget) && GTK_WIDGET_DRAWABLE (widget) && gdk_window_is_viewable (widget->window);
}

/**
 * gtk_widget_can_activate_accel:
 * @widget: a #GtkWidget
 * @signal_id: the ID of a signal installed on @widget
 * 
 * Determines whether an accelerator that activates the signal
 * identified by @signal_id can currently be activated.
 * This is done by emitting the GtkWidget::can-activate-accel
 * signal on @widget; if the signal isn't overridden by a
 * handler or in a derived widget, then the default check is
 * that the widget must be sensitive, and the widget and all
 * its ancestors mapped.
 *
 * Return value: %TRUE if the accelerator can be activated.
 *
 * Since: 2.4
 **/
gboolean
gtk_widget_can_activate_accel (GtkWidget *widget,
                               guint      signal_id)
{
  gboolean can_activate = FALSE;
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_signal_emit (widget, widget_signals[CAN_ACTIVATE_ACCEL], 0, signal_id, &can_activate);
  return can_activate;
}

typedef struct {
  GClosure   closure;
  guint      signal_id;
} AccelClosure;

static void
closure_accel_activate (GClosure     *closure,
			GValue       *return_value,
			guint         n_param_values,
			const GValue *param_values,
			gpointer      invocation_hint,
			gpointer      marshal_data)
{
  AccelClosure *aclosure = (AccelClosure*) closure;
  gboolean can_activate = gtk_widget_can_activate_accel (closure->data, aclosure->signal_id);

  if (can_activate)
    g_signal_emit (closure->data, aclosure->signal_id, 0);

  /* whether accelerator was handled */
  g_value_set_boolean (return_value, can_activate);
}

static void
closures_destroy (gpointer data)
{
  GSList *slist, *closures = data;

  for (slist = closures; slist; slist = slist->next)
    {
      g_closure_invalidate (slist->data);
      g_closure_unref (slist->data);
    }
  g_slist_free (closures);
}

static GClosure*
widget_new_accel_closure (GtkWidget *widget,
			  guint      signal_id)
{
  AccelClosure *aclosure;
  GClosure *closure = NULL;
  GSList *slist, *closures;

  closures = g_object_steal_qdata (G_OBJECT (widget), quark_accel_closures);
  for (slist = closures; slist; slist = slist->next)
    if (!gtk_accel_group_from_accel_closure (slist->data))
      {
	/* reuse this closure */
	closure = slist->data;
	break;
      }
  if (!closure)
    {
      closure = g_closure_new_object (sizeof (AccelClosure), G_OBJECT (widget));
      closures = g_slist_prepend (closures, g_closure_ref (closure));
      g_closure_sink (closure);
      g_closure_set_marshal (closure, closure_accel_activate);
    }
  g_object_set_qdata_full (G_OBJECT (widget), quark_accel_closures, closures, closures_destroy);
  
  aclosure = (AccelClosure*) closure;
  g_assert (closure->data == widget);
  g_assert (closure->marshal == closure_accel_activate);
  aclosure->signal_id = signal_id;

  return closure;
}

/**
 * gtk_widget_add_accelerator
 * @widget:       widget to install an accelerator on
 * @accel_signal: widget signal to emit on accelerator activation
 * @accel_group:  accel group for this widget, added to its toplevel
 * @accel_key:    GDK keyval of the accelerator
 * @accel_mods:   modifier key combination of the accelerator
 * @accel_flags:  flag accelerators, e.g. %GTK_ACCEL_VISIBLE
 *
 * Installs an accelerator for this @widget in @accel_group that causes
 * @accel_signal to be emitted if the accelerator is activated.
 * The @accel_group needs to be added to the widget's toplevel via
 * gtk_window_add_accel_group(), and the signal must be of type %G_RUN_ACTION.
 * Accelerators added through this function are not user changeable during
 * runtime. If you want to support accelerators that can be changed by the
 * user, use gtk_accel_map_add_entry() and gtk_widget_set_accel_path() or
 * gtk_menu_item_set_accel_path() instead.
 */
void
gtk_widget_add_accelerator (GtkWidget      *widget,
			    const gchar    *accel_signal,
			    GtkAccelGroup  *accel_group,
			    guint           accel_key,
			    GdkModifierType accel_mods,
			    GtkAccelFlags   accel_flags)
{
  GClosure *closure;
  GSignalQuery query;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (accel_signal != NULL);
  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));

  g_signal_query (g_signal_lookup (accel_signal, G_OBJECT_TYPE (widget)), &query);
  if (!query.signal_id ||
      !(query.signal_flags & G_SIGNAL_ACTION) ||
      query.return_type != G_TYPE_NONE ||
      query.n_params)
    {
      /* hmm, should be elaborate enough */
      g_warning (G_STRLOC ": widget `%s' has no activatable signal \"%s\" without arguments",
		 G_OBJECT_TYPE_NAME (widget), accel_signal);
      return;
    }

  closure = widget_new_accel_closure (widget, query.signal_id);

  g_object_ref (widget);

  /* install the accelerator. since we don't map this onto an accel_path,
   * the accelerator will automatically be locked.
   */
  gtk_accel_group_connect (accel_group,
			   accel_key,
			   accel_mods,
			   accel_flags | GTK_ACCEL_LOCKED,
			   closure);

  g_signal_emit (widget, widget_signals[ACCEL_CLOSURES_CHANGED], 0);

  g_object_unref (widget);
}

/**
 * gtk_widget_remove_accelerator:
 * @widget:       widget to install an accelerator on
 * @accel_group:  accel group for this widget
 * @accel_key:    GDK keyval of the accelerator
 * @accel_mods:   modifier key combination of the accelerator
 * @returns:      whether an accelerator was installed and could be removed
 *
 * Removes an accelerator from @widget, previously installed with
 * gtk_widget_add_accelerator().
 */
gboolean
gtk_widget_remove_accelerator (GtkWidget      *widget,
			       GtkAccelGroup  *accel_group,
			       guint           accel_key,
			       GdkModifierType accel_mods)
{
  GtkAccelGroupEntry *ag_entry;
  GList *slist, *clist;
  guint n;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_ACCEL_GROUP (accel_group), FALSE);

  ag_entry = gtk_accel_group_query (accel_group, accel_key, accel_mods, &n);
  clist = gtk_widget_list_accel_closures (widget);
  for (slist = clist; slist; slist = slist->next)
    {
      guint i;

      for (i = 0; i < n; i++)
	if (slist->data == (gpointer) ag_entry[i].closure)
	  {
	    gboolean is_removed = gtk_accel_group_disconnect (accel_group, slist->data);

	    g_signal_emit (widget, widget_signals[ACCEL_CLOSURES_CHANGED], 0);

	    g_list_free (clist);

	    return is_removed;
	  }
    }
  g_list_free (clist);

  g_warning (G_STRLOC ": no accelerator (%u,%u) installed in accel group (%p) for %s (%p)",
	     accel_key, accel_mods, accel_group,
	     G_OBJECT_TYPE_NAME (widget), widget);

  return FALSE;
}

/**
 * gtk_widget_list_accel_closures
 * @widget:  widget to list accelerator closures for
 * @returns: a newly allocated #GList of closures
 *
 * Lists the closures used by @widget for accelerator group connections
 * with gtk_accel_group_connect_by_path() or gtk_accel_group_connect().
 * The closures can be used to monitor accelerator changes on @widget,
 * by connecting to the ::accel_changed signal of the #GtkAccelGroup of a 
 * closure which can be found out with gtk_accel_group_from_accel_closure().
 */
GList*
gtk_widget_list_accel_closures (GtkWidget *widget)
{
  GSList *slist;
  GList *clist = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  for (slist = g_object_get_qdata (G_OBJECT (widget), quark_accel_closures); slist; slist = slist->next)
    if (gtk_accel_group_from_accel_closure (slist->data))
      clist = g_list_prepend (clist, slist->data);
  return clist;
}

typedef struct {
  GQuark         path_quark;
  GtkAccelGroup *accel_group;
  GClosure      *closure;
} AccelPath;

static void
destroy_accel_path (gpointer data)
{
  AccelPath *apath = data;

  gtk_accel_group_disconnect (apath->accel_group, apath->closure);

  /* closures_destroy takes care of unrefing the closure */
  g_object_unref (apath->accel_group);
  
  g_slice_free (AccelPath, apath);
}


/**
 * gtk_widget_set_accel_path:
 * @widget: a #GtkWidget
 * @accel_path: path used to look up the accelerator
 * @accel_group: a #GtkAccelGroup.
 * 
 * Given an accelerator group, @accel_group, and an accelerator path,
 * @accel_path, sets up an accelerator in @accel_group so whenever the
 * key binding that is defined for @accel_path is pressed, @widget
 * will be activated.  This removes any accelerators (for any
 * accelerator group) installed by previous calls to
 * gtk_widget_set_accel_path(). Associating accelerators with
 * paths allows them to be modified by the user and the modifications
 * to be saved for future use. (See gtk_accel_map_save().)
 *
 * This function is a low level function that would most likely
 * be used by a menu creation system like #GtkItemFactory. If you
 * use #GtkItemFactory, setting up accelerator paths will be done
 * automatically.
 *
 * Even when you you aren't using #GtkItemFactory, if you only want to
 * set up accelerators on menu items gtk_menu_item_set_accel_path()
 * provides a somewhat more convenient interface.
 **/
void
gtk_widget_set_accel_path (GtkWidget     *widget,
			   const gchar   *accel_path,
			   GtkAccelGroup *accel_group)
{
  AccelPath *apath;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_GET_CLASS (widget)->activate_signal != 0);

  if (accel_path)
    {
      g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));
      g_return_if_fail (_gtk_accel_path_is_valid (accel_path));

      gtk_accel_map_add_entry (accel_path, 0, 0);
      apath = g_slice_new (AccelPath);
      apath->accel_group = g_object_ref (accel_group);
      apath->path_quark = g_quark_from_string (accel_path);
      apath->closure = widget_new_accel_closure (widget, GTK_WIDGET_GET_CLASS (widget)->activate_signal);
    }
  else
    apath = NULL;

  /* also removes possible old settings */
  g_object_set_qdata_full (G_OBJECT (widget), quark_accel_path, apath, destroy_accel_path);

  if (apath)
    gtk_accel_group_connect_by_path (apath->accel_group, g_quark_to_string (apath->path_quark), apath->closure);

  g_signal_emit (widget, widget_signals[ACCEL_CLOSURES_CHANGED], 0);
}

const gchar*
_gtk_widget_get_accel_path (GtkWidget *widget,
			    gboolean  *locked)
{
  AccelPath *apath;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  apath = g_object_get_qdata (G_OBJECT (widget), quark_accel_path);
  if (locked)
    *locked = apath ? apath->accel_group->lock_count > 0 : TRUE;
  return apath ? g_quark_to_string (apath->path_quark) : NULL;
}

gboolean
gtk_widget_mnemonic_activate (GtkWidget *widget,
                              gboolean   group_cycling)
{
  gboolean handled;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  group_cycling = group_cycling != FALSE;
  if (!GTK_WIDGET_IS_SENSITIVE (widget))
    handled = TRUE;
  else
    g_signal_emit (widget,
		   widget_signals[MNEMONIC_ACTIVATE],
		   0,
		   group_cycling,
		   &handled);
  return handled;
}

static gboolean
gtk_widget_real_mnemonic_activate (GtkWidget *widget,
                                   gboolean   group_cycling)
{
  if (!group_cycling && GTK_WIDGET_GET_CLASS (widget)->activate_signal)
    gtk_widget_activate (widget);
  else if (GTK_WIDGET_CAN_FOCUS (widget))
    gtk_widget_grab_focus (widget);
  else
    {
      g_warning ("widget `%s' isn't suitable for mnemonic activation",
		 G_OBJECT_TYPE_NAME (widget));
      gdk_display_beep (gtk_widget_get_display (widget));
    }
  return TRUE;
}

static gboolean
gtk_widget_real_key_press_event (GtkWidget         *widget,
				 GdkEventKey       *event)
{
  return gtk_bindings_activate_event (GTK_OBJECT (widget), event);
}

static gboolean
gtk_widget_real_key_release_event (GtkWidget         *widget,
				   GdkEventKey       *event)
{
  return gtk_bindings_activate_event (GTK_OBJECT (widget), event);
}

static gboolean
gtk_widget_real_focus_in_event (GtkWidget     *widget,
                                GdkEventFocus *event)
{
  gtk_widget_queue_shallow_draw (widget);

  return FALSE;
}

static gboolean
gtk_widget_real_focus_out_event (GtkWidget     *widget,
                                 GdkEventFocus *event)
{
  gtk_widget_queue_shallow_draw (widget);

  return FALSE;
}

#define WIDGET_REALIZED_FOR_EVENT(widget, event) \
     (event->type == GDK_FOCUS_CHANGE || GTK_WIDGET_REALIZED(widget))

/**
 * gtk_widget_event:
 * @widget: a #GtkWidget
 * @event: a #GdkEvent
 * 
 * Rarely-used function. This function is used to emit
 * the event signals on a widget (those signals should never
 * be emitted without using this function to do so).
 * If you want to synthesize an event though, don't use this function;
 * instead, use gtk_main_do_event() so the event will behave as if
 * it were in the event queue. Don't synthesize expose events; instead,
 * use gdk_window_invalidate_rect() to invalidate a region of the
 * window.
 * 
 * Return value: return from the event signal emission (%TRUE if the event was handled)
 **/
gboolean
gtk_widget_event (GtkWidget *widget,
		  GdkEvent  *event)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);
  g_return_val_if_fail (WIDGET_REALIZED_FOR_EVENT (widget, event), TRUE);

  if (event->type == GDK_EXPOSE)
    {
      g_warning ("Events of type GDK_EXPOSE cannot be synthesized. To get "
		 "the same effect, call gdk_window_invalidate_rect/region(), "
		 "followed by gdk_window_process_updates().");
      return TRUE;
    }
  
  return gtk_widget_event_internal (widget, event);
}


/**
 * gtk_widget_send_expose:
 * @widget: a #GtkWidget
 * @event: a expose #GdkEvent
 * 
 * Very rarely-used function. This function is used to emit
 * an expose event signals on a widget. This function is not
 * normally used directly. The only time it is used is when
 * propagating an expose event to a child %NO_WINDOW widget, and
 * that is normally done using gtk_container_propagate_expose().
 *
 * If you want to force an area of a window to be redrawn, 
 * use gdk_window_invalidate_rect() or gdk_window_invalidate_region().
 * To cause the redraw to be done immediately, follow that call
 * with a call to gdk_window_process_updates().
 * 
 * Return value: return from the event signal emission (%TRUE if the event was handled)
 **/
gint
gtk_widget_send_expose (GtkWidget *widget,
			GdkEvent  *event)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);
  g_return_val_if_fail (GTK_WIDGET_REALIZED (widget), TRUE);
  g_return_val_if_fail (event != NULL, TRUE);
  g_return_val_if_fail (event->type == GDK_EXPOSE, TRUE);

  return gtk_widget_event_internal (widget, event);
}

static gboolean
event_window_is_still_viewable (GdkEvent *event)
{
  /* Some programs, such as gnome-theme-manager, fake widgets
   * into exposing onto a pixmap by sending expose events with
   * event->window pointing to a pixmap
   */
  if (GDK_IS_PIXMAP (event->any.window))
    return event->type == GDK_EXPOSE;
  
  /* Check that we think the event's window is viewable before
   * delivering the event, to prevent suprises. We do this here
   * at the last moment, since the event may have been queued
   * up behind other events, held over a recursive main loop, etc.
   */
  switch (event->type)
    {
    case GDK_EXPOSE:
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_KEY_PRESS:
    case GDK_ENTER_NOTIFY:
    case GDK_PROXIMITY_IN:
    case GDK_SCROLL:
      return event->any.window && gdk_window_is_viewable (event->any.window);

#if 0
    /* The following events are the second half of paired events;
     * we always deliver them to deal with widgets that clean up
     * on the second half.
     */
    case GDK_BUTTON_RELEASE:
    case GDK_KEY_RELEASE:
    case GDK_LEAVE_NOTIFY:
    case GDK_PROXIMITY_OUT:
#endif      
      
    default:
      /* Remaining events would make sense on an not-viewable window,
       * or don't have an associated window.
       */
      return TRUE;
    }
}

static gint
gtk_widget_event_internal (GtkWidget *widget,
			   GdkEvent  *event)
{
  gboolean return_val = FALSE;

  /* We check only once for is-still-visible; if someone
   * hides the window in on of the signals on the widget,
   * they are responsible for returning TRUE to terminate
   * handling.
   */
  if (!event_window_is_still_viewable (event))
    return TRUE;

  g_object_ref (widget);

  g_signal_emit (widget, widget_signals[EVENT], 0, event, &return_val);
  return_val |= !WIDGET_REALIZED_FOR_EVENT (widget, event);
  if (!return_val)
    {
      gint signal_num;

      switch (event->type)
	{
	case GDK_NOTHING:
	  signal_num = -1;
	  break;
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	  signal_num = BUTTON_PRESS_EVENT;
	  break;
	case GDK_SCROLL:
	  signal_num = SCROLL_EVENT;
	  break;
	case GDK_BUTTON_RELEASE:
	  signal_num = BUTTON_RELEASE_EVENT;
	  break;
	case GDK_MOTION_NOTIFY:
	  signal_num = MOTION_NOTIFY_EVENT;
	  break;
	case GDK_DELETE:
	  signal_num = DELETE_EVENT;
	  break;
	case GDK_DESTROY:
	  signal_num = DESTROY_EVENT;
	  break;
	case GDK_KEY_PRESS:
	  signal_num = KEY_PRESS_EVENT;
	  break;
	case GDK_KEY_RELEASE:
	  signal_num = KEY_RELEASE_EVENT;
	  break;
	case GDK_ENTER_NOTIFY:
	  signal_num = ENTER_NOTIFY_EVENT;
	  break;
	case GDK_LEAVE_NOTIFY:
	  signal_num = LEAVE_NOTIFY_EVENT;
	  break;
	case GDK_FOCUS_CHANGE:
	  signal_num = event->focus_change.in ? FOCUS_IN_EVENT : FOCUS_OUT_EVENT;
	  break;
	case GDK_CONFIGURE:
	  signal_num = CONFIGURE_EVENT;
	  break;
	case GDK_MAP:
	  signal_num = MAP_EVENT;
	  break;
	case GDK_UNMAP:
	  signal_num = UNMAP_EVENT;
	  break;
	case GDK_WINDOW_STATE:
	  signal_num = WINDOW_STATE_EVENT;
	  break;
	case GDK_PROPERTY_NOTIFY:
	  signal_num = PROPERTY_NOTIFY_EVENT;
	  break;
	case GDK_SELECTION_CLEAR:
	  signal_num = SELECTION_CLEAR_EVENT;
	  break;
	case GDK_SELECTION_REQUEST:
	  signal_num = SELECTION_REQUEST_EVENT;
	  break;
	case GDK_SELECTION_NOTIFY:
	  signal_num = SELECTION_NOTIFY_EVENT;
	  break;
	case GDK_PROXIMITY_IN:
	  signal_num = PROXIMITY_IN_EVENT;
	  break;
	case GDK_PROXIMITY_OUT:
	  signal_num = PROXIMITY_OUT_EVENT;
	  break;
	case GDK_NO_EXPOSE:
	  signal_num = NO_EXPOSE_EVENT;
	  break;
	case GDK_CLIENT_EVENT:
	  signal_num = CLIENT_EVENT;
	  break;
	case GDK_EXPOSE:
	  signal_num = EXPOSE_EVENT;
	  break;
	case GDK_VISIBILITY_NOTIFY:
	  signal_num = VISIBILITY_NOTIFY_EVENT;
	  break;
	case GDK_GRAB_BROKEN:
	  signal_num = GRAB_BROKEN;
	  break;
	default:
	  g_warning ("gtk_widget_event(): unhandled event type: %d", event->type);
	  signal_num = -1;
	  break;
	}
      if (signal_num != -1)
	g_signal_emit (widget, widget_signals[signal_num], 0, event, &return_val);
    }
  if (WIDGET_REALIZED_FOR_EVENT (widget, event))
    g_signal_emit (widget, widget_signals[EVENT_AFTER], 0, event);
  else
    return_val = TRUE;

  g_object_unref (widget);

  return return_val;
}

/**
 * gtk_widget_activate:
 * @widget: a #GtkWidget that's activatable
 * 
 * For widgets that can be "activated" (buttons, menu items, etc.)
 * this function activates them. Activation is what happens when you
 * press Enter on a widget during key navigation. If @widget isn't 
 * activatable, the function returns %FALSE.
 * 
 * Return value: %TRUE if the widget was activatable
 **/
gboolean
gtk_widget_activate (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  
  if (WIDGET_CLASS (widget)->activate_signal)
    {
      /* FIXME: we should eventually check the signals signature here */
      g_signal_emit (widget, WIDGET_CLASS (widget)->activate_signal, 0);

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_widget_set_scroll_adjustments:
 * @widget: a #GtkWidget
 * @hadjustment: an adjustment for horizontal scrolling, or %NULL
 * @vadjustment: an adjustment for vertical scrolling, or %NULL
 *
 * For widgets that support scrolling, sets the scroll adjustments and
 * returns %TRUE.  For widgets that don't support scrolling, does
 * nothing and returns %FALSE. Widgets that don't support scrolling
 * can be scrolled by placing them in a #GtkViewport, which does
 * support scrolling.
 * 
 * Return value: %TRUE if the widget supports scrolling
 **/
gboolean
gtk_widget_set_scroll_adjustments (GtkWidget     *widget,
				   GtkAdjustment *hadjustment,
				   GtkAdjustment *vadjustment)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  if (hadjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (hadjustment), FALSE);
  if (vadjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (vadjustment), FALSE);

  if (WIDGET_CLASS (widget)->set_scroll_adjustments_signal)
    {
      /* FIXME: we should eventually check the signals signature here */
      g_signal_emit (widget,
		     WIDGET_CLASS (widget)->set_scroll_adjustments_signal, 0,
		     hadjustment, vadjustment);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_widget_reparent_subwindows (GtkWidget *widget,
				GdkWindow *new_window)
{
  if (GTK_WIDGET_NO_WINDOW (widget))
    {
      GList *children = gdk_window_get_children (widget->window);
      GList *tmp_list;

      for (tmp_list = children; tmp_list; tmp_list = tmp_list->next)
	{
	  GtkWidget *child;
	  GdkWindow *window = tmp_list->data;

	  gdk_window_get_user_data (window, (void **)&child);
	  while (child && child != widget)
	    child = child->parent;

	  if (child)
	    gdk_window_reparent (window, new_window, 0, 0);
	}

      g_list_free (children);
    }
  else
   {
     GdkWindow *parent;
     GList *tmp_list, *children;

     parent = gdk_window_get_parent (widget->window);

     if (parent == NULL)
       gdk_window_reparent (widget->window, new_window, 0, 0);
     else
       {
	 children = gdk_window_get_children (parent);
	 
	 for (tmp_list = children; tmp_list; tmp_list = tmp_list->next)
	   {
	     GtkWidget *child;
	     GdkWindow *window = tmp_list->data;
	     
	     gdk_window_get_user_data (window, (void **)&child);
	     if (child == widget)
	       gdk_window_reparent (window, new_window, 0, 0);
	   }
	 
	 g_list_free (children);
       }
   }
}

static void
gtk_widget_reparent_fixup_child (GtkWidget *widget,
				 gpointer   client_data)
{
  g_assert (client_data != NULL);
  
  if (GTK_WIDGET_NO_WINDOW (widget))
    {
      if (widget->window)
	g_object_unref (widget->window);
      widget->window = (GdkWindow*) client_data;
      if (widget->window)
	g_object_ref (widget->window);

      if (GTK_IS_CONTAINER (widget))
        gtk_container_forall (GTK_CONTAINER (widget),
                              gtk_widget_reparent_fixup_child,
                              client_data);
    }
}

/**
 * gtk_widget_reparent:
 * @widget: a #GtkWidget
 * @new_parent: a #GtkContainer to move the widget into
 *
 * Moves a widget from one #GtkContainer to another, handling reference
 * count issues to avoid destroying the widget.
 * 
 **/
void
gtk_widget_reparent (GtkWidget *widget,
		     GtkWidget *new_parent)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_CONTAINER (new_parent));
  g_return_if_fail (widget->parent != NULL);
  
  if (widget->parent != new_parent)
    {
      /* First try to see if we can get away without unrealizing
       * the widget as we reparent it. if so we set a flag so
       * that gtk_widget_unparent doesn't unrealize widget
       */
      if (GTK_WIDGET_REALIZED (widget) && GTK_WIDGET_REALIZED (new_parent))
	GTK_PRIVATE_SET_FLAG (widget, GTK_IN_REPARENT);
      
      g_object_ref (widget);
      gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
      gtk_container_add (GTK_CONTAINER (new_parent), widget);
      g_object_unref (widget);
      
      if (GTK_WIDGET_IN_REPARENT (widget))
	{
	  GTK_PRIVATE_UNSET_FLAG (widget, GTK_IN_REPARENT);

	  gtk_widget_reparent_subwindows (widget, gtk_widget_get_parent_window (widget));
	  gtk_widget_reparent_fixup_child (widget,
					   gtk_widget_get_parent_window (widget));
	}

      g_object_notify (G_OBJECT (widget), "parent");
    }
}

/**
 * gtk_widget_intersect:
 * @widget: a #GtkWidget
 * @area: a rectangle
 * @intersection: rectangle to store intersection of @widget and @area
 * 
 * Computes the intersection of a @widget's area and @area, storing
 * the intersection in @intersection, and returns %TRUE if there was
 * an intersection.  @intersection may be %NULL if you're only
 * interested in whether there was an intersection.
 * 
 * Return value: %TRUE if there was an intersection
 **/
gboolean
gtk_widget_intersect (GtkWidget	   *widget,
		      GdkRectangle *area,
		      GdkRectangle *intersection)
{
  GdkRectangle *dest;
  GdkRectangle tmp;
  gint return_val;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (area != NULL, FALSE);
  
  if (intersection)
    dest = intersection;
  else
    dest = &tmp;
  
  return_val = gdk_rectangle_intersect (&widget->allocation, area, dest);
  
  if (return_val && intersection && !GTK_WIDGET_NO_WINDOW (widget))
    {
      intersection->x -= widget->allocation.x;
      intersection->y -= widget->allocation.y;
    }
  
  return return_val;
}

/**
 * gtk_widget_region_intersect:
 * @widget: a #GtkWidget
 * @region: a #GdkRegion, in the same coordinate system as 
 *          @widget->allocation. That is, relative to @widget->window
 *          for %NO_WINDOW widgets; relative to the parent window
 *          of @widget->window for widgets with their own window.
 * @returns: A newly allocated region holding the intersection of @widget
 *           and @region. The coordinates of the return value are
 *           relative to @widget->window for %NO_WINDOW widgets, and
 *           relative to the parent window of @widget->window for
 *           widgets with their own window.
 * 
 * Computes the intersection of a @widget's area and @region, returning
 * the intersection. The result may be empty, use gdk_region_empty() to
 * check.
 **/
GdkRegion *
gtk_widget_region_intersect (GtkWidget *widget,
			     GdkRegion *region)
{
  GdkRectangle rect;
  GdkRegion *dest;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (region != NULL, NULL);

  gtk_widget_get_draw_rectangle (widget, &rect);
  
  dest = gdk_region_rectangle (&rect);
 
  gdk_region_intersect (dest, region);

  return dest;
}

/**
 * _gtk_widget_grab_notify:
 * @widget: a #GtkWidget
 * @was_grabbed: whether a grab is now in effect
 * 
 * Emits the signal "grab_notify" on @widget.
 * 
 * Since: 2.6
 **/
void
_gtk_widget_grab_notify (GtkWidget *widget,
			 gboolean   was_grabbed)
{
  g_signal_emit (widget, widget_signals[GRAB_NOTIFY], 0, was_grabbed);
}

/**
 * gtk_widget_grab_focus:
 * @widget: a #GtkWidget
 * 
 * Causes @widget to have the keyboard focus for the #GtkWindow it's
 * inside. @widget must be a focusable widget, such as a #GtkEntry;
 * something like #GtkFrame won't work. (More precisely, it must have the
 * %GTK_CAN_FOCUS flag set.)
 * 
 **/
void
gtk_widget_grab_focus (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!GTK_WIDGET_IS_SENSITIVE (widget))
    return;
  
  g_object_ref (widget);
  g_signal_emit (widget, widget_signals[GRAB_FOCUS], 0);
  g_object_notify (G_OBJECT (widget), "has-focus");
  g_object_unref (widget);
}

static void
reset_focus_recurse (GtkWidget *widget,
		     gpointer   data)
{
  if (GTK_IS_CONTAINER (widget))
    {
      GtkContainer *container;

      container = GTK_CONTAINER (widget);
      gtk_container_set_focus_child (container, NULL);

      gtk_container_foreach (container,
			     reset_focus_recurse,
			     NULL);
    }
}

static void
gtk_widget_real_grab_focus (GtkWidget *focus_widget)
{
  if (GTK_WIDGET_CAN_FOCUS (focus_widget))
    {
      GtkWidget *toplevel;
      GtkWidget *widget;
      
      /* clear the current focus setting, break if the current widget
       * is the focus widget's parent, since containers above that will
       * be set by the next loop.
       */
      toplevel = gtk_widget_get_toplevel (focus_widget);
      if (GTK_WIDGET_TOPLEVEL (toplevel))
	{
	  widget = GTK_WINDOW (toplevel)->focus_widget;
	  
	  if (widget == focus_widget)
	    {
	      /* We call _gtk_window_internal_set_focus() here so that the
	       * toplevel window can request the focus if necessary.
	       * This is needed when the toplevel is a GtkPlug
	       */
	      if (!GTK_WIDGET_HAS_FOCUS (widget))
		_gtk_window_internal_set_focus (GTK_WINDOW (toplevel), focus_widget);

	      return;
	    }
	  
	  if (widget)
	    {
	      while (widget->parent && widget->parent != focus_widget->parent)
		{
		  widget = widget->parent;
		  gtk_container_set_focus_child (GTK_CONTAINER (widget), NULL);
		}
	    }
	}
      else if (toplevel != focus_widget)
	{
	  /* gtk_widget_grab_focus() operates on a tree without window...
	   * actually, this is very questionable behaviour.
	   */
	  
	  gtk_container_foreach (GTK_CONTAINER (toplevel),
				 reset_focus_recurse,
				 NULL);
	}

      /* now propagate the new focus up the widget tree and finally
       * set it on the window
       */
      widget = focus_widget;
      while (widget->parent)
	{
	  gtk_container_set_focus_child (GTK_CONTAINER (widget->parent), widget);
	  widget = widget->parent;
	}
      if (GTK_IS_WINDOW (widget))
	_gtk_window_internal_set_focus (GTK_WINDOW (widget), focus_widget);
    }
}

static gboolean
gtk_widget_real_show_help (GtkWidget        *widget,
                           GtkWidgetHelpType help_type)
{
  if (help_type == GTK_WIDGET_HELP_TOOLTIP)
    {
      _gtk_tooltips_toggle_keyboard_mode (widget);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_widget_real_focus (GtkWidget         *widget,
                       GtkDirectionType   direction)
{
  if (!GTK_WIDGET_CAN_FOCUS (widget))
    return FALSE;
  
  if (!gtk_widget_is_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_widget_is_focus:
 * @widget: a #GtkWidget
 * 
 * Determines if the widget is the focus widget within its
 * toplevel. (This does not mean that the %HAS_FOCUS flag is
 * necessarily set; %HAS_FOCUS will only be set if the
 * toplevel widget additionally has the global input focus.)
 * 
 * Return value: %TRUE if the widget is the focus widget.
 **/
gboolean
gtk_widget_is_focus (GtkWidget *widget)
{
  GtkWidget *toplevel;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  toplevel = gtk_widget_get_toplevel (widget);
  
  if (GTK_IS_WINDOW (toplevel))
    return widget == GTK_WINDOW (toplevel)->focus_widget;
  else
    return FALSE;
}

/**
 * gtk_widget_grab_default:
 * @widget: a #GtkWidget
 *
 * Causes @widget to become the default widget. @widget must have the
 * %GTK_CAN_DEFAULT flag set; typically you have to set this flag
 * yourself by calling <literal>GTK_WIDGET_SET_FLAGS (@widget,
 * GTK_CAN_DEFAULT)</literal>.  The default widget is activated when the user
 * presses Enter in a window.  Default widgets must be activatable,
 * that is, gtk_widget_activate() should affect them.
 * 
 **/
void
gtk_widget_grab_default (GtkWidget *widget)
{
  GtkWidget *window;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_CAN_DEFAULT (widget));
  
  window = gtk_widget_get_toplevel (widget);
  
  if (window && GTK_WIDGET_TOPLEVEL (window))
    gtk_window_set_default (GTK_WINDOW (window), widget);
  else
    g_warning (G_STRLOC ": widget not within a GtkWindow");
}

/**
 * gtk_widget_set_name:
 * @widget: a #GtkWidget
 * @name: name for the widget
 *
 * Widgets can be named, which allows you to refer to them from a
 * gtkrc file. You can apply a style to widgets with a particular name
 * in the gtkrc file. See the documentation for gtkrc files (on the
 * same page as the docs for #GtkRcStyle).
 * 
 * Note that widget names are separated by periods in paths (see 
 * gtk_widget_path()), so names with embedded periods may cause confusion.
 **/
void
gtk_widget_set_name (GtkWidget	 *widget,
		     const gchar *name)
{
  gchar *new_name;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));

  new_name = g_strdup (name);
  g_free (widget->name);
  widget->name = new_name;

  if (GTK_WIDGET_RC_STYLE (widget))
    gtk_widget_reset_rc_style (widget);

  g_object_notify (G_OBJECT (widget), "name");
}

/**
 * gtk_widget_get_name:
 * @widget: a #GtkWidget
 * 
 * Retrieves the name of a widget. See gtk_widget_set_name() for the
 * significance of widget names.
 * 
 * Return value: name of the widget. This string is owned by GTK+ and
 * should not be modified or freed
 **/
G_CONST_RETURN gchar*
gtk_widget_get_name (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  if (widget->name)
    return widget->name;
  return g_type_name (GTK_WIDGET_TYPE (widget));
}

/**
 * gtk_widget_set_state:
 * @widget: a #GtkWidget
 * @state: new state for @widget
 *
 * This function is for use in widget implementations. Sets the state
 * of a widget (insensitive, prelighted, etc.) Usually you should set
 * the state using wrapper functions such as gtk_widget_set_sensitive().
 * 
 **/
void
gtk_widget_set_state (GtkWidget           *widget,
		      GtkStateType         state)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (state == GTK_WIDGET_STATE (widget))
    return;

  if (state == GTK_STATE_INSENSITIVE)
    gtk_widget_set_sensitive (widget, FALSE);
  else
    {
      GtkStateData data;

      data.state = state;
      data.state_restoration = FALSE;
      data.use_forall = FALSE;
      if (widget->parent)
	data.parent_sensitive = (GTK_WIDGET_IS_SENSITIVE (widget->parent) != FALSE);
      else
	data.parent_sensitive = TRUE;

      gtk_widget_propagate_state (widget, &data);
  
      if (GTK_WIDGET_DRAWABLE (widget))
	gtk_widget_queue_draw (widget);
    }
}


/**
 * gtk_widget_set_app_paintable:
 * @widget: a #GtkWidget
 * @app_paintable: %TRUE if the application will paint on the widget
 *
 * Sets whether the application intends to draw on the widget in
 * an ::expose-event handler. 
 *
 * This is a hint to the widget and does not affect the behavior of 
 * the GTK+ core; many widgets ignore this flag entirely. For widgets 
 * that do pay attention to the flag, such as #GtkEventBox and #GtkWindow, 
 * the effect is to suppress default themed drawing of the widget's 
 * background. (Children of the widget will still be drawn.) The application 
 * is then entirely responsible for drawing the widget background.
 *
 * Note that the background is still drawn when the widget is mapped.
 * If this is not suitable (e.g. because you want to make a transparent
 * window using an RGBA visual), you can work around this by doing:
 * <informalexample><programlisting>
 *  gtk_widget_realize (window);
 *  gdk_window_set_back_pixmap (window->window, NULL, FALSE);
 *  gtk_widget_show (window);
 * </programlisting></informalexample> 
 **/
void
gtk_widget_set_app_paintable (GtkWidget *widget,
			      gboolean   app_paintable)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  app_paintable = (app_paintable != FALSE);

  if (GTK_WIDGET_APP_PAINTABLE (widget) != app_paintable)
    {
      if (app_paintable)
	GTK_WIDGET_SET_FLAGS (widget, GTK_APP_PAINTABLE);
      else
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_APP_PAINTABLE);

      if (GTK_WIDGET_DRAWABLE (widget))
	gtk_widget_queue_draw (widget);

      g_object_notify (G_OBJECT (widget), "app-paintable");
    }
}

/**
 * gtk_widget_set_double_buffered:
 * @widget: a #GtkWidget
 * @double_buffered: %TRUE to double-buffer a widget
 *
 * Widgets are double buffered by default; you can use this function
 * to turn off the buffering. "Double buffered" simply means that
 * gdk_window_begin_paint_region() and gdk_window_end_paint() are called
 * automatically around expose events sent to the
 * widget. gdk_window_begin_paint() diverts all drawing to a widget's
 * window to an offscreen buffer, and gdk_window_end_paint() draws the
 * buffer to the screen. The result is that users see the window
 * update in one smooth step, and don't see individual graphics
 * primitives being rendered.
 *
 * In very simple terms, double buffered widgets don't flicker,
 * so you would only use this function to turn off double buffering
 * if you had special needs and really knew what you were doing.
 * 
 * Note: if you turn off double-buffering, you have to handle
 * expose events, since even the clearing to the background color or 
 * pixmap will not happen automatically (as it is done in 
 * gdk_window_begin_paint()).
 * 
 **/
void
gtk_widget_set_double_buffered (GtkWidget *widget,
				gboolean   double_buffered)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (double_buffered)
    GTK_WIDGET_SET_FLAGS (widget, GTK_DOUBLE_BUFFERED);
  else
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_DOUBLE_BUFFERED);
}

/**
 * gtk_widget_set_redraw_on_allocate:
 * @widget: a #GtkWidget
 * @redraw_on_allocate: if %TRUE, the entire widget will be redrawn
 *   when it is allocated to a new size. Otherwise, only the
 *   new portion of the widget will be redrawn.
 *
 * Sets whether the entire widget is queued for drawing when its size 
 * allocation changes. By default, this setting is %TRUE and
 * the entire widget is redrawn on every size change. If your widget
 * leaves the upper left unchanged when made bigger, turning this
 * setting on will improve performance.

 * Note that for %NO_WINDOW widgets setting this flag to %FALSE turns
 * off all allocation on resizing: the widget will not even redraw if
 * its position changes; this is to allow containers that don't draw
 * anything to avoid excess invalidations. If you set this flag on a
 * %NO_WINDOW widget that <emphasis>does</emphasis> draw on @widget->window, 
 * you are responsible for invalidating both the old and new allocation 
 * of the widget when the widget is moved and responsible for invalidating
 * regions newly when the widget increases size.
 **/
void
gtk_widget_set_redraw_on_allocate (GtkWidget *widget,
				   gboolean   redraw_on_allocate)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (redraw_on_allocate)
    GTK_PRIVATE_SET_FLAG (widget, GTK_REDRAW_ON_ALLOC);
  else
    GTK_PRIVATE_UNSET_FLAG (widget, GTK_REDRAW_ON_ALLOC);
}

/**
 * gtk_widget_set_sensitive:
 * @widget: a #GtkWidget
 * @sensitive: %TRUE to make the widget sensitive
 *
 * Sets the sensitivity of a widget. A widget is sensitive if the user
 * can interact with it. Insensitive widgets are "grayed out" and the
 * user can't interact with them. Insensitive widgets are known as
 * "inactive", "disabled", or "ghosted" in some other toolkits.
 * 
 **/
void
gtk_widget_set_sensitive (GtkWidget *widget,
			  gboolean   sensitive)
{
  GtkStateData data;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  sensitive = (sensitive != FALSE);

  if (sensitive == (GTK_WIDGET_SENSITIVE (widget) != FALSE))
    return;

  if (sensitive)
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_SENSITIVE);
      data.state = GTK_WIDGET_SAVED_STATE (widget);
    }
  else
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_SENSITIVE);
      data.state = GTK_WIDGET_STATE (widget);
    }
  data.state_restoration = TRUE;
  data.use_forall = TRUE;

  if (widget->parent)
    data.parent_sensitive = (GTK_WIDGET_IS_SENSITIVE (widget->parent) != FALSE);
  else
    data.parent_sensitive = TRUE;

  gtk_widget_propagate_state (widget, &data);
  if (GTK_WIDGET_DRAWABLE (widget))
    gtk_widget_queue_draw (widget);

  g_object_notify (G_OBJECT (widget), "sensitive");
}

/**
 * gtk_widget_set_parent:
 * @widget: a #GtkWidget
 * @parent: parent container
 *
 * This function is useful only when implementing subclasses of #GtkContainer.
 * Sets the container as the parent of @widget, and takes care of
 * some details such as updating the state and style of the child
 * to reflect its new location. The opposite function is
 * gtk_widget_unparent().
 * 
 **/
void
gtk_widget_set_parent (GtkWidget *widget,
		       GtkWidget *parent)
{
  GtkStateData data;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (parent));
  g_return_if_fail (widget != parent);
  if (widget->parent != NULL)
    {
      g_warning ("Can't set a parent on widget which has a parent\n");
      return;
    }
  if (GTK_WIDGET_TOPLEVEL (widget))
    {
      g_warning ("Can't set a parent on a toplevel widget\n");
      return;
    }

  /* keep this function in sync with gtk_menu_attach_to_widget()
   */

  g_object_ref_sink (widget);
  widget->parent = parent;

  if (GTK_WIDGET_STATE (parent) != GTK_STATE_NORMAL)
    data.state = GTK_WIDGET_STATE (parent);
  else
    data.state = GTK_WIDGET_STATE (widget);
  data.state_restoration = FALSE;
  data.parent_sensitive = (GTK_WIDGET_IS_SENSITIVE (parent) != FALSE);
  data.use_forall = GTK_WIDGET_IS_SENSITIVE (parent) != GTK_WIDGET_IS_SENSITIVE (widget);

  gtk_widget_propagate_state (widget, &data);
  
  gtk_widget_reset_rc_styles (widget);

  g_signal_emit (widget, widget_signals[PARENT_SET], 0, NULL);
  if (GTK_WIDGET_ANCHORED (widget->parent))
    _gtk_widget_propagate_hierarchy_changed (widget, NULL);
  g_object_notify (G_OBJECT (widget), "parent");

  /* Enforce realized/mapped invariants
   */
  if (GTK_WIDGET_REALIZED (widget->parent))
    gtk_widget_realize (widget);

  if (GTK_WIDGET_VISIBLE (widget->parent) &&
      GTK_WIDGET_VISIBLE (widget))
    {
      if (GTK_WIDGET_CHILD_VISIBLE (widget) &&
	  GTK_WIDGET_MAPPED (widget->parent))
	gtk_widget_map (widget);

      gtk_widget_queue_resize (widget);
    }
}

/**
 * gtk_widget_get_parent:
 * @widget: a #GtkWidget
 *
 * Returns the parent container of @widget.
 *
 * Return value: the parent container of @widget, or %NULL
 **/
GtkWidget *
gtk_widget_get_parent (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return widget->parent;
}

/*****************************************
 * Widget styles
 * see docs/styles.txt
 *****************************************/

/**
 * gtk_widget_set_style:
 * @widget: a #GtkWidget
 * @style: a #GtkStyle, or %NULL to remove the effect of a previous
 *         gtk_widget_set_style() and go back to the default style
 *
 * Sets the #GtkStyle for a widget (@widget->style). You probably don't
 * want to use this function; it interacts badly with themes, because
 * themes work by replacing the #GtkStyle. Instead, use
 * gtk_widget_modify_style().
 * 
 **/
void
gtk_widget_set_style (GtkWidget *widget,
		      GtkStyle	*style)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (style)
    {
      gboolean initial_emission;

      initial_emission = !GTK_WIDGET_RC_STYLE (widget) && !GTK_WIDGET_USER_STYLE (widget);
      
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_RC_STYLE);
      GTK_PRIVATE_SET_FLAG (widget, GTK_USER_STYLE);
      
      gtk_widget_set_style_internal (widget, style, initial_emission);
    }
  else
    {
      if (GTK_WIDGET_USER_STYLE (widget))
	gtk_widget_reset_rc_style (widget);
    }
}

/**
 * gtk_widget_ensure_style:
 * @widget: a #GtkWidget
 *
 * Ensures that @widget has a style (@widget->style). Not a very useful
 * function; most of the time, if you want the style, the widget is
 * realized, and realized widgets are guaranteed to have a style
 * already.
 * 
 **/
void
gtk_widget_ensure_style (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!GTK_WIDGET_USER_STYLE (widget) &&
      !GTK_WIDGET_RC_STYLE (widget))
    gtk_widget_reset_rc_style (widget);
}

/* Look up the RC style for this widget, unsetting any user style that
 * may be in effect currently
 **/
static void
gtk_widget_reset_rc_style (GtkWidget *widget)
{
  GtkStyle *new_style = NULL;
  gboolean initial_emission;
  
  initial_emission = !GTK_WIDGET_RC_STYLE (widget) && !GTK_WIDGET_USER_STYLE (widget);

  GTK_PRIVATE_UNSET_FLAG (widget, GTK_USER_STYLE);
  GTK_WIDGET_SET_FLAGS (widget, GTK_RC_STYLE);
  
  if (gtk_widget_has_screen (widget))
    new_style = gtk_rc_get_style (widget);
  if (!new_style)
    new_style = gtk_widget_get_default_style ();

  if (initial_emission || new_style != widget->style)
    gtk_widget_set_style_internal (widget, new_style, initial_emission);
}

/**
 * gtk_widget_get_style:
 * @widget: a #GtkWidget
 * 
 * Simply an accessor function that returns @widget->style.
 * 
 * Return value: the widget's #GtkStyle
 **/
GtkStyle*
gtk_widget_get_style (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  return widget->style;
}

/**
 * gtk_widget_modify_style:
 * @widget: a #GtkWidget
 * @style: the #GtkRcStyle holding the style modifications
 * 
 * Modifies style values on the widget. Modifications made using this
 * technique take precedence over style values set via an RC file,
 * however, they will be overriden if a style is explicitely set on
 * the widget using gtk_widget_set_style(). The #GtkRcStyle structure
 * is designed so each field can either be set or unset, so it is
 * possible, using this function, to modify some style values and
 * leave the others unchanged.
 *
 * Note that modifications made with this function are not cumulative
 * with previous calls to gtk_widget_modify_style() or with such
 * functions as gtk_widget_modify_fg(). If you wish to retain
 * previous values, you must first call gtk_widget_get_modifier_style(),
 * make your modifications to the returned style, then call
 * gtk_widget_modify_style() with that style. On the other hand,
 * if you first call gtk_widget_modify_style(), subsequent calls
 * to such functions gtk_widget_modify_fg() will have a cumulative
 * effect with the initial modifications.
 **/
void       
gtk_widget_modify_style (GtkWidget      *widget,
			 GtkRcStyle     *style)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_RC_STYLE (style));
  
  g_object_set_qdata_full (G_OBJECT (widget),
			   quark_rc_style,
			   gtk_rc_style_copy (style),
			   (GDestroyNotify) gtk_rc_style_unref);

  /* note that "style" may be invalid here if it was the old
   * modifier style and the only reference was our own.
   */
  
  if (GTK_WIDGET_RC_STYLE (widget))
    gtk_widget_reset_rc_style (widget);
}

/**
 * gtk_widget_get_modifier_style:
 * @widget: a #GtkWidget
 * 
 * Returns the current modifier style for the widget. (As set by
 * gtk_widget_modify_style().) If no style has previously set, a new
 * #GtkRcStyle will be created with all values unset, and set as the
 * modifier style for the widget. If you make changes to this rc
 * style, you must call gtk_widget_modify_style(), passing in the
 * returned rc style, to make sure that your changes take effect.
 *
 * Caution: passing the style back to gtk_widget_modify_style() will
 * normally end up destroying it, because gtk_widget_modify_style() copies
 * the passed-in style and sets the copy as the new modifier style,
 * thus dropping any reference to the old modifier style. Add a reference
 * to the modifier style if you want to keep it alive.
 * 
 * Return value: the modifier style for the widget. This rc style is
 *   owned by the widget. If you want to keep a pointer to value this
 *   around, you must add a refcount using g_object_ref().
 **/
GtkRcStyle *
gtk_widget_get_modifier_style (GtkWidget      *widget)
{
  GtkRcStyle *rc_style;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  rc_style = g_object_get_qdata (G_OBJECT (widget), quark_rc_style);

  if (!rc_style)
    {
      rc_style = gtk_rc_style_new ();
      g_object_set_qdata_full (G_OBJECT (widget),
			       quark_rc_style,
			       rc_style,
			       (GDestroyNotify) gtk_rc_style_unref);
    }

  return rc_style;
}

static void
gtk_widget_modify_color_component (GtkWidget      *widget,
				   GtkRcFlags      component,
				   GtkStateType    state,
				   const GdkColor *color)
{
  GtkRcStyle *rc_style = gtk_widget_get_modifier_style (widget);  

  if (color)
    {
      switch (component)
	{
	case GTK_RC_FG:
	  rc_style->fg[state] = *color;
	  break;
	case GTK_RC_BG:
	  rc_style->bg[state] = *color;
	  break;
	case GTK_RC_TEXT:
	  rc_style->text[state] = *color;
	  break;
	case GTK_RC_BASE:
	  rc_style->base[state] = *color;
	  break;
	default:
	  g_assert_not_reached();
	}
      
      rc_style->color_flags[state] |= component;
    }
  else
    rc_style->color_flags[state] &= ~component;

  gtk_widget_modify_style (widget, rc_style);
}

/**
 * gtk_widget_modify_fg:
 * @widget: a #GtkWidget.
 * @state: the state for which to set the foreground color.
 * @color: the color to assign (does not need to be allocated),
 *         or %NULL to undo the effect of previous calls to
 *         of gtk_widget_modify_fg().
 * 
 * Sets the foreground color for a widget in a particular state.  All
 * other style values are left untouched. See also
 * gtk_widget_modify_style().
 **/
void
gtk_widget_modify_fg (GtkWidget      *widget,
		      GtkStateType    state,
		      const GdkColor *color)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);

  gtk_widget_modify_color_component (widget, GTK_RC_FG, state, color);
}

/**
 * gtk_widget_modify_bg:
 * @widget: a #GtkWidget.
 * @state: the state for which to set the background color.
 * @color: the color to assign (does not need to be allocated),
 *         or %NULL to undo the effect of previous calls to
 *         of gtk_widget_modify_bg().
 * 
 * Sets the background color for a widget in a particular state.  All
 * other style values are left untouched. See also
 * gtk_widget_modify_style(). 
 *
 * Note that "no window" widgets (which have the %GTK_NO_WINDOW flag set)
 * draw on their parent container's window and thus may not draw any background
 * themselves. This is the case for e.g. #GtkLabel. To modify the background
 * of such widgets, you have to set the background color on their parent; if you want 
 * to set the background of a rectangular area around a label, try placing the 
 * label in a #GtkEventBox widget and setting the background color on that.
 **/
void
gtk_widget_modify_bg (GtkWidget      *widget,
		      GtkStateType    state,
		      const GdkColor *color)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);

  gtk_widget_modify_color_component (widget, GTK_RC_BG, state, color);
}

/**
 * gtk_widget_modify_text:
 * @widget: a #GtkWidget.
 * @state: the state for which to set the text color.
 * @color: the color to assign (does not need to be allocated),
 *         or %NULL to undo the effect of previous calls to
 *         of gtk_widget_modify_text().
 * 
 * Sets the text color for a widget in a particular state.  All other
 * style values are left untouched. The text color is the foreground
 * color used along with the base color (see gtk_widget_modify_base())
 * for widgets such as #GtkEntry and #GtkTextView. See also
 * gtk_widget_modify_style().
 **/
void
gtk_widget_modify_text (GtkWidget      *widget,
			GtkStateType    state,
			const GdkColor *color)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);

  gtk_widget_modify_color_component (widget, GTK_RC_TEXT, state, color);
}

/**
 * gtk_widget_modify_base:
 * @widget: a #GtkWidget.
 * @state: the state for which to set the base color.
 * @color: the color to assign (does not need to be allocated),
 *         or %NULL to undo the effect of previous calls to
 *         of gtk_widget_modify_base().
 * 
 * Sets the base color for a widget in a particular state.
 * All other style values are left untouched. The base color
 * is the background color used along with the text color
 * (see gtk_widget_modify_text()) for widgets such as #GtkEntry
 * and #GtkTextView. See also gtk_widget_modify_style().
 *
 * Note that "no window" widgets (which have the %GTK_NO_WINDOW flag set)
 * draw on their parent container's window and thus may not draw any background
 * themselves. This is the case for e.g. #GtkLabel. To modify the background
 * of such widgets, you have to set the base color on their parent; if you want 
 * to set the background of a rectangular area around a label, try placing the 
 * label in a #GtkEventBox widget and setting the base color on that.
 **/
void
gtk_widget_modify_base (GtkWidget      *widget,
			GtkStateType    state,
			const GdkColor *color)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);

  gtk_widget_modify_color_component (widget, GTK_RC_BASE, state, color);
}

/**
 * gtk_widget_modify_font:
 * @widget: a #GtkWidget
 * @font_desc: the font description to use, or %NULL to undo
 *   the effect of previous calls to gtk_widget_modify_font().
 * 
 * Sets the font to use for a widget.  All other style values are left
 * untouched. See also gtk_widget_modify_style().
 **/
void
gtk_widget_modify_font (GtkWidget            *widget,
			PangoFontDescription *font_desc)
{
  GtkRcStyle *rc_style;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  rc_style = gtk_widget_get_modifier_style (widget);  

  if (rc_style->font_desc)
    pango_font_description_free (rc_style->font_desc);

  if (font_desc)
    rc_style->font_desc = pango_font_description_copy (font_desc);
  else
    rc_style->font_desc = NULL;
  
  gtk_widget_modify_style (widget, rc_style);
}

static void
gtk_widget_direction_changed (GtkWidget        *widget,
			      GtkTextDirection  previous_direction)
{
  gtk_widget_queue_resize (widget);
}

static void
gtk_widget_style_set (GtkWidget *widget,
		      GtkStyle  *previous_style)
{
  if (GTK_WIDGET_REALIZED (widget) &&
      !GTK_WIDGET_NO_WINDOW (widget))
    gtk_style_set_background (widget->style, widget->window, widget->state);
}

static void
gtk_widget_set_style_internal (GtkWidget *widget,
			       GtkStyle	 *style,
			       gboolean   initial_emission)
{
  g_object_ref (widget);
  g_object_freeze_notify (G_OBJECT (widget));

  if (widget->style != style)
    {
      GtkStyle *previous_style;

      if (GTK_WIDGET_REALIZED (widget))
	{
	  gtk_widget_reset_shapes (widget);
	  gtk_style_detach (widget->style);
	}
      
      previous_style = widget->style;
      widget->style = style;
      g_object_ref (widget->style);
      
      if (GTK_WIDGET_REALIZED (widget))
	widget->style = gtk_style_attach (widget->style, widget->window);

      gtk_widget_update_pango_context (widget);
      g_signal_emit (widget,
		     widget_signals[STYLE_SET],
		     0,
		     initial_emission ? NULL : previous_style);
      g_object_unref (previous_style);

      if (GTK_WIDGET_ANCHORED (widget) && !initial_emission)
	gtk_widget_queue_resize (widget);
    }
  else if (initial_emission)
    {
      gtk_widget_update_pango_context (widget);
      g_signal_emit (widget,
		     widget_signals[STYLE_SET],
		     0,
		     NULL);
    }
  g_object_notify (G_OBJECT (widget), "style");
  g_object_thaw_notify (G_OBJECT (widget));
  g_object_unref (widget);
}

typedef struct {
  GtkWidget *previous_toplevel;
  GdkScreen *previous_screen;
  GdkScreen *new_screen;
} HierarchyChangedInfo;

static void
do_screen_change (GtkWidget *widget,
		  GdkScreen *old_screen,
		  GdkScreen *new_screen)
{
  if (old_screen != new_screen)
    {
      if (old_screen)
	{
	  PangoContext *context = g_object_get_qdata (G_OBJECT (widget), quark_pango_context);
	  if (context)
	    g_object_set_qdata (G_OBJECT (widget), quark_pango_context, NULL);
	}
      
      g_signal_emit (widget, widget_signals[SCREEN_CHANGED], 0, old_screen);
    }
}

static void
gtk_widget_propagate_hierarchy_changed_recurse (GtkWidget *widget,
						gpointer   client_data)
{
  gboolean new_anchored;
  HierarchyChangedInfo *info = client_data;

  new_anchored = GTK_WIDGET_TOPLEVEL (widget) ||
                 (widget->parent && GTK_WIDGET_ANCHORED (widget->parent));

  if (GTK_WIDGET_ANCHORED (widget) != new_anchored)
    {
      g_object_ref (widget);
      
      if (new_anchored)
	GTK_PRIVATE_SET_FLAG (widget, GTK_ANCHORED);
      else
	GTK_PRIVATE_UNSET_FLAG (widget, GTK_ANCHORED);
      
      g_signal_emit (widget, widget_signals[HIERARCHY_CHANGED], 0, info->previous_toplevel);
      do_screen_change (widget, info->previous_screen, info->new_screen);
      
      if (GTK_IS_CONTAINER (widget))
	gtk_container_forall (GTK_CONTAINER (widget),
			      gtk_widget_propagate_hierarchy_changed_recurse,
			      client_data);
      
      g_object_unref (widget);
    }
}

/**
 * _gtk_widget_propagate_hierarchy_changed:
 * @widget: a #GtkWidget
 * @previous_toplevel: Previous toplevel
 * 
 * Propagates changes in the anchored state to a widget and all
 * children, unsetting or setting the %ANCHORED flag, and
 * emitting ::hierarchy_changed.
 **/
void
_gtk_widget_propagate_hierarchy_changed (GtkWidget    *widget,
					 GtkWidget    *previous_toplevel)
{
  HierarchyChangedInfo info;

  info.previous_toplevel = previous_toplevel;
  info.previous_screen = previous_toplevel ? gtk_widget_get_screen (previous_toplevel) : NULL;

  if (GTK_WIDGET_TOPLEVEL (widget) ||
      (widget->parent && GTK_WIDGET_ANCHORED (widget->parent)))
    info.new_screen = gtk_widget_get_screen (widget);
  else
    info.new_screen = NULL;

  if (info.previous_screen)
    g_object_ref (info.previous_screen);
  if (previous_toplevel)
    g_object_ref (previous_toplevel);

  gtk_widget_propagate_hierarchy_changed_recurse (widget, &info);

  if (previous_toplevel)
    g_object_unref (previous_toplevel);
  if (info.previous_screen)
    g_object_unref (info.previous_screen);
}

static void
gtk_widget_propagate_screen_changed_recurse (GtkWidget *widget,
					     gpointer   client_data)
{
  HierarchyChangedInfo *info = client_data;

  g_object_ref (widget);
  
  do_screen_change (widget, info->previous_screen, info->new_screen);
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  gtk_widget_propagate_screen_changed_recurse,
			  client_data);
  
  g_object_unref (widget);
}

/**
 * gtk_widget_is_composited:
 * @widget: a #GtkWidget
 * 
 * Whether @widget can rely on having its alpha channel
 * drawn correctly. On X11 this function returns whether a
 * compositing manager is running for @widget's screen
 * 
 * Return value: %TRUE if the widget can rely on its alpha
 * channel being drawn correctly.
 * 
 * Since: 2.10
 */
gboolean
gtk_widget_is_composited (GtkWidget *widget)
{
  GdkScreen *screen;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  
  screen = gtk_widget_get_screen (widget);
  
  return gdk_screen_is_composited (screen);
}

static void
propagate_composited_changed (GtkWidget *widget,
			      gpointer dummy)
{
  if (GTK_IS_CONTAINER (widget))
    {
      gtk_container_forall (GTK_CONTAINER (widget),
			    propagate_composited_changed,
			    NULL);
    }
  
  g_signal_emit (widget, widget_signals[COMPOSITED_CHANGED], 0);
}

void
_gtk_widget_propagate_composited_changed (GtkWidget *widget)
{
  propagate_composited_changed (widget, NULL);
}

/**
 * _gtk_widget_propagate_screen_changed:
 * @widget: a #GtkWidget
 * @previous_screen: Previous screen
 * 
 * Propagates changes in the screen for a widget to all
 * children, emitting ::screen_changed.
 **/
void
_gtk_widget_propagate_screen_changed (GtkWidget    *widget,
				      GdkScreen    *previous_screen)
{
  HierarchyChangedInfo info;

  info.previous_screen = previous_screen;
  info.new_screen = gtk_widget_get_screen (widget);

  if (previous_screen)
    g_object_ref (previous_screen);

  gtk_widget_propagate_screen_changed_recurse (widget, &info);

  if (previous_screen)
    g_object_unref (previous_screen);
}

static void
reset_rc_styles_recurse (GtkWidget *widget, gpointer data)
{
  if (GTK_WIDGET_RC_STYLE (widget))
    gtk_widget_reset_rc_style (widget);
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  reset_rc_styles_recurse,
			  NULL);
}

void
gtk_widget_reset_rc_styles (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  reset_rc_styles_recurse (widget, NULL);
}

/**
 * gtk_widget_get_default_style:
 * 
 * Returns the default style used by all widgets initially.
 * 
 * Returns: the default style. This #GtkStyle object is owned by GTK+ and
 * should not be modified or freed.
 */ 
GtkStyle*
gtk_widget_get_default_style (void)
{
  if (!gtk_default_style)
    {
      gtk_default_style = gtk_style_new ();
      g_object_ref (gtk_default_style);
    }
  
  return gtk_default_style;
}

static PangoContext *
gtk_widget_peek_pango_context (GtkWidget *widget)
{
  return g_object_get_qdata (G_OBJECT (widget), quark_pango_context);
}

/**
 * gtk_widget_get_pango_context:
 * @widget: a #GtkWidget
 * 
 * Gets a #PangoContext with the appropriate font map, font description,
 * and base direction for this widget. Unlike the context returned
 * by gtk_widget_create_pango_context(), this context is owned by
 * the widget (it can be used until the screen for the widget changes
 * or the widget is removed from its toplevel), and will be updated to
 * match any changes to the widget's attributes.
 *
 * If you create and keep a #PangoLayout using this context, you must
 * deal with changes to the context by calling pango_layout_context_changed()
 * on the layout in response to the ::style-set and ::direction-changed signals
 * for the widget.
 *
 * Return value: the #PangoContext for the widget.
 **/
PangoContext *
gtk_widget_get_pango_context (GtkWidget *widget)
{
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  context = g_object_get_qdata (G_OBJECT (widget), quark_pango_context);
  if (!context)
    {
      context = gtk_widget_create_pango_context (GTK_WIDGET (widget));
      g_object_set_qdata_full (G_OBJECT (widget),
			       quark_pango_context,
			       context,
			       g_object_unref);
    }

  return context;
}

static void
update_pango_context (GtkWidget    *widget,
		      PangoContext *context)
{
  pango_context_set_font_description (context, widget->style->font_desc);
  pango_context_set_base_dir (context,
			      gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR ?
			      PANGO_DIRECTION_LTR : PANGO_DIRECTION_RTL);
}

static void
gtk_widget_update_pango_context (GtkWidget *widget)
{
  PangoContext *context = gtk_widget_peek_pango_context (widget);
  
  if (context)
    {
      GdkScreen *screen;

      update_pango_context (widget, context);

      screen = gtk_widget_get_screen_unchecked (widget);
      if (screen)
	{
	  pango_cairo_context_set_resolution (context,
					      gdk_screen_get_resolution (screen));
	  pango_cairo_context_set_font_options (context,
						gdk_screen_get_font_options (screen));
	}
    }
}

/**
 * gtk_widget_create_pango_context:
 * @widget: a #GtkWidget
 * 
 * Creates a new #PangoContext with the appropriate font map,
 * font description, and base direction for drawing text for
 * this widget. See also gtk_widget_get_pango_context().
 * 
 * Return value: the new #PangoContext
 **/
PangoContext *
gtk_widget_create_pango_context (GtkWidget *widget)
{
  GdkScreen *screen;
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  screen = gtk_widget_get_screen_unchecked (widget);
  if (!screen)
    {
      GTK_NOTE (MULTIHEAD,
		g_warning ("gtk_widget_create_pango_context ()) called without screen"));

      screen = gdk_screen_get_default ();
    }

  context = gdk_pango_context_get_for_screen (screen);

  update_pango_context (widget, context);
  pango_context_set_language (context, gtk_get_default_language ());

  return context;
}

/**
 * gtk_widget_create_pango_layout:
 * @widget: a #GtkWidget
 * @text:   text to set on the layout (can be %NULL)
 * 
 * Creates a new #PangoLayout with the appropriate font map,
 * font description, and base direction for drawing text for
 * this widget.
 *
 * If you keep a #PangoLayout created in this way around, in order to
 * notify the layout of changes to the base direction or font of this
 * widget, you must call pango_layout_context_changed() in response to
 * the ::style-set and ::direction-changed signals for the widget.
 * 
 * Return value: the new #PangoLayout
 **/
PangoLayout *
gtk_widget_create_pango_layout (GtkWidget   *widget,
				const gchar *text)
{
  PangoLayout *layout;
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  context = gtk_widget_get_pango_context (widget);
  layout = pango_layout_new (context);

  if (text)
    pango_layout_set_text (layout, text, -1);

  return layout;
}

/**
 * gtk_widget_render_icon:
 * @widget: a #GtkWidget
 * @stock_id: a stock ID
 * @size: a stock size. A size of (GtkIconSize)-1 means render at 
 *     the size of the source and don't scale (if there are multiple 
 *     source sizes, GTK+ picks one of the available sizes).
 * @detail: render detail to pass to theme engine
 * 
 * A convenience function that uses the theme engine and RC file
 * settings for @widget to look up @stock_id and render it to
 * a pixbuf. @stock_id should be a stock icon ID such as
 * #GTK_STOCK_OPEN or #GTK_STOCK_OK. @size should be a size
 * such as #GTK_ICON_SIZE_MENU. @detail should be a string that
 * identifies the widget or code doing the rendering, so that
 * theme engines can special-case rendering for that widget or code.
 *
 * The pixels in the returned #GdkPixbuf are shared with the rest of
 * the application and should not be modified. The pixbuf should be freed
 * after use with g_object_unref().
 *
 * Return value: a new pixbuf, or %NULL if the stock ID wasn't known
 **/
GdkPixbuf*
gtk_widget_render_icon (GtkWidget      *widget,
                        const gchar    *stock_id,
                        GtkIconSize     size,
                        const gchar    *detail)
{
  GtkIconSet *icon_set;
  GdkPixbuf *retval;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);
  g_return_val_if_fail (size > GTK_ICON_SIZE_INVALID || size == -1, NULL);
  
  gtk_widget_ensure_style (widget);
  
  icon_set = gtk_style_lookup_icon_set (widget->style, stock_id);

  if (icon_set == NULL)
    return NULL;

  retval = gtk_icon_set_render_icon (icon_set,
                                     widget->style,
                                     gtk_widget_get_direction (widget),
                                     GTK_WIDGET_STATE (widget),
                                     size,
                                     widget,
                                     detail);

  return retval;
}

/**
 * gtk_widget_set_parent_window:
 * @widget: a #GtkWidget.
 * @parent_window: the new parent window.
 *  
 * Sets a non default parent window for @widget.
 **/
void
gtk_widget_set_parent_window   (GtkWidget           *widget,
				GdkWindow           *parent_window)
{
  GdkWindow *old_parent_window;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  old_parent_window = g_object_get_qdata (G_OBJECT (widget),
					  quark_parent_window);

  if (parent_window != old_parent_window)
    {
      g_object_set_qdata (G_OBJECT (widget), quark_parent_window, 
			  parent_window);
      if (old_parent_window)
	g_object_unref (old_parent_window);
      if (parent_window)
	g_object_ref (parent_window);
    }
}


/**
 * gtk_widget_set_child_visible:
 * @widget: a #GtkWidget
 * @is_visible: if %TRUE, @widget should be mapped along with its parent.
 *
 * Sets whether @widget should be mapped along with its when its parent
 * is mapped and @widget has been shown with gtk_widget_show(). 
 *
 * The child visibility can be set for widget before it is added to
 * a container with gtk_widget_set_parent(), to avoid mapping
 * children unnecessary before immediately unmapping them. However
 * it will be reset to its default state of %TRUE when the widget
 * is removed from a container.
 * 
 * Note that changing the child visibility of a widget does not
 * queue a resize on the widget. Most of the time, the size of
 * a widget is computed from all visible children, whether or
 * not they are mapped. If this is not the case, the container
 * can queue a resize itself.
 *
 * This function is only useful for container implementations and
 * never should be called by an application.
 **/
void
gtk_widget_set_child_visible (GtkWidget *widget,
			      gboolean   is_visible)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!GTK_WIDGET_TOPLEVEL (widget));

  g_object_ref (widget);

  if (is_visible)
    GTK_PRIVATE_SET_FLAG (widget, GTK_CHILD_VISIBLE);
  else
    {
      GtkWidget *toplevel;
      
      GTK_PRIVATE_UNSET_FLAG (widget, GTK_CHILD_VISIBLE);

      toplevel = gtk_widget_get_toplevel (widget);
      if (toplevel != widget && GTK_WIDGET_TOPLEVEL (toplevel))
	_gtk_window_unset_focus_and_default (GTK_WINDOW (toplevel), widget);
    }

  if (widget->parent && GTK_WIDGET_REALIZED (widget->parent))
    {
      if (GTK_WIDGET_MAPPED (widget->parent) &&
	  GTK_WIDGET_CHILD_VISIBLE (widget) &&
	  GTK_WIDGET_VISIBLE (widget))
	gtk_widget_map (widget);
      else
	gtk_widget_unmap (widget);
    }

  g_object_unref (widget);
}

/**
 * gtk_widget_get_child_visible:
 * @widget: a #GtkWidget
 * 
 * Gets the value set with gtk_widget_set_child_visible().
 * If you feel a need to use this function, your code probably
 * needs reorganization. 
 *
 * This function is only useful for container implementations and
 * never should be called by an application.
 *
 * Return value: %TRUE if the widget is mapped with the parent.
 **/
gboolean
gtk_widget_get_child_visible (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  
  return GTK_WIDGET_CHILD_VISIBLE (widget);
}

static GdkScreen *
gtk_widget_get_screen_unchecked (GtkWidget *widget)
{
  GtkWidget *toplevel;
  
  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_WIDGET_TOPLEVEL (toplevel))
    {
      if (GTK_IS_WINDOW (toplevel))
	return GTK_WINDOW (toplevel)->screen;
      else if (GTK_IS_INVISIBLE (toplevel))
	return GTK_INVISIBLE (widget)->screen;
    }

  return NULL;
}

/**
 * gtk_widget_get_screen:
 * @widget: a #GtkWidget
 * 
 * Get the #GdkScreen from the toplevel window associated with
 * this widget. This function can only be called after the widget
 * has been added to a widget hierarchy with a #GtkWindow
 * at the top.
 *
 * In general, you should only create screen specific
 * resources when a widget has been realized, and you should
 * free those resources when the widget is unrealized.
 * 
 * Return value: the #GdkScreen for the toplevel for this widget.
 *
 * Since: 2.2
 **/
GdkScreen*
gtk_widget_get_screen (GtkWidget *widget)
{
  GdkScreen *screen;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  screen = gtk_widget_get_screen_unchecked (widget);

  if (screen)
    return screen;
  else
    {
#if 0
      g_warning (G_STRLOC ": Can't get associated screen"
		 " for a widget unless it is inside a toplevel GtkWindow\n"
		 " widget type is %s associated top level type is %s",
		 g_type_name (G_OBJECT_TYPE(G_OBJECT (widget))),
		 g_type_name (G_OBJECT_TYPE(G_OBJECT (toplevel))));
#endif
      return gdk_screen_get_default ();
    }
}

/**
 * gtk_widget_has_screen:
 * @widget: a #GtkWidget
 * 
 * Checks whether there is a #GdkScreen is associated with
 * this widget. All toplevel widgets have an associated
 * screen, and all widgets added into a heirarchy with a toplevel
 * window at the top.
 * 
 * Return value: %TRUE if there is a #GdkScreen associcated
 *   with the widget.
 *
 * Since: 2.2
 **/
gboolean
gtk_widget_has_screen (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return (gtk_widget_get_screen_unchecked (widget) != NULL);
}

/**
 * gtk_widget_get_display:
 * @widget: a #GtkWidget
 * 
 * Get the #GdkDisplay for the toplevel window associated with
 * this widget. This function can only be called after the widget
 * has been added to a widget hierarchy with a #GtkWindow at the top.
 *
 * In general, you should only create display specific
 * resources when a widget has been realized, and you should
 * free those resources when the widget is unrealized.
 * 
 * Return value: the #GdkDisplay for the toplevel for this widget.
 *
 * Since: 2.2
 **/
GdkDisplay*
gtk_widget_get_display (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  return gdk_screen_get_display (gtk_widget_get_screen (widget));
}

/**
 * gtk_widget_get_root_window:
 * @widget: a #GtkWidget
 * 
 * Get the root window where this widget is located. This function can
 * only be called after the widget has been added to a widget
 * heirarchy with #GtkWindow at the top.
 *
 * The root window is useful for such purposes as creating a popup
 * #GdkWindow associated with the window. In general, you should only
 * create display specific resources when a widget has been realized,
 * and you should free those resources when the widget is unrealized.
 * 
 * Return value: the #GdkWindow root window for the toplevel for this widget.
 *
 * Since: 2.2
 **/
GdkWindow*
gtk_widget_get_root_window (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gdk_screen_get_root_window (gtk_widget_get_screen (widget));
}

/**
 * gtk_widget_get_parent_window:
 * @widget: a #GtkWidget.
 * @returns: the parent window of @widget.
 * 
 * Gets @widget's parent window.
 **/
GdkWindow *
gtk_widget_get_parent_window   (GtkWidget           *widget)
{
  GdkWindow *parent_window;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  parent_window = g_object_get_qdata (G_OBJECT (widget), quark_parent_window);

  return (parent_window != NULL) ? parent_window : 
	 (widget->parent != NULL) ? widget->parent->window : NULL;
}

/**
 * gtk_widget_child_focus:
 * @widget: a #GtkWidget
 * @direction: direction of focus movement
 *
 * This function is used by custom widget implementations; if you're
 * writing an app, you'd use gtk_widget_grab_focus() to move the focus
 * to a particular widget, and gtk_container_set_focus_chain() to
 * change the focus tab order. So you may want to investigate those
 * functions instead.
 * 
 * gtk_widget_child_focus() is called by containers as the user moves
 * around the window using keyboard shortcuts. @direction indicates
 * what kind of motion is taking place (up, down, left, right, tab
 * forward, tab backward).  gtk_widget_child_focus() invokes the
 * "focus" signal on #GtkWidget; widgets override the default handler
 * for this signal in order to implement appropriate focus behavior.
 *
 * The "focus" default handler for a widget should return %TRUE if
 * moving in @direction left the focus on a focusable location inside
 * that widget, and %FALSE if moving in @direction moved the focus
 * outside the widget. If returning %TRUE, widgets normally
 * call gtk_widget_grab_focus() to place the focus accordingly;
 * if returning %FALSE, they don't modify the current focus location.
 * 
 * This function replaces gtk_container_focus() from GTK+ 1.2.  It was
 * necessary to check that the child was visible, sensitive, and
 * focusable before calling
 * gtk_container_focus(). gtk_widget_child_focus() returns %FALSE if
 * the widget is not currently in a focusable state, so there's no
 * need for those checks.
 * 
 * Return value: %TRUE if focus ended up inside @widget
 **/
gboolean
gtk_widget_child_focus (GtkWidget       *widget,
                        GtkDirectionType direction)
{
  gboolean return_val;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (!GTK_WIDGET_VISIBLE (widget) ||
      !GTK_WIDGET_IS_SENSITIVE (widget))
    return FALSE;

  /* child widgets must set CAN_FOCUS, containers
   * don't have to though.
   */
  if (!GTK_IS_CONTAINER (widget) &&
      !GTK_WIDGET_CAN_FOCUS (widget))
    return FALSE;
  
  g_signal_emit (widget,
		 widget_signals[FOCUS],
		 0,
		 direction, &return_val);

  return return_val;
}

/**
 * gtk_widget_set_uposition:
 * @widget: a #GtkWidget
 * @x: x position; -1 to unset x; -2 to leave x unchanged
 * @y: y position; -1 to unset y; -2 to leave y unchanged
 * 
 *
 * Sets the position of a widget. The funny "u" in the name comes from
 * the "user position" hint specified by the X Window System, and
 * exists for legacy reasons. This function doesn't work if a widget
 * is inside a container; it's only really useful on #GtkWindow.
 *
 * Don't use this function to center dialogs over the main application
 * window; most window managers will do the centering on your behalf
 * if you call gtk_window_set_transient_for(), and it's really not
 * possible to get the centering to work correctly in all cases from
 * application code. But if you insist, use gtk_window_set_position()
 * to set #GTK_WIN_POS_CENTER_ON_PARENT, don't do the centering
 * manually.
 *
 * Note that although @x and @y can be individually unset, the position
 * is not honoured unless both @x and @y are set.
 **/
void
gtk_widget_set_uposition (GtkWidget *widget,
			  gint	     x,
			  gint	     y)
{
  /* FIXME this function is the only place that aux_info->x and
   * aux_info->y are even used I believe, and this function is
   * deprecated. Should be cleaned up.
   *
   * (Actually, size_allocate uses them) -Yosh
   */
  
  GtkWidgetAuxInfo *aux_info;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  aux_info =_gtk_widget_get_aux_info (widget, TRUE);
  
  if (x > -2)
    {
      if (x == -1)
	aux_info->x_set = FALSE;
      else
	{
	  aux_info->x_set = TRUE;
	  aux_info->x = x;
	}
    }

  if (y > -2)
    {
      if (y == -1)
	aux_info->y_set = FALSE;
      else
	{
	  aux_info->y_set = TRUE;
	  aux_info->y = y;
	}
    }

  if (GTK_IS_WINDOW (widget) && aux_info->x_set && aux_info->y_set)
    _gtk_window_reposition (GTK_WINDOW (widget), aux_info->x, aux_info->y);
  
  if (GTK_WIDGET_VISIBLE (widget) && widget->parent)
    gtk_widget_size_allocate (widget, &widget->allocation);
}

static void
gtk_widget_set_usize_internal (GtkWidget *widget,
			       gint       width,
			       gint       height)
{
  GtkWidgetAuxInfo *aux_info;
  gboolean changed = FALSE;
  
  g_object_freeze_notify (G_OBJECT (widget));

  aux_info = _gtk_widget_get_aux_info (widget, TRUE);
  
  if (width > -2 && aux_info->width != width)
    {
      g_object_notify (G_OBJECT (widget), "width-request");
      aux_info->width = width;
      changed = TRUE;
    }
  if (height > -2 && aux_info->height != height)
    {
      g_object_notify (G_OBJECT (widget), "height-request");  
      aux_info->height = height;
      changed = TRUE;
    }
  
  if (GTK_WIDGET_VISIBLE (widget) && changed)
    gtk_widget_queue_resize (widget);

  g_object_thaw_notify (G_OBJECT (widget));
}

/**
 * gtk_widget_set_usize:
 * @widget: a #GtkWidget
 * @width: minimum width, or -1 to unset
 * @height: minimum height, or -1 to unset
 *
 * Sets the minimum size of a widget; that is, the widget's size
 * request will be @width by @height. You can use this function to
 * force a widget to be either larger or smaller than it is. The
 * strange "usize" name dates from the early days of GTK+, and derives
 * from X Window System terminology. In many cases,
 * gtk_window_set_default_size() is a better choice for toplevel
 * windows than this function; setting the default size will still
 * allow users to shrink the window. Setting the usize will force them
 * to leave the window at least as large as the usize. When dealing
 * with window sizes, gtk_window_set_geometry_hints() can be a useful
 * function as well.
 * 
 * Note the inherent danger of setting any fixed size - themes,
 * translations into other languages, different fonts, and user action
 * can all change the appropriate size for a given widget. So, it's
 * basically impossible to hardcode a size that will always be
 * correct.
 * 
 * @Deprecated: Use gtk_widget_set_size_request() instead.
 **/
void
gtk_widget_set_usize (GtkWidget *widget,
		      gint	 width,
		      gint	 height)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  gtk_widget_set_usize_internal (widget, width, height);
}

/**
 * gtk_widget_set_size_request:
 * @widget: a #GtkWidget
 * @width: width @widget should request, or -1 to unset
 * @height: height @widget should request, or -1 to unset
 *
 * Sets the minimum size of a widget; that is, the widget's size
 * request will be @width by @height. You can use this function to
 * force a widget to be either larger or smaller than it normally
 * would be.
 *
 * In most cases, gtk_window_set_default_size() is a better choice for
 * toplevel windows than this function; setting the default size will
 * still allow users to shrink the window. Setting the size request
 * will force them to leave the window at least as large as the size
 * request. When dealing with window sizes,
 * gtk_window_set_geometry_hints() can be a useful function as well.
 * 
 * Note the inherent danger of setting any fixed size - themes,
 * translations into other languages, different fonts, and user action
 * can all change the appropriate size for a given widget. So, it's
 * basically impossible to hardcode a size that will always be
 * correct.
 *
 * The size request of a widget is the smallest size a widget can
 * accept while still functioning well and drawing itself correctly.
 * However in some strange cases a widget may be allocated less than
 * its requested size, and in many cases a widget may be allocated more
 * space than it requested.
 *
 * If the size request in a given direction is -1 (unset), then
 * the "natural" size request of the widget will be used instead.
 *
 * Widgets can't actually be allocated a size less than 1 by 1, but
 * you can pass 0,0 to this function to mean "as small as possible."
 **/
void
gtk_widget_set_size_request (GtkWidget *widget,
                             gint       width,
                             gint       height)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (width >= -1);
  g_return_if_fail (height >= -1);

  if (width == 0)
    width = 1;
  if (height == 0)
    height = 1;
  
  gtk_widget_set_usize_internal (widget, width, height);
}


/**
 * gtk_widget_get_size_request:
 * @widget: a #GtkWidget
 * @width: return location for width, or %NULL
 * @height: return location for height, or %NULL
 *
 * Gets the size request that was explicitly set for the widget using
 * gtk_widget_set_size_request().  A value of -1 stored in @width or
 * @height indicates that that dimension has not been set explicitly
 * and the natural requisition of the widget will be used intead. See
 * gtk_widget_set_size_request(). To get the size a widget will
 * actually use, call gtk_widget_size_request() instead of
 * this function.
 **/
void
gtk_widget_get_size_request (GtkWidget *widget,
                             gint      *width,
                             gint      *height)
{
  GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  aux_info = _gtk_widget_get_aux_info (widget, FALSE);

  if (width)
    *width = aux_info ? aux_info->width : -1;

  if (height)
    *height = aux_info ? aux_info->height : -1;
}

/**
 * gtk_widget_set_events:
 * @widget: a #GtkWidget
 * @events: event mask
 *
 * Sets the event mask (see #GdkEventMask) for a widget. The event
 * mask determines which events a widget will receive. Keep in mind
 * that different widgets have different default event masks, and by
 * changing the event mask you may disrupt a widget's functionality,
 * so be careful. This function must be called while a widget is
 * unrealized. Consider gtk_widget_add_events() for widgets that are
 * already realized, or if you want to preserve the existing event
 * mask. This function can't be used with #GTK_NO_WINDOW widgets;
 * to get events on those widgets, place them inside a #GtkEventBox
 * and receive events on the event box.
 **/
void
gtk_widget_set_events (GtkWidget *widget,
		       gint	  events)
{
  gint *eventp;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!GTK_WIDGET_REALIZED (widget));
  
  eventp = g_object_get_qdata (G_OBJECT (widget), quark_event_mask);
  
  if (events)
    {
      if (!eventp)
	eventp = g_slice_new (gint);
      
      *eventp = events;
      g_object_set_qdata (G_OBJECT (widget), quark_event_mask, eventp);
    }
  else if (eventp)
    {
      g_slice_free (gint, eventp);
      g_object_set_qdata (G_OBJECT (widget), quark_event_mask, NULL);
    }

  g_object_notify (G_OBJECT (widget), "events");
}

static void
gtk_widget_add_events_internal (GtkWidget *widget,
				gint       events,
				GList     *window_list)
{
  GList *l;

  for (l = window_list; l != NULL; l = l->next)
    {
      GdkWindow *window = l->data;
      gpointer user_data;

      gdk_window_get_user_data (window, &user_data);
      if (user_data == widget)
	{
	  GList *children;

	  gdk_window_set_events (window, gdk_window_get_events (window) | events);

	  children = gdk_window_get_children (window);
	  gtk_widget_add_events_internal (widget, events, children);
	  g_list_free (children);
	}
    }
}

/**
 * gtk_widget_add_events:
 * @widget: a #GtkWidget
 * @events: an event mask, see #GdkEventMask
 *
 * Adds the events in the bitfield @events to the event mask for
 * @widget. See gtk_widget_set_events() for details.
 * 
 **/
void
gtk_widget_add_events (GtkWidget *widget,
		       gint	  events)
{
  gint *eventp;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));

  eventp = g_object_get_qdata (G_OBJECT (widget), quark_event_mask);
  
  if (events)
    {
      if (!eventp)
	{
	  eventp = g_slice_new (gint);
	  *eventp = 0;
	}
      
      *eventp |= events;
      g_object_set_qdata (G_OBJECT (widget), quark_event_mask, eventp);
    }
  else if (eventp)
    {
      g_slice_free (gint, eventp);
      g_object_set_qdata (G_OBJECT (widget), quark_event_mask, NULL);
    }

  if (GTK_WIDGET_REALIZED (widget))
    {
      GList *window_list;

      if (GTK_WIDGET_NO_WINDOW (widget))
	window_list = gdk_window_get_children (widget->window);
      else
	window_list = g_list_prepend (NULL, widget->window);

      gtk_widget_add_events_internal (widget, events, window_list);

      g_list_free (window_list);
    }

  g_object_notify (G_OBJECT (widget), "events");
}

/**
 * gtk_widget_set_extension_events:
 * @widget: a #GtkWidget
 * @mode: bitfield of extension events to receive
 *
 * Sets the extension events mask to @mode. See #GdkExtensionMode
 * and gdk_input_set_extension_events().
 * 
 **/
void
gtk_widget_set_extension_events (GtkWidget *widget,
				 GdkExtensionMode mode)
{
  GdkExtensionMode *modep;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  modep = g_object_get_qdata (G_OBJECT (widget), quark_extension_event_mode);
  
  if (!modep)
    modep = g_slice_new (GdkExtensionMode);
  
  *modep = mode;
  g_object_set_qdata (G_OBJECT (widget), quark_extension_event_mode, modep);
  g_object_notify (G_OBJECT (widget), "extension-events");
}

/**
 * gtk_widget_get_toplevel:
 * @widget: a #GtkWidget
 * 
 * This function returns the topmost widget in the container hierarchy
 * @widget is a part of. If @widget has no parent widgets, it will be
 * returned as the topmost widget. No reference will be added to the
 * returned widget; it should not be unreferenced.
 *
 * Note the difference in behavior vs. gtk_widget_get_ancestor();
 * <literal>gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW)</literal> 
 * would return
 * %NULL if @widget wasn't inside a toplevel window, and if the
 * window was inside a #GtkWindow-derived widget which was in turn
 * inside the toplevel #GtkWindow. While the second case may
 * seem unlikely, it actually happens when a #GtkPlug is embedded
 * inside a #GtkSocket within the same application.
 * 
 * To reliably find the toplevel #GtkWindow, use
 * gtk_widget_get_toplevel() and check if the %TOPLEVEL flags
 * is set on the result.
 * <informalexample><programlisting>
 *  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
 *  if (GTK_WIDGET_TOPLEVEL (toplevel))
 *    {
 *      [ Perform action on toplevel. ]
 *    }
 * </programlisting></informalexample>
 *
 * Return value: the topmost ancestor of @widget, or @widget itself if there's no ancestor.
 **/
GtkWidget*
gtk_widget_get_toplevel (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  while (widget->parent)
    widget = widget->parent;
  
  return widget;
}

/**
 * gtk_widget_get_ancestor:
 * @widget: a #GtkWidget
 * @widget_type: ancestor type
 * 
 * Gets the first ancestor of @widget with type @widget_type. For example,
 * <literal>gtk_widget_get_ancestor (widget, GTK_TYPE_BOX)</literal> gets the 
 * first #GtkBox that's
 * an ancestor of @widget. No reference will be added to the returned widget;
 * it should not be unreferenced. See note about checking for a toplevel
 * #GtkWindow in the docs for gtk_widget_get_toplevel().
 * 
 * Note that unlike gtk_widget_is_ancestor(), gtk_widget_get_ancestor() 
 * considers @widget to be an ancestor of itself.
 *
 * Return value: the ancestor widget, or %NULL if not found
 **/
GtkWidget*
gtk_widget_get_ancestor (GtkWidget *widget,
			 GType      widget_type)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  while (widget && !g_type_is_a (GTK_WIDGET_TYPE (widget), widget_type))
    widget = widget->parent;
  
  if (!(widget && g_type_is_a (GTK_WIDGET_TYPE (widget), widget_type)))
    return NULL;
  
  return widget;
}

/**
 * gtk_widget_get_colormap:
 * @widget: a #GtkWidget
 * 
 * Gets the colormap that will be used to render @widget. No reference will
 * be added to the returned colormap; it should not be unreferenced.
 * 
 * Return value: the colormap used by @widget
 **/
GdkColormap*
gtk_widget_get_colormap (GtkWidget *widget)
{
  GdkColormap *colormap;
  GtkWidget *tmp_widget;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  if (widget->window)
    {
      colormap = gdk_drawable_get_colormap (widget->window);
      /* If window was destroyed previously, we'll get NULL here */
      if (colormap)
	return colormap;
    }

  tmp_widget = widget;
  while (tmp_widget)
    {
      colormap = g_object_get_qdata (G_OBJECT (tmp_widget), quark_colormap);
      if (colormap)
	return colormap;

      tmp_widget= tmp_widget->parent;
    }

  return gdk_screen_get_default_colormap (gtk_widget_get_screen (widget));
}

/**
 * gtk_widget_get_visual:
 * @widget: a #GtkWidget
 * 
 * Gets the visual that will be used to render @widget.
 * 
 * Return value: the visual for @widget
 **/
GdkVisual*
gtk_widget_get_visual (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gdk_colormap_get_visual (gtk_widget_get_colormap (widget));
}

/**
 * gtk_widget_get_settings:
 * @widget: a #GtkWidget
 * 
 * Gets the settings object holding the settings (global property
 * settings, RC file information, etc) used for this widget.
 *
 * Note that this function can only be called when the #GtkWidget
 * is attached to a toplevel, since the settings object is specific
 * to a particular #GdkScreen.
 * 
 * Return value: the relevant #GtkSettings object
 **/
GtkSettings*
gtk_widget_get_settings (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  return gtk_settings_get_for_screen (gtk_widget_get_screen (widget));
}

/**
 * gtk_widget_set_colormap:
 * @widget: a #GtkWidget
 * @colormap: a colormap
 *
 * Sets the colormap for the widget to the given value. Widget must not
 * have been previously realized. This probably should only be used
 * from an <function>init()</function> function (i.e. from the constructor 
 * for the widget).
 * 
 **/
void
gtk_widget_set_colormap (GtkWidget   *widget,
                         GdkColormap *colormap)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!GTK_WIDGET_REALIZED (widget));
  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  g_object_ref (colormap);
  
  g_object_set_qdata_full (G_OBJECT (widget), 
			   quark_colormap,
			   colormap,
			   g_object_unref);
}

/**
 * gtk_widget_get_events:
 * @widget: a #GtkWidget
 * 
 * Returns the event mask for the widget (a bitfield containing flags
 * from the #GdkEventMask enumeration). These are the events that the widget
 * will receive.
 * 
 * Return value: event mask for @widget
 **/
gint
gtk_widget_get_events (GtkWidget *widget)
{
  gint *events;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  
  events = g_object_get_qdata (G_OBJECT (widget), quark_event_mask);
  if (events)
    return *events;
  
  return 0;
}

/**
 * gtk_widget_get_extension_events:
 * @widget: a #GtkWidget
 * 
 * Retrieves the extension events the widget will receive; see
 * gdk_input_set_extension_events().
 * 
 * Return value: extension events for @widget
 **/
GdkExtensionMode
gtk_widget_get_extension_events (GtkWidget *widget)
{
  GdkExtensionMode *mode;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  
  mode = g_object_get_qdata (G_OBJECT (widget), quark_extension_event_mode);
  if (mode)
    return *mode;
  
  return 0;
}

/**
 * gtk_widget_get_pointer:
 * @widget: a #GtkWidget
 * @x: return location for the X coordinate, or %NULL
 * @y: return location for the Y coordinate, or %NULL
 *
 * Obtains the location of the mouse pointer in widget coordinates.
 * Widget coordinates are a bit odd; for historical reasons, they are
 * defined as @widget->window coordinates for widgets that are not
 * #GTK_NO_WINDOW widgets, and are relative to @widget->allocation.x,
 * @widget->allocation.y for widgets that are #GTK_NO_WINDOW widgets.
 * 
 **/
void
gtk_widget_get_pointer (GtkWidget *widget,
			gint	  *x,
			gint	  *y)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (x)
    *x = -1;
  if (y)
    *y = -1;
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_get_pointer (widget->window, x, y, NULL);
      
      if (GTK_WIDGET_NO_WINDOW (widget))
	{
	  if (x)
	    *x -= widget->allocation.x;
	  if (y)
	    *y -= widget->allocation.y;
	}
    }
}

/**
 * gtk_widget_is_ancestor:
 * @widget: a #GtkWidget
 * @ancestor: another #GtkWidget
 * 
 * Determines whether @widget is somewhere inside @ancestor, possibly with
 * intermediate containers.
 * 
 * Return value: %TRUE if @ancestor contains @widget as a child, grandchild, great grandchild, etc.
 **/
gboolean
gtk_widget_is_ancestor (GtkWidget *widget,
			GtkWidget *ancestor)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (ancestor != NULL, FALSE);
  
  while (widget)
    {
      if (widget->parent == ancestor)
	return TRUE;
      widget = widget->parent;
    }
  
  return FALSE;
}

static GQuark quark_composite_name = 0;

/**
 * gtk_widget_set_composite_name:
 * @widget: a #GtkWidget.
 * @name: the name to set.
 * 
 * Sets a widgets composite name. The widget must be
 * a composite child of its parent; see gtk_widget_push_composite_child().
 **/
void
gtk_widget_set_composite_name (GtkWidget   *widget,
			       const gchar *name)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_COMPOSITE_CHILD (widget));
  g_return_if_fail (name != NULL);

  if (!quark_composite_name)
    quark_composite_name = g_quark_from_static_string ("gtk-composite-name");

  g_object_set_qdata_full (G_OBJECT (widget),
			   quark_composite_name,
			   g_strdup (name),
			   g_free);
}

/**
 * gtk_widget_get_composite_name:
 * @widget: a #GtkWidget.
 * @returns: the composite name of @widget, or %NULL if @widget is not
 *   a composite child. The string should not be freed when it is no 
 *   longer needed.
 *
 * Obtains the composite name of a widget. 
 **/
gchar*
gtk_widget_get_composite_name (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (GTK_WIDGET_COMPOSITE_CHILD (widget) && widget->parent)
    return _gtk_container_child_composite_name (GTK_CONTAINER (widget->parent),
					       widget);
  else
    return NULL;
}

/**
 * gtk_widget_push_composite_child:
 * 
 * Makes all newly-created widgets as composite children until
 * the corresponding gtk_widget_pop_composite_child() call.
 * 
 * A composite child is a child that's an implementation detail of the
 * container it's inside and should not be visible to people using the
 * container. Composite children aren't treated differently by GTK (but
 * see gtk_container_foreach() vs. gtk_container_forall()), but e.g. GUI 
 * builders might want to treat them in a different way.
 * 
 * Here is a simple example:
 * <informalexample><programlisting>
 *   gtk_widget_push_composite_child (<!-- -->);
 *   scrolled_window->hscrollbar = gtk_hscrollbar_new (hadjustment);
 *   gtk_widget_set_composite_name (scrolled_window->hscrollbar, "hscrollbar");
 *   gtk_widget_pop_composite_child (<!-- -->);
 *   gtk_widget_set_parent (scrolled_window->hscrollbar, 
 *                          GTK_WIDGET (scrolled_window));
 *   g_object_ref (scrolled_window->hscrollbar);
 * </programlisting></informalexample>
 **/
void
gtk_widget_push_composite_child (void)
{
  composite_child_stack++;
}

/**
 * gtk_widget_pop_composite_child:
 *
 * Cancels the effect of a previous call to gtk_widget_push_composite_child().
 **/ 
void
gtk_widget_pop_composite_child (void)
{
  if (composite_child_stack)
    composite_child_stack--;
}

/**
 * gtk_widget_push_colormap:
 * @cmap: a #GdkColormap
 *
 * Pushes @cmap onto a global stack of colormaps; the topmost
 * colormap on the stack will be used to create all widgets.
 * Remove @cmap with gtk_widget_pop_colormap(). There's little
 * reason to use this function.
 * 
 **/
void
gtk_widget_push_colormap (GdkColormap *cmap)
{
  g_return_if_fail (!cmap || GDK_IS_COLORMAP (cmap));

  colormap_stack = g_slist_prepend (colormap_stack, cmap);
}

/**
 * gtk_widget_pop_colormap:
 *
 * Removes a colormap pushed with gtk_widget_push_colormap().
 * 
 **/
void
gtk_widget_pop_colormap (void)
{
  if (colormap_stack)
    colormap_stack = g_slist_delete_link (colormap_stack, colormap_stack);
}

/**
 * gtk_widget_set_default_colormap:
 * @colormap: a #GdkColormap
 * 
 * Sets the default colormap to use when creating widgets.
 * gtk_widget_push_colormap() is a better function to use if
 * you only want to affect a few widgets, rather than all widgets.
 **/
void
gtk_widget_set_default_colormap (GdkColormap *colormap)
{
  g_return_if_fail (GDK_IS_COLORMAP (colormap));
  
  gdk_screen_set_default_colormap (gdk_colormap_get_screen (colormap),
				   colormap);
}

/**
 * gtk_widget_get_default_colormap:
 * 
 * Obtains the default colormap used to create widgets.
 * 
 * Return value: default widget colormap
 **/
GdkColormap*
gtk_widget_get_default_colormap (void)
{
  return gdk_screen_get_default_colormap (gdk_screen_get_default ());
}

/**
 * gtk_widget_get_default_visual:
 * 
 * Obtains the visual of the default colormap. Not really useful;
 * used to be useful before gdk_colormap_get_visual() existed.
 * 
 * Return value: visual of the default colormap
 **/
GdkVisual*
gtk_widget_get_default_visual (void)
{
  return gdk_colormap_get_visual (gtk_widget_get_default_colormap ());
}

static void
gtk_widget_emit_direction_changed (GtkWidget        *widget,
				   GtkTextDirection  old_dir)
{
  gtk_widget_update_pango_context (widget);
  
  g_signal_emit (widget, widget_signals[DIRECTION_CHANGED], 0, old_dir);
}

/**
 * gtk_widget_set_direction:
 * @widget: a #GtkWidget
 * @dir:    the new direction
 * 
 * Sets the reading direction on a particular widget. This direction
 * controls the primary direction for widgets containing text,
 * and also the direction in which the children of a container are
 * packed. The ability to set the direction is present in order
 * so that correct localization into languages with right-to-left
 * reading directions can be done. Generally, applications will
 * let the default reading direction present, except for containers
 * where the containers are arranged in an order that is explicitely
 * visual rather than logical (such as buttons for text justification).
 *
 * If the direction is set to %GTK_TEXT_DIR_NONE, then the value
 * set by gtk_widget_set_default_direction() will be used.
 **/
void
gtk_widget_set_direction (GtkWidget        *widget,
			  GtkTextDirection  dir)
{
  GtkTextDirection old_dir;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (dir >= GTK_TEXT_DIR_NONE && dir <= GTK_TEXT_DIR_RTL);

  old_dir = gtk_widget_get_direction (widget);
  
  if (dir == GTK_TEXT_DIR_NONE)
    GTK_PRIVATE_UNSET_FLAG (widget, GTK_DIRECTION_SET);
  else
    {
      GTK_PRIVATE_SET_FLAG (widget, GTK_DIRECTION_SET);
      if (dir == GTK_TEXT_DIR_LTR)
	GTK_PRIVATE_SET_FLAG (widget, GTK_DIRECTION_LTR);
      else
	GTK_PRIVATE_UNSET_FLAG (widget, GTK_DIRECTION_LTR);
    }

  if (old_dir != gtk_widget_get_direction (widget))
    gtk_widget_emit_direction_changed (widget, old_dir);
}

/**
 * gtk_widget_get_direction:
 * @widget: a #GtkWidget
 * 
 * Gets the reading direction for a particular widget. See
 * gtk_widget_set_direction().
 * 
 * Return value: the reading direction for the widget.
 **/
GtkTextDirection
gtk_widget_get_direction (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_TEXT_DIR_LTR);
  
  if (GTK_WIDGET_DIRECTION_SET (widget))
    return GTK_WIDGET_DIRECTION_LTR (widget) ? GTK_TEXT_DIR_LTR : GTK_TEXT_DIR_RTL;
  else
    return gtk_default_direction;
}

static void
gtk_widget_set_default_direction_recurse (GtkWidget *widget, gpointer data)
{
  GtkTextDirection old_dir = GPOINTER_TO_UINT (data);

  g_object_ref (widget);
  
  if (!GTK_WIDGET_DIRECTION_SET (widget))
    gtk_widget_emit_direction_changed (widget, old_dir);
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  gtk_widget_set_default_direction_recurse,
			  data);

  g_object_unref (widget);
}

/**
 * gtk_widget_set_default_direction:
 * @dir: the new default direction. This cannot be
 *        %GTK_TEXT_DIR_NONE.
 * 
 * Sets the default reading direction for widgets where the
 * direction has not been explicitly set by gtk_widget_set_direction().
 **/
void
gtk_widget_set_default_direction (GtkTextDirection dir)
{
  g_return_if_fail (dir == GTK_TEXT_DIR_RTL || dir == GTK_TEXT_DIR_LTR);

  if (dir != gtk_default_direction)
    {
      GList *toplevels, *tmp_list;
      GtkTextDirection old_dir = gtk_default_direction;
      
      gtk_default_direction = dir;

      tmp_list = toplevels = gtk_window_list_toplevels ();
      g_list_foreach (toplevels, (GFunc)g_object_ref, NULL);
      
      while (tmp_list)
	{
	  gtk_widget_set_default_direction_recurse (tmp_list->data,
						    GUINT_TO_POINTER (old_dir));
	  g_object_unref (tmp_list->data);
	  tmp_list = tmp_list->next;
	}

      g_list_free (toplevels);
    }
}

/**
 * gtk_widget_get_default_direction:
 * 
 * Obtains the current default reading direction. See
 * gtk_widget_set_default_direction().
 *
 * Return value: the current default direction. 
 **/
GtkTextDirection
gtk_widget_get_default_direction (void)
{
  return gtk_default_direction;
}

static void
gtk_widget_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  if (widget->parent)
    gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
  else if (GTK_WIDGET_VISIBLE (widget))
    gtk_widget_hide (widget);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_VISIBLE);
  if (GTK_WIDGET_REALIZED (widget))
    gtk_widget_unrealize (widget);
  
  G_OBJECT_CLASS (gtk_widget_parent_class)->dispose (object);
}

static void
gtk_widget_real_destroy (GtkObject *object)
{
  /* gtk_object_destroy() will already hold a refcount on object */
  GtkWidget *widget = GTK_WIDGET (object);

  /* wipe accelerator closures (keep order) */
  g_object_set_qdata (G_OBJECT (widget), quark_accel_path, NULL);
  g_object_set_qdata (G_OBJECT (widget), quark_accel_closures, NULL);

  /* Callers of add_mnemonic_label() should disconnect on ::destroy */
  g_object_set_qdata (G_OBJECT (widget), quark_mnemonic_labels, NULL);
  
  gtk_grab_remove (widget);
  
  g_object_unref (widget->style);
  widget->style = gtk_widget_get_default_style ();
  g_object_ref (widget->style);

  GTK_OBJECT_CLASS (gtk_widget_parent_class)->destroy (object);
}

static void
gtk_widget_finalize (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetAuxInfo *aux_info;
  gint *events;
  GdkExtensionMode *mode;
  GtkAccessible *accessible;
  
  gtk_grab_remove (widget);

  g_object_unref (widget->style);
  widget->style = NULL;

  if (widget->name)
    g_free (widget->name);
  
  aux_info =_gtk_widget_get_aux_info (widget, FALSE);
  if (aux_info)
    gtk_widget_aux_info_destroy (aux_info);
  
  events = g_object_get_qdata (G_OBJECT (widget), quark_event_mask);
  if (events)
    g_slice_free (gint, events);
  
  mode = g_object_get_qdata (G_OBJECT (widget), quark_extension_event_mode);
  if (mode)
    g_slice_free (GdkExtensionMode, mode);

  accessible = g_object_get_qdata (G_OBJECT (widget), quark_accessible_object);
  if (accessible)
    g_object_unref (accessible);

  G_OBJECT_CLASS (gtk_widget_parent_class)->finalize (object);
}

/*****************************************
 * gtk_widget_real_map:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_real_map (GtkWidget *widget)
{
  g_assert (GTK_WIDGET_REALIZED (widget));
  
  if (!GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
      
      if (!GTK_WIDGET_NO_WINDOW (widget))
	gdk_window_show (widget->window);
    }
}

/*****************************************
 * gtk_widget_real_unmap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_real_unmap (GtkWidget *widget)
{
  if (GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

      if (!GTK_WIDGET_NO_WINDOW (widget))
	gdk_window_hide (widget->window);
    }
}

/*****************************************
 * gtk_widget_real_realize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_real_realize (GtkWidget *widget)
{
  g_assert (GTK_WIDGET_NO_WINDOW (widget));
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  if (widget->parent)
    {
      widget->window = gtk_widget_get_parent_window (widget);
      g_object_ref (widget->window);
    }
  widget->style = gtk_style_attach (widget->style, widget->window);
}

/*****************************************
 * gtk_widget_real_unrealize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_real_unrealize (GtkWidget *widget)
{
  if (GTK_WIDGET_MAPPED (widget))
    gtk_widget_real_unmap (widget);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

  /* printf ("unrealizing %s\n", g_type_name (G_TYPE_FROM_INSTANCE (widget)));
   */

   /* We must do unrealize child widget BEFORE container widget.
    * gdk_window_destroy() destroys specified xwindow and its sub-xwindows.
    * So, unrealizing container widget bofore its children causes the problem 
    * (for example, gdk_ic_destroy () with destroyed window causes crash. )
    */

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  (GtkCallback) gtk_widget_unrealize,
			  NULL);

  gtk_style_detach (widget->style);
  if (!GTK_WIDGET_NO_WINDOW (widget))
    {
      gdk_window_set_user_data (widget->window, NULL);
      gdk_window_destroy (widget->window);
      widget->window = NULL;
    }
  else
    {
      g_object_unref (widget->window);
      widget->window = NULL;
    }

  gtk_selection_remove_all (widget);
  
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED);
}

static void
gtk_widget_real_size_request (GtkWidget         *widget,
			      GtkRequisition    *requisition)
{
  requisition->width = widget->requisition.width;
  requisition->height = widget->requisition.height;
}

/**
 * _gtk_widget_peek_colormap:
 * 
 * Returns colormap currently pushed by gtk_widget_push_colormap, if any.
 * 
 * Return value: the currently pushed colormap, or %NULL if there is none.
 **/
GdkColormap*
_gtk_widget_peek_colormap (void)
{
  if (colormap_stack)
    return (GdkColormap*) colormap_stack->data;
  return NULL;
}

static void
gtk_widget_propagate_state (GtkWidget           *widget,
			    GtkStateData        *data)
{
  guint8 old_state = GTK_WIDGET_STATE (widget);
  guint8 old_saved_state = GTK_WIDGET_SAVED_STATE (widget);

  /* don't call this function with state==GTK_STATE_INSENSITIVE,
   * parent_sensitive==TRUE on a sensitive widget
   */


  if (data->parent_sensitive)
    GTK_WIDGET_SET_FLAGS (widget, GTK_PARENT_SENSITIVE);
  else
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_PARENT_SENSITIVE);

  if (GTK_WIDGET_IS_SENSITIVE (widget))
    {
      if (data->state_restoration)
        GTK_WIDGET_STATE (widget) = GTK_WIDGET_SAVED_STATE (widget);
      else
        GTK_WIDGET_STATE (widget) = data->state;
    }
  else
    {
      if (!data->state_restoration)
	{
	  if (data->state != GTK_STATE_INSENSITIVE)
	    GTK_WIDGET_SAVED_STATE (widget) = data->state;
	}
      else if (GTK_WIDGET_STATE (widget) != GTK_STATE_INSENSITIVE)
	GTK_WIDGET_SAVED_STATE (widget) = GTK_WIDGET_STATE (widget);
      GTK_WIDGET_STATE (widget) = GTK_STATE_INSENSITIVE;
    }

  if (gtk_widget_is_focus (widget) && !GTK_WIDGET_IS_SENSITIVE (widget))
    {
      GtkWidget *window;

      window = gtk_widget_get_toplevel (widget);
      if (window && GTK_WIDGET_TOPLEVEL (window))
	gtk_window_set_focus (GTK_WINDOW (window), NULL);
    }

  if (old_state != GTK_WIDGET_STATE (widget) ||
      old_saved_state != GTK_WIDGET_SAVED_STATE (widget))
    {
      g_object_ref (widget);

      if (!GTK_WIDGET_IS_SENSITIVE (widget) && GTK_WIDGET_HAS_GRAB (widget))
	gtk_grab_remove (widget);

      g_signal_emit (widget, widget_signals[STATE_CHANGED], 0, old_state);

      if (GTK_IS_CONTAINER (widget))
	{
	  data->parent_sensitive = (GTK_WIDGET_IS_SENSITIVE (widget) != FALSE);
	  if (data->use_forall)
	    gtk_container_forall (GTK_CONTAINER (widget),
				  (GtkCallback) gtk_widget_propagate_state,
				  data);
	  else
	    gtk_container_foreach (GTK_CONTAINER (widget),
				   (GtkCallback) gtk_widget_propagate_state,
				   data);
	}
      g_object_unref (widget);
    }
}

/**
 * _gtk_widget_get_aux_info:
 * @widget: a #GtkWidget
 * @create: if %TRUE, create the structure if it doesn't exist
 * 
 * Get the #GtkWidgetAuxInfo structure for the widget.
 * 
 * Return value: the #GtkAuxInfo structure for the widget, or
 *    %NULL if @create is %FALSE and one doesn't already exist.
 **/
GtkWidgetAuxInfo*
_gtk_widget_get_aux_info (GtkWidget *widget,
			  gboolean   create)
{
  GtkWidgetAuxInfo *aux_info;
  
  aux_info = g_object_get_qdata (G_OBJECT (widget), quark_aux_info);
  if (!aux_info && create)
    {
      aux_info = g_slice_new (GtkWidgetAuxInfo);

      aux_info->width = -1;
      aux_info->height = -1;
      aux_info->x = 0;
      aux_info->y = 0;
      aux_info->x_set = FALSE;
      aux_info->y_set = FALSE;
      g_object_set_qdata (G_OBJECT (widget), quark_aux_info, aux_info);
    }
  
  return aux_info;
}

/*****************************************
 * gtk_widget_aux_info_destroy:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_aux_info_destroy (GtkWidgetAuxInfo *aux_info)
{
  g_slice_free (GtkWidgetAuxInfo, aux_info);
}

static void
gtk_widget_shape_info_destroy (GtkWidgetShapeInfo *info)
{
  g_object_unref (info->shape_mask);
  g_slice_free (GtkWidgetShapeInfo, info);
}

/**
 * gtk_widget_shape_combine_mask: 
 * @widget: a #GtkWidget.
 * @shape_mask: shape to be added, or %NULL to remove an existing shape. 
 * @offset_x: X position of shape mask with respect to @window.
 * @offset_y: Y position of shape mask with respect to @window.
 * 
 * Sets a shape for this widget's GDK window. This allows for
 * transparent windows etc., see gdk_window_shape_combine_mask()
 * for more information.
 **/
void
gtk_widget_shape_combine_mask (GtkWidget *widget,
			       GdkBitmap *shape_mask,
			       gint	  offset_x,
			       gint	  offset_y)
{
  GtkWidgetShapeInfo* shape_info;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  /*  set_shape doesn't work on widgets without gdk window */
  g_return_if_fail (!GTK_WIDGET_NO_WINDOW (widget));

  if (!shape_mask)
    {
      GTK_PRIVATE_UNSET_FLAG (widget, GTK_HAS_SHAPE_MASK);
      
      if (widget->window)
	gdk_window_shape_combine_mask (widget->window, NULL, 0, 0);
      
      g_object_set_qdata (G_OBJECT (widget), quark_shape_info, NULL);
    }
  else
    {
      GTK_PRIVATE_SET_FLAG (widget, GTK_HAS_SHAPE_MASK);
      
      shape_info = g_slice_new (GtkWidgetShapeInfo);
      g_object_set_qdata_full (G_OBJECT (widget), quark_shape_info, shape_info,
			       (GDestroyNotify) gtk_widget_shape_info_destroy);
      
      shape_info->shape_mask = g_object_ref (shape_mask);
      shape_info->offset_x = offset_x;
      shape_info->offset_y = offset_y;
      
      /* set shape if widget has a gdk window already.
       * otherwise the shape is scheduled to be set by gtk_widget_realize().
       */
      if (widget->window)
	gdk_window_shape_combine_mask (widget->window, shape_mask,
				       offset_x, offset_y);
    }
}

/**
 * gtk_widget_input_shape_combine_mask: 
 * @widget: a #GtkWidget.
 * @shape_mask: shape to be added, or %NULL to remove an existing shape. 
 * @offset_x: X position of shape mask with respect to @window.
 * @offset_y: Y position of shape mask with respect to @window.
 * 
 * Sets an input shape for this widget's GDK window. This allows for
 * windows which react to mouse click in a nonrectangular region, see 
 * gdk_window_input_shape_combine_mask() for more information.
 *
 * Since: 2.10
 **/
void
gtk_widget_input_shape_combine_mask (GtkWidget *widget,
				     GdkBitmap *shape_mask,
				     gint       offset_x,
				     gint	offset_y)
{
  GtkWidgetShapeInfo* shape_info;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  /*  set_shape doesn't work on widgets without gdk window */
  g_return_if_fail (!GTK_WIDGET_NO_WINDOW (widget));

  if (!shape_mask)
    {
      if (widget->window)
	gdk_window_input_shape_combine_mask (widget->window, NULL, 0, 0);
      
      g_object_set_qdata (G_OBJECT (widget), quark_input_shape_info, NULL);
    }
  else
    {
      shape_info = g_slice_new (GtkWidgetShapeInfo);
      g_object_set_qdata_full (G_OBJECT (widget), quark_input_shape_info, 
			       shape_info,
			       (GDestroyNotify) gtk_widget_shape_info_destroy);
      
      shape_info->shape_mask = g_object_ref (shape_mask);
      shape_info->offset_x = offset_x;
      shape_info->offset_y = offset_y;
      
      /* set shape if widget has a gdk window already.
       * otherwise the shape is scheduled to be set by gtk_widget_realize().
       */
      if (widget->window)
	gdk_window_input_shape_combine_mask (widget->window, shape_mask,
					     offset_x, offset_y);
    }
}


static void
gtk_reset_shapes_recurse (GtkWidget *widget,
			  GdkWindow *window)
{
  gpointer data;
  GList *list;

  gdk_window_get_user_data (window, &data);
  if (data != widget)
    return;

  gdk_window_shape_combine_mask (window, NULL, 0, 0);
  for (list = gdk_window_peek_children (window); list; list = list->next)
    gtk_reset_shapes_recurse (widget, list->data);
}

/**
 * gtk_widget_reset_shapes:
 * @widget: a #GtkWidget.
 *
 * Recursively resets the shape on this widget and its descendants.
 **/
void
gtk_widget_reset_shapes (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_REALIZED (widget));

  if (!GTK_WIDGET_HAS_SHAPE_MASK (widget))
    gtk_reset_shapes_recurse (widget, widget->window);
}

/**
 * gtk_widget_ref:
 * @widget: a #GtkWidget
 * 
 * Adds a reference to a widget. This function is exactly the same
 * as calling g_object_ref(), and exists mostly for historical
 * reasons. It can still be convenient to avoid casting a widget
 * to a #GObject, it saves a small amount of typing.
 * 
 * Return value: the widget that was referenced
 **/
GtkWidget*
gtk_widget_ref (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return (GtkWidget*) g_object_ref ((GObject*) widget);
}

/**
 * gtk_widget_unref:
 * @widget: a #GtkWidget
 *
 * Inverse of gtk_widget_ref(). Equivalent to g_object_unref().
 * 
 **/
void
gtk_widget_unref (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_unref ((GObject*) widget);
}


/* style properties
 */

/**
 * gtk_widget_class_install_style_property_parser:
 * @klass: a #GtkWidgetClass
 * @pspec: the #GParamSpec for the style property
 * @parser: the parser for the style property
 * 
 * Installs a style property on a widget class. 
 **/
void
gtk_widget_class_install_style_property_parser (GtkWidgetClass     *klass,
						GParamSpec         *pspec,
						GtkRcPropertyParser parser)
{
  g_return_if_fail (GTK_IS_WIDGET_CLASS (klass));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (pspec->flags & G_PARAM_READABLE);
  g_return_if_fail (!(pspec->flags & (G_PARAM_CONSTRUCT_ONLY | G_PARAM_CONSTRUCT)));
  
  if (g_param_spec_pool_lookup (style_property_spec_pool, pspec->name, G_OBJECT_CLASS_TYPE (klass), FALSE))
    {
      g_warning (G_STRLOC ": class `%s' already contains a style property named `%s'",
		 G_OBJECT_CLASS_NAME (klass),
		 pspec->name);
      return;
    }

  g_param_spec_ref_sink (pspec);
  g_param_spec_set_qdata (pspec, quark_property_parser, (gpointer) parser);
  g_param_spec_pool_insert (style_property_spec_pool, pspec, G_OBJECT_CLASS_TYPE (klass));
}

/**
 * gtk_widget_class_install_style_property:
 * @klass: a #GtkWidgetClass
 * @pspec: the #GParamSpec for the property
 * 
 * Installs a style property on a widget class. The parser for the
 * style property is determined by the value type of @pspec.
 **/
void
gtk_widget_class_install_style_property (GtkWidgetClass *klass,
					 GParamSpec     *pspec)
{
  GtkRcPropertyParser parser;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (klass));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  parser = _gtk_rc_property_parser_from_type (G_PARAM_SPEC_VALUE_TYPE (pspec));

  gtk_widget_class_install_style_property_parser (klass, pspec, parser);
}

/**
 * gtk_widget_class_find_style_property:
 * @klass: a #GtkWidgetClass
 * @property_name: the name of the style property to find
 * @returns: the #GParamSpec of the style property or %NULL if @class has no
 *   style property with that name.
 *
 * Finds a style property of a widget class by name.
 *
 * Since: 2.2
 */
GParamSpec*
gtk_widget_class_find_style_property (GtkWidgetClass *klass,
				      const gchar    *property_name)
{
  g_return_val_if_fail (property_name != NULL, NULL);

  return g_param_spec_pool_lookup (style_property_spec_pool,
				   property_name,
				   G_OBJECT_CLASS_TYPE (klass),
				   TRUE);
}

/**
 * gtk_widget_class_list_style_properties:
 * @klass: a #GtkWidgetClass
 * @n_properties: location to return the number of style properties found
 * @returns: an newly allocated array of #GParamSpec*. The array must be freed with g_free().
 *
 * Returns all style properties of a widget class.
 *
 * Since: 2.2
 */
GParamSpec**
gtk_widget_class_list_style_properties (GtkWidgetClass *klass,
					guint          *n_properties)
{
  GParamSpec **pspecs;
  guint n;

  pspecs = g_param_spec_pool_list (style_property_spec_pool,
				   G_OBJECT_CLASS_TYPE (klass),
				   &n);
  if (n_properties)
    *n_properties = n;

  return pspecs;
}

/**
 * gtk_widget_style_get_property:
 * @widget: a #GtkWidget
 * @property_name: the name of a style property
 * @value: location to return the property value 
 *
 * Gets the value of a style property of @widget.
 */
void
gtk_widget_style_get_property (GtkWidget   *widget,
			       const gchar *property_name,
			       GValue      *value)
{
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  g_object_ref (widget);
  pspec = g_param_spec_pool_lookup (style_property_spec_pool,
				    property_name,
				    G_OBJECT_TYPE (widget),
				    TRUE);
  if (!pspec)
    g_warning ("%s: widget class `%s' has no property named `%s'",
	       G_STRLOC,
	       G_OBJECT_TYPE_NAME (widget),
	       property_name);
  else
    {
      const GValue *peek_value;

      peek_value = _gtk_style_peek_property_value (widget->style,
						   G_OBJECT_TYPE (widget),
						   pspec,
						   (GtkRcPropertyParser) g_param_spec_get_qdata (pspec, quark_property_parser));
      
      /* auto-conversion of the caller's value type
       */
      if (G_VALUE_TYPE (value) == G_PARAM_SPEC_VALUE_TYPE (pspec))
	g_value_copy (peek_value, value);
      else if (g_value_type_transformable (G_PARAM_SPEC_VALUE_TYPE (pspec), G_VALUE_TYPE (value)))
	g_value_transform (peek_value, value);
      else
	g_warning ("can't retrieve style property `%s' of type `%s' as value of type `%s'",
		   pspec->name,
		   g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
		   G_VALUE_TYPE_NAME (value));
    }
  g_object_unref (widget);
}

/**
 * gtk_widget_style_get_valist:
 * @widget: a #GtkWidget
 * @first_property_name: the name of the first property to get
 * @var_args: a <type>va_list</type> of pairs of property names and
 *     locations to return the property values, starting with the location
 *     for @first_property_name.
 * 
 * Non-vararg variant of gtk_widget_style_get(). Used primarily by language 
 * bindings.
 */ 
void
gtk_widget_style_get_valist (GtkWidget   *widget,
			     const gchar *first_property_name,
			     va_list      var_args)
{
  const gchar *name;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_ref (widget);

  name = first_property_name;
  while (name)
    {
      const GValue *peek_value;
      GParamSpec *pspec;
      gchar *error;

      pspec = g_param_spec_pool_lookup (style_property_spec_pool,
					name,
					G_OBJECT_TYPE (widget),
					TRUE);
      if (!pspec)
	{
	  g_warning ("%s: widget class `%s' has no property named `%s'",
		     G_STRLOC,
		     G_OBJECT_TYPE_NAME (widget),
		     name);
	  break;
	}
      /* style pspecs are always readable so we can spare that check here */

      peek_value = _gtk_style_peek_property_value (widget->style,
						   G_OBJECT_TYPE (widget),
						   pspec,
						   (GtkRcPropertyParser) g_param_spec_get_qdata (pspec, quark_property_parser));
      G_VALUE_LCOPY (peek_value, var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);
	  break;
	}

      name = va_arg (var_args, gchar*);
    }

  g_object_unref (widget);
}

/**
 * gtk_widget_style_get:
 * @widget: a #GtkWidget
 * @first_property_name: the name of the first property to get
 * @Varargs: pairs of property names and locations to 
 *   return the property values, starting with the location for 
 *   @first_property_name, terminated by %NULL.
 *
 * Gets the values of a multiple style properties of @widget.
 */
void
gtk_widget_style_get (GtkWidget   *widget,
		      const gchar *first_property_name,
		      ...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  va_start (var_args, first_property_name);
  gtk_widget_style_get_valist (widget, first_property_name, var_args);
  va_end (var_args);
}

/**
 * gtk_widget_path:
 * @widget: a #GtkWidget
 * @path_length: location to store length of the path, or %NULL
 * @path: location to store allocated path string, or %NULL 
 * @path_reversed: location to store allocated reverse path string, or %NULL
 *
 * Obtains the full path to @widget. The path is simply the name of a
 * widget and all its parents in the container hierarchy, separated by
 * periods. The name of a widget comes from
 * gtk_widget_get_name(). Paths are used to apply styles to a widget
 * in gtkrc configuration files.  Widget names are the type of the
 * widget by default (e.g. "GtkButton") or can be set to an
 * application-specific value with gtk_widget_set_name().  By setting
 * the name of a widget, you allow users or theme authors to apply
 * styles to that specific widget in their gtkrc
 * file. @path_reversed_p fills in the path in reverse order,
 * i.e. starting with @widget's name instead of starting with the name
 * of @widget's outermost ancestor.
 * 
 **/
void
gtk_widget_path (GtkWidget *widget,
		 guint     *path_length,
		 gchar    **path,
		 gchar    **path_reversed)
{
  static gchar *rev_path = NULL;
  static guint tmp_path_len = 0;
  guint len;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  len = 0;
  do
    {
      const gchar *string;
      const gchar *s;
      gchar *d;
      guint l;
      
      string = gtk_widget_get_name (widget);
      l = strlen (string);
      while (tmp_path_len <= len + l + 1)
	{
	  tmp_path_len += INIT_PATH_SIZE;
	  rev_path = g_realloc (rev_path, tmp_path_len);
	}
      s = string + l - 1;
      d = rev_path + len;
      while (s >= string)
	*(d++) = *(s--);
      len += l;
      
      widget = widget->parent;
      
      if (widget)
	rev_path[len++] = '.';
      else
	rev_path[len++] = 0;
    }
  while (widget);
  
  if (path_length)
    *path_length = len - 1;
  if (path_reversed)
    *path_reversed = g_strdup (rev_path);
  if (path)
    {
      *path = g_strdup (rev_path);
      g_strreverse (*path);
    }
}

/**
 * gtk_widget_class_path:
 * @widget: a #GtkWidget
 * @path_length: location to store the length of the class path, or %NULL
 * @path: location to store the class path as an allocated string, or %NULL
 * @path_reversed: location to store the reverse class path as an allocated string, or %NULL
 *
 * Same as gtk_widget_path(), but always uses the name of a widget's type,
 * never uses a custom name set with gtk_widget_set_name().
 * 
 **/
void
gtk_widget_class_path (GtkWidget *widget,
		       guint     *path_length,
		       gchar    **path,
		       gchar    **path_reversed)
{
  static gchar *rev_path = NULL;
  static guint tmp_path_len = 0;
  guint len;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  len = 0;
  do
    {
      const gchar *string;
      const gchar *s;
      gchar *d;
      guint l;
      
      string = g_type_name (GTK_WIDGET_TYPE (widget));
      l = strlen (string);
      while (tmp_path_len <= len + l + 1)
	{
	  tmp_path_len += INIT_PATH_SIZE;
	  rev_path = g_realloc (rev_path, tmp_path_len);
	}
      s = string + l - 1;
      d = rev_path + len;
      while (s >= string)
	*(d++) = *(s--);
      len += l;
      
      widget = widget->parent;
      
      if (widget)
	rev_path[len++] = '.';
      else
	rev_path[len++] = 0;
    }
  while (widget);
  
  if (path_length)
    *path_length = len - 1;
  if (path_reversed)
    *path_reversed = g_strdup (rev_path);
  if (path)
    {
      *path = g_strdup (rev_path);
      g_strreverse (*path);
    }
}

/**
 * gtk_requisition_copy:
 * @requisition: a #GtkRequisition.
 * @returns: a copy of @requisition.
 *
 * Copies a #GtkRequisition.
 **/
GtkRequisition *
gtk_requisition_copy (const GtkRequisition *requisition)
{
  return (GtkRequisition *)g_memdup (requisition, sizeof (GtkRequisition));
}

/**
 * gtk_requisition_free:
 * @requisition: a #GtkRequisition.
 * 
 * Frees a #GtkRequisition.
 **/
void
gtk_requisition_free (GtkRequisition *requisition)
{
  g_free (requisition);
}

GType
gtk_requisition_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static (I_("GtkRequisition"),
					     (GBoxedCopyFunc) gtk_requisition_copy,
					     (GBoxedFreeFunc) gtk_requisition_free);

  return our_type;
}

/**
 * gtk_widget_get_accessible:
 * @widget: a #GtkWidget
 *
 * Returns the accessible object that describes the widget to an
 * assistive technology. 
 * 
 * If no accessibility library is loaded (i.e. no ATK implementation library is 
 * loaded via <envar>GTK_MODULES</envar> or via another application library, 
 * such as libgnome), then this #AtkObject instance may be a no-op. Likewise, 
 * if no class-specific #AtkObject implementation is available for the widget 
 * instance in question, it will inherit an #AtkObject implementation from the 
 * first ancestor class for which such an implementation is defined.
 *
 * The documentation of the <ulink url="http://developer.gnome.org/doc/API/2.0/atk/index.html">ATK</ulink>
 * library contains more information about accessible objects and their uses.
 * 
 * Returns: the #AtkObject associated with @widget
 */
AtkObject* 
gtk_widget_get_accessible (GtkWidget *widget)
{
  GtkWidgetClass *klass;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  klass = GTK_WIDGET_GET_CLASS (widget);

  g_return_val_if_fail (klass->get_accessible != NULL, NULL);

  return klass->get_accessible (widget);
}

static AtkObject* 
gtk_widget_real_get_accessible (GtkWidget *widget)
{
  AtkObject* accessible;

  accessible = g_object_get_qdata (G_OBJECT (widget), 
                                   quark_accessible_object);
  if (!accessible)
  {
    AtkObjectFactory *factory;
    AtkRegistry *default_registry;

    default_registry = atk_get_default_registry ();
    factory = atk_registry_get_factory (default_registry, 
                                        G_TYPE_FROM_INSTANCE (widget));
    accessible =
      atk_object_factory_create_accessible (factory,
					    G_OBJECT (widget));
    g_object_set_qdata (G_OBJECT (widget), 
                        quark_accessible_object,
                        accessible);
  }
  return accessible;
}

/*
 * Initialize a AtkImplementorIface instance's virtual pointers as
 * appropriate to this implementor's class (GtkWidget).
 */
static void
gtk_widget_accessible_interface_init (AtkImplementorIface *iface)
{
  iface->ref_accessible = gtk_widget_ref_accessible;
}

static AtkObject*
gtk_widget_ref_accessible (AtkImplementor *implementor)
{
  AtkObject *accessible;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (implementor));
  if (accessible)
    g_object_ref (accessible);
  return accessible;
}

/**
 * gtk_widget_get_clipboard:
 * @widget: a #GtkWidget
 * @selection: a #GdkAtom which identifies the clipboard
 *             to use. %GDK_SELECTION_CLIPBOARD gives the
 *             default clipboard. Another common value
 *             is %GDK_SELECTION_PRIMARY, which gives
 *             the primary X selection. 
 * 
 * Returns the clipboard object for the given selection to
 * be used with @widget. @widget must have a #GdkDisplay
 * associated with it, so must be attached to a toplevel
 * window.
 * 
 * Return value: the appropriate clipboard object. If no
 *             clipboard already exists, a new one will
 *             be created. Once a clipboard object has
 *             been created, it is persistent for all time.
 *
 * Since: 2.2
 **/
GtkClipboard *
gtk_widget_get_clipboard (GtkWidget *widget, GdkAtom selection)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (gtk_widget_has_screen (widget), NULL);
  
  return gtk_clipboard_get_for_display (gtk_widget_get_display (widget),
					selection);
}

/**
 * gtk_widget_list_mnemonic_labels:
 * @widget: a #GtkWidget
 * 
 * Returns a newly allocated list of the widgets, normally labels, for 
 * which this widget is a the target of a mnemonic (see for example, 
 * gtk_label_set_mnemonic_widget()).

 * The widgets in the list are not individually referenced. If you
 * want to iterate through the list and perform actions involving
 * callbacks that might destroy the widgets, you
 * <emphasis>must</emphasis> call <literal>g_list_foreach (result,
 * (GFunc)g_object_ref, NULL)</literal> first, and then unref all the
 * widgets afterwards.

 * Return value: the list of mnemonic labels; free this list
 *  with g_list_free() when you are done with it.
 *
 * Since: 2.4
 **/
GList *
gtk_widget_list_mnemonic_labels (GtkWidget *widget)
{
  GList *list = NULL;
  GSList *l;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  for (l = g_object_get_qdata (G_OBJECT (widget), quark_mnemonic_labels); l; l = l->next)
    list = g_list_prepend (list, l->data);

  return list;
}

/**
 * gtk_widget_add_mnemonic_label:
 * @widget: a #GtkWidget
 * @label: a #GtkWidget that acts as a mnemonic label for @widget.
 * 
 * Adds a widget to the list of mnemonic labels for
 * this widget. (See gtk_widget_list_mnemonic_labels()). Note the
 * list of mnemonic labels for the widget is cleared when the
 * widget is destroyed, so the caller must make sure to update
 * its internal state at this point as well, by using a connection
 * to the ::destroy signal or a weak notifier.
 *
 * Since: 2.4
 **/
void
gtk_widget_add_mnemonic_label (GtkWidget *widget,
                               GtkWidget *label)
{
  GSList *old_list, *new_list;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (label));

  old_list = g_object_steal_qdata (G_OBJECT (widget), quark_mnemonic_labels);
  new_list = g_slist_prepend (old_list, label);
  
  g_object_set_qdata_full (G_OBJECT (widget), quark_mnemonic_labels,
			   new_list, (GDestroyNotify) g_slist_free);
}

/**
 * gtk_widget_remove_mnemonic_label:
 * @widget: a #GtkWidget
 * @label: a #GtkWidget that was previously set as a mnemnic label for
 *         @widget with gtk_widget_add_mnemonic_label().
 * 
 * Removes a widget from the list of mnemonic labels for
 * this widget. (See gtk_widget_list_mnemonic_labels()). The widget
 * must have previously been added to the list with
 * gtk_widget_add_mnemonic_label().
 *
 * Since: 2.4
 **/
void
gtk_widget_remove_mnemonic_label (GtkWidget *widget,
                                  GtkWidget *label)
{
  GSList *old_list, *new_list;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (label));

  old_list = g_object_steal_qdata (G_OBJECT (widget), quark_mnemonic_labels);
  new_list = g_slist_remove (old_list, label);

  if (new_list)
    g_object_set_qdata_full (G_OBJECT (widget), quark_mnemonic_labels,
			     new_list, (GDestroyNotify) g_slist_free);
}

/**
 * gtk_widget_get_no_show_all:
 * @widget: a #GtkWidget
 * 
 * Returns the current value of the "no_show_all" property, which determines
 * whether calls to gtk_widget_show_all() and gtk_widget_hide_all() 
 * will affect this widget. 
 * 
 * Return value: the current value of the "no_show_all" property.
 *
 * Since: 2.4
 **/
gboolean
gtk_widget_get_no_show_all (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  
  return (GTK_WIDGET_FLAGS (widget) & GTK_NO_SHOW_ALL) != 0;
}

/**
 * gtk_widget_set_no_show_all:
 * @widget: a #GtkWidget
 * @no_show_all: the new value for the "no_show_all" property
 * 
 * Sets the "no_show_all" property, which determines whether calls to 
 * gtk_widget_show_all() and gtk_widget_hide_all() will affect this widget. 
 *
 * This is mostly for use in constructing widget hierarchies with externally
 * controlled visibility, see #GtkUIManager.
 * 
 * Since: 2.4
 **/
void
gtk_widget_set_no_show_all (GtkWidget *widget,
			    gboolean   no_show_all)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  no_show_all = (no_show_all != FALSE);

  if (no_show_all == ((GTK_WIDGET_FLAGS (widget) & GTK_NO_SHOW_ALL) != 0))
    return;

  if (no_show_all)
    GTK_WIDGET_SET_FLAGS (widget, GTK_NO_SHOW_ALL);
  else
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_NO_SHOW_ALL);
  
  g_object_notify (G_OBJECT (widget), "no-show-all");
}

#define __GTK_WIDGET_C__
#include "gtkaliasdef.c"
