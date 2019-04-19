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

/**
 * SECTION:gtkmenu
 * @Short_description: A menu widget
 * @Title: GtkMenu
 *
 * A #GtkMenu is a #GtkMenuShell that implements a drop down menu
 * consisting of a list of #GtkMenuItem objects which can be navigated
 * and activated by the user to perform application functions.
 *
 * A #GtkMenu is most commonly dropped down by activating a
 * #GtkMenuItem in a #GtkMenuBar or popped up by activating a
 * #GtkMenuItem in another #GtkMenu.
 *
 * A #GtkMenu can also be popped up by activating a #GtkComboBox.
 * Other composite widgets such as the #GtkNotebook can pop up a
 * #GtkMenu as well.
 *
 * Applications can display a #GtkMenu as a popup menu by calling one of the
 * gtk_menu_popup_*() function. The example below shows how an application can
 * pop up a menu when the 3rd mouse button is pressed.
 *
 * ## Connecting the popup signal handler.
 *
 * |[<!-- language="C" -->
 *   // connect our handler which will popup the menu
 *   gesture = gtk_gesture_multi_press_new (window);
 *   gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture),
 *                                  GDK_BUTTON_SECONDARY);
 *   g_signal_connect (gesture, "begin", G_CALLBACK (my_popup_handler), menu);
 * ]|
 *
 * ## Signal handler which displays a popup menu.
 *
 * |[<!-- language="C" -->
 * static void
 * my_popup_handler (GtkGesture       *gesture,
 *                   GdkEventSequence *sequence
 *                   gpointer          data)
 * {
 *   GtkMenu *menu = data;
 *   const GdkEvent *event;
 *
 *   event = gtk_gesture_get_last_event (gesture, sequence);
 *   gtk_menu_popup_at_pointer (menu, event);
 * }
 * ]|
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * menu
 * ├── arrow.top
 * ├── <child>
 * ┊
 * ├── <child>
 * ╰── arrow.bottom
 * ]|
 *
 * The main CSS node of GtkMenu has name menu, and there are two subnodes
 * with name arrow, for scrolling menu arrows. These subnodes get the
 * .top and .bottom style classes.
 */

#include "config.h"

#include "gtkmenuprivate.h"

#include "gtkaccellabel.h"
#include "gtkaccelmap.h"
#include "gtkadjustment.h"
#include "gtkbindings.h"
#include "gtkbox.h"
#include "gtkcheckmenuitemprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkdnd.h"
#include "gtkeventcontrollerscroll.h"
#include "gtkiconprivate.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenuitemprivate.h"
#include "gtkmenushellprivate.h"
#include "gtkprivate.h"
#include "gtkscrollbar.h"
#include "gtksettings.h"
#include "gtksnapshot.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetpath.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowgroup.h"
#include "gtkwindowprivate.h"
#include "gtkeventcontrollerkey.h"

#include "a11y/gtkmenuaccessible.h"

#include "gdk/gdk-private.h"

#include <gobject/gvaluecollector.h>
#include <string.h>


#define NAVIGATION_REGION_OVERSHOOT 50  /* How much the navigation region
                                         * extends below the submenu
                                         */

#define MENU_SCROLL_STEP1      8
#define MENU_SCROLL_STEP2     15
#define MENU_SCROLL_FAST_ZONE  8
#define MENU_SCROLL_TIMEOUT1  50
#define MENU_SCROLL_TIMEOUT2  20

#define MENU_POPUP_DELAY     225

#define ATTACH_INFO_KEY "gtk-menu-child-attach-info-key"
#define ATTACHED_MENUS "gtk-attached-menus"

typedef struct _GtkMenuAttachData  GtkMenuAttachData;
typedef struct _GtkMenuPopdownData GtkMenuPopdownData;

struct _GtkMenuAttachData
{
  GtkWidget *attach_widget;
  GtkMenuDetachFunc detacher;
};

struct _GtkMenuPopdownData
{
  GtkMenu *menu;
  GdkDevice *device;
};

typedef struct
{
  gint effective_top_attach;
} AttachInfo;

enum {
  MOVE_SCROLL,
  POPPED_UP,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_ACCEL_GROUP,
  PROP_ACCEL_PATH,
  PROP_ATTACH_WIDGET,
  PROP_TEAROFF_STATE,
  PROP_TEAROFF_TITLE,
  PROP_MONITOR,
  PROP_RESERVE_TOGGLE_SIZE,
  PROP_ANCHOR_HINTS,
  PROP_RECT_ANCHOR_DX,
  PROP_RECT_ANCHOR_DY,
  PROP_MENU_TYPE_HINT
};

static void     gtk_menu_set_property      (GObject          *object,
                                            guint             prop_id,
                                            const GValue     *value,
                                            GParamSpec       *pspec);
static void     gtk_menu_get_property      (GObject          *object,
                                            guint             prop_id,
                                            GValue           *value,
                                            GParamSpec       *pspec);
static void     gtk_menu_finalize          (GObject          *object);
static void     gtk_menu_destroy           (GtkWidget        *widget);
static void     gtk_menu_realize           (GtkWidget        *widget);
static void     gtk_menu_unrealize         (GtkWidget        *widget);
static void     gtk_menu_size_allocate     (GtkWidget        *widget,
                                            int               widget_width,
                                            int               widget_height,
                                            int               baseline);
static void     gtk_menu_show              (GtkWidget        *widget);
static void     gtk_menu_snapshot          (GtkWidget        *widget,
                                            GtkSnapshot      *snapshot);
static void     gtk_menu_motion            (GtkEventController *controller,
                                            double              x,
                                            double              y,
                                            gpointer            user_data);
static void     gtk_menu_enter             (GtkEventController *controller,
                                            double              x,
                                            double              y,
                                            GdkCrossingMode     mode,
                                            GdkNotifyType       detail,
                                            gpointer            user_data);
static void     gtk_menu_leave             (GtkEventController *controller,
                                            GdkCrossingMode     mode,
                                            GdkNotifyType       detail,
                                            gpointer            user_data);
static gboolean gtk_menu_key_pressed       (GtkEventControllerKey *controller,
                                            guint                  keyval,
                                            guint                  keycode,
                                            GdkModifierType        state,
                                            GtkMenu               *menu);
static void     gtk_menu_scroll_to         (GtkMenu          *menu,
                                            gint              offset);
static void     gtk_menu_grab_notify       (GtkWidget        *widget,
                                            gboolean          was_grabbed);
static gboolean gtk_menu_captured_event    (GtkWidget        *widget,
                                            GdkEvent         *event);

static gboolean gtk_menu_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                                   gdouble                   dx,
                                                   gdouble                   dy,
                                                   GtkMenu                  *menu);

static void     gtk_menu_stop_scrolling         (GtkMenu  *menu);
static void     gtk_menu_remove_scroll_timeout  (GtkMenu  *menu);
static gboolean gtk_menu_scroll_timeout         (gpointer  data);

static void     gtk_menu_scroll_item_visible (GtkMenuShell    *menu_shell,
                                              GtkWidget       *menu_item);
static void     gtk_menu_select_item       (GtkMenuShell     *menu_shell,
                                            GtkWidget        *menu_item);
static void     gtk_menu_real_insert       (GtkMenuShell     *menu_shell,
                                            GtkWidget        *child,
                                            gint              position);
static void     gtk_menu_handle_scrolling  (GtkMenu          *menu,
                                            gint              event_x,
                                            gint              event_y,
                                            gboolean          enter);
static gboolean gtk_menu_focus             (GtkWidget        *widget,
                                            GtkDirectionType direction);
static gint     gtk_menu_get_popup_delay   (GtkMenuShell     *menu_shell);
static void     gtk_menu_move_current      (GtkMenuShell     *menu_shell,
                                            GtkMenuDirectionType direction);
static void     gtk_menu_real_move_scroll  (GtkMenu          *menu,
                                            GtkScrollType     type);

static void     gtk_menu_stop_navigating_submenu       (GtkMenu          *menu);
static gboolean gtk_menu_navigating_submenu            (GtkMenu          *menu,
                                                        gint              event_x,
                                                        gint              event_y);

static void gtk_menu_deactivate     (GtkMenuShell      *menu_shell);
static void gtk_menu_position       (GtkMenu           *menu,
                                     gboolean           set_scroll_offset);
static void gtk_menu_remove         (GtkContainer      *menu,
                                     GtkWidget         *widget);

static void       menu_grab_transfer_surface_destroy (GtkMenu *menu);
static GdkSurface *menu_grab_transfer_surface_get    (GtkMenu *menu);

static gboolean gtk_menu_real_can_activate_accel (GtkWidget *widget,
                                                  guint      signal_id);
static void _gtk_menu_refresh_accel_paths (GtkMenu *menu,
                                           gboolean group_changed);
static void gtk_menu_measure (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline);
static void gtk_menu_pressed_cb (GtkGestureMultiPress *gesture,
                                 int                   n_press,
                                 double                x,
                                 double                y,
                                 gpointer              user_data);
static void gtk_menu_released_cb (GtkGestureMultiPress *gesture,
                                  int                   n_press,
                                  double                x,
                                  double                y,
                                  gpointer              user_data);


static const gchar attach_data_key[] = "gtk-menu-attach-data";

static guint menu_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkMenu, gtk_menu, GTK_TYPE_MENU_SHELL)

static void
menu_queue_resize (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;

  priv->have_layout = FALSE;
  gtk_widget_queue_resize (GTK_WIDGET (menu));
}

static void
attach_info_free (AttachInfo *info)
{
  g_slice_free (AttachInfo, info);
}

static AttachInfo *
get_attach_info (GtkWidget *child)
{
  GObject *object = G_OBJECT (child);
  AttachInfo *ai = g_object_get_data (object, ATTACH_INFO_KEY);

  if (!ai)
    {
      ai = g_slice_new0 (AttachInfo);
      g_object_set_data_full (object, I_(ATTACH_INFO_KEY), ai,
                              (GDestroyNotify) attach_info_free);
    }

  return ai;
}

static void
menu_ensure_layout (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;

  if (!priv->have_layout)
    {
      GtkMenuShell *menu_shell = GTK_MENU_SHELL (menu);
      GList *l;
      gchar *row_occupied;
      gint current_row;
      gint max_bottom_attach;

      /* Find extents of gridded portion
       */
      max_bottom_attach = 0;

      /* Find empty rows */
      row_occupied = g_malloc0 (max_bottom_attach);

      /* Lay non-grid-items out in those rows
       */
      current_row = 0;
      for (l = menu_shell->priv->children; l; l = l->next)
        {
          GtkWidget *child = l->data;
          AttachInfo *ai = get_attach_info (child);

          while (current_row < max_bottom_attach && row_occupied[current_row])
            current_row++;

          ai->effective_top_attach = current_row;

          current_row++;
        }

      g_free (row_occupied);

      priv->n_rows = MAX (current_row, max_bottom_attach);
      priv->have_layout = TRUE;
    }
}

static gint
gtk_menu_get_n_rows (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;

  menu_ensure_layout (menu);

  return priv->n_rows;
}

static void
get_effective_child_attach (GtkWidget *child,
                            int       *t)
{
  GtkMenu *menu = GTK_MENU (gtk_widget_get_parent (child));
  AttachInfo *ai;
  
  menu_ensure_layout (menu);

  ai = get_attach_info (child);

  if (t)
    *t = ai->effective_top_attach;
}

