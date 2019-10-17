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
 * SECTION:gtkmenushell
 * @Title: GtkMenuShell
 * @Short_description: A base class for menu objects
 *
 * A #GtkMenuShell is the abstract base class used to derive the
 * #GtkMenu and #GtkMenuBar subclasses.
 *
 * A #GtkMenuShell is a container of #GtkMenuItem objects arranged
 * in a list which can be navigated, selected, and activated by the
 * user to perform application functions. A #GtkMenuItem can have a
 * submenu associated with it, allowing for nested hierarchical menus.
 *
 * # Terminology
 *
 * A menu item can be “selected”, this means that it is displayed
 * in the prelight state, and if it has a submenu, that submenu
 * will be popped up.
 *
 * A menu is “active” when it is visible onscreen and the user
 * is selecting from it. A menubar is not active until the user
 * clicks on one of its menuitems. When a menu is active,
 * passing the mouse over a submenu will pop it up.
 *
 * There is also a concept of the current menu and a current
 * menu item. The current menu item is the selected menu item
 * that is furthest down in the hierarchy. (Every active menu shell
 * does not necessarily contain a selected menu item, but if
 * it does, then the parent menu shell must also contain
 * a selected menu item.) The current menu is the menu that
 * contains the current menu item. It will always have a GTK
 * grab and receive all key presses.
 */
#include "config.h"

#include "gtkmenushellprivate.h"

#include "gtkbindings.h"
#include "gtkintl.h"
#include "gtkkeyhash.h"
#include "gtklabelprivate.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenubarprivate.h"
#include "gtkmenuitemprivate.h"
#include "gtkmnemonichash.h"
#include "gtkmodelmenuitemprivate.h"
#include "gtkprivate.h"
#include "gtkseparatormenuitem.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkwindow.h"
#include "gtkwindowprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtkgestureclick.h"

#include "a11y/gtkmenushellaccessible.h"


#define MENU_SHELL_TIMEOUT   500
#define MENU_POPUP_DELAY     225
#define MENU_POPDOWN_DELAY   1000

enum {
  DEACTIVATE,
  SELECTION_DONE,
  MOVE_CURRENT,
  ACTIVATE_CURRENT,
  CANCEL,
  CYCLE_FOCUS,
  MOVE_SELECTED,
  INSERT,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_TAKE_FOCUS
};


static void gtk_menu_shell_set_property      (GObject           *object,
                                              guint              prop_id,
                                              const GValue      *value,
                                              GParamSpec        *pspec);
static void gtk_menu_shell_get_property      (GObject           *object,
                                              guint              prop_id,
                                              GValue            *value,
                                              GParamSpec        *pspec);
static void gtk_menu_shell_finalize          (GObject           *object);
static void gtk_menu_shell_dispose           (GObject           *object);
static gboolean gtk_menu_shell_key_press     (GtkEventControllerKey *key,
                                              guint                  keyval,
                                              guint                  keycode,
                                              GdkModifierType        modifiers,
                                              GtkWidget             *widget);
static void gtk_menu_shell_root              (GtkWidget         *widget);
static void click_pressed  (GtkGestureClick *gesture,
                                  gint                  n_press,
                                  gdouble               x,
                                  gdouble               y,
                                  GtkMenuShell         *menu_shell);
static void click_released (GtkGestureClick *gesture,
                                  gint                  n_press,
                                  gdouble               x,
                                  gdouble               y,
                                  GtkMenuShell         *menu_shell);
static void click_stopped  (GtkGestureClick *gesture,
                                  GtkMenuShell         *menu_shell);


static void gtk_menu_shell_add               (GtkContainer      *container,
                                              GtkWidget         *widget);
static void gtk_menu_shell_remove            (GtkContainer      *container,
                                              GtkWidget         *widget);
static void gtk_real_menu_shell_deactivate   (GtkMenuShell      *menu_shell);
static GType    gtk_menu_shell_child_type  (GtkContainer      *container);
static void gtk_menu_shell_real_select_item  (GtkMenuShell      *menu_shell,
                                              GtkWidget         *menu_item);
static gboolean gtk_menu_shell_select_submenu_first (GtkMenuShell   *menu_shell); 

static void gtk_real_menu_shell_move_current (GtkMenuShell      *menu_shell,
                                              GtkMenuDirectionType direction);
static void gtk_real_menu_shell_activate_current (GtkMenuShell      *menu_shell,
                                                  gboolean           force_hide);
static void gtk_real_menu_shell_cancel           (GtkMenuShell      *menu_shell);
static void gtk_real_menu_shell_cycle_focus      (GtkMenuShell      *menu_shell,
                                                  GtkDirectionType   dir);

static void     gtk_menu_shell_reset_key_hash    (GtkMenuShell *menu_shell);
static gboolean gtk_menu_shell_activate_mnemonic (GtkMenuShell    *menu_shell,
                                                  guint            keycode,
                                                  GdkModifierType  state,
                                                  guint            group);
static gboolean gtk_menu_shell_real_move_selected (GtkMenuShell  *menu_shell, 
                                                   gint           distance);

static guint menu_shell_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkMenuShell, gtk_menu_shell, GTK_TYPE_CONTAINER)

