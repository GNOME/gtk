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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtkwindow.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <graphene.h>

#include "gtkprivate.h"
#include "gtkwindowprivate.h"
#include "gtkaccelgroupprivate.h"
#include "gtkbindings.h"
#include "gtkcontainerprivate.h"
#include "gtkcsscornervalueprivate.h"
#include "gtkcssiconthemevalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkdebugupdatesprivate.h"
#include "gtkkeyhash.h"
#include "gtkmain.h"
#include "gtkmnemonichash.h"
#include "gtkmenubar.h"
#include "gtkmenushellprivate.h"
#include "gtkicontheme.h"
#include "gtkmarshalers.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkintl.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkbox.h"
#include "gtkbutton.h"
#include "gtkheaderbar.h"
#include "gtkheaderbarprivate.h"
#include "gtkpopoverprivate.h"
#include "a11y/gtkwindowaccessible.h"
#include "a11y/gtkcontaineraccessibleprivate.h"
#include "gtkapplicationprivate.h"
#include "gtkgestureprivate.h"
#include "inspector/init.h"
#include "inspector/window.h"
#include "gtkcssstylepropertyprivate.h"

#include "gdk/gdk-private.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadway.h"
#endif

#ifdef GDK_WINDOWING_MIR
#include "mir/gdkmir.h"
#endif

/**
 * SECTION:gtkwindow
 * @title: GtkWindow
 * @short_description: Toplevel which can contain other widgets
 *
 * A GtkWindow is a toplevel window which can contain other widgets.
 * Windows normally have decorations that are under the control
 * of the windowing system and allow the user to manipulate the window
 * (resize it, move it, close it,...).
 *
 * # GtkWindow as GtkBuildable
 *
 * The GtkWindow implementation of the GtkBuildable interface supports a
 * custom <accel-groups> element, which supports any number of <group>
 * elements representing the #GtkAccelGroup objects you want to add to
 * your window (synonymous with gtk_window_add_accel_group().
 *
 * It also supports the <initial-focus> element, whose name property names
 * the widget to receive the focus when the window is mapped.
 *
 * An example of a UI definition fragment with accel groups:
 * |[
 * <object class="GtkWindow">
 *   <accel-groups>
 *     <group name="accelgroup1"/>
 *   </accel-groups>
 *   <initial-focus name="thunderclap"/>
 * </object>
 * 
 * ...
 * 
 * <object class="GtkAccelGroup" id="accelgroup1"/>
 * ]|
 * 
 * The GtkWindow implementation of the GtkBuildable interface supports
 * setting a child as the titlebar by specifying “titlebar” as the “type”
 * attribute of a <child> element.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * window.background
 * ├── decoration
 * ├── <titlebar child>.titlebar [.default-decoration]
 * ╰── <child>
 * ]|
 *
 * GtkWindow has a main CSS node with name window and style class .background,
 * and a subnode with name decoration.
 *
 * Style classes that are typically used with the main CSS node are .csd (when
 * client-side decorations are in use), .solid-csd (for client-side decorations
 * without invisible borders), .ssd (used by mutter when rendering server-side
 * decorations). GtkWindow also represents window states with the following
 * style classes on the main node: .tiled, .maximized, .fullscreen. Specialized
 * types of window often add their own discriminating style classes, such as
 * .popup or .tooltip.
 *
 * GtkWindow adds the .titlebar and .default-decoration style classes to the
 * widget that is added as a titlebar child.
 */

#define MENU_BAR_ACCEL "F10"
#define RESIZE_HANDLE_SIZE 20
#define MNEMONICS_DELAY 300 /* ms */
#define NO_CONTENT_CHILD_NAT 200
/* In case the content (excluding header bar and shadows) of the window
 * would be empty, either because there is no visible child widget or only an
 * empty container widget, we use NO_CONTENT_CHILD_NAT as natural width/height
 * instead.
 */

typedef struct _GtkWindowPopover GtkWindowPopover;

struct _GtkWindowPopover
{
  GtkWidget *widget;
  GtkWidget *parent;
  GdkWindow *window;
  GtkPositionType pos;
  cairo_rectangle_int_t rect;
  gulong unmap_id;
  guint clamp_allocation : 1;
};

struct _GtkWindowPrivate
{
  GtkMnemonicHash       *mnemonic_hash;

  GtkWidget             *attach_widget;
  GtkWidget             *default_widget;
  GtkWidget             *initial_focus;
  GtkWidget             *focus_widget;
  GtkWindow             *transient_parent;
  GtkWindowGeometryInfo *geometry_info;
  GtkWindowGroup        *group;
  GdkScreen             *screen;
  GdkDisplay            *display;
  GtkApplication        *application;

  GList                 *popovers;

  GdkModifierType        mnemonic_modifier;

  gchar   *startup_id;
  gchar   *title;
  gchar   *wm_role;

  guint    keys_changed_handler;
  guint    delete_event_handler;

  guint32  initial_timestamp;

  guint16  configure_request_count;

  guint    mnemonics_display_timeout_id;

  gint     scale;

  gint title_height;
  GtkWidget *title_box;
  GtkWidget *titlebar;
  GtkWidget *popup_menu;

  GdkWindow *border_window[8];
  gint       initial_fullscreen_monitor;

  /* The following flags are initially TRUE (before a window is mapped).
   * They cause us to compute a configure request that involves
   * default-only parameters. Once mapped, we set them to FALSE.
   * Then we set them to TRUE again on unmap (for position)
   * and on unrealize (for size).
   */
  guint    need_default_position     : 1;
  guint    need_default_size         : 1;

  guint    above_initially           : 1;
  guint    accept_focus              : 1;
  guint    below_initially           : 1;
  guint    builder_visible           : 1;
  guint    configure_notify_received : 1;
  guint    decorated                 : 1;
  guint    deletable                 : 1;
  guint    destroy_with_parent       : 1;
  guint    focus_on_map              : 1;
  guint    fullscreen_initially      : 1;
  guint    has_user_ref_count        : 1;
  guint    iconify_initially         : 1; /* gtk_window_iconify() called before realization */
  guint    is_active                 : 1;
  guint    maximize_initially        : 1;
  guint    mnemonics_visible         : 1;
  guint    mnemonics_visible_set     : 1;
  guint    focus_visible             : 1;
  guint    modal                     : 1;
  guint    position                  : 3;
  guint    resizable                 : 1;
  guint    skips_pager               : 1;
  guint    skips_taskbar             : 1;
  guint    stick_initially           : 1;
  guint    transient_parent_group    : 1;
  guint    type                      : 4; /* GtkWindowType */
  guint    urgent                    : 1;
  guint    gravity                   : 5; /* GdkGravity */
  guint    csd_requested             : 1;
  guint    client_decorated          : 1; /* Decorations drawn client-side */
  guint    use_client_shadow         : 1; /* Decorations use client-side shadows */
  guint    maximized                 : 1;
  guint    fullscreen                : 1;
  guint    tiled                     : 1;

  guint    use_subsurface            : 1;

  GdkWindowTypeHint type_hint;

  GtkGesture *multipress_gesture;
  GtkGesture *drag_gesture;

  GdkWindow *hardcoded_window;

  GtkCssNode *decoration_node;

  GskRenderer *renderer;
};

static const GtkTargetEntry dnd_dest_targets [] = {
  { (char *) "application/x-rootwindow-drop", 0, 0 },
};

enum {
  SET_FOCUS,
  FRAME_EVENT,
  ACTIVATE_FOCUS,
  ACTIVATE_DEFAULT,
  KEYS_CHANGED,
  ENABLE_DEBUGGING,
  LAST_SIGNAL
};

enum {
  PROP_0,

  /* Construct */
  PROP_TYPE,

  /* Normal Props */
  PROP_TITLE,
  PROP_ROLE,
  PROP_RESIZABLE,
  PROP_MODAL,
  PROP_WIN_POS,
  PROP_DEFAULT_WIDTH,
  PROP_DEFAULT_HEIGHT,
  PROP_DESTROY_WITH_PARENT,
  PROP_HIDE_TITLEBAR_WHEN_MAXIMIZED,
  PROP_ICON,
  PROP_ICON_NAME,
  PROP_SCREEN,
  PROP_TYPE_HINT,
  PROP_SKIP_TASKBAR_HINT,
  PROP_SKIP_PAGER_HINT,
  PROP_URGENCY_HINT,
  PROP_ACCEPT_FOCUS,
  PROP_FOCUS_ON_MAP,
  PROP_DECORATED,
  PROP_DELETABLE,
  PROP_GRAVITY,
  PROP_TRANSIENT_FOR,
  PROP_ATTACHED_TO,
  PROP_APPLICATION,
  /* Readonly properties */
  PROP_IS_ACTIVE,

  /* Writeonly properties */
  PROP_STARTUP_ID,

  PROP_MNEMONICS_VISIBLE,
  PROP_FOCUS_VISIBLE,

  PROP_IS_MAXIMIZED,

  LAST_ARG
};

static GParamSpec *window_props[LAST_ARG] = { NULL, };

/* Must be kept in sync with GdkWindowEdge ! */
typedef enum
{
  GTK_WINDOW_REGION_EDGE_NW,
  GTK_WINDOW_REGION_EDGE_N,
  GTK_WINDOW_REGION_EDGE_NE,
  GTK_WINDOW_REGION_EDGE_W,
  GTK_WINDOW_REGION_EDGE_E,
  GTK_WINDOW_REGION_EDGE_SW,
  GTK_WINDOW_REGION_EDGE_S,
  GTK_WINDOW_REGION_EDGE_SE,
  GTK_WINDOW_REGION_CONTENT,
  GTK_WINDOW_REGION_TITLE,
} GtkWindowRegion;

typedef struct
{
  GList     *icon_list;
  gchar     *icon_name;
  guint      realized : 1;
  guint      using_default_icon : 1;
  guint      using_parent_icon : 1;
  guint      using_themed_icon : 1;
} GtkWindowIconInfo;

typedef struct {
  GdkGeometry    geometry; /* Last set of geometry hints we set */
  GdkWindowHints flags;
  GdkRectangle   configure_request;
} GtkWindowLastGeometryInfo;

struct _GtkWindowGeometryInfo
{
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


static void gtk_window_constructed        (GObject           *object);
static void gtk_window_dispose            (GObject           *object);
static void gtk_window_finalize           (GObject           *object);
static void gtk_window_destroy            (GtkWidget         *widget);
static void gtk_window_show               (GtkWidget         *widget);
static void gtk_window_hide               (GtkWidget         *widget);
static void gtk_window_map                (GtkWidget         *widget);
static void gtk_window_unmap              (GtkWidget         *widget);
static void gtk_window_realize            (GtkWidget         *widget);
static void gtk_window_unrealize          (GtkWidget         *widget);
static void gtk_window_size_allocate      (GtkWidget         *widget,
					   GtkAllocation     *allocation);
static gboolean gtk_window_map_event      (GtkWidget         *widget,
                                           GdkEventAny       *event);
static gint gtk_window_configure_event    (GtkWidget         *widget,
					   GdkEventConfigure *event);
static gboolean gtk_window_event          (GtkWidget         *widget,
                                           GdkEvent          *event);
static gint gtk_window_key_press_event    (GtkWidget         *widget,
					   GdkEventKey       *event);
static gint gtk_window_key_release_event  (GtkWidget         *widget,
					   GdkEventKey       *event);
static gint gtk_window_focus_in_event     (GtkWidget         *widget,
					   GdkEventFocus     *event);
static gint gtk_window_focus_out_event    (GtkWidget         *widget,
					   GdkEventFocus     *event);
static gboolean gtk_window_state_event    (GtkWidget          *widget,
                                           GdkEventWindowState *event);
static void gtk_window_remove             (GtkContainer      *container,
                                           GtkWidget         *widget);
static void gtk_window_check_resize       (GtkContainer      *container);
static void gtk_window_forall             (GtkContainer   *container,
					   GtkCallback     callback,
					   gpointer        callback_data);
static gint gtk_window_focus              (GtkWidget        *widget,
				           GtkDirectionType  direction);
static void gtk_window_move_focus         (GtkWidget         *widget,
                                           GtkDirectionType   dir);
static void gtk_window_real_set_focus     (GtkWindow         *window,
					   GtkWidget         *focus);

static void gtk_window_real_activate_default (GtkWindow         *window);
static void gtk_window_real_activate_focus   (GtkWindow         *window);
static void gtk_window_keys_changed          (GtkWindow         *window);
static gboolean gtk_window_enable_debugging  (GtkWindow         *window,
                                              gboolean           toggle);
static void gtk_window_snapshot                    (GtkWidget   *widget,
                                                    GtkSnapshot *snapshot);
static void gtk_window_unset_transient_for         (GtkWindow  *window);
static void gtk_window_transient_parent_realized   (GtkWidget  *parent,
						    GtkWidget  *window);
static void gtk_window_transient_parent_unrealized (GtkWidget  *parent,
						    GtkWidget  *window);

static GdkScreen *gtk_window_check_screen (GtkWindow *window);

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
static void     gtk_window_update_fixed_size         (GtkWindow    *window,
                                                      GdkGeometry  *new_geometry,
                                                      gint          new_width,
                                                      gint          new_height);
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

static void     update_themed_icon                    (GtkWindow    *window);
static GList   *icon_list_from_theme                  (GtkWindow    *window,
						       const gchar  *name);
static void     gtk_window_realize_icon               (GtkWindow    *window);
static void     gtk_window_unrealize_icon             (GtkWindow    *window);
static void     update_window_buttons                 (GtkWindow    *window);
static void     get_shadow_width                      (GtkWindow    *window,
                                                       GtkBorder    *shadow_width);

static GtkKeyHash *gtk_window_get_key_hash        (GtkWindow   *window);
static void        gtk_window_free_key_hash       (GtkWindow   *window);
#ifdef GDK_WINDOWING_X11
static void        gtk_window_on_theme_variant_changed (GtkSettings *settings,
                                                        GParamSpec  *pspec,
                                                        GtkWindow   *window);
#endif
static void        gtk_window_set_theme_variant         (GtkWindow  *window);

static void        gtk_window_do_popup         (GtkWindow      *window,
                                                GdkEventButton *event);
static void gtk_window_style_updated (GtkWidget     *widget);
static void gtk_window_state_flags_changed (GtkWidget     *widget,
                                            GtkStateFlags  previous_state);
static void gtk_window_queue_draw_region (GtkWidget            *widget,
				          const cairo_region_t *region);

static GSList      *toplevel_list = NULL;
static guint        window_signals[LAST_SIGNAL] = { 0 };
static GList       *default_icon_list = NULL;
static gchar       *default_icon_name = NULL;
static guint        default_icon_serial = 0;
static gboolean     disable_startup_notification = FALSE;

static GQuark       quark_gtk_window_key_hash = 0;
static GQuark       quark_gtk_window_icon_info = 0;
static GQuark       quark_gtk_buildable_accels = 0;

static GtkBuildableIface *parent_buildable_iface;

static void gtk_window_set_property (GObject         *object,
				     guint            prop_id,
				     const GValue    *value,
				     GParamSpec      *pspec);
static void gtk_window_get_property (GObject         *object,
				     guint            prop_id,
				     GValue          *value,
				     GParamSpec      *pspec);

/* GtkBuildable */
static void gtk_window_buildable_interface_init  (GtkBuildableIface *iface);
static void gtk_window_buildable_add_child (GtkBuildable *buildable,
                                            GtkBuilder   *builder,
                                            GObject      *child,
                                            const gchar  *type);
static void gtk_window_buildable_set_buildable_property (GtkBuildable        *buildable,
							 GtkBuilder          *builder,
							 const gchar         *name,
							 const GValue        *value);
static void gtk_window_buildable_parser_finished (GtkBuildable     *buildable,
						  GtkBuilder       *builder);
static gboolean gtk_window_buildable_custom_tag_start (GtkBuildable  *buildable,
						       GtkBuilder    *builder,
						       GObject       *child,
						       const gchar   *tagname,
						       GMarkupParser *parser,
						       gpointer      *data);
static void gtk_window_buildable_custom_finished (GtkBuildable  *buildable,
						      GtkBuilder    *builder,
						      GObject       *child,
						      const gchar   *tagname,
						      gpointer       user_data);

static void ensure_state_flag_backdrop (GtkWidget *widget);
static void unset_titlebar (GtkWindow *window);
static void on_titlebar_title_notify (GtkHeaderBar *titlebar,
                                      GParamSpec   *pspec,
                                      GtkWindow    *self);
static GtkWindowRegion get_active_region_type (GtkWindow   *window,
                                               GdkEventAny *event,
                                               gint         x,
                                               gint         y);

static void gtk_window_update_debugging (void);

G_DEFINE_TYPE_WITH_CODE (GtkWindow, gtk_window, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkWindow)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_window_buildable_interface_init))

static void
add_tab_bindings (GtkBindingSet    *binding_set,
		  GdkModifierType   modifiers,
		  GtkDirectionType  direction)
{
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Tab, modifiers,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Tab, modifiers,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
add_arrow_bindings (GtkBindingSet    *binding_set,
		    guint             keysym,
		    GtkDirectionType  direction)
{
  guint keypad_keysym = keysym - GDK_KEY_Left + GDK_KEY_KP_Left;
  
  gtk_binding_entry_add_signal (binding_set, keysym, 0,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, keysym, GDK_CONTROL_MASK,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, keypad_keysym, 0,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, keypad_keysym, GDK_CONTROL_MASK,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
}

static guint32
extract_time_from_startup_id (const gchar* startup_id)
{
  gchar *timestr = g_strrstr (startup_id, "_TIME");
  guint32 retval = GDK_CURRENT_TIME;

  if (timestr)
    {
      gchar *end;
      guint32 timestamp; 
    
      /* Skip past the "_TIME" part */
      timestr += 5;

      end = NULL;
      errno = 0;
      timestamp = g_ascii_strtoull (timestr, &end, 0);
      if (errno == 0 && end != timestr)
        retval = timestamp;
    }

  return retval;
}

static gboolean
startup_id_is_fake (const gchar* startup_id)
{
  return strncmp (startup_id, "_TIME", 5) == 0;
}

static void
gtk_window_measure (GtkWidget      *widget,
                    GtkOrientation  orientation,
                    int             for_size,
                    int            *minimum,
                    int            *natural,
                    int            *minimum_baseline,
                    int            *natural_baseline)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = window->priv;
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));
  gboolean has_size_request = gtk_widget_has_size_request (widget);
  int title_min_size = 0;
  int title_nat_size = 0;
  int child_min_size = 0;
  int child_nat_size = 0;
  GtkBorder window_border = { 0 };


  if (priv->decorated && !priv->fullscreen)
    {
      get_shadow_width (window, &window_border);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        for_size -= window_border.left + window_border.right;
      else
        for_size -= window_border.top + window_border.bottom;

      if (priv->title_box != NULL &&
          gtk_widget_get_visible (priv->title_box) &&
          gtk_widget_get_child_visible (priv->title_box))
        {
          int size = for_size;
          if (orientation == GTK_ORIENTATION_HORIZONTAL && for_size >= 0)
            gtk_widget_measure (priv->title_box,
                                GTK_ORIENTATION_VERTICAL,
                                -1,
                                NULL, &size,
                                NULL, NULL);

          gtk_widget_measure (priv->title_box,
                              orientation,
                              MAX (size, -1),
                              &title_min_size, &title_nat_size,
                              NULL, NULL);
        }
    }

  if (child != NULL && gtk_widget_get_visible (child))
    {
      gtk_widget_measure (child,
                          orientation,
                          MAX (for_size, -1),
                          &child_min_size, &child_nat_size,
                          NULL, NULL);

      if (child_nat_size == 0 && !has_size_request)
        child_nat_size = NO_CONTENT_CHILD_NAT;
    }
  else if (!has_size_request)
    {
      child_nat_size = NO_CONTENT_CHILD_NAT;
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      title_min_size += window_border.left + window_border.right;
      title_nat_size += window_border.left + window_border.right;
      child_min_size += window_border.left + window_border.right;
      child_nat_size += window_border.left + window_border.right;
      *minimum = MAX (title_min_size, child_min_size);
      *natural = MAX (title_nat_size, child_nat_size);
    }
  else
    {
      *minimum = title_min_size + child_min_size + window_border.top + window_border.bottom;
      *natural = title_nat_size + child_nat_size + window_border.top + window_border.bottom;
    }
}

static void
gtk_window_add (GtkContainer *container,
                GtkWidget    *child)
{
  /* Insert the child's css node now at the end so the order wrt. decoration_node is correct */
  gtk_css_node_insert_before (gtk_widget_get_css_node (GTK_WIDGET (container)),
                              gtk_widget_get_css_node (child),
                              NULL);

  GTK_CONTAINER_CLASS (gtk_window_parent_class)->add (container, child);
}

