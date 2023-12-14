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
 * Applications can display a #GtkMenu as a popup menu by calling the 
 * gtk_menu_popup() function.  The example below shows how an application
 * can pop up a menu when the 3rd mouse button is pressed.  
 *
 * ## Connecting the popup signal handler.
 *
 * |[<!-- language="C" -->
 *   // connect our handler which will popup the menu
 *   g_signal_connect_swapped (window, "button_press_event",
 *	G_CALLBACK (my_popup_handler), menu);
 * ]|
 *
 * ## Signal handler which displays a popup menu.
 *
 * |[<!-- language="C" -->
 * static gint
 * my_popup_handler (GtkWidget *widget, GdkEvent *event)
 * {
 *   GtkMenu *menu;
 *   GdkEventButton *event_button;
 *
 *   g_return_val_if_fail (widget != NULL, FALSE);
 *   g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);
 *   g_return_val_if_fail (event != NULL, FALSE);
 *
 *   // The "widget" is the menu that was supplied when 
 *   // g_signal_connect_swapped() was called.
 *   menu = GTK_MENU (widget);
 *
 *   if (event->type == GDK_BUTTON_PRESS)
 *     {
 *       event_button = (GdkEventButton *) event;
 *       if (event_button->button == GDK_BUTTON_SECONDARY)
 *         {
 *           gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 
 *                           event_button->button, event_button->time);
 *           return TRUE;
 *         }
 *     }
 *
 *   return FALSE;
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

#include <math.h>
#include <string.h>

#include  <gobject/gvaluecollector.h>

#include "gtkaccellabel.h"
#include "gtkaccelmap.h"
#include "gtkadjustment.h"
#include "gtkbindings.h"
#include "gtkbuiltiniconprivate.h"
#include "gtkcheckmenuitem.h"
#include "gtkcheckmenuitemprivate.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenuprivate.h"
#include "gtkmenuitemprivate.h"
#include "gtkmenushellprivate.h"
#include "gtkwindow.h"
#include "gtkbox.h"
#include "gtkscrollbar.h"
#include "gtksettings.h"
#include "gtkprivate.h"
#include "gtkwidgetpath.h"
#include "gtkwidgetprivate.h"
#include "gtkdnd.h"
#include "gtkintl.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtktooltipprivate.h"

#include "deprecated/gtktearoffmenuitem.h"


#include "a11y/gtkmenuaccessible.h"
#include "gdk/gdk-private.h"

#define NAVIGATION_REGION_OVERSHOOT 50  /* How much the navigation region
                                         * extends below the submenu
                                         */

#define MENU_SCROLL_STEP1      8
#define MENU_SCROLL_STEP2     15
#define MENU_SCROLL_FAST_ZONE  8
#define MENU_SCROLL_TIMEOUT1  50
#define MENU_SCROLL_TIMEOUT2  20

#define MENU_POPUP_DELAY     225
#define MENU_POPDOWN_DELAY  1000

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
  gint left_attach;
  gint right_attach;
  gint top_attach;
  gint bottom_attach;
  gint effective_left_attach;
  gint effective_right_attach;
  gint effective_top_attach;
  gint effective_bottom_attach;
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

enum {
  CHILD_PROP_0,
  CHILD_PROP_LEFT_ATTACH,
  CHILD_PROP_RIGHT_ATTACH,
  CHILD_PROP_TOP_ATTACH,
  CHILD_PROP_BOTTOM_ATTACH
};

typedef enum _GtkMenuScrollFlag
{
  GTK_MENU_SCROLL_FLAG_NONE = 0,
  GTK_MENU_SCROLL_FLAG_ADAPT = 1 << 0,
} GtkMenuScrollFlag;

static void     gtk_menu_set_property      (GObject          *object,
                                            guint             prop_id,
                                            const GValue     *value,
                                            GParamSpec       *pspec);
static void     gtk_menu_get_property      (GObject          *object,
                                            guint             prop_id,
                                            GValue           *value,
                                            GParamSpec       *pspec);
static void     gtk_menu_finalize          (GObject          *object);
static void     gtk_menu_set_child_property(GtkContainer     *container,
                                            GtkWidget        *child,
                                            guint             property_id,
                                            const GValue     *value,
                                            GParamSpec       *pspec);
static void     gtk_menu_get_child_property(GtkContainer     *container,
                                            GtkWidget        *child,
                                            guint             property_id,
                                            GValue           *value,
                                            GParamSpec       *pspec);
static void     gtk_menu_destroy           (GtkWidget        *widget);
static void     gtk_menu_realize           (GtkWidget        *widget);
static void     gtk_menu_unrealize         (GtkWidget        *widget);
static void     gtk_menu_size_allocate     (GtkWidget        *widget,
                                            GtkAllocation    *allocation);
static void     gtk_menu_show              (GtkWidget        *widget);
static gboolean gtk_menu_draw              (GtkWidget        *widget,
                                            cairo_t          *cr);
static gboolean gtk_menu_key_press         (GtkWidget        *widget,
                                            GdkEventKey      *event);
static gboolean gtk_menu_scroll            (GtkWidget        *widget,
                                            GdkEventScroll   *event);
static gboolean gtk_menu_button_press      (GtkWidget        *widget,
                                            GdkEventButton   *event);
static gboolean gtk_menu_button_release    (GtkWidget        *widget,
                                            GdkEventButton   *event);
static gboolean gtk_menu_motion_notify     (GtkWidget        *widget,
                                            GdkEventMotion   *event);
static gboolean gtk_menu_enter_notify      (GtkWidget        *widget,
                                            GdkEventCrossing *event);
static gboolean gtk_menu_leave_notify      (GtkWidget        *widget,
                                            GdkEventCrossing *event);
static void     gtk_menu_scroll_to         (GtkMenu          *menu,
                                            gint              offset,
                                            GtkMenuScrollFlag flags);
static void     gtk_menu_grab_notify       (GtkWidget        *widget,
                                            gboolean          was_grabbed);
static gboolean gtk_menu_captured_event    (GtkWidget        *widget,
                                            GdkEvent         *event);


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
static void     gtk_menu_scrollbar_changed (GtkAdjustment    *adjustment,
                                            GtkMenu          *menu);
static void     gtk_menu_handle_scrolling  (GtkMenu          *menu,
                                            gint              event_x,
                                            gint              event_y,
                                            gboolean          enter,
                                            gboolean          motion);
static void     gtk_menu_set_tearoff_hints (GtkMenu          *menu,
                                            gint             width);
static gboolean gtk_menu_focus             (GtkWidget        *widget,
                                            GtkDirectionType direction);
static gint     gtk_menu_get_popup_delay   (GtkMenuShell     *menu_shell);
static void     gtk_menu_move_current      (GtkMenuShell     *menu_shell,
                                            GtkMenuDirectionType direction);
static void     gtk_menu_real_move_scroll  (GtkMenu          *menu,
                                            GtkScrollType     type);

static void     gtk_menu_stop_navigating_submenu       (GtkMenu          *menu);
static gboolean gtk_menu_stop_navigating_submenu_cb    (gpointer          user_data);
static gboolean gtk_menu_navigating_submenu            (GtkMenu          *menu,
                                                        gint              event_x,
                                                        gint              event_y);
static void     gtk_menu_set_submenu_navigation_region (GtkMenu          *menu,
                                                        GtkMenuItem      *menu_item,
                                                        GdkEventCrossing *event);
 
static void gtk_menu_deactivate     (GtkMenuShell      *menu_shell);
static void gtk_menu_show_all       (GtkWidget         *widget);
static void gtk_menu_position       (GtkMenu           *menu,
                                     gboolean           set_scroll_offset);
static void gtk_menu_reparent       (GtkMenu           *menu,
                                     GtkWidget         *new_parent,
                                     gboolean           unrealize);
static void gtk_menu_remove         (GtkContainer      *menu,
                                     GtkWidget         *widget);

static void gtk_menu_update_title   (GtkMenu           *menu);

static void       menu_grab_transfer_window_destroy (GtkMenu *menu);
static GdkWindow *menu_grab_transfer_window_get     (GtkMenu *menu);

static gboolean gtk_menu_real_can_activate_accel (GtkWidget *widget,
                                                  guint      signal_id);
static void _gtk_menu_refresh_accel_paths (GtkMenu *menu,
                                           gboolean group_changed);

static void gtk_menu_get_preferred_width            (GtkWidget           *widget,
                                                     gint                *minimum_size,
                                                     gint                *natural_size);
static void gtk_menu_get_preferred_height           (GtkWidget           *widget,
                                                     gint                *minimum_size,
                                                     gint                *natural_size);
static void gtk_menu_get_preferred_height_for_width (GtkWidget           *widget,
                                                     gint                 for_size,
                                                     gint                *minimum_size,
                                                     gint                *natural_size);


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

static gboolean
is_grid_attached (AttachInfo *ai)
{
  return (ai->left_attach >= 0 &&
          ai->right_attach >= 0 &&
          ai->top_attach >= 0 &&
          ai->bottom_attach >= 0);
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
      gint max_right_attach;
      gint max_bottom_attach;

      /* Find extents of gridded portion
       */
      max_right_attach = 1;
      max_bottom_attach = 0;

      for (l = menu_shell->priv->children; l; l = l->next)
        {
          GtkWidget *child = l->data;
          AttachInfo *ai = get_attach_info (child);

          if (is_grid_attached (ai))
            {
              max_bottom_attach = MAX (max_bottom_attach, ai->bottom_attach);
              max_right_attach = MAX (max_right_attach, ai->right_attach);
            }
        }

      /* Find empty rows */
      row_occupied = g_malloc0 (max_bottom_attach);

      for (l = menu_shell->priv->children; l; l = l->next)
        {
          GtkWidget *child = l->data;
          AttachInfo *ai = get_attach_info (child);

          if (is_grid_attached (ai))
            {
              gint i;

              for (i = ai->top_attach; i < ai->bottom_attach; i++)
                row_occupied[i] = TRUE;
            }
        }

      /* Lay non-grid-items out in those rows
       */
      current_row = 0;
      for (l = menu_shell->priv->children; l; l = l->next)
        {
          GtkWidget *child = l->data;
          AttachInfo *ai = get_attach_info (child);

          if (!is_grid_attached (ai))
            {
              while (current_row < max_bottom_attach && row_occupied[current_row])
                current_row++;

              ai->effective_left_attach = 0;
              ai->effective_right_attach = max_right_attach;
              ai->effective_top_attach = current_row;
              ai->effective_bottom_attach = current_row + 1;

              current_row++;
            }
          else
            {
              ai->effective_left_attach = ai->left_attach;
              ai->effective_right_attach = ai->right_attach;
              ai->effective_top_attach = ai->top_attach;
              ai->effective_bottom_attach = ai->bottom_attach;
            }
        }

      g_free (row_occupied);

      priv->n_rows = MAX (current_row, max_bottom_attach);
      priv->n_columns = max_right_attach;
      priv->have_layout = TRUE;
    }
}


