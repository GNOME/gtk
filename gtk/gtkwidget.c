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

#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include "gtkcontainer.h"
#include "gtkiconfactory.h"
#include "gtkmain.h"
#include "gtkrc.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtkbindings.h"
#include "gtkprivate.h"
#include "gdk/gdk.h"
#include "gdk/gdkprivate.h" /* Used in gtk_reset_shapes_recurse to avoid copy */


#define WIDGET_CLASS(w)	 GTK_WIDGET_GET_CLASS (w)
#define	INIT_PATH_SIZE	(512)


enum {
  SHOW,
  HIDE,
  MAP,
  UNMAP,
  REALIZE,
  UNREALIZE,
  DRAW_FOCUS,
  DRAW_DEFAULT,
  SIZE_REQUEST,
  SIZE_ALLOCATE,
  STATE_CHANGED,
  PARENT_SET,
  STYLE_SET,
  DIRECTION_CHANGED,
  ADD_ACCELERATOR,
  REMOVE_ACCELERATOR,
  GRAB_FOCUS,
  EVENT,
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
  DEBUG_MSG,
  LAST_SIGNAL
};

enum {
  ARG_0,
  ARG_NAME,
  ARG_PARENT,
  ARG_X,
  ARG_Y,
  ARG_WIDTH,
  ARG_HEIGHT,
  ARG_VISIBLE,
  ARG_SENSITIVE,
  ARG_APP_PAINTABLE,
  ARG_CAN_FOCUS,
  ARG_HAS_FOCUS,
  ARG_CAN_DEFAULT,
  ARG_HAS_DEFAULT,
  ARG_RECEIVES_DEFAULT,
  ARG_COMPOSITE_CHILD,
  ARG_STYLE,
  ARG_EVENTS,
  ARG_EXTENSION_EVENTS
};

typedef	struct	_GtkStateData	 GtkStateData;

struct _GtkStateData
{
  GtkStateType  state;
  guint		state_restoration : 1;
  guint         parent_sensitive : 1;
  guint		use_forall : 1;
};

static void gtk_widget_class_init		 (GtkWidgetClass    *klass);
static void gtk_widget_init			 (GtkWidget	    *widget);
static void gtk_widget_set_arg			 (GtkObject         *object,
						  GtkArg	    *arg,
						  guint		     arg_id);
static void gtk_widget_get_arg			 (GtkObject         *object,
						  GtkArg	    *arg,
						  guint		     arg_id);
static void gtk_widget_shutdown			 (GObject	    *object);
static void gtk_widget_real_destroy		 (GtkObject	    *object);
static void gtk_widget_finalize			 (GObject	    *object);
static void gtk_widget_real_show		 (GtkWidget	    *widget);
static void gtk_widget_real_hide		 (GtkWidget	    *widget);
static void gtk_widget_real_map			 (GtkWidget	    *widget);
static void gtk_widget_real_unmap		 (GtkWidget	    *widget);
static void gtk_widget_real_realize		 (GtkWidget	    *widget);
static void gtk_widget_real_unrealize		 (GtkWidget	    *widget);
static void gtk_widget_real_size_request	 (GtkWidget	    *widget,
						  GtkRequisition    *requisition);
static void gtk_widget_real_size_allocate	 (GtkWidget	    *widget,
						  GtkAllocation	    *allocation);
static gint gtk_widget_real_key_press_event      (GtkWidget         *widget,
						  GdkEventKey       *event);
static gint gtk_widget_real_key_release_event    (GtkWidget         *widget,
						  GdkEventKey       *event);
static void gtk_widget_style_set		 (GtkWidget	    *widget,
						  GtkStyle          *previous_style);
static void gtk_widget_direction_changed	 (GtkWidget	    *widget,
						  GtkTextDirection   previous_direction);
static void gtk_widget_real_grab_focus           (GtkWidget         *focus_widget);

static GdkColormap*  gtk_widget_peek_colormap      (void);
static GtkStyle*     gtk_widget_peek_style         (void);
static PangoContext *gtk_widget_peek_pango_context (GtkWidget *widget);

static void gtk_widget_reparent_container_child  (GtkWidget     *widget,
						  gpointer       client_data);
static void gtk_widget_propagate_state		 (GtkWidget	*widget,
						  GtkStateData 	*data);
static void gtk_widget_set_style_internal	 (GtkWidget	*widget,
						  GtkStyle	*style,
						  gboolean	 initial_emission);
static void gtk_widget_set_style_recurse	 (GtkWidget	*widget,
						  gpointer	 client_data);

static GtkWidgetAuxInfo* gtk_widget_aux_info_new     (void);
static void		 gtk_widget_aux_info_destroy (GtkWidgetAuxInfo *aux_info);

static gpointer parent_class = NULL;
static guint widget_signals[LAST_SIGNAL] = { 0 };

static GMemChunk *aux_info_mem_chunk = NULL;

static GdkColormap *default_colormap = NULL;
static GtkStyle *gtk_default_style = NULL;

static GSList *colormap_stack = NULL;
static GSList *style_stack = NULL;
static guint   composite_child_stack = 0;

static const gchar *aux_info_key = "gtk-aux-info";
static guint        aux_info_key_id = 0;
static const gchar *event_key = "gtk-event-mask";
static guint        event_key_id = 0;
static const gchar *extension_event_key = "gtk-extension-event-mode";
static guint        extension_event_key_id = 0;
static const gchar *parent_window_key = "gtk-parent-window";
static guint        parent_window_key_id = 0;
static const gchar *saved_default_style_key = "gtk-saved-default-style";
static guint        saved_default_style_key_id = 0;
static const gchar *shape_info_key = "gtk-shape-info";
static const gchar *colormap_key = "gtk-colormap";
static const gchar *pango_context_key = "gtk-pango-context";
static guint        pango_context_key_id = 0;

static const gchar *rc_style_key = "gtk-rc-style";
static guint        rc_style_key_id = 0;

static GtkTextDirection gtk_default_direction = GTK_TEXT_DIR_LTR;

/*****************************************
 * gtk_widget_get_type:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkType
gtk_widget_get_type (void)
{
  static GtkType widget_type = 0;
  
  if (!widget_type)
    {
      static const GtkTypeInfo widget_info =
      {
	"GtkWidget",
	sizeof (GtkWidget),
	sizeof (GtkWidgetClass),
	(GtkClassInitFunc) gtk_widget_class_init,
	(GtkObjectInitFunc) gtk_widget_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };
      
      widget_type = gtk_type_unique (GTK_TYPE_OBJECT, &widget_info);
    }
  
  return widget_type;
}

/*****************************************
 * gtk_widget_class_init:
 *
 *   arguments:
 *
 *   results:
 *****************************************/
#include "stdio.h"
static void
gtk_widget_debug_msg (GtkWidget          *widget,
		      const gchar        *string)
{
  fprintf (stderr, "Gtk-DEBUG: %s\n", string);
}

