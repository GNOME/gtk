/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#include "config.h"

#include <string.h>

#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "gailoptionmenu.h"

static void                  gail_option_menu_class_init       (GailOptionMenuClass *klass);
static void                  gail_option_menu_init             (GailOptionMenu  *menu);
static void		     gail_option_menu_real_initialize  (AtkObject       *obj,
                                                                gpointer        data);

static gint                  gail_option_menu_get_n_children   (AtkObject       *obj);
static AtkObject*            gail_option_menu_ref_child        (AtkObject       *obj,
                                                                gint            i);
static gint                  gail_option_menu_real_add_gtk     (GtkContainer    *container,
                                                                GtkWidget       *widget,
                                                                gpointer        data);
static gint                  gail_option_menu_real_remove_gtk  (GtkContainer    *container,
                                                                GtkWidget       *widget,
                                                                gpointer        data);


static void                  atk_action_interface_init         (AtkActionIface  *iface);

static gboolean              gail_option_menu_do_action        (AtkAction       *action,
                                                                gint            i);
static gboolean              idle_do_action                    (gpointer        data);
static gint                  gail_option_menu_get_n_actions    (AtkAction       *action);
static const gchar*          gail_option_menu_get_description  (AtkAction       *action,
                                                                gint            i);
static const gchar*          gail_option_menu_action_get_name  (AtkAction       *action,
                                                                gint            i);
static gboolean              gail_option_menu_set_description  (AtkAction       *action,
                                                                gint            i,
                                                                const gchar     *desc);
static void                  gail_option_menu_changed          (GtkOptionMenu   *option_menu);

G_DEFINE_TYPE_WITH_CODE (GailOptionMenu, gail_option_menu, GAIL_TYPE_BUTTON,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init))

static void
gail_option_menu_class_init (GailOptionMenuClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GailContainerClass *container_class;

  container_class = (GailContainerClass *) klass;

  class->get_n_children = gail_option_menu_get_n_children;
  class->ref_child = gail_option_menu_ref_child;
  class->initialize = gail_option_menu_real_initialize;

  container_class->add_gtk = gail_option_menu_real_add_gtk;
  container_class->remove_gtk = gail_option_menu_real_remove_gtk;
}

static void
gail_option_menu_init (GailOptionMenu  *menu)
{
}

static void
gail_option_menu_real_initialize (AtkObject *obj,
                                  gpointer  data)
{
  GtkOptionMenu *option_menu;

  ATK_OBJECT_CLASS (gail_option_menu_parent_class)->initialize (obj, data);

  option_menu = GTK_OPTION_MENU (data);

  g_signal_connect (option_menu, "changed",
                    G_CALLBACK (gail_option_menu_changed), NULL);

  obj->role = ATK_ROLE_COMBO_BOX;
}

static gint
gail_option_menu_get_n_children (AtkObject *obj)
{
  GtkWidget *widget;
  GtkOptionMenu *option_menu;
  gint n_children = 0;

  g_return_val_if_fail (GAIL_IS_OPTION_MENU (obj), 0);

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return 0;

  option_menu = GTK_OPTION_MENU (widget);
  if (gtk_option_menu_get_menu (option_menu))
      n_children++;

  return n_children;;
}

static AtkObject*
gail_option_menu_ref_child (AtkObject *obj,
                            gint      i)
{
  GtkWidget *widget;
  AtkObject *accessible;

  g_return_val_if_fail (GAIL_IS_OPTION_MENU (obj), NULL);

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return NULL;


  if (i == 0)
    accessible = g_object_ref (gtk_widget_get_accessible (gtk_option_menu_get_menu (GTK_OPTION_MENU (widget))));
   else
    accessible = NULL;

  return accessible;
}

static gint
gail_option_menu_real_add_gtk (GtkContainer *container,
                               GtkWidget    *widget,
                               gpointer     data)
{
  AtkObject* atk_parent = ATK_OBJECT (data);
  AtkObject* atk_child = gtk_widget_get_accessible (widget);

  GAIL_CONTAINER_CLASS (gail_option_menu_parent_class)->add_gtk (container, widget, data);

  g_object_notify (G_OBJECT (atk_child), "accessible_parent");

  g_signal_emit_by_name (atk_parent, "children_changed::add",
			 1, atk_child, NULL);

  return 1;
}

