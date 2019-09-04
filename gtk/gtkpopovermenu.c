/* GTK - The GIMP Toolkit
 * Copyright © 2014 Red Hat, Inc.
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

#include "config.h"
#include "gtkpopovermenu.h"
#include "gtkpopovermenuprivate.h"

#include "gtkstack.h"
#include "gtkstylecontext.h"
#include "gtkintl.h"
#include "gtkmenusectionboxprivate.h"
#include "gtkmenubutton.h"
#include "gtkactionmuxerprivate.h"
#include "gtkmenutrackerprivate.h"
#include "gtkpopoverprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtkmain.h"
#include "gtktypebuiltins.h"
#include "gtkbindings.h"


/**
 * SECTION:gtkpopovermenu
 * @Short_description: Popovers to use as menus
 * @Title: GtkPopoverMenu
 *
 * GtkPopoverMenu is a subclass of #GtkPopover that treats its
 * children like menus and allows switching between them. It is
 * meant to be used primarily together with #GtkModelButton, but
 * any widget can be used, such as #GtkSpinButton or #GtkScale.
 * In this respect, GtkPopoverMenu is more flexible than popovers
 * that are created from a #GMenuModel with gtk_popover_new_from_model().
 *
 * To add a child as a submenu, use gtk_popover_menu_add_submenu().
 * To let the user open this submenu, add a #GtkModelButton whose
 * #GtkModelButton:menu-name property is set to the name you've given
 * to the submenu.
 *
 * To add a named submenu in a ui file, set the #GtkWidget:name property
 * of the widget that you are adding as a child of the popover menu.
 *
 * By convention, the first child of a submenu should be a #GtkModelButton
 * to switch back to the parent menu. Such a button should use the
 * #GtkModelButton:inverted and #GtkModelButton:centered properties
 * to achieve a title-like appearance and place the submenu indicator
 * at the opposite side. To switch back to the main menu, use "main"
 * as the menu name.
 *
 * # Example
 *
 * |[
 * <object class="GtkPopoverMenu">
 *   <child>
 *     <object class="GtkBox">
 *       <property name="visible">True</property>
 *       <property name="margin">10</property>
 *       <child>
 *         <object class="GtkModelButton">
 *           <property name="visible">True</property>
 *           <property name="action-name">win.frob</property>
 *           <property name="text" translatable="yes">Frob</property>
 *         </object>
 *       </child>
 *       <child>
 *         <object class="GtkModelButton">
 *           <property name="visible">True</property>
 *           <property name="menu-name">more</property>
 *           <property name="text" translatable="yes">More</property>
 *         </object>
 *       </child>
 *     </object>
 *   </child>
 *   <child>
 *     <object class="GtkBox">
 *       <property name="visible">True</property>
 *       <property name="margin">10</property>
 *       <property name="name">more</property>
 *       <child>
 *         <object class="GtkModelButton">
 *           <property name="visible">True</property>
 *           <property name="action-name">win.foo</property>
 *           <property name="text" translatable="yes">Foo</property>
 *         </object>
 *       </child>
 *       <child>
 *         <object class="GtkModelButton">
 *           <property name="visible">True</property>
 *           <property name="action-name">win.bar</property>
 *           <property name="text" translatable="yes">Bar</property>
 *         </object>
 *       </child>
 *     </object>
 *   </child>
 * </object>
 * ]|
 *
 * # CSS Nodes
 *
 * #GtkPopoverMenu is just a subclass of #GtkPopover that adds
 * custom content to it, therefore it has the same CSS nodes.
 * It is one of the cases that add a .menu style class to the
 * popover's contents node.
 */

typedef struct _GtkPopoverMenuClass GtkPopoverMenuClass;

struct _GtkPopoverMenu
{
  GtkPopover parent_instance;

  GtkWidget *active_item;
  GtkWidget *open_submenu;
  GtkWidget *parent_menu;
};

struct _GtkPopoverMenuClass
{
  GtkPopoverClass parent_class;
};

enum {
  PROP_VISIBLE_SUBMENU = 1
};

G_DEFINE_TYPE (GtkPopoverMenu, gtk_popover_menu, GTK_TYPE_POPOVER)

