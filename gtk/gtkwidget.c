/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdarg.h>
#include <string.h>
#include "gtkcontainer.h"
#include "gtkmain.h"
#include "gtkrc.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtkbindings.h"
#include "gtkprivate.h"
#include "gdk/gdk.h"
#include "gdk/gdkx.h"


#define WIDGET_CLASS(w)	 GTK_WIDGET_CLASS (GTK_OBJECT (w)->klass)
#define	INIT_PATH_SIZE	(512)


enum {
  SHOW,
  HIDE,
  MAP,
  UNMAP,
  REALIZE,
  UNREALIZE,
  DRAW,
  DRAW_FOCUS,
  DRAW_DEFAULT,
  SIZE_REQUEST,
  SIZE_ALLOCATE,
  STATE_CHANGED,
  PARENT_SET,
  STYLE_SET,
  ADD_ACCELERATOR,
  REMOVE_ACCELERATOR,
  GRAB_FOCUS,
  EVENT,
  BUTTON_PRESS_EVENT,
  BUTTON_RELEASE_EVENT,
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
static void gtk_widget_shutdown			 (GtkObject	    *object);
static void gtk_widget_real_destroy		 (GtkObject	    *object);
static void gtk_widget_finalize			 (GtkObject	    *object);
static void gtk_widget_real_show		 (GtkWidget	    *widget);
static void gtk_widget_real_hide		 (GtkWidget	    *widget);
static void gtk_widget_real_map			 (GtkWidget	    *widget);
static void gtk_widget_real_unmap		 (GtkWidget	    *widget);
static void gtk_widget_real_realize		 (GtkWidget	    *widget);
static void gtk_widget_real_unrealize		 (GtkWidget	    *widget);
static void gtk_widget_real_draw		 (GtkWidget	    *widget,
						  GdkRectangle	    *area);
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
static void gtk_widget_real_grab_focus           (GtkWidget         *focus_widget);

static void  gtk_widget_redraw_queue_remove       (GtkWidget         *widget);

     
static GdkColormap* gtk_widget_peek_colormap (void);
static GdkVisual*   gtk_widget_peek_visual   (void);
static GtkStyle*    gtk_widget_peek_style    (void);

static void gtk_widget_reparent_container_child  (GtkWidget     *widget,
						  gpointer       client_data);
static void gtk_widget_propagate_state		 (GtkWidget	*widget,
						  GtkStateData 	*data);
static void gtk_widget_set_style_internal	 (GtkWidget	*widget,
						  GtkStyle	*style,
						  gboolean	 initial_emission);
static void gtk_widget_set_style_recurse	 (GtkWidget	*widget,
						  gpointer	 client_data);

static gboolean gtk_widget_is_offscreen           (GtkWidget     *widget);

static GtkWidgetAuxInfo* gtk_widget_aux_info_new     (void);
static void		 gtk_widget_aux_info_destroy (GtkWidgetAuxInfo *aux_info);

static GtkObjectClass *parent_class = NULL;
static guint widget_signals[LAST_SIGNAL] = { 0 };

static GMemChunk *aux_info_mem_chunk = NULL;

static GdkColormap *default_colormap = NULL;
static GdkVisual *default_visual = NULL;
static GtkStyle *gtk_default_style = NULL;

static GSList *colormap_stack = NULL;
static GSList *visual_stack = NULL;
static GSList *style_stack = NULL;
static guint   composite_child_stack = 0;
static GSList *gtk_widget_redraw_queue = NULL;

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
static const gchar *visual_key = "gtk-visual";

static const gchar *rc_style_key = "gtk-rc-style";
static guint        rc_style_key_id = 0;

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
      
      widget_type = gtk_type_unique (gtk_object_get_type (), &widget_info);
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
  GtkObjectClass *object_class;
  
  object_class = (GtkObjectClass*) klass;
  