static void
gtk_window_class_init (GtkWindowClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkBindingSet *binding_set;

  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;
  
  quark_gtk_window_key_hash = g_quark_from_static_string ("gtk-window-key-hash");
  quark_gtk_window_icon_info = g_quark_from_static_string ("gtk-window-icon-info");
  quark_gtk_buildable_accels = g_quark_from_static_string ("gtk-window-buildable-accels");

  gobject_class->constructed = gtk_window_constructed;
  gobject_class->dispose = gtk_window_dispose;
  gobject_class->finalize = gtk_window_finalize;

  gobject_class->set_property = gtk_window_set_property;
  gobject_class->get_property = gtk_window_get_property;

  widget_class->destroy = gtk_window_destroy;
  widget_class->show = gtk_window_show;
  widget_class->hide = gtk_window_hide;
  widget_class->map = gtk_window_map;
  widget_class->map_event = gtk_window_map_event;
  widget_class->unmap = gtk_window_unmap;
  widget_class->realize = gtk_window_realize;
  widget_class->unrealize = gtk_window_unrealize;
  widget_class->size_allocate = gtk_window_size_allocate;
  widget_class->configure_event = gtk_window_configure_event;
  widget_class->event = gtk_window_event;
  widget_class->key_press_event = gtk_window_key_press_event;
  widget_class->key_release_event = gtk_window_key_release_event;
  widget_class->focus_in_event = gtk_window_focus_in_event;
  widget_class->focus_out_event = gtk_window_focus_out_event;
  widget_class->focus = gtk_window_focus;
  widget_class->move_focus = gtk_window_move_focus;
  widget_class->window_state_event = gtk_window_state_event;
  widget_class->measure = gtk_window_measure;
  widget_class->state_flags_changed = gtk_window_state_flags_changed;
  widget_class->style_updated = gtk_window_style_updated;
  widget_class->snapshot = gtk_window_snapshot;
  widget_class->queue_draw_region = gtk_window_queue_draw_region;

  container_class->add = gtk_window_add;
  container_class->remove = gtk_window_remove;
  container_class->check_resize = gtk_window_check_resize;
  container_class->forall = gtk_window_forall;

  klass->set_focus = gtk_window_real_set_focus;

  klass->activate_default = gtk_window_real_activate_default;
  klass->activate_focus = gtk_window_real_activate_focus;
  klass->keys_changed = gtk_window_keys_changed;
  klass->enable_debugging = gtk_window_enable_debugging;

  window_props[PROP_TYPE] =
      g_param_spec_enum ("type",
                         P_("Window Type"),
                         P_("The type of the window"),
                         GTK_TYPE_WINDOW_TYPE,
                         GTK_WINDOW_TOPLEVEL,
                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  window_props[PROP_TITLE] =
      g_param_spec_string ("title",
                           P_("Window Title"),
                           P_("The title of the window"),
                           NULL,
                           GTK_PARAM_READWRITE);

  window_props[PROP_ROLE] =
      g_param_spec_string ("role",
                           P_("Window Role"),
                           P_("Unique identifier for the window to be used when restoring a session"),
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkWindow:startup-id:
   *
   * The :startup-id is a write-only property for setting window's
   * startup notification identifier. See gtk_window_set_startup_id()
   * for more details.
   *
   * Since: 2.12
   */
  window_props[PROP_STARTUP_ID] =
      g_param_spec_string ("startup-id",
                           P_("Startup ID"),
                           P_("Unique startup identifier for the window used by startup-notification"),
                           NULL,
                           GTK_PARAM_WRITABLE);

  window_props[PROP_RESIZABLE] =
      g_param_spec_boolean ("resizable",
                            P_("Resizable"),
                            P_("If TRUE, users can resize the window"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  window_props[PROP_MODAL] =
      g_param_spec_boolean ("modal",
                            P_("Modal"),
                            P_("If TRUE, the window is modal (other windows are not usable while this one is up)"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  window_props[PROP_WIN_POS] =
      g_param_spec_enum ("window-position",
                         P_("Window Position"),
                         P_("The initial position of the window"),
                         GTK_TYPE_WINDOW_POSITION,
                         GTK_WIN_POS_NONE,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  window_props[PROP_DEFAULT_WIDTH] =
      g_param_spec_int ("default-width",
                        P_("Default Width"),
                        P_("The default width of the window, used when initially showing the window"),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  window_props[PROP_DEFAULT_HEIGHT] =
      g_param_spec_int ("default-height",
                        P_("Default Height"),
                        P_("The default height of the window, used when initially showing the window"),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  window_props[PROP_DESTROY_WITH_PARENT] =
      g_param_spec_boolean ("destroy-with-parent",
                            P_("Destroy with Parent"),
                            P_("If this window should be destroyed when the parent is destroyed"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:hide-titlebar-when-maximized:
   *
   * Whether the titlebar should be hidden during maximization.
   *
   * Since: 3.4
   */
  window_props[PROP_HIDE_TITLEBAR_WHEN_MAXIMIZED] =
      g_param_spec_boolean ("hide-titlebar-when-maximized",
                            P_("Hide the titlebar during maximization"),
                            P_("If this window’s titlebar should be hidden when the window is maximized"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  window_props[PROP_ICON] =
      g_param_spec_object ("icon",
                           P_("Icon"),
                           P_("Icon for this window"),
                           GDK_TYPE_PIXBUF,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:mnemonics-visible:
   *
   * Whether mnemonics are currently visible in this window.
   *
   * This property is maintained by GTK+ based on user input,
   * and should not be set by applications.
   *
   * Since: 2.20
   */
  window_props[PROP_MNEMONICS_VISIBLE] =
      g_param_spec_boolean ("mnemonics-visible",
                            P_("Mnemonics Visible"),
                            P_("Whether mnemonics are currently visible in this window"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:focus-visible:
   *
   * Whether 'focus rectangles' are currently visible in this window.
   *
   * This property is maintained by GTK+ based on user input
   * and should not be set by applications.
   *
   * Since: 2.20
   */
  window_props[PROP_FOCUS_VISIBLE] =
      g_param_spec_boolean ("focus-visible",
                            P_("Focus Visible"),
                            P_("Whether focus rectangles are currently visible in this window"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:icon-name:
   *
   * The :icon-name property specifies the name of the themed icon to
   * use as the window icon. See #GtkIconTheme for more details.
   *
   * Since: 2.6
   */
  window_props[PROP_ICON_NAME] =
      g_param_spec_string ("icon-name",
                           P_("Icon Name"),
                           P_("Name of the themed icon for this window"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  window_props[PROP_SCREEN] =
      g_param_spec_object ("screen",
                           P_("Screen"),
                           P_("The screen where this window will be displayed"),
                           GDK_TYPE_SCREEN,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  window_props[PROP_IS_ACTIVE] =
      g_param_spec_boolean ("is-active",
                            P_("Is Active"),
                            P_("Whether the toplevel is the current active window"),
                            FALSE,
                            GTK_PARAM_READABLE);

  window_props[PROP_TYPE_HINT] =
      g_param_spec_enum ("type-hint",
                         P_("Type hint"),
                         P_("Hint to help the desktop environment understand what kind of window this is and how to treat it."),
                         GDK_TYPE_WINDOW_TYPE_HINT,
                         GDK_WINDOW_TYPE_HINT_NORMAL,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  window_props[PROP_SKIP_TASKBAR_HINT] =
      g_param_spec_boolean ("skip-taskbar-hint",
                            P_("Skip taskbar"),
                            P_("TRUE if the window should not be in the task bar."),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  window_props[PROP_SKIP_PAGER_HINT] =
      g_param_spec_boolean ("skip-pager-hint",
                            P_("Skip pager"),
                            P_("TRUE if the window should not be in the pager."),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  window_props[PROP_URGENCY_HINT] =
      g_param_spec_boolean ("urgency-hint",
                            P_("Urgent"),
                            P_("TRUE if the window should be brought to the user’s attention."),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:accept-focus:
   *
   * Whether the window should receive the input focus.
   *
   * Since: 2.4
   */
  window_props[PROP_ACCEPT_FOCUS] =
      g_param_spec_boolean ("accept-focus",
                            P_("Accept focus"),
                            P_("TRUE if the window should receive the input focus."),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:focus-on-map:
   *
   * Whether the window should receive the input focus when mapped.
   *
   * Since: 2.6
   */
  window_props[PROP_FOCUS_ON_MAP] =
      g_param_spec_boolean ("focus-on-map",
                            P_("Focus on map"),
                            P_("TRUE if the window should receive the input focus when mapped."),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:decorated:
   *
   * Whether the window should be decorated by the window manager.
   *
   * Since: 2.4
   */
  window_props[PROP_DECORATED] =
      g_param_spec_boolean ("decorated",
                            P_("Decorated"),
                            P_("Whether the window should be decorated by the window manager"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:deletable:
   *
   * Whether the window frame should have a close button.
   *
   * Since: 2.10
   */
  window_props[PROP_DELETABLE] =
      g_param_spec_boolean ("deletable",
                            P_("Deletable"),
                            P_("Whether the window frame should have a close button"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:gravity:
   *
   * The window gravity of the window. See gtk_window_move() and #GdkGravity for
   * more details about window gravity.
   *
   * Since: 2.4
   */
  window_props[PROP_GRAVITY] =
      g_param_spec_enum ("gravity",
                         P_("Gravity"),
                         P_("The window gravity of the window"),
                         GDK_TYPE_GRAVITY,
                         GDK_GRAVITY_NORTH_WEST,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:transient-for:
   *
   * The transient parent of the window. See gtk_window_set_transient_for() for
   * more details about transient windows.
   *
   * Since: 2.10
   */
  window_props[PROP_TRANSIENT_FOR] =
      g_param_spec_object ("transient-for",
                           P_("Transient for Window"),
                           P_("The transient parent of the dialog"),
                           GTK_TYPE_WINDOW,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:attached-to:
   *
   * The widget to which this window is attached.
   * See gtk_window_set_attached_to().
   *
   * Examples of places where specifying this relation is useful are
   * for instance a #GtkMenu created by a #GtkComboBox, a completion
   * popup window created by #GtkEntry or a typeahead search entry
   * created by #GtkTreeView.
   *
   * Since: 3.4
   */
  window_props[PROP_ATTACHED_TO] =
      g_param_spec_object ("attached-to",
                           P_("Attached to Widget"),
                           P_("The widget where the window is attached"),
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  window_props[PROP_IS_MAXIMIZED] =
      g_param_spec_boolean ("is-maximized",
                            P_("Is maximized"),
                            P_("Whether the window is maximized"),
                            FALSE,
                            GTK_PARAM_READABLE);

  /**
   * GtkWindow:application:
   *
   * The #GtkApplication associated with the window.
   *
   * The application will be kept alive for at least as long as it
   * has any windows associated with it (see g_application_hold()
   * for a way to keep it alive without windows).
   *
   * Normally, the connection between the application and the window
   * will remain until the window is destroyed, but you can explicitly
   * remove it by setting the :application property to %NULL.
   *
   * Since: 3.0
   */
  window_props[PROP_APPLICATION] =
      g_param_spec_object ("application",
                           P_("GtkApplication"),
                           P_("The GtkApplication for the window"),
                           GTK_TYPE_APPLICATION,
                           GTK_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_ARG, window_props);

  /**
   * GtkWindow:set-focus:
   * @window: the window which received the signal
   * @widget: (nullable): the newly focused widget (or %NULL for no focus)
   *
   * This signal is emitted whenever the currently focused widget in
   * this window changes.
   *
   * Since: 2.24
   */
  window_signals[SET_FOCUS] =
    g_signal_new (I_("set-focus"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkWindowClass, set_focus),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_WIDGET);

  /**
   * GtkWindow::activate-focus:
   * @window: the window which received the signal
   *
   * The ::activate-focus signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user activates the currently
   * focused widget of @window.
   */
  window_signals[ACTIVATE_FOCUS] =
    g_signal_new (I_("activate-focus"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkWindowClass, activate_focus),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * GtkWindow::activate-default:
   * @window: the window which received the signal
   *
   * The ::activate-default signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user activates the default widget
   * of @window.
   */
  window_signals[ACTIVATE_DEFAULT] =
    g_signal_new (I_("activate-default"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkWindowClass, activate_default),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * GtkWindow::keys-changed:
   * @window: the window which received the signal
   *
   * The ::keys-changed signal gets emitted when the set of accelerators
   * or mnemonics that are associated with @window changes.
   */
  window_signals[KEYS_CHANGED] =
    g_signal_new (I_("keys-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWindowClass, keys_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * GtkWindow::enable-debugging:
   * @window: the window on which the signal is emitted
   * @toggle: toggle the debugger
   *
   * The ::enable-debugging signal is a [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user enables or disables interactive
   * debugging. When @toggle is %TRUE, interactive debugging is toggled
   * on or off, when it is %FALSE, the debugger will be pointed at the
   * widget under the pointer.
   *
   * The default bindings for this signal are Ctrl-Shift-I
   * and Ctrl-Shift-D.
   *
   * Return: %TRUE if the key binding was handled
   */
  window_signals[ENABLE_DEBUGGING] =
    g_signal_new (I_("enable-debugging"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkWindowClass, enable_debugging),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__BOOLEAN,
                  G_TYPE_BOOLEAN,
                  1, G_TYPE_BOOLEAN);

  /*
   * Key bindings
   */

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, 0,
                                "activate-focus", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, 0,
                                "activate-focus", 0);
  
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, 0,
                                "activate-default", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_ISO_Enter, 0,
                                "activate-default", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter, 0,
                                "activate-default", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_I, GDK_CONTROL_MASK|GDK_SHIFT_MASK,
                                "enable-debugging", 1,
                                G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_D, GDK_CONTROL_MASK|GDK_SHIFT_MASK,
                                "enable-debugging", 1,
                                G_TYPE_BOOLEAN, TRUE);

  add_arrow_bindings (binding_set, GDK_KEY_Up, GTK_DIR_UP);
  add_arrow_bindings (binding_set, GDK_KEY_Down, GTK_DIR_DOWN);
  add_arrow_bindings (binding_set, GDK_KEY_Left, GTK_DIR_LEFT);
  add_arrow_bindings (binding_set, GDK_KEY_Right, GTK_DIR_RIGHT);

  add_tab_bindings (binding_set, 0, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_WINDOW_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, "window");
}

/**
 * gtk_window_is_maximized:
 * @window: a #GtkWindow
 *
 * Retrieves the current maximized state of @window.
 *
 * Note that since maximization is ultimately handled by the window
 * manager and happens asynchronously to an application request, you
 * shouldn’t assume the return value of this function changing
 * immediately (or at all), as an effect of calling
 * gtk_window_maximize() or gtk_window_unmaximize().
 *
 * Returns: whether the window has a maximized state.
 *
 * Since: 3.12
 */
gboolean
gtk_window_is_maximized (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return priv->maximized;
}

void
_gtk_window_toggle_maximized (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  if (priv->maximized)
    gtk_window_unmaximize (window);
  else
    gtk_window_maximize (window);
}

static gboolean
send_delete_event (gpointer data)
{
  GtkWidget *window = data;
  GtkWindowPrivate *priv = GTK_WINDOW (window)->priv;

  GdkEvent *event;

  event = gdk_event_new (GDK_DELETE);

  event->any.window = g_object_ref (_gtk_widget_get_window (window));
  event->any.send_event = TRUE;
  priv->delete_event_handler = 0;

  gtk_main_do_event (event);
  gdk_event_free (event);

  return G_SOURCE_REMOVE;
}

/**
 * gtk_window_close:
 * @window: a #GtkWindow
 *
 * Requests that the window is closed, similar to what happens
 * when a window manager close button is clicked.
 *
 * This function can be used with close buttons in custom
 * titlebars.
 *
 * Since: 3.10
 */
void
gtk_window_close (GtkWindow *window)
{
  if (!_gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  window->priv->delete_event_handler = gdk_threads_add_idle_full (G_PRIORITY_DEFAULT, send_delete_event, window, NULL);
  g_source_set_name_by_id (window->priv->delete_event_handler, "[gtk+] send_delete_event");
}

static void
popover_destroy (GtkWindowPopover *popover)
{
  if (popover->unmap_id)
    {
      g_signal_handler_disconnect (popover->widget, popover->unmap_id);
      popover->unmap_id = 0;
    }

  if (popover->widget && _gtk_widget_get_parent (popover->widget))
    gtk_widget_unparent (popover->widget);

  if (popover->window)
    gdk_window_destroy (popover->window);

  g_free (popover);
}

static gboolean
gtk_window_titlebar_action (GtkWindow      *window,
                            const GdkEvent *event,
                            guint           button,
                            gint            n_press)
{
  GtkSettings *settings;
  gchar *action = NULL;
  gboolean retval = TRUE;

  settings = gtk_widget_get_settings (GTK_WIDGET (window));
  switch (button)
    {
    case GDK_BUTTON_PRIMARY:
      if (n_press == 2)
        g_object_get (settings, "gtk-titlebar-double-click", &action, NULL);
      break;
    case GDK_BUTTON_MIDDLE:
      g_object_get (settings, "gtk-titlebar-middle-click", &action, NULL);
      break;
    case GDK_BUTTON_SECONDARY:
      g_object_get (settings, "gtk-titlebar-right-click", &action, NULL);
      break;
    }

  if (action == NULL)
    retval = FALSE;
  else if (g_str_equal (action, "none"))
    retval = FALSE;
    /* treat all maximization variants the same */
  else if (g_str_has_prefix (action, "toggle-maximize"))
    {
      /*
       * gtk header bar won't show the maximize button if the following
       * properties are not met, apply the same to title bar actions for
       * consistency.
       */
      if (gtk_window_get_resizable (window) &&
          gtk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_NORMAL)
            _gtk_window_toggle_maximized (window);
    }
  else if (g_str_equal (action, "lower"))
    gdk_window_lower (_gtk_widget_get_window (GTK_WIDGET (window)));
  else if (g_str_equal (action, "minimize"))
    gdk_window_iconify (_gtk_widget_get_window (GTK_WIDGET (window)));
  else if (g_str_equal (action, "menu"))
    gtk_window_do_popup (window, (GdkEventButton*) event);
  else
    {
      g_warning ("Unsupported titlebar action %s", action);
      retval = FALSE;
    }

  g_free (action);

  return retval;
}

static void
multipress_gesture_pressed_cb (GtkGestureMultiPress *gesture,
                               gint                  n_press,
                               gdouble               x,
                               gdouble               y,
                               GtkWindow            *window)
{
  GtkWidget *event_widget, *widget;
  GdkEventSequence *sequence;
  GtkWindowRegion region;
  GtkWindowPrivate *priv;
  const GdkEvent *event;
  guint button;
  gboolean window_drag = FALSE;

  widget = GTK_WIDGET (window);
  priv = gtk_window_get_instance_private (window);
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  if (!event)
    return;

  if (n_press > 1)
    gtk_gesture_set_state (priv->drag_gesture, GTK_EVENT_SEQUENCE_DENIED);

  region = get_active_region_type (window, (GdkEventAny*) event, x, y);

  if (gdk_display_device_is_grabbed (gtk_widget_get_display (widget),
                                     gtk_gesture_get_device (GTK_GESTURE (gesture))))
    {
      gtk_gesture_set_state (priv->drag_gesture, GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  if (button == GDK_BUTTON_SECONDARY && region == GTK_WINDOW_REGION_TITLE)
    {
      if (gtk_window_titlebar_action (window, event, button, n_press))
        gtk_gesture_set_sequence_state (GTK_GESTURE (gesture),
                                        sequence, GTK_EVENT_SEQUENCE_CLAIMED);

      gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
      gtk_event_controller_reset (GTK_EVENT_CONTROLLER (priv->drag_gesture));
      return;
    }
  else if (button == GDK_BUTTON_MIDDLE && region == GTK_WINDOW_REGION_TITLE)
    {
      if (gtk_window_titlebar_action (window, event, button, n_press))
        gtk_gesture_set_sequence_state (GTK_GESTURE (gesture),
                                        sequence, GTK_EVENT_SEQUENCE_CLAIMED);
      return;
    }
  else if (button != GDK_BUTTON_PRIMARY)
    return;

  event_widget = gtk_get_event_widget ((GdkEvent *) event);

  if (region == GTK_WINDOW_REGION_TITLE)
    gdk_window_raise (_gtk_widget_get_window (widget));

  switch (region)
    {
    case GTK_WINDOW_REGION_CONTENT:
      if (event_widget != widget)
        {
          /* TODO: Have some way of enabling/disabling window-dragging on random widgets */
        }

      if (!window_drag)
        {
          gtk_gesture_set_sequence_state (GTK_GESTURE (gesture),
                                          sequence, GTK_EVENT_SEQUENCE_DENIED);
          return;
        }
      /* fall through */

    case GTK_WINDOW_REGION_TITLE:
      if (n_press == 2)
        gtk_window_titlebar_action (window, event, button, n_press);

      if (gtk_widget_has_grab (widget))
        gtk_gesture_set_sequence_state (GTK_GESTURE (gesture),
                                        sequence, GTK_EVENT_SEQUENCE_CLAIMED);
      break;
    default:
      if (!priv->maximized)
        {
          gdouble x_root, y_root;

          gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

          gdk_event_get_root_coords (event, &x_root, &y_root);
          gdk_window_begin_resize_drag_for_device (_gtk_widget_get_window (widget),
                                                   (GdkWindowEdge) region,
                                                   gdk_event_get_device ((GdkEvent *) event),
                                                   GDK_BUTTON_PRIMARY,
                                                   x_root, y_root,
                                                   gdk_event_get_time (event));

          gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
          gtk_event_controller_reset (GTK_EVENT_CONTROLLER (priv->drag_gesture));
        }

      break;
    }
}

static void
drag_gesture_begin_cb (GtkGestureDrag *gesture,
                       gdouble         x,
                       gdouble         y,
                       GtkWindow      *window)
{
  GdkEventSequence *sequence;
  GtkWindowRegion region;
  gboolean widget_drag = FALSE;
  const GdkEvent *event;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  if (!event)
    return;

  region = get_active_region_type (window, (GdkEventAny*) event, x, y);

  switch (region)
    {
      case GTK_WINDOW_REGION_TITLE:
        /* Claim it */
        break;
      case GTK_WINDOW_REGION_CONTENT:
          /* TODO: Have some way of enabling/disabling window-dragging on random widgets */

        if (!widget_drag)
          gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);

        break;
      default:
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
    }
}

static void
drag_gesture_update_cb (GtkGestureDrag *gesture,
                        gdouble         offset_x,
                        gdouble         offset_y,
                        GtkWindow      *window)
{
  GtkWindowPrivate *priv = window->priv;
  gint double_click_distance;
  GtkSettings *settings;

  settings = gtk_widget_get_settings (GTK_WIDGET (window));
  g_object_get (settings,
                "gtk-double-click-distance", &double_click_distance,
                NULL);

  if (ABS (offset_x) > double_click_distance ||
      ABS (offset_y) > double_click_distance)
    {
      GdkEventSequence *sequence;
      gdouble start_x, start_y;
      gint x_root, y_root;
      const GdkEvent *event;
      GtkWidget *event_widget;

      sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
      event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
      event_widget = gtk_get_event_widget ((GdkEvent *) event);

      /* Check whether the target widget should be left alone at handling
       * the sequence, this is better done late to give room for gestures
       * there to go denied.
       *
       * Besides claiming gestures, we must bail out too if there's gestures
       * in the "none" state at this point, as those are still handling events
       * and can potentially go claimed, and we don't want to stop the target
       * widget from doing anything.
       */
      if (event_widget != GTK_WIDGET (window) &&
          !gtk_widget_has_grab (event_widget) &&
          _gtk_widget_consumes_motion (event_widget, sequence))
        {
          gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
          return;
        }

      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

      gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);
      gdk_window_get_root_coords (_gtk_widget_get_window (GTK_WIDGET (window)),
                                  start_x, start_y, &x_root, &y_root);

      gdk_window_begin_move_drag_for_device (_gtk_widget_get_window (GTK_WIDGET (window)),
                                             gtk_gesture_get_device (GTK_GESTURE (gesture)),
                                             gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture)),
                                             x_root, y_root,
                                             gtk_get_current_event_time ());

      gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
      gtk_event_controller_reset (GTK_EVENT_CONTROLLER (priv->multipress_gesture));
    }
}

static void
node_style_changed_cb (GtkCssNode        *node,
                       GtkCssStyleChange *change,
                       GtkWidget         *widget)
{
  if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_SIZE | GTK_CSS_AFFECTS_CLIP))
    gtk_widget_queue_resize (widget);
  else
    gtk_widget_queue_draw (widget);
}

static void
gtk_window_init (GtkWindow *window)
{
  GtkWindowPrivate *priv;
  GtkWidget *widget;
  GtkCssNode *widget_node;

  widget = GTK_WIDGET (window);

  window->priv = gtk_window_get_instance_private (window);
  priv = window->priv;

  gtk_widget_set_has_window (widget, TRUE);
  _gtk_widget_set_is_toplevel (widget, TRUE);
  _gtk_widget_set_anchored (widget, TRUE);

  priv->title = NULL;
  priv->wm_role = NULL;
  priv->geometry_info = NULL;
  priv->type = GTK_WINDOW_TOPLEVEL;
  priv->focus_widget = NULL;
  priv->default_widget = NULL;
  priv->configure_request_count = 0;
  priv->resizable = TRUE;
  priv->configure_notify_received = FALSE;
  priv->position = GTK_WIN_POS_NONE;
  priv->need_default_size = TRUE;
  priv->need_default_position = TRUE;
  priv->modal = FALSE;
  priv->gravity = GDK_GRAVITY_NORTH_WEST;
  priv->decorated = TRUE;
  priv->mnemonic_modifier = GDK_MOD1_MASK;
  priv->screen = gdk_screen_get_default ();

  priv->accept_focus = TRUE;
  priv->focus_on_map = TRUE;
  priv->deletable = TRUE;
  priv->type_hint = GDK_WINDOW_TYPE_HINT_NORMAL;
  priv->startup_id = NULL;
  priv->initial_timestamp = GDK_CURRENT_TIME;
  priv->mnemonics_visible = TRUE;
  priv->focus_visible = TRUE;
  priv->initial_fullscreen_monitor = -1;

  g_object_ref_sink (window);
  priv->has_user_ref_count = TRUE;
  toplevel_list = g_slist_prepend (toplevel_list, window);
  gtk_window_update_debugging ();

#ifdef GDK_WINDOWING_X11
  g_signal_connect_object (gtk_settings_get_for_screen (priv->screen),
                           "notify::gtk-application-prefer-dark-theme",
                           G_CALLBACK (gtk_window_on_theme_variant_changed), window, 0);
#endif

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (window));
  priv->decoration_node = gtk_css_node_new ();
  gtk_css_node_set_name (priv->decoration_node, I_("decoration"));
  gtk_css_node_set_parent (priv->decoration_node, widget_node);
  gtk_css_node_set_state (priv->decoration_node, gtk_css_node_get_state (widget_node));
  g_signal_connect_object (priv->decoration_node, "style-changed", G_CALLBACK (node_style_changed_cb), window, 0);
  g_object_unref (priv->decoration_node);

  gtk_css_node_add_class (widget_node, g_quark_from_static_string (GTK_STYLE_CLASS_BACKGROUND));

  priv->scale = gtk_widget_get_scale_factor (widget);

  gtk_drag_dest_set (GTK_WIDGET (window),
                     GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                     dnd_dest_targets, G_N_ELEMENTS (dnd_dest_targets),
                     GDK_ACTION_MOVE);
}

static void
gtk_window_constructed (GObject *object)
{
  GtkWindow *window = GTK_WINDOW (object);
  GtkWindowPrivate *priv = window->priv;

  G_OBJECT_CLASS (gtk_window_parent_class)->constructed (object);

  if (priv->type == GTK_WINDOW_TOPLEVEL)
    {
      priv->multipress_gesture = gtk_gesture_multi_press_new (GTK_WIDGET (object));
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->multipress_gesture), 0);
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->multipress_gesture),
                                                  GTK_PHASE_NONE);
      g_signal_connect (priv->multipress_gesture, "pressed",
                        G_CALLBACK (multipress_gesture_pressed_cb), object);

      priv->drag_gesture = gtk_gesture_drag_new (GTK_WIDGET (object));
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->drag_gesture),
                                                  GTK_PHASE_CAPTURE);
      g_signal_connect (priv->drag_gesture, "drag-begin",
                        G_CALLBACK (drag_gesture_begin_cb), object);
      g_signal_connect (priv->drag_gesture, "drag-update",
                        G_CALLBACK (drag_gesture_update_cb), object);
    }
}

static void
gtk_window_set_property (GObject      *object,
			 guint         prop_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
  GtkWindow  *window = GTK_WINDOW (object);
  GtkWindowPrivate *priv = window->priv;

  switch (prop_id)
    {
    case PROP_TYPE:
      priv->type = g_value_get_enum (value);
      break;
    case PROP_TITLE:
      gtk_window_set_title (window, g_value_get_string (value));
      break;
    case PROP_ROLE:
      gtk_window_set_role (window, g_value_get_string (value));
      break;
    case PROP_STARTUP_ID:
      gtk_window_set_startup_id (window, g_value_get_string (value));
      break;
    case PROP_RESIZABLE:
      gtk_window_set_resizable (window, g_value_get_boolean (value));
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
    case PROP_ICON_NAME:
      gtk_window_set_icon_name (window, g_value_get_string (value));
      break;
    case PROP_SCREEN:
      gtk_window_set_screen (window, g_value_get_object (value));
      break;
    case PROP_TYPE_HINT:
      gtk_window_set_type_hint (window,
                                g_value_get_enum (value));
      break;
    case PROP_SKIP_TASKBAR_HINT:
      gtk_window_set_skip_taskbar_hint (window,
                                        g_value_get_boolean (value));
      break;
    case PROP_SKIP_PAGER_HINT:
      gtk_window_set_skip_pager_hint (window,
                                      g_value_get_boolean (value));
      break;
    case PROP_URGENCY_HINT:
      gtk_window_set_urgency_hint (window,
				   g_value_get_boolean (value));
      break;
    case PROP_ACCEPT_FOCUS:
      gtk_window_set_accept_focus (window,
				   g_value_get_boolean (value));
      break;
    case PROP_FOCUS_ON_MAP:
      gtk_window_set_focus_on_map (window,
				   g_value_get_boolean (value));
      break;
    case PROP_DECORATED:
      gtk_window_set_decorated (window, g_value_get_boolean (value));
      break;
    case PROP_DELETABLE:
      gtk_window_set_deletable (window, g_value_get_boolean (value));
      break;
    case PROP_GRAVITY:
      gtk_window_set_gravity (window, g_value_get_enum (value));
      break;
    case PROP_TRANSIENT_FOR:
      gtk_window_set_transient_for (window, g_value_get_object (value));
      break;
    case PROP_ATTACHED_TO:
      gtk_window_set_attached_to (window, g_value_get_object (value));
      break;
    case PROP_APPLICATION:
      gtk_window_set_application (window, g_value_get_object (value));
      break;
    case PROP_MNEMONICS_VISIBLE:
      gtk_window_set_mnemonics_visible (window, g_value_get_boolean (value));
      break;
    case PROP_FOCUS_VISIBLE:
      gtk_window_set_focus_visible (window, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_window_get_property (GObject      *object,
			 guint         prop_id,
			 GValue       *value,
			 GParamSpec   *pspec)
{
  GtkWindow  *window = GTK_WINDOW (object);
  GtkWindowPrivate *priv = window->priv;

  switch (prop_id)
    {
      GtkWindowGeometryInfo *info;
    case PROP_TYPE:
      g_value_set_enum (value, priv->type);
      break;
    case PROP_ROLE:
      g_value_set_string (value, priv->wm_role);
      break;
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_RESIZABLE:
      g_value_set_boolean (value, priv->resizable);
      break;
    case PROP_MODAL:
      g_value_set_boolean (value, priv->modal);
      break;
    case PROP_WIN_POS:
      g_value_set_enum (value, priv->position);
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
      g_value_set_boolean (value, priv->destroy_with_parent);
      break;
    case PROP_ICON:
      g_value_set_object (value, gtk_window_get_icon (window));
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, gtk_window_get_icon_name (window));
      break;
    case PROP_SCREEN:
      g_value_set_object (value, priv->screen);
      break;
    case PROP_IS_ACTIVE:
      g_value_set_boolean (value, priv->is_active);
      break;
    case PROP_TYPE_HINT:
      g_value_set_enum (value, priv->type_hint);
      break;
    case PROP_SKIP_TASKBAR_HINT:
      g_value_set_boolean (value,
                           gtk_window_get_skip_taskbar_hint (window));
      break;
    case PROP_SKIP_PAGER_HINT:
      g_value_set_boolean (value,
                           gtk_window_get_skip_pager_hint (window));
      break;
    case PROP_URGENCY_HINT:
      g_value_set_boolean (value,
                           gtk_window_get_urgency_hint (window));
      break;
    case PROP_ACCEPT_FOCUS:
      g_value_set_boolean (value,
                           gtk_window_get_accept_focus (window));
      break;
    case PROP_FOCUS_ON_MAP:
      g_value_set_boolean (value,
                           gtk_window_get_focus_on_map (window));
      break;
    case PROP_DECORATED:
      g_value_set_boolean (value, gtk_window_get_decorated (window));
      break;
    case PROP_DELETABLE:
      g_value_set_boolean (value, gtk_window_get_deletable (window));
      break;
    case PROP_GRAVITY:
      g_value_set_enum (value, gtk_window_get_gravity (window));
      break;
    case PROP_TRANSIENT_FOR:
      g_value_set_object (value, gtk_window_get_transient_for (window));
      break;
    case PROP_ATTACHED_TO:
      g_value_set_object (value, gtk_window_get_attached_to (window));
      break;
    case PROP_APPLICATION:
      g_value_set_object (value, gtk_window_get_application (window));
      break;
    case PROP_MNEMONICS_VISIBLE:
      g_value_set_boolean (value, priv->mnemonics_visible);
      break;
    case PROP_FOCUS_VISIBLE:
      g_value_set_boolean (value, priv->focus_visible);
      break;
    case PROP_IS_MAXIMIZED:
      g_value_set_boolean (value, gtk_window_is_maximized (window));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_window_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->set_buildable_property = gtk_window_buildable_set_buildable_property;
  iface->parser_finished = gtk_window_buildable_parser_finished;
  iface->custom_tag_start = gtk_window_buildable_custom_tag_start;
  iface->custom_finished = gtk_window_buildable_custom_finished;
  iface->add_child = gtk_window_buildable_add_child;
}

static void
gtk_window_buildable_add_child (GtkBuildable *buildable,
                                GtkBuilder   *builder,
                                GObject      *child,
                                const gchar  *type)
{
  if (type && strcmp (type, "titlebar") == 0)
    gtk_window_set_titlebar (GTK_WINDOW (buildable), GTK_WIDGET (child));
  else if (!type)
    gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
}

static void
gtk_window_buildable_set_buildable_property (GtkBuildable *buildable,
                                             GtkBuilder   *builder,
                                             const gchar  *name,
                                             const GValue *value)
{
  GtkWindow *window = GTK_WINDOW (buildable);
  GtkWindowPrivate *priv = window->priv;

  if (strcmp (name, "visible") == 0 && g_value_get_boolean (value))
    priv->builder_visible = TRUE;
  else
    parent_buildable_iface->set_buildable_property (buildable, builder, name, value);
}

typedef struct {
  gchar *name;
  gint line;
  gint col;
} ItemData;

static void
item_data_free (gpointer data)
{
  ItemData *item_data = data;

  g_free (item_data->name);
  g_free (item_data);
}

static void
item_list_free (gpointer data)
{
  GSList *list = data;

  g_slist_free_full (list, item_data_free);
}

static void
gtk_window_buildable_parser_finished (GtkBuildable *buildable,
				      GtkBuilder   *builder)
{
  GtkWindow *window = GTK_WINDOW (buildable);
  GtkWindowPrivate *priv = window->priv;
  GObject *object;
  GSList *accels, *l;

  if (priv->builder_visible)
    gtk_widget_show (GTK_WIDGET (buildable));

  accels = g_object_get_qdata (G_OBJECT (buildable), quark_gtk_buildable_accels);
  for (l = accels; l; l = l->next)
    {
      ItemData *data = l->data;

      object = _gtk_builder_lookup_object (builder, data->name, data->line, data->col);
      if (!object)
	continue;
      gtk_window_add_accel_group (GTK_WINDOW (buildable), GTK_ACCEL_GROUP (object));
    }

  g_object_set_qdata (G_OBJECT (buildable), quark_gtk_buildable_accels, NULL);

  parent_buildable_iface->parser_finished (buildable, builder);
}

typedef struct {
  GObject *object;
  GtkBuilder *builder;
  GSList *items;
} GSListSubParserData;

static void
window_start_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **names,
                      const gchar         **values,
                      gpointer              user_data,
                      GError              **error)
{
  GSListSubParserData *data = (GSListSubParserData*)user_data;

  if (strcmp (element_name, "group") == 0)
    {
      const gchar *name;
      ItemData *item_data;

      if (!_gtk_builder_check_parent (data->builder, context, "accel-groups", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      item_data = g_new (ItemData, 1);
      item_data->name = g_strdup (name);
      g_markup_parse_context_get_position (context, &item_data->line, &item_data->col);
      data->items = g_slist_prepend (data->items, item_data);
    }
  else if (strcmp (element_name, "accel-groups") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkWindow", element_name,
                                        error);
    }
}

static const GMarkupParser window_parser =
  {
    window_start_element
  };

typedef struct {
  GObject *object;
  GtkBuilder *builder;
  gchar *name;
  gint line;
  gint col;
} NameSubParserData;

static void
focus_start_element (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     const gchar         **names,
                     const gchar         **values,
                     gpointer              user_data,
                     GError              **error)
{
  NameSubParserData *data = (NameSubParserData*)user_data;

  if (strcmp (element_name, "initial-focus") == 0)
    {
      const gchar *name;

      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->name = g_strdup (name);
      g_markup_parse_context_get_position (context, &data->line, &data->col);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkWindow", element_name,
                                        error);
    }
}

static const GMarkupParser focus_parser =
{
  focus_start_element
};

static gboolean
gtk_window_buildable_custom_tag_start (GtkBuildable  *buildable,
                                       GtkBuilder    *builder,
                                       GObject       *child,
                                       const gchar   *tagname,
                                       GMarkupParser *parser,
                                       gpointer      *parser_data)
{
  if (parent_buildable_iface->custom_tag_start (buildable, builder, child,
						tagname, parser, parser_data))
    return TRUE;

  if (strcmp (tagname, "accel-groups") == 0)
    {
      GSListSubParserData *data;

      data = g_slice_new0 (GSListSubParserData);
      data->items = NULL;
      data->object = G_OBJECT (buildable);
      data->builder = builder;

      *parser = window_parser;
      *parser_data = data;

      return TRUE;
    }

  if (strcmp (tagname, "initial-focus") == 0)
    {
      NameSubParserData *data;

      data = g_slice_new0 (NameSubParserData);
      data->name = NULL;
      data->object = G_OBJECT (buildable);
      data->builder = builder;

      *parser = focus_parser;
      *parser_data = data;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_window_buildable_custom_finished (GtkBuildable  *buildable,
                                      GtkBuilder    *builder,
                                      GObject       *child,
                                      const gchar   *tagname,
                                      gpointer       user_data)
{
  parent_buildable_iface->custom_finished (buildable, builder, child,
					   tagname, user_data);

  if (strcmp (tagname, "accel-groups") == 0)
    {
      GSListSubParserData *data = (GSListSubParserData*)user_data;

      g_object_set_qdata_full (G_OBJECT (buildable), quark_gtk_buildable_accels,
                               data->items, (GDestroyNotify) item_list_free);

      g_slice_free (GSListSubParserData, data);
    }

  if (strcmp (tagname, "initial-focus") == 0)
    {
      NameSubParserData *data = (NameSubParserData*)user_data;

      if (data->name)
        {
          GObject *object;

          object = _gtk_builder_lookup_object (builder, data->name, data->line, data->col);
          if (object)
            gtk_window_set_focus (GTK_WINDOW (buildable), GTK_WIDGET (object));
          g_free (data->name);
        }

      g_slice_free (NameSubParserData, data);
    }
}

/**
 * gtk_window_new:
 * @type: type of window
 * 
 * Creates a new #GtkWindow, which is a toplevel window that can
 * contain other widgets. Nearly always, the type of the window should
 * be #GTK_WINDOW_TOPLEVEL. If you’re implementing something like a
 * popup menu from scratch (which is a bad idea, just use #GtkMenu),
 * you might use #GTK_WINDOW_POPUP. #GTK_WINDOW_POPUP is not for
 * dialogs, though in some other toolkits dialogs are called “popups”.
 * In GTK+, #GTK_WINDOW_POPUP means a pop-up menu or pop-up tooltip.
 * On X11, popup windows are not controlled by the
 * [window manager][gtk-X11-arch].
 *
 * If you simply want an undecorated window (no window borders), use
 * gtk_window_set_decorated(), don’t use #GTK_WINDOW_POPUP.
 *
 * All top-level windows created by gtk_window_new() are stored in
 * an internal top-level window list.  This list can be obtained from
 * gtk_window_list_toplevels().  Due to Gtk+ keeping a reference to
 * the window internally, gtk_window_new() does not return a reference
 * to the caller.
 *
 * To delete a #GtkWindow, call gtk_widget_destroy().
 * 
 * Returns: a new #GtkWindow.
 **/
GtkWidget*
gtk_window_new (GtkWindowType type)
{
  GtkWindow *window;

  g_return_val_if_fail (type >= GTK_WINDOW_TOPLEVEL && type <= GTK_WINDOW_POPUP, NULL);

  window = g_object_new (GTK_TYPE_WINDOW, "type", type, NULL);

  return GTK_WIDGET (window);
}

static void
gtk_window_set_title_internal (GtkWindow   *window,
                               const gchar *title,
                               gboolean     update_titlebar)
{
  GtkWindowPrivate *priv;
  GtkWidget *widget;
  char *new_title;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;
  widget = GTK_WIDGET (window);

  new_title = g_strdup (title);
  g_free (priv->title);
  priv->title = new_title;

  if (_gtk_widget_get_realized (widget))
    gdk_window_set_title (_gtk_widget_get_window (widget), new_title != NULL ? new_title : "");

  if (update_titlebar && GTK_IS_HEADER_BAR (priv->title_box))
    gtk_header_bar_set_title (GTK_HEADER_BAR (priv->title_box), new_title != NULL ? new_title : "");

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_TITLE]);
}

/**
 * gtk_window_set_title:
 * @window: a #GtkWindow
 * @title: title of the window
 * 
 * Sets the title of the #GtkWindow. The title of a window will be
 * displayed in its title bar; on the X Window System, the title bar
 * is rendered by the [window manager][gtk-X11-arch],
 * so exactly how the title appears to users may vary
 * according to a user’s exact configuration. The title should help a
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

  gtk_window_set_title_internal (window, title, TRUE);
}

/**
 * gtk_window_get_title:
 * @window: a #GtkWindow
 *
 * Retrieves the title of the window. See gtk_window_set_title().
 *
 * Returns: (nullable): the title of the window, or %NULL if none has
 * been set explicitly. The returned string is owned by the widget
 * and must not be modified or freed.
 **/
const gchar *
gtk_window_get_title (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return window->priv->title;
}

/**
 * gtk_window_set_role:
 * @window: a #GtkWindow
 * @role: unique identifier for the window to be used when restoring a session
 *
 * This function is only useful on X11, not with other GTK+ targets.
 * 
 * In combination with the window title, the window role allows a
 * [window manager][gtk-X11-arch] to identify "the
 * same" window when an application is restarted. So for example you
 * might set the “toolbox” role on your app’s toolbox window, so that
 * when the user restarts their session, the window manager can put
 * the toolbox back in the same place.
 *
 * If a window already has a unique title, you don’t need to set the
 * role, since the WM can use the title to identify the window when
 * restoring the session.
 * 
 **/
void
gtk_window_set_role (GtkWindow   *window,
                     const gchar *role)
{
  GtkWindowPrivate *priv;
  GtkWidget *widget;
  char *new_role;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;
  widget = GTK_WIDGET (window);

  new_role = g_strdup (role);
  g_free (priv->wm_role);
  priv->wm_role = new_role;

  if (_gtk_widget_get_realized (widget))
    gdk_window_set_role (_gtk_widget_get_window (widget), priv->wm_role);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_ROLE]);
}

/**
 * gtk_window_set_startup_id:
 * @window: a #GtkWindow
 * @startup_id: a string with startup-notification identifier
 *
 * Startup notification identifiers are used by desktop environment to 
 * track application startup, to provide user feedback and other 
 * features. This function changes the corresponding property on the
 * underlying GdkWindow. Normally, startup identifier is managed 
 * automatically and you should only use this function in special cases
 * like transferring focus from other processes. You should use this
 * function before calling gtk_window_present() or any equivalent
 * function generating a window map event.
 *
 * This function is only useful on X11, not with other GTK+ targets.
 * 
 * Since: 2.12
 **/
void
gtk_window_set_startup_id (GtkWindow   *window,
                           const gchar *startup_id)
{
  GtkWindowPrivate *priv;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;
  widget = GTK_WIDGET (window);

  g_free (priv->startup_id);
  priv->startup_id = g_strdup (startup_id);

  if (_gtk_widget_get_realized (widget))
    {
      GdkWindow *gdk_window;
      guint32 timestamp = extract_time_from_startup_id (priv->startup_id);

      gdk_window = _gtk_widget_get_window (widget);

#ifdef GDK_WINDOWING_X11
      if (timestamp != GDK_CURRENT_TIME && GDK_IS_X11_WINDOW(gdk_window))
	gdk_x11_window_set_user_time (gdk_window, timestamp);
#endif

      /* Here we differentiate real and "fake" startup notification IDs,
       * constructed on purpose just to pass interaction timestamp
       */
      if (startup_id_is_fake (priv->startup_id))
	gtk_window_present_with_time (window, timestamp);
      else 
        {
          gdk_window_set_startup_id (gdk_window,
                                     priv->startup_id);
          
          /* If window is mapped, terminate the startup-notification too */
          if (_gtk_widget_get_mapped (widget) &&
              !disable_startup_notification)
            gdk_notify_startup_complete_with_id (priv->startup_id);
        }
    }

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_STARTUP_ID]);
}

/**
 * gtk_window_get_role:
 * @window: a #GtkWindow
 *
 * Returns the role of the window. See gtk_window_set_role() for
 * further explanation.
 *
 * Returns: (nullable): the role of the window if set, or %NULL. The
 * returned is owned by the widget and must not be modified or freed.
 **/
const gchar *
gtk_window_get_role (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return window->priv->wm_role;
}

/**
 * gtk_window_set_focus:
 * @window: a #GtkWindow
 * @focus: (allow-none): widget to be the new focus widget, or %NULL to unset
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
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *parent;

  g_return_if_fail (GTK_IS_WINDOW (window));

  if (focus)
    {
      g_return_if_fail (GTK_IS_WIDGET (focus));
      g_return_if_fail (gtk_widget_get_can_focus (focus));

      if (!gtk_widget_get_visible (GTK_WIDGET (window)))
        priv->initial_focus = focus;
      else
        gtk_widget_grab_focus (focus);
    }
  else
    {
      /* Clear the existing focus chain, so that when we focus into
       * the window again, we start at the beginnning.
       */
      GtkWidget *widget = priv->focus_widget;
      if (widget)
	{
	  while ((parent = _gtk_widget_get_parent (widget)))
	    {
	      widget = parent;
              gtk_widget_set_focus_child (widget, NULL);
	    }
	}
      
      _gtk_window_internal_set_focus (window, NULL);
    }
}

void
_gtk_window_internal_set_focus (GtkWindow *window,
				GtkWidget *focus)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  priv->initial_focus = NULL;
  if ((priv->focus_widget != focus) ||
      (focus && !gtk_widget_has_focus (focus)))
    g_signal_emit (window, window_signals[SET_FOCUS], 0, focus);
}

/**
 * gtk_window_set_default:
 * @window: a #GtkWindow
 * @default_widget: (allow-none): widget to be the default, or %NULL
 *     to unset the default widget for the toplevel
 *
 * The default widget is the widget that’s activated when the user
 * presses Enter in a dialog (for example). This function sets or
 * unsets the default widget for a #GtkWindow. When setting (rather
 * than unsetting) the default widget it’s generally easier to call
 * gtk_widget_grab_default() on the widget. Before making a widget
 * the default widget, you must call gtk_widget_set_can_default() on
 * the widget you’d like to make the default.
 */
void
gtk_window_set_default (GtkWindow *window,
			GtkWidget *default_widget)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  if (default_widget)
    g_return_if_fail (gtk_widget_get_can_default (default_widget));

  if (priv->default_widget != default_widget)
    {
      GtkWidget *old_default_widget = NULL;
      
      if (default_widget)
	g_object_ref (default_widget);

      if (priv->default_widget)
	{
          old_default_widget = priv->default_widget;

          if (priv->focus_widget != priv->default_widget ||
              !gtk_widget_get_receives_default (priv->default_widget))
            _gtk_widget_set_has_default (priv->default_widget, FALSE);

          gtk_widget_queue_draw (priv->default_widget);
	}

      priv->default_widget = default_widget;

      if (priv->default_widget)
	{
          if (priv->focus_widget == NULL ||
              !gtk_widget_get_receives_default (priv->focus_widget))
            _gtk_widget_set_has_default (priv->default_widget, TRUE);

          gtk_widget_queue_draw (priv->default_widget);
	}

      if (old_default_widget)
	g_object_notify (G_OBJECT (old_default_widget), "has-default");
      
      if (default_widget)
	{
	  g_object_notify (G_OBJECT (default_widget), "has-default");
	  g_object_unref (default_widget);
	}
    }
}

/**
 * gtk_window_get_default_widget:
 * @window: a #GtkWindow
 *
 * Returns the default widget for @window. See
 * gtk_window_set_default() for more details.
 *
 * Returns: (nullable) (transfer none): the default widget, or %NULL
 * if there is none.
 *
 * Since: 2.14
 **/
GtkWidget *
gtk_window_get_default_widget (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return window->priv->default_widget;
}

static gboolean
handle_keys_changed (gpointer data)
{
  GtkWindow *window = GTK_WINDOW (data);
  GtkWindowPrivate *priv = window->priv;

  if (priv->keys_changed_handler)
    {
      g_source_remove (priv->keys_changed_handler);
      priv->keys_changed_handler = 0;
    }

  g_signal_emit (window, window_signals[KEYS_CHANGED], 0);
  
  return FALSE;
}

void
_gtk_window_notify_keys_changed (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  if (!priv->keys_changed_handler)
    {
      priv->keys_changed_handler = gdk_threads_add_idle (handle_keys_changed, window);
      g_source_set_name_by_id (priv->keys_changed_handler, "[gtk+] handle_keys_changed");
    }
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
  g_signal_connect_object (accel_group, "accel-changed",
			   G_CALLBACK (_gtk_window_notify_keys_changed),
			   window, G_CONNECT_SWAPPED);
  _gtk_window_notify_keys_changed (window);
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
					_gtk_window_notify_keys_changed,
					window);
  _gtk_accel_group_detach (accel_group, G_OBJECT (window));
  _gtk_window_notify_keys_changed (window);
}

static GtkMnemonicHash *
gtk_window_get_mnemonic_hash (GtkWindow *window,
			      gboolean   create)
{
  GtkWindowPrivate *private = window->priv;

  if (!private->mnemonic_hash && create)
    private->mnemonic_hash = _gtk_mnemonic_hash_new ();

  return private->mnemonic_hash;
}

/**
 * gtk_window_add_mnemonic:
 * @window: a #GtkWindow
 * @keyval: the mnemonic
 * @target: the widget that gets activated by the mnemonic
 *
 * Adds a mnemonic to this window.
 */
void
gtk_window_add_mnemonic (GtkWindow *window,
			 guint      keyval,
			 GtkWidget *target)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_WIDGET (target));

  _gtk_mnemonic_hash_add (gtk_window_get_mnemonic_hash (window, TRUE),
			  keyval, target);
  _gtk_window_notify_keys_changed (window);
}

/**
 * gtk_window_remove_mnemonic:
 * @window: a #GtkWindow
 * @keyval: the mnemonic
 * @target: the widget that gets activated by the mnemonic
 *
 * Removes a mnemonic from this window.
 */
void
gtk_window_remove_mnemonic (GtkWindow *window,
			    guint      keyval,
			    GtkWidget *target)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_WIDGET (target));
  
  _gtk_mnemonic_hash_remove (gtk_window_get_mnemonic_hash (window, TRUE),
			     keyval, target);
  _gtk_window_notify_keys_changed (window);
}

/**
 * gtk_window_mnemonic_activate:
 * @window: a #GtkWindow
 * @keyval: the mnemonic
 * @modifier: the modifiers
 *
 * Activates the targets associated with the mnemonic.
 *
 * Returns: %TRUE if the activation is done.
 */
gboolean
gtk_window_mnemonic_activate (GtkWindow      *window,
			      guint           keyval,
			      GdkModifierType modifier)
{
  GtkWindowPrivate *priv;

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  priv = window->priv;

  if (priv->mnemonic_modifier == (modifier & gtk_accelerator_get_default_mod_mask ()))
      {
	GtkMnemonicHash *mnemonic_hash = gtk_window_get_mnemonic_hash (window, FALSE);
	if (mnemonic_hash)
	  return _gtk_mnemonic_hash_activate (mnemonic_hash, keyval);
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
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail ((modifier & ~GDK_MODIFIER_MASK) == 0);

  priv = window->priv;

  priv->mnemonic_modifier = modifier;
  _gtk_window_notify_keys_changed (window);
}

/**
 * gtk_window_get_mnemonic_modifier:
 * @window: a #GtkWindow
 *
 * Returns the mnemonic modifier for this window. See
 * gtk_window_set_mnemonic_modifier().
 *
 * Returns: the modifier mask used to activate
 *               mnemonics on this window.
 **/
GdkModifierType
gtk_window_get_mnemonic_modifier (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), 0);

  return window->priv->mnemonic_modifier;
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
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  if (position == GTK_WIN_POS_CENTER_ALWAYS ||
      priv->position == GTK_WIN_POS_CENTER_ALWAYS)
    {
      GtkWindowGeometryInfo *info;

      info = gtk_window_get_geometry_info (window, TRUE);

      /* this flag causes us to re-request the CENTER_ALWAYS
       * constraint in gtk_window_move_resize(), see
       * comment in that function.
       */
      info->position_constraints_changed = TRUE;

      gtk_widget_queue_resize_no_redraw (GTK_WIDGET (window));
    }

  if (priv->position != position)
    {
      priv->position = position;
  
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_WIN_POS]);
    }
}

/**
 * gtk_window_activate_focus:
 * @window: a #GtkWindow
 * 
 * Activates the current focused widget within the window.
 * 
 * Returns: %TRUE if a widget got activated.
 **/
gboolean 
gtk_window_activate_focus (GtkWindow *window)
{
  GtkWindowPrivate *priv;

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  priv = window->priv;

  if (priv->focus_widget && gtk_widget_is_sensitive (priv->focus_widget))
    return gtk_widget_activate (priv->focus_widget);

  return FALSE;
}

/**
 * gtk_window_get_focus:
 * @window: a #GtkWindow
 * 
 * Retrieves the current focused widget within the window.
 * Note that this is the widget that would have the focus
 * if the toplevel window focused; if the toplevel window
 * is not focused then  `gtk_widget_has_focus (widget)` will
 * not be %TRUE for the widget.
 *
 * Returns: (nullable) (transfer none): the currently focused widget,
 * or %NULL if there is none.
 **/
GtkWidget *
gtk_window_get_focus (GtkWindow *window)
{
  GtkWindowPrivate *priv;

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  priv = window->priv;

  if (priv->initial_focus)
    return priv->initial_focus;
  else
    return priv->focus_widget;
}

/**
 * gtk_window_activate_default:
 * @window: a #GtkWindow
 * 
 * Activates the default widget for the window, unless the current 
 * focused widget has been configured to receive the default action 
 * (see gtk_widget_set_receives_default()), in which case the
 * focused widget is activated. 
 * 
 * Returns: %TRUE if a widget got activated.
 **/
gboolean
gtk_window_activate_default (GtkWindow *window)
{
  GtkWindowPrivate *priv;

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  priv = window->priv;

  if (priv->default_widget && gtk_widget_is_sensitive (priv->default_widget) &&
      (!priv->focus_widget || !gtk_widget_get_receives_default (priv->focus_widget)))
    return gtk_widget_activate (priv->default_widget);
  else if (priv->focus_widget && gtk_widget_is_sensitive (priv->focus_widget))
    return gtk_widget_activate (priv->focus_widget);

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
 * parent; most [window managers][gtk-X11-arch]
 * will then disallow lowering the dialog below the parent.
 * 
 * 
 **/
void
gtk_window_set_modal (GtkWindow *window,
		      gboolean   modal)
{
  GtkWindowPrivate *priv;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  modal = modal != FALSE;
  if (priv->modal == modal)
    return;

  priv->modal = modal;
  widget = GTK_WIDGET (window);
  
  /* adjust desired modality state */
  if (_gtk_widget_get_realized (widget))
    gdk_window_set_modal_hint (_gtk_widget_get_window (widget), priv->modal);

  if (gtk_widget_get_visible (widget))
    {
      if (priv->modal)
	gtk_grab_add (widget);
      else
	gtk_grab_remove (widget);
    }

  update_window_buttons (window);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_MODAL]);
}

/**
 * gtk_window_get_modal:
 * @window: a #GtkWindow
 * 
 * Returns whether the window is modal. See gtk_window_set_modal().
 *
 * Returns: %TRUE if the window is set to be modal and
 *               establishes a grab when shown
 **/
gboolean
gtk_window_get_modal (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->priv->modal;
}

/**
 * gtk_window_list_toplevels:
 * 
 * Returns a list of all existing toplevel windows. The widgets
 * in the list are not individually referenced. If you want
 * to iterate through the list and perform actions involving
 * callbacks that might destroy the widgets, you must call
 * `g_list_foreach (result, (GFunc)g_object_ref, NULL)` first, and
 * then unref all the widgets afterwards.
 *
 * Returns: (element-type GtkWidget) (transfer container): list of toplevel widgets
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

static void
remove_attach_widget (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  if (priv->attach_widget)
    {
      _gtk_widget_remove_attached_window (priv->attach_widget, window);

      priv->attach_widget = NULL;
    }
}

static void
gtk_window_dispose (GObject *object)
{
  GtkWindow *window = GTK_WINDOW (object);
  GtkWindowPrivate *priv = window->priv;

  gtk_window_set_focus (window, NULL);
  gtk_window_set_default (window, NULL);
  remove_attach_widget (window);

  G_OBJECT_CLASS (gtk_window_parent_class)->dispose (object);
  unset_titlebar (window);

  while (priv->popovers)
    {
      GtkWindowPopover *popover = priv->popovers->data;
      priv->popovers = g_list_delete_link (priv->popovers, priv->popovers);
      popover_destroy (popover);
    }

}

static void
parent_destroyed_callback (GtkWindow *parent, GtkWindow *child)
{
  gtk_widget_destroy (GTK_WIDGET (child));
}

static void
connect_parent_destroyed (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  if (priv->transient_parent)
    {
      g_signal_connect (priv->transient_parent,
                        "destroy",
                        G_CALLBACK (parent_destroyed_callback),
                        window);
    }  
}

static void
disconnect_parent_destroyed (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  if (priv->transient_parent)
    {
      g_signal_handlers_disconnect_by_func (priv->transient_parent,
					    parent_destroyed_callback,
					    window);
    }
}

static void
gtk_window_transient_parent_realized (GtkWidget *parent,
				      GtkWidget *window)
{
  if (_gtk_widget_get_realized (window))
    gdk_window_set_transient_for (_gtk_widget_get_window (window),
                                  _gtk_widget_get_window (parent));
}

static void
gtk_window_transient_parent_unrealized (GtkWidget *parent,
					GtkWidget *window)
{
  if (_gtk_widget_get_realized (window))
    gdk_property_delete (_gtk_widget_get_window (window),
			 gdk_atom_intern_static_string ("WM_TRANSIENT_FOR"));
}

static void
gtk_window_transient_parent_screen_changed (GtkWindow	*parent,
					    GParamSpec	*pspec,
					    GtkWindow   *window)
{
  gtk_window_set_screen (window, parent->priv->screen);
}

static void       
gtk_window_unset_transient_for  (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  if (priv->transient_parent)
    {
      g_signal_handlers_disconnect_by_func (priv->transient_parent,
					    gtk_window_transient_parent_realized,
					    window);
      g_signal_handlers_disconnect_by_func (priv->transient_parent,
					    gtk_window_transient_parent_unrealized,
					    window);
      g_signal_handlers_disconnect_by_func (priv->transient_parent,
					    gtk_window_transient_parent_screen_changed,
					    window);
      g_signal_handlers_disconnect_by_func (priv->transient_parent,
					    gtk_widget_destroyed,
					    &priv->transient_parent);

      if (priv->destroy_with_parent)
        disconnect_parent_destroyed (window);

      priv->transient_parent = NULL;

      if (priv->transient_parent_group)
	{
	  priv->transient_parent_group = FALSE;
	  gtk_window_group_remove_window (priv->group,
					  window);
	}
    }
}

/**
 * gtk_window_set_transient_for:
 * @window: a #GtkWindow
 * @parent: (allow-none): parent window, or %NULL
 *
 * Dialog windows should be set transient for the main application
 * window they were spawned from. This allows
 * [window managers][gtk-X11-arch] to e.g. keep the
 * dialog on top of the main window, or center the dialog over the
 * main window. gtk_dialog_new_with_buttons() and other convenience
 * functions in GTK+ will sometimes call
 * gtk_window_set_transient_for() on your behalf.
 *
 * Passing %NULL for @parent unsets the current transient window.
 *
 * On Wayland, this function can also be used to attach a new
 * #GTK_WINDOW_POPUP to a #GTK_WINDOW_TOPLEVEL parent already mapped
 * on screen so that the #GTK_WINDOW_POPUP will be created as a
 * subsurface-based window #GDK_WINDOW_SUBSURFACE which can be
 * positioned at will relatively to the #GTK_WINDOW_TOPLEVEL surface.
 *
 * On Windows, this function puts the child window on top of the parent,
 * much as the window manager would have done on X.
 */
void
gtk_window_set_transient_for  (GtkWindow *window,
			       GtkWindow *parent)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
  g_return_if_fail (window != parent);

  priv = window->priv;

  if (priv->transient_parent)
    {
      if (_gtk_widget_get_realized (GTK_WIDGET (window)) &&
          _gtk_widget_get_realized (GTK_WIDGET (priv->transient_parent)) &&
          (!parent || !_gtk_widget_get_realized (GTK_WIDGET (parent))))
	gtk_window_transient_parent_unrealized (GTK_WIDGET (priv->transient_parent),
						GTK_WIDGET (window));

      gtk_window_unset_transient_for (window);
    }

  priv->transient_parent = parent;

  if (parent)
    {
      g_signal_connect (parent, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&priv->transient_parent);
      g_signal_connect (parent, "realize",
			G_CALLBACK (gtk_window_transient_parent_realized),
			window);
      g_signal_connect (parent, "unrealize",
			G_CALLBACK (gtk_window_transient_parent_unrealized),
			window);
      g_signal_connect (parent, "notify::screen",
			G_CALLBACK (gtk_window_transient_parent_screen_changed),
			window);

      gtk_window_set_screen (window, parent->priv->screen);

      if (priv->destroy_with_parent)
        connect_parent_destroyed (window);
      
      if (_gtk_widget_get_realized (GTK_WIDGET (window)) &&
	  _gtk_widget_get_realized (GTK_WIDGET (parent)))
	gtk_window_transient_parent_realized (GTK_WIDGET (parent),
					      GTK_WIDGET (window));

      if (parent->priv->group)
	{
	  gtk_window_group_add_window (parent->priv->group, window);
	  priv->transient_parent_group = TRUE;
	}
    }

  update_window_buttons (window);
}

/**
 * gtk_window_get_transient_for:
 * @window: a #GtkWindow
 *
 * Fetches the transient parent for this window. See
 * gtk_window_set_transient_for().
 *
 * Returns: (nullable) (transfer none): the transient parent for this
 * window, or %NULL if no transient parent has been set.
 **/
GtkWindow *
gtk_window_get_transient_for (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return window->priv->transient_parent;
}

/**
 * gtk_window_set_attached_to:
 * @window: a #GtkWindow
 * @attach_widget: (allow-none): a #GtkWidget, or %NULL
 *
 * Marks @window as attached to @attach_widget. This creates a logical binding
 * between the window and the widget it belongs to, which is used by GTK+ to
 * propagate information such as styling or accessibility to @window as if it
 * was a children of @attach_widget.
 *
 * Examples of places where specifying this relation is useful are for instance
 * a #GtkMenu created by a #GtkComboBox, a completion popup window
 * created by #GtkEntry or a typeahead search entry created by #GtkTreeView.
 *
 * Note that this function should not be confused with
 * gtk_window_set_transient_for(), which specifies a window manager relation
 * between two toplevels instead.
 *
 * Passing %NULL for @attach_widget detaches the window.
 *
 * Since: 3.4
 **/
void
gtk_window_set_attached_to (GtkWindow *window,
                            GtkWidget *attach_widget)
{
  GtkStyleContext *context;
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_WIDGET (window) != attach_widget);

  priv = window->priv;

  if (priv->attach_widget == attach_widget)
    return;

  remove_attach_widget (window);

  priv->attach_widget = attach_widget;

  if (priv->attach_widget)
    {
      _gtk_widget_add_attached_window (priv->attach_widget, window);
    }

  /* Update the style, as the widget path might change. */
  context = gtk_widget_get_style_context (GTK_WIDGET (window));
  if (priv->attach_widget)
    gtk_style_context_set_parent (context, gtk_widget_get_style_context (priv->attach_widget));
  else
    gtk_style_context_set_parent (context, NULL);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_ATTACHED_TO]);
}

/**
 * gtk_window_get_attached_to:
 * @window: a #GtkWindow
 *
 * Fetches the attach widget for this window. See
 * gtk_window_set_attached_to().
 *
 * Returns: (nullable) (transfer none): the widget where the window
 * is attached, or %NULL if the window is not attached to any widget.
 *
 * Since: 3.4
 **/
GtkWidget *
gtk_window_get_attached_to (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return window->priv->attach_widget;
}

/**
 * gtk_window_get_application:
 * @window: a #GtkWindow
 *
 * Gets the #GtkApplication associated with the window (if any).
 *
 * Returns: (nullable) (transfer none): a #GtkApplication, or %NULL
 *
 * Since: 3.0
 **/
GtkApplication *
gtk_window_get_application (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return window->priv->application;
}

static void
gtk_window_release_application (GtkWindow *window)
{
  if (window->priv->application)
    {
      GtkApplication *application;

      /* steal reference into temp variable */
      application = window->priv->application;
      window->priv->application = NULL;

      gtk_application_remove_window (application, window);
      g_object_unref (application);
    }
}

/**
 * gtk_window_set_application:
 * @window: a #GtkWindow
 * @application: (allow-none): a #GtkApplication, or %NULL
 *
 * Sets or unsets the #GtkApplication associated with the window.
 *
 * The application will be kept alive for at least as long as the window
 * is open.
 *
 * Since: 3.0
 **/
void
gtk_window_set_application (GtkWindow      *window,
                            GtkApplication *application)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;
  if (priv->application != application)
    {
      gtk_window_release_application (window);

      priv->application = application;

      if (priv->application != NULL)
        {
          g_object_ref (priv->application);

          gtk_application_add_window (priv->application, window);
        }

      _gtk_widget_update_parent_muxer (GTK_WIDGET (window));

      _gtk_window_notify_keys_changed (window);

      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_APPLICATION]);
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
  GtkWindowPrivate *priv;
  GdkWindow *gdk_window;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  if (priv->type_hint == hint)
    return;

  priv->type_hint = hint;

  gdk_window = _gtk_widget_get_window (GTK_WIDGET (window));
  if (gdk_window)
    gdk_window_set_type_hint (gdk_window, hint);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_TYPE_HINT]);

  update_window_buttons (window);
}

/**
 * gtk_window_get_type_hint:
 * @window: a #GtkWindow
 *
 * Gets the type hint for this window. See gtk_window_set_type_hint().
 *
 * Returns: the type hint for @window.
 **/
GdkWindowTypeHint
gtk_window_get_type_hint (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), GDK_WINDOW_TYPE_HINT_NORMAL);

  return window->priv->type_hint;
}

/**
 * gtk_window_set_skip_taskbar_hint:
 * @window: a #GtkWindow 
 * @setting: %TRUE to keep this window from appearing in the task bar
 * 
 * Windows may set a hint asking the desktop environment not to display
 * the window in the task bar. This function sets this hint.
 * 
 * Since: 2.2
 **/
void
gtk_window_set_skip_taskbar_hint (GtkWindow *window,
                                  gboolean   setting)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  setting = setting != FALSE;

  if (priv->skips_taskbar != setting)
    {
      priv->skips_taskbar = setting;
      if (_gtk_widget_get_realized (GTK_WIDGET (window)))
        gdk_window_set_skip_taskbar_hint (_gtk_widget_get_window (GTK_WIDGET (window)),
                                          priv->skips_taskbar);
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_SKIP_TASKBAR_HINT]);
    }
}

