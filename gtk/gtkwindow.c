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

#include <string.h>
#include <limits.h>
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"

#if defined (GDK_WINDOWING_X11)
#include "x11/gdkx.h"
#elif defined (GDK_WINDOWING_WIN32)
#include "win32/gdkwin32.h"
#elif defined (GDK_WINDOWING_NANOX)
#include "nanox/gdkprivate-nanox.h"
#elif defined (GDK_WINDOWING_FB)
#include "linux-fb/gdkfb.h"
#endif

#include "gtkprivate.h"
#include "gtkrc.h"
#include "gtksignal.h"
#include "gtkwindow.h"
#include "gtkwindow-decorate.h"
#include "gtkbindings.h"
#include "gtkmain.h"
#include "gtkiconfactory.h"
#include "gtkintl.h"

enum {
  SET_FOCUS,
  FRAME_EVENT,
  ACTIVATE_FOCUS,
  ACTIVATE_DEFAULT,
  MOVE_FOCUS,
  LAST_SIGNAL
};

enum {
  PROP_0,

  /* Construct */
  PROP_TYPE,

  /* Style Props */
  PROP_TITLE,
  PROP_AUTO_SHRINK,
  PROP_ALLOW_SHRINK,
  PROP_ALLOW_GROW,
  PROP_MODAL,
  PROP_WIN_POS,
  PROP_DEFAULT_WIDTH,
  PROP_DEFAULT_HEIGHT,
  PROP_DESTROY_WITH_PARENT,

  LAST_ARG
};

typedef struct {
  GdkGeometry    geometry; /* Last set of geometry hints we set */
  GdkWindowHints flags;
  gint           width;
  gint           height;
} GtkWindowLastGeometryInfo;

struct _GtkWindowGeometryInfo
{
  /* Properties that the app has set on the window
   */
  GdkGeometry    geometry;	/* Geometry hints */
  GdkWindowHints mask;
  GtkWidget     *widget;	/* subwidget to which hints apply */
  gint           width;		/* Default size */
  gint           height;
  guint		 may_shrink : 1; /* one-shot auto_shrink behaviour after set_default_size */

  GtkWindowLastGeometryInfo last;
};

typedef struct {
  GtkWindow *window;
  guint keyval;

  GSList *targets;
} GtkWindowMnemonic;


static void gtk_window_class_init         (GtkWindowClass    *klass);
static void gtk_window_init               (GtkWindow         *window);
static void gtk_window_shutdown           (GObject           *object);
static void gtk_window_destroy            (GtkObject         *object);
static void gtk_window_finalize           (GObject           *object);
static void gtk_window_show               (GtkWidget         *widget);
static void gtk_window_hide               (GtkWidget         *widget);
static void gtk_window_map                (GtkWidget         *widget);
static void gtk_window_unmap              (GtkWidget         *widget);
static void gtk_window_realize            (GtkWidget         *widget);
static void gtk_window_unrealize          (GtkWidget         *widget);
static void gtk_window_size_request       (GtkWidget         *widget,
					   GtkRequisition    *requisition);
static void gtk_window_size_allocate      (GtkWidget         *widget,
					   GtkAllocation     *allocation);
static gint gtk_window_event              (GtkWidget *widget,
					   GdkEvent *event);
static gboolean gtk_window_frame_event    (GtkWidget *widget,
					   GdkEvent *event);
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
static gint gtk_window_focus              (GtkWidget        *widget,
				           GtkDirectionType  direction);
static void gtk_window_real_set_focus     (GtkWindow         *window,
					   GtkWidget         *focus);

static void gtk_window_real_activate_default (GtkWindow         *window);
static void gtk_window_real_activate_focus   (GtkWindow         *window);
static void gtk_window_move_focus            (GtkWindow         *window,
                                              GtkDirectionType   dir);

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
static gint gtk_window_compute_reposition (GtkWindow         *window,
					   gint               new_width,
					   gint               new_height,
					   gint              *x,
					   gint              *y);

static void gtk_window_read_rcfiles       (GtkWidget         *widget,
					   GdkEventClient    *event);
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


static GSList      *toplevel_list = NULL;
static GHashTable  *mnemonic_hash_table = NULL;
static GtkBinClass *parent_class = NULL;
static guint        window_signals[LAST_SIGNAL] = { 0 };

static void gtk_window_set_property (GObject         *object,
				     guint            prop_id,
				     const GValue    *value,
				     GParamSpec      *pspec);
static void gtk_window_get_property (GObject         *object,
				     guint            prop_id,
				     GValue          *value,
				     GParamSpec      *pspec);


static guint
mnemonic_hash (gconstpointer key)
{
  const GtkWindowMnemonic *k;
  guint h;
  
  k = (GtkWindowMnemonic *)key;
  
  h = (gulong) k->window;
  h ^= k->keyval << 16;
  h ^= k->keyval >> 16;

  return h;
}

