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
 * SECTION:gtkmenubar
 * @Title: GtkMenuBar
 * @Short_description: A subclass of GtkMenuShell which holds GtkMenuItem widgets
 * @See_also: #GtkMenuShell, #GtkMenu, #GtkMenuItem
 *
 * The #GtkMenuBar is a subclass of #GtkMenuShell which contains one or
 * more #GtkMenuItems. The result is a standard menu bar which can hold
 * many menu items.
 *
 * # CSS nodes
 *
 * GtkMenuBar has a single CSS node with name menubar.
 */

#include "config.h"

#include "gtkmenubar.h"

#include "gtkbindings.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenuitemprivate.h"
#include "gtkmenuprivate.h"
#include "gtkmenushellprivate.h"
#include "gtksettings.h"
#include "gtksizerequest.h"
#include "gtkwindow.h"
#include "gtkcontainerprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

#define MENU_BAR_POPUP_DELAY 0

static void gtk_menu_bar_measure (GtkWidget     *widget,
                                  GtkOrientation  orientation,
                                  int             for_size,
                                  int            *minimum,
                                  int            *natural,
                                  int            *minimum_baseline,
                                  int            *natural_baseline);
static void gtk_menu_bar_size_allocate     (GtkWidget *widget,
                                            int        width,
                                            int        height,
                                            int        baseline);
static void gtk_menu_bar_root              (GtkWidget *widget);
static void gtk_menu_bar_unroot            (GtkWidget *widget);
static gint gtk_menu_bar_get_popup_delay   (GtkMenuShell    *menu_shell);
static void gtk_menu_bar_move_current      (GtkMenuShell     *menu_shell,
                                            GtkMenuDirectionType direction);

G_DEFINE_TYPE (GtkMenuBar, gtk_menu_bar, GTK_TYPE_MENU_SHELL)

static void
gtk_menu_bar_class_init (GtkMenuBarClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkMenuShellClass *menu_shell_class = GTK_MENU_SHELL_CLASS (class);

  GtkBindingSet *binding_set;

  widget_class->measure = gtk_menu_bar_measure;
  widget_class->size_allocate = gtk_menu_bar_size_allocate;
  widget_class->root = gtk_menu_bar_root;
  widget_class->unroot = gtk_menu_bar_unroot;

  menu_shell_class->submenu_placement = GTK_TOP_BOTTOM;
  menu_shell_class->get_popup_delay = gtk_menu_bar_get_popup_delay;
  menu_shell_class->move_current = gtk_menu_bar_move_current;

  binding_set = gtk_binding_set_by_class (class);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_Left, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PREV);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_KP_Left, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PREV);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_Right, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_NEXT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_KP_Right, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_NEXT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_Up, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PARENT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_KP_Up, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PARENT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_Down, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_CHILD);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_KP_Down, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_CHILD);

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_MENU_BAR);
  gtk_widget_class_set_css_name (widget_class, I_("menubar"));
}

static void
gtk_menu_bar_init (GtkMenuBar *menu_bar)
{
}

/**
 * gtk_menu_bar_new:
 *
 * Creates a new #GtkMenuBar
 *
 * Returns: the new menu bar, as a #GtkWidget
 */
GtkWidget*
gtk_menu_bar_new (void)
{
  return g_object_new (GTK_TYPE_MENU_BAR, NULL);
}

static void
gtk_menu_bar_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;
  gboolean use_toggle_size, use_maximize;
  gint child_minimum, child_natural;

  *minimum = 0;
  *natural = 0;

  menu_shell = GTK_MENU_SHELL (widget);

  children = menu_shell->priv->children;

  use_toggle_size = (orientation == GTK_ORIENTATION_HORIZONTAL);
  use_maximize = (orientation == GTK_ORIENTATION_VERTICAL);

  while (children)
    {
      child = children->data;
      children = children->next;

      if (gtk_widget_get_visible (child))
        {
          gtk_widget_measure (child,
                              orientation,
                              for_size,
                              &child_minimum, &child_natural,
                              NULL, NULL);

          if (use_toggle_size)
            {
              gint toggle_size;

              gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (child),
                                                 &toggle_size);

              child_minimum += toggle_size;
              child_natural += toggle_size;
            }

          if (use_maximize)
            {
              *minimum = MAX (*minimum, child_minimum);
              *natural = MAX (*natural, child_natural);
            }
          else
            {
              *minimum += child_minimum;
              *natural += child_natural;
            }
        }
    }
}

