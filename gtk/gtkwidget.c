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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "gdk/gdk.h"
#include "gdk/gdkx.h"


#define WIDGET_CLASS(w)	 GTK_WIDGET_CLASS (GTK_OBJECT (w)->klass)


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
  INSTALL_ACCELERATOR,
  REMOVE_ACCELERATOR,
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
  SELECTION_RECEIVED,
  PROXIMITY_IN_EVENT,
  PROXIMITY_OUT_EVENT,
  DRAG_BEGIN_EVENT,
  DRAG_REQUEST_EVENT,
  DROP_ENTER_EVENT,
  DROP_LEAVE_EVENT,
  DROP_DATA_AVAILABLE_EVENT,
  OTHER_EVENT,
  CLIENT_EVENT,
  LAST_SIGNAL
};


typedef void (*GtkWidgetSignal1) (GtkObject *object,
				  gpointer   arg1,
				  gpointer   data);
typedef gint (*GtkWidgetSignal2) (GtkObject *object,
				  gpointer   arg1,
				  gchar	     arg2,
				  gchar	     arg3,
				  gpointer   data);
typedef void (*GtkWidgetSignal3) (GtkObject *object,
				  gpointer   arg1,
				  gpointer   data);
typedef gint (*GtkWidgetSignal4) (GtkObject *object,
				  gpointer   arg1,
				  gpointer   data);


static void gtk_widget_marshal_signal_1 (GtkObject	*object,
					 GtkSignalFunc	 func,
					 gpointer	 func_data,
					 GtkArg		*args);
static void gtk_widget_marshal_signal_2 (GtkObject	*object,
					 GtkSignalFunc	 func,
					 gpointer	 func_data,
					 GtkArg		*args);
static void gtk_widget_marshal_signal_3 (GtkObject	*object,
					 GtkSignalFunc	 func,
					 gpointer	 func_data,
					 GtkArg		*args);
static void gtk_widget_marshal_signal_4 (GtkObject	*object,
					 GtkSignalFunc	 func,
					 gpointer	 func_data,
					 GtkArg		*args);

static void gtk_widget_class_init		 (GtkWidgetClass    *klass);
static void gtk_widget_init			 (GtkWidget	    *widget);
static void gtk_widget_arg			 (GtkWidget	    *widget,
						  GtkArg	    *arg);
static void gtk_real_widget_destroy		 (GtkObject	    *object);
static void gtk_real_widget_show		 (GtkWidget	    *widget);
static void gtk_real_widget_hide		 (GtkWidget	    *widget);
static void gtk_real_widget_map			 (GtkWidget	    *widget);
static void gtk_real_widget_unmap		 (GtkWidget	    *widget);
static void gtk_real_widget_realize		 (GtkWidget	    *widget);
static void gtk_real_widget_unrealize		 (GtkWidget	    *widget);
static void gtk_real_widget_draw		 (GtkWidget	    *widget,
						  GdkRectangle	    *area);
static gint gtk_real_widget_queue_draw		 (GtkWidget	    *widget);
static gint gtk_real_widget_queue_resize	 (GtkWidget	    *widget);
static void gtk_real_widget_size_allocate	 (GtkWidget	    *widget,
						  GtkAllocation	    *allocation);

static GdkColormap* gtk_widget_peek_colormap (void);
static GdkVisual*   gtk_widget_peek_visual   (void);
static GtkStyle*    gtk_widget_peek_style    (void);

static void gtk_widget_reparent_container_child  (GtkWidget     *widget,
						  gpointer       client_data);
static void gtk_widget_set_parent_sensitive  (GtkWidget *widget,
					      gpointer	 client_data);
static void gtk_widget_propagate_restore     (GtkWidget *widget,
					      gpointer	 client_data);
static void gtk_widget_propagate_state	     (GtkWidget *widget,
					      gpointer	 client_data);
static void gtk_widget_draw_children_recurse (GtkWidget *widget,
					      gpointer	 client_data);
static void gtk_widget_set_style_internal    (GtkWidget *widget,
					      GtkStyle	*style);
static void gtk_widget_set_style_recurse     (GtkWidget *widget,
					      gpointer	 client_data);

extern GtkArg* gtk_object_collect_args (gint	*nargs,
					va_list	 args1,
					va_list	 args2);

static GtkWidgetAuxInfo* gtk_widget_aux_info_new     (void);
static void		 gtk_widget_aux_info_destroy (GtkWidgetAuxInfo *aux_info);

static GtkObjectClass *parent_class = NULL;
static gint widget_signals[LAST_SIGNAL] = { 0 };

static GMemChunk *aux_info_mem_chunk = NULL;

static GdkColormap *default_colormap = NULL;
static GdkVisual *default_visual = NULL;
static GtkStyle *default_style = NULL;

static GSList *colormap_stack = NULL;
static GSList *visual_stack = NULL;
static GSList *style_stack = NULL;

static const char *aux_info_key = "aux_info";
static const char *colormap_key = "colormap";
static const char *visual_key = "visual";
static const char *event_key = "event_mask";
static const char *resize_widgets_key = "resize_widgets";
static const char *extension_event_key = "extension_event_mode";
static const char *redraw_handler_key = "redraw_handler_tag";
static const char *resize_handler_key = "resize_handler_tag";
static const char *shape_info_key = "shape_info";



