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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

#include <string.h>
#include <limits.h>
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"
#include "gdk/gdkx.h"
#include "gtkprivate.h"
#include "gtkrc.h"
#include "gtksignal.h"
#include "gtkwindow.h"
#include "gtkbindings.h"
#include "gtkmain.h"

/* TODO: remove this define and assorted code in 1.3 and fix up the
 * real culprits.
 */
#define FIXME_ZVT_ME_HARDER

enum {
  SET_FOCUS,
  LAST_SIGNAL
};
enum {
  ARG_0,
  ARG_TYPE,
  ARG_TITLE,
  ARG_AUTO_SHRINK,
  ARG_ALLOW_SHRINK,
  ARG_ALLOW_GROW,
  ARG_MODAL,
  ARG_WIN_POS,
  ARG_DEFAULT_WIDTH,
  ARG_DEFAULT_HEIGHT
};

typedef struct {
  GdkGeometry    geometry; /* Last set of geometry hints we set */
  GdkWindowHints flags;
  gint           width;
  gint           height;
} GtkWindowLastGeometryInfo;

typedef struct {
  /* Properties that the app has set on the window
   */
  GdkGeometry    geometry;	/* Geometry hints */
  GdkWindowHints mask;
  GtkWidget     *widget;	/* subwidget to which hints apply */
  gint           width;		/* Default size */
  gint           height;

  GtkWindowLastGeometryInfo last;
} GtkWindowGeometryInfo;

#define GTK_WINDOW_HAS_FOCUS(window)                           \
  ((window)->window_has_focus || (window)->window_has_pointer_focus)

static void gtk_window_class_init         (GtkWindowClass    *klass);
static void gtk_window_init               (GtkWindow         *window);
static void gtk_window_set_arg            (GtkObject         *object,
					   GtkArg            *arg,
					   guint	      arg_id);
static void gtk_window_get_arg            (GtkObject         *object,
					   GtkArg            *arg,
					   guint	      arg_id);
static void gtk_window_shutdown           (GtkObject         *object);
static void gtk_window_destroy            (GtkObject         *object);
static void gtk_window_finalize           (GtkObject         *object);
static void gtk_window_show               (GtkWidget         *widget);
static void gtk_window_hide               (GtkWidget         *widget);
static void gtk_window_map                (GtkWidget         *widget);
static void gtk_window_unmap              (GtkWidget         *widget);
static void gtk_window_realize            (GtkWidget         *widget);
static void gtk_window_size_request       (GtkWidget         *widget,
					   GtkRequisition    *requisition);
static void gtk_window_size_allocate      (GtkWidget         *widget,
					   GtkAllocation     *allocation);
static gint gtk_window_configure_event    (GtkWidget         *widget,
					   GdkEventConfigure *event);
static gint gtk_window_key_press_event    (GtkWidget         *widget,
					   GdkEventKey       *event);
static gint gtk_window_key_release_event  (GtkWidget         *widget,
					   GdkEventKey       *event);
static gint gtk_window_enter_notify_event (GtkWidget         *widget,
					   GdkEventCrossing  *event);
static gint gtk_window_leave_notify_event (GtkWidget         *widget,
					   GdkEventCrossing  *event);
static gint gtk_window_focus_in_event     (GtkWidget         *widget,
					   GdkEventFocus     *event);
static gint gtk_window_focus_out_event    (GtkWidget         *widget,
					   GdkEventFocus     *event);
static gint gtk_window_client_event	  (GtkWidget	     *widget,
					   GdkEventClient    *event);
static void gtk_window_check_resize       (GtkContainer      *container);
static void gtk_window_real_set_focus     (GtkWindow         *window,
					   GtkWidget         *focus);

static void gtk_window_move_resize        (GtkWindow         *window);
static gboolean gtk_window_compare_hints  (GdkGeometry       *geometry_a,
					   guint              flags_a,
					   GdkGeometry       *geometry_b,
					   guint              flags_b);
static void gtk_window_compute_default_size (GtkWindow       *window,
					     guint           *width,
					     guint           *height);
static void  gtk_window_constrain_size      (GtkWindow       *window,
					     GdkGeometry     *geometry,
					     guint            flags,
					     gint             width,
					     gint             height,
					     gint            *new_width,
					     gint            *new_height);
static void gtk_window_compute_hints      (GtkWindow         *window, 
					   GdkGeometry       *new_geometry,
					   guint             *new_flags);
static void gtk_window_compute_reposition (GtkWindow         *window,
					   gint               new_width,
					   gint               new_height,
					   gint              *x,
					   gint              *y);

static void gtk_window_read_rcfiles       (GtkWidget         *widget,
					   GdkEventClient    *event);
static void gtk_window_draw               (GtkWidget         *widget,
				           GdkRectangle      *area);
static void gtk_window_paint              (GtkWidget         *widget,
					   GdkRectangle      *area);
static gint gtk_window_expose             (GtkWidget         *widget,
				           GdkEventExpose    *event);
static void gtk_window_unset_transient_for         (GtkWindow  *window);
static void gtk_window_transient_parent_realized   (GtkWidget  *parent,
						    GtkWidget  *window);
static void gtk_window_transient_parent_unrealized (GtkWidget  *parent,
						    GtkWidget  *window);

static GtkWindowGeometryInfo* gtk_window_get_geometry_info (GtkWindow *window,
							    gboolean   create);
static void gtk_window_geometry_destroy  (GtkWindowGeometryInfo *info);

static GtkBinClass *parent_class = NULL;
static guint window_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_window_get_type (void)
{
  static GtkType window_type = 0;

  if (!window_type)
    {
      static const GtkTypeInfo window_info =
      {
	"GtkWindow",
	sizeof (GtkWindow),
	sizeof (GtkWindowClass),
	(GtkClassInitFunc) gtk_window_class_init,
	(GtkObjectInitFunc) gtk_window_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      window_type = gtk_type_unique (gtk_bin_get_type (), &window_info);
    }

  return window_type;
}

static void
gtk_window_class_init (GtkWindowClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;

  parent_class = gtk_type_class (gtk_bin_get_type ());

  gtk_object_add_arg_type ("GtkWindow::type", GTK_TYPE_WINDOW_TYPE, GTK_ARG_READWRITE, ARG_TYPE);
  gtk_object_add_arg_type ("GtkWindow::title", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_TITLE);
  gtk_object_add_arg_type ("GtkWindow::auto_shrink", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_AUTO_SHRINK);
  gtk_object_add_arg_type ("GtkWindow::allow_shrink", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_ALLOW_SHRINK);
  gtk_object_add_arg_type ("GtkWindow::allow_grow", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_ALLOW_GROW);
  gtk_object_add_arg_type ("GtkWindow::modal", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_MODAL);
  gtk_object_add_arg_type ("GtkWindow::window_position", GTK_TYPE_WINDOW_POSITION, GTK_ARG_READWRITE, ARG_WIN_POS);
  gtk_object_add_arg_type ("GtkWindow::default_width", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_DEFAULT_WIDTH);
  gtk_object_add_arg_type ("GtkWindow::default_height", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_DEFAULT_HEIGHT);
  
  window_signals[SET_FOCUS] =
    gtk_signal_new ("set_focus",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkWindowClass, set_focus),
                    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_WIDGET);

  gtk_object_class_add_signals (object_class, window_signals, LAST_SIGNAL);

  object_class->set_arg = gtk_window_set_arg;
  object_class->get_arg = gtk_window_get_arg;
  object_class->shutdown = gtk_window_shutdown;
  object_class->destroy = gtk_window_destroy;
  object_class->finalize = gtk_window_finalize;

  widget_class->show = gtk_window_show;
  widget_class->hide = gtk_window_hide;
  widget_class->map = gtk_window_map;
  widget_class->unmap = gtk_window_unmap;
  widget_class->realize = gtk_window_realize;
  widget_class->size_request = gtk_window_size_request;
  widget_class->size_allocate = gtk_window_size_allocate;
  widget_class->configure_event = gtk_window_configure_event;
  widget_class->key_press_event = gtk_window_key_press_event;
  widget_class->key_release_event = gtk_window_key_release_event;
  widget_class->enter_notify_event = gtk_window_enter_notify_event;
  widget_class->leave_notify_event = gtk_window_leave_notify_event;
  widget_class->focus_in_event = gtk_window_focus_in_event;
  widget_class->focus_out_event = gtk_window_focus_out_event;
  widget_class->client_event = gtk_window_client_event;

  widget_class->draw = gtk_window_draw;
  widget_class->expose_event = gtk_window_expose;
   
  container_class->check_resize = gtk_window_check_resize;

  klass->set_focus = gtk_window_real_set_focus;
}