static void
gtk_menu_class_init (GtkMenuClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);
  GtkMenuShellClass *menu_shell_class = GTK_MENU_SHELL_CLASS (class);
  GtkBindingSet *binding_set;
  
  gobject_class->set_property = gtk_menu_set_property;
  gobject_class->get_property = gtk_menu_get_property;
  gobject_class->finalize = gtk_menu_finalize;

  widget_class->destroy = gtk_menu_destroy;
  widget_class->realize = gtk_menu_realize;
  widget_class->unrealize = gtk_menu_unrealize;
  widget_class->size_allocate = gtk_menu_size_allocate;
  widget_class->show = gtk_menu_show;
  widget_class->snapshot = gtk_menu_snapshot;
  widget_class->focus = gtk_menu_focus;
  widget_class->can_activate_accel = gtk_menu_real_can_activate_accel;
  widget_class->grab_notify = gtk_menu_grab_notify;
  widget_class->measure = gtk_menu_measure;

  container_class->remove = gtk_menu_remove;
  
  menu_shell_class->submenu_placement = GTK_LEFT_RIGHT;
  menu_shell_class->deactivate = gtk_menu_deactivate;
  menu_shell_class->select_item = gtk_menu_select_item;
  menu_shell_class->insert = gtk_menu_real_insert;
  menu_shell_class->get_popup_delay = gtk_menu_get_popup_delay;
  menu_shell_class->move_current = gtk_menu_move_current;

  /**
   * GtkMenu::move-scroll:
   * @menu: a #GtkMenu
   * @scroll_type: a #GtkScrollType
   */
  menu_signals[MOVE_SCROLL] =
    g_signal_new_class_handler (I_("move-scroll"),
                                G_OBJECT_CLASS_TYPE (gobject_class),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_menu_real_move_scroll),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1,
                                GTK_TYPE_SCROLL_TYPE);

  /**
   * GtkMenu::popped-up:
   * @menu: the #GtkMenu that popped up
   * @flipped_rect: (nullable): the position of @menu after any possible
   *                flipping or %NULL if the backend can't obtain it
   * @final_rect: (nullable): the final position of @menu or %NULL if the
   *              backend can't obtain it
   * @flipped_x: %TRUE if the anchors were flipped horizontally
   * @flipped_y: %TRUE if the anchors were flipped vertically
   *
   * Emitted when the position of @menu is finalized after being popped up
   * using gtk_menu_popup_at_rect (), gtk_menu_popup_at_widget (), or
   * gtk_menu_popup_at_pointer ().
   *
   * @menu might be flipped over the anchor rectangle in order to keep it
   * on-screen, in which case @flipped_x and @flipped_y will be set to %TRUE
   * accordingly.
   *
   * @flipped_rect is the ideal position of @menu after any possible flipping,
   * but before any possible sliding. @final_rect is @flipped_rect, but possibly
   * translated in the case that flipping is still ineffective in keeping @menu
   * on-screen.
   *
   * ![](popup-slide.png)
   *
   * The blue menu is @menu's ideal position, the green menu is @flipped_rect,
   * and the red menu is @final_rect.
   *
   * See gtk_menu_popup_at_rect (), gtk_menu_popup_at_widget (),
   * gtk_menu_popup_at_pointer (), #GtkMenu:anchor-hints,
   * #GtkMenu:rect-anchor-dx, #GtkMenu:rect-anchor-dy, and
   * #GtkMenu:menu-type-hint.
   */
  menu_signals[POPPED_UP] =
    g_signal_new_class_handler (I_("popped-up"),
                                G_OBJECT_CLASS_TYPE (gobject_class),
                                G_SIGNAL_RUN_FIRST,
                                NULL,
                                NULL,
                                NULL,
                                _gtk_marshal_VOID__POINTER_POINTER_BOOLEAN_BOOLEAN,
                                G_TYPE_NONE,
                                4,
                                G_TYPE_POINTER,
                                G_TYPE_POINTER,
                                G_TYPE_BOOLEAN,
                                G_TYPE_BOOLEAN);

  /**
   * GtkMenu:active:
   *
   * The index of the currently selected menu item, or -1 if no
   * menu item is selected.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_int ("active",
                                                     P_("Active"),
                                                     P_("The currently selected menu item"),
                                                     -1, G_MAXINT, -1,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkMenu:accel-group:
   *
   * The accel group holding accelerators for the menu.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ACCEL_GROUP,
                                   g_param_spec_object ("accel-group",
                                                        P_("Accel Group"),
                                                        P_("The accel group holding accelerators for the menu"),
                                                        GTK_TYPE_ACCEL_GROUP,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkMenu:accel-path:
   *
   * An accel path used to conveniently construct accel paths of child items.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ACCEL_PATH,
                                   g_param_spec_string ("accel-path",
                                                        P_("Accel Path"),
                                                        P_("An accel path used to conveniently construct accel paths of child items"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkMenu:attach-widget:
   *
   * The widget the menu is attached to. Setting this property attaches
   * the menu without a #GtkMenuDetachFunc. If you need to use a detacher,
   * use gtk_menu_attach_to_widget() directly.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ATTACH_WIDGET,
                                   g_param_spec_object ("attach-widget",
                                                        P_("Attach Widget"),
                                                        P_("The widget the menu is attached to"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkMenu:monitor:
   *
   * The monitor the menu will be popped up on.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MONITOR,
                                   g_param_spec_int ("monitor",
                                                     P_("Monitor"),
                                                     P_("The monitor the menu will be popped up on"),
                                                     -1, G_MAXINT, -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkMenu:reserve-toggle-size:
   *
   * A boolean that indicates whether the menu reserves space for
   * toggles and icons, regardless of their actual presence.
   *
   * This property should only be changed from its default value
   * for special-purposes such as tabular menus. Regular menus that
   * are connected to a menu bar or context menus should reserve
   * toggle space for consistency.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_RESERVE_TOGGLE_SIZE,
                                   g_param_spec_boolean ("reserve-toggle-size",
                                                         P_("Reserve Toggle Size"),
                                                         P_("A boolean that indicates whether the menu reserves space for toggles and icons"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkMenu:anchor-hints:
   *
   * Positioning hints for aligning the menu relative to a rectangle.
   *
   * These hints determine how the menu should be positioned in the case that
   * the menu would fall off-screen if placed in its ideal position.
   *
   * ![](popup-flip.png)
   *
   * For example, %GDK_ANCHOR_FLIP_Y will replace %GDK_GRAVITY_NORTH_WEST with
   * %GDK_GRAVITY_SOUTH_WEST and vice versa if the menu extends beyond the
   * bottom edge of the monitor.
   *
   * See gtk_menu_popup_at_rect (), gtk_menu_popup_at_widget (),
   * gtk_menu_popup_at_pointer (), #GtkMenu:rect-anchor-dx,
   * #GtkMenu:rect-anchor-dy, #GtkMenu:menu-type-hint, and #GtkMenu::popped-up.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ANCHOR_HINTS,
                                   g_param_spec_flags ("anchor-hints",
                                                       P_("Anchor hints"),
                                                       P_("Positioning hints for when the menu might fall off-screen"),
                                                       GDK_TYPE_ANCHOR_HINTS,
                                                       GDK_ANCHOR_FLIP |
                                                       GDK_ANCHOR_SLIDE |
                                                       GDK_ANCHOR_RESIZE,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_CONSTRUCT |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_NICK |
                                                       G_PARAM_STATIC_BLURB |
                                                       G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkMenu:rect-anchor-dx:
   *
   * Horizontal offset to apply to the menu, i.e. the rectangle or widget
   * anchor.
   *
   * See gtk_menu_popup_at_rect (), gtk_menu_popup_at_widget (),
   * gtk_menu_popup_at_pointer (), #GtkMenu:anchor-hints,
   * #GtkMenu:rect-anchor-dy, #GtkMenu:menu-type-hint, and #GtkMenu::popped-up.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_RECT_ANCHOR_DX,
                                   g_param_spec_int ("rect-anchor-dx",
                                                     P_("Rect anchor dx"),
                                                     P_("Rect anchor horizontal offset"),
                                                     G_MININT,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT |
                                                     G_PARAM_STATIC_NAME |
                                                     G_PARAM_STATIC_NICK |
                                                     G_PARAM_STATIC_BLURB |
                                                     G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkMenu:rect-anchor-dy:
   *
   * Vertical offset to apply to the menu, i.e. the rectangle or widget anchor.
   *
   * See gtk_menu_popup_at_rect (), gtk_menu_popup_at_widget (),
   * gtk_menu_popup_at_pointer (), #GtkMenu:anchor-hints,
   * #GtkMenu:rect-anchor-dx, #GtkMenu:menu-type-hint, and #GtkMenu::popped-up.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_RECT_ANCHOR_DY,
                                   g_param_spec_int ("rect-anchor-dy",
                                                     P_("Rect anchor dy"),
                                                     P_("Rect anchor vertical offset"),
                                                     G_MININT,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT |
                                                     G_PARAM_STATIC_NAME |
                                                     G_PARAM_STATIC_NICK |
                                                     G_PARAM_STATIC_BLURB |
                                                     G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkMenu:menu-type-hint:
   *
   * The #GdkSurfaceTypeHint to use for the menu's #GdkSurface.
   *
   * See gtk_menu_popup_at_rect (), gtk_menu_popup_at_widget (),
   * gtk_menu_popup_at_pointer (), #GtkMenu:anchor-hints,
   * #GtkMenu:rect-anchor-dx, #GtkMenu:rect-anchor-dy, and #GtkMenu::popped-up.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MENU_TYPE_HINT,
                                   g_param_spec_enum ("menu-type-hint",
                                                      P_("Menu type hint"),
                                                      P_("Menu window type hint"),
                                                      GDK_TYPE_SURFACE_TYPE_HINT,
                                                      GDK_SURFACE_TYPE_HINT_POPUP_MENU,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK |
                                                      G_PARAM_STATIC_BLURB |
                                                      G_PARAM_EXPLICIT_NOTIFY));

  binding_set = gtk_binding_set_by_class (class);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Up, 0,
                                I_("move-current"), 1,
                                GTK_TYPE_MENU_DIRECTION_TYPE,
                                GTK_MENU_DIR_PREV);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_KP_Up, 0,
                                "move-current", 1,
                                GTK_TYPE_MENU_DIRECTION_TYPE,
                                GTK_MENU_DIR_PREV);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Down, 0,
                                "move-current", 1,
                                GTK_TYPE_MENU_DIRECTION_TYPE,
                                GTK_MENU_DIR_NEXT);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_KP_Down, 0,
                                "move-current", 1,
                                GTK_TYPE_MENU_DIRECTION_TYPE,
                                GTK_MENU_DIR_NEXT);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Left, 0,
                                "move-current", 1,
                                GTK_TYPE_MENU_DIRECTION_TYPE,
                                GTK_MENU_DIR_PARENT);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_KP_Left, 0,
                                "move-current", 1,
                                GTK_TYPE_MENU_DIRECTION_TYPE,
                                GTK_MENU_DIR_PARENT);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Right, 0,
                                "move-current", 1,
                                GTK_TYPE_MENU_DIRECTION_TYPE,
                                GTK_MENU_DIR_CHILD);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_KP_Right, 0,
                                "move-current", 1,
                                GTK_TYPE_MENU_DIRECTION_TYPE,
                                GTK_MENU_DIR_CHILD);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Home, 0,
                                "move-scroll", 1,
                                GTK_TYPE_SCROLL_TYPE,
                                GTK_SCROLL_START);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_KP_Home, 0,
                                "move-scroll", 1,
                                GTK_TYPE_SCROLL_TYPE,
                                GTK_SCROLL_START);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_End, 0,
                                "move-scroll", 1,
                                GTK_TYPE_SCROLL_TYPE,
                                GTK_SCROLL_END);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_KP_End, 0,
                                "move-scroll", 1,
                                GTK_TYPE_SCROLL_TYPE,
                                GTK_SCROLL_END);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Page_Up, 0,
                                "move-scroll", 1,
                                GTK_TYPE_SCROLL_TYPE,
                                GTK_SCROLL_PAGE_UP);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_KP_Page_Up, 0,
                                "move-scroll", 1,
                                GTK_TYPE_SCROLL_TYPE,
                                GTK_SCROLL_PAGE_UP);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Page_Down, 0,
                                "move-scroll", 1,
                                GTK_TYPE_SCROLL_TYPE,
                                GTK_SCROLL_PAGE_DOWN);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_KP_Page_Down, 0,
                                "move-scroll", 1,
                                GTK_TYPE_SCROLL_TYPE,
                                GTK_SCROLL_PAGE_DOWN);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_MENU_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("menu"));
}


static void
gtk_menu_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GtkMenu *menu = GTK_MENU (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      gtk_menu_set_active (menu, g_value_get_int (value));
      break;
    case PROP_ACCEL_GROUP:
      gtk_menu_set_accel_group (menu, g_value_get_object (value));
      break;
    case PROP_ACCEL_PATH:
      gtk_menu_set_accel_path (menu, g_value_get_string (value));
      break;
    case PROP_ATTACH_WIDGET:
      {
        GtkWidget *widget;

        widget = gtk_menu_get_attach_widget (menu);
        if (widget)
          gtk_menu_detach (menu);

        widget = (GtkWidget*) g_value_get_object (value); 
        if (widget)
          gtk_menu_attach_to_widget (menu, widget, NULL);
      }
      break;
    case PROP_MONITOR:
      gtk_menu_set_monitor (menu, g_value_get_int (value));
      break;
    case PROP_RESERVE_TOGGLE_SIZE:
      gtk_menu_set_reserve_toggle_size (menu, g_value_get_boolean (value));
      break;
    case PROP_ANCHOR_HINTS:
      if (menu->priv->anchor_hints != g_value_get_flags (value))
        {
          menu->priv->anchor_hints = g_value_get_flags (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_RECT_ANCHOR_DX:
      if (menu->priv->rect_anchor_dx != g_value_get_int (value))
        {
          menu->priv->rect_anchor_dx = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_RECT_ANCHOR_DY:
      if (menu->priv->rect_anchor_dy != g_value_get_int (value))
        {
          menu->priv->rect_anchor_dy = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_MENU_TYPE_HINT:
      if (menu->priv->menu_type_hint != g_value_get_enum (value))
        {
          menu->priv->menu_type_hint = g_value_get_enum (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_get_property (GObject     *object,
                       guint        prop_id,
                       GValue      *value,
                       GParamSpec  *pspec)
{
  GtkMenu *menu = GTK_MENU (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_int (value, g_list_index (GTK_MENU_SHELL (menu)->priv->children, gtk_menu_get_active (menu)));
      break;
    case PROP_ACCEL_GROUP:
      g_value_set_object (value, gtk_menu_get_accel_group (menu));
      break;
    case PROP_ACCEL_PATH:
      g_value_set_string (value, gtk_menu_get_accel_path (menu));
      break;
    case PROP_ATTACH_WIDGET:
      g_value_set_object (value, gtk_menu_get_attach_widget (menu));
      break;
    case PROP_MONITOR:
      g_value_set_int (value, gtk_menu_get_monitor (menu));
      break;
    case PROP_RESERVE_TOGGLE_SIZE:
      g_value_set_boolean (value, gtk_menu_get_reserve_toggle_size (menu));
      break;
    case PROP_ANCHOR_HINTS:
      g_value_set_flags (value, menu->priv->anchor_hints);
      break;
    case PROP_RECT_ANCHOR_DX:
      g_value_set_int (value, menu->priv->rect_anchor_dx);
      break;
    case PROP_RECT_ANCHOR_DY:
      g_value_set_int (value, menu->priv->rect_anchor_dy);
      break;
    case PROP_MENU_TYPE_HINT:
      g_value_set_enum (value, menu->priv->menu_type_hint);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_init (GtkMenu *menu)
{
  GtkMenuPrivate *priv;
  GtkGesture *gesture;
  GtkEventController *controller;

  priv = gtk_menu_get_instance_private (menu);

  menu->priv = priv;

  priv->toplevel = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_container_add (GTK_CONTAINER (priv->toplevel), GTK_WIDGET (menu));
  g_signal_connect (priv->toplevel, "destroy", G_CALLBACK (gtk_widget_destroyed), &priv->toplevel);
  gtk_window_set_resizable (GTK_WINDOW (priv->toplevel), FALSE);
  gtk_window_set_mnemonic_modifier (GTK_WINDOW (priv->toplevel), 0);

  _gtk_window_request_csd (GTK_WINDOW (priv->toplevel));
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->toplevel),
                               GTK_STYLE_CLASS_POPUP);

  /* Refloat the menu, so that reference counting for the menu isn't
   * affected by it being a child of the toplevel
   */
  g_object_force_floating (G_OBJECT (menu));
  priv->needs_destruction_ref = TRUE;

  priv->monitor_num = -1;
  priv->drag_start_y = -1;

  priv->anchor_hints = GDK_ANCHOR_FLIP | GDK_ANCHOR_SLIDE | GDK_ANCHOR_RESIZE;
  priv->rect_anchor_dx = 0;
  priv->rect_anchor_dy = 0;
  priv->menu_type_hint = GDK_SURFACE_TYPE_HINT_POPUP_MENU;

  _gtk_widget_set_captured_event_handler (GTK_WIDGET (menu), gtk_menu_captured_event);


  priv->top_arrow_widget = gtk_icon_new ("arrow");
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->top_arrow_widget),
                               GTK_STYLE_CLASS_TOP);
  gtk_widget_set_parent (priv->top_arrow_widget, GTK_WIDGET (menu));
  gtk_widget_set_child_visible (priv->top_arrow_widget, FALSE);

  priv->bottom_arrow_widget = gtk_icon_new ("arrow");
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->bottom_arrow_widget),
                               GTK_STYLE_CLASS_BOTTOM);
  gtk_widget_set_parent (priv->bottom_arrow_widget, GTK_WIDGET (menu));
  gtk_widget_set_child_visible (priv->bottom_arrow_widget, FALSE);

  gesture = gtk_gesture_multi_press_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_BUBBLE);
  g_signal_connect (gesture, "pressed", G_CALLBACK (gtk_menu_pressed_cb), menu);
  g_signal_connect (gesture, "released", G_CALLBACK (gtk_menu_released_cb), menu);
  gtk_widget_add_controller (GTK_WIDGET (menu), GTK_EVENT_CONTROLLER (gesture));

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  g_signal_connect (controller, "scroll",
                    G_CALLBACK (gtk_menu_scroll_controller_scroll), menu);
  gtk_widget_add_controller (GTK_WIDGET (menu), controller);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (gtk_menu_enter), menu);
  g_signal_connect (controller, "motion", G_CALLBACK (gtk_menu_motion), menu);
  g_signal_connect (controller, "leave", G_CALLBACK (gtk_menu_leave), menu);
  gtk_widget_add_controller (GTK_WIDGET (menu), controller);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (gtk_menu_key_pressed), menu);
  gtk_widget_add_controller (GTK_WIDGET (menu), controller);
}

static void
moved_to_rect_cb (GdkSurface          *surface,
                  const GdkRectangle *flipped_rect,
                  const GdkRectangle *final_rect,
                  gboolean            flipped_x,
                  gboolean            flipped_y,
                  GtkMenu            *menu)
{
  g_signal_emit (menu,
                 menu_signals[POPPED_UP],
                 0,
                 flipped_rect,
                 final_rect,
                 flipped_x,
                 flipped_y);
}