static gboolean
mnemonic_equal (gconstpointer a, gconstpointer b)
{
  const GtkWindowMnemonic *ka;
  const GtkWindowMnemonic *kb;
  
  ka = (GtkWindowMnemonic *)a;
  kb = (GtkWindowMnemonic *)b;

  return
    (ka->window == kb->window) &&
    (ka->keyval == kb->keyval);
}

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
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkBindingSet *binding_set;
  
  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;
  
  parent_class = gtk_type_class (gtk_bin_get_type ());

  gobject_class->shutdown = gtk_window_shutdown;
  gobject_class->finalize = gtk_window_finalize;

  gobject_class->set_property = gtk_window_set_property;
  gobject_class->get_property = gtk_window_get_property;
  
  object_class->destroy = gtk_window_destroy;

  widget_class->show = gtk_window_show;
  widget_class->hide = gtk_window_hide;
  widget_class->map = gtk_window_map;
  widget_class->unmap = gtk_window_unmap;
  widget_class->realize = gtk_window_realize;
  widget_class->unrealize = gtk_window_unrealize;
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
  widget_class->focus = gtk_window_focus;
  
  widget_class->expose_event = gtk_window_expose;
   
  container_class->check_resize = gtk_window_check_resize;

  klass->set_focus = gtk_window_real_set_focus;
  klass->frame_event = gtk_window_frame_event;

  klass->activate_default = gtk_window_real_activate_default;
  klass->activate_focus = gtk_window_real_activate_focus;
  klass->move_focus = gtk_window_move_focus;
  
  /* Construct */
  g_object_class_install_property (gobject_class,
                                   PROP_TYPE,
                                   g_param_spec_enum ("type",
						      _("Window Type"),
						      _("The type of the window"),
						      GTK_TYPE_WINDOW_TYPE,
						      GTK_WINDOW_TOPLEVEL,
						      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /* Style Props */
  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        _("Window Title"),
                                                        _("The title of the window"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_AUTO_SHRINK,
                                   g_param_spec_boolean ("auto_shrink",
							 _("Auto Shrink"),
							 _("If TRUE, the window automatically shrinks to its size request anytime a resize occurs. Don't use this feature, it makes no sense."),
							 FALSE,
							 G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ALLOW_SHRINK,
                                   g_param_spec_boolean ("allow_shrink",
							 _("Allow Shrink"),
							 _("If TRUE, the window has no mimimum size. Don't use this feature, it makes no sense."),
							 FALSE,
							 G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ALLOW_GROW,
                                   g_param_spec_boolean ("allow_grow",
							 _("Allow Grow"),
							 _("If TRUE, users can expand the window beyond its minimum size."),
							 TRUE,
							 G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_MODAL,
                                   g_param_spec_boolean ("modal",
							 _("Modal"),
							 _("If TRUE, the window is modal (other windows are not usable while this one is up)."),
							 FALSE,
							 G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_WIN_POS,
                                   g_param_spec_enum ("window_position",
						      _("Window Position"),
						      _("The initial position of the window."),
						      GTK_TYPE_WINDOW_POSITION,
						      GTK_WIN_POS_NONE,
						      G_PARAM_READWRITE));
 
  g_object_class_install_property (gobject_class,
                                   PROP_DEFAULT_WIDTH,
                                   g_param_spec_int ("default_width",
						     _("Default Width"),
						     _("The default width of the window, or 0 to use the size request."),
						     0,
						     G_MAXINT,
						     0,
						     G_PARAM_READWRITE));
 
  g_object_class_install_property (gobject_class,
                                   PROP_DEFAULT_HEIGHT,
                                   g_param_spec_int ("default_height",
						     _("Default Height"),
						     _("The default height of the windo, or 0 to use the size request."),
						     0,
						     G_MAXINT,
						     0,
						     G_PARAM_READWRITE));
 
  g_object_class_install_property (gobject_class,
                                   PROP_DESTROY_WITH_PARENT,
                                   g_param_spec_boolean ("destroy_with_parent",
							 _("Destroy with Parent"),
							 _("If this window should be destroyed when the parent is destroyed,"),
                                                         FALSE,
							 G_PARAM_READWRITE));

  /* Style props are set or not */

  window_signals[SET_FOCUS] =
    g_signal_newc ("set_focus",
                   G_TYPE_FROM_CLASS (object_class),
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET (GtkWindowClass, set_focus),
                   NULL, NULL,
                   gtk_marshal_VOID__OBJECT,
                   G_TYPE_NONE, 1,
                   GTK_TYPE_WIDGET);
  
  window_signals[FRAME_EVENT] =
    g_signal_newc ("frame_event",
		   G_TYPE_FROM_CLASS(object_class),
		   G_SIGNAL_RUN_LAST,
		   G_STRUCT_OFFSET(GtkWindowClass, frame_event),
		   _gtk_boolean_handled_accumulator, NULL,
		   gtk_marshal_BOOLEAN__BOXED,
		   G_TYPE_BOOLEAN, 1,
		   GDK_TYPE_EVENT);

  window_signals[ACTIVATE_FOCUS] =
    g_signal_newc ("activate_focus",
                   G_OBJECT_CLASS_TYPE (object_class),
                   G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                   GTK_SIGNAL_OFFSET (GtkWindowClass, activate_focus),
		   NULL, NULL,
		   gtk_marshal_VOID__VOID,
                   G_TYPE_NONE,
                   0);

  window_signals[ACTIVATE_DEFAULT] =
    g_signal_newc ("activate_default",
                   G_OBJECT_CLASS_TYPE (object_class),
                   G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                   GTK_SIGNAL_OFFSET (GtkWindowClass, activate_default),
		   NULL, NULL,
		   gtk_marshal_VOID__VOID,
                   G_TYPE_NONE,
                   0);

  window_signals[MOVE_FOCUS] =
    g_signal_newc ("move_focus",
                   G_OBJECT_CLASS_TYPE (object_class),
                   G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                   GTK_SIGNAL_OFFSET (GtkWindowClass, move_focus),
		   NULL, NULL,
		   gtk_marshal_VOID__ENUM,
                   G_TYPE_NONE,
                   1,
                   GTK_TYPE_DIRECTION_TYPE);
  
  if (!mnemonic_hash_table)
    mnemonic_hash_table = g_hash_table_new (mnemonic_hash,
					    mnemonic_equal);

  /*
   * Key bindings
   */

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set, GDK_space, 0,
                                "activate_focus", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_Return, 0,
                                "activate_default", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KP_Enter, 0,
                                "activate_default", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_Up, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_UP);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Up, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_UP);

  gtk_binding_entry_add_signal (binding_set, GDK_Down, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Down, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_DOWN);

  gtk_binding_entry_add_signal (binding_set, GDK_Left, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_LEFT);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Left, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_LEFT);  

  gtk_binding_entry_add_signal (binding_set, GDK_Right, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_RIGHT);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Right, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_RIGHT);  

  gtk_binding_entry_add_signal (binding_set, GDK_Tab, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_FORWARD);
  gtk_binding_entry_add_signal (binding_set, GDK_ISO_Left_Tab, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_FORWARD);

  gtk_binding_entry_add_signal (binding_set, GDK_Tab, GDK_SHIFT_MASK,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_BACKWARD);
  gtk_binding_entry_add_signal (binding_set, GDK_ISO_Left_Tab, GDK_SHIFT_MASK,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_BACKWARD);
}

static void
gtk_window_init (GtkWindow *window)
{
  GTK_WIDGET_UNSET_FLAGS (window, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (window, GTK_TOPLEVEL);

  GTK_PRIVATE_SET_FLAG (window, GTK_ANCHORED);

  gtk_container_set_resize_mode (GTK_CONTAINER (window), GTK_RESIZE_QUEUE);

  window->title = NULL;
  window->wmclass_name = g_strdup (g_get_prgname ());
  window->wmclass_class = g_strdup (gdk_progclass);
  window->wm_role = NULL;
  window->geometry_info = NULL;
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
  window->frame = NULL;
  window->has_frame = FALSE;
  window->frame_left = 0;
  window->frame_right = 0;
  window->frame_top = 0;
  window->frame_bottom = 0;
  window->type_hint = GDK_WINDOW_TYPE_HINT_NORMAL;
  window->gravity = GDK_GRAVITY_NORTH_WEST;
  window->decorated = TRUE;
  window->mnemonic_modifier = GDK_MOD1_MASK;
  
  gtk_widget_ref (GTK_WIDGET (window));
  gtk_object_sink (GTK_OBJECT (window));
  window->has_user_ref_count = TRUE;
  toplevel_list = g_slist_prepend (toplevel_list, window);

  gtk_decorated_window_init (window);

  gtk_signal_connect (GTK_OBJECT (window),
		      "event",
		      GTK_SIGNAL_FUNC (gtk_window_event),
		      NULL);
}

static void
gtk_window_set_property (GObject      *object,
			 guint         prop_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
  GtkWindow  *window;

  window = GTK_WINDOW (object);

  switch (prop_id)
    {
    case PROP_TYPE:
      window->type = g_value_get_enum (value);
      break;
    case PROP_TITLE:
      gtk_window_set_title (window, g_value_get_string (value));
      break;
    case PROP_AUTO_SHRINK:
      window->auto_shrink = g_value_get_boolean (value);
      gtk_widget_queue_resize (GTK_WIDGET (window));
      break;
    case PROP_ALLOW_SHRINK:
      window->allow_shrink = g_value_get_boolean (value);
      gtk_widget_queue_resize (GTK_WIDGET (window));
      break;
    case PROP_ALLOW_GROW:
      window->allow_grow = g_value_get_boolean (value);
      gtk_widget_queue_resize (GTK_WIDGET (window));
      break;
    case PROP_MODAL:
      gtk_window_set_modal (window, g_value_get_boolean (value));
      break;
    case PROP_WIN_POS:
      gtk_window_set_position (window, g_value_get_enum (value));
      break;
    case PROP_DEFAULT_WIDTH:
      gtk_window_set_default_size (window, g_value_get_int (value), -1);
      break;
    case PROP_DEFAULT_HEIGHT:
      gtk_window_set_default_size (window, -1, g_value_get_int (value));
      break;
    case PROP_DESTROY_WITH_PARENT:
      gtk_window_set_destroy_with_parent (window, g_value_get_boolean (value));
      break;
    default:
      break;
    }
}

static void
gtk_window_get_property (GObject      *object,
			 guint         prop_id,
			 GValue       *value,
			 GParamSpec   *pspec)
{
  GtkWindow  *window;

  window = GTK_WINDOW (object);

  switch (prop_id)
    {
      GtkWindowGeometryInfo *info;
    case PROP_TYPE:
      g_value_set_enum (value, window->type);
      break;
    case PROP_TITLE:
      g_value_set_string (value, window->title);
      break;
    case PROP_AUTO_SHRINK:
      g_value_set_boolean (value, window->auto_shrink);
      break;
    case PROP_ALLOW_SHRINK:
      g_value_set_boolean (value, window->allow_shrink);
      break;
    case PROP_ALLOW_GROW:
      g_value_set_boolean (value, window->allow_grow);
      break;
    case PROP_MODAL:
      g_value_set_boolean (value, window->modal);
      break;
    case PROP_WIN_POS:
      g_value_set_enum (value, window->position);
      break;
    case PROP_DEFAULT_WIDTH:
      info = gtk_window_get_geometry_info (window, FALSE);
      if (!info)
	g_value_set_int (value, 0);
      else
	g_value_set_int (value, info->width);
      break;
    case PROP_DEFAULT_HEIGHT:
      info = gtk_window_get_geometry_info (window, FALSE);
      if (!info)
	g_value_set_int (value, 0);
      else
	g_value_set_int (value, info->height);
      break;
    case PROP_DESTROY_WITH_PARENT:
      g_value_set_boolean (value, window->destroy_with_parent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_window_new:
 * @type: type of window
 * 
 * Creates a new #GtkWindow, which is a toplevel window that can
 * contain other widgets. Nearly always, the type of the window should
 * be #GTK_WINDOW_TOPLEVEL. If you're implementing something like a
 * popup menu from scratch (which is a bad idea, just use #GtkMenu),
 * you might use #GTK_WINDOW_POPUP. #GTK_WINDOW_POPUP is not for
 * dialogs, though in some other toolkits dialogs are called "popups."
 * In GTK+, #GTK_WINDOW_POPUP means a pop-up menu or pop-up tooltip.
 * Popup windows are not controlled by the window manager.
 *
 * If you simply want an undecorated window (no window borders), use
 * gtk_window_set_decorated(), don't use #GTK_WINDOW_POPUP.
 * 
 * Return value: a new #GtkWindow.
 **/
GtkWidget*
gtk_window_new (GtkWindowType type)
{
  GtkWindow *window;

  g_return_val_if_fail (type >= GTK_WINDOW_TOPLEVEL && type <= GTK_WINDOW_POPUP, NULL);

  window = gtk_type_new (GTK_TYPE_WINDOW);

  window->type = type;

  return GTK_WIDGET (window);
}

/**
 * gtk_window_set_title:
 * @window: a #GtkWindow
 * @title: title of the window
 * 
 * Sets the title of the #GtkWindow. The title of a window will be displayed in
 * its title bar; on the X Window System, the title bar is rendered by the
 * window manager, so exactly how the title appears to users may vary according
 * to a user's exact configuration. The title should help a user distinguish
 * this window from other windows they may have open. A good title might
 * include the application name and current document filename, for example.
 * 
 **/
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
    {
      gdk_window_set_title (GTK_WIDGET (window)->window, window->title);

      gtk_decorated_window_set_title (window, title);
    }

  g_object_notify (G_OBJECT (window), "title");
}

/**
 * gtk_window_set_wmclass:
 * @window: a #GtkWindow
 * @wmclass_name: window name hint
 * @wmclass_class: window class hint
 *
 * Don't use this function. It sets the X Window System "class" and
 * "name" hints for a window.  According to the ICCCM, you should
 * always set these to the same value for all windows in an
 * application, and GTK sets them to that value by default, so calling
 * this function is sort of pointless. However, you may want to call
 * gtk_window_set_role() on each window in your application, for the
 * benefit of the session manager. Setting the role allows the window
 * manager to restore window positions when loading a saved session.
 * 
 **/
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
    g_warning ("gtk_window_set_wmclass: shouldn't set wmclass after window is realized!\n");
}

/**
 * gtk_window_set_role:
 * @window: a #GtkWindow
 * @role: unique identifier for the window to be used when restoring a session
 *
 * In combination with the window title, the window role allows a
 * window manager to identify "the same" window when an application is
 * restarted. So for example you might set the "toolbox" role on your
 * app's toolbox window, so that when the user restarts their session,
 * the window manager can put the toolbox back in the same place.
 *
 * If a window already has a unique title, you don't need to set the
 * role, since the WM can use the title to identify the window when
 * restoring the session.
 * 
 **/
void
gtk_window_set_role (GtkWindow   *window,
                     const gchar *role)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (role == window->wm_role)
    return;
  
  g_free (window->wm_role);
  window->wm_role = g_strdup (role);
  
  if (GTK_WIDGET_REALIZED (window))
    g_warning ("gtk_window_set_role(): shouldn't set role after window is realized!\n");
}

/**
 * gtk_window_set_focus:
 * @window: a #GtkWindow
 * @focus: widget to be the new focus widget
 *
 * If @focus is not the current focus widget, and is focusable, emits
 * the "set_focus" signal to set @focus as the focus widget for the
 * window.  This function is more or less GTK-internal; to focus an
 * entry widget or the like, you should use gtk_widget_grab_focus()
 * instead of this function.
 * 
 **/
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

/**
 * gtk_window_set_default:
 * @window: a #GtkWindow
 * @default_widget: widget to be the default
 *
 * The default widget is the widget that's activated when the user
 * presses Enter in a dialog (for example). This function tells a
 * #GtkWindow about the current default widget; it's really a GTK
 * internal function and you shouldn't need it. Instead, to change the
 * default widget, first set the #GTK_CAN_DEFAULT flag on the widget
 * you'd like to make the default using GTK_WIDGET_SET_FLAGS(), then
 * call gtk_widget_grab_default() to move the default.
 * 
 **/
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
	  gtk_widget_queue_draw (window->default_widget);
	}

      window->default_widget = default_widget;

      if (window->default_widget)
	{
	  if (window->focus_widget == NULL ||
	      !GTK_WIDGET_RECEIVES_DEFAULT (window->focus_widget))
	    GTK_WIDGET_SET_FLAGS (window->default_widget, GTK_HAS_DEFAULT);
	  gtk_widget_queue_draw (window->default_widget);
	}
    }
}