static gint
gtk_menu_get_n_columns (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;

  menu_ensure_layout (menu);

  return priv->n_columns;
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
                            int       *l,
                            int       *r,
                            int       *t,
                            int       *b)
{
  GtkMenu *menu = GTK_MENU (gtk_widget_get_parent (child));
  AttachInfo *ai;
  
  menu_ensure_layout (menu);

  ai = get_attach_info (child);

  if (l)
    *l = ai->effective_left_attach;
  if (r)
    *r = ai->effective_right_attach;
  if (t)
    *t = ai->effective_top_attach;
  if (b)
    *b = ai->effective_bottom_attach;

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
  widget_class->draw = gtk_menu_draw;
  widget_class->scroll_event = gtk_menu_scroll;
  widget_class->key_press_event = gtk_menu_key_press;
  widget_class->button_press_event = gtk_menu_button_press;
  widget_class->button_release_event = gtk_menu_button_release;
  widget_class->motion_notify_event = gtk_menu_motion_notify;
  widget_class->show_all = gtk_menu_show_all;
  widget_class->enter_notify_event = gtk_menu_enter_notify;
  widget_class->leave_notify_event = gtk_menu_leave_notify;
  widget_class->focus = gtk_menu_focus;
  widget_class->can_activate_accel = gtk_menu_real_can_activate_accel;
  widget_class->grab_notify = gtk_menu_grab_notify;
  widget_class->get_preferred_width = gtk_menu_get_preferred_width;
  widget_class->get_preferred_height = gtk_menu_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_menu_get_preferred_height_for_width;

  container_class->remove = gtk_menu_remove;
  container_class->get_child_property = gtk_menu_get_child_property;
  container_class->set_child_property = gtk_menu_set_child_property;
  
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
   *
   * Since: 3.22
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
   *
   * Since: 2.14
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
   *
   * Since: 2.14
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
   *
   * Since: 2.14
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
   *
   * Since: 2.14
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ATTACH_WIDGET,
                                   g_param_spec_object ("attach-widget",
                                                        P_("Attach Widget"),
                                                        P_("The widget the menu is attached to"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkMenu:tearoff-title:
   *
   * A title that may be displayed by the window manager when this
   * menu is torn-off.
   *
   * Deprecated: 3.10
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TEAROFF_TITLE,
                                   g_param_spec_string ("tearoff-title",
                                                        P_("Tearoff Title"),
                                                        P_("A title that may be displayed by the window manager when this menu is torn-off"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE | G_PARAM_DEPRECATED));

  /**
   * GtkMenu:tearoff-state:
   *
   * A boolean that indicates whether the menu is torn-off.
   *
   * Since: 2.6
   *
   * Deprecated: 3.10
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TEAROFF_STATE,
                                   g_param_spec_boolean ("tearoff-state",
                                                         P_("Tearoff State"),
                                                         P_("A boolean that indicates whether the menu is torn-off"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE | G_PARAM_DEPRECATED));

  /**
   * GtkMenu:monitor:
   *
   * The monitor the menu will be popped up on.
   *
   * Since: 2.14
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
   *
   * Since: 2.18
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
   *
   * Since: 3.22
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
   *
   * Since: 3.22
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
   *
   * Since: 3.22
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
   * The #GdkWindowTypeHint to use for the menu's #GdkWindow.
   *
   * See gtk_menu_popup_at_rect (), gtk_menu_popup_at_widget (),
   * gtk_menu_popup_at_pointer (), #GtkMenu:anchor-hints,
   * #GtkMenu:rect-anchor-dx, #GtkMenu:rect-anchor-dy, and #GtkMenu::popped-up.
   *
   * Since: 3.22
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MENU_TYPE_HINT,
                                   g_param_spec_enum ("menu-type-hint",
                                                      P_("Menu type hint"),
                                                      P_("Menu window type hint"),
                                                      GDK_TYPE_WINDOW_TYPE_HINT,
                                                      GDK_WINDOW_TYPE_HINT_POPUP_MENU,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK |
                                                      G_PARAM_STATIC_BLURB |
                                                      G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkMenu:horizontal-padding:
   *
   * Extra space at the left and right edges of the menu.
   *
   * Deprecated: 3.8: use the standard padding CSS property (through objects
   *   like #GtkStyleContext and #GtkCssProvider); the value of this style
   *   property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("horizontal-padding",
                                                             P_("Horizontal Padding"),
                                                             P_("Extra space at the left and right edges of the menu"),
                                                             0,
                                                             G_MAXINT,
                                                             0,
                                                             GTK_PARAM_READABLE |
                                                             G_PARAM_DEPRECATED));

  /**
   * GtkMenu:vertical-padding:
   *
   * Extra space at the top and bottom of the menu.
   *
   * Deprecated: 3.8: use the standard padding CSS property (through objects
   *   like #GtkStyleContext and #GtkCssProvider); the value of this style
   *   property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("vertical-padding",
                                                             P_("Vertical Padding"),
                                                             P_("Extra space at the top and bottom of the menu"),
                                                             0,
                                                             G_MAXINT,
                                                             1,
                                                             GTK_PARAM_READABLE |
                                                             G_PARAM_DEPRECATED));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("vertical-offset",
                                                             P_("Vertical Offset"),
                                                             P_("When the menu is a submenu, position it this number of pixels offset vertically"),
                                                             G_MININT,
                                                             G_MAXINT,
                                                             0,
                                                             GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("horizontal-offset",
                                                             P_("Horizontal Offset"),
                                                             P_("When the menu is a submenu, position it this number of pixels offset horizontally"),
                                                             G_MININT,
                                                             G_MAXINT,
                                                             -2,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkMenu:double-arrows:
   *
   * When %TRUE, both arrows are shown when scrolling.
   *
   * Deprecated: 3.20: the value of this style property is ignored.
   **/
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("double-arrows",
                                                                 P_("Double Arrows"),
                                                                 P_("When scrolling, always show both arrows."),
                                                                 TRUE,
                                                                 GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  /**
   * GtkMenu:arrow-placement:
   *
   * Indicates where scroll arrows should be placed.
   *
   * Since: 2.16
   *
   * Deprecated: 3.20: the value of this style property is ignored.
   **/
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("arrow-placement",
                                                              P_("Arrow Placement"),
                                                              P_("Indicates where scroll arrows should be placed"),
                                                              GTK_TYPE_ARROW_PLACEMENT,
                                                              GTK_ARROWS_BOTH,
                                                              GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

 gtk_container_class_install_child_property (container_class,
                                             CHILD_PROP_LEFT_ATTACH,
                                              g_param_spec_int ("left-attach",
                                                               P_("Left Attach"),
                                                               P_("The column number to attach the left side of the child to"),
                                                                -1, INT_MAX, -1,
                                                               GTK_PARAM_READWRITE));

 gtk_container_class_install_child_property (container_class,
                                             CHILD_PROP_RIGHT_ATTACH,
                                              g_param_spec_int ("right-attach",
                                                               P_("Right Attach"),
                                                               P_("The column number to attach the right side of the child to"),
                                                                -1, INT_MAX, -1,
                                                               GTK_PARAM_READWRITE));

 gtk_container_class_install_child_property (container_class,
                                             CHILD_PROP_TOP_ATTACH,
                                              g_param_spec_int ("top-attach",
                                                               P_("Top Attach"),
                                                               P_("The row number to attach the top of the child to"),
                                                                -1, INT_MAX, -1,
                                                               GTK_PARAM_READWRITE));

 gtk_container_class_install_child_property (container_class,
                                             CHILD_PROP_BOTTOM_ATTACH,
                                              g_param_spec_int ("bottom-attach",
                                                               P_("Bottom Attach"),
                                                               P_("The row number to attach the bottom of the child to"),
                                                                -1, INT_MAX, -1,
                                                               GTK_PARAM_READWRITE));

 /**
  * GtkMenu:arrow-scaling:
  *
  * Arbitrary constant to scale down the size of the scroll arrow.
  *
  * Since: 2.16
  *
  * Deprecated: 3.20: use the standard min-width/min-height CSS properties on
  *   the arrow node; the value of this style property is ignored.
  */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("arrow-scaling",
                                                               P_("Arrow Scaling"),
                                                               P_("Arbitrary constant to scale down the size of the scroll arrow"),
                                                               0.0, 1.0, 0.7,
                                                               GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

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
  gtk_widget_class_set_css_name (widget_class, "menu");
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
    case PROP_TEAROFF_STATE:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_menu_set_tearoff_state (menu, g_value_get_boolean (value));
G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case PROP_TEAROFF_TITLE:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_menu_set_title (menu, g_value_get_string (value));
G_GNUC_END_IGNORE_DEPRECATIONS;
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
    case PROP_TEAROFF_STATE:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      g_value_set_boolean (value, gtk_menu_get_tearoff_state (menu));
G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case PROP_TEAROFF_TITLE:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      g_value_set_string (value, gtk_menu_get_title (menu));
G_GNUC_END_IGNORE_DEPRECATIONS;
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
gtk_menu_set_child_property (GtkContainer *container,
                             GtkWidget    *child,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkMenu *menu = GTK_MENU (container);
  AttachInfo *ai = get_attach_info (child);

  switch (property_id)
    {
    case CHILD_PROP_LEFT_ATTACH:
      ai->left_attach = g_value_get_int (value);
      break;
    case CHILD_PROP_RIGHT_ATTACH:
      ai->right_attach = g_value_get_int (value);
      break;
    case CHILD_PROP_TOP_ATTACH:
      ai->top_attach = g_value_get_int (value);
      break;
    case CHILD_PROP_BOTTOM_ATTACH:
      ai->bottom_attach = g_value_get_int (value);
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      return;
    }

  menu_queue_resize (menu);
}

static void
gtk_menu_get_child_property (GtkContainer *container,
                             GtkWidget    *child,
                             guint         property_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  AttachInfo *ai = get_attach_info (child);

  switch (property_id)
    {
    case CHILD_PROP_LEFT_ATTACH:
      g_value_set_int (value, ai->left_attach);
      break;
    case CHILD_PROP_RIGHT_ATTACH:
      g_value_set_int (value, ai->right_attach);
      break;
    case CHILD_PROP_TOP_ATTACH:
      g_value_set_int (value, ai->top_attach);
      break;
    case CHILD_PROP_BOTTOM_ATTACH:
      g_value_set_int (value, ai->bottom_attach);
      break;
      
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      return;
    }
}

static gboolean
gtk_menu_window_event (GtkWidget *window,
                       GdkEvent  *event,
                       GtkWidget *menu)
{
  gboolean handled = FALSE;

  g_object_ref (window);
  g_object_ref (menu);

  switch (event->type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      handled = gtk_widget_event (menu, event);
      break;
    case GDK_WINDOW_STATE:
      /* Window for the menu has been closed by the display server or by GDK.
       * Update the internal state as if the user had clicked outside the
       * menu
       */
      if (event->window_state.new_window_state & GDK_WINDOW_STATE_WITHDRAWN &&
          event->window_state.changed_mask & GDK_WINDOW_STATE_WITHDRAWN)
        gtk_menu_shell_deactivate (GTK_MENU_SHELL(menu));
      break;
    default:
      break;
    }

  g_object_unref (window);
  g_object_unref (menu);

  return handled;
}

static void
gtk_menu_init (GtkMenu *menu)
{
  GtkMenuPrivate *priv;
  GtkCssNode *top_arrow_node, *bottom_arrow_node, *widget_node;

  priv = gtk_menu_get_instance_private (menu);

  menu->priv = priv;

  priv->toplevel = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_container_add (GTK_CONTAINER (priv->toplevel), GTK_WIDGET (menu));
  g_signal_connect (priv->toplevel, "event", G_CALLBACK (gtk_menu_window_event), menu);
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
  priv->menu_type_hint = GDK_WINDOW_TYPE_HINT_POPUP_MENU;

  _gtk_widget_set_captured_event_handler (GTK_WIDGET (menu), gtk_menu_captured_event);

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (menu));
  priv->top_arrow_gadget = gtk_builtin_icon_new ("arrow",
                                                 GTK_WIDGET (menu),
                                                 NULL, NULL);
  gtk_css_gadget_add_class (priv->top_arrow_gadget, GTK_STYLE_CLASS_TOP);
  top_arrow_node = gtk_css_gadget_get_node (priv->top_arrow_gadget);
  gtk_css_node_set_parent (top_arrow_node, widget_node);
  gtk_css_node_set_visible (top_arrow_node, FALSE);
  gtk_css_node_set_state (top_arrow_node, gtk_css_node_get_state (widget_node));

  priv->bottom_arrow_gadget = gtk_builtin_icon_new ("arrow",
                                                    GTK_WIDGET (menu),
                                                    NULL, NULL);
  gtk_css_gadget_add_class (priv->bottom_arrow_gadget, GTK_STYLE_CLASS_BOTTOM);
  bottom_arrow_node = gtk_css_gadget_get_node (priv->bottom_arrow_gadget);
  gtk_css_node_set_parent (bottom_arrow_node, widget_node);
  gtk_css_node_set_visible (bottom_arrow_node, FALSE);
  gtk_css_node_set_state (bottom_arrow_node, gtk_css_node_get_state (widget_node));
}

static void
moved_to_rect_cb (GdkWindow          *window,
                  const GdkRectangle *flipped_rect,
                  const GdkRectangle *final_rect,
                  gboolean            flipped_x,
                  gboolean            flipped_y,
                  GtkMenu            *menu)
{
  GtkMenuPrivate *priv = menu->priv;

  gtk_window_fixate_size (GTK_WINDOW (priv->toplevel));

  if (!priv->emulated_move_to_rect)
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

  if (priv->tearoff_window)
    gtk_widget_destroy (priv->tearoff_window);

  g_clear_pointer (&priv->heights, g_free);

  g_clear_pointer (&priv->title, g_free);

  if (priv->position_func_data_destroy)
    {
      priv->position_func_data_destroy (priv->position_func_data);
      priv->position_func_data = NULL;
      priv->position_func_data_destroy = NULL;
    }

  GTK_WIDGET_CLASS (gtk_menu_parent_class)->destroy (widget);
}

static void
gtk_menu_finalize (GObject *object)
{
  GtkMenu *menu = GTK_MENU (object);
  GtkMenuPrivate *priv = menu->priv;

  g_clear_object (&priv->top_arrow_gadget);
  g_clear_object (&priv->bottom_arrow_gadget);

  G_OBJECT_CLASS (gtk_menu_parent_class)->finalize (object);
}

static void
menu_change_screen (GtkMenu   *menu,
                    GdkScreen *new_screen)
{
  GtkMenuPrivate *priv = menu->priv;

  if (gtk_widget_has_screen (GTK_WIDGET (menu)))
    {
      if (new_screen == gtk_widget_get_screen (GTK_WIDGET (menu)))
        return;
    }

  if (priv->torn_off)
    {
      gtk_window_set_screen (GTK_WINDOW (priv->tearoff_window), new_screen);
      gtk_menu_position (menu, TRUE);
    }

  gtk_window_set_screen (GTK_WINDOW (priv->toplevel), new_screen);
  priv->monitor_num = -1;
}

static void
attach_widget_screen_changed (GtkWidget *attach_widget,
                              GdkScreen *previous_screen,
                              GtkMenu   *menu)
{
  if (gtk_widget_has_screen (attach_widget) &&
      !g_object_get_data (G_OBJECT (menu), "gtk-menu-explicit-screen"))
    menu_change_screen (menu, gtk_widget_get_screen (attach_widget));
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

  g_signal_connect (attach_widget, "screen-changed",
                    G_CALLBACK (attach_widget_screen_changed), menu);
  attach_widget_screen_changed (attach_widget, NULL, menu);

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

  /* Fallback title for menu comes from attach widget */
  gtk_menu_update_title (menu);

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
                                        (gpointer) attach_widget_screen_changed,
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

  /* Fallback title for menu comes from attach widget */
  gtk_menu_update_title (menu);

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
  AttachInfo *ai = get_attach_info (child);
  GtkCssNode *widget_node, *child_node;

  ai->left_attach = -1;
  ai->right_attach = -1;
  ai->top_attach = -1;
  ai->bottom_attach = -1;

  if (gtk_widget_get_realized (GTK_WIDGET (menu_shell)))
    gtk_widget_set_parent_window (child, priv->bin_window);

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (menu));
  child_node = gtk_widget_get_css_node (child);
  gtk_css_node_insert_before (widget_node, child_node,
                              gtk_css_gadget_get_node (priv->bottom_arrow_gadget));

  GTK_MENU_SHELL_CLASS (gtk_menu_parent_class)->insert (menu_shell, child, position);

  menu_queue_resize (menu);
}

static void
gtk_menu_tearoff_bg_copy (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;
  gint width, height;

  if (priv->torn_off)
    {
      GdkWindow *window;
      cairo_surface_t *surface;
      cairo_pattern_t *pattern;
      cairo_t *cr;

      priv->tearoff_active = FALSE;
      priv->saved_scroll_offset = priv->scroll_offset;

      window = gtk_widget_get_window (priv->tearoff_window);
      width = gdk_window_get_width (window);
      height = gdk_window_get_height (window);

      surface = gdk_window_create_similar_surface (window,
                                                   CAIRO_CONTENT_COLOR,
                                                   width,
                                                   height);

      cr = cairo_create (surface);
      gdk_cairo_set_source_window (cr, window, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);

      gtk_widget_set_size_request (priv->tearoff_window, width, height);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      pattern = cairo_pattern_create_for_surface (surface);
      gdk_window_set_background_pattern (window, pattern);
G_GNUC_END_IGNORE_DEPRECATIONS

      cairo_pattern_destroy (pattern);
      cairo_surface_destroy (surface);
    }
}

static gboolean
popup_grab_on_window (GdkWindow *window,
                      GdkDevice *pointer)
{
  GdkGrabStatus status;
  GdkSeat *seat;

  seat = gdk_device_get_seat (pointer);

/* Let GtkMenu use pointer emulation instead of touch events under X11. */
#define GDK_SEAT_CAPABILITY_NO_TOUCH (GDK_SEAT_CAPABILITY_POINTER | \
                                      GDK_SEAT_CAPABILITY_TABLET_STYLUS | \
                                      GDK_SEAT_CAPABILITY_KEYBOARD)
  status = gdk_seat_grab (seat, window,
                          GDK_SEAT_CAPABILITY_NO_TOUCH, TRUE,
                          NULL, NULL, NULL, NULL);

  return status == GDK_GRAB_SUCCESS;
}

static void
associate_menu_grab_transfer_window (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;
  GdkWindow *toplevel_window;
  GdkWindow *transfer_window;

  toplevel_window = gtk_widget_get_window (priv->toplevel);
  transfer_window = g_object_get_data (G_OBJECT (menu), "gtk-menu-transfer-window");

  if (toplevel_window == NULL || transfer_window == NULL)
    return;

  g_object_set_data (G_OBJECT (toplevel_window), I_("gdk-attached-grab-window"), transfer_window);
}

static void
gtk_menu_popup_internal (GtkMenu             *menu,
                         GdkDevice           *device,
                         GtkWidget           *parent_menu_shell,
                         GtkWidget           *parent_menu_item,
                         GtkMenuPositionFunc  func,
                         gpointer             data,
                         GDestroyNotify       destroy,
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

  _gtk_tooltip_hide_in_display (gtk_widget_get_display (GTK_WIDGET (menu)));
  display = gtk_widget_get_display (GTK_WIDGET (menu));

  if (device == NULL)
    device = gtk_get_current_event_device ();

  if (device && gdk_device_get_display (device) != display)
    device = NULL;

  if (device == NULL)
    device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));

  widget = GTK_WIDGET (menu);
  menu_shell = GTK_MENU_SHELL (menu);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    pointer = gdk_device_get_associated_device (device);
  else
    pointer = device;

  menu_shell->priv->parent_menu_shell = parent_menu_shell;

  priv->seen_item_enter = FALSE;

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
      if (popup_grab_on_window (gtk_widget_get_window (xgrab_shell), pointer))
        {
          _gtk_menu_shell_set_grab_device (GTK_MENU_SHELL (xgrab_shell), pointer);
          GTK_MENU_SHELL (xgrab_shell)->priv->have_xgrab = TRUE;
        }
    }
  else
    {
      GdkWindow *transfer_window;

      xgrab_shell = widget;
      transfer_window = menu_grab_transfer_window_get (menu);
      if (popup_grab_on_window (transfer_window, pointer))
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
      menu_grab_transfer_window_destroy (menu);
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
      if ((current_event->type != GDK_BUTTON_PRESS) &&
          (current_event->type != GDK_ENTER_NOTIFY))
        menu_shell->priv->ignore_enter = TRUE;

      source_device = gdk_event_get_source_device (current_event);
      gdk_event_free (current_event);
    }
  else
    menu_shell->priv->ignore_enter = TRUE;

  if (priv->torn_off)
    {
      gtk_menu_tearoff_bg_copy (menu);

      gtk_menu_reparent (menu, priv->toplevel, FALSE);
    }

  parent_toplevel = NULL;
  if (parent_menu_shell)
    parent_toplevel = gtk_widget_get_toplevel (parent_menu_shell);
  else if (!g_object_get_data (G_OBJECT (menu), "gtk-menu-explicit-screen"))
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
  priv->position_func = func;
  priv->position_func_data = data;
  priv->position_func_data_destroy = destroy;
  menu_shell->priv->activate_time = activate_time;

  /* We need to show the menu here rather in the init function
   * because code expects to be able to tell if the menu is onscreen
   * by looking at gtk_widget_get_visible (menu)
   */
  gtk_widget_show (GTK_WIDGET (menu));

  /* Position the menu, possibly changing the size request
   */
  gtk_menu_position (menu, TRUE);

  associate_menu_grab_transfer_window (menu);

  gtk_menu_scroll_to (menu, priv->scroll_offset, GTK_MENU_SCROLL_FLAG_NONE);

  /* if no item is selected, select the first one */
  if (!menu_shell->priv->active_menu_item &&
      source_device && gdk_device_get_source (source_device) == GDK_SOURCE_TOUCHSCREEN)
    gtk_menu_shell_select_first (menu_shell, TRUE);

  /* Once everything is set up correctly, map the toplevel */
  gtk_window_force_resize (GTK_WINDOW (priv->toplevel));
  gtk_widget_show (priv->toplevel);

  if (xgrab_shell == widget)
    popup_grab_on_window (gtk_widget_get_window (widget), pointer); /* Should always succeed */

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

/**
 * gtk_menu_popup_for_device:
 * @menu: a #GtkMenu
 * @device: (allow-none): a #GdkDevice
 * @parent_menu_shell: (allow-none): the menu shell containing the triggering
 *     menu item, or %NULL
 * @parent_menu_item: (allow-none): the menu item whose activation triggered
 *     the popup, or %NULL
 * @func: (allow-none): a user supplied function used to position the menu,
 *     or %NULL
 * @data: (allow-none): user supplied data to be passed to @func
 * @destroy: (allow-none): destroy notify for @data
 * @button: the mouse button which was pressed to initiate the event
 * @activate_time: the time at which the activation event occurred
 *
 * Displays a menu and makes it available for selection.
 *
 * Applications can use this function to display context-sensitive menus,
 * and will typically supply %NULL for the @parent_menu_shell,
 * @parent_menu_item, @func, @data and @destroy parameters. The default
 * menu positioning function will position the menu at the current position
 * of @device (or its corresponding pointer).
 *
 * The @button parameter should be the mouse button pressed to initiate
 * the menu popup. If the menu popup was initiated by something other than
 * a mouse button press, such as a mouse button release or a keypress,
 * @button should be 0.
 *
 * The @activate_time parameter is used to conflict-resolve initiation of
 * concurrent requests for mouse/keyboard grab requests. To function
 * properly, this needs to be the time stamp of the user event (such as
 * a mouse click or key press) that caused the initiation of the popup.
 * Only if no such event is available, gtk_get_current_event_time() can
 * be used instead.
 *
 * Note that this function does not work very well on GDK backends that
 * do not have global coordinates, such as Wayland or Mir. You should
 * probably use one of the gtk_menu_popup_at_ variants, which do not
 * have this problem.
 *
 * Since: 3.0
 *
 * Deprecated: 3.22: Please use gtk_menu_popup_at_widget(),
 *     gtk_menu_popup_at_pointer(). or gtk_menu_popup_at_rect() instead
 */
void
gtk_menu_popup_for_device (GtkMenu             *menu,
                           GdkDevice           *device,
                           GtkWidget           *parent_menu_shell,
                           GtkWidget           *parent_menu_item,
                           GtkMenuPositionFunc  func,
                           gpointer             data,
                           GDestroyNotify       destroy,
                           guint                button,
                           guint32              activate_time)
{
  GtkMenuPrivate *priv;

  g_return_if_fail (GTK_IS_MENU (menu));

  priv = menu->priv;
  priv->rect_window = NULL;
  priv->widget = NULL;

  gtk_menu_popup_internal (menu,
                           device,
                           parent_menu_shell,
                           parent_menu_item,
                           func,
                           data,
                           destroy,
                           button,
                           activate_time);
}

/**
 * gtk_menu_popup:
 * @menu: a #GtkMenu
 * @parent_menu_shell: (allow-none): the menu shell containing the
 *     triggering menu item, or %NULL
 * @parent_menu_item: (allow-none): the menu item whose activation
 *     triggered the popup, or %NULL
 * @func: (scope async) (allow-none): a user supplied function used to position
 *     the menu, or %NULL
 * @data: user supplied data to be passed to @func.
 * @button: the mouse button which was pressed to initiate the event.
 * @activate_time: the time at which the activation event occurred.
 *
 * Displays a menu and makes it available for selection.
 *
 * Applications can use this function to display context-sensitive
 * menus, and will typically supply %NULL for the @parent_menu_shell,
 * @parent_menu_item, @func and @data parameters. The default menu
 * positioning function will position the menu at the current mouse
 * cursor position.
 *
 * The @button parameter should be the mouse button pressed to initiate
 * the menu popup. If the menu popup was initiated by something other
 * than a mouse button press, such as a mouse button release or a keypress,
 * @button should be 0.
 *
 * The @activate_time parameter is used to conflict-resolve initiation
 * of concurrent requests for mouse/keyboard grab requests. To function
 * properly, this needs to be the timestamp of the user event (such as
 * a mouse click or key press) that caused the initiation of the popup.
 * Only if no such event is available, gtk_get_current_event_time() can
 * be used instead.
 *
 * Note that this function does not work very well on GDK backends that
 * do not have global coordinates, such as Wayland or Mir. You should
 * probably use one of the gtk_menu_popup_at_ variants, which do not
 * have this problem.
 *
 * Deprecated: 3.22: Please use gtk_menu_popup_at_widget(),
 *     gtk_menu_popup_at_pointer(). or gtk_menu_popup_at_rect() instead
 */
void
gtk_menu_popup (GtkMenu             *menu,
                GtkWidget           *parent_menu_shell,
                GtkWidget           *parent_menu_item,
                GtkMenuPositionFunc  func,
                gpointer             data,
                guint                button,
                guint32              activate_time)
{
  g_return_if_fail (GTK_IS_MENU (menu));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_menu_popup_for_device (menu,
                             NULL,
                             parent_menu_shell,
                             parent_menu_item,

                             func, data, NULL,
                             button, activate_time);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static GdkDevice *
get_device_for_event (const GdkEvent *event)
{
  GdkDevice *device = NULL;
  GdkSeat *seat = NULL;
  GdkScreen *screen = NULL;
  GdkDisplay *display = NULL;

  device = gdk_event_get_device (event);

  if (device)
    return device;

  seat = gdk_event_get_seat (event);

  if (!seat)
    {
      screen = gdk_event_get_screen (event);

      if (screen)
        display = gdk_screen_get_display (screen);

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
 * @rect_window: (not nullable): the #GdkWindow @rect is relative to
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
 * relative to the top-left corner of @rect_window. @rect_anchor and
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
 *
 * Since: 3.22
 */
void
gtk_menu_popup_at_rect (GtkMenu            *menu,
                        GdkWindow          *rect_window,
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
  g_return_if_fail (GDK_IS_WINDOW (rect_window));
  g_return_if_fail (rect);

  priv = menu->priv;
  priv->rect_window = rect_window;
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
                           NULL,
                           NULL,
                           NULL,
                           button,
                           activate_time);

  g_clear_pointer (&current_event, gdk_event_free);
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
 *
 * Since: 3.22
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
  priv->rect_window = NULL;
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
                           NULL,
                           NULL,
                           NULL,
                           button,
                           activate_time);

  g_clear_pointer (&current_event, gdk_event_free);
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
 *
 * Since: 3.22
 */
void
gtk_menu_popup_at_pointer (GtkMenu        *menu,
                           const GdkEvent *trigger_event)
{
  GdkEvent *current_event = NULL;
  GdkWindow *rect_window = NULL;
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
      rect_window = gdk_event_get_window (trigger_event);

      if (rect_window)
        {
          device = get_device_for_event (trigger_event);

          if (device && gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
            device = gdk_device_get_associated_device (device);

          if (device)
            gdk_window_get_device_position (rect_window, device, &rect.x, &rect.y, NULL);
        }
    }
  else
    g_warning ("no trigger event for menu popup");

  gtk_menu_popup_at_rect (menu,
                          rect_window,
                          &rect,
                          GDK_GRAVITY_SOUTH_EAST,
                          GDK_GRAVITY_NORTH_WEST,
                          trigger_event);

  g_clear_pointer (&current_event, gdk_event_free);
}

static void
get_arrows_border (GtkMenu   *menu,
                   GtkBorder *border)
{
  GtkMenuPrivate *priv = menu->priv;
  gint top_arrow_height, bottom_arrow_height;

  gtk_css_gadget_get_preferred_size (priv->top_arrow_gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     &top_arrow_height, NULL,
                                     NULL, NULL);
  gtk_css_gadget_get_preferred_size (priv->bottom_arrow_gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     &bottom_arrow_height, NULL,
                                     NULL, NULL);

  border->top = priv->upper_arrow_visible ? top_arrow_height : 0;
  border->bottom = priv->lower_arrow_visible ? bottom_arrow_height : 0;
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
 *
 * Since: 3.22
 * Stability: Private
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
  gtk_menu_scroll_to (menu, menu->priv->scroll_offset,
                      GTK_MENU_SCROLL_FLAG_ADAPT);
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
  GdkDevice *pointer;

  g_return_if_fail (GTK_IS_MENU (menu));

  menu_shell = GTK_MENU_SHELL (menu);
  priv = menu->priv;

  menu_shell->priv->parent_menu_shell = NULL;
  menu_shell->priv->active = FALSE;
  menu_shell->priv->ignore_enter = FALSE;

  priv->have_position = FALSE;

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

  pointer = _gtk_menu_shell_get_grab_device (menu_shell);

  if (priv->torn_off)
    {
      gtk_widget_set_size_request (priv->tearoff_window, -1, -1);

      if (gtk_bin_get_child (GTK_BIN (priv->toplevel)))
        {
          gtk_menu_reparent (menu, priv->tearoff_hbox, TRUE);
        }
      else
        {
          /* We popped up the menu from the tearoff, so we need to
           * release the grab - we aren't actually hiding the menu.
           */
          if (menu_shell->priv->have_xgrab && pointer)
            gdk_seat_ungrab (gdk_device_get_seat (pointer));
        }

      /* gtk_menu_popdown is called each time a menu item is selected from
       * a torn off menu. Only scroll back to the saved position if the
       * non-tearoff menu was popped down.
       */
      if (!priv->tearoff_active)
        gtk_menu_scroll_to (menu, priv->saved_scroll_offset,
                            GTK_MENU_SCROLL_FLAG_NONE);
      priv->tearoff_active = TRUE;
    }
  else
    gtk_widget_hide (GTK_WIDGET (menu));

  menu_shell->priv->have_xgrab = FALSE;

  gtk_grab_remove (GTK_WIDGET (menu));

  _gtk_menu_shell_set_grab_device (menu_shell, NULL);

  menu_grab_transfer_window_destroy (menu);
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
  g_return_if_fail (!accel_group || GTK_IS_ACCEL_GROUP (accel_group));

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
 *
 * Since: 2.14
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

  if (priv->accel_path)
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

  if (!menu->priv->torn_off && gtk_widget_is_drawable (GTK_WIDGET (menu)))
    gtk_menu_position (menu, FALSE);
}

static void
gtk_menu_scrollbar_changed (GtkAdjustment *adjustment,
                            GtkMenu       *menu)
{
  double value;

  value = gtk_adjustment_get_value (adjustment);
  if (menu->priv->scroll_offset != value)
    gtk_menu_scroll_to (menu, value, GTK_MENU_SCROLL_FLAG_NONE);
}

static void
gtk_menu_set_tearoff_hints (GtkMenu *menu,
                            gint     width)
{
  GtkMenuPrivate *priv = menu->priv;
  GdkGeometry geometry_hints;

  if (!priv->tearoff_window)
    return;

  if (gtk_widget_get_visible (priv->tearoff_scrollbar))
    {
      GtkRequisition requisition;

      gtk_widget_get_preferred_size (priv->tearoff_scrollbar,
                                     &requisition, NULL);
      width += requisition.width;
    }

  geometry_hints.min_width = width;
  geometry_hints.max_width = width;

  geometry_hints.min_height = 0;
  geometry_hints.max_height = priv->requested_height;

  gtk_window_set_geometry_hints (GTK_WINDOW (priv->tearoff_window),
                                 NULL,
                                 &geometry_hints,
                                 GDK_HINT_MAX_SIZE|GDK_HINT_MIN_SIZE);
}

static void
gtk_menu_update_title (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;

  if (priv->tearoff_window)
    {
      const gchar *title;
      GtkWidget *attach_widget;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      title = gtk_menu_get_title (menu);
G_GNUC_END_IGNORE_DEPRECATIONS;
      if (!title)
        {
          attach_widget = gtk_menu_get_attach_widget (menu);
          if (GTK_IS_MENU_ITEM (attach_widget))
            {
              GtkWidget *child = gtk_bin_get_child (GTK_BIN (attach_widget));
              if (GTK_IS_LABEL (child))
                title = gtk_label_get_text (GTK_LABEL (child));
            }
        }

      if (title)
        gtk_window_set_title (GTK_WINDOW (priv->tearoff_window), title);
    }
}

static GtkWidget*
gtk_menu_get_toplevel (GtkWidget *menu)
{
  GtkWidget *attach, *toplevel;

  attach = gtk_menu_get_attach_widget (GTK_MENU (menu));

  if (GTK_IS_MENU_ITEM (attach))
    attach = gtk_widget_get_parent (attach);

  if (GTK_IS_MENU (attach))
    return gtk_menu_get_toplevel (attach);
  else if (GTK_IS_WIDGET (attach))
    {
      toplevel = gtk_widget_get_toplevel (attach);
      if (gtk_widget_is_toplevel (toplevel))
        return toplevel;
    }

  return NULL;
}

static void
tearoff_window_destroyed (GtkWidget *widget,
                          GtkMenu   *menu)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_menu_set_tearoff_state (menu, FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS;
}

/**
 * gtk_menu_set_tearoff_state:
 * @menu: a #GtkMenu
 * @torn_off: If %TRUE, menu is displayed as a tearoff menu.
 *
 * Changes the tearoff state of the menu.  A menu is normally
 * displayed as drop down menu which persists as long as the menu is
 * active.  It can also be displayed as a tearoff menu which persists
 * until it is closed or reattached.
 *
 * Deprecated: 3.10
 */
void
gtk_menu_set_tearoff_state (GtkMenu  *menu,
                            gboolean  torn_off)
{
  GtkMenuPrivate *priv;
  gint height;

  g_return_if_fail (GTK_IS_MENU (menu));

  priv = menu->priv;

  torn_off = !!torn_off;
  if (priv->torn_off != torn_off)
    {
      priv->torn_off = torn_off;
      priv->tearoff_active = torn_off;

      if (priv->torn_off)
        {
          if (gtk_widget_get_visible (GTK_WIDGET (menu)))
            gtk_menu_popdown (menu);

          if (!priv->tearoff_window)
            {
              GtkWidget *toplevel;

              priv->tearoff_window = g_object_new (GTK_TYPE_WINDOW,
                                                   "type", GTK_WINDOW_TOPLEVEL,
                                                   "screen", gtk_widget_get_screen (priv->toplevel),
                                                   "app-paintable", TRUE,
                                                   NULL);

              gtk_window_set_type_hint (GTK_WINDOW (priv->tearoff_window),
                                        GDK_WINDOW_TYPE_HINT_MENU);
              gtk_window_set_mnemonic_modifier (GTK_WINDOW (priv->tearoff_window), 0);
              g_signal_connect (priv->tearoff_window, "destroy",
                                G_CALLBACK (tearoff_window_destroyed), menu);
              g_signal_connect (priv->tearoff_window, "event",
                                G_CALLBACK (gtk_menu_window_event), menu);

              gtk_menu_update_title (menu);

              gtk_widget_realize (priv->tearoff_window);

              toplevel = gtk_menu_get_toplevel (GTK_WIDGET (menu));
              if (toplevel != NULL)
                gtk_window_set_transient_for (GTK_WINDOW (priv->tearoff_window),
                                              GTK_WINDOW (toplevel));

              priv->tearoff_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
              gtk_container_add (GTK_CONTAINER (priv->tearoff_window),
                                 priv->tearoff_hbox);

              height = gdk_window_get_height (gtk_widget_get_window (GTK_WIDGET (menu)));
              priv->tearoff_adjustment = gtk_adjustment_new (0,
                                                             0, priv->requested_height,
                                                             MENU_SCROLL_STEP2,
                                                             height/2,
                                                             height);
              g_object_connect (priv->tearoff_adjustment,
                                "signal::value-changed", gtk_menu_scrollbar_changed, menu,
                                NULL);
              priv->tearoff_scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, priv->tearoff_adjustment);

              gtk_box_pack_end (GTK_BOX (priv->tearoff_hbox),
                                priv->tearoff_scrollbar,
                                FALSE, FALSE, 0);

              if (gtk_adjustment_get_upper (priv->tearoff_adjustment) > height)
                gtk_widget_show (priv->tearoff_scrollbar);

              gtk_widget_show (priv->tearoff_hbox);
            }

          gtk_menu_reparent (menu, priv->tearoff_hbox, FALSE);

          /* Update menu->requisition */
          gtk_widget_get_preferred_size (GTK_WIDGET (menu), NULL, NULL);

          gtk_menu_set_tearoff_hints (menu, gdk_window_get_width (gtk_widget_get_window (GTK_WIDGET (menu))));

          gtk_widget_realize (priv->tearoff_window);
          gtk_menu_position (menu, TRUE);

          gtk_widget_show (GTK_WIDGET (menu));
          gtk_widget_show (priv->tearoff_window);

          gtk_menu_scroll_to (menu, 0, GTK_MENU_SCROLL_FLAG_NONE);

        }
      else
        {
          gtk_widget_hide (GTK_WIDGET (menu));
          gtk_widget_hide (priv->tearoff_window);
          if (GTK_IS_CONTAINER (priv->toplevel))
            gtk_menu_reparent (menu, priv->toplevel, FALSE);
          gtk_widget_destroy (priv->tearoff_window);

          priv->tearoff_window = NULL;
          priv->tearoff_hbox = NULL;
          priv->tearoff_scrollbar = NULL;
          priv->tearoff_adjustment = NULL;
        }

      g_object_notify (G_OBJECT (menu), "tearoff-state");
    }
}

/**
 * gtk_menu_get_tearoff_state:
 * @menu: a #GtkMenu
 *
 * Returns whether the menu is torn off.
 * See gtk_menu_set_tearoff_state().
 *
 * Returns: %TRUE if the menu is currently torn off.
 *
 * Deprecated: 3.10
 */
gboolean
gtk_menu_get_tearoff_state (GtkMenu *menu)
{
  g_return_val_if_fail (GTK_IS_MENU (menu), FALSE);

  return menu->priv->torn_off;
}

/**
 * gtk_menu_set_title:
 * @menu: a #GtkMenu
 * @title: (nullable): a string containing the title for the menu, or %NULL to
 *   inherit the title of the parent menu item, if any
 *
 * Sets the title string for the menu.
 *
 * The title is displayed when the menu is shown as a tearoff
 * menu. If @title is %NULL, the menu will see if it is attached
 * to a parent menu item, and if so it will try to use the same
 * text as that menu item’s label.
 *
 * Deprecated: 3.10
 */
void
gtk_menu_set_title (GtkMenu     *menu,
                    const gchar *title)
{
  GtkMenuPrivate *priv;
  char *old_title;

  g_return_if_fail (GTK_IS_MENU (menu));

  priv = menu->priv;

  old_title = priv->title;
  priv->title = g_strdup (title);
  g_free (old_title);

  gtk_menu_update_title (menu);
  g_object_notify (G_OBJECT (menu), "tearoff-title");
}

/**
 * gtk_menu_get_title:
 * @menu: a #GtkMenu
 *
 * Returns the title of the menu. See gtk_menu_set_title().
 *
 * Returns: the title of the menu, or %NULL if the menu
 *     has no title set on it. This string is owned by GTK+
 *     and should not be modified or freed.
 *
 * Deprecated: 3.10
 **/
const gchar *
gtk_menu_get_title (GtkMenu *menu)
{
  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

  return menu->priv->title;
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
get_menu_padding (GtkWidget *widget,
                  GtkBorder *padding)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_get_padding (context,
                                 gtk_style_context_get_state (context),
                                 padding);
}

static void
get_menu_margin (GtkWidget *widget,
                 GtkBorder *margin)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_get_margin (context,
                                gtk_style_context_get_state (context),
                                margin);
}

static void
gtk_menu_realize (GtkWidget *widget)
{
  GtkMenu *menu = GTK_MENU (widget);
  GtkMenuPrivate *priv = menu->priv;
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;
  GtkWidget *child;
  GList *children;
  GtkBorder arrow_border, padding;

  g_return_if_fail (GTK_IS_MENU (widget));

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_KEY_PRESS_MASK |
                            GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK );

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gtk_widget_register_window (widget, window);

  get_menu_padding (widget, &padding);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  gtk_widget_get_allocation (widget, &allocation);

  attributes.x = border_width + padding.left;
  attributes.y = border_width + padding.top;
  attributes.width = allocation.width -
    (2 * border_width) - padding.left - padding.right;
  attributes.height = allocation.height -
    (2 * border_width) - padding.top - padding.bottom;

  get_arrows_border (menu, &arrow_border);
  attributes.y += arrow_border.top;
  attributes.height -= arrow_border.top;
  attributes.height -= arrow_border.bottom;

  attributes.width = MAX (1, attributes.width);
  attributes.height = MAX (1, attributes.height);

  priv->view_window = gdk_window_new (window,
                                      &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->view_window);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.x = 0;
  attributes.y = - priv->scroll_offset;
  attributes.width = allocation.width + (2 * border_width) +
    padding.left + padding.right;
  attributes.height = priv->requested_height - (2 * border_width) +
    padding.top + padding.bottom;

  attributes.width = MAX (1, attributes.width);
  attributes.height = MAX (1, attributes.height);

  priv->bin_window = gdk_window_new (priv->view_window,
                                     &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->bin_window);

  children = GTK_MENU_SHELL (menu)->priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      gtk_widget_set_parent_window (child, priv->bin_window);
    }

  if (GTK_MENU_SHELL (widget)->priv->active_menu_item)
    gtk_menu_scroll_item_visible (GTK_MENU_SHELL (widget),
                                  GTK_MENU_SHELL (widget)->priv->active_menu_item);

  gdk_window_show (priv->bin_window);
  gdk_window_show (priv->view_window);
}