static void
gtk_menu_destroy (GtkWidget *widget)
{
  GtkMenu *menu = GTK_MENU (widget);
  GtkMenuPrivate *priv = menu->priv;
  GtkMenuAttachData *data;

  gtk_menu_remove_scroll_timeout (menu);

  data = g_object_get_data (G_OBJECT (widget), attach_data_key);
  if (data)
    gtk_menu_detach (menu);

  gtk_menu_stop_navigating_submenu (menu);

  g_clear_object (&priv->old_active_menu_item);

  /* Add back the reference count for being a child */
  if (priv->needs_destruction_ref)
    {
      priv->needs_destruction_ref = FALSE;
      g_object_ref (widget);
    }

  g_clear_object (&priv->accel_group);

  if (priv->toplevel)
    {
      g_signal_handlers_disconnect_by_func (priv->toplevel, moved_to_rect_cb, menu);
      gtk_widget_destroy (priv->toplevel);
    }

  g_clear_pointer (&priv->heights, g_free);

  GTK_WIDGET_CLASS (gtk_menu_parent_class)->destroy (widget);
}

static void
gtk_menu_finalize (GObject *object)
{
  GtkMenu *menu = GTK_MENU (object);
  GtkMenuPrivate *priv = menu->priv;

  gtk_widget_unparent (priv->top_arrow_widget);
  gtk_widget_unparent (priv->bottom_arrow_widget);

  G_OBJECT_CLASS (gtk_menu_parent_class)->finalize (object);
}

static void
menu_change_display (GtkMenu    *menu,
                     GdkDisplay *new_display)
{
  GtkMenuPrivate *priv = menu->priv;

  if (new_display == gtk_widget_get_display (GTK_WIDGET (menu)))
    return;

  gtk_window_set_display (GTK_WINDOW (priv->toplevel), new_display);
  priv->monitor_num = -1;
}

static void
attach_widget_root_changed (GObject    *attach_widget,
                            GParamSpec *pspec,
                            gpointer    menu)
{
  if (!g_object_get_data (G_OBJECT (menu), "gtk-menu-explicit-display"))
    menu_change_display (menu, gtk_widget_get_display (GTK_WIDGET (attach_widget)));
}

static void
menu_toplevel_attached_to (GtkWindow *toplevel, GParamSpec *pspec, GtkMenu *menu)
{
  GtkMenuAttachData *data;

  data = g_object_get_data (G_OBJECT (menu), attach_data_key);

  g_return_if_fail (data);

  gtk_menu_detach (menu);
}

/**
 * gtk_menu_attach_to_widget:
 * @menu: a #GtkMenu
 * @attach_widget: the #GtkWidget that the menu will be attached to
 * @detacher: (scope async)(allow-none): the user supplied callback function
 *             that will be called when the menu calls gtk_menu_detach()
 *
 * Attaches the menu to the widget and provides a callback function
 * that will be invoked when the menu calls gtk_menu_detach() during
 * its destruction.
 *
 * If the menu is attached to the widget then it will be destroyed
 * when the widget is destroyed, as if it was a child widget.
 * An attached menu will also move between screens correctly if the
 * widgets moves between screens.
 */
void
gtk_menu_attach_to_widget (GtkMenu           *menu,
                           GtkWidget         *attach_widget,
                           GtkMenuDetachFunc  detacher)
{
  GtkMenuAttachData *data;
  GList *list;

  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GTK_IS_WIDGET (attach_widget));

  /* keep this function in sync with gtk_widget_set_parent() */
  data = g_object_get_data (G_OBJECT (menu), attach_data_key);
  if (data)
    {
      g_warning ("gtk_menu_attach_to_widget(): menu already attached to %s",
                 g_type_name (G_TYPE_FROM_INSTANCE (data->attach_widget)));
     return;
    }

  g_object_ref_sink (menu);

  data = g_slice_new (GtkMenuAttachData);
  data->attach_widget = attach_widget;

  g_signal_connect (attach_widget, "notify::root",
                    G_CALLBACK (attach_widget_root_changed), menu);
  attach_widget_root_changed (G_OBJECT (attach_widget), NULL, menu);

  data->detacher = detacher;
  g_object_set_data (G_OBJECT (menu), I_(attach_data_key), data);
  list = g_object_steal_data (G_OBJECT (attach_widget), ATTACHED_MENUS);
  if (!g_list_find (list, menu))
    list = g_list_prepend (list, menu);

  g_object_set_data_full (G_OBJECT (attach_widget), I_(ATTACHED_MENUS), list,
                          (GDestroyNotify) g_list_free);

  /* Attach the widget to the toplevel window. */
  gtk_window_set_attached_to (GTK_WINDOW (menu->priv->toplevel), attach_widget);
  g_signal_connect (GTK_WINDOW (menu->priv->toplevel), "notify::attached-to",
                    G_CALLBACK (menu_toplevel_attached_to), menu);

  _gtk_widget_update_parent_muxer (GTK_WIDGET (menu));

  g_object_notify (G_OBJECT (menu), "attach-widget");
}

/**
 * gtk_menu_get_attach_widget:
 * @menu: a #GtkMenu
 *
 * Returns the #GtkWidget that the menu is attached to.
 *
 * Returns: (transfer none): the #GtkWidget that the menu is attached to
 */
GtkWidget*
gtk_menu_get_attach_widget (GtkMenu *menu)
{
  GtkMenuAttachData *data;

  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

  data = g_object_get_data (G_OBJECT (menu), attach_data_key);
  if (data)
    return data->attach_widget;
  return NULL;
}

/**
 * gtk_menu_detach:
 * @menu: a #GtkMenu
 *
 * Detaches the menu from the widget to which it had been attached.
 * This function will call the callback function, @detacher, provided
 * when the gtk_menu_attach_to_widget() function was called.
 */
void
gtk_menu_detach (GtkMenu *menu)
{
  GtkWindow *toplevel;
  GtkMenuAttachData *data;
  GList *list;

  g_return_if_fail (GTK_IS_MENU (menu));
  toplevel = GTK_WINDOW (menu->priv->toplevel);

  /* keep this function in sync with gtk_widget_unparent() */
  data = g_object_get_data (G_OBJECT (menu), attach_data_key);
  if (!data)
    {
      g_warning ("gtk_menu_detach(): menu is not attached");
      return;
    }
  g_object_set_data (G_OBJECT (menu), I_(attach_data_key), NULL);

  /* Detach the toplevel window. */
  if (toplevel)
    {
      g_signal_handlers_disconnect_by_func (toplevel,
                                            (gpointer) menu_toplevel_attached_to,
                                            menu);
      if (gtk_window_get_attached_to (toplevel) == data->attach_widget)
        gtk_window_set_attached_to (toplevel, NULL);
    }

  g_signal_handlers_disconnect_by_func (data->attach_widget,
                                        (gpointer) attach_widget_root_changed,
                                        menu);

  if (data->detacher)
    data->detacher (data->attach_widget, menu);
  list = g_object_steal_data (G_OBJECT (data->attach_widget), ATTACHED_MENUS);
  list = g_list_remove (list, menu);
  if (list)
    g_object_set_data_full (G_OBJECT (data->attach_widget), I_(ATTACHED_MENUS), list,
                            (GDestroyNotify) g_list_free);
  else
    g_object_set_data (G_OBJECT (data->attach_widget), I_(ATTACHED_MENUS), NULL);

  if (gtk_widget_get_realized (GTK_WIDGET (menu)))
    gtk_widget_unrealize (GTK_WIDGET (menu));

  g_slice_free (GtkMenuAttachData, data);

  _gtk_widget_update_parent_muxer (GTK_WIDGET (menu));

  g_object_notify (G_OBJECT (menu), "attach-widget");
  g_object_unref (menu);
}

static void
gtk_menu_remove (GtkContainer *container,
                 GtkWidget    *widget)
{
  GtkMenu *menu = GTK_MENU (container);
  GtkMenuPrivate *priv = menu->priv;

  /* Clear out old_active_menu_item if it matches the item we are removing */
  if (priv->old_active_menu_item == widget)
    g_clear_object (&priv->old_active_menu_item);

  GTK_CONTAINER_CLASS (gtk_menu_parent_class)->remove (container, widget);

  g_object_set_data (G_OBJECT (widget), I_(ATTACH_INFO_KEY), NULL);

  menu_queue_resize (menu);
}

/**
 * gtk_menu_new:
 *
 * Creates a new #GtkMenu
 *
 * Returns: a new #GtkMenu
 */
GtkWidget*
gtk_menu_new (void)
{
  return g_object_new (GTK_TYPE_MENU, NULL);
}

static void
gtk_menu_real_insert (GtkMenuShell *menu_shell,
                      GtkWidget    *child,
                      gint          position)
{
  GtkMenu *menu = GTK_MENU (menu_shell);
  GtkMenuPrivate *priv = menu->priv;

  gtk_widget_insert_before (child, GTK_WIDGET (menu), priv->bottom_arrow_widget);

  GTK_MENU_SHELL_CLASS (gtk_menu_parent_class)->insert (menu_shell, child, position);

  menu_queue_resize (menu);
}

static gboolean
popup_grab_on_surface (GdkSurface *surface,
		       GdkDevice *pointer)
{
  GdkGrabStatus status;
  GdkSeat *seat;

  g_return_val_if_fail (gdk_surface_get_display (surface) == gdk_device_get_display (pointer), FALSE);

  seat = gdk_device_get_seat (pointer);
  status = gdk_seat_grab (seat, surface,
                          GDK_SEAT_CAPABILITY_ALL, TRUE,
                          NULL, NULL, NULL, NULL);

  return status == GDK_GRAB_SUCCESS;
}

static void
associate_menu_grab_transfer_surface (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;
  GdkSurface *toplevel_surface;
  GdkSurface *transfer_surface;

  toplevel_surface = gtk_widget_get_surface (priv->toplevel);
  transfer_surface = g_object_get_data (G_OBJECT (menu), "gtk-menu-transfer-surface");

  if (toplevel_surface == NULL || transfer_surface == NULL)
    return;

  g_object_set_data (G_OBJECT (toplevel_surface), I_("gdk-attached-grab-surface"), transfer_surface);
}

static void
gtk_menu_popup_internal (GtkMenu             *menu,
                         GdkDevice           *device,
                         GtkWidget           *parent_menu_shell,
                         GtkWidget           *parent_menu_item,
                         guint                button,
                         guint32              activate_time)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkWidget *widget;
  GtkWidget *xgrab_shell;
  GtkWidget *parent;
  GdkEvent *current_event;
  GtkMenuShell *menu_shell;
  gboolean grab_keyboard;
  GtkWidget *parent_toplevel;
  GdkDevice *pointer, *source_device = NULL;
  GdkDisplay *display;

  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (device == NULL || GDK_IS_DEVICE (device));

  display = gtk_widget_get_display (GTK_WIDGET (menu));

  if (device == NULL)
    device = gtk_get_current_event_device ();

  if (device && gdk_device_get_display (device) != display)
    device = NULL;

  if (device == NULL)
    {
      device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
      g_return_if_fail (gdk_device_get_display (device) == display);
    }


  widget = GTK_WIDGET (menu);
  menu_shell = GTK_MENU_SHELL (menu);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    pointer = gdk_device_get_associated_device (device);
  else
    pointer = device;

  g_return_if_fail (gdk_device_get_display (pointer) == display);

  menu_shell->priv->parent_menu_shell = parent_menu_shell;

  /* Find the last viewable ancestor, and make an X grab on it
   */
  parent = GTK_WIDGET (menu);
  xgrab_shell = NULL;
  while (parent)
    {
      gboolean viewable = TRUE;
      GtkWidget *tmp = parent;

      while (tmp)
        {
          if (!gtk_widget_get_mapped (tmp))
            {
              viewable = FALSE;
              break;
            }
          tmp = gtk_widget_get_parent (tmp);
        }

      if (viewable)
        xgrab_shell = parent;

      parent = GTK_MENU_SHELL (parent)->priv->parent_menu_shell;
    }

  /* We want to receive events generated when we map the menu;
   * unfortunately, since there is probably already an implicit
   * grab in place from the button that the user used to pop up
   * the menu, we won't receive then -- in particular, the EnterNotify
   * when the menu pops up under the pointer.
   *
   * If we are grabbing on a parent menu shell, no problem; just grab
   * on that menu shell first before popping up the window with
   * owner_events = TRUE.
   *
   * When grabbing on the menu itself, things get more convoluted --
   * we do an explicit grab on a specially created window with
   * owner_events = TRUE, which we override further down with a
   * grab on the menu. (We can't grab on the menu until it is mapped;
   * we probably could just leave the grab on the other window,
   * with a little reorganization of the code in gtkmenu*).
   */
  grab_keyboard = gtk_menu_shell_get_take_focus (menu_shell);
  gtk_window_set_accept_focus (GTK_WINDOW (priv->toplevel), grab_keyboard);

  if (xgrab_shell && xgrab_shell != widget)
    {
      if (popup_grab_on_surface (gtk_widget_get_surface (xgrab_shell), pointer))
        {
          _gtk_menu_shell_set_grab_device (GTK_MENU_SHELL (xgrab_shell), pointer);
          GTK_MENU_SHELL (xgrab_shell)->priv->have_xgrab = TRUE;
        }
    }
  else
    {
      GdkSurface *transfer_surface;

      xgrab_shell = widget;
      transfer_surface = menu_grab_transfer_surface_get (menu);
      if (popup_grab_on_surface (transfer_surface, pointer))
        {
          _gtk_menu_shell_set_grab_device (GTK_MENU_SHELL (xgrab_shell), pointer);
          GTK_MENU_SHELL (xgrab_shell)->priv->have_xgrab = TRUE;
        }
    }

  if (!GTK_MENU_SHELL (xgrab_shell)->priv->have_xgrab)
    {
      /* We failed to make our pointer/keyboard grab.
       * Rather than leaving the user with a stuck up window,
       * we just abort here. Presumably the user will try again.
       */
      menu_shell->priv->parent_menu_shell = NULL;
      menu_grab_transfer_surface_destroy (menu);
      return;
    }

  _gtk_menu_shell_set_grab_device (GTK_MENU_SHELL (menu), pointer);
  menu_shell->priv->active = TRUE;
  menu_shell->priv->button = button;

  /* If we are popping up the menu from something other than, a button
   * press then, as a heuristic, we ignore enter events for the menu
   * until we get a MOTION_NOTIFY.
   */

  current_event = gtk_get_current_event ();
  if (current_event)
    {
      GdkEventType event_type = gdk_event_get_event_type (current_event);

      if ((event_type != GDK_BUTTON_PRESS) &&
          (event_type != GDK_ENTER_NOTIFY))
        menu_shell->priv->ignore_enter = TRUE;

      source_device = gdk_event_get_source_device (current_event);
      g_object_unref (current_event);
    }
  else
    menu_shell->priv->ignore_enter = TRUE;

  parent_toplevel = NULL;
  if (parent_menu_shell)
    parent_toplevel = gtk_widget_get_toplevel (parent_menu_shell);
  else
    {
      GtkWidget *attach_widget = gtk_menu_get_attach_widget (menu);
      if (attach_widget)
        parent_toplevel = gtk_widget_get_toplevel (attach_widget);
    }

  /* Set transient for to get the right window group and parent */
  if (GTK_IS_WINDOW (parent_toplevel))
    gtk_window_set_transient_for (GTK_WINDOW (priv->toplevel),
                                  GTK_WINDOW (parent_toplevel));

  priv->parent_menu_item = parent_menu_item;
  menu_shell->priv->activate_time = activate_time;

  /* We need to show the menu here rather in the init function
   * because code expects to be able to tell if the menu is onscreen
   * by looking at gtk_widget_get_visible (menu)
   */
  gtk_widget_show (GTK_WIDGET (menu));

  /* Position the menu, possibly changing the size request
   */
  gtk_menu_position (menu, TRUE);

  associate_menu_grab_transfer_surface (menu);

  gtk_menu_scroll_to (menu, priv->scroll_offset);

  /* if no item is selected, select the first one */
  if (!menu_shell->priv->active_menu_item &&
      source_device && gdk_device_get_source (source_device) == GDK_SOURCE_TOUCHSCREEN)
    gtk_menu_shell_select_first (menu_shell, TRUE);

  /* Once everything is set up correctly, map the toplevel */
  gtk_widget_show (priv->toplevel);

  if (xgrab_shell == widget)
    popup_grab_on_surface (gtk_widget_get_surface (widget), pointer); /* Should always succeed */

  gtk_grab_add (GTK_WIDGET (menu));

  if (parent_menu_shell)
    {
      gboolean keyboard_mode;

      keyboard_mode = _gtk_menu_shell_get_keyboard_mode (GTK_MENU_SHELL (parent_menu_shell));
      _gtk_menu_shell_set_keyboard_mode (menu_shell, keyboard_mode);
    }
  else if (menu_shell->priv->button == 0) /* a keynav-activated context menu */
    _gtk_menu_shell_set_keyboard_mode (menu_shell, TRUE);

  _gtk_menu_shell_update_mnemonics (menu_shell);
}