static void
gtk_menu_bar_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;
  GtkAllocation remaining_space;
  GArray *requested_sizes;
  gint toggle_size;
  guint i;
  int size;
  gboolean ltr = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR);

  menu_shell = GTK_MENU_SHELL (widget);

  if (!menu_shell->priv->children)
    return;

  remaining_space = (GtkAllocation) {0, 0, width, height};
  requested_sizes = g_array_new (FALSE, FALSE, sizeof (GtkRequestedSize));
  size = remaining_space.width;

  for (children = menu_shell->priv->children; children; children = children->next)
    {
      GtkRequestedSize request;
      child = children->data;

      if (!gtk_widget_get_visible (child))
        continue;

      request.data = child;
      gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL,
                          remaining_space.height,
                          &request.minimum_size, &request.natural_size,
                          NULL, NULL);
      gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (child),
                                         &toggle_size);
      request.minimum_size += toggle_size;
      request.natural_size += toggle_size;

      gtk_menu_item_toggle_size_allocate (GTK_MENU_ITEM (child), toggle_size);

      g_array_append_val (requested_sizes, request);

      size -= request.minimum_size;
    }

  size = gtk_distribute_natural_allocation (size,
                                            requested_sizes->len,
                                            (GtkRequestedSize *) requested_sizes->data);

  for (i = 0; i < requested_sizes->len; i++)
    {
      GtkAllocation child_allocation = remaining_space;
      GtkRequestedSize *request = &g_array_index (requested_sizes, GtkRequestedSize, i);

      child_allocation.width = request->minimum_size;
      remaining_space.width -= request->minimum_size;

      if (i + 1 == requested_sizes->len && GTK_IS_MENU_ITEM (request->data) &&
          GTK_MENU_ITEM (request->data)->priv->right_justify)
        ltr = !ltr;

      if (ltr)
        remaining_space.x += request->minimum_size;
      else
        child_allocation.x += remaining_space.width;

      gtk_widget_size_allocate (request->data, &child_allocation, -1);
    }

  g_array_free (requested_sizes, TRUE);
}

static GList *
get_menu_bars (GtkWindow *window)
{
  return g_object_get_data (G_OBJECT (window), "gtk-menu-bar-list");
}

GList *
_gtk_menu_bar_get_viewable_menu_bars (GtkWindow *window)
{
  GList *menu_bars;
  GList *viewable_menu_bars = NULL;

  for (menu_bars = get_menu_bars (window);
       menu_bars;
       menu_bars = menu_bars->next)
    {
      GtkWidget *widget = menu_bars->data;
      gboolean viewable = TRUE;
      
      while (widget)
	{
	  if (!gtk_widget_get_mapped (widget))
	    viewable = FALSE;

          widget = gtk_widget_get_parent (widget);
	}

      if (viewable)
	viewable_menu_bars = g_list_prepend (viewable_menu_bars, menu_bars->data);
    }

  return g_list_reverse (viewable_menu_bars);
}

static void
set_menu_bars (GtkWindow *window,
	       GList     *menubars)
{
  g_object_set_data (G_OBJECT (window), I_("gtk-menu-bar-list"), menubars);
}

static void
add_to_window (GtkWindow  *window,
               GtkMenuBar *menubar)
{
  GList *menubars = get_menu_bars (window);

  set_menu_bars (window, g_list_prepend (menubars, menubar));
}

static void
remove_from_window (GtkWindow  *window,
                    GtkMenuBar *menubar)
{
  GList *menubars = get_menu_bars (window);

  menubars = g_list_remove (menubars, menubar);
  set_menu_bars (window, menubars);
}