static gboolean
gtk_menu_focus (GtkWidget       *widget,
                GtkDirectionType direction)
{
  /* A menu or its menu items cannot have focus */
  return FALSE;
}

/* See notes in gtk_menu_popup() for information
 * about the “grab transfer window”
 */
static GdkWindow *
menu_grab_transfer_window_get (GtkMenu *menu)
{
  GdkWindow *window = g_object_get_data (G_OBJECT (menu), "gtk-menu-transfer-window");
  if (!window)
    {
      GdkWindowAttr attributes;
      gint attributes_mask;
      GdkWindow *parent;

      attributes.x = -100;
      attributes.y = -100;
      attributes.width = 10;
      attributes.height = 10;
      attributes.window_type = GDK_WINDOW_TEMP;
      attributes.wclass = GDK_INPUT_ONLY;
      attributes.override_redirect = TRUE;
      attributes.event_mask = 0;

      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

      parent = gdk_screen_get_root_window (gtk_widget_get_screen (GTK_WIDGET (menu)));
      window = gdk_window_new (parent,
                               &attributes, attributes_mask);
      gtk_widget_register_window (GTK_WIDGET (menu), window);

      gdk_window_show (window);

      g_object_set_data (G_OBJECT (menu), I_("gtk-menu-transfer-window"), window);
    }

  return window;
}

