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

#include "gdk/gdkkeysyms.h"
#include "gtkbindings.h"
#include "gtkmain.h"
#include "gtkmenubar.h"
#include "gtkmenuitem.h"
#include "gtksettings.h"
#include "gtkintl.h"
#include "gtkwindow.h"
#include "gtksignal.h"


#define BORDER_SPACING  0
#define CHILD_SPACING   3
#define DEFAULT_IPADDING 1

static void gtk_menu_bar_class_init    (GtkMenuBarClass *klass);
static void gtk_menu_bar_size_request  (GtkWidget       *widget,
					GtkRequisition  *requisition);
static void gtk_menu_bar_size_allocate (GtkWidget       *widget,
					GtkAllocation   *allocation);
static void gtk_menu_bar_paint         (GtkWidget       *widget,
					GdkRectangle    *area);
static gint gtk_menu_bar_expose        (GtkWidget       *widget,
					GdkEventExpose  *event);
static void gtk_menu_bar_hierarchy_changed (GtkWidget   *widget);                                            
static GtkShadowType get_shadow_type   (GtkMenuBar      *menubar);

static GtkMenuShellClass *parent_class = NULL;

GtkType
gtk_menu_bar_get_type (void)
{
  static GtkType menu_bar_type = 0;

  if (!menu_bar_type)
    {
      static const GtkTypeInfo menu_bar_info =
      {
	"GtkMenuBar",
	sizeof (GtkMenuBar),
	sizeof (GtkMenuBarClass),
	(GtkClassInitFunc) gtk_menu_bar_class_init,
	(GtkObjectInitFunc) NULL,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      menu_bar_type = gtk_type_unique (gtk_menu_shell_get_type (), &menu_bar_info);
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
  
  gtk_settings_install_property (g_param_spec_string ("gtk-menu-bar-accel",
                                                      _("Menu bar accelerator"),
                                                      _("Keybinding to activate the menu bar"),
                                                      "F10",
                                                      G_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_enum ("shadow_type",
                                                              _("Shadow type"),
                                                              _("Style of bevel around the menubar"),
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_OUT,
                                                              G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("internal_padding",
							     _("Internal padding"),
							     _("Amount of border space between the menubar shadow and the menu items"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_IPADDING,
                                                             G_PARAM_READABLE));

}
 
GtkWidget*
gtk_menu_bar_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_menu_bar_get_type ()));
}

void
gtk_menu_bar_append (GtkMenuBar *menu_bar,
		     GtkWidget  *child)
{
  gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), child);
}

void
gtk_menu_bar_prepend (GtkMenuBar *menu_bar,
		      GtkWidget  *child)
{
  gtk_menu_shell_prepend (GTK_MENU_SHELL (menu_bar), child);
}