static void
gtk_menu_shell_class_init (GtkMenuShellClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  GtkBindingSet *binding_set;

  object_class = (GObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;

  object_class->set_property = gtk_menu_shell_set_property;
  object_class->get_property = gtk_menu_shell_get_property;
  object_class->finalize = gtk_menu_shell_finalize;
  object_class->dispose = gtk_menu_shell_dispose;

  widget_class->root = gtk_menu_shell_root;

  container_class->add = gtk_menu_shell_add;
  container_class->remove = gtk_menu_shell_remove;
  container_class->child_type = gtk_menu_shell_child_type;

  klass->submenu_placement = GTK_TOP_BOTTOM;
  klass->deactivate = gtk_real_menu_shell_deactivate;
  klass->selection_done = NULL;
  klass->move_current = gtk_real_menu_shell_move_current;
  klass->activate_current = gtk_real_menu_shell_activate_current;
  klass->cancel = gtk_real_menu_shell_cancel;
  klass->select_item = gtk_menu_shell_real_select_item;
  klass->move_selected = gtk_menu_shell_real_move_selected;

  /**
   * GtkMenuShell::deactivate:
   * @menushell: the object which received the signal
   *
   * This signal is emitted when a menu shell is deactivated.
   */
  menu_shell_signals[DEACTIVATE] =
    g_signal_new (I_("deactivate"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuShellClass, deactivate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkMenuShell::selection-done:
   * @menushell: the object which received the signal
   *
   * This signal is emitted when a selection has been
   * completed within a menu shell.
   */
  menu_shell_signals[SELECTION_DONE] =
    g_signal_new (I_("selection-done"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuShellClass, selection_done),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkMenuShell::move-current:
   * @menushell: the object which received the signal
   * @direction: the direction to move
   *
   * A keybinding signal which moves the current menu item
   * in the direction specified by @direction.
   */
  menu_shell_signals[MOVE_CURRENT] =
    g_signal_new (I_("move-current"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkMenuShellClass, move_current),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_MENU_DIRECTION_TYPE);

  /**
   * GtkMenuShell::activate-current:
   * @menushell: the object which received the signal
   * @force_hide: if %TRUE, hide the menu after activating the menu item
   *
   * An action signal that activates the current menu item within
   * the menu shell.
   */
  menu_shell_signals[ACTIVATE_CURRENT] =
    g_signal_new (I_("activate-current"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkMenuShellClass, activate_current),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_BOOLEAN);

  /**
   * GtkMenuShell::cancel:
   * @menushell: the object which received the signal
   *
   * An action signal which cancels the selection within the menu shell.
   * Causes the #GtkMenuShell::selection-done signal to be emitted.
   */
  menu_shell_signals[CANCEL] =
    g_signal_new (I_("cancel"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkMenuShellClass, cancel),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkMenuShell::cycle-focus:
   * @menushell: the object which received the signal
   * @direction: the direction to cycle in
   *
   * A keybinding signal which moves the focus in the
   * given @direction.
   */
  menu_shell_signals[CYCLE_FOCUS] =
    g_signal_new_class_handler (I_("cycle-focus"),
                                G_OBJECT_CLASS_TYPE (object_class),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_real_menu_shell_cycle_focus),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1,
                                GTK_TYPE_DIRECTION_TYPE);

  /**
   * GtkMenuShell::move-selected:
   * @menu_shell: the object on which the signal is emitted
   * @distance: +1 to move to the next item, -1 to move to the previous
   *
   * The ::move-selected signal is emitted to move the selection to
   * another item.
   *
   * Returns: %TRUE to stop the signal emission, %FALSE to continue
   */
  menu_shell_signals[MOVE_SELECTED] =
    g_signal_new (I_("move-selected"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkMenuShellClass, move_selected),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__INT,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_INT);

  /**
   * GtkMenuShell::insert:
   * @menu_shell: the object on which the signal is emitted
   * @child: the #GtkMenuItem that is being inserted
   * @position: the position at which the insert occurs
   *
   * The ::insert signal is emitted when a new #GtkMenuItem is added to
   * a #GtkMenuShell.  A separate signal is used instead of
   * GtkContainer::add because of the need for an additional position
   * parameter.
   *
   * The inverse of this signal is the GtkContainer::removed signal.
   **/
  menu_shell_signals[INSERT] =
    g_signal_new (I_("insert"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuShellClass, insert),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_INT,
                  G_TYPE_NONE, 2, GTK_TYPE_WIDGET, G_TYPE_INT);
  g_signal_set_va_marshaller (menu_shell_signals[INSERT],
                              G_OBJECT_CLASS_TYPE (object_class),
                              _gtk_marshal_VOID__OBJECT_INTv);


  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Escape, 0,
                                "cancel", 0);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Return, 0,
                                "activate-current", 1,
                                G_TYPE_BOOLEAN,
                                TRUE);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_ISO_Enter, 0,
                                "activate-current", 1,
                                G_TYPE_BOOLEAN,
                                TRUE);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_KP_Enter, 0,
                                "activate-current", 1,
                                G_TYPE_BOOLEAN,
                                TRUE);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_space, 0,
                                "activate-current", 1,
                                G_TYPE_BOOLEAN,
                                FALSE);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_KP_Space, 0,
                                "activate-current", 1,
                                G_TYPE_BOOLEAN,
                                FALSE);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_F10, 0,
                                "cycle-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_FORWARD);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_F10, GDK_SHIFT_MASK,
                                "cycle-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_BACKWARD);

  /**
   * GtkMenuShell:take-focus:
   *
   * A boolean that determines whether the menu and its submenus grab the
   * keyboard focus. See gtk_menu_shell_set_take_focus() and
   * gtk_menu_shell_get_take_focus().
   **/
  g_object_class_install_property (object_class,
                                   PROP_TAKE_FOCUS,
                                   g_param_spec_boolean ("take-focus",
                                                         P_("Take Focus"),
                                                         P_("A boolean that determines whether the menu grabs the keyboard focus"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_MENU_SHELL_ACCESSIBLE);
}

static GType
gtk_menu_shell_child_type (GtkContainer *container)
{
  return GTK_TYPE_MENU_ITEM;
}

static void
gtk_menu_shell_init (GtkMenuShell *menu_shell)
{
  GtkWidget *widget = GTK_WIDGET (menu_shell);
  GtkEventController *controller;

  menu_shell->priv = gtk_menu_shell_get_instance_private (menu_shell);
  menu_shell->priv->take_focus = TRUE;

  controller = gtk_event_controller_key_new ();
  gtk_event_controller_set_propagation_limit (controller, GTK_LIMIT_NONE);
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (gtk_menu_shell_key_press), widget);
  gtk_widget_add_controller (widget, controller);

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  gtk_event_controller_set_propagation_limit (controller, GTK_LIMIT_NONE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
  g_signal_connect (controller, "pressed",
                    G_CALLBACK (click_pressed), menu_shell);
  g_signal_connect (controller, "released",
                    G_CALLBACK (click_released), menu_shell);
  g_signal_connect (controller, "stopped",
                    G_CALLBACK (click_stopped), menu_shell);
  gtk_widget_add_controller (widget, controller);
}

static void
gtk_menu_shell_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (object);

  switch (prop_id)
    {
    case PROP_TAKE_FOCUS:
      gtk_menu_shell_set_take_focus (menu_shell, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_shell_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (object);

  switch (prop_id)
    {
    case PROP_TAKE_FOCUS:
      g_value_set_boolean (value, gtk_menu_shell_get_take_focus (menu_shell));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_shell_finalize (GObject *object)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (object);
  GtkMenuShellPrivate *priv = menu_shell->priv;

  if (priv->mnemonic_hash)
    _gtk_mnemonic_hash_free (priv->mnemonic_hash);
  if (priv->key_hash)
    _gtk_key_hash_free (priv->key_hash);

  G_OBJECT_CLASS (gtk_menu_shell_parent_class)->finalize (object);
}


static void
gtk_menu_shell_dispose (GObject *object)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (object);

  g_clear_pointer (&menu_shell->priv->tracker, gtk_menu_tracker_free);
  gtk_menu_shell_deactivate (menu_shell);

  G_OBJECT_CLASS (gtk_menu_shell_parent_class)->dispose (object);
}

/**
 * gtk_menu_shell_append:
 * @menu_shell: a #GtkMenuShell
 * @child: (type Gtk.MenuItem): The #GtkMenuItem to add
 *
 * Adds a new #GtkMenuItem to the end of the menu shell's
 * item list.
 */
void
gtk_menu_shell_append (GtkMenuShell *menu_shell,
                       GtkWidget    *child)
{
  gtk_menu_shell_insert (menu_shell, child, -1);
}

/**
 * gtk_menu_shell_prepend:
 * @menu_shell: a #GtkMenuShell
 * @child: The #GtkMenuItem to add
 *
 * Adds a new #GtkMenuItem to the beginning of the menu shell's
 * item list.
 */
void
gtk_menu_shell_prepend (GtkMenuShell *menu_shell,
                        GtkWidget    *child)
{
  gtk_menu_shell_insert (menu_shell, child, 0);
}

/**
 * gtk_menu_shell_insert:
 * @menu_shell: a #GtkMenuShell
 * @child: The #GtkMenuItem to add
 * @position: The position in the item list where @child
 *     is added. Positions are numbered from 0 to n-1
 *
 * Adds a new #GtkMenuItem to the menu shell’s item list
 * at the position indicated by @position.
 */
void
gtk_menu_shell_insert (GtkMenuShell *menu_shell,
                       GtkWidget    *child,
                       gint          position)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (GTK_IS_MENU_ITEM (child));

  g_signal_emit (menu_shell, menu_shell_signals[INSERT], 0, child, position);
}

/**
 * gtk_menu_shell_deactivate:
 * @menu_shell: a #GtkMenuShell
 *
 * Deactivates the menu shell.
 *
 * Typically this results in the menu shell being erased
 * from the screen.
 */
void
gtk_menu_shell_deactivate (GtkMenuShell *menu_shell)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  if (menu_shell->priv->active)
    g_signal_emit (menu_shell, menu_shell_signals[DEACTIVATE], 0);
}