static GdkDevice *
get_device_for_event (const GdkEvent *event)
{
  GdkDevice *device = NULL;
  GdkSeat *seat = NULL;
  GdkDisplay *display = NULL;

  device = gdk_event_get_device (event);

  if (device)
    return device;

  seat = gdk_event_get_seat (event);

  if (!seat)
    {
      display = gdk_event_get_display (event);

      if (!display)
        {
          g_warning ("no display for event, using default");
          display = gdk_display_get_default ();
        }

      if (display)
        seat = gdk_display_get_default_seat (display);
    }

  return seat ? gdk_seat_get_pointer (seat) : NULL;
}

/**
 * gtk_menu_popup_at_rect:
 * @menu: the #GtkMenu to pop up
 * @rect_surface: (not nullable): the #GdkSurface @rect is relative to
 * @rect: (not nullable): the #GdkRectangle to align @menu with
 * @rect_anchor: the point on @rect to align with @menu's anchor point
 * @menu_anchor: the point on @menu to align with @rect's anchor point
 * @trigger_event: (nullable): the #GdkEvent that initiated this request or
 *                 %NULL if it's the current event
 *
 * Displays @menu and makes it available for selection.
 *
 * See gtk_menu_popup_at_widget () and gtk_menu_popup_at_pointer (), which
 * handle more common cases for popping up menus.
 *
 * @menu will be positioned at @rect, aligning their anchor points. @rect is
 * relative to the top-left corner of @rect_surface. @rect_anchor and
 * @menu_anchor determine anchor points on @rect and @menu to pin together.
 * @menu can optionally be offset by #GtkMenu:rect-anchor-dx and
 * #GtkMenu:rect-anchor-dy.
 *
 * Anchors should be specified under the assumption that the text direction is
 * left-to-right; they will be flipped horizontally automatically if the text
 * direction is right-to-left.
 *
 * Other properties that influence the behaviour of this function are
 * #GtkMenu:anchor-hints and #GtkMenu:menu-type-hint. Connect to the
 * #GtkMenu::popped-up signal to find out how it was actually positioned.
 */
void
gtk_menu_popup_at_rect (GtkMenu            *menu,
                        GdkSurface          *rect_surface,
                        const GdkRectangle *rect,
                        GdkGravity          rect_anchor,
                        GdkGravity          menu_anchor,
                        const GdkEvent     *trigger_event)
{
  GtkMenuPrivate *priv;
  GdkEvent *current_event = NULL;
  GdkDevice *device = NULL;
  guint button = 0;
  guint32 activate_time = GDK_CURRENT_TIME;

  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GDK_IS_SURFACE (rect_surface));
  g_return_if_fail (rect);

  priv = menu->priv;
  priv->rect_surface = rect_surface;
  priv->rect = *rect;
  priv->widget = NULL;
  priv->rect_anchor = rect_anchor;
  priv->menu_anchor = menu_anchor;

  if (!trigger_event)
    {
      current_event = gtk_get_current_event ();
      trigger_event = current_event;
    }

  if (trigger_event)
    {
      device = get_device_for_event (trigger_event);
      gdk_event_get_button (trigger_event, &button);
      activate_time = gdk_event_get_time (trigger_event);
    }
  else
    g_warning ("no trigger event for menu popup");

  gtk_menu_popup_internal (menu,
                           device,
                           NULL,
                           NULL,
                           button,
                           activate_time);

  g_clear_object (&current_event);
}

/**
 * gtk_menu_popup_at_widget:
 * @menu: the #GtkMenu to pop up
 * @widget: (not nullable): the #GtkWidget to align @menu with
 * @widget_anchor: the point on @widget to align with @menu's anchor point
 * @menu_anchor: the point on @menu to align with @widget's anchor point
 * @trigger_event: (nullable): the #GdkEvent that initiated this request or
 *                 %NULL if it's the current event
 *
 * Displays @menu and makes it available for selection.
 *
 * See gtk_menu_popup_at_pointer () to pop up a menu at the master pointer.
 * gtk_menu_popup_at_rect () also allows you to position a menu at an arbitrary
 * rectangle.
 *
 * ![](popup-anchors.png)
 *
 * @menu will be positioned at @widget, aligning their anchor points.
 * @widget_anchor and @menu_anchor determine anchor points on @widget and @menu
 * to pin together. @menu can optionally be offset by #GtkMenu:rect-anchor-dx
 * and #GtkMenu:rect-anchor-dy.
 *
 * Anchors should be specified under the assumption that the text direction is
 * left-to-right; they will be flipped horizontally automatically if the text
 * direction is right-to-left.
 *
 * Other properties that influence the behaviour of this function are
 * #GtkMenu:anchor-hints and #GtkMenu:menu-type-hint. Connect to the
 * #GtkMenu::popped-up signal to find out how it was actually positioned.
 */
void
gtk_menu_popup_at_widget (GtkMenu        *menu,
                          GtkWidget      *widget,
                          GdkGravity      widget_anchor,
                          GdkGravity      menu_anchor,
                          const GdkEvent *trigger_event)
{
  GtkMenuPrivate *priv;
  GdkEvent *current_event = NULL;
  GdkDevice *device = NULL;
  guint button = 0;
  guint32 activate_time = GDK_CURRENT_TIME;
  GtkWidget *parent_menu_shell = NULL;
  GtkWidget *parent_menu_item = NULL;

  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = menu->priv;
  priv->rect_surface = NULL;
  priv->widget = widget;
  priv->rect_anchor = widget_anchor;
  priv->menu_anchor = menu_anchor;

  if (!trigger_event)
    {
      current_event = gtk_get_current_event ();
      trigger_event = current_event;
    }

  if (trigger_event)
    {
      device = get_device_for_event (trigger_event);
      gdk_event_get_button (trigger_event, &button);
      activate_time = gdk_event_get_time (trigger_event);
    }
  else
    g_warning ("no trigger event for menu popup");

  if (GTK_IS_MENU_ITEM (priv->widget))
    {
      parent_menu_item = priv->widget;

      if (GTK_IS_MENU_SHELL (gtk_widget_get_parent (parent_menu_item)))
        parent_menu_shell = gtk_widget_get_parent (parent_menu_item);
    }

  gtk_menu_popup_internal (menu,
                           device,
                           parent_menu_shell,
                           parent_menu_item,
                           button,
                           activate_time);

  g_clear_object (&current_event);
}

/**
 * gtk_menu_popup_at_pointer:
 * @menu: the #GtkMenu to pop up
 * @trigger_event: (nullable): the #GdkEvent that initiated this request or
 *                 %NULL if it's the current event
 *
 * Displays @menu and makes it available for selection.
 *
 * See gtk_menu_popup_at_widget () to pop up a menu at a widget.
 * gtk_menu_popup_at_rect () also allows you to position a menu at an arbitrary
 * rectangle.
 *
 * @menu will be positioned at the pointer associated with @trigger_event.
 *
 * Properties that influence the behaviour of this function are
 * #GtkMenu:anchor-hints, #GtkMenu:rect-anchor-dx, #GtkMenu:rect-anchor-dy, and
 * #GtkMenu:menu-type-hint. Connect to the #GtkMenu::popped-up signal to find
 * out how it was actually positioned.
 */
void
gtk_menu_popup_at_pointer (GtkMenu        *menu,
                           const GdkEvent *trigger_event)
{
  GdkEvent *current_event = NULL;
  GdkSurface *rect_surface = NULL;
  GdkDevice *device = NULL;
  GdkRectangle rect = { 0, 0, 1, 1 };

  g_return_if_fail (GTK_IS_MENU (menu));

  if (!trigger_event)
    {
      current_event = gtk_get_current_event ();
      trigger_event = current_event;
    }

  if (trigger_event)
    {
      rect_surface = gdk_event_get_surface (trigger_event);

      if (rect_surface)
        {
          device = get_device_for_event (trigger_event);

          if (device && gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
            device = gdk_device_get_associated_device (device);

          if (device)
            {
              double px, py;
              gdk_surface_get_device_position (rect_surface, device, &px, &py, NULL);
              rect.x = round (px);
              rect.y = round (py);
            }
        }
    }
  else
    g_warning ("no trigger event for menu popup");

  gtk_menu_popup_at_rect (menu,
                          rect_surface,
                          &rect,
                          GDK_GRAVITY_SOUTH_EAST,
                          GDK_GRAVITY_NORTH_WEST,
                          trigger_event);

  g_clear_object (&current_event);
}

static void
get_arrows_border (GtkMenu   *menu,
                   GtkBorder *border)
{
  GtkMenuPrivate *priv = menu->priv;
  gint top_arrow_height, bottom_arrow_height;

  gtk_widget_measure (priv->top_arrow_widget,
                      GTK_ORIENTATION_VERTICAL,
                      -1,
                      &top_arrow_height, NULL,
                      NULL, NULL);
  gtk_widget_measure (priv->bottom_arrow_widget,
                      GTK_ORIENTATION_VERTICAL,
                      -1,
                      &bottom_arrow_height, NULL,
                      NULL, NULL);

  border->top = gtk_widget_get_child_visible (priv->top_arrow_widget) ? top_arrow_height : 0;
  border->bottom = gtk_widget_get_child_visible (priv->bottom_arrow_widget) ? bottom_arrow_height : 0;
  border->left = border->right = 0;
}

/**
 * gtk_menu_update_scroll_offset:
 * @menu: the #GtkMenu that popped up
 * @flipped_rect: (nullable): the position of @menu after any possible flipping
 *                or %NULL if unknown
 * @final_rect: (nullable): the final position of @menu or %NULL if unknown
 * @flipped_x: %TRUE if the anchors were flipped horizontally
 * @flipped_y: %TRUE if the anchors were flipped vertically
 * @user_data: user data
 *
 * Updates the scroll offset of @menu based on the amount of sliding done while
 * positioning @menu. Connect this to the #GtkMenu::popped-up signal to keep the
 * contents of the menu vertically aligned with their ideal position, for combo
 * boxes for example.
 */
void
gtk_menu_update_scroll_offset (GtkMenu            *menu,
                               const GdkRectangle *flipped_rect,
                               const GdkRectangle *final_rect,
                               gboolean            flipped_x,
                               gboolean            flipped_y,
                               gpointer            user_data)
{
  GtkBorder arrows_border;

  g_return_if_fail (GTK_IS_MENU (menu));

  if (!flipped_rect || !final_rect)
    return;

  get_arrows_border (menu, &arrows_border);
  menu->priv->scroll_offset = arrows_border.top + (final_rect->y - flipped_rect->y);
  gtk_menu_scroll_to (menu, menu->priv->scroll_offset);
}

/**
 * gtk_menu_popdown:
 * @menu: a #GtkMenu
 *
 * Removes the menu from the screen.
 */
void
gtk_menu_popdown (GtkMenu *menu)
{
  GtkMenuPrivate *priv;
  GtkMenuShell *menu_shell;

  g_return_if_fail (GTK_IS_MENU (menu));

  menu_shell = GTK_MENU_SHELL (menu);
  priv = menu->priv;

  menu_shell->priv->parent_menu_shell = NULL;
  menu_shell->priv->active = FALSE;
  menu_shell->priv->ignore_enter = FALSE;

  gtk_menu_stop_scrolling (menu);
  gtk_menu_stop_navigating_submenu (menu);

  if (menu_shell->priv->active_menu_item)
    {
      if (priv->old_active_menu_item)
        g_object_unref (priv->old_active_menu_item);
      priv->old_active_menu_item = menu_shell->priv->active_menu_item;
      g_object_ref (priv->old_active_menu_item);
    }

  gtk_menu_shell_deselect (menu_shell);

  /* The X Grab, if present, will automatically be removed
   * when we hide the window
   */
  if (priv->toplevel)
    {
      gtk_widget_hide (priv->toplevel);
      gtk_window_set_transient_for (GTK_WINDOW (priv->toplevel), NULL);
    }

  gtk_widget_hide (GTK_WIDGET (menu));

  menu_shell->priv->have_xgrab = FALSE;

  gtk_grab_remove (GTK_WIDGET (menu));

  _gtk_menu_shell_set_grab_device (menu_shell, NULL);

  menu_grab_transfer_surface_destroy (menu);
}

/**
 * gtk_menu_get_active:
 * @menu: a #GtkMenu
 *
 * Returns the selected menu item from the menu.  This is used by the
 * #GtkComboBox.
 *
 * Returns: (transfer none): the #GtkMenuItem that was last selected
 *          in the menu.  If a selection has not yet been made, the
 *          first menu item is selected.
 */
GtkWidget*
gtk_menu_get_active (GtkMenu *menu)
{
  GtkMenuPrivate *priv;
  GtkWidget *child;
  GList *children;

  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

  priv = menu->priv;

  if (!priv->old_active_menu_item)
    {
      child = NULL;
      children = GTK_MENU_SHELL (menu)->priv->children;

      while (children)
        {
          child = children->data;
          children = children->next;

          if (gtk_bin_get_child (GTK_BIN (child)))
            break;
          child = NULL;
        }

      priv->old_active_menu_item = child;
      if (priv->old_active_menu_item)
        g_object_ref (priv->old_active_menu_item);
    }

  return priv->old_active_menu_item;
}

/**
 * gtk_menu_set_active:
 * @menu: a #GtkMenu
 * @index: the index of the menu item to select.  Index values are
 *         from 0 to n-1
 *
 * Selects the specified menu item within the menu.  This is used by
 * the #GtkComboBox and should not be used by anyone else.
 */
void
gtk_menu_set_active (GtkMenu *menu,
                     guint    index)
{
  GtkMenuPrivate *priv;
  GtkWidget *child;
  GList *tmp_list;

  g_return_if_fail (GTK_IS_MENU (menu));

  priv = menu->priv;

  tmp_list = g_list_nth (GTK_MENU_SHELL (menu)->priv->children, index);
  if (tmp_list)
    {
      child = tmp_list->data;
      if (gtk_bin_get_child (GTK_BIN (child)))
        {
          if (priv->old_active_menu_item)
            g_object_unref (priv->old_active_menu_item);
          priv->old_active_menu_item = child;
          g_object_ref (priv->old_active_menu_item);
        }
    }
  g_object_notify (G_OBJECT (menu), "active");
}

/**
 * gtk_menu_set_accel_group:
 * @menu: a #GtkMenu
 * @accel_group: (allow-none): the #GtkAccelGroup to be associated
 *               with the menu.
 *
 * Set the #GtkAccelGroup which holds global accelerators for the
 * menu.  This accelerator group needs to also be added to all windows
 * that this menu is being used in with gtk_window_add_accel_group(),
 * in order for those windows to support all the accelerators
 * contained in this group.
 */
void
gtk_menu_set_accel_group (GtkMenu       *menu,
                          GtkAccelGroup *accel_group)
{
  GtkMenuPrivate *priv;

  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));

  priv = menu->priv;

  if (priv->accel_group != accel_group)
    {
      if (priv->accel_group)
        g_object_unref (priv->accel_group);
      priv->accel_group = accel_group;
      if (priv->accel_group)
        g_object_ref (priv->accel_group);
      _gtk_menu_refresh_accel_paths (menu, TRUE);
    }
}