/*****************************************
 * gtk_widget_get_type:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

guint
gtk_widget_get_type ()
{
  static guint widget_type = 0;
  
  if (!widget_type)
    {
      GtkTypeInfo widget_info =
      {
	"GtkWidget",
	sizeof (GtkWidget),
	sizeof (GtkWidgetClass),
	(GtkClassInitFunc) gtk_widget_class_init,
	(GtkObjectInitFunc) gtk_widget_init,
	(GtkArgFunc) gtk_widget_arg,
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

static void
gtk_widget_class_init (GtkWidgetClass *klass)
{
  GtkObjectClass *object_class;
  
  object_class = (GtkObjectClass*) klass;
  
  parent_class = gtk_type_class (gtk_object_get_type ());
  
  gtk_object_add_arg_type ("GtkWidget::x", GTK_TYPE_INT);
  gtk_object_add_arg_type ("GtkWidget::y", GTK_TYPE_INT);
  gtk_object_add_arg_type ("GtkWidget::width", GTK_TYPE_INT);
  gtk_object_add_arg_type ("GtkWidget::height", GTK_TYPE_INT);
  gtk_object_add_arg_type ("GtkWidget::visible", GTK_TYPE_BOOL);
  gtk_object_add_arg_type ("GtkWidget::sensitive", GTK_TYPE_BOOL);
  gtk_object_add_arg_type ("GtkWidget::events", GTK_TYPE_GDK_EVENT_MASK);
  gtk_object_add_arg_type ("GtkWidget::extension_events", GTK_TYPE_GDK_EVENT_MASK);
  gtk_object_add_arg_type ("GtkWidget::name", GTK_TYPE_STRING);
  gtk_object_add_arg_type ("GtkWidget::style", GTK_TYPE_STYLE);
  gtk_object_add_arg_type ("GtkWidget::parent", GTK_TYPE_CONTAINER);
  
  widget_signals[SHOW] =
    gtk_signal_new ("show",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, show),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  widget_signals[HIDE] =
    gtk_signal_new ("hide",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, hide),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  widget_signals[MAP] =
    gtk_signal_new ("map",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, map),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  widget_signals[UNMAP] =
    gtk_signal_new ("unmap",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, unmap),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  widget_signals[REALIZE] =
    gtk_signal_new ("realize",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, realize),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  widget_signals[UNREALIZE] =
    gtk_signal_new ("unrealize",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, unrealize),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  widget_signals[DRAW] =
    gtk_signal_new ("draw",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, draw),
		    gtk_widget_marshal_signal_1,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);
  widget_signals[DRAW_FOCUS] =
    gtk_signal_new ("draw_focus",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, draw_focus),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  widget_signals[DRAW_DEFAULT] =
    gtk_signal_new ("draw_default",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, draw_default),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  widget_signals[SIZE_REQUEST] =
    gtk_signal_new ("size_request",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, size_request),
		    gtk_widget_marshal_signal_1,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);
  widget_signals[SIZE_ALLOCATE] =
    gtk_signal_new ("size_allocate",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, size_allocate),
		    gtk_widget_marshal_signal_1,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);
  widget_signals[STATE_CHANGED] =
    gtk_signal_new ("state_changed",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, state_changed),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  widget_signals[INSTALL_ACCELERATOR] =
    gtk_signal_new ("install_accelerator",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, install_accelerator),
		    gtk_widget_marshal_signal_2,
		    GTK_TYPE_BOOL, 3,
		    GTK_TYPE_STRING,
		    GTK_TYPE_CHAR,
		    GTK_TYPE_INT);
  widget_signals[REMOVE_ACCELERATOR] =
    gtk_signal_new ("remove_accelerator",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, remove_accelerator),
		    gtk_widget_marshal_signal_3,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_STRING);
  widget_signals[EVENT] =
    gtk_signal_new ("event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[BUTTON_PRESS_EVENT] =
    gtk_signal_new ("button_press_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, button_press_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[BUTTON_RELEASE_EVENT] =
    gtk_signal_new ("button_release_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, button_release_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[MOTION_NOTIFY_EVENT] =
    gtk_signal_new ("motion_notify_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, motion_notify_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DELETE_EVENT] =
    gtk_signal_new ("delete_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, delete_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DESTROY_EVENT] =
    gtk_signal_new ("destroy_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, destroy_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[EXPOSE_EVENT] =
    gtk_signal_new ("expose_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, expose_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[KEY_PRESS_EVENT] =
    gtk_signal_new ("key_press_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, key_press_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[KEY_RELEASE_EVENT] =
    gtk_signal_new ("key_release_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, key_release_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[ENTER_NOTIFY_EVENT] =
    gtk_signal_new ("enter_notify_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, enter_notify_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[LEAVE_NOTIFY_EVENT] =
    gtk_signal_new ("leave_notify_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, leave_notify_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[CONFIGURE_EVENT] =
    gtk_signal_new ("configure_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, configure_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[FOCUS_IN_EVENT] =
    gtk_signal_new ("focus_in_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, focus_in_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[FOCUS_OUT_EVENT] =
    gtk_signal_new ("focus_out_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, focus_out_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[MAP_EVENT] =
    gtk_signal_new ("map_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, map_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[UNMAP_EVENT] =
    gtk_signal_new ("unmap_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, unmap_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[PROPERTY_NOTIFY_EVENT] =
    gtk_signal_new ("property_notify_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, property_notify_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[SELECTION_CLEAR_EVENT] =
    gtk_signal_new ("selection_clear_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_clear_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[SELECTION_REQUEST_EVENT] =
    gtk_signal_new ("selection_request_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_request_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[SELECTION_NOTIFY_EVENT] =
    gtk_signal_new ("selection_notify_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_notify_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[SELECTION_RECEIVED] =
    gtk_signal_new ("selection_received",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, selection_received),
		    gtk_widget_marshal_signal_1,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[PROXIMITY_IN_EVENT] =
    gtk_signal_new ("proximity_in_event",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, proximity_in_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[PROXIMITY_OUT_EVENT] =
    gtk_signal_new ("proximity_out_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, proximity_out_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DRAG_BEGIN_EVENT] =
    gtk_signal_new ("drag_begin_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_begin_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DRAG_REQUEST_EVENT] =
    gtk_signal_new ("drag_request_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drag_request_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DROP_ENTER_EVENT] =
    gtk_signal_new ("drop_enter_event",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drop_enter_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DROP_LEAVE_EVENT] =
    gtk_signal_new ("drop_leave_event",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, drop_leave_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[DROP_DATA_AVAILABLE_EVENT] =
    gtk_signal_new ("drop_data_available_event",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass,
				       drop_data_available_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[OTHER_EVENT] =
    gtk_signal_new ("other_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, other_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);
  widget_signals[CLIENT_EVENT] =
    gtk_signal_new ("client_event",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkWidgetClass, client_event),
		    gtk_widget_marshal_signal_4,
		    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_GDK_EVENT);

  gtk_object_class_add_signals (object_class, widget_signals, LAST_SIGNAL);
  
  object_class->destroy = gtk_real_widget_destroy;
  
  klass->activate_signal = 0;
  klass->show = gtk_real_widget_show;
  klass->hide = gtk_real_widget_hide;
  klass->map = gtk_real_widget_map;
  klass->unmap = gtk_real_widget_unmap;
  klass->realize = gtk_real_widget_realize;
  klass->unrealize = gtk_real_widget_unrealize;
  klass->draw = gtk_real_widget_draw;
  klass->draw_focus = NULL;
  klass->size_request = NULL;
  klass->size_allocate = gtk_real_widget_size_allocate;
  klass->state_changed = NULL;
  klass->install_accelerator = NULL;
  klass->remove_accelerator = NULL;
  klass->event = NULL;
  klass->button_press_event = NULL;
  klass->button_release_event = NULL;
  klass->motion_notify_event = NULL;
  klass->delete_event = NULL;
  klass->destroy_event = NULL;
  klass->expose_event = NULL;
  klass->key_press_event = NULL;
  klass->key_release_event = NULL;
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
  klass->drag_begin_event = NULL;
  klass->drag_request_event = NULL;
  klass->drop_enter_event = NULL;
  klass->drop_leave_event = NULL;
  klass->drop_data_available_event = NULL;
  klass->other_event = NULL;
}

/*****************************************
 * gtk_widget_arg:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_arg (GtkWidget *widget,
		GtkArg	  *arg)
{
  if (strcmp (arg->name, "x") == 0)
    {
      gtk_widget_set_uposition (widget, GTK_VALUE_INT(*arg), -2);
    }
  else if (strcmp (arg->name, "y") == 0)
    {
      gtk_widget_set_uposition (widget, -2, GTK_VALUE_INT(*arg));
    }
  else if (strcmp (arg->name, "width") == 0)
    {
      gtk_widget_set_usize (widget, GTK_VALUE_INT(*arg), -1);
    }
  else if (strcmp (arg->name, "height") == 0)
    {
      gtk_widget_set_usize (widget, -1, GTK_VALUE_INT(*arg));
    }
  else if (strcmp (arg->name, "visible") == 0)
    {
      if (GTK_VALUE_BOOL(*arg))
	gtk_widget_show (widget);
      else
	gtk_widget_hide (widget);
    }
  else if (strcmp (arg->name, "sensitive") == 0)
    {
      gtk_widget_set_sensitive (widget, GTK_VALUE_BOOL(*arg));
    }
  else if (strcmp (arg->name, "events") == 0)
    {
      gtk_widget_set_events (widget, GTK_VALUE_FLAGS(*arg));
    }
  else if (strcmp (arg->name, "extension_events") == 0)
    {
      gtk_widget_set_extension_events (widget, GTK_VALUE_FLAGS(*arg));
    }
  else if (strcmp (arg->name, "name") == 0)
    {
      gtk_widget_set_name (widget, GTK_VALUE_STRING(*arg));
    }
  else if (strcmp (arg->name, "style") == 0)
    {
      gtk_widget_set_style (widget, (GtkStyle*)GTK_VALUE_BOXED(*arg));
    }
  else if (strcmp (arg->name, "parent") == 0)
    {
      gtk_container_add (GTK_CONTAINER (GTK_VALUE_OBJECT(*arg)), widget);
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
  
  GTK_OBJECT_FLAGS (widget) = GTK_SENSITIVE | GTK_PARENT_SENSITIVE;
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
  
  widget->style = gtk_widget_peek_style ();
  gtk_style_ref (widget->style);
  
  colormap = gtk_widget_peek_colormap ();
  visual = gtk_widget_peek_visual ();
  
  /* XXX - should we ref the colormap and visual, too? */

  if (colormap != gtk_widget_get_default_colormap ())
    {
      /* gdk_colormap_ref (colormap); */
      gtk_object_set_data (GTK_OBJECT (widget), colormap_key, colormap);
    }

  if (visual != gtk_widget_get_default_visual ())
    {
      /* gdk_visual_ref (visual); */
      gtk_object_set_data (GTK_OBJECT (widget), visual_key, visual);
    }
}