static void
gtk_menu_shell_activate (GtkMenuShell *menu_shell)
{
  GtkMenuShellPrivate *priv = menu_shell->priv;

  if (!priv->active)
    {
      GdkDevice *device;

      device = gtk_get_current_event_device ();

      _gtk_menu_shell_set_grab_device (menu_shell, device);
      gtk_grab_add (GTK_WIDGET (menu_shell));

      priv->have_grab = TRUE;
      priv->active = TRUE;
    }
}

static void
gtk_menu_shell_deactivate_and_emit_done (GtkMenuShell *menu_shell)
{
  gtk_menu_shell_deactivate (menu_shell);
  g_signal_emit (menu_shell, menu_shell_signals[SELECTION_DONE], 0);
}

static GtkMenuShell *
gtk_menu_shell_get_toplevel_shell (GtkMenuShell *menu_shell)
{
  while (menu_shell->priv->parent_menu_shell)
    menu_shell = GTK_MENU_SHELL (menu_shell->priv->parent_menu_shell);

  return menu_shell;
}

static void
click_stopped (GtkGestureClick *gesture,
                     GtkMenuShell         *menu_shell)
{
  GtkMenuShellPrivate *priv = menu_shell->priv;
  GdkSurface *surface;
  GdkEvent *event;

  event = gtk_get_current_event ();
  if (!event)
    return;

  if (gdk_event_get_grab_surface (event, &surface))
    {
      if (priv->have_xgrab && surface == NULL)
        {
          /* Unset the active menu item so gtk_menu_popdown() doesn't see it. */
          gtk_menu_shell_deselect (menu_shell);
          gtk_menu_shell_deactivate_and_emit_done (menu_shell);
        }
    }

  g_object_unref (event);
}

static void
click_pressed (GtkGestureClick *gesture,
               gint             n_press,
               gdouble          x,
               gdouble          y,
               GtkMenuShell    *menu_shell)
{
  GtkMenuShellPrivate *priv = menu_shell->priv;
  GtkWidget *menu_item;
  GdkEvent *event;
  GtkMenuShell *item_shell;

  event = gtk_get_current_event ();
  menu_item = gtk_get_event_target_with_type (event, GTK_TYPE_MENU_ITEM);
  if (menu_item)
    item_shell = gtk_menu_item_get_menu_shell (GTK_MENU_ITEM (menu_item));

  if (menu_item &&
      _gtk_menu_item_is_selectable (menu_item) &&
      item_shell == menu_shell)
    {
      if (menu_item != menu_shell->priv->active_menu_item)
        {
          /*  select the menu item *before* activating the shell, so submenus
           *  which might be open are closed the friendly way. If we activate
           *  (and thus grab) this menu shell first, we might get grab_broken
           *  events which will close the entire menu hierarchy. Selecting the
           *  menu item also fixes up the state as if enter_notify() would
           *  have run before (which normally selects the item).
           */
          if (GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement != GTK_TOP_BOTTOM)
            gtk_menu_shell_select_item (menu_shell, menu_item);
        }

      if (GTK_MENU_ITEM (menu_item)->priv->submenu != NULL &&
          !gtk_widget_get_visible (GTK_MENU_ITEM (menu_item)->priv->submenu))
        {
          _gtk_menu_item_popup_submenu (menu_item, FALSE);
          priv->activated_submenu = TRUE;
        }
    }

  if (!priv->active || !priv->button)
    {
      gboolean initially_active = priv->active;
      guint button;
      guint32 time;

      button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
      time = gdk_event_get_time (event);

      priv->button = button;

      if (menu_item)
        {
          if (_gtk_menu_item_is_selectable (menu_item) &&
              item_shell == menu_shell &&
              menu_item != priv->active_menu_item)
            {
              priv->active = TRUE;

              if (GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement == GTK_TOP_BOTTOM)
                {
                  priv->activate_time = time;
                  gtk_menu_shell_select_item (menu_shell, menu_item);
                }
            }
        }
      else if (!initially_active)
        {
          gtk_menu_shell_deactivate (menu_shell);
          gtk_gesture_set_state (GTK_GESTURE (gesture),
                                 GTK_EVENT_SEQUENCE_CLAIMED);
        }
    }

  g_object_unref (event);
}

static void
click_released (GtkGestureClick *gesture,
                gint             n_press,
                gdouble          x,
                gdouble          y,
                GtkMenuShell    *menu_shell)
{
  GtkMenuShellPrivate *priv = menu_shell->priv;
  GtkMenuShell *parent_shell = GTK_MENU_SHELL (priv->parent_menu_shell);
  gboolean activated_submenu = FALSE;
  guint new_button;
  guint32 time;
  GdkEvent *event;

  event = gtk_get_current_event ();
  new_button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  time = gdk_event_get_time (event);

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  if (parent_shell)
    {
      /* If a submenu was just activated, it is its shell which is receiving
       * the button release event. In this case, we must check the parent
       * shell to know about the submenu state.
       */
      activated_submenu = parent_shell->priv->activated_submenu;
      parent_shell->priv->activated_submenu = FALSE;
    }

  if (priv->parent_menu_shell &&
      (time - GTK_MENU_SHELL (priv->parent_menu_shell)->priv->activate_time) < MENU_SHELL_TIMEOUT)
    {
      /* The button-press originated in the parent menu bar and we are
       * a pop-up menu. It was a quick press-and-release so we don't want
       * to activate an item but we leave the popup in place instead.
       * https://bugzilla.gnome.org/show_bug.cgi?id=703069
       */
      GTK_MENU_SHELL (priv->parent_menu_shell)->priv->activate_time = 0;
      return;
    }

  if (priv->active)
    {
      GtkWidget *menu_item;
      guint button = priv->button;

      priv->button = 0;

      if (button && (new_button != button) && priv->parent_menu_shell)
        {
          gtk_menu_shell_deactivate_and_emit_done (gtk_menu_shell_get_toplevel_shell (menu_shell));
          return;
        }

      if ((time - priv->activate_time) <= MENU_SHELL_TIMEOUT)
        {
          /* We only ever want to prevent deactivation on the first
           * press/release. Setting the time to zero is a bit of a
           * hack, since we could be being triggered in the first
           * few fractions of a second after a server time wraparound.
           * the chances of that happening are ~1/10^6, without
           * serious harm if we lose.
           */
          priv->activate_time = 0;
          return;
        }

      menu_item = gtk_get_event_target_with_type (event, GTK_TYPE_MENU_ITEM);

      if (menu_item)
        {
          GtkWidget *submenu = GTK_MENU_ITEM (menu_item)->priv->submenu;
          GtkMenuShell *parent_menu_item_shell = gtk_menu_item_get_menu_shell (GTK_MENU_ITEM (menu_item));

          if (!_gtk_menu_item_is_selectable (GTK_WIDGET (menu_item)))
            return;

          if (submenu == NULL)
            {
              gtk_menu_shell_activate_item (menu_shell, GTK_WIDGET (menu_item), TRUE);
              return;
            }
          else if (GTK_MENU_SHELL (parent_menu_item_shell)->priv->parent_menu_shell &&
                   (activated_submenu ||
                    GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement != GTK_TOP_BOTTOM))
            {
              GTimeVal *popup_time;
              gint64 usec_since_popup = 0;

              popup_time = g_object_get_data (G_OBJECT (menu_shell),
                                              "gtk-menu-exact-popup-time");
              if (popup_time)
                {
                  GTimeVal current_time;

                  g_get_current_time (&current_time);

                  usec_since_popup = ((gint64) current_time.tv_sec * 1000 * 1000 +
                                      (gint64) current_time.tv_usec -
                                      (gint64) popup_time->tv_sec * 1000 * 1000 -
                                      (gint64) popup_time->tv_usec);

                  g_object_set_data (G_OBJECT (menu_shell),
                                     "gtk-menu-exact-popup-time", NULL);
                }

              /* Only close the submenu on click if we opened the
               * menu explicitly (usec_since_popup == 0) or
               * enough time has passed since it was opened by
               * GtkMenuItem's timeout (usec_since_popup > delay).
               */
              if (!activated_submenu &&
                  (usec_since_popup == 0 ||
                   usec_since_popup > MENU_POPDOWN_DELAY * 1000))
                {
                  _gtk_menu_item_popdown_submenu (menu_item);
                }
              else
                {
                  gtk_menu_item_select (GTK_MENU_ITEM (menu_item));
                }

              return;
            }
        }

      gtk_menu_shell_deactivate_and_emit_done (gtk_menu_shell_get_toplevel_shell (menu_shell));
    }

  g_object_unref (event);
}

