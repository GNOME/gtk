/*
 * Copyright © 2011 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkapplicationwindow.h"

#include "gtkapplicationprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtkheaderbar.h"
#include "gtkintl.h"
#include "gtksettings.h"
#include "gtkshortcutswindowprivate.h"

#if defined(HAVE_GIO_UNIX) && !defined(__APPLE__)
#include <gio/gdesktopappinfo.h>
#endif

/**
 * SECTION:gtkapplicationwindow
 * @title: GtkApplicationWindow
 * @short_description: GtkWindow subclass with GtkApplication support
 *
 * #GtkApplicationWindow is a #GtkWindow subclass that offers some
 * extra functionality for better integration with #GtkApplication
 * features.  Notably, it can handle both the application menu as well
 * as the menubar. See gtk_application_set_app_menu() and
 * gtk_application_set_menubar().
 *
 * This class implements the #GActionGroup and #GActionMap interfaces,
 * to let you add window-specific actions that will be exported by the
 * associated #GtkApplication, together with its application-wide
 * actions.  Window-specific actions are prefixed with the “win.”
 * prefix and application-wide actions are prefixed with the “app.”
 * prefix.  Actions must be addressed with the prefixed name when
 * referring to them from a #GMenuModel.
 *
 * Note that widgets that are placed inside a #GtkApplicationWindow
 * can also activate these actions, if they implement the
 * #GtkActionable interface.
 *
 * As with #GtkApplication, the GDK lock will be acquired when
 * processing actions arriving from other processes and should therefore
 * be held when activating actions locally (if GDK threads are enabled).
 *
 * The settings #GtkSettings:gtk-shell-shows-app-menu and
 * #GtkSettings:gtk-shell-shows-menubar tell GTK+ whether the
 * desktop environment is showing the application menu and menubar
 * models outside the application as part of the desktop shell.
 * For instance, on OS X, both menus will be displayed remotely;
 * on Windows neither will be. gnome-shell (starting with version 3.4)
 * will display the application menu, but not the menubar.
 *
 * If the desktop environment does not display the menubar, then
 * #GtkApplicationWindow will automatically show a menubar for it.
 * This behaviour can be overridden with the #GtkApplicationWindow:show-menubar
 * property. If the desktop environment does not display the application
 * menu, then it will automatically be included in the menubar or in the
 * windows client-side decorations.
 *
 * See #GtkPopoverMenu for information about the XML language
 * used by #GtkBuilder for menu models.
 *
 * ## A GtkApplicationWindow with a menubar
 *
 * |[<!-- language="C" -->
 * GtkApplication *app = gtk_application_new ("org.gtk.test", 0);
 *
 * GtkBuilder *builder = gtk_builder_new_from_string (
 *     "<interface>"
 *     "  <menu id='menubar'>"
 *     "    <submenu label='_Edit'>"
 *     "      <item label='_Copy' action='win.copy'/>"
 *     "      <item label='_Paste' action='win.paste'/>"
 *     "    </submenu>"
 *     "  </menu>"
 *     "</interface>",
 *     -1);
 *
 * GMenuModel *menubar = G_MENU_MODEL (gtk_builder_get_object (builder,
 *                                                            "menubar"));
 * gtk_application_set_menubar (GTK_APPLICATION (app), menubar);
 * g_object_unref (builder);
 *
 * // ...
 *
 * GtkWidget *window = gtk_application_window_new (app);
 * ]|
 */

typedef GSimpleActionGroupClass GtkApplicationWindowActionsClass;
typedef struct
{
  GSimpleActionGroup parent_instance;
  GtkWindow *window;
} GtkApplicationWindowActions;

static GType gtk_application_window_actions_get_type   (void);
static void  gtk_application_window_actions_iface_init (GRemoteActionGroupInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GtkApplicationWindowActions, gtk_application_window_actions, G_TYPE_SIMPLE_ACTION_GROUP,
                         G_IMPLEMENT_INTERFACE (G_TYPE_REMOTE_ACTION_GROUP, gtk_application_window_actions_iface_init))

static void
gtk_application_window_actions_activate_action_full (GRemoteActionGroup *remote,
                                                     const gchar        *action_name,
                                                     GVariant           *parameter,
                                                     GVariant           *platform_data)
{
  GtkApplicationWindowActions *actions = (GtkApplicationWindowActions *) remote;
  GApplication *application;
  GApplicationClass *class;

  application = G_APPLICATION (gtk_window_get_application (actions->window));
  class = G_APPLICATION_GET_CLASS (application);

  class->before_emit (application, platform_data);
  g_action_group_activate_action (G_ACTION_GROUP (actions), action_name, parameter);
  class->after_emit (application, platform_data);
}