/*****************************************
 * gtk_widget_new:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkWidget*
gtk_widget_new (guint type,
		...)
{
  GtkObject *obj;
  GtkArg *args;
  gint nargs;
  va_list args1;
  va_list args2;
  
  g_return_val_if_fail (gtk_type_is_a (type, gtk_widget_get_type ()), NULL);
  
  obj = gtk_type_new (type);
  
  va_start (args1, type);
  va_start (args2, type);
  
  args = gtk_object_collect_args (&nargs, args1, args2);
  gtk_object_setv (obj, nargs, args);
  g_free (args);
  
  va_end (args1);
  va_end (args2);
  
  return GTK_WIDGET (obj);
}

/*****************************************
 * gtk_widget_newv:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkWidget*
gtk_widget_newv (guint	 type,
		 gint	 nargs,
		 GtkArg *args)
{
  g_return_val_if_fail (gtk_type_is_a (type, gtk_widget_get_type ()), NULL);
  
  return GTK_WIDGET (gtk_object_newv (type, nargs, args));
}

/*****************************************
 * gtk_widget_set:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_set (GtkWidget *widget,
		...)
{
  GtkArg *args;
  gint nargs;
  va_list args1;
  va_list args2;
  
  g_return_if_fail (widget != NULL);
  
  va_start (args1, widget);
  va_start (args2, widget);
  
  args = gtk_object_collect_args (&nargs, args1, args2);
  gtk_object_setv (GTK_OBJECT (widget), nargs, args);
  g_free (args);
  
  va_end (args1);
  va_end (args2);
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
		 gint	    nargs,
		 GtkArg	   *args)
{
  gtk_object_setv (GTK_OBJECT (widget), nargs, args);
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
  GSList *resize_widgets;
  GSList *tmp_list;
  gint tag;
  
  g_return_if_fail (widget != NULL);
  
  if (GTK_WIDGET_REDRAW_PENDING (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_REDRAW_PENDING);
      gtk_object_unref (GTK_OBJECT (widget));
      tag = (gint) gtk_object_get_data (GTK_OBJECT (widget), redraw_handler_key);
      gtk_idle_remove (tag);
      gtk_object_remove_data (GTK_OBJECT (widget), redraw_handler_key);
    }
  
  if (GTK_WIDGET_ANCHORED (widget) &&
      GTK_WIDGET_RESIZE_PENDING (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_RESIZE_PENDING);
      gtk_object_unref (GTK_OBJECT (widget));
      tag = (gint) gtk_object_get_data (GTK_OBJECT (widget), resize_handler_key);
      gtk_idle_remove (tag);
      gtk_object_remove_data (GTK_OBJECT (widget), resize_handler_key);
      
      resize_widgets = gtk_object_get_data (GTK_OBJECT (widget), resize_widgets_key);
      
      tmp_list = resize_widgets;
      while (tmp_list)
	{
	  GtkWidget *child;
	  
	  child = tmp_list->data;
	  tmp_list = tmp_list->next;
	  
	  /* referencing needed? */
	  GTK_WIDGET_UNSET_FLAGS (child, GTK_RESIZE_NEEDED);
	  gtk_object_unref (GTK_OBJECT (child));
	}
      
      if (resize_widgets)
	{
	  gtk_object_set_data (GTK_OBJECT (widget), resize_widgets_key, NULL);
	  g_slist_free (resize_widgets);
	}
    }
  
  if (GTK_WIDGET_RESIZE_NEEDED (widget))
    {
      GtkWidget *toplevel;
      
      toplevel = gtk_widget_get_toplevel (widget);
      resize_widgets = gtk_object_get_data (GTK_OBJECT (toplevel), resize_widgets_key);
      
      if (resize_widgets)
	{
	  resize_widgets = g_slist_remove (resize_widgets, widget);
	  GTK_WIDGET_UNSET_FLAGS (widget, GTK_RESIZE_NEEDED);
	  gtk_object_unref (GTK_OBJECT (widget));
	  
	  gtk_object_set_data (GTK_OBJECT (toplevel), resize_widgets_key, resize_widgets);
	}
    }
  
  
  if (widget->parent)
    {
      if (!GTK_OBJECT_BEING_DESTROYED (widget->parent))
	gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
      else
	gtk_object_unref (GTK_OBJECT (widget));
    }
  gtk_object_destroy (GTK_OBJECT (widget));
}

/*****************************************
 * gtk_widget_unparent: do any cleanup necessary necessary before
 *			setting parent = NULL. In particular, remove
 *			the focus properly.
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_unparent (GtkWidget *widget)
{
  GtkWidget *toplevel;
  GtkWidget *child;
  
  g_return_if_fail (widget != NULL);
  
  toplevel = gtk_widget_get_toplevel (widget);
  if (GTK_IS_WINDOW (toplevel))
    {
      child = GTK_WINDOW (toplevel)->focus_widget;
      
      while (child && child != widget)
	child = child->parent;
      
      if (child == widget)
	gtk_window_set_focus (GTK_WINDOW (toplevel), NULL);
    }
  
  if (widget->window &&
      GTK_WIDGET_NO_WINDOW (widget) &&
      GTK_WIDGET_DRAWABLE (widget))
    gdk_window_clear_area (widget->window,
			   widget->allocation.x,
			   widget->allocation.y,
			   widget->allocation.width,
			   widget->allocation.height);
  
  widget->parent = NULL;
  
  gtk_object_unref (GTK_OBJECT (widget));
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
  
  if (!GTK_WIDGET_VISIBLE (widget))
    gtk_signal_emit (GTK_OBJECT (widget), widget_signals[SHOW]);
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
  
  if (GTK_WIDGET_VISIBLE (widget))
    gtk_signal_emit (GTK_OBJECT (widget), widget_signals[HIDE]);
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
  g_return_if_fail (widget != NULL);
  
  if (!GTK_WIDGET_MAPPED (widget))
    {
      if (!GTK_WIDGET_REALIZED (widget))
	gtk_widget_realize (widget);
      
      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[MAP]);
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
  
  if (GTK_WIDGET_MAPPED (widget))
    gtk_signal_emit (GTK_OBJECT (widget), widget_signals[UNMAP]);
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
  GtkStyle *new_style;
  gint events;
  GdkExtensionMode mode;
  GtkWidgetShapeInfo *shape_info;
  
  g_return_if_fail (widget != NULL);
  
  if (!GTK_WIDGET_REALIZED (widget))
    {
      /*
	if (GTK_IS_CONTAINER (widget) && !GTK_WIDGET_NO_WINDOW (widget))
	  g_print ("%s\n", gtk_type_name (GTK_WIDGET_TYPE (widget)));
      */
      
      if (widget->parent && !GTK_WIDGET_REALIZED (widget->parent))
	gtk_widget_realize (widget->parent);
      
      if (!GTK_WIDGET_USER_STYLE (widget))
	{
	  new_style = gtk_rc_get_style (widget);
	  if (new_style != widget->style)
	    gtk_widget_set_style_internal (widget, new_style);
	}
      
      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[REALIZE]);
      
      if (GTK_WIDGET_HAS_SHAPE_MASK(widget))
	{
	  shape_info = gtk_object_get_data (GTK_OBJECT (widget),
					    shape_info_key);
	  g_assert (shape_info != 0);
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
  
  if (GTK_WIDGET_REALIZED (widget))
    gtk_signal_emit (GTK_OBJECT (widget), widget_signals[UNREALIZE]);
}