void
_gtk_menu_shell_set_keyboard_mode (GtkMenuShell *menu_shell,
                                   gboolean      keyboard_mode)
{
  menu_shell->priv->keyboard_mode = keyboard_mode;
}

gboolean
_gtk_menu_shell_get_keyboard_mode (GtkMenuShell *menu_shell)
{
  return menu_shell->priv->keyboard_mode;
}

void
_gtk_menu_shell_update_mnemonics (GtkMenuShell *menu_shell)
{
  GtkMenuShell *target;
  gboolean found;
  gboolean mnemonics_visible;

  target = menu_shell;
  found = FALSE;
  while (target)
    {
      GtkMenuShellPrivate *priv = target->priv;
      GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (target)));

      /* The idea with keyboard mode is that once you start using
       * the keyboard to navigate the menus, we show mnemonics
       * until the menu navigation is over. To that end, we spread
       * the keyboard mode upwards in the menu hierarchy here.
       * Also see gtk_menu_popup, where we inherit it downwards.
       */
      if (menu_shell->priv->keyboard_mode)
        target->priv->keyboard_mode = TRUE;

      /* While navigating menus, the first parent menu with an active
       * item is the one where mnemonics are effective, as can be seen
       * in gtk_menu_shell_key_press below.
       * We also show mnemonics in context menus. The grab condition is
       * necessary to ensure we remove underlines from menu bars when
       * dismissing menus.
       */
      mnemonics_visible = target->priv->keyboard_mode &&
                          (((target->priv->active_menu_item || priv->in_unselectable_item) && !found) ||
                           (target == menu_shell &&
                            !target->priv->parent_menu_shell &&
                            gtk_widget_has_grab (GTK_WIDGET (target))));

      /* While menus are up, only show underlines inside the menubar,
       * not in the entire window.
       */
      if (GTK_IS_MENU_BAR (target))
        {
          gtk_window_set_mnemonics_visible (GTK_WINDOW (toplevel), FALSE);
          _gtk_label_mnemonics_visible_apply_recursively (GTK_WIDGET (target),
                                                          mnemonics_visible);
        }
      else
        gtk_window_set_mnemonics_visible (GTK_WINDOW (toplevel), mnemonics_visible);

      if (target->priv->active_menu_item || priv->in_unselectable_item)
        found = TRUE;

      target = GTK_MENU_SHELL (target->priv->parent_menu_shell);
    }
}

static gboolean
gtk_menu_shell_key_press (GtkEventControllerKey *key,
                          guint                  keyval,
                          guint                  keycode,
                          GdkModifierType        modifiers,
                          GtkWidget             *widget)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (widget);
  GtkMenuShellPrivate *priv = menu_shell->priv;

  priv->keyboard_mode = TRUE;

  if (!(priv->active_menu_item || priv->in_unselectable_item) &&
      priv->parent_menu_shell)
    return gtk_event_controller_key_forward (key, priv->parent_menu_shell);

  return gtk_menu_shell_activate_mnemonic (menu_shell, keycode, modifiers,
                                           gtk_event_controller_key_get_group (key));
}

static void
gtk_menu_shell_root (GtkWidget  *widget)
{
  GTK_WIDGET_CLASS (gtk_menu_shell_parent_class)->root (widget);

  gtk_menu_shell_reset_key_hash (GTK_MENU_SHELL (widget));
}

static void
gtk_menu_shell_add (GtkContainer *container,
                    GtkWidget    *widget)
{
  gtk_menu_shell_append (GTK_MENU_SHELL (container), widget);
}

static void
gtk_menu_shell_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (container);
  GtkMenuShellPrivate *priv = menu_shell->priv;

  if (widget == priv->active_menu_item)
    {
      g_signal_emit_by_name (priv->active_menu_item, "deselect");
      priv->active_menu_item = NULL;
    }
}

static void
gtk_real_menu_shell_deactivate (GtkMenuShell *menu_shell)
{
  GtkMenuShellPrivate *priv = menu_shell->priv;

  if (priv->active)
    {
      priv->button = 0;
      priv->active = FALSE;
      priv->activate_time = 0;

      if (priv->active_menu_item)
        {
          gtk_menu_item_deselect (GTK_MENU_ITEM (priv->active_menu_item));
          priv->active_menu_item = NULL;
        }

      if (priv->have_grab)
        {
          priv->have_grab = FALSE;
          gtk_grab_remove (GTK_WIDGET (menu_shell));
        }
      if (priv->have_xgrab)
        {
          gdk_seat_ungrab (gdk_device_get_seat (priv->grab_pointer));
          priv->have_xgrab = FALSE;
        }

      priv->keyboard_mode = FALSE;
      _gtk_menu_shell_set_grab_device (menu_shell, NULL);

      _gtk_menu_shell_update_mnemonics (menu_shell);
    }
}

/* Handlers for action signals */

/**
 * gtk_menu_shell_select_item:
 * @menu_shell: a #GtkMenuShell
 * @menu_item: The #GtkMenuItem to select
 *
 * Selects the menu item from the menu shell.
 */
void
gtk_menu_shell_select_item (GtkMenuShell *menu_shell,
                            GtkWidget    *menu_item)
{
  GtkMenuShellPrivate *priv;
  GtkMenuShellClass *class;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  priv = menu_shell->priv;
  class = GTK_MENU_SHELL_GET_CLASS (menu_shell);

  if (class->select_item &&
      !(priv->active &&
        priv->active_menu_item == menu_item))
    class->select_item (menu_shell, menu_item);
}

void _gtk_menu_item_set_placement (GtkMenuItem         *menu_item,
                                   GtkSubmenuPlacement  placement);