static void
gtk_widget_class_init (GtkWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
  
  parent_class = gtk_type_class (GTK_TYPE_OBJECT);

  gobject_class->shutdown = gtk_widget_shutdown;
  gobject_class->finalize = gtk_widget_finalize;

  object_class->set_arg = gtk_widget_set_arg;
  object_class->get_arg = gtk_widget_get_arg;
  object_class->destroy = gtk_widget_real_destroy;
  
  klass->activate_signal = 0;
  klass->set_scroll_adjustments_signal = 0;
  klass->show = gtk_widget_real_show;
  klass->show_all = gtk_widget_show;
  klass->hide = gtk_widget_real_hide;
  klass->hide_all = gtk_widget_hide;
  klass->map = gtk_widget_real_map;
  klass->unmap = gtk_widget_real_unmap;
  klass->realize = gtk_widget_real_realize;
  klass->unrealize = gtk_widget_real_unrealize;
  klass->draw_focus = NULL;
  klass->size_request = gtk_widget_real_size_request;
  klass->size_allocate = gtk_widget_real_size_allocate;
  klass->state_changed = NULL;
  klass->parent_set = NULL;
  klass->style_set = gtk_widget_style_set;
  klass->direction_changed = gtk_widget_direction_changed;
  klass->add_accelerator = (void*) gtk_accel_group_handle_add;
  klass->remove_accelerator = (void*) gtk_accel_group_handle_remove;
  klass->grab_focus = gtk_widget_real_grab_focus;
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
  klass->focus_in_event = NULL;
  klass->focus_out_event = NULL;
  klass->map_event = NULL;
  klass->unmap_event = NULL;
  klass->property_notify_event = gtk_selection_property_notify;
  klass->selection_clear_event = gtk_selection_clear;
  klass->selection_request_event = gtk_selection_request;
  klass->selection_notify_event = gtk_selection_notify;
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

  klass->no_expose_event = NULL;

  klass->debug_msg = gtk_widget_debug_msg;
  
  gtk_object_add_arg_type ("GtkWidget::name", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_NAME);
  gtk_object_add_arg_type ("GtkWidget::parent", GTK_TYPE_CONTAINER, GTK_ARG_READWRITE, ARG_PARENT);
  gtk_object_add_arg_type ("GtkWidget::x", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_X);
  gtk_object_add_arg_type ("GtkWidget::y", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_Y);
  gtk_object_add_arg_type ("GtkWidget::width", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_WIDTH);
  gtk_object_add_arg_type ("GtkWidget::height", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_HEIGHT);
  gtk_object_add_arg_type ("GtkWidget::visible", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_VISIBLE);
  gtk_object_add_arg_type ("GtkWidget::sensitive", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_SENSITIVE);
  gtk_object_add_arg_type ("GtkWidget::app_paintable", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_APP_PAINTABLE);
  gtk_object_add_arg_type ("GtkWidget::can_focus", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_CAN_FOCUS);
  gtk_object_add_arg_type ("GtkWidget::has_focus", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HAS_FOCUS);
  gtk_object_add_arg_type ("GtkWidget::can_default", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_CAN_DEFAULT);
  gtk_object_add_arg_type ("GtkWidget::has_default", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HAS_DEFAULT);
  gtk_object_add_arg_type ("GtkWidget::receives_default", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_RECEIVES_DEFAULT);
  gtk_object_add_arg_type ("GtkWidget::composite_child", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_COMPOSITE_CHILD);
  gtk_object_add_arg_type ("GtkWidget::style", GTK_TYPE_STYLE, GTK_ARG_READWRITE, ARG_STYLE);
  gtk_object_add_arg_type ("GtkWidget::events", GTK_TYPE_GDK_EVENT_MASK, GTK_ARG_READWRITE, ARG_EVENTS);
  gtk_object_add_arg_type ("GtkWidget::extension_events", GTK_TYPE_GDK_EVENT_MASK, GTK_ARG_READWRITE, ARG_EXTENSION_EVENTS);
  
  widget_signals[SHOW] =
    gtk_signal_new ("show",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, show),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  widget_signals[HIDE] =
    gtk_signal_new ("hide",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, hide),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  widget_signals[MAP] =
    gtk_signal_new ("map",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, map),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  widget_signals[UNMAP] =
    gtk_signal_new ("unmap",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, unmap),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  widget_signals[REALIZE] =
    gtk_signal_new ("realize",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, realize),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  widget_signals[UNREALIZE] =
    gtk_signal_new ("unrealize",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, unrealize),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  widget_signals[DRAW_FOCUS] =
    gtk_signal_new ("draw_focus",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, draw_focus),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  widget_signals[DRAW_DEFAULT] =
    gtk_signal_new ("draw_default",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, draw_default),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  widget_signals[SIZE_REQUEST] =
    gtk_signal_new ("size_request",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, size_request),
		    gtk_marshal_VOID__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);
  widget_signals[SIZE_ALLOCATE] =
    gtk_signal_new ("size_allocate",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, size_allocate),
		    gtk_marshal_VOID__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);
  widget_signals[STATE_CHANGED] =
    gtk_signal_new ("state_changed",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, state_changed),
		    gtk_marshal_VOID__ENUM,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_STATE_TYPE);
  widget_signals[PARENT_SET] =
    gtk_signal_new ("parent_set",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, parent_set),
		    gtk_marshal_VOID__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_OBJECT);
  widget_signals[STYLE_SET] =
    gtk_signal_new ("style_set",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, style_set),
		    gtk_marshal_VOID__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_STYLE);
  widget_signals[DIRECTION_CHANGED] =
    gtk_signal_new ("direction_changed",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, direction_changed),
		    gtk_marshal_VOID__ENUM,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_TEXT_DIRECTION);
  widget_signals[ADD_ACCELERATOR] =
    gtk_accel_group_create_add (GTK_CLASS_TYPE (object_class), GTK_RUN_LAST,
				GTK_SIGNAL_OFFSET (GtkWidgetClass, add_accelerator));
  widget_signals[REMOVE_ACCELERATOR] =
    gtk_accel_group_create_remove (GTK_CLASS_TYPE (object_class), GTK_RUN_LAST,
				   GTK_SIGNAL_OFFSET (GtkWidgetClass, remove_accelerator));
  widget_signals[GRAB_FOCUS] =
    gtk_signal_new ("grab_focus",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, grab_focus),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  widget_signals[EVENT] =
    gtk_signal_new ("event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[BUTTON_PRESS_EVENT] =
    gtk_signal_new ("button_press_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, button_press_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[BUTTON_RELEASE_EVENT] =
    gtk_signal_new ("button_release_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, button_release_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[SCROLL_EVENT] =
    gtk_signal_new ("scroll_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, scroll_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[MOTION_NOTIFY_EVENT] =
    gtk_signal_new ("motion_notify_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, motion_notify_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DELETE_EVENT] =
    gtk_signal_new ("delete_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, delete_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DESTROY_EVENT] =
    gtk_signal_new ("destroy_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, destroy_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[EXPOSE_EVENT] =
    gtk_signal_new ("expose_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, expose_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[KEY_PRESS_EVENT] =
    gtk_signal_new ("key_press_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, key_press_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[KEY_RELEASE_EVENT] =
    gtk_signal_new ("key_release_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, key_release_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[ENTER_NOTIFY_EVENT] =
    gtk_signal_new ("enter_notify_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, enter_notify_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[LEAVE_NOTIFY_EVENT] =
    gtk_signal_new ("leave_notify_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, leave_notify_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[CONFIGURE_EVENT] =
    gtk_signal_new ("configure_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, configure_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[FOCUS_IN_EVENT] =
    gtk_signal_new ("focus_in_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, focus_in_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[FOCUS_OUT_EVENT] =
    gtk_signal_new ("focus_out_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, focus_out_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[MAP_EVENT] =
    gtk_signal_new ("map_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, map_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[UNMAP_EVENT] =
    gtk_signal_new ("unmap_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, unmap_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[PROPERTY_NOTIFY_EVENT] =
    gtk_signal_new ("property_notify_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, property_notify_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[SELECTION_CLEAR_EVENT] =
    gtk_signal_new ("selection_clear_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_clear_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[SELECTION_REQUEST_EVENT] =
    gtk_signal_new ("selection_request_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_request_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[SELECTION_NOTIFY_EVENT] =
    gtk_signal_new ("selection_notify_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_notify_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[SELECTION_RECEIVED] =
    gtk_signal_new ("selection_received",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_received),
		    gtk_marshal_VOID__POINTER_UINT,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_SELECTION_DATA,
		    GTK_TYPE_UINT);
  widget_signals[SELECTION_GET] =
    gtk_signal_new ("selection_get",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_get),
		    gtk_marshal_VOID__POINTER_UINT_UINT,
		    GTK_TYPE_NONE, 3,
		    GTK_TYPE_SELECTION_DATA,
		    GTK_TYPE_UINT,
		    GTK_TYPE_UINT);
  widget_signals[PROXIMITY_IN_EVENT] =
    gtk_signal_new ("proximity_in_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, proximity_in_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[PROXIMITY_OUT_EVENT] =
    gtk_signal_new ("proximity_out_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, proximity_out_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DRAG_LEAVE] =
    gtk_signal_new ("drag_leave",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_leave),
		    gtk_marshal_VOID__POINTER_UINT,
		    GTK_TYPE_NONE, 2,
		    GDK_TYPE_DRAG_CONTEXT,
		    GTK_TYPE_UINT);
  widget_signals[DRAG_BEGIN] =
    gtk_signal_new ("drag_begin",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_begin),
		    gtk_marshal_VOID__POINTER,
		    GTK_TYPE_NONE, 1,
		    GDK_TYPE_DRAG_CONTEXT);
  widget_signals[DRAG_END] =
    gtk_signal_new ("drag_end",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_end),
		    gtk_marshal_VOID__POINTER,
		    GTK_TYPE_NONE, 1,
		    GDK_TYPE_DRAG_CONTEXT);
  widget_signals[DRAG_DATA_DELETE] =
    gtk_signal_new ("drag_data_delete",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_data_delete),
		    gtk_marshal_VOID__POINTER,
		    GTK_TYPE_NONE, 1,
		    GDK_TYPE_DRAG_CONTEXT);
  widget_signals[DRAG_MOTION] =
    gtk_signal_new ("drag_motion",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_motion),
		    gtk_marshal_BOOLEAN__POINTER_INT_INT_UINT,
		    GTK_TYPE_BOOL, 4,
		    GDK_TYPE_DRAG_CONTEXT,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT,
		    GTK_TYPE_UINT);
  widget_signals[DRAG_DROP] =
    gtk_signal_new ("drag_drop",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_drop),
		    gtk_marshal_BOOLEAN__POINTER_INT_INT_UINT,
		    GTK_TYPE_BOOL, 4,
		    GDK_TYPE_DRAG_CONTEXT,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT,
		    GTK_TYPE_UINT);
  widget_signals[DRAG_DATA_GET] =
    gtk_signal_new ("drag_data_get",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_data_get),
		    gtk_marshal_VOID__POINTER_POINTER_UINT_UINT,
		    GTK_TYPE_NONE, 4,
		    GDK_TYPE_DRAG_CONTEXT,
		    GTK_TYPE_SELECTION_DATA,
		    GTK_TYPE_UINT,
		    GTK_TYPE_UINT);
  widget_signals[DRAG_DATA_RECEIVED] =
    gtk_signal_new ("drag_data_received",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_data_received),
		    gtk_marshal_VOID__POINTER_INT_INT_POINTER_UINT_UINT,
		    GTK_TYPE_NONE, 6,
		    GDK_TYPE_DRAG_CONTEXT,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT,
		    GTK_TYPE_SELECTION_DATA,
		    GTK_TYPE_UINT,
		    GTK_TYPE_UINT);
  widget_signals[VISIBILITY_NOTIFY_EVENT] =
    gtk_signal_new ("visibility_notify_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, visibility_notify_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[CLIENT_EVENT] =
    gtk_signal_new ("client_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, client_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[NO_EXPOSE_EVENT] =
    gtk_signal_new ("no_expose_event",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, no_expose_event),
		    gtk_marshal_BOOLEAN__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DEBUG_MSG] =
    gtk_signal_new ("debug_msg",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, debug_msg),
		    gtk_marshal_VOID__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_STRING);

  gtk_object_class_add_signals (object_class, widget_signals, LAST_SIGNAL);
}

static void
gtk_widget_set_arg (GtkObject   *object,
		    GtkArg	*arg,
		    guint	 arg_id)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (object);

  switch (arg_id)
    {
      guint32 saved_flags;
      
    case ARG_NAME:
      gtk_widget_set_name (widget, GTK_VALUE_STRING (*arg));
      break;
    case ARG_PARENT:
      gtk_container_add (GTK_CONTAINER (GTK_VALUE_OBJECT (*arg)), widget);
      break;
    case ARG_X:
      gtk_widget_set_uposition (widget, GTK_VALUE_INT (*arg), -2);
      break;
    case ARG_Y:
      gtk_widget_set_uposition (widget, -2, GTK_VALUE_INT (*arg));
      break;
    case ARG_WIDTH:
      gtk_widget_set_usize (widget, GTK_VALUE_INT (*arg), -2);
      break;
    case ARG_HEIGHT:
      gtk_widget_set_usize (widget, -2, GTK_VALUE_INT (*arg));
      break;
    case ARG_VISIBLE:
      if (GTK_VALUE_BOOL(*arg))
	gtk_widget_show (widget);
      else
	gtk_widget_hide (widget);
      break;
    case ARG_SENSITIVE:
      gtk_widget_set_sensitive (widget, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_APP_PAINTABLE:
      gtk_widget_set_app_paintable (widget, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_CAN_FOCUS:
      saved_flags = GTK_WIDGET_FLAGS (widget);
      if (GTK_VALUE_BOOL (*arg))
	GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
      else
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_FOCUS);
      if (saved_flags != GTK_WIDGET_FLAGS (widget))
	gtk_widget_queue_resize (widget);
      break;
    case ARG_HAS_FOCUS:
      if (GTK_VALUE_BOOL (*arg))
	gtk_widget_grab_focus (widget);
      break;
    case ARG_CAN_DEFAULT:
      saved_flags = GTK_WIDGET_FLAGS (widget);
      if (GTK_VALUE_BOOL (*arg))
	GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
      else
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_DEFAULT);
      if (saved_flags != GTK_WIDGET_FLAGS (widget))
	gtk_widget_queue_resize (widget);
      break;
    case ARG_HAS_DEFAULT:
      if (GTK_VALUE_BOOL (*arg))
	gtk_widget_grab_default (widget);
      break;
    case ARG_RECEIVES_DEFAULT:
      if (GTK_VALUE_BOOL (*arg))
	GTK_WIDGET_SET_FLAGS (widget, GTK_RECEIVES_DEFAULT);
      else
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_RECEIVES_DEFAULT);
      break;
    case ARG_COMPOSITE_CHILD:
      if (GTK_VALUE_BOOL(*arg))
	GTK_WIDGET_SET_FLAGS (widget, GTK_COMPOSITE_CHILD);
      else
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_COMPOSITE_CHILD);
      break;
    case ARG_STYLE:
      gtk_widget_set_style (widget, (GtkStyle*) GTK_VALUE_BOXED (*arg));
      break;
    case ARG_EVENTS:
      if (!GTK_WIDGET_REALIZED (widget) && !GTK_WIDGET_NO_WINDOW (widget))
	gtk_widget_set_events (widget, GTK_VALUE_FLAGS (*arg));
      break;
    case ARG_EXTENSION_EVENTS:
      gtk_widget_set_extension_events (widget, GTK_VALUE_FLAGS (*arg));
      break;
    default:
      break;
    }
}

/*****************************************
 * gtk_widget_get_arg:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_get_arg (GtkObject   *object,
		    GtkArg	*arg,
		    guint	 arg_id)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (object);
  
  switch (arg_id)
    {
      GtkWidgetAuxInfo *aux_info;
      gint *eventp;
      GdkExtensionMode *modep;

    case ARG_NAME:
      if (widget->name)
	GTK_VALUE_STRING (*arg) = g_strdup (widget->name);
      else
	GTK_VALUE_STRING (*arg) = g_strdup ("");
      break;
    case ARG_PARENT:
      GTK_VALUE_OBJECT (*arg) = (GtkObject*) widget->parent;
      break;
    case ARG_X:
      aux_info = gtk_object_get_data_by_id (GTK_OBJECT (widget), aux_info_key_id);
      if (!aux_info)
	GTK_VALUE_INT (*arg) = -1;
      else
	GTK_VALUE_INT (*arg) = aux_info->x;
      break;
    case ARG_Y:
      aux_info = gtk_object_get_data_by_id (GTK_OBJECT (widget), aux_info_key_id);
      if (!aux_info)
	GTK_VALUE_INT (*arg) = -1;
      else
	GTK_VALUE_INT (*arg) = aux_info->y;
      break;
    case ARG_WIDTH:
      aux_info = gtk_object_get_data_by_id (GTK_OBJECT (widget), aux_info_key_id);
      if (!aux_info)
	GTK_VALUE_INT (*arg) = -1;
      else
	GTK_VALUE_INT (*arg) = aux_info->width;
      break;
    case ARG_HEIGHT:
      aux_info = gtk_object_get_data_by_id (GTK_OBJECT (widget), aux_info_key_id);
      if (!aux_info)
	GTK_VALUE_INT (*arg) = -1;
      else
	GTK_VALUE_INT (*arg) = aux_info->height;
      break;
    case ARG_VISIBLE:
      GTK_VALUE_BOOL (*arg) = (GTK_WIDGET_VISIBLE (widget) != FALSE);
      break;
    case ARG_SENSITIVE:
      GTK_VALUE_BOOL (*arg) = (GTK_WIDGET_SENSITIVE (widget) != FALSE);
      break;
    case ARG_APP_PAINTABLE:
      GTK_VALUE_BOOL (*arg) = (GTK_WIDGET_APP_PAINTABLE (widget) != FALSE);
      break;
    case ARG_CAN_FOCUS:
      GTK_VALUE_BOOL (*arg) = (GTK_WIDGET_CAN_FOCUS (widget) != FALSE);
      break;
    case ARG_HAS_FOCUS:
      GTK_VALUE_BOOL (*arg) = (GTK_WIDGET_HAS_FOCUS (widget) != FALSE);
      break;
    case ARG_CAN_DEFAULT:
      GTK_VALUE_BOOL (*arg) = (GTK_WIDGET_CAN_DEFAULT (widget) != FALSE);
      break;
    case ARG_HAS_DEFAULT:
      GTK_VALUE_BOOL (*arg) = (GTK_WIDGET_HAS_DEFAULT (widget) != FALSE);
      break;
    case ARG_RECEIVES_DEFAULT:
      GTK_VALUE_BOOL (*arg) = (GTK_WIDGET_RECEIVES_DEFAULT (widget) != FALSE);
      break;
    case ARG_COMPOSITE_CHILD:
      GTK_VALUE_BOOL (*arg) = (GTK_WIDGET_COMPOSITE_CHILD (widget) != FALSE);
      break;
    case ARG_STYLE:
      GTK_VALUE_BOXED (*arg) = (gpointer) gtk_widget_get_style (widget);
      break;
    case ARG_EVENTS:
      eventp = gtk_object_get_data_by_id (GTK_OBJECT (widget), event_key_id);
      if (!eventp)
	GTK_VALUE_FLAGS (*arg) = 0;
      else
	GTK_VALUE_FLAGS (*arg) = *eventp;
      break;
    case ARG_EXTENSION_EVENTS:
      modep = gtk_object_get_data_by_id (GTK_OBJECT (widget), extension_event_key_id);
      if (!modep)
	GTK_VALUE_FLAGS (*arg) = 0;
      else
	GTK_VALUE_FLAGS (*arg) = *modep;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

/*****************************************
 * gtk_widget_init:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_init (GtkWidget *widget)
{
  GdkColormap *colormap;
  
  GTK_PRIVATE_FLAGS (widget) = 0;
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

  widget->style = gtk_widget_peek_style ();
  gtk_style_ref (widget->style);
  
  colormap = gtk_widget_peek_colormap ();
  
  if (colormap != gtk_widget_get_default_colormap ())
    gtk_widget_set_colormap (widget, colormap);
}


/**
 * gtk_widget_new:
 * @type: type ID of the widget to create
 * @first_arg_name: name of first property to set
 * @Varargs: value of first property, followed by more properties, NULL-terminated
 * 
 * This is a convenience function for creating a widget and setting
 * its properties in one go. For example you might write:
 * gtk_widget_new (GTK_TYPE_LABEL, "label", "Hello World", "xalign",
 * 0.0, NULL) to create a left-aligned label. Equivalent to
 * gtk_object_new(), but returns a widget so you don't have to
 * cast the object yourself.
 * 
 * Return value: a new #GtkWidget of type @widget_type
 **/
GtkWidget*
gtk_widget_new (GtkType      type,
		const gchar *first_arg_name,
		...)
{
  GtkObject *object;
  va_list var_args;
  GSList *arg_list = NULL;
  GSList *info_list = NULL;
  gchar *error;
  
  g_return_val_if_fail (gtk_type_is_a (type, GTK_TYPE_WIDGET), NULL);
  
  object = gtk_type_new (type);
  
  va_start (var_args, first_arg_name);
  error = gtk_object_args_collect (GTK_OBJECT_TYPE (object),
				   &arg_list,
				   &info_list,
				   first_arg_name,
				   var_args);
  va_end (var_args);
  
  if (error)
    {
      g_warning ("gtk_widget_new(): %s", error);
      g_free (error);
    }
  else
    {
      GSList *slist_arg;
      GSList *slist_info;
      
      slist_arg = arg_list;
      slist_info = info_list;
      while (slist_arg)
	{
	  gtk_object_arg_set (object, slist_arg->data, slist_info->data);
	  slist_arg = slist_arg->next;
	  slist_info = slist_info->next;
	}
      gtk_args_collect_cleanup (arg_list, info_list);
    }
  
  if (!GTK_OBJECT_CONSTRUCTED (object))
    gtk_object_default_construct (object);

  return GTK_WIDGET (object);
}

/**
 * gtk_widget_newv:
 * @type: a #GtkType for the widget to create
 * @nargs: number of args in @args
 * @args: array of args specifying widget properties
 *
 * Same as gtk_widget_new(), but takes an array instead of using
 * varargs. This version is only useful if you need to construct
 * the args programmatically. Equivalent to gtk_object_newv(), but
 * returns a #GtkWidget so you don't have to case the object yourself.
 * 
 * Return value: a #GtkWidget
 **/
GtkWidget*
gtk_widget_newv (GtkType type,
		 guint	 nargs,
		 GtkArg *args)
{
  g_return_val_if_fail (gtk_type_is_a (type, GTK_TYPE_WIDGET), NULL);
  
  return GTK_WIDGET (gtk_object_newv (type, nargs, args));
}

/**
 * gtk_widget_get:
 * @widget: a #GtkWidget
 * @arg: single #GtkArg with the argument name filled in
 * 
 * Queries the value of @arg, storing it in @arg.
 **/
void
gtk_widget_get (GtkWidget	*widget,
		GtkArg		*arg)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (arg != NULL);
  
  gtk_object_getv (GTK_OBJECT (widget), 1, arg);
}

/**
 * gtk_widget_getv:
 * @widget: a #GtkWidget
 * @nargs: number of args in @args
 * @args: array of #GtkArg
 * 
 * Like calling gtk_widget_get() on each arg in @args.
 **/
void
gtk_widget_getv (GtkWidget	*widget,
		 guint           nargs,
		 GtkArg         *args)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_object_getv (GTK_OBJECT (widget), nargs, args);
}

/**
 * gtk_widget_set:
 * @widget: a #GtkWidget
 * @first_arg_name: name of first arg to set
 * @Varargs: value of first arg, then more name-value pairs, and %NULL-terminated
 * 
 * Like gtk_object_set() - there's no reason to use this instead of
 * gtk_object_set().
 **/
void
gtk_widget_set (GtkWidget   *widget,
		const gchar *first_arg_name,
		...)
{
  GtkObject *object;
  va_list var_args;
  GSList *arg_list = NULL;
  GSList *info_list = NULL;
  gchar *error;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  object = GTK_OBJECT (widget);

  va_start (var_args, first_arg_name);
  error = gtk_object_args_collect (GTK_OBJECT_TYPE (object),
				   &arg_list,
				   &info_list,
				   first_arg_name,
				   var_args);
  va_end (var_args);

  if (error)
    {
      g_warning ("gtk_widget_set(): %s", error);
      g_free (error);
    }
  else
    {
      GSList *slist_arg;
      GSList *slist_info;

      slist_arg = arg_list;
      slist_info = info_list;
      while (slist_arg)
	{
	  gtk_object_arg_set (object, slist_arg->data, slist_info->data);
	  slist_arg = slist_arg->next;
	  slist_info = slist_info->next;
	}
      gtk_args_collect_cleanup (arg_list, info_list);
    }
}

/**
 * gtk_widget_setv:
 * @widget: a #GtkWidget
 * @nargs: args in @args
 * @args: array of args to set on the widget
 * 
 * Each arg in @args should have its name and value filled in. The
 * corresponding properties will be set on the
 * widget. gtk_object_set() is a more convenient version of this
 * function, since it takes varargs in place of the array. You may
 * need the array version to construct the list of args to set at
 * runtime.
 * 
 **/
void
gtk_widget_setv (GtkWidget *widget,
		 guint	    nargs,
		 GtkArg	   *args)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_object_setv (GTK_OBJECT (widget), nargs, args);
}

static inline void	   
gtk_widget_queue_clear_child (GtkWidget *widget)
{
  GtkWidget *parent;

  parent = widget->parent;
  if (parent && GTK_WIDGET_DRAWABLE (parent))
    gtk_widget_queue_clear_area (parent,
				 widget->allocation.x,
				 widget->allocation.y,
				 widget->allocation.width,
				 widget->allocation.height);
}

/**
 * gtk_widget_unparent:
 * @widget: a #GtkWidget
 * 
 * INTERNAL FUNCTION, only for use in widget implementations.
 * Should be called by implementations of the remove method
 * on #GtkContainer, to dissociate a child from the container.
 **/
void
gtk_widget_unparent (GtkWidget *widget)
{
  GtkWidget *toplevel;
  GtkWidget *old_parent;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  if (widget->parent == NULL)
    return;
  
  /* keep this function in sync with gtk_menu_detach()
   */

  /* unset focused and default children properly, this code
   * should eventually move into some gtk_window_unparent_branch() or
   * similar function.
   */
  
  toplevel = gtk_widget_get_toplevel (widget);
  if (GTK_CONTAINER (widget->parent)->focus_child == widget)
    {
      gtk_container_set_focus_child (GTK_CONTAINER (widget->parent), NULL);

      if (GTK_IS_WINDOW (toplevel))
	{
	  GtkWidget *child;
      
	  child = GTK_WINDOW (toplevel)->focus_widget;
	  
	  while (child && child != widget)
	    child = child->parent;
	  
	  if (child == widget)
	    gtk_window_set_focus (GTK_WINDOW (toplevel), NULL);
	}
    }
  if (GTK_IS_WINDOW (toplevel))
    {
      GtkWidget *child;
      
      child = GTK_WINDOW (toplevel)->default_widget;
      
      while (child && child != widget)
	child = child->parent;
      
      if (child == widget)
	gtk_window_set_default (GTK_WINDOW (toplevel), NULL);
    }

  if (GTK_IS_RESIZE_CONTAINER (widget))
    gtk_container_clear_resize_widgets (GTK_CONTAINER (widget));
  
  /* Remove the widget and all its children from any ->resize_widgets list
   * of all the parents in our branch. This code should move into gtkcontainer.c
   * somwhen, since we mess around with ->resize_widgets, which is
   * actually not of our business.
   *
   * Two ways to make this prettier:
   *   Write a g_slist_conditional_remove (GSList, gboolean (*)(gpointer))
   *   Change resize_widgets to a GList
   */
  toplevel = widget->parent;
  while (toplevel)
    {
      GSList *slist;
      GSList *prev;

      if (!GTK_CONTAINER (toplevel)->resize_widgets)
	{
	  toplevel = toplevel->parent;
	  continue;
	}

      prev = NULL;
      slist = GTK_CONTAINER (toplevel)->resize_widgets;
      while (slist)
	{
	  GtkWidget *child;
	  GtkWidget *parent;
	  GSList *last;

	  last = slist;
	  slist = last->next;
	  child = last->data;
	  
	  parent = child;
	  while (parent && (parent != widget))
	    parent = parent->parent;
	  
	  if (parent == widget)
	    {
	      GTK_PRIVATE_UNSET_FLAG (child, GTK_RESIZE_NEEDED);
	      
	      if (prev)
		prev->next = slist;
	      else
		GTK_CONTAINER (toplevel)->resize_widgets = slist;
	      
	      g_slist_free_1 (last);
	    }
	  else
	    prev = last;
	}

      toplevel = toplevel->parent;
    }

  gtk_widget_queue_clear_child (widget);

  /* Reset the width and height here, to force reallocation if we
   * get added back to a new parent. This won't work if our new
   * allocation is smaller than 1x1 and we actually want a size of 1x1...
   * (would 0x0 be OK here?)
   */
  widget->allocation.width = 1;
  widget->allocation.height = 1;
  
  if (GTK_WIDGET_REALIZED (widget) && !GTK_WIDGET_IN_REPARENT (widget))
    gtk_widget_unrealize (widget);

  old_parent = widget->parent;
  widget->parent = NULL;
  gtk_widget_set_parent_window (widget, NULL);
  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[PARENT_SET], old_parent);
  
  gtk_widget_unref (widget);
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
 * to the widget with gtk_object_ref().
 *
 * In most cases, only toplevel widgets (windows) require explicit
 * destruction, because when you destroy a toplevel its children will
 * be destroyed as well.
 * 
 **/
void
gtk_widget_destroy (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_OBJECT_CONSTRUCTED (widget));

  gtk_object_destroy ((GtkObject*) widget);
}

/**
 * gtk_widget_destroyed:
 * @widget: a #GtkWidget
 * @widget_pointer: address of a variable that contains @widget
 *
 * This function sets *@widget_pointer to NULL if @widget_pointer !=
 * NULL.  It's intended to be used as a callback connected to the
 * "destroy" signal of a widget. You connect gtk_widget_destroyed()
 * as a signal handler, and pass the address of your widget variable
 * as user data. Then when the widget is destroyed, the variable will
 * be set to NULL. Useful for example to avoid multiple copies
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
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (!GTK_WIDGET_VISIBLE (widget))
    {
      if (!GTK_WIDGET_TOPLEVEL (widget))
	gtk_widget_queue_resize (widget);
      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[SHOW]);
    }
}

static void
gtk_widget_real_show (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (!GTK_WIDGET_VISIBLE (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);

      if (widget->parent &&
	  GTK_WIDGET_MAPPED (widget->parent) &&
	  !GTK_WIDGET_MAPPED (widget))
	gtk_widget_map (widget);
    }
}

static void
gtk_widget_show_map_callback (GtkWidget *widget, GdkEvent *event, gint *flag)
{
  *flag = TRUE;
  gtk_signal_disconnect_by_data (GTK_OBJECT (widget), flag);
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
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* make sure we will get event */
  if (!GTK_WIDGET_MAPPED (widget) &&
      GTK_WIDGET_TOPLEVEL (widget))
    {
      gtk_widget_show (widget);

      gtk_signal_connect (GTK_OBJECT (widget), "map_event",
			  GTK_SIGNAL_FUNC (gtk_widget_show_map_callback), 
			  &flag);

      while (!flag)
	gtk_main_iteration();
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
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (GTK_WIDGET_VISIBLE (widget))
    {
      gtk_widget_ref (widget);
      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[HIDE]);
      if (!GTK_WIDGET_TOPLEVEL (widget) && !GTK_OBJECT_DESTROYED (widget))
	gtk_widget_queue_resize (widget);
      gtk_widget_unref (widget);
    }
}

static void
gtk_widget_real_hide (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
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
 * argument, then returns %TRUE. If connected to "delete_event",
 * the result is that clicking the window manager close button for
 * will hide but not destroy the window. By default, GTK+ destroys
 * windows when "delete_event" is received.
 * 
 * Return value: %TRUE
 **/
gint
gtk_widget_hide_on_delete (GtkWidget      *widget)
{
  g_return_val_if_fail (widget != NULL, FALSE);
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

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

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

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  class = GTK_WIDGET_GET_CLASS (widget);

  if (class->hide_all)
    class->hide_all (widget);
}

/**
 * gtk_widget_map:
 * @widget: a #GtkWidget
 * 
 * INTERNAL FUNCTION, only for use in widget implementations. Causes
 * a widget to be mapped if it isn't already.
 * 
 **/
void
gtk_widget_map (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_VISIBLE (widget) == TRUE);
  
  if (!GTK_WIDGET_MAPPED (widget))
    {
      if (!GTK_WIDGET_REALIZED (widget))
	gtk_widget_realize (widget);

      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[MAP]);

      if (GTK_WIDGET_NO_WINDOW (widget))
	gtk_widget_queue_draw (widget);
    }
}

/**
 * gtk_widget_unmap:
 * @widget: a #GtkWidget
 *
 * INTERNAL FUNCTION, only for use in widget implementations. Causes
 * a widget to be unmapped if it's currently mapped.
 * 
 **/
void
gtk_widget_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (GTK_WIDGET_MAPPED (widget))
    {
      if (GTK_WIDGET_NO_WINDOW (widget))
	gtk_widget_queue_clear_child (widget);
      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[UNMAP]);
    }
}

/**
 * gtk_widget_realize:
 * @widget: a #GtkWidget
 * 
 * Creates the GDK (windowing system) resources associated with a
 * widget.  For example, widget->window will be created when a widget
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
 * "expose_event". Or simply gtk_signal_connect_after() to the
 * "realize" signal.
 *  
 **/
void
gtk_widget_realize (GtkWidget *widget)
{
  gint events;
  GdkExtensionMode mode;
  GtkWidgetShapeInfo *shape_info;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (!GTK_WIDGET_REALIZED (widget))
    {
      /*
	if (GTK_IS_CONTAINER (widget) && !GTK_WIDGET_NO_WINDOW (widget))
	  g_message ("gtk_widget_realize(%s)", gtk_type_name (GTK_WIDGET_TYPE (widget)));
      */

      if (widget->parent == NULL &&
          !GTK_WIDGET_TOPLEVEL (widget))
        g_warning ("Calling gtk_widget_realize() on a widget that isn't "
                   "inside a toplevel window is not going to work very well. "
                   "Widgets must be inside a toplevel container before realizing them.");
      
      if (widget->parent && !GTK_WIDGET_REALIZED (widget->parent))
	gtk_widget_realize (widget->parent);

      gtk_widget_ensure_style (widget);
      
      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[REALIZE]);
      
      if (GTK_WIDGET_HAS_SHAPE_MASK (widget))
	{
	  shape_info = gtk_object_get_data (GTK_OBJECT (widget),
					    shape_info_key);
	  gdk_window_shape_combine_mask (widget->window,
					 shape_info->shape_mask,
					 shape_info->offset_x,
					 shape_info->offset_y);
	}
      
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
 * INTERNAL FUNCTION, only useful in widget implementations.
 * Causes a widget to be unrealized (frees all GDK resources
 * associated with the widget, such as widget->window).
 * 
 **/
void
gtk_widget_unrealize (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (GTK_WIDGET_HAS_SHAPE_MASK (widget))
    gtk_widget_shape_combine_mask (widget, NULL, -1, -1);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gtk_widget_ref (widget);
      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[UNREALIZE]);
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED | GTK_MAPPED);
      gtk_widget_unref (widget);
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
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_queue_clear_area (widget, x, y, width, height);
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
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_queue_clear (widget);
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
 * DEPRECATED. This function is no longer different from
 * gtk_widget_queue_draw_area(), though it once was. Now it just calls
 * gtk_widget_queue_draw_area(). Originally
 * gtk_widget_queue_clear_area() would force a redraw of the
 * background for GTK_NO_WINDOW widgets, and
 * gtk_widget_queue_draw_area() would not. Now both functions ensure
 * the background will be redrawn.
 * 
 **/
void	   
gtk_widget_queue_clear_area (GtkWidget *widget,
			     gint       x,
			     gint       y,
			     gint       width,
			     gint       height)
{
  GdkRectangle invalid_rect;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!(widget->window && gdk_window_is_viewable (widget->window)))
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
	  
	  gdk_window_get_size (widget->window, &wwidth, &wheight);

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

/**
 * gtk_widget_queue_clear:
 * @widget: a #GtkWidget
 * 
 * DEPRECATED. Use gtk_widget_queue_draw() instead.
 **/
void	   
gtk_widget_queue_clear (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (widget->allocation.width || widget->allocation.height)
    {
      if (GTK_WIDGET_NO_WINDOW (widget))
	gtk_widget_queue_clear_area (widget, widget->allocation.x,
				     widget->allocation.y,
				     widget->allocation.width, 
				     widget->allocation.height);
      else
	gtk_widget_queue_clear_area (widget, 0, 0, 
				     widget->allocation.width, 
				     widget->allocation.height);
    }
}

/**
 * gtk_widget_queue_resize:
 * @widget: a #GtkWidget
 *
 * INTERNAL FUNCTION, for use in widget implementations.
 * Flags a widget to have its size renegotiated; should
 * be called when a widget for some reason has a new size request.
 * For example, when you change the text in a #GtkLabel, #GtkLabel
 * queues a resize to ensure there's enough space for the new text.
 * 
 **/
void
gtk_widget_queue_resize (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (GTK_IS_RESIZE_CONTAINER (widget))
    gtk_container_clear_resize_widgets (GTK_CONTAINER (widget));

  gtk_widget_queue_clear (widget);

  if (widget->parent)
    gtk_container_queue_resize (GTK_CONTAINER (widget->parent));
  else if (GTK_WIDGET_TOPLEVEL (widget) && GTK_IS_CONTAINER (widget))
    gtk_container_queue_resize (GTK_CONTAINER (widget));
}

/**
 * gtk_widget_draw:
 * @widget: a #GtkWidget
 * @area: area to draw
 *
 * DEPRECATED. In GTK+ 1.2, this function would immediately render the
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
  g_return_if_fail (widget != NULL);
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
 * gtk_widget_draw_focus:
 * @widget: a #GtkWidget
 *
 * INTERNAL FUNCTION for use in widget implementations. Invokes the
 * "draw_focus" virtual method/signal on @widget, causing the focus
 * rectangle to be drawn or undrawn according to the focus state of
 * the widget. Normally called from widget implementations in the
 * "focus_in_event" and "focus_out_event" handlers.
 * 
 **/
void
gtk_widget_draw_focus (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[DRAW_FOCUS]);
}