void
gtk_menu_bar_insert (GtkMenuBar *menu_bar,
		     GtkWidget  *child,
		     gint        position)
{
  gtk_menu_shell_insert (GTK_MENU_SHELL (menu_bar), child, position);
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

  g_return_if_fail (widget != NULL);
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
	      /* Support for the right justified help menu */
	      if ((children == NULL) && GTK_IS_MENU_ITEM(child) &&
		  GTK_MENU_ITEM(child)->right_justify)
		{
		  requisition->width += CHILD_SPACING;
		}

	      nchildren += 1;
	    }
	}

      gtk_widget_style_get (widget, "internal_padding", &ipadding, NULL);
      
      requisition->width += (GTK_CONTAINER (menu_bar)->border_width +
			     widget->style->xthickness +
                             ipadding + 
			     BORDER_SPACING) * 2;
      requisition->height += (GTK_CONTAINER (menu_bar)->border_width +
			      widget->style->ythickness +
                              ipadding +
			      BORDER_SPACING) * 2;

      if (nchildren > 0)
	requisition->width += 2 * CHILD_SPACING * (nchildren - 1);
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
  gint ipadding;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_BAR (widget));
  g_return_if_fail (allocation != NULL);

  menu_bar = GTK_MENU_BAR (widget);
  menu_shell = GTK_MENU_SHELL (widget);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

  gtk_widget_style_get (widget, "internal_padding", &ipadding, NULL);
  
  if (menu_shell->children)
    {
      child_allocation.x = (GTK_CONTAINER (menu_bar)->border_width +
			    widget->style->xthickness +
                            ipadding + 
			    BORDER_SPACING);
      offset = child_allocation.x; 	/* Window edge to menubar start */

      child_allocation.y = (GTK_CONTAINER (menu_bar)->border_width +
			    widget->style->ythickness +
                            ipadding +
			    BORDER_SPACING);
      child_allocation.height = MAX (1, (gint)allocation->height - child_allocation.y * 2);

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
	  if ( (children == NULL) && (GTK_IS_MENU_ITEM(child))
	      && (GTK_MENU_ITEM(child)->right_justify)) 
	    {
	      child_allocation.x = allocation->width -
		  child_requisition.width - CHILD_SPACING - offset;
	    }
	  if (GTK_WIDGET_VISIBLE (child))
	    {
	      child_allocation.width = child_requisition.width;

              gtk_menu_item_toggle_size_allocate (GTK_MENU_ITEM (child),
                                                  toggle_size);
	      gtk_widget_size_allocate (child, &child_allocation);

	      child_allocation.x += child_allocation.width + CHILD_SPACING * 2;
	    }
	}
    }
}

static void
gtk_menu_bar_paint (GtkWidget *widget, GdkRectangle *area)
{
  g_return_if_fail (widget != NULL);
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
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_BAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_menu_bar_paint (widget, &event->area);

      (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
    }

  return FALSE;
}

static gboolean
window_key_press_handler (GtkWidget   *widget,
                          GdkEventKey *event,
                          gpointer     data)
{
  gchar *accel = NULL;
  gboolean retval = FALSE;
  
  g_object_get (G_OBJECT (gtk_widget_get_settings (widget)),
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
          (mods & event->state) == mods)
        {
          GtkMenuBar *menubar;
          GtkMenuShell *menushell;
          
          menubar = GTK_MENU_BAR (data);
          menushell = GTK_MENU_SHELL (menubar);

          if (menushell->children)
            {
              gtk_signal_emit_by_name (GTK_OBJECT (menushell->children->data),
                                       "activate_item");
              
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
  GtkMenuBar *old_menubar;

  old_menubar = g_object_get_data (G_OBJECT (window),
                                   "gtk-menu-bar");
  
  if (old_menubar)
    return; /* ignore this case; app programmer on crack, but
             * shouldn't spew stuff, just don't support the accel
             * for menubar #2
             */

  g_object_set_data (G_OBJECT (window),
                     "gtk-menu-bar",
                     menubar);

  g_signal_connect (G_OBJECT (window),
		    "key_press_event",
		    G_CALLBACK (window_key_press_handler),
		    menubar);

  menubar->toplevel = GTK_WIDGET (window);
}

static void
remove_from_window (GtkWindow  *window,
                    GtkMenuBar *menubar)
{
  g_return_if_fail (menubar->toplevel == GTK_WIDGET (window));

  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        G_CALLBACK (window_key_press_handler),
                                        menubar);

  /* dnotify zeroes menubar->toplevel */
  g_object_set_data (G_OBJECT (window),
                     "gtk-menu-bar",
                     NULL);

  menubar->toplevel = NULL;
}

static void
gtk_menu_bar_hierarchy_changed (GtkWidget *widget)
{
  GtkWidget *toplevel;  
  GtkMenuBar *menubar;

  menubar = GTK_MENU_BAR (widget);

  toplevel = gtk_widget_get_toplevel (widget);

  if (menubar->toplevel &&
      toplevel != menubar->toplevel)
    {
      remove_from_window (GTK_WINDOW (menubar->toplevel),
                          menubar);
    }
  
  if (toplevel &&
      GTK_IS_WINDOW (toplevel))
    {
      add_to_window (GTK_WINDOW (toplevel),
                     menubar);
    }
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