static void
gtk_menu_shell_real_select_item (GtkMenuShell *menu_shell,
                                 GtkWidget    *menu_item)
{
  GtkMenuShellPrivate *priv = menu_shell->priv;

  if (priv->active_menu_item)
    {
      gtk_menu_item_deselect (GTK_MENU_ITEM (priv->active_menu_item));
      priv->active_menu_item = NULL;
    }

  if (!_gtk_menu_item_is_selectable (menu_item))
    {
      priv->in_unselectable_item = TRUE;
      _gtk_menu_shell_update_mnemonics (menu_shell);
      return;
    }

  gtk_menu_shell_activate (menu_shell);

  priv->active_menu_item = menu_item;
  _gtk_menu_item_set_placement (GTK_MENU_ITEM (priv->active_menu_item),
                                GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement);
  gtk_menu_item_select (GTK_MENU_ITEM (priv->active_menu_item));

  _gtk_menu_shell_update_mnemonics (menu_shell);

  /* This allows the bizarre radio buttons-with-submenus-display-history
   * behavior
   */
  if (GTK_MENU_ITEM (priv->active_menu_item)->priv->submenu)
    gtk_widget_activate (priv->active_menu_item);
}

/**
 * gtk_menu_shell_deselect:
 * @menu_shell: a #GtkMenuShell
 *
 * Deselects the currently selected item from the menu shell,
 * if any.
 */
void
gtk_menu_shell_deselect (GtkMenuShell *menu_shell)
{
  GtkMenuShellPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  priv = menu_shell->priv;

  if (priv->active_menu_item)
    {
      gtk_menu_item_deselect (GTK_MENU_ITEM (priv->active_menu_item));
      priv->active_menu_item = NULL;
      _gtk_menu_shell_update_mnemonics (menu_shell);
    }
}

/**
 * gtk_menu_shell_activate_item:
 * @menu_shell: a #GtkMenuShell
 * @menu_item: the #GtkMenuItem to activate
 * @force_deactivate: if %TRUE, force the deactivation of the
 *     menu shell after the menu item is activated
 *
 * Activates the menu item within the menu shell.
 */
void
gtk_menu_shell_activate_item (GtkMenuShell *menu_shell,
                              GtkWidget    *menu_item,
                              gboolean      force_deactivate)
{
  GSList *slist, *shells = NULL;
  gboolean deactivate = force_deactivate;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  if (!deactivate)
    deactivate = GTK_MENU_ITEM_GET_CLASS (menu_item)->hide_on_activate;

  g_object_ref (menu_shell);
  g_object_ref (menu_item);

  if (deactivate)
    {
      GtkMenuShell *parent_menu_shell = menu_shell;

      do
        {
          parent_menu_shell->priv->selection_done_coming_soon = TRUE;

          g_object_ref (parent_menu_shell);
          shells = g_slist_prepend (shells, parent_menu_shell);
          parent_menu_shell = (GtkMenuShell*) parent_menu_shell->priv->parent_menu_shell;
        }
      while (parent_menu_shell);
      shells = g_slist_reverse (shells);

      gtk_menu_shell_deactivate (menu_shell);

      /* Flush the x-queue, so any grabs are removed and
       * the menu is actually taken down
       */
      gdk_display_sync (gtk_widget_get_display (menu_item));
    }

  gtk_widget_activate (menu_item);

  for (slist = shells; slist; slist = slist->next)
    {
      GtkMenuShell *parent_menu_shell = slist->data;

      g_signal_emit (parent_menu_shell, menu_shell_signals[SELECTION_DONE], 0);
      parent_menu_shell->priv->selection_done_coming_soon = FALSE;
      g_object_unref (slist->data);
    }
  g_slist_free (shells);

  g_object_unref (menu_shell);
  g_object_unref (menu_item);
}

GList *
gtk_menu_shell_get_items (GtkMenuShell *menu_shell)
{
  return GTK_MENU_SHELL_GET_CLASS (menu_shell)->get_items (menu_shell);
}

/* Distance should be +/- 1 */
static gboolean
gtk_menu_shell_real_move_selected (GtkMenuShell  *menu_shell,
                                   gint           distance)
{
  GtkMenuShellPrivate *priv = menu_shell->priv;

  if (priv->active_menu_item)
    {
      GList *children = gtk_menu_shell_get_items (menu_shell);
      GList *node = g_list_find (children, priv->active_menu_item);
      GList *start_node = node;

      if (distance > 0)
        {
          node = node->next;
          while (node != start_node &&
                 (!node || !_gtk_menu_item_is_selectable (node->data)))
            {
              if (node)
                node = node->next;
              else
                node = children;
            }
        }
      else
        {
          node = node->prev;
          while (node != start_node &&
                 (!node || !_gtk_menu_item_is_selectable (node->data)))
            {
              if (node)
                node = node->prev;
              else
                node = g_list_last (children);
            }
        }
      
      if (node)
        gtk_menu_shell_select_item (menu_shell, node->data);

      g_list_free (children);
    }

  return TRUE;
}

/* Distance should be +/- 1 */
static void
gtk_menu_shell_move_selected (GtkMenuShell  *menu_shell,
                              gint           distance)
{
  gboolean handled = FALSE;

  g_signal_emit (menu_shell, menu_shell_signals[MOVE_SELECTED], 0,
                 distance, &handled);
}

/**
 * gtk_menu_shell_select_first:
 * @menu_shell: a #GtkMenuShell
 * @search_sensitive: if %TRUE, search for the first selectable
 *                    menu item, otherwise select nothing if
 *                    the first item isn’t sensitive. This
 *                    should be %FALSE if the menu is being
 *                    popped up initially.
 *
 * Select the first visible or selectable child of the menu shell.
 */
void
gtk_menu_shell_select_first (GtkMenuShell *menu_shell,
                             gboolean      search_sensitive)
{
  GList *children;
  GList *tmp_list;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  children = gtk_menu_shell_get_items (menu_shell);
  tmp_list = children;
  while (tmp_list)
    {
      GtkWidget *child = tmp_list->data;

      if ((!search_sensitive && gtk_widget_get_visible (child)) ||
          _gtk_menu_item_is_selectable (child))
        {
          gtk_menu_shell_select_item (menu_shell, child);
          break;
        }

      tmp_list = tmp_list->next;
    }

  g_list_free (children);
}

void
_gtk_menu_shell_select_last (GtkMenuShell *menu_shell,
                             gboolean      search_sensitive)
{
  GList *tmp_list;
  GList *children;

  children = gtk_menu_shell_get_items (menu_shell);
  tmp_list = g_list_last (children);
  while (tmp_list)
    {
      GtkWidget *child = tmp_list->data;

      if ((!search_sensitive && gtk_widget_get_visible (child)) ||
          _gtk_menu_item_is_selectable (child))
        {
          gtk_menu_shell_select_item (menu_shell, child);
          break;
        }

      tmp_list = tmp_list->prev;
    }

  g_list_free (children);
}

static gboolean
gtk_menu_shell_select_submenu_first (GtkMenuShell *menu_shell)
{
  GtkMenuShellPrivate *priv = menu_shell->priv;
  GtkMenuItem *menu_item;

  if (priv->active_menu_item == NULL)
    return FALSE;

  menu_item = GTK_MENU_ITEM (priv->active_menu_item);

  if (menu_item->priv->submenu)
    {
      _gtk_menu_item_popup_submenu (GTK_WIDGET (menu_item), FALSE);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (menu_item->priv->submenu), TRUE);
      if (GTK_MENU_SHELL (menu_item->priv->submenu)->priv->active_menu_item)
        return TRUE;
    }

  return FALSE;
}

