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
#include "gtkstack.h"
#include "gtkbox.h"
#include "gtkintl.h"


struct _GtkPopoverMenu
{
  GtkPopover parent_instance;
};

enum {
  CHILD_PROP_SUBMENU = 1
};

G_DEFINE_TYPE (GtkPopoverMenu, gtk_popover_menu, GTK_TYPE_POPOVER)

static void
gtk_popover_menu_init (GtkPopoverMenu *popover)
{
  GtkWidget *stack;

  stack = gtk_stack_new ();
  gtk_stack_set_vhomogeneous (GTK_STACK (stack), FALSE);
  gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
  gtk_widget_show (stack);
  gtk_container_add (GTK_CONTAINER (popover), stack);
}

static void
back_to_main (GtkWidget *popover)
{
  GtkWidget *stack;

  stack = gtk_bin_get_child (GTK_BIN (popover));
  gtk_stack_set_visible_child_name (GTK_STACK (stack), "main");
}

static void
gtk_popover_menu_map (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_popover_menu_parent_class)->map (widget);
  back_to_main (widget);
}

static void
gtk_popover_menu_unmap (GtkWidget *widget)
{
  back_to_main (widget);
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
      gtk_widget_set_parent (child, GTK_WIDGET (container));
      _gtk_bin_set_child (GTK_BIN (container), child);
    }
  else
    {
      gchar *name;

      if (gtk_stack_get_child_by_name (GTK_STACK (stack), "main"))
        name = "submenu";
      else
        name = "main";

      gtk_stack_add_named (GTK_STACK (stack), child, name);
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
gtk_popover_menu_get_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         property_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  GtkWidget *stack;

  stack = gtk_bin_get_child (GTK_BIN (container));

  if (child == stack)
    return;

  switch (property_id)
 
    {
    case CHILD_PROP_SUBMENU:
      {
        gchar *name;
        gtk_container_child_get (GTK_CONTAINER (stack), child, "name", &name, NULL);
        g_value_set_string (value, name);
      }
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec); 
      break;
    }
}

static void
gtk_popover_menu_set_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkWidget *stack;

  stack = gtk_bin_get_child (GTK_BIN (container));

  if (child == stack)
    return;

  switch (property_id)
    {
    case CHILD_PROP_SUBMENU:
      {
        const gchar *name;
        name = g_value_get_string (value);
        gtk_container_child_set (GTK_CONTAINER (stack), child, "name", name, NULL);
      }
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec); 
      break;
    }
}

static void
gtk_popover_menu_class_init (GtkPopoverMenuClass *klass)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->map = gtk_popover_menu_map;
  widget_class->unmap = gtk_popover_menu_unmap;

  container_class->add = gtk_popover_menu_add;
  container_class->remove = gtk_popover_menu_remove;
  container_class->set_child_property = gtk_popover_menu_set_child_property;
  container_class->get_child_property = gtk_popover_menu_get_child_property;

  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_SUBMENU,
                                              g_param_spec_string ("submenu",
                                                                   P_("Submenu"),
                                                                   P_("The name of the submenu"),
                                                                   NULL,
                                                                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

GtkWidget *
gtk_popover_menu_new (void)
{
  return g_object_new (GTK_TYPE_POPOVER_MENU, NULL);
}