static void
gtk_window_init (GtkWindow *window)
{
  GTK_WIDGET_UNSET_FLAGS (window, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (window, GTK_TOPLEVEL);

  gtk_container_set_resize_mode (GTK_CONTAINER (window), GTK_RESIZE_QUEUE);

  window->title = NULL;
  window->wmclass_name = g_strdup (g_get_prgname ());
  window->wmclass_class = g_strdup (gdk_progclass);
  window->type = GTK_WINDOW_TOPLEVEL;
  window->focus_widget = NULL;
  window->default_widget = NULL;
  window->resize_count = 0;
  window->allow_shrink = FALSE;
  window->allow_grow = TRUE;
  window->auto_shrink = FALSE;
  window->handling_resize = FALSE;
  window->position = GTK_WIN_POS_NONE;
  window->use_uposition = TRUE;
  window->modal = FALSE;
  
  gtk_container_register_toplevel (GTK_CONTAINER (window));
}

static void
gtk_window_set_arg (GtkObject  *object,
		    GtkArg     *arg,
		    guint	arg_id)
{
  GtkWindow  *window;

  window = GTK_WINDOW (object);

  switch (arg_id)
    {
    case ARG_TYPE:
      window->type = GTK_VALUE_ENUM (*arg);
      break;
    case ARG_TITLE:
      gtk_window_set_title (window, GTK_VALUE_STRING (*arg));
      break;
    case ARG_AUTO_SHRINK:
      window->auto_shrink = (GTK_VALUE_BOOL (*arg) != FALSE);
      gtk_widget_queue_resize (GTK_WIDGET (window));
      break;
    case ARG_ALLOW_SHRINK:
      window->allow_shrink = (GTK_VALUE_BOOL (*arg) != FALSE);
      gtk_widget_queue_resize (GTK_WIDGET (window));
      break;
    case ARG_ALLOW_GROW:
      window->allow_grow = (GTK_VALUE_BOOL (*arg) != FALSE);
      gtk_widget_queue_resize (GTK_WIDGET (window));
      break;
    case ARG_MODAL:
      gtk_window_set_modal (window, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_WIN_POS:
      gtk_window_set_position (window, GTK_VALUE_ENUM (*arg));
      break;
    case ARG_DEFAULT_WIDTH:
      gtk_window_set_default_size (window, GTK_VALUE_INT (*arg), -2);
      break;
    case ARG_DEFAULT_HEIGHT:
      gtk_window_set_default_size (window, -2, GTK_VALUE_INT (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_window_get_arg (GtkObject  *object,
		    GtkArg     *arg,
		    guint	arg_id)
{
  GtkWindow  *window;

  window = GTK_WINDOW (object);

  switch (arg_id)
    {
      GtkWindowGeometryInfo *info;
    case ARG_TYPE:
      GTK_VALUE_ENUM (*arg) = window->type;
      break;
    case ARG_TITLE:
      GTK_VALUE_STRING (*arg) = g_strdup (window->title);
      break;
    case ARG_AUTO_SHRINK:
      GTK_VALUE_BOOL (*arg) = window->auto_shrink;
      break;
    case ARG_ALLOW_SHRINK:
      GTK_VALUE_BOOL (*arg) = window->allow_shrink;
      break;
    case ARG_ALLOW_GROW:
      GTK_VALUE_BOOL (*arg) = window->allow_grow;
      break;
    case ARG_MODAL:
      GTK_VALUE_BOOL (*arg) = window->modal;
      break;
    case ARG_WIN_POS:
      GTK_VALUE_ENUM (*arg) = window->position;
      break;
    case ARG_DEFAULT_WIDTH:
      info = gtk_window_get_geometry_info (window, FALSE);
      if (!info)
	GTK_VALUE_INT (*arg) = -1;
      else
	GTK_VALUE_INT (*arg) = info->width;
      break;
    case ARG_DEFAULT_HEIGHT:
      info = gtk_window_get_geometry_info (window, FALSE);
      if (!info)
	GTK_VALUE_INT (*arg) = -1;
      else
	GTK_VALUE_INT (*arg) = info->height;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

GtkWidget*
gtk_window_new (GtkWindowType type)
{
  GtkWindow *window;

  g_return_val_if_fail (type >= GTK_WINDOW_TOPLEVEL && type <= GTK_WINDOW_POPUP, NULL);

  window = gtk_type_new (GTK_TYPE_WINDOW);

  window->type = type;

  return GTK_WIDGET (window);
}

void
gtk_window_set_title (GtkWindow   *window,
		      const gchar *title)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (window->title)
    g_free (window->title);
  window->title = g_strdup (title);

  if (GTK_WIDGET_REALIZED (window))
    gdk_window_set_title (GTK_WIDGET (window)->window, window->title);
}

void
gtk_window_set_wmclass (GtkWindow *window,
			const gchar *wmclass_name,
			const gchar *wmclass_class)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  g_free (window->wmclass_name);
  window->wmclass_name = g_strdup (wmclass_name);

  g_free (window->wmclass_class);
  window->wmclass_class = g_strdup (wmclass_class);

  if (GTK_WIDGET_REALIZED (window))
    g_warning ("shouldn't set wmclass after window is realized!\n");
}

void
gtk_window_set_focus (GtkWindow *window,
		      GtkWidget *focus)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));
  if (focus)
    {
      g_return_if_fail (GTK_IS_WIDGET (focus));
      g_return_if_fail (GTK_WIDGET_CAN_FOCUS (focus));
    }

  if ((window->focus_widget != focus) ||
      (focus && !GTK_WIDGET_HAS_FOCUS (focus)))
    gtk_signal_emit (GTK_OBJECT (window), window_signals[SET_FOCUS], focus);
}

void
gtk_window_set_default (GtkWindow *window,
			GtkWidget *default_widget)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (default_widget)
    g_return_if_fail (GTK_WIDGET_CAN_DEFAULT (default_widget));

  if (window->default_widget != default_widget)
    {
      if (window->default_widget)
	{
	  if (window->focus_widget != window->default_widget ||
	      !GTK_WIDGET_RECEIVES_DEFAULT (window->default_widget))
	    GTK_WIDGET_UNSET_FLAGS (window->default_widget, GTK_HAS_DEFAULT);
	  gtk_widget_draw_default (window->default_widget);
	}

      window->default_widget = default_widget;

      if (window->default_widget)
	{
	  if (window->focus_widget == NULL ||
	      !GTK_WIDGET_RECEIVES_DEFAULT (window->focus_widget))
	    GTK_WIDGET_SET_FLAGS (window->default_widget, GTK_HAS_DEFAULT);
	  gtk_widget_draw_default (window->default_widget);
	}
    }
}

void
gtk_window_set_policy (GtkWindow *window,
		       gint       allow_shrink,
		       gint       allow_grow,
		       gint       auto_shrink)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  window->allow_shrink = (allow_shrink != FALSE);
  window->allow_grow = (allow_grow != FALSE);
  window->auto_shrink = (auto_shrink != FALSE);

  gtk_widget_queue_resize (GTK_WIDGET (window));
}

void
gtk_window_add_accel_group (GtkWindow        *window,
			    GtkAccelGroup    *accel_group)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (accel_group != NULL);

  gtk_accel_group_attach (accel_group, GTK_OBJECT (window));
}