void
gtk_window_set_policy (GtkWindow *window,
		       gboolean   allow_shrink,
		       gboolean   allow_grow,
		       gboolean   auto_shrink)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  window->allow_shrink = (allow_shrink != FALSE);
  window->allow_grow = (allow_grow != FALSE);
  window->auto_shrink = (auto_shrink != FALSE);

  g_object_notify (G_OBJECT (window), "allow_shrink");
  g_object_notify (G_OBJECT (window), "allow_grow");
  g_object_notify (G_OBJECT (window), "auto_shrink");
  
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
gtk_window_add_mnemonic (GtkWindow *window,
			 guint      keyval,
			 GtkWidget *target)
{
  GtkWindowMnemonic key;
  GtkWindowMnemonic *mnemonic;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_WIDGET (target));
  
  key.window = window;
  key.keyval = keyval;
  mnemonic = g_hash_table_lookup (mnemonic_hash_table, &key);

  if (mnemonic)
    {
      g_return_if_fail (g_slist_find (mnemonic->targets, target) == NULL);
      mnemonic->targets = g_slist_prepend (mnemonic->targets, target);
    }
  else
    {
      mnemonic = g_new (GtkWindowMnemonic, 1);
      *mnemonic = key;
      mnemonic->targets = g_slist_prepend (NULL, target);
      g_hash_table_insert (mnemonic_hash_table, mnemonic, mnemonic);
    }
}

void
gtk_window_remove_mnemonic (GtkWindow *window,
			    guint      keyval,
			    GtkWidget *target)
{
  GtkWindowMnemonic key;
  GtkWindowMnemonic *mnemonic;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_WIDGET (target));
  
  key.window = window;
  key.keyval = keyval;
  mnemonic = g_hash_table_lookup (mnemonic_hash_table, &key);

  g_return_if_fail (mnemonic && g_slist_find (mnemonic->targets, target) != NULL);

  mnemonic->targets = g_slist_remove (mnemonic->targets, target);
  if (mnemonic->targets == NULL)
    {
      g_hash_table_remove (mnemonic_hash_table, mnemonic);
      g_free (mnemonic);
    }
}

gboolean
gtk_window_mnemonic_activate (GtkWindow      *window,
			      guint           keyval,
			      GdkModifierType modifier)
{
  GtkWindowMnemonic key;
  GtkWindowMnemonic *mnemonic;
  GSList *list;
  GtkWidget *widget, *chosen_widget;
  gboolean overloaded;

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  if (window->mnemonic_modifier != (modifier & gtk_accelerator_get_default_mod_mask ()))
    return FALSE;
  
  key.window = window;
  key.keyval = keyval;
  mnemonic = g_hash_table_lookup (mnemonic_hash_table, &key);

  if (!mnemonic)
    return FALSE;
  
  overloaded = FALSE;
  chosen_widget = NULL;
  list = mnemonic->targets;
  while (list)
    {
      widget = GTK_WIDGET (list->data);
      
      if (GTK_WIDGET_IS_SENSITIVE (widget) &&
	  GTK_WIDGET_MAPPED (widget))
	{
	  if (chosen_widget)
	    {
	      overloaded = TRUE;
	      break;
	    }
	  else
	    chosen_widget = widget;
	}
      list = g_slist_next (list);
    }

  if (chosen_widget)
    {
      /* For round robin we put the activated entry on
       * the end of the list after activation
       */
      mnemonic->targets = g_slist_remove (mnemonic->targets, chosen_widget);
      mnemonic->targets = g_slist_append (mnemonic->targets, chosen_widget);

      return gtk_widget_mnemonic_activate (chosen_widget, overloaded);
    }
  return FALSE;
}

void
gtk_window_set_mnemonic_modifier (GtkWindow      *window,
				  GdkModifierType modifier)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail ((modifier & ~GDK_MODIFIER_MASK) == 0);

  window->mnemonic_modifier = modifier;
}

void
gtk_window_set_position (GtkWindow         *window,
			 GtkWindowPosition  position)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  window->position = position;

  g_object_notify (G_OBJECT (window), "window_position");
}

gboolean 
gtk_window_activate_focus (GtkWindow *window)
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

gboolean
gtk_window_activate_default (GtkWindow *window)
{
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  if (window->default_widget && GTK_WIDGET_IS_SENSITIVE (window->default_widget) &&
      (!window->focus_widget || !GTK_WIDGET_RECEIVES_DEFAULT (window->focus_widget)))
    {
      gtk_widget_activate (window->default_widget);
      return TRUE;
    }
  else if (window->focus_widget)
    {
      if (GTK_WIDGET_IS_SENSITIVE (window->focus_widget))
        gtk_widget_activate (window->focus_widget);
      return TRUE;
    }

  return FALSE;
}

/**
 * gtk_window_set_modal:
 * @window: a #GtkWindow
 * @modal: whether the window is modal
 * 
 * Sets a window modal or non-modal. Modal windows prevent interaction
 * with other windows in the same application. To keep modal dialogs
 * on top of main application windows, use
 * gtk_window_set_transient_for() to make the dialog transient for the
 * parent; most window managers will then disallow lowering the dialog
 * below the parent.
 * 
 * 
 **/
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

  g_object_notify (G_OBJECT (window), "modal");
}