/**
 * gtk_window_get_skip_taskbar_hint:
 * @window: a #GtkWindow
 * 
 * Gets the value set by gtk_window_set_skip_taskbar_hint()
 * 
 * Returns: %TRUE if window shouldn’t be in taskbar
 * 
 * Since: 2.2
 **/
gboolean
gtk_window_get_skip_taskbar_hint (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->priv->skips_taskbar;
}

/**
 * gtk_window_set_skip_pager_hint:
 * @window: a #GtkWindow 
 * @setting: %TRUE to keep this window from appearing in the pager
 * 
 * Windows may set a hint asking the desktop environment not to display
 * the window in the pager. This function sets this hint.
 * (A "pager" is any desktop navigation tool such as a workspace
 * switcher that displays a thumbnail representation of the windows
 * on the screen.)
 * 
 * Since: 2.2
 **/
void
gtk_window_set_skip_pager_hint (GtkWindow *window,
                                gboolean   setting)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  setting = setting != FALSE;

  if (priv->skips_pager != setting)
    {
      priv->skips_pager = setting;
      if (_gtk_widget_get_realized (GTK_WIDGET (window)))
        gdk_window_set_skip_pager_hint (_gtk_widget_get_window (GTK_WIDGET (window)),
                                        priv->skips_pager);
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_SKIP_PAGER_HINT]);
    }
}

/**
 * gtk_window_get_skip_pager_hint:
 * @window: a #GtkWindow
 * 
 * Gets the value set by gtk_window_set_skip_pager_hint().
 * 
 * Returns: %TRUE if window shouldn’t be in pager
 * 
 * Since: 2.2
 **/
gboolean
gtk_window_get_skip_pager_hint (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->priv->skips_pager;
}

/**
 * gtk_window_set_urgency_hint:
 * @window: a #GtkWindow 
 * @setting: %TRUE to mark this window as urgent
 * 
 * Windows may set a hint asking the desktop environment to draw
 * the users attention to the window. This function sets this hint.
 * 
 * Since: 2.8
 **/
void
gtk_window_set_urgency_hint (GtkWindow *window,
			     gboolean   setting)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  setting = setting != FALSE;

  if (priv->urgent != setting)
    {
      priv->urgent = setting;
      if (_gtk_widget_get_realized (GTK_WIDGET (window)))
        gdk_window_set_urgency_hint (_gtk_widget_get_window (GTK_WIDGET (window)),
				     priv->urgent);
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_URGENCY_HINT]);
    }
}

/**
 * gtk_window_get_urgency_hint:
 * @window: a #GtkWindow
 * 
 * Gets the value set by gtk_window_set_urgency_hint()
 * 
 * Returns: %TRUE if window is urgent
 * 
 * Since: 2.8
 **/
gboolean
gtk_window_get_urgency_hint (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->priv->urgent;
}

/**
 * gtk_window_set_accept_focus:
 * @window: a #GtkWindow 
 * @setting: %TRUE to let this window receive input focus
 * 
 * Windows may set a hint asking the desktop environment not to receive
 * the input focus. This function sets this hint.
 * 
 * Since: 2.4
 **/
void
gtk_window_set_accept_focus (GtkWindow *window,
			     gboolean   setting)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  setting = setting != FALSE;

  if (priv->accept_focus != setting)
    {
      priv->accept_focus = setting;
      if (_gtk_widget_get_realized (GTK_WIDGET (window)))
        gdk_window_set_accept_focus (_gtk_widget_get_window (GTK_WIDGET (window)),
				     priv->accept_focus);
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_ACCEPT_FOCUS]);
    }
}

/**
 * gtk_window_get_accept_focus:
 * @window: a #GtkWindow
 * 
 * Gets the value set by gtk_window_set_accept_focus().
 * 
 * Returns: %TRUE if window should receive the input focus
 * 
 * Since: 2.4
 **/
gboolean
gtk_window_get_accept_focus (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->priv->accept_focus;
}

/**
 * gtk_window_set_focus_on_map:
 * @window: a #GtkWindow 
 * @setting: %TRUE to let this window receive input focus on map
 * 
 * Windows may set a hint asking the desktop environment not to receive
 * the input focus when the window is mapped.  This function sets this
 * hint.
 * 
 * Since: 2.6
 **/
void
gtk_window_set_focus_on_map (GtkWindow *window,
			     gboolean   setting)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  setting = setting != FALSE;

  if (priv->focus_on_map != setting)
    {
      priv->focus_on_map = setting;
      if (_gtk_widget_get_realized (GTK_WIDGET (window)))
        gdk_window_set_focus_on_map (_gtk_widget_get_window (GTK_WIDGET (window)),
				     priv->focus_on_map);
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_FOCUS_ON_MAP]);
    }
}

/**
 * gtk_window_get_focus_on_map:
 * @window: a #GtkWindow
 * 
 * Gets the value set by gtk_window_set_focus_on_map().
 * 
 * Returns: %TRUE if window should receive the input focus when
 * mapped.
 * 
 * Since: 2.6
 **/
gboolean
gtk_window_get_focus_on_map (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->priv->focus_on_map;
}

/**
 * gtk_window_set_destroy_with_parent:
 * @window: a #GtkWindow
 * @setting: whether to destroy @window with its transient parent
 * 
 * If @setting is %TRUE, then destroying the transient parent of @window
 * will also destroy @window itself. This is useful for dialogs that
 * shouldn’t persist beyond the lifetime of the main window they're
 * associated with, for example.
 **/
void
gtk_window_set_destroy_with_parent  (GtkWindow *window,
                                     gboolean   setting)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  if (priv->destroy_with_parent == (setting != FALSE))
    return;

  if (priv->destroy_with_parent)
    {
      disconnect_parent_destroyed (window);
    }
  else
    {
      connect_parent_destroyed (window);
    }

  priv->destroy_with_parent = setting;

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_DESTROY_WITH_PARENT]);
}

/**
 * gtk_window_get_destroy_with_parent:
 * @window: a #GtkWindow
 * 
 * Returns whether the window will be destroyed with its transient parent. See
 * gtk_window_set_destroy_with_parent ().
 *
 * Returns: %TRUE if the window will be destroyed with its transient parent.
 **/
gboolean
gtk_window_get_destroy_with_parent (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->priv->destroy_with_parent;
}

static GtkWindowGeometryInfo*
gtk_window_get_geometry_info (GtkWindow *window,
			      gboolean   create)
{
  GtkWindowPrivate *priv = window->priv;
  GtkWindowGeometryInfo *info;

  info = priv->geometry_info;
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
      priv->geometry_info = info;
    }

  return info;
}

static void
unset_titlebar (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  if (priv->title_box != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->title_box,
                                            on_titlebar_title_notify,
                                            window);
      gtk_widget_unparent (priv->title_box);
      priv->title_box = NULL;
      priv->titlebar = NULL;
    }
}

static gboolean
gtk_window_supports_client_shadow (GtkWindow *window)
{
  GdkDisplay *display;
  GdkScreen *screen;

  screen = _gtk_window_get_screen (window);
  display = gdk_screen_get_display (screen);

  if (!gdk_display_is_rgba (display))
    return FALSE;

  if (!gdk_display_is_composited (display))
    return FALSE;

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    {
      if (!gdk_x11_screen_supports_net_wm_hint (screen, gdk_atom_intern_static_string ("_GTK_FRAME_EXTENTS")))
        return FALSE;
    }
#endif

  return TRUE;
}

static void
gtk_window_enable_csd (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;
  GtkWidget *widget = GTK_WIDGET (window);

  /* We need a visual with alpha for client shadows */
  if (priv->use_client_shadow)
    {
      gtk_style_context_add_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_CSD);
    }
  else
    {
      gtk_style_context_add_class (gtk_widget_get_style_context (widget), "solid-csd");
    }

  priv->client_decorated = TRUE;
}

static void
on_titlebar_title_notify (GtkHeaderBar *titlebar,
                          GParamSpec   *pspec,
                          GtkWindow    *self)
{
  const gchar *title;

  title = gtk_header_bar_get_title (titlebar);
  gtk_window_set_title_internal (self, title, FALSE);
}

/**
 * gtk_window_set_titlebar:
 * @window: a #GtkWindow
 * @titlebar: (allow-none): the widget to use as titlebar
 *
 * Sets a custom titlebar for @window.
 *
 * If you set a custom titlebar, GTK+ will do its best to convince
 * the window manager not to put its own titlebar on the window.
 * Depending on the system, this function may not work for a window
 * that is already visible, so you set the titlebar before calling
 * gtk_widget_show().
 *
 * Since: 3.10
 */
void
gtk_window_set_titlebar (GtkWindow *window,
                         GtkWidget *titlebar)
{
  GtkWidget *widget = GTK_WIDGET (window);
  GtkWindowPrivate *priv = window->priv;
  gboolean was_mapped;

  g_return_if_fail (GTK_IS_WINDOW (window));

  if ((!priv->title_box && titlebar) || (priv->title_box && !titlebar))
    {
      was_mapped = _gtk_widget_get_mapped (widget);
      if (_gtk_widget_get_realized (widget))
        {
          g_warning ("gtk_window_set_titlebar() called on a realized window");
          gtk_widget_unrealize (widget);
        }
    }
  else
    was_mapped = FALSE;

  unset_titlebar (window);

  if (titlebar == NULL)
    {
      priv->client_decorated = FALSE;
      gtk_style_context_remove_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_CSD);

      goto out;
    }

  priv->use_client_shadow = gtk_window_supports_client_shadow (window);

  gtk_window_enable_csd (window);
  priv->title_box = titlebar;
  /* Same reason as in gtk_window_add */
  gtk_css_node_insert_before (gtk_widget_get_css_node (GTK_WIDGET (window)),
                              gtk_widget_get_css_node (titlebar),
                              NULL);

  gtk_widget_set_parent (priv->title_box, widget);
  if (GTK_IS_HEADER_BAR (titlebar))
    {
      g_signal_connect (titlebar, "notify::title",
                        G_CALLBACK (on_titlebar_title_notify), window);
      on_titlebar_title_notify (GTK_HEADER_BAR (titlebar), NULL, window);
    }

  gtk_style_context_add_class (gtk_widget_get_style_context (titlebar),
                               GTK_STYLE_CLASS_TITLEBAR);

out:
  if (was_mapped)
    gtk_widget_map (widget);
}

/**
 * gtk_window_get_titlebar:
 * @window: a #GtkWindow
 *
 * Returns the custom titlebar that has been set with
 * gtk_window_set_titlebar().
 *
 * Returns: (nullable) (transfer none): the custom titlebar, or %NULL
 *
 * Since: 3.16
 */
GtkWidget *
gtk_window_get_titlebar (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  /* Don't return the internal titlebar */
  if (priv->title_box == priv->titlebar)
    return NULL;

  return priv->title_box;
}

gboolean
_gtk_window_titlebar_shows_app_menu (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  if (GTK_IS_HEADER_BAR (priv->title_box))
    return _gtk_header_bar_shows_app_menu (GTK_HEADER_BAR (priv->title_box));

  return FALSE;
}

/**
 * gtk_window_set_decorated:
 * @window: a #GtkWindow
 * @setting: %TRUE to decorate the window
 *
 * By default, windows are decorated with a title bar, resize
 * controls, etc.  Some [window managers][gtk-X11-arch]
 * allow GTK+ to disable these decorations, creating a
 * borderless window. If you set the decorated property to %FALSE
 * using this function, GTK+ will do its best to convince the window
 * manager not to decorate the window. Depending on the system, this
 * function may not have any effect when called on a window that is
 * already visible, so you should call it before calling gtk_widget_show().
 *
 * On Windows, this function always works, since there’s no window manager
 * policy involved.
 * 
 **/
void
gtk_window_set_decorated (GtkWindow *window,
                          gboolean   setting)
{
  GtkWindowPrivate *priv;
  GdkWindow *gdk_window;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  setting = setting != FALSE;

  if (setting == priv->decorated)
    return;

  priv->decorated = setting;

  gdk_window = _gtk_widget_get_window (GTK_WIDGET (window));
  if (gdk_window)
    {
      if (priv->decorated)
        {
          if (priv->client_decorated)
            gdk_window_set_decorations (gdk_window, 0);
          else
            gdk_window_set_decorations (gdk_window, GDK_DECOR_ALL);
        }
      else
        gdk_window_set_decorations (gdk_window, 0);
    }

  update_window_buttons (window);
  gtk_widget_queue_resize (GTK_WIDGET (window));

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_DECORATED]);
}

/**
 * gtk_window_get_decorated:
 * @window: a #GtkWindow
 *
 * Returns whether the window has been set to have decorations
 * such as a title bar via gtk_window_set_decorated().
 *
 * Returns: %TRUE if the window has been set to have decorations
 **/
gboolean
gtk_window_get_decorated (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), TRUE);

  return window->priv->decorated;
}

/**
 * gtk_window_set_deletable:
 * @window: a #GtkWindow
 * @setting: %TRUE to decorate the window as deletable
 *
 * By default, windows have a close button in the window frame. Some 
 * [window managers][gtk-X11-arch] allow GTK+ to 
 * disable this button. If you set the deletable property to %FALSE
 * using this function, GTK+ will do its best to convince the window
 * manager not to show a close button. Depending on the system, this
 * function may not have any effect when called on a window that is
 * already visible, so you should call it before calling gtk_widget_show().
 *
 * On Windows, this function always works, since there’s no window manager
 * policy involved.
 *
 * Since: 2.10
 */
void
gtk_window_set_deletable (GtkWindow *window,
			  gboolean   setting)
{
  GtkWindowPrivate *priv;
  GdkWindow *gdk_window;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  setting = setting != FALSE;

  if (setting == priv->deletable)
    return;

  priv->deletable = setting;

  gdk_window = _gtk_widget_get_window (GTK_WIDGET (window));
  if (gdk_window)
    {
      if (priv->deletable)
        gdk_window_set_functions (gdk_window,
				  GDK_FUNC_ALL);
      else
        gdk_window_set_functions (gdk_window,
				  GDK_FUNC_ALL | GDK_FUNC_CLOSE);
    }

  update_window_buttons (window);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_DELETABLE]);
}

/**
 * gtk_window_get_deletable:
 * @window: a #GtkWindow
 *
 * Returns whether the window has been set to have a close button
 * via gtk_window_set_deletable().
 *
 * Returns: %TRUE if the window has been set to have a close button
 *
 * Since: 2.10
 **/
gboolean
gtk_window_get_deletable (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), TRUE);

  return window->priv->deletable;
}

static GtkWindowIconInfo*
get_icon_info (GtkWindow *window)
{
  return g_object_get_qdata (G_OBJECT (window), quark_gtk_window_icon_info);
}
     
static void
free_icon_info (GtkWindowIconInfo *info)
{
  g_free (info->icon_name);
  g_slice_free (GtkWindowIconInfo, info);
}


static GtkWindowIconInfo*
ensure_icon_info (GtkWindow *window)
{
  GtkWindowIconInfo *info;

  info = get_icon_info (window);
  
  if (info == NULL)
    {
      info = g_slice_new0 (GtkWindowIconInfo);
      g_object_set_qdata_full (G_OBJECT (window),
                              quark_gtk_window_icon_info,
                              info,
                              (GDestroyNotify)free_icon_info);
    }

  return info;
}

static GList *
icon_list_from_theme (GtkWindow   *window,
		      const gchar *name)
{
  GList *list;

  GtkIconTheme *icon_theme;
  GdkPixbuf *icon;
  gint *sizes;
  gint i;

  icon_theme = gtk_css_icon_theme_value_get_icon_theme
    (_gtk_style_context_peek_property (gtk_widget_get_style_context (GTK_WIDGET (window)),
                                       GTK_CSS_PROPERTY_ICON_THEME));

  sizes = gtk_icon_theme_get_icon_sizes (icon_theme, name);

  list = NULL;
  for (i = 0; sizes[i]; i++)
    {
      /* FIXME
       * We need an EWMH extension to handle scalable icons 
       * by passing their name to the WM. For now just use a 
       * fixed size of 48.
       */ 
      if (sizes[i] == -1)
	icon = gtk_icon_theme_load_icon (icon_theme, name,
					 48, 0, NULL);
      else
	icon = gtk_icon_theme_load_icon (icon_theme, name,
					 sizes[i], 0, NULL);
      if (icon)
	list = g_list_append (list, icon);
    }

  g_free (sizes);

  return list;
}