/*****************************************
 * gtk_widget_queue_draw:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_queue_draw (GtkWidget *widget)
{
  GtkWidget *parent;
  gint tag;
  
  g_return_if_fail (widget != NULL);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      /* We queue the redraw if:
       *  a) the widget is not already queued for redraw and
       *  b) non of the widgets ancestors are queued for redraw.
       */
      parent = widget;
      while (parent)
	{
	  if (GTK_WIDGET_REDRAW_PENDING (parent))
	    return;
	  parent = parent->parent;
	}
      
      GTK_WIDGET_SET_FLAGS (widget, GTK_REDRAW_PENDING);
      gtk_object_ref (GTK_OBJECT (widget));
      tag = gtk_idle_add ((GtkFunction) gtk_real_widget_queue_draw, widget);
      gtk_object_set_data (GTK_OBJECT (widget), redraw_handler_key, (gpointer) tag);
    }
}

/*****************************************
 * gtk_widget_queue_resize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_queue_resize (GtkWidget *widget)
{
  GtkWidget *toplevel;
  GSList *resize_widgets;
  gint tag;
  
  g_return_if_fail (widget != NULL);
  
  toplevel = gtk_widget_get_toplevel (widget);
  if (GTK_WIDGET_ANCHORED (toplevel))
    {
      if (GTK_WIDGET_VISIBLE (toplevel))
	{
	  if (!GTK_WIDGET_RESIZE_PENDING (toplevel))
	    {
	      GTK_WIDGET_SET_FLAGS (toplevel, GTK_RESIZE_PENDING);
	      gtk_object_ref (GTK_OBJECT (toplevel));
	      tag = gtk_idle_add ((GtkFunction) gtk_real_widget_queue_resize, toplevel);
	      gtk_object_set_data (GTK_OBJECT (toplevel), resize_handler_key, (gpointer) tag);
	    }
	  
	  resize_widgets = gtk_object_get_data (GTK_OBJECT (toplevel), resize_widgets_key);
	  if (g_slist_find (resize_widgets, widget) == NULL)
	    {
	      /* referencing needed? */
	      GTK_WIDGET_SET_FLAGS (widget, GTK_RESIZE_NEEDED);
	      gtk_object_ref (GTK_OBJECT (widget));
	      resize_widgets = g_slist_prepend (resize_widgets, widget);
	      
	      gtk_object_set_data (GTK_OBJECT (toplevel), resize_widgets_key, resize_widgets);
	    }
	}
      else
	{
	  gtk_container_need_resize (GTK_CONTAINER (toplevel));
	}
    }
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
  
  if (GTK_WIDGET_DRAWABLE (widget) &&
      !GTK_WIDGET_REDRAW_PENDING (widget))
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
  
  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[DRAW_DEFAULT]);
}