static void
menu_grab_transfer_window_destroy (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;
  GdkWindow *window = g_object_get_data (G_OBJECT (menu), "gtk-menu-transfer-window");
  if (window)
    {
      GdkWindow *toplevel_window;

      gtk_widget_unregister_window (GTK_WIDGET (menu), window);
      gdk_window_destroy (window);
      g_object_set_data (G_OBJECT (menu), I_("gtk-menu-transfer-window"), NULL);

      toplevel_window = gtk_widget_get_window (priv->toplevel);

      if (toplevel_window != NULL)
        g_object_set_data (G_OBJECT (toplevel_window), I_("gdk-attached-grab-window"), NULL);
    }
}

static void
gtk_menu_unrealize (GtkWidget *widget)
{
  GtkMenu *menu = GTK_MENU (widget);
  GtkMenuPrivate *priv = menu->priv;

  menu_grab_transfer_window_destroy (menu);

  gtk_widget_unregister_window (widget, priv->view_window);
  gdk_window_destroy (priv->view_window);
  priv->view_window = NULL;

  gtk_widget_unregister_window (widget, priv->bin_window);
  gdk_window_destroy (priv->bin_window);
  priv->bin_window = NULL;

  GTK_WIDGET_CLASS (gtk_menu_parent_class)->unrealize (widget);
}

static gint
calculate_line_heights (GtkMenu *menu,
                        gint     for_width,
                        guint  **ret_min_heights,
                        guint  **ret_nat_heights)
{
  GtkBorder       padding;
  GtkMenuPrivate *priv;
  GtkMenuShell   *menu_shell;
  GtkWidget      *child, *widget;
  GList          *children;
  guint           border_width;
  guint           n_columns;
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
  n_columns    = gtk_menu_get_n_columns (menu);
  avail_width  = for_width - (2 * priv->toggle_size + priv->accel_size) * n_columns;

  get_menu_padding (widget, &padding);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (menu));
  avail_width -= (border_width) * 2 + padding.left + padding.right;

  for (children = menu_shell->priv->children; children; children = children->next)
    {
      gint part;
      gint toggle_size;
      gint l, r, t, b;
      gint child_min, child_nat;

      child = children->data;

      if (!gtk_widget_get_visible (child))
        continue;

      get_effective_child_attach (child, &l, &r, &t, &b);

      part = avail_width / (r - l);

      gtk_widget_get_preferred_height_for_width (child, part,
                                                 &child_min, &child_nat);

      gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (child), &toggle_size);

      part = MAX (child_min, toggle_size) / (b - t);
      min_heights[t] = MAX (min_heights[t], part);

      part = MAX (child_nat, toggle_size) / (b - t);
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
gtk_menu_size_allocate (GtkWidget     *widget,
                        GtkAllocation *allocation)
{
  GtkMenu *menu;
  GtkMenuPrivate *priv;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GtkAllocation arrow_allocation, child_allocation;
  GtkAllocation clip;
  GList *children;
  gint x, y, i;
  gint width, height;
  guint border_width;
  GtkBorder arrow_border, padding;

  g_return_if_fail (GTK_IS_MENU (widget));
  g_return_if_fail (allocation != NULL);

  menu = GTK_MENU (widget);
  menu_shell = GTK_MENU_SHELL (widget);
  priv = menu->priv;

  gtk_widget_set_allocation (widget, allocation);

  get_menu_padding (widget, &padding);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (menu));

  g_free (priv->heights);
  priv->heights_length = calculate_line_heights (menu,
                                                 allocation->width,
                                                 &priv->heights,
                                                 NULL);

  /* refresh our cached height request */
  priv->requested_height = (2 * border_width) + padding.top + padding.bottom;
  for (i = 0; i < priv->heights_length; i++)
    priv->requested_height += priv->heights[i];

  x = border_width + padding.left;
  y = border_width + padding.top;
  width = allocation->width - (2 * border_width) - padding.left - padding.right;
  height = allocation->height - (2 * border_width) - padding.top - padding.bottom;

  if (menu_shell->priv->active)
    gtk_menu_scroll_to (menu, priv->scroll_offset, GTK_MENU_SCROLL_FLAG_NONE);

  get_arrows_border (menu, &arrow_border);

  arrow_allocation.x = x;
  arrow_allocation.y = y;
  arrow_allocation.width = width;
  arrow_allocation.height = arrow_border.top;

  if (priv->upper_arrow_visible)
    gtk_css_gadget_allocate (priv->top_arrow_gadget,
                             &arrow_allocation, -1,
                             &clip);

  arrow_allocation.y = height - y - arrow_border.bottom;
  arrow_allocation.height = arrow_border.bottom;

  if (priv->lower_arrow_visible)
    gtk_css_gadget_allocate (priv->bottom_arrow_gadget,
                             &arrow_allocation, -1,
                             &clip);

  if (!priv->tearoff_active)
    {
      y += arrow_border.top;
      height -= arrow_border.top;
      height -= arrow_border.bottom;
    }

  width = MAX (1, width);
  height = MAX (1, height);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (gtk_widget_get_window (widget),
                              allocation->x, allocation->y,
                              allocation->width, allocation->height);

      gdk_window_move_resize (priv->view_window, x, y, width, height);
    }

  if (menu_shell->priv->children)
    {
      gint base_width = width / gtk_menu_get_n_columns (menu);

      children = menu_shell->priv->children;
      while (children)
        {
          child = children->data;
          children = children->next;

          if (gtk_widget_get_visible (child))
            {
              gint l, r, t, b;

              get_effective_child_attach (child, &l, &r, &t, &b);

              if (gtk_widget_get_direction (GTK_WIDGET (menu)) == GTK_TEXT_DIR_RTL)
                {
                  guint tmp;
                  tmp = gtk_menu_get_n_columns (menu) - l;
                  l = gtk_menu_get_n_columns (menu) - r;
                  r = tmp;
                }

              child_allocation.width = (r - l) * base_width;
              child_allocation.height = 0;
              child_allocation.x = l * base_width;
              child_allocation.y = 0;

              for (i = 0; i < b; i++)
                {
                  if (i < t)
                    child_allocation.y += priv->heights[i];
                  else
                    child_allocation.height += priv->heights[i];
                }

              gtk_menu_item_toggle_size_allocate (GTK_MENU_ITEM (child),
                                                  priv->toggle_size);

              gtk_widget_size_allocate (child, &child_allocation);
              gtk_widget_queue_draw (child);
            }
        }

      /* Resize the item window */
      if (gtk_widget_get_realized (widget))
        {
          gint w, h;

          h = 0;
          for (i = 0; i < gtk_menu_get_n_rows (menu); i++)
            h += priv->heights[i];

          w = gtk_menu_get_n_columns (menu) * base_width;
          gdk_window_resize (priv->bin_window, w, h);
        }

      if (priv->tearoff_active)
        {
          if (height >= priv->requested_height)
            {
              if (gtk_widget_get_visible (priv->tearoff_scrollbar))
                {
                  gtk_widget_hide (priv->tearoff_scrollbar);
                  gtk_menu_set_tearoff_hints (menu, allocation->width);

                  gtk_menu_scroll_to (menu, 0, GTK_MENU_SCROLL_FLAG_NONE);
                }
            }
          else
            {
              gtk_adjustment_configure (priv->tearoff_adjustment,
                                        gtk_adjustment_get_value (priv->tearoff_adjustment),
                                        0,
                                        priv->requested_height,
                                        gtk_adjustment_get_step_increment (priv->tearoff_adjustment),
                                        gtk_adjustment_get_page_increment (priv->tearoff_adjustment),
                                        allocation->height);

              if (!gtk_widget_get_visible (priv->tearoff_scrollbar))
                {
                  gtk_widget_show (priv->tearoff_scrollbar);
                  gtk_menu_set_tearoff_hints (menu, allocation->width);
                }
            }
        }
    }
}