/* Moves the current menu item in direction 'direction':
 *
 * - GTK_MENU_DIR_PARENT: To the parent menu shell
 * - GTK_MENU_DIR_CHILD: To the child menu shell (if this item has a submenu).
 * - GTK_MENU_DIR_NEXT/PREV: To the next or previous item in this menu.
 *
 * As a bit of a hack to get movement between menus and
 * menubars working, if submenu_placement is different for
 * the menu and its MenuShell then the following apply:
 *
 * - For “parent” the current menu is not just moved to
 *   the parent, but moved to the previous entry in the parent
 * - For 'child', if there is no child, then current is
 *   moved to the next item in the parent.
 *
 * Note that the above explanation of ::move_current was written
 * before menus and menubars had support for RTL flipping and
 * different packing directions, and therefore only applies for
 * when text direction and packing direction are both left-to-right.
 */
static void
gtk_real_menu_shell_move_current (GtkMenuShell         *menu_shell,
                                  GtkMenuDirectionType  direction)
{
  GtkMenuShellPrivate *priv = menu_shell->priv;
  GtkMenuShell *parent_menu_shell = NULL;
  gboolean had_selection;

  priv->in_unselectable_item = FALSE;

  had_selection = priv->active_menu_item != NULL;

  if (priv->parent_menu_shell)
    parent_menu_shell = GTK_MENU_SHELL (priv->parent_menu_shell);

  switch (direction)
    {
    case GTK_MENU_DIR_PARENT:
      if (parent_menu_shell)
        {
          if (GTK_MENU_SHELL_GET_CLASS (parent_menu_shell)->submenu_placement ==
              GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement)
            gtk_menu_shell_deselect (menu_shell);
          else
            {
              gtk_menu_shell_move_selected (parent_menu_shell, -1);
              gtk_menu_shell_select_submenu_first (parent_menu_shell);
            }
        }
      /* If there is no parent and the submenu is in the opposite direction
       * to the menu, then make the PARENT direction wrap around to
       * the bottom of the submenu.
       */
      else if (priv->active_menu_item &&
               _gtk_menu_item_is_selectable (priv->active_menu_item) &&
               GTK_MENU_ITEM (priv->active_menu_item)->priv->submenu)
        {
          GtkMenuShell *submenu = GTK_MENU_SHELL (GTK_MENU_ITEM (priv->active_menu_item)->priv->submenu);

          if (GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement !=
              GTK_MENU_SHELL_GET_CLASS (submenu)->submenu_placement)
            _gtk_menu_shell_select_last (submenu, TRUE);
        }
      break;

    case GTK_MENU_DIR_CHILD:
      if (priv->active_menu_item &&
          _gtk_menu_item_is_selectable (priv->active_menu_item) &&
          GTK_MENU_ITEM (priv->active_menu_item)->priv->submenu)
        {
          if (gtk_menu_shell_select_submenu_first (menu_shell))
            break;
        }

      /* Try to find a menu running the opposite direction */
      while (parent_menu_shell &&
             (GTK_MENU_SHELL_GET_CLASS (parent_menu_shell)->submenu_placement ==
              GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement))
        {
          parent_menu_shell = GTK_MENU_SHELL (parent_menu_shell->priv->parent_menu_shell);
        }

      if (parent_menu_shell)
        {
          gtk_menu_shell_move_selected (parent_menu_shell, 1);

          gtk_menu_shell_select_submenu_first (parent_menu_shell);
        }
      break;

    case GTK_MENU_DIR_PREV:
      gtk_menu_shell_move_selected (menu_shell, -1);
      if (!had_selection && !priv->active_menu_item)
        _gtk_menu_shell_select_last (menu_shell, TRUE);
      break;

    case GTK_MENU_DIR_NEXT:
      gtk_menu_shell_move_selected (menu_shell, 1);
      if (!had_selection && !priv->active_menu_item)
        gtk_menu_shell_select_first (menu_shell, TRUE);
      break;

    default:
      break;
    }
}

/* Activate the current item. If 'force_hide' is true, hide
 * the current menu item always. Otherwise, only hide
 * it if menu_item->klass->hide_on_activate is true.
 */
static void
gtk_real_menu_shell_activate_current (GtkMenuShell *menu_shell,
                                      gboolean      force_hide)
{
  GtkMenuShellPrivate *priv = menu_shell->priv;

  if (priv->active_menu_item &&
      _gtk_menu_item_is_selectable (priv->active_menu_item))
  {
    if (GTK_MENU_ITEM (priv->active_menu_item)->priv->submenu == NULL)
      gtk_menu_shell_activate_item (menu_shell,
                                    priv->active_menu_item,
                                    force_hide);
    else
      gtk_menu_shell_select_submenu_first (menu_shell);
  }
}

static void
gtk_real_menu_shell_cancel (GtkMenuShell *menu_shell)
{
  /* Unset the active menu item so gtk_menu_popdown() doesn't see it. */
  gtk_menu_shell_deselect (menu_shell);
  gtk_menu_shell_deactivate (menu_shell);
  g_signal_emit (menu_shell, menu_shell_signals[SELECTION_DONE], 0);
}

static void
gtk_real_menu_shell_cycle_focus (GtkMenuShell     *menu_shell,
                                 GtkDirectionType  dir)
{
  while (menu_shell && !GTK_IS_MENU_BAR (menu_shell))
    {
      if (menu_shell->priv->parent_menu_shell)
        menu_shell = GTK_MENU_SHELL (menu_shell->priv->parent_menu_shell);
      else
        menu_shell = NULL;
    }

  if (menu_shell)
    _gtk_menu_bar_cycle_focus (GTK_MENU_BAR (menu_shell), dir);
}

gint
_gtk_menu_shell_get_popup_delay (GtkMenuShell *menu_shell)
{
  GtkMenuShellClass *klass = GTK_MENU_SHELL_GET_CLASS (menu_shell);
  
  if (klass->get_popup_delay)
    {
      return klass->get_popup_delay (menu_shell);
    }
  else
    {
      return MENU_POPUP_DELAY;
    }
}

/**
 * gtk_menu_shell_cancel:
 * @menu_shell: a #GtkMenuShell
 *
 * Cancels the selection within the menu shell.
 */
void
gtk_menu_shell_cancel (GtkMenuShell *menu_shell)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  g_signal_emit (menu_shell, menu_shell_signals[CANCEL], 0);
}

static GtkMnemonicHash *
gtk_menu_shell_get_mnemonic_hash (GtkMenuShell *menu_shell,
                                  gboolean      create)
{
  GtkMenuShellPrivate *priv = menu_shell->priv;

  if (!priv->mnemonic_hash && create)
    priv->mnemonic_hash = _gtk_mnemonic_hash_new ();
  
  return priv->mnemonic_hash;
}

static void
menu_shell_add_mnemonic_foreach (guint     keyval,
                                 GSList   *targets,
                                 gpointer  data)
{
  GtkKeyHash *key_hash = data;

  _gtk_key_hash_add_entry (key_hash, keyval, 0, GUINT_TO_POINTER (keyval));
}

static GtkKeyHash *
gtk_menu_shell_get_key_hash (GtkMenuShell *menu_shell,
                             gboolean      create)
{
  GtkMenuShellPrivate *priv = menu_shell->priv;
  GtkWidget *widget = GTK_WIDGET (menu_shell);

  if (!priv->key_hash && create)
    {
      GtkMnemonicHash *mnemonic_hash = gtk_menu_shell_get_mnemonic_hash (menu_shell, FALSE);
      GdkKeymap *keymap = gdk_display_get_keymap (gtk_widget_get_display (widget));

      if (!mnemonic_hash)
        return NULL;

      priv->key_hash = _gtk_key_hash_new (keymap, NULL);

      _gtk_mnemonic_hash_foreach (mnemonic_hash,
                                  menu_shell_add_mnemonic_foreach,
                                  priv->key_hash);
    }

  return priv->key_hash;
}

