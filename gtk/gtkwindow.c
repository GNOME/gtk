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

#include "gtkprivate.h"
#include "gtkrc.h"
#include "gtksignal.h"
#include "gtkwindow.h"
#include "gtkwindow-decorate.h"
#include "gtkbindings.h"
#include "gtkkeyhash.h"
#include "gtkmain.h"
#include "gtkiconfactory.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"

enum {
  SET_FOCUS,
  FRAME_EVENT,
  ACTIVATE_FOCUS,
  ACTIVATE_DEFAULT,
  MOVE_FOCUS,
  KEYS_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,

  /* Construct */
  PROP_TYPE,

  /* Normal Props */
  PROP_TITLE,
  PROP_ALLOW_SHRINK,
  PROP_ALLOW_GROW,
  PROP_RESIZABLE,
  PROP_MODAL,
  PROP_WIN_POS,
  PROP_DEFAULT_WIDTH,
  PROP_DEFAULT_HEIGHT,
  PROP_DESTROY_WITH_PARENT,
  PROP_ICON,
  
  LAST_ARG
};

typedef struct
{
  GList     *icon_list;
  GdkPixmap *icon_pixmap;
  GdkPixmap *icon_mask;
  guint      realized : 1;
  guint      using_default_icon : 1;
  guint      using_parent_icon : 1;
} GtkWindowIconInfo;

typedef struct {
  GdkGeometry    geometry; /* Last set of geometry hints we set */
  GdkWindowHints flags;
  GdkRectangle   configure_request;
} GtkWindowLastGeometryInfo;

struct _GtkWindowGeometryInfo
{
  /* Properties that the app has set on the window
   */
  GdkGeometry    geometry;	/* Geometry hints */
  GdkWindowHints mask;
  GtkWidget     *widget;	/* subwidget to which hints apply */
  /* from last gtk_window_resize () - if > 0, indicates that
   * we should resize to this size.
   */
  gint           resize_width;  
  gint           resize_height;

  /* From last gtk_window_move () prior to mapping -
   * only used if initial_pos_set
   */
  gint           initial_x;
  gint           initial_y;
  
  /* Default size - used only the FIRST time we map a window,
   * only if > 0.
   */
  gint           default_width; 
  gint           default_height;
  /* whether to use initial_x, initial_y */
  guint          initial_pos_set : 1;
  /* CENTER_ALWAYS or other position constraint changed since
   * we sent the last configure request.
   */
  guint          position_constraints_changed : 1;
  
  GtkWindowLastGeometryInfo last;
};

typedef struct {
  GtkWindow *window;
  guint keyval;

  GSList *targets;
} GtkWindowMnemonic;