static gboolean
gtk_menu_draw (GtkWidget *widget,
               cairo_t   *cr)
{
  GtkMenu *menu;
  GtkMenuPrivate *priv;
  GtkStyleContext *context;
  gint width, height;

  menu = GTK_MENU (widget);
  priv = menu->priv;
  context = gtk_widget_get_style_context (widget);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  if (gtk_cairo_should_draw_window (cr, gtk_widget_get_window (widget)))
    {
      gtk_render_background (context, cr, 0, 0,
                             width, height);
      gtk_render_frame (context, cr, 0, 0,
                        width, height);

      if (priv->upper_arrow_visible && !priv->tearoff_active)
        gtk_css_gadget_draw (priv->top_arrow_gadget, cr);

      if (priv->lower_arrow_visible && !priv->tearoff_active)
        gtk_css_gadget_draw (priv->bottom_arrow_gadget, cr);
    }

  if (gtk_cairo_should_draw_window (cr, priv->bin_window))
    {
      int x, y;

      gdk_window_get_position (priv->view_window, &x, &y);
      cairo_rectangle (cr,
                       x, y,
                       gdk_window_get_width (priv->view_window),
                       gdk_window_get_height (priv->view_window));
      cairo_clip (cr);

      GTK_WIDGET_CLASS (gtk_menu_parent_class)->draw (widget, cr);
    }

  return FALSE;
}

static void
gtk_menu_show (GtkWidget *widget)
{
  GtkMenu *menu = GTK_MENU (widget);

  _gtk_menu_refresh_accel_paths (menu, FALSE);

  GTK_WIDGET_CLASS (gtk_menu_parent_class)->show (widget);
}


static void 
gtk_menu_get_preferred_width (GtkWidget *widget,
                              gint      *minimum_size,
                              gint      *natural_size)
{
  GtkMenu        *menu;
  GtkMenuShell   *menu_shell;
  GtkMenuPrivate *priv;
  GtkWidget      *child;
  GList          *children;
  guint           max_toggle_size;
  guint           max_accel_width;
  guint           border_width;
  gint            child_min, child_nat;
  gint            min_width, nat_width;
  GtkBorder       padding;

  menu       = GTK_MENU (widget);
  menu_shell = GTK_MENU_SHELL (widget);
  priv       = menu->priv;

  min_width = nat_width = 0;

  max_toggle_size = 0;
  max_accel_width = 0;

  children = menu_shell->priv->children;
  while (children)
    {
      gint part;
      gint toggle_size;
      gint l, r, t, b;

      child = children->data;
      children = children->next;

      if (! gtk_widget_get_visible (child))
        continue;

      get_effective_child_attach (child, &l, &r, &t, &b);

      /* It's important to size_request the child
       * before doing the toggle size request, in
       * case the toggle size request depends on the size
       * request of a child of the child (e.g. for ImageMenuItem)
       */
       gtk_widget_get_preferred_width (child, &child_min, &child_nat);

       gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (child), &toggle_size);
       max_toggle_size = MAX (max_toggle_size, toggle_size);
       max_accel_width = MAX (max_accel_width,
                              GTK_MENU_ITEM (child)->priv->accelerator_width);

       part = child_min / (r - l);
       min_width = MAX (min_width, part);

       part = child_nat / (r - l);
       nat_width = MAX (nat_width, part);
    }

  /* If the menu doesn't include any images or check items
   * reserve the space so that all menus are consistent.
   * We only do this for 'ordinary' menus, not for combobox
   * menus or multi-column menus
   */
  if (max_toggle_size == 0 &&
      gtk_menu_get_n_columns (menu) == 1 &&
      !priv->no_toggle_size)
    {
      GtkWidget *menu_item;
      GtkCssGadget *indicator_gadget;
      gint indicator_width;

      /* Create a GtkCheckMenuItem, to query indicator size */
      menu_item = gtk_check_menu_item_new ();
      indicator_gadget = _gtk_check_menu_item_get_indicator_gadget
        (GTK_CHECK_MENU_ITEM (menu_item));

      gtk_css_gadget_get_preferred_size (indicator_gadget,
                                         GTK_ORIENTATION_HORIZONTAL,
                                         -1,
                                         &indicator_width, NULL,
                                         NULL, NULL);
      max_toggle_size = indicator_width;

      gtk_widget_destroy (menu_item);
      g_object_ref_sink (menu_item);
      g_object_unref (menu_item);
    }

  min_width += 2 * max_toggle_size + max_accel_width;
  min_width *= gtk_menu_get_n_columns (menu);

  nat_width += 2 * max_toggle_size + max_accel_width;
  nat_width *= gtk_menu_get_n_columns (menu);

  get_menu_padding (widget, &padding);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (menu));
  min_width   += (2 * border_width) + padding.left + padding.right;
  nat_width   += (2 * border_width) + padding.left + padding.right;

  priv->toggle_size = max_toggle_size;
  priv->accel_size  = max_accel_width;

  *minimum_size = min_width;
  *natural_size = nat_width;

  /* Don't resize the tearoff if it is not active,
   * because it won't redraw (it is only a background pixmap).
   */
  if (priv->tearoff_active)
    gtk_menu_set_tearoff_hints (menu, min_width);
}

static void
gtk_menu_get_preferred_height (GtkWidget *widget,
                               gint      *minimum_size,
                               gint      *natural_size)
{
  gint min_width, nat_width;

  /* Menus are height-for-width only, just return the height
   * for the minimum width
   */
  GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, &min_width, &nat_width);
  GTK_WIDGET_GET_CLASS (widget)->get_preferred_height_for_width (widget, min_width, minimum_size, natural_size);
}

static void
gtk_menu_get_preferred_height_for_width (GtkWidget *widget,
                                         gint       for_size,
                                         gint      *minimum_size,
                                         gint      *natural_size)
{
  GtkBorder       padding, arrow_border;
  GtkMenu        *menu = GTK_MENU (widget);
  GtkMenuPrivate *priv = menu->priv;
  guint          *min_heights, *nat_heights;
  guint           border_width;
  gint            n_heights, i;
  gint            min_height, single_height, nat_height;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (menu));
  get_menu_padding (widget, &padding);

  min_height = nat_height = (2 * border_width) + padding.top + padding.bottom;
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
  single_height += (2 * border_width) 
                   + padding.top + padding.bottom
                   + arrow_border.top + arrow_border.bottom;
  min_height = MIN (min_height, single_height);

  if (priv->have_position)
    {
      GdkDisplay *display;
      GdkMonitor *monitor;
      GdkRectangle workarea;
      GtkBorder border;

      display = gtk_widget_get_display (priv->toplevel);
      monitor = gdk_display_get_monitor (display, priv->monitor_num);
      gdk_monitor_get_workarea (monitor, &workarea);

      if (priv->position_y + min_height > workarea.y + workarea.height)
        min_height = workarea.y + workarea.height - priv->position_y;

      if (priv->position_y + nat_height > workarea.y + workarea.height)
        nat_height = workarea.y + workarea.height - priv->position_y;

      _gtk_window_get_shadow_width (GTK_WINDOW (priv->toplevel), &border);

      if (priv->position_y + border.top < workarea.y)
        {
          min_height -= workarea.y - (priv->position_y + border.top);
          nat_height -= workarea.y - (priv->position_y + border.top);
        }
    }

  *minimum_size = min_height;
  *natural_size = nat_height;

  g_free (min_heights);
  g_free (nat_heights);
}

static gboolean
pointer_in_menu_window (GtkWidget *widget,
                        gdouble    x_root,
                        gdouble    y_root)
{
  GtkMenu *menu = GTK_MENU (widget);
  GtkMenuPrivate *priv = menu->priv;
  GtkAllocation allocation;

  if (gtk_widget_get_mapped (priv->toplevel))
    {
      GtkMenuShell *menu_shell;
      gint          window_x, window_y;

      gdk_window_get_position (gtk_widget_get_window (priv->toplevel),
                               &window_x, &window_y);

      gtk_widget_get_allocation (widget, &allocation);
      if (x_root >= window_x && x_root < window_x + allocation.width &&
          y_root >= window_y && y_root < window_y + allocation.height)
        return TRUE;

      menu_shell = GTK_MENU_SHELL (widget);

      if (GTK_IS_MENU (menu_shell->priv->parent_menu_shell))
        return pointer_in_menu_window (menu_shell->priv->parent_menu_shell,
                                       x_root, y_root);
    }

  return FALSE;
}

static gboolean
gtk_menu_button_press (GtkWidget      *widget,
                       GdkEventButton *event)
{
  GdkDevice *source_device;
  GtkWidget *event_widget;
  GtkMenu *menu;

  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  source_device = gdk_event_get_source_device ((GdkEvent *) event);
  event_widget = gtk_get_event_widget ((GdkEvent *) event);
  menu = GTK_MENU (widget);

  /*  Don't pass down to menu shell if a non-menuitem part of the menu
   *  was clicked. The check for the event_widget being a GtkMenuShell
   *  works because we have the pointer grabbed on menu_shell->window
   *  with owner_events=TRUE, so all events that are either outside
   *  the menu or on its border are delivered relative to
   *  menu_shell->window.
   */
  if (GTK_IS_MENU_SHELL (event_widget) &&
      pointer_in_menu_window (widget, event->x_root, event->y_root))
    return TRUE;

  if (GTK_IS_MENU_ITEM (event_widget) &&
      gdk_device_get_source (source_device) == GDK_SOURCE_TOUCHSCREEN &&
      GTK_MENU_ITEM (event_widget)->priv->submenu != NULL &&
      !gtk_widget_is_drawable (GTK_MENU_ITEM (event_widget)->priv->submenu))
    menu->priv->ignore_button_release = TRUE;

  return GTK_WIDGET_CLASS (gtk_menu_parent_class)->button_press_event (widget, event);
}

static gboolean
gtk_menu_button_release (GtkWidget      *widget,
                         GdkEventButton *event)
{
  GtkMenuPrivate *priv = GTK_MENU (widget)->priv;

  if (priv->ignore_button_release)
    {
      priv->ignore_button_release = FALSE;
      return FALSE;
    }

  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

  /*  Don't pass down to menu shell if a non-menuitem part of the menu
   *  was clicked (see comment in button_press()).
   */
  if (GTK_IS_MENU_SHELL (gtk_get_event_widget ((GdkEvent *) event)) &&
      pointer_in_menu_window (widget, event->x_root, event->y_root))
    {
      /*  Ugly: make sure menu_shell->button gets reset to 0 when we
       *  bail out early here so it is in a consistent state for the
       *  next button_press/button_release in GtkMenuShell.
       *  See bug #449371.
       */
      if (GTK_MENU_SHELL (widget)->priv->active)
        GTK_MENU_SHELL (widget)->priv->button = 0;

      return TRUE;
    }

  return GTK_WIDGET_CLASS (gtk_menu_parent_class)->button_release_event (widget, event);
}