static void
gtk_menu_bar_root (GtkWidget *widget)
{
  GtkMenuBar *menubar = GTK_MENU_BAR (widget);
  GtkWidget *toplevel;  

  GTK_WIDGET_CLASS (gtk_menu_bar_parent_class)->root (widget);

  toplevel = gtk_widget_get_toplevel (widget);
  add_to_window (GTK_WINDOW (toplevel), menubar);
}

static void
gtk_menu_bar_unroot (GtkWidget *widget)
{
  GtkMenuBar *menubar = GTK_MENU_BAR (widget);
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (widget);
  remove_from_window (GTK_WINDOW (toplevel), menubar);

  GTK_WIDGET_CLASS (gtk_menu_bar_parent_class)->unroot (widget);
}

/**
 * _gtk_menu_bar_cycle_focus:
 * @menubar: a #GtkMenuBar
 * @dir: direction in which to cycle the focus
 * 
 * Move the focus between menubars in the toplevel.
 **/
void
_gtk_menu_bar_cycle_focus (GtkMenuBar       *menubar,
			   GtkDirectionType  dir)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (menubar));
  GtkMenuItem *to_activate = NULL;

  if (GTK_IS_WINDOW (toplevel))
    {
      GList *tmp_menubars = _gtk_menu_bar_get_viewable_menu_bars (GTK_WINDOW (toplevel));
      GList *l;
      GPtrArray *menubars;
      gboolean found;
      guint index;

      menubars = g_ptr_array_sized_new (g_list_length (tmp_menubars));

      for (l = tmp_menubars; l; l = l->next)
        g_ptr_array_add (menubars, l->data);

      gtk_widget_focus_sort (toplevel, dir, menubars);

      found = g_ptr_array_find (menubars, menubar, &index);

      if (found && index < menubars->len - 1)
        {
          GtkWidget *next = g_ptr_array_index (menubars, index + 1);
          GtkMenuShell *new_menushell = GTK_MENU_SHELL (next);

          if (new_menushell->priv->children)
            to_activate = new_menushell->priv->children->data;
        }

      g_ptr_array_free (menubars, TRUE);
    }

  gtk_menu_shell_cancel (GTK_MENU_SHELL (menubar));

  if (to_activate)
    g_signal_emit_by_name (to_activate, "activate_item");
}

static gint
gtk_menu_bar_get_popup_delay (GtkMenuShell *menu_shell)
{
  return MENU_BAR_POPUP_DELAY;
}

static void
gtk_menu_bar_move_current (GtkMenuShell         *menu_shell,
			   GtkMenuDirectionType  direction)
{
  GtkMenuBar *menubar = GTK_MENU_BAR (menu_shell);

  if (gtk_widget_get_direction (GTK_WIDGET (menubar)) == GTK_TEXT_DIR_RTL)
    {
      switch (direction) 
        {      
        case GTK_MENU_DIR_PREV:
          direction = GTK_MENU_DIR_NEXT;
          break;
        case GTK_MENU_DIR_NEXT:
          direction = GTK_MENU_DIR_PREV;
          break;
        case GTK_MENU_DIR_PARENT:
        case GTK_MENU_DIR_CHILD:
        default:
          break;
        }
    }
  
  GTK_MENU_SHELL_CLASS (gtk_menu_bar_parent_class)->move_current (menu_shell, direction);
}

/**
 * gtk_menu_bar_new_from_model:
 * @model: a #GMenuModel
 *
 * Creates a new #GtkMenuBar and populates it with menu items
 * and submenus according to @model.
 *
 * The created menu items are connected to actions found in the
 * #GtkApplicationWindow to which the menu bar belongs - typically
 * by means of being contained within the #GtkApplicationWindows
 * widget hierarchy.
 *
 * Returns: a new #GtkMenuBar
 */
GtkWidget *
gtk_menu_bar_new_from_model (GMenuModel *model)
{
  GtkWidget *menubar;

  g_return_val_if_fail (G_IS_MENU_MODEL (model), NULL);

  menubar = gtk_menu_bar_new ();
  gtk_menu_shell_bind_model (GTK_MENU_SHELL (menubar), model, NULL, FALSE);

  return menubar;
}