/**
 * gtk_menu_get_accel_group:
 * @menu: a #GtkMenu
 *
 * Gets the #GtkAccelGroup which holds global accelerators for the
 * menu. See gtk_menu_set_accel_group().
 *
 * Returns: (transfer none): the #GtkAccelGroup associated with the menu
 */
GtkAccelGroup*
gtk_menu_get_accel_group (GtkMenu *menu)
{
  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

  return menu->priv->accel_group;
}

static gboolean
gtk_menu_real_can_activate_accel (GtkWidget *widget,
                                  guint      signal_id)
{
  /* Menu items chain here to figure whether they can activate their
   * accelerators.  Unlike ordinary widgets, menus allow accel
   * activation even if invisible since that's the usual case for
   * submenus/popup-menus. however, the state of the attach widget
   * affects the "activeness" of the menu.
   */
  GtkWidget *awidget = gtk_menu_get_attach_widget (GTK_MENU (widget));

  if (awidget)
    return gtk_widget_can_activate_accel (awidget, signal_id);
  else
    return gtk_widget_is_sensitive (widget);
}

/**
 * gtk_menu_set_accel_path:
 * @menu:       a valid #GtkMenu
 * @accel_path: (nullable): a valid accelerator path, or %NULL to unset the path
 *
 * Sets an accelerator path for this menu from which accelerator paths
 * for its immediate children, its menu items, can be constructed.
 * The main purpose of this function is to spare the programmer the
 * inconvenience of having to call gtk_menu_item_set_accel_path() on
 * each menu item that should support runtime user changable accelerators.
 * Instead, by just calling gtk_menu_set_accel_path() on their parent,
 * each menu item of this menu, that contains a label describing its
 * purpose, automatically gets an accel path assigned.
 *
 * For example, a menu containing menu items “New” and “Exit”, will, after
 * `gtk_menu_set_accel_path (menu, "<Gnumeric-Sheet>/File");` has been
 * called, assign its items the accel paths: `"<Gnumeric-Sheet>/File/New"`
 * and `"<Gnumeric-Sheet>/File/Exit"`.
 *
 * Assigning accel paths to menu items then enables the user to change
 * their accelerators at runtime. More details about accelerator paths
 * and their default setups can be found at gtk_accel_map_add_entry().
 *
 * Note that @accel_path string will be stored in a #GQuark. Therefore,
 * if you pass a static string, you can save some memory by interning
 * it first with g_intern_static_string().
 */
void
gtk_menu_set_accel_path (GtkMenu     *menu,
                         const gchar *accel_path)
{
  GtkMenuPrivate *priv;

  g_return_if_fail (GTK_IS_MENU (menu));

  priv = menu->priv;

  if (accel_path)
    g_return_if_fail (accel_path[0] == '<' && strchr (accel_path, '/')); /* simplistic check */

  priv->accel_path = g_intern_string (accel_path);
  if (priv->accel_path)
    _gtk_menu_refresh_accel_paths (menu, FALSE);
}

/**
 * gtk_menu_get_accel_path:
 * @menu: a valid #GtkMenu
 *
 * Retrieves the accelerator path set on the menu.
 *
 * Returns: the accelerator path set on the menu.
 */
const gchar*
gtk_menu_get_accel_path (GtkMenu *menu)
{
  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

  return menu->priv->accel_path;
}

typedef struct {
  GtkMenu *menu;
  gboolean group_changed;
} AccelPropagation;

static void
refresh_accel_paths_foreach (GtkWidget *widget,
                             gpointer   data)
{
  GtkMenuPrivate *priv;
  AccelPropagation *prop = data;

  if (GTK_IS_MENU_ITEM (widget))  /* should always be true */
    {
      priv = prop->menu->priv;
      _gtk_menu_item_refresh_accel_path (GTK_MENU_ITEM (widget),
                                         priv->accel_path,
                                         priv->accel_group,
                                         prop->group_changed);
    }
}

static void
_gtk_menu_refresh_accel_paths (GtkMenu  *menu,
                               gboolean  group_changed)
{
  GtkMenuPrivate *priv = menu->priv;

  if (priv->accel_path && priv->accel_group)
    {
      AccelPropagation prop;

      prop.menu = menu;
      prop.group_changed = group_changed;
      gtk_container_foreach (GTK_CONTAINER (menu),
                             refresh_accel_paths_foreach,
                             &prop);
    }
}

/**
 * gtk_menu_reposition:
 * @menu: a #GtkMenu
 *
 * Repositions the menu according to its position function.
 */
void
gtk_menu_reposition (GtkMenu *menu)
{
  g_return_if_fail (GTK_IS_MENU (menu));

  if (gtk_widget_is_drawable (GTK_WIDGET (menu)))
    gtk_menu_position (menu, FALSE);
}

/**
 * gtk_menu_reorder_child:
 * @menu: a #GtkMenu
 * @child: the #GtkMenuItem to move
 * @position: the new position to place @child.
 *     Positions are numbered from 0 to n - 1
 *
 * Moves @child to a new @position in the list of @menu
 * children.
 */
void
gtk_menu_reorder_child (GtkMenu   *menu,
                        GtkWidget *child,
                        gint       position)
{
  GtkMenuShell *menu_shell;

  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GTK_IS_MENU_ITEM (child));

  menu_shell = GTK_MENU_SHELL (menu);

  if (g_list_find (menu_shell->priv->children, child))
    {
      menu_shell->priv->children = g_list_remove (menu_shell->priv->children, child);
      menu_shell->priv->children = g_list_insert (menu_shell->priv->children, child, position);

      menu_queue_resize (menu);
    }
}

static void
gtk_menu_realize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_menu_parent_class)->realize (widget);

  if (GTK_MENU_SHELL (widget)->priv->active_menu_item)
    gtk_menu_scroll_item_visible (GTK_MENU_SHELL (widget),
                                  GTK_MENU_SHELL (widget)->priv->active_menu_item);
}

static gboolean
gtk_menu_focus (GtkWidget       *widget,
                GtkDirectionType direction)
{
  /* A menu or its menu items cannot have focus */
  return FALSE;
}

static GdkSurface *
menu_grab_transfer_surface_get (GtkMenu *menu)
{
  GdkSurface *surface = g_object_get_data (G_OBJECT (menu), "gtk-menu-transfer-surface");
  if (!surface)
    {
      surface = gdk_surface_new_temp (gtk_widget_get_display (GTK_WIDGET (menu)));

      gdk_surface_show (surface);

      g_object_set_data (G_OBJECT (menu), I_("gtk-menu-transfer-surface"), surface);
    }

  return surface;
}

static void
menu_grab_transfer_surface_destroy (GtkMenu *menu)
{
  GdkSurface *surface = g_object_get_data (G_OBJECT (menu), "gtk-menu-transfer-surface");
  if (surface)
    {
      GdkSurface *widget_surface;

      gdk_surface_destroy (surface);
      g_object_set_data (G_OBJECT (menu), I_("gtk-menu-transfer-surface"), NULL);

      widget_surface = gtk_widget_get_surface (GTK_WIDGET (menu));
      g_object_set_data (G_OBJECT (widget_surface), I_("gdk-attached-grab-surface"), surface);
    }
}

static void
gtk_menu_unrealize (GtkWidget *widget)
{
  GtkMenu *menu = GTK_MENU (widget);

  menu_grab_transfer_surface_destroy (menu);

  GTK_WIDGET_CLASS (gtk_menu_parent_class)->unrealize (widget);
}

static gint
calculate_line_heights (GtkMenu *menu,
                        gint     for_width,
                        guint  **ret_min_heights,
                        guint  **ret_nat_heights)
{
  GtkMenuPrivate *priv;
  GtkMenuShell   *menu_shell;
  GtkWidget      *child, *widget;
  GList          *children;
  gint            n_heights;
  guint          *min_heights;
  guint          *nat_heights;
  gint            avail_width;

  priv         = menu->priv;
  widget       = GTK_WIDGET (menu);
  menu_shell   = GTK_MENU_SHELL (widget);

  min_heights  = g_new0 (guint, gtk_menu_get_n_rows (menu));
  nat_heights  = g_new0 (guint, gtk_menu_get_n_rows (menu));
  n_heights    = gtk_menu_get_n_rows (menu);
  avail_width  = for_width - (2 * priv->toggle_size + priv->accel_size);

  for (children = menu_shell->priv->children; children; children = children->next)
    {
      gint part;
      gint toggle_size;
      gint t;
      gint child_min, child_nat;

      child = children->data;

      if (!gtk_widget_get_visible (child))
        continue;

      get_effective_child_attach (child, &t);

      gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL,
                          avail_width,
                          &child_min, &child_nat,
                          NULL, NULL);

      gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (child), &toggle_size);

      part = MAX (child_min, toggle_size);
      min_heights[t] = MAX (min_heights[t], part);

      part = MAX (child_nat, toggle_size);
      nat_heights[t] = MAX (nat_heights[t], part);
    }

  if (ret_min_heights)
    *ret_min_heights = min_heights;
  else
    g_free (min_heights);

  if (ret_nat_heights)
    *ret_nat_heights = nat_heights;
  else
    g_free (nat_heights);

  return n_heights;
}

static void
gtk_menu_size_allocate (GtkWidget *widget,
                        int        widget_width,
                        int        widget_height,
                        int        baseline)
{
  GtkMenu *menu;
  GtkMenuPrivate *priv;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GtkAllocation arrow_allocation, child_allocation;
  GList *children;
  gint x, y, i;
  gint width, height;
  GtkBorder arrow_border;
  int base_width;

  g_return_if_fail (GTK_IS_MENU (widget));

  menu = GTK_MENU (widget);
  menu_shell = GTK_MENU_SHELL (widget);
  priv = menu->priv;

  g_free (priv->heights);
  priv->heights_length = calculate_line_heights (menu,
                                                 widget_width,
                                                 &priv->heights,
                                                 NULL);

  /* refresh our cached height request */
  priv->requested_height = 0;
  for (i = 0; i < priv->heights_length; i++)
    priv->requested_height += priv->heights[i];


  /* Show scroll arrows if necessary */
  if (priv->requested_height > widget_height)
    {
      gtk_widget_set_child_visible (priv->top_arrow_widget, TRUE);
      gtk_widget_set_child_visible (priv->bottom_arrow_widget, TRUE);
    }
  else
    {
      gtk_widget_set_child_visible (priv->top_arrow_widget, FALSE);
      gtk_widget_set_child_visible (priv->bottom_arrow_widget, FALSE);
    }

  x = 0;
  y = 0;
  width = widget_width;
  height = widget_height;

  if (menu_shell->priv->active)
    gtk_menu_scroll_to (menu, priv->scroll_offset);

  get_arrows_border (menu, &arrow_border);

  arrow_allocation.x = x;
  arrow_allocation.y = y;
  arrow_allocation.width = width;
  arrow_allocation.height = arrow_border.top;

  if (gtk_widget_get_child_visible (priv->top_arrow_widget))
    gtk_widget_size_allocate (priv->top_arrow_widget, &arrow_allocation, -1);

  arrow_allocation.y = height - y - arrow_border.bottom;
  arrow_allocation.height = arrow_border.bottom;

  if (gtk_widget_get_child_visible (priv->bottom_arrow_widget))
    gtk_widget_size_allocate (priv->bottom_arrow_widget, &arrow_allocation, -1);


  base_width = width;
  children = menu_shell->priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (gtk_widget_get_visible (child))
        {
          gint t;

          get_effective_child_attach (child, &t);

          child_allocation.width = base_width;
          child_allocation.height = 0;
          child_allocation.x = 0;
          child_allocation.y = - priv->scroll_offset;

          for (i = 0; i < t + 1; i++)
            {
              if (i < t)
                child_allocation.y += priv->heights[i];
              else
                child_allocation.height += priv->heights[i];
            }

          gtk_menu_item_toggle_size_allocate (GTK_MENU_ITEM (child),
                                              priv->toggle_size);

          gtk_widget_size_allocate (child, &child_allocation, -1);
        }
    }
}

static void
gtk_menu_snapshot (GtkWidget   *widget,
                   GtkSnapshot *snapshot)
{
  GtkMenuPrivate *priv = gtk_menu_get_instance_private (GTK_MENU (widget));
  GtkBorder arrows_border;

  get_arrows_border (GTK_MENU (widget), &arrows_border);

  /* TODO: This snapshots the arrow widgets twice. */

  if (gtk_widget_get_child_visible (priv->top_arrow_widget))
    gtk_widget_snapshot_child (widget, priv->top_arrow_widget, snapshot);

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT(
                            0, arrows_border.top,
                            gtk_widget_get_width (widget),
                            gtk_widget_get_height (widget) - arrows_border.top - arrows_border.bottom));

  GTK_WIDGET_CLASS (gtk_menu_parent_class)->snapshot (widget, snapshot);

  gtk_snapshot_pop (snapshot);

  if (gtk_widget_get_child_visible (priv->bottom_arrow_widget))
    gtk_widget_snapshot_child (widget, priv->bottom_arrow_widget, snapshot);
}

static void
gtk_menu_show (GtkWidget *widget)
{
  GtkMenu *menu = GTK_MENU (widget);

  _gtk_menu_refresh_accel_paths (menu, FALSE);

  GTK_WIDGET_CLASS (gtk_menu_parent_class)->show (widget);
}