static gboolean
gtk_menu_key_press (GtkWidget   *widget,
                    GdkEventKey *event)
{
  GtkMenu *menu;

  g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  menu = GTK_MENU (widget);

  gtk_menu_stop_navigating_submenu (menu);

  return GTK_WIDGET_CLASS (gtk_menu_parent_class)->key_press_event (widget, event);
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
definitely_within_item (GtkWidget *widget,
                        gint       x,
                        gint       y)
{
  GdkWindow *window = GTK_MENU_ITEM (widget)->priv->event_window;
  int w, h;

  w = gdk_window_get_width (window);
  h = gdk_window_get_height (window);

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

static gboolean
gtk_menu_motion_notify (GtkWidget      *widget,
                        GdkEventMotion *event)
{
  GtkWidget *menu_item;
  GtkMenu *menu;
  GtkMenuShell *menu_shell;
  GtkWidget *parent;
  GdkDevice *source_device;

  gboolean need_enter;

  source_device = gdk_event_get_source_device ((GdkEvent *) event);

  if (GTK_IS_MENU (widget) &&
      gdk_device_get_source (source_device) != GDK_SOURCE_TOUCHSCREEN)
    {
      GtkMenuPrivate *priv = GTK_MENU(widget)->priv;

      if (priv->ignore_button_release)
        priv->ignore_button_release = FALSE;

      gtk_menu_handle_scrolling (GTK_MENU (widget), event->x_root, event->y_root,
                                 TRUE, TRUE);
    }

  /* We received the event for one of two reasons:
   *
   * a) We are the active menu, and did gtk_grab_add()
   * b) The widget is a child of ours, and the event was propagated
   *
   * Since for computation of navigation regions, we want the menu which
   * is the parent of the menu item, for a), we need to find that menu,
   * which may be different from 'widget'.
   */
  menu_item = gtk_get_event_widget ((GdkEvent*) event);
  parent = gtk_widget_get_parent (menu_item);
  if (!GTK_IS_MENU_ITEM (menu_item) ||
      !GTK_IS_MENU (parent))
    return FALSE;

  menu_shell = GTK_MENU_SHELL (parent);
  menu = GTK_MENU (menu_shell);

  if (definitely_within_item (menu_item, event->x, event->y))
    menu_shell->priv->activate_time = 0;

  need_enter = (gtk_menu_has_navigation_triangle (menu) || menu_shell->priv->ignore_enter);

  /* Check to see if we are within an active submenu's navigation region
   */
  if (gtk_menu_navigating_submenu (menu, event->x_root, event->y_root))
    return TRUE;

  /* Make sure we pop down if we enter a non-selectable menu item, so we
   * don't show a submenu when the cursor is outside the stay-up triangle.
   */
  if (!_gtk_menu_item_is_selectable (menu_item))
    {
      /* We really want to deselect, but this gives the menushell code
       * a chance to do some bookkeeping about the menuitem.
       */
      gtk_menu_shell_select_item (menu_shell, menu_item);
      return FALSE;
    }

  if (need_enter)
    {
      /* The menu is now sensitive to enter events on its items, but
       * was previously sensitive.  So we fake an enter event.
       */
      menu_shell->priv->ignore_enter = FALSE;

      if (event->x >= 0 && event->x < gdk_window_get_width (event->window) &&
          event->y >= 0 && event->y < gdk_window_get_height (event->window))
        {
          GdkEvent *send_event = gdk_event_new (GDK_ENTER_NOTIFY);
          gboolean result;

          send_event->crossing.window = g_object_ref (event->window);
          send_event->crossing.time = event->time;
          send_event->crossing.send_event = TRUE;
          send_event->crossing.x_root = event->x_root;
          send_event->crossing.y_root = event->y_root;
          send_event->crossing.x = event->x;
          send_event->crossing.y = event->y;
          send_event->crossing.state = event->state;
          gdk_event_set_device (send_event, gdk_event_get_device ((GdkEvent *) event));

          /* We send the event to 'widget', the currently active menu,
           * instead of 'menu', the menu that the pointer is in. This
           * will ensure that the event will be ignored unless the
           * menuitem is a child of the active menu or some parent
           * menu of the active menu.
           */
          result = gtk_widget_event (widget, send_event);
          gdk_event_free (send_event);

          return result;
        }
    }

  return FALSE;
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

  view_height = gdk_window_get_height (gtk_widget_get_window (widget));

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
    gtk_menu_scroll_to (menu, offset, GTK_MENU_SCROLL_FLAG_NONE);
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
gtk_menu_scroll (GtkWidget      *widget,
                 GdkEventScroll *event)
{
  GtkMenu *menu = GTK_MENU (widget);

  if (gdk_event_get_pointer_emulated ((GdkEvent *) event))
    return GDK_EVENT_PROPAGATE;

  switch (event->direction)
    {
    case GDK_SCROLL_DOWN:
      gtk_menu_scroll_by (menu, MENU_SCROLL_STEP2);
      break;
    case GDK_SCROLL_UP:
      gtk_menu_scroll_by (menu, - MENU_SCROLL_STEP2);
      break;
    case GDK_SCROLL_SMOOTH:
      gtk_menu_scroll_by (menu, event->delta_y * MENU_SCROLL_STEP2);
      break;
    default:
      return GDK_EVENT_PROPAGATE;
      break;
    }

  return GDK_EVENT_STOP;
}

static void
get_arrows_sensitive_area (GtkMenu      *menu,
                           GdkRectangle *upper,
                           GdkRectangle *lower)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkWidget *widget = GTK_WIDGET (menu);
  GdkWindow *window;
  gint width, height;
  guint border;
  gint win_x, win_y;
  GtkBorder padding;
  gint top_arrow_height, bottom_arrow_height;

  gtk_css_gadget_get_preferred_size (priv->top_arrow_gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     &top_arrow_height, NULL,
                                     NULL, NULL);
  gtk_css_gadget_get_preferred_size (priv->bottom_arrow_gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     &bottom_arrow_height, NULL,
                                     NULL, NULL);

  window = gtk_widget_get_window (widget);
  width = gdk_window_get_width (window);
  height = gdk_window_get_height (window);

  border = gtk_container_get_border_width (GTK_CONTAINER (menu));
  get_menu_padding (widget, &padding);

  gdk_window_get_position (window, &win_x, &win_y);

  if (upper)
    {
      upper->x = win_x;
      upper->y = win_y;
      upper->width = width;
      upper->height = top_arrow_height + border + padding.top;
    }

  if (lower)
    {
      lower->x = win_x;
      lower->y = win_y + height - border - padding.bottom - bottom_arrow_height;
      lower->width = width;
      lower->height = bottom_arrow_height + border + padding.bottom;
    }
}

static void
gtk_menu_handle_scrolling (GtkMenu *menu,
                           gint     x,
                           gint     y,
                           gboolean enter,
                           gboolean motion)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkMenuShell *menu_shell;
  GdkRectangle rect;
  gboolean in_arrow;
  gboolean scroll_fast = FALSE;
  gint top_x, top_y;

  menu_shell = GTK_MENU_SHELL (menu);

  gdk_window_get_position (gtk_widget_get_window (priv->toplevel),
                           &top_x, &top_y);
  x -= top_x;
  y -= top_y;

  /*  upper arrow handling  */

  get_arrows_sensitive_area (menu, &rect, NULL);

  in_arrow = FALSE;
  if (priv->upper_arrow_visible && !priv->tearoff_active &&
      (x >= rect.x) && (x < rect.x + rect.width) &&
      (y >= rect.y) && (y < rect.y + rect.height))
    {
      in_arrow = TRUE;
    }

  if ((priv->upper_arrow_state & GTK_STATE_FLAG_INSENSITIVE) == 0)
    {
      gboolean arrow_pressed = FALSE;

      if (priv->upper_arrow_visible && !priv->tearoff_active)
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

              priv->scroll_timeout =
                gdk_threads_add_timeout (scroll_fast
                                           ? MENU_SCROLL_TIMEOUT2
                                           : MENU_SCROLL_TIMEOUT1,
                                         gtk_menu_scroll_timeout, menu);
              g_source_set_name_by_id (priv->scroll_timeout, "[gtk+] gtk_menu_scroll_timeout");
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

          if (arrow_pressed)
            arrow_state |= GTK_STATE_FLAG_ACTIVE;

          if (priv->upper_arrow_prelight)
            arrow_state |= GTK_STATE_FLAG_PRELIGHT;

          if (arrow_state != priv->upper_arrow_state)
            {
              priv->upper_arrow_state = arrow_state;
              gtk_css_gadget_set_state (priv->top_arrow_gadget, arrow_state);

              gdk_window_invalidate_rect (gtk_widget_get_window (GTK_WIDGET (menu)),
                                          &rect, FALSE);
            }
        }
    }

  /*  lower arrow handling  */

  get_arrows_sensitive_area (menu, NULL, &rect);

  in_arrow = FALSE;
  if (priv->lower_arrow_visible && !priv->tearoff_active &&
      (x >= rect.x) && (x < rect.x + rect.width) &&
      (y >= rect.y) && (y < rect.y + rect.height))
    {
      in_arrow = TRUE;
    }

  if ((priv->lower_arrow_state & GTK_STATE_FLAG_INSENSITIVE) == 0)
    {
      gboolean arrow_pressed = FALSE;

      if (priv->lower_arrow_visible && !priv->tearoff_active)
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

              priv->scroll_timeout =
                gdk_threads_add_timeout (scroll_fast
                                           ? MENU_SCROLL_TIMEOUT2
                                           : MENU_SCROLL_TIMEOUT1,
                                         gtk_menu_scroll_timeout, menu);
              g_source_set_name_by_id (priv->scroll_timeout, "[gtk+] gtk_menu_scroll_timeout");
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

          if (arrow_pressed)
            arrow_state |= GTK_STATE_FLAG_ACTIVE;

          if (priv->lower_arrow_prelight)
            arrow_state |= GTK_STATE_FLAG_PRELIGHT;

          if (arrow_state != priv->lower_arrow_state)
            {
              priv->lower_arrow_state = arrow_state;
              gtk_css_gadget_set_state (priv->bottom_arrow_gadget, arrow_state);

              gdk_window_invalidate_rect (gtk_widget_get_window (GTK_WIDGET (menu)),
                                          &rect, FALSE);
            }
        }
    }
}

static gboolean
gtk_menu_enter_notify (GtkWidget        *widget,
                       GdkEventCrossing *event)
{
  GtkWidget *menu_item;
  GtkWidget *parent;
  GdkDevice *source_device;

  if (event->mode == GDK_CROSSING_GTK_GRAB ||
      event->mode == GDK_CROSSING_GTK_UNGRAB ||
      event->mode == GDK_CROSSING_STATE_CHANGED)
    return TRUE;

  source_device = gdk_event_get_source_device ((GdkEvent *) event);
  menu_item = gtk_get_event_widget ((GdkEvent*) event);

  if (GTK_IS_MENU (widget) &&
      gdk_device_get_source (source_device) != GDK_SOURCE_TOUCHSCREEN)
    {
      GtkMenuShell *menu_shell = GTK_MENU_SHELL (widget);

      if (!menu_shell->priv->ignore_enter)
        gtk_menu_handle_scrolling (GTK_MENU (widget),
                                   event->x_root, event->y_root, TRUE, TRUE);
    }

  if (gdk_device_get_source (source_device) != GDK_SOURCE_TOUCHSCREEN &&
      GTK_IS_MENU_ITEM (menu_item))
    {
      GtkWidget *menu = gtk_widget_get_parent (menu_item);

      if (GTK_IS_MENU (menu))
        {
          GtkMenuPrivate *priv = (GTK_MENU (menu))->priv;
          GtkMenuShell *menu_shell = GTK_MENU_SHELL (menu);

          if (priv->seen_item_enter)
            {
              /* This is the second enter we see for an item
               * on this menu. This means a release should always
               * mean activate.
               */
              menu_shell->priv->activate_time = 0;
            }
          else if ((event->detail != GDK_NOTIFY_NONLINEAR &&
                    event->detail != GDK_NOTIFY_NONLINEAR_VIRTUAL))
            {
              if (definitely_within_item (menu_item, event->x, event->y))
                {
                  /* This is an actual user-enter (ie. not a pop-under)
                   * In this case, the user must either have entered
                   * sufficiently far enough into the item, or he must move
                   * far enough away from the enter point. (see
                   * gtk_menu_motion_notify())
                   */
                  menu_shell->priv->activate_time = 0;
                }
            }

          priv->seen_item_enter = TRUE;
        }
    }

  /* If this is a faked enter (see gtk_menu_motion_notify), 'widget'
   * will not correspond to the event widget's parent.  Check to see
   * if we are in the parent's navigation region.
   */
  parent = gtk_widget_get_parent (menu_item);
  if (GTK_IS_MENU_ITEM (menu_item) && GTK_IS_MENU (parent) &&
      gtk_menu_navigating_submenu (GTK_MENU (parent),
                                   event->x_root, event->y_root))
    return TRUE;

  return GTK_WIDGET_CLASS (gtk_menu_parent_class)->enter_notify_event (widget, event);
}

static gboolean
gtk_menu_leave_notify (GtkWidget        *widget,
                       GdkEventCrossing *event)
{
  GtkMenuShell *menu_shell;
  GtkMenu *menu;
  GtkMenuItem *menu_item;
  GtkWidget *event_widget;
  GdkDevice *source_device;

  if (event->mode == GDK_CROSSING_GTK_GRAB ||
      event->mode == GDK_CROSSING_GTK_UNGRAB ||
      event->mode == GDK_CROSSING_STATE_CHANGED)
    return TRUE;

  menu = GTK_MENU (widget);
  menu_shell = GTK_MENU_SHELL (widget);

  if (gtk_menu_navigating_submenu (menu, event->x_root, event->y_root))
    return TRUE;

  source_device = gdk_event_get_source_device ((GdkEvent *) event);

  if (gdk_device_get_source (source_device) != GDK_SOURCE_TOUCHSCREEN)
    gtk_menu_handle_scrolling (menu, event->x_root, event->y_root, FALSE, TRUE);

  event_widget = gtk_get_event_widget ((GdkEvent*) event);

  if (!GTK_IS_MENU_ITEM (event_widget))
    return TRUE;

  menu_item = GTK_MENU_ITEM (event_widget);

  /* Here we check to see if we're leaving an active menu item
   * with a submenu, in which case we enter submenu navigation mode.
   */
  if (menu_shell->priv->active_menu_item != NULL
      && menu_item->priv->submenu != NULL
      && menu_item->priv->submenu_placement == GTK_LEFT_RIGHT)
    {
      if (GTK_MENU_SHELL (menu_item->priv->submenu)->priv->active)
        {
          gtk_menu_set_submenu_navigation_region (menu, menu_item, event);
          return TRUE;
        }
      else if (menu_item == GTK_MENU_ITEM (menu_shell->priv->active_menu_item))
        {
          /* We are leaving an active menu item with nonactive submenu.
           * Deselect it so we don't surprise the user with by popping
           * up a submenu _after_ he left the item.
           */
          gtk_menu_shell_deselect (menu_shell);
          return TRUE;
        }
    }

  return GTK_WIDGET_CLASS (gtk_menu_parent_class)->leave_notify_event (widget, event);
}

static gboolean
pointer_on_menu_widget (GtkMenu *menu,
                        gdouble  x_root,
                        gdouble  y_root)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkAllocation allocation;
  gint window_x, window_y;

  gtk_widget_get_allocation (GTK_WIDGET (menu), &allocation);
  gdk_window_get_position (gtk_widget_get_window (priv->toplevel),
                           &window_x, &window_y);

  if (x_root >= window_x && x_root < window_x + allocation.width &&
      y_root >= window_y && y_root < window_y + allocation.height)
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

  if (!priv->upper_arrow_visible && !priv->lower_arrow_visible && priv->drag_start_y < 0)
    return retval;

  source_device = gdk_event_get_source_device (event);
  gdk_event_get_root_coords (event, &x_root, &y_root);

  switch (event->type)
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

              view_height = gdk_window_get_height (gtk_widget_get_window (widget));
              get_arrows_border (menu, &arrow_border);

              if (priv->upper_arrow_visible)
                view_height -= arrow_border.top;

              if (priv->lower_arrow_visible)
                view_height -= arrow_border.bottom;

              offset = CLAMP (offset,
                              MIN (priv->scroll_offset, 0),
                              MAX (priv->scroll_offset, priv->requested_height - view_height));

              gtk_menu_scroll_to (menu, offset, GTK_MENU_SCROLL_FLAG_NONE);

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

/* When the timeout is elapsed, the navigation region is destroyed
 * and the menuitem under the pointer (if any) is selected.
 */