/*****************************************
 * gtk_widget_draw_children:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_draw_children (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
			   gtk_widget_draw_children_recurse,
			   NULL);
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
  GtkWidgetAuxInfo *aux_info;
  
  g_return_if_fail (widget != NULL);
  
  if (gtk_signal_emit (GTK_OBJECT (widget), widget_signals[SIZE_REQUEST], requisition))
    {
      aux_info = gtk_object_get_data (GTK_OBJECT (widget), aux_info_key);
      if (aux_info)
	{
	  if (aux_info->width > 0)
	    requisition->width = aux_info->width;
	  if (aux_info->height > 0)
	    requisition->height = aux_info->height;
	}
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
  
  g_return_if_fail (widget != NULL);
  
  real_allocation = *allocation;
  aux_info = gtk_object_get_data (GTK_OBJECT (widget), aux_info_key);
  
  if (aux_info)
    {
      if (aux_info->x != -1)
	real_allocation.x = aux_info->x;
      if (aux_info->y != -1)
	real_allocation.y = aux_info->y;
    }
  
  gtk_signal_emit (GTK_OBJECT (widget), widget_signals[SIZE_ALLOCATE], &real_allocation);
}

/*****************************************
 * gtk_widget_install_accelerator:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_install_accelerator (GtkWidget	    *widget,
				GtkAcceleratorTable *table,
				const gchar	    *signal_name,
				gchar		     key,
				guint8		     modifiers)
{
  gint return_val;
  
  g_return_if_fail (widget != NULL);
  
  return_val = TRUE;
  if (gtk_signal_emit (GTK_OBJECT (widget), widget_signals[INSTALL_ACCELERATOR],
		       signal_name, key, modifiers, &return_val) && return_val)
    gtk_accelerator_table_install (table, GTK_OBJECT (widget), signal_name, key, modifiers);
}

/*****************************************
 * gtk_widget_remove_accelerator:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_remove_accelerator (GtkWidget	   *widget,
			       GtkAcceleratorTable *table,
			       const gchar	   *signal_name)
{
  g_return_if_fail (widget != NULL);
  
  if (gtk_signal_emit (GTK_OBJECT (widget), widget_signals[REMOVE_ACCELERATOR], signal_name))
    gtk_accelerator_table_remove (table, GTK_OBJECT (widget), signal_name);
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
  
  g_return_val_if_fail (widget != NULL, FALSE);
  
  return_val = FALSE;
  if (gtk_signal_emit (GTK_OBJECT (widget), widget_signals[EVENT], event, &return_val))
    {
      if (return_val)
	return TRUE;
      
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
	case GDK_EXPOSE:
	  signal_num = EXPOSE_EVENT;
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
	case GDK_DRAG_BEGIN:
	  signal_num = DRAG_BEGIN_EVENT;
	  break;
	case GDK_DRAG_REQUEST:
	  signal_num = DRAG_REQUEST_EVENT;
	  break;
	case GDK_DROP_ENTER:
	  signal_num = DROP_ENTER_EVENT;
	  break;
	case GDK_DROP_LEAVE:
	  signal_num = DROP_LEAVE_EVENT;
	  break;
	case GDK_DROP_DATA_AVAIL:
	  signal_num = DROP_DATA_AVAILABLE_EVENT;
	  break;
	case GDK_OTHER_EVENT:
	  signal_num = OTHER_EVENT;
	  break;
	case GDK_CLIENT_EVENT:
	  signal_num = CLIENT_EVENT;
	  break;
	default:
	  g_warning ("could not determine signal number for event: %d", event->type);
	  return return_val;
	}
      
      if (signal_num != -1)
	gtk_signal_emit (GTK_OBJECT (widget), widget_signals[signal_num], event, &return_val);
    }
  
  return return_val;
}

/*****************************************
 * gtk_widget_activate:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_activate (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (WIDGET_CLASS (widget)->activate_signal)
    gtk_signal_emit (GTK_OBJECT (widget), WIDGET_CLASS (widget)->activate_signal);
}

/*****************************************
 * gtk_widget_reparent:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_reparent_container_child(GtkWidget *widget,
                                   gpointer   client_data)
{
  GtkWidget *new_parent = GTK_WIDGET (client_data);

  g_return_if_fail (widget != NULL);
  g_return_if_fail (client_data != NULL);

  if (!GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_reparent (widget->window, new_parent->window, 0, 0);
  else if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
			   gtk_widget_reparent_container_child,
			   new_parent);
  else
    widget->window = new_parent->window;
}


void
gtk_widget_reparent (GtkWidget *widget,
		     GtkWidget *new_parent)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (new_parent != NULL);
  g_return_if_fail (GTK_IS_CONTAINER (new_parent));
  
  if (widget->parent != new_parent)
    {
      gtk_object_ref (GTK_OBJECT (widget));
      gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
      gtk_container_add (GTK_CONTAINER (new_parent), widget);
      gtk_object_unref (GTK_OBJECT (widget));

      if (GTK_WIDGET_REALIZED (widget))
	{
	  if (GTK_WIDGET_REALIZED (new_parent))
	    {
	      if (GTK_WIDGET_NO_WINDOW (widget))
		{
		  if (GTK_IS_CONTAINER (widget))
		    gtk_container_foreach (GTK_CONTAINER(widget),
					   gtk_widget_reparent_container_child,
					   new_parent);
		  else
		    widget->window = widget->parent->window;
		}
	      else
		gdk_window_reparent (widget->window, widget->parent->window, 0, 0);
	    }
	  else
	    gtk_widget_unrealize (widget);
	}
      
      if (!GTK_WIDGET_REALIZED (widget) && GTK_WIDGET_REALIZED (new_parent))
	gtk_widget_realize (widget);
      if (GTK_WIDGET_VISIBLE (widget) &&
	  !GTK_WIDGET_MAPPED (widget) && GTK_WIDGET_MAPPED (new_parent))
	gtk_widget_map (widget);
      
      gtk_widget_queue_resize (widget);
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


gint
gtk_widget_basic (GtkWidget *widget)
{
  GList *children;
  GList *tmp_list;
  gint return_val;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  
  if (!GTK_WIDGET_BASIC (widget))
    return FALSE;
  else if (GTK_IS_CONTAINER (widget))
    {
      children = gtk_container_children (GTK_CONTAINER (widget));
      if (children)
	{
	  return_val = TRUE;
	  tmp_list = children;
	  
	  while (tmp_list)
	    {
	      if (!gtk_widget_basic (GTK_WIDGET (tmp_list->data)))
		{
		  return_val = FALSE;
		  break;
		}
	      
	      tmp_list = tmp_list->next;
	    }
	  
	  g_list_free (children);
	  return return_val;
	}
    }
  
  return TRUE;
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
  GtkWidget *window;
  GtkWidget *child;
  gint window_type;
  
  g_return_if_fail (widget != NULL);
  
  window_type = gtk_window_get_type ();
  window = widget->parent;
  child = widget;
  
  while (window && !gtk_type_is_a (GTK_WIDGET_TYPE (window), window_type))
    {
      GTK_CONTAINER (window)->focus_child = child;
      child = window;
      window = window->parent;
    }
  
  if (window && gtk_type_is_a (GTK_WIDGET_TYPE (window), window_type))
    {
      GTK_CONTAINER (window)->focus_child = child;
      gtk_window_set_focus (GTK_WINDOW (window), widget);
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
  gint window_type;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_WIDGET_CAN_DEFAULT (widget));
  
  window_type = gtk_window_get_type ();
  window = widget->parent;
  
  while (window && !gtk_type_is_a (GTK_WIDGET_TYPE (window), window_type))
    window = window->parent;
  
  if (window && gtk_type_is_a (GTK_WIDGET_TYPE (window), window_type))
    gtk_window_set_default (GTK_WINDOW (window), widget);
}

/*****************************************
 * gtk_widget_restore_state:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_restore_state (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  
  widget->state = widget->saved_state;
  if (gtk_signal_emit (GTK_OBJECT (widget), widget_signals[STATE_CHANGED]))
    {
      if (GTK_IS_CONTAINER (widget))
	gtk_container_foreach (GTK_CONTAINER (widget),
			       gtk_widget_propagate_restore,
			       NULL);
    }
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
  GtkStyle *new_style;
  
  g_return_if_fail (widget != NULL);
  
  if (widget->name)
    g_free (widget->name);
  widget->name = g_strdup (name);
  
  if (!GTK_WIDGET_USER_STYLE (widget))
    {
      new_style = gtk_rc_get_style (widget);
      gtk_widget_set_style_internal (widget, new_style);
    }
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
  
  if (widget->name)
    return widget->name;
  return gtk_type_name (GTK_WIDGET_TYPE (widget));
}

/*****************************************
 * gtk_widget_set_state:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_set_state (GtkWidget	   *widget,
		      GtkStateType  state)
{
  g_return_if_fail (widget != NULL);
  
  if (widget->state != state)
    {
      widget->saved_state = widget->state;
      widget->state = state;
      
      if (!gtk_signal_emit (GTK_OBJECT (widget), widget_signals[STATE_CHANGED]))
	return;
    }
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
			   gtk_widget_propagate_state,
			   (gpointer) &state);
}

/*****************************************
 * gtk_widget_set_sensitive:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_set_sensitive (GtkWidget *widget,
			  gint	     sensitive)
{
  GtkWidget *window;
  gint old_val;
  
  g_return_if_fail (widget != NULL);
  
  old_val = GTK_WIDGET_IS_SENSITIVE (widget);
  
  if (sensitive)
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_SENSITIVE);
    }
  else
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_SENSITIVE);
      
      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  window = gtk_widget_get_ancestor (widget, gtk_window_get_type ());
	  if (window)
	    gtk_window_set_focus (GTK_WINDOW (window), NULL);
	}
    }
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
			   gtk_widget_set_parent_sensitive,
			   &sensitive);
  
  if (old_val != GTK_WIDGET_IS_SENSITIVE (widget))
    gtk_widget_queue_draw (widget);
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
  GtkStyle *style;
  gint sensitive;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (parent != NULL);
  
  gtk_object_ref (GTK_OBJECT (widget));
  
  widget->parent = parent;
  
  sensitive = GTK_WIDGET_IS_SENSITIVE (parent);
  gtk_widget_set_parent_sensitive (widget, &sensitive);
  
  if ((widget->parent->state != GTK_STATE_NORMAL) &&
      (widget->parent->state != widget->state))
    gtk_widget_set_state (widget, widget->parent->state);
  
  while (parent->parent != NULL)
    parent = parent->parent;
  
  if (GTK_WIDGET_ANCHORED (parent))
    {
      if (!GTK_WIDGET_USER_STYLE (widget))
	{
	  style = gtk_rc_get_style (widget);
	  if (style != widget->style)
	    gtk_widget_set_style_internal (widget, style);
	}
      
      if (GTK_IS_CONTAINER (widget))
	gtk_container_foreach (GTK_CONTAINER (widget),
			       gtk_widget_set_style_recurse,
			       NULL);
    }
}

/*****************************************
 * gtk_widget_set_style:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_set_style (GtkWidget *widget,
		      GtkStyle	*style)
{
  g_return_if_fail (widget != NULL);
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_USER_STYLE);
  gtk_widget_set_style_internal (widget, style);
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
  
  aux_info = gtk_object_get_data (GTK_OBJECT (widget), aux_info_key);
  if (!aux_info)
    {
      aux_info = gtk_widget_aux_info_new ();
      gtk_object_set_data (GTK_OBJECT (widget), aux_info_key, aux_info);
    }
  
  if (x > -2)
    aux_info->x = x;
  if (y > -2)
    aux_info->y = y;
  
  if (GTK_WIDGET_REALIZED (widget) && GTK_IS_WINDOW (widget) &&
      (aux_info->x != -1) && (aux_info->y != -1))
    {
      gdk_window_set_hints (widget->window, aux_info->x, aux_info->y, 0, 0, 0, 0, GDK_HINT_POS);
      gdk_window_move (widget->window, aux_info->x, aux_info->y);
    }
  
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
  
  aux_info = gtk_object_get_data (GTK_OBJECT (widget), aux_info_key);
  if (!aux_info)
    {
      aux_info = gtk_widget_aux_info_new ();
      gtk_object_set_data (GTK_OBJECT (widget), aux_info_key, aux_info);
    }
  
  if (width > -1)
    aux_info->width = width;
  if (height > -1)
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
  g_return_if_fail (!GTK_WIDGET_NO_WINDOW (widget));
  g_return_if_fail (!GTK_WIDGET_REALIZED (widget));
  
  eventp = gtk_object_get_data (GTK_OBJECT (widget), event_key);
  
  if (events)
    {
      if (!eventp)
	eventp = g_new (gint, 1);
      
      *eventp = events;
      gtk_object_set_data (GTK_OBJECT (widget), event_key, eventp);
    }
  else
    {
      if (eventp)
	g_free (eventp);
      
      gtk_object_remove_data (GTK_OBJECT (widget), event_key);
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
  
  modep = gtk_object_get_data (GTK_OBJECT (widget), extension_event_key);
  
  if (!modep)
    modep = g_new (GdkExtensionMode, 1);
  
  *modep = mode;
  gtk_object_set_data (GTK_OBJECT (widget), extension_event_key, modep);
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
			 gint	    type)
{
  g_return_val_if_fail (widget != NULL, NULL);
  
  while (widget && !gtk_type_is_a (GTK_WIDGET_TYPE (widget), type))
    widget = widget->parent;
  
  if (!(widget && gtk_type_is_a (GTK_WIDGET_TYPE (widget), type)))
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
  
  if (!widget->window)
    {
      colormap = gtk_object_get_data (GTK_OBJECT (widget), colormap_key);
      if (colormap)
	return colormap;
      return gtk_widget_get_default_colormap ();
    }
  
  return gdk_window_get_colormap (widget->window);
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
  
  if (!widget->window)
    {
      visual = gtk_object_get_data (GTK_OBJECT (widget), visual_key);
      if (visual)
	return visual;
      return gtk_widget_get_default_visual ();
    }
  
  return gdk_window_get_visual (widget->window);
}

/*****************************************
 * gtk_widget_get_style:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkStyle*
gtk_widget_get_style (GtkWidget *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);
  
  return widget->style;
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
  
  events = gtk_object_get_data (GTK_OBJECT (widget), event_key);
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
  
  mode = gtk_object_get_data (GTK_OBJECT (widget), extension_event_key);
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

gint
gtk_widget_is_ancestor (GtkWidget *widget,
			GtkWidget *ancestor)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (ancestor != NULL, FALSE);
  
  while (widget)
    {
      if (widget->parent == ancestor)
	return TRUE;
      widget = widget->parent;
    }
  
  return FALSE;
}

/*****************************************
 * gtk_widget_is_child:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

gint
gtk_widget_is_child (GtkWidget *widget,
		     GtkWidget *child)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (child != NULL, FALSE);
  
  return (child->parent == widget);
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
  visual_stack = g_slist_prepend (visual_stack, visual);
}

/*****************************************
 * gtk_widget_push_style:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_push_style (GtkStyle *style)
{
  gtk_style_ref (style);
  style_stack = g_slist_prepend (style_stack, style);
}

/*****************************************
 * gtk_widget_pop_colormap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_pop_colormap ()
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
gtk_widget_pop_visual ()
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
 * gtk_widget_pop_style:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_pop_style ()
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
 * gtk_widget_set_default_style:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

void
gtk_widget_set_default_style (GtkStyle *style)
{
   if (style != default_style)
     {
       if (default_style)
	 gtk_style_unref (default_style);
       default_style = style;
       if (default_style)
	 gtk_style_ref (default_style);
     }
}

/* Basically, send a message to all toplevel windows telling them
 * that a new _GTK_STYLE_COLORS property is available on the root
 * window
 */