static void gtk_menu_measure (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  GtkMenu *menu = GTK_MENU (widget);
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (widget);
  GtkMenuPrivate *priv = gtk_menu_get_instance_private (menu);
  GtkWidget *child;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      GList          *children;
      guint           max_toggle_size;
      guint           max_accel_width;
      gint            child_min, child_nat;
      gint            min_width, nat_width;

      min_width = nat_width = 0;

      max_toggle_size = 0;
      max_accel_width = 0;

      children = menu_shell->priv->children;
      while (children)
        {
          gint toggle_size;

          child = children->data;
          children = children->next;

          if (! gtk_widget_get_visible (child))
            continue;

          /* It's important to size_request the child
           * before doing the toggle size request, in
           * case the toggle size request depends on the size
           * request of a child of the child (e.g. for ImageMenuItem)
           */
           gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL,
                               -1, &child_min, &child_nat, NULL, NULL);

           gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (child), &toggle_size);
           max_toggle_size = MAX (max_toggle_size, toggle_size);
           max_accel_width = MAX (max_accel_width,
                                  GTK_MENU_ITEM (child)->priv->accelerator_width);

           min_width = MAX (min_width, child_min);
           nat_width = MAX (nat_width, child_min);
        }

      /* If the menu doesn't include any images or check items
       * reserve the space so that all menus are consistent.
       * We only do this for 'ordinary' menus, not for combobox
       * menus or multi-column menus
       */
      if (max_toggle_size == 0 &&
          !priv->no_toggle_size)
        {
          GtkWidget *menu_item;
          GtkWidget *indicator_widget;
          gint indicator_width;

          /* Create a GtkCheckMenuItem, to query indicator size */
          menu_item = gtk_check_menu_item_new ();
          indicator_widget = _gtk_check_menu_item_get_indicator_widget (GTK_CHECK_MENU_ITEM (menu_item));

          gtk_widget_measure (indicator_widget,
                              GTK_ORIENTATION_HORIZONTAL,
                              -1,
                              &indicator_width, NULL,
                              NULL, NULL);
          max_toggle_size = indicator_width;

          g_object_ref_sink (menu_item);
          g_object_unref (menu_item);
        }

      min_width += 2 * max_toggle_size + max_accel_width;
      nat_width += 2 * max_toggle_size + max_accel_width;

      priv->toggle_size = max_toggle_size;
      priv->accel_size  = max_accel_width;

      *minimum = min_width;
      *natural = nat_width;

    }
  else /* VERTICAL */
    {
      if (for_size < 0)
        {
          gint min_width, nat_width;

          /* Menus are height-for-width only, just return the height
           * for the minimum width
           */
          GTK_WIDGET_GET_CLASS (widget)->measure (widget, GTK_ORIENTATION_HORIZONTAL, -1,
                                                  &min_width, &nat_width, NULL, NULL);
          GTK_WIDGET_GET_CLASS (widget)->measure (widget, GTK_ORIENTATION_VERTICAL, min_width,
                                                  minimum, natural,
                                                  minimum_baseline, natural_baseline);
        }
      else
        {
          GtkBorder       arrow_border;
          guint          *min_heights, *nat_heights;
          gint            n_heights, i;
          gint            min_height, single_height, nat_height;

          min_height = nat_height = 0;
          single_height = 0;

          n_heights =
            calculate_line_heights (menu, for_size, &min_heights, &nat_heights);

          for (i = 0; i < n_heights; i++)
            {
              min_height += min_heights[i];
              single_height = MAX (single_height, min_heights[i]);
              nat_height += nat_heights[i];
            }

          get_arrows_border (menu, &arrow_border);
          single_height += arrow_border.top + arrow_border.bottom;
          min_height = MIN (min_height, single_height);

          *minimum = min_height;
          *natural = nat_height;

          g_free (min_heights);
          g_free (nat_heights);


        }
    }
}

static gboolean
pointer_in_menu_surface (GtkWidget *widget,
			 gdouble    x_root,
			 gdouble    y_root)
{
  GtkMenu *menu = GTK_MENU (widget);
  GtkMenuPrivate *priv = menu->priv;
  GtkAllocation allocation;

  if (gtk_widget_get_mapped (priv->toplevel))
    {
      GtkMenuShell *menu_shell;
      gint          surface_x, surface_y;

      gdk_surface_get_position (gtk_widget_get_surface (priv->toplevel),
				&surface_x, &surface_y);

      gtk_widget_get_allocation (widget, &allocation);
      if (x_root >= surface_x && x_root < surface_x + allocation.width &&
          y_root >= surface_y && y_root < surface_y + allocation.height)
        return TRUE;

      menu_shell = GTK_MENU_SHELL (widget);

      if (GTK_IS_MENU (menu_shell->priv->parent_menu_shell))
        return pointer_in_menu_surface (menu_shell->priv->parent_menu_shell,
                                       x_root, y_root);
    }

  return FALSE;
}


static void
gtk_menu_pressed_cb (GtkGestureMultiPress *gesture,
                     int                   n_press,
                     double                x,
                     double                y,
                     gpointer              user_data)
{
  GtkMenu *menu = user_data;
  GdkEventSequence *sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  const GdkEvent *event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  const GdkEventButton *button_event = (GdkEventButton *)event;
  GdkDevice *source_device;
  GtkWidget *event_widget;

  source_device = gdk_event_get_source_device (event);
  event_widget = gtk_get_event_widget ((GdkEvent *)event);
  /*  Don't pass down to menu shell if a non-menuitem part of the menu
   *  was clicked. The check for the event_widget being a GtkMenuShell
   *  works because we have the pointer grabbed on menu_shell->window
   *  with owner_events=TRUE, so all events that are either outside
   *  the menu or on its border are delivered relative to
   *  menu_shell->window.
   */
  if (GTK_IS_MENU_SHELL (event_widget) &&
      pointer_in_menu_surface (GTK_WIDGET (menu), button_event->x_root, button_event->y_root))
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      return;
    }

  if (GTK_IS_MENU_ITEM (event_widget) &&
      gdk_device_get_source (source_device) == GDK_SOURCE_TOUCHSCREEN &&
      GTK_MENU_ITEM (event_widget)->priv->submenu != NULL &&
      !gtk_widget_is_drawable (GTK_MENU_ITEM (event_widget)->priv->submenu))
    menu->priv->ignore_button_release = TRUE;

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
}

static void
gtk_menu_released_cb (GtkGestureMultiPress *gesture,
                      int                   n_press,
                      double                x,
                      double                y,
                      gpointer              user_data)
{
  GtkMenu *menu = user_data;
  GtkMenuPrivate *priv = menu->priv;
  GdkEventSequence *sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  const GdkEvent *event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  const GdkEventButton *button_event = (GdkEventButton *)event;

  if (priv->ignore_button_release)
    {
      priv->ignore_button_release = FALSE;
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  /*  Don't pass down to menu shell if a non-menuitem part of the menu
   *  was clicked (see comment in gtk_menu_pressed_cb()).
   */
  if (GTK_IS_MENU_SHELL (gtk_get_event_widget ((GdkEvent *) event)) &&
      pointer_in_menu_surface (GTK_WIDGET (menu), button_event->x_root, button_event->y_root))
    {
      /*  Ugly: make sure menu_shell->button gets reset to 0 when we
       *  bail out early here so it is in a consistent state for the
       *  next button press or release in GtkMenuShell.
       *  See bug #449371.
       */
      if (GTK_MENU_SHELL (menu)->priv->active)
        GTK_MENU_SHELL (menu)->priv->button = 0;

      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
}

static gboolean
check_threshold (GtkWidget *widget,
                 gint       start_x,
                 gint       start_y,
                 gint       x,
                 gint       y)
{
#define THRESHOLD 8

  return
    ABS (start_x - x) > THRESHOLD ||
    ABS (start_y - y) > THRESHOLD;
}

static gboolean
definitely_within_item (GtkMenu   *menu,
                        GtkWidget *widget,
                        gint       x,
                        gint       y)
{
  int w, h;
  graphene_rect_t bounds;

  if (!gtk_widget_compute_bounds (widget, GTK_WIDGET (menu), &bounds))
    return FALSE;

  w = bounds.size.width;
  h = bounds.size.height;

  return
    check_threshold (widget, 0, 0, x, y) &&
    check_threshold (widget, w - 1, 0, x, y) &&
    check_threshold (widget, w - 1, h - 1, x, y) &&
    check_threshold (widget, 0, h - 1, x, y);
}

static gboolean
gtk_menu_has_navigation_triangle (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;

  return priv->navigation_height && priv->navigation_width;
}

static void
gtk_menu_motion (GtkEventController *controller,
                 double              x,
                 double              y,
                 gpointer            user_data)
{
  GtkWidget *menu_item;
  GtkMenu *menu;
  GtkMenuShell *menu_shell;
  GtkWidget *parent;
  GdkDevice *source_device;
  GdkEventMotion *event;

  menu_item = GTK_WIDGET (user_data);

  event = (GdkEventMotion *)gtk_get_current_event (); /* FIXME: controller event */

  source_device = gdk_event_get_source_device ((GdkEvent *)event);

  if (GTK_IS_MENU (menu_item) &&
      gdk_device_get_source (source_device) != GDK_SOURCE_TOUCHSCREEN)
    {
      GtkMenuPrivate *priv = GTK_MENU(menu_item)->priv;

      if (priv->ignore_button_release)
        priv->ignore_button_release = FALSE;

      gtk_menu_handle_scrolling (GTK_MENU (menu_item), event->x_root, event->y_root, TRUE);
    }

  /*We received the event for one of two reasons:
   *
   * a) We are the active menu, and did gtk_grab_add()
   * b) The widget is a child of ours, and the event was propagated
   *
   * Since for computation of navigation regions, we want the menu which
   * is the parent of the menu item, for a), we need to find that menu,
   * which may be different from 'widget'.
   */
  parent = gtk_widget_get_parent (menu_item);
  if (!GTK_IS_MENU_ITEM (menu_item) ||
      !GTK_IS_MENU (parent))
    return;
  menu_shell = GTK_MENU_SHELL (parent);
  menu = GTK_MENU (menu_shell);

  if (definitely_within_item (menu, menu_item, event->x, event->y))
    menu_shell->priv->activate_time = 0;

  /* Check to see if we are within an active submenu's navigation region
   */
  if (gtk_menu_navigating_submenu (menu, event->x_root, event->y_root))
    return;

  /* Make sure we pop down if we enter a non-selectable menu item, so we
   * don't show a submenu when the cursor is outside the stay-up triangle.
   */
  if (!_gtk_menu_item_is_selectable (menu_item))
    {
      /* We really want to deselect, but this gives the menushell code
       * a chance to do some bookkeeping about the menuitem.
       */
      gtk_menu_shell_select_item (menu_shell, menu_item);
    }
}

static void
gtk_menu_scroll_by (GtkMenu *menu,
                    gint     step)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkBorder arrow_border;
  GtkWidget *widget;
  gint offset;
  gint view_height;

  widget = GTK_WIDGET (menu);
  offset = priv->scroll_offset + step;

  get_arrows_border (menu, &arrow_border);

  /* Don't scroll over the top if we weren't before: */
  if ((priv->scroll_offset >= 0) && (offset < 0))
    offset = 0;

  view_height = gdk_surface_get_height (gtk_widget_get_surface (widget));

  if (priv->scroll_offset == 0 &&
      view_height >= priv->requested_height)
    return;

  /* Don't scroll past the bottom if we weren't before: */
  if (priv->scroll_offset > 0)
    view_height -= arrow_border.top;

  /* Since arrows are shown, reduce view height even more */
  view_height -= arrow_border.bottom;

  if ((priv->scroll_offset + view_height <= priv->requested_height) &&
      (offset + view_height > priv->requested_height))
    offset = priv->requested_height - view_height;

  if (offset != priv->scroll_offset)
    gtk_menu_scroll_to (menu, offset);
}

static gboolean
gtk_menu_scroll_timeout (gpointer data)
{
  GtkMenu  *menu;

  menu = GTK_MENU (data);
  gtk_menu_scroll_by (menu, menu->priv->scroll_step);

  return TRUE;
}

static gboolean
gtk_menu_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                   gdouble                   dx,
                                   gdouble                   dy,
                                   GtkMenu                  *menu)
{
  gtk_menu_scroll_by (menu, dy * MENU_SCROLL_STEP2);

  return GDK_EVENT_STOP;
}

static void
get_arrows_sensitive_area (GtkMenu      *menu,
                           GdkRectangle *upper,
                           GdkRectangle *lower)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkWidget *widget = GTK_WIDGET (menu);
  GdkSurface *surface;
  gint width, height;
  gint win_x, win_y;
  gint top_arrow_height, bottom_arrow_height;

  gtk_widget_measure (priv->top_arrow_widget,
                      GTK_ORIENTATION_VERTICAL,
                      -1,
                      &top_arrow_height, NULL,
                      NULL, NULL);
  gtk_widget_measure (priv->bottom_arrow_widget,
                      GTK_ORIENTATION_VERTICAL,
                      -1,
                      &bottom_arrow_height, NULL,
                      NULL, NULL);

  surface = gtk_widget_get_surface (widget);
  width = gdk_surface_get_width (surface);
  height = gdk_surface_get_height (surface);

  gdk_surface_get_position (surface, &win_x, &win_y);

  if (upper)
    {
      upper->x = win_x;
      upper->y = win_y;
      upper->width = width;
      upper->height = top_arrow_height;
    }

  if (lower)
    {
      lower->x = win_x;
      lower->y = win_y + height - bottom_arrow_height;
      lower->width = width;
      lower->height = bottom_arrow_height;
    }
}