/**
 * gtk_widget_draw_default:
 * @widget: a #GtkWidget
 * 
 * INTERNAL FUNCTION for use in widget implementations. Invokes the
 * "draw_default" virtual method/signal on a widget, causing it to
 * draw the default rectangle (indicating that the widget is
 * the default widget, i.e. the one that's activated by pressing
 * the enter key, generally).
 **/
void
gtk_widget_draw_default (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[DRAW_DEFAULT]);
}

/**
 * gtk_widget_size_request:
 * @widget: a #GtkWidget
 * @requisition: a #GtkRequisition to be filled in
 * 
 * INTERNAL FUNCTION used when implementing a #GtkContainer subclass.
 * Obtains the preferred size of a widget. The container uses this
 * information to arrange its child widgets and decide what size allocations
 * to give them with gtk_widget_size_allocate().
 **/
void
gtk_widget_size_request (GtkWidget	*widget,
			 GtkRequisition *requisition)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

#ifdef G_ENABLE_DEBUG
  if (requisition == &widget->requisition)
    g_warning ("gtk_widget_size_request() called on child widget with request equal\n to widget->requisition. gtk_widget_set_usize() may not work properly.");
#endif /* G_ENABLE_DEBUG */

  gtk_widget_ref (widget);
  gtk_widget_ensure_style (widget);
  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[SIZE_REQUEST],
		   &widget->requisition);

  if (requisition)
    gtk_widget_get_child_requisition (widget, requisition);

  gtk_widget_unref (widget);
}

