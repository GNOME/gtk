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

#define GTK_MENU_INTERNALS

#include <config.h>
#include "gdk/gdkkeysyms.h"
#include "gtkbindings.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenubar.h"
#include "gtkmenuitem.h"
#include "gtksettings.h"
#include "gtkintl.h"
#include "gtkwindow.h"
#include "gtkalias.h"


#define BORDER_SPACING  0
#define DEFAULT_IPADDING 1

static void gtk_menu_bar_class_init        (GtkMenuBarClass *klass);
static void gtk_menu_bar_size_request      (GtkWidget       *widget,
					    GtkRequisition  *requisition);
static void gtk_menu_bar_size_allocate     (GtkWidget       *widget,
					    GtkAllocation   *allocation);
static void gtk_menu_bar_paint             (GtkWidget       *widget,
					    GdkRectangle    *area);
static gint gtk_menu_bar_expose            (GtkWidget       *widget,
					    GdkEventExpose  *event);
static void gtk_menu_bar_hierarchy_changed (GtkWidget       *widget,
					    GtkWidget       *old_toplevel);
static gint gtk_menu_bar_get_popup_delay  (GtkMenuShell    *menu_shell);
					    

static GtkShadowType get_shadow_type   (GtkMenuBar      *menubar);

static GtkMenuShellClass *parent_class = NULL;

GType
gtk_menu_bar_get_type (void)
{
  static GType menu_bar_type = 0;

  if (!menu_bar_type)
    {
      static const GTypeInfo menu_bar_info =
      {
	sizeof (GtkMenuBarClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_menu_bar_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkMenuBar),
	0,		/* n_preallocs */
	NULL,		/* instance_init */
      };

      menu_bar_type = g_type_register_static (GTK_TYPE_MENU_SHELL, "GtkMenuBar",
					      &menu_bar_info, 0);
    }

  return menu_bar_type;
}

static void
gtk_menu_bar_class_init (GtkMenuBarClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkMenuShellClass *menu_shell_class;

  GtkBindingSet *binding_set;

  parent_class = g_type_class_peek_parent (class);
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  menu_shell_class = (GtkMenuShellClass*) class;

  widget_class->size_request = gtk_menu_bar_size_request;
  widget_class->size_allocate = gtk_menu_bar_size_allocate;
  widget_class->expose_event = gtk_menu_bar_expose;
  widget_class->hierarchy_changed = gtk_menu_bar_hierarchy_changed;
  
  menu_shell_class->submenu_placement = GTK_TOP_BOTTOM;
  menu_shell_class->get_popup_delay = gtk_menu_bar_get_popup_delay;

  binding_set = gtk_binding_set_by_class (class);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Left, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PREV);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Left, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PREV);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Right, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_NEXT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Right, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_NEXT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Up, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PARENT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Up, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PARENT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Down, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_CHILD);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Down, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_CHILD);

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_enum ("shadow_type",
                                                              P_("Shadow type"),
                                                              P_("Style of bevel around the menubar"),
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_OUT,
                                                              G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("internal_padding",
							     P_("Internal padding"),
							     P_("Amount of border space between the menubar shadow and the menu items"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_IPADDING,
                                                             G_PARAM_READABLE));

  gtk_settings_install_property (g_param_spec_int ("gtk-menu-bar-popup-delay",
						   P_("Delay before drop down menus appear"),
						   P_("Delay before the submenus of a menu bar appear"),
						   0,
						   G_MAXINT,
						   0,
						   G_PARAM_READWRITE));
}
 
GtkWidget*
gtk_menu_bar_new (void)
{
  return g_object_new (GTK_TYPE_MENU_BAR, NULL);
}