GtkWidget *
gtk_popover_menu_get_parent_menu (GtkPopoverMenu *menu)
{
  return menu->parent_menu;
}

void
gtk_popover_menu_set_parent_menu (GtkPopoverMenu *menu,
                                  GtkWidget      *parent)
{
  menu->parent_menu = parent;
}

GtkWidget *
gtk_popover_menu_get_open_submenu (GtkPopoverMenu *menu)
{
  return menu->open_submenu;
}

void
gtk_popover_menu_set_open_submenu (GtkPopoverMenu *menu,
                                   GtkWidget      *submenu)
{
  if (menu->open_submenu != submenu)
    menu->open_submenu = submenu;
}

GtkWidget *
gtk_popover_menu_get_active_item (GtkPopoverMenu *menu)
{
  return menu->active_item;
}

void
gtk_popover_menu_set_active_item (GtkPopoverMenu *menu,
                                  GtkWidget      *item)
{
  if (menu->active_item != item)
    {
      if (menu->active_item)
        gtk_widget_unset_state_flags (menu->active_item, GTK_STATE_FLAG_SELECTED);

      menu->active_item = item;

      if (menu->active_item)
        {
          gtk_widget_set_state_flags (menu->active_item, GTK_STATE_FLAG_SELECTED, FALSE);
          gtk_widget_grab_focus (menu->active_item);
       }
    }
}

static void
visible_submenu_changed (GObject        *object,
                         GParamSpec     *pspec,
                         GtkPopoverMenu *popover)
{
  g_object_notify (G_OBJECT (popover), "visible-submenu");
}

static void
focus_out (GtkEventController *controller,
           GdkCrossingMode     mode,
           GdkNotifyType       detail,
           GtkPopoverMenu     *menu)
{
  gboolean contains_focus;

  g_object_get (controller, "contains-focus", &contains_focus, NULL);

  if (!contains_focus)
    {
      if (menu->parent_menu &&
          GTK_POPOVER_MENU (menu->parent_menu)->open_submenu == (GtkWidget*)menu)
        GTK_POPOVER_MENU (menu->parent_menu)->open_submenu = NULL;
      gtk_popover_popdown (GTK_POPOVER (menu));
    }
}

static void
gtk_popover_menu_init (GtkPopoverMenu *popover)
{
  GtkWidget *stack;
  GtkStyleContext *style_context;
  GtkEventController *controller;

  stack = gtk_stack_new ();
  gtk_stack_set_vhomogeneous (GTK_STACK (stack), FALSE);
  gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
  gtk_stack_set_interpolate_size (GTK_STACK (stack), TRUE);
  gtk_container_add (GTK_CONTAINER (popover), stack);
  g_signal_connect (stack, "notify::visible-child-name",
                    G_CALLBACK (visible_submenu_changed), popover);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (popover));
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_MENU);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "focus-out", G_CALLBACK (focus_out), popover);
  gtk_widget_add_controller (GTK_WIDGET (popover), controller);
}

static void
gtk_popover_menu_map (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_popover_menu_parent_class)->map (widget);
  gtk_popover_menu_open_submenu (GTK_POPOVER_MENU (widget), "main");
}

static void
gtk_popover_menu_unmap (GtkWidget *widget)
{
  gtk_popover_menu_open_submenu (GTK_POPOVER_MENU (widget), "main");
  GTK_WIDGET_CLASS (gtk_popover_menu_parent_class)->unmap (widget);
}

static void
gtk_popover_menu_add (GtkContainer *container,
                      GtkWidget    *child)
{
  GtkWidget *stack;

  stack = gtk_bin_get_child (GTK_BIN (container));

  if (stack == NULL)
    {
      GTK_CONTAINER_CLASS (gtk_popover_menu_parent_class)->add (container, child);
    }
  else
    {
      const char *name;

      if (gtk_widget_get_name (child))
        name = gtk_widget_get_name (child);
      else if (gtk_stack_get_child_by_name (GTK_STACK (stack), "main"))
        name = "submenu";
      else
        name = "main";

      gtk_popover_menu_add_submenu (GTK_POPOVER_MENU (container), child, name);
    }
}