/**
 * gtk_window_list_toplevels:
 * 
 * Returns a list of all existing toplevel windows. The widgets
 * in the list are not individually referenced. If you want
 * to iterate through the list and perform actions involving
 * callbacks that might destroy the widgets, you MUST call
 * g_list_foreach (result, (GFunc)g_object_ref, NULL) first, and
 * then unref all the widgets afterwards.
 * 
 * Return value: list of toplevel widgets
 **/
GList*
gtk_window_list_toplevels (void)
{
  GList *list = NULL;
  GSList *slist;

  for (slist = toplevel_list; slist; slist = slist->next)
    list = g_list_prepend (list, slist->data);

  return list;
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
_gtk_window_reposition (GtkWindow *window,
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

      if (window->frame)
	gdk_window_move (window->frame,	 x - window->frame_left, y - window->frame_top);
      else
	gdk_window_move (GTK_WIDGET (window)->window, x, y);
    }
}

static void
gtk_window_shutdown (GObject *object)
{
  GtkWindow *window;

  g_return_if_fail (GTK_IS_WINDOW (object));

  window = GTK_WINDOW (object);

  gtk_window_set_focus (window, NULL);
  gtk_window_set_default (window, NULL);

  G_OBJECT_CLASS (parent_class)->shutdown (object);
}

static void
parent_destroyed_callback (GtkWindow *parent, GtkWindow *child)
{
  gtk_widget_destroy (GTK_WIDGET (child));
}

static void
connect_parent_destroyed (GtkWindow *window)
{
  if (window->transient_parent)
    {
      gtk_signal_connect (GTK_OBJECT (window->transient_parent),
                          "destroy",
                          GTK_SIGNAL_FUNC (parent_destroyed_callback),
                          window);
    }  
}

static void
disconnect_parent_destroyed (GtkWindow *window)
{
  if (window->transient_parent)
    {
      gtk_signal_disconnect_by_func (GTK_OBJECT (window->transient_parent),
                                     GTK_SIGNAL_FUNC (parent_destroyed_callback),
                                     window);
    }
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

      if (window->destroy_with_parent)
        disconnect_parent_destroyed (window);
      
      window->transient_parent = NULL;
    }
}

/**
 * gtk_window_set_transient_for:
 * @window: a #GtkWindow
 * @parent: parent window
 *
 * Dialog windows should be set transient for the main application
 * window they were spawned from. This allows window managers to
 * e.g. keep the dialog on top of the main window, or center the
 * dialog over the main window. gtk_dialog_new_with_buttons() and
 * other convenience functions in GTK+ will sometimes call
 * gtk_window_set_transient_for() on your behalf.
 * 
 **/
void       
gtk_window_set_transient_for  (GtkWindow *window, 
			       GtkWindow *parent)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
  g_return_if_fail (window != parent);

    
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

      if (window->destroy_with_parent)
        connect_parent_destroyed (window);
      
      if (GTK_WIDGET_REALIZED (window) &&
	  GTK_WIDGET_REALIZED (parent))
	gtk_window_transient_parent_realized (GTK_WIDGET (parent),
					      GTK_WIDGET (window));
    }
}

/**
 * gtk_window_set_type_hint:
 * @window: a #GtkWindow
 * @hint: the window type
 *
 * By setting the type hint for the window, you allow the window
 * manager to decorate and handle the window in a way which is
 * suitable to the function of the window in your application.
 *
 * This function should be called before the window becomes visible.
 *
 * gtk_dialog_new_with_buttons() and other convenience functions in GTK+
 * will sometimes call gtk_window_set_type_hint() on your behalf.
 * 
 **/
void
gtk_window_set_type_hint (GtkWindow           *window, 
			  GdkWindowTypeHint    hint)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (!GTK_WIDGET_VISIBLE (window));
  window->type_hint = hint;
}

/**
 * gtk_window_set_destroy_with_parent:
 * @window: a #GtkWindow
 * @setting: whether to destroy @window with its transient parent
 * 
 * If @setting is TRUE, then destroying the transient parent of @window
 * will also destroy @window itself. This is useful for dialogs that
 * shouldn't persist beyond the lifetime of the main window they're
 * associated with, for example.
 **/
void
gtk_window_set_destroy_with_parent  (GtkWindow *window,
                                     gboolean   setting)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (window->destroy_with_parent == (setting != FALSE))
    return;

  if (window->destroy_with_parent)
    {
      disconnect_parent_destroyed (window);
    }
  else
    {
      connect_parent_destroyed (window);
    }
  
  window->destroy_with_parent = setting;

  g_object_notify (G_OBJECT (window), "destroy_with_parent");
}

static GtkWindowGeometryInfo*
gtk_window_get_geometry_info (GtkWindow *window,
			      gboolean   create)
{
  GtkWindowGeometryInfo *info;

  info = window->geometry_info;
  if (!info && create)
    {
      info = g_new0 (GtkWindowGeometryInfo, 1);

      info->width = 0;
      info->height = 0;
      info->last.width = -1;
      info->last.height = -1;
      info->widget = NULL;
      info->mask = 0;
      info->may_shrink = FALSE;
      window->geometry_info = info;
    }

  return info;
}

/**
 * gtk_window_set_geometry_hints:
 * @window: a #GtkWindow
 * @geometry_widget: widget the geometry hints will be applied to
 * @geometry: struct containing geometry information
 * @geom_mask: mask indicating which struct fields should be paid attention to
 *
 * This function sets up hints about how a window can be resized by
 * the user.  You can set a minimum and maximum size; allowed resize
 * increments (e.g. for xterm, you can only resize by the size of a
 * character); aspect ratios; and more. See the #GdkGeometry struct.
 * 
 **/
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

/**
 * gtk_window_set_decorated:
 * @window: a #GtkWindow
 * @setting: %TRUE to decorate the window
 *
 * By default, windows are decorated with a title bar, resize
 * controls, etc.  Some window managers allow GTK+ to disable these
 * decorations, creating a borderless window. If you set the decorated
 * property to %FALSE using this function, GTK+ will do its best to
 * convince the window manager not to decorate the window.
 * 
 **/
void
gtk_window_set_decorated (GtkWindow *window,
                          gboolean   setting)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  setting = setting != FALSE;

  if (setting == window->decorated)
    return;

  if (GTK_WIDGET (window)->window)
    {
      if (window->decorated)
        gdk_window_set_decorations (GTK_WIDGET (window)->window,
                                    GDK_DECOR_ALL);
      else
        gdk_window_set_decorations (GTK_WIDGET (window)->window,
                                    0);
    }
}

/**
 * gtk_window_set_default_size:
 * @window: a #GtkWindow
 * @width: width in pixels, 0 to unset, or -1 to leave the width unchanged
 * @height: height in pixels, 0 to unset, or -1 to leave the height unchanged
 *
 * Sets the default size of a window. If the window's "natural" size
 * (its size request) is larger than the default, the default will be
 * ignored. So the default size is a minimum initial size.  Unlike
 * gtk_widget_set_usize(), which sets a size request for a widget and
 * thus would keep users from shrinking the window, this function only
 * sets the initial size, just as if the user had resized the window
 * themselves. Users can still shrink the window again as they
 * normally would. Setting a default size of 0 means to use the
 * "natural" default size (the size request of the window).
 *
 * For more control over a window's initial size and how resizing works,
 * investigate gtk_window_set_geometry_hints().
 *
 * A useful feature: if you set the "geometry widget" via
 * gtk_window_set_geometry_hints(), the default size specified by
 * gtk_window_set_default_size() will be the default size of that
 * widget, not of the entire window.
 * 
 **/
void       
gtk_window_set_default_size (GtkWindow   *window,
			     gint         width,
			     gint         height)
{
  GtkWindowGeometryInfo *info;

  g_return_if_fail (GTK_IS_WINDOW (window));

  info = gtk_window_get_geometry_info (window, TRUE);

  g_object_freeze_notify (G_OBJECT (window));
  if (width >= 0)
    {
      info->width = width;
      g_object_notify (G_OBJECT (window), "default_width");
      info->may_shrink = TRUE;
    }
  if (height >= 0)
    {
      info->height = height;
      g_object_notify (G_OBJECT (window), "default_height");
      info->may_shrink = TRUE;
    }
  g_object_thaw_notify (G_OBJECT (window));
  
  gtk_widget_queue_resize (GTK_WIDGET (window));
}
  