static void
gtk_menu_handle_scrolling (GtkMenu *menu,
                           gint     x,
                           gint     y,
                           gboolean enter)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkMenuShell *menu_shell;
  GdkRectangle rect;
  gboolean in_arrow;
  gboolean scroll_fast = FALSE;
  gint top_x, top_y;

  menu_shell = GTK_MENU_SHELL (menu);

  gdk_surface_get_position (gtk_widget_get_surface (priv->toplevel),
                           &top_x, &top_y);
  x -= top_x;
  y -= top_y;

  /*  upper arrow handling  */

  get_arrows_sensitive_area (menu, &rect, NULL);

  in_arrow = FALSE;
  if (gtk_widget_get_child_visible (priv->top_arrow_widget) &&
      (x >= rect.x) && (x < rect.x + rect.width) &&
      (y >= rect.y) && (y < rect.y + rect.height))
    {
      in_arrow = TRUE;
    }

  if ((priv->upper_arrow_state & GTK_STATE_FLAG_INSENSITIVE) == 0)
    {
      if (gtk_widget_get_child_visible (priv->top_arrow_widget))
        {
          scroll_fast = (y < rect.y + MENU_SCROLL_FAST_ZONE);

          if (enter && in_arrow &&
              (!priv->upper_arrow_prelight ||
               priv->scroll_fast != scroll_fast))
            {
              priv->upper_arrow_prelight = TRUE;
              priv->scroll_fast = scroll_fast;

              /* Deselect the active item so that
               * any submenus are popped down
               */
              gtk_menu_shell_deselect (menu_shell);

              gtk_menu_remove_scroll_timeout (menu);
              priv->scroll_step = scroll_fast
                                    ? -MENU_SCROLL_STEP2
                                    : -MENU_SCROLL_STEP1;

              priv->scroll_timeout = g_timeout_add (scroll_fast
                                                      ? MENU_SCROLL_TIMEOUT2
                                                      : MENU_SCROLL_TIMEOUT1,
                                                    gtk_menu_scroll_timeout,
                                                    menu);
              g_source_set_name_by_id (priv->scroll_timeout, "[gtk] gtk_menu_scroll_timeout");
            }
          else if (!enter && !in_arrow && priv->upper_arrow_prelight)
            {
              gtk_menu_stop_scrolling (menu);
            }
        }

      /*  check if the button isn't insensitive before
       *  changing it to something else.
       */
      if ((priv->upper_arrow_state & GTK_STATE_FLAG_INSENSITIVE) == 0)
        {
          GtkStateFlags arrow_state = 0;

          if (priv->upper_arrow_prelight)
            arrow_state |= GTK_STATE_FLAG_PRELIGHT;

          if (arrow_state != priv->upper_arrow_state)
            {
              priv->upper_arrow_state = arrow_state;
              gtk_widget_set_state_flags (priv->top_arrow_widget, arrow_state, TRUE);
            }
        }
    }

  /*  lower arrow handling  */

  get_arrows_sensitive_area (menu, NULL, &rect);

  in_arrow = FALSE;
  if (gtk_widget_get_child_visible (priv->bottom_arrow_widget) &&
      (x >= rect.x) && (x < rect.x + rect.width) &&
      (y >= rect.y) && (y < rect.y + rect.height))
    {
      in_arrow = TRUE;
    }

  if ((priv->lower_arrow_state & GTK_STATE_FLAG_INSENSITIVE) == 0)
    {
      if (gtk_widget_get_child_visible (priv->bottom_arrow_widget))
        {
          scroll_fast = (y > rect.y + rect.height - MENU_SCROLL_FAST_ZONE);

          if (enter && in_arrow &&
              (!priv->lower_arrow_prelight ||
               priv->scroll_fast != scroll_fast))
            {
              priv->lower_arrow_prelight = TRUE;
              priv->scroll_fast = scroll_fast;

              /* Deselect the active item so that
               * any submenus are popped down
               */
              gtk_menu_shell_deselect (menu_shell);

              gtk_menu_remove_scroll_timeout (menu);
              priv->scroll_step = scroll_fast
                                    ? MENU_SCROLL_STEP2
                                    : MENU_SCROLL_STEP1;

              priv->scroll_timeout = g_timeout_add (scroll_fast
                                                      ? MENU_SCROLL_TIMEOUT2
                                                      : MENU_SCROLL_TIMEOUT1,
                                                    gtk_menu_scroll_timeout,
                                                    menu);
              g_source_set_name_by_id (priv->scroll_timeout, "[gtk] gtk_menu_scroll_timeout");
            }
          else if (!enter && !in_arrow && priv->lower_arrow_prelight)
            {
              gtk_menu_stop_scrolling (menu);
            }
        }

      /*  check if the button isn't insensitive before
       *  changing it to something else.
       */
      if ((priv->lower_arrow_state & GTK_STATE_FLAG_INSENSITIVE) == 0)
        {
          GtkStateFlags arrow_state = 0;

          if (priv->lower_arrow_prelight)
            arrow_state |= GTK_STATE_FLAG_PRELIGHT;

          if (arrow_state != priv->lower_arrow_state)
            {
              priv->lower_arrow_state = arrow_state;
              gtk_widget_set_state_flags (priv->bottom_arrow_widget, arrow_state, TRUE);
            }
        }
    }
}

static void
gtk_menu_enter (GtkEventController *controller,
                double              x,
                double              y,
                GdkCrossingMode     mode,
                GdkNotifyType       detail,
                gpointer            user_data)
{
  GdkDevice *source_device;
  GdkEventCrossing *event;

  event = (GdkEventCrossing *)gtk_get_current_event ();

  if (event->mode == GDK_CROSSING_GTK_GRAB ||
      event->mode == GDK_CROSSING_GTK_UNGRAB ||
      event->mode == GDK_CROSSING_STATE_CHANGED)
    return;

  source_device = gdk_event_get_source_device ((GdkEvent *) event);

  if (gdk_device_get_source (source_device) != GDK_SOURCE_TOUCHSCREEN)
    {
      GtkMenuShell *menu_shell = GTK_MENU_SHELL (user_data);

      if (!menu_shell->priv->ignore_enter)
        gtk_menu_handle_scrolling (GTK_MENU (user_data),
                                   event->x_root, event->y_root, TRUE);
    }
}

static void
gtk_menu_leave (GtkEventController *controller,
                GdkCrossingMode     mode,
                GdkNotifyType       detail,
                gpointer            user_data)
{
  GtkMenu *menu;
  GdkDevice *source_device;
  GdkEventCrossing *event;

  event = (GdkEventCrossing *)gtk_get_current_event ();

  if (event->mode == GDK_CROSSING_GTK_GRAB ||
      event->mode == GDK_CROSSING_GTK_UNGRAB ||
      event->mode == GDK_CROSSING_STATE_CHANGED)
    return;

  menu = GTK_MENU (user_data);

  if (gtk_menu_navigating_submenu (menu, event->x_root, event->y_root))
    return;

  source_device = gdk_event_get_source_device ((GdkEvent *) event);

  if (gdk_device_get_source (source_device) != GDK_SOURCE_TOUCHSCREEN)
    gtk_menu_handle_scrolling (menu, event->x_root, event->y_root, FALSE);
}

static gboolean
gtk_menu_key_pressed (GtkEventControllerKey *controller,
                      guint                  keyval,
                      guint                  keycode,
                      GdkModifierType        state,
                      GtkMenu               *menu)
{
  gtk_menu_stop_navigating_submenu (menu);
  return FALSE;
}

static gboolean
pointer_on_menu_widget (GtkMenu *menu,
                        gdouble  x_root,
                        gdouble  y_root)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkAllocation allocation;
  gint surface_x, surface_y;

  gtk_widget_get_allocation (GTK_WIDGET (menu), &allocation);
  gdk_surface_get_position (gtk_widget_get_surface (priv->toplevel),
			    &surface_x, &surface_y);

  if (x_root >= surface_x && x_root < surface_x + allocation.width &&
      y_root >= surface_y && y_root < surface_y + allocation.height)
    return TRUE;

  return FALSE;
}

static gboolean
gtk_menu_captured_event (GtkWidget *widget,
                         GdkEvent  *event)
{
  GdkDevice *source_device;
  gboolean retval = FALSE;
  GtkMenuPrivate *priv;
  GtkMenu *menu;
  gdouble x_root, y_root;
  guint button;
  GdkModifierType state;

  menu = GTK_MENU (widget);
  priv = menu->priv;

  if (!gtk_widget_get_child_visible (priv->top_arrow_widget) &&
      !gtk_widget_get_child_visible (priv->bottom_arrow_widget) &&
      priv->drag_start_y < 0)
    return retval;

  source_device = gdk_event_get_source_device (event);
  gdk_event_get_root_coords (event, &x_root, &y_root);

  switch ((guint) gdk_event_get_event_type (event))
    {
    case GDK_TOUCH_BEGIN:
    case GDK_BUTTON_PRESS:
      if ((!gdk_event_get_button (event, &button) || button == 1) &&
          gdk_device_get_source (source_device) == GDK_SOURCE_TOUCHSCREEN &&
          pointer_on_menu_widget (menu, x_root, y_root))
        {
          priv->drag_start_y = event->button.y_root;
          priv->initial_drag_offset = priv->scroll_offset;
          priv->drag_scroll_started = FALSE;
        }
      else
        priv->drag_start_y = -1;

      priv->drag_already_pressed = TRUE;
      break;
    case GDK_TOUCH_END:
    case GDK_BUTTON_RELEASE:
      if (priv->drag_scroll_started)
        {
          priv->drag_scroll_started = FALSE;
          priv->drag_start_y = -1;
          priv->drag_already_pressed = FALSE;
          retval = TRUE;
        }
      break;
    case GDK_TOUCH_UPDATE:
    case GDK_MOTION_NOTIFY:
      if ((!gdk_event_get_state (event, &state) || (state & GDK_BUTTON1_MASK) != 0) &&
          gdk_device_get_source (source_device) == GDK_SOURCE_TOUCHSCREEN)
        {
          if (!priv->drag_already_pressed)
            {
              if (pointer_on_menu_widget (menu, x_root, y_root))
                {
                  priv->drag_start_y = y_root;
                  priv->initial_drag_offset = priv->scroll_offset;
                  priv->drag_scroll_started = FALSE;
                }
              else
                priv->drag_start_y = -1;

              priv->drag_already_pressed = TRUE;
            }

          if (priv->drag_start_y < 0 && !priv->drag_scroll_started)
            break;

          if (priv->drag_scroll_started)
            {
              gint offset, view_height;
              GtkBorder arrow_border;
              gdouble y_diff;

              y_diff = y_root - priv->drag_start_y;
              offset = priv->initial_drag_offset - y_diff;

              view_height = gdk_surface_get_height (gtk_widget_get_surface (widget));
              get_arrows_border (menu, &arrow_border);

              if (gtk_widget_get_child_visible (priv->top_arrow_widget))
                view_height -= arrow_border.top;

              if (gtk_widget_get_child_visible (priv->bottom_arrow_widget))
                view_height -= arrow_border.bottom;

              offset = CLAMP (offset,
                              MIN (priv->scroll_offset, 0),
                              MAX (priv->scroll_offset, priv->requested_height - view_height));

              gtk_menu_scroll_to (menu, offset);

              retval = TRUE;
            }
          else if (gtk_drag_check_threshold (widget,
                                             0, priv->drag_start_y,
                                             0, y_root))
            {
              priv->drag_scroll_started = TRUE;
              gtk_menu_shell_deselect (GTK_MENU_SHELL (menu));
              retval = TRUE;
            }
        }
      break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      if (priv->drag_scroll_started)
        retval = TRUE;
      break;
    default:
      break;
    }

  return retval;
}

static void
gtk_menu_stop_navigating_submenu (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;

  priv->navigation_x = 0;
  priv->navigation_y = 0;
  priv->navigation_width = 0;
  priv->navigation_height = 0;

  if (priv->navigation_timeout)
    {
      g_source_remove (priv->navigation_timeout);
      priv->navigation_timeout = 0;
    }
}

static gboolean
gtk_menu_navigating_submenu (GtkMenu *menu,
                             gint     event_x,
                             gint     event_y)
{
  GtkMenuPrivate *priv = menu->priv;
  gint width, height;

  if (!gtk_menu_has_navigation_triangle (menu))
    return FALSE;

  width = priv->navigation_width;
  height = priv->navigation_height;

  /* Check if x/y are in the triangle spanned by the navigation parameters */

  /* 1) Move the coordinates so the triangle starts at 0,0 */
  event_x -= priv->navigation_x;
  event_y -= priv->navigation_y;

  /* 2) Ensure both legs move along the positive axis */
  if (width < 0)
    {
      event_x = -event_x;
      width = -width;
    }
  if (height < 0)
    {
      event_y = -event_y;
      height = -height;
    }

  /* 3) Check that the given coordinate is inside the triangle. The formula
   * is a transformed form of this formula: x/w + y/h <= 1
   */
  if (event_x >= 0 && event_y >= 0 &&
      event_x * height + event_y * width <= width * height)
    {
      return TRUE;
    }
  else
    {
      gtk_menu_stop_navigating_submenu (menu);
      return FALSE;
    }
}

static void
gtk_menu_deactivate (GtkMenuShell *menu_shell)
{
  GtkWidget *parent;

  g_return_if_fail (GTK_IS_MENU (menu_shell));

  parent = menu_shell->priv->parent_menu_shell;

  menu_shell->priv->activate_time = 0;
  gtk_menu_popdown (GTK_MENU (menu_shell));

  if (parent)
    gtk_menu_shell_deactivate (GTK_MENU_SHELL (parent));
}

static GdkGravity
get_horizontally_flipped_anchor (GdkGravity anchor)
{
  switch (anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      return GDK_GRAVITY_NORTH_EAST;
    case GDK_GRAVITY_NORTH:
      return GDK_GRAVITY_NORTH;
    case GDK_GRAVITY_NORTH_EAST:
      return GDK_GRAVITY_NORTH_WEST;
    case GDK_GRAVITY_WEST:
      return GDK_GRAVITY_EAST;
    case GDK_GRAVITY_CENTER:
      return GDK_GRAVITY_CENTER;
    case GDK_GRAVITY_EAST:
      return GDK_GRAVITY_WEST;
    case GDK_GRAVITY_SOUTH_WEST:
      return GDK_GRAVITY_SOUTH_EAST;
    case GDK_GRAVITY_SOUTH:
      return GDK_GRAVITY_SOUTH;
    case GDK_GRAVITY_SOUTH_EAST:
      return GDK_GRAVITY_SOUTH_WEST;
    default:
      g_warning ("unknown GdkGravity: %d", anchor);
      return anchor;
    }
}

static void
gtk_menu_position (GtkMenu  *menu,
                   gboolean  set_scroll_offset)
{
  GtkMenuPrivate *priv = menu->priv;
  GdkSurface *rect_surface = NULL;
  GdkRectangle rect;
  GtkTextDirection text_direction = GTK_TEXT_DIR_NONE;
  GdkGravity rect_anchor;
  GdkGravity menu_anchor;
  GdkAnchorHints anchor_hints;
  gint rect_anchor_dx, rect_anchor_dy;
  GdkSurface *toplevel;
  gboolean emulated_move_to_rect = FALSE;

  rect_anchor = priv->rect_anchor;
  menu_anchor = priv->menu_anchor;
  anchor_hints = priv->anchor_hints;
  rect_anchor_dx = priv->rect_anchor_dx;
  rect_anchor_dy = priv->rect_anchor_dy;

  if (priv->rect_surface)
    {
      rect_surface = priv->rect_surface;
      rect = priv->rect;
    }
  else if (priv->widget)
    {
      rect_surface = gtk_widget_get_surface (priv->widget);
      gtk_widget_get_surface_allocation (priv->widget, &rect);
      text_direction = gtk_widget_get_direction (priv->widget);
    }
  else
    {
      g_assert_not_reached ();
    }

  /* Realize so we have the proper width and height to figure out
   * the right place to popup the menu.
   */
  gtk_widget_realize (priv->toplevel);

  if (!gtk_widget_get_visible (priv->toplevel))
    gtk_window_set_type_hint (GTK_WINDOW (priv->toplevel), priv->menu_type_hint);

  if (text_direction == GTK_TEXT_DIR_NONE)
    text_direction = gtk_widget_get_direction (GTK_WIDGET (menu));

  if (text_direction == GTK_TEXT_DIR_RTL)
    {
      rect_anchor = get_horizontally_flipped_anchor (rect_anchor);
      menu_anchor = get_horizontally_flipped_anchor (menu_anchor);
    }

  toplevel = gtk_widget_get_surface (priv->toplevel);

  gdk_surface_set_transient_for (toplevel, rect_surface);

  g_signal_handlers_disconnect_by_func (toplevel, moved_to_rect_cb, menu);

  if (!emulated_move_to_rect)
    g_signal_connect (toplevel, "moved-to-rect", G_CALLBACK (moved_to_rect_cb),
                      menu);


  gdk_surface_move_to_rect (toplevel,
                           &rect,
                           rect_anchor,
                           menu_anchor,
                           anchor_hints,
                           rect_anchor_dx,
                           rect_anchor_dy);
}

static void
gtk_menu_remove_scroll_timeout (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;

  if (priv->scroll_timeout)
    {
      g_source_remove (priv->scroll_timeout);
      priv->scroll_timeout = 0;
    }
}