static void
gtk_menu_bar_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  GtkMenuBar *menu_bar;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;
  gint nchildren;
  GtkRequisition child_requisition;
  gint ipadding;

  g_return_if_fail (GTK_IS_MENU_BAR (widget));
  g_return_if_fail (requisition != NULL);

  requisition->width = 0;
  requisition->height = 0;
  
  if (GTK_WIDGET_VISIBLE (widget))
    {
      menu_bar = GTK_MENU_BAR (widget);
      menu_shell = GTK_MENU_SHELL (widget);

      nchildren = 0;
      children = menu_shell->children;

      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_WIDGET_VISIBLE (child))
	    {
              gint toggle_size;
              
	      GTK_MENU_ITEM (child)->show_submenu_indicator = FALSE;
	      gtk_widget_size_request (child, &child_requisition);
              gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (child),
                                                 &toggle_size);
              
	      requisition->width += child_requisition.width;
              requisition->width += toggle_size;
              
	      requisition->height = MAX (requisition->height, child_requisition.height);
	      nchildren += 1;
	    }
	}

      gtk_widget_style_get (widget, "internal_padding", &ipadding, NULL);
      
      requisition->width += (GTK_CONTAINER (menu_bar)->border_width +
                             ipadding + 
			     BORDER_SPACING) * 2;
      requisition->height += (GTK_CONTAINER (menu_bar)->border_width +
                              ipadding +
			      BORDER_SPACING) * 2;

      if (get_shadow_type (menu_bar) != GTK_SHADOW_NONE)
	{
	  requisition->width += widget->style->xthickness * 2;
	  requisition->height += widget->style->ythickness * 2;
	}
    }
}

static void
gtk_menu_bar_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkMenuBar *menu_bar;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;
  GtkAllocation child_allocation;
  GtkRequisition child_requisition;
  guint offset;
  GtkTextDirection direction;
  gint ltr_x;
  gint ipadding;

  g_return_if_fail (GTK_IS_MENU_BAR (widget));
  g_return_if_fail (allocation != NULL);

  menu_bar = GTK_MENU_BAR (widget);
  menu_shell = GTK_MENU_SHELL (widget);

  direction = gtk_widget_get_direction (widget);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

  gtk_widget_style_get (widget, "internal_padding", &ipadding, NULL);
  
  if (menu_shell->children)
    {
      child_allocation.x = (GTK_CONTAINER (menu_bar)->border_width +
			    ipadding + 
			    BORDER_SPACING);
      child_allocation.y = (GTK_CONTAINER (menu_bar)->border_width +
			    BORDER_SPACING);

      if (get_shadow_type (menu_bar) != GTK_SHADOW_NONE)
	{
	  child_allocation.x += widget->style->xthickness;
	  child_allocation.y += widget->style->ythickness;
	}
      
      child_allocation.height = MAX (1, (gint)allocation->height - child_allocation.y * 2);

      offset = child_allocation.x; 	/* Window edge to menubar start */
      ltr_x = child_allocation.x;

      children = menu_shell->children;
      while (children)
	{
          gint toggle_size;          

	  child = children->data;
	  children = children->next;

          gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (child),
                                             &toggle_size);
	  gtk_widget_get_child_requisition (child, &child_requisition);

          child_requisition.width += toggle_size;
          
	  /* Support for the right justified help menu */
	  if ((children == NULL) && (GTK_IS_MENU_ITEM(child))
	      && (GTK_MENU_ITEM(child)->right_justify)) 
	    {
	      ltr_x = allocation->width -
		child_requisition.width - offset;
	    }
	  if (GTK_WIDGET_VISIBLE (child))
	    {
	      if (direction == GTK_TEXT_DIR_LTR) 
		child_allocation.x = ltr_x;
	      else
		child_allocation.x = allocation->width -
		  child_requisition.width - ltr_x; 

	      child_allocation.width = child_requisition.width;

              gtk_menu_item_toggle_size_allocate (GTK_MENU_ITEM (child),
                                                  toggle_size);
	      gtk_widget_size_allocate (child, &child_allocation);

	      ltr_x += child_allocation.width;
	    }
	}
    }
}