/**
 * gtk_widget_get_child_requisition:
 * @widget: a #GtkWidget
 * @requisition: a #GtkRequisition to be filled in
 * 
 * INTERNAL FUNCTION for use in widget implementations. Obtains
 * @widget->requisition, unless someone has forced a particular
 * geometry on the widget (e.g. with gtk_widget_set_usize()), in which
 * case it returns that geometry instead of the widget's requisition.
 **/
void
gtk_widget_get_child_requisition (GtkWidget	 *widget,
				  GtkRequisition *requisition)
{
  GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  *requisition = widget->requisition;
  
  aux_info = gtk_object_get_data_by_id (GTK_OBJECT (widget), aux_info_key_id);
  if (aux_info)
    {
      if (aux_info->width > 0)
	requisition->width = aux_info->width;
      if (aux_info->height > 0)
	requisition->height = aux_info->height;
    }
}

/**
 * gtk_widget_size_allocate:
 * @widget: a #GtkWidget
 * @allocation: position and size to be allocated to @widget
 *
 * INTERNAL FUNCTION used by containers to assign a size
 * and position to their child widgets. 
 * 
 **/
void
gtk_widget_size_allocate (GtkWidget	*widget,
			  GtkAllocation *allocation)
{
  GtkWidgetAuxInfo *aux_info;
  GtkAllocation real_allocation;
  gboolean needs_draw = FALSE;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  real_allocation = *allocation;
  aux_info = gtk_object_get_data_by_id (GTK_OBJECT (widget), aux_info_key_id);
  
  if (aux_info)
    {
      if (aux_info->x != -1)
	real_allocation.x = aux_info->x;
      if (aux_info->y != -1)
	real_allocation.y = aux_info->y;
    }

  real_allocation.width = MAX (real_allocation.width, 1);
  real_allocation.height = MAX (real_allocation.height, 1);

  if (real_allocation.width < 0 || real_allocation.height < 0)
    {
      g_warning ("gtk_widget_size_allocate(): attempt to allocate widget with width %d and height %d",
		 real_allocation.width,
		 real_allocation.height);
      real_allocation.width = 1;
      real_allocation.height = 1;
    }
  
  if (GTK_WIDGET_NO_WINDOW (widget))
    {
      if (widget->allocation.x != real_allocation.x ||
	  widget->allocation.y != real_allocation.y ||
	  widget->allocation.width != real_allocation.width ||
	  widget->allocation.height != real_allocation.height)
	{
	  gtk_widget_queue_clear_child (widget);
	  needs_draw = TRUE;
	}
    }
  else if (widget->allocation.width != real_allocation.width ||
	   widget->allocation.height != real_allocation.height)
    {
      needs_draw = TRUE;
    }

  if (GTK_IS_RESIZE_CONTAINER (widget))
    gtk_container_clear_resize_widgets (GTK_CONTAINER (widget));

  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[SIZE_ALLOCATE], &real_allocation);

  if (needs_draw)
    {
      gtk_widget_queue_draw (widget);
      if (widget->parent && GTK_CONTAINER (widget->parent)->reallocate_redraws)
	gtk_widget_queue_draw (widget->parent);
    }
}

static void
gtk_widget_real_size_allocate (GtkWidget     *widget,
			       GtkAllocation *allocation)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  widget->allocation = *allocation;
  
  if (GTK_WIDGET_REALIZED (widget) &&
      !GTK_WIDGET_NO_WINDOW (widget))
     {
	gdk_window_move_resize (widget->window,
				allocation->x, allocation->y,
				allocation->width, allocation->height);
     }
}

static void
gtk_widget_stop_add_accelerator (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_signal_emit_stop (GTK_OBJECT (widget), widget_signals[ADD_ACCELERATOR]);
}

static void
gtk_widget_stop_remove_accelerator (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_signal_emit_stop (GTK_OBJECT (widget), widget_signals[REMOVE_ACCELERATOR]);
}

void
gtk_widget_lock_accelerators (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (!gtk_widget_accelerators_locked (widget))
    {
      gtk_signal_connect (GTK_OBJECT (widget),
			  "add_accelerator",
			  GTK_SIGNAL_FUNC (gtk_widget_stop_add_accelerator),
			  NULL);
      gtk_signal_connect (GTK_OBJECT (widget),
			  "remove_accelerator",
			  GTK_SIGNAL_FUNC (gtk_widget_stop_remove_accelerator),
			  NULL);
    }
}

void
gtk_widget_unlock_accelerators (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (gtk_widget_accelerators_locked (widget))
    {
      gtk_signal_disconnect_by_func (GTK_OBJECT (widget),
				     GTK_SIGNAL_FUNC (gtk_widget_stop_add_accelerator),
				     NULL);
      gtk_signal_disconnect_by_func (GTK_OBJECT (widget),
				     GTK_SIGNAL_FUNC (gtk_widget_stop_remove_accelerator),
				     NULL);
    }
}

gboolean
gtk_widget_accelerators_locked (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  
  return gtk_signal_handler_pending_by_func (GTK_OBJECT (widget),
					     widget_signals[ADD_ACCELERATOR],
					     TRUE,
					     GTK_SIGNAL_FUNC (gtk_widget_stop_add_accelerator),
					     NULL) > 0;
}

void
gtk_widget_add_accelerator (GtkWidget           *widget,
			    const gchar         *accel_signal,
			    GtkAccelGroup       *accel_group,
			    guint                accel_key,
			    guint                accel_mods,
			    GtkAccelFlags        accel_flags)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (accel_group != NULL);

  gtk_accel_group_add (accel_group,
		       accel_key,
		       accel_mods,
		       accel_flags,
		       (GtkObject*) widget,
		       accel_signal);
}

void
gtk_widget_remove_accelerator (GtkWidget           *widget,
			       GtkAccelGroup       *accel_group,
			       guint                accel_key,
			       guint                accel_mods)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (accel_group != NULL);

  gtk_accel_group_remove (accel_group,
			  accel_key,
			  accel_mods,
			  (GtkObject*) widget);
}

void
gtk_widget_remove_accelerators (GtkWidget           *widget,
				const gchar         *accel_signal,
				gboolean             visible_only)
{
  GSList *slist;
  guint signal_id;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (accel_signal != NULL);
  
  signal_id = gtk_signal_lookup (accel_signal, GTK_OBJECT_TYPE (widget));
  g_return_if_fail (signal_id != 0);
  
  slist = gtk_accel_group_entries_from_object (GTK_OBJECT (widget));
  while (slist)
    {
      GtkAccelEntry *ac_entry;
      
      ac_entry = slist->data;
      slist = slist->next;
      if (ac_entry->accel_flags & GTK_ACCEL_VISIBLE &&
	  ac_entry->signal_id == signal_id)
	gtk_widget_remove_accelerator (GTK_WIDGET (widget),
				       ac_entry->accel_group,
				       ac_entry->accelerator_key,
				       ac_entry->accelerator_mods);
    }
}