static gboolean
gtk_menu_stop_navigating_submenu_cb (gpointer user_data)
{
  GtkMenuPopdownData *popdown_data = user_data;
  GtkMenu *menu = popdown_data->menu;
  GtkMenuPrivate *priv = menu->priv;
  GdkWindow *child_window;

  gtk_menu_stop_navigating_submenu (menu);

  if (gtk_widget_get_realized (GTK_WIDGET (menu)))
    {
      child_window = gdk_window_get_device_position (priv->bin_window,
                                                     popdown_data->device,
                                                     NULL, NULL, NULL);

      if (child_window)
        {
          GdkEvent *send_event = gdk_event_new (GDK_ENTER_NOTIFY);

          send_event->crossing.window = g_object_ref (child_window);
          send_event->crossing.time = GDK_CURRENT_TIME; /* Bogus */
          send_event->crossing.send_event = TRUE;
          gdk_event_set_device (send_event, popdown_data->device);

          GTK_WIDGET_CLASS (gtk_menu_parent_class)->enter_notify_event (GTK_WIDGET (menu), (GdkEventCrossing *)send_event);

          gdk_event_free (send_event);
        }
    }

  return FALSE;
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
gtk_menu_set_submenu_navigation_region (GtkMenu          *menu,
                                        GtkMenuItem      *menu_item,
                                        GdkEventCrossing *event)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkWidget *event_widget;
  GdkWindow *window;
  int submenu_left;
  int submenu_right;
  int submenu_top;
  int submenu_bottom;
  int width;

  g_return_if_fail (menu_item->priv->submenu != NULL);
  g_return_if_fail (event != NULL);

  event_widget = gtk_get_event_widget ((GdkEvent*) event);

  window = gtk_widget_get_window (menu_item->priv->submenu);
  gdk_window_get_origin (window, &submenu_left, &submenu_top);

  submenu_right = submenu_left + gdk_window_get_width (window);
  submenu_bottom = submenu_top + gdk_window_get_height (window);

  width = gdk_window_get_width (gtk_widget_get_window (event_widget));

  if (event->x >= 0 && event->x < width)
    {
      GtkMenuPopdownData *popdown_data;
      /* The calculations below assume floored coordinates */
      int x_root = floor (event->x_root);
      int y_root = floor (event->y_root);

      gtk_menu_stop_navigating_submenu (menu);

      /* The navigation region is the triangle closest to the x/y
       * location of the rectangle. This is why the width or height
       * can be negative.
       */
      if (menu_item->priv->submenu_direction == GTK_DIRECTION_RIGHT)
        {
          /* right */
          priv->navigation_x = submenu_left;
          priv->navigation_width = x_root - submenu_left;
        }
      else
        {
          /* left */
          priv->navigation_x = submenu_right;
          priv->navigation_width = x_root - submenu_right;
        }

      if (event->y < 0)
        {
          /* top */
          priv->navigation_y = y_root;
          priv->navigation_height = submenu_top - y_root - NAVIGATION_REGION_OVERSHOOT;

          if (priv->navigation_height >= 0)
            return;
        }
      else
        {
          /* bottom */
          priv->navigation_y = y_root;
          priv->navigation_height = submenu_bottom - y_root + NAVIGATION_REGION_OVERSHOOT;

          if (priv->navigation_height <= 0)
            return;
        }

      popdown_data = g_new (GtkMenuPopdownData, 1);
      popdown_data->menu = menu;
      popdown_data->device = gdk_event_get_device ((GdkEvent *) event);

      priv->navigation_timeout = gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT,
                                                               MENU_POPDOWN_DELAY,
                                                               gtk_menu_stop_navigating_submenu_cb,
                                                               popdown_data,
                                                               (GDestroyNotify) g_free);
      g_source_set_name_by_id (priv->navigation_timeout, "[gtk+] gtk_menu_stop_navigating_submenu_cb");
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

static void
gtk_menu_position_legacy (GtkMenu  *menu,
                          gboolean  set_scroll_offset)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkWidget *widget;
  GtkRequisition requisition;
  gint x, y;
  gint scroll_offset;
  GdkDisplay *display;
  GdkMonitor *monitor;
  GdkRectangle workarea;
  gint monitor_num;
  GdkDevice *pointer;
  GtkBorder border;
  gint i;

  widget = GTK_WIDGET (menu);

  display = gtk_widget_get_display (widget);
  pointer = _gtk_menu_shell_get_grab_device (GTK_MENU_SHELL (menu));
  gdk_device_get_position (pointer, NULL, &x, &y);

  /* Realize so we have the proper width and height to figure out
   * the right place to popup the menu.
   */
  gtk_widget_realize (priv->toplevel);

  _gtk_window_get_shadow_width (GTK_WINDOW (priv->toplevel), &border);

  requisition.width = gtk_widget_get_allocated_width (widget);
  requisition.height = gtk_widget_get_allocated_height (widget);

  monitor = gdk_display_get_monitor_at_point (display, x, y);
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

  priv->monitor_num = monitor_num;
  priv->initially_pushed_in = FALSE;

  /* Set the type hint here to allow custom position functions
   * to set a different hint
   */
  if (!gtk_widget_get_visible (priv->toplevel))
    gtk_window_set_type_hint (GTK_WINDOW (priv->toplevel), GDK_WINDOW_TYPE_HINT_POPUP_MENU);

  if (priv->position_func)
    {
      (* priv->position_func) (menu, &x, &y, &priv->initially_pushed_in,
                               priv->position_func_data);

      if (priv->monitor_num < 0)
        priv->monitor_num = monitor_num;

      monitor = gdk_display_get_monitor (display, priv->monitor_num);
      gdk_monitor_get_workarea (monitor, &workarea);
    }
  else
    {
      gint space_left, space_right, space_above, space_below;
      gint needed_width;
      gint needed_height;
      GtkBorder padding;
      GtkBorder margin;
      gboolean rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

      get_menu_padding (widget, &padding);
      get_menu_margin (widget, &margin);

      /* The placement of popup menus horizontally works like this (with
       * RTL in parentheses)
       *
       * - If there is enough room to the right (left) of the mouse cursor,
       *   position the menu there.
       *
       * - Otherwise, if if there is enough room to the left (right) of the
       *   mouse cursor, position the menu there.
       *
       * - Otherwise if the menu is smaller than the monitor, position it
       *   on the side of the mouse cursor that has the most space available
       *
       * - Otherwise (if there is simply not enough room for the menu on the
       *   monitor), position it as far left (right) as possible.
       *
       * Positioning in the vertical direction is similar: first try below
       * mouse cursor, then above.
       */
      monitor = gdk_display_get_monitor (display, priv->monitor_num);
      gdk_monitor_get_workarea (monitor, &workarea);

      space_left = x - workarea.x;
      space_right = workarea.x + workarea.width - x - 1;
      space_above = y - workarea.y;
      space_below = workarea.y + workarea.height - y - 1;

      /* Position horizontally. */

      /* the amount of space we need to position the menu.
       * Note the menu is offset "thickness" pixels
       */
      needed_width = requisition.width - padding.left;

      if (needed_width <= space_left ||
          needed_width <= space_right)
        {
          if ((rtl  && needed_width <= space_left) ||
              (!rtl && needed_width >  space_right))
            {
              /* position left */
              x = x - margin.left + padding.left - requisition.width + 1;
            }
          else
            {
              /* position right */
              x = x + margin.right - padding.right;
            }

          /* x is clamped on-screen further down */
        }
      else if (requisition.width <= workarea.width)
        {
          /* the menu is too big to fit on either side of the mouse
           * cursor, but smaller than the monitor. Position it on
           * the side that has the most space
           */
          if (space_left > space_right)
            {
              /* left justify */
              x = workarea.x;
            }
          else
            {
              /* right justify */
              x = workarea.x + workarea.width - requisition.width;
            }
        }
      else /* menu is simply too big for the monitor */
        {
          if (rtl)
            {
              /* right justify */
              x = workarea.x + workarea.width - requisition.width;
            }
          else
            {
              /* left justify */
              x = workarea.x;
            }
        }

      /* Position vertically.
       * The algorithm is the same as above, but simpler
       * because we don't have to take RTL into account.
       */
      needed_height = requisition.height - padding.top;

      if (needed_height <= space_above ||
          needed_height <= space_below)
        {
          if (needed_height <= space_below)
            y = y + margin.top - padding.top;
          else
            y = y - margin.bottom + padding.bottom - requisition.height + 1;

          y = CLAMP (y, workarea.y,
                     workarea.y + workarea.height - requisition.height);
        }
      else if (needed_height > space_below && needed_height > space_above)
        {
          if (space_below >= space_above)
            y = workarea.y + workarea.height - requisition.height;
          else
            y = workarea.y;
        }
      else
        {
          y = workarea.y;
        }
    }

  scroll_offset = 0;

  if (y + requisition.height > workarea.y + workarea.height)
    {
      if (priv->initially_pushed_in)
        scroll_offset += (workarea.y + workarea.height) - requisition.height - y;
      y = (workarea.y + workarea.height) - requisition.height;
    }

  if (y < workarea.y)
    {
      if (priv->initially_pushed_in)
        scroll_offset += workarea.y - y;
      y = workarea.y;
    }

  x = CLAMP (x, workarea.x, MAX (workarea.x, workarea.x + workarea.width - requisition.width));

  x -= border.left;
  y -= border.top;

  if (GTK_MENU_SHELL (menu)->priv->active)
    {
      priv->have_position = TRUE;
      priv->position_x = x;
      priv->position_y = y;
    }

  if (scroll_offset != 0)
    {
      GtkBorder arrow_border;

      get_arrows_border (menu, &arrow_border);
      scroll_offset += arrow_border.top;
    }

  gtk_window_move (GTK_WINDOW (GTK_MENU_SHELL (menu)->priv->active ? priv->toplevel : priv->tearoff_window),
                   x, y);

  if (!GTK_MENU_SHELL (menu)->priv->active)
    {
      gtk_window_resize (GTK_WINDOW (priv->tearoff_window),
                         requisition.width, requisition.height);
    }

  if (set_scroll_offset)
    priv->scroll_offset = scroll_offset;
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
    }

  g_warning ("unknown GdkGravity: %d", anchor);
  return anchor;
}

static void
gtk_menu_position (GtkMenu  *menu,
                   gboolean  set_scroll_offset)
{
  GtkMenuPrivate *priv = menu->priv;
  GdkWindow *rect_window = NULL;
  GdkRectangle rect;
  GtkTextDirection text_direction = GTK_TEXT_DIR_NONE;
  GdkGravity rect_anchor;
  GdkGravity menu_anchor;
  GdkAnchorHints anchor_hints;
  gint rect_anchor_dx, rect_anchor_dy;
  GdkWindow *toplevel;
  gboolean emulated_move_to_rect = FALSE;

  rect_anchor = priv->rect_anchor;
  menu_anchor = priv->menu_anchor;
  anchor_hints = priv->anchor_hints;
  rect_anchor_dx = priv->rect_anchor_dx;
  rect_anchor_dy = priv->rect_anchor_dy;

  if (priv->rect_window &&
      !GDK_PRIVATE_CALL (gdk_window_is_impl_offscreen (priv->rect_window)))
    {
      rect_window = priv->rect_window;
      rect = priv->rect;
    }
  else if (priv->widget)
    {
      rect_window = gtk_widget_get_window (priv->widget);
      gtk_widget_get_allocation (priv->widget, &rect);
      text_direction = gtk_widget_get_direction (priv->widget);
    }
  else if (!priv->position_func)
    {
      GtkWidget *attach_widget;
      GdkDevice *grab_device;

      /*
       * One of the legacy gtk_menu_popup*() functions were used to popup but
       * without a custom positioning function, so make an attempt to let the
       * backend do the position constraining when required conditions are met.
       */

      grab_device = _gtk_menu_shell_get_grab_device (GTK_MENU_SHELL (menu));
      attach_widget = gtk_menu_get_attach_widget (menu);

      if (grab_device && attach_widget)
        {
          rect.x = 0;
          rect.y = 0;
          rect.width = 1;
          rect.height = 1;

          rect_window = gtk_widget_get_window (attach_widget);
          gdk_window_get_device_position (rect_window, grab_device,
                                          &rect.x, &rect.y, NULL);
          text_direction = gtk_widget_get_direction (attach_widget);
          rect_anchor = GDK_GRAVITY_SOUTH_EAST;
          menu_anchor = GDK_GRAVITY_NORTH_WEST;
          anchor_hints = GDK_ANCHOR_FLIP | GDK_ANCHOR_SLIDE | GDK_ANCHOR_RESIZE;
          rect_anchor_dx = 0;
          rect_anchor_dy = 0;
          emulated_move_to_rect = TRUE;
        }
    }

  if (rect_window != NULL &&
      GDK_PRIVATE_CALL (gdk_window_is_impl_offscreen (rect_window)))
    {
      GdkWindow *effective = gdk_offscreen_window_get_embedder (rect_window);

      if (effective)
        {
          double x = rect.x, y = rect.y;

          gdk_window_coords_to_parent (rect_window, x, y, &x, &y);

          rect.x = x;
          rect.y = y;
        }

      rect_window = effective;
    }

  if (!rect_window)
    {
      gtk_window_set_unlimited_guessed_size (GTK_WINDOW (priv->toplevel),
                                             FALSE, FALSE);
      gtk_menu_position_legacy (menu, set_scroll_offset);
      return;
    }

  gtk_window_set_unlimited_guessed_size (GTK_WINDOW (priv->toplevel),
                                         !!(anchor_hints & GDK_ANCHOR_RESIZE_X),
                                         !!(anchor_hints & GDK_ANCHOR_RESIZE_Y));

  if (!gtk_widget_get_visible (priv->toplevel))
    gtk_window_set_type_hint (GTK_WINDOW (priv->toplevel), priv->menu_type_hint);

  /* Realize so we have the proper width and height to figure out
   * the right place to popup the menu.
   */
  gtk_widget_realize (priv->toplevel);
  gtk_window_move_resize (GTK_WINDOW (priv->toplevel));

  if (text_direction == GTK_TEXT_DIR_NONE)
    text_direction = gtk_widget_get_direction (GTK_WIDGET (menu));

  if (text_direction == GTK_TEXT_DIR_RTL)
    {
      rect_anchor = get_horizontally_flipped_anchor (rect_anchor);
      menu_anchor = get_horizontally_flipped_anchor (menu_anchor);
    }

  toplevel = gtk_widget_get_window (priv->toplevel);

  gdk_window_set_transient_for (toplevel, rect_window);

  g_signal_handlers_disconnect_by_func (toplevel, moved_to_rect_cb, menu);

  g_signal_connect (toplevel, "moved-to-rect", G_CALLBACK (moved_to_rect_cb),
                    menu);
  priv->emulated_move_to_rect = emulated_move_to_rect;

  gdk_window_move_to_rect (toplevel,
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

  top_arrow_node = gtk_css_gadget_get_node (priv->top_arrow_gadget);
  state = gtk_css_node_get_state (top_arrow_node);
  gtk_css_node_set_state (top_arrow_node, state & ~GTK_STATE_FLAG_PRELIGHT);

  bottom_arrow_node = gtk_css_gadget_get_node (priv->bottom_arrow_gadget);
  state = gtk_css_node_get_state (bottom_arrow_node);
  gtk_css_node_set_state (bottom_arrow_node, state & ~GTK_STATE_FLAG_PRELIGHT);
}

static void
sync_arrows_state (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkCssNode *top_arrow_node, *bottom_arrow_node;

  top_arrow_node = gtk_css_gadget_get_node (priv->top_arrow_gadget);
  gtk_css_node_set_visible (top_arrow_node, priv->upper_arrow_visible);
  gtk_css_node_set_state (top_arrow_node, priv->upper_arrow_state);

  bottom_arrow_node = gtk_css_gadget_get_node (priv->bottom_arrow_gadget);
  gtk_css_node_set_visible (bottom_arrow_node, priv->lower_arrow_visible);
  gtk_css_node_set_state (bottom_arrow_node, priv->lower_arrow_state);
}

static void
gtk_menu_scroll_to (GtkMenu           *menu,
                    gint               offset,
                    GtkMenuScrollFlag  flags)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkBorder arrow_border, padding;
  GtkWidget *widget;
  gint x, y;
  gint view_width, view_height;
  gint border_width;
  gint menu_height;

  widget = GTK_WIDGET (menu);

  if (priv->tearoff_active && priv->tearoff_adjustment)
    gtk_adjustment_set_value (priv->tearoff_adjustment, offset);

  /* Move/resize the viewport according to arrows: */
  view_width = gtk_widget_get_allocated_width (widget);
  view_height = gtk_widget_get_allocated_height (widget);

  get_menu_padding (widget, &padding);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (menu));

  view_width -= (2 * border_width) + padding.left + padding.right;
  view_height -= (2 * border_width) + padding.top + padding.bottom;
  menu_height = priv->requested_height - (2 * border_width) - padding.top - padding.bottom;

  x = border_width + padding.left;
  y = border_width + padding.top;

  if (!priv->tearoff_active)
    {
      if (view_height < menu_height               ||
          (offset > 0 && priv->scroll_offset > 0) ||
          (offset < 0 && priv->scroll_offset < 0))
        {
          GtkStateFlags upper_arrow_previous_state = priv->upper_arrow_state;
          GtkStateFlags lower_arrow_previous_state = priv->lower_arrow_state;
          gboolean should_offset_by_arrow;

          if (!priv->upper_arrow_visible || !priv->lower_arrow_visible)
            gtk_widget_queue_draw (GTK_WIDGET (menu));

          if (!priv->upper_arrow_visible &&
              flags & GTK_MENU_SCROLL_FLAG_ADAPT)
            should_offset_by_arrow = TRUE;
          else
            should_offset_by_arrow = FALSE;

          priv->upper_arrow_visible = priv->lower_arrow_visible = TRUE;

          if (flags & GTK_MENU_SCROLL_FLAG_ADAPT)
            sync_arrows_state (menu);

          get_arrows_border (menu, &arrow_border);
          if (should_offset_by_arrow)
            offset += arrow_border.top;
          y += arrow_border.top;
          view_height -= arrow_border.top;
          view_height -= arrow_border.bottom;

          if (offset <= 0)
            priv->upper_arrow_state |= GTK_STATE_FLAG_INSENSITIVE;
          else
            {
              priv->upper_arrow_state &= ~(GTK_STATE_FLAG_INSENSITIVE);

              if (priv->upper_arrow_prelight)
                priv->upper_arrow_state |= GTK_STATE_FLAG_PRELIGHT;
              else
                priv->upper_arrow_state &= ~(GTK_STATE_FLAG_PRELIGHT);
            }

          if (offset >= menu_height - view_height)
            priv->lower_arrow_state |= GTK_STATE_FLAG_INSENSITIVE;
          else
            {
              priv->lower_arrow_state &= ~(GTK_STATE_FLAG_INSENSITIVE);

              if (priv->lower_arrow_prelight)
                priv->lower_arrow_state |= GTK_STATE_FLAG_PRELIGHT;
              else
                priv->lower_arrow_state &= ~(GTK_STATE_FLAG_PRELIGHT);
            }

          if ((priv->upper_arrow_state != upper_arrow_previous_state) ||
              (priv->lower_arrow_state != lower_arrow_previous_state))
            gtk_widget_queue_draw (GTK_WIDGET (menu));

          if ((upper_arrow_previous_state & GTK_STATE_FLAG_INSENSITIVE) == 0 &&
              (priv->upper_arrow_state & GTK_STATE_FLAG_INSENSITIVE) != 0)
            {
              /* At the upper border, possibly remove timeout */
              if (priv->scroll_step < 0)
                {
                  gtk_menu_stop_scrolling (menu);
                  gtk_widget_queue_draw (GTK_WIDGET (menu));
                }
            }

          if ((lower_arrow_previous_state & GTK_STATE_FLAG_INSENSITIVE) == 0 &&
              (priv->lower_arrow_state & GTK_STATE_FLAG_INSENSITIVE) != 0)
            {
              /* At the lower border, possibly remove timeout */
              if (priv->scroll_step > 0)
                {
                  gtk_menu_stop_scrolling (menu);
                  gtk_widget_queue_draw (GTK_WIDGET (menu));
                }
            }
        }
      else if (priv->upper_arrow_visible || priv->lower_arrow_visible)
        {
          offset = 0;

          priv->upper_arrow_visible = priv->lower_arrow_visible = FALSE;
          priv->upper_arrow_prelight = priv->lower_arrow_prelight = FALSE;

          gtk_menu_stop_scrolling (menu);
          gtk_widget_queue_draw (GTK_WIDGET (menu));
        }
    }

  sync_arrows_state (menu);

  /* Scroll the menu: */
  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move (priv->bin_window, 0, -offset);
      gdk_window_move_resize (priv->view_window, x, y, view_width, view_height);
    }

  priv->scroll_offset = offset;
}