static void
gtk_menu_shell_reset_key_hash (GtkMenuShell *menu_shell)
{
  GtkMenuShellPrivate *priv = menu_shell->priv;

  if (priv->key_hash)
    {
      _gtk_key_hash_free (priv->key_hash);
      priv->key_hash = NULL;
    }
}

static gboolean
gtk_menu_shell_activate_mnemonic (GtkMenuShell    *menu_shell,
                                  guint            keycode,
                                  GdkModifierType  state,
                                  guint            group)
{
  GtkMnemonicHash *mnemonic_hash;
  GtkKeyHash *key_hash;
  GSList *entries;
  gboolean result = FALSE;

  mnemonic_hash = gtk_menu_shell_get_mnemonic_hash (menu_shell, FALSE);
  if (!mnemonic_hash)
    return FALSE;

  key_hash = gtk_menu_shell_get_key_hash (menu_shell, TRUE);
  if (!key_hash)
    return FALSE;

  entries = _gtk_key_hash_lookup (key_hash,
                                  keycode,
                                  state,
                                  gtk_accelerator_get_default_mod_mask (),
                                  group);

  if (entries)
    {
      result = _gtk_mnemonic_hash_activate (mnemonic_hash,
                                            GPOINTER_TO_UINT (entries->data));
      g_slist_free (entries);
    }

  return result;
}

void
_gtk_menu_shell_add_mnemonic (GtkMenuShell *menu_shell,
                              guint         keyval,
                              GtkWidget    *target)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (GTK_IS_WIDGET (target));

  _gtk_mnemonic_hash_add (gtk_menu_shell_get_mnemonic_hash (menu_shell, TRUE),
                          keyval, target);
  gtk_menu_shell_reset_key_hash (menu_shell);
}

void
_gtk_menu_shell_remove_mnemonic (GtkMenuShell *menu_shell,
                                 guint         keyval,
                                 GtkWidget    *target)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (GTK_IS_WIDGET (target));

  _gtk_mnemonic_hash_remove (gtk_menu_shell_get_mnemonic_hash (menu_shell, TRUE),
                             keyval, target);
  gtk_menu_shell_reset_key_hash (menu_shell);
}

void
_gtk_menu_shell_set_grab_device (GtkMenuShell *menu_shell,
                                 GdkDevice    *device)
{
  GtkMenuShellPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (device == NULL || GDK_IS_DEVICE (device));

  priv = menu_shell->priv;

  if (!device)
    priv->grab_pointer = NULL;
  else if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    priv->grab_pointer = gdk_device_get_associated_device (device);
  else
    priv->grab_pointer = device;
}

GdkDevice *
_gtk_menu_shell_get_grab_device (GtkMenuShell *menu_shell)
{
  g_return_val_if_fail (GTK_IS_MENU_SHELL (menu_shell), NULL);

  return menu_shell->priv->grab_pointer;
}

/**
 * gtk_menu_shell_get_take_focus:
 * @menu_shell: a #GtkMenuShell
 *
 * Returns %TRUE if the menu shell will take the keyboard focus on popup.
 *
 * Returns: %TRUE if the menu shell will take the keyboard focus on popup.
 */
gboolean
gtk_menu_shell_get_take_focus (GtkMenuShell *menu_shell)
{
  g_return_val_if_fail (GTK_IS_MENU_SHELL (menu_shell), FALSE);

  return menu_shell->priv->take_focus;
}

/**
 * gtk_menu_shell_set_take_focus:
 * @menu_shell: a #GtkMenuShell
 * @take_focus: %TRUE if the menu shell should take the keyboard
 *     focus on popup
 *
 * If @take_focus is %TRUE (the default) the menu shell will take
 * the keyboard focus so that it will receive all keyboard events
 * which is needed to enable keyboard navigation in menus.
 *
 * Setting @take_focus to %FALSE is useful only for special applications
 * like virtual keyboard implementations which should not take keyboard
 * focus.
 *
 * The @take_focus state of a menu or menu bar is automatically
 * propagated to submenus whenever a submenu is popped up, so you
 * don’t have to worry about recursively setting it for your entire
 * menu hierarchy. Only when programmatically picking a submenu and
 * popping it up manually, the @take_focus property of the submenu
 * needs to be set explicitly.
 *
 * Note that setting it to %FALSE has side-effects:
 *
 * If the focus is in some other app, it keeps the focus and keynav in
 * the menu doesn’t work. Consequently, keynav on the menu will only
 * work if the focus is on some toplevel owned by the onscreen keyboard.
 *
 * To avoid confusing the user, menus with @take_focus set to %FALSE
 * should not display mnemonics or accelerators, since it cannot be
 * guaranteed that they will work.
 */
void
gtk_menu_shell_set_take_focus (GtkMenuShell *menu_shell,
                               gboolean      take_focus)
{
  GtkMenuShellPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  priv = menu_shell->priv;

  take_focus = !!take_focus;
  if (priv->take_focus != take_focus)
    {
      priv->take_focus = take_focus;
      g_object_notify (G_OBJECT (menu_shell), "take-focus");
    }
}

/**
 * gtk_menu_shell_get_selected_item:
 * @menu_shell: a #GtkMenuShell
 *
 * Gets the currently selected item.
 *
 * Returns: (transfer none): the currently selected item
 */
GtkWidget *
gtk_menu_shell_get_selected_item (GtkMenuShell *menu_shell)
{
  g_return_val_if_fail (GTK_IS_MENU_SHELL (menu_shell), NULL);

  return menu_shell->priv->active_menu_item;
}

/**
 * gtk_menu_shell_get_parent_shell:
 * @menu_shell: a #GtkMenuShell
 *
 * Gets the parent menu shell.
 *
 * The parent menu shell of a submenu is the #GtkMenu or #GtkMenuBar
 * from which it was opened up.
 *
 * Returns: (transfer none): the parent #GtkMenuShell
 */
GtkWidget *
gtk_menu_shell_get_parent_shell (GtkMenuShell *menu_shell)
{
  g_return_val_if_fail (GTK_IS_MENU_SHELL (menu_shell), NULL);

  return menu_shell->priv->parent_menu_shell;
}

static void
gtk_menu_shell_item_activate (GtkMenuItem *menuitem,
                              gpointer     user_data)
{
  GtkMenuTrackerItem *item = user_data;

  gtk_menu_tracker_item_activated (item);
}

static void
gtk_menu_shell_submenu_shown (GtkWidget *submenu,
                              gpointer   user_data)
{
  GtkMenuTrackerItem *item = user_data;

  gtk_menu_tracker_item_request_submenu_shown (item, TRUE);
}

static void
gtk_menu_shell_submenu_hidden (GtkWidget *submenu,
                               gpointer   user_data)
{
  GtkMenuTrackerItem *item = user_data;

  if (!GTK_MENU_SHELL (submenu)->priv->selection_done_coming_soon)
    gtk_menu_tracker_item_request_submenu_shown (item, FALSE);
}

static void
gtk_menu_shell_submenu_selection_done (GtkWidget *submenu,
                                       gpointer   user_data)
{
  GtkMenuTrackerItem *item = user_data;

  if (GTK_MENU_SHELL (submenu)->priv->selection_done_coming_soon)
    gtk_menu_tracker_item_request_submenu_shown (item, FALSE);
}