void
gtk_window_remove_accel_group (GtkWindow       *window,
			       GtkAccelGroup   *accel_group)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (accel_group != NULL);

  gtk_accel_group_detach (accel_group, GTK_OBJECT (window));
}

void
gtk_window_set_position (GtkWindow         *window,
			 GtkWindowPosition  position)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  window->position = position;
}

gint
gtk_window_activate_focus (GtkWindow      *window)
{
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  if (window->focus_widget)
    {
      if (GTK_WIDGET_IS_SENSITIVE (window->focus_widget))
	gtk_widget_activate (window->focus_widget);
      return TRUE;
    }

  return FALSE;
}

gint
gtk_window_activate_default (GtkWindow      *window)
{
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  if (window->default_widget && GTK_WIDGET_IS_SENSITIVE (window->default_widget))
    {
      gtk_widget_activate (window->default_widget);
      return TRUE;
    }

  return FALSE;
}

void
gtk_window_set_modal (GtkWindow *window,
		      gboolean   modal)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  window->modal = modal != FALSE;

  /* adjust desired modality state */
  if (GTK_WIDGET_VISIBLE (window) && window->modal)
    gtk_grab_add (GTK_WIDGET (window));
  else
    gtk_grab_remove (GTK_WIDGET (window));
}

void
gtk_window_add_embedded_xid (GtkWindow *window, guint xid)
{
  GList *embedded_windows;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  embedded_windows = gtk_object_get_data (GTK_OBJECT (window), "gtk-embedded");
  if (embedded_windows)
    gtk_object_remove_no_notify_by_id (GTK_OBJECT (window), 
				       g_quark_from_static_string ("gtk-embedded"));
  embedded_windows = g_list_prepend (embedded_windows,
				     GUINT_TO_POINTER (xid));

  gtk_object_set_data_full (GTK_OBJECT (window), "gtk-embedded", 
			    embedded_windows,
			    embedded_windows ?
			      (GtkDestroyNotify) g_list_free : NULL);
}

void
gtk_window_remove_embedded_xid (GtkWindow *window, guint xid)
{
  GList *embedded_windows;
  GList *node;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));
  
  embedded_windows = gtk_object_get_data (GTK_OBJECT (window), "gtk-embedded");
  if (embedded_windows)
    gtk_object_remove_no_notify_by_id (GTK_OBJECT (window), 
				       g_quark_from_static_string ("gtk-embedded"));

  node = g_list_find (embedded_windows, GUINT_TO_POINTER (xid));
  if (node)
    {
      embedded_windows = g_list_remove_link (embedded_windows, node);
      g_list_free_1 (node);
    }
  
  gtk_object_set_data_full (GTK_OBJECT (window), 
			    "gtk-embedded", embedded_windows,
			    embedded_windows ?
			      (GtkDestroyNotify) g_list_free : NULL);
}

void       
gtk_window_reposition (GtkWindow *window,
		       gint       x,
		       gint       y)
{
  GtkWindowGeometryInfo *info;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  /* keep this in sync with gtk_window_compute_reposition()
   */
  if (GTK_WIDGET_REALIZED (window))
    {
      info = gtk_window_get_geometry_info (window, TRUE);

      if (!(info->last.flags & GDK_HINT_POS))
	{
	  info->last.flags |= GDK_HINT_POS;
	  gdk_window_set_geometry_hints (GTK_WIDGET (window)->window,
					 &info->last.geometry,
					 info->last.flags);
	}
  
      gdk_window_move (GTK_WIDGET (window)->window, x, y);
    }
}

static void
gtk_window_shutdown (GtkObject *object)
{
  GtkWindow *window;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_WINDOW (object));

  window = GTK_WINDOW (object);

  gtk_window_set_focus (window, NULL);
  gtk_window_set_default (window, NULL);

  GTK_OBJECT_CLASS (parent_class)->shutdown (object);
}

static void
gtk_window_transient_parent_realized (GtkWidget *parent,
				      GtkWidget *window)
{
  if (GTK_WIDGET_REALIZED (window))
    gdk_window_set_transient_for (window->window, parent->window);
}

static void
gtk_window_transient_parent_unrealized (GtkWidget *parent,
					GtkWidget *window)
{
  if (GTK_WIDGET_REALIZED (window))
    gdk_property_delete (window->window, 
			 gdk_atom_intern ("WM_TRANSIENT_FOR", FALSE));
}

static void       
gtk_window_unset_transient_for  (GtkWindow *window)
{
  if (window->transient_parent)
    {
      gtk_signal_disconnect_by_func (GTK_OBJECT (window->transient_parent),
				     GTK_SIGNAL_FUNC (gtk_window_transient_parent_realized),
				     window);
      gtk_signal_disconnect_by_func (GTK_OBJECT (window->transient_parent),
				     GTK_SIGNAL_FUNC (gtk_window_transient_parent_unrealized),
				     window);
      gtk_signal_disconnect_by_func (GTK_OBJECT (window->transient_parent),
				     GTK_SIGNAL_FUNC (gtk_widget_destroyed),
				     &window->transient_parent);

      window->transient_parent = NULL;
    }
}

void       
gtk_window_set_transient_for  (GtkWindow *window, 
			       GtkWindow *parent)
{
  g_return_if_fail (window != 0);

  if (window->transient_parent)
    {
      if (GTK_WIDGET_REALIZED (window) && 
	  GTK_WIDGET_REALIZED (window->transient_parent) && 
	  (!parent || !GTK_WIDGET_REALIZED (parent)))
	gtk_window_transient_parent_unrealized (GTK_WIDGET (window->transient_parent),
						GTK_WIDGET (window));

      gtk_window_unset_transient_for (window);
    }

  window->transient_parent = parent;

  if (parent)
    {
      gtk_signal_connect (GTK_OBJECT (parent), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window->transient_parent);
      gtk_signal_connect (GTK_OBJECT (parent), "realize",
			  GTK_SIGNAL_FUNC (gtk_window_transient_parent_realized),
			  window);
      gtk_signal_connect (GTK_OBJECT (parent), "unrealize",
			  GTK_SIGNAL_FUNC (gtk_window_transient_parent_unrealized),
			  window);

      if (GTK_WIDGET_REALIZED (window) &&
	  GTK_WIDGET_REALIZED (parent))
	gtk_window_transient_parent_realized (GTK_WIDGET (parent),
					      GTK_WIDGET (window));
    }
}

static void
gtk_window_geometry_destroy (GtkWindowGeometryInfo *info)
{
  if (info->widget)
    gtk_signal_disconnect_by_func (GTK_OBJECT (info->widget),
				   GTK_SIGNAL_FUNC (gtk_widget_destroyed),
				   &info->widget);
  g_free (info);
}

static GtkWindowGeometryInfo*
gtk_window_get_geometry_info (GtkWindow *window,
			      gboolean   create)
{
  GtkWindowGeometryInfo *info;

  info = gtk_object_get_data (GTK_OBJECT (window), "gtk-window-geometry");

  if (!info && create)
    {
      info = g_new0 (GtkWindowGeometryInfo, 1);

      info->width = 0;
      info->height = 0;
      info->last.width = -1;
      info->last.height = -1;
      info->widget = NULL;
      info->mask = 0;

      gtk_object_set_data_full (GTK_OBJECT (window), 
				"gtk-window-geometry",
				info, 
				(GtkDestroyNotify) gtk_window_geometry_destroy);
    }

  return info;
}