void
gtk_widget_propagate_default_style (void)
{
  GdkEventClient sev;
  int i;
  
  /* Set the property on the root window */
  gdk_property_change(GDK_ROOT_PARENT(),
		      gdk_atom_intern("_GTK_DEFAULT_COLORS", FALSE),
		      gdk_atom_intern("STRING", FALSE),
		      8*sizeof(gushort),
		      GDK_PROP_MODE_REPLACE,
		      (guchar *)gtk_widget_get_default_style(),
		      GTK_STYLE_NUM_STYLECOLORS() * sizeof(GdkColor));

  for(i = 0; i < 5; i++)
    sev.data.l[i] = 0;
  sev.data_format = 32;
  sev.message_type = gdk_atom_intern ("_GTK_STYLE_CHANGED", FALSE);
  gdk_event_send_clientmessage_toall ((GdkEvent *) &sev);
}

/*****************************************
 * gtk_widget_get_default_colormap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GdkColormap*
gtk_widget_get_default_colormap ()
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
gtk_widget_get_default_visual ()
{
  if (!default_visual)
    default_visual = gdk_visual_get_system ();
  
  return default_visual;
}

/*****************************************
 * gtk_widget_get_default_style:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

GtkStyle*
gtk_widget_get_default_style ()
{
  if (!default_style)
    {
      default_style = gtk_style_new ();
      gtk_style_ref (default_style);
    }
  
  return default_style;
}


/*****************************************
 * gtk_widget_marshal_signal_1:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_marshal_signal_1 (GtkObject	    *object,
			     GtkSignalFunc   func,
			     gpointer	     func_data,
			     GtkArg	    *args)
{
  GtkWidgetSignal1 rfunc;
  
  rfunc = (GtkWidgetSignal1) func;
  
  (* rfunc) (object, GTK_VALUE_POINTER (args[0]), func_data);
}

/*****************************************
 * gtk_widget_marshal_signal_2:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_marshal_signal_2 (GtkObject	    *object,
			     GtkSignalFunc   func,
			     gpointer	     func_data,
			     GtkArg	    *args)
{
  GtkWidgetSignal2 rfunc;
  gint *return_val;
  
  rfunc = (GtkWidgetSignal2) func;
  return_val = GTK_RETLOC_BOOL (args[3]);
  
  *return_val = (* rfunc) (object, GTK_VALUE_STRING (args[0]),
			   GTK_VALUE_CHAR (args[1]), GTK_VALUE_INT (args[2]),
			   func_data);
}

/*****************************************
 * gtk_widget_marshal_signal_3:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_marshal_signal_3 (GtkObject	    *object,
			     GtkSignalFunc   func,
			     gpointer	     func_data,
			     GtkArg	    *args)
{
  GtkWidgetSignal3 rfunc;
  
  rfunc = (GtkWidgetSignal3) func;
  
  (* rfunc) (object, GTK_VALUE_STRING (args[0]), func_data);
}

/*****************************************
 * gtk_widget_marshal_signal_4:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_marshal_signal_4 (GtkObject	    *object,
			     GtkSignalFunc   func,
			     gpointer	     func_data,
			     GtkArg	    *args)
{
  GtkWidgetSignal4 rfunc;
  gint *return_val;
  
  rfunc = (GtkWidgetSignal4) func;
  return_val = GTK_RETLOC_BOOL (args[1]);
  
  *return_val = (* rfunc) (object, GTK_VALUE_BOXED (args[0]), func_data);
}

/*****************************************
 * gtk_real_widget_destroy:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_real_widget_destroy (GtkObject *object)
{
  GtkWidget *widget;
  GtkWidgetAuxInfo *aux_info;
  gint *events;
  GdkExtensionMode *mode;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_WIDGET (object));
  
  widget = GTK_WIDGET (object);
  
  if (GTK_WIDGET_REDRAW_PENDING (widget))
    g_warning ("redraw pending\n");
  if (GTK_WIDGET_RESIZE_PENDING (widget))
    g_warning ("resize pending\n");
  if (GTK_WIDGET_RESIZE_NEEDED (widget))
    g_warning ("resize needed\n");
  
  gtk_grab_remove (widget);
  
  gtk_selection_remove_all (widget);
  
  if (widget->name)
    g_free (widget->name);
  
  aux_info = gtk_object_get_data (GTK_OBJECT (widget), aux_info_key);
  if (aux_info)
    {
      gtk_widget_aux_info_destroy (aux_info);
      gtk_object_remove_data (GTK_OBJECT (widget), aux_info_key);
    }
  
  events = gtk_object_get_data (GTK_OBJECT (object), event_key);
  if (events)
    g_free (events);
  
  mode = gtk_object_get_data (GTK_OBJECT (object), extension_event_key);
  if (mode)
    g_free (mode);
  
  if (GTK_WIDGET_REALIZED (widget))
    gtk_widget_unrealize (widget);
  gtk_style_unref (widget->style);
  
  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/*****************************************
 * gtk_real_widget_show:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_real_widget_show (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (!GTK_WIDGET_VISIBLE (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);
      
      if (widget->parent)
	{
	  gtk_widget_queue_resize (widget);
	  
	  if (GTK_WIDGET_MAPPED (widget->parent))
	    gtk_widget_map (widget);
	}
    }
}

/*****************************************
 * gtk_real_widget_hide:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_real_widget_hide (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (GTK_WIDGET_VISIBLE (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_VISIBLE);
      
      if (GTK_WIDGET_MAPPED (widget))
	gtk_widget_unmap (widget);
      
      if (widget->parent)
	gtk_widget_queue_resize (widget);
    }
}

/*****************************************
 * gtk_real_widget_map:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_real_widget_map (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (GTK_WIDGET_REALIZED (widget) && !GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
      
      if (!GTK_WIDGET_NO_WINDOW (widget))
	gdk_window_show (widget->window);
      else
	gtk_widget_queue_draw (widget);
    }
}

/*****************************************
 * gtk_real_widget_unmap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_real_widget_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
      
      if (GTK_WIDGET_NO_WINDOW (widget))
	gdk_window_clear_area (widget->window,
			       widget->allocation.x,
			       widget->allocation.y,
			       widget->allocation.width,
			       widget->allocation.height);
      else
	gdk_window_hide (widget->window);
    }
}

/*****************************************
 * gtk_real_widget_realize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_real_widget_realize (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  if(widget->parent)
    widget->window = widget->parent->window;
  widget->style = gtk_style_attach (widget->style, widget->window);
}

/*****************************************
 * gtk_real_widget_unrealize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_real_widget_unrealize (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED | GTK_MAPPED);

  gtk_style_detach (widget->style);
  if (!GTK_WIDGET_NO_WINDOW (widget))
    {
      gdk_window_set_user_data (widget->window, NULL);
      gdk_window_destroy (widget->window);
    }
  widget->window = NULL;
}

/*****************************************
 * gtk_real_widget_draw:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_real_widget_draw (GtkWidget	   *widget,
		      GdkRectangle *area)
{
  GdkEventExpose event;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (area != NULL);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      event.type = GDK_EXPOSE;
      event.window = widget->window;
      event.area = *area;
      
      gtk_widget_event (widget, (GdkEvent*) &event);
    }
}

/*****************************************
 * gtk_real_widget_queue_draw:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static gint
gtk_real_widget_queue_draw (GtkWidget *widget)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_REDRAW_PENDING);
  
  gtk_object_unref (GTK_OBJECT (widget));
  if (GTK_OBJECT_NEED_DESTROY (widget) &&
      (GTK_OBJECT (widget)->ref_count == 0))
    gtk_widget_destroy (widget);
  else
    gtk_widget_draw (widget, NULL);
  
  return FALSE;
}

/*****************************************
 * gtk_real_widget_queue_resize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static gint
gtk_real_widget_queue_resize (GtkWidget *widget)
{
  GSList *resize_widgets;
  GSList *tmp_list;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_RESIZE_PENDING);
  
  gtk_object_unref (GTK_OBJECT (widget));
  if (GTK_OBJECT_NEED_DESTROY (widget) &&
      (GTK_OBJECT (widget)->ref_count == 0))
    {
      gtk_widget_destroy (widget);
    }
  else
    {
      gtk_container_need_resize (GTK_CONTAINER (widget));
      
      if (!GTK_WIDGET_RESIZE_PENDING (widget))
	{
	  resize_widgets = gtk_object_get_data (GTK_OBJECT (widget), resize_widgets_key);
	  
	  tmp_list = resize_widgets;
	  while (tmp_list)
	    {
	      GtkWidget *child;
	      
	      child = tmp_list->data;
	      tmp_list = tmp_list->next;
	      
	      /* referencing needed? */
	      GTK_WIDGET_UNSET_FLAGS (child, GTK_RESIZE_NEEDED);
	      gtk_object_unref (GTK_OBJECT (child));
	    }
	  
	  if (resize_widgets)
	    {
	      gtk_object_set_data (GTK_OBJECT (widget), resize_widgets_key, NULL);
	      g_slist_free (resize_widgets);
	    }
	}
    }
  
  return FALSE;
}