static void
gtk_menu_shell_tracker_remove_func (gint     position,
                                    gpointer user_data)
{
  GtkMenuShell *menu_shell = user_data;
  GList *children;
  GtkWidget *child;

  children = gtk_menu_shell_get_items (menu_shell);

  child = g_list_nth_data (children, position);
  /* We use destroy here because in the case of an item with a submenu,
   * the attached-to from the submenu holds a ref on the item and a
   * simple gtk_container_remove() isn't good enough to break that.
   */
  gtk_widget_destroy (child);

  g_list_free (children);
}

static void
gtk_menu_shell_tracker_insert_func (GtkMenuTrackerItem *item,
                                    gint                position,
                                    gpointer            user_data)
{
  GtkMenuShell *menu_shell = user_data;
  GtkWidget *widget;

  if (gtk_menu_tracker_item_get_is_separator (item))
    {
      const gchar *label;

      widget = gtk_separator_menu_item_new ();

      /* For separators, we may have a section heading, so check the
       * "label" property.
       *
       * Note: we only do this once, and we only do it if the label is
       * non-NULL because even setting a NULL label on the separator
       * will be enough to create a GtkLabel and add it, changing the
       * appearance in the process.
       */

      label = gtk_menu_tracker_item_get_label (item);
      if (label)
        gtk_menu_item_set_label (GTK_MENU_ITEM (widget), label);
    }
  else if (gtk_menu_tracker_item_get_has_link (item, G_MENU_LINK_SUBMENU))
    {
      GtkMenuShell *submenu;

      widget = gtk_model_menu_item_new ();
      g_object_bind_property (item, "label", widget, "text", G_BINDING_SYNC_CREATE);

      submenu = GTK_MENU_SHELL (gtk_menu_new ());
      gtk_widget_hide (GTK_WIDGET (submenu));

      /* We recurse directly here: we could use an idle instead to
       * prevent arbitrary recursion depth.  We could also do it
       * lazy...
       */
      submenu->priv->tracker = gtk_menu_tracker_new_for_item_link (item,
                                                                   G_MENU_LINK_SUBMENU, TRUE, FALSE,
                                                                   gtk_menu_shell_tracker_insert_func,
                                                                   gtk_menu_shell_tracker_remove_func,
                                                                   submenu);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget), GTK_WIDGET (submenu));

      if (gtk_menu_tracker_item_get_should_request_show (item))
        {
          /* We don't request show in the strictest sense of the
           * word: we just notify when we are showing and don't
           * bother waiting for the reply.
           *
           * This could be fixed one day, but it would be slightly
           * complicated and would have a strange interaction with
           * the submenu pop-up delay.
           *
           * Note: 'item' is already kept alive from above.
           */
          g_signal_connect (submenu, "show", G_CALLBACK (gtk_menu_shell_submenu_shown), item);
          g_signal_connect (submenu, "hide", G_CALLBACK (gtk_menu_shell_submenu_hidden), item);
          g_signal_connect (submenu, "selection-done", G_CALLBACK (gtk_menu_shell_submenu_selection_done), item);
        }
    }
  else
    {
      widget = gtk_model_menu_item_new ();

      /* We bind to "text" instead of "label" because GtkModelMenuItem
       * uses this property (along with "icon") to control its child
       * widget.  Once this is merged into GtkMenuItem we can go back to
       * using "label".
       */
      g_object_bind_property (item, "label", widget, "text", G_BINDING_SYNC_CREATE);
      g_object_bind_property (item, "icon", widget, "icon", G_BINDING_SYNC_CREATE);
      g_object_bind_property (item, "sensitive", widget, "sensitive", G_BINDING_SYNC_CREATE);
      g_object_bind_property (item, "role", widget, "action-role", G_BINDING_SYNC_CREATE);
      g_object_bind_property (item, "toggled", widget, "toggled", G_BINDING_SYNC_CREATE);
      g_object_bind_property (item, "accel", widget, "accel", G_BINDING_SYNC_CREATE);

      g_signal_connect (widget, "activate", G_CALLBACK (gtk_menu_shell_item_activate), item);
      gtk_widget_show (widget);
    }

  /* TODO: drop this when we have bindings that ref the source */
  g_object_set_data_full (G_OBJECT (widget), "GtkMenuTrackerItem", g_object_ref (item), g_object_unref);

  gtk_menu_shell_insert (menu_shell, widget, position);
}

/**
 * gtk_menu_shell_bind_model:
 * @menu_shell: a #GtkMenuShell
 * @model: (allow-none): the #GMenuModel to bind to or %NULL to remove
 *   binding
 * @action_namespace: (allow-none): the namespace for actions in @model
 * @with_separators: %TRUE if toplevel items in @shell should have
 *   separators between them
 *
 * Establishes a binding between a #GtkMenuShell and a #GMenuModel.
 *
 * The contents of @shell are removed and then refilled with menu items
 * according to @model.  When @model changes, @shell is updated.
 * Calling this function twice on @shell with different @model will
 * cause the first binding to be replaced with a binding to the new
 * model. If @model is %NULL then any previous binding is undone and
 * all children are removed.
 *
 * @with_separators determines if toplevel items (eg: sections) have
 * separators inserted between them.  This is typically desired for
 * menus but doesn’t make sense for menubars.
 *
 * If @action_namespace is non-%NULL then the effect is as if all
 * actions mentioned in the @model have their names prefixed with the
 * namespace, plus a dot.  For example, if the action “quit” is
 * mentioned and @action_namespace is “app” then the effective action
 * name is “app.quit”.
 *
 * This function uses #GtkActionable to define the action name and
 * target values on the created menu items.  If you want to use an
 * action group other than “app” and “win”, or if you want to use a
 * #GtkMenuShell outside of a #GtkApplicationWindow, then you will need
 * to attach your own action group to the widget hierarchy using
 * gtk_widget_insert_action_group().  As an example, if you created a
 * group with a “quit” action and inserted it with the name “mygroup”
 * then you would use the action name “mygroup.quit” in your
 * #GMenuModel.
 *
 * For most cases you are probably better off using
 * gtk_menu_new_from_model() or gtk_menu_bar_new_from_model() or just
 * directly passing the #GMenuModel to gtk_application_set_app_menu() or
 * gtk_application_set_menubar().
 */
void
gtk_menu_shell_bind_model (GtkMenuShell *menu_shell,
                           GMenuModel   *model,
                           const gchar  *action_namespace,
                           gboolean      with_separators)
{
  GtkActionMuxer *muxer;
  GList *children, *l;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (model == NULL || G_IS_MENU_MODEL (model));

  muxer = _gtk_widget_get_action_muxer (GTK_WIDGET (menu_shell), TRUE);

  g_clear_pointer (&menu_shell->priv->tracker, gtk_menu_tracker_free);

  children = gtk_menu_shell_get_items (menu_shell);
  for (l = children; l; l = l->next)
    gtk_widget_destroy (GTK_WIDGET (l->data));
  g_list_free (children);

  if (model)
    menu_shell->priv->tracker = gtk_menu_tracker_new (GTK_ACTION_OBSERVABLE (muxer), model,
                                                      with_separators, TRUE, FALSE, action_namespace,
                                                      gtk_menu_shell_tracker_insert_func,
                                                      gtk_menu_shell_tracker_remove_func,
                                                      menu_shell);
}