void       
gtk_window_set_geometry_hints (GtkWindow       *window,
			       GtkWidget       *geometry_widget,
			       GdkGeometry     *geometry,
			       GdkWindowHints   geom_mask)
{
  GtkWindowGeometryInfo *info;

  g_return_if_fail (window != NULL);

  info = gtk_window_get_geometry_info (window, TRUE);
  
  if (info->widget)
    gtk_signal_disconnect_by_func (GTK_OBJECT (info->widget),
				   GTK_SIGNAL_FUNC (gtk_widget_destroyed),
				   &info->widget);
  
  info->widget = geometry_widget;
  if (info->widget)
    gtk_signal_connect (GTK_OBJECT (geometry_widget), "destroy",
			GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			&info->widget);

  if (geometry)
    info->geometry = *geometry;

  info->mask = geom_mask;

  gtk_widget_queue_resize (GTK_WIDGET (window));
}

void       
gtk_window_set_default_size (GtkWindow   *window,
			     gint         width,
			     gint         height)
{
  GtkWindowGeometryInfo *info;

  g_return_if_fail (GTK_IS_WINDOW (window));

  info = gtk_window_get_geometry_info (window, TRUE);

  if (width >= 0)
    info->width = width;
  if (height >= 0)
    info->height = height;

  gtk_widget_queue_resize (GTK_WIDGET (window));
}
  
static void
gtk_window_destroy (GtkObject *object)
{
  GtkWindow *window;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_WINDOW (object));

  window = GTK_WINDOW (object);
  
  gtk_container_unregister_toplevel (GTK_CONTAINER (object));

  if (window->transient_parent)
    gtk_window_unset_transient_for (window);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_window_finalize (GtkObject *object)
{
  GtkWindow *window;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_WINDOW (object));

  window = GTK_WINDOW (object);
  g_free (window->title);
  g_free (window->wmclass_name);
  g_free (window->wmclass_class);

  GTK_OBJECT_CLASS(parent_class)->finalize (object);
}

static void
gtk_window_show (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkContainer *container = GTK_CONTAINER (window);
  gboolean need_resize;

  GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);

  need_resize = container->need_resize || !GTK_WIDGET_REALIZED (widget);
  container->need_resize = FALSE;

  if (need_resize)
    {
      GtkWindowGeometryInfo *info = gtk_window_get_geometry_info (window, TRUE);
      GtkAllocation allocation = { 0, 0 };
      GdkGeometry new_geometry;
      guint width, height, new_flags;

      /* determine default size to initially show the window with */
      gtk_widget_size_request (widget, NULL);
      gtk_window_compute_default_size (window, &width, &height);

      /* save away the last default size for later comparisions */
      info->last.width = width;
      info->last.height = height;

      /* constrain size to geometry */
      gtk_window_compute_hints (window, &new_geometry, &new_flags);
      gtk_window_constrain_size (window,
				 &new_geometry, new_flags,
				 width, height,
				 &width, &height);

      /* and allocate the window */
      allocation.width  = width;
      allocation.height = height;
      gtk_widget_size_allocate (widget, &allocation);
      
      if (GTK_WIDGET_REALIZED (widget))
	gdk_window_resize (widget->window, width, height);
      else
	gtk_widget_realize (widget);
    }
  
  gtk_container_check_resize (container);

  gtk_widget_map (widget);

  if (window->modal)
    gtk_grab_add (widget);
}

static void
gtk_window_hide (GtkWidget *widget)
{
  GtkWindow *window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));

  window = GTK_WINDOW (widget);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_VISIBLE);
  gtk_widget_unmap (widget);

  if (window->modal)
    gtk_grab_remove (widget);
}

/* The algorithm used for window_has_pointer_focus is taken
 * from the XTerm code.
 */
static GdkFilterReturn
gtk_window_focus_filter (GdkXEvent *xevent,
			 GdkEvent  *event,
			 gpointer   data)
{
  GtkWindow *window = GTK_WINDOW (data);
  XEvent *xev = (XEvent *)xevent;
  GdkEvent extra_event;

  switch (xev->xany.type)
    {
    case FocusIn:
      {
	switch (xev->xfocus.detail)
	  {
	  case NotifyAncestor:
	  case NotifyNonlinear:
	  case NotifyVirtual:
	  case NotifyNonlinearVirtual:
	    window->window_has_focus = TRUE;
	    break;
	  case NotifyPointer:
	    window->window_has_pointer_focus = TRUE;
	    break;
	  case NotifyInferior:
	  case NotifyPointerRoot:
	  case NotifyDetailNone:
	    break;
	  }
      }
      break;
    case FocusOut:
      {
	switch (xev->xfocus.detail)
	  {
	  case NotifyAncestor:
	  case NotifyNonlinear:
	  case NotifyVirtual:
	  case NotifyNonlinearVirtual:
	    window->window_has_focus = FALSE;
	    break;
	  case NotifyPointer:
	    window->window_has_pointer_focus = FALSE;
	    break;
	  case NotifyInferior:
	  case NotifyPointerRoot:
	  case NotifyDetailNone:
	    break;
	  }
      }
      break;
    case EnterNotify:
    case LeaveNotify:
      if (xev->xcrossing.detail != NotifyInferior &&
	  xev->xcrossing.focus && !window->window_has_focus)
	{
	  window->window_has_pointer_focus = (xev->xany.type == EnterNotify) ? TRUE : FALSE;

	  extra_event.type = GDK_FOCUS_CHANGE;
	  extra_event.focus_change.window = GTK_WIDGET (window)->window;
	  extra_event.focus_change.send_event = FALSE;
	  extra_event.focus_change.in = window->window_has_pointer_focus;

	  gdk_event_put (&extra_event);
	}
      break;
    }

  return GDK_FILTER_CONTINUE;
}

static void
gtk_window_map (GtkWidget *widget)
{
  GtkWindow *window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  window = GTK_WINDOW (widget);

  if (window->bin.child &&
      GTK_WIDGET_VISIBLE (window->bin.child) &&
      !GTK_WIDGET_MAPPED (window->bin.child))
    gtk_widget_map (window->bin.child);

  gdk_window_add_filter (widget->window, gtk_window_focus_filter, widget);

  gdk_window_show (widget->window);
}

static void
gtk_window_unmap (GtkWidget *widget)
{
  GtkWindow *window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  gdk_window_withdraw (widget->window);

  window = GTK_WINDOW (widget);
  window->use_uposition = TRUE;
  window->resize_count = 0;
  window->handling_resize = FALSE;
}

static void
gtk_window_realize (GtkWidget *widget)
{
  GtkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_return_if_fail (GTK_IS_WINDOW (widget));

  window = GTK_WINDOW (widget);

  /* ensure widget tree is properly size allocated */
  if (widget->allocation.x == -1 &&
      widget->allocation.y == -1 &&
      widget->allocation.width == 1 &&
      widget->allocation.height == 1)
    {
      GtkRequisition requisition;
      GtkAllocation allocation = { 0, 0, 200, 200 };

      gtk_widget_size_request (widget, &requisition);
      if (requisition.width || requisition.height)
	{
	  /* non-empty window */
	  allocation.width = requisition.width;
	  allocation.height = requisition.height;
	}
      gtk_widget_size_allocate (widget, &allocation);
      
      gtk_container_queue_resize (GTK_CONTAINER (widget));

      g_return_if_fail (!GTK_WIDGET_REALIZED (widget));
    }
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  
  switch (window->type)
    {
    case GTK_WINDOW_TOPLEVEL:
      attributes.window_type = GDK_WINDOW_TOPLEVEL;
      break;
    case GTK_WINDOW_DIALOG:
      attributes.window_type = GDK_WINDOW_DIALOG;
      break;
    case GTK_WINDOW_POPUP:
      attributes.window_type = GDK_WINDOW_TEMP;
      break;
    }
   
  attributes.title = window->title;
  attributes.wmclass_name = window->wmclass_name;
  attributes.wmclass_class = window->wmclass_class;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_KEY_PRESS_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK |
			    GDK_FOCUS_CHANGE_MASK |
			    GDK_STRUCTURE_MASK);
   
  attributes_mask = GDK_WA_VISUAL | GDK_WA_COLORMAP;
  attributes_mask |= (window->title ? GDK_WA_TITLE : 0);
  attributes_mask |= (window->wmclass_name ? GDK_WA_WMCLASS : 0);
   
  widget->window = gdk_window_new (NULL, &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, window);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  gtk_window_paint (widget, NULL);

  if (window->transient_parent &&
      GTK_WIDGET_REALIZED (window->transient_parent))
    gdk_window_set_transient_for (widget->window,
				  GTK_WIDGET (window->transient_parent)->window);
}