static void
gtk_window_destroy (GtkObject *object)
{
  GtkWindow *window;
  
  g_return_if_fail (GTK_IS_WINDOW (object));

  window = GTK_WINDOW (object);

  if (window->transient_parent)
    gtk_window_set_transient_for (window, NULL);

  if (window->has_user_ref_count)
    {
      window->has_user_ref_count = FALSE;
      gtk_widget_unref (GTK_WIDGET (window));
    }

  if (window->group)
    gtk_window_group_remove_window (window->group, window);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static gboolean
gtk_window_mnemonic_hash_remove (gpointer	key,
				 gpointer	value,
				 gpointer	user)
{
  GtkWindowMnemonic *mnemonic = key;
  GtkWindow *window = user;

  if (mnemonic->window == window)
    {
      if (mnemonic->targets)
	{
	  gchar *name = gtk_accelerator_name (mnemonic->keyval, 0);

	  g_warning ("mnemonic \"%s\" wasn't removed for widget (%p)",
		     name, mnemonic->targets->data);
	  g_free (name);
	}
      g_slist_free (mnemonic->targets);
      g_free (mnemonic);
      
      return TRUE;
    }
  return FALSE;
}

static void
gtk_window_finalize (GObject *object)
{
  GtkWindow *window;

  g_return_if_fail (GTK_IS_WINDOW (object));

  window = GTK_WINDOW (object);

  toplevel_list = g_slist_remove (toplevel_list, window);

  g_free (window->title);
  g_free (window->wmclass_name);
  g_free (window->wmclass_class);
  g_free (window->wm_role);

  g_hash_table_foreach_remove (mnemonic_hash_table,
			       gtk_window_mnemonic_hash_remove,
			       window);
  if (window->geometry_info)
    {
      if (window->geometry_info->widget)
	gtk_signal_disconnect_by_func (GTK_OBJECT (window->geometry_info->widget),
				       GTK_SIGNAL_FUNC (gtk_widget_destroyed),
				       &window->geometry_info->widget);
      g_free (window->geometry_info);
    }
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
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
      gboolean was_realized;
      
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
      
      was_realized = FALSE;
      if (!GTK_WIDGET_REALIZED (widget))
	{
	  gtk_widget_realize (widget);
	  was_realized = TRUE;;
	}

      /* Must be done after the windows are realized,
       * so that the decorations can be read
       */
      gtk_decorated_window_calculate_frame_size (window);
      
      if (!was_realized)
	gdk_window_resize (widget->window, width, height);
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

static void
gtk_window_map (GtkWidget *widget)
{
  GtkWindow *window;
  GdkWindow *toplevel;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  window = GTK_WINDOW (widget);

  if (window->bin.child &&
      GTK_WIDGET_VISIBLE (window->bin.child) &&
      !GTK_WIDGET_MAPPED (window->bin.child))
    gtk_widget_map (window->bin.child);

  if (window->frame)
    toplevel = window->frame;
  else
    toplevel = widget->window;
  
  if (window->maximize_initially)
    gdk_window_maximize (toplevel);
  else
    gdk_window_unmaximize (toplevel);
  
  if (window->stick_initially)
    gdk_window_stick (toplevel);
  else
    gdk_window_unstick (toplevel);
  
  if (window->iconify_initially)
    gdk_window_iconify (toplevel);
  else
    gdk_window_deiconify (toplevel);
  
  gdk_window_show (widget->window);

  if (window->frame)
    gdk_window_show (window->frame);
}

static void
gtk_window_unmap (GtkWidget *widget)
{
  GtkWindow *window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));

  window = GTK_WINDOW (widget);
  
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  if (window->frame)
    gdk_window_withdraw (window->frame);
  else 
    gdk_window_withdraw (widget->window);

  window->use_uposition = TRUE;
  window->resize_count = 0;
  window->handling_resize = FALSE;

}

static void
gtk_window_realize (GtkWidget *widget)
{
  GtkWindow *window;
  GdkWindow *parent_window;
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
    case GTK_WINDOW_POPUP:
      attributes.window_type = GDK_WINDOW_TEMP;
      break;
    default:
      g_warning (G_STRLOC": Unknown window type %d!", window->type);
      break;
    }
   
  attributes.title = window->title;
  attributes.wmclass_name = window->wmclass_name;
  attributes.wmclass_class = window->wmclass_class;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);

  if (window->has_frame)
    {
      attributes.width = widget->allocation.width + window->frame_left + window->frame_right;
      attributes.height = widget->allocation.height + window->frame_top + window->frame_bottom;
      attributes.event_mask = (GDK_EXPOSURE_MASK |
			       GDK_KEY_PRESS_MASK |
			       GDK_ENTER_NOTIFY_MASK |
			       GDK_LEAVE_NOTIFY_MASK |
			       GDK_FOCUS_CHANGE_MASK |
			       GDK_STRUCTURE_MASK |
			       GDK_BUTTON_MOTION_MASK |
			       GDK_POINTER_MOTION_HINT_MASK |
			       GDK_BUTTON_PRESS_MASK |
			       GDK_BUTTON_RELEASE_MASK);
      
      attributes_mask = GDK_WA_VISUAL | GDK_WA_COLORMAP;
      
      window->frame = gdk_window_new (NULL, &attributes, attributes_mask);
      gdk_window_set_user_data (window->frame, widget);
      
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.x = window->frame_left;
      attributes.y = window->frame_right;
    
      attributes_mask = GDK_WA_X | GDK_WA_Y;

      parent_window = window->frame;
    }
  else
    {
      attributes_mask = 0;
      parent_window = NULL;
    }
  
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_KEY_PRESS_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK |
			    GDK_FOCUS_CHANGE_MASK |
			    GDK_STRUCTURE_MASK);

  attributes_mask |= GDK_WA_VISUAL | GDK_WA_COLORMAP;
  attributes_mask |= (window->title ? GDK_WA_TITLE : 0);
  attributes_mask |= (window->wmclass_name ? GDK_WA_WMCLASS : 0);
  widget->window = gdk_window_new (parent_window, &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, window);
      
  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  if (window->frame)
    gtk_style_set_background (widget->style, window->frame, GTK_STATE_NORMAL);

  /* This is a bad hack to set the window background. */
  gtk_window_paint (widget, NULL);
  
  if (window->transient_parent &&
      GTK_WIDGET_REALIZED (window->transient_parent))
    gdk_window_set_transient_for (widget->window,
				  GTK_WIDGET (window->transient_parent)->window);

  if (window->wm_role)
    gdk_window_set_role (widget->window, window->wm_role);
  
  if (!window->decorated)
    gdk_window_set_decorations (widget->window, 0);

  gdk_window_set_type_hint (widget->window, window->type_hint);

  /* transient_for must be set to allow the modal hint */
  if (window->transient_parent && window->modal)
    gdk_window_set_modal_hint (widget->window, TRUE);
  else
    gdk_window_set_modal_hint (widget->window, FALSE);
}

static void
gtk_window_unrealize (GtkWidget *widget)
{
  GtkWindow *window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));

  window = GTK_WINDOW (widget);

  if (window->frame)
    {
      gdk_window_set_user_data (window->frame, NULL);
      gdk_window_destroy (window->frame);
      window->frame = NULL;
    }
  
  (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
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

  if (GTK_WIDGET_REALIZED (widget) && window->frame)
    {
      gdk_window_resize (window->frame,
			 allocation->width + window->frame_left + window->frame_right,
			 allocation->height + window->frame_top + window->frame_bottom);
    }
}

static gint
gtk_window_event (GtkWidget *widget, GdkEvent *event)
{
  GtkWindow *window;
  gboolean return_val;

  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  window = GTK_WINDOW (widget);

  if (window->frame && (event->any.window == window->frame))
    {
      if ((event->type != GDK_KEY_PRESS) &&
	  (event->type != GDK_KEY_RELEASE) &&
	  (event->type != GDK_FOCUS_CHANGE))
	{
	  gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "event");
	  return_val = FALSE;
	  gtk_signal_emit (GTK_OBJECT (widget), window_signals[FRAME_EVENT], event, &return_val);
	  return TRUE;
	}
      else
	{
	  g_object_unref (event->any.window);
	  event->any.window = g_object_ref (widget->window);
	}
    }

  return FALSE;
}

static gboolean
gtk_window_frame_event (GtkWidget *widget, GdkEvent *event)
{
  GdkEventConfigure *configure_event;
  GtkWindow *window = GTK_WINDOW (widget);
  GdkRectangle rect;

  switch (event->type)
    {
    case GDK_CONFIGURE:
      configure_event = (GdkEventConfigure *)event;
      
      /* Invalidate the decorations */
      rect.x = 0;
      rect.y = 0;
      rect.width = configure_event->width;
      rect.height = configure_event->height;
      
      gdk_window_invalidate_rect (window->frame, &rect, FALSE);

      /* Pass on the (modified) configure event */
      configure_event->width -= window->frame_left + window->frame_right;
      configure_event->height -= window->frame_top + window->frame_bottom;
      return gtk_window_configure_event (widget, configure_event);
      break;
    default:
      break;
    }
  return FALSE;
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
  gboolean handled;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  window = GTK_WINDOW (widget);

  handled = FALSE;
  
  if (window->focus_widget && window->focus_widget != widget &&
      GTK_WIDGET_IS_SENSITIVE (window->focus_widget))
    handled = gtk_widget_event (window->focus_widget, (GdkEvent*) event);

  if (!handled)
    handled = gtk_window_mnemonic_activate (window,
					    event->keyval,
					    event->state);

  if (!handled)
    handled = gtk_accel_groups_activate (GTK_OBJECT (window), event->keyval, event->state);

  /* Chain up, invokes binding set */
  if (!handled && GTK_WIDGET_CLASS (parent_class)->key_press_event)
    handled = GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);

  return handled;
}


