/*
 * Copyright © 2011 Canonical Ltd.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkapplicationmenubutton.h"

#include "gtkapplicationwindow.h"
#include "gtkbutton.h"
#include "gtkmain.h"

#include <string.h>

/**
 * SECTION:gtkapplicationmenubutton
 * @title: GtkApplicationMenuButton
 * @short_description: A button that shows the application menu
 *
 * A GtkApplicationMenuButton can be added to a #GtkApplicationWindow
 * as an alternative way to present the application menu, if it is
 * not shown by the desktop environment. GtkApplicationMenuButton
 * automatically hides itself, and only appears when necessary. It
 * is derived from #GtkButton, and you should use regular #GtkButton
 * API to add a suitable icon or label. Note that #GtkApplicationWindow
 * already provides a way to present the application menu, so a
 * #GtkApplicationMenuButton is only needed if the default
 * appearance (as part of a menubar) is not suitable.
 *
 * To configure the contents of the application menu, use
 * g_application_set_menu() and g_application_set_action_group() on
 * the #GtkApplication associated with the #GtkApplicationWindow.
 */

struct _GtkApplicationMenuButton
{
  GtkButton parent_instance;

  GtkSettings *settings;

  gboolean user_shown;
  gboolean required;
};

typedef GtkButtonClass GtkApplicationMenuButtonClass;

G_DEFINE_TYPE (GtkApplicationMenuButton, gtk_application_menu_button, GTK_TYPE_BUTTON)

static void
gtk_application_menu_update_visibility (GtkApplicationMenuButton *amb)
{
  GtkWidget *widget = GTK_WIDGET (amb);
  gboolean should_be_visible;
  gboolean was_visible;

  should_be_visible = amb->user_shown && amb->required;
  was_visible = gtk_widget_get_visible (widget);

  if (!was_visible && should_be_visible)
    GTK_WIDGET_CLASS (gtk_application_menu_button_parent_class)->show (widget);

  else if (was_visible && !should_be_visible)
    GTK_WIDGET_CLASS (gtk_application_menu_button_parent_class)->hide (widget);
}

static void
gtk_application_menu_button_clicked (GtkButton *button)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
  if (toplevel != NULL && GTK_IS_APPLICATION_WINDOW (toplevel))
    {
      GtkWidget *menu;

      menu = gtk_application_window_get_app_menu (GTK_APPLICATION_WINDOW (toplevel));
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                      1, gtk_get_current_event_time ());
    }
}

static void
gtk_application_menu_button_show (GtkWidget *widget)
{
  GtkApplicationMenuButton *amb = GTK_APPLICATION_MENU_BUTTON (widget);

  amb->user_shown = TRUE;

  gtk_application_menu_update_visibility (amb);
}

static void
gtk_application_menu_button_hide (GtkWidget *widget)
{
  GtkApplicationMenuButton *amb = GTK_APPLICATION_MENU_BUTTON (widget);

  amb->user_shown = FALSE;

  gtk_application_menu_update_visibility (amb);
}

static void
gtk_application_menu_button_show_all (GtkWidget *widget)
{
  GtkApplicationMenuButton *amb = GTK_APPLICATION_MENU_BUTTON (widget);

  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_show_all, NULL);
  amb->user_shown = TRUE;

  gtk_application_menu_update_visibility (amb);
}

static void
gtk_application_menu_button_required_changed (GObject    *object,
                                              GParamSpec *pspec,
                                              gpointer    user_data)
{
  GtkApplicationMenuButton *amb = user_data;
  gboolean required;

  g_object_get (object, "gtk-shell-shows-app-menu", &required, NULL);
  required = !required;

  if (required != amb->required)
    {
      amb->required = required;

      gtk_application_menu_update_visibility (amb);
    }
}

static void
gtk_application_menu_button_screen_changed (GtkWidget *widget,
                                            GdkScreen *old_screen)
{
  GtkApplicationMenuButton *amb = GTK_APPLICATION_MENU_BUTTON (widget);
  GtkSettings *settings;
  GdkScreen *screen;

  screen = gtk_widget_get_screen (widget);
  settings = gtk_settings_get_for_screen (screen);

  if (settings != amb->settings)
    {
      if (amb->settings)
        {
          g_signal_handlers_disconnect_by_func (amb->settings, gtk_application_menu_button_required_changed, amb);
          g_object_unref (amb->settings);
        }

      amb->settings = g_object_ref (settings);
      g_signal_connect (amb->settings, "notify::gtk-shell-shows-app-menu",
                        G_CALLBACK (gtk_application_menu_button_required_changed), amb);
      gtk_application_menu_button_required_changed (G_OBJECT (settings), NULL, amb);
    }
}

static void
gtk_application_menu_button_hierarchy_changed (GtkWidget *widget,
                                               GtkWidget *previous_toplevel)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_APPLICATION_WINDOW (toplevel))
    gtk_application_window_set_show_app_menu (GTK_APPLICATION_WINDOW (toplevel), FALSE);
}

static void
gtk_application_menu_button_finalize (GObject *object)
{
  GtkApplicationMenuButton *amb = GTK_APPLICATION_MENU_BUTTON (object);

  if (amb->settings)
    {
      g_signal_handlers_disconnect_by_func (amb->settings, gtk_application_menu_button_required_changed, amb);
      g_object_unref (amb->settings);
    }

  G_OBJECT_CLASS (gtk_application_menu_button_parent_class)
    ->finalize (object);
}

static void
gtk_application_menu_button_init (GtkApplicationMenuButton *menu)
{
}

static void
gtk_application_menu_button_class_init (GtkApplicationMenuButtonClass *class)
{
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  button_class->clicked = gtk_application_menu_button_clicked;

  widget_class->show = gtk_application_menu_button_show;
  widget_class->show_all = gtk_application_menu_button_show_all;
  widget_class->hide = gtk_application_menu_button_hide;
  widget_class->screen_changed = gtk_application_menu_button_screen_changed;
  widget_class->hierarchy_changed = gtk_application_menu_button_hierarchy_changed;

  object_class->finalize = gtk_application_menu_button_finalize;
}

/**
 * gtk_application_menu_button_new:
 *
 * Creates a new #GtkApplicationMenuButton.
 *
 * Returns: a newly created #GtkApplicationMenuButton
 *
 * Since: 3.4
 */
GtkWidget *
gtk_application_menu_button_new (void)
{
  return g_object_new (GTK_TYPE_APPLICATION_MENU_BUTTON, NULL);
}