static void
gtk_window_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  GtkWindow *window;
  GtkBin *bin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));
  g_return_if_fail (requisition != NULL);

  window = GTK_WINDOW (widget);
  bin = GTK_BIN (window);
  
  requisition->width = GTK_CONTAINER (window)->border_width * 2;
  requisition->height = GTK_CONTAINER (window)->border_width * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkRequisition child_requisition;
      
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }
}

static void
gtk_window_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkWindow *window;
  GtkAllocation child_allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));
  g_return_if_fail (allocation != NULL);

  window = GTK_WINDOW (widget);
  widget->allocation = *allocation;

  if (window->bin.child && GTK_WIDGET_VISIBLE (window->bin.child))
    {
      child_allocation.x = GTK_CONTAINER (window)->border_width;
      child_allocation.y = GTK_CONTAINER (window)->border_width;
      child_allocation.width =
	MAX (1, (gint)allocation->width - child_allocation.x * 2);
      child_allocation.height =
	MAX (1, (gint)allocation->height - child_allocation.y * 2);

      gtk_widget_size_allocate (window->bin.child, &child_allocation);
    }
}

static gint
gtk_window_configure_event (GtkWidget         *widget,
			    GdkEventConfigure *event)
{
  GtkWindow *window;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  window = GTK_WINDOW (widget);

  /* we got a configure event specifying the new window size and position,
   * in principle we have to distinguish 4 cases here:
   * 1) the size didn't change and resize_count == 0
   *    -> the window was merely moved (sometimes not even that)
   * 2) the size didn't change and resize_count > 0
   *    -> we requested a new size, but didn't get it
   * 3) the size changed and resize_count > 0
   *    -> we asked for a new size and we got one
   * 4) the size changed and resize_count == 0
   *    -> we got resized from outside the toolkit, and have to
   *    accept that size since we don't want to fight neither the
   *    window manager nor the user
   * in the three latter cases we have to reallocate the widget tree,
   * which happens in gtk_window_move_resize(), so we set a flag for
   * that function and assign the new size. if resize_count > 1,
   * we simply do nothing and wait for more configure events.
   */

  if (window->resize_count > 0 ||
      widget->allocation.width != event->width ||
      widget->allocation.height != event->height)
    {
      if (window->resize_count > 0)
	window->resize_count -= 1;

      if (window->resize_count == 0)
	{
	  window->handling_resize = TRUE;
	  
	  widget->allocation.width = event->width;
	  widget->allocation.height = event->height;
	  
	  gtk_widget_queue_resize (widget);
	}
    }

  return TRUE;
}

static gint
gtk_window_key_press_event (GtkWidget   *widget,
			    GdkEventKey *event)
{
  GtkWindow *window;
  GtkDirectionType direction = 0;
  gboolean handled;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  window = GTK_WINDOW (widget);

  handled = FALSE;
  
  if (window->focus_widget &&
      window->focus_widget != widget &&
      GTK_WIDGET_IS_SENSITIVE (window->focus_widget))
    {
      handled = gtk_widget_event (window->focus_widget, (GdkEvent*) event);
    }
    
  if (!handled)
    handled = gtk_accel_groups_activate (GTK_OBJECT (window), event->keyval, event->state);

  if (!handled)
    {
      switch (event->keyval)
	{
	case GDK_space:
	  if (window->focus_widget)
	    {
	      if (GTK_WIDGET_IS_SENSITIVE (window->focus_widget))
		gtk_widget_activate (window->focus_widget);
	      handled = TRUE;
	    }
	  break;
	case GDK_Return:
	case GDK_KP_Enter:
	  if (window->default_widget && GTK_WIDGET_IS_SENSITIVE (window->default_widget) &&
	      (!window->focus_widget || !GTK_WIDGET_RECEIVES_DEFAULT (window->focus_widget)))
	    {
	      gtk_widget_activate (window->default_widget);
	      handled = TRUE;
	    }
          else if (window->focus_widget)
	    {
	      if (GTK_WIDGET_IS_SENSITIVE (window->focus_widget))
		gtk_widget_activate (window->focus_widget);
	      handled = TRUE;
	    }
	  break;
	case GDK_Up:
	case GDK_Down:
	case GDK_Left:
	case GDK_Right:
	case GDK_KP_Up:
	case GDK_KP_Down:
	case GDK_KP_Left:
	case GDK_KP_Right:
	case GDK_Tab:
	case GDK_ISO_Left_Tab:
	  switch (event->keyval)
	    {
	    case GDK_Up:
	    case GDK_KP_Up:
	      direction = GTK_DIR_UP;
	      break;
	    case GDK_Down:
	    case GDK_KP_Down:
	      direction = GTK_DIR_DOWN;
	      break;
	    case GDK_Left:
	    case GDK_KP_Left:
	      direction = GTK_DIR_LEFT;
	      break;
	    case GDK_Right:
	    case GDK_KP_Right:
	      direction = GTK_DIR_RIGHT;
	      break;
	    case GDK_Tab:
	    case GDK_ISO_Left_Tab:
	      if (event->state & GDK_SHIFT_MASK)
		direction = GTK_DIR_TAB_BACKWARD;
	      else
		direction = GTK_DIR_TAB_FORWARD;
              break;
            default :
              direction = GTK_DIR_UP; /* never reached, but makes compiler happy */
	    }

	  gtk_container_focus (GTK_CONTAINER (widget), direction);

	  if (!GTK_CONTAINER (window)->focus_child)
	    gtk_window_set_focus (GTK_WINDOW (widget), NULL);
	  else
	    handled = TRUE;
	  break;
	}
    }

  if (!handled && GTK_WIDGET_CLASS (parent_class)->key_press_event)
    handled = GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);

  return handled;
}

static gint
gtk_window_key_release_event (GtkWidget   *widget,
			      GdkEventKey *event)
{
  GtkWindow *window;
  gint handled;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  window = GTK_WINDOW (widget);
  handled = FALSE;
  if (window->focus_widget &&
      window->focus_widget != widget &&
      GTK_WIDGET_SENSITIVE (window->focus_widget))
    {
      handled = gtk_widget_event (window->focus_widget, (GdkEvent*) event);
    }

  if (!handled && GTK_WIDGET_CLASS (parent_class)->key_release_event)
    handled = GTK_WIDGET_CLASS (parent_class)->key_release_event (widget, event);

  return handled;
}

static gint
gtk_window_enter_notify_event (GtkWidget        *widget,
			       GdkEventCrossing *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  return FALSE;
}

static gint
gtk_window_leave_notify_event (GtkWidget        *widget,
			       GdkEventCrossing *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  return FALSE;
}

static gint
gtk_window_focus_in_event (GtkWidget     *widget,
			   GdkEventFocus *event)
{
  GtkWindow *window;
  GdkEventFocus fevent;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  /* It appears spurious focus in events can occur when
   *  the window is hidden. So we'll just check to see if
   *  the window is visible before actually handling the
   *  event
   */
  if (GTK_WIDGET_VISIBLE (widget))
    {
      window = GTK_WINDOW (widget);
      if (window->focus_widget &&
	  window->focus_widget != widget &&
	  !GTK_WIDGET_HAS_FOCUS (window->focus_widget))
	{
	  fevent.type = GDK_FOCUS_CHANGE;
	  fevent.window = window->focus_widget->window;
	  fevent.in = TRUE;

	  gtk_widget_event (window->focus_widget, (GdkEvent*) &fevent);
	}
    }

  return FALSE;
}