static void
gtk_window_realize_icon (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;
  GtkWidget *widget;
  GtkWindowIconInfo *info;
  GdkWindow *gdk_window;
  GList *icon_list;

  widget = GTK_WIDGET (window);
  gdk_window = _gtk_widget_get_window (widget);

  g_return_if_fail (gdk_window != NULL);

  /* no point setting an icon on override-redirect */
  if (priv->type == GTK_WINDOW_POPUP)
    return;

  icon_list = NULL;
  
  info = ensure_icon_info (window);

  if (info->realized)
    return;

  info->using_default_icon = FALSE;
  info->using_parent_icon = FALSE;
  info->using_themed_icon = FALSE;
  
  icon_list = info->icon_list;

  /* Look up themed icon */
  if (icon_list == NULL && info->icon_name) 
    {
      icon_list = icon_list_from_theme (window, info->icon_name);
      if (icon_list)
	info->using_themed_icon = TRUE;
    }

  /* Inherit from transient parent */
  if (icon_list == NULL && priv->transient_parent)
    {
      icon_list = ensure_icon_info (priv->transient_parent)->icon_list;
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

  /* Look up themed icon */
  if (icon_list == NULL && default_icon_name) 
    {
      icon_list = icon_list_from_theme (window, default_icon_name);
      info->using_default_icon = TRUE;
      info->using_themed_icon = TRUE;
    }

  info->realized = TRUE;

  gdk_window_set_icon_list (_gtk_widget_get_window (widget), icon_list);
  if (GTK_IS_HEADER_BAR (priv->title_box))
    _gtk_header_bar_update_window_icon (GTK_HEADER_BAR (priv->title_box), window);

  if (info->using_themed_icon) 
    {
      g_list_free_full (icon_list, g_object_unref);
    }
}

static GdkPixbuf *
icon_from_list (GList *list,
                gint   size)
{
  GdkPixbuf *best;
  GdkPixbuf *pixbuf;
  GList *l;

  best = NULL;
  for (l = list; l; l = l->next)
    {
      pixbuf = list->data;
      if (gdk_pixbuf_get_width (pixbuf) <= size)
        {
          best = g_object_ref (pixbuf);
          break;
        }
    }

  if (best == NULL)
    best = gdk_pixbuf_scale_simple (GDK_PIXBUF (list->data), size, size, GDK_INTERP_BILINEAR);

  return best;
}

static GdkPixbuf *
icon_from_name (const gchar *name,
                gint         size)
{
  return gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                   name, size,
                                   GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
}

GdkPixbuf *
gtk_window_get_icon_for_size (GtkWindow *window,
                              gint       size)
{
  GtkWindowPrivate *priv = window->priv;
  GtkWindowIconInfo *info;
  const gchar *name;

  info = ensure_icon_info (window);

  if (info->icon_list != NULL)
    return icon_from_list (info->icon_list, size);

  name = gtk_window_get_icon_name (window);
  if (name != NULL)
    return icon_from_name (name, size);

  if (priv->transient_parent != NULL)
    {
      info = ensure_icon_info (priv->transient_parent);
      if (info->icon_list)
        return icon_from_list (info->icon_list, size);
    }

  if (default_icon_list != NULL)
    return icon_from_list (default_icon_list, size);

  if (default_icon_name != NULL)
    return icon_from_name (default_icon_name, size);

  return NULL;
}

static void
gtk_window_unrealize_icon (GtkWindow *window)
{
  GtkWindowIconInfo *info;

  info = get_icon_info (window);

  if (info == NULL)
    return;
  
  /* We don't clear the properties on the window, just figure the
   * window is going away.
   */

  info->realized = FALSE;

}

/**
 * gtk_window_set_icon_list:
 * @window: a #GtkWindow
 * @list: (element-type GdkPixbuf): list of #GdkPixbuf
 *
 * Sets up the icon representing a #GtkWindow. The icon is used when
 * the window is minimized (also known as iconified).  Some window
 * managers or desktop environments may also place it in the window
 * frame, or display it in other contexts. On others, the icon is not
 * used at all, so your mileage may vary.
 *
 * gtk_window_set_icon_list() allows you to pass in the same icon in
 * several hand-drawn sizes. The list should contain the natural sizes
 * your icon is available in; that is, don’t scale the image before
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
 * icon from their transient parent. So there’s no need to explicitly
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

  g_list_foreach (list,
                  (GFunc) g_object_ref, NULL);

  g_list_free_full (info->icon_list, g_object_unref);

  info->icon_list = g_list_copy (list);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_ICON]);
  
  gtk_window_unrealize_icon (window);
  
  if (_gtk_widget_get_realized (GTK_WIDGET (window)))
    gtk_window_realize_icon (window);

  /* We could try to update our transient children, but I don't think
   * it's really worth it. If we did it, the best way would probably
   * be to have children connect to notify::icon-list
   */
}

/**
 * gtk_window_get_icon_list:
 * @window: a #GtkWindow
 * 
 * Retrieves the list of icons set by gtk_window_set_icon_list().
 * The list is copied, but the reference count on each
 * member won’t be incremented.
 *
 * Returns: (element-type GdkPixbuf) (transfer container): copy of window’s icon list
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
 * @icon: (allow-none): icon image, or %NULL
 *
 * Sets up the icon representing a #GtkWindow. This icon is used when
 * the window is minimized (also known as iconified).  Some window
 * managers or desktop environments may also place it in the window
 * frame, or display it in other contexts. On others, the icon is not
 * used at all, so your mileage may vary.
 *
 * The icon should be provided in whatever size it was naturally
 * drawn; that is, don’t scale the image before passing it to
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

  if (icon)
    list = g_list_append (list, icon);
  
  gtk_window_set_icon_list (window, list);
  g_list_free (list);  
}


static void 
update_themed_icon (GtkWindow *window)
{
  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_ICON_NAME]);
  
  gtk_window_unrealize_icon (window);
  
  if (_gtk_widget_get_realized (GTK_WIDGET (window)))
    gtk_window_realize_icon (window);  
}

/**
 * gtk_window_set_icon_name:
 * @window: a #GtkWindow
 * @name: (allow-none): the name of the themed icon
 *
 * Sets the icon for the window from a named themed icon.
 * See the docs for #GtkIconTheme for more details.
 * On some platforms, the window icon is not used at all.
 *
 * Note that this has nothing to do with the WM_ICON_NAME 
 * property which is mentioned in the ICCCM.
 *
 * Since: 2.6
 */
void 
gtk_window_set_icon_name (GtkWindow   *window,
			  const gchar *name)
{
  GtkWindowIconInfo *info;
  gchar *tmp;
  
  g_return_if_fail (GTK_IS_WINDOW (window));

  info = ensure_icon_info (window);

  if (g_strcmp0 (info->icon_name, name) == 0)
    return;

  tmp = info->icon_name;
  info->icon_name = g_strdup (name);
  g_free (tmp);

  g_list_free_full (info->icon_list, g_object_unref);
  info->icon_list = NULL;
  
  update_themed_icon (window);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_ICON_NAME]);
}

/**
 * gtk_window_get_icon_name:
 * @window: a #GtkWindow
 *
 * Returns the name of the themed icon for the window,
 * see gtk_window_set_icon_name().
 *
 * Returns: (nullable): the icon name or %NULL if the window has
 * no themed icon
 *
 * Since: 2.6
 */
const gchar *
gtk_window_get_icon_name (GtkWindow *window)
{
  GtkWindowIconInfo *info;

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  info = ensure_icon_info (window);

  return info->icon_name;
}

/**
 * gtk_window_get_icon:
 * @window: a #GtkWindow
 * 
 * Gets the value set by gtk_window_set_icon() (or if you've
 * called gtk_window_set_icon_list(), gets the first icon in
 * the icon list).
 *
 * Returns: (transfer none): icon for window
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

/* Load pixbuf, printing warning on failure if error == NULL
 */
static GdkPixbuf *
load_pixbuf_verbosely (const char *filename,
		       GError    **err)
{
  GError *local_err = NULL;
  GdkPixbuf *pixbuf;

  pixbuf = gdk_pixbuf_new_from_file (filename, &local_err);

  if (!pixbuf)
    {
      if (err)
	*err = local_err;
      else
	{
	  g_warning ("Error loading icon from file '%s':\n\t%s",
		     filename, local_err->message);
	  g_error_free (local_err);
	}
    }

  return pixbuf;
}

/**
 * gtk_window_set_icon_from_file:
 * @window: a #GtkWindow
 * @filename: (type filename): location of icon file
 * @err: (allow-none): location to store error, or %NULL.
 *
 * Sets the icon for @window.
 * Warns on failure if @err is %NULL.
 *
 * This function is equivalent to calling gtk_window_set_icon()
 * with a pixbuf created by loading the image from @filename.
 *
 * Returns: %TRUE if setting the icon succeeded.
 *
 * Since: 2.2
 **/
gboolean
gtk_window_set_icon_from_file (GtkWindow   *window,
			       const gchar *filename,
			       GError     **err)
{
  GdkPixbuf *pixbuf = load_pixbuf_verbosely (filename, err);

  if (pixbuf)
    {
      gtk_window_set_icon (window, pixbuf);
      g_object_unref (pixbuf);
      
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_window_set_default_icon_list:
 * @list: (element-type GdkPixbuf) (transfer container): a list of #GdkPixbuf
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

  /* Update serial so we don't used cached pixmaps/masks
   */
  default_icon_serial++;
  
  g_list_foreach (list,
                  (GFunc) g_object_ref, NULL);

  g_list_free_full (default_icon_list, g_object_unref);

  default_icon_list = g_list_copy (list);
  
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
          if (_gtk_widget_get_realized (GTK_WIDGET (w)))
            gtk_window_realize_icon (w);
        }

      tmp_list = tmp_list->next;
    }
  g_list_free (toplevels);
}

/**
 * gtk_window_set_default_icon:
 * @icon: the icon
 *
 * Sets an icon to be used as fallback for windows that haven't
 * had gtk_window_set_icon() called on them from a pixbuf.
 *
 * Since: 2.4
 **/
void
gtk_window_set_default_icon (GdkPixbuf *icon)
{
  GList *list;
  
  g_return_if_fail (GDK_IS_PIXBUF (icon));

  list = g_list_prepend (NULL, icon);
  gtk_window_set_default_icon_list (list);
  g_list_free (list);
}

/**
 * gtk_window_set_default_icon_name:
 * @name: the name of the themed icon
 *
 * Sets an icon to be used as fallback for windows that haven't
 * had gtk_window_set_icon_list() called on them from a named
 * themed icon, see gtk_window_set_icon_name().
 *
 * Since: 2.6
 **/
void
gtk_window_set_default_icon_name (const gchar *name)
{
  GList *tmp_list;
  GList *toplevels;

  /* Update serial so we don't used cached pixmaps/masks
   */
  default_icon_serial++;

  g_free (default_icon_name);
  default_icon_name = g_strdup (name);

  g_list_free_full (default_icon_list, g_object_unref);
  default_icon_list = NULL;
  
  /* Update all toplevels */
  toplevels = gtk_window_list_toplevels ();
  tmp_list = toplevels;
  while (tmp_list != NULL)
    {
      GtkWindowIconInfo *info;
      GtkWindow *w = tmp_list->data;
      
      info = get_icon_info (w);
      if (info && info->using_default_icon && info->using_themed_icon)
        {
          gtk_window_unrealize_icon (w);
          if (_gtk_widget_get_realized (GTK_WIDGET (w)))
            gtk_window_realize_icon (w);
        }

      tmp_list = tmp_list->next;
    }
  g_list_free (toplevels);
}

/**
 * gtk_window_get_default_icon_name:
 *
 * Returns the fallback icon name for windows that has been set
 * with gtk_window_set_default_icon_name(). The returned
 * string is owned by GTK+ and should not be modified. It
 * is only valid until the next call to
 * gtk_window_set_default_icon_name().
 *
 * Returns: the fallback icon name for windows
 *
 * Since: 2.16
 */
const gchar *
gtk_window_get_default_icon_name (void)
{
  return default_icon_name;
}

/**
 * gtk_window_set_default_icon_from_file:
 * @filename: (type filename): location of icon file
 * @err: (allow-none): location to store error, or %NULL.
 *
 * Sets an icon to be used as fallback for windows that haven't
 * had gtk_window_set_icon_list() called on them from a file
 * on disk. Warns on failure if @err is %NULL.
 *
 * Returns: %TRUE if setting the icon succeeded.
 *
 * Since: 2.2
 **/
gboolean
gtk_window_set_default_icon_from_file (const gchar *filename,
				       GError     **err)
{
  GdkPixbuf *pixbuf = load_pixbuf_verbosely (filename, err);

  if (pixbuf)
    {
      gtk_window_set_default_icon (pixbuf);
      g_object_unref (pixbuf);
      
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_window_get_default_icon_list:
 * 
 * Gets the value set by gtk_window_set_default_icon_list().
 * The list is a copy and should be freed with g_list_free(),
 * but the pixbufs in the list have not had their reference count
 * incremented.
 * 
 * Returns: (element-type GdkPixbuf) (transfer container): copy of default icon list 
 **/
GList*
gtk_window_get_default_icon_list (void)
{
  return g_list_copy (default_icon_list);
}

#define INCLUDE_CSD_SIZE 1
#define EXCLUDE_CSD_SIZE -1

static void
gtk_window_update_csd_size (GtkWindow *window,
                            gint      *width,
                            gint      *height,
                            gint       apply)
{
  GtkWindowPrivate *priv = window->priv;
  GtkBorder window_border = { 0 };
  gint w, h;

  if (priv->type != GTK_WINDOW_TOPLEVEL)
    return;

  if (!priv->decorated ||
      priv->fullscreen)
    return;

  get_shadow_width (window, &window_border);
  w = *width + apply * (window_border.left + window_border.right);
  h = *height + apply * (window_border.top + window_border.bottom);

  if (priv->title_box != NULL &&
      gtk_widget_get_visible (priv->title_box) &&
      gtk_widget_get_child_visible (priv->title_box))
    {
      gint minimum_height;
      gint natural_height;

      gtk_widget_measure (priv->title_box, GTK_ORIENTATION_VERTICAL, -1,
                          &minimum_height, &natural_height,
                          NULL, NULL);
      h += apply * natural_height;
    }

  /* Make sure the size remains acceptable */
  if (w < 1)
    w = 1;
  if (h < 1)
    h = 1;

  /* Only update given size if not negative */
  if (*width > -1)
    *width = w;
  if (*height > -1)
    *height = h;
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

      if (info->default_width != width)
        {
          info->default_width = width;
          g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_DEFAULT_WIDTH]);
        }
    }

  if (change_height)
    {
      if (height == 0)
        height = 1;

      if (height < 0)
        height = -1;

      if (info->default_height != height)
        {
          info->default_height = height;
          g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_DEFAULT_HEIGHT]);
        }
    }
  
  g_object_thaw_notify (G_OBJECT (window));
  
  gtk_widget_queue_resize_no_redraw (GTK_WIDGET (window));
}

/**
 * gtk_window_set_default_size:
 * @window: a #GtkWindow
 * @width: width in pixels, or -1 to unset the default width
 * @height: height in pixels, or -1 to unset the default height
 *
 * Sets the default size of a window. If the window’s “natural” size
 * (its size request) is larger than the default, the default will be
 * ignored.
 *
 * Unlike gtk_widget_set_size_request(), which sets a size request for
 * a widget and thus would keep users from shrinking the window, this
 * function only sets the initial size, just as if the user had
 * resized the window themselves. Users can still shrink the window
 * again as they normally would. Setting a default size of -1 means to
 * use the “natural” default size (the size request of the window).
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
 * Windows can’t actually be 0x0 in size, they must be at least 1x1, but
 * passing 0 for @width and @height is OK, resulting in a 1x1 default size.
 *
 * If you use this function to reestablish a previously saved window size,
 * note that the appropriate size to save is the one returned by
 * gtk_window_get_size(). Using the window allocation directly will not
 * work in all circumstances and can lead to growing or shrinking windows.
 */
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
 * @width: (out) (allow-none): location to store the default width, or %NULL
 * @height: (out) (allow-none): location to store the default height, or %NULL
 *
 * Gets the default size of the window. A value of -1 for the width or
 * height indicates that a default size has not been explicitly set
 * for that dimension, so the “natural” size of the window will be
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
    *width = info ? info->default_width : -1;

  if (height)
    *height = info ? info->default_height : -1;
}

/**
 * gtk_window_resize:
 * @window: a #GtkWindow
 * @width: width in pixels to resize the window to
 * @height: height in pixels to resize the window to
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
 * When using client side decorations, GTK+ will do its best to adjust
 * the given size so that the resulting window size matches the
 * requested size without the title bar, borders and shadows added for
 * the client side decorations, but there is no garantee that the
 * result will be totally accurate because these widgets added for
 * client side decorations depend on the theme and may not be realized
 * or visible at the time gtk_window_resize() is issued.
 *
 * Typically, gtk_window_resize() will compensate for the GtkHeaderBar
 * height only if it's known at the time the resulting GtkWindow
 * configuration is issued.
 * For example, if new widgets are added after the GtkWindow configuration
 * and cause the GtkHeaderBar to grow in height, this will result in a
 * window content smaller that specified by gtk_window_resize() and not
 * a larger window.
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

  gtk_widget_queue_resize_no_redraw (GTK_WIDGET (window));
}

/**
 * gtk_window_get_size:
 * @window: a #GtkWindow
 * @width: (out) (nullable): return location for width, or %NULL
 * @height: (out) (nullable): return location for height, or %NULL
 *
 * Obtains the current size of @window.
 *
 * If @window is not visible on screen, this function return the size GTK+
 * will suggest to the [window manager][gtk-X11-arch] for the initial window
 * size (but this is not reliably the same as the size the window manager
 * will actually select). See: gtk_window_set_default_size().
 *
 * Depending on the windowing system and the window manager constraints,
 * the size returned by this function may not match the size set using
 * gtk_window_resize(); additionally, since gtk_window_resize() may be
 * implemented as an asynchronous operation, GTK+ cannot guarantee in any
 * way that this code:
 *
 * |[<!-- language="C" -->
 *   // width and height are set elsewhere
 *   gtk_window_resize (window, width, height);
 *
 *   int new_width, new_height;
 *   gtk_window_get_size (window, &new_width, &new_height);
 * ]|
 *
 * will result in `new_width` and `new_height` matching `width` and
 * `height`, respectively.
 *
 * This function will return the logical size of the #GtkWindow,
 * excluding the widgets used in client side decorations; there is,
 * however, no guarantee that the result will be completely accurate
 * because client side decoration may include widgets that depend on
 * the user preferences and that may not be visibile at the time you
 * call this function.
 *
 * The dimensions returned by this function are suitable for being
 * stored across sessions; use gtk_window_set_default_size() to
 * restore them when before showing the window.
 *
 * To avoid potential race conditions, you should only call this
 * function in response to a size change notification, for instance
 * inside a handler for the #GtkWidget::size-allocate signal, or
 * inside a handler for the #GtkWidget::configure-event signal:
 *
 * |[<!-- language="C" -->
 * static void
 * on_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
 * {
 *   int new_width, new_height;
 *
 *   gtk_window_get_size (GTK_WINDOW (widget), &new_width, &new_height);
 *
 *   ...
 * }
 * ]|
 *
 * Note that, if you connect to the #GtkWidget::size-allocate signal,
 * you should not use the dimensions of the #GtkAllocation passed to
 * the signal handler, as the allocation may contain client side
 * decorations added by GTK+, depending on the windowing system in
 * use.
 *
 * If you are getting a window size in order to position the window
 * on the screen, you should, instead, simply set the window’s semantic
 * type with gtk_window_set_type_hint(), which allows the window manager
 * to e.g. center dialogs. Also, if you set the transient parent of
 * dialogs with gtk_window_set_transient_for() window managers will
 * often center the dialog over its parent window. It's much preferred
 * to let the window manager handle these cases rather than doing it
 * yourself, because all apps will behave consistently and according to
 * user or system preferences, if the window manager handles it. Also,
 * the window manager can take into account the size of the window
 * decorations and border that it may add, and of which GTK+ has no
 * knowledge. Additionally, positioning windows in global screen coordinates
 * may not be allowed by the windowing system. For more information,
 * see: gtk_window_set_position().
 */
void
gtk_window_get_size (GtkWindow *window,
                     gint      *width,
                     gint      *height)
{
  gint w, h;
  
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (width == NULL && height == NULL)
    return;

  if (_gtk_widget_get_mapped (GTK_WIDGET (window)))
    {
      w = gdk_window_get_width (_gtk_widget_get_window (GTK_WIDGET (window)));
      h = gdk_window_get_height (_gtk_widget_get_window (GTK_WIDGET (window)));
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

  gtk_window_update_csd_size (window, &w, &h, EXCLUDE_CSD_SIZE);

  if (width)
    *width = w;
  if (height)
    *height = h;
}

static void
gtk_window_translate_csd_pos (GtkWindow *window,
                              gint      *root_x,
                              gint      *root_y,
                              gint       apply)
{
  GtkWindowPrivate *priv = window->priv;

  if (priv->type != GTK_WINDOW_TOPLEVEL)
    return;

  if (priv->decorated &&
      !priv->fullscreen)
    {
      GtkBorder window_border = { 0 };
      gint title_height = 0;
      gint dx;
      gint dy;

      get_shadow_width (window, &window_border);
      if (priv->title_box != NULL &&
          gtk_widget_get_visible (priv->title_box) &&
          gtk_widget_get_child_visible (priv->title_box))
        {
          gint minimum_height;

          gtk_widget_measure (priv->title_box, GTK_ORIENTATION_VERTICAL, -1,
                              &minimum_height, &title_height,
                              NULL, NULL);
        }

      switch (priv->gravity)
        {
        case GDK_GRAVITY_NORTH:
        case GDK_GRAVITY_CENTER:
        case GDK_GRAVITY_SOUTH:
          dx = (window_border.left + window_border.right) / 2;
          break;

        case GDK_GRAVITY_NORTH_WEST:
        case GDK_GRAVITY_WEST:
        case GDK_GRAVITY_SOUTH_WEST:
        case GDK_GRAVITY_SOUTH_EAST:
        case GDK_GRAVITY_EAST:
        case GDK_GRAVITY_NORTH_EAST:
          dx = window_border.left;
          break;

        default:
          dx = 0;
          break;
        }

      switch (priv->gravity)
        {
        case GDK_GRAVITY_WEST:
        case GDK_GRAVITY_CENTER:
        case GDK_GRAVITY_EAST:
          dy = (window_border.top + title_height + window_border.bottom) / 2;
          break;

        case GDK_GRAVITY_NORTH_WEST:
        case GDK_GRAVITY_NORTH:
        case GDK_GRAVITY_NORTH_EAST:
          dy = window_border.top;
          break;

        case GDK_GRAVITY_SOUTH_WEST:
        case GDK_GRAVITY_SOUTH:
        case GDK_GRAVITY_SOUTH_EAST:
          dy = window_border.top + title_height;
          break;

        default:
          dy = 0;
          break;
        }

      if (root_x)
        *root_x = *root_x + (dx * apply);
      if (root_y)
        *root_y = *root_y + (dy * apply);
    }
}

/**
 * gtk_window_move:
 * @window: a #GtkWindow
 * @x: X coordinate to move window to
 * @y: Y coordinate to move window to
 *
 * Asks the [window manager][gtk-X11-arch] to move
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
 * `gtk_window_move (window, gdk_screen_width () - window_width,
 * gdk_screen_height () - window_height)` (note that this
 * example does not take multi-head scenarios into account).
 *
 * The [Extended Window Manager Hints Specification](http://www.freedesktop.org/Standards/wm-spec)
 * has a nice table of gravities in the “implementation notes” section.
 *
 * The gtk_window_get_position() documentation may also be relevant.
 */
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
  gtk_window_translate_csd_pos (window, &x, &y, EXCLUDE_CSD_SIZE);

  if (_gtk_widget_get_mapped (widget))
    {
      GtkAllocation allocation;

      _gtk_widget_get_allocation (widget, &allocation);

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
                                     allocation.width, allocation.height,
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
      gdk_window_move (_gtk_widget_get_window (GTK_WIDGET (window)), x, y);
    }
  else
    {
      /* Save this position to apply on mapping */
      gtk_widget_queue_resize (widget);
      info->initial_x = x;
      info->initial_y = y;
      info->initial_pos_set = TRUE;
    }
}

/**
 * gtk_window_get_position:
 * @window: a #GtkWindow
 * @root_x: (out) (allow-none): return location for X coordinate of
 *     gravity-determined reference point, or %NULL
 * @root_y: (out) (allow-none): return location for Y coordinate of
 *     gravity-determined reference point, or %NULL
 *
 * This function returns the position you need to pass to
 * gtk_window_move() to keep @window in its current position.
 * This means that the meaning of the returned value varies with
 * window gravity. See gtk_window_move() for more details.
 *
 * The reliability of this function depends on the windowing system
 * currently in use. Some windowing systems, such as Wayland, do not
 * support a global coordinate system, and thus the position of the
 * window will always be (0, 0). Others, like X11, do not have a reliable
 * way to obtain the geometry of the decorations of a window if they are
 * provided by the window manager. Additionally, on X11, window manager
 * have been known to mismanage window gravity, which result in windows
 * moving even if you use the coordinates of the current position as
 * returned by this function.
 *
 * If you haven’t changed the window gravity, its gravity will be
 * #GDK_GRAVITY_NORTH_WEST. This means that gtk_window_get_position()
 * gets the position of the top-left corner of the window manager
 * frame for the window. gtk_window_move() sets the position of this
 * same top-left corner.
 *
 * If a window has gravity #GDK_GRAVITY_STATIC the window manager
 * frame is not relevant, and thus gtk_window_get_position() will
 * always produce accurate results. However you can’t use static
 * gravity to do things like place a window in a corner of the screen,
 * because static gravity ignores the window manager decorations.
 *
 * Ideally, this function should return appropriate values if the
 * window has client side decorations, assuming that the windowing
 * system supports global coordinates.
 *
 * In practice, saving the window position should not be left to
 * applications, as they lack enough knowledge of the windowing
 * system and the window manager state to effectively do so. The
 * appropriate way to implement saving the window position is to
 * use a platform-specific protocol, wherever that is available.
 */