static void gtk_window_class_init         (GtkWindowClass    *klass);
static void gtk_window_init               (GtkWindow         *window);
static void gtk_window_dispose            (GObject           *object);
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
static gboolean gtk_window_frame_event    (GtkWindow *window,
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
static void gtk_window_keys_changed          (GtkWindow         *window);
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

static GtkWindowGeometryInfo* gtk_window_get_geometry_info         (GtkWindow    *window,
                                                                    gboolean      create);

static void     gtk_window_move_resize               (GtkWindow    *window);
static gboolean gtk_window_compare_hints             (GdkGeometry  *geometry_a,
                                                      guint         flags_a,
                                                      GdkGeometry  *geometry_b,
                                                      guint         flags_b);
static void     gtk_window_constrain_size            (GtkWindow    *window,
                                                      GdkGeometry  *geometry,
                                                      guint         flags,
                                                      gint          width,
                                                      gint          height,
                                                      gint         *new_width,
                                                      gint         *new_height);
static void     gtk_window_constrain_position        (GtkWindow    *window,
                                                      gint          new_width,
                                                      gint          new_height,
                                                      gint         *x,
                                                      gint         *y);
static void     gtk_window_compute_hints             (GtkWindow    *window,
                                                      GdkGeometry  *new_geometry,
                                                      guint        *new_flags);
static void     gtk_window_compute_configure_request (GtkWindow    *window,
                                                      GdkRectangle *request,
                                                      GdkGeometry  *geometry,
                                                      guint        *flags);

static void     gtk_window_set_default_size_internal (GtkWindow    *window,
                                                      gboolean      change_width,
                                                      gint          width,
                                                      gboolean      change_height,
                                                      gint          height);

static void     gtk_window_realize_icon               (GtkWindow    *window);
static void     gtk_window_unrealize_icon             (GtkWindow    *window);

static void        gtk_window_notify_keys_changed (GtkWindow   *window);
static GtkKeyHash *gtk_window_get_key_hash        (GtkWindow   *window);
static void        gtk_window_free_key_hash       (GtkWindow   *window);

static GSList      *toplevel_list = NULL;
static GHashTable  *mnemonic_hash_table = NULL;
static GtkBinClass *parent_class = NULL;
static guint        window_signals[LAST_SIGNAL] = { 0 };
static GList       *default_icon_list = NULL;
/* FIXME need to be per-screen */
static GdkPixmap   *default_icon_pixmap = NULL;
static GdkPixmap   *default_icon_mask = NULL;

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
add_tab_bindings (GtkBindingSet    *binding_set,
		  GdkModifierType   modifiers,
		  GtkDirectionType  direction)
{
  gtk_binding_entry_add_signal (binding_set, GDK_Tab, modifiers,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Tab, modifiers,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
add_arrow_bindings (GtkBindingSet    *binding_set,
		    guint             keysym,
		    GtkDirectionType  direction)
{
  guint keypad_keysym = keysym - GDK_Left + GDK_KP_Left;
  
  gtk_binding_entry_add_signal (binding_set, keysym, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, keysym, GDK_CONTROL_MASK,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, keypad_keysym, 0,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, keypad_keysym, GDK_CONTROL_MASK,
                                "move_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
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

  mnemonic_hash_table = g_hash_table_new (mnemonic_hash, mnemonic_equal);

  gobject_class->dispose = gtk_window_dispose;
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
  klass->keys_changed = gtk_window_keys_changed;
  
  /* Construct */
  g_object_class_install_property (gobject_class,
                                   PROP_TYPE,
                                   g_param_spec_enum ("type",
						      _("Window Type"),
						      _("The type of the window"),
						      GTK_TYPE_WINDOW_TYPE,
						      GTK_WINDOW_TOPLEVEL,
						      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /* Regular Props */
  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        _("Window Title"),
                                                        _("The title of the window"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ALLOW_SHRINK,
                                   g_param_spec_boolean ("allow_shrink",
							 _("Allow Shrink"),
							 /* xgettext:no-c-format */
							 _("If TRUE, the window has no mimimum size. Setting this to TRUE is 99% of the time a bad idea."),
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
                                   PROP_RESIZABLE,
                                   g_param_spec_boolean ("resizable",
							 _("Resizable"),
							 _("If TRUE, users can resize the window."),
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
						     _("The default width of the window, used when initially showing the window."),
						     -1,
						     G_MAXINT,
						     -1,
						     G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_DEFAULT_HEIGHT,
                                   g_param_spec_int ("default_height",
						     _("Default Height"),
						     _("The default height of the window, used when initially showing the window."),
						     -1,
						     G_MAXINT,
						     -1,
						     G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_DESTROY_WITH_PARENT,
                                   g_param_spec_boolean ("destroy_with_parent",
							 _("Destroy with Parent"),
							 _("If this window should be destroyed when the parent is destroyed"),
                                                         FALSE,
							 G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ICON,
                                   g_param_spec_object ("icon",
                                                        _("Icon"),
                                                        _("Icon for this window"),
                                                        GDK_TYPE_PIXBUF,
                                                        G_PARAM_READWRITE));
  
  window_signals[SET_FOCUS] =
    g_signal_new ("set_focus",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkWindowClass, set_focus),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_WIDGET);
  
  window_signals[FRAME_EVENT] =
    g_signal_new ("frame_event",
                  G_TYPE_FROM_CLASS(object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET(GtkWindowClass, frame_event),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__BOXED,
                  G_TYPE_BOOLEAN, 1,
                  GDK_TYPE_EVENT);

  window_signals[ACTIVATE_FOCUS] =
    g_signal_new ("activate_focus",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  GTK_SIGNAL_OFFSET (GtkWindowClass, activate_focus),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  window_signals[ACTIVATE_DEFAULT] =
    g_signal_new ("activate_default",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  GTK_SIGNAL_OFFSET (GtkWindowClass, activate_default),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  window_signals[MOVE_FOCUS] =
    g_signal_new ("move_focus",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  GTK_SIGNAL_OFFSET (GtkWindowClass, move_focus),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_DIRECTION_TYPE);

  window_signals[KEYS_CHANGED] =
    g_signal_new ("keys_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  GTK_SIGNAL_OFFSET (GtkWindowClass, keys_changed),
                  NULL, NULL,
                  gtk_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  /*
   * Key bindings
   */

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set, GDK_space, 0,
                                "activate_focus", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Space, 0,
                                "activate_focus", 0);
  
  gtk_binding_entry_add_signal (binding_set, GDK_Return, 0,
                                "activate_default", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KP_Enter, 0,
                                "activate_default", 0);

  add_arrow_bindings (binding_set, GDK_Up, GTK_DIR_UP);
  add_arrow_bindings (binding_set, GDK_Down, GTK_DIR_DOWN);
  add_arrow_bindings (binding_set, GDK_Left, GTK_DIR_LEFT);
  add_arrow_bindings (binding_set, GDK_Right, GTK_DIR_RIGHT);

  add_tab_bindings (binding_set, 0, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
}

static void
gtk_window_init (GtkWindow *window)
{
  GdkColormap *colormap;
  
  GTK_WIDGET_UNSET_FLAGS (window, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (window, GTK_TOPLEVEL);

  GTK_PRIVATE_SET_FLAG (window, GTK_ANCHORED);

  gtk_container_set_resize_mode (GTK_CONTAINER (window), GTK_RESIZE_QUEUE);

  window->title = NULL;
  window->wmclass_name = g_strdup (g_get_prgname ());
  window->wmclass_class = g_strdup (gdk_get_program_class ());
  window->wm_role = NULL;
  window->geometry_info = NULL;
  window->type = GTK_WINDOW_TOPLEVEL;
  window->focus_widget = NULL;
  window->default_widget = NULL;
  window->configure_request_count = 0;
  window->allow_shrink = FALSE;
  window->allow_grow = TRUE;
  window->configure_notify_received = FALSE;
  window->position = GTK_WIN_POS_NONE;
  window->need_default_size = TRUE;
  window->need_default_position = TRUE;
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
  
  colormap = _gtk_widget_peek_colormap ();
  if (colormap)
    gtk_widget_set_colormap (GTK_WIDGET (window), colormap);
  
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
    case PROP_ALLOW_SHRINK:
      window->allow_shrink = g_value_get_boolean (value);
      gtk_widget_queue_resize (GTK_WIDGET (window));
      break;
    case PROP_ALLOW_GROW:
      window->allow_grow = g_value_get_boolean (value);
      gtk_widget_queue_resize (GTK_WIDGET (window));
      g_object_notify (G_OBJECT (window), "resizable");
      break;
    case PROP_RESIZABLE:
      window->allow_grow = g_value_get_boolean (value);
      gtk_widget_queue_resize (GTK_WIDGET (window));
      g_object_notify (G_OBJECT (window), "allow_grow");
      break;
    case PROP_MODAL:
      gtk_window_set_modal (window, g_value_get_boolean (value));
      break;
    case PROP_WIN_POS:
      gtk_window_set_position (window, g_value_get_enum (value));
      break;
    case PROP_DEFAULT_WIDTH:
      gtk_window_set_default_size_internal (window,
                                            TRUE, g_value_get_int (value),
                                            FALSE, -1);
      break;
    case PROP_DEFAULT_HEIGHT:
      gtk_window_set_default_size_internal (window,
                                            FALSE, -1,
                                            TRUE, g_value_get_int (value));
      break;
    case PROP_DESTROY_WITH_PARENT:
      gtk_window_set_destroy_with_parent (window, g_value_get_boolean (value));
      break;
    case PROP_ICON:
      gtk_window_set_icon (window,
                           g_value_get_object (value));
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
    case PROP_ALLOW_SHRINK:
      g_value_set_boolean (value, window->allow_shrink);
      break;
    case PROP_ALLOW_GROW:
      g_value_set_boolean (value, window->allow_grow);
      break;
    case PROP_RESIZABLE:
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
	g_value_set_int (value, -1);
      else
	g_value_set_int (value, info->default_width);
      break;
    case PROP_DEFAULT_HEIGHT:
      info = gtk_window_get_geometry_info (window, FALSE);
      if (!info)
	g_value_set_int (value, -1);
      else
	g_value_set_int (value, info->default_height);
      break;
    case PROP_DESTROY_WITH_PARENT:
      g_value_set_boolean (value, window->destroy_with_parent);
      break;
    case PROP_ICON:
      g_value_set_object (value, gtk_window_get_icon (window));
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
 * dialogs, though in some other toolkits dialogs are called "popups".
 * In GTK+, #GTK_WINDOW_POPUP means a pop-up menu or pop-up tooltip.
 * On X11, popup windows are not controlled by the <link
 * linkend="gtk-X11-arch">window manager</link>.
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
 * Sets the title of the #GtkWindow. The title of a window will be
 * displayed in its title bar; on the X Window System, the title bar
 * is rendered by the <link linkend="gtk-X11-arch">window
 * manager</link>, so exactly how the title appears to users may vary
 * according to a user's exact configuration. The title should help a
 * user distinguish this window from other windows they may have
 * open. A good title might include the application name and current
 * document filename, for example.
 * 
 **/
void
gtk_window_set_title (GtkWindow   *window,
		      const gchar *title)
{
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
 * gtk_window_get_title:
 * @window: a #GtkWindow
 *
 * Retrieves the title of the window. See gtk_window_set_title().
 *
 * Return value: the title of the window, or %NULL if none has
 *    been set explicitely. The returned string is owned by the widget
 *    and must not be modified or freed.
 **/
G_CONST_RETURN gchar *
gtk_window_get_title (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return window->title;
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
 * application, and GTK+ sets them to that value by default, so calling
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
 * This function is only useful on X11, not with other GTK+ targets.
 * 
 * In combination with the window title, the window role allows a
 * <link linkend="gtk-X11-arch">window manager</link> to identify "the
 * same" window when an application is restarted. So for example you
 * might set the "toolbox" role on your app's toolbox window, so that
 * when the user restarts their session, the window manager can put
 * the toolbox back in the same place.
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
 * gtk_window_get_role:
 * @window: a #GtkWindow
 *
 * Returns the role of the window. See gtk_window_set_role() for
 * further explanation.
 *
 * Return value: the role of the window if set, or %NULL. The
 *   returned is owned by the widget and must not be modified
 *   or freed.
 **/
G_CONST_RETURN gchar *
gtk_window_get_role (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return window->wm_role;
}

/**
 * gtk_window_set_focus:
 * @window: a #GtkWindow
 * @focus: widget to be the new focus widget, or %NULL to unset
 *   any focus widget for the toplevel window.
 *
 * If @focus is not the current focus widget, and is focusable, sets
 * it as the focus widget for the window. If @focus is %NULL, unsets
 * the focus widget for this window. To set the focus to a particular
 * widget in the toplevel, it is usually more convenient to use
 * gtk_widget_grab_focus() instead of this function.
 **/
void
gtk_window_set_focus (GtkWindow *window,
		      GtkWidget *focus)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  if (focus)
    {
      g_return_if_fail (GTK_IS_WIDGET (focus));
      g_return_if_fail (GTK_WIDGET_CAN_FOCUS (focus));
    }

  if (focus)
    gtk_widget_grab_focus (focus);
  else
    _gtk_window_internal_set_focus (window, NULL);
}

void
_gtk_window_internal_set_focus (GtkWindow *window,
				GtkWidget *focus)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  if ((window->focus_widget != focus) ||
      (focus && !GTK_WIDGET_HAS_FOCUS (focus)))
    gtk_signal_emit (GTK_OBJECT (window), window_signals[SET_FOCUS], focus);
}

/**
 * gtk_window_set_default:
 * @window: a #GtkWindow
 * @default_widget: widget to be the default, or %NULL to unset the
 *                  default widget for the toplevel.
 *
 * The default widget is the widget that's activated when the user
 * presses Enter in a dialog (for example). This function sets or
 * unsets the default widget for a #GtkWindow about. When setting
 * (rather than unsetting) the default widget it's generally easier to
 * call gtk_widget_grab_focus() on the widget. Before making a widget
 * the default widget, you must set the #GTK_CAN_DEFAULT flag on the
 * widget you'd like to make the default using GTK_WIDGET_SET_FLAGS().
 **/
void
gtk_window_set_default (GtkWindow *window,
			GtkWidget *default_widget)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (default_widget)
    g_return_if_fail (GTK_WIDGET_CAN_DEFAULT (default_widget));
  
  if (window->default_widget != default_widget)
    {
      GtkWidget *old_default_widget = NULL;
      
      if (default_widget)
	g_object_ref (default_widget);
      
      if (window->default_widget)
	{
	  old_default_widget = window->default_widget;
	  
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

      if (old_default_widget)
	g_object_notify (G_OBJECT (old_default_widget), "has_default");
      
      if (default_widget)
	{
	  g_object_notify (G_OBJECT (default_widget), "has_default");
	  g_object_unref (default_widget);
	}
    }
}

void
gtk_window_set_policy (GtkWindow *window,
		       gboolean   allow_shrink,
		       gboolean   allow_grow,
		       gboolean   auto_shrink)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  window->allow_shrink = (allow_shrink != FALSE);
  window->allow_grow = (allow_grow != FALSE);

  g_object_freeze_notify (G_OBJECT (window));
  g_object_notify (G_OBJECT (window), "allow_shrink");
  g_object_notify (G_OBJECT (window), "allow_grow");
  g_object_notify (G_OBJECT (window), "resizable");
  g_object_thaw_notify (G_OBJECT (window));
  
  gtk_widget_queue_resize (GTK_WIDGET (window));
}

static gboolean
handle_keys_changed (gpointer data)
{
  GtkWindow *window;

  GDK_THREADS_ENTER ();
  window = GTK_WINDOW (data);

  if (window->keys_changed_handler)
    {
      gtk_idle_remove (window->keys_changed_handler);
      window->keys_changed_handler = 0;
    }

  g_signal_emit (window, window_signals[KEYS_CHANGED], 0);
  GDK_THREADS_LEAVE ();
  
  return FALSE;
}

static void
gtk_window_notify_keys_changed (GtkWindow *window)
{
  if (!window->keys_changed_handler)
    window->keys_changed_handler = gtk_idle_add (handle_keys_changed, window);
}

/**
 * gtk_window_add_accel_group:
 * @window: window to attach accelerator group to
 * @accel_group: a #GtkAccelGroup
 *
 * Associate @accel_group with @window, such that calling
 * gtk_accel_groups_activate() on @window will activate accelerators
 * in @accel_group.
 **/
void
gtk_window_add_accel_group (GtkWindow     *window,
			    GtkAccelGroup *accel_group)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));

  _gtk_accel_group_attach (accel_group, G_OBJECT (window));
  g_signal_connect_object (accel_group, "accel_changed",
			   G_CALLBACK (gtk_window_notify_keys_changed),
			   window, G_CONNECT_SWAPPED);
}

/**
 * gtk_window_remove_accel_group:
 * @window: a #GtkWindow
 * @accel_group: a #GtkAccelGroup
 *
 * Reverses the effects of gtk_window_add_accel_group().
 **/
void
gtk_window_remove_accel_group (GtkWindow     *window,
			       GtkAccelGroup *accel_group)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));

  g_signal_handlers_disconnect_by_func (accel_group,
					G_CALLBACK (gtk_window_notify_keys_changed),
					window);
  _gtk_accel_group_detach (accel_group, G_OBJECT (window));
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
  gtk_window_notify_keys_changed (window);
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
  gtk_window_notify_keys_changed (window);
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

/**
 * gtk_window_set_mnemonic_modifier:
 * @window: a #GtkWindow
 * @modifier: the modifier mask used to activate
 *               mnemonics on this window.
 *
 * Sets the mnemonic modifier for this window. 
 **/
void
gtk_window_set_mnemonic_modifier (GtkWindow      *window,
				  GdkModifierType modifier)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail ((modifier & ~GDK_MODIFIER_MASK) == 0);

  window->mnemonic_modifier = modifier;
  gtk_window_notify_keys_changed (window);
}

/**
 * gtk_window_get_mnemonic_modifier:
 * @window: a #GtkWindow
 *
 * Returns the mnemonic modifier for this window. See
 * gtk_window_set_mnemonic_modifier().
 *
 * Return value: the modifier mask used to activate
 *               mnemonics on this window.
 **/
GdkModifierType
gtk_window_get_mnemonic_modifier (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), 0);

  return window->mnemonic_modifier;
}

/**
 * gtk_window_set_position:
 * @window: a #GtkWindow.
 * @position: a position constraint.
 *
 * Sets a position constraint for this window. If the old or new
 * constraint is %GTK_WIN_POS_CENTER_ALWAYS, this will also cause
 * the window to be repositioned to satisfy the new constraint. 
 **/
void
gtk_window_set_position (GtkWindow         *window,
			 GtkWindowPosition  position)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (position == GTK_WIN_POS_CENTER_ALWAYS ||
      window->position == GTK_WIN_POS_CENTER_ALWAYS)
    {
      GtkWindowGeometryInfo *info;

      info = gtk_window_get_geometry_info (window, TRUE);

      /* this flag causes us to re-request the CENTER_ALWAYS
       * constraint in gtk_window_move_resize(), see
       * comment in that function.
       */
      info->position_constraints_changed = TRUE;

      gtk_widget_queue_resize (GTK_WIDGET (window));
    }

  window->position = position;
  
  g_object_notify (G_OBJECT (window), "window_position");
}

gboolean 
gtk_window_activate_focus (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  if (window->focus_widget)
    {
      if (GTK_WIDGET_IS_SENSITIVE (window->focus_widget))
        gtk_widget_activate (window->focus_widget);
      return TRUE;
    }

  return FALSE;
}

/**
 * gtk_window_get_focus:
 * @window: a #GtkWindow
 * 
 * Retrieves the current focused widget within the window.
 * Note that this is the widget that would have the focus
 * if the toplevel window focused; if the toplevel window
 * is not focused then  <literal>GTK_WIDGET_HAS_FOCUS (widget)</literal> will
 * not be %TRUE for the widget. 
 * 
 * Return value: the currently focused widget.
 **/
GtkWidget *
gtk_window_get_focus (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return window->focus_widget;
}

gboolean
gtk_window_activate_default (GtkWindow *window)
{
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
 * parent; most <link linkend="gtk-X11-arch">window managers</link>
 * will then disallow lowering the dialog below the parent.
 * 
 * 
 **/
void
gtk_window_set_modal (GtkWindow *window,
		      gboolean   modal)
{
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
 * gtk_window_get_modal:
 * @window: a #GtkWindow
 * 
 * Returns whether the window is modal. See gtk_window_set_modal().
 *
 * Return value: %TRUE if the window is set to be modal and
 *               establishes a grab when shown
 **/
gboolean
gtk_window_get_modal (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->modal;
}

/**
 * gtk_window_list_toplevels:
 * 
 * Returns a list of all existing toplevel windows. The widgets
 * in the list are not individually referenced. If you want
 * to iterate through the list and perform actions involving
 * callbacks that might destroy the widgets, you <emphasis>must</emphasis> call
 * <literal>g_list_foreach (result, (GFunc)g_object_ref, NULL)</literal> first, and
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
  g_return_if_fail (GTK_IS_WINDOW (window));

  gtk_window_move (window, x, y);
}

static void
gtk_window_dispose (GObject *object)
{
  GtkWindow *window = GTK_WINDOW (object);

  gtk_window_set_focus (window, NULL);
  gtk_window_set_default (window, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (object);
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
 * window they were spawned from. This allows <link
 * linkend="gtk-X11-arch">window managers</link> to e.g. keep the
 * dialog on top of the main window, or center the dialog over the
 * main window. gtk_dialog_new_with_buttons() and other convenience
 * functions in GTK+ will sometimes call
 * gtk_window_set_transient_for() on your behalf.
 *
 * On Windows, this function will and put the child window
 * on top of the parent, much as the window manager would have
 * done on X.
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
 * gtk_window_get_transient_for:
 * @window: a #GtkWindow
 *
 * Fetches the transient parent for this window. See
 * gtk_window_set_transient_for().
 *
 * Return value: the transient parent for this window, or %NULL
 *    if no transient parent has been set.
 **/
GtkWindow *
gtk_window_get_transient_for (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return window->transient_parent;
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
 * gtk_window_get_type_hint:
 * @window: a #GtkWindow
 *
 * Gets the type hint for this window. See gtk_window_set_type_hint().
 *
 * Return value: the type hint for @window.
 **/
GdkWindowTypeHint
gtk_window_get_type_hint (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), GDK_WINDOW_TYPE_HINT_NORMAL);

  return window->type_hint;
}

/**
 * gtk_window_set_destroy_with_parent:
 * @window: a #GtkWindow
 * @setting: whether to destroy @window with its transient parent
 * 
 * If @setting is %TRUE, then destroying the transient parent of @window
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

/**
 * gtk_window_get_destroy_with_parent:
 * @window: a #GtkWindow
 * 
 * Returns whether the window will be destroyed with its transient parent. See
 * gtk_window_set_destroy_with_parent ().
 *
 * Return value: %TRUE if the window will be destroyed with its transient parent.
 **/
gboolean
gtk_window_get_destroy_with_parent (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->destroy_with_parent;
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

      info->default_width = -1;
      info->default_height = -1;
      info->resize_width = -1;
      info->resize_height = -1;
      info->initial_x = 0;
      info->initial_y = 0;
      info->initial_pos_set = FALSE;
      info->position_constraints_changed = FALSE;
      info->last.configure_request.x = 0;
      info->last.configure_request.y = 0;
      info->last.configure_request.width = -1;
      info->last.configure_request.height = -1;
      info->widget = NULL;
      info->mask = 0;
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

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (geometry_widget == NULL || GTK_IS_WIDGET (geometry_widget));

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

  /* We store gravity in window->gravity not in the hints. */
  info->mask = geom_mask & ~(GDK_HINT_WIN_GRAVITY);

  if (geom_mask & GDK_HINT_WIN_GRAVITY)
    {
      gtk_window_set_gravity (window, geometry->win_gravity);
    }
  
  gtk_widget_queue_resize (GTK_WIDGET (window));
}

/**
 * gtk_window_set_decorated:
 * @window: a #GtkWindow
 * @setting: %TRUE to decorate the window
 *
 * By default, windows are decorated with a title bar, resize
 * controls, etc.  Some <link linkend="gtk-X11-arch">window
 * managers</link> allow GTK+ to disable these decorations, creating a
 * borderless window. If you set the decorated property to %FALSE
 * using this function, GTK+ will do its best to convince the window
 * manager not to decorate the window.
 *
 * On Windows, this function always works, since there's no window manager
 * policy involved.
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

  window->decorated = setting;
  
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
 * gtk_window_get_decorated:
 * @window: a #GtkWindow
 *
 * Returns whether the window has been set to have decorations
 * such as a title bar via gtk_window_set_decorated().
 *
 * Return value: %TRUE if the window has been set to have decorations
 **/
gboolean
gtk_window_get_decorated (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), TRUE);

  return window->decorated;
}

static GtkWindowIconInfo*
get_icon_info (GtkWindow *window)
{
  return g_object_get_data (G_OBJECT (window),
                            "gtk-window-icon-info");
}
     
static GtkWindowIconInfo*
ensure_icon_info (GtkWindow *window)
{
  GtkWindowIconInfo *info;

  info = get_icon_info (window);
  
  if (info == NULL)
    {
      info = g_new0 (GtkWindowIconInfo, 1);
      g_object_set_data_full (G_OBJECT (window),
                              "gtk-window-icon-info",
                              info,
                              g_free);
    }

  return info;
}

static void
get_pixmap_and_mask (GtkWindowIconInfo  *parent_info,
                     gboolean            is_default_list,
                     GList              *icon_list,
                     GdkPixmap         **pmap_return,
                     GdkBitmap         **mask_return)
{
  GdkPixbuf *best_icon;
  GList *tmp_list;
  int best_size;

  *pmap_return = NULL;
  *mask_return = NULL;
  
  if (is_default_list &&
      default_icon_pixmap != NULL)
    {
      /* Use shared icon pixmap (eventually will be stored on the
       * GdkScreen)
       */
      if (default_icon_pixmap)
        g_object_ref (G_OBJECT (default_icon_pixmap));
      if (default_icon_mask)
        g_object_ref (G_OBJECT (default_icon_mask));
      
      *pmap_return = default_icon_pixmap;
      *mask_return = default_icon_mask;
    }
  else if (parent_info && parent_info->icon_pixmap)
    {
      if (parent_info->icon_pixmap)
        g_object_ref (G_OBJECT (parent_info->icon_pixmap));
      if (parent_info->icon_mask)
        g_object_ref (G_OBJECT (parent_info->icon_mask));
      
      *pmap_return = parent_info->icon_pixmap;
      *mask_return = parent_info->icon_mask;
    }
  else
    {
#define IDEAL_SIZE 48
  
      best_size = G_MAXINT;
      best_icon = NULL;
      tmp_list = icon_list;
      while (tmp_list != NULL)
        {
          GdkPixbuf *pixbuf = tmp_list->data;
          int this;
      
          /* average width and height - if someone passes in a rectangular
           * icon they deserve what they get.
           */
          this = gdk_pixbuf_get_width (pixbuf) + gdk_pixbuf_get_height (pixbuf);
          this /= 2;
      
          if (best_icon == NULL)
            {
              best_icon = pixbuf;
              best_size = this;
            }
          else
            {
              /* icon is better if it's 32 pixels or larger, and closer to
               * the ideal size than the current best.
               */
              if (this >= 32 &&
                  (ABS (best_size - IDEAL_SIZE) <
                   ABS (this - IDEAL_SIZE)))
                {
                  best_icon = pixbuf;
                  best_size = this;
                }
            }

          tmp_list = tmp_list->next;
        }

      if (best_icon)
        gdk_pixbuf_render_pixmap_and_mask_for_colormap (best_icon,
							gdk_colormap_get_system (),
							pmap_return,
							mask_return,
							128);

      /* Save pmap/mask for others to use if appropriate */
      if (parent_info)
        {
          parent_info->icon_pixmap = *pmap_return;
          parent_info->icon_mask = *mask_return;

          if (parent_info->icon_pixmap)
            g_object_ref (G_OBJECT (parent_info->icon_pixmap));
          if (parent_info->icon_mask)
            g_object_ref (G_OBJECT (parent_info->icon_mask));
        }
      else if (is_default_list)
        {
          default_icon_pixmap = *pmap_return;
          default_icon_mask = *mask_return;

          if (default_icon_pixmap)
            g_object_add_weak_pointer (G_OBJECT (default_icon_pixmap),
                                       (gpointer*)&default_icon_pixmap);
          if (default_icon_mask)
            g_object_add_weak_pointer (G_OBJECT (default_icon_mask),
                                       (gpointer*)&default_icon_mask);
        }
    }
}

static void
gtk_window_realize_icon (GtkWindow *window)
{
  GtkWidget *widget;
  GtkWindowIconInfo *info;
  GList *icon_list;
  
  widget = GTK_WIDGET (window);

  g_return_if_fail (widget->window != NULL);

  /* no point setting an icon on override-redirect */
  if (window->type == GTK_WINDOW_POPUP)
    return;

  icon_list = NULL;
  
  info = ensure_icon_info (window);

  if (info->realized)
    return;

  g_return_if_fail (info->icon_pixmap == NULL);
  g_return_if_fail (info->icon_mask == NULL);
  
  info->using_default_icon = FALSE;
  info->using_parent_icon = FALSE;
  
  icon_list = info->icon_list;
  
  /* Inherit from transient parent */
  if (icon_list == NULL && window->transient_parent)
    {
      icon_list = ensure_icon_info (window->transient_parent)->icon_list;
      if (icon_list)
        info->using_parent_icon = TRUE;
    }      

  /* Inherit from default */
  if (icon_list == NULL)
    {
      icon_list = default_icon_list;
      if (icon_list)
        info->using_default_icon = TRUE;
    }
  
  gdk_window_set_icon_list (widget->window, icon_list);

  get_pixmap_and_mask (info->using_parent_icon ?
                       ensure_icon_info (window->transient_parent) : NULL,
                       info->using_default_icon,
                       icon_list,
                       &info->icon_pixmap,
                       &info->icon_mask);
  
  /* This is a slight ICCCM violation since it's a color pixmap not
   * a bitmap, but everyone does it.
   */
  gdk_window_set_icon (widget->window,
                       NULL,
                       info->icon_pixmap,
                       info->icon_mask);

  info->realized = TRUE;
}

static void
gtk_window_unrealize_icon (GtkWindow *window)
{
  GtkWindowIconInfo *info;
  GtkWidget *widget;

  widget = GTK_WIDGET (window);
  
  info = get_icon_info (window);

  if (info == NULL)
    return;
  
  if (info->icon_pixmap)
    g_object_unref (G_OBJECT (info->icon_pixmap));

  if (info->icon_mask)
    g_object_unref (G_OBJECT (info->icon_mask));

  info->icon_pixmap = NULL;
  info->icon_mask = NULL;

  /* We don't clear the properties on the window, just figure the
   * window is going away.
   */

  info->realized = FALSE;
}

/**
 * gtk_window_set_icon_list:
 * @window: a #GtkWindow
 * @list: list of #GdkPixbuf
 *
 * Sets up the icon representing a #GtkWindow. The icon is used when
 * the window is minimized (also known as iconified).  Some window
 * managers or desktop environments may also place it in the window
 * frame, or display it in other contexts.
 *
 * gtk_window_set_icon_list() allows you to pass in the same icon in
 * several hand-drawn sizes. The list should contain the natural sizes
 * your icon is available in; that is, don't scale the image before
 * passing it to GTK+. Scaling is postponed until the last minute,
 * when the desired final size is known, to allow best quality.
 *
 * By passing several sizes, you may improve the final image quality
 * of the icon, by reducing or eliminating automatic image scaling.
 *
 * Recommended sizes to provide: 16x16, 32x32, 48x48 at minimum, and
 * larger images (64x64, 128x128) if you have them.
 *
 * See also gtk_window_set_default_icon_list() to set the icon
 * for all windows in your application in one go.
 *
 * Note that transient windows (those who have been set transient for another
 * window using gtk_window_set_transient_for()) will inherit their
 * icon from their transient parent. So there's no need to explicitly
 * set the icon on transient windows.
 **/
void
gtk_window_set_icon_list (GtkWindow  *window,
                          GList      *list)
{
  GtkWindowIconInfo *info;

  g_return_if_fail (GTK_IS_WINDOW (window));

  info = ensure_icon_info (window);

  if (info->icon_list == list) /* check for NULL mostly */
    return;

  g_list_foreach (info->icon_list,
                  (GFunc) g_object_unref, NULL);

  g_list_free (info->icon_list);

  info->icon_list = g_list_copy (list);
  g_list_foreach (info->icon_list,
                  (GFunc) g_object_ref, NULL);

  g_object_notify (G_OBJECT (window), "icon");
  
  gtk_window_unrealize_icon (window);
  
  if (GTK_WIDGET_REALIZED (window))
    gtk_window_realize_icon (window);

  /* We could try to update our transient children, but I don't think
   * it's really worth it. If we did it, the best way would probably
   * be to have children connect to notify::icon_list
   */
}

/**
 * gtk_window_get_icon_list:
 * @window: a #GtkWindow
 * 
 * Retrieves the list of icons set by gtk_window_set_icon_list().
 * The list is copied, but the reference count on each
 * member won't be incremented.
 * 
 * Return value: copy of window's icon list
 **/
GList*
gtk_window_get_icon_list (GtkWindow  *window)
{
  GtkWindowIconInfo *info;
  
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  info = get_icon_info (window);

  if (info)
    return g_list_copy (info->icon_list);
  else
    return NULL;  
}

/**
 * gtk_window_set_icon:
 * @window: a #GtkWindow
 * @icon: icon image, or %NULL
 * 
 * Sets up the icon representing a #GtkWindow. This icon is used when
 * the window is minimized (also known as iconified).  Some window
 * managers or desktop environments may also place it in the window
 * frame, or display it in other contexts.
 *
 * The icon should be provided in whatever size it was naturally
 * drawn; that is, don't scale the image before passing it to
 * GTK+. Scaling is postponed until the last minute, when the desired
 * final size is known, to allow best quality.
 *
 * If you have your icon hand-drawn in multiple sizes, use
 * gtk_window_set_icon_list(). Then the best size will be used.
 *
 * This function is equivalent to calling gtk_window_set_icon_list()
 * with a 1-element list.
 *
 * See also gtk_window_set_default_icon_list() to set the icon
 * for all windows in your application in one go.
 **/
void
gtk_window_set_icon (GtkWindow  *window,
                     GdkPixbuf  *icon)
{
  GList *list;
  
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (icon == NULL || GDK_IS_PIXBUF (icon));

  list = NULL;
  list = g_list_append (list, icon);
  gtk_window_set_icon_list (window, list);
  g_list_free (list);  
}

/**
 * gtk_window_get_icon:
 * @window: a #GtkWindow
 * 
 * Gets the value set by gtk_window_set_icon() (or if you've
 * called gtk_window_set_icon_list(), gets the first icon in
 * the icon list).
 * 
 * Return value: icon for window
 **/
GdkPixbuf*
gtk_window_get_icon (GtkWindow  *window)
{
  GtkWindowIconInfo *info;

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  info = get_icon_info (window);
  if (info && info->icon_list)
    return GDK_PIXBUF (info->icon_list->data);
  else
    return NULL;
}

/**
 * gtk_window_set_default_icon_list:
 * @list: a list of #GdkPixbuf
 *
 * Sets an icon list to be used as fallback for windows that haven't
 * had gtk_window_set_icon_list() called on them to set up a
 * window-specific icon list. This function allows you to set up the
 * icon for all windows in your app at once.
 *
 * See gtk_window_set_icon_list() for more details.
 * 
 **/
void
gtk_window_set_default_icon_list (GList *list)
{
  GList *toplevels;
  GList *tmp_list;
  if (list == default_icon_list)
    return;

  if (default_icon_pixmap)
    g_object_unref (G_OBJECT (default_icon_pixmap));
  if (default_icon_mask)
    g_object_unref (G_OBJECT (default_icon_mask));

  default_icon_pixmap = NULL;
  default_icon_mask = NULL;
  
  g_list_foreach (default_icon_list,
                  (GFunc) g_object_unref, NULL);

  g_list_free (default_icon_list);

  default_icon_list = g_list_copy (list);
  g_list_foreach (default_icon_list,
                  (GFunc) g_object_ref, NULL);
  
  /* Update all toplevels */
  toplevels = gtk_window_list_toplevels ();
  tmp_list = toplevels;
  while (tmp_list != NULL)
    {
      GtkWindowIconInfo *info;
      GtkWindow *w = tmp_list->data;
      
      info = get_icon_info (w);
      if (info && info->using_default_icon)
        {
          gtk_window_unrealize_icon (w);
          if (GTK_WIDGET_REALIZED (w))
            gtk_window_realize_icon (w);
        }

      tmp_list = tmp_list->next;
    }
  g_list_free (toplevels);
}

/**
 * gtk_window_get_default_icon_list:
 * 
 * Gets the value set by gtk_window_set_default_icon_list().
 * The list is a copy and should be freed with g_list_free(),
 * but the pixbufs in the list have not had their reference count
 * incremented.
 * 
 * Return value: copy of default icon list 
 **/
GList*
gtk_window_get_default_icon_list (void)
{
  return g_list_copy (default_icon_list);
}

static void
gtk_window_set_default_size_internal (GtkWindow    *window,
                                      gboolean      change_width,
                                      gint          width,
                                      gboolean      change_height,
                                      gint          height)
{
  GtkWindowGeometryInfo *info;

  g_return_if_fail (change_width == FALSE || width >= -1);
  g_return_if_fail (change_height == FALSE || height >= -1);

  info = gtk_window_get_geometry_info (window, TRUE);

  g_object_freeze_notify (G_OBJECT (window));

  if (change_width)
    {
      if (width == 0)
        width = 1;

      if (width < 0)
        width = -1;

      info->default_width = width;

      g_object_notify (G_OBJECT (window), "default_width");
    }

  if (change_height)
    {
      if (height == 0)
        height = 1;

      if (height < 0)
        height = -1;

      info->default_height = height;
      
      g_object_notify (G_OBJECT (window), "default_height");
    }
  
  g_object_thaw_notify (G_OBJECT (window));
  
  gtk_widget_queue_resize (GTK_WIDGET (window));
}

/**
 * gtk_window_set_default_size:
 * @window: a #GtkWindow
 * @width: width in pixels, or -1 to unset the default width
 * @height: height in pixels, or -1 to unset the default height
 *
 * Sets the default size of a window. If the window's "natural" size
 * (its size request) is larger than the default, the default will be
 * ignored. More generally, if the default size does not obey the
 * geometry hints for the window (gtk_window_set_geometry_hints() can
 * be used to set these explicitly), the default size will be clamped
 * to the nearest permitted size.
 * 
 * Unlike gtk_widget_set_size_request(), which sets a size request for
 * a widget and thus would keep users from shrinking the window, this
 * function only sets the initial size, just as if the user had
 * resized the window themselves. Users can still shrink the window
 * again as they normally would. Setting a default size of -1 means to
 * use the "natural" default size (the size request of the window).
 *
 * For more control over a window's initial size and how resizing works,
 * investigate gtk_window_set_geometry_hints().
 *
 * A useful feature: if you set the "geometry widget" via
 * gtk_window_set_geometry_hints(), the default size specified by
 * gtk_window_set_default_size() will be the default size of that
 * widget, not of the entire window.
 *
 * For some uses, gtk_window_resize() is a more appropriate function.
 * gtk_window_resize() changes the current size of the window, rather
 * than the size to be used on initial display. gtk_window_resize() always
 * affects the window itself, not the geometry widget.
 *
 * The default size of a window only affects the first time a window is
 * shown; if a window is hidden and re-shown, it will remember the size
 * it had prior to hiding, rather than using the default size.
 *
 * Windows can't actually be 0x0 in size, they must be at least 1x1, but
 * passing 0 for @width and @height is OK, resulting in a 1x1 default size.
 **/
void       
gtk_window_set_default_size (GtkWindow   *window,
			     gint         width,
			     gint         height)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (width >= -1);
  g_return_if_fail (height >= -1);

  gtk_window_set_default_size_internal (window, TRUE, width, TRUE, height);
}

/**
 * gtk_window_get_default_size:
 * @window: a #GtkWindow
 * @width: location to store the default width, or %NULL
 * @height: location to store the default height, or %NULL
 *
 * Gets the default size of the window. A value of -1 for the width or
 * height indicates that a default size has not been explicitly set
 * for that dimension, so the "natural" size of the window will be
 * used.
 * 
 **/
void
gtk_window_get_default_size (GtkWindow *window,
			     gint      *width,
			     gint      *height)
{
  GtkWindowGeometryInfo *info;

  g_return_if_fail (GTK_IS_WINDOW (window));

  info = gtk_window_get_geometry_info (window, FALSE);

  if (width)
    *width = info->default_width;

  if (height)
    *height = info->default_height;
}

/**
 * gtk_window_resize:
 * @window: a #GtkWindow
 * @width: width to resize the window to
 * @height: height to resize the window to
 *
 * Resizes the window as if the user had done so, obeying geometry
 * constraints. The default geometry constraint is that windows may
 * not be smaller than their size request; to override this
 * constraint, call gtk_widget_set_size_request() to set the window's
 * request to a smaller value.
 *
 * If gtk_window_resize() is called before showing a window for the
 * first time, it overrides any default size set with
 * gtk_window_set_default_size().
 *
 * Windows may not be resized smaller than 1 by 1 pixels.
 * 
 **/
void
gtk_window_resize (GtkWindow *window,
                   gint       width,
                   gint       height)
{
  GtkWindowGeometryInfo *info;
  
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  info = gtk_window_get_geometry_info (window, TRUE);

  info->resize_width = width;
  info->resize_height = height;

  gtk_widget_queue_resize (GTK_WIDGET (window));
}

/**
 * gtk_window_get_size:
 * @window: a #GtkWindow
 * @width: return location for width, or %NULL
 * @height: return location for height, or %NULL
 *
 * Obtains the current size of @window. If @window is not onscreen,
 * it returns the size GTK+ will suggest to the <link
 * linkend="gtk-X11-arch">window manager</link> for the initial window
 * size (but this is not reliably the same as the size the window
 * manager will actually select). The size obtained by
 * gtk_window_get_size() is the last size received in a
 * #GdkEventConfigure, that is, GTK+ uses its locally-stored size,
 * rather than querying the X server for the size. As a result, if you
 * call gtk_window_resize() then immediately call
 * gtk_window_get_size(), the size won't have taken effect yet. After
 * the window manager processes the resize request, GTK+ receives
 * notification that the size has changed via a configure event, and
 * the size of the window gets updated.
 *
 * Note 1: Nearly any use of this function creates a race condition,
 * because the size of the window may change between the time that you
 * get the size and the time that you perform some action assuming
 * that size is the current size. To avoid race conditions, connect to
 * "configure_event" on the window and adjust your size-dependent
 * state to match the size delivered in the #GdkEventConfigure.
 *
 * Note 2: The returned size does <emphasis>not</emphasis> include the
 * size of the window manager decorations (aka the window frame or
 * border). Those are not drawn by GTK+ and GTK+ has no reliable
 * method of determining their size.
 *
 * Note 3: If you are getting a window size in order to position
 * the window onscreen, there may be a better way. The preferred
 * way is to simply set the window's semantic type with
 * gtk_window_set_type_hint(), which allows the window manager to
 * e.g. center dialogs. Also, if you set the transient parent of
 * dialogs with gtk_window_set_transient_for() window managers
 * will often center the dialog over its parent window. It's
 * much preferred to let the window manager handle these
 * things rather than doing it yourself, because all apps will
 * behave consistently and according to user prefs if the window
 * manager handles it. Also, the window manager can take the size
 * of the window decorations/border into account, while your
 * application cannot.
 *
 * In any case, if you insist on application-specified window
 * positioning, there's <emphasis>still</emphasis> a better way than
 * doing it yourself - gtk_window_set_position() will frequently
 * handle the details for you.
 * 
 **/
void
gtk_window_get_size (GtkWindow *window,
                     gint      *width,
                     gint      *height)
{
  gint w, h;
  GtkWidget *widget;
  
  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);
  
  if (width == NULL && height == NULL)
    return;

  if (GTK_WIDGET_MAPPED (window))
    {
      gdk_drawable_get_size (GTK_WIDGET (window)->window,
                             &w, &h);
    }
  else
    {
      GdkRectangle configure_request;

      gtk_window_compute_configure_request (window,
                                            &configure_request,
                                            NULL, NULL);

      w = configure_request.width;
      h = configure_request.height;
    }
  
  if (width)
    *width = w;
  if (height)
    *height = h;
}

/**
 * gtk_window_move:
 * @window: a #GtkWindow
 * @x: X coordinate to move window to
 * @y: Y coordinate to move window to
 *
 * Asks the <link linkend="gtk-X11-arch">window manager</link> to move
 * @window to the given position.  Window managers are free to ignore
 * this; most window managers ignore requests for initial window
 * positions (instead using a user-defined placement algorithm) and
 * honor requests after the window has already been shown.
 *
 * Note: the position is the position of the gravity-determined
 * reference point for the window. The gravity determines two things:
 * first, the location of the reference point in root window
 * coordinates; and second, which point on the window is positioned at
 * the reference point.
 *
 * By default the gravity is #GDK_GRAVITY_NORTH_WEST, so the reference
 * point is simply the @x, @y supplied to gtk_window_move(). The
 * top-left corner of the window decorations (aka window frame or
 * border) will be placed at @x, @y.  Therefore, to position a window
 * at the top left of the screen, you want to use the default gravity
 * (which is #GDK_GRAVITY_NORTH_WEST) and move the window to 0,0.
 *
 * To position a window at the bottom right corner of the screen, you
 * would set #GDK_GRAVITY_SOUTH_EAST, which means that the reference
 * point is at @x + the window width and @y + the window height, and
 * the bottom-right corner of the window border will be placed at that
 * reference point. So, to place a window in the bottom right corner
 * you would first set gravity to south east, then write:
 * <literal>gtk_window_move (window, gdk_screen_width () - window_width,
 * gdk_screen_height () - window_height)</literal>.
 *
 * The extended window manager hints specification at <ulink 
 * url="http://www.freedesktop.org/standards/wm-spec.html"
 * >http://www.freedesktop.org/standards/wm-spec.html</ulink> has a 
 * nice table of gravities in the "implementation notes" section.
 *
 * The gtk_window_get_position() documentation may also be relevant.
 * 
 **/
void
gtk_window_move (GtkWindow *window,
                 gint       x,
                 gint       y)
{
  GtkWindowGeometryInfo *info;
  GtkWidget *widget;
  
  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);

  info = gtk_window_get_geometry_info (window, TRUE);  
  
  if (GTK_WIDGET_MAPPED (window))
    {
      /* we have now sent a request with this position
       * with currently-active constraints, so toggle flag.
       */
      info->position_constraints_changed = FALSE;

      /* we only constrain if mapped - if not mapped,
       * then gtk_window_compute_configure_request()
       * will apply the constraints later, and we
       * don't want to lose information about
       * what position the user set before then.
       * i.e. if you do a move() then turn off POS_CENTER
       * then show the window, your move() will work.
       */
      gtk_window_constrain_position (window,
                                     widget->allocation.width,
                                     widget->allocation.height,
                                     &x, &y);
      
      /* Note that this request doesn't go through our standard request
       * framework, e.g. doesn't increment configure_request_count,
       * doesn't set info->last, etc.; that's because
       * we don't save the info needed to arrive at this same request
       * again.
       *
       * To gtk_window_move_resize(), this will end up looking exactly
       * the same as the position being changed by the window
       * manager.
       */
      
      /* FIXME are we handling gravity properly for framed windows? */
      if (window->frame)
        gdk_window_move (window->frame,
                         x - window->frame_left,
                         y - window->frame_top);
      else
        gdk_window_move (GTK_WIDGET (window)->window,
                         x, y);
    }
  else
    {
      /* Save this position to apply on mapping */
      info->initial_x = x;
      info->initial_y = y;
      info->initial_pos_set = TRUE;
    }
}

/**
 * gtk_window_get_position:
 * @window: a #GtkWindow
 * @root_x: return location for X coordinate of gravity-determined reference p\oint
 * @root_y: return location for Y coordinate of gravity-determined reference p\oint
 *
 * This function returns the position you need to pass to
 * gtk_window_move() to keep @window in its current position.  This
 * means that the meaning of the returned value varies with window
 * gravity. See gtk_window_move() for more details.
 * 
 * If you haven't changed the window gravity, its gravity will be
 * #GDK_GRAVITY_NORTH_WEST. This means that gtk_window_get_position()
 * gets the position of the top-left corner of the window manager
 * frame for the window. gtk_window_move() sets the position of this
 * same top-left corner.
 *
 * gtk_window_get_position() is not 100% reliable because the X Window System
 * does not specify a way to obtain the geometry of the
 * decorations placed on a window by the window manager.
 * Thus GTK+ is using a "best guess" that works with most
 * window managers.
 *
 * Moreover, nearly all window managers are historically broken with
 * respect to their handling of window gravity. So moving a window to
 * its current position as returned by gtk_window_get_position() tends
 * to result in moving the window slightly. Window managers are
 * slowly getting better over time.
 *
 * If a window has gravity #GDK_GRAVITY_STATIC the window manager
 * frame is not relevant, and thus gtk_window_get_position() will
 * always produce accurate results. However you can't use static
 * gravity to do things like place a window in a corner of the screen,
 * because static gravity ignores the window manager decorations.
 *
 * If you are saving and restoring your application's window
 * positions, you should know that it's impossible for applications to
 * do this without getting it somewhat wrong because applications do
 * not have sufficient knowledge of window manager state. The Correct
 * Mechanism is to support the session management protocol (see the
 * "GnomeClient" object in the GNOME libraries for example) and allow
 * the window manager to save your window sizes and positions.
 * 
 **/

void
gtk_window_get_position (GtkWindow *window,
                         gint      *root_x,
                         gint      *root_y)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);
  
  if (window->gravity == GDK_GRAVITY_STATIC)
    {
      if (GTK_WIDGET_MAPPED (widget))
        {
          /* This does a server round-trip, which is sort of wrong;
           * but a server round-trip is inevitable for
           * gdk_window_get_frame_extents() in the usual
           * NorthWestGravity case below, so not sure what else to
           * do. We should likely be consistent about whether we get
           * the client-side info or the server-side info.
           */
          gdk_window_get_origin (widget->window, root_x, root_y);
        }
      else
        {
          GdkRectangle configure_request;
          
          gtk_window_compute_configure_request (window,
                                                &configure_request,
                                                NULL, NULL);
          
          *root_x = configure_request.x;
          *root_y = configure_request.y;
        }
    }
  else
    {
      GdkRectangle frame_extents;
      
      gint x, y;
      gint w, h;
      
      if (GTK_WIDGET_MAPPED (widget))
        {
	  if (window->frame)
	    gdk_window_get_frame_extents (window->frame, &frame_extents);
	  else
	    gdk_window_get_frame_extents (widget->window, &frame_extents);
          x = frame_extents.x;
          y = frame_extents.y;
          gtk_window_get_size (window, &w, &h);
        }
      else
        {
          /* We just say the frame has 0 size on all sides.
           * Not sure what else to do.
           */             
          gtk_window_compute_configure_request (window,
                                                &frame_extents,
                                                NULL, NULL);
          x = frame_extents.x;
          y = frame_extents.y;
          w = frame_extents.width;
          h = frame_extents.height;
        }
      
      switch (window->gravity)
        {
        case GDK_GRAVITY_NORTH:
        case GDK_GRAVITY_CENTER:
        case GDK_GRAVITY_SOUTH:
          /* Find center of frame. */
          x += frame_extents.width / 2;
          /* Center client window on that point. */
          x -= w / 2;
          break;

        case GDK_GRAVITY_SOUTH_EAST:
        case GDK_GRAVITY_EAST:
        case GDK_GRAVITY_NORTH_EAST:
          /* Find right edge of frame */
          x += frame_extents.width;
          /* Align left edge of client at that point. */
          x -= w;
          break;
        default:
          break;
        }

      switch (window->gravity)
        {
        case GDK_GRAVITY_WEST:
        case GDK_GRAVITY_CENTER:
        case GDK_GRAVITY_EAST:
          /* Find center of frame. */
          y += frame_extents.height / 2;
          /* Center client window there. */
          y -= h / 2;
          break;
        case GDK_GRAVITY_SOUTH_WEST:
        case GDK_GRAVITY_SOUTH:
        case GDK_GRAVITY_SOUTH_EAST:
          /* Find south edge of frame */
          y += frame_extents.height;
          /* Place bottom edge of client there */
          y -= h;
          break;
        default:
          break;
        }
      
      if (root_x)
        *root_x = x;
      if (root_y)
        *root_y = y;
    }
}