static gint
gtk_window_focus_out_event (GtkWidget     *widget,
			    GdkEventFocus *event)
{
  GtkWindow *window;
  GdkEventFocus fevent;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  window = GTK_WINDOW (widget);

  if (window->focus_widget &&
      window->focus_widget != widget &&
      GTK_WIDGET_HAS_FOCUS (window->focus_widget))
    {
      fevent.type = GDK_FOCUS_CHANGE;
      fevent.window = window->focus_widget->window;
      fevent.in = FALSE;

      gtk_widget_event (window->focus_widget, (GdkEvent*) &fevent);
    }

  return FALSE;
}

static GdkAtom atom_rcfiles = GDK_NONE;

static void
gtk_window_read_rcfiles (GtkWidget *widget,
			 GdkEventClient *event)
{
  GList *embedded_windows;

  embedded_windows = gtk_object_get_data (GTK_OBJECT (widget), "gtk-embedded");
  if (embedded_windows)
    {
      GdkEventClient sev;
      int i;
      
      for(i = 0; i < 5; i++)
	sev.data.l[i] = 0;
      sev.data_format = 32;
      sev.message_type = atom_rcfiles;
      
      while (embedded_windows)
	{
	  guint xid = GPOINTER_TO_UINT (embedded_windows->data);
	  gdk_event_send_client_message ((GdkEvent *) &sev, xid);
	  embedded_windows = embedded_windows->next;
	}
    }

  if (gtk_rc_reparse_all ())
    {
      /* If the above returned true, some of our RC files are out
       * of date, so we need to reset all our widgets. Our other
       * toplevel windows will also get the message, but by
       * then, the RC file will up to date, so we have to tell
       * them now.
       */
      GList *toplevels;
      
      toplevels = gtk_container_get_toplevels();
      while (toplevels)
	{
	  gtk_widget_reset_rc_styles (toplevels->data);
	  toplevels = toplevels->next;
	}
    }
}

static gint
gtk_window_client_event (GtkWidget	*widget,
			 GdkEventClient	*event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (!atom_rcfiles)
    atom_rcfiles = gdk_atom_intern("_GTK_READ_RCFILES", FALSE);

  if(event->message_type == atom_rcfiles) 
    gtk_window_read_rcfiles (widget, event);    

  return FALSE;
}

static void
gtk_window_check_resize (GtkContainer *container)
{
  GtkWindow *window;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_WINDOW (container));

  window = GTK_WINDOW (container);

  if (GTK_WIDGET_VISIBLE (container))
    gtk_window_move_resize (window);
}

static void
gtk_window_real_set_focus (GtkWindow *window,
			   GtkWidget *focus)
{
  GdkEventFocus event;
  gboolean def_flags = 0;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));
  
  if (window->default_widget)
    def_flags = GTK_WIDGET_HAS_DEFAULT (window->default_widget);
  
  if (window->focus_widget)
    {
      if (GTK_WIDGET_RECEIVES_DEFAULT (window->focus_widget) &&
	  (window->focus_widget != window->default_widget))
        {
	  GTK_WIDGET_UNSET_FLAGS (window->focus_widget, GTK_HAS_DEFAULT);

	  if (window->default_widget)
	    GTK_WIDGET_SET_FLAGS (window->default_widget, GTK_HAS_DEFAULT);
        }

      if (GTK_WINDOW_HAS_FOCUS (window))
	{
	  event.type = GDK_FOCUS_CHANGE;
	  event.window = window->focus_widget->window;
	  event.in = FALSE;
	  
	  gtk_widget_event (window->focus_widget, (GdkEvent*) &event);
	}
    }
  
  window->focus_widget = focus;
  
  if (window->focus_widget)
    {
      if (GTK_WIDGET_RECEIVES_DEFAULT (window->focus_widget) &&
	  (window->focus_widget != window->default_widget))
	{
	  if (GTK_WIDGET_CAN_DEFAULT (window->focus_widget))
	    GTK_WIDGET_SET_FLAGS (window->focus_widget, GTK_HAS_DEFAULT);

	  if (window->default_widget)
	    GTK_WIDGET_UNSET_FLAGS (window->default_widget, GTK_HAS_DEFAULT);
	}
      
      if (GTK_WINDOW_HAS_FOCUS (window))
	{
	  event.type = GDK_FOCUS_CHANGE;
	  event.window = window->focus_widget->window;
	  event.in = TRUE;

	  gtk_widget_event (window->focus_widget, (GdkEvent*) &event);
	}
    }
  
  if (window->default_widget &&
      (def_flags != GTK_WIDGET_FLAGS (window->default_widget)))
    gtk_widget_queue_draw (window->default_widget);
}

/*********************************
 * Functions related to resizing *
 *********************************/