static void
gtk_application_window_actions_change_action_state_full (GRemoteActionGroup *remote,
                                                         const gchar        *action_name,
                                                         GVariant           *value,
                                                         GVariant           *platform_data)
{
  GtkApplicationWindowActions *actions = (GtkApplicationWindowActions *) remote;
  GApplication *application;
  GApplicationClass *class;

  application = G_APPLICATION (gtk_window_get_application (actions->window));
  class = G_APPLICATION_GET_CLASS (application);

  class->before_emit (application, platform_data);
  g_action_group_change_action_state (G_ACTION_GROUP (actions), action_name, value);
  class->after_emit (application, platform_data);
}

static void
gtk_application_window_actions_init (GtkApplicationWindowActions *actions)
{
}

static void
gtk_application_window_actions_iface_init (GRemoteActionGroupInterface *iface)
{
  iface->activate_action_full = gtk_application_window_actions_activate_action_full;
  iface->change_action_state_full = gtk_application_window_actions_change_action_state_full;
}

static void
gtk_application_window_actions_class_init (GtkApplicationWindowActionsClass *class)
{
}

static GSimpleActionGroup *
gtk_application_window_actions_new (GtkApplicationWindow *window)
{
  GtkApplicationWindowActions *actions;

  actions = g_object_new (gtk_application_window_actions_get_type (), NULL);
  actions->window = GTK_WINDOW (window);

  return G_SIMPLE_ACTION_GROUP (actions);
}

/* Now onto GtkApplicationWindow... */

typedef struct _GtkApplicationWindowPrivate GtkApplicationWindowPrivate;
struct _GtkApplicationWindowPrivate
{
  GSimpleActionGroup *actions;

  guint            id;

  GtkShortcutsWindow *help_overlay;
};

static void gtk_application_window_group_iface_init (GActionGroupInterface *iface);
static void gtk_application_window_map_iface_init (GActionMapInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkApplicationWindow, gtk_application_window, GTK_TYPE_WINDOW,
                         G_ADD_PRIVATE (GtkApplicationWindow)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gtk_application_window_group_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_MAP, gtk_application_window_map_iface_init))

static gchar **
gtk_application_window_list_actions (GActionGroup *group)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  /* may be NULL after dispose has run */
  if (!priv->actions)
    return g_new0 (char *, 0 + 1);

  return g_action_group_list_actions (G_ACTION_GROUP (priv->actions));
}

static gboolean
gtk_application_window_query_action (GActionGroup        *group,
                                     const gchar         *action_name,
                                     gboolean            *enabled,
                                     const GVariantType **parameter_type,
                                     const GVariantType **state_type,
                                     GVariant           **state_hint,
                                     GVariant           **state)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (!priv->actions)
    return FALSE;

  return g_action_group_query_action (G_ACTION_GROUP (priv->actions),
                                      action_name, enabled, parameter_type, state_type, state_hint, state);
}

static void
gtk_application_window_activate_action (GActionGroup *group,
                                        const gchar  *action_name,
                                        GVariant     *parameter)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (!priv->actions)
    return;

  g_action_group_activate_action (G_ACTION_GROUP (priv->actions), action_name, parameter);
}

static void
gtk_application_window_change_action_state (GActionGroup *group,
                                            const gchar  *action_name,
                                            GVariant     *state)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (!priv->actions)
    return;

  g_action_group_change_action_state (G_ACTION_GROUP (priv->actions), action_name, state);
}

static GAction *
gtk_application_window_lookup_action (GActionMap  *action_map,
                                      const gchar *action_name)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (!priv->actions)
    return NULL;

  return g_action_map_lookup_action (G_ACTION_MAP (priv->actions), action_name);
}

static void
gtk_application_window_add_action (GActionMap *action_map,
                                   GAction    *action)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (!priv->actions)
    return;

  g_action_map_add_action (G_ACTION_MAP (priv->actions), action);
}

static void
gtk_application_window_remove_action (GActionMap  *action_map,
                                      const gchar *action_name)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (!priv->actions)
    return;

  g_action_map_remove_action (G_ACTION_MAP (priv->actions), action_name);
}

static void
gtk_application_window_group_iface_init (GActionGroupInterface *iface)
{
  iface->list_actions = gtk_application_window_list_actions;
  iface->query_action = gtk_application_window_query_action;
  iface->activate_action = gtk_application_window_activate_action;
  iface->change_action_state = gtk_application_window_change_action_state;
}

static void
gtk_application_window_map_iface_init (GActionMapInterface *iface)
{
  iface->lookup_action = gtk_application_window_lookup_action;
  iface->add_action = gtk_application_window_add_action;
  iface->remove_action = gtk_application_window_remove_action;
}

GActionGroup *
gtk_application_window_get_action_group (GtkApplicationWindow *window)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  return G_ACTION_GROUP (priv->actions);
}