static void
gtk_window_real_activate_default (GtkWindow *window)
{
  gtk_window_activate_default (window);
}

static void
gtk_window_real_activate_focus (GtkWindow *window)
{
  gtk_window_activate_focus (window);
}

static void
gtk_window_move_focus (GtkWindow       *window,
                       GtkDirectionType dir)
{
  gtk_widget_child_focus (GTK_WIDGET (window), dir);
  
  if (!GTK_CONTAINER (window)->focus_child)
    gtk_window_set_focus (window, NULL);
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
  GtkWindow *window = GTK_WINDOW (widget);
  GdkEventFocus fevent;

  /* It appears spurious focus in events can occur when
   *  the window is hidden. So we'll just check to see if
   *  the window is visible before actually handling the
   *  event
   */
  if (GTK_WIDGET_VISIBLE (widget))
    {
      window->has_focus = TRUE;
      
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
  GtkWindow *window = GTK_WINDOW (widget);
  GdkEventFocus fevent;

  window->has_focus = FALSE;
  
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
       * them now. Also, we have to invalidate cached icons in
       * icon sets so they get re-rendered.
       */
      GList *list, *toplevels;

      _gtk_icon_set_invalidate_caches ();
      
      toplevels = gtk_window_list_toplevels ();
      g_list_foreach (toplevels, (GFunc)g_object_ref, NULL);
      
      for (list = toplevels; list; list = list->next)
	{
	  gtk_widget_reset_rc_styles (list->data);
	  gtk_widget_unref (list->data);
	}
      g_list_free (toplevels);
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

static gboolean
gtk_window_focus (GtkWidget        *widget,
		  GtkDirectionType  direction)
{
  GtkBin *bin;
  GtkWindow *window;
  GtkContainer *container;
  GtkWidget *old_focus_child;
  GtkWidget *parent;

  container = GTK_CONTAINER (widget);
  window = GTK_WINDOW (widget);
  bin = GTK_BIN (widget);

  old_focus_child = container->focus_child;
  
  /* We need a special implementation here to deal properly with wrapping
   * around in the tab chain without the danger of going into an
   * infinite loop.
   */
  if (old_focus_child)
    {
      if (gtk_widget_child_focus (old_focus_child, direction))
	return TRUE;
    }

  if (window->focus_widget)
    {
      /* Wrapped off the end, clear the focus setting for the toplpevel */
      parent = window->focus_widget->parent;
      while (parent)
	{
	  gtk_container_set_focus_child (GTK_CONTAINER (parent), NULL);
	  parent = GTK_WIDGET (parent)->parent;
	}
      
      gtk_window_set_focus (GTK_WINDOW (container), NULL);
    }

  /* Now try to focus the first widget in the window */
  if (bin->child)
    {
      if (gtk_widget_child_focus (bin->child, direction))
        return TRUE;
    }

  return FALSE;
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

      if (window->has_focus)
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

      if (window->has_focus)
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
  gboolean need_reposition;
  gboolean default_size_changed = FALSE;
  gboolean hints_changed = FALSE;
  gboolean may_shrink = window->auto_shrink;

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
      may_shrink |= info->may_shrink;
      info->last.width = new_width;
      info->last.height = new_height;

      /* We need to force a reposition in this case
       */
      if (window->position == GTK_WIN_POS_CENTER_ALWAYS)
	window->use_uposition = TRUE;
    }
  info->may_shrink = FALSE;
  
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
      (!may_shrink &&
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
  need_reposition = gtk_window_compute_reposition (window, new_width, new_height, &x, &y);
  if (need_reposition && !(new_flags & GDK_HINT_POS))
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

      if ((default_size_changed || hints_changed) && (width != new_width || height != new_height))
	{
	  /* We could be here for two reasons
	   *  1) We coincidentally got a resize while handling
	   *     another resize.
	   *  2) Our computation of default_size_changed was completely
	   *     screwed up, probably because one of our children
	   *     is changed requisition during size allocation).
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
	  
	  if (need_reposition)
	    {
	      if (window->frame)
		gdk_window_move (window->frame, x - window->frame_left, y - window->frame_top);
	      else
		gdk_window_move (GTK_WIDGET (window)->window, x, y);
	    }

	  /* we have to preserve the values and flags that are used
	   * for computation of default_size_changed and hints_changed
	   */
	  info->last = saved_last_info;
	  
	  gtk_widget_queue_resize (widget);

	  return;
	}
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
      if (need_reposition)
	{
	  if (window->frame)
	    {
	      gdk_window_move_resize (window->frame,
				      x - window->frame_left, y - window->frame_top,
				      new_width + window->frame_left + window->frame_right,
				      new_height + window->frame_top + window->frame_bottom);
	      gdk_window_resize (GTK_WIDGET (window)->window, new_width, new_height);
	    }
	  else
	    gdk_window_move_resize (GTK_WIDGET (window)->window, x, y, new_width, new_height);
	}
      else
	{
	  if (window->frame)
	    gdk_window_resize (window->frame,
			       new_width + window->frame_left + window->frame_right,
			       new_height + window->frame_top + window->frame_bottom);
	  gdk_window_resize (GTK_WIDGET (window)->window, new_width, new_height);
	}
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
      if (need_reposition)
	{
	  if (window->frame)
	    gdk_window_move (window->frame, x - window->frame_left, y - window->frame_top);
	  else
	    gdk_window_move (widget->window, x, y);
	}

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

  if ((flags_a & GDK_HINT_WIN_GRAVITY) &&
      geometry_a->win_gravity != geometry_b->win_gravity)
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

void
_gtk_window_constrain_size (GtkWindow   *window,
			    gint         width,
			    gint         height,
			    gint        *new_width,
			    gint        *new_height)
{
  GtkWindowGeometryInfo *info;

  g_return_if_fail (GTK_IS_WINDOW (window));

  info = window->geometry_info;
  if (info)
    {
      GdkWindowHints flags = info->last.flags;
      GdkGeometry *geometry = &info->last.geometry;
      
      gtk_window_constrain_size (window,
				 geometry,
				 flags,
				 width,
				 height,
				 new_width,
				 new_height);
    }
}

static void 
gtk_window_constrain_size (GtkWindow   *window,
			   GdkGeometry *geometry,
			   guint        flags,
			   gint         width,
			   gint         height,
			   gint        *new_width,
			   gint        *new_height)
{
  gdk_window_constrain_size (geometry, flags, width, height,
                             new_width, new_height);
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
  
  aux_info = _gtk_widget_get_aux_info (widget, FALSE);
  if (aux_info && aux_info->x_set && aux_info->y_set)
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
      if (new_geometry->min_width < 0)
	new_geometry->min_width = requisition.width;
      else
	new_geometry->min_width += extra_width;

      if (new_geometry->min_height < 0)
	new_geometry->min_width = requisition.height;
      else
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
      if (new_geometry->max_width < 0)
	new_geometry->max_width = requisition.width;
      else
	new_geometry->max_width += extra_width;

      if (new_geometry->max_height < 0)
	new_geometry->max_width = requisition.height;
      else
	new_geometry->max_height += extra_height;
    }
  else if (!window->allow_grow)
    {
      *new_flags |= GDK_HINT_MAX_SIZE;
      
      new_geometry->max_width = requisition.width;
      new_geometry->max_height = requisition.height;
    }

  *new_flags |= GDK_HINT_WIN_GRAVITY;
  new_geometry->win_gravity = window->gravity;
}

/* Compute a new position for the window based on a new
 * size. *x and *y will be set to the new coordinates. Returns
 * TRUE if the window needs to be moved (and thus x and y got
 * assigned)
 */
static gint
gtk_window_compute_reposition (GtkWindow *window,
			       gint       new_width,
			       gint       new_height,
			       gint      *x,
			       gint      *y)
{
  GtkWidget *widget = GTK_WIDGET (window);
  GtkWindowPosition pos;
  GtkWidget *parent_widget;
  gboolean needs_move = FALSE;

  parent_widget = (GtkWidget*) window->transient_parent;
  
  pos = window->position;
  if (pos == GTK_WIN_POS_CENTER_ON_PARENT &&
      (parent_widget == NULL ||
       !GTK_WIDGET_MAPPED (parent_widget)))
    pos = GTK_WIN_POS_NONE;
  
  switch (pos)
  {
    case GTK_WIN_POS_CENTER:
    case GTK_WIN_POS_CENTER_ALWAYS:
      if (window->use_uposition)
	{
	  gint screen_width = gdk_screen_width ();
	  gint screen_height = gdk_screen_height ();
	  
	  *x = (screen_width - new_width) / 2;
	  *y = (screen_height - new_height) / 2;
	  needs_move = TRUE;
	}
      break;

    case GTK_WIN_POS_CENTER_ON_PARENT:
      if (window->use_uposition)
        {
          gint ox, oy;
	  gdk_window_get_origin (parent_widget->window,
				   &ox, &oy);
                                 
          *x = ox + (parent_widget->allocation.width - new_width) / 2;
          *y = oy + (parent_widget->allocation.height - new_height) / 2;
	  needs_move = TRUE;
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
	  needs_move = TRUE;
	}
      break;
    default:
      if (window->use_uposition)
	{
	  GtkWidgetAuxInfo *aux_info = _gtk_widget_get_aux_info (widget, FALSE);

	  if (aux_info && aux_info->x_set && aux_info->y_set)
	    {
	      *x = aux_info->x;
	      *y = aux_info->y;
	      needs_move = TRUE;
	    }
	}
      break;
    }

  if (needs_move)
    {
      GtkWidgetAuxInfo *aux_info = _gtk_widget_get_aux_info (widget, TRUE);

      /* we handle necessary window positioning by hand here,
       * so we can coalesce the window movement with possible
       * resizes to get only one configure event.
       */
      aux_info->x_set = TRUE;
      aux_info->y_set = TRUE;
      aux_info->x = *x;
      aux_info->y = *y;
      window->use_uposition = FALSE;
    }

  return needs_move;
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

/**
 * gtk_window_set_has_frame:
 * @window: a #GtkWindow
 * @setting: a boolean
 * 
 * If this function is called on a window with setting of TRUE, before
 * it is realized or showed, it will have a "frame" window around
 * widget-window, accessible in window->frame. Using the signal 
 * frame_event you can recieve all events targeted at the frame.
 * 
 * This function is used by the linux-fb port to implement managed
 * windows, but it could concievably be used by X-programs that
 * want to do their own window decorations.
 **/
void
gtk_window_set_has_frame (GtkWindow *window, 
			  gboolean   setting)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (!GTK_WIDGET_REALIZED (window));

  window->has_frame = setting != FALSE;
}

/**
 * gtk_window_set_frame_dimensions:
 * @window: a #GtkWindow that has a frame
 * @left: The width of the left border
 * @top: The height of the top border
 * @right: The width of the right border
 * @bottom: The height of the bottom border
 *
 * For windows with frames (see #gtk_window_set_has_frame) this function
 * can be used to change the size of the frame border.
 **/
void
gtk_window_set_frame_dimensions (GtkWindow *window, 
				 gint       left,
				 gint       top,
				 gint       right,
				 gint       bottom)
{
  GtkWidget *widget = GTK_WIDGET (window);

  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (window->frame_left == left &&
      window->frame_top == top &&
      window->frame_right == right && 
      window->frame_bottom == bottom)
    return;

  window->frame_left = left;
  window->frame_top = top;
  window->frame_right = right;
  window->frame_bottom = bottom;

  if (GTK_WIDGET_REALIZED (widget) && window->frame)
    {
      gint width = widget->allocation.width + left + right;
      gint height = widget->allocation.height + top + bottom;
      gdk_window_resize (window->frame, width, height);
      gtk_decorated_window_move_resize_window (window,
					       left, top,
					       widget->allocation.width,
					       widget->allocation.height);
    }
}

/**
 * gtk_window_present:
 * @window: a #GtkWindow
 *
 * Presents a window to the user. This may mean raising the window
 * in the stacking order, deiconifying it, moving it to the current
 * desktop, and/or giving it the keyboard focus, possibly dependent
 * on the user's platform, window manager, and preferences.
 *
 * If @window is hidden, this function calls gtk_widget_show()
 * as well.
 * 
 * This function should be used when the user tries to open a window
 * that's already open. Say for example the preferences dialog is
 * currently open, and the user chooses Preferences from the menu
 * a second time; use gtk_window_present() to move the already-open dialog
 * where the user can see it.
 * 
 **/
void
gtk_window_present (GtkWindow *window)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);

  if (GTK_WIDGET_VISIBLE (window))
    {
      g_assert (widget->window != NULL);
      
      gdk_window_show (widget->window);

      /* note that gdk_window_focus() will also move the window to
       * the current desktop, for WM spec compliant window managers.
       */
      gdk_window_focus (widget->window,
                        gtk_get_current_event_time ());
    }
  else
    {
      gtk_widget_show (widget);
    }
}