static gboolean
compute_child_offset (GtkMenu   *menu,
                      GtkWidget *menu_item,
                      gint      *offset,
                      gint      *height,
                      gboolean  *is_last_child)
{
  GtkMenuPrivate *priv = menu->priv;
  gint item_top_attach;
  gint item_bottom_attach;
  gint child_offset = 0;
  gint i;

  get_effective_child_attach (menu_item, NULL, NULL,
                              &item_top_attach, &item_bottom_attach);

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

  if (is_last_child)
    *is_last_child = (item_bottom_attach == gtk_menu_get_n_rows (menu));
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
  gboolean last_child = 0;

  /* We need to check if the selected item fully visible.
   * If not we need to scroll the menu so that it becomes fully
   * visible.
   */
  if (compute_child_offset (menu, menu_item,
                            &child_offset, &child_height, &last_child))
    {
      GtkBorder padding;

      y = priv->scroll_offset;
      height = gdk_window_get_height (gtk_widget_get_window (widget));

      get_menu_padding (widget, &padding);

      height -= 2 * gtk_container_get_border_width (GTK_CONTAINER (menu)) +
        padding.top + padding.bottom;

      if (child_offset < y)
        {
          /* Ignore the enter event we might get if the pointer
           * is on the menu
           */
          menu_shell->priv->ignore_enter = TRUE;
          gtk_menu_scroll_to (menu, child_offset, GTK_MENU_SCROLL_FLAG_NONE);
        }
      else
        {
          GtkBorder arrow_border;

          arrow_height = 0;

          get_arrows_border (menu, &arrow_border);
          if (!priv->tearoff_active)
            arrow_height = arrow_border.top + arrow_border.bottom;

          if (child_offset + child_height > y + height - arrow_height)
            {
              arrow_height = arrow_border.bottom + arrow_border.top;
              y = child_offset + child_height - height + arrow_height;

              /* Ignore the enter event we might get if the pointer
               * is on the menu
               */
              menu_shell->priv->ignore_enter = TRUE;
              gtk_menu_scroll_to (menu, y, GTK_MENU_SCROLL_FLAG_NONE);
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


/* Reparent the menu, taking care of the refcounting
 *
 * If unrealize is true we force a unrealize while reparenting the parent.
 * This can help eliminate flicker in some cases.
 *
 * What happens is that when the menu is unrealized and then re-realized,
 * the allocations are as follows:
 *
 *  parent - 1x1 at (0,0)
 *  child1 - 100x20 at (0,0)
 *  child2 - 100x20 at (0,20)
 *  child3 - 100x20 at (0,40)
 *
 * That is, the parent is small but the children are full sized. Then,
 * when the queued_resize gets processed, the parent gets resized to
 * full size.
 *
 * But in order to eliminate flicker when scrolling, gdkgeometry-x11.c
 * contains the following logic:
 *
 * - if a move or resize operation on a window would change the clip
 *   region on the children, then before the window is resized
 *   the background for children is temporarily set to None, the
 *   move/resize done, and the background for the children restored.
 *
 * So, at the point where the parent is resized to final size, the
 * background for the children is temporarily None, and thus they
 * are not cleared to the background color and the previous background
 * (the image of the menu) is left in place.
 */
static void
gtk_menu_reparent (GtkMenu   *menu,
                   GtkWidget *new_parent,
                   gboolean   unrealize)
{
  GObject *object = G_OBJECT (menu);
  GtkWidget *widget = GTK_WIDGET (menu);
  gboolean was_floating = g_object_is_floating (object);

  g_object_ref_sink (object);

  if (unrealize)
    {
      g_object_ref (object);
      gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (widget)), widget);
      gtk_container_add (GTK_CONTAINER (new_parent), widget);
      g_object_unref (object);
    }
  else
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_widget_reparent (widget, new_parent);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }

  if (was_floating)
    g_object_force_floating (object);
  else
    g_object_unref (object);
}

static void
gtk_menu_show_all (GtkWidget *widget)
{
  /* Show children, but not self. */
  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_show_all, NULL);
}

/**
 * gtk_menu_set_screen:
 * @menu: a #GtkMenu
 * @screen: (allow-none): a #GdkScreen, or %NULL if the screen should be
 *          determined by the widget the menu is attached to
 *
 * Sets the #GdkScreen on which the menu will be displayed.
 *
 * Since: 2.2
 */
void
gtk_menu_set_screen (GtkMenu   *menu,
                     GdkScreen *screen)
{
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (screen == NULL || GDK_IS_SCREEN (screen));

  g_object_set_data (G_OBJECT (menu), I_("gtk-menu-explicit-screen"), screen);

  if (screen)
    {
      menu_change_screen (menu, screen);
    }
  else
    {
      GtkWidget *attach_widget = gtk_menu_get_attach_widget (menu);
      if (attach_widget)
        attach_widget_screen_changed (attach_widget, NULL, menu);
    }
}

/**
 * gtk_menu_attach:
 * @menu: a #GtkMenu
 * @child: a #GtkMenuItem
 * @left_attach: The column number to attach the left side of the item to
 * @right_attach: The column number to attach the right side of the item to
 * @top_attach: The row number to attach the top of the item to
 * @bottom_attach: The row number to attach the bottom of the item to
 *
 * Adds a new #GtkMenuItem to a (table) menu. The number of “cells” that
 * an item will occupy is specified by @left_attach, @right_attach,
 * @top_attach and @bottom_attach. These each represent the leftmost,
 * rightmost, uppermost and lower column and row numbers of the table.
 * (Columns and rows are indexed from zero).
 *
 * Note that this function is not related to gtk_menu_detach().
 *
 * Since: 2.4
 */
void
gtk_menu_attach (GtkMenu   *menu,
                 GtkWidget *child,
                 guint      left_attach,
                 guint      right_attach,
                 guint      top_attach,
                 guint      bottom_attach)
{
  GtkMenuShell *menu_shell;
  GtkWidget *parent;
  GtkCssNode *widget_node, *child_node;

  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GTK_IS_MENU_ITEM (child));
  parent = gtk_widget_get_parent (child);
  g_return_if_fail (parent == NULL || parent == GTK_WIDGET (menu));
  g_return_if_fail (left_attach < right_attach);
  g_return_if_fail (top_attach < bottom_attach);

  menu_shell = GTK_MENU_SHELL (menu);

  if (!parent)
    {
      AttachInfo *ai = get_attach_info (child);

      ai->left_attach = left_attach;
      ai->right_attach = right_attach;
      ai->top_attach = top_attach;
      ai->bottom_attach = bottom_attach;

      menu_shell->priv->children = g_list_append (menu_shell->priv->children, child);

      widget_node = gtk_widget_get_css_node (GTK_WIDGET (menu));
      child_node = gtk_widget_get_css_node (child);
      gtk_css_node_insert_before (widget_node, child_node,
                                  gtk_css_gadget_get_node (menu->priv->bottom_arrow_gadget));

      gtk_widget_set_parent (child, GTK_WIDGET (menu));

      menu_queue_resize (menu);
    }
  else
    {
      gtk_container_child_set (GTK_CONTAINER (parent), child,
                               "left-attach",   left_attach,
                               "right-attach",  right_attach,
                               "top-attach",    top_attach,
                               "bottom-attach", bottom_attach,
                               NULL);
    }
}

static gint
gtk_menu_get_popup_delay (GtkMenuShell *menu_shell)
{
  return MENU_POPUP_DELAY;
}

static GtkWidget *
find_child_containing (GtkMenuShell *menu_shell,
                       int           left,
                       int           right,
                       int           top,
                       int           bottom)
{
  GList *list;

  /* find a child which includes the area given by
   * left, right, top, bottom.
   */
  for (list = menu_shell->priv->children; list; list = list->next)
    {
      gint l, r, t, b;

      if (!_gtk_menu_item_is_selectable (list->data))
        continue;

      get_effective_child_attach (list->data, &l, &r, &t, &b);

      if (l <= left && right <= r && t <= top && bottom <= b)
        return GTK_WIDGET (list->data);
    }

  return NULL;
}

static void
gtk_menu_move_current (GtkMenuShell         *menu_shell,
                       GtkMenuDirectionType  direction)
{
  GtkMenu *menu = GTK_MENU (menu_shell);
  gint i;
  gint l, r, t, b;
  GtkWidget *match = NULL;

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
        default: ;
        }
    }

  /* use special table menu key bindings */
  if (menu_shell->priv->active_menu_item && gtk_menu_get_n_columns (menu) > 1)
    {
      get_effective_child_attach (menu_shell->priv->active_menu_item, &l, &r, &t, &b);

      if (direction == GTK_MENU_DIR_NEXT)
        {
          for (i = b; i < gtk_menu_get_n_rows (menu); i++)
            {
              match = find_child_containing (menu_shell, l, l + 1, i, i + 1);
              if (match)
                break;
            }

          if (!match)
            {
              /* wrap around */
              for (i = 0; i < t; i++)
                {
                  match = find_child_containing (menu_shell,
                                                 l, l + 1, i, i + 1);
                  if (match)
                    break;
                }
            }
        }
      else if (direction == GTK_MENU_DIR_PREV)
        {
          for (i = t; i > 0; i--)
            {
              match = find_child_containing (menu_shell,
                                             l, l + 1, i - 1, i);
              if (match)
                break;
            }

          if (!match)
            {
              /* wrap around */
              for (i = gtk_menu_get_n_rows (menu); i > b; i--)
                {
                  match = find_child_containing (menu_shell,
                                                 l, l + 1, i - 1, i);
                  if (match)
                    break;
                }
            }
        }
      else if (direction == GTK_MENU_DIR_PARENT)
        {
          /* we go one left if possible */
          if (l > 0)
            match = find_child_containing (menu_shell,
                                           l - 1, l, t, t + 1);

          if (!match)
            {
              GtkWidget *parent = menu_shell->priv->parent_menu_shell;

              if (!parent
                  || g_list_length (GTK_MENU_SHELL (parent)->priv->children) <= 1)
                match = menu_shell->priv->active_menu_item;
            }
        }
      else if (direction == GTK_MENU_DIR_CHILD)
        {
          /* we go one right if possible */
          if (r < gtk_menu_get_n_columns (menu))
            match = find_child_containing (menu_shell, r, r + 1, t, t + 1);

          if (!match)
            {
              GtkWidget *parent = menu_shell->priv->parent_menu_shell;

              if (! GTK_MENU_ITEM (menu_shell->priv->active_menu_item)->priv->submenu &&
                  (!parent ||
                   g_list_length (GTK_MENU_SHELL (parent)->priv->children) <= 1))
                match = menu_shell->priv->active_menu_item;
            }
        }

      if (match)
        {
          gtk_menu_shell_select_item (menu_shell, match);
          return;
        }
    }

  GTK_MENU_SHELL_CLASS (gtk_menu_parent_class)->move_current (menu_shell, direction);
}

static gint
get_visible_size (GtkMenu *menu)
{
  GtkMenuPrivate *priv = menu->priv;
  GtkAllocation allocation;
  GtkWidget *widget = GTK_WIDGET (menu);
  GtkContainer *container = GTK_CONTAINER (menu);
  GtkBorder padding;
  gint menu_height;

  gtk_widget_get_allocation (widget, &allocation);
  get_menu_padding (widget, &padding);

  menu_height = (allocation.height -
                 (2 * gtk_container_get_border_width (container)) -
                 padding.top - padding.bottom);

  if (!priv->tearoff_active)
    {
      GtkBorder arrow_border;

      get_arrows_border (menu, &arrow_border);
      menu_height -= arrow_border.top;
      menu_height -= arrow_border.bottom;
    }

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

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

              if (child_offset + child_requisition.height > y &&
                  !GTK_IS_TEAROFF_MENU_ITEM (child))
                return child;

G_GNUC_END_IGNORE_DEPRECATIONS

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
  GtkWidget *widget = GTK_WIDGET (menu);
  GtkBorder padding;
  gint height;

  get_menu_padding (widget, &padding);

  height = priv->requested_height;
  height -= (gtk_container_get_border_width (GTK_CONTAINER (widget)) * 2) +
    padding.top + padding.bottom;

  if (!priv->tearoff_active)
    {
      GtkBorder arrow_border;

      get_arrows_border (menu, &arrow_border);
      height -= arrow_border.top;
      height -= arrow_border.bottom;
    }

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

  switch (type)
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
                                      &child_offset, &child_height, NULL))
              child_offset += child_height / 2;
          }

        menu_shell->priv->ignore_enter = TRUE;
        old_upper_arrow_visible = priv->upper_arrow_visible && !priv->tearoff_active;
        old_offset = priv->scroll_offset;

        new_offset = priv->scroll_offset + step;
        new_offset = CLAMP (new_offset, 0, end_position - page_size);

        gtk_menu_scroll_to (menu, new_offset, GTK_MENU_SCROLL_FLAG_NONE);

        if (menu_shell->priv->active_menu_item)
          {
            GtkWidget *new_child;
            gboolean new_upper_arrow_visible = priv->upper_arrow_visible && !priv->tearoff_active;
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
 *
 * Since: 2.4
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
 *
 * Since: 2.14
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
 *
 * Since: 3.22
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
 *
 * Since: 2.6
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

  if (GTK_MENU_SHELL (widget)->priv->active && !GTK_IS_MENU_SHELL (grab) &&
      !gtk_widget_is_ancestor (grab, widget))
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
 *
 * Since: 2.18
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
 *
 * Since: 2.18
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
 *
 * Since: 3.4
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