/**
 * gtk_window_reshow_with_initial_size:
 * @window: a #GtkWindow
 * 
 * Hides @window, then reshows it, resetting the
 * default size and position of the window. Used
 * by GUI builders only.
 **/
void
gtk_window_reshow_with_initial_size (GtkWindow *window)
{
  GtkWidget *widget;
  
  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);
  
  gtk_widget_hide (widget);
  gtk_widget_unrealize (widget);
  gtk_widget_show (widget);
}

static void
gtk_window_destroy (GtkObject *object)
{
  GtkWindow *window = GTK_WINDOW (object);
  
  if (window->transient_parent)
    gtk_window_set_transient_for (window, NULL);

  /* frees the icons */
  gtk_window_set_icon_list (window, NULL);
  
  if (window->has_user_ref_count)
    {
      window->has_user_ref_count = FALSE;
      gtk_widget_unref (GTK_WIDGET (window));
    }

  if (window->group)
    gtk_window_group_remove_window (window->group, window);

   gtk_window_free_key_hash (window);

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
  GtkWindow *window = GTK_WINDOW (object);

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

  if (window->keys_changed_handler)
    {
      gtk_idle_remove (window->keys_changed_handler);
      window->keys_changed_handler = 0;
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
      GdkRectangle configure_request;
      GdkGeometry new_geometry;
      guint new_flags;
      gboolean was_realized;

      /* We are going to go ahead and perform this configure request
       * and then emulate a configure notify by going ahead and
       * doing a size allocate. Sort of a synchronous
       * mini-copy of gtk_window_move_resize() here.
       */
      gtk_window_compute_configure_request (window,
                                            &configure_request,
                                            &new_geometry,
                                            &new_flags);
      
      /* We update this because we are going to go ahead
       * and gdk_window_resize() below, rather than
       * queuing it.
       */
      info->last.configure_request.width = configure_request.width;
      info->last.configure_request.height = configure_request.height;
      
      /* and allocate the window - this is normally done
       * in move_resize in response to configure notify
       */
      allocation.width  = configure_request.width;
      allocation.height = configure_request.height;
      gtk_widget_size_allocate (widget, &allocation);

      /* Then we guarantee we have a realize */
      was_realized = FALSE;
      if (!GTK_WIDGET_REALIZED (widget))
	{
	  gtk_widget_realize (widget);
	  was_realized = TRUE;
	}

      /* Must be done after the windows are realized,
       * so that the decorations can be read
       */
      gtk_decorated_window_calculate_frame_size (window);

      /* We only send configure request if we didn't just finish
       * creating the window; if we just created the window
       * then we created it with widget->allocation anyhow.
       */
      if (!was_realized)
	gdk_window_resize (widget->window,
                           configure_request.width,
                           configure_request.height);
    }
  
  gtk_container_check_resize (container);

  gtk_widget_map (widget);

  /* Try to make sure that we have some focused widget
   */
  if (!window->focus_widget)
    gtk_window_move_focus (window, GTK_DIR_TAB_FORWARD);
  
  if (window->modal)
    gtk_grab_add (widget);
}