/**
 * gtk_window_iconify:
 * @window: a #GtkWindow
 *
 * Asks to iconify @window. Note that you shouldn't assume the window
 * is definitely iconified afterward, because other entities (e.g. the
 * user or window manager) could deiconify it again, or there may not
 * be a window manager in which case iconification isn't possible,
 * etc. But normally the window will end up iconified. Just don't write
 * code that crashes if not.
 *
 * It's permitted to call this function before showing a window,
 * in which case the window will be iconified before it ever appears
 * onscreen.
 *
 * You can track iconification via the "window_state_event" signal
 * on #GtkWidget.
 * 
 **/
void
gtk_window_iconify (GtkWindow *window)
{
  GtkWidget *widget;
  GdkWindow *toplevel;
  
  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);

  window->iconify_initially = TRUE;

  if (window->frame)
    toplevel = window->frame;
  else
    toplevel = widget->window;
  
  if (toplevel != NULL)
    gdk_window_iconify (toplevel);
}

/**
 * gtk_window_deiconify:
 * @window: a #GtkWindow
 *
 * Asks to deiconify @window. Note that you shouldn't assume the
 * window is definitely deiconified afterward, because other entities
 * (e.g. the user or window manager) could iconify it again before
 * your code which assumes deiconification gets to run.
 *
 * You can track iconification via the "window_state_event" signal
 * on #GtkWidget.
 **/
void
gtk_window_deiconify (GtkWindow *window)
{
  GtkWidget *widget;
  GdkWindow *toplevel;
  
  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);

  window->iconify_initially = FALSE;

  if (window->frame)
    toplevel = window->frame;
  else
    toplevel = widget->window;
  
  if (toplevel != NULL)
    gdk_window_deiconify (toplevel);
}

/**
 * gtk_window_stick:
 * @window: a #GtkWindow
 *
 * Asks to stick @window, which means that it will appear on all user
 * desktops. Note that you shouldn't assume the window is definitely
 * stuck afterward, because other entities (e.g. the user or window
 * manager) could unstick it again, and some window managers do not
 * support sticking windows. But normally the window will end up
 * stuck. Just don't write code that crashes if not.
 *
 * It's permitted to call this function before showing a window.
 *
 * You can track stickiness via the "window_state_event" signal
 * on #GtkWidget.
 * 
 **/
void
gtk_window_stick (GtkWindow *window)
{
  GtkWidget *widget;
  GdkWindow *toplevel;
  
  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);

  window->stick_initially = TRUE;

  if (window->frame)
    toplevel = window->frame;
  else
    toplevel = widget->window;
  
  if (toplevel != NULL)
    gdk_window_stick (toplevel);
}

/**
 * gtk_window_unstick:
 * @window: a #GtkWindow
 *
 * Asks to unstick @window, which means that it will appear on only
 * one of the user's desktops. Note that you shouldn't assume the
 * window is definitely unstuck afterward, because other entities
 * (e.g. the user or window manager) could stick it again. But
 * normally the window will end up stuck. Just don't write code that
 * crashes if not.
 *
 * You can track stickiness via the "window_state_event" signal
 * on #GtkWidget.
 * 
 **/
void
gtk_window_unstick (GtkWindow *window)
{
  GtkWidget *widget;
  GdkWindow *toplevel;
  
  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);

  window->stick_initially = FALSE;

  if (window->frame)
    toplevel = window->frame;
  else
    toplevel = widget->window;
  
  if (toplevel != NULL)
    gdk_window_unstick (toplevel);
}

/**
 * gtk_window_maximize:
 * @window: a #GtkWindow
 *
 * Asks to maximize @window, so that it becomes full-screen. Note that
 * you shouldn't assume the window is definitely maximized afterward,
 * because other entities (e.g. the user or window manager) could
 * unmaximize it again, and not all window managers support
 * maximization. But normally the window will end up maximized. Just
 * don't write code that crashes if not.
 *
 * It's permitted to call this function before showing a window,
 * in which case the window will be maximized when it appears onscreen
 * initially.
 *
 * You can track maximization via the "window_state_event" signal
 * on #GtkWidget.
 * 
 **/
void
gtk_window_maximize (GtkWindow *window)
{
  GtkWidget *widget;
  GdkWindow *toplevel;
  
  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);

  window->maximize_initially = TRUE;

  if (window->frame)
    toplevel = window->frame;
  else
    toplevel = widget->window;
  
  if (toplevel != NULL)
    gdk_window_maximize (toplevel);
}