/*****************************************
 * gtk_real_widget_size_allocate:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_real_widget_size_allocate (GtkWidget     *widget,
			       GtkAllocation *allocation)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  if (GTK_WIDGET_NO_WINDOW (widget) &&
      GTK_WIDGET_MAPPED (widget) &&
      ((widget->allocation.x != allocation->x) ||
       (widget->allocation.y != allocation->y) ||
       (widget->allocation.width != allocation->width) ||
       (widget->allocation.height != allocation->height)) &&
      (widget->allocation.width != 0) &&
      (widget->allocation.height != 0))
    gdk_window_clear_area (widget->window,
			   widget->allocation.x,
			   widget->allocation.y,
			   widget->allocation.width,
			   widget->allocation.height);
  
  widget->allocation = *allocation;
  
  if (GTK_WIDGET_REALIZED (widget) &&
      !GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);
}

/*****************************************
 * gtk_widget_peek_colormap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static GdkColormap*
gtk_widget_peek_colormap ()
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
gtk_widget_peek_visual ()
{
  if (visual_stack)
    return (GdkVisual*) visual_stack->data;
  return gtk_widget_get_default_visual ();
}

/*****************************************
 * gtk_widget_peek_style:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static GtkStyle*
gtk_widget_peek_style ()
{
  if (style_stack)
    return (GtkStyle*) style_stack->data;
  return gtk_widget_get_default_style ();
}


/*****************************************
 * gtk_widget_set_parent_sensitive:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_set_parent_sensitive (GtkWidget *widget,
				 gpointer   client_data)
{
  GtkWidget *window;
  gint *sensitive;
  
  sensitive = client_data;
  if (*sensitive)
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_PARENT_SENSITIVE);
      
      if (GTK_IS_CONTAINER (widget) && GTK_WIDGET_SENSITIVE (widget))
	gtk_container_foreach (GTK_CONTAINER (widget),
			       gtk_widget_set_parent_sensitive,
			       client_data);
    }
  else
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_PARENT_SENSITIVE);
      
      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  window = gtk_widget_get_ancestor (widget, gtk_window_get_type ());
	  if (window)
	    gtk_window_set_focus (GTK_WINDOW (window), NULL);
	}
      
      if (GTK_IS_CONTAINER (widget))
	gtk_container_foreach (GTK_CONTAINER (widget),
			       gtk_widget_set_parent_sensitive,
			       client_data);
    }
}

/*****************************************
 * gtk_widget_propagate_restore:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_propagate_restore (GtkWidget *widget,
			      gpointer	 client_data)
{
  gtk_widget_restore_state (widget);
}

/*****************************************
 * gtk_widget_propagate_state:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_propagate_state (GtkWidget *widget,
			    gpointer   client_data)
{
  GtkStateType *state;
  
  state = (GtkStateType*) client_data;
  gtk_widget_set_state (widget, *state);
}

/*****************************************
 * gtk_widget_draw_children_recurse:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_draw_children_recurse (GtkWidget *widget,
				  gpointer   client_data)
{
  gtk_widget_draw (widget, NULL);
  gtk_widget_draw_children (widget);
}

/*****************************************
 * gtk_widget_set_style_internal:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_set_style_internal (GtkWidget *widget,
			       GtkStyle	 *style)
{
  GtkRequisition old_requisition;
  
  g_return_if_fail (widget != NULL);
  
  if (widget->style != style)
    {
      if (GTK_WIDGET_REALIZED (widget))
	gtk_style_detach (widget->style);
      
      gtk_style_unref (widget->style);
      widget->style = style;
      gtk_style_ref (widget->style);
      
      if (GTK_WIDGET_REALIZED (widget))
	widget->style = gtk_style_attach (widget->style, widget->window);
      
      if (widget->parent)
	{
	  old_requisition = widget->requisition;
	  gtk_widget_size_request (widget, &widget->requisition);
	  
	  if ((old_requisition.width != widget->requisition.width) ||
	      (old_requisition.height != widget->requisition.height))
	    gtk_widget_queue_resize (widget);
	  else if (GTK_WIDGET_DRAWABLE (widget))
	    gtk_widget_queue_draw (widget);
	}
    }
}

/*****************************************
 * gtk_widget_set_style_recurse:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_set_style_recurse (GtkWidget *widget,
			      gpointer	 client_data)
{
  GtkStyle *style;
  
  style = gtk_rc_get_style (widget);
  if (style != widget->style)
    gtk_widget_set_style_internal (widget, style);
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
			   gtk_widget_set_style_recurse,
			   NULL);
}

/*****************************************
 * gtk_widget_aux_info_new:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static GtkWidgetAuxInfo*
gtk_widget_aux_info_new ()
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
  g_return_if_fail (shape_mask != NULL);
  /*  set_shape doesn't work on widgets without gdk window */
  g_return_if_fail (!GTK_WIDGET_NO_WINDOW (widget));
  
  /*
   * remember shape mask for later gtk_widget_realize's
   */
  shape_info = gtk_object_get_data (GTK_OBJECT (widget), shape_info_key);
  if (!shape_info)
    {
      shape_info = g_new (GtkWidgetShapeInfo, 1);
      gtk_object_set_data (GTK_OBJECT (widget), shape_info_key, shape_info);
    }
  shape_info->shape_mask = shape_mask;
  shape_info->offset_x = offset_x;
  shape_info->offset_y = offset_y;
  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_SHAPE_MASK);
  
  /* 
   * set shape if widget has a gdk window allready.
   * otherwise the shape is scheduled to be set by gtk_widget_realize.
   */
  if (widget->window)
    gdk_window_shape_combine_mask (widget->window, shape_mask,
				   offset_x, offset_y);	
  
}

