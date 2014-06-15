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
 */

#include "config.h"

#include "gtkmenubar.h"

#include "gtkbindings.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenuitemprivate.h"
#include "gtkmenuprivate.h"
#include "gtkmenushellprivate.h"
#include "gtkrender.h"
#include "gtksettings.h"
#include "gtksizerequest.h"
#include "gtkwindow.h"
#include "gtkcontainerprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

#define MENU_BAR_POPUP_DELAY 0

/* Properties */
enum {
  PROP_0,
  PROP_PACK_DIRECTION,
  PROP_CHILD_PACK_DIRECTION
};

struct _GtkMenuBarPrivate
{
  GtkPackDirection pack_direction;
  GtkPackDirection child_pack_direction;
};


static void gtk_menu_bar_set_property      (GObject             *object,
					    guint                prop_id,
					    const GValue        *value,
					    GParamSpec          *pspec);
static void gtk_menu_bar_get_property      (GObject             *object,
					    guint                prop_id,
					    GValue              *value,
					    GParamSpec          *pspec);
static void gtk_menu_bar_get_preferred_width (GtkWidget     *widget,
					      gint          *minimum,
					      gint          *natural);
static void gtk_menu_bar_get_preferred_height (GtkWidget    *widget,
					       gint         *minimum,
					       gint         *natural);
static void gtk_menu_bar_get_preferred_width_for_height (GtkWidget    *widget,
                                                         gint          height,
                                                         gint         *minimum,
                                                         gint         *natural);
static void gtk_menu_bar_get_preferred_height_for_width (GtkWidget    *widget,
                                                         gint          width,
                                                         gint         *minimum,
                                                         gint         *natural);
static void gtk_menu_bar_size_allocate     (GtkWidget       *widget,
					    GtkAllocation   *allocation);
static gint gtk_menu_bar_draw              (GtkWidget       *widget,
                                            cairo_t         *cr);
static void gtk_menu_bar_hierarchy_changed (GtkWidget       *widget,
					    GtkWidget       *old_toplevel);
static gint gtk_menu_bar_get_popup_delay   (GtkMenuShell    *menu_shell);
static void gtk_menu_bar_move_current      (GtkMenuShell     *menu_shell,
                                            GtkMenuDirectionType direction);

static GtkShadowType get_shadow_type   (GtkMenuBar      *menubar);

G_DEFINE_TYPE_WITH_PRIVATE (GtkMenuBar, gtk_menu_bar, GTK_TYPE_MENU_SHELL)