/**
 * gtk_window_unmaximize:
 * @window: a #GtkWindow
 *
 * Asks to unmaximize @window. Note that you shouldn't assume the
 * window is definitely unmaximized afterward, because other entities
 * (e.g. the user or window manager) could maximize it again, and not
 * all window managers honor requests to unmaximize. But normally the
 * window will end up unmaximized. Just don't write code that crashes
 * if not.
 *
 * You can track maximization via the "window_state_event" signal
 * on #GtkWidget.
 * 
 **/
void
gtk_window_unmaximize (GtkWindow *window)
{
  GtkWidget *widget;
  GdkWindow *toplevel;
  
  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);

  window->maximize_initially = FALSE;

  if (window->frame)
    toplevel = window->frame;
  else
    toplevel = widget->window;
  
  if (toplevel != NULL)
    gdk_window_unmaximize (toplevel);
}

/**
 * gtk_window_set_resizeable:
 * @window: a #GtkWindow
 * @resizeable: %TRUE if the user can resize this window
 *
 * Sets whether the user can resize a window. Windows are user resizeable
 * by default.
 **/
void
gtk_window_set_resizeable (GtkWindow *window,
			   gboolean   resizeable)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  gtk_window_set_policy (window, FALSE, resizeable, FALSE);
}

/**
 * gtk_window_get_resizeable:
 * @window: a #GtkWindow
 *
 * Gets the value set by gtk_window_set_resizeable().
 *
 * Return value: %TRUE if the user can resize the window
 **/
gboolean
gtk_window_get_resizeable (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  /* allow_grow is most likely to indicate the semantic concept we
   * mean by "resizeable" (and will be a reliable indicator if
   * set_policy() hasn't been called)
   */
  return window->allow_grow;
}

/**
 * gtk_window_set_gravity:
 * @window: a #GtkWindow
 * @gravity: window gravity
 *
 * Window gravity defines the "reference point" to be used when
 * positioning or resizing a window. Calls to
 * gtk_widget_set_uposition() will position a different point on the
 * window depending on the window gravity. When the window changes size
 * the reference point determined by the window's gravity will stay in
 * a fixed location.
 *
 * See #GdkGravity for full details. To briefly summarize,
 * #GDK_GRAVITY_NORTH_WEST means that the reference point is the
 * northwest (top left) corner of the window
 * frame. #GDK_GRAVITY_SOUTH_EAST would be the bottom right corner of
 * the frame, and so on. If you want to position the window contents,
 * rather than the window manager's frame, #GDK_GRAVITY_STATIC moves
 * the reference point to the northwest corner of the #GtkWindow
 * itself.
 *
 * The default window gravity is #GDK_GRAVITY_NORTH_WEST.
 *
 **/
void
gtk_window_set_gravity (GtkWindow *window,
			GdkGravity gravity)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (gravity != window->gravity)
    {
      window->gravity = gravity;

      /* gtk_window_move_resize() will adapt gravity
       */
      gtk_widget_queue_resize (GTK_WIDGET (window));
    }
}

/**
 * gtk_window_get_gravity:
 * @window: a #GtkWindow
 *
 * Gets the value set by gtk_window_set_gravity().
 *
 * Return value: window gravity
 **/
GdkGravity
gtk_window_get_gravity (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), 0);

  return window->gravity;
}

/**
 * gtk_window_begin_resize_drag:
 * @window: a #GtkWindow
 * @button: mouse button that initiated the drag
 * @edge: position of the resize control
 * @root_x: X position where the user clicked to initiate the drag, in root window coordinates
 * @root_y: Y position where the user clicked to initiate the drag
 * @timestamp: timestamp from the click event that initiated the drag
 *
 * Starts resizing a window. This function is used if an application
 * has window resizing controls. When GDK can support it, the resize
 * will be done using the standard mechanism for the window manager or
 * windowing system. Otherwise, GDK will try to emulate window
 * resizing, potentially not all that well, depending on the windowing system.
 * 
 **/
void
gtk_window_begin_resize_drag  (GtkWindow    *window,
                               GdkWindowEdge edge,
                               gint          button,
                               gint          root_x,
                               gint          root_y,
                               guint32       timestamp)
{
  GtkWidget *widget;
  GdkWindow *toplevel;
  
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_WIDGET_VISIBLE (window));
  
  widget = GTK_WIDGET (window);
  
  if (window->frame)
    toplevel = window->frame;
  else
    toplevel = widget->window;
  
  gdk_window_begin_resize_drag (toplevel,
                                edge, button,
                                root_x, root_y,
                                timestamp);
}


/**
 * gtk_window_begin_move_drag:
 * @window: a #GtkWindow
 * @button: mouse button that initiated the drag
 * @root_x: X position where the user clicked to initiate the drag, in root window coordinates
 * @root_y: Y position where the user clicked to initiate the drag
 * @timestamp: timestamp from the click event that initiated the drag
 *
 * Starts moving a window. This function is used if an application
 * has window movement grips. When GDK can support it, the window movement
 * will be done using the standard mechanism for the window manager or
 * windowing system. Otherwise, GDK will try to emulate window
 * movement, potentially not all that well, depending on the windowing system.
 * 
 **/
void
gtk_window_begin_move_drag  (GtkWindow *window,
                             gint       button,
                             gint       root_x,
                             gint       root_y,
                             guint32    timestamp)
{
  GtkWidget *widget;
  GdkWindow *toplevel;
  
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_WIDGET_VISIBLE (window));
  
  widget = GTK_WIDGET (window);
  
  if (window->frame)
    toplevel = window->frame;
  else
    toplevel = widget->window;
  
  gdk_window_begin_move_drag (toplevel,
                              button,
                              root_x, root_y,
                              timestamp);
}


static void
gtk_window_group_class_init (GtkWindowGroupClass *klass)
{
}

GtkType
gtk_window_group_get_type (void)
{
  static GtkType window_group_type = 0;

  if (!window_group_type)
    {
      static const GTypeInfo window_group_info =
      {
	sizeof (GtkWindowGroupClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_window_group_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkWindowGroup),
	16,		/* n_preallocs */
	(GInstanceInitFunc) NULL,
      };

      window_group_type = g_type_register_static (G_TYPE_OBJECT, "GtkWindowGroup", &window_group_info, 0);
    }

  return window_group_type;
}

/**
 * gtk_window_group_new:
 * 
 * Create a new #GtkWindowGroup object. Grabs added with
 * gtk_window_grab_add() only affect windows within the
 * same #GtkWindowGroup
 * 
 * Return value: 
 **/
GtkWindowGroup *
gtk_window_group_new (void)
{
  return g_object_new (GTK_TYPE_WINDOW_GROUP, NULL);
}

static void
window_group_cleanup_grabs (GtkWindowGroup *group,
			    GtkWindow      *window)
{
  GSList *tmp_list;
  GSList *to_remove = NULL;

  tmp_list = group->grabs;
  while (tmp_list)
    {
      if (gtk_widget_get_toplevel (tmp_list->data) == (GtkWidget*) window)
	to_remove = g_slist_prepend (to_remove, g_object_ref (tmp_list->data));
      tmp_list = tmp_list->next;
    }

  while (to_remove)
    {
      gtk_grab_remove (to_remove->data);
      g_object_unref (to_remove->data);
      to_remove = g_slist_delete_link (to_remove, to_remove);
    }
}

/**
 * gtk_window_group_add_widget:
 * @window_group: a #GtkWindowGroup
 * @window: the #GtkWindow to add
 * 
 * Add a window to a #GtkWindowGroup. 
 **/
void
gtk_window_group_add_window (GtkWindowGroup *window_group,
			     GtkWindow      *window)
{
  g_return_if_fail (GTK_IS_WINDOW_GROUP (window_group));
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (window->group != window_group)
    {
      g_object_ref (window);
      g_object_ref (window_group);
      
      if (window->group)
	gtk_window_group_remove_window (window->group, window);
      else
	window_group_cleanup_grabs (_gtk_window_get_group (NULL), window);

      window->group = window_group;

      g_object_unref (window);
    }
}

/**
 * gtk_window_group_remove_window:
 * @window_group: a #GtkWindowGroup
 * @window: the #GtkWindow to remove
 * 
 * Removes a window from a #GtkWindowGroup.
 **/
void
gtk_window_group_remove_window (GtkWindowGroup *window_group,
				GtkWindow      *window)
{
  g_return_if_fail (GTK_IS_WINDOW_GROUP (window_group));
  g_return_if_fail (GTK_IS_WIDGET (window));
  g_return_if_fail (window->group == window_group);

  g_object_ref (window);

  window_group_cleanup_grabs (window_group, window);
  window->group = NULL;
  
  g_object_unref (G_OBJECT (window_group));
  g_object_unref (window);
}

/* Return the group for the window or the default group
 */
GtkWindowGroup *
_gtk_window_get_group (GtkWindow *window)
{
  if (window && window->group)
    return window->group;
  else
    {
      static GtkWindowGroup *default_group = NULL;

      if (!default_group)
	default_group = gtk_window_group_new ();

      return default_group;
    }
}