guint
gtk_widget_accelerator_signal (GtkWidget           *widget,
			       GtkAccelGroup       *accel_group,
			       guint                accel_key,
			       guint                accel_mods)
{
  GtkAccelEntry *ac_entry;

  g_return_val_if_fail (widget != NULL, 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  g_return_val_if_fail (accel_group != NULL, 0);

  ac_entry = gtk_accel_group_get_entry (accel_group, accel_key, accel_mods);

  if (ac_entry && ac_entry->object == (GtkObject*) widget)
    return ac_entry->signal_id;
  return 0;
}

static gint
gtk_widget_real_key_press_event (GtkWidget         *widget,
				 GdkEventKey       *event)
{
  gboolean handled = FALSE;

  g_return_val_if_fail (widget != NULL, handled);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), handled);
  g_return_val_if_fail (event != NULL, handled);

  if (!handled)
    handled = gtk_bindings_activate (GTK_OBJECT (widget),
				     event->keyval,
				     event->state);

  return handled;
}

static gint
gtk_widget_real_key_release_event (GtkWidget         *widget,
				   GdkEventKey       *event)
{
  gboolean handled = FALSE;

  g_return_val_if_fail (widget != NULL, handled);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), handled);
  g_return_val_if_fail (event != NULL, handled);

  if (!handled)
    handled = gtk_bindings_activate (GTK_OBJECT (widget),
				     event->keyval,
				     event->state | GDK_RELEASE_MASK);

  return handled;
}

/**
 * gtk_widget_event:
 * @widget: a #GtkWidget
 * @event: a #GdkEvent
 * 
 * INTERNAL FUNCTION. This function is used to emit
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
gint
gtk_widget_event (GtkWidget *widget,
		  GdkEvent  *event)
{
  gboolean return_val;
  gint signal_num;

  g_return_val_if_fail (widget != NULL, TRUE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);

  gtk_widget_ref (widget);
  return_val = FALSE;
  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[EVENT], event,
		   &return_val);
  if (return_val || GTK_OBJECT_DESTROYED (widget))
    goto out;

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
      if (event->focus_change.in)
	signal_num = FOCUS_IN_EVENT;
      else
	signal_num = FOCUS_OUT_EVENT;
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
      if (!event->any.window)	/* Why is this necessary */
	goto out;

      signal_num = EXPOSE_EVENT;
      break;
    case GDK_VISIBILITY_NOTIFY:
      signal_num = VISIBILITY_NOTIFY_EVENT;
      break;
    default:
      g_warning ("could not determine signal number for event: %d", event->type);
      goto out;
    }
  
  if (signal_num != -1)
    gtk_signal_emit (GTK_OBJECT (widget), widget_signals[signal_num], event, &return_val);

  return_val |= GTK_OBJECT_DESTROYED (widget);

 out:
  gtk_widget_unref (widget);

  return return_val;
}

/**
 * gtk_widget_activate:
 * @widget: a #GtkWidget that's activatable
 * 
 * For widgets that can be "activated" (buttons, menu items, etc.)
 * this function activates them. Activation is what happens when you
 * press Enter on a widget during key navigation; clicking a button,
 * selecting a menu item, etc. If @widget isn't activatable,
 * the function returns %FALSE.
 * 
 * Return value: %TRUE if the widget was activatable
 **/
gboolean
gtk_widget_activate (GtkWidget *widget)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  
  if (WIDGET_CLASS (widget)->activate_signal)
    {
      /* FIXME: we should eventually check the signals signature here */
      gtk_signal_emit (GTK_OBJECT (widget), WIDGET_CLASS (widget)->activate_signal);

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
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  if (hadjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (hadjustment), FALSE);
  if (vadjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (vadjustment), FALSE);

  if (WIDGET_CLASS (widget)->set_scroll_adjustments_signal)
    {
      /* FIXME: we should eventually check the signals signature here */
      gtk_signal_emit (GTK_OBJECT (widget),
		       WIDGET_CLASS (widget)->set_scroll_adjustments_signal,
		       hadjustment, vadjustment);
      return TRUE;
    }
  else
    return FALSE;
}

/*****************************************
 * gtk_widget_reparent_container_child:
 *   assistent function to gtk_widget_reparent
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_reparent_container_child (GtkWidget *widget,
				     gpointer   client_data)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (client_data != NULL);
  
  if (GTK_WIDGET_NO_WINDOW (widget))
    {
      if (widget->window)
	gdk_window_unref (widget->window);
      widget->window = (GdkWindow*) client_data;
      if (widget->window)
	gdk_window_ref (widget->window);

      if (GTK_IS_CONTAINER (widget))
	gtk_container_forall (GTK_CONTAINER (widget),
			      gtk_widget_reparent_container_child,
			      client_data);
    }
  else
    gdk_window_reparent (widget->window, 
			 (GdkWindow*) client_data, 0, 0);
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
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (new_parent != NULL);
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
      
      gtk_widget_ref (widget);
      gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
      gtk_container_add (GTK_CONTAINER (new_parent), widget);
      gtk_widget_unref (widget);
      
      if (GTK_WIDGET_IN_REPARENT (widget))
	{
	  GTK_PRIVATE_UNSET_FLAG (widget, GTK_IN_REPARENT);
	  
	  gtk_widget_reparent_container_child (widget,
					       gtk_widget_get_parent_window (widget));
	}
    }
}

/**
 * gtk_widget_popup:
 * @widget: 
 * @x: 
 * @y: 
 *
 * DEPRECATED. Completely useless function as far as we know.
 * Probably does something bad.
 * 
 **/
void
gtk_widget_popup (GtkWidget *widget,
		  gint	     x,
		  gint	     y)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (!GTK_WIDGET_VISIBLE (widget))
    {
      if (!GTK_WIDGET_REALIZED (widget))
	gtk_widget_realize (widget);
      if (!GTK_WIDGET_NO_WINDOW (widget))
	gdk_window_move (widget->window, x, y);
      gtk_widget_show (widget);
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
  
  g_return_val_if_fail (widget != NULL, FALSE);
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
 * gtk_widget_grab_focus:
 * @widget: a #GtkWidget
 * 
 * Causes @widget to have the keyboard focus for the #GtkWindow it's
 * inside. @widget must be a focusable widget, such as a #GtkEntry;
 * something like #GtkFrame won't work. (More precisely, it must have the
 * #GTK_CAN_FOCUS flag set.)
 * 
 **/
void
gtk_widget_grab_focus (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[GRAB_FOCUS]);
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
  g_return_if_fail (focus_widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (focus_widget));
  
  if (GTK_WIDGET_CAN_FOCUS (focus_widget))
    {
      GtkWidget *toplevel;
      GtkWidget *widget;
      
      /* clear the current focus setting, break if the current widget
       * is the focus widget's parent, since containers above that will
       * be set by the next loop.
       */
      toplevel = gtk_widget_get_toplevel (focus_widget);
      if (GTK_IS_WINDOW (toplevel))
	{
	  widget = GTK_WINDOW (toplevel)->focus_widget;
	  
	  if (widget == focus_widget)
	    {
	      /* We call gtk_window_set_focus() here so that the
	       * toplevel window can request the focus if necessary.
	       * This is needed when the toplevel is a GtkPlug
	       */
	      if (!GTK_WIDGET_HAS_FOCUS (widget))
		gtk_window_set_focus (GTK_WINDOW (toplevel), focus_widget);

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
	gtk_window_set_focus (GTK_WINDOW (widget), focus_widget);
    }
}

/**
 * gtk_widget_grab_default:
 * @widget: a #GtkWidget
 *
 * Causes @widget to become the default widget. @widget must have the
 * #GTK_CAN_DEFAULT flag set; typically you have to set this flag
 * yourself by calling GTK_WIDGET_SET_FLAGS (@widget,
 * GTK_CAN_DEFAULT).  The default widget is activated when the user
 * presses Enter in a window.  Default widgets must be activatable,
 * that is, gtk_widget_activate() should affect them.
 * 
 **/
void
gtk_widget_grab_default (GtkWidget *widget)
{
  GtkWidget *window;
  GtkType window_type;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_CAN_DEFAULT (widget));
  
  window_type = GTK_TYPE_WINDOW;
  window = widget->parent;
  
  while (window && !gtk_type_is_a (GTK_WIDGET_TYPE (window), window_type))
    window = window->parent;
  
  if (window && gtk_type_is_a (GTK_WIDGET_TYPE (window), window_type))
    gtk_window_set_default (GTK_WINDOW (window), widget);
  else
    g_warning("gtk_widget_grab_default() called on a widget not within a GtkWindow");
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
 **/
void
gtk_widget_set_name (GtkWidget	 *widget,
		     const gchar *name)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (widget->name)
    g_free (widget->name);
  widget->name = g_strdup (name);

  if (GTK_WIDGET_RC_STYLE (widget))
    gtk_widget_set_rc_style (widget);
}

/**
 * gtk_widget_get_name:
 * @widget: a #GtkWidget
 * 
 * Retrieves the name of a widget. The return value should not be
 * freed. See gtk_widget_set_name() for the significance of widget
 * names.
 * 
 * Return value: name of the widget
 **/
gchar*
gtk_widget_get_name (GtkWidget *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  if (widget->name)
    return widget->name;
  return gtk_type_name (GTK_WIDGET_TYPE (widget));
}

/**
 * gtk_widget_set_state:
 * @widget: a #GtkWidget
 * @state: new state for @widget
 *
 * INTERNAL FUNCTION for use in widget implementations. Sets the state
 * of a widget (insensitive, prelighted, etc.) Usually you should set
 * the state using wrapper functions such as gtk_widget_set_sensitive().
 * 
 **/
void
gtk_widget_set_state (GtkWidget           *widget,
		      GtkStateType         state)
{
  g_return_if_fail (widget != NULL);
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
	gtk_widget_queue_clear (widget);
    }
}

void
gtk_widget_set_app_paintable (GtkWidget *widget,
			      gboolean   app_paintable)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  app_paintable = (app_paintable != FALSE);

  if (GTK_WIDGET_APP_PAINTABLE (widget) != app_paintable)
    {
      if (app_paintable)
	GTK_WIDGET_SET_FLAGS (widget, GTK_APP_PAINTABLE);
      else
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_APP_PAINTABLE);

      if (GTK_WIDGET_DRAWABLE (widget))
	gtk_widget_queue_clear (widget);
    }
}

/**
 * gtk_widget_set_double_buffered:
 * @widget: a #GtkWidget
 * @double_buffered: %TRUE to double-buffer a widget
 *
 * Widgets are double buffered by default; you can use this function
 * to turn off the buffering. "Double buffered" simply means that
 * gdk_window_begin_paint() and gdk_window_end_paint() are called
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
 **/
void
gtk_widget_set_double_buffered (GtkWidget *widget,
				gboolean   double_buffered)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (double_buffered)
    GTK_WIDGET_SET_FLAGS (widget, GTK_DOUBLE_BUFFERED);
  else
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_DOUBLE_BUFFERED);
}

/**
 * gtk_widget_set_sensitive:
 * @widget: a @widget
 * @sensitive: %TRUE to make the widget sensitive
 *
 * Sets the sensitivity of a widget. A widget is sensitive if the user
 * can interact with it. Insensitive widgets are "grayed out" and the
 * user can't interact with them. Insensitive widgets are known as
 * "inactive" in some other toolkits.
 * 
 **/
void
gtk_widget_set_sensitive (GtkWidget *widget,
			  gboolean   sensitive)
{
  GtkStateData data;

  g_return_if_fail (widget != NULL);
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
    gtk_widget_queue_clear (widget);
}

/**
 * gtk_widget_set_parent:
 * @widget: a #GtkWidget
 * @parent: parent container
 *
 * INTERNAL FUNCTION for use while implementing subclasses of #GtkContainer.
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
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->parent == NULL);
  g_return_if_fail (!GTK_WIDGET_TOPLEVEL (widget));
  g_return_if_fail (parent != NULL);
  g_return_if_fail (GTK_IS_WIDGET (parent));
  g_return_if_fail (widget != parent);

  /* keep this function in sync with gtk_menu_attach_to_widget()
   */

  gtk_widget_ref (widget);
  gtk_object_sink (GTK_OBJECT (widget));
  widget->parent = parent;

  if (GTK_WIDGET_STATE (parent) != GTK_STATE_NORMAL)
    data.state = GTK_WIDGET_STATE (parent);
  else
    data.state = GTK_WIDGET_STATE (widget);
  data.state_restoration = FALSE;
  data.parent_sensitive = (GTK_WIDGET_IS_SENSITIVE (parent) != FALSE);
  data.use_forall = GTK_WIDGET_IS_SENSITIVE (parent) != GTK_WIDGET_IS_SENSITIVE (widget);

  gtk_widget_propagate_state (widget, &data);
  
  gtk_widget_set_style_recurse (widget, NULL);

  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[PARENT_SET], NULL);
}

/*****************************************
 * Widget styles
 * see docs/styles.txt
 *****************************************/

/**
 * gtk_widget_set_style:
 * @widget: a #GtkWidget
 * @style: a #GtkStyle
 *
 * Sets the #GtkStyle for a widget (widget->style). You probably don't
 * want to use this function; it interacts badly with themes, because
 * themes work by replacing the #GtkStyle. Instead, use
 * gtk_widget_modify_style().
 * 
 **/