static gint 
gail_option_menu_real_remove_gtk (GtkContainer *container,
                                  GtkWidget    *widget,
                                  gpointer     data)
{
  AtkPropertyValues values = { NULL };
  AtkObject* atk_parent = ATK_OBJECT (data);
  AtkObject *atk_child = gtk_widget_get_accessible (widget);

  g_value_init (&values.old_value, G_TYPE_POINTER);
  g_value_set_pointer (&values.old_value, atk_parent);

  values.property_name = "accessible-parent";
  g_signal_emit_by_name (atk_child,
                         "property_change::accessible-parent", &values, NULL);
  g_signal_emit_by_name (atk_parent, "children_changed::remove",
			 1, atk_child, NULL);

  return 1;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gail_option_menu_do_action;
  iface->get_n_actions = gail_option_menu_get_n_actions;
  iface->get_description = gail_option_menu_get_description;
  iface->get_name = gail_option_menu_action_get_name;
  iface->set_description = gail_option_menu_set_description;
}

static gboolean
gail_option_menu_do_action (AtkAction *action,
                            gint      i)
{
  GtkWidget *widget;
  GailButton *button; 
  gboolean return_value = TRUE;

  button = GAIL_BUTTON (action);
  widget = GTK_ACCESSIBLE (action)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;

  if (!gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  switch (i)
    {
    case 0:
      if (button->action_idle_handler)
        return_value = FALSE;
      else
        button->action_idle_handler = gdk_threads_add_idle (idle_do_action, button);
      break;
    default:
      return_value = FALSE;
      break;
    }
  return return_value; 
}

static gboolean 
idle_do_action (gpointer data)
{
  GtkButton *button; 
  GtkWidget *widget;
  GdkEvent tmp_event;
  GailButton *gail_button;

  gail_button = GAIL_BUTTON (data);
  gail_button->action_idle_handler = 0;

  widget = GTK_ACCESSIBLE (gail_button)->widget;
  if (widget == NULL /* State is defunct */ ||
      !gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  button = GTK_BUTTON (widget); 

  button->in_button = TRUE;
  g_signal_emit_by_name (button, "enter");
  /*
   * Simulate a button press event. calling gtk_button_pressed() does
   * not get the job done for a GtkOptionMenu.  
   */
  tmp_event.button.type = GDK_BUTTON_PRESS;
  tmp_event.button.window = widget->window;
  tmp_event.button.button = 1;
  tmp_event.button.send_event = TRUE;
  tmp_event.button.time = GDK_CURRENT_TIME;
  tmp_event.button.axes = NULL;

  gtk_widget_event (widget, &tmp_event);

  return FALSE;
}

static gint
gail_option_menu_get_n_actions (AtkAction *action)
{
  return 1;
}

static const gchar*
gail_option_menu_get_description (AtkAction *action,
                                  gint      i)
{
  GailButton *button;
  const gchar *return_value;

  button = GAIL_BUTTON (action);

  switch (i)
    {
    case 0:
      return_value = button->press_description;
      break;
    default:
      return_value = NULL;
      break;
    }
  return return_value; 
}

static const gchar*
gail_option_menu_action_get_name (AtkAction *action,
                                  gint      i)
{
  const gchar *return_value;

  switch (i)
    {
    case 0:
      /*
       * This action simulates a button press by simulating moving the
       * mouse into the button followed by pressing the left mouse button.
       */
      return_value = "press";
      break;
    default:
      return_value = NULL;
      break;
  }
  return return_value; 
}

static gboolean
gail_option_menu_set_description (AtkAction      *action,
                                  gint           i,
                                  const gchar    *desc)
{
  GailButton *button;
  gchar **value;

  button = GAIL_BUTTON (action);

  switch (i)
    {
    case 0:
      value = &button->press_description;
      break;
    default:
      value = NULL;
      break;
    }

  if (value)
    {
      g_free (*value);
      *value = g_strdup (desc);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gail_option_menu_changed (GtkOptionMenu   *option_menu)
{
  GailOptionMenu *gail_option_menu;

  gail_option_menu = GAIL_OPTION_MENU (gtk_widget_get_accessible (GTK_WIDGET (option_menu)));
  g_object_notify (G_OBJECT (gail_option_menu), "accessible-name");
}