void
gtk_window_get_position (GtkWindow *window,
                         gint      *root_x,
                         gint      *root_y)
{
  GtkWindowPrivate *priv;
  GtkWidget *widget;
  GdkWindow *gdk_window;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;
  widget = GTK_WIDGET (window);
  gdk_window = _gtk_widget_get_window (widget);

  if (priv->gravity == GDK_GRAVITY_STATIC)
    {
      if (_gtk_widget_get_mapped (widget))
        {
          /* This does a server round-trip, which is sort of wrong;
           * but a server round-trip is inevitable for
           * gdk_window_get_frame_extents() in the usual
           * NorthWestGravity case below, so not sure what else to
           * do. We should likely be consistent about whether we get
           * the client-side info or the server-side info.
           */
          gdk_window_get_origin (gdk_window, root_x, root_y);
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
      gtk_window_translate_csd_pos (window, root_x, root_y, INCLUDE_CSD_SIZE);
    }
  else
    {
      GdkRectangle frame_extents;
      
      gint x, y;
      gint w, h;
      
      if (_gtk_widget_get_mapped (widget))
        {
          gdk_window_get_frame_extents (gdk_window, &frame_extents);
          x = frame_extents.x;
          y = frame_extents.y;
          gtk_window_get_size (window, &w, &h);
          /* gtk_window_get_size() will have already taken into account
           * the padding added by the CSD shadow and title bar, so we need
           * to revert it here, otherwise we'll end up counting it twice...
           */
          gtk_window_update_csd_size (window, &w, &h, INCLUDE_CSD_SIZE);
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
      
      gtk_window_translate_csd_pos (window, &x, &y, INCLUDE_CSD_SIZE);
      switch (priv->gravity)
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

      switch (priv->gravity)
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

static void
gtk_window_destroy (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = window->priv;

  gtk_window_release_application (window);

  toplevel_list = g_slist_remove (toplevel_list, window);
  gtk_window_update_debugging ();

  if (priv->transient_parent)
    gtk_window_set_transient_for (window, NULL);

  remove_attach_widget (window);

  /* frees the icons */
  gtk_window_set_icon_list (window, NULL);

  if (priv->has_user_ref_count)
    {
      priv->has_user_ref_count = FALSE;
      g_object_unref (window);
    }

  if (priv->group)
    gtk_window_group_remove_window (priv->group, window);

   gtk_window_free_key_hash (window);

  GTK_WIDGET_CLASS (gtk_window_parent_class)->destroy (widget);
}

static void
gtk_window_finalize (GObject *object)
{
  GtkWindow *window = GTK_WINDOW (object);
  GtkWindowPrivate *priv = window->priv;
  GtkMnemonicHash *mnemonic_hash;

  g_free (priv->title);
  g_free (priv->wm_role);
  gtk_window_release_application (window);

  mnemonic_hash = gtk_window_get_mnemonic_hash (window, FALSE);
  if (mnemonic_hash)
    _gtk_mnemonic_hash_free (mnemonic_hash);

  if (priv->geometry_info)
    {
      g_free (priv->geometry_info);
    }

  if (priv->keys_changed_handler)
    {
      g_source_remove (priv->keys_changed_handler);
      priv->keys_changed_handler = 0;
    }

  if (priv->delete_event_handler)
    {
      g_source_remove (priv->delete_event_handler);
      priv->delete_event_handler = 0;
    }

#ifdef GDK_WINDOWING_X11
  if (priv->screen)
    {
      g_signal_handlers_disconnect_by_func (gtk_settings_get_for_screen (priv->screen),
                                            gtk_window_on_theme_variant_changed,
                                            window);
    }
#endif

  g_free (priv->startup_id);

  if (priv->mnemonics_display_timeout_id)
    {
      g_source_remove (priv->mnemonics_display_timeout_id);
      priv->mnemonics_display_timeout_id = 0;
    }

  if (priv->multipress_gesture)
    g_object_unref (priv->multipress_gesture);

  if (priv->drag_gesture)
    g_object_unref (priv->drag_gesture);

  g_clear_object (&priv->renderer);

  G_OBJECT_CLASS (gtk_window_parent_class)->finalize (object);
}

/* copied from gdkwindow-x11.c */
static const gchar *
get_default_title (void)
{
  const gchar *title;

  title = g_get_application_name ();
  if (!title)
    title = g_get_prgname ();
  if (!title)
    title = "";

  return title;
}

static gboolean
update_csd_visibility (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;
  gboolean visible;

  if (priv->title_box == NULL)
    return FALSE;

  visible = !priv->fullscreen;

  gtk_widget_set_child_visible (priv->title_box, visible);

  return visible;
}

static void
update_window_buttons (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  if (!update_csd_visibility (window))
    return;

  if (GTK_IS_HEADER_BAR (priv->title_box))
    _gtk_header_bar_update_window_buttons (GTK_HEADER_BAR (priv->title_box));
}

static GtkWidget *
create_titlebar (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;
  GtkWidget *titlebar;
  GtkStyleContext *context;

  titlebar = gtk_header_bar_new ();
  g_object_set (titlebar,
                "title", priv->title ? priv->title : get_default_title (),
                "has-subtitle", FALSE,
                "show-close-button", TRUE,
                NULL);
  context = gtk_widget_get_style_context (titlebar);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TITLEBAR);
  gtk_style_context_add_class (context, "default-decoration");

  return titlebar;
}

void
_gtk_window_request_csd (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  priv->csd_requested = TRUE;
}

static gboolean
gtk_window_should_use_csd (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;
  const gchar *csd_env;

  if (priv->csd_requested)
    return TRUE;

  if (!priv->decorated)
    return FALSE;

  if (priv->type == GTK_WINDOW_POPUP)
    return FALSE;

  csd_env = g_getenv ("GTK_CSD");

#ifdef GDK_WINDOWING_BROADWAY
  if (GDK_IS_BROADWAY_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    return TRUE;
#endif

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    return TRUE;
#endif

#ifdef GDK_WINDOWING_MIR
  if (GDK_IS_MIR_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    return TRUE;
#endif

#ifdef GDK_WINDOWING_WIN32
  if (g_strcmp0 (csd_env, "0") != 0 &&
      GDK_IS_WIN32_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    return TRUE;
#endif

  return (g_strcmp0 (csd_env, "1") == 0);
}

static void
create_decoration (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = window->priv;

  priv->use_client_shadow = gtk_window_supports_client_shadow (window);
  if (!priv->use_client_shadow)
    return;

  gtk_window_enable_csd (window);

  if (priv->type == GTK_WINDOW_POPUP)
    return;

  if (priv->title_box == NULL)
    {
      priv->titlebar = create_titlebar (window);
      gtk_widget_set_parent (priv->titlebar, widget);
      priv->title_box = priv->titlebar;
    }

  update_window_buttons (window);
}

static void
gtk_window_show (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = window->priv;
  GtkContainer *container = GTK_CONTAINER (window);

  if (!_gtk_widget_is_toplevel (GTK_WIDGET (widget)))
    {
      GTK_WIDGET_CLASS (gtk_window_parent_class)->show (widget);
      return;
    }

  _gtk_widget_set_visible_flag (widget, TRUE);

  gtk_css_node_validate (gtk_widget_get_css_node (widget));

  gtk_widget_realize (widget);

  gtk_container_check_resize (container);

  gtk_widget_map (widget);

  if (!priv->focus_widget)
    {
      if (priv->initial_focus)
        gtk_window_set_focus (window, priv->initial_focus);
      else
        gtk_window_move_focus (widget, GTK_DIR_TAB_FORWARD);
    }
  
  if (priv->modal)
    gtk_grab_add (widget);
}

static void
gtk_window_hide (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = window->priv;

  if (!_gtk_widget_is_toplevel (GTK_WIDGET (widget)))
    {
      GTK_WIDGET_CLASS (gtk_window_parent_class)->hide (widget);
      return;
    }

  _gtk_widget_set_visible_flag (widget, FALSE);
  gtk_widget_unmap (widget);

  if (priv->modal)
    gtk_grab_remove (widget);
}

static void
popover_unmap (GtkWidget        *widget,
               GtkWindowPopover *popover)
{
  if (popover->unmap_id)
    {
      g_signal_handler_disconnect (widget, popover->unmap_id);
      popover->unmap_id = 0;
    }

  if (popover->window)
    {
      gdk_window_hide (popover->window);
      gtk_widget_unmap (popover->widget);
    }
}

static void
popover_map (GtkWidget        *widget,
             GtkWindowPopover *popover)
{
  if (popover->window && gtk_widget_get_visible (popover->widget))
    {
      gdk_window_show_unraised (popover->window);
      gtk_widget_map (popover->widget);
      popover->unmap_id = g_signal_connect (popover->widget, "unmap",
                                            G_CALLBACK (popover_unmap), popover);
    }
}

static void
gtk_window_map (GtkWidget *widget)
{
  GtkWidget *child;
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = window->priv;
  GdkWindow *gdk_window;
  GList *link;
  GdkDisplay *display;

  if (!_gtk_widget_is_toplevel (widget))
    {
      GTK_WIDGET_CLASS (gtk_window_parent_class)->map (widget);
      return;
    }

  display = gtk_widget_get_display (widget);
  if (priv->initial_fullscreen_monitor > gdk_display_get_n_monitors (display))
    priv->initial_fullscreen_monitor = -1;

  GTK_WIDGET_CLASS (gtk_window_parent_class)->map (widget);

  child = gtk_bin_get_child (&(window->bin));
  if (child != NULL && gtk_widget_get_visible (child))
    gtk_widget_map (child);

  if (priv->title_box != NULL &&
      gtk_widget_get_visible (priv->title_box) &&
      gtk_widget_get_child_visible (priv->title_box))
    gtk_widget_map (priv->title_box);

  gdk_window = _gtk_widget_get_window (widget);

  if (priv->maximize_initially)
    gdk_window_maximize (gdk_window);
  else
    gdk_window_unmaximize (gdk_window);

  if (priv->stick_initially)
    gdk_window_stick (gdk_window);
  else
    gdk_window_unstick (gdk_window);

  if (priv->iconify_initially)
    gdk_window_iconify (gdk_window);
  else
    gdk_window_deiconify (gdk_window);

  if (priv->fullscreen_initially)
    {
      if (priv->initial_fullscreen_monitor < 0)
        gdk_window_fullscreen (gdk_window);
      else
        gdk_window_fullscreen_on_monitor (gdk_window, 
                                          priv->initial_fullscreen_monitor);
    }
  else
    gdk_window_unfullscreen (gdk_window);

  gdk_window_set_keep_above (gdk_window, priv->above_initially);

  gdk_window_set_keep_below (gdk_window, priv->below_initially);

  if (priv->type == GTK_WINDOW_TOPLEVEL)
    gtk_window_set_theme_variant (window);

  /* No longer use the default settings */
  priv->need_default_size = FALSE;
  priv->need_default_position = FALSE;

  gdk_window_show (gdk_window);

  if (!disable_startup_notification &&
      priv->type != GTK_WINDOW_POPUP)
    {
      /* Do we have a custom startup-notification id? */
      if (priv->startup_id != NULL)
        {
          /* Make sure we have a "real" id */
          if (!startup_id_is_fake (priv->startup_id))
            gdk_notify_startup_complete_with_id (priv->startup_id);

          g_free (priv->startup_id);
          priv->startup_id = NULL;
        }
      else
        {
          gdk_notify_startup_complete ();
        }
    }

  /* if mnemonics visible is not already set
   * (as in the case of popup menus), then hide mnemonics initially
   */
  if (!priv->mnemonics_visible_set)
    gtk_window_set_mnemonics_visible (window, FALSE);

  /* inherit from transient parent, so that a dialog that is
   * opened via keynav shows focus initially
   */
  if (priv->transient_parent)
    gtk_window_set_focus_visible (window, gtk_window_get_focus_visible (priv->transient_parent));
  else
    gtk_window_set_focus_visible (window, FALSE);

  if (priv->application)
    gtk_application_handle_window_map (priv->application, window);

  link = priv->popovers;

  while (link)
    {
      GtkWindowPopover *popover = link->data;
      link = link->next;
      popover_map (popover->widget, popover);
    }
}

static gboolean
gtk_window_map_event (GtkWidget   *widget,
                      GdkEventAny *event)
{
  if (!_gtk_widget_get_mapped (widget))
    {
      /* we should be be unmapped, but are getting a MapEvent, this may happen
       * to toplevel XWindows if mapping was intercepted by a window manager
       * and an unmap request occoured while the MapRequestEvent was still
       * being handled. we work around this situaiton here by re-requesting
       * the window being unmapped. more details can be found in:
       *   http://bugzilla.gnome.org/show_bug.cgi?id=316180
       */
      gdk_window_hide (_gtk_widget_get_window (widget));
    }
  return FALSE;
}

static void
gtk_window_unmap (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = window->priv;
  GtkWidget *child;
  GtkWindowGeometryInfo *info;
  GdkWindow *gdk_window;
  GdkWindowState state;
  GList *link;

  if (!_gtk_widget_is_toplevel (GTK_WIDGET (widget)))
    {
      GTK_WIDGET_CLASS (gtk_window_parent_class)->unmap (widget);
      return;
    }

  link = priv->popovers;

  while (link)
    {
      GtkWindowPopover *popover = link->data;
      link = link->next;
      popover_unmap (popover->widget, popover);
    }

  gdk_window = _gtk_widget_get_window (widget);

  GTK_WIDGET_CLASS (gtk_window_parent_class)->unmap (widget);
  gdk_window_withdraw (gdk_window);

  while (priv->configure_request_count > 0)
    {
      priv->configure_request_count--;
      gdk_window_thaw_toplevel_updates (_gtk_widget_get_window (widget));
    }
  priv->configure_notify_received = FALSE;

  /* on unmap, we reset the default positioning of the window,
   * so it's placed again, but we don't reset the default
   * size of the window, so it's remembered.
   */
  priv->need_default_position = TRUE;

  info = gtk_window_get_geometry_info (window, FALSE);
  if (info)
    {
      info->initial_pos_set = FALSE;
      info->position_constraints_changed = FALSE;
    }

  state = gdk_window_get_state (gdk_window);
  priv->iconify_initially = (state & GDK_WINDOW_STATE_ICONIFIED) != 0;
  priv->maximize_initially = (state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
  priv->stick_initially = (state & GDK_WINDOW_STATE_STICKY) != 0;
  priv->above_initially = (state & GDK_WINDOW_STATE_ABOVE) != 0;
  priv->below_initially = (state & GDK_WINDOW_STATE_BELOW) != 0;

  if (priv->title_box != NULL)
    gtk_widget_unmap (priv->title_box);

  child = gtk_bin_get_child (&(window->bin));
  if (child != NULL)
    gtk_widget_unmap (child);
}

/* (Note: Replace "size" with "width" or "height". Also, the request
 * mode is honoured.)
 * For selecting the default window size, the following conditions
 * should hold (in order of importance):
 * - the size is not below the minimum size
 *   Windows cannot be resized below their minimum size, so we must
 *   ensure we don’t do that either.
 * - the size is not above the natural size
 *   It seems weird to allocate more than this in an initial guess.
 * - the size does not exceed that of a maximized window
 *   We want to see the whole window after all.
 *   (Note that this may not be possible to achieve due to imperfect
 *    information from the windowing system.)
 */

static void
gtk_window_guess_default_size (GtkWindow *window,
                               gint      *width,
                               gint      *height)
{
  GtkWidget *widget;
  GdkDisplay *display;
  GdkWindow *gdkwindow;
  GdkMonitor *monitor;
  GdkRectangle workarea;
  int minimum, natural;

  widget = GTK_WIDGET (window);
  display = gtk_widget_get_display (widget);
  gdkwindow = _gtk_widget_get_window (widget);

  if (gdkwindow)
    monitor = gdk_display_get_monitor_at_window (display, gdkwindow);
  else
    monitor = gdk_display_get_monitor (display, 0);

  gdk_monitor_get_workarea (monitor, &workarea);

  *width = workarea.width;
  *height = workarea.height;

  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT)
    {
      gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, -1,
                          &minimum, &natural,
                          NULL, NULL);
      *height = MAX (minimum, MIN (*height, natural));

      gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL,
                          *height,
                          &minimum, &natural,
                          NULL, NULL);
      *width = MAX (minimum, MIN (*width, natural));
    }
  else /* GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH or CONSTANT_SIZE */
    {
      gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1,
                          &minimum, &natural,
                          NULL, NULL);
      *width = MAX (minimum, MIN (*width, natural));

      gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL,
                          *width,
                          &minimum, &natural,
                          NULL, NULL);
      *height = MAX (minimum, MIN (*height, natural));
    }
}

static void
gtk_window_get_remembered_size (GtkWindow *window,
                                int       *width,
                                int       *height)
{
  GtkWindowGeometryInfo *info;
  GdkWindow *gdk_window;

  *width = 0;
  *height = 0;

  gdk_window = _gtk_widget_get_window (GTK_WIDGET (window));
  if (gdk_window)
    {
      *width = gdk_window_get_width (gdk_window);
      *height = gdk_window_get_height (gdk_window);
      return;
    }

  info = gtk_window_get_geometry_info (window, FALSE);
  if (info)
    {
      /* MAX() works even if the last request is unset with -1 */
      *width = MAX (*width, info->last.configure_request.width);
      *height = MAX (*height, info->last.configure_request.height);
    }
}

static void
popover_get_rect (GtkWindowPopover      *popover,
                  GtkWindow             *window,
                  cairo_rectangle_int_t *rect)
{
  GtkAllocation win_alloc;
  GtkRequisition req;
  GtkBorder win_border;
  gdouble min, max;

  gtk_widget_get_preferred_size (popover->widget, NULL, &req);
  _gtk_widget_get_allocation (GTK_WIDGET (window), &win_alloc);

  get_shadow_width (window, &win_border);
  win_alloc.x += win_border.left;
  win_alloc.y += win_border.top;
  win_alloc.width -= win_border.left + win_border.right;
  win_alloc.height -= win_border.top + win_border.bottom;

  rect->width = req.width;
  rect->height = req.height;

  if (popover->pos == GTK_POS_LEFT || popover->pos == GTK_POS_RIGHT)
    {
      if (req.height < win_alloc.height &&
          gtk_widget_get_vexpand (popover->widget))
        {
          rect->y = win_alloc.y;
          rect->height = win_alloc.height;
        }
      else
        {
          min = 0;
          max = win_alloc.y + win_alloc.height + win_border.bottom - req.height;

          if (popover->clamp_allocation)
            {
              min += win_border.top;
              max -= win_border.bottom;
            }

          rect->y = CLAMP (popover->rect.y + (popover->rect.height / 2) -
                           (req.height / 2), min, max);
        }

      if ((popover->pos == GTK_POS_LEFT) ==
          (gtk_widget_get_direction (popover->widget) == GTK_TEXT_DIR_LTR))
        {
          rect->x = popover->rect.x - req.width;

          if (rect->x > win_alloc.x && gtk_widget_get_hexpand (popover->widget))
            {
              rect->x = win_alloc.x;
              rect->width = popover->rect.x;
            }
        }
      else
        {
          rect->x = popover->rect.x + popover->rect.width;

          if (rect->x + rect->width < win_alloc.x + win_alloc.width &&
              gtk_widget_get_hexpand (popover->widget))
            rect->width = win_alloc.x + win_alloc.width - rect->x;
        }
    }
  else if (popover->pos == GTK_POS_TOP || popover->pos == GTK_POS_BOTTOM)
    {
      if (req.width < win_alloc.width &&
          gtk_widget_get_hexpand (popover->widget))
        {
          rect->x = win_alloc.x;
          rect->width = win_alloc.width;
        }
      else
        {
          min = 0;
          max = win_alloc.x + win_alloc.width + win_border.right - req.width;

          if (popover->clamp_allocation)
            {
              min += win_border.left;
              max -= win_border.right;
            }

          rect->x = CLAMP (popover->rect.x + (popover->rect.width / 2) -
                           (req.width / 2), min, max);
        }

      if (popover->pos == GTK_POS_TOP)
        {
          rect->y = popover->rect.y - req.height;

          if (rect->y > win_alloc.y &&
              gtk_widget_get_vexpand (popover->widget))
            {
              rect->y = win_alloc.y;
              rect->height = popover->rect.y;
            }
        }
      else
        {
          rect->y = popover->rect.y + popover->rect.height;

          if (rect->y + rect->height < win_alloc.y + win_alloc.height &&
              gtk_widget_get_vexpand (popover->widget))
            rect->height = win_alloc.y + win_alloc.height - rect->y;
        }
    }
}

static void
popover_realize (GtkWidget        *widget,
                 GtkWindowPopover *popover,
                 GtkWindow        *window)
{
  cairo_rectangle_int_t rect;

  if (popover->window)
    return;

  popover_get_rect (popover, window, &rect);

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (widget)))
    {
      popover->window = gdk_wayland_window_new_subsurface (gtk_widget_get_display (GTK_WIDGET (window)),
                                                           gtk_widget_get_events (popover->widget)
                                                           | GDK_EXPOSURE_MASK,
                                                           &rect);
      gdk_window_set_transient_for (popover->window,
                                    _gtk_widget_get_window (GTK_WIDGET (window)));
    }
  else
#endif
    {
      popover->window = gdk_window_new_child (_gtk_widget_get_window (GTK_WIDGET (window)),
                                              gtk_widget_get_events (popover->widget) | GDK_EXPOSURE_MASK,
                                              &rect);
    }

  gtk_widget_register_window (GTK_WIDGET (window), popover->window);
  gtk_widget_set_parent_window (popover->widget, popover->window);
}

static void
check_scale_changed (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;
  GtkWidget *widget = GTK_WIDGET (window);
  int old_scale;

  old_scale = priv->scale;
  priv->scale = gtk_widget_get_scale_factor (widget);
  if (old_scale != priv->scale)
    _gtk_widget_scale_changed (widget);
}

static void
sum_borders (GtkBorder *one,
             GtkBorder *two)
{
  one->top += two->top;
  one->right += two->right;
  one->bottom += two->bottom;
  one->left += two->left;
}

static void
max_borders (GtkBorder *one,
             GtkBorder *two)
{
  one->top = MAX (one->top, two->top);
  one->right = MAX (one->right, two->right);
  one->bottom = MAX (one->bottom, two->bottom);
  one->left = MAX (one->left, two->left);
}

static void
subtract_borders (GtkBorder *one,
                  GtkBorder *two)
{
  one->top -= two->top;
  one->right -= two->right;
  one->bottom -= two->bottom;
  one->left -= two->left;
}

static void
get_shadow_width (GtkWindow *window,
                  GtkBorder *shadow_width)
{
  GtkWindowPrivate *priv = window->priv;
  GtkBorder border = { 0 };
  GtkBorder d = { 0 };
  GtkBorder margin;
  GtkStyleContext *context;
  GtkCssValue *shadows;

  *shadow_width = border;

  if (!priv->decorated)
    return;

  if (!priv->client_decorated &&
      !(gtk_window_should_use_csd (window) &&
        gtk_window_supports_client_shadow (window)))
    return;

  if (priv->maximized ||
      priv->fullscreen)
    return;

  if (!_gtk_widget_is_toplevel (GTK_WIDGET (window)))
    return;

  context = _gtk_widget_get_style_context (GTK_WIDGET (window));

  gtk_style_context_save_to_node (context, priv->decoration_node);

  /* Always sum border + padding */
  gtk_style_context_get_border (context, &border);
  gtk_style_context_get_padding (context, &d);
  sum_borders (&d, &border);

  /* Calculate the size of the drop shadows ... */
  shadows = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BOX_SHADOW);
  _gtk_css_shadows_value_get_extents (shadows, &border);

  if (priv->type != GTK_WINDOW_POPUP)
    {
      /* ... and compare it to the margin size, which we use for resize grips */
      gtk_style_context_get_margin (context, &margin);
      max_borders (&border, &margin);
    }

  sum_borders (&d, &border);
  *shadow_width = d;

  gtk_style_context_restore (context);
}

/* We're placing 8 input-only windows around
 * the window content as resize handles, as
 * follows:
 *
 * +-----------------------------------+
 * | +------+-----------------+------+ |
 * | |      |                 |      | |
 * | |   +--+-----------------+--+   | |
 * | |   |                       |   | |
 * | +---+                       +---+ |
 * | |   |                       |   | |
 * | |   |                       |   | |
 * | |   |                       |   | |
 * | +---+                       +---+ |
 * | |   |                       |   | |
 * | |   +--+-----------------+--+   | |
 * | |      |                 |      | |
 * | +------+-----------------+------+ |
 * +-----------------------------------+
 *
 * The corner windows are shaped to allow them
 * to extend into the edges. If the window is
 * not resizable in both dimensions, we hide
 * the corner windows and the edge windows in
 * the nonresizable dimension and make the
 * remaining edge window extend all the way.
 *
 * The border are where we place the resize handles
 * is also used to draw the window shadow, which may
 * extend out farther than the handles (or the other
 * way around).
 */
static void
update_border_windows (GtkWindow *window)
{
  GtkWidget *widget = (GtkWidget *)window;
  GtkWindowPrivate *priv = window->priv;
  gboolean resize_h, resize_v;
  gint handle_h, handle_v;
  cairo_region_t *region;
  cairo_rectangle_int_t rect;
  gint width, height;
  GtkBorder border, tmp;
  GtkBorder window_border;
  GtkStyleContext *context;

  if (!priv->client_decorated)
    return;

  context = _gtk_widget_get_style_context (widget);

  gtk_style_context_save_to_node (context, priv->decoration_node);
  gtk_style_context_get_margin (context, &border);
  gtk_style_context_get_border (context, &tmp);
  sum_borders (&border, &tmp);
  gtk_style_context_get_padding (context, &tmp);
  sum_borders (&border, &tmp);
  gtk_style_context_restore (context);
  get_shadow_width (window, &window_border);

  if (priv->border_window[0] == NULL)
    goto shape;

  if (!priv->resizable ||
      priv->tiled ||
      priv->fullscreen ||
      priv->maximized)
    {
      resize_h = resize_v = FALSE;
    }
  else
    {
      resize_h = resize_v = TRUE;
    }

  width = gtk_widget_get_allocated_width (widget) - (window_border.left + window_border.right);
  height = gtk_widget_get_allocated_height (widget) - (window_border.top + window_border.bottom);

  handle_h = MIN (RESIZE_HANDLE_SIZE, width / 2);
  handle_v = MIN (RESIZE_HANDLE_SIZE, height / 2);

  if (resize_h && resize_v)
    {
      gdk_window_move_resize (priv->border_window[GDK_WINDOW_EDGE_NORTH_WEST],
                              window_border.left - border.left, window_border.top - border.top,
                              border.left + handle_h, border.top + handle_v);
      gdk_window_move_resize (priv->border_window[GDK_WINDOW_EDGE_NORTH_EAST],
                              window_border.left + width - handle_h, window_border.top - border.top,
                              border.right + handle_h, border.top + handle_v);
      gdk_window_move_resize (priv->border_window[GDK_WINDOW_EDGE_SOUTH_WEST],
                              window_border.left - border.left, window_border.top + height - handle_v,
                              border.left + handle_h, border.bottom + handle_v);
      gdk_window_move_resize (priv->border_window[GDK_WINDOW_EDGE_SOUTH_EAST],
                              window_border.left + width - handle_h, window_border.top + height - handle_v,
                              border.right + handle_h, border.bottom + handle_v);

      rect.x = 0;
      rect.y = 0;
      rect.width = border.left + handle_h;
      rect.height = border.top + handle_v;
      region = cairo_region_create_rectangle (&rect);
      rect.x = border.left;
      rect.y = border.top;
      rect.width = handle_h;
      rect.height = handle_v;
      cairo_region_subtract_rectangle (region, &rect);
      gdk_window_shape_combine_region (priv->border_window[GDK_WINDOW_EDGE_NORTH_WEST],
                                       region, 0, 0);
      cairo_region_destroy (region);

      rect.x = 0;
      rect.y = 0;
      rect.width = border.right + handle_h;
      rect.height = border.top + handle_v;
      region = cairo_region_create_rectangle (&rect);
      rect.x = 0;
      rect.y = border.top;
      rect.width = handle_h;
      rect.height = handle_v;
      cairo_region_subtract_rectangle (region, &rect);
      gdk_window_shape_combine_region (priv->border_window[GDK_WINDOW_EDGE_NORTH_EAST],
                                       region, 0, 0);
      cairo_region_destroy (region);

      rect.x = 0;
      rect.y = 0;
      rect.width = border.left + handle_h;
      rect.height = border.bottom + handle_v;
      region = cairo_region_create_rectangle (&rect);
      rect.x = border.left;
      rect.y = 0;
      rect.width = handle_h;
      rect.height = handle_v;
      cairo_region_subtract_rectangle (region, &rect);
      gdk_window_shape_combine_region (priv->border_window[GDK_WINDOW_EDGE_SOUTH_WEST],
                                       region, 0, 0);
      cairo_region_destroy (region);

      rect.x = 0;
      rect.y = 0;
      rect.width = border.right + handle_h;
      rect.height = border.bottom + handle_v;
      region = cairo_region_create_rectangle (&rect);
      rect.x = 0;
      rect.y = 0;
      rect.width = handle_h;
      rect.height = handle_v;
      cairo_region_subtract_rectangle (region, &rect);
      gdk_window_shape_combine_region (priv->border_window[GDK_WINDOW_EDGE_SOUTH_EAST],
                                       region, 0, 0);
      cairo_region_destroy (region);

      gdk_window_show_unraised (priv->border_window[GDK_WINDOW_EDGE_NORTH_WEST]);
      gdk_window_show_unraised (priv->border_window[GDK_WINDOW_EDGE_NORTH_EAST]);
      gdk_window_show_unraised (priv->border_window[GDK_WINDOW_EDGE_SOUTH_WEST]);
      gdk_window_show_unraised (priv->border_window[GDK_WINDOW_EDGE_SOUTH_EAST]);
    }
  else
    {
      gdk_window_hide (priv->border_window[GDK_WINDOW_EDGE_NORTH_WEST]);
      gdk_window_hide (priv->border_window[GDK_WINDOW_EDGE_NORTH_EAST]);
      gdk_window_hide (priv->border_window[GDK_WINDOW_EDGE_SOUTH_WEST]);
      gdk_window_hide (priv->border_window[GDK_WINDOW_EDGE_SOUTH_EAST]);
    }

  if (resize_v)
    {
      gint x, w;

      if (resize_h)
        {
          x = window_border.left + handle_h;
          w = width - 2 * handle_h;
        }
      else
        {
          x = 0;
          w = width + window_border.left + window_border.right;
        }

      gdk_window_move_resize (priv->border_window[GDK_WINDOW_EDGE_NORTH],
                              x, window_border.top - border.top,
                              w, border.top);
      gdk_window_move_resize (priv->border_window[GDK_WINDOW_EDGE_SOUTH],
                              x, window_border.top + height,
                              w, border.bottom);

      gdk_window_show_unraised (priv->border_window[GDK_WINDOW_EDGE_NORTH]);
      gdk_window_show_unraised (priv->border_window[GDK_WINDOW_EDGE_SOUTH]);
    }
  else
    {
      gdk_window_hide (priv->border_window[GDK_WINDOW_EDGE_NORTH]);
      gdk_window_hide (priv->border_window[GDK_WINDOW_EDGE_SOUTH]);
    }

  if (resize_h)
    {
      gint y, h;

      if (resize_v)
        {
          y = window_border.top + handle_v;
          h = height - 2 * handle_v;
        }
      else
        {
          y = 0;
          h = height + window_border.top + window_border.bottom;
        }

      gdk_window_move_resize (priv->border_window[GDK_WINDOW_EDGE_WEST],
                              window_border.left - border.left, y,
                              border.left, h);

      gdk_window_move_resize (priv->border_window[GDK_WINDOW_EDGE_EAST],
                              window_border.left + width, y,
                              border.right, h);

      gdk_window_show_unraised (priv->border_window[GDK_WINDOW_EDGE_WEST]);
      gdk_window_show_unraised (priv->border_window[GDK_WINDOW_EDGE_EAST]);
    }
  else
    {
      gdk_window_hide (priv->border_window[GDK_WINDOW_EDGE_WEST]);
      gdk_window_hide (priv->border_window[GDK_WINDOW_EDGE_EAST]);
    }

shape:
  /* we also update the input shape, which makes it so that clicks
   * outside the border windows go through
   */

  if (priv->type != GTK_WINDOW_POPUP)
    subtract_borders (&window_border, &border);

  rect.x = window_border.left;
  rect.y = window_border.top;
  rect.width = gtk_widget_get_allocated_width (widget) - window_border.left - window_border.right;
  rect.height = gtk_widget_get_allocated_height (widget) - window_border.top - window_border.bottom;
  region = cairo_region_create_rectangle (&rect);
  gtk_widget_set_csd_input_shape (widget, region);
  cairo_region_destroy (region);
}

static void
update_shadow_width (GtkWindow *window,
                     GtkBorder *border)
{
  GdkWindow *gdk_window;

  gdk_window = _gtk_widget_get_window (GTK_WIDGET (window));

  if (gdk_window)
    gdk_window_set_shadow_width (gdk_window,
                                 border->left,
                                 border->right,
                                 border->top,
                                 border->bottom);
}

static void
corner_rect (cairo_rectangle_int_t *rect,
             const GtkCssValue     *value)
{
  rect->width = _gtk_css_corner_value_get_x (value, 100);
  rect->height = _gtk_css_corner_value_get_y (value, 100);
}

static void
subtract_corners_from_region (cairo_region_t        *region,
                              cairo_rectangle_int_t *extents,
                              GtkStyleContext       *context,
                              GtkWindow             *window)
{
  cairo_rectangle_int_t rect;

  gtk_style_context_save_to_node (context, window->priv->decoration_node);

  corner_rect (&rect, _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS));
  rect.x = extents->x;
  rect.y = extents->y;
  cairo_region_subtract_rectangle (region, &rect);

  corner_rect (&rect, _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS));
  rect.x = extents->x + extents->width - rect.width;
  rect.y = extents->y;
  cairo_region_subtract_rectangle (region, &rect);

  corner_rect (&rect, _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS));
  rect.x = extents->x;
  rect.y = extents->y + extents->height - rect.height;
  cairo_region_subtract_rectangle (region, &rect);

  corner_rect (&rect, _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS));
  rect.x = extents->x + extents->width - rect.width;
  rect.y = extents->y + extents->height - rect.height;
  cairo_region_subtract_rectangle (region, &rect);

  gtk_style_context_restore (context);
}

static void
update_opaque_region (GtkWindow           *window,
                      GtkBorder           *border,
                      const GtkAllocation *allocation)
{
  GtkWidget *widget = GTK_WIDGET (window);
  cairo_region_t *opaque_region;
  GtkStyleContext *context;
  gboolean is_opaque = FALSE;

  if (!_gtk_widget_get_realized (widget))
      return;

  context = gtk_widget_get_style_context (widget);

  is_opaque = gdk_rgba_is_opaque (_gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BACKGROUND_COLOR)));

  if (gtk_widget_get_opacity (widget) < 1.0)
    is_opaque = FALSE;

  if (is_opaque)
    {
      cairo_rectangle_int_t rect;

      rect.x = border->left;
      rect.y = border->top;
      rect.width = allocation->width - border->left - border->right;
      rect.height = allocation->height - border->top - border->bottom;

      opaque_region = cairo_region_create_rectangle (&rect);

      subtract_corners_from_region (opaque_region, &rect, context, window);
    }
  else
    {
      opaque_region = NULL;
    }

  gdk_window_set_opaque_region (_gtk_widget_get_window (widget), opaque_region);

  cairo_region_destroy (opaque_region);
}

static void
update_realized_window_properties (GtkWindow     *window,
                                   GtkAllocation *child_allocation,
                                   GtkBorder     *window_border)
{
  GtkWindowPrivate *priv = window->priv;

  if (!_gtk_widget_is_toplevel (GTK_WIDGET (window)))
    return;

  if (priv->client_decorated && priv->use_client_shadow)
    update_shadow_width (window, window_border);

  update_opaque_region (window, window_border, child_allocation);

  update_border_windows (window);
}

static void
gtk_window_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GtkAllocation child_allocation;
  GtkWindow *window;
  GdkWindow *gdk_window;
  GtkBorder window_border;
  GtkWindowPrivate *priv;
  gint event_mask;
  gint i;
  GList *link;

  window = GTK_WINDOW (widget);
  priv = window->priv;

  if (!priv->client_decorated && gtk_window_should_use_csd (window))
    create_decoration (widget);

  _gtk_widget_get_allocation (widget, &allocation);

  if (gtk_widget_get_parent_window (widget))
    {
      gdk_window = gdk_window_new_child (gtk_widget_get_parent_window (widget),
                                         gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK | GDK_STRUCTURE_MASK,
                                         &allocation);
      gtk_widget_set_window (widget, gdk_window);
      gtk_widget_register_window (widget, gdk_window);
      gtk_widget_set_realized (widget, TRUE);

      link = priv->popovers;

      while (link)
        {
          GtkWindowPopover *popover = link->data;
          link = link->next;
          popover_realize (popover->widget, popover, window);
        }

      priv->renderer = gsk_renderer_new_for_window (gdk_window);
      gsk_renderer_set_scale_factor (priv->renderer, gtk_widget_get_scale_factor (widget));

      return;
    }

  /* ensure widget tree is properly size allocated */
  if (allocation.x == -1 &&
      allocation.y == -1 &&
      allocation.width == 1 &&
      allocation.height == 1)
    {
      GdkRectangle request;

      gtk_window_compute_configure_request (window, &request, NULL, NULL);

      allocation.x = 0;
      allocation.y = 0;
      allocation.width = request.width;
      allocation.height = request.height;
      gtk_widget_size_allocate (widget, &allocation);

      gtk_widget_queue_resize (widget);

      g_return_if_fail (!_gtk_widget_get_realized (widget));
    }

  if (priv->hardcoded_window)
    {
      gdk_window = priv->hardcoded_window;
      _gtk_widget_get_allocation (widget, &allocation);
      gdk_window_resize (gdk_window, allocation.width, allocation.height);
    }
  else
    {
      _gtk_widget_get_allocation (widget, &allocation);
      event_mask = gtk_widget_get_events (widget);
      event_mask |= (GDK_EXPOSURE_MASK |
                     GDK_BUTTON_PRESS_MASK |
                     GDK_BUTTON_RELEASE_MASK |
                     GDK_BUTTON_MOTION_MASK |
                     GDK_KEY_PRESS_MASK |
                     GDK_KEY_RELEASE_MASK |
                     GDK_ENTER_NOTIFY_MASK |
                     GDK_LEAVE_NOTIFY_MASK |
                     GDK_FOCUS_CHANGE_MASK |
                     GDK_STRUCTURE_MASK);
      if (priv->decorated && priv->client_decorated)
        event_mask |= GDK_POINTER_MOTION_MASK;

      switch (priv->type)
        {
        case GTK_WINDOW_TOPLEVEL:
          gdk_window = gdk_window_new_toplevel (gtk_widget_get_display (widget),
                                                event_mask,
                                                allocation.width,
                                                allocation.height);
          break;
        case GTK_WINDOW_POPUP:
#ifdef GDK_WINDOWING_WAYLAND
          if (priv->use_subsurface &&
              GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (widget)))
            {
              gdk_window = gdk_wayland_window_new_subsurface (gtk_widget_get_display (widget),
                                                              event_mask,
                                                              &allocation);
            }
          else
#endif
            {
              gdk_window = gdk_window_new_popup (gtk_widget_get_display (widget),
                                                 event_mask,
                                                 &allocation);
            }
          break;
        default:
          g_error (G_STRLOC": Unknown window type %d!", priv->type);
          break;
        }
    }

  gtk_widget_set_window (widget, gdk_window);
  gtk_widget_register_window (widget, gdk_window);
  gtk_widget_set_realized (widget, TRUE);

  if (priv->renderer == NULL)
    {
      priv->renderer = gsk_renderer_new_for_window (gdk_window);
      gsk_renderer_set_scale_factor (priv->renderer, gtk_widget_get_scale_factor (widget));
    }

  if (priv->client_decorated && priv->type == GTK_WINDOW_TOPLEVEL)
    {
      const gchar *cursor_names[8] = {
        "nw-resize", "n-resize", "ne-resize",
        "w-resize",               "e-resize",
        "sw-resize", "s-resize", "se-resize"
      };

      for (i = 0; i < 8; i++)
        {
          GdkCursor *cursor;

          priv->border_window[i] = gdk_window_new_input (gdk_window,
                                                         GDK_BUTTON_PRESS_MASK,
                                                         &(GdkRectangle) { 0, 0, 1, 1 });

          cursor = gdk_cursor_new_from_name (gtk_widget_get_display (widget), cursor_names[i]);
          gdk_window_set_cursor (priv->border_window[i], cursor);
          g_object_unref (cursor);

          gdk_window_show (priv->border_window[i]);
          gtk_widget_register_window (widget, priv->border_window[i]);
        }
    }

  if (priv->transient_parent &&
      _gtk_widget_get_realized (GTK_WIDGET (priv->transient_parent)))
    gdk_window_set_transient_for (gdk_window,
                                  _gtk_widget_get_window (GTK_WIDGET (priv->transient_parent)));

  gdk_window_set_type_hint (gdk_window, priv->type_hint);

  if (priv->title)
    gdk_window_set_title (gdk_window, priv->title);

  if (priv->wm_role)
    gdk_window_set_role (gdk_window, priv->wm_role);

  if (!priv->decorated || priv->client_decorated)
    gdk_window_set_decorations (gdk_window, 0);

  if (!priv->deletable)
    gdk_window_set_functions (gdk_window, GDK_FUNC_ALL | GDK_FUNC_CLOSE);

  if (gtk_window_get_skip_pager_hint (window))
    gdk_window_set_skip_pager_hint (gdk_window, TRUE);

  if (gtk_window_get_skip_taskbar_hint (window))
    gdk_window_set_skip_taskbar_hint (gdk_window, TRUE);

  if (gtk_window_get_accept_focus (window))
    gdk_window_set_accept_focus (gdk_window, TRUE);
  else
    gdk_window_set_accept_focus (gdk_window, FALSE);

  if (gtk_window_get_focus_on_map (window))
    gdk_window_set_focus_on_map (gdk_window, TRUE);
  else
    gdk_window_set_focus_on_map (gdk_window, FALSE);

  if (priv->modal)
    gdk_window_set_modal_hint (gdk_window, TRUE);
  else
    gdk_window_set_modal_hint (gdk_window, FALSE);

  if (priv->startup_id)
    {
#ifdef GDK_WINDOWING_X11
      if (GDK_IS_X11_WINDOW (gdk_window))
        {
          guint32 timestamp = extract_time_from_startup_id (priv->startup_id);
          if (timestamp != GDK_CURRENT_TIME)
            gdk_x11_window_set_user_time (gdk_window, timestamp);
        }
#endif
      if (!startup_id_is_fake (priv->startup_id))
        gdk_window_set_startup_id (gdk_window, priv->startup_id);
    }

#ifdef GDK_WINDOWING_X11
  if (priv->initial_timestamp != GDK_CURRENT_TIME)
    {
      if (GDK_IS_X11_WINDOW (gdk_window))
        gdk_x11_window_set_user_time (gdk_window, priv->initial_timestamp);
    }
#endif

  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = allocation.width;
  child_allocation.height = allocation.height;

  get_shadow_width (window, &window_border);

  update_realized_window_properties (window, &child_allocation, &window_border);

  if (priv->application)
    gtk_application_handle_window_realize (priv->application, window);

  /* Icons */
  gtk_window_realize_icon (window);

  link = priv->popovers;

  while (link)
    {
      GtkWindowPopover *popover = link->data;
      link = link->next;
      popover_realize (popover->widget, popover, window);
    }

  check_scale_changed (window);
}

static void
popover_unrealize (GtkWidget        *widget,
                   GtkWindowPopover *popover,
                   GtkWindow        *window)
{
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (widget)))
    gdk_window_set_transient_for (popover->window, NULL);
#endif

  gtk_widget_unregister_window (GTK_WIDGET (window), popover->window);
  gtk_widget_unrealize (popover->widget);
  gdk_window_destroy (popover->window);
  popover->window = NULL;
}