/*****************************************
 * gtk_widget_dnd_drag_add:
 *   when you get a DRAG_ENTER event, you can use this		 
 *   to tell Gtk ofother widgets that are to be dragged as well
 *
 *   arguments:
 *
 *   results:
 *****************************************/
void
gtk_widget_dnd_drag_add (GtkWidget *widget)
{
}

/*****************************************
 * gtk_widget_dnd_drag_set:
 *   these two functions enable drag and/or drop on a
 *   widget and also let Gtk know what data types will be accepted
 *   use MIME type naming,plus tacking "URL:" on the front for link 
 *   dragging
 *	       
 *
 *   arguments:
 *
 *   results:
 *****************************************/
void
gtk_widget_dnd_drag_set (GtkWidget   *widget,
			 guint8	      drag_enable,
			 gchar	    **type_accept_list,
			 guint	      numtypes)
{
  g_return_if_fail(widget != NULL);
  
  if (!widget->window)
    gtk_widget_realize (widget);
  
  g_return_if_fail (widget->window != NULL);
  gdk_window_dnd_drag_set (widget->window,
			   drag_enable,
			   type_accept_list,
			   numtypes);
}

/*****************************************
 * gtk_widget_dnd_drop_set:
 *
 *   arguments:
 *
 *   results:
 *****************************************/
void
gtk_widget_dnd_drop_set (GtkWidget   *widget,
			 guint8	      drop_enable,
			 gchar	    **type_accept_list,
			 guint	      numtypes,
			 guint8	      is_destructive_operation)
{
  g_return_if_fail(widget != NULL);
  
  if (!widget->window)
    gtk_widget_realize (widget);
  
  g_return_if_fail (widget->window != NULL);
  gdk_window_dnd_drop_set (widget->window,
			   drop_enable,
			   type_accept_list,
			   numtypes,
			   is_destructive_operation);
}

/*****************************************
 * gtk_widget_dnd_data_set:
 *
 *   arguments:
 *
 *   results:
 *****************************************/
void
gtk_widget_dnd_data_set (GtkWidget   *widget,
			 GdkEvent    *event,
			 gpointer     data,
			 gulong	      data_numbytes)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (widget->window != NULL);
  
  gdk_window_dnd_data_set (widget->window, event, data, data_numbytes);
}