static void
gtk_popover_menu_remove (GtkContainer *container,
                         GtkWidget    *child)
{
  GtkWidget *stack;

  stack = gtk_bin_get_child (GTK_BIN (container));

  if (child == stack)
    GTK_CONTAINER_CLASS (gtk_popover_menu_parent_class)->remove (container, child);
  else
    gtk_container_remove (GTK_CONTAINER (stack), child);
}

static void
gtk_popover_menu_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkWidget *stack;

  stack = gtk_bin_get_child (GTK_BIN (object));

  switch (property_id)
    {
    case PROP_VISIBLE_SUBMENU:
      g_value_set_string (value, gtk_stack_get_visible_child_name (GTK_STACK (stack)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_popover_menu_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkWidget *stack;

  stack = gtk_bin_get_child (GTK_BIN (object));

  switch (property_id)
    {
    case PROP_VISIBLE_SUBMENU:
      gtk_stack_set_visible_child_name (GTK_STACK (stack), g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static gboolean
gtk_popover_menu_focus (GtkWidget        *widget,
                        GtkDirectionType  direction)
{
  if (gtk_widget_get_first_child (widget) == NULL)
    {
      return FALSE;
    }
  else
    {
      if (GTK_POPOVER_MENU (widget)->open_submenu)
        {
          if (gtk_widget_child_focus (GTK_POPOVER_MENU (widget)->open_submenu, direction))
            return TRUE;
          return FALSE;
        }

      if (gtk_widget_focus_move (widget, direction))
        return TRUE;

      if (direction == GTK_DIR_LEFT || direction == GTK_DIR_RIGHT)
        {
          return FALSE;
        }
      else if (direction == GTK_DIR_UP || direction == GTK_DIR_DOWN)
        {
          GtkWidget *p;

          /* cycle around */
          for (p = gtk_window_get_focus (GTK_WINDOW (gtk_widget_get_root (widget)));
               p != widget;
               p = gtk_widget_get_parent (p))
            {
              gtk_widget_set_focus_child (p, NULL);
            }
          if (gtk_widget_focus_move (widget, direction))
            return TRUE;
       }
    }

  return FALSE;
}


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

static void
gtk_popover_menu_class_init (GtkPopoverMenuClass *klass)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set;

  object_class->set_property = gtk_popover_menu_set_property;
  object_class->get_property = gtk_popover_menu_get_property;

  widget_class->map = gtk_popover_menu_map;
  widget_class->unmap = gtk_popover_menu_unmap;
  widget_class->focus = gtk_popover_menu_focus;

  container_class->add = gtk_popover_menu_add;
  container_class->remove = gtk_popover_menu_remove;

  g_object_class_install_property (object_class,
                                   PROP_VISIBLE_SUBMENU,
                                   g_param_spec_string ("visible-submenu",
                                                        P_("Visible submenu"),
                                                        P_("The name of the visible submenu"),
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  binding_set = gtk_binding_set_by_class (klass);

  add_arrow_bindings (binding_set, GDK_KEY_Up, GTK_DIR_UP);
  add_arrow_bindings (binding_set, GDK_KEY_Down, GTK_DIR_DOWN);
  add_arrow_bindings (binding_set, GDK_KEY_Left, GTK_DIR_LEFT);
  add_arrow_bindings (binding_set, GDK_KEY_Right, GTK_DIR_RIGHT);

  add_tab_bindings (binding_set, 0, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, 0,
                                "activate-default", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_ISO_Enter, 0,
                                "activate-default", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter, 0,
                                "activate-default", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, 0,
                                "activate-default", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, 0,
                                "activate-default", 0);
}

/**
 * gtk_popover_menu_new:
 * @relative_to: (allow-none): #GtkWidget the popover is related to
 *
 * Creates a new popover menu.
 *
 * Returns: a new #GtkPopoverMenu
 */
GtkWidget *
gtk_popover_menu_new (GtkWidget *relative_to)
{
  GtkWidget *popover;

  g_return_val_if_fail (relative_to == NULL || GTK_IS_WIDGET (relative_to), NULL);

  popover = g_object_new (GTK_TYPE_POPOVER_MENU,
                          "relative-to", relative_to,
                          "autohide", TRUE,
                          NULL);

  return popover;
}

/**
 * gtk_popover_menu_open_submenu:
 * @popover: a #GtkPopoverMenu
 * @name: the name of the menu to switch to
 *
 * Opens a submenu of the @popover. The @name
 * must be one of the names given to the submenus
 * of @popover with #GtkPopoverMenu:submenu, or
 * "main" to switch back to the main menu.
 *
 * #GtkModelButton will open submenus automatically
 * when the #GtkModelButton:menu-name property is set,
 * so this function is only needed when you are using
 * other kinds of widgets to initiate menu changes.
 */
void
gtk_popover_menu_open_submenu (GtkPopoverMenu *popover,
                               const gchar    *name)
{
  GtkWidget *stack;

  g_return_if_fail (GTK_IS_POPOVER_MENU (popover));

  stack = gtk_bin_get_child (GTK_BIN (popover));
  gtk_stack_set_visible_child_name (GTK_STACK (stack), name);
}

/**
 * gtk_popover_menu_add_submenu:
 * @popover: a #GtkPopoverMenu
 * @submenu: a widget to add as submenu
 * @name: the name for the submenu
 *
 * Adds a submenu to the popover menu.
 */
void
gtk_popover_menu_add_submenu (GtkPopoverMenu *popover,
                              GtkWidget      *submenu,
                              const char     *name)
{
  GtkWidget *stack;

  stack = gtk_bin_get_child (GTK_BIN (popover));

  gtk_stack_add_named (GTK_STACK (stack), submenu, name);
}

/**
 * gtk_popover_menu_new_from_model:
 * @relative_to: (allow-none): #GtkWidget the popover is related to
 * @model: a #GMenuModel
 *
 * Creates a #GtkPopoverMenu and populates it according to
 * @model. The popover is pointed to the @relative_to widget.
 *
 * The created buttons are connected to actions found in the
 * #GtkApplicationWindow to which the popover belongs - typically
 * by means of being attached to a widget that is contained within
 * the #GtkApplicationWindows widget hierarchy.
 *
 * Actions can also be added using gtk_widget_insert_action_group()
 * on the menus attach widget or on any of its parent widgets.
 *
 * This function creates menus with sliding submenus.
 * See gtk_popover_menu_new_from_model_full() for a way
 * to control this.
 *
 * Returns: the new #GtkPopoverMenu
 */
GtkWidget *
gtk_popover_menu_new_from_model (GtkWidget  *relative_to,
                                 GMenuModel *model)

{
  return gtk_popover_menu_new_from_model_full (relative_to, model, 0);
}

/**
 * gtk_popover_menu_new_from_model_full:
 * @relative_to: (allow-none): #GtkWidget the popover is related to
 * @model: a #GMenuModel
 * @flags: flags that affect how the menu is created
 *
 * Creates a #GtkPopoverMenu and populates it according to
 * @model. The popover is pointed to the @relative_to widget.
 *
 * The created buttons are connected to actions found in the
 * #GtkApplicationWindow to which the popover belongs - typically
 * by means of being attached to a widget that is contained within
 * the #GtkApplicationWindows widget hierarchy.
 *
 * Actions can also be added using gtk_widget_insert_action_group()
 * on the menus attach widget or on any of its parent widgets.
 *
 * The only flag that is supported currently is
 * #GTK_POPOVER_MENU_NESTED, which makes GTK create traditional,
 * nested submenus instead of the default sliding submenus.
 *
 * Returns: the new #GtkPopoverMenu
 */
GtkWidget *
gtk_popover_menu_new_from_model_full (GtkWidget           *relative_to,
                                      GMenuModel          *model,
                                      GtkPopoverMenuFlags  flags)
{
  GtkWidget *popover;

  g_return_val_if_fail (relative_to == NULL || GTK_IS_WIDGET (relative_to), NULL);
  g_return_val_if_fail (G_IS_MENU_MODEL (model), NULL);

  popover = gtk_popover_menu_new (relative_to);
  gtk_menu_section_box_new_toplevel (GTK_POPOVER_MENU (popover), model, flags);

  return popover;
}