static void
gtk_window_hide (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_VISIBLE);
  gtk_widget_unmap (widget);

  if (window->modal)
    gtk_grab_remove (widget);
}

static void
gtk_window_map (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GdkWindow *toplevel;
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

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

  /* No longer use the default settings */
  window->need_default_size = FALSE;
  window->need_default_position = FALSE;
  
  gdk_window_show (widget->window);

  if (window->frame)
    gdk_window_show (window->frame);
}

static void
gtk_window_unmap (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowGeometryInfo *info;    

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  if (window->frame)
    gdk_window_withdraw (window->frame);
  else 
    gdk_window_withdraw (widget->window);
  
  window->configure_request_count = 0;
  window->configure_notify_received = FALSE;

  /* on unmap, we reset the default positioning of the window,
   * so it's placed again, but we don't reset the default
   * size of the window, so it's remembered.
   */
  window->need_default_position = TRUE;

  info = gtk_window_get_geometry_info (window, FALSE);
  if (info)
    {
      info->initial_pos_set = FALSE;
      info->position_constraints_changed = FALSE;
    }
}

static void
gtk_window_realize (GtkWidget *widget)
{
  GtkWindow *window;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
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
      
      _gtk_container_queue_resize (GTK_CONTAINER (widget));

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
			    GDK_KEY_RELEASE_MASK |
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

  /* Icons */
  gtk_window_realize_icon (window);
}