static void
gtk_window_move_resize (GtkWindow *window)
{
  GtkWidget *widget;
  GtkContainer *container;
  GtkWindowGeometryInfo *info;
  GtkWindowLastGeometryInfo saved_last_info;
  GdkGeometry new_geometry;
  guint new_flags;
  gint x, y;
  gint width, height;
  gint new_width, new_height;
  gboolean default_size_changed = FALSE;
  gboolean hints_changed = FALSE;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_WIDGET_REALIZED (window));

  widget = GTK_WIDGET (window);
  container = GTK_CONTAINER (widget);
  info = gtk_window_get_geometry_info (window, TRUE);
  saved_last_info = info->last;

  gtk_widget_size_request (widget, NULL);
  gtk_window_compute_default_size (window, &new_width, &new_height);
  
  if (info->last.width < 0 ||
      info->last.width != new_width ||
      info->last.height != new_height)
    {
      default_size_changed = TRUE;
      info->last.width = new_width;
      info->last.height = new_height;

      /* We need to force a reposition in this case
       */
      if (window->position == GTK_WIN_POS_CENTER_ALWAYS)
	window->use_uposition = TRUE;
    }
  
  /* Compute new set of hints for the window
   */
  gtk_window_compute_hints (window, &new_geometry, &new_flags);
  if (!gtk_window_compare_hints (&info->last.geometry, info->last.flags,
				 &new_geometry, new_flags))
    {
      hints_changed = TRUE;
      info->last.geometry = new_geometry;
      info->last.flags = new_flags;
    }

  /* From the default size and the allocation, figure out the size
   * the window should be.
   */
  if (!default_size_changed ||
      (!window->auto_shrink &&
       new_width <= widget->allocation.width &&
       new_height <= widget->allocation.height))
    {
      new_width = widget->allocation.width;
      new_height = widget->allocation.height;
    }

  /* constrain the window size to the specified geometry */
  gtk_window_constrain_size (window,
			     &new_geometry, new_flags,
			     new_width, new_height,
			     &new_width, &new_height);

  /* compute new window position if a move is required
   */
  gtk_window_compute_reposition (window, new_width, new_height, &x, &y);
  if (x != 1 && y != -1 && !(new_flags & GDK_HINT_POS))
    {
      new_flags |= GDK_HINT_POS;
      hints_changed = TRUE;
    }


  /* handle actual resizing:
   * - handle reallocations due to configure events
   * - figure whether we need to request a new window size
   * - handle simple resizes within our widget tree
   * - reposition window if neccessary
   */
  width = widget->allocation.width;
  height = widget->allocation.height;

  if (window->handling_resize)
    { 
      GtkAllocation allocation;
      
      /* if we are just responding to a configure event, which
       * might be due to a resize by the window manager, the
       * user, or a response to a resizing request we made
       * earlier, we go ahead, allocate the new size and we're done
       * (see gtk_window_configure_event() for more details).
       */
      
      window->handling_resize = FALSE;
      
      allocation = widget->allocation;
      
      gtk_widget_size_allocate (widget, &allocation);
      gtk_widget_queue_draw (widget);

#ifdef FIXME_ZVT_ME_HARDER
      if ((default_size_changed || hints_changed) && (width != new_width || height != new_height))
	{
	  /* We could be here for two reasons
	   *  1) We coincidentally got a resize while handling
	   *     another resize.
	   *  2) Our computation of default_size_changed was completely
	   *     screwed up, probably because one of our children
	   *     is broken (i.e. changes requisition during
	   *     size allocation). It's probably a zvt widget.
	   *
	   * For 1), we could just go ahead and ask for the
	   * new size right now, but doing that for 2)
	   * might well be fighting the user (and can even
	   * trigger a loop). Since we really don't want to
	   * do that, we requeue a resize in hopes that
	   * by the time it gets handled, the child has seen
	   * the light and is willing to go along with the
	   * new size. (this happens for the zvt widget, since
	   * the size_allocate() above will have stored the
	   * requisition corresponding to the new size in the
	   * zvt widget)
	   *
	   * This doesn't buy us anything for 1), but it shouldn't
	   * hurt us too badly, since it is what would have
	   * happened if we had gotten the configure event before
	   * the new size had been set.
	   */
	  
	  if (x != -1 && y != -1)
	    gdk_window_move (widget->window, x, y);

	  /* we have to preserve the values and flags that are used
	   * for computation of default_size_changed and hints_changed
	   */

	  info->last = saved_last_info;
	  
	  gtk_widget_queue_resize (widget);

	  return;
	}
#endif /* FIXME_ZVT_ME_HARDER */
    }

  /* Now set hints if necessary
   */
  if (hints_changed)
    gdk_window_set_geometry_hints (widget->window,
				   &new_geometry,
				   new_flags);

  if ((default_size_changed || hints_changed) &&
      (width != new_width || height != new_height))
    {
      /* given that (width != new_width || height != new_height), we are in one
       * of the following situations:
       * 
       * default_size_changed
       *   our requisition has changed and we need a different window size,
       *   so we request it from the window manager.
       *
       * !default_size_changed
       *   the window manager wouldn't assign us the size we requested, in this
       *   case we don't try to request a new size with every resize.
       *
       * !default_size_changed && hints_changed
       *   the window manager rejects our size, but we have just changed the
       *   window manager hints, so there's a certain chance our request will
       *   be honoured this time, so we try again.
       */
      
      /* request a new window size */
      if (x != -1 && y != -1)
	gdk_window_move_resize (GTK_WIDGET (window)->window, x, y, new_width, new_height);
      else
	gdk_window_resize (GTK_WIDGET (window)->window, new_width, new_height);
      window->resize_count += 1;
      
      /* we are now awaiting the new configure event in response to our
       * resizing request. the configure event will cause a new resize
       * with ->handling_resize=TRUE.
       * until then, we want to
       * - discard expose events
       * - coalesce resizes for our children
       * - defer any window resizes until the configure event arrived
       * to achive this, we queue a resize for the window, but remove its
       * resizing handler, so resizing will not be handled from the next
       * idle handler but when the configure event arrives.
       *
       * FIXME: we should also dequeue the pending redraws here, since
       * we handle those ourselves in ->handling_resize==TRUE.
       */
      gtk_widget_queue_resize (GTK_WIDGET (container));
      if (container->resize_mode == GTK_RESIZE_QUEUE)
	gtk_container_dequeue_resize_handler (container);
    }
  else
    {
      if (x != -1 && y != -1)
	gdk_window_move (widget->window, x, y);

      if (container->resize_widgets)
	gtk_container_resize_children (GTK_CONTAINER (window));
    }
}

/* Compare two sets of Geometry hints for equality.
 */
static gboolean
gtk_window_compare_hints (GdkGeometry *geometry_a,
			  guint        flags_a,
			  GdkGeometry *geometry_b,
			  guint        flags_b)
{
  if (flags_a != flags_b)
    return FALSE;
  
  if ((flags_a & GDK_HINT_MIN_SIZE) &&
      (geometry_a->min_width != geometry_b->min_width ||
       geometry_a->min_height != geometry_b->min_height))
    return FALSE;

  if ((flags_a & GDK_HINT_MAX_SIZE) &&
      (geometry_a->max_width != geometry_b->max_width ||
       geometry_a->max_height != geometry_b->max_height))
    return FALSE;

  if ((flags_a & GDK_HINT_BASE_SIZE) &&
      (geometry_a->base_width != geometry_b->base_width ||
       geometry_a->base_height != geometry_b->base_height))
    return FALSE;

  if ((flags_a & GDK_HINT_ASPECT) &&
      (geometry_a->min_aspect != geometry_b->min_aspect ||
       geometry_a->max_aspect != geometry_b->max_aspect))
    return FALSE;

  if ((flags_a & GDK_HINT_RESIZE_INC) &&
      (geometry_a->width_inc != geometry_b->width_inc ||
       geometry_a->height_inc != geometry_b->height_inc))
    return FALSE;

  return TRUE;
}

/* Compute the default_size for a window. The result will
 * be stored in *width and *height. The default size is
 * the size the window should have when initially mapped.
 * This routine does not attempt to constrain the size
 * to obey the geometry hints - that must be done elsewhere.
 */
static void 
gtk_window_compute_default_size (GtkWindow       *window,
				 guint           *width,
				 guint           *height)
{
  GtkRequisition requisition;
  GtkWindowGeometryInfo *info;
  
  gtk_widget_get_child_requisition (GTK_WIDGET (window), &requisition);
  *width = requisition.width;
  *height = requisition.height;

  info = gtk_window_get_geometry_info (window, FALSE);
  
  if (*width == 0 && *height == 0)
    {
      /* empty window */
      *width = 200;
      *height = 200;
    }
  
  if (info)
    {
      *width = info->width > 0 ? info->width : *width;
      *height = info->height > 0 ? info->height : *height;
    }
}

/* Constrain a window size to obey the hints passed in geometry
 * and flags. The result will be stored in *new_width and *new_height
 *
 * This routine is partially borrowed from fvwm.
 *
 * Copyright 1993, Robert Nation
 *     You may use this code for any purpose, as long as the original
 *     copyright remains in the source code and all documentation
 *
 * which in turn borrows parts of the algorithm from uwm
 */