static void
gtk_window_unrealize (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = window->priv;
  GtkWindowGeometryInfo *info;
  GList *link;
  gint i;

  /* On unrealize, we reset the size of the window such
   * that we will re-apply the default sizing stuff
   * next time we show the window.
   *
   * Default positioning is reset on unmap, instead of unrealize.
   */
  priv->need_default_size = TRUE;
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

  if (priv->popup_menu)
    {
      gtk_widget_destroy (priv->popup_menu);
      priv->popup_menu = NULL;
    }

  /* Icons */
  gtk_window_unrealize_icon (window);

  if (priv->border_window[0] != NULL)
    {
      for (i = 0; i < 8; i++)
        {
          gtk_widget_unregister_window (widget, priv->border_window[i]);
          gdk_window_destroy (priv->border_window[i]);
          priv->border_window[i] = NULL;
        }
    }

  link = priv->popovers;

  while (link)
    {
      GtkWindowPopover *popover = link->data;
      link = link->next;
      popover_unrealize (popover->widget, popover, window);
    }

  gtk_container_forall (GTK_CONTAINER (widget),
                        (GtkCallback) gtk_widget_unrealize,
                        NULL);

  if (priv->renderer != NULL)
    gsk_renderer_unrealize (priv->renderer);

  GTK_WIDGET_CLASS (gtk_window_parent_class)->unrealize (widget);

  priv->hardcoded_window = NULL;
}

static void
update_window_style_classes (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (window));

  if (priv->tiled)
    gtk_style_context_add_class (context, "tiled");
  else
    gtk_style_context_remove_class (context, "tiled");

  if (priv->maximized)
    gtk_style_context_add_class (context, "maximized");
  else
    gtk_style_context_remove_class (context, "maximized");

  if (priv->fullscreen)
    gtk_style_context_add_class (context, "fullscreen");
  else
    gtk_style_context_remove_class (context, "fullscreen");
}

static void
popover_size_allocate (GtkWidget        *widget,
                       GtkWindowPopover *popover,
                       GtkWindow        *window)
{
  cairo_rectangle_int_t rect;

  if (!popover->window)
    return;

  if (GTK_IS_POPOVER (popover->widget))
    gtk_popover_update_position (GTK_POPOVER (popover->widget));

  popover_get_rect (popover, window, &rect);
  gdk_window_move_resize (popover->window, rect.x, rect.y,
                          rect.width, rect.height);
  rect.x = rect.y = 0;
  gtk_widget_size_allocate (widget, &rect);

  if (gtk_widget_is_drawable (GTK_WIDGET (window)) &&
      gtk_widget_is_visible (widget))
    {
      if (!gdk_window_is_visible (popover->window))
        gdk_window_show_unraised (popover->window);
    }
  else if (gdk_window_is_visible (popover->window))
    gdk_window_hide (popover->window);
}

/* _gtk_window_set_allocation:
 * @window: a #GtkWindow
 * @allocation: the original allocation for the window
 * @allocation_out: @allocation taking decorations into
 * consideration
 *
 * This function is like gtk_widget_set_allocation()
 * but does the necessary extra work to update
 * the resize grip positioning, etc.
 *
 * Call this instead of gtk_widget_set_allocation()
 * when overriding ::size_allocate in a GtkWindow
 * subclass without chaining up.
 *
 * The @allocation parameter will be adjusted to
 * reflect any internal decorations that the window
 * may have. That revised allocation will then be
 * returned in the @allocation_out parameter.
 */
void
_gtk_window_set_allocation (GtkWindow           *window,
                            const GtkAllocation *allocation,
                            GtkAllocation       *allocation_out)
{
  GtkWidget *widget = (GtkWidget *)window;
  GtkWindowPrivate *priv = window->priv;
  GtkAllocation child_allocation;
  GtkBorder window_border = { 0 };
  GList *link;

  g_assert (allocation != NULL);
  g_assert (allocation_out != NULL);

  gtk_widget_set_allocation (widget, allocation);

  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = allocation->width;
  child_allocation.height = allocation->height;

  if (priv->renderer != NULL)
    {
      graphene_rect_t viewport;
      int scale;

      scale = gtk_widget_get_scale_factor (widget);
      gsk_renderer_set_scale_factor (priv->renderer, scale);

      graphene_rect_init (&viewport, 0, 0, allocation->width, allocation->height);
      gsk_renderer_set_viewport (priv->renderer, &viewport);
    }

  get_shadow_width (window, &window_border);

  if (_gtk_widget_get_realized (widget))
    update_realized_window_properties (window, &child_allocation, &window_border);

  priv->title_height = 0;

  if (priv->title_box != NULL &&
      gtk_widget_get_visible (priv->title_box) &&
      gtk_widget_get_child_visible (priv->title_box) &&
      priv->decorated &&
      !priv->fullscreen)
    {
      GtkAllocation title_allocation;

      title_allocation.x = window_border.left;
      title_allocation.y = window_border.top;
      title_allocation.width =
        MAX (1, (gint) allocation->width -
             window_border.left - window_border.right);

      gtk_widget_measure (priv->title_box, GTK_ORIENTATION_VERTICAL,
                          title_allocation.width,
                          NULL, &priv->title_height,
                          NULL, NULL);

      title_allocation.height = priv->title_height;

      gtk_widget_size_allocate (priv->title_box, &title_allocation);
    }

  if (priv->decorated &&
      !priv->fullscreen)
    {
      child_allocation.x += window_border.left;
      child_allocation.y += window_border.top + priv->title_height;
      child_allocation.width -= window_border.left + window_border.right;
      child_allocation.height -= window_border.top + window_border.bottom +
                                 priv->title_height;
    }

  if (!_gtk_widget_is_toplevel (widget) && _gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (_gtk_widget_get_window (widget),
                              allocation->x, allocation->y,
                              allocation->width, allocation->height);
    }

  *allocation_out = child_allocation;

  link = priv->popovers;
  while (link)
    {
      GtkWindowPopover *popover = link->data;
      link = link->next;
      popover_size_allocate (popover->widget, popover, window);
    }

}

static void
gtk_window_restack_popovers (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;
  GList *link = priv->popovers;

  while (link)
    {
      GtkWindowPopover *popover = link->data;
      link = link->next;

      if (popover->window)
        gdk_window_raise (popover->window);
    }
}

static void
gtk_window_size_allocate (GtkWidget     *widget,
                          GtkAllocation *allocation)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWidget *child;
  GtkAllocation child_allocation;

  _gtk_window_set_allocation (window, allocation, &child_allocation);

  child = gtk_bin_get_child (GTK_BIN (window));
  if (child && gtk_widget_get_visible (child))
    gtk_widget_size_allocate (child, &child_allocation);

  gtk_window_restack_popovers (window);
}

static gint
gtk_window_configure_event (GtkWidget         *widget,
			    GdkEventConfigure *event)
{
  GtkAllocation allocation;
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = window->priv;

  check_scale_changed (window);

  if (!_gtk_widget_is_toplevel (widget))
    return FALSE;

  if (_gtk_widget_get_window (widget) != event->window)
    return TRUE;

  /* If this is a gratuitous ConfigureNotify that's already
   * the same as our allocation, then we can fizzle it out.
   * This is the case for dragging windows around.
   *
   * We can't do this for a ConfigureRequest, since it might
   * have been a queued resize from child widgets, and so we
   * need to reallocate our children in case *they* changed.
   */
  _gtk_widget_get_allocation (widget, &allocation);
  if (priv->configure_request_count == 0 &&
      (allocation.width == event->width &&
       allocation.height == event->height))
    {
      return TRUE;
    }

  /* priv->configure_request_count incremented for each
   * configure request, and decremented to a min of 0 for
   * each configure notify.
   *
   * All it means is that we know we will get at least
   * priv->configure_request_count more configure notifies.
   * We could get more configure notifies than that; some
   * of the configure notifies we get may be unrelated to
   * the configure requests. But we will get at least
   * priv->configure_request_count notifies.
   */

  if (priv->configure_request_count > 0)
    {
      priv->configure_request_count -= 1;

      gdk_window_thaw_toplevel_updates (_gtk_widget_get_window (widget));
    }

  /*
   * If we do need to resize, we do that by:
   *   - setting configure_notify_received to TRUE
   *     for use in gtk_window_move_resize()
   *   - queueing a resize, leading to invocation of
   *     gtk_window_move_resize() in an idle handler
   *
   */
  
  priv->configure_notify_received = TRUE;

  gtk_widget_queue_allocate (widget);
  gtk_container_queue_resize_handler (GTK_CONTAINER (widget));
  
  return TRUE;
}

static gboolean
gtk_window_state_event (GtkWidget           *widget,
                        GdkEventWindowState *event)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = window->priv;

  if (event->changed_mask & GDK_WINDOW_STATE_FOCUSED)
    ensure_state_flag_backdrop (widget);

  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
      priv->fullscreen =
        (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) ? 1 : 0;
    }

  if (event->changed_mask & GDK_WINDOW_STATE_TILED)
    {
      priv->tiled =
        (event->new_window_state & GDK_WINDOW_STATE_TILED) ? 1 : 0;
    }

  if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED)
    {
      priv->maximized =
        (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) ? 1 : 0;
      g_object_notify_by_pspec (G_OBJECT (widget), window_props[PROP_IS_MAXIMIZED]);
    }

  if (event->changed_mask & (GDK_WINDOW_STATE_FULLSCREEN | GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_TILED))
    {
      update_window_style_classes (window);
      update_window_buttons (window);
      gtk_widget_queue_resize (widget);
    }

  return FALSE;
}


/* the accel_key and accel_mods fields of the key have to be setup
 * upon calling this function. it’ll then return whether that key
 * is at all used as accelerator, and if so will OR in the
 * accel_flags member of the key.
 */
gboolean
_gtk_window_query_nonaccels (GtkWindow      *window,
			     guint           accel_key,
			     GdkModifierType accel_mods)
{
  GtkWindowPrivate *priv;

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  priv = window->priv;

  /* movement keys are considered locked accels */
  if (!accel_mods)
    {
      static const guint bindings[] = {
	GDK_KEY_space, GDK_KEY_KP_Space, GDK_KEY_Return, GDK_KEY_ISO_Enter, GDK_KEY_KP_Enter, GDK_KEY_Up, GDK_KEY_KP_Up, GDK_KEY_Down, GDK_KEY_KP_Down,
	GDK_KEY_Left, GDK_KEY_KP_Left, GDK_KEY_Right, GDK_KEY_KP_Right, GDK_KEY_Tab, GDK_KEY_KP_Tab, GDK_KEY_ISO_Left_Tab,
      };
      guint i;
      
      for (i = 0; i < G_N_ELEMENTS (bindings); i++)
	if (bindings[i] == accel_key)
	  return TRUE;
    }

  /* mnemonics are considered locked accels */
  if (accel_mods == priv->mnemonic_modifier)
    {
      GtkMnemonicHash *mnemonic_hash = gtk_window_get_mnemonic_hash (window, FALSE);
      if (mnemonic_hash && _gtk_mnemonic_hash_lookup (mnemonic_hash, accel_key))
	return TRUE;
    }

  return FALSE;
}

/**
 * gtk_window_propagate_key_event:
 * @window:  a #GtkWindow
 * @event:   a #GdkEventKey
 *
 * Propagate a key press or release event to the focus widget and
 * up the focus container chain until a widget handles @event.
 * This is normally called by the default ::key_press_event and
 * ::key_release_event handlers for toplevel windows,
 * however in some cases it may be useful to call this directly when
 * overriding the standard key handling for a toplevel window.
 *
 * Returns: %TRUE if a widget in the focus chain handled the event.
 *
 * Since: 2.4
 */
gboolean
gtk_window_propagate_key_event (GtkWindow        *window,
                                GdkEventKey      *event)
{
  GtkWindowPrivate *priv = window->priv;
  gboolean handled = FALSE;
  GtkWidget *widget, *focus;

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  widget = GTK_WIDGET (window);

  focus = priv->focus_widget;
  if (focus)
    g_object_ref (focus);
  
  while (!handled &&
         focus && focus != widget &&
         gtk_widget_get_toplevel (focus) == widget)
    {
      GtkWidget *parent;
      
      if (gtk_widget_is_sensitive (focus))
        {
          handled = gtk_widget_event (focus, (GdkEvent*) event);
          if (handled)
            break;
        }

      parent = _gtk_widget_get_parent (focus);
      if (parent)
        g_object_ref (parent);
      
      g_object_unref (focus);
      
      focus = parent;
    }
  
  if (focus)
    g_object_unref (focus);

  return handled;
}

static gint
gtk_window_key_press_event (GtkWidget   *widget,
			    GdkEventKey *event)
{
  GtkWindow *window = GTK_WINDOW (widget);
  gboolean handled = FALSE;

  /* handle mnemonics and accelerators */
  if (!handled)
    handled = gtk_window_activate_key (window, event);

  /* handle focus widget key events */
  if (!handled)
    handled = gtk_window_propagate_key_event (window, event);

  /* Chain up, invokes binding set */
  if (!handled)
    handled = GTK_WIDGET_CLASS (gtk_window_parent_class)->key_press_event (widget, event);

  return handled;
}

static gint
gtk_window_key_release_event (GtkWidget   *widget,
			      GdkEventKey *event)
{
  GtkWindow *window = GTK_WINDOW (widget);
  gboolean handled = FALSE;

  /* handle focus widget key events */
  if (!handled)
    handled = gtk_window_propagate_key_event (window, event);

  /* Chain up, invokes binding set */
  if (!handled)
    handled = GTK_WIDGET_CLASS (gtk_window_parent_class)->key_release_event (widget, event);

  return handled;
}

static GtkWindowRegion
get_active_region_type (GtkWindow *window, GdkEventAny *event, gint x, gint y)
{
  GtkWindowPrivate *priv = window->priv;
  GtkAllocation allocation;
  gint i;

  for (i = 0; i < 8; i++)
    {
      if (event->window == priv->border_window[i])
        return i;
    }

  if (priv->title_box != NULL &&
      gtk_widget_get_visible (priv->title_box) &&
      gtk_widget_get_child_visible (priv->title_box))
    {
      _gtk_widget_get_allocation (priv->title_box, &allocation);
      if (allocation.x <= x && allocation.x + allocation.width > x &&
          allocation.y <= y && allocation.y + allocation.height > y)
        return GTK_WINDOW_REGION_TITLE;
    }

  return GTK_WINDOW_REGION_CONTENT;
}

static gboolean
controller_handle_wm_event (GtkGesture     *gesture,
                            const GdkEvent *event)
{
  GdkEventSequence *seq;
  gboolean retval;

  seq = gdk_event_get_event_sequence (event);
  retval = gtk_event_controller_handle_event (GTK_EVENT_CONTROLLER (gesture),
                                              event);

  /* Reset immediately the gestures, here we don't get many guarantees
   * about whether the target window event mask will be complete enough
   * to keep gestures consistent, or whether any widget across the
   * hierarchy will be inconsistent about event handler return values.
   */
  if (gtk_gesture_get_sequence_state (gesture, seq) == GTK_EVENT_SEQUENCE_DENIED)
    gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));

  return retval;
}

static gboolean
gtk_window_handle_wm_event (GtkWindow *window,
                            GdkEvent  *event,
                            gboolean   run_drag)
{
  gboolean retval = GDK_EVENT_PROPAGATE;
  GtkWindowPrivate *priv;

  if (event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE ||
      event->type == GDK_TOUCH_BEGIN || event->type == GDK_TOUCH_UPDATE ||
      event->type == GDK_MOTION_NOTIFY || event->type == GDK_TOUCH_END)
    {
      priv = window->priv;

      if (run_drag && priv->drag_gesture)
        retval |= controller_handle_wm_event (priv->drag_gesture,
                                              (const GdkEvent*) event);

      if (priv->multipress_gesture)
        retval |= controller_handle_wm_event (priv->multipress_gesture,
                                              (const GdkEvent*) event);
    }

  return retval;
}

gboolean
_gtk_window_check_handle_wm_event (GdkEvent *event)
{
  GtkWindowPrivate *priv;
  GtkWidget *widget;

  widget = gtk_get_event_widget (event);

  if (!GTK_IS_WINDOW (widget))
    widget = gtk_widget_get_toplevel (widget);

  if (!GTK_IS_WINDOW (widget))
    return GDK_EVENT_PROPAGATE;

  priv = GTK_WINDOW (widget)->priv;

  if (!priv->multipress_gesture)
    return GDK_EVENT_PROPAGATE;

  if (event->type != GDK_BUTTON_PRESS && event->type != GDK_BUTTON_RELEASE &&
      event->type != GDK_MOTION_NOTIFY && event->type != GDK_TOUCH_BEGIN &&
      event->type != GDK_TOUCH_END && event->type != GDK_TOUCH_UPDATE)
    return GDK_EVENT_PROPAGATE;

  if (gtk_widget_event (widget, event))
    return GDK_EVENT_STOP;

  return gtk_window_handle_wm_event (GTK_WINDOW (widget), event, TRUE);
}

static gboolean
gtk_window_event (GtkWidget *widget,
                  GdkEvent  *event)
{
  if (widget != gtk_get_event_widget (event))
    return gtk_window_handle_wm_event (GTK_WINDOW (widget), event, FALSE);

  return GDK_EVENT_PROPAGATE;
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
do_focus_change (GtkWidget *widget,
		 gboolean   in)
{
  GdkWindow *window;
  GdkDeviceManager *device_manager;
  GList *devices, *d;

  g_object_ref (widget);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  device_manager = gdk_display_get_device_manager (gtk_widget_get_display (widget));
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);
  devices = g_list_concat (devices, gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_SLAVE));
  devices = g_list_concat (devices, gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_FLOATING));
  G_GNUC_END_IGNORE_DEPRECATIONS;

  for (d = devices; d; d = d->next)
    {
      GdkDevice *dev = d->data;
      GdkEvent *fevent;

      if (gdk_device_get_source (dev) != GDK_SOURCE_KEYBOARD)
        continue;

      /* Skip non-master keyboards that haven't
       * selected for events from this window
       */
      window = _gtk_widget_get_window (widget);
      if (gdk_device_get_device_type (dev) != GDK_DEVICE_TYPE_MASTER &&
          window && !gdk_window_get_device_events (window, dev))
        continue;

      fevent = gdk_event_new (GDK_FOCUS_CHANGE);

      fevent->focus_change.type = GDK_FOCUS_CHANGE;
      fevent->focus_change.window = window;
      if (window)
        g_object_ref (window);
      fevent->focus_change.in = in;
      gdk_event_set_device (fevent, dev);

      gtk_widget_send_focus_change (widget, fevent);

      gdk_event_free (fevent);
    }

  g_list_free (devices);
  g_object_unref (widget);
}

static gboolean
gtk_window_has_mnemonic_modifier_pressed (GtkWindow *window)
{
  GList *seats, *s;
  gboolean retval = FALSE;

  if (!window->priv->mnemonic_modifier)
    return FALSE;

  seats = gdk_display_list_seats (gtk_widget_get_display (GTK_WIDGET (window)));

  for (s = seats; s; s = s->next)
    {
      GdkDevice *dev = gdk_seat_get_pointer (s->data);
      GdkModifierType mask;

      gdk_device_get_state (dev, _gtk_widget_get_window (GTK_WIDGET (window)),
                            NULL, &mask);
      if (window->priv->mnemonic_modifier == (mask & gtk_accelerator_get_default_mod_mask ()))
        {
          retval = TRUE;
          break;
        }
    }

  g_list_free (seats);

  return retval;
}

static gint
gtk_window_focus_in_event (GtkWidget     *widget,
			   GdkEventFocus *event)
{
  GtkWindow *window = GTK_WINDOW (widget);

  /* It appears spurious focus in events can occur when
   *  the window is hidden. So we'll just check to see if
   *  the window is visible before actually handling the
   *  event
   */
  if (gtk_widget_get_visible (widget))
    {
      _gtk_window_set_is_active (window, TRUE);

      if (gtk_window_has_mnemonic_modifier_pressed (window))
        _gtk_window_schedule_mnemonics_visible (window);
    }

  return FALSE;
}

static gint
gtk_window_focus_out_event (GtkWidget     *widget,
			    GdkEventFocus *event)
{
  GtkWindow *window = GTK_WINDOW (widget);

  _gtk_window_set_is_active (window, FALSE);

  /* set the mnemonic-visible property to false */
  gtk_window_set_mnemonics_visible (window, FALSE);

  return FALSE;
}

static GtkWindowPopover *
_gtk_window_has_popover (GtkWindow *window,
                         GtkWidget *widget)
{
  GtkWindowPrivate *priv = window->priv;
  GList *link;

  for (link = priv->popovers; link; link = link->next)
    {
      GtkWindowPopover *popover = link->data;

      if (popover->widget == widget)
        return popover;
    }

  return NULL;
}

static void
gtk_window_remove (GtkContainer *container,
                  GtkWidget     *widget)
{
  GtkWindow *window = GTK_WINDOW (container);

  if (widget == window->priv->title_box)
    unset_titlebar (window);
  else if (_gtk_window_has_popover (window, widget))
    _gtk_window_remove_popover (window, widget);
  else
    GTK_CONTAINER_CLASS (gtk_window_parent_class)->remove (container, widget);
}

static void
gtk_window_check_resize (GtkContainer *container)
{
  if (!_gtk_widget_get_alloc_needed (GTK_WIDGET (container)))
    GTK_CONTAINER_CLASS (gtk_window_parent_class)->check_resize (container);
  else if (gtk_widget_get_visible (GTK_WIDGET (container)))
    gtk_window_move_resize (GTK_WINDOW (container));
}

static void
gtk_window_forall (GtkContainer *container,
                   GtkCallback   callback,
                   gpointer      callback_data)
{
  GtkWindow *window = GTK_WINDOW (container);
  GtkWindowPrivate *priv = window->priv;
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (container));
  if (child != NULL)
    (* callback) (child, callback_data);

  if (priv->title_box != NULL &&
      priv->titlebar == NULL)
    (* callback) (priv->title_box, callback_data);
}

static gboolean
gtk_window_focus (GtkWidget        *widget,
		  GtkDirectionType  direction)
{
  GtkWindowPrivate *priv;
  GtkBin *bin;
  GtkWindow *window;
  GtkContainer *container;
  GtkWidget *child;
  GtkWidget *old_focus_child;
  GtkWidget *parent;

  if (!_gtk_widget_is_toplevel (widget))
    return GTK_WIDGET_CLASS (gtk_window_parent_class)->focus (widget, direction);

  container = GTK_CONTAINER (widget);
  window = GTK_WINDOW (widget);
  priv = window->priv;
  bin = GTK_BIN (widget);

  old_focus_child = gtk_widget_get_focus_child (widget);

  /* We need a special implementation here to deal properly with wrapping
   * around in the tab chain without the danger of going into an
   * infinite loop.
   */
  if (old_focus_child)
    {
      if (gtk_widget_child_focus (old_focus_child, direction))
	return TRUE;
    }

  if (priv->focus_widget)
    {
      if (direction == GTK_DIR_LEFT ||
	  direction == GTK_DIR_RIGHT ||
	  direction == GTK_DIR_UP ||
	  direction == GTK_DIR_DOWN)
	{
	  return FALSE;
	}
      
      /* Wrapped off the end, clear the focus setting for the toplpevel */
      parent = _gtk_widget_get_parent (priv->focus_widget);
      while (parent)
	{
          gtk_widget_set_focus_child (parent, NULL);
	  parent = _gtk_widget_get_parent (parent);
	}
      
      gtk_window_set_focus (GTK_WINDOW (container), NULL);
    }

  /* Now try to focus the first widget in the window,
   * taking care to hook titlebar widgets into the
   * focus chain.
  */
  if (priv->title_box != NULL &&
      old_focus_child != NULL &&
      priv->title_box != old_focus_child)
    child = priv->title_box;
  else
    child = gtk_bin_get_child (bin);

  if (child)
    {
      if (gtk_widget_child_focus (child, direction))
        return TRUE;
    }

  return FALSE;
}

static void
gtk_window_move_focus (GtkWidget        *widget,
                       GtkDirectionType  dir)
{
  if (!_gtk_widget_is_toplevel (widget))
    {
      GTK_WIDGET_CLASS (gtk_window_parent_class)->move_focus (widget, dir);
      return;
    }

  gtk_widget_child_focus (widget, dir);

  if (!gtk_widget_get_focus_child (widget))
    gtk_window_set_focus (GTK_WINDOW (widget), NULL);
}

static void
gtk_window_real_set_focus (GtkWindow *window,
			   GtkWidget *focus)
{
  GtkWindowPrivate *priv = window->priv;
  GtkWidget *old_focus = priv->focus_widget;
  gboolean had_default = FALSE;
  gboolean focus_had_default = FALSE;
  gboolean old_focus_had_default = FALSE;

  if (old_focus)
    {
      g_object_ref (old_focus);
      g_object_freeze_notify (G_OBJECT (old_focus));
      old_focus_had_default = gtk_widget_has_default (old_focus);
    }
  if (focus)
    {
      g_object_ref (focus);
      g_object_freeze_notify (G_OBJECT (focus));
      focus_had_default = gtk_widget_has_default (focus);
    }

  if (priv->default_widget)
    had_default = gtk_widget_has_default (priv->default_widget);

  if (priv->focus_widget)
    {
      if (gtk_widget_get_receives_default (priv->focus_widget) &&
	  (priv->focus_widget != priv->default_widget))
        {
          _gtk_widget_set_has_default (priv->focus_widget, FALSE);
	  gtk_widget_queue_draw (priv->focus_widget);

	  if (priv->default_widget)
            _gtk_widget_set_has_default (priv->default_widget, TRUE);
	}

      priv->focus_widget = NULL;

      if (priv->is_active)
	do_focus_change (old_focus, FALSE);

      g_object_notify (G_OBJECT (old_focus), "is-focus");
    }

  /* The above notifications may have set a new focus widget,
   * if so, we don't want to override it.
   */
  if (focus && !priv->focus_widget)
    {
      priv->focus_widget = focus;

      if (gtk_widget_get_receives_default (priv->focus_widget) &&
	  (priv->focus_widget != priv->default_widget))
	{
	  if (gtk_widget_get_can_default (priv->focus_widget))
            _gtk_widget_set_has_default (priv->focus_widget, TRUE);

	  if (priv->default_widget)
            _gtk_widget_set_has_default (priv->default_widget, FALSE);
	}

      if (priv->is_active)
	do_focus_change (priv->focus_widget, TRUE);

      /* It's possible for do_focus_change() above to have callbacks
       * that clear priv->focus_widget here.
       */
      if (priv->focus_widget)
        g_object_notify (G_OBJECT (priv->focus_widget), "is-focus");
    }

  /* If the default widget changed, a redraw will have been queued
   * on the old and new default widgets by gtk_window_set_default(), so
   * we only have to worry about the case where it didn't change.
   * We'll sometimes queue a draw twice on the new widget but that
   * is harmless.
   */
  if (priv->default_widget &&
      (had_default != gtk_widget_has_default (priv->default_widget)))
    gtk_widget_queue_draw (priv->default_widget);
  
  if (old_focus)
    {
      if (old_focus_had_default != gtk_widget_has_default (old_focus))
	gtk_widget_queue_draw (old_focus);
	
      g_object_thaw_notify (G_OBJECT (old_focus));
      g_object_unref (old_focus);
    }
  if (focus)
    {
      if (focus_had_default != gtk_widget_has_default (focus))
	gtk_widget_queue_draw (focus);

      g_object_thaw_notify (G_OBJECT (focus));
      g_object_unref (focus);
    }
}

static void
gtk_window_state_flags_changed (GtkWidget     *widget,
                                GtkStateFlags  previous_state)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = window->priv;
  GtkStateFlags state;

  state = gtk_widget_get_state_flags (widget);
  gtk_css_node_set_state (priv->decoration_node, state);

  GTK_WIDGET_CLASS (gtk_window_parent_class)->state_flags_changed (widget, previous_state);
}

static void
gtk_window_style_updated (GtkWidget *widget)
{
  GtkCssStyleChange *change = gtk_style_context_get_change (gtk_widget_get_style_context (widget));

  GTK_WIDGET_CLASS (gtk_window_parent_class)->style_updated (widget);

  if (change == NULL || gtk_css_style_change_changes_property (change, GTK_CSS_PROPERTY_ICON_THEME))
    update_themed_icon (GTK_WINDOW (widget));
}

static void
gtk_window_queue_draw_region (GtkWidget            *widget,
		              const cairo_region_t *region)
{
  if (_gtk_widget_get_parent (widget))
    {
      GTK_WIDGET_CLASS (gtk_window_parent_class)->queue_draw_region (widget, region);
      return;
    }

  gtk_debug_updates_add (widget, region);

  gdk_window_invalidate_region (_gtk_widget_get_window (widget), region, TRUE);
}

/**
 * _gtk_window_unset_focus_and_default:
 * @window: a #GtkWindow
 * @widget: a widget inside of @window
 * 
 * Checks whether the focus and default widgets of @window are
 * @widget or a descendent of @widget, and if so, unset them.
 **/
void
_gtk_window_unset_focus_and_default (GtkWindow *window,
				     GtkWidget *widget)

{
  GtkWindowPrivate *priv = window->priv;
  GtkWidget *child;
  GtkWidget *parent;

  g_object_ref (window);
  g_object_ref (widget);

  parent = _gtk_widget_get_parent (widget);
  if (gtk_widget_get_focus_child (parent) == widget)
    {
      child = priv->focus_widget;
      
      while (child && child != widget)
        child = _gtk_widget_get_parent (child);

      if (child == widget)
	gtk_window_set_focus (GTK_WINDOW (window), NULL);
    }
      
  child = priv->default_widget;
      
  while (child && child != widget)
    child = _gtk_widget_get_parent (child);

  if (child == widget)
    gtk_window_set_default (window, NULL);
  
  g_object_unref (widget);
  g_object_unref (window);
}

static void
popup_menu_detach (GtkWidget *widget,
                   GtkMenu   *menu)
{
  GTK_WINDOW (widget)->priv->popup_menu = NULL;
}

static GdkWindowState
gtk_window_get_state (GtkWindow *window)
{
  GdkWindowState state;
  GdkWindow *gdk_window;

  gdk_window = gtk_widget_get_window (GTK_WIDGET (window));

  state = 0;

  if (gdk_window)
    state = gdk_window_get_state (gdk_window);

  return state;
}

static void
restore_window_clicked (GtkMenuItem *menuitem,
                        gpointer     user_data)
{
  GtkWindow *window = GTK_WINDOW (user_data);
  GtkWindowPrivate *priv = window->priv;
  GdkWindowState state;

  if (priv->maximized)
    {
      gtk_window_unmaximize (window);

      return;
    }

  state = gtk_window_get_state (window);

  if (state & GDK_WINDOW_STATE_ICONIFIED)
    gtk_window_deiconify (window);
}

static void
move_window_clicked (GtkMenuItem *menuitem,
                     gpointer     user_data)
{
  GtkWindow *window = GTK_WINDOW (user_data);

  gtk_window_begin_move_drag (window,
                              0, /* 0 means "use keyboard" */
                              0, 0,
                              GDK_CURRENT_TIME);
}

static void
resize_window_clicked (GtkMenuItem *menuitem,
                       gpointer     user_data)
{
  GtkWindow *window = GTK_WINDOW (user_data);

  gtk_window_begin_resize_drag  (window,
                                 0,
                                 0, /* 0 means "use keyboard" */
                                 0, 0,
                                 GDK_CURRENT_TIME);
}

static void
minimize_window_clicked (GtkMenuItem *menuitem,
                         gpointer     user_data)
{
  GtkWindow *window = GTK_WINDOW (user_data);
  GtkWindowPrivate *priv = window->priv;

  /* Turns out, we can't iconify a maximized window */
  if (priv->maximized)
    gtk_window_unmaximize (window);

  gtk_window_iconify (window);
}

static void
maximize_window_clicked (GtkMenuItem *menuitem,
                         gpointer     user_data)
{
  GtkWindow *window = GTK_WINDOW (user_data);
  GdkWindowState state;

  state = gtk_window_get_state (window);

  if (state & GDK_WINDOW_STATE_ICONIFIED)
    gtk_window_deiconify (window);

  gtk_window_maximize (window);
}

static void
ontop_window_clicked (GtkMenuItem *menuitem,
                      gpointer     user_data)
{
  GtkWindow *window = (GtkWindow *)user_data;

  gtk_window_set_keep_above (window, !window->priv->above_initially);
}

static void
close_window_clicked (GtkMenuItem *menuitem,
                      gpointer     user_data)
{
  GtkWindow *window = (GtkWindow *)user_data;

  if (window->priv->delete_event_handler == 0)
    send_delete_event (window);
}

static void
gtk_window_do_popup_fallback (GtkWindow      *window,
                              GdkEventButton *event)
{
  GtkWindowPrivate *priv = window->priv;
  GtkWidget *menuitem;
  GdkWindowState state;
  gboolean maximized, iconified;

  if (priv->popup_menu)
    gtk_widget_destroy (priv->popup_menu);

  state = gtk_window_get_state (window);

  iconified = (state & GDK_WINDOW_STATE_ICONIFIED) == GDK_WINDOW_STATE_ICONIFIED;
  maximized = priv->maximized && !iconified;

  priv->popup_menu = gtk_menu_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->popup_menu),
                               GTK_STYLE_CLASS_CONTEXT_MENU);

  gtk_menu_attach_to_widget (GTK_MENU (priv->popup_menu),
                             GTK_WIDGET (window),
                             popup_menu_detach);

  menuitem = gtk_menu_item_new_with_label (_("Restore"));
  gtk_widget_show (menuitem);
  /* "Restore" means "Unmaximize" or "Unminimize"
   * (yes, some WMs allow window menu to be shown for minimized windows).
   * Not restorable:
   *   - visible windows that are not maximized or minimized
   *   - non-resizable windows that are not minimized
   *   - non-normal windows
   */
  if ((gtk_widget_is_visible (GTK_WIDGET (window)) &&
       !(maximized || iconified)) ||
      (!iconified && !priv->resizable) ||
      priv->type_hint != GDK_WINDOW_TYPE_HINT_NORMAL)
    gtk_widget_set_sensitive (menuitem, FALSE);
  g_signal_connect (G_OBJECT (menuitem), "activate",
                    G_CALLBACK (restore_window_clicked), window);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), menuitem);

  menuitem = gtk_menu_item_new_with_label (_("Move"));
  gtk_widget_show (menuitem);
  if (maximized || iconified)
    gtk_widget_set_sensitive (menuitem, FALSE);
  g_signal_connect (G_OBJECT (menuitem), "activate",
                    G_CALLBACK (move_window_clicked), window);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), menuitem);

  menuitem = gtk_menu_item_new_with_label (_("Resize"));
  gtk_widget_show (menuitem);
  if (!priv->resizable || maximized || iconified)
    gtk_widget_set_sensitive (menuitem, FALSE);
  g_signal_connect (G_OBJECT (menuitem), "activate",
                    G_CALLBACK (resize_window_clicked), window);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), menuitem);

  menuitem = gtk_menu_item_new_with_label (_("Minimize"));
  gtk_widget_show (menuitem);
  if (iconified ||
      priv->type_hint != GDK_WINDOW_TYPE_HINT_NORMAL)
    gtk_widget_set_sensitive (menuitem, FALSE);
  g_signal_connect (G_OBJECT (menuitem), "activate",
                    G_CALLBACK (minimize_window_clicked), window);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), menuitem);

  menuitem = gtk_menu_item_new_with_label (_("Maximize"));
  gtk_widget_show (menuitem);
  if (maximized ||
      !priv->resizable ||
      priv->type_hint != GDK_WINDOW_TYPE_HINT_NORMAL)
    gtk_widget_set_sensitive (menuitem, FALSE);
  g_signal_connect (G_OBJECT (menuitem), "activate",
                    G_CALLBACK (maximize_window_clicked), window);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), menuitem);

  menuitem = gtk_separator_menu_item_new ();
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), menuitem);

  menuitem = gtk_check_menu_item_new_with_label (_("Always on Top"));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem), priv->above_initially);
  if (maximized)
    gtk_widget_set_sensitive (menuitem, FALSE);
  gtk_widget_show (menuitem);
  g_signal_connect (G_OBJECT (menuitem), "activate",
                    G_CALLBACK (ontop_window_clicked), window);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), menuitem);

  menuitem = gtk_separator_menu_item_new ();
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), menuitem);

  menuitem = gtk_menu_item_new_with_label (_("Close"));
  gtk_widget_show (menuitem);
  if (!priv->deletable)
    gtk_widget_set_sensitive (menuitem, FALSE);
  g_signal_connect (G_OBJECT (menuitem), "activate",
                    G_CALLBACK (close_window_clicked), window);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), menuitem);
  gtk_menu_popup_at_pointer (GTK_MENU (priv->popup_menu), (GdkEvent *) event);
}