static void
gtk_window_unrealize (GtkWidget *widget)
{
  GtkWindow *window;
  GtkWindowGeometryInfo *info;

  window = GTK_WINDOW (widget);

  /* On unrealize, we reset the size of the window such
   * that we will re-apply the default sizing stuff
   * next time we show the window.
   *
   * Default positioning is reset on unmap, instead of unrealize.
   */
  window->need_default_size = TRUE;
  info = gtk_window_get_geometry_info (window, FALSE);
  if (info)
    {
      info->resize_width = -1;
      info->resize_height = -1;
      info->last.configure_request.x = 0;
      info->last.configure_request.y = 0;
      info->last.configure_request.width = -1;
      info->last.configure_request.height = -1;
      /* be sure we reset geom hints on re-realize */
      info->last.flags = 0;
    }
  
  if (window->frame)
    {
      gdk_window_set_user_data (window->frame, NULL);
      gdk_window_destroy (window->frame);
      window->frame = NULL;
    }

  /* Icons */
  gtk_window_unrealize_icon (window);
  
  (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_window_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  GtkWindow *window;
  GtkBin *bin;

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
gtk_window_frame_event (GtkWindow *window, GdkEvent *event)
{
  GdkEventConfigure *configure_event;
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
      return gtk_window_configure_event (GTK_WIDGET (window), configure_event);
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
  GtkWindow *window = GTK_WINDOW (widget);

  /* window->configure_request_count incremented for each 
   * configure request, and decremented to a min of 0 for
   * each configure notify.
   *
   * All it means is that we know we will get at least
   * window->configure_request_count more configure notifies.
   * We could get more configure notifies than that; some
   * of the configure notifies we get may be unrelated to
   * the configure requests. But we will get at least
   * window->configure_request_count notifies.
   */

  if (window->configure_request_count > 0)
    window->configure_request_count -= 1;
  
  /* As an optimization, we avoid a resize when possible.
   *
   * The only times we can avoid a resize are:
   *   - we know only the position changed, not the size
   *   - we know we have made more requests and so will get more
   *     notifies and can wait to resize when we get them
   */
  
  if (window->configure_request_count > 0 ||
      (widget->allocation.width == event->width &&
       widget->allocation.height == event->height))
    return TRUE;

  /*
   * If we do need to resize, we do that by:
   *   - filling in widget->allocation with the new size
   *   - setting configure_notify_received to TRUE
   *     for use in gtk_window_move_resize()
   *   - queueing a resize, leading to invocation of
   *     gtk_window_move_resize() in an idle handler
   *
   */
  
  window->configure_notify_received = TRUE;
  
  widget->allocation.width = event->width;
  widget->allocation.height = event->height;
  
  _gtk_container_queue_resize (GTK_CONTAINER (widget));
  
  return TRUE;
}

/* the accel_key and accel_mods fields of the key have to be setup
 * upon calling this function. it'll then return whether that key
 * is at all used as accelerator, and if so will OR in the
 * accel_flags member of the key.
 */
gboolean
_gtk_window_query_nonaccels (GtkWindow      *window,
			     guint           accel_key,
			     GdkModifierType accel_mods)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  /* movement keys are considered locked accels */
  if (!accel_mods)
    {
      static const guint bindings[] = {
	GDK_space, GDK_KP_Space, GDK_Return, GDK_KP_Enter, GDK_Up, GDK_KP_Up, GDK_Down, GDK_KP_Down,
	GDK_Left, GDK_KP_Left, GDK_Right, GDK_KP_Right, GDK_Tab, GDK_KP_Tab, GDK_ISO_Left_Tab,
      };
      guint i;
      
      for (i = 0; i < G_N_ELEMENTS (bindings); i++)
	if (bindings[i] == accel_key)
	  return TRUE;
    }

  /* mnemonics are considered locked accels */
  if (accel_mods == window->mnemonic_modifier)
    {
      GtkWindowMnemonic mkey;

      mkey.window = window;
      mkey.keyval = accel_key;
      if (g_hash_table_lookup (mnemonic_hash_table, &mkey))
	return TRUE;
    }

  return FALSE;
}

static gint
gtk_window_key_press_event (GtkWidget   *widget,
			    GdkEventKey *event)
{
  GtkWindow *window;
  GtkWidget *focus;
  gboolean handled;

  window = GTK_WINDOW (widget);

  handled = FALSE;

  /* Check for mnemonics and accelerators
   */
  if (!handled)
    handled = _gtk_window_activate_key (window, event);

  if (!handled)
    {
      focus = window->focus_widget;
      if (focus)
	g_object_ref (focus);
      
      while (!handled &&
	     focus && focus != widget &&
	     gtk_widget_get_toplevel (focus) == widget)
	{
	  GtkWidget *parent;
	  
	  if (GTK_WIDGET_IS_SENSITIVE (focus))
	    handled = gtk_widget_event (focus, (GdkEvent*) event);
	  
	  parent = focus->parent;
	  if (parent)
	    g_object_ref (parent);
	  
	  g_object_unref (focus);
	  
	  focus = parent;
	}

      if (focus)
	g_object_unref (focus);
    }

  /* Chain up, invokes binding set */
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
gtk_window_enter_notify_event (GtkWidget        *widget,
			       GdkEventCrossing *event)
{
  return FALSE;
}

static gint
gtk_window_leave_notify_event (GtkWidget        *widget,
			       GdkEventCrossing *event)
{
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
      
      for (i = 0; i < 5; i++)
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

  gtk_rc_reparse_all ();
}

static gint
gtk_window_client_event (GtkWidget	*widget,
			 GdkEventClient	*event)
{
  if (!atom_rcfiles)
    atom_rcfiles = gdk_atom_intern ("_GTK_READ_RCFILES", FALSE);

  if (event->message_type == atom_rcfiles) 
    gtk_window_read_rcfiles (widget, event);    

  return FALSE;
}

static void
gtk_window_check_resize (GtkContainer *container)
{
  GtkWindow *window = GTK_WINDOW (container);

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

/* This function doesn't constrain to geometry hints */
static void 
gtk_window_compute_configure_request_size (GtkWindow *window,
                                           guint     *width,
                                           guint     *height)
{
  GtkRequisition requisition;
  GtkWindowGeometryInfo *info;
  GtkWidget *widget;

  /* Preconditions:
   *  - we've done a size request
   */
  
  widget = GTK_WIDGET (window);

  info = gtk_window_get_geometry_info (window, FALSE);
  
  if (window->need_default_size)
    {
      gtk_widget_get_child_requisition (widget, &requisition);

      /* Default to requisition */
      *width = requisition.width;
      *height = requisition.height;

      /* If window is empty so requests 0, default to random nonzero size */
       if (*width == 0 && *height == 0)
         {
           *width = 200;
           *height = 200;
         }

       /* Override requisition with default size */

       if (info)
         {
           if (info->default_width > 0)
             *width = info->default_width;
           
           if (info->default_height > 0)
             *height = info->default_height;
         }
    }
  else
    {
      /* Default to keeping current size */
      *width = widget->allocation.width;
      *height = widget->allocation.height;
    }

  /* Override any size with gtk_window_resize() values */
  if (info)
    {
      if (info->resize_width > 0)
        *width = info->resize_width;

      if (info->resize_height > 0)
        *height = info->resize_height;
    }
}

static void
gtk_window_compute_configure_request (GtkWindow    *window,
                                      GdkRectangle *request,
                                      GdkGeometry  *geometry,
                                      guint        *flags)
{
  GdkGeometry new_geometry;
  guint new_flags;
  int w, h;
  GtkWidget *widget;
  GtkWindowPosition pos;
  GtkWidget *parent_widget;
  GtkWindowGeometryInfo *info;
  int x, y;
  
  widget = GTK_WIDGET (window);
  
  gtk_widget_size_request (widget, NULL);
  gtk_window_compute_configure_request_size (window, &w, &h);
  
  gtk_window_compute_hints (window, &new_geometry, &new_flags);
  gtk_window_constrain_size (window,
                             &new_geometry, new_flags,
                             w, h,
                             &w, &h);

  parent_widget = (GtkWidget*) window->transient_parent;
  
  pos = window->position;
  if (pos == GTK_WIN_POS_CENTER_ON_PARENT &&
      (parent_widget == NULL ||
       !GTK_WIDGET_MAPPED (parent_widget)))
    pos = GTK_WIN_POS_NONE;

  info = gtk_window_get_geometry_info (window, TRUE);

  /* by default, don't change position requested */
  x = info->last.configure_request.x;
  y = info->last.configure_request.y;
  
  if (window->need_default_position)
    {

      /* FIXME this all interrelates with window gravity.
       * For most of them I think we want to set GRAVITY_CENTER.
       *
       * Not sure how to go about that.
       */
      
      switch (pos)
        {
          /* here we are only handling CENTER_ALWAYS
           * as it relates to default positioning,
           * where it's equivalent to simply CENTER
           */
        case GTK_WIN_POS_CENTER_ALWAYS:
        case GTK_WIN_POS_CENTER:
          {
            gint screen_width = gdk_screen_width ();
            gint screen_height = gdk_screen_height ();
            
            x = (screen_width - w) / 2;
            y = (screen_height - h) / 2;
          }
          break;
      
        case GTK_WIN_POS_CENTER_ON_PARENT:
          {
            gint ox, oy;
            
            g_assert (GTK_WIDGET_MAPPED (parent_widget)); /* established earlier */
            
            gdk_window_get_origin (parent_widget->window,
                                   &ox, &oy);
            
            x = ox + (parent_widget->allocation.width - w) / 2;
            y = oy + (parent_widget->allocation.height - h) / 2;
          }
          break;

        case GTK_WIN_POS_MOUSE:
          {
            gint screen_width = gdk_screen_width ();
            gint screen_height = gdk_screen_height ();
            int px, py;
            
            gdk_window_get_pointer (NULL, &px, &py, NULL);
            x = px - w / 2;
            y = py - h / 2;
            x = CLAMP (x, 0, screen_width - w);
            y = CLAMP (y, 0, screen_height - h);
          }
          break;

        default:
          break;
        }
    } /* if (window->need_default_position) */

  if (window->need_default_position &&
      info->initial_pos_set)
    {
      x = info->initial_x;
      y = info->initial_y;
      gtk_window_constrain_position (window, w, h, &x, &y);
    }
  
  request->x = x;
  request->y = y;
  request->width = w;
  request->height = h;

  if (geometry)
    *geometry = new_geometry;
  if (flags)
    *flags = new_flags;
}

static void
gtk_window_constrain_position (GtkWindow    *window,
                               gint          new_width,
                               gint          new_height,
                               gint         *x,
                               gint         *y)
{
  /* See long comments in gtk_window_move_resize()
   * on when it's safe to call this function.
   */
  if (window->position == GTK_WIN_POS_CENTER_ALWAYS)
    {
      gint center_x, center_y;
      gint screen_width = gdk_screen_width ();
      gint screen_height = gdk_screen_height ();
      
      center_x = (screen_width - new_width) / 2;
      center_y = (screen_height - new_height) / 2;
      
      *x = center_x;
      *y = center_y;
    }
}

static void
gtk_window_move_resize (GtkWindow *window)
{
  /* Overview:
   *
   * First we determine whether any information has changed that would
   * cause us to revise our last configure request.  If we would send
   * a different configure request from last time, then
   * configure_request_size_changed = TRUE or
   * configure_request_pos_changed = TRUE. configure_request_size_changed
   * may be true due to new hints, a gtk_window_resize(), or whatever.
   * configure_request_pos_changed may be true due to gtk_window_set_position()
   * or gtk_window_move().
   *
   * If the configure request has changed, we send off a new one.  To
   * ensure GTK+ invariants are maintained (resize queue does what it
   * should), we go ahead and size_allocate the requested size in this
   * function.
   *
   * If the configure request has not changed, we don't ever resend
   * it, because it could mean fighting the user or window manager.
   *
   * 
   *   To prepare the configure request, we come up with a base size/pos:
   *      - the one from gtk_window_move()/gtk_window_resize()
   *      - else default_width, default_height if we haven't ever
   *        been mapped
   *      - else the size request if we haven't ever been mapped,
   *        as a substitute default size
   *      - else the current size of the window, as received from
   *        configure notifies (i.e. the current allocation)
   *
   *   If GTK_WIN_POS_CENTER_ALWAYS is active, we constrain
   *   the position request to be centered.
   */
  GtkWidget *widget;
  GtkContainer *container;
  GtkWindowGeometryInfo *info;
  GdkGeometry new_geometry;
  guint new_flags;
  GdkRectangle new_request;
  gboolean configure_request_size_changed;
  gboolean configure_request_pos_changed;
  gboolean hints_changed; /* do we need to send these again */
  GtkWindowLastGeometryInfo saved_last_info;
  
  widget = GTK_WIDGET (window);
  container = GTK_CONTAINER (widget);
  info = gtk_window_get_geometry_info (window, TRUE);
  
  configure_request_size_changed = FALSE;
  configure_request_pos_changed = FALSE;
  
  gtk_window_compute_configure_request (window, &new_request,
                                        &new_geometry, &new_flags);  
  
  /* This check implies the invariant that we never set info->last
   * without setting the hints and sending off a configure request.
   *
   * If we change info->last without sending the request, we may
   * miss a request.
   */
  if (info->last.configure_request.x != new_request.x ||
      info->last.configure_request.y != new_request.y)
    configure_request_pos_changed = TRUE;

  if ((info->last.configure_request.width != new_request.width ||
       info->last.configure_request.height != new_request.height))
    configure_request_size_changed = TRUE;
  
  hints_changed = FALSE;
  
  if (!gtk_window_compare_hints (&info->last.geometry, info->last.flags,
				 &new_geometry, new_flags))
    {
      hints_changed = TRUE;
    }
  
  /* Position Constraints
   * ====================
   * 
   * POS_CENTER_ALWAYS is conceptually a constraint rather than
   * a default. The other POS_ values are used only when the
   * window is shown, not after that.
   * 
   * However, we can't implement a position constraint as
   * "anytime the window size changes, center the window"
   * because this may well end up fighting the WM or user.  In
   * fact it gets in an infinite loop with at least one WM.
   *
   * Basically, applications are in no way in a position to
   * constrain the position of a window, with one exception:
   * override redirect windows. (Really the intended purpose
   * of CENTER_ALWAYS anyhow, I would think.)
   *
   * So the way we implement this "constraint" is to say that when WE
   * cause a move or resize, i.e. we make a configure request changing
   * window size, we recompute the CENTER_ALWAYS position to reflect
   * the new window size, and include it in our request.  Also, if we
   * just turned on CENTER_ALWAYS we snap to center with a new
   * request.  Otherwise, if we are just NOTIFIED of a move or resize
   * done by someone else e.g. the window manager, we do NOT send a
   * new configure request.
   *
   * For override redirect windows, this works fine; all window
   * sizes are from our configure requests. For managed windows,
   * it is at least semi-sane, though who knows what the
   * app author is thinking.
   */

  /* This condition should be kept in sync with the condition later on
   * that determines whether we send a configure request.  i.e. we
   * should do this position constraining anytime we were going to
   * send a configure request anyhow, plus when constraints have
   * changed.
   */
  if (configure_request_pos_changed ||
      configure_request_size_changed ||
      hints_changed ||
      info->position_constraints_changed)
    {
      /* We request the constrained position if:
       *  - we were changing position, and need to clamp
       *    the change to the constraint
       *  - we're changing the size anyway
       *  - set_position() was called to toggle CENTER_ALWAYS on
       */

      gtk_window_constrain_position (window,
                                     new_request.width,
                                     new_request.height,
                                     &new_request.x,
                                     &new_request.y);
      
      /* Update whether we need to request a move */
      if (info->last.configure_request.x != new_request.x ||
          info->last.configure_request.y != new_request.y)
        configure_request_pos_changed = TRUE;
      else
        configure_request_pos_changed = FALSE;
    }

#if 0
  {
    int notify_x, notify_y;

    /* this is the position from the last configure notify */
    gdk_window_get_position (widget->window, &notify_x, &notify_y);
    
    g_print ("--- %s ---\n"
             "last : %d,%d\t%d x %d\n"
             "this : %d,%d\t%d x %d\n"
             "alloc: %d,%d\t%d x %d\n"
             "req  :      \t%d x %d\n"
             "size_changed: %d pos_changed: %d hints_changed: %d\n"
             "configure_notify_received: %d\n"
             "configure_request_count: %d\n"
             "position_constraints_changed: %d\n",
             window->title ? window->title : "(no title)",
             info->last.configure_request.x,
             info->last.configure_request.y,
             info->last.configure_request.width,
             info->last.configure_request.height,
             new_request.x,
             new_request.y,
             new_request.width,
             new_request.height,
             notify_x, notify_y,
             widget->allocation.width,
             widget->allocation.height,
             widget->requisition.width,
             widget->requisition.height,
             configure_request_pos_changed,
             configure_request_size_changed,
             hints_changed,
             window->configure_notify_received,
             window->configure_request_count,
             info->position_constraints_changed);
  }
#endif
  
  saved_last_info = info->last;
  info->last.geometry = new_geometry;
  info->last.flags = new_flags;
  info->last.configure_request = new_request;
  
  /* need to set PPosition so the WM will look at our position,
   * but we don't want to count PPosition coming and going as a hints
   * change for future iterations. So we saved info->last prior to
   * this.
   */
  
  /* Also, if the initial position was explicitly set, then we always
   * toggle on PPosition. This makes gtk_window_move(window, 0, 0)
   * work.
   */

  /* Also, we toggle on PPosition if GTK_WIN_POS_ is in use and
   * this is an initial map
   */
  
  if ((configure_request_pos_changed ||
       info->initial_pos_set ||
       (window->need_default_position &&
        window->position != GTK_WIN_POS_NONE)) &&
      (new_flags & GDK_HINT_POS) == 0)
    {
      new_flags |= GDK_HINT_POS;
      hints_changed = TRUE;
    }
  
  /* Set hints if necessary
   */
  if (hints_changed)
    gdk_window_set_geometry_hints (widget->window,
				   &new_geometry,
				   new_flags);

  /* handle resizing/moving and widget tree allocation
   */
  if (window->configure_notify_received)
    { 
      GtkAllocation allocation;

      /* If we have received a configure event since
       * the last time in this function, we need to
       * accept our new size and size_allocate child widgets.
       * (see gtk_window_configure_event() for more details).
       *
       * 1 or more configure notifies may have been received.
       * Also, configure_notify_received will only be TRUE
       * if all expected configure notifies have been received
       * (one per configure request), as an optimization.
       *
       */
      window->configure_notify_received = FALSE;

      /* gtk_window_configure_event() filled in widget->allocation */
      allocation = widget->allocation;
      gtk_widget_size_allocate (widget, &allocation);

      /* If the configure request changed, it means that
       * we either:
       *   1) coincidentally changed hints or widget properties
       *      impacting the configure request before getting
       *      a configure notify, or
       *   2) some broken widget is changing its size request
       *      during size allocation, resulting in
       *      a false appearance of changed configure request.
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

      if (configure_request_size_changed ||
          configure_request_pos_changed)
        {
          /* Don't change the recorded last info after all, because we
           * haven't actually updated to the new info yet - we decided
           * to postpone our configure request until later.
           */
	  info->last = saved_last_info;
          
	  gtk_widget_queue_resize (widget); /* migth recurse for GTK_RESIZE_IMMEDIATE */
	}
    }
  else if ((configure_request_size_changed || hints_changed) &&
	   (widget->allocation.width != new_request.width ||
	    widget->allocation.height != new_request.height))

    {
      /* We are in one of the following situations:
       * A. configure_request_size_changed
       *    our requisition has changed and we need a different window size,
       *    so we request it from the window manager.
       * B. !configure_request_size_changed && hints_changed
       *    the window manager rejects our size, but we have just changed the
       *    window manager hints, so there's a chance our request will
       *    be honoured this time, so we try again.
       *
       * However, if the new requisition is the same as the current allocation,
       * we don't request it again, since we won't get a ConfigureNotify back from
       * the window manager unless it decides to change our requisition. If
       * we don't get the ConfigureNotify back, the resize queue will never be run.
       */

      /* Now send the configure request */
      if (configure_request_pos_changed)
	{
	  if (window->frame)
	    {
	      gdk_window_move_resize (window->frame,
				      new_request.x - window->frame_left,
                                      new_request.y - window->frame_top,
				      new_request.width + window->frame_left + window->frame_right,
				      new_request.height + window->frame_top + window->frame_bottom);
	      gdk_window_resize (GTK_WIDGET (window)->window,
                                 new_request.width, new_request.height);
	    }
	  else
	    gdk_window_move_resize (widget->window,
                                    new_request.x, new_request.y,
                                    new_request.width, new_request.height);
	}
      else  /* only size changed */
	{
	  if (window->frame)
	    gdk_window_resize (window->frame,
			       new_request.width + window->frame_left + window->frame_right,
			       new_request.height + window->frame_top + window->frame_bottom);
	  gdk_window_resize (widget->window,
                             new_request.width, new_request.height);
	}
      
      /* Increment the number of have-not-yet-received-notify requests */
      window->configure_request_count += 1;

      /* We have now sent a request since the last position constraint
       * change and definitely don't need a an initial size again (not
       * resetting this here can lead to infinite loops for
       * GTK_RESIZE_IMMEDIATE containers)
       */
      info->position_constraints_changed = FALSE;
      window->need_default_position = FALSE;
      info->initial_pos_set = FALSE;

      /* for GTK_RESIZE_QUEUE toplevels, we are now awaiting a new
       * configure event in response to our resizing request.
       * the configure event will cause a new resize with
       * ->configure_notify_received=TRUE.
       * until then, we want to
       * - discard expose events
       * - coalesce resizes for our children
       * - defer any window resizes until the configure event arrived
       * to achieve this, we queue a resize for the window, but remove its
       * resizing handler, so resizing will not be handled from the next
       * idle handler but when the configure event arrives.
       *
       * FIXME: we should also dequeue the pending redraws here, since
       * we handle those ourselves upon ->configure_notify_received==TRUE.
       */
      if (container->resize_mode == GTK_RESIZE_QUEUE)
	{
	  gtk_widget_queue_resize (widget);
	  _gtk_container_dequeue_resize_handler (container);
	}
    }
  else
    {
      /* Handle any position changes.
       */
      if (configure_request_pos_changed)
	{
	  if (window->frame)
	    {
	      gdk_window_move (window->frame,
			       new_request.x - window->frame_left,
			       new_request.y - window->frame_top);
	    }
	  else
	    gdk_window_move (widget->window,
			     new_request.x, new_request.y);
	}
      
      /* And run the resize queue.
       */
      gtk_container_resize_children (container);
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
  gint extra_width = 0;
  gint extra_height = 0;
  GtkWindowGeometryInfo *geometry_info;
  GtkRequisition requisition;

  widget = GTK_WIDGET (window);
  
  gtk_widget_get_child_requisition (widget, &requisition);
  geometry_info = gtk_window_get_geometry_info (GTK_WINDOW (widget), FALSE);

  if (geometry_info)
    {
      *new_flags = geometry_info->mask;
      *new_geometry = geometry_info->geometry;
    }
  else
    {
      *new_flags = 0;
    }
  
  if (geometry_info && geometry_info->widget)
    {
      GtkRequisition child_requisition;

      /* FIXME: This really isn't right. It gets the min size wrong and forces
       * callers to do horrible hacks like set a huge usize on the child requisition
       * to get the base size right. We really want to find the answers to:
       *
       *  - If the geometry widget was infinitely big, how much extra space
       *    would be needed for the stuff around it.
       *
       *  - If the geometry widget was infinitely small, how big would the
       *    window still have to be.
       *
       * Finding these answers would be a bit of a mess here. (Bug #68668)
       */
      gtk_widget_get_child_requisition (geometry_info->widget, &child_requisition);
      
      extra_width = widget->requisition.width - child_requisition.width;
      extra_height = widget->requisition.height - child_requisition.height;
    }

  /* We don't want to set GDK_HINT_POS in here, we just set it
   * in gtk_window_move_resize() when we want the position
   * honored.
   */
  
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
	new_geometry->min_height = requisition.height;
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
  if (!GTK_WIDGET_APP_PAINTABLE (widget))
    gtk_window_paint (widget, &event->area);
  
  if (GTK_WIDGET_CLASS (parent_class)->expose_event)
    return GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);

  return FALSE;
}

/**
 * gtk_window_set_has_frame:
 * @window: a #GtkWindow
 * @setting: a boolean
 *
 * (Note: this is a special-purpose function for the framebuffer port,
 *  that causes GTK+ to draw its own window border. For most applications,
 *  you want gtk_window_set_decorated() instead, which tells the window
 *  manager whether to draw the window border.)
 * 
 * If this function is called on a window with setting of %TRUE, before
 * it is realized or showed, it will have a "frame" window around
 * @window->window, accessible in @window->frame. Using the signal 
 * frame_event you can recieve all events targeted at the frame.
 * 
 * This function is used by the linux-fb port to implement managed
 * windows, but it could concievably be used by X-programs that
 * want to do their own window decorations.
 *
 **/
void
gtk_window_set_has_frame (GtkWindow *window, 
			  gboolean   setting)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (!GTK_WIDGET_REALIZED (window));

  window->has_frame = setting != FALSE;
}

/**
 * gtk_window_get_has_frame:
 * @window: a #GtkWindow
 * 
 * Accessor for whether the window has a frame window exterior to
 * @window->window. Gets the value set by gtk_window_set_has_frame ().
 *
 * Return value: %TRUE if a frame has been added to the window
 *   via gtk_window_set_has_frame().
 **/
gboolean
gtk_window_get_has_frame (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->has_frame;
}

/**
 * gtk_window_set_frame_dimensions:
 * @window: a #GtkWindow that has a frame
 * @left: The width of the left border
 * @top: The height of the top border
 * @right: The width of the right border
 * @bottom: The height of the bottom border
 *
 * (Note: this is a special-purpose function intended for the framebuffer
 *  port; see gtk_window_set_has_frame(). It will have no effect on the
 *  window border drawn by the window manager, which is the normal
 *  case when using the X Window system.)
 *
 * For windows with frames (see gtk_window_set_has_frame()) this function
 * can be used to change the size of the frame border.
 **/
void
gtk_window_set_frame_dimensions (GtkWindow *window, 
				 gint       left,
				 gint       top,
				 gint       right,
				 gint       bottom)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);

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
 * Asks to iconify (i.e. minimize) the specified @window. Note that
 * you shouldn't assume the window is definitely iconified afterward,
 * because other entities (e.g. the user or <link
 * linkend="gtk-X11-arch">window manager</link>) could deiconify it
 * again, or there may not be a window manager in which case
 * iconification isn't possible, etc. But normally the window will end
 * up iconified. Just don't write code that crashes if not.
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
 * Asks to deiconify (i.e. unminimize) the specified @window. Note
 * that you shouldn't assume the window is definitely deiconified
 * afterward, because other entities (e.g. the user or <link
 * linkend="gtk-X11-arch">window manager</link>) could iconify it
 * again before your code which assumes deiconification gets to run.
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
 * stuck afterward, because other entities (e.g. the user or <link
 * linkend="gtk-X11-arch">window manager</link>) could unstick it
 * again, and some window managers do not support sticking
 * windows. But normally the window will end up stuck. Just don't
 * write code that crashes if not.
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
 * (e.g. the user or <link linkend="gtk-X11-arch">window
 * manager</link>) could stick it again. But normally the window will
 * end up stuck. Just don't write code that crashes if not.
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
 * because other entities (e.g. the user or <link
 * linkend="gtk-X11-arch">window manager</link>) could unmaximize it
 * again, and not all window managers support maximization. But
 * normally the window will end up maximized. Just don't write code
 * that crashes if not.
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
 * (e.g. the user or <link linkend="gtk-X11-arch">window
 * manager</link>) could maximize it again, and not all window
 * managers honor requests to unmaximize. But normally the window will
 * end up unmaximized. Just don't write code that crashes if not.
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
 * gtk_window_set_resizable:
 * @window: a #GtkWindow
 * @resizable: %TRUE if the user can resize this window
 *
 * Sets whether the user can resize a window. Windows are user resizable
 * by default.
 **/