static void
gtk_menu_stop_scrolling (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkCssNode *top_arrow_node, *bottom_arrow_node;
  GtkStateFlags state;

  gtk_menu_remove_scroll_timeout (menu);
  priv->upper_arrow_prelight = FALSE;
  priv->lower_arrow_prelight = FALSE;

  top_arrow_node = gtk_widget_get_css_node (priv->top_arrow_widget);
  state = gtk_css_node_get_state (top_arrow_node);
  gtk_css_node_set_state (top_arrow_node, state & ~GTK_STATE_FLAG_PRELIGHT);

  bottom_arrow_node = gtk_widget_get_css_node (priv->bottom_arrow_widget);
  state = gtk_css_node_get_state (bottom_arrow_node);
  gtk_css_node_set_state (bottom_arrow_node, state & ~GTK_STATE_FLAG_PRELIGHT);
}

static void
gtk_menu_scroll_to (GtkMenu *menu,
                    gint    offset)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkCssNode *top_arrow_node, *bottom_arrow_node;

  top_arrow_node = gtk_widget_get_css_node (priv->top_arrow_widget);
  gtk_css_node_set_visible (top_arrow_node,
                            gtk_widget_get_child_visible (priv->top_arrow_widget));
  gtk_css_node_set_state (top_arrow_node, priv->upper_arrow_state);

  bottom_arrow_node = gtk_widget_get_css_node (priv->bottom_arrow_widget);
  gtk_css_node_set_visible (top_arrow_node,
                            gtk_widget_get_child_visible (priv->bottom_arrow_widget));
  gtk_css_node_set_state (bottom_arrow_node, priv->lower_arrow_state);

  priv->scroll_offset = offset;
  gtk_widget_queue_allocate (GTK_WIDGET (menu));
}

static gboolean
compute_child_offset (GtkMenu   *menu,
                      GtkWidget *menu_item,
                      gint      *offset,
                      gint      *height)
{
  GtkMenuPrivate *priv = menu->priv;
  gint item_top_attach;
  gint child_offset = 0;
  gint i;

  get_effective_child_attach (menu_item, &item_top_attach);

  /* there is a possibility that we get called before _size_request,
   * so check the height table for safety.
   */
  if (!priv->heights || priv->heights_length < gtk_menu_get_n_rows (menu))
    return FALSE;

  /* when we have a row with only invisible children, its height will
   * be zero, so there's no need to check WIDGET_VISIBLE here
   */
  for (i = 0; i < item_top_attach; i++)
    child_offset += priv->heights[i];

  if (offset)
    *offset = child_offset;
  if (height)
    *height = priv->heights[item_top_attach];

  return TRUE;
}

static void
gtk_menu_scroll_item_visible (GtkMenuShell *menu_shell,
                              GtkWidget    *menu_item)
{
  GtkMenu *menu = GTK_MENU (menu_shell);
  GtkMenuPrivate *priv = menu->priv;
  GtkWidget *widget = GTK_WIDGET (menu_shell);
  gint child_offset, child_height;
  gint height;
  gint y;
  gint arrow_height;

  /* We need to check if the selected item fully visible.
   * If not we need to scroll the menu so that it becomes fully
   * visible.
   */
  if (compute_child_offset (menu, menu_item, &child_offset, &child_height))
    {
      y = priv->scroll_offset;
      height = gdk_surface_get_height (gtk_widget_get_surface (widget));

      if (child_offset < y)
        {
          /* Ignore the enter event we might get if the pointer
           * is on the menu
           */
          menu_shell->priv->ignore_enter = TRUE;
          gtk_menu_scroll_to (menu, child_offset);
        }
      else
        {
          GtkBorder arrow_border;

          arrow_height = 0;

          get_arrows_border (menu, &arrow_border);
          arrow_height = arrow_border.top + arrow_border.bottom;

          if (child_offset + child_height > y + height - arrow_height)
            {
              arrow_height = arrow_border.bottom + arrow_border.top;
              y = child_offset + child_height - height + arrow_height;

              /* Ignore the enter event we might get if the pointer
               * is on the menu
               */
              menu_shell->priv->ignore_enter = TRUE;
              gtk_menu_scroll_to (menu, y);
            }
        }
    }
}

static void
gtk_menu_select_item (GtkMenuShell *menu_shell,
                      GtkWidget    *menu_item)
{
  GtkMenu *menu = GTK_MENU (menu_shell);

  if (gtk_widget_get_realized (GTK_WIDGET (menu)))
    gtk_menu_scroll_item_visible (menu_shell, menu_item);

  GTK_MENU_SHELL_CLASS (gtk_menu_parent_class)->select_item (menu_shell, menu_item);
}

static gint
gtk_menu_get_popup_delay (GtkMenuShell *menu_shell)
{
  return MENU_POPUP_DELAY;
}

static void
gtk_menu_move_current (GtkMenuShell         *menu_shell,
                       GtkMenuDirectionType  direction)
{
  if (gtk_widget_get_direction (GTK_WIDGET (menu_shell)) == GTK_TEXT_DIR_RTL)
    {
      switch (direction)
        {
        case GTK_MENU_DIR_CHILD:
          direction = GTK_MENU_DIR_PARENT;
          break;
        case GTK_MENU_DIR_PARENT:
          direction = GTK_MENU_DIR_CHILD;
          break;
        case GTK_MENU_DIR_NEXT:
        case GTK_MENU_DIR_PREV:
        default:
          break;
        }
    }

  GTK_MENU_SHELL_CLASS (gtk_menu_parent_class)->move_current (menu_shell, direction);
}

static gint
get_visible_size (GtkMenu *menu)
{
  GtkBorder arrow_border;
  gint menu_height;

  menu_height = gtk_widget_get_height (GTK_WIDGET (menu));

  get_arrows_border (menu, &arrow_border);
  menu_height -= arrow_border.top;
  menu_height -= arrow_border.bottom;

  return menu_height;
}

/* Find the sensitive on-screen child containing @y, or if none,
 * the nearest selectable onscreen child. (%NULL if none)
 */
static GtkWidget *
child_at (GtkMenu *menu,
          gint     y)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (menu);
  GtkWidget *child = NULL;
  gint child_offset = 0;
  GList *children;
  gint menu_height;
  gint lower, upper; /* Onscreen bounds */

  menu_height = get_visible_size (menu);
  lower = priv->scroll_offset;
  upper = priv->scroll_offset + menu_height;

  for (children = menu_shell->priv->children; children; children = children->next)
    {
      if (gtk_widget_get_visible (children->data))
        {
          GtkRequisition child_requisition;

          gtk_widget_get_preferred_size (children->data,
                                         &child_requisition, NULL);

          if (_gtk_menu_item_is_selectable (children->data) &&
              child_offset >= lower &&
              child_offset + child_requisition.height <= upper)
            {
              child = children->data;

              if (child_offset + child_requisition.height > y)
                return child;
            }

          child_offset += child_requisition.height;
        }
    }

  return child;
}

static gint
get_menu_height (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkBorder arrow_border;
  gint height;

  height = priv->requested_height;

  get_arrows_border (menu, &arrow_border);
  height -= arrow_border.top;
  height -= arrow_border.bottom;

  return height;
}

static void
gtk_menu_real_move_scroll (GtkMenu       *menu,
                           GtkScrollType  type)
{
  GtkMenuPrivate *priv = menu->priv;
  gint page_size = get_visible_size (menu);
  gint end_position = get_menu_height (menu);
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (menu);

  switch ((guint) type)
    {
    case GTK_SCROLL_PAGE_UP:
    case GTK_SCROLL_PAGE_DOWN:
      {
        gint old_offset;
        gint new_offset;
        gint child_offset = 0;
        gboolean old_upper_arrow_visible;
        gint step;

        if (type == GTK_SCROLL_PAGE_UP)
          step = - page_size;
        else
          step = page_size;

        if (menu_shell->priv->active_menu_item)
          {
            gint child_height;

            if (compute_child_offset (menu, menu_shell->priv->active_menu_item,
                                      &child_offset, &child_height))
              child_offset += child_height / 2;
          }

        menu_shell->priv->ignore_enter = TRUE;
        old_upper_arrow_visible = gtk_widget_get_child_visible (priv->top_arrow_widget);
        old_offset = priv->scroll_offset;

        new_offset = priv->scroll_offset + step;
        new_offset = CLAMP (new_offset, 0, end_position - page_size);

        gtk_menu_scroll_to (menu, new_offset);

        if (menu_shell->priv->active_menu_item)
          {
            GtkWidget *new_child;
            gboolean new_upper_arrow_visible = gtk_widget_get_child_visible (priv->top_arrow_widget);
            GtkBorder arrow_border;

            get_arrows_border (menu, &arrow_border);

            if (priv->scroll_offset != old_offset)
              step = priv->scroll_offset - old_offset;

            step -= (new_upper_arrow_visible - old_upper_arrow_visible) * arrow_border.top;

            new_child = child_at (menu, child_offset + step);
            if (new_child)
              gtk_menu_shell_select_item (menu_shell, new_child);
          }
      }
      break;
    case GTK_SCROLL_START:
      /* Ignore the enter event we might get if the pointer is on the menu */
      menu_shell->priv->ignore_enter = TRUE;
      gtk_menu_shell_select_first (menu_shell, TRUE);
      break;
    case GTK_SCROLL_END:
      /* Ignore the enter event we might get if the pointer is on the menu */
      menu_shell->priv->ignore_enter = TRUE;
      _gtk_menu_shell_select_last (menu_shell, TRUE);
      break;
    default:
      break;
    }
}

/**
 * gtk_menu_set_monitor:
 * @menu: a #GtkMenu
 * @monitor_num: the number of the monitor on which the menu should
 *    be popped up
 *
 * Informs GTK+ on which monitor a menu should be popped up.
 * See gdk_monitor_get_geometry().
 *
 * This function should be called from a #GtkMenuPositionFunc
 * if the menu should not appear on the same monitor as the pointer.
 * This information can’t be reliably inferred from the coordinates
 * returned by a #GtkMenuPositionFunc, since, for very long menus,
 * these coordinates may extend beyond the monitor boundaries or even
 * the screen boundaries.
 */
void
gtk_menu_set_monitor (GtkMenu *menu,
                      gint     monitor_num)
{
  GtkMenuPrivate *priv;

  g_return_if_fail (GTK_IS_MENU (menu));

  priv = menu->priv;

  if (priv->monitor_num != monitor_num)
    {
      priv->monitor_num = monitor_num;
      g_object_notify (G_OBJECT (menu), "monitor");
    }
}

/**
 * gtk_menu_get_monitor:
 * @menu: a #GtkMenu
 *
 * Retrieves the number of the monitor on which to show the menu.
 *
 * Returns: the number of the monitor on which the menu should
 *    be popped up or -1, if no monitor has been set
 */
gint
gtk_menu_get_monitor (GtkMenu *menu)
{
  g_return_val_if_fail (GTK_IS_MENU (menu), -1);

  return menu->priv->monitor_num;
}

/**
 * gtk_menu_place_on_monitor:
 * @menu: a #GtkMenu
 * @monitor: the monitor to place the menu on
 *
 * Places @menu on the given monitor.
 */
void
gtk_menu_place_on_monitor (GtkMenu    *menu,
                           GdkMonitor *monitor)
{
  GdkDisplay *display;
  gint i, monitor_num;

  g_return_if_fail (GTK_IS_MENU (menu));

  display = gtk_widget_get_display (GTK_WIDGET (menu));
  monitor_num = 0;
  for (i = 0; ; i++)
    {
      GdkMonitor *m = gdk_display_get_monitor (display, i);
      if (m == monitor)
        {
          monitor_num = i;
          break;
        }
      if (m == NULL)
        break;
    }

  gtk_menu_set_monitor (menu, monitor_num);
}

/**
 * gtk_menu_get_for_attach_widget:
 * @widget: a #GtkWidget
 *
 * Returns a list of the menus which are attached to this widget.
 * This list is owned by GTK+ and must not be modified.
 *
 * Returns: (element-type GtkWidget) (transfer none): the list
 *     of menus attached to his widget.
 */
GList*
gtk_menu_get_for_attach_widget (GtkWidget *widget)
{
  GList *list;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  list = g_object_get_data (G_OBJECT (widget), ATTACHED_MENUS);

  return list;
}

static void
gtk_menu_grab_notify (GtkWidget *widget,
                      gboolean   was_grabbed)
{
  GtkMenu *menu;
  GtkWidget *toplevel;
  GtkWindowGroup *group;
  GtkWidget *grab;
  GdkDevice *pointer;

  GTK_WIDGET_CLASS (gtk_menu_parent_class)->grab_notify (widget, was_grabbed);

  menu = GTK_MENU (widget);
  pointer = _gtk_menu_shell_get_grab_device (GTK_MENU_SHELL (widget));

  if (!pointer ||
      !gtk_widget_device_is_shadowed (widget, pointer))
    return;

  toplevel = gtk_widget_get_toplevel (widget);

  if (!GTK_IS_WINDOW (toplevel))
    return;

  group = gtk_window_get_group (GTK_WINDOW (toplevel));
  grab = gtk_window_group_get_current_grab (group);

  if (GTK_MENU_SHELL (widget)->priv->active && !GTK_IS_MENU_SHELL (grab))
    gtk_menu_shell_cancel (GTK_MENU_SHELL (widget));

  menu->priv->drag_scroll_started = FALSE;
}

/**
 * gtk_menu_set_reserve_toggle_size:
 * @menu: a #GtkMenu
 * @reserve_toggle_size: whether to reserve size for toggles
 *
 * Sets whether the menu should reserve space for drawing toggles
 * or icons, regardless of their actual presence.
 */
void
gtk_menu_set_reserve_toggle_size (GtkMenu  *menu,
                                  gboolean  reserve_toggle_size)
{
  GtkMenuPrivate *priv;
  gboolean no_toggle_size;

  g_return_if_fail (GTK_IS_MENU (menu));

  priv = menu->priv;

  no_toggle_size = !reserve_toggle_size;
  if (priv->no_toggle_size != no_toggle_size)
    {
      priv->no_toggle_size = no_toggle_size;

      g_object_notify (G_OBJECT (menu), "reserve-toggle-size");
    }
}

/**
 * gtk_menu_get_reserve_toggle_size:
 * @menu: a #GtkMenu
 *
 * Returns whether the menu reserves space for toggles and
 * icons, regardless of their actual presence.
 *
 * Returns: Whether the menu reserves toggle space
 */
gboolean
gtk_menu_get_reserve_toggle_size (GtkMenu *menu)
{
  g_return_val_if_fail (GTK_IS_MENU (menu), FALSE);

  return !menu->priv->no_toggle_size;
}

/**
 * gtk_menu_new_from_model:
 * @model: a #GMenuModel
 *
 * Creates a #GtkMenu and populates it with menu items and
 * submenus according to @model.
 *
 * The created menu items are connected to actions found in the
 * #GtkApplicationWindow to which the menu belongs - typically
 * by means of being attached to a widget (see gtk_menu_attach_to_widget())
 * that is contained within the #GtkApplicationWindows widget hierarchy.
 *
 * Actions can also be added using gtk_widget_insert_action_group() on the menu's
 * attach widget or on any of its parent widgets.
 *
 * Returns: a new #GtkMenu
 */
GtkWidget *
gtk_menu_new_from_model (GMenuModel *model)
{
  GtkWidget *menu;

  g_return_val_if_fail (G_IS_MENU_MODEL (model), NULL);

  menu = gtk_menu_new ();
  gtk_menu_shell_bind_model (GTK_MENU_SHELL (menu), model, NULL, TRUE);

  return menu;
}