static void 
gtk_window_constrain_size (GtkWindow   *window,
			   GdkGeometry *geometry,
			   guint        flags,
			   gint         width,
			   gint         height,
			   gint        *new_width,
			   gint        *new_height)
{
  gint min_width = 0;
  gint min_height = 0;
  gint base_width = 0;
  gint base_height = 0;
  gint xinc = 1;
  gint yinc = 1;
  gint max_width = G_MAXINT;
  gint max_height = G_MAXINT;
  
#define FLOOR(value, base)	( ((gint) ((value) / (base))) * (base) )

  if ((flags & GDK_HINT_BASE_SIZE) && (flags & GDK_HINT_MIN_SIZE))
    {
      base_width = geometry->base_width;
      base_height = geometry->base_height;
      min_width = geometry->min_width;
      min_height = geometry->min_height;
    }
  else if (flags & GDK_HINT_BASE_SIZE)
    {
      base_width = geometry->base_width;
      base_height = geometry->base_height;
      min_width = geometry->base_width;
      min_height = geometry->base_height;
    }
  else if (flags & GDK_HINT_MIN_SIZE)
    {
      base_width = geometry->min_width;
      base_height = geometry->min_height;
      min_width = geometry->min_width;
      min_height = geometry->min_height;
    }

  if (flags & GDK_HINT_MAX_SIZE)
    {
      max_width = geometry->max_width ;
      max_height = geometry->max_height;
    }

  if (flags & GDK_HINT_RESIZE_INC)
    {
      xinc = MAX (xinc, geometry->width_inc);
      yinc = MAX (yinc, geometry->height_inc);
    }
  
  /* clamp width and height to min and max values
   */
  width = CLAMP (width, min_width, max_width);
  height = CLAMP (height, min_height, max_height);
  
  /* shrink to base + N * inc
   */
  width = base_width + FLOOR (width - base_width, xinc);
  height = base_height + FLOOR (height - base_height, yinc);

  /* constrain aspect ratio, according to:
   *
   *                width     
   * min_aspect <= -------- <= max_aspect
   *                height    
   */
  
  if (flags & GDK_HINT_ASPECT &&
      geometry->min_aspect > 0 &&
      geometry->max_aspect > 0)
    {
      gint delta;

      if (geometry->min_aspect * height > width)
	{
	  delta = FLOOR (height - width * geometry->min_aspect, yinc);
	  if (height - delta >= min_height)
	    height -= delta;
	  else
	    { 
	      delta = FLOOR (height * geometry->min_aspect - width, xinc);
	      if (width + delta <= max_width) 
		width += delta;
	    }
	}
      
      if (geometry->max_aspect * height < width)
	{
	  delta = FLOOR (width - height * geometry->max_aspect, xinc);
	  if (width - delta >= min_width) 
	    width -= delta;
	  else
	    {
	      delta = FLOOR (width / geometry->max_aspect - height, yinc);
	      if (height + delta <= max_height)
		height += delta;
	    }
	}
    }

#undef FLOOR
  
  *new_width = width;
  *new_height = height;
}

/* Compute the set of geometry hints and flags for a window
 * based on the application set geometry, and requisiition
 * of the window. gtk_widget_size_request() must have been
 * called first.
 */
static void
gtk_window_compute_hints (GtkWindow   *window,
			  GdkGeometry *new_geometry,
			  guint       *new_flags)
{
  GtkWidget *widget;
  GtkWidgetAuxInfo *aux_info;
  gint ux, uy;
  gint extra_width = 0;
  gint extra_height = 0;
  GtkWindowGeometryInfo *geometry_info;
  GtkRequisition requisition;

  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);
  
  gtk_widget_get_child_requisition (widget, &requisition);
  geometry_info = gtk_window_get_geometry_info (GTK_WINDOW (widget), FALSE);

  g_return_if_fail (geometry_info != NULL);
  
  *new_flags = geometry_info->mask;
  *new_geometry = geometry_info->geometry;
  
  if (geometry_info->widget)
    {
      extra_width = widget->requisition.width - geometry_info->widget->requisition.width;
      extra_height = widget->requisition.height - geometry_info->widget->requisition.height;
    }
  
  ux = 0;
  uy = 0;
  
  aux_info = gtk_object_get_data (GTK_OBJECT (widget), "gtk-aux-info");
  if (aux_info && (aux_info->x != -1) && (aux_info->y != -1))
    {
      ux = aux_info->x;
      uy = aux_info->y;
      *new_flags |= GDK_HINT_POS;
    }
  
  if (*new_flags & GDK_HINT_BASE_SIZE)
    {
      new_geometry->base_width += extra_width;
      new_geometry->base_height += extra_height;
    }
  else if (!(*new_flags & GDK_HINT_MIN_SIZE) &&
	   (*new_flags & GDK_HINT_RESIZE_INC) &&
	   ((extra_width != 0) || (extra_height != 0)))
    {
      *new_flags |= GDK_HINT_BASE_SIZE;
      
      new_geometry->base_width = extra_width;
      new_geometry->base_height = extra_height;
    }
  
  if (*new_flags & GDK_HINT_MIN_SIZE)
    {
      new_geometry->min_width += extra_width;
      new_geometry->min_height += extra_height;
    }
  else if (!window->allow_shrink)
    {
      *new_flags |= GDK_HINT_MIN_SIZE;
      
      new_geometry->min_width = requisition.width;
      new_geometry->min_height = requisition.height;
    }
  
  if (*new_flags & GDK_HINT_MAX_SIZE)
    {
      new_geometry->max_width += extra_width;
      new_geometry->max_height += extra_height;
    }
  else if (!window->allow_grow)
    {
      *new_flags |= GDK_HINT_MAX_SIZE;
      
      new_geometry->max_width = requisition.width;
      new_geometry->max_height = requisition.height;
    }
}

/* Compute a new position for the window based on a new
 * size. *x and *y will be set to the new coordinates, or to -1 if the
 * window does not need to be moved
 */
static void 
gtk_window_compute_reposition (GtkWindow  *window,
			       gint        new_width,
			       gint        new_height,
			       gint       *x,
			       gint       *y)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (window);

  *x = -1;
  *y = -1;
  
  switch (window->position)
    {
    case GTK_WIN_POS_CENTER:
    case GTK_WIN_POS_CENTER_ALWAYS:
      if (window->use_uposition)
	{
	  gint screen_width = gdk_screen_width ();
	  gint screen_height = gdk_screen_height ();
	  
	  *x = (screen_width - new_width) / 2;
	  *y = (screen_height - new_height) / 2;
	}
      break;
    case GTK_WIN_POS_MOUSE:
      if (window->use_uposition)
	{
	  gint screen_width = gdk_screen_width ();
	  gint screen_height = gdk_screen_height ();
	  
	  gdk_window_get_pointer (NULL, x, y, NULL);
	  *x -= new_width / 2;
	  *y -= new_height / 2;
	  *x = CLAMP (*x, 0, screen_width - new_width);
	  *y = CLAMP (*y, 0, screen_height - new_height);
	}
      break;
    default:
      if (window->use_uposition)
	{
	  GtkWidgetAuxInfo *aux_info;
	  
	  aux_info = gtk_object_get_data (GTK_OBJECT (window), "gtk-aux-info");
	  if (aux_info &&
	      aux_info->x != -1 && aux_info->y != -1 &&
	      aux_info->x != -2 && aux_info->y != -2)
	    {
	      *x = aux_info->x;
	      *y = aux_info->y;
	    }
	}
      break;
    }

  if (*x != -1 && *y != -1)
    {
      GtkWidgetAuxInfo *aux_info;
      
      /* we handle necessary window positioning by hand here,
       * so we can coalesce the window movement with possible
       * resizes to get only one configure event.
       * keep this in sync with gtk_widget_set_uposition()
       * and gtk_window_reposition().
       */
      gtk_widget_set_uposition (widget, -1, -1); /* ensure we have aux_info */
      
      aux_info = gtk_object_get_data (GTK_OBJECT (widget), "gtk-aux-info");
      aux_info->x = *x;
      aux_info->y = *y;

      window->use_uposition = FALSE;
    }
}

/***********************
 * Redrawing functions *
 ***********************/

static void
gtk_window_paint (GtkWidget     *widget,
		  GdkRectangle *area)
{
  gtk_paint_flat_box (widget->style, widget->window, GTK_STATE_NORMAL, 
		      GTK_SHADOW_NONE, area, widget, "base", 0, 0, -1, -1);
}

static gint
gtk_window_expose (GtkWidget      *widget,
		   GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (!GTK_WIDGET_APP_PAINTABLE (widget))
    gtk_window_paint (widget, &event->area);
  
  if (GTK_WIDGET_CLASS (parent_class)->expose_event)
    return GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);

  return TRUE;
}

static void
gtk_window_draw (GtkWidget    *widget,
		 GdkRectangle *area)
{
  if (!GTK_WIDGET_APP_PAINTABLE (widget))
    gtk_window_paint (widget, area);
  
  if (GTK_WIDGET_CLASS (parent_class)->draw)
    (* GTK_WIDGET_CLASS (parent_class)->draw) (widget, area);
}