static void
gtk_window_do_popup (GtkWindow      *window,
                     GdkEventButton *event)
{
  if (!gdk_window_show_window_menu (_gtk_widget_get_window (GTK_WIDGET (window)),
                                    (GdkEvent *) event))
    gtk_window_do_popup_fallback (window, event);
}

/*********************************
 * Functions related to resizing *
 *********************************/

/* This function doesn't constrain to geometry hints */
static void 
gtk_window_compute_configure_request_size (GtkWindow   *window,
                                           GdkGeometry *geometry,
                                           guint        flags,
                                           gint        *width,
                                           gint        *height)
{
  GtkWindowPrivate *priv = window->priv;
  GtkWindowGeometryInfo *info;
  int w, h;

  /* Preconditions:
   *  - we've done a size request
   */
  
  info = gtk_window_get_geometry_info (window, FALSE);

  if (priv->need_default_size)
    {
      gtk_window_guess_default_size (window, width, height);
      gtk_window_get_remembered_size (window, &w, &h);
      *width = MAX (*width, w);
      *height = MAX (*height, h);

      /* Override with default size */
      if (info)
        {
          /* Take width of shadows/headerbar into account. We want to set the
           * default size of the content area and not the window area.
           */
          gint default_width_csd = info->default_width;
          gint default_height_csd = info->default_height;
          gtk_window_update_csd_size (window,
                                      &default_width_csd, &default_height_csd,
                                      INCLUDE_CSD_SIZE);

          if (info->default_width > 0)
            *width = default_width_csd;
          if (info->default_height > 0)
            *height = default_height_csd;
        }
    }
  else
    {
      /* Default to keeping current size */
      gtk_window_get_remembered_size (window, width, height);
    }

  /* Override any size with gtk_window_resize() values */
  if (priv->maximized || priv->fullscreen)
    {
      /* Unless we are maximized or fullscreen */
      gtk_window_get_remembered_size (window, width, height);
    }
  else if (info)
    {
      gint resize_width_csd = info->resize_width;
      gint resize_height_csd = info->resize_height;
      gtk_window_update_csd_size (window,
                                  &resize_width_csd, &resize_height_csd,
                                  INCLUDE_CSD_SIZE);

      if (info->resize_width > 0)
        *width = resize_width_csd;
      if (info->resize_height > 0)
        *height = resize_height_csd;
    }

  /* Don't ever request zero width or height, it's not supported by
     gdk. The size allocation code will round it to 1 anyway but if
     we do it then the value returned from this function will is
     not comparable to the size allocation read from the GtkWindow. */
  *width = MAX (*width, 1);
  *height = MAX (*height, 1);
}

static GtkWindowPosition
get_effective_position (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;
  GtkWindowPosition pos = priv->position;

  if (pos == GTK_WIN_POS_CENTER_ON_PARENT &&
      (priv->transient_parent == NULL ||
       !_gtk_widget_get_mapped (GTK_WIDGET (priv->transient_parent))))
    pos = GTK_WIN_POS_NONE;

  return pos;
}

static GdkMonitor *
get_center_monitor_of_window (GtkWindow *window)
{
  GdkDisplay *display;

  /* We could try to sort out the relative positions of the monitors and
   * stuff, or we could just be losers and assume you have a row
   * or column of monitors.
   */
  display = gdk_screen_get_display (gtk_window_check_screen (window));
  return gdk_display_get_monitor (display, gdk_display_get_n_monitors (display) / 2);
}

static GdkMonitor *
get_monitor_containing_pointer (GtkWindow *window)
{
  gint px, py;
  GdkDisplay *display;
  GdkDevice *pointer;

  display = gdk_screen_get_display (gtk_window_check_screen (window));
  pointer = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
  gdk_device_get_position (pointer, NULL, &px, &py);

  return gdk_display_get_monitor_at_point (display, px, py);
}

static void
center_window_on_monitor (GtkWindow *window,
                          gint       w,
                          gint       h,
                          gint      *x,
                          gint      *y)
{
  GdkRectangle area;
  GdkMonitor *monitor;

  monitor = get_monitor_containing_pointer (window);

  if (monitor == NULL)
    monitor = get_center_monitor_of_window (window);

 gdk_monitor_get_workarea (monitor, &area);

  *x = (area.width - w) / 2 + area.x;
  *y = (area.height - h) / 2 + area.y;

  /* Be sure we aren't off the monitor, ignoring _NET_WM_STRUT
   * and WM decorations.
   */
  if (*x < area.x)
    *x = area.x;
  if (*y < area.y)
    *y = area.y;
}

static void
clamp (gint *base,
       gint  extent,
       gint  clamp_base,
       gint  clamp_extent)
{
  if (extent > clamp_extent)
    /* Center */
    *base = clamp_base + clamp_extent/2 - extent/2;
  else if (*base < clamp_base)
    *base = clamp_base;
  else if (*base + extent > clamp_base + clamp_extent)
    *base = clamp_base + clamp_extent - extent;
}

static void
clamp_window_to_rectangle (gint               *x,
                           gint               *y,
                           gint                w,
                           gint                h,
                           const GdkRectangle *rect)
{
  /* If it is too large, center it. If it fits on the monitor but is
   * partially outside, move it to the closest edge. Do this
   * separately in x and y directions.
   */
  clamp (x, w, rect->x, rect->width);
  clamp (y, h, rect->y, rect->height);
}


static void
gtk_window_compute_configure_request (GtkWindow    *window,
                                      GdkRectangle *request,
                                      GdkGeometry  *geometry,
                                      guint        *flags)
{
  GtkWindowPrivate *priv = window->priv;
  GdkGeometry new_geometry;
  guint new_flags;
  int w, h;
  GtkWindowPosition pos;
  GtkWidget *parent_widget;
  GtkWindowGeometryInfo *info;
  GdkScreen *screen;
  int x, y;

  screen = gtk_window_check_screen (window);

  gtk_window_compute_hints (window, &new_geometry, &new_flags);
  gtk_window_compute_configure_request_size (window,
                                             &new_geometry, new_flags,
                                             &w, &h);
  gtk_window_update_fixed_size (window, &new_geometry, w, h);
  gtk_window_constrain_size (window,
                             &new_geometry, new_flags,
                             w, h,
                             &w, &h);

  parent_widget = (GtkWidget*) priv->transient_parent;

  pos = get_effective_position (window);
  info = gtk_window_get_geometry_info (window, FALSE);

  /* by default, don't change position requested */
  if (info)
    {
      x = info->last.configure_request.x;
      y = info->last.configure_request.y;
    }
  else
    {
      x = 0;
      y = 0;
    }


  if (priv->need_default_position)
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
          center_window_on_monitor (window, w, h, &x, &y);
          break;

        case GTK_WIN_POS_CENTER_ON_PARENT:
          {
            GdkDisplay *display;
            GtkAllocation allocation;
            GdkWindow *gdk_window;
            GdkMonitor *monitor;
            GdkRectangle area;
            gint ox, oy;

            g_assert (_gtk_widget_get_mapped (parent_widget)); /* established earlier */

            display = gdk_screen_get_display (screen);
            gdk_window = _gtk_widget_get_window (parent_widget);
            monitor = gdk_display_get_monitor_at_window (display, gdk_window);

            gdk_window_get_origin (gdk_window, &ox, &oy);

            _gtk_widget_get_allocation (parent_widget, &allocation);
            x = ox + (allocation.width - w) / 2;
            y = oy + (allocation.height - h) / 2;

            /* Clamp onto current monitor, ignoring _NET_WM_STRUT and
             * WM decorations. If parent wasn't on a monitor, just
             * give up.
             */
            if (monitor != NULL)
              {
                gdk_monitor_get_geometry (monitor, &area);
                clamp_window_to_rectangle (&x, &y, w, h, &area);
              }
          }
          break;

        case GTK_WIN_POS_MOUSE:
          {
            GdkRectangle area;
            GdkDisplay *display;
            GdkDevice *pointer;
            GdkMonitor *monitor;
            gint px, py;

            display = gdk_screen_get_display (screen);
            pointer = gdk_seat_get_pointer (gdk_display_get_default_seat (display));

            gdk_device_get_position (pointer, NULL, &px, &py);
            monitor = gdk_display_get_monitor_at_point (display, px, py);

            x = px - w / 2;
            y = py - h / 2;

            /* Clamp onto current monitor, ignoring _NET_WM_STRUT and
             * WM decorations.
             */
            gdk_monitor_get_geometry (monitor, &area);
            clamp_window_to_rectangle (&x, &y, w, h, &area);
          }
          break;

        default:
          break;
        }
    } /* if (priv->need_default_position) */

  if (priv->need_default_position && info &&
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
  GtkWindowPrivate *priv = window->priv;

  /* See long comments in gtk_window_move_resize()
   * on when it's safe to call this function.
   */
  if (priv->position == GTK_WIN_POS_CENTER_ALWAYS)
    {
      gint center_x, center_y;

      center_window_on_monitor (window, new_width, new_height, &center_x, &center_y);
      
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
  GtkWindowPrivate *priv = window->priv;
  GtkWidget *widget;
  GtkWindowGeometryInfo *info;
  GdkGeometry new_geometry;
  GdkWindow *gdk_window;
  guint new_flags;
  GdkRectangle new_request;
  gboolean configure_request_size_changed;
  gboolean configure_request_pos_changed;
  gboolean hints_changed; /* do we need to send these again */
  GtkWindowLastGeometryInfo saved_last_info;
  int current_width, current_height;

  widget = GTK_WIDGET (window);

  gdk_window = _gtk_widget_get_window (widget);
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
  if (priv->type == GTK_WINDOW_TOPLEVEL)
    {
      GtkAllocation alloc;

      gtk_widget_get_allocation (widget, &alloc);

      g_message ("--- %s ---\n"
		 "last  : %d,%d\t%d x %d\n"
		 "this  : %d,%d\t%d x %d\n"
		 "alloc : %d,%d\t%d x %d\n"
		 "resize:      \t%d x %d\n" 
		 "size_changed: %d pos_changed: %d hints_changed: %d\n"
		 "configure_notify_received: %d\n"
		 "configure_request_count: %d\n"
		 "position_constraints_changed: %d",
		 priv->title ? priv->title : "(no title)",
		 info->last.configure_request.x,
		 info->last.configure_request.y,
		 info->last.configure_request.width,
		 info->last.configure_request.height,
		 new_request.x,
		 new_request.y,
		 new_request.width,
		 new_request.height,
		 alloc.x,
		 alloc.y,
		 alloc.width,
		 alloc.height,
		 info->resize_width,
		 info->resize_height,
		 configure_request_size_changed,
		 configure_request_pos_changed,
		 hints_changed,
		 priv->configure_notify_received,
		 priv->configure_request_count,
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
       (priv->need_default_position &&
        get_effective_position (window) != GTK_WIN_POS_NONE)) &&
      (new_flags & GDK_HINT_POS) == 0)
    {
      new_flags |= GDK_HINT_POS;
      hints_changed = TRUE;
    }
  
  /* Set hints if necessary
   */
  if (hints_changed)
    gdk_window_set_geometry_hints (gdk_window,
				   &new_geometry,
				   new_flags);

  current_width = gdk_window_get_width (gdk_window);
  current_height = gdk_window_get_height (gdk_window);

  /* handle resizing/moving and widget tree allocation
   */
  if (priv->configure_notify_received)
    {
      GtkAllocation allocation;
      int min, nat;

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
      priv->configure_notify_received = FALSE;

      allocation.x = 0;
      allocation.y = 0;
      /*allocation.width = current_width;*/

      gtk_widget_get_preferred_width (widget, &min, &nat);
      allocation.width = MAX (min, current_width);
      gtk_widget_get_preferred_height_for_width (widget, allocation.width, &min, &nat);
      allocation.height = MAX (min, current_height);

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
	  gtk_widget_queue_resize_no_redraw (widget); /* might recurse for GTK_RESIZE_IMMEDIATE */
	}

      return;			/* Bail out, we didn't really process the move/resize */
    }
  else if ((configure_request_size_changed || hints_changed) &&
           (current_width != new_request.width || current_height != new_request.height))
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
          gdk_window_move_resize (gdk_window,
                                  new_request.x, new_request.y,
                                  new_request.width, new_request.height);
        }
      else  /* only size changed */
        {
          gdk_window_resize (gdk_window,
                             new_request.width, new_request.height);
        }

      if (priv->type == GTK_WINDOW_POPUP)
        {
	  GtkAllocation allocation;

	  /* Directly size allocate for override redirect (popup) windows. */
          allocation.x = 0;
	  allocation.y = 0;
	  allocation.width = new_request.width;
	  allocation.height = new_request.height;

	  gtk_widget_size_allocate (widget, &allocation);
	}
      else
        {
	  /* Increment the number of have-not-yet-received-notify requests */
	  priv->configure_request_count += 1;

          gdk_window_freeze_toplevel_updates (gdk_window);

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
	}
    }
  else
    {
      GtkAllocation allocation;

      /* Handle any position changes.
       */
      if (configure_request_pos_changed)
        {
          gdk_window_move (gdk_window,
                           new_request.x, new_request.y);
        }

      /* Our configure request didn't change size, but maybe some of
       * our child widgets have. Run a size allocate with our current
       * size to make sure that we re-layout our child widgets. */
      allocation.x = 0;
      allocation.y = 0;
      allocation.width = current_width;
      allocation.height = current_height;

      gtk_widget_size_allocate (widget, &allocation);
    }
  
  /* We have now processed a move/resize since the last position
   * constraint change, setting of the initial position, or resize.
   * (Not resetting these flags here can lead to infinite loops for
   * GTK_RESIZE_IMMEDIATE containers)
   */
  info->position_constraints_changed = FALSE;
  info->initial_pos_set = FALSE;
  info->resize_width = -1;
  info->resize_height = -1;
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

static void 
gtk_window_constrain_size (GtkWindow   *window,
			   GdkGeometry *geometry,
			   guint        flags,
			   gint         width,
			   gint         height,
			   gint        *new_width,
			   gint        *new_height)
{
  GtkWindowPrivate *priv = window->priv;
  guint geometry_flags;

  /* ignore size increments for windows that fit in a fixed space */
  if (priv->maximized || priv->fullscreen || priv->tiled)
    geometry_flags = flags & ~GDK_HINT_RESIZE_INC;
  else
    geometry_flags = flags;

  gdk_window_constrain_size (geometry, geometry_flags, width, height,
                             new_width, new_height);
}

/* For non-resizable windows, make sure the given width/height fits
 * in the geometry contrains and update the geometry hints to match
 * the given width/height if not.
 * This is to make sure that non-resizable windows get the default
 * width/height if set, but can still grow if their content requires.
 *
 * Note: Fixed size windows with a default size set will not shrink
 * smaller than the default size when their content requires less size.
 */
static void
gtk_window_update_fixed_size (GtkWindow   *window,
                              GdkGeometry *new_geometry,
                              gint         new_width,
                              gint         new_height)
{
  GtkWindowPrivate *priv = window->priv;
  GtkWindowGeometryInfo *info;
  gboolean has_size_request;

  /* Adjust the geometry hints for non-resizable windows only */
  has_size_request = gtk_widget_has_size_request (GTK_WIDGET (window));
  if (priv->resizable || has_size_request)
    return;

  info = gtk_window_get_geometry_info (window, FALSE);
  if (info)
    {
      gint default_width_csd = info->default_width;
      gint default_height_csd = info->default_height;

      gtk_window_update_csd_size (window,
                                  &default_width_csd, &default_height_csd,
                                  INCLUDE_CSD_SIZE);

      if (info->default_width > -1)
        {
          gint w = MAX (MAX (default_width_csd, new_width), new_geometry->min_width);
          new_geometry->min_width = w;
          new_geometry->max_width = w;
        }

      if (info->default_height > -1)
        {
          gint h = MAX (MAX (default_height_csd, new_height), new_geometry->min_height);
          new_geometry->min_height = h;
          new_geometry->max_height = h;
        }
    }
}

/* Compute the set of geometry hints and flags for a window
 * based on the application set geometry, and requisition
 * of the window. gtk_widget_get_preferred_size() must have been
 * called first.
 */
static void
gtk_window_compute_hints (GtkWindow   *window,
			  GdkGeometry *new_geometry,
			  guint       *new_flags)
{
  GtkWindowPrivate *priv = window->priv;
  GtkWidget *widget;
  GtkRequisition requisition;

  widget = GTK_WIDGET (window);

  /* Use a good size for unresizable widgets, otherwise the minimum one. */
  if (priv->resizable)
    gtk_widget_get_preferred_size (widget, &requisition, NULL);
  else
    gtk_window_guess_default_size (window, &requisition.width, &requisition.height);

  /* We don't want to set GDK_HINT_POS in here, we just set it
   * in gtk_window_move_resize() when we want the position
   * honored.
   */
  *new_flags = 0;
  
  /* For simplicity, we always set the base hint, even when we
   * don't expect it to have any visible effect.
   * (Note: geometry_size_to_pixels() depends on this.)
   */
  *new_flags |= GDK_HINT_BASE_SIZE;
  new_geometry->base_width = 0;
  new_geometry->base_height = 0;

  *new_flags |= GDK_HINT_MIN_SIZE;
  new_geometry->min_width = requisition.width;
  new_geometry->min_height = requisition.height;
  
  if (!priv->resizable)
    {
      *new_flags |= GDK_HINT_MAX_SIZE;

      new_geometry->max_width = new_geometry->min_width;
      new_geometry->max_height = new_geometry->min_height;
    }

  *new_flags |= GDK_HINT_WIN_GRAVITY;
  new_geometry->win_gravity = priv->gravity;
}

#undef INCLUDE_CSD_SIZE
#undef EXCLUDE_CSD_SIZE

/***********************
 * Redrawing functions *
 ***********************/

static void
gtk_window_snapshot (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  GtkWindowPrivate *priv = GTK_WINDOW (widget)->priv;
  GtkStyleContext *context;
  GtkAllocation allocation;
  GtkBorder window_border;
  gint title_height;
  GList *l;

  context = gtk_widget_get_style_context (widget);

  get_shadow_width (GTK_WINDOW (widget), &window_border);
  _gtk_widget_get_allocation (widget, &allocation);

  if (priv->client_decorated &&
      priv->decorated &&
      !priv->fullscreen &&
      !priv->maximized)
    {
      gtk_style_context_save_to_node (context, priv->decoration_node);

      if (priv->use_client_shadow)
        {
          GtkBorder padding, border;

          gtk_style_context_get_padding (context, &padding);
          gtk_style_context_get_border (context, &border);
          sum_borders (&border, &padding);

          gtk_snapshot_render_background (snapshot, context,
                                          window_border.left - border.left, window_border.top - border.top,
                                          allocation.width -
                                            (window_border.left + window_border.right - border.left - border.right),
                                          allocation.height -
                                            (window_border.top + window_border.bottom - border.top - border.bottom));
          gtk_snapshot_render_frame (snapshot, context,
                                     window_border.left - border.left, window_border.top - border.top,
                                     allocation.width -
                                       (window_border.left + window_border.right - border.left - border.right),
                                     allocation.height -
                                       (window_border.top + window_border.bottom - border.top - border.bottom));
        }
      else
        {
          gtk_snapshot_render_background (snapshot, context, 0, 0,
                                          allocation.width,
                                          allocation.height);

          gtk_snapshot_render_frame (snapshot, context, 0, 0,
                                     allocation.width,
                                     allocation.height);
        }
      gtk_style_context_restore (context);
    }

  if (priv->title_box &&
      gtk_widget_get_visible (priv->title_box) &&
      gtk_widget_get_child_visible (priv->title_box))
    title_height = priv->title_height;
  else
    title_height = 0;

  gtk_snapshot_render_background (snapshot, context,
                                  window_border.left,
                                  window_border.top + title_height,
                                  allocation.width -
                                    (window_border.left + window_border.right),
                                  allocation.height -
                                    (window_border.top + window_border.bottom + title_height));
  gtk_snapshot_render_frame (snapshot, context,
                             window_border.left,
                             window_border.top + title_height,
                             allocation.width -
                               (window_border.left + window_border.right),
                             allocation.height -
                               (window_border.top + window_border.bottom + title_height));

  if (priv->title_box != NULL)
    gtk_widget_snapshot_child (widget, priv->title_box, snapshot);

  if (gtk_bin_get_child (GTK_BIN (widget)))
    gtk_widget_snapshot_child (widget, gtk_bin_get_child (GTK_BIN (widget)), snapshot);

  for (l = priv->popovers; l; l = l->next)
    {
      GtkWindowPopover *data = l->data;
      gtk_widget_snapshot_child (widget, data->widget, snapshot);
    }

  gtk_debug_updates_snapshot (widget, snapshot);
}

/**
 * gtk_window_present:
 * @window: a #GtkWindow
 *
 * Presents a window to the user. This may mean raising the window
 * in the stacking order, deiconifying it, moving it to the current
 * desktop, and/or giving it the keyboard focus, possibly dependent
 * on the user’s platform, window manager, and preferences.
 *
 * If @window is hidden, this function calls gtk_widget_show()
 * as well.
 * 
 * This function should be used when the user tries to open a window
 * that’s already open. Say for example the preferences dialog is
 * currently open, and the user chooses Preferences from the menu
 * a second time; use gtk_window_present() to move the already-open dialog
 * where the user can see it.
 *
 * If you are calling this function in response to a user interaction,
 * it is preferable to use gtk_window_present_with_time().
 * 
 **/
void
gtk_window_present (GtkWindow *window)
{
  gtk_window_present_with_time (window, GDK_CURRENT_TIME);
}

/**
 * gtk_window_present_with_time:
 * @window: a #GtkWindow
 * @timestamp: the timestamp of the user interaction (typically a 
 *   button or key press event) which triggered this call
 *
 * Presents a window to the user in response to a user interaction.
 * If you need to present a window without a timestamp, use 
 * gtk_window_present(). See gtk_window_present() for details. 
 * 
 * Since: 2.8
 **/
void
gtk_window_present_with_time (GtkWindow *window,
			      guint32    timestamp)
{
  GtkWindowPrivate *priv;
  GtkWidget *widget;
  GdkWindow *gdk_window;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;
  widget = GTK_WIDGET (window);

  if (gtk_widget_get_visible (widget))
    {
      gdk_window = _gtk_widget_get_window (widget);

      g_assert (gdk_window != NULL);

      gdk_window_show (gdk_window);

      /* Translate a timestamp of GDK_CURRENT_TIME appropriately */
      if (timestamp == GDK_CURRENT_TIME)
        {
#ifdef GDK_WINDOWING_X11
	  if (GDK_IS_X11_WINDOW(gdk_window))
	    {
	      GdkDisplay *display;

	      display = gtk_widget_get_display (widget);
	      timestamp = gdk_x11_display_get_user_time (display);
	    }
	  else
#endif
	    timestamp = gtk_get_current_event_time ();
        }

      gdk_window_focus (gdk_window, timestamp);
    }
  else
    {
      priv->initial_timestamp = timestamp;
      gtk_widget_show (widget);
    }
}

/**
 * gtk_window_iconify:
 * @window: a #GtkWindow
 *
 * Asks to iconify (i.e. minimize) the specified @window. Note that
 * you shouldn’t assume the window is definitely iconified afterward,
 * because other entities (e.g. the user or
 * [window manager][gtk-X11-arch]) could deiconify it
 * again, or there may not be a window manager in which case
 * iconification isn’t possible, etc. But normally the window will end
 * up iconified. Just don’t write code that crashes if not.
 *
 * It’s permitted to call this function before showing a window,
 * in which case the window will be iconified before it ever appears
 * onscreen.
 *
 * You can track iconification via the “window-state-event” signal
 * on #GtkWidget.
 **/
void
gtk_window_iconify (GtkWindow *window)
{
  GdkWindow *toplevel;

  g_return_if_fail (GTK_IS_WINDOW (window));

  window->priv->iconify_initially = TRUE;

  toplevel = _gtk_widget_get_window (GTK_WIDGET (window));

  if (toplevel != NULL)
    gdk_window_iconify (toplevel);
}

/**
 * gtk_window_deiconify:
 * @window: a #GtkWindow
 *
 * Asks to deiconify (i.e. unminimize) the specified @window. Note
 * that you shouldn’t assume the window is definitely deiconified
 * afterward, because other entities (e.g. the user or
 * [window manager][gtk-X11-arch])) could iconify it
 * again before your code which assumes deiconification gets to run.
 *
 * You can track iconification via the “window-state-event” signal
 * on #GtkWidget.
 **/
void
gtk_window_deiconify (GtkWindow *window)
{
  GdkWindow *toplevel;

  g_return_if_fail (GTK_IS_WINDOW (window));

  window->priv->iconify_initially = FALSE;

  toplevel = _gtk_widget_get_window (GTK_WIDGET (window));

  if (toplevel != NULL)
    gdk_window_deiconify (toplevel);
}

/**
 * gtk_window_stick:
 * @window: a #GtkWindow
 *
 * Asks to stick @window, which means that it will appear on all user
 * desktops. Note that you shouldn’t assume the window is definitely
 * stuck afterward, because other entities (e.g. the user or
 * [window manager][gtk-X11-arch] could unstick it
 * again, and some window managers do not support sticking
 * windows. But normally the window will end up stuck. Just don't
 * write code that crashes if not.
 *
 * It’s permitted to call this function before showing a window.
 *
 * You can track stickiness via the “window-state-event” signal
 * on #GtkWidget.
 **/
void
gtk_window_stick (GtkWindow *window)
{
  GdkWindow *toplevel;

  g_return_if_fail (GTK_IS_WINDOW (window));

  window->priv->stick_initially = TRUE;

  toplevel = _gtk_widget_get_window (GTK_WIDGET (window));

  if (toplevel != NULL)
    gdk_window_stick (toplevel);
}

/**
 * gtk_window_unstick:
 * @window: a #GtkWindow
 *
 * Asks to unstick @window, which means that it will appear on only
 * one of the user’s desktops. Note that you shouldn’t assume the
 * window is definitely unstuck afterward, because other entities
 * (e.g. the user or [window manager][gtk-X11-arch]) could
 * stick it again. But normally the window will
 * end up stuck. Just don’t write code that crashes if not.
 *
 * You can track stickiness via the “window-state-event” signal
 * on #GtkWidget.
 **/
void
gtk_window_unstick (GtkWindow *window)
{
  GdkWindow *toplevel;

  g_return_if_fail (GTK_IS_WINDOW (window));

  window->priv->stick_initially = FALSE;

  toplevel = _gtk_widget_get_window (GTK_WIDGET (window));

  if (toplevel != NULL)
    gdk_window_unstick (toplevel);
}

/**
 * gtk_window_maximize:
 * @window: a #GtkWindow
 *
 * Asks to maximize @window, so that it becomes full-screen. Note that
 * you shouldn’t assume the window is definitely maximized afterward,
 * because other entities (e.g. the user or
 * [window manager][gtk-X11-arch]) could unmaximize it
 * again, and not all window managers support maximization. But
 * normally the window will end up maximized. Just don’t write code
 * that crashes if not.
 *
 * It’s permitted to call this function before showing a window,
 * in which case the window will be maximized when it appears onscreen
 * initially.
 *
 * You can track maximization via the “window-state-event” signal
 * on #GtkWidget, or by listening to notifications on the
 * #GtkWindow:is-maximized property.
 **/
void
gtk_window_maximize (GtkWindow *window)
{
  GdkWindow *toplevel;

  g_return_if_fail (GTK_IS_WINDOW (window));

  window->priv->maximize_initially = TRUE;

  toplevel = _gtk_widget_get_window (GTK_WIDGET (window));

  if (toplevel != NULL)
    gdk_window_maximize (toplevel);
}

/**
 * gtk_window_unmaximize:
 * @window: a #GtkWindow
 *
 * Asks to unmaximize @window. Note that you shouldn’t assume the
 * window is definitely unmaximized afterward, because other entities
 * (e.g. the user or [window manager][gtk-X11-arch])
 * could maximize it again, and not all window
 * managers honor requests to unmaximize. But normally the window will
 * end up unmaximized. Just don’t write code that crashes if not.
 *
 * You can track maximization via the “window-state-event” signal
 * on #GtkWidget.
 **/
void
gtk_window_unmaximize (GtkWindow *window)
{
  GdkWindow *toplevel;

  g_return_if_fail (GTK_IS_WINDOW (window));

  window->priv->maximize_initially = FALSE;

  toplevel = _gtk_widget_get_window (GTK_WIDGET (window));

  if (toplevel != NULL)
    gdk_window_unmaximize (toplevel);
}

/**
 * gtk_window_fullscreen:
 * @window: a #GtkWindow
 *
 * Asks to place @window in the fullscreen state. Note that you
 * shouldn’t assume the window is definitely full screen afterward,
 * because other entities (e.g. the user or
 * [window manager][gtk-X11-arch]) could unfullscreen it
 * again, and not all window managers honor requests to fullscreen
 * windows. But normally the window will end up fullscreen. Just
 * don’t write code that crashes if not.
 *
 * You can track the fullscreen state via the “window-state-event” signal
 * on #GtkWidget.
 *
 * Since: 2.2
 **/
void
gtk_window_fullscreen (GtkWindow *window)
{
  GdkWindow *toplevel;

  g_return_if_fail (GTK_IS_WINDOW (window));

  window->priv->fullscreen_initially = TRUE;

  toplevel = _gtk_widget_get_window (GTK_WIDGET (window));

  if (toplevel != NULL)
    gdk_window_fullscreen (toplevel);
}

/**
 * gtk_window_fullscreen_on_monitor:
 * @window: a #GtkWindow
 * @screen: a #GdkScreen to draw to
 * @monitor: which monitor to go fullscreen on
 *
 * Asks to place @window in the fullscreen state. Note that you shouldn't assume
 * the window is definitely full screen afterward.
 *
 * You can track the fullscreen state via the "window-state-event" signal
 * on #GtkWidget.
 *
 * Since: 3.18
 */
void
gtk_window_fullscreen_on_monitor (GtkWindow *window,
                                  GdkScreen *screen,
                                  gint monitor)
{
  GtkWindowPrivate *priv;
  GtkWidget *widget;
  GdkWindow *toplevel;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (gdk_display_get_monitor (gdk_screen_get_display (screen), monitor) != NULL);

  priv = window->priv;
  widget = GTK_WIDGET (window);

  gtk_window_set_screen (window, screen);

  priv->initial_fullscreen_monitor = monitor;
  priv->fullscreen_initially = TRUE;

  toplevel = _gtk_widget_get_window (widget);

  if (toplevel != NULL)
    gdk_window_fullscreen_on_monitor (toplevel, monitor);
}

/**
 * gtk_window_unfullscreen:
 * @window: a #GtkWindow
 *
 * Asks to toggle off the fullscreen state for @window. Note that you
 * shouldn’t assume the window is definitely not full screen
 * afterward, because other entities (e.g. the user or
 * [window manager][gtk-X11-arch]) could fullscreen it
 * again, and not all window managers honor requests to unfullscreen
 * windows. But normally the window will end up restored to its normal
 * state. Just don’t write code that crashes if not.
 *
 * You can track the fullscreen state via the “window-state-event” signal
 * on #GtkWidget.
 *
 * Since: 2.2
 **/
void
gtk_window_unfullscreen (GtkWindow *window)
{
  GdkWindow *toplevel;

  g_return_if_fail (GTK_IS_WINDOW (window));

  window->priv->initial_fullscreen_monitor = -1;
  window->priv->fullscreen_initially = FALSE;

  toplevel = _gtk_widget_get_window (GTK_WIDGET (window));

  if (toplevel != NULL)
    gdk_window_unfullscreen (toplevel);
}

/**
 * gtk_window_set_keep_above:
 * @window: a #GtkWindow
 * @setting: whether to keep @window above other windows
 *
 * Asks to keep @window above, so that it stays on top. Note that
 * you shouldn’t assume the window is definitely above afterward,
 * because other entities (e.g. the user or
 * [window manager][gtk-X11-arch]) could not keep it above,
 * and not all window managers support keeping windows above. But
 * normally the window will end kept above. Just don’t write code
 * that crashes if not.
 *
 * It’s permitted to call this function before showing a window,
 * in which case the window will be kept above when it appears onscreen
 * initially.
 *
 * You can track the above state via the “window-state-event” signal
 * on #GtkWidget.
 *
 * Note that, according to the
 * [Extended Window Manager Hints Specification](http://www.freedesktop.org/Standards/wm-spec),
 * the above state is mainly meant for user preferences and should not
 * be used by applications e.g. for drawing attention to their
 * dialogs.
 *
 * Since: 2.4
 **/
void
gtk_window_set_keep_above (GtkWindow *window,
			   gboolean   setting)
{
  GdkWindow *toplevel;

  g_return_if_fail (GTK_IS_WINDOW (window));

  setting = setting != FALSE;

  window->priv->above_initially = setting;
  window->priv->below_initially &= !setting;

  toplevel = _gtk_widget_get_window (GTK_WIDGET (window));

  if (toplevel != NULL)
    gdk_window_set_keep_above (toplevel, setting);
}

/**
 * gtk_window_set_keep_below:
 * @window: a #GtkWindow
 * @setting: whether to keep @window below other windows
 *
 * Asks to keep @window below, so that it stays in bottom. Note that
 * you shouldn’t assume the window is definitely below afterward,
 * because other entities (e.g. the user or
 * [window manager][gtk-X11-arch]) could not keep it below,
 * and not all window managers support putting windows below. But
 * normally the window will be kept below. Just don’t write code
 * that crashes if not.
 *
 * It’s permitted to call this function before showing a window,
 * in which case the window will be kept below when it appears onscreen
 * initially.
 *
 * You can track the below state via the “window-state-event” signal
 * on #GtkWidget.
 *
 * Note that, according to the
 * [Extended Window Manager Hints Specification](http://www.freedesktop.org/Standards/wm-spec),
 * the above state is mainly meant for user preferences and should not
 * be used by applications e.g. for drawing attention to their
 * dialogs.
 *
 * Since: 2.4
 **/
void
gtk_window_set_keep_below (GtkWindow *window,
			   gboolean   setting)
{
  GdkWindow *toplevel;

  g_return_if_fail (GTK_IS_WINDOW (window));

  setting = setting != FALSE;

  window->priv->below_initially = setting;
  window->priv->above_initially &= !setting;

  toplevel = _gtk_widget_get_window (GTK_WIDGET (window));

  if (toplevel != NULL)
    gdk_window_set_keep_below (toplevel, setting);
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
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  resizable = (resizable != FALSE);

  if (priv->resizable != resizable)
    {
      priv->resizable = resizable;

      update_window_buttons (window);

      gtk_widget_queue_resize_no_redraw (GTK_WIDGET (window));

      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_RESIZABLE]);
    }
}