void
gtk_window_set_resizable (GtkWindow *window,
                          gboolean   resizable)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  gtk_window_set_policy (window, FALSE, resizable, FALSE);
}

/**
 * gtk_window_get_resizable:
 * @window: a #GtkWindow
 *
 * Gets the value set by gtk_window_set_resizable().
 *
 * Return value: %TRUE if the user can resize the window
 **/
gboolean
gtk_window_get_resizable (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  /* allow_grow is most likely to indicate the semantic concept we
   * mean by "resizable" (and will be a reliable indicator if
   * set_policy() hasn't been called)
   */
  return window->allow_grow;
}

/**
 * gtk_window_set_gravity:
 * @window: a #GtkWindow
 * @gravity: window gravity
 *
 * Window gravity defines the meaning of coordinates passed to
 * gtk_window_move(). See gtk_window_move() and #GdkGravity for
 * more details.
 *
 * The default window gravity is #GDK_GRAVITY_NORTH_WEST which will
 * typically "do what you mean."
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
 * will be done using the standard mechanism for the <link
 * linkend="gtk-X11-arch">window manager</link> or windowing
 * system. Otherwise, GDK will try to emulate window resizing,
 * potentially not all that well, depending on the windowing system.
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
 * gtk_window_get_frame_dimensions:
 * @window: a #GtkWindow
 * @left: location to store the width of the frame at the left, or %NULL
 * @top: location to store the height of the frame at the top, or %NULL
 * @right: location to store the width of the frame at the returns, or %NULL
 * @bottom: location to store the height of the frame at the bottom, or %NULL
 *
 * (Note: this is a special-purpose function intended for the
 *  framebuffer port; see gtk_window_set_has_frame(). It will not
 *  return the size of the window border drawn by the <link
 *  linkend="gtk-X11-arch">window manager</link>, which is the normal
 *  case when using a windowing system.  See
 *  gdk_window_get_frame_extents() to get the standard window border
 *  extents.)
 * 
 * Retrieves the dimensions of the frame window for this toplevel.
 * See gtk_window_set_has_frame(), gtk_window_set_frame_dimensions().
 **/