static void
gtk_menu_bar_class_init (GtkMenuBarClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkMenuShellClass *menu_shell_class;

  GtkBindingSet *binding_set;

  gobject_class = (GObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  menu_shell_class = (GtkMenuShellClass*) class;

  gobject_class->get_property = gtk_menu_bar_get_property;
  gobject_class->set_property = gtk_menu_bar_set_property;

  widget_class->get_preferred_width = gtk_menu_bar_get_preferred_width;
  widget_class->get_preferred_height = gtk_menu_bar_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_menu_bar_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_menu_bar_get_preferred_height_for_width;
  widget_class->size_allocate = gtk_menu_bar_size_allocate;
  widget_class->draw = gtk_menu_bar_draw;
  widget_class->hierarchy_changed = gtk_menu_bar_hierarchy_changed;
  
  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_MENU_BAR);

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

  /**
   * GtkMenuBar:pack-direction:
   *
   * The pack direction of the menubar. It determines how
   * menuitems are arranged in the menubar.
   *
   * Since: 2.8
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PACK_DIRECTION,
                                   g_param_spec_enum ("pack-direction",
                                                      P_("Pack direction"),
                                                      P_("The pack direction of the menubar"),
                                                      GTK_TYPE_PACK_DIRECTION,
                                                      GTK_PACK_DIRECTION_LTR,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  /**
   * GtkMenuBar:child-pack-direction:
   *
   * The child pack direction of the menubar. It determines how
   * the widgets contained in child menuitems are arranged.
   *
   * Since: 2.8
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CHILD_PACK_DIRECTION,
                                   g_param_spec_enum ("child-pack-direction",
                                                      P_("Child Pack direction"),
                                                      P_("The child pack direction of the menubar"),
                                                      GTK_TYPE_PACK_DIRECTION,
                                                      GTK_PACK_DIRECTION_LTR,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_enum ("shadow-type",
                                                              P_("Shadow type"),
                                                              P_("Style of bevel around the menubar"),
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_OUT,
                                                              GTK_PARAM_READABLE));

  /**
   * GtkMenuBar:internal-padding:
   *
   * Amount of border space between the menubar shadow and the menu items
   *
   * Deprecated: 3.8: use the standard padding CSS property (through objects
   *   like #GtkStyleContext and #GtkCssProvider); the value of this style
   *   property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("internal-padding",
							     P_("Internal padding"),
							     P_("Amount of border space between the menubar shadow and the menu items"),
							     0,
							     G_MAXINT,
                                                             0,
                                                             GTK_PARAM_READABLE |
                                                             G_PARAM_DEPRECATED));
}

static void
gtk_menu_bar_init (GtkMenuBar *menu_bar)
{
  GtkStyleContext *context;

  menu_bar->priv = gtk_menu_bar_get_instance_private (menu_bar);

  context = gtk_widget_get_style_context (GTK_WIDGET (menu_bar));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_MENUBAR);
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
gtk_menu_bar_set_property (GObject      *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  GtkMenuBar *menubar = GTK_MENU_BAR (object);
  
  switch (prop_id)
    {
    case PROP_PACK_DIRECTION:
      gtk_menu_bar_set_pack_direction (menubar, g_value_get_enum (value));
      break;
    case PROP_CHILD_PACK_DIRECTION:
      gtk_menu_bar_set_child_pack_direction (menubar, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_bar_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
  GtkMenuBar *menubar = GTK_MENU_BAR (object);
  
  switch (prop_id)
    {
    case PROP_PACK_DIRECTION:
      g_value_set_enum (value, gtk_menu_bar_get_pack_direction (menubar));
      break;
    case PROP_CHILD_PACK_DIRECTION:
      g_value_set_enum (value, gtk_menu_bar_get_child_pack_direction (menubar));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_bar_size_request (GtkWidget      *widget,
                           GtkOrientation  orientation,
                           gint            size,
                           gint           *minimum,
                           gint           *natural)
{
  GtkMenuBar *menu_bar;
  GtkMenuBarPrivate *priv;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;
  guint border_width;
  gboolean use_toggle_size, use_maximize;
  gint child_minimum, child_natural;
  GtkStyleContext *context;
  GtkBorder border;
  GtkStateFlags flags;

  *minimum = 0;
  *natural = 0;

  menu_bar = GTK_MENU_BAR (widget);
  menu_shell = GTK_MENU_SHELL (widget);
  priv = menu_bar->priv;

  children = menu_shell->priv->children;

  if (priv->child_pack_direction == GTK_PACK_DIRECTION_LTR ||
      priv->child_pack_direction == GTK_PACK_DIRECTION_RTL)
    use_toggle_size = (orientation == GTK_ORIENTATION_HORIZONTAL);
  else
    use_toggle_size = (orientation == GTK_ORIENTATION_VERTICAL);

  if (priv->pack_direction == GTK_PACK_DIRECTION_LTR ||
      priv->pack_direction == GTK_PACK_DIRECTION_RTL)
    use_maximize = (orientation == GTK_ORIENTATION_VERTICAL);
  else
    use_maximize = (orientation == GTK_ORIENTATION_HORIZONTAL);

  while (children)
    {
      child = children->data;
      children = children->next;

      if (gtk_widget_get_visible (child))
        {
          _gtk_widget_get_preferred_size_for_size (child, orientation, size, &child_minimum, &child_natural, NULL, NULL);

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

  context = gtk_widget_get_style_context (widget);
  flags = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, flags, &border);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum += border.left + border.right;
      *natural += border.left + border.right;
    }
  else
    {
      *minimum += border.top + border.bottom;
      *natural += border.top + border.bottom;
    }

  border_width = gtk_container_get_border_width (GTK_CONTAINER (menu_bar));
  *minimum += border_width * 2;
  *natural += border_width * 2;

  if (get_shadow_type (menu_bar) != GTK_SHADOW_NONE)
    {
      gtk_style_context_get_border (context, flags, &border);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          *minimum += border.left + border.right;
          *natural += border.left + border.right;
        }
      else
        {
          *minimum += border.top + border.bottom;
          *natural += border.top + border.bottom;
        }
    }
}

static void
gtk_menu_bar_get_preferred_width (GtkWidget *widget,
				  gint      *minimum,
				  gint      *natural)
{
  gtk_menu_bar_size_request (widget, GTK_ORIENTATION_HORIZONTAL, -1, minimum, natural);
}

static void
gtk_menu_bar_get_preferred_height (GtkWidget *widget,
				   gint      *minimum,
				   gint      *natural)
{
  gtk_menu_bar_size_request (widget, GTK_ORIENTATION_VERTICAL, -1, minimum, natural);
}

static void
gtk_menu_bar_get_preferred_width_for_height (GtkWidget *widget,
                                             gint       height,
                                             gint      *minimum,
                                             gint      *natural)
{
  gtk_menu_bar_size_request (widget, GTK_ORIENTATION_HORIZONTAL, height, minimum, natural);
}

static void
gtk_menu_bar_get_preferred_height_for_width (GtkWidget *widget,
                                             gint       width,
                                             gint      *minimum,
                                             gint      *natural)
{
  gtk_menu_bar_size_request (widget, GTK_ORIENTATION_VERTICAL, width, minimum, natural);
}

static void
gtk_menu_bar_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkMenuBar *menu_bar;
  GtkMenuShell *menu_shell;
  GtkMenuBarPrivate *priv;
  GtkWidget *child;
  GList *children;
  GtkAllocation remaining_space;
  guint border_width;
  GArray *requested_sizes;
  gint toggle_size;
  guint i;

  g_return_if_fail (GTK_IS_MENU_BAR (widget));
  g_return_if_fail (allocation != NULL);

  menu_bar = GTK_MENU_BAR (widget);
  menu_shell = GTK_MENU_SHELL (widget);
  priv = menu_bar->priv;

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (gtk_widget_get_window (widget),
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

  if (menu_shell->priv->children)
    {
      GtkStyleContext *context;
      GtkStateFlags flags;
      GtkBorder border;

      context = gtk_widget_get_style_context (widget);
      flags = gtk_widget_get_state_flags (widget);
      gtk_style_context_get_padding (context, flags, &border);

      border_width = gtk_container_get_border_width (GTK_CONTAINER (menu_bar));

      remaining_space.x = (border_width + border.left);
      remaining_space.y = (border_width + border.top);
      remaining_space.width = allocation->width -
        2 * border_width - border.left - border.right;
      remaining_space.height = allocation->height -
        2 * border_width - border.top - border.bottom;

      if (get_shadow_type (menu_bar) != GTK_SHADOW_NONE)
	{
          gtk_style_context_get_border (context, flags, &border);

          remaining_space.x += border.left;
          remaining_space.y += border.top;
          remaining_space.width -= border.left + border.right;
          remaining_space.height -= border.top + border.bottom;
	}
      
      requested_sizes = g_array_new (FALSE, FALSE, sizeof (GtkRequestedSize));

      if (priv->pack_direction == GTK_PACK_DIRECTION_LTR ||
	  priv->pack_direction == GTK_PACK_DIRECTION_RTL)
	{
          int size = remaining_space.width;
          gboolean ltr = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) == (priv->pack_direction == GTK_PACK_DIRECTION_LTR);

          for (children = menu_shell->priv->children; children; children = children->next)
            {
              GtkRequestedSize request;
	      child = children->data;

	      if (!gtk_widget_get_visible (child))
                continue;

              request.data = child;
              gtk_widget_get_preferred_width_for_height (child, 
                                                         remaining_space.height,
                                                         &request.minimum_size, 
                                                         &request.natural_size);
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

              gtk_widget_size_allocate (request->data, &child_allocation);
            }
	}
      else
	{
          int size = remaining_space.height;
          gboolean ttb = (priv->pack_direction == GTK_PACK_DIRECTION_TTB);

          for (children = menu_shell->priv->children; children; children = children->next)
            {
              GtkRequestedSize request;
	      child = children->data;

	      if (!gtk_widget_get_visible (child))
                continue;

              request.data = child;
              gtk_widget_get_preferred_height_for_width (child, 
                                                         remaining_space.width,
                                                         &request.minimum_size, 
                                                         &request.natural_size);
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

              child_allocation.height = request->minimum_size;
              remaining_space.height -= request->minimum_size;

	      if (i + 1 == requested_sizes->len && GTK_IS_MENU_ITEM (request->data) &&
                  GTK_MENU_ITEM (request->data)->priv->right_justify)
                ttb = !ttb;

              if (ttb)
                remaining_space.y += request->minimum_size;
              else
                child_allocation.y += remaining_space.height;

              gtk_widget_size_allocate (request->data, &child_allocation);
            }
	}

      g_array_free (requested_sizes, TRUE);
    }
}

static gint
gtk_menu_bar_draw (GtkWidget *widget,
		   cairo_t   *cr)
{
  GtkStyleContext *context;
  int border;

  border = gtk_container_get_border_width (GTK_CONTAINER (widget));
  context = gtk_widget_get_style_context (widget);

  gtk_render_background (context, cr,
                         border, border,
                         gtk_widget_get_allocated_width (widget) - border * 2,
                         gtk_widget_get_allocated_height (widget) - border * 2);

  if (get_shadow_type (GTK_MENU_BAR (widget)) != GTK_SHADOW_NONE)
    gtk_render_frame (context, cr,
                      border, border,
                      gtk_widget_get_allocated_width (widget) - border * 2,
                      gtk_widget_get_allocated_height (widget) - border * 2);

  GTK_WIDGET_CLASS (gtk_menu_bar_parent_class)->draw (widget, cr);

  return FALSE;
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
gtk_menu_bar_hierarchy_changed (GtkWidget *widget,
				GtkWidget *old_toplevel)
{
  GtkWidget *toplevel;  
  GtkMenuBar *menubar;

  menubar = GTK_MENU_BAR (widget);

  toplevel = gtk_widget_get_toplevel (widget);

  if (old_toplevel)
    remove_from_window (GTK_WINDOW (old_toplevel), menubar);
  
  if (gtk_widget_is_toplevel (toplevel))
    add_to_window (GTK_WINDOW (toplevel), menubar);
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

  if (gtk_widget_is_toplevel (toplevel))
    {
      GList *tmp_menubars = _gtk_menu_bar_get_viewable_menu_bars (GTK_WINDOW (toplevel));
      GList *menubars;
      GList *current;

      menubars = _gtk_container_focus_sort (GTK_CONTAINER (toplevel), tmp_menubars,
					    dir, GTK_WIDGET (menubar));
      g_list_free (tmp_menubars);

      if (menubars)
	{
	  current = g_list_find (menubars, menubar);

	  if (current && current->next)
	    {
	      GtkMenuShell *new_menushell = GTK_MENU_SHELL (current->next->data);
	      if (new_menushell->priv->children)
		to_activate = new_menushell->priv->children->data;
	    }
	}
	  
      g_list_free (menubars);
    }

  gtk_menu_shell_cancel (GTK_MENU_SHELL (menubar));

  if (to_activate)
    g_signal_emit_by_name (to_activate, "activate_item");
}

static GtkShadowType
get_shadow_type (GtkMenuBar *menubar)
{
  GtkShadowType shadow_type = GTK_SHADOW_OUT;
  
  gtk_widget_style_get (GTK_WIDGET (menubar),
			"shadow-type", &shadow_type,
			NULL);

  return shadow_type;
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
  GtkTextDirection text_dir;
  GtkPackDirection pack_dir;

  text_dir = gtk_widget_get_direction (GTK_WIDGET (menubar));
  pack_dir = gtk_menu_bar_get_pack_direction (menubar);
  
  if (pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL)
     {
      if ((text_dir == GTK_TEXT_DIR_RTL) == (pack_dir == GTK_PACK_DIRECTION_LTR))
	{
	  switch (direction) 
	    {      
	    case GTK_MENU_DIR_PREV:
	      direction = GTK_MENU_DIR_NEXT;
	      break;
	    case GTK_MENU_DIR_NEXT:
	      direction = GTK_MENU_DIR_PREV;
	      break;
	    default: ;
	    }
	}
    }
  else
    {
      switch (direction) 
	{
	case GTK_MENU_DIR_PARENT:
	  if ((text_dir == GTK_TEXT_DIR_LTR) == (pack_dir == GTK_PACK_DIRECTION_TTB))
	    direction = GTK_MENU_DIR_PREV;
	  else
	    direction = GTK_MENU_DIR_NEXT;
	  break;
	case GTK_MENU_DIR_CHILD:
	  if ((text_dir == GTK_TEXT_DIR_LTR) == (pack_dir == GTK_PACK_DIRECTION_TTB))
	    direction = GTK_MENU_DIR_NEXT;
	  else
	    direction = GTK_MENU_DIR_PREV;
	  break;
	case GTK_MENU_DIR_PREV:
	  if (text_dir == GTK_TEXT_DIR_RTL)	  
	    direction = GTK_MENU_DIR_CHILD;
	  else
	    direction = GTK_MENU_DIR_PARENT;
	  break;
	case GTK_MENU_DIR_NEXT:
	  if (text_dir == GTK_TEXT_DIR_RTL)	  
	    direction = GTK_MENU_DIR_PARENT;
	  else
	    direction = GTK_MENU_DIR_CHILD;
	  break;
	default: ;
	}
    }
  
  GTK_MENU_SHELL_CLASS (gtk_menu_bar_parent_class)->move_current (menu_shell, direction);
}

/**
 * gtk_menu_bar_get_pack_direction:
 * @menubar: a #GtkMenuBar
 * 
 * Retrieves the current pack direction of the menubar. 
 * See gtk_menu_bar_set_pack_direction().
 *
 * Returns: the pack direction
 *
 * Since: 2.8
 */
GtkPackDirection
gtk_menu_bar_get_pack_direction (GtkMenuBar *menubar)
{
  g_return_val_if_fail (GTK_IS_MENU_BAR (menubar), 
			GTK_PACK_DIRECTION_LTR);

  return menubar->priv->pack_direction;
}

/**
 * gtk_menu_bar_set_pack_direction:
 * @menubar: a #GtkMenuBar
 * @pack_dir: a new #GtkPackDirection
 * 
 * Sets how items should be packed inside a menubar.
 * 
 * Since: 2.8
 */
void
gtk_menu_bar_set_pack_direction (GtkMenuBar       *menubar,
                                 GtkPackDirection  pack_dir)
{
  GtkMenuBarPrivate *priv;
  GList *l;

  g_return_if_fail (GTK_IS_MENU_BAR (menubar));

  priv = menubar->priv;

  if (priv->pack_direction != pack_dir)
    {
      priv->pack_direction = pack_dir;

      gtk_widget_queue_resize (GTK_WIDGET (menubar));

      for (l = GTK_MENU_SHELL (menubar)->priv->children; l; l = l->next)
	gtk_widget_queue_resize (GTK_WIDGET (l->data));

      g_object_notify (G_OBJECT (menubar), "pack-direction");
    }
}

/**
 * gtk_menu_bar_get_child_pack_direction:
 * @menubar: a #GtkMenuBar
 * 
 * Retrieves the current child pack direction of the menubar.
 * See gtk_menu_bar_set_child_pack_direction().
 *
 * Returns: the child pack direction
 *
 * Since: 2.8
 */
GtkPackDirection
gtk_menu_bar_get_child_pack_direction (GtkMenuBar *menubar)
{
  g_return_val_if_fail (GTK_IS_MENU_BAR (menubar), 
			GTK_PACK_DIRECTION_LTR);

  return menubar->priv->child_pack_direction;
}

/**
 * gtk_menu_bar_set_child_pack_direction:
 * @menubar: a #GtkMenuBar
 * @child_pack_dir: a new #GtkPackDirection
 * 
 * Sets how widgets should be packed inside the children of a menubar.
 * 
 * Since: 2.8
 */
void
gtk_menu_bar_set_child_pack_direction (GtkMenuBar       *menubar,
                                       GtkPackDirection  child_pack_dir)
{
  GtkMenuBarPrivate *priv;
  GList *l;

  g_return_if_fail (GTK_IS_MENU_BAR (menubar));

  priv = menubar->priv;

  if (priv->child_pack_direction != child_pack_dir)
    {
      priv->child_pack_direction = child_pack_dir;

      gtk_widget_queue_resize (GTK_WIDGET (menubar));

      for (l = GTK_MENU_SHELL (menubar)->priv->children; l; l = l->next)
	gtk_widget_queue_resize (GTK_WIDGET (l->data));

      g_object_notify (G_OBJECT (menubar), "child-pack-direction");
    }
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
 *
 * Since: 3.4
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