static void
gtk_application_window_dispose (GObject *object)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (object);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (priv->help_overlay)
    {
      gtk_window_destroy (GTK_WINDOW (priv->help_overlay));
      g_clear_object (&priv->help_overlay);
    }

  G_OBJECT_CLASS (gtk_application_window_parent_class)->dispose (object);

  /* We do this below the chain-up above to give us a chance to be
   * removed from the GtkApplication (which is done in the dispose
   * handler of GtkWindow).
   *
   * That reduces our chances of being watched as a GActionGroup from a
   * muxer constructed by GtkApplication.
   */
  g_clear_object (&priv->actions);
}

static void
gtk_application_window_init (GtkApplicationWindow *window)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  priv->actions = gtk_application_window_actions_new (window);

  gtk_widget_insert_action_group (GTK_WIDGET (window), "win", G_ACTION_GROUP (priv->actions));

  /* priv->actions is the one and only ref on the group, so when
   * we dispose, the action group will die, disconnecting all signals.
   */
  g_signal_connect_swapped (priv->actions, "action-added",
                            G_CALLBACK (g_action_group_action_added), window);
  g_signal_connect_swapped (priv->actions, "action-enabled-changed",
                            G_CALLBACK (g_action_group_action_enabled_changed), window);
  g_signal_connect_swapped (priv->actions, "action-state-changed",
                            G_CALLBACK (g_action_group_action_state_changed), window);
  g_signal_connect_swapped (priv->actions, "action-removed",
                            G_CALLBACK (g_action_group_action_removed), window);
}

static void
gtk_application_window_class_init (GtkApplicationWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gtk_application_window_dispose;
}

/**
 * gtk_application_window_new:
 * @application: a #GtkApplication
 *
 * Creates a new #GtkApplicationWindow.
 *
 * Returns: a newly created #GtkApplicationWindow
 */
GtkWidget *
gtk_application_window_new (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return g_object_new (GTK_TYPE_APPLICATION_WINDOW,
                       "application", application,
                       NULL);
}

/**
 * gtk_application_window_get_id:
 * @window: a #GtkApplicationWindow
 *
 * Returns the unique ID of the window. If the window has not yet been added to
 * a #GtkApplication, returns `0`.
 *
 * Returns: the unique ID for @window, or `0` if the window
 *   has not yet been added to a #GtkApplication
 */
guint
gtk_application_window_get_id (GtkApplicationWindow *window)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  g_return_val_if_fail (GTK_IS_APPLICATION_WINDOW (window), 0);

  return priv->id;
}

void
gtk_application_window_set_id (GtkApplicationWindow *window,
                               guint                 id)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  g_return_if_fail (GTK_IS_APPLICATION_WINDOW (window));
  priv->id = id;
}

static void
show_help_overlay (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  GtkApplicationWindow *window = user_data;
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (priv->help_overlay)
    gtk_widget_show (GTK_WIDGET (priv->help_overlay));
}

/**
 * gtk_application_window_set_help_overlay:
 * @window: a #GtkApplicationWindow
 * @help_overlay: (nullable): a #GtkShortcutsWindow
 *
 * Associates a shortcuts window with the application window, and
 * sets up an action with the name win.show-help-overlay to present
 * it.
 *
 * @window takes resposibility for destroying @help_overlay.
 */
void
gtk_application_window_set_help_overlay (GtkApplicationWindow *window,
                                         GtkShortcutsWindow   *help_overlay)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  g_return_if_fail (GTK_IS_APPLICATION_WINDOW (window));
  g_return_if_fail (help_overlay == NULL || GTK_IS_SHORTCUTS_WINDOW (help_overlay));

  if (priv->help_overlay)
    gtk_window_destroy (GTK_WINDOW (priv->help_overlay));
  g_set_object (&priv->help_overlay, help_overlay);

  if (!priv->help_overlay)
    return;

  gtk_window_set_modal (GTK_WINDOW (help_overlay), TRUE);
  gtk_window_set_hide_on_close (GTK_WINDOW (help_overlay), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (help_overlay), GTK_WINDOW (window));
  gtk_shortcuts_window_set_window (help_overlay, GTK_WINDOW (window));

  if (!g_action_map_lookup_action (G_ACTION_MAP (priv->actions), "show-help-overlay"))
    {
      GSimpleAction *action;

      action = g_simple_action_new ("show-help-overlay", NULL);
      g_signal_connect (action, "activate", G_CALLBACK (show_help_overlay), window);

      g_action_map_add_action (G_ACTION_MAP (priv->actions), G_ACTION (action));
      g_object_unref (G_OBJECT (action));
    }
}

/**
 * gtk_application_window_get_help_overlay:
 * @window: a #GtkApplicationWindow
 *
 * Gets the #GtkShortcutsWindow that has been set up with
 * a prior call to gtk_application_window_set_help_overlay().
 *
 * Returns: (transfer none) (nullable): the help overlay associated with @window, or %NULL
 */
GtkShortcutsWindow *
gtk_application_window_get_help_overlay (GtkApplicationWindow *window)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  g_return_val_if_fail (GTK_IS_APPLICATION_WINDOW (window), NULL);

  return priv->help_overlay;
}