void
gtk_window_get_frame_dimensions (GtkWindow *window,
				 gint      *left,
				 gint      *top,
				 gint      *right,
				 gint      *bottom)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (left)
    *left = window->frame_left;
  if (top)
    *top = window->frame_top;
  if (right)
    *right = window->frame_right;
  if (bottom)
    *bottom = window->frame_bottom;
}

/**
 * gtk_window_begin_move_drag:
 * @window: a #GtkWindow
 * @button: mouse button that initiated the drag
 * @root_x: X position where the user clicked to initiate the drag, in root window coordinates
 * @root_y: Y position where the user clicked to initiate the drag
 * @timestamp: timestamp from the click event that initiated the drag
 *
 * Starts moving a window. This function is used if an application has
 * window movement grips. When GDK can support it, the window movement
 * will be done using the standard mechanism for the <link
 * linkend="gtk-X11-arch">window manager</link> or windowing
 * system. Otherwise, GDK will try to emulate window movement,
 * potentially not all that well, depending on the windowing system.
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
 * Creates a new #GtkWindowGroup object. Grabs added with
 * gtk_grab_add() only affect windows within the same #GtkWindowGroup.
 * 
 * Return value: a new #GtkWindowGroup. 
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
 * gtk_window_group_add_window:
 * @window_group: a #GtkWindowGroup
 * @window: the #GtkWindow to add
 * 
 * Adds a window to a #GtkWindowGroup. 
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


/*
  Derived from XParseGeometry() in XFree86  

  Copyright 1985, 1986, 1987,1998  The Open Group

  All Rights Reserved.

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.

  Except as contained in this notice, the name of The Open Group shall
  not be used in advertising or otherwise to promote the sale, use or
  other dealings in this Software without prior written authorization
  from The Open Group.
*/