void
gtk_widget_set_style (GtkWidget *widget,
		      GtkStyle	*style)
{
  GtkStyle *default_style;
  gboolean initial_emission;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (style != NULL);

  initial_emission = !GTK_WIDGET_RC_STYLE (widget) && !GTK_WIDGET_USER_STYLE (widget);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_RC_STYLE);
  GTK_PRIVATE_SET_FLAG (widget, GTK_USER_STYLE);

  default_style = gtk_object_get_data_by_id (GTK_OBJECT (widget), saved_default_style_key_id);
  if (!default_style)
    {
      gtk_style_ref (widget->style);
      if (!saved_default_style_key_id)
	saved_default_style_key_id = g_quark_from_static_string (saved_default_style_key);
      gtk_object_set_data_by_id (GTK_OBJECT (widget), saved_default_style_key_id, widget->style);
    }

  gtk_widget_set_style_internal (widget, style, initial_emission);
}

/**
 * gtk_widget_ensure_style:
 * @widget: a #GtkWidget
 *
 * Ensures that @widget has a style (widget->style). Not a very useful
 * function; most of the time, if you want the style, the widget is
 * realized, and realized widgets are guaranteed to have a style
 * already.
 * 
 **/
void
gtk_widget_ensure_style (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!GTK_WIDGET_USER_STYLE (widget) &&
      !GTK_WIDGET_RC_STYLE (widget))
    gtk_widget_set_rc_style (widget);
}

/**
 * gtk_widget_set_rc_style:
 * @widget: a #GtkWidget
 * 
 * 
 **/
void
gtk_widget_set_rc_style (GtkWidget *widget)
{
  GtkStyle *saved_style;
  GtkStyle *new_style;
  gboolean initial_emission;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  initial_emission = !GTK_WIDGET_RC_STYLE (widget) && !GTK_WIDGET_USER_STYLE (widget);

  GTK_PRIVATE_UNSET_FLAG (widget, GTK_USER_STYLE);
  GTK_WIDGET_SET_FLAGS (widget, GTK_RC_STYLE);

  saved_style = gtk_object_get_data_by_id (GTK_OBJECT (widget), saved_default_style_key_id);
  new_style = gtk_rc_get_style (widget);
  if (new_style)
    {
      if (!saved_style)
	{
	  gtk_style_ref (widget->style);
	  if (!saved_default_style_key_id)
	    saved_default_style_key_id = g_quark_from_static_string (saved_default_style_key);
	  gtk_object_set_data_by_id (GTK_OBJECT (widget), saved_default_style_key_id, widget->style);
	}
      gtk_widget_set_style_internal (widget, new_style, initial_emission);
    }
  else
    {
      if (saved_style)
	{
	  g_assert (initial_emission == FALSE); /* FIXME: remove this line */

	  gtk_object_remove_data_by_id (GTK_OBJECT (widget), saved_default_style_key_id);
	  gtk_widget_set_style_internal (widget, saved_style, initial_emission);
	  gtk_style_unref (saved_style);
	}
      else
	{
	  if (initial_emission)
	    gtk_widget_set_style_internal (widget, widget->style, TRUE);
	}
    }
}

void
gtk_widget_restore_default_style (GtkWidget *widget)
{
  GtkStyle *default_style;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  GTK_PRIVATE_UNSET_FLAG (widget, GTK_USER_STYLE);

  default_style = gtk_object_get_data_by_id (GTK_OBJECT (widget), saved_default_style_key_id);
  if (default_style)
    {
      gtk_object_remove_data_by_id (GTK_OBJECT (widget), saved_default_style_key_id);
      gtk_widget_set_style_internal (widget, default_style, FALSE);
      gtk_style_unref (default_style);
    }
}

/**
 * gtk_widget_get_style:
 * @widget: a #GtkWidget
 * 
 * Simply an accessor function that returns widget->style.
 * 
 * Return value: the widget's #GtkStyle
 **/
GtkStyle*
gtk_widget_get_style (GtkWidget *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  return widget->style;
}

/**
 * gtk_widget_modify_style:
 * @widget: a #GtkWidget
 * @style: the #GtkRcStyle holding the style modifications
 * 
 * Modify style values on the widget. Modifications made using this
 * technique take precendence over style values set via an RC file,
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
 * to such functions gtk_widget_modify_fg() will be have a cumulative
 * effect with the inital modifications.
 **/
void       
gtk_widget_modify_style (GtkWidget      *widget,
			 GtkRcStyle     *style)
{
  GtkRcStyle *old_style;

  g_return_if_fail (GTK_IS_RC_STYLE (style));
  
  if (!rc_style_key_id)
    rc_style_key_id = g_quark_from_static_string (rc_style_key);

  old_style = gtk_object_get_data_by_id (GTK_OBJECT (widget),
					 rc_style_key_id);

  if (style != old_style)
    gtk_object_set_data_by_id_full (GTK_OBJECT (widget),
				    rc_style_key_id,
				    gtk_rc_style_copy (style),
				    (GtkDestroyNotify)gtk_rc_style_unref);

  if (GTK_WIDGET_RC_STYLE (widget))
    gtk_widget_set_rc_style (widget);
}

/**
 * gtk_widget_get_modifier_style:
 * @widget: a #GtkWidget
 * 
 * Return the current modifier style for the widget. (As set by
 * gtk_widget_modify_style().) If no style has previously set, a new
 * #GtkRcStyle will be created with all values unset, and set as the
 * modifier style for the widget. If you make changes to this rc
 * style, you must call gtk_widget_modify_style(), passing in the
 * returned rc style, to make sure that your changes take effect.
 * 
 * Return value: the modifier style for the widget. This rc style is
 *   owned by the widget. If you want to keep a pointer to value this
 *   around, you must add a refcount using gtk_rc_style_ref().
 **/
GtkRcStyle *
gtk_widget_get_modifier_style (GtkWidget      *widget)
{
  GtkRcStyle *rc_style;
  
  if (!rc_style_key_id)
    rc_style_key_id = g_quark_from_static_string (rc_style_key);
  
  rc_style = gtk_object_get_data_by_id (GTK_OBJECT (widget),
					rc_style_key_id);

  if (!rc_style)
    {
      rc_style = gtk_rc_style_new();
      gtk_object_set_data_by_id_full (GTK_OBJECT (widget),
				      rc_style_key_id,
				      rc_style,
				      (GtkDestroyNotify)gtk_rc_style_unref);
    }

  return rc_style;
}

static void
gtk_widget_modify_color_component (GtkWidget     *widget,
				   GtkRcFlags     component,
				   GtkStateType   state,
				   GdkColor      *color)
{
  GtkRcStyle *rc_style = gtk_widget_get_modifier_style (widget);  

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

  if (GTK_WIDGET_RC_STYLE (widget))
    gtk_widget_set_rc_style (widget);
}

/**
 * gtk_widget_modify_fg:
 * @widget: a #GtkWidget
 * @state: the state for which to set the foreground color.
 * @color: the color to assign (does not need to be allocated)
 * 
 * Set the foreground color for a widget in a particular state.  All
 * other style values are left untouched. See also
 * gtk_widget_modify_style().
 **/
void
gtk_widget_modify_fg (GtkWidget   *widget,
		      GtkStateType state,
		      GdkColor    *color)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);
  g_return_if_fail (color != NULL);

  gtk_widget_modify_color_component (widget, GTK_RC_FG, state, color);
}

/**
 * gtk_widget_modify_bg:
 * @widget: a #GtkWidget
 * @state: the state for which to set the foreground color.
 * @color: the color to assign (does not need to be allocated)
 * 
 * Set the background color for a widget in a particular state.  All
 * other style values are left untouched. See also
 * gtk_widget_modify_style().
 **/
void
gtk_widget_modify_bg (GtkWidget   *widget,
		      GtkStateType state,
		      GdkColor    *color)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);
  g_return_if_fail (color != NULL);

  gtk_widget_modify_color_component (widget, GTK_RC_BG, state, color);
}

/**
 * gtk_widget_modify_base:
 * @widget: a #GtkWidget
 * @state: the state for which to set the foreground color.
 * @color: the color to assign (does not need to be allocated)
 * 
 * Set the text color for a widget in a particular state.  All other
 * style values are left untouched. The text color is the foreground
 * color used along with the base color (see gtk_widget_modify_base)
 * for widgets such as #GtkEntry and #GtkTextView. See also
 * gtk_widget_modify_style().
 **/
void
gtk_widget_modify_text (GtkWidget   *widget,
			GtkStateType state,
			GdkColor    *color)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);
  g_return_if_fail (color != NULL);

  gtk_widget_modify_color_component (widget, GTK_RC_TEXT, state, color);
}

/**
 * gtk_widget_modify_base:
 * @widget: a #GtkWidget
 * @state: the state for which to set the foreground color.
 * @color: the color to assign (does not need to be allocated)
 * 
 * Set the text color for a widget in a particular state.
 * All other style values are left untouched. The base color
 * is the background color used along with the text color
 * (see gtk_widget_modify_text) for widgets such as #GtkEntry
 * and #GtkTextView. See also gtk_widget_modify_style().
 **/
void
gtk_widget_modify_base (GtkWidget  *widget,
			GtkStateType state,
			GdkColor    *color)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);
  g_return_if_fail (color != NULL);

  gtk_widget_modify_color_component (widget, GTK_RC_BASE, state, color);
}

/**
 * gtk_widget_modify_font:
 * @widget: a #GtkWidget
 * @font_desc: the font description to use
 * 
 * Set the font to use for a widget.  All other style values are left
 * untouched. See also gtk_widget_modify_style().
 **/
void
gtk_widget_modify_font (GtkWidget            *widget,
			PangoFontDescription *font_desc)
{
  GtkRcStyle *rc_style;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (font_desc != NULL);

  rc_style = gtk_widget_get_modifier_style (widget);  

  if (rc_style->font_desc)
    pango_font_description_free (rc_style->font_desc);
  
  rc_style->font_desc = pango_font_description_copy (font_desc);
  
  if (GTK_WIDGET_RC_STYLE (widget))
    gtk_widget_set_rc_style (widget);
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
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (style != NULL);

  if (widget->style != style || initial_emission)
    {
      PangoContext *context = gtk_widget_peek_pango_context (widget);
      if (context)
	pango_context_set_font_description (context, style->font_desc);
    }
  
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
      gtk_style_ref (widget->style);
      
      if (GTK_WIDGET_REALIZED (widget))
	widget->style = gtk_style_attach (widget->style, widget->window);

      gtk_signal_emit (GTK_OBJECT (widget),
		       widget_signals[STYLE_SET],
		       initial_emission ? NULL : previous_style);
      gtk_style_unref (previous_style);

      if (widget->parent && !initial_emission)
	{
	  GtkRequisition old_requisition;
	  
	  old_requisition = widget->requisition;
	  gtk_widget_size_request (widget, NULL);
	  
	  if ((old_requisition.width != widget->requisition.width) ||
	      (old_requisition.height != widget->requisition.height))
	    gtk_widget_queue_resize (widget);
	  else if (GTK_WIDGET_DRAWABLE (widget))
	    gtk_widget_queue_clear (widget);
	}
    }
  else if (initial_emission)
    {
      gtk_signal_emit (GTK_OBJECT (widget),
		       widget_signals[STYLE_SET],
		       NULL);
    }
}

static void
gtk_widget_set_style_recurse (GtkWidget *widget,
			      gpointer	 client_data)
{
  if (GTK_WIDGET_RC_STYLE (widget))
    gtk_widget_set_rc_style (widget);
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  gtk_widget_set_style_recurse,
			  NULL);
}

void
gtk_widget_reset_rc_styles (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_style_recurse (widget, NULL);
}

void
gtk_widget_set_default_style (GtkStyle *style)
{
  if (style != gtk_default_style)
     {
       if (gtk_default_style)
	 gtk_style_unref (gtk_default_style);
       gtk_default_style = style;
       if (gtk_default_style)
	 gtk_style_ref (gtk_default_style);
     }
}

GtkStyle*
gtk_widget_get_default_style (void)
{
  if (!gtk_default_style)
    {
      gtk_default_style = gtk_style_new ();
      gtk_style_ref (gtk_default_style);
    }
  
  return gtk_default_style;
}

void
gtk_widget_push_style (GtkStyle *style)
{
  g_return_if_fail (style != NULL);

  gtk_style_ref (style);
  style_stack = g_slist_prepend (style_stack, style);
}

static GtkStyle*
gtk_widget_peek_style (void)
{
  if (style_stack)
    return (GtkStyle*) style_stack->data;
  else
    return gtk_widget_get_default_style ();
}

void
gtk_widget_pop_style (void)
{
  GSList *tmp;
  
  if (style_stack)
    {
      tmp = style_stack;
      style_stack = style_stack->next;
      gtk_style_unref ((GtkStyle*) tmp->data);
      g_slist_free_1 (tmp);
    }
}

static PangoContext *
gtk_widget_peek_pango_context (GtkWidget *widget)
{
  if (!pango_context_key_id)
    pango_context_key_id = g_quark_from_static_string (pango_context_key);

  return gtk_object_get_data_by_id (GTK_OBJECT (widget), pango_context_key_id);
}

/**
 * gtk_widget_get_pango_context:
 * @widget: a #GtkWidget
 * 
 * Get a #PangoContext with the appropriate colormap, font description
 * and base direction for this widget. Unlike the context returned
 * by gtk_widget_create_pango_context(), this context is owned by
 * the widget (it can be used as long as widget exists), and will
 * be updated to match any changes to the widget's attributes.
 *
 * If you create and keep a #PangoLayout using this context, you must
 * deal with changes to the context by calling pango_layout_context_changed()
 * on the layout in response to the ::style_set and ::direction_set signals
 * for the widget.
 *
 * Return value: the #PangoContext for the widget.
 **/