static void
gtk_menu_bar_paint (GtkWidget *widget, GdkRectangle *area)
{
  g_return_if_fail (GTK_IS_MENU_BAR (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gint border;

      border = GTK_CONTAINER (widget)->border_width;
      
      gtk_paint_box (widget->style,
		     widget->window,
                     GTK_WIDGET_STATE (widget),
                     get_shadow_type (GTK_MENU_BAR (widget)),
		     area, widget, "menubar",
		     border, border,
		     widget->allocation.width - border * 2,
                     widget->allocation.height - border * 2);
    }
}

static gint
gtk_menu_bar_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  g_return_val_if_fail (GTK_IS_MENU_BAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_menu_bar_paint (widget, &event->area);

      (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
    }

  return FALSE;
}

static GList *
get_menu_bars (GtkWindow *window)
{
  return g_object_get_data (G_OBJECT (window), "gtk-menu-bar-list");
}

static GList *
get_viewable_menu_bars (GtkWindow *window)
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
	  if (!GTK_WIDGET_MAPPED (widget))
	    viewable = FALSE;
	  
	  widget = widget->parent;
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
  g_object_set_data (G_OBJECT (window), "gtk-menu-bar-list", menubars);
}

static gboolean
window_key_press_handler (GtkWidget   *widget,
                          GdkEventKey *event,
                          gpointer     data)
{
  gchar *accel = NULL;
  gboolean retval = FALSE;
  
  g_object_get (gtk_widget_get_settings (widget),
                "gtk-menu-bar-accel",
                &accel,
                NULL);

  if (accel)
    {
      guint keyval = 0;
      GdkModifierType mods = 0;

      gtk_accelerator_parse (accel, &keyval, &mods);

      if (keyval == 0)
        g_warning ("Failed to parse menu bar accelerator '%s'\n", accel);

      /* FIXME this is wrong, needs to be in the global accel resolution
       * thing, to properly consider i18n etc., but that probably requires
       * AccelGroup changes etc.
       */
      if (event->keyval == keyval &&
          ((event->state & gtk_accelerator_get_default_mod_mask ()) ==
	   (mods & gtk_accelerator_get_default_mod_mask ())))
        {
	  GList *tmp_menubars = get_viewable_menu_bars (GTK_WINDOW (widget));
	  GList *menubars;

	  menubars = _gtk_container_focus_sort (GTK_CONTAINER (widget), tmp_menubars,
						GTK_DIR_TAB_FORWARD, NULL);
	  g_list_free (tmp_menubars);
	  
	  if (menubars)
	    {
	      GtkMenuShell *menu_shell = GTK_MENU_SHELL (menubars->data);

	      _gtk_menu_shell_activate (menu_shell);
	      gtk_menu_shell_select_first (menu_shell, FALSE);
	      
	      g_list_free (menubars);
	      
	      retval = TRUE;	      
	    }
        }

      g_free (accel);
    }

  return retval;
}

static void
add_to_window (GtkWindow  *window,
               GtkMenuBar *menubar)
{
  GList *menubars = get_menu_bars (window);

  if (!menubars)
    {
      g_signal_connect (window,
			"key_press_event",
			G_CALLBACK (window_key_press_handler),
			NULL);
    }

  set_menu_bars (window, g_list_prepend (menubars, menubar));
}

static void
remove_from_window (GtkWindow  *window,
                    GtkMenuBar *menubar)
{
  GList *menubars = get_menu_bars (window);

  menubars = g_object_get_data (G_OBJECT (window),
				    "gtk-menu-bar-list");

  menubars = g_list_remove (menubars, menubar);

  if (!menubars)
    {
      g_signal_handlers_disconnect_by_func (window,
					    window_key_press_handler,
					    NULL);
    }

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
  
  if (GTK_WIDGET_TOPLEVEL (toplevel))
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

  if (GTK_WIDGET_TOPLEVEL (toplevel))
    {
      GList *tmp_menubars = get_viewable_menu_bars (GTK_WINDOW (toplevel));
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
	      if (new_menushell->children)
		to_activate = new_menushell->children->data;
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
			"shadow_type", &shadow_type,
			NULL);

  return shadow_type;
}

static gint
gtk_menu_bar_get_popup_delay (GtkMenuShell *menu_shell)
{
  gint popup_delay;
  
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (menu_shell)),
		"gtk-menu-bar-popup-delay", &popup_delay,
		NULL);

  return popup_delay;
}

#define __GTK_MENU_BAR_C__
#include "gtkaliasdef.c"