/*
 *    XParseGeometry parses strings of the form
 *   "=<width>x<height>{+-}<xoffset>{+-}<yoffset>", where
 *   width, height, xoffset, and yoffset are unsigned integers.
 *   Example:  "=80x24+300-49"
 *   The equal sign is optional.
 *   It returns a bitmask that indicates which of the four values
 *   were actually found in the string.  For each value found,
 *   the corresponding argument is updated;  for each value
 *   not found, the corresponding argument is left unchanged. 
 */

/* The following code is from Xlib, and is minimally modified, so we
 * can track any upstream changes if required.  Don't change this
 * code. Or if you do, put in a huge comment marking which thing
 * changed.
 */

static int
read_int (gchar   *string,
          gchar  **next)
{
  int result = 0;
  int sign = 1;
  
  if (*string == '+')
    string++;
  else if (*string == '-')
    {
      string++;
      sign = -1;
    }

  for (; (*string >= '0') && (*string <= '9'); string++)
    {
      result = (result * 10) + (*string - '0');
    }

  *next = string;

  if (sign >= 0)
    return (result);
  else
    return (-result);
}

/* 
 * Bitmask returned by XParseGeometry().  Each bit tells if the corresponding
 * value (x, y, width, height) was found in the parsed string.
 */
#define NoValue         0x0000
#define XValue          0x0001
#define YValue          0x0002
#define WidthValue      0x0004
#define HeightValue     0x0008
#define AllValues       0x000F
#define XNegative       0x0010
#define YNegative       0x0020

/* Try not to reformat/modify, so we can compare/sync with X sources */
static int
gtk_XParseGeometry (const char   *string,
                    int          *x,
                    int          *y,
                    unsigned int *width,   
                    unsigned int *height)  
{
  int mask = NoValue;
  char *strind;
  unsigned int tempWidth, tempHeight;
  int tempX, tempY;
  char *nextCharacter;

  /* These initializations are just to silence gcc */
  tempWidth = 0;
  tempHeight = 0;
  tempX = 0;
  tempY = 0;
  
  if ( (string == NULL) || (*string == '\0')) return(mask);
  if (*string == '=')
    string++;  /* ignore possible '=' at beg of geometry spec */

  strind = (char *)string;
  if (*strind != '+' && *strind != '-' && *strind != 'x') {
    tempWidth = read_int(strind, &nextCharacter);
    if (strind == nextCharacter) 
      return (0);
    strind = nextCharacter;
    mask |= WidthValue;
  }

  if (*strind == 'x' || *strind == 'X') {	
    strind++;
    tempHeight = read_int(strind, &nextCharacter);
    if (strind == nextCharacter)
      return (0);
    strind = nextCharacter;
    mask |= HeightValue;
  }

  if ((*strind == '+') || (*strind == '-')) {
    if (*strind == '-') {
      strind++;
      tempX = -read_int(strind, &nextCharacter);
      if (strind == nextCharacter)
        return (0);
      strind = nextCharacter;
      mask |= XNegative;

    }
    else
      {	strind++;
      tempX = read_int(strind, &nextCharacter);
      if (strind == nextCharacter)
        return(0);
      strind = nextCharacter;
      }
    mask |= XValue;
    if ((*strind == '+') || (*strind == '-')) {
      if (*strind == '-') {
        strind++;
        tempY = -read_int(strind, &nextCharacter);
        if (strind == nextCharacter)
          return(0);
        strind = nextCharacter;
        mask |= YNegative;

      }
      else
        {
          strind++;
          tempY = read_int(strind, &nextCharacter);
          if (strind == nextCharacter)
            return(0);
          strind = nextCharacter;
        }
      mask |= YValue;
    }
  }
	
  /* If strind isn't at the end of the string the it's an invalid
		geometry specification. */

  if (*strind != '\0') return (0);

  if (mask & XValue)
    *x = tempX;
  if (mask & YValue)
    *y = tempY;
  if (mask & WidthValue)
    *width = tempWidth;
  if (mask & HeightValue)
    *height = tempHeight;
  return (mask);
}

/**
 * gtk_window_parse_geometry:
 * @window: a #GtkWindow
 * @geometry: geometry string
 * 
 * Parses a standard X Window System geometry string - see the
 * manual page for X (type 'man X') for details on this.
 * gtk_window_parse_geometry() does work on all GTK+ ports
 * including Win32 but is primarily intended for an X environment.
 *
 * If either a size or a position can be extracted from the
 * geometry string, gtk_window_parse_geometry() returns %TRUE
 * and calls gtk_window_set_default_size() and/or gtk_window_move()
 * to resize/move the window.
 *
 * If gtk_window_parse_geometry() returns %TRUE, it will also
 * set the #GDK_HINT_USER_POS and/or #GDK_HINT_USER_SIZE hints
 * indicating to the window manager that the size/position of
 * the window was user-specified. This causes most window
 * managers to honor the geometry.
 * 
 * Return value: %TRUE if string was parsed successfully
 **/
gboolean
gtk_window_parse_geometry (GtkWindow   *window,
                           const gchar *geometry)
{
  gint result, x, y;
  guint w, h;
  GdkGravity grav;
  gboolean size_set, pos_set;
  
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (geometry != NULL, FALSE);
  
  result = gtk_XParseGeometry (geometry, &x, &y, &w, &h);

  if ((result & WidthValue) == 0 ||
      w < 0)
    w = -1;
  if ((result & HeightValue) == 0 ||
      h < 0)
    h = -1;

  size_set = FALSE;
  if ((result & WidthValue) || (result & HeightValue))
    {
      GtkWindowGeometryInfo *info;
      info = gtk_window_get_geometry_info (window, FALSE);
      if (info && info->mask & GDK_HINT_RESIZE_INC)
        {
          w *= info->geometry.width_inc;
          h *= info->geometry.height_inc;
        }
      gtk_window_set_default_size (window, w, h);
      size_set = TRUE;
    }

  gtk_window_get_size (window, &w, &h);
  
  grav = GDK_GRAVITY_NORTH_WEST;

  if ((result & XNegative) && (result & YNegative))
    grav = GDK_GRAVITY_SOUTH_EAST;
  else if (result & XNegative)
    grav = GDK_GRAVITY_NORTH_EAST;
  else if (result & YNegative)
    grav = GDK_GRAVITY_SOUTH_WEST;

  if ((result & XValue) == 0)
    x = 0;

  if ((result & YValue) == 0)
    y = 0;

  if (grav == GDK_GRAVITY_SOUTH_WEST ||
      grav == GDK_GRAVITY_SOUTH_EAST)
    y = gdk_screen_height () - h + y;

  if (grav == GDK_GRAVITY_SOUTH_EAST ||
      grav == GDK_GRAVITY_NORTH_EAST)
    x = gdk_screen_width () - w + x;

  /* we don't let you put a window offscreen; maybe some people would
   * prefer to be able to, but it's kind of a bogus thing to do.
   */
  if (y < 0)
    y = 0;

  if (x < 0)
    x = 0;

  pos_set = FALSE;
  if ((result & XValue) || (result & YValue))
    {
      gtk_window_set_gravity (window, grav);
      gtk_window_move (window, x, y);
      pos_set = TRUE;
    }

  if (size_set || pos_set)
    {
      /* Set USSize, USPosition hints */
      GtkWindowGeometryInfo *info;

      info = gtk_window_get_geometry_info (window, TRUE);

      if (pos_set)
        info->mask |= GDK_HINT_USER_POS;
      if (size_set)
        info->mask |= GDK_HINT_USER_SIZE;
    }
  
  return result != 0;
}

typedef void (*GtkWindowKeysForeach) (GtkWindow      *window,
				      guint           keyval,
				      GdkModifierType modifiers,
				      gboolean        is_mnemonic,
				      gpointer        data);

static void
gtk_window_mnemonic_hash_foreach (gpointer key,
				  gpointer value,
				  gpointer data)
{
  struct {
    GtkWindow *window;
    GtkWindowKeysForeach func;
    gpointer func_data;
  } *info = data;

  GtkWindowMnemonic *mnemonic = value;

  if (mnemonic->window == info->window)
    (*info->func) (info->window, mnemonic->keyval, info->window->mnemonic_modifier, TRUE, info->func_data);
}

static void
gtk_window_keys_foreach (GtkWindow           *window,
			 GtkWindowKeysForeach func,
			 gpointer             func_data)
{
  GSList *groups;

  struct {
    GtkWindow *window;
    GtkWindowKeysForeach func;
    gpointer func_data;
  } info;

  info.window = window;
  info.func = func;
  info.func_data = func_data;

  g_hash_table_foreach (mnemonic_hash_table,
			gtk_window_mnemonic_hash_foreach,
			&info);

  groups = gtk_accel_groups_from_object (G_OBJECT (window));
  while (groups)
    {
      GtkAccelGroup *group = groups->data;
      gint i;

      for (i = 0; i < group->n_accels; i++)
	{
	  GtkAccelKey *key = &group->priv_accels[i].key;
	  
	  if (key->accel_key)
	    (*func) (window, key->accel_key, key->accel_mods, FALSE, func_data);
	}
      
      groups = groups->next;
    }
}

static void
gtk_window_keys_changed (GtkWindow *window)
{
  gtk_window_free_key_hash (window);
  gtk_window_get_key_hash (window);
}

typedef struct _GtkWindowKeyEntry GtkWindowKeyEntry;

struct _GtkWindowKeyEntry
{
  guint keyval;
  guint modifiers;
  gboolean is_mnemonic;
};

static void
add_to_key_hash (GtkWindow      *window,
		 guint           keyval,
		 GdkModifierType modifiers,
		 gboolean        is_mnemonic,
		 gpointer        data)
{
  GtkKeyHash *key_hash = data;

  GtkWindowKeyEntry *entry = g_new (GtkWindowKeyEntry, 1);

  entry->keyval = keyval;
  entry->modifiers = modifiers;
  entry->is_mnemonic = is_mnemonic;

  /* GtkAccelGroup stores lowercased accelerators. To deal
   * with this, if <Shift> was specified, uppercase.
   */
  if (modifiers & GDK_SHIFT_MASK)
    {
      if (keyval == GDK_Tab)
	keyval = GDK_ISO_Left_Tab;
      else
	keyval = gdk_keyval_to_upper (keyval);
    }
  
  _gtk_key_hash_add_entry (key_hash, keyval, entry->modifiers, entry);
}

static GtkKeyHash *
gtk_window_get_key_hash (GtkWindow *window)
{
  GtkKeyHash *key_hash = g_object_get_data (G_OBJECT (window), "gtk-window-key-hash");
  if (key_hash)
    return key_hash;
  
  key_hash = _gtk_key_hash_new (gdk_keymap_get_default(), (GDestroyNotify)g_free);
  gtk_window_keys_foreach (window, add_to_key_hash, key_hash);
  g_object_set_data (G_OBJECT (window), "gtk-window-key-hash", key_hash);

  return key_hash;
}

static void
gtk_window_free_key_hash (GtkWindow *window)
{
  GtkKeyHash *key_hash = g_object_get_data (G_OBJECT (window), "gtk-window-key-hash");
  if (key_hash)
    {
      _gtk_key_hash_free (key_hash);
      g_object_set_data (G_OBJECT (window), "gtk-window-key-hash", NULL);
    }
}

/**
 * _gtk_window_activate_key:
 * @window: a #GtkWindow
 * @event: a #GdkEventKey
 * 
 * Activates mnemonics and accelerators for this #GtKWindow
 * 
 * Return value: %TRUE if a mnemonic or accelerator was found and activated.
 **/
gboolean
_gtk_window_activate_key (GtkWindow   *window,
			  GdkEventKey *event)
{
  GtkKeyHash *key_hash = g_object_get_data (G_OBJECT (window), "gtk-window-key-hash");
  GtkWindowKeyEntry *found_entry = NULL;

  if (!key_hash)
    {
      gtk_window_keys_changed (window);
      key_hash = g_object_get_data (G_OBJECT (window), "gtk-window-key-hash");
    }
  
  if (key_hash)
    {
      GSList *entries = _gtk_key_hash_lookup (key_hash,
					      event->hardware_keycode,
					      event->state & gtk_accelerator_get_default_mod_mask (),
					      event->group);
      GSList *tmp_list;

      for (tmp_list = entries; tmp_list; tmp_list = tmp_list->next)
	{
	  GtkWindowKeyEntry *entry = tmp_list->data;
	  if (entry->is_mnemonic)
	    {
	      found_entry = entry;
	      break;
	    }
	}
      
      if (!found_entry && entries)
	found_entry = entries->data;

      g_slist_free (entries);
    }

  if (found_entry)
    {
      if (found_entry->is_mnemonic)
	return gtk_window_mnemonic_activate (window, found_entry->keyval, found_entry->modifiers);
      else
	return gtk_accel_groups_activate (G_OBJECT (window), found_entry->keyval, found_entry->modifiers);
    }
  else
    return FALSE;
}