PangoContext *
gtk_widget_get_pango_context (GtkWidget *widget)
{
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  if (!pango_context_key_id)
    pango_context_key_id = g_quark_from_static_string (pango_context_key);

  context = gtk_object_get_data_by_id (GTK_OBJECT (widget), pango_context_key_id);
  if (!context)
    {
      context = gtk_widget_create_pango_context (GTK_WIDGET (widget));
      gtk_object_set_data_by_id_full (GTK_OBJECT (widget), pango_context_key_id, context,
				      (GDestroyNotify)g_object_unref);
    }

  return context;
}

/**
 * gtk_widget_create_pango_context:
 * @widget: a #PangoWidget
 * 
 * Create a new pango context with the appropriate colormap,
 * font description, and base direction for drawing text for
 * this widget. See also gtk_widget_get_pango_context()
 * 
 * Return value: the new #PangoContext
 **/
PangoContext *
gtk_widget_create_pango_context (GtkWidget *widget)
{
  PangoContext *context;
  char *lang;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  context = gdk_pango_context_get ();

  gdk_pango_context_set_colormap (context, gtk_widget_get_colormap (widget));
  pango_context_set_base_dir (context,
			      gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR ?
			        PANGO_DIRECTION_LTR : PANGO_DIRECTION_RTL);
  pango_context_set_font_description (context, widget->style->font_desc);

  lang = gtk_get_default_language ();  
  pango_context_set_lang (context, lang);
  g_free (lang);

  return context;
}

/**
 * gtk_widget_create_pango_layout:
 * @widget: a #PangoWidget
 * @text:   text to set on the layout (can be %NULL)
 * 
 * Create a new #PangoLayout with the appropriate colormap,
 * font description, and base direction for drawing text for
 * this widget.
 *
 * If you keep a #PangoLayout created in this way around, in order
 * notify the layout of changes to the base direction or font of this
 * widget, you must call pango_layout_context_changed() in response to
 * the ::style_set and ::direction_set signals for the widget.
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
 * gtk_widget_render_stock_icon:
 * @widget: a #GtkWidget
 * @stock_id: a stock ID
 * @size: a stock size
 * @detail: render detail to pass to theme engine
 * 
 * A convenience function that uses the theme engine and RC file
 * settings for @widget to look up @stock_id and render it to
 * a pixbuf. @stock_id should be a stock icon ID such as
 * #GTK_STOCK_OPEN or #GTK_STOCK_BUTTON_OK. @size should be a size
 * such as #GTK_ICON_SIZE_MENU. @detail should be a string that
 * identifies the widget or code doing the rendering, so that
 * theme engines can special-case rendering for that widget or code.
 * 
 * Return value: a new pixbuf, or NULL if the stock ID wasn't known
 **/
GdkPixbuf*
gtk_widget_render_stock_icon (GtkWidget      *widget,
                              const gchar    *stock_id,
                              const gchar    *size,
                              const gchar    *detail)
{
  GtkIconSet *icon_set;
  GdkPixbuf *retval;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);
  g_return_val_if_fail (size != NULL, NULL);
  
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

/*************************************************************
 * gtk_widget_set_parent_window:
 *     Set a non default parent window for widget
 *
 *   arguments:
 *     widget:
 *     parent_window 
 *     
 *   results:
 *************************************************************/

void
gtk_widget_set_parent_window   (GtkWidget           *widget,
				GdkWindow           *parent_window)
{
  GdkWindow *old_parent_window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  old_parent_window = gtk_object_get_data_by_id (GTK_OBJECT (widget),
						 parent_window_key_id);

  if (parent_window != old_parent_window)
    {
      if (!parent_window_key_id)
	parent_window_key_id = g_quark_from_static_string (parent_window_key);
      gtk_object_set_data_by_id (GTK_OBJECT (widget), parent_window_key_id, 
				 parent_window);
      if (old_parent_window)
	gdk_window_unref (old_parent_window);
      if (parent_window)
	gdk_window_ref (parent_window);
    }
}

/*************************************************************
 * gtk_widget_get_parent_window:
 *     Get widget's parent window
 *
 *   arguments:
 *     widget:
 *     
 *   results:
 *     parent window
 *************************************************************/

GdkWindow *
gtk_widget_get_parent_window   (GtkWidget           *widget)
{
  GdkWindow *parent_window;

  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (widget->parent != NULL, NULL);
  
  parent_window = gtk_object_get_data_by_id (GTK_OBJECT (widget),
					     parent_window_key_id);

  return (parent_window != NULL) ? parent_window : widget->parent->window;
}

/*****************************************
 * gtk_widget_set_uposition:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_set_uposition (GtkWidget *widget,
			  gint	     x,
			  gint	     y)
{
  GtkWidgetAuxInfo *aux_info;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  aux_info = gtk_object_get_data_by_id (GTK_OBJECT (widget), aux_info_key_id);
  if (!aux_info)
    {
      if (!aux_info_key_id)
	aux_info_key_id = g_quark_from_static_string (aux_info_key);
      aux_info = gtk_widget_aux_info_new ();
      gtk_object_set_data_by_id (GTK_OBJECT (widget), aux_info_key_id, aux_info);
    }

  /* keep this in sync with gtk_window_compute_reposition() */
  
  if (x > -2)
    aux_info->x = x;
  if (y > -2)
    aux_info->y = y;
  
  if (GTK_IS_WINDOW (widget) && (aux_info->x != -1) && (aux_info->y != -1))
    gtk_window_reposition (GTK_WINDOW (widget), x, y);
  
  if (GTK_WIDGET_VISIBLE (widget) && widget->parent)
    gtk_widget_size_allocate (widget, &widget->allocation);
}

/*****************************************
 * gtk_widget_set_usize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_set_usize (GtkWidget *widget,
		      gint	 width,
		      gint	 height)
{
  GtkWidgetAuxInfo *aux_info;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  aux_info = gtk_object_get_data_by_id (GTK_OBJECT (widget), aux_info_key_id);
  if (!aux_info)
    {
      if (!aux_info_key_id)
	aux_info_key_id = g_quark_from_static_string (aux_info_key);
      aux_info = gtk_widget_aux_info_new ();
      gtk_object_set_data_by_id (GTK_OBJECT (widget), aux_info_key_id, aux_info);
    }
  
  if (width > -2)
    aux_info->width = width;
  if (height > -2)
    aux_info->height = height;
  
  if (GTK_WIDGET_VISIBLE (widget))
    gtk_widget_queue_resize (widget);
}

/*****************************************
 * gtk_widget_set_events:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_set_events (GtkWidget *widget,
		       gint	  events)
{
  gint *eventp;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!GTK_WIDGET_NO_WINDOW (widget));
  g_return_if_fail (!GTK_WIDGET_REALIZED (widget));
  
  eventp = gtk_object_get_data_by_id (GTK_OBJECT (widget), event_key_id);
  
  if (events)
    {
      if (!eventp)
	eventp = g_new (gint, 1);
      
      *eventp = events;
      if (!event_key_id)
	event_key_id = g_quark_from_static_string (event_key);
      gtk_object_set_data_by_id (GTK_OBJECT (widget), event_key_id, eventp);
    }
  else if (eventp)
    {
      g_free (eventp);
      gtk_object_remove_data_by_id (GTK_OBJECT (widget), event_key_id);
    }
}

/*****************************************
 * gtk_widget_add_events:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_add_events (GtkWidget *widget,
		       gint	  events)
{
  gint *eventp;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!GTK_WIDGET_NO_WINDOW (widget));
  
  eventp = gtk_object_get_data_by_id (GTK_OBJECT (widget), event_key_id);
  
  if (events)
    {
      if (!eventp)
	{
	  eventp = g_new (gint, 1);
	  *eventp = 0;
	}
      
      *eventp |= events;
      if (!event_key_id)
	event_key_id = g_quark_from_static_string (event_key);
      gtk_object_set_data_by_id (GTK_OBJECT (widget), event_key_id, eventp);
    }
  else if (eventp)
    {
      g_free (eventp);
      gtk_object_remove_data_by_id (GTK_OBJECT (widget), event_key_id);
    }

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_set_events (widget->window,
			     gdk_window_get_events (widget->window) | events);
    }
}

/*****************************************
 * gtk_widget_set_extension_events:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_set_extension_events (GtkWidget *widget,
				 GdkExtensionMode mode)
{
  GdkExtensionMode *modep;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  modep = gtk_object_get_data_by_id (GTK_OBJECT (widget), extension_event_key_id);
  
  if (!modep)
    modep = g_new (GdkExtensionMode, 1);
  
  *modep = mode;
  if (!extension_event_key_id)
    extension_event_key_id = g_quark_from_static_string (extension_event_key);
  gtk_object_set_data_by_id (GTK_OBJECT (widget), extension_event_key_id, modep);
}

/*****************************************
 * gtk_widget_get_toplevel:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkWidget*
gtk_widget_get_toplevel (GtkWidget *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  while (widget->parent)
    widget = widget->parent;
  
  return widget;
}

/*****************************************
 * gtk_widget_get_ancestor:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkWidget*
gtk_widget_get_ancestor (GtkWidget *widget,
			 GtkType    widget_type)
{
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  while (widget && !gtk_type_is_a (GTK_WIDGET_TYPE (widget), widget_type))
    widget = widget->parent;
  
  if (!(widget && gtk_type_is_a (GTK_WIDGET_TYPE (widget), widget_type)))
    return NULL;
  
  return widget;
}

/*****************************************
 * gtk_widget_get_colormap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GdkColormap*
gtk_widget_get_colormap (GtkWidget *widget)
{
  GdkColormap *colormap;
  
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  if (widget->window)
    {
      colormap = gdk_window_get_colormap (widget->window);
      /* If window was destroyed previously, we'll get NULL here */
      if (colormap)
	return colormap;
    }
  
  colormap = gtk_object_get_data (GTK_OBJECT (widget), colormap_key);
  if (colormap)
    return colormap;

  return gtk_widget_get_default_colormap ();
}

/*****************************************
 * gtk_widget_get_visual:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GdkVisual*
gtk_widget_get_visual (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gdk_colormap_get_visual (gtk_widget_get_colormap (widget));
}

/*****************************************
 * gtk_widget_set_colormap:
 *    Set the colormap for the widget to the given
 *    value. Widget must not have been previously
 *    realized. This probably should only be used
 *    from an init() function.
 *   arguments:
 *    widget:
 *    colormap:
 *   results:
 *****************************************/

void
gtk_widget_set_colormap (GtkWidget *widget, GdkColormap *colormap)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!GTK_WIDGET_REALIZED (widget));
  g_return_if_fail (colormap != NULL);

  /* FIXME: reference count the colormap.
   */
  
  gtk_object_set_data (GTK_OBJECT (widget), 
		       colormap_key,
		       colormap);
}

/*****************************************
 * gtk_widget_get_events:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

gint
gtk_widget_get_events (GtkWidget *widget)
{
  gint *events;
  
  g_return_val_if_fail (widget != NULL, 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  
  events = gtk_object_get_data_by_id (GTK_OBJECT (widget), event_key_id);
  if (events)
    return *events;
  
  return 0;
}

/*****************************************
 * gtk_widget_get_extension_events:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GdkExtensionMode
gtk_widget_get_extension_events (GtkWidget *widget)
{
  GdkExtensionMode *mode;
  
  g_return_val_if_fail (widget != NULL, 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  
  mode = gtk_object_get_data_by_id (GTK_OBJECT (widget), extension_event_key_id);
  if (mode)
    return *mode;
  
  return 0;
}

/*****************************************
 * gtk_widget_get_pointer:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_get_pointer (GtkWidget *widget,
			gint	  *x,
			gint	  *y)
{
  g_return_if_fail (widget != NULL);
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

/*****************************************
 * gtk_widget_is_ancestor:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

gboolean
gtk_widget_is_ancestor (GtkWidget *widget,
			GtkWidget *ancestor)
{
  g_return_val_if_fail (widget != NULL, FALSE);
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

void
gtk_widget_set_composite_name (GtkWidget   *widget,
			       const gchar *name)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_COMPOSITE_CHILD (widget));
  g_return_if_fail (name != NULL);

  if (!quark_composite_name)
    quark_composite_name = g_quark_from_static_string ("gtk-composite-name");

  gtk_object_set_data_by_id_full (GTK_OBJECT (widget),
				  quark_composite_name,
				  g_strdup (name),
				  g_free);
}

gchar*
gtk_widget_get_composite_name (GtkWidget *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (GTK_WIDGET_COMPOSITE_CHILD (widget) && widget->parent)
    return gtk_container_child_composite_name (GTK_CONTAINER (widget->parent),
					       widget);
  else
    return NULL;
}

void
gtk_widget_push_composite_child (void)
{
  composite_child_stack++;
}

void
gtk_widget_pop_composite_child (void)
{
  if (composite_child_stack)
    composite_child_stack--;
}

/*****************************************
 * gtk_widget_push_colormap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_push_colormap (GdkColormap *cmap)
{
  g_return_if_fail (cmap != NULL);

  colormap_stack = g_slist_prepend (colormap_stack, cmap);
}

/*****************************************
 * gtk_widget_pop_colormap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_pop_colormap (void)
{
  GSList *tmp;
  
  if (colormap_stack)
    {
      tmp = colormap_stack;
      colormap_stack = colormap_stack->next;
      g_slist_free_1 (tmp);
    }
}

/*****************************************
 * gtk_widget_set_default_colormap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_set_default_colormap (GdkColormap *colormap)
{
  if (default_colormap != colormap)
    {
      if (default_colormap)
	gdk_colormap_unref (default_colormap);
      default_colormap = colormap;
      if (default_colormap)
	gdk_colormap_ref (default_colormap);
    }
}

/*****************************************
 * gtk_widget_get_default_colormap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GdkColormap*
gtk_widget_get_default_colormap (void)
{
  if (!default_colormap)
    default_colormap = gdk_colormap_get_system ();
  
  return default_colormap;
}

/*****************************************
 * gtk_widget_get_default_visual:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GdkVisual*
gtk_widget_get_default_visual (void)
{
  return gdk_colormap_get_visual (gtk_widget_get_default_colormap ());
}

static void
gtk_widget_emit_direction_changed (GtkWidget        *widget,
				   GtkTextDirection  old_dir)
{
  PangoContext *context = gtk_widget_peek_pango_context (widget);

  if (context)
    pango_context_set_base_dir (context,
				gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR ?
				  PANGO_DIRECTION_LTR : PANGO_DIRECTION_RTL);
  
  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[DIRECTION_CHANGED], old_dir);
}

/**
 * gtk_widget_set_direction:
 * @widget: a #GtkWidget
 * @dir:    the new direction
 * 
 * Set the reading direction on a particular widget. This direction
 * controls the primary direction for widgets containing text,
 * and also the direction in which the children of a container are
 * packed. The ability to set the direction is present in order
 * so that correct localization into languages with right-to-left
 * reading directions can be done. Generally, applications will
 * let the default reading direction present, except for containers
 * where the containers are arranged in an order that is explicitely
 * visual rather than logical (such as buttons for text justificiation).
 *
 * If the direction is set to %GTK_TEXT_DIR_NONE, then the value
 * set by gtk_widget_set_default_direction() will be used.
 **/