  parent_class = gtk_type_class (gtk_object_get_type ());
  
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
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, show),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  widget_signals[HIDE] =
    gtk_signal_new ("hide",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, hide),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  widget_signals[MAP] =
    gtk_signal_new ("map",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, map),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  widget_signals[UNMAP] =
    gtk_signal_new ("unmap",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, unmap),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  widget_signals[REALIZE] =
    gtk_signal_new ("realize",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, realize),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  widget_signals[UNREALIZE] =
    gtk_signal_new ("unrealize",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, unrealize),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  widget_signals[DRAW] =
    gtk_signal_new ("draw",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, draw),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);
  widget_signals[DRAW_FOCUS] =
    gtk_signal_new ("draw_focus",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, draw_focus),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  widget_signals[DRAW_DEFAULT] =
    gtk_signal_new ("draw_default",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, draw_default),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  widget_signals[SIZE_REQUEST] =
    gtk_signal_new ("size_request",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, size_request),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);
  widget_signals[SIZE_ALLOCATE] =
    gtk_signal_new ("size_allocate",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, size_allocate),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);
  widget_signals[STATE_CHANGED] =
    gtk_signal_new ("state_changed",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, state_changed),
		    gtk_marshal_NONE__UINT,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_STATE_TYPE);
  widget_signals[PARENT_SET] =
    gtk_signal_new ("parent_set",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, parent_set),
		    gtk_marshal_NONE__OBJECT,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_OBJECT);
  widget_signals[STYLE_SET] =
    gtk_signal_new ("style_set",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, style_set),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_STYLE);
  widget_signals[ADD_ACCELERATOR] =
    gtk_accel_group_create_add (object_class->type, GTK_RUN_LAST,
				GTK_SIGNAL_OFFSET (GtkWidgetClass, add_accelerator));
  widget_signals[REMOVE_ACCELERATOR] =
    gtk_accel_group_create_remove (object_class->type, GTK_RUN_LAST,
				   GTK_SIGNAL_OFFSET (GtkWidgetClass, remove_accelerator));
  widget_signals[GRAB_FOCUS] =
    gtk_signal_new ("grab_focus",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, grab_focus),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  widget_signals[EVENT] =
    gtk_signal_new ("event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[BUTTON_PRESS_EVENT] =
    gtk_signal_new ("button_press_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, button_press_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[BUTTON_RELEASE_EVENT] =
    gtk_signal_new ("button_release_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, button_release_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[MOTION_NOTIFY_EVENT] =
    gtk_signal_new ("motion_notify_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, motion_notify_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DELETE_EVENT] =
    gtk_signal_new ("delete_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, delete_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DESTROY_EVENT] =
    gtk_signal_new ("destroy_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, destroy_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[EXPOSE_EVENT] =
    gtk_signal_new ("expose_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, expose_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[KEY_PRESS_EVENT] =
    gtk_signal_new ("key_press_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, key_press_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[KEY_RELEASE_EVENT] =
    gtk_signal_new ("key_release_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, key_release_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[ENTER_NOTIFY_EVENT] =
    gtk_signal_new ("enter_notify_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, enter_notify_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[LEAVE_NOTIFY_EVENT] =
    gtk_signal_new ("leave_notify_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, leave_notify_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[CONFIGURE_EVENT] =
    gtk_signal_new ("configure_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, configure_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[FOCUS_IN_EVENT] =
    gtk_signal_new ("focus_in_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, focus_in_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[FOCUS_OUT_EVENT] =
    gtk_signal_new ("focus_out_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, focus_out_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[MAP_EVENT] =
    gtk_signal_new ("map_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, map_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[UNMAP_EVENT] =
    gtk_signal_new ("unmap_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, unmap_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[PROPERTY_NOTIFY_EVENT] =
    gtk_signal_new ("property_notify_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, property_notify_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[SELECTION_CLEAR_EVENT] =
    gtk_signal_new ("selection_clear_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_clear_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[SELECTION_REQUEST_EVENT] =
    gtk_signal_new ("selection_request_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_request_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[SELECTION_NOTIFY_EVENT] =
    gtk_signal_new ("selection_notify_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_notify_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[SELECTION_RECEIVED] =
    gtk_signal_new ("selection_received",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_received),
		    gtk_marshal_NONE__POINTER_UINT,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_SELECTION_DATA,
		    GTK_TYPE_UINT);
  widget_signals[SELECTION_GET] =
    gtk_signal_new ("selection_get",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_get),
		    gtk_marshal_NONE__POINTER_UINT_UINT,
		    GTK_TYPE_NONE, 3,
		    GTK_TYPE_SELECTION_DATA,
		    GTK_TYPE_UINT,
		    GTK_TYPE_UINT);
  widget_signals[PROXIMITY_IN_EVENT] =
    gtk_signal_new ("proximity_in_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, proximity_in_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[PROXIMITY_OUT_EVENT] =
    gtk_signal_new ("proximity_out_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, proximity_out_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DRAG_LEAVE] =
    gtk_signal_new ("drag_leave",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_leave),
		    gtk_marshal_NONE__POINTER_UINT,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_GDK_DRAG_CONTEXT,
		    GTK_TYPE_UINT);
  widget_signals[DRAG_BEGIN] =
    gtk_signal_new ("drag_begin",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_begin),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_GDK_DRAG_CONTEXT);
  widget_signals[DRAG_END] =
    gtk_signal_new ("drag_end",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_end),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_GDK_DRAG_CONTEXT);
  widget_signals[DRAG_DATA_DELETE] =
    gtk_signal_new ("drag_data_delete",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_data_delete),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_GDK_DRAG_CONTEXT);
  widget_signals[DRAG_MOTION] =
    gtk_signal_new ("drag_motion",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_motion),
		    gtk_marshal_BOOL__POINTER_INT_INT_UINT,
		    GTK_TYPE_BOOL, 4,
		    GTK_TYPE_GDK_DRAG_CONTEXT,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT,
		    GTK_TYPE_UINT);
  widget_signals[DRAG_DROP] =
    gtk_signal_new ("drag_drop",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_drop),
		    gtk_marshal_BOOL__POINTER_INT_INT_UINT,
		    GTK_TYPE_BOOL, 4,
		    GTK_TYPE_GDK_DRAG_CONTEXT,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT,
		    GTK_TYPE_UINT);
  widget_signals[DRAG_DATA_GET] =
    gtk_signal_new ("drag_data_get",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_data_get),
		    gtk_marshal_NONE__POINTER_POINTER_UINT_UINT,
		    GTK_TYPE_NONE, 4,
		    GTK_TYPE_GDK_DRAG_CONTEXT,
		    GTK_TYPE_SELECTION_DATA,
		    GTK_TYPE_UINT,
		    GTK_TYPE_UINT);
  widget_signals[DRAG_DATA_RECEIVED] =
    gtk_signal_new ("drag_data_received",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_data_received),
		    gtk_marshal_NONE__POINTER_INT_INT_POINTER_UINT_UINT,
		    GTK_TYPE_NONE, 6,
		    GTK_TYPE_GDK_DRAG_CONTEXT,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT,
		    GTK_TYPE_SELECTION_DATA,
		    GTK_TYPE_UINT,
		    GTK_TYPE_UINT);
  widget_signals[VISIBILITY_NOTIFY_EVENT] =
    gtk_signal_new ("visibility_notify_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, visibility_notify_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[CLIENT_EVENT] =
    gtk_signal_new ("client_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, client_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[NO_EXPOSE_EVENT] =
    gtk_signal_new ("no_expose_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, no_expose_event),
		    gtk_marshal_BOOL__POINTER,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DEBUG_MSG] =
    gtk_signal_new ("debug_msg",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, debug_msg),
		    gtk_marshal_NONE__STRING,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_STRING);

  gtk_object_class_add_signals (object_class, widget_signals, LAST_SIGNAL);

  object_class->set_arg = gtk_widget_set_arg;
  object_class->get_arg = gtk_widget_get_arg;
  object_class->shutdown = gtk_widget_shutdown;
  object_class->destroy = gtk_widget_real_destroy;
  object_class->finalize = gtk_widget_finalize;
  
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
  klass->draw = gtk_widget_real_draw;
  klass->draw_focus = NULL;
  klass->size_request = gtk_widget_real_size_request;
  klass->size_allocate = gtk_widget_real_size_allocate;
  klass->state_changed = NULL;
  klass->parent_set = NULL;
  klass->style_set = gtk_widget_style_set;
  klass->add_accelerator = (gint (*) (GtkWidget *, guint, GtkAccelGroup *,
				      guint, GdkModifierType,
				      GtkAccelFlags)) gtk_accel_group_handle_add;
  klass->remove_accelerator = (void (*) (GtkWidget *, GtkAccelGroup *,
					 guint, GdkModifierType)) gtk_accel_group_handle_remove;
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
  GdkVisual *visual;
  
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
			(composite_child_stack ? GTK_COMPOSITE_CHILD : 0));

  widget->style = gtk_widget_peek_style ();
  gtk_style_ref (widget->style);
  
  colormap = gtk_widget_peek_colormap ();
  visual = gtk_widget_peek_visual ();
  
  if (colormap != gtk_widget_get_default_colormap ())
    gtk_widget_set_colormap (widget, colormap);

  if (visual != gtk_widget_get_default_visual ())
    gtk_widget_set_visual (widget, visual);
}

/*****************************************
 * gtk_widget_new:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkWidget*
gtk_widget_new (GtkType      widget_type,
		const gchar *first_arg_name,
		...)
{
  GtkObject *object;
  va_list var_args;
  GSList *arg_list = NULL;
  GSList *info_list = NULL;
  gchar *error;
  
  g_return_val_if_fail (gtk_type_is_a (widget_type, GTK_TYPE_WIDGET), NULL);
  
  object = gtk_type_new (widget_type);
  
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

/*****************************************
 * gtk_widget_newv:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkWidget*
gtk_widget_newv (GtkType type,
		 guint	 nargs,
		 GtkArg *args)
{
  g_return_val_if_fail (gtk_type_is_a (type, GTK_TYPE_WIDGET), NULL);
  
  return GTK_WIDGET (gtk_object_newv (type, nargs, args));
}

/*****************************************
 * gtk_widget_get:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_get (GtkWidget	*widget,
		GtkArg		*arg)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (arg != NULL);
  
  gtk_object_getv (GTK_OBJECT (widget), 1, arg);
}

/*****************************************
 * gtk_widget_getv:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_getv (GtkWidget	*widget,
		 guint           nargs,
		 GtkArg         *args)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_object_getv (GTK_OBJECT (widget), nargs, args);
}

/*****************************************
 * gtk_widget_set:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_setv:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

  /* We check for GTK_WIDGET_IS_OFFSCREEN (widget), 
   * and queue_clear_area(parent...) will check the rest of
   * way up the tree with gtk_widget_is_offscreen (parent)
   */
  parent = widget->parent;
  if (parent && GTK_WIDGET_DRAWABLE (parent) &&
      !GTK_WIDGET_IS_OFFSCREEN (widget))
    gtk_widget_queue_clear_area (parent,
				 widget->allocation.x,
				 widget->allocation.y,
				 widget->allocation.width,
				 widget->allocation.height);
}

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

  if (GTK_WIDGET_REDRAW_PENDING (widget))
    gtk_widget_redraw_queue_remove (widget);

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

/*****************************************
 * gtk_widget_destroy:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_destroy (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_OBJECT_CONSTRUCTED (widget));

  gtk_object_destroy ((GtkObject*) widget);
}

/*****************************************
 * gtk_widget_destroyed:
 *   Utility function: sets widget_pointer 
 *   to NULL when widget is destroyed.
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_show:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

/*************************************************************
 * gtk_widget_show_now:
 *   Show a widget, and if it is an unmapped toplevel widget
 *   wait for the map_event before returning
 *
 *   Warning: This routine will call the main loop recursively.
 *       
 *   arguments:
 *     
 *   results:
 *************************************************************/

static void
gtk_widget_show_map_callback (GtkWidget *widget, GdkEvent *event, gint *flag)
{
  *flag = TRUE;
  gtk_signal_disconnect_by_data (GTK_OBJECT (widget), flag);
}

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

/*****************************************
 * gtk_widget_hide:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

gint
gtk_widget_hide_on_delete (GtkWidget      *widget)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  
  gtk_widget_hide (widget);
  
  return TRUE;
}

void
gtk_widget_show_all (GtkWidget *widget)
{
  GtkWidgetClass *class;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  class = GTK_WIDGET_CLASS (GTK_OBJECT (widget)->klass);

  if (class->show_all)
    class->show_all (widget);
}

void
gtk_widget_hide_all (GtkWidget *widget)
{
  GtkWidgetClass *class;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  class = GTK_WIDGET_CLASS (GTK_OBJECT (widget)->klass);

  if (class->hide_all)
    class->hide_all (widget);
}

/*****************************************
 * gtk_widget_map:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_unmap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_realize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_unrealize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_unrealize (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (GTK_WIDGET_REDRAW_PENDING (widget))
    gtk_widget_redraw_queue_remove (widget);
  
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
 * gtk_widget_queue_draw:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

typedef struct _GtkDrawData GtkDrawData;
struct _GtkDrawData {
  GdkRectangle rect;
  GdkWindow *window;
};

static GMemChunk   *draw_data_mem_chunk = NULL;
static GSList      *draw_data_free_list = NULL;
static const gchar *draw_data_key  = "gtk-draw-data";
static GQuark       draw_data_key_id = 0;
static const gchar *draw_data_tmp_key  = "gtk-draw-data-tmp";
static GQuark       draw_data_tmp_key_id = 0;

static gint gtk_widget_idle_draw (gpointer data);

static void
gtk_widget_queue_draw_data (GtkWidget *widget,
			    gint       x,
			    gint       y,
			    gint       width,
			    gint       height,
			    GdkWindow *window)
{
  GSList      *node;
  GtkDrawData *data;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (!(width < 0 || height < 0) || window == NULL);

  if ((width != 0) && (height != 0) && GTK_WIDGET_DRAWABLE (widget))
    {
      if (!draw_data_key_id)
	draw_data_key_id = g_quark_from_static_string (draw_data_key);
      
      if (draw_data_free_list)
	{
	  node = draw_data_free_list;
	  data = node->data;
	  draw_data_free_list = draw_data_free_list->next;
	}
      else
	{
	  if (!draw_data_mem_chunk)
	    draw_data_mem_chunk = g_mem_chunk_create (GtkDrawData, 64,
						      G_ALLOC_ONLY);
	  data = g_chunk_new (GtkDrawData, draw_data_mem_chunk);
	  node = g_slist_alloc();
	  node->data = data;
	}

      data->rect.x = x;
      data->rect.y = y;

      if ((width < 1 && height < 1) ||
	  (width >= widget->allocation.width &&
	   height >= widget->allocation.height))
	GTK_PRIVATE_SET_FLAG (widget, GTK_FULLDRAW_PENDING);

      if ((width < 0) || (height < 0))
	{
	  data->rect.width = 0;
	  data->rect.height = 0;
	}
      else
	{
	  data->rect.width = width;
	  data->rect.height = height;
	}
      data->window = window;
      
      if ((width < 0) || (height < 0))
	{
	  GSList *draw_data_list = 
	    gtk_object_get_data_by_id (GTK_OBJECT (widget),
				       draw_data_key_id);
	  if (draw_data_list)
	    draw_data_free_list = g_slist_concat (draw_data_list,
						  draw_data_free_list);
	  node->next = NULL;
	}
      else
	node->next = gtk_object_get_data_by_id (GTK_OBJECT (widget),
						draw_data_key_id);

      if (!GTK_WIDGET_REDRAW_PENDING (widget))
	{
	  GTK_PRIVATE_SET_FLAG (widget, GTK_REDRAW_PENDING);
	  if (gtk_widget_redraw_queue == NULL)
	    gtk_idle_add_priority (GTK_PRIORITY_REDRAW,
				   gtk_widget_idle_draw,
				   NULL);
	  gtk_widget_redraw_queue = g_slist_prepend (gtk_widget_redraw_queue, widget);
	}

      gtk_object_set_data_by_id (GTK_OBJECT (widget), draw_data_key_id, node);
    }
}

void	   
gtk_widget_queue_draw_area (GtkWidget *widget,
			    gint       x,
			    gint       y,
			    gint       width,
			    gint       height)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (widget->window && gdk_window_is_viewable (widget->window) &&
      !gtk_widget_is_offscreen (widget))
    gtk_widget_queue_draw_data (widget, x, y, width, height, NULL);
}

void	   
gtk_widget_queue_draw (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (widget->window && gdk_window_is_viewable (widget->window) &&
      !gtk_widget_is_offscreen (widget))
    gtk_widget_queue_draw_data (widget, 0, 0, -1, -1, NULL);
}

void	   
gtk_widget_queue_clear_area (GtkWidget *widget,
			     gint       x,
			     gint       y,
			     gint       width,
			     gint       height)
{
  GtkWidget *parent;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!(widget->window && gdk_window_is_viewable (widget->window)) ||
      gtk_widget_is_offscreen (widget))
    return;

  /* Find the correct widget */

  if (GTK_WIDGET_NO_WINDOW (widget))
    {
      /* The following deals with the fact that while we are in
       * a reparent, the widget and window heirarchies
       * may be different, and the redraw queing code will be utterly
       * screwed by that. 
       *
       * So, continuing at this point is a bad idea, and returning is
       * generally harmless. (More redraws will be queued then necessary
       * for a reparent in any case.) This can go away, when we
       * make reparent simply ref/remove/add/unref.
       */
      if (GTK_WIDGET_IN_REPARENT (widget))
	return;

      parent = widget;
      while (parent && GTK_WIDGET_NO_WINDOW (parent))
	parent = parent->parent;
      
      if (parent)
	gtk_widget_queue_draw_data (parent, x, y, width, height, widget->window);
    }
  else
    {
      if (widget->parent)
	{
	  gint wx, wy, wwidth, wheight;
	  /* Translate widget relative to window-relative */
	  
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

      gtk_widget_queue_draw_data (widget, x, y, width, height, widget->window);
    }
}

static void
gtk_widget_redraw_queue_remove (GtkWidget *widget)
{
  GSList *draw_data_list;
  GSList *tmp_list;

  g_return_if_fail (GTK_WIDGET_REDRAW_PENDING (widget));

  gtk_widget_redraw_queue = g_slist_remove (gtk_widget_redraw_queue, widget);

  draw_data_list = gtk_object_get_data_by_id (GTK_OBJECT (widget),
					      draw_data_key_id);
  tmp_list = g_slist_last (draw_data_list);
  if (tmp_list)
    {
      tmp_list->next = draw_data_free_list;
      draw_data_free_list = draw_data_list;
    }

  gtk_object_set_data_by_id (GTK_OBJECT (widget),
			     draw_data_key_id,
			     NULL);
  
  GTK_PRIVATE_UNSET_FLAG (widget, GTK_REDRAW_PENDING);
  GTK_PRIVATE_UNSET_FLAG (widget, GTK_FULLDRAW_PENDING);
}

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

static gint
gtk_widget_draw_data_combine (GtkDrawData *parent, GtkDrawData *child)
{
  gint parent_x2, parent_y2;
  gint child_x2, child_y2;

  /* Check for intersection */

  parent_x2 = parent->rect.x + parent->rect.width;
  child_x2 = child->rect.x + child->rect.width;
  parent_y2 = parent->rect.y + parent->rect.height;
  child_y2 = child->rect.y + child->rect.height;

  if ((child->rect.x > parent_x2) || (parent->rect.x > child_x2) ||
      (child->rect.y > parent_y2) || (parent->rect.y > child_y2))
    return FALSE;
  else
    {
      parent->rect.x = MIN (parent->rect.x, child->rect.x);
      parent->rect.y = MIN (parent->rect.y, child->rect.y);
      parent->rect.width = MAX (parent_x2, child_x2) - parent->rect.x;
      parent->rect.height = MAX (parent_y2, child_y2) - parent->rect.y;
    }

  return TRUE;
}

/* Take a rectangle with respect to window, and translate it
 * to coordinates relative to widget's allocation, clipping through
 * intermediate windows. Returns whether translation failed. If the
 * translation failed, we have something like a handlebox, where
 * the child widget's GdkWindow is not a child of the parents GdkWindow.
 */
static gboolean
gtk_widget_clip_rect (GtkWidget *widget,
		      GdkWindow *window,
		      GdkRectangle *rect,
		      gint      *x_offset,
		      gint      *y_offset)
{
  gint x,y, width, height;

  while (window && (window != widget->window))
    {
      gdk_window_get_position (window, &x, &y);
      rect->x += x;
      if (x_offset)
	*x_offset += x;
      rect->y += y;
      if (y_offset)
	*y_offset += y;
      
      window = gdk_window_get_parent (window);
      if (!window)
	return FALSE;

      gdk_window_get_size (window, &width, &height);
      
      if (rect->x < 0)
	{
	  rect->width = (rect->width > -rect->x) ? rect->width + rect->x : 0;
	  rect->x = 0;
	}
      if (rect->y < 0)
	{
	  rect->height = (rect->height > -rect->y) ? rect->height + rect->y : 0;
	  rect->y = 0;
	}
      if (rect->x + rect->width > width)
	rect->width = (width > rect->x) ? width - rect->x : 0;
      if (rect->y + rect->height > height)
	rect->height = (height > rect->y) ? height - rect->y : 0;
    }

  if (!window)
    return FALSE;

  if (!GTK_WIDGET_NO_WINDOW (widget))
    {
      if (gdk_window_get_toplevel (window) != window)
	{
	  gdk_window_get_position (window, &x, &y);
	  rect->x += x - widget->allocation.x;
	  if (x_offset)
	    *x_offset += x - widget->allocation.x;
	  rect->y += y - widget->allocation.y;
	  if (y_offset)
	    *y_offset += y - widget->allocation.y;
	}
    }

  return TRUE;
}

static gint
gtk_widget_idle_draw (gpointer cb_data)
{
  GSList *widget_list;
  GSList *old_queue;
  GSList *draw_data_list;
  GtkWidget *widget;
  
  if (!draw_data_tmp_key_id)
    draw_data_tmp_key_id = g_quark_from_static_string (draw_data_tmp_key);
      
  GDK_THREADS_ENTER ();

  old_queue = gtk_widget_redraw_queue;
  gtk_widget_redraw_queue = NULL;
  
  /* Translate all draw requests to be allocation-relative.
   * At the same time, move all the data out of the way,
   * so when we get down to the draw step, we can queue
   * more information for "next time", if the application
   * is that foolhardy.
   */
  widget_list = old_queue;
  
  while (widget_list)
    {
      widget = widget_list->data;
      draw_data_list = gtk_object_get_data_by_id (GTK_OBJECT (widget),
						  draw_data_key_id);
      gtk_object_set_data_by_id (GTK_OBJECT (widget),
				 draw_data_key_id,
				 NULL);
      gtk_object_set_data_by_id (GTK_OBJECT (widget),
				 draw_data_tmp_key_id,
				 draw_data_list);
     
      /* XXX: Since we are unsetting this flag here, further
       * down the only way we can check if a redraw is queued
       * on a given widget is by calling gtk_object_get_data.
       * for speed purposes we might well want a private
       * flag GTK_REDRAW_PROCESSING or something.
       */
      GTK_PRIVATE_UNSET_FLAG (widget, GTK_REDRAW_PENDING);
      GTK_PRIVATE_UNSET_FLAG (widget, GTK_FULLDRAW_PENDING);
      
      while (draw_data_list)
	{
	  gboolean full_allocation = FALSE;
	  GtkDrawData *data = draw_data_list->data;

	  if (data->window)
	    {
	      /* If the translation fails, we have a handlebox,
	       * so redraw the whole widget. Could be done better?
	       */
	      full_allocation = !gtk_widget_clip_rect (widget, 
						       data->window, 
						       &data->rect,
						       NULL, NULL);
	      data->window = NULL;
	    }
	  else if ((data->rect.width == 0) && (data->rect.height == 0))
	    full_allocation = TRUE;

	  if (full_allocation)
	    {
	      if (GTK_WIDGET_NO_WINDOW (widget))
		{
		  data->rect.x = widget->allocation.x;
		  data->rect.y = widget->allocation.y;
		}
	      else
		{
		  data->rect.x = 0;
		  data->rect.y = 0;
		}
	      data->rect.width = widget->allocation.width;
	      data->rect.height = widget->allocation.height;
	    }

	  draw_data_list = draw_data_list->next;
	}

      widget_list = widget_list->next;
    }

  /* Coalesce redraws.
   */
  widget_list = old_queue;
  while (widget_list)
    {
      GSList *prev_node = NULL;
      widget = widget_list->data;
      draw_data_list = gtk_object_get_data_by_id (GTK_OBJECT (widget),
						  draw_data_tmp_key_id);

      while (draw_data_list)
	{
	  gint x_offset, y_offset;
	  GtkDrawData *data = draw_data_list->data;
	  GSList *parent_list = draw_data_list->next;
	  GtkWidget *parent;
	  GdkWindow *window;

	  x_offset = 0;
	  y_offset = 0;
	  
	  parent = widget;
	  while (parent)
	    {
	      while (parent_list)
		{
		  if (gtk_widget_draw_data_combine (parent_list->data, data))
		    {
		      GSList *tmp;
		      if (prev_node)
			prev_node->next = draw_data_list->next;
		      else
			gtk_object_set_data_by_id (GTK_OBJECT (widget),
						   draw_data_tmp_key_id,
						   draw_data_list->next);

		      tmp = draw_data_list->next;
		      draw_data_list->next = draw_data_free_list;
		      draw_data_free_list = draw_data_list;
		      draw_data_list = tmp;

		      goto next_rect;
		    }
		  
		  parent_list = parent_list->next;
		}

	      window = parent->window;

	      if (parent->parent && parent->parent->window != window)
		{
		  if (!GTK_WIDGET_NO_WINDOW (parent))
		    {
		      gint x, y;
		      gdk_window_get_position (window, &x, &y);
		      data->rect.x -= x - parent->allocation.x;
		      x_offset -=  x - parent->allocation.x;
		      data->rect.y -= y - parent->allocation.y;
		      y_offset -=  y - parent->allocation.y;
		    }
		  /* If we can't translate the rectangle, stop trying to
		   * merge. (This occurs for a handlebox)
		   */
		  if (!gtk_widget_clip_rect (parent->parent, window, &data->rect,
					     &x_offset, &y_offset))
		    parent = NULL;
		}

	      if (parent)
		parent = parent->parent;

	      if (parent)
		parent_list = gtk_object_get_data_by_id (GTK_OBJECT (parent),
							 draw_data_tmp_key_id);
	      else
		parent_list = NULL;
	    }

	  /* OK, this rectangle stays around. But take advantage
	   * of the work we've done to clip it to the visible area -
	   * rect.width/height have already been appropriately 
	   * decreased
	   */
	  data->rect.x -= x_offset;
	  data->rect.y -= y_offset;


	  prev_node = draw_data_list;

	  draw_data_list = draw_data_list->next;
	next_rect:
	  continue;
	}
      widget_list = widget_list->next;
    }

  /* Process the draws */
  
  widget_list = old_queue;

  while (widget_list)
    {
      GSList *tmp_list;
      
      widget = widget_list->data;
      draw_data_list = gtk_object_get_data_by_id (GTK_OBJECT (widget),
						  draw_data_tmp_key_id);
      gtk_object_set_data_by_id (GTK_OBJECT (widget),
				 draw_data_tmp_key_id,
				 NULL);

      tmp_list = draw_data_list;
      while (tmp_list)
	{
	  GtkDrawData *data = tmp_list->data;
	  if ((data->rect.width != 0) && (data->rect.height != 0))
	    gtk_widget_draw (widget, &data->rect);
	  
	  if (tmp_list->next)
	    tmp_list = tmp_list->next;
	  else
	    {
	      tmp_list->next = draw_data_free_list;
	      draw_data_free_list = draw_data_list;
	      break;
	    }
	}

      widget_list = widget_list->next;
    }

  g_slist_free (old_queue);

  GDK_THREADS_LEAVE ();
  
  return FALSE;
}

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
  else if (GTK_WIDGET_TOPLEVEL (widget))
    gtk_container_queue_resize (GTK_CONTAINER (widget));
}

/*****************************************
 * gtk_widget_draw:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_draw (GtkWidget    *widget,
		 GdkRectangle *area)
{
  GdkRectangle temp_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      if (!area)
	{
	  if (GTK_WIDGET_NO_WINDOW (widget))
	    {
	      temp_area.x = widget->allocation.x;
	      temp_area.y = widget->allocation.y;
	    }
	  else
	    {
	      temp_area.x = 0;
	      temp_area.y = 0;
	    }

	  temp_area.width = widget->allocation.width;
	  temp_area.height = widget->allocation.height;
	  area = &temp_area;
	}

      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[DRAW], area);
    }
}

/*****************************************
 * gtk_widget_draw_focus:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_draw_focus (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[DRAW_FOCUS]);
}

/*****************************************
 * gtk_widget_draw_default:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_draw_default (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[DRAW_DEFAULT]);
}

/*****************************************
 * gtk_widget_size_request:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_get_requesition:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_size_allocate:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

  if (real_allocation.width > 32767 ||
      real_allocation.height > 32767)
    {
      g_warning ("gtk_widget_size_allocate(): attempt to allocate widget with width %d and height %d",
		 real_allocation.width,
		 real_allocation.height);
      real_allocation.width = MIN (real_allocation.width, 32767);
      real_allocation.height = MIN (real_allocation.height, 32767);
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

/*****************************************
 * gtk_widget_event:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

gint
gtk_widget_event (GtkWidget *widget,
		  GdkEvent  *event)
{
  gint return_val;
  gint signal_num;

  g_return_val_if_fail (widget != NULL, TRUE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);

  gtk_widget_ref (widget);
  return_val = FALSE;
  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[EVENT], event,
		   &return_val);
  if (return_val || GTK_OBJECT_DESTROYED (widget))
    {
      gtk_widget_unref (widget);
      return TRUE;
    }

  switch (event->type)
    {
      GtkWidget *parent;
    case GDK_NOTHING:
      signal_num = -1;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
      signal_num = BUTTON_PRESS_EVENT;
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
      /* there is no sense in providing a widget with bogus expose events.
       * also we make the optimization to discard expose events for widgets
       * that have a full redraw pending (given that the event is !send_event,
       * otherwise we assume we can trust the event).
       */
      if (!event->any.send_event && event->any.window)
	{
	  parent = widget;
	  while (parent)
	    {
	      if (GTK_WIDGET_FULLDRAW_PENDING (parent))
		break;
	      parent = parent->parent;
	    }
	  /* <HACK> gnome-dock didn't propagate draws to torn off
	   *        children. So don't consider those ancestors.
	   */
	  if (parent)
	    {
	      GdkWindow *parent_window = event->any.window;

	      while (parent_window && parent_window != parent->window)
		  parent_window = gdk_window_get_parent (parent_window);

	      if (!parent_window)
		parent = NULL;
	    }
	  /* </HACK> */
	}
      else
	parent = NULL;
      
      if (!event->any.window || parent)
	{
	  gtk_widget_unref (widget);
	  return TRUE;
	}
      signal_num = EXPOSE_EVENT;
      break;
    case GDK_VISIBILITY_NOTIFY:
      signal_num = VISIBILITY_NOTIFY_EVENT;
      break;
    default:
      g_warning ("could not determine signal number for event: %d", event->type);
      gtk_widget_unref (widget);
      return TRUE;
    }
  
  if (signal_num != -1)
    gtk_signal_emit (GTK_OBJECT (widget), widget_signals[signal_num], event, &return_val);

  return_val |= GTK_OBJECT_DESTROYED (widget);

  gtk_widget_unref (widget);

  return return_val;
}

/*****************************************
 * gtk_widget_activate:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_reparent:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_popup:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_intersect:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

gint
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
  
  return_val = gdk_rectangle_intersect ((GdkRectangle*) &widget->allocation, area, dest);
  
  if (return_val && intersection && !GTK_WIDGET_NO_WINDOW (widget))
    {
      intersection->x -= widget->allocation.x;
      intersection->y -= widget->allocation.y;
    }
  
  return return_val;
}

/*****************************************
 * gtk_widget_grab_focus:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_grab_default:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_grab_default (GtkWidget *widget)
{
  GtkWidget *window;
  GtkType window_type;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_CAN_DEFAULT (widget));
  
  window_type = gtk_window_get_type ();
  window = widget->parent;
  
  while (window && !gtk_type_is_a (GTK_WIDGET_TYPE (window), window_type))
    window = window->parent;
  
  if (window && gtk_type_is_a (GTK_WIDGET_TYPE (window), window_type))
    gtk_window_set_default (GTK_WINDOW (window), widget);
}

/*****************************************
 * gtk_widget_set_name:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_get_name:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

gchar*
gtk_widget_get_name (GtkWidget *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  if (widget->name)
    return widget->name;
  return gtk_type_name (GTK_WIDGET_TYPE (widget));
}

/*****************************************
 * gtk_widget_set_state:
 *
 *   arguments:
 *     widget
 *     state
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_set_sensitive:
 *
 *   arguments:
 *     widget
 *     boolean value for sensitivity
 *
 *   results:
 *****************************************/

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

/*****************************************
 * gtk_widget_set_parent:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

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

void
gtk_widget_ensure_style (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!GTK_WIDGET_USER_STYLE (widget) &&
      !GTK_WIDGET_RC_STYLE (widget))
    gtk_widget_set_rc_style (widget);
}

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

GtkStyle*
gtk_widget_get_style (GtkWidget *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  return widget->style;
}

void       
gtk_widget_modify_style (GtkWidget      *widget,
			 GtkRcStyle     *style)
{
  GtkRcStyle *old_style;
  
  if (!rc_style_key_id)
    rc_style_key_id = g_quark_from_static_string (rc_style_key);

  old_style = gtk_object_get_data_by_id (GTK_OBJECT (widget), rc_style_key_id);

  if (style != old_style)
    {
      gtk_rc_style_ref (style);
      
      gtk_object_set_data_by_id_full (GTK_OBJECT (widget),
				      rc_style_key_id,
				      style,
				      (GtkDestroyNotify)gtk_rc_style_unref);
    }

  if (GTK_WIDGET_RC_STYLE (widget))
    gtk_widget_set_rc_style (widget);
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
  GdkVisual *visual;
  
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  if (widget->window)
    {
      visual = gdk_window_get_visual (widget->window);
      /* If window was destroyed previously, we'll get NULL here */
      if (visual)
	return visual;
    }
  
  visual = gtk_object_get_data (GTK_OBJECT (widget), visual_key);
  if (visual)
    return visual;

  return gtk_widget_get_default_visual ();
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
 * gtk_widget_set_visual:
 *    Set the colormap for the widget to the given
 *    value. Widget must not have been previously
 *    realized. This probably should only be used
 *    from an init() function.
 *   arguments:
 *    widget:
 *    visual:
 *   results:
 *****************************************/

void
gtk_widget_set_visual (GtkWidget *widget, GdkVisual *visual)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!GTK_WIDGET_REALIZED (widget));
  g_return_if_fail (visual != NULL);
  
  gtk_object_set_data (GTK_OBJECT (widget), 
		       visual_key,
		       visual);
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
 * gtk_widget_push_visual:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_push_visual (GdkVisual *visual)
{
  g_return_if_fail (visual != NULL);

  visual_stack = g_slist_prepend (visual_stack, visual);
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
 * gtk_widget_pop_visual:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_pop_visual (void)
{
  GSList *tmp;
  
  if (visual_stack)
    {
      tmp = visual_stack;
      visual_stack = visual_stack->next;
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
 * gtk_widget_set_default_visual:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_set_default_visual (GdkVisual *visual)
{
  default_visual = visual;
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
  if (!default_visual)
    default_visual = gdk_visual_get_system ();
  
  return default_visual;
}

static void
gtk_widget_shutdown (GtkObject *object)
{
  GtkWidget *widget;
  
  /* gtk_object_destroy() will already hold a refcount on object
   */
  widget = GTK_WIDGET (object);

  if (widget->parent)
    gtk_container_remove (GTK_CONTAINER (widget->parent), widget);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_VISIBLE);
  if (GTK_WIDGET_REALIZED (widget))
    gtk_widget_unrealize (widget);
  
  parent_class->shutdown (object);
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
  widget->style = NULL;

  parent_class->destroy (object);
}

static void
gtk_widget_finalize (GtkObject *object)
{
  GtkWidget *widget;
  GtkWidgetAuxInfo *aux_info;
  gint *events;
  GdkExtensionMode *mode;
  
  widget = GTK_WIDGET (object);
  
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

  parent_class->finalize (object);
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
gtk_widget_real_draw (GtkWidget	   *widget,
		      GdkRectangle *area)
{
  GdkEventExpose event;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (area != NULL);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      event.type = GDK_EXPOSE;
      event.send_event = TRUE;
      event.window = widget->window;
      event.area = *area;
      event.count = 0;
      
      gdk_window_ref (event.window);
      gtk_widget_event (widget, (GdkEvent*) &event);
      gdk_window_unref (event.window);
    }
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

/*****************************************
 * gtk_widget_peek_visual:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static GdkVisual*
gtk_widget_peek_visual (void)
{
  if (visual_stack)
    return (GdkVisual*) visual_stack->data;
  return gtk_widget_get_default_visual ();
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

      window = gtk_widget_get_ancestor (widget, gtk_window_get_type ());
      if (window)
	gtk_window_set_focus (GTK_WINDOW (window), NULL);
    }

  if (old_state != GTK_WIDGET_STATE (widget))
    {
      if (!GTK_WIDGET_IS_SENSITIVE (widget) && GTK_WIDGET_HAS_GRAB (widget))
	gtk_grab_remove (widget);
      
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

/*************************************************************
 * gtk_widget_is_offscreen:
 *     Check if a widget is "offscreen"
 *   arguments:
 *     widget: a widget
 *   results:
 *     TRUE if the widget or any of ancestors has the
 *     PRIVATE_GTK_WIDGET_IS_OFFSCREEN set.
 *************************************************************/

static gboolean 
gtk_widget_is_offscreen (GtkWidget *widget)
{
  while (widget)
    {
      if (GTK_WIDGET_IS_OFFSCREEN (widget))
	return TRUE;
      widget = widget->parent;
    }

  return FALSE;
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
      
      shape_info = gtk_object_get_data (GTK_OBJECT (widget), shape_info_key);
      gtk_object_remove_data (GTK_OBJECT (widget), shape_info_key);
      g_free (shape_info);
    }
  else
    {
      GTK_PRIVATE_SET_FLAG (widget, GTK_HAS_SHAPE_MASK);
      
      shape_info = gtk_object_get_data (GTK_OBJECT (widget), shape_info_key);
      if (!shape_info)
	{
	  shape_info = g_new (GtkWidgetShapeInfo, 1);
	  gtk_object_set_data (GTK_OBJECT (widget), shape_info_key, shape_info);
	}
      shape_info->shape_mask = shape_mask;
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
  GdkWindowPrivate *private;
  gpointer data;
  GList *list;

  private = (GdkWindowPrivate*) window;

  if (private->destroyed)
    return;
  gdk_window_get_user_data (window, &data);
  if (data != widget)
    return;

  gdk_window_shape_combine_mask (window, NULL, 0, 0);
  for (list = private->children; list; list = list->next)
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

void
gtk_widget_ref (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_object_ref ((GtkObject*) widget);
}

void
gtk_widget_unref (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_object_unref ((GtkObject*) widget);
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