/**
 * gtk_window_get_resizable:
 * @window: a #GtkWindow
 *
 * Gets the value set by gtk_window_set_resizable().
 *
 * Returns: %TRUE if the user can resize the window
 **/
gboolean
gtk_window_get_resizable (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->priv->resizable;
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
 * typically “do what you mean.”
 *
 **/
void
gtk_window_set_gravity (GtkWindow *window,
			GdkGravity gravity)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  if (gravity != priv->gravity)
    {
      priv->gravity = gravity;

      /* gtk_window_move_resize() will adapt gravity
       */
      gtk_widget_queue_resize_no_redraw (GTK_WIDGET (window));

      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_GRAVITY]);
    }
}

/**
 * gtk_window_get_gravity:
 * @window: a #GtkWindow
 *
 * Gets the value set by gtk_window_set_gravity().
 *
 * Returns: (transfer none): window gravity
 **/
GdkGravity
gtk_window_get_gravity (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), 0);

  return window->priv->gravity;
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
 * will be done using the standard mechanism for the
 * [window manager][gtk-X11-arch] or windowing
 * system. Otherwise, GDK will try to emulate window resizing,
 * potentially not all that well, depending on the windowing system.
 */
void
gtk_window_begin_resize_drag  (GtkWindow     *window,
                               GdkWindowEdge  edge,
                               gint           button,
                               gint           root_x,
                               gint           root_y,
                               guint32        timestamp)
{
  GtkWidget *widget;
  GdkWindow *toplevel;

  g_return_if_fail (GTK_IS_WINDOW (window));
  widget = GTK_WIDGET (window);
  g_return_if_fail (gtk_widget_get_visible (widget));

  toplevel = _gtk_widget_get_window (widget);

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
 * Starts moving a window. This function is used if an application has
 * window movement grips. When GDK can support it, the window movement
 * will be done using the standard mechanism for the
 * [window manager][gtk-X11-arch] or windowing
 * system. Otherwise, GDK will try to emulate window movement,
 * potentially not all that well, depending on the windowing system.
 */
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
  widget = GTK_WIDGET (window);
  g_return_if_fail (gtk_widget_get_visible (widget));

  toplevel = _gtk_widget_get_window (widget);

  gdk_window_begin_move_drag (toplevel,
                              button,
                              root_x, root_y,
                              timestamp);
}

/**
 * gtk_window_set_screen:
 * @window: a #GtkWindow.
 * @screen: a #GdkScreen.
 *
 * Sets the #GdkScreen where the @window is displayed; if
 * the window is already mapped, it will be unmapped, and
 * then remapped on the new screen.
 *
 * Since: 2.2
 */
void
gtk_window_set_screen (GtkWindow *window,
		       GdkScreen *screen)
{
  GtkWindowPrivate *priv;
  GtkWidget *widget;
  GdkScreen *previous_screen;
  gboolean was_mapped;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_SCREEN (screen));

  priv = window->priv;

  if (screen == priv->screen)
    return;

  /* reset initial_fullscreen_monitor since they are relative to the screen */
  priv->initial_fullscreen_monitor = -1;
  
  widget = GTK_WIDGET (window);

  previous_screen = priv->screen;

  was_mapped = _gtk_widget_get_mapped (widget);

  if (was_mapped)
    gtk_widget_unmap (widget);
  if (_gtk_widget_get_realized (widget))
    gtk_widget_unrealize (widget);

  gtk_window_free_key_hash (window);
  priv->screen = screen;
  if (screen != previous_screen)
    {
#ifdef GDK_WINDOWING_X11
      if (previous_screen)
        {
          g_signal_handlers_disconnect_by_func (gtk_settings_get_for_screen (previous_screen),
                                                gtk_window_on_theme_variant_changed, window);
        }
      g_signal_connect (gtk_settings_get_for_screen (screen),
                        "notify::gtk-application-prefer-dark-theme",
                        G_CALLBACK (gtk_window_on_theme_variant_changed), window);
#endif

      _gtk_widget_propagate_screen_changed (widget, previous_screen);
    }
  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_SCREEN]);

  if (was_mapped)
    gtk_widget_map (widget);

  check_scale_changed (window);
}

static void
gtk_window_set_theme_variant (GtkWindow *window)
{
#ifdef GDK_WINDOWING_X11
  GdkWindow *gdk_window;
  gboolean   dark_theme_requested;

  g_object_get (gtk_settings_get_for_screen (window->priv->screen),
                "gtk-application-prefer-dark-theme", &dark_theme_requested,
                NULL);

  gdk_window = _gtk_widget_get_window (GTK_WIDGET (window));

  if (GDK_IS_X11_WINDOW (gdk_window))
    gdk_x11_window_set_theme_variant (gdk_window,
                                      dark_theme_requested ? "dark" : NULL);
#endif
}

#ifdef GDK_WINDOWING_X11
static void
gtk_window_on_theme_variant_changed (GtkSettings *settings,
                                     GParamSpec  *pspec,
                                     GtkWindow   *window)
{
  if (window->priv->type == GTK_WINDOW_TOPLEVEL)
    gtk_window_set_theme_variant (window);
}
#endif

static GdkScreen *
gtk_window_check_screen (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  if (priv->screen)
    return priv->screen;
  else
    {
      g_warning ("Screen for GtkWindow not set; you must always set\n"
		 "a screen for a GtkWindow before using the window");
      return NULL;
    }
}

/**
 * gtk_window_get_screen:
 * @window: a #GtkWindow.
 *
 * Returns the #GdkScreen associated with @window.
 *
 * Returns: (transfer none): a #GdkScreen.
 *
 * Since: 2.2
 */
GdkScreen*
gtk_window_get_screen (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return window->priv->screen;
}

GdkScreen *
_gtk_window_get_screen (GtkWindow *window)
{
  return window->priv->screen;
}

/**
 * gtk_window_is_active:
 * @window: a #GtkWindow
 * 
 * Returns whether the window is part of the current active toplevel.
 * (That is, the toplevel window receiving keystrokes.)
 * The return value is %TRUE if the window is active toplevel itself.
 * You might use this function if you wanted to draw a widget
 * differently in an active window from a widget in an inactive window.
 * 
 * Returns: %TRUE if the window part of the current active window.
 *
 * Since: 2.4
 **/
gboolean
gtk_window_is_active (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->priv->is_active;
}

/**
 * gtk_window_get_group:
 * @window: (allow-none): a #GtkWindow, or %NULL
 *
 * Returns the group for @window or the default group, if
 * @window is %NULL or if @window does not have an explicit
 * window group.
 *
 * Returns: (transfer none): the #GtkWindowGroup for a window or the default group
 *
 * Since: 2.10
 */
GtkWindowGroup *
gtk_window_get_group (GtkWindow *window)
{
  if (window && window->priv->group)
    return window->priv->group;
  else
    {
      static GtkWindowGroup *default_group = NULL;

      if (!default_group)
	default_group = gtk_window_group_new ();

      return default_group;
    }
}

/**
 * gtk_window_has_group:
 * @window: a #GtkWindow
 *
 * Returns whether @window has an explicit window group.
 *
 * Returns: %TRUE if @window has an explicit window group.
 *
 * Since 2.22
 **/
gboolean
gtk_window_has_group (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->priv->group != NULL;
}

GtkWindowGroup *
_gtk_window_get_window_group (GtkWindow *window)
{
  return window->priv->group;
}

void
_gtk_window_set_window_group (GtkWindow      *window,
                              GtkWindowGroup *group)
{
  window->priv->group = group;
}

static gboolean
gtk_window_activate_menubar (GtkWindow   *window,
                             GdkEventKey *event)
{
  GtkWindowPrivate *priv = window->priv;
  guint keyval = 0;
  GdkModifierType mods = 0;

  gtk_accelerator_parse (MENU_BAR_ACCEL, &keyval, &mods);

  if (keyval == 0)
    {
      g_warning ("Failed to parse menu bar accelerator '%s'", MENU_BAR_ACCEL);
      return FALSE;
    }

  /* FIXME this is wrong, needs to be in the global accel resolution
   * thing, to properly consider i18n etc., but that probably requires
   * AccelGroup changes etc.
   */
  if (event->keyval == keyval &&
      ((event->state & gtk_accelerator_get_default_mod_mask ()) ==
       (mods & gtk_accelerator_get_default_mod_mask ())))
    {
      GList *tmp_menubars;
      GList *menubars;
      GtkMenuShell *menu_shell;
      GtkWidget *focus;

      focus = gtk_window_get_focus (window);

      if (priv->title_box != NULL &&
          (focus == NULL || !gtk_widget_is_ancestor (focus, priv->title_box)) &&
          gtk_widget_child_focus (priv->title_box, GTK_DIR_TAB_FORWARD))
        return TRUE;

      tmp_menubars = _gtk_menu_bar_get_viewable_menu_bars (window);
      if (tmp_menubars == NULL)
        return FALSE;

      menubars = _gtk_container_focus_sort (GTK_CONTAINER (window), tmp_menubars,
                                            GTK_DIR_TAB_FORWARD, NULL);
      g_list_free (tmp_menubars);

      if (menubars == NULL)
        return FALSE;

      menu_shell = GTK_MENU_SHELL (menubars->data);

      _gtk_menu_shell_set_keyboard_mode (menu_shell, TRUE);
      gtk_menu_shell_select_first (menu_shell, FALSE);

      g_list_free (menubars);

      return TRUE;
    }
  return FALSE;
}

static void
gtk_window_mnemonic_hash_foreach (guint      keyval,
				  GSList    *targets,
				  gpointer   data)
{
  struct {
    GtkWindow *window;
    GtkWindowKeysForeachFunc func;
    gpointer func_data;
  } *info = data;

  (*info->func) (info->window, keyval, info->window->priv->mnemonic_modifier, TRUE, info->func_data);
}

void
_gtk_window_keys_foreach (GtkWindow                *window,
			  GtkWindowKeysForeachFunc func,
			  gpointer                 func_data)
{
  GSList *groups;
  GtkMnemonicHash *mnemonic_hash;

  struct {
    GtkWindow *window;
    GtkWindowKeysForeachFunc func;
    gpointer func_data;
  } info;

  info.window = window;
  info.func = func;
  info.func_data = func_data;

  mnemonic_hash = gtk_window_get_mnemonic_hash (window, FALSE);
  if (mnemonic_hash)
    _gtk_mnemonic_hash_foreach (mnemonic_hash,
				gtk_window_mnemonic_hash_foreach, &info);

  groups = gtk_accel_groups_from_object (G_OBJECT (window));
  while (groups)
    {
      GtkAccelGroup *group = groups->data;
      gint i;

      for (i = 0; i < group->priv->n_accels; i++)
	{
	  GtkAccelKey *key = &group->priv->priv_accels[i].key;
	  
	  if (key->accel_key)
	    (*func) (window, key->accel_key, key->accel_mods, FALSE, func_data);
	}
      
      groups = groups->next;
    }

  if (window->priv->application)
    {
      GtkApplicationAccels *app_accels;

      app_accels = gtk_application_get_application_accels (window->priv->application);
      gtk_application_accels_foreach_key (app_accels, window, func, func_data);
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
  guint is_mnemonic : 1;
};

static void 
window_key_entry_destroy (gpointer data)
{
  g_slice_free (GtkWindowKeyEntry, data);
}

static void
add_to_key_hash (GtkWindow      *window,
		 guint           keyval,
		 GdkModifierType modifiers,
		 gboolean        is_mnemonic,
		 gpointer        data)
{
  GtkKeyHash *key_hash = data;

  GtkWindowKeyEntry *entry = g_slice_new (GtkWindowKeyEntry);

  entry->keyval = keyval;
  entry->modifiers = modifiers;
  entry->is_mnemonic = is_mnemonic;

  /* GtkAccelGroup stores lowercased accelerators. To deal
   * with this, if <Shift> was specified, uppercase.
   */
  if (modifiers & GDK_SHIFT_MASK)
    {
      if (keyval == GDK_KEY_Tab)
	keyval = GDK_KEY_ISO_Left_Tab;
      else
	keyval = gdk_keyval_to_upper (keyval);
    }
  
  _gtk_key_hash_add_entry (key_hash, keyval, entry->modifiers, entry);
}

static GtkKeyHash *
gtk_window_get_key_hash (GtkWindow *window)
{
  GdkScreen *screen = gtk_window_check_screen (window);
  GtkKeyHash *key_hash = g_object_get_qdata (G_OBJECT (window), quark_gtk_window_key_hash);
  
  if (key_hash)
    return key_hash;
  
  key_hash = _gtk_key_hash_new (gdk_keymap_get_for_display (gdk_screen_get_display (screen)),
				(GDestroyNotify)window_key_entry_destroy);
  _gtk_window_keys_foreach (window, add_to_key_hash, key_hash);
  g_object_set_qdata (G_OBJECT (window), quark_gtk_window_key_hash, key_hash);

  return key_hash;
}

static void
gtk_window_free_key_hash (GtkWindow *window)
{
  GtkKeyHash *key_hash = g_object_get_qdata (G_OBJECT (window), quark_gtk_window_key_hash);
  if (key_hash)
    {
      _gtk_key_hash_free (key_hash);
      g_object_set_qdata (G_OBJECT (window), quark_gtk_window_key_hash, NULL);
    }
}

/**
 * gtk_window_activate_key:
 * @window:  a #GtkWindow
 * @event:   a #GdkEventKey
 *
 * Activates mnemonics and accelerators for this #GtkWindow. This is normally
 * called by the default ::key_press_event handler for toplevel windows,
 * however in some cases it may be useful to call this directly when
 * overriding the standard key handling for a toplevel window.
 *
 * Returns: %TRUE if a mnemonic or accelerator was found and activated.
 *
 * Since: 2.4
 */
gboolean
gtk_window_activate_key (GtkWindow   *window,
			 GdkEventKey *event)
{
  GtkKeyHash *key_hash;
  GtkWindowKeyEntry *found_entry = NULL;
  gboolean enable_accels;

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  key_hash = gtk_window_get_key_hash (window);

  if (key_hash)
    {
      GSList *tmp_list;
      GSList *entries = _gtk_key_hash_lookup (key_hash,
					      event->hardware_keycode,
					      event->state,
					      gtk_accelerator_get_default_mod_mask (),
					      event->group);

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (window)),
                    "gtk-enable-accels", &enable_accels,
                    NULL);

      for (tmp_list = entries; tmp_list; tmp_list = tmp_list->next)
	{
	  GtkWindowKeyEntry *entry = tmp_list->data;
	  if (entry->is_mnemonic)
            {
              found_entry = entry;
              break;
            }
          else 
            {
              if (enable_accels && !found_entry)
                {
	          found_entry = entry;
                }
            }
	}

      g_slist_free (entries);
    }

  if (found_entry)
    {
      if (found_entry->is_mnemonic)
        {
          return gtk_window_mnemonic_activate (window, found_entry->keyval,
                                               found_entry->modifiers);
        }
      else
        {
          if (enable_accels)
            {
              if (gtk_accel_groups_activate (G_OBJECT (window), found_entry->keyval, found_entry->modifiers))
                return TRUE;

              if (window->priv->application)
                {
                  GtkWidget *focused_widget;
                  GtkActionMuxer *muxer;
                  GtkApplicationAccels *app_accels;

                  focused_widget = gtk_window_get_focus (window);

                  if (focused_widget)
                    muxer = _gtk_widget_get_action_muxer (focused_widget, FALSE);
                  else
                    muxer = _gtk_widget_get_action_muxer (GTK_WIDGET (window), FALSE);

                  if (muxer == NULL)
                    return FALSE;

                  app_accels = gtk_application_get_application_accels (window->priv->application);
                  return gtk_application_accels_activate (app_accels,
                                                          G_ACTION_GROUP (muxer),
                                                          found_entry->keyval, found_entry->modifiers);
                }
            }
        }
    }

  return gtk_window_activate_menubar (window, event);
}

/**
 * _gtk_window_set_is_active:
 * @window: a #GtkWindow
 * @is_active: %TRUE if the window is in the currently active toplevel
 * 
 * Internal function that sets whether the #GtkWindow is part
 * of the currently active toplevel window (taking into account inter-process
 * embedding.)
 **/
void
_gtk_window_set_is_active (GtkWindow *window,
			   gboolean   is_active)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  is_active = is_active != FALSE;

  if (is_active != priv->is_active)
    {
      GtkWidget *widget = GTK_WIDGET (window);

      priv->is_active = is_active;

      if (is_active)
        {
          if (priv->focus_widget &&
              priv->focus_widget != widget &&
              !gtk_widget_has_focus (priv->focus_widget))
            do_focus_change (priv->focus_widget, TRUE);
        }
      else
        {
          if (priv->focus_widget &&
              priv->focus_widget != widget &&
              gtk_widget_has_focus (priv->focus_widget))
            do_focus_change (priv->focus_widget, FALSE);
        }

      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_IS_ACTIVE]);
    }
}

/**
 * gtk_window_set_auto_startup_notification:
 * @setting: %TRUE to automatically do startup notification
 *
 * By default, after showing the first #GtkWindow, GTK+ calls 
 * gdk_notify_startup_complete().  Call this function to disable 
 * the automatic startup notification. You might do this if your 
 * first window is a splash screen, and you want to delay notification 
 * until after your real main window has been shown, for example.
 *
 * In that example, you would disable startup notification
 * temporarily, show your splash screen, then re-enable it so that
 * showing the main window would automatically result in notification.
 * 
 * Since: 2.2
 **/
void
gtk_window_set_auto_startup_notification (gboolean setting)
{
  disable_startup_notification = !setting;
}

/**
 * gtk_window_get_window_type:
 * @window: a #GtkWindow
 *
 * Gets the type of the window. See #GtkWindowType.
 *
 * Returns: the type of the window
 *
 * Since: 2.20
 **/
GtkWindowType
gtk_window_get_window_type (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), GTK_WINDOW_TOPLEVEL);

  return window->priv->type;
}

/**
 * gtk_window_get_mnemonics_visible:
 * @window: a #GtkWindow
 *
 * Gets the value of the #GtkWindow:mnemonics-visible property.
 *
 * Returns: %TRUE if mnemonics are supposed to be visible
 * in this window.
 *
 * Since: 2.20
 */
gboolean
gtk_window_get_mnemonics_visible (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->priv->mnemonics_visible;
}

/**
 * gtk_window_set_mnemonics_visible:
 * @window: a #GtkWindow
 * @setting: the new value
 *
 * Sets the #GtkWindow:mnemonics-visible property.
 *
 * Since: 2.20
 */
void
gtk_window_set_mnemonics_visible (GtkWindow *window,
                                  gboolean   setting)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  setting = setting != FALSE;

  if (priv->mnemonics_visible != setting)
    {
      priv->mnemonics_visible = setting;
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_MNEMONICS_VISIBLE]);
    }

  if (priv->mnemonics_display_timeout_id)
    {
      g_source_remove (priv->mnemonics_display_timeout_id);
      priv->mnemonics_display_timeout_id = 0;
    }

  priv->mnemonics_visible_set = TRUE;
}

static gboolean
schedule_mnemonics_visible_cb (gpointer data)
{
  GtkWindow *window = data;

  window->priv->mnemonics_display_timeout_id = 0;

  gtk_window_set_mnemonics_visible (window, TRUE);

  return FALSE;
}

void
_gtk_window_schedule_mnemonics_visible (GtkWindow *window)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (window->priv->mnemonics_display_timeout_id)
    return;

  window->priv->mnemonics_display_timeout_id =
    gdk_threads_add_timeout (MNEMONICS_DELAY, schedule_mnemonics_visible_cb, window);
  g_source_set_name_by_id (window->priv->mnemonics_display_timeout_id, "[gtk+] schedule_mnemonics_visible_cb");
}

/**
 * gtk_window_get_focus_visible:
 * @window: a #GtkWindow
 *
 * Gets the value of the #GtkWindow:focus-visible property.
 *
 * Returns: %TRUE if “focus rectangles” are supposed to be visible
 *     in this window.
 *
 * Since: 3.2
 */
gboolean
gtk_window_get_focus_visible (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return window->priv->focus_visible;
}

/**
 * gtk_window_set_focus_visible:
 * @window: a #GtkWindow
 * @setting: the new value
 *
 * Sets the #GtkWindow:focus-visible property.
 *
 * Since: 3.2
 */
void
gtk_window_set_focus_visible (GtkWindow *window,
                              gboolean   setting)
{
  GtkWindowPrivate *priv;

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = window->priv;

  setting = setting != FALSE;

  if (priv->focus_visible != setting)
    {
      priv->focus_visible = setting;
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_FOCUS_VISIBLE]);
    }
}

/**
 * gtk_window_set_has_user_ref_count:
 * @window: a #GtkWindow
 * @setting: the new value
 *
 * Tells GTK+ whether to drop its extra reference to the window
 * when gtk_widget_destroy() is called.
 *
 * This function is only exported for the benefit of language
 * bindings which may need to keep the window alive until their
 * wrapper object is garbage collected. There is no justification
 * for ever calling this function in an application.
 *
 * Since: 3.0
 */
void
gtk_window_set_has_user_ref_count (GtkWindow *window,
                                   gboolean   setting)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  window->priv->has_user_ref_count = setting;
}

static void
ensure_state_flag_backdrop (GtkWidget *widget)
{
  GdkWindow *window;
  gboolean window_focused = TRUE;

  window = _gtk_widget_get_window (widget);

  window_focused = gdk_window_get_state (window) & GDK_WINDOW_STATE_FOCUSED;

  if (!window_focused)
    gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_BACKDROP, FALSE);
  else
    gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_BACKDROP);
}

void
_gtk_window_get_shadow_width (GtkWindow *window,
                              GtkBorder *border)
{
  get_shadow_width (window, border);
}

void
_gtk_window_add_popover (GtkWindow *window,
                         GtkWidget *popover,
                         GtkWidget *parent,
                         gboolean   clamp_allocation)
{
  GtkWindowPrivate *priv;
  GtkWindowPopover *data;
  AtkObject *accessible;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_WIDGET (popover));
  g_return_if_fail (GTK_IS_WIDGET (parent));
  g_return_if_fail (_gtk_widget_get_parent (popover) == NULL);
  g_return_if_fail (gtk_widget_is_ancestor (parent, GTK_WIDGET (window)));

  priv = window->priv;

  if (_gtk_window_has_popover (window, popover))
    return;

  data = g_new0 (GtkWindowPopover, 1);
  data->widget = popover;
  data->parent = parent;
  data->clamp_allocation = !!clamp_allocation;
  priv->popovers = g_list_prepend (priv->popovers, data);

  if (_gtk_widget_get_realized (GTK_WIDGET (window)))
    popover_realize (popover, data, window);

  gtk_widget_set_parent (popover, GTK_WIDGET (window));

  accessible = gtk_widget_get_accessible (GTK_WIDGET (window));
  _gtk_container_accessible_add_child (GTK_CONTAINER_ACCESSIBLE (accessible),
                                       gtk_widget_get_accessible (popover), -1);
}

void
_gtk_window_remove_popover (GtkWindow *window,
                            GtkWidget *popover)
{
  GtkWindowPrivate *priv;
  GtkWindowPopover *data;
  AtkObject *accessible;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_WIDGET (popover));

  priv = window->priv;

  data = _gtk_window_has_popover (window, popover);

  if (!data)
    return;

  g_object_ref (popover);
  gtk_widget_unparent (popover);

  popover_unmap (popover, data);

  if (_gtk_widget_get_realized (GTK_WIDGET (popover)))
    popover_unrealize (popover, data, window);

  priv->popovers = g_list_remove (priv->popovers, data);

  accessible = gtk_widget_get_accessible (GTK_WIDGET (window));
  _gtk_container_accessible_remove_child (GTK_CONTAINER_ACCESSIBLE (accessible),
                                          gtk_widget_get_accessible (popover), -1);
  popover_destroy (data);
  g_object_unref (popover);
}

void
_gtk_window_set_popover_position (GtkWindow                   *window,
                                  GtkWidget                   *popover,
                                  GtkPositionType              pos,
                                  const cairo_rectangle_int_t *rect)
{
  gboolean need_resize;
  gboolean need_move;
  GtkWindowPopover *data;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_WIDGET (popover));

  data = _gtk_window_has_popover (window, popover);

  if (!data)
    {
      g_warning ("Widget %s(%p) is not a popover of window %s",
                 gtk_widget_get_name (popover), popover,
                 gtk_widget_get_name (GTK_WIDGET (window)));
      return;
    }


  /* Don't queue a resize if the position as well as the size are the same */
  need_move = data->pos    != pos ||
              data->rect.x != rect->x ||
              data->rect.y != rect->y;

  need_resize = data->pos != pos ||
                data->rect.width  != rect->width ||
                data->rect.height != rect->height;

  data->rect = *rect;
  data->pos = pos;

  if (gtk_widget_is_visible (popover) && !data->window &&
      gtk_widget_get_realized (GTK_WIDGET (window)))
    {
      popover_realize (popover, data, window);
      popover_map (popover, data);
    }

  if (need_resize)
    {
      gtk_widget_queue_resize (popover);
    }
  else if (need_move)
    {
      cairo_rectangle_int_t new_size;
      popover_get_rect (data, window, &new_size);
      gdk_window_move (data->window, new_size.x, new_size.y);
    }
}

void
_gtk_window_get_popover_position (GtkWindow             *window,
                                  GtkWidget             *popover,
                                  GtkPositionType       *pos,
                                  cairo_rectangle_int_t *rect)
{
  GtkWindowPopover *data;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_WIDGET (popover));

  data = _gtk_window_has_popover (window, popover);

  if (!data)
    {
      g_warning ("Widget %s(%p) is not a popover of window %s",
                 gtk_widget_get_name (popover), popover,
                 gtk_widget_get_name (GTK_WIDGET (window)));
      return;
    }

  if (pos)
    *pos = data->pos;

  if (rect)
    *rect = data->rect;
}

/*<private>
 * _gtk_window_get_popover_parent:
 * @window: A #GtkWindow
 * @popover: A popover #GtkWidget
 *
 * Returns the conceptual parent of this popover, the real
 * parent will always be @window.
 *
 * Returns: (nullable): The conceptual parent widget, or %NULL.
 **/
GtkWidget *
_gtk_window_get_popover_parent (GtkWindow *window,
                                GtkWidget *popover)
{
  GtkWindowPopover *data;

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (popover), NULL);

  data = _gtk_window_has_popover (window, popover);

  if (data && data->parent)
    return data->parent;

  return NULL;
}

/*<private>
 * _gtk_window_is_popover_widget:
 * @window: A #GtkWindow
 * @possible_popover: A possible popover of @window
 *
 * Returns #TRUE if @possible_popover is a popover of @window.
 *
 * Returns: Whether the widget is a popover of @window
 **/
gboolean
_gtk_window_is_popover_widget (GtkWindow *window,
                               GtkWidget *possible_popover)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (possible_popover), FALSE);

  return _gtk_window_has_popover (window, possible_popover) != NULL;
}

void
_gtk_window_raise_popover (GtkWindow *window,
                           GtkWidget *widget)
{
  GtkWindowPrivate *priv = window->priv;
  GList *link;

  for (link = priv->popovers; link; link = link->next)
    {
      GtkWindowPopover *popover = link->data;

      if (popover->widget != widget)
        continue;

      priv->popovers = g_list_remove_link (priv->popovers, link);
      priv->popovers = g_list_append (priv->popovers, link->data);
      g_list_free (link);
      break;
    }
  gtk_window_restack_popovers (window);
}

static GtkWidget *inspector_window = NULL;

static guint gtk_window_update_debugging_id;

static void set_warn_again (gboolean warn);

static void
warn_response (GtkDialog *dialog,
               gint       response)
{
  GtkWidget *check;
  gboolean remember;

  check = g_object_get_data (G_OBJECT (dialog), "check");
  remember = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check));

  gtk_widget_destroy (GTK_WIDGET (dialog));
  g_object_set_data (G_OBJECT (inspector_window), "warning_dialog", NULL);
  if (response == GTK_RESPONSE_NO)
    {
      GtkWidget *window;

      if (gtk_window_update_debugging_id)
        {
          g_source_remove (gtk_window_update_debugging_id);
          gtk_window_update_debugging_id = 0;
        }

      /* Steal reference into temp variable, so not to mess up with
       * inspector_window during gtk_widget_destroy().
       */
      window = inspector_window;
      inspector_window = NULL;
      gtk_widget_destroy (window);
    }
  else
    {
      set_warn_again (!remember);
    }
}

static gboolean
update_debugging (gpointer data)
{
  gtk_inspector_window_rescan (inspector_window);
  gtk_window_update_debugging_id = 0;
  return G_SOURCE_REMOVE;
}

static void
gtk_window_update_debugging (void)
{
  if (inspector_window &&
      gtk_window_update_debugging_id == 0)
    {
      gtk_window_update_debugging_id = gdk_threads_add_idle (update_debugging, NULL);
      g_source_set_name_by_id (gtk_window_update_debugging_id, "[gtk+] gtk_window_update_debugging");
    }
}

static void
gtk_window_set_debugging (gboolean enable,
                          gboolean select,
                          gboolean warn)
{
  GtkWidget *dialog = NULL;
  GtkWidget *area;
  GtkWidget *check;

  if (inspector_window == NULL)
    {
      gtk_inspector_init ();
      inspector_window = gtk_inspector_window_new ();
      g_signal_connect (inspector_window, "delete-event",
                        G_CALLBACK (gtk_widget_hide_on_delete), NULL);

      if (warn)
        {
          dialog = gtk_message_dialog_new (GTK_WINDOW (inspector_window),
                                           GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_QUESTION,
                                           GTK_BUTTONS_NONE,
                                           _("Do you want to use GTK+ Inspector?"));
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
              _("GTK+ Inspector is an interactive debugger that lets you explore and "
                "modify the internals of any GTK+ application. Using it may cause the "
                "application to break or crash."));

          area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));
          check = gtk_check_button_new_with_label (_("Don’t show this message again"));
          gtk_widget_set_margin_start (check, 10);
          gtk_widget_show (check);
          gtk_container_add (GTK_CONTAINER (area), check);
          g_object_set_data (G_OBJECT (dialog), "check", check);
          gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_NO);
          gtk_dialog_add_button (GTK_DIALOG (dialog), _("_OK"), GTK_RESPONSE_YES);
          g_signal_connect (dialog, "response", G_CALLBACK (warn_response), NULL);
          g_object_set_data (G_OBJECT (inspector_window), "warning_dialog", dialog);
        }
    }

  dialog = g_object_get_data (G_OBJECT (inspector_window), "warning_dialog");

  if (enable)
    {
      gtk_window_present (GTK_WINDOW (inspector_window));
      if (dialog)
        gtk_widget_show (dialog);

      if (select)
        gtk_inspector_window_select_widget_under_pointer (GTK_INSPECTOR_WINDOW (inspector_window));
    }
  else
    {
      if (dialog)
        gtk_widget_hide (dialog);
      gtk_widget_hide (inspector_window);
    }
}

/**
 * gtk_window_set_interactive_debugging:
 * @enable: %TRUE to enable interactive debugging
 *
 * Opens or closes the [interactive debugger][interactive-debugging],
 * which offers access to the widget hierarchy of the application
 * and to useful debugging tools.
 *
 * Since: 3.14
 */
void
gtk_window_set_interactive_debugging (gboolean enable)
{
  gtk_window_set_debugging (enable, FALSE, FALSE);
}

static gboolean
inspector_keybinding_enabled (gboolean *warn)
{
  GSettingsSchema *schema;
  GSettings *settings;
  gboolean enabled;

  enabled = FALSE;
  *warn = FALSE;

  schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                            "org.gtk.Settings.Debug",
                                            TRUE);

  if (schema)
    {
      settings = g_settings_new_full (schema, NULL, NULL);
      enabled = g_settings_get_boolean (settings, "enable-inspector-keybinding");
      *warn = g_settings_get_boolean (settings, "inspector-warning");
      g_object_unref (settings);
      g_settings_schema_unref (schema);
    }

  return enabled;
}

static void
set_warn_again (gboolean warn)
{
  GSettingsSchema *schema;
  GSettings *settings;

  schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                            "org.gtk.Settings.Debug",
                                            TRUE);

  if (schema)
    {
      settings = g_settings_new_full (schema, NULL, NULL);
      g_settings_set_boolean (settings, "inspector-warning", warn);
      g_object_unref (settings);
      g_settings_schema_unref (schema);
    }
}

static gboolean
gtk_window_enable_debugging (GtkWindow *window,
                             gboolean   toggle)
{
  gboolean warn;

  if (!inspector_keybinding_enabled (&warn))
    return FALSE;

  if (toggle)
    {
      if (GTK_IS_WIDGET (inspector_window) &&
          gtk_widget_is_visible (inspector_window))
        gtk_window_set_debugging (FALSE, FALSE, FALSE);
      else
        gtk_window_set_debugging (TRUE, FALSE, warn);
    }
  else
    gtk_window_set_debugging (TRUE, TRUE, warn);

  return TRUE;
}

void
gtk_window_set_use_subsurface (GtkWindow *window,
                               gboolean   use_subsurface)
{
  GtkWindowPrivate *priv = window->priv;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (!_gtk_widget_get_realized (GTK_WIDGET (window)));

  priv->use_subsurface = use_subsurface;
}

void
gtk_window_set_hardcoded_window (GtkWindow *window,
                                 GdkWindow *gdk_window)
{
  GtkWindowPrivate *priv = window->priv;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (!_gtk_widget_get_realized (GTK_WIDGET (window)));

  g_set_object (&priv->hardcoded_window, gdk_window);
}

#ifdef GDK_WINDOWING_WAYLAND
typedef struct {
  GtkWindow *window;
  GtkWindowHandleExported callback;
  gpointer user_data;
} WaylandWindowHandleExportedData;

static void
wayland_window_handle_exported (GdkWindow  *window,
                                const char *wayland_handle_str,
                                gpointer    user_data)
{
  WaylandWindowHandleExportedData *data = user_data;
  char *handle_str;

  handle_str = g_strdup_printf ("wayland:%s", wayland_handle_str);
  data->callback (data->window, handle_str, data->user_data);
  g_free (handle_str);

  g_free (data);
}
#endif

gboolean
gtk_window_export_handle (GtkWindow               *window,
                          GtkWindowHandleExported  callback,
                          gpointer                 user_data)
{

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
      GdkWindow *gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
      char *handle_str;
      guint32 xid = (guint32) gdk_x11_window_get_xid (gdk_window);

      handle_str = g_strdup_printf ("x11:%x", xid);
      callback (window, handle_str, user_data);

      return TRUE;
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
      GdkWindow *gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
      WaylandWindowHandleExportedData *data;

      data = g_new0 (WaylandWindowHandleExportedData, 1);
      data->window = window;
      data->callback = callback;
      data->user_data = user_data;

      if (!gdk_wayland_window_export_handle (gdk_window,
                                             wayland_window_handle_exported,
                                             data,
                                             g_free))
        {
          g_free (data);
          return FALSE;
        }
      else
        {
          return TRUE;
        }
    }
#endif

  g_warning ("Couldn't export handle, unsupported windowing system");

  return FALSE;
}

void
gtk_window_unexport_handle (GtkWindow *window)
{
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
      GdkWindow *gdk_window = gtk_widget_get_window (GTK_WIDGET (window));

      gdk_wayland_window_unexport_handle (gdk_window);
    }
#endif
}

GskRenderer *
gtk_window_get_renderer (GtkWindow *window)
{
  GtkWindowPrivate *priv = window->priv;

  return priv->renderer;
}