void
gtk_widget_set_direction (GtkWidget        *widget,
			  GtkTextDirection  dir)
{
  GtkTextDirection old_dir;
  
  g_return_if_fail (widget != NULL);
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
 * Get the reading direction for a particular widget. See
 * gtk_widget_set_direction().
 * 
 * Return value: the reading direction for the widget.
 **/
GtkTextDirection
gtk_widget_get_direction (GtkWidget *widget)
{
  g_return_val_if_fail (widget != NULL, GTK_TEXT_DIR_LTR);
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

  g_object_ref (G_OBJECT (widget));
  
  if (!GTK_WIDGET_DIRECTION_SET (widget))
    gtk_widget_emit_direction_changed (widget, old_dir);
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  gtk_widget_set_default_direction_recurse,
			  data);

  g_object_unref (G_OBJECT (widget));
}

/**
 * gtk_widget_set_default_direction:
 * @dir: the new default direction. This cannot be
 *        %GTK_TEXT_DIR_NONE.
 * 
 * Set the default reading direction for widgets where the
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
 * Return value: the current default direction. See
 * gtk_widget_set_direction().
 **/
GtkTextDirection
gtk_widget_get_default_direction (void)
{
  return gtk_default_direction;
}

static void
gtk_widget_shutdown (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  if (widget->parent)
    gtk_container_remove (GTK_CONTAINER (widget->parent), widget);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_VISIBLE);
  if (GTK_WIDGET_REALIZED (widget))
    gtk_widget_unrealize (widget);
  
  G_OBJECT_CLASS (parent_class)->shutdown (object);
}

static void
gtk_widget_real_destroy (GtkObject *object)
{
  GtkWidget *widget;
  GtkStyle *saved_style;

  /* gtk_object_destroy() will already hold a refcount on object
   */
  widget = GTK_WIDGET (object);

  gtk_grab_remove (widget);
  gtk_selection_remove_all (widget);
  
  saved_style = gtk_object_get_data_by_id (object, saved_default_style_key_id);
  if (saved_style)
    {
      gtk_style_unref (saved_style);
      gtk_object_remove_data_by_id (object, saved_default_style_key_id);
    }

  gtk_style_unref (widget->style);
  widget->style = gtk_widget_peek_style ();
  gtk_style_ref (widget->style);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_widget_finalize (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetAuxInfo *aux_info;
  gint *events;
  GdkExtensionMode *mode;
  GtkStyle *saved_style;
  
  gtk_grab_remove (widget);
  gtk_selection_remove_all (widget);

  saved_style = gtk_object_get_data_by_id (GTK_OBJECT (widget), saved_default_style_key_id);
  if (saved_style)
    {
      gtk_style_unref (saved_style);
      gtk_object_remove_data_by_id (GTK_OBJECT (widget), saved_default_style_key_id);
    }

  gtk_style_unref (widget->style);
  widget->style = NULL;

  if (widget->name)
    g_free (widget->name);
  
  aux_info = gtk_object_get_data_by_id (GTK_OBJECT (widget), aux_info_key_id);
  if (aux_info)
    gtk_widget_aux_info_destroy (aux_info);
  
  events = gtk_object_get_data_by_id (GTK_OBJECT (widget), event_key_id);
  if (events)
    g_free (events);
  
  mode = gtk_object_get_data_by_id (GTK_OBJECT (widget), extension_event_key_id);
  if (mode)
    g_free (mode);

  G_OBJECT_CLASS (parent_class)->finalize (object);
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
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_REALIZED (widget) == TRUE);
  
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
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
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
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_NO_WINDOW (widget));
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  if (widget->parent)
    {
      widget->window = gtk_widget_get_parent_window (widget);
      gdk_window_ref (widget->window);
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
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (GTK_WIDGET_MAPPED (widget))
    gtk_widget_real_unmap (widget);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

  /* printf ("unrealizing %s\n", gtk_type_name (GTK_OBJECT (widget)->klass->type));
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
      gdk_window_unref (widget->window);
      widget->window = NULL;
    }

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED);
}

static void
gtk_widget_real_size_request (GtkWidget         *widget,
			      GtkRequisition    *requisition)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  requisition->width = widget->requisition.width;
  requisition->height = widget->requisition.height;
}

/*****************************************
 * gtk_widget_peek_colormap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static GdkColormap*
gtk_widget_peek_colormap (void)
{
  if (colormap_stack)
    return (GdkColormap*) colormap_stack->data;
  return gtk_widget_get_default_colormap ();
}

static void
gtk_widget_propagate_state (GtkWidget           *widget,
			    GtkStateData        *data)
{
  guint8 old_state;

  /* don't call this function with state==GTK_STATE_INSENSITIVE,
   * parent_sensitive==TRUE on a sensitive widget
   */

  old_state = GTK_WIDGET_STATE (widget);

  if (data->parent_sensitive)
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_PARENT_SENSITIVE);

      if (GTK_WIDGET_IS_SENSITIVE (widget))
	{
	  if (data->state_restoration)
	    GTK_WIDGET_STATE (widget) = GTK_WIDGET_SAVED_STATE (widget);
	  else
	    GTK_WIDGET_STATE (widget) = data->state;
	}
      else
	{
	  GTK_WIDGET_STATE (widget) = GTK_STATE_INSENSITIVE;
	  if (!data->state_restoration &&
	      data->state != GTK_STATE_INSENSITIVE)
	    GTK_WIDGET_SAVED_STATE (widget) = data->state;
	}
    }
  else
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_PARENT_SENSITIVE);
      if (!data->state_restoration)
	{
	  if (data->state != GTK_STATE_INSENSITIVE)
	    GTK_WIDGET_SAVED_STATE (widget) = data->state;
	}
      else if (GTK_WIDGET_STATE (widget) != GTK_STATE_INSENSITIVE)
	GTK_WIDGET_SAVED_STATE (widget) = GTK_WIDGET_STATE (widget);
      GTK_WIDGET_STATE (widget) = GTK_STATE_INSENSITIVE;
    }

  if (GTK_WIDGET_HAS_FOCUS (widget) && !GTK_WIDGET_IS_SENSITIVE (widget))
    {
      GtkWidget *window;

      window = gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW);
      if (window)
	gtk_window_set_focus (GTK_WINDOW (window), NULL);
    }

  if (old_state != GTK_WIDGET_STATE (widget))
    {
      gtk_widget_ref (widget);
      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[STATE_CHANGED], old_state);
      
      if (GTK_IS_CONTAINER (widget))
	{
	  data->parent_sensitive = (GTK_WIDGET_IS_SENSITIVE (widget) != FALSE);
	  data->state = GTK_WIDGET_STATE (widget);
	  if (data->use_forall)
	    gtk_container_forall (GTK_CONTAINER (widget),
				  (GtkCallback) gtk_widget_propagate_state,
				  data);
	  else
	    gtk_container_foreach (GTK_CONTAINER (widget),
				   (GtkCallback) gtk_widget_propagate_state,
				   data);
	}
      gtk_widget_unref (widget);
    }
}

/*****************************************
 * gtk_widget_aux_info_new:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static GtkWidgetAuxInfo*
gtk_widget_aux_info_new (void)
{
  GtkWidgetAuxInfo *aux_info;
  
  if (!aux_info_mem_chunk)
    aux_info_mem_chunk = g_mem_chunk_new ("widget aux info mem chunk",
					  sizeof (GtkWidgetAuxInfo),
					  1024, G_ALLOC_AND_FREE);
  
  aux_info = g_chunk_new (GtkWidgetAuxInfo, aux_info_mem_chunk);
  
  aux_info->x = -1;
  aux_info->y = -1;
  aux_info->width = 0;
  aux_info->height = 0;
  
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
  g_return_if_fail (aux_info != NULL);
  
  g_mem_chunk_free (aux_info_mem_chunk, aux_info);
}

static void
gtk_widget_shape_info_destroy (GtkWidgetShapeInfo *info)
{
  gdk_drawable_unref (info->shape_mask);
  g_free (info);
}

/*****************************************
 * gtk_widget_shape_combine_mask: 
 *   set a shape for this widgets' gdk window, this allows for
 *   transparent windows etc., see gdk_window_shape_combine_mask
 *   for more information
 *
 *   arguments:
 *
 *   results:
 *****************************************/
void
gtk_widget_shape_combine_mask (GtkWidget *widget,
			       GdkBitmap *shape_mask,
			       gint	  offset_x,
			       gint	  offset_y)
{
  GtkWidgetShapeInfo* shape_info;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  /*  set_shape doesn't work on widgets without gdk window */
  g_return_if_fail (!GTK_WIDGET_NO_WINDOW (widget));

  if (!shape_mask)
    {
      GTK_PRIVATE_UNSET_FLAG (widget, GTK_HAS_SHAPE_MASK);
      
      if (widget->window)
	gdk_window_shape_combine_mask (widget->window, NULL, 0, 0);
      
      gtk_object_remove_data (GTK_OBJECT (widget), shape_info_key);
    }
  else
    {
      GTK_PRIVATE_SET_FLAG (widget, GTK_HAS_SHAPE_MASK);
      
      shape_info = g_new (GtkWidgetShapeInfo, 1);
      gtk_object_set_data_full (GTK_OBJECT (widget), shape_info_key, shape_info,
				(GDestroyNotify)gtk_widget_shape_info_destroy);
      
      shape_info->shape_mask = gdk_drawable_ref (shape_mask);
      shape_info->offset_x = offset_x;
      shape_info->offset_y = offset_y;
      
      /* set shape if widget has a gdk window allready.
       * otherwise the shape is scheduled to be set by gtk_widget_realize.
       */
      if (widget->window)
	gdk_window_shape_combine_mask (widget->window, shape_mask,
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

void
gtk_widget_reset_shapes (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_REALIZED (widget));

  if (!GTK_WIDGET_HAS_SHAPE_MASK (widget))
    gtk_reset_shapes_recurse (widget, widget->window);
}

GtkWidget*
gtk_widget_ref (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return (GtkWidget*) g_object_ref ((GObject*) widget);
}

void
gtk_widget_unref (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_unref ((GObject*) widget);
}

void
gtk_widget_path (GtkWidget *widget,
		 guint     *path_length_p,
		 gchar    **path_p,
		 gchar    **path_reversed_p)
{
  static gchar *rev_path = NULL;
  static guint  path_len = 0;
  guint len;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  len = 0;
  do
    {
      gchar *string;
      gchar *d, *s;
      guint l;
      
      string = gtk_widget_get_name (widget);
      l = strlen (string);
      while (path_len <= len + l + 1)
	{
	  path_len += INIT_PATH_SIZE;
	  rev_path = g_realloc (rev_path, path_len);
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
  
  if (path_length_p)
    *path_length_p = len - 1;
  if (path_reversed_p)
    *path_reversed_p = g_strdup (rev_path);
  if (path_p)
    {
      *path_p = g_strdup (rev_path);
      g_strreverse (*path_p);
    }
}

void
gtk_widget_class_path (GtkWidget *widget,
		       guint     *path_length_p,
		       gchar    **path_p,
		       gchar    **path_reversed_p)
{
  static gchar *rev_path = NULL;
  static guint  path_len = 0;
  guint len;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  len = 0;
  do
    {
      gchar *string;
      gchar *d, *s;
      guint l;
      
      string = gtk_type_name (GTK_WIDGET_TYPE (widget));
      l = strlen (string);
      while (path_len <= len + l + 1)
	{
	  path_len += INIT_PATH_SIZE;
	  rev_path = g_realloc (rev_path, path_len);
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
  
  if (path_length_p)
    *path_length_p = len - 1;
  if (path_reversed_p)
    *path_reversed_p = g_strdup (rev_path);
  if (path_p)
    {
      *path_p = g_strdup (rev_path);
      g_strreverse (*path_p);
    }
}
