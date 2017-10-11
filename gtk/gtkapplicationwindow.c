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
#include "gtkmenubar.h"
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
 * #GtkApplicationWindow will automatically show a #GtkMenuBar for it.
 * This behaviour can be overridden with the #GtkApplicationWindow:show-menubar
 * property. If the desktop environment does not display the application
 * menu, then it will automatically be included in the menubar or in the
 * windows client-side decorations.
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
 *                                                             "menubar"));
 * gtk_application_set_menubar (GTK_APPLICATION (app), menubar);
 * g_object_unref (builder);
 *
 * // ...
 *
 * GtkWidget *window = gtk_application_window_new (app);
 * ]|
 *
 * ## Handling fallback yourself
 *
 * [A simple example](https://git.gnome.org/browse/gtk+/tree/examples/sunny.c)
 *
 * The XML format understood by #GtkBuilder for #GMenuModel consists
 * of a toplevel `<menu>` element, which contains one or more `<item>`
 * elements. Each `<item>` element contains `<attribute>` and `<link>`
 * elements with a mandatory name attribute. `<link>` elements have the
 * same content model as `<menu>`. Instead of `<link name="submenu>` or
 * `<link name="section">`, you can use `<submenu>` or `<section>`
 * elements.
 *
 * Attribute values can be translated using gettext, like other #GtkBuilder
 * content. `<attribute>` elements can be marked for translation with a
 * `translatable="yes"` attribute. It is also possible to specify message
 * context and translator comments, using the context and comments attributes.
 * To make use of this, the #GtkBuilder must have been given the gettext
 * domain to use.
 *
 * The following attributes are used when constructing menu items:
 * - "label": a user-visible string to display
 * - "action": the prefixed name of the action to trigger
 * - "target": the parameter to use when activating the action
 * - "icon" and "verb-icon": names of icons that may be displayed
 * - "submenu-action": name of an action that may be used to determine
 *      if a submenu can be opened
 * - "hidden-when": a string used to determine when the item will be hidden.
 *      Possible values include "action-disabled", "action-missing", "macos-menubar".
 *
 * The following attributes are used when constructing sections:
 * - "label": a user-visible string to use as section heading
 * - "display-hint": a string used to determine special formatting for the section.
 *     Possible values include "horizontal-buttons".
 * - "text-direction": a string used to determine the #GtkTextDirection to use
 *     when "display-hint" is set to "horizontal-buttons". Possible values
 *     include "rtl", "ltr", and "none".
 *
 * The following attributes are used when constructing submenus:
 * - "label": a user-visible string to display
 * - "icon": icon name to display
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

struct _GtkApplicationWindowPrivate
{
  GSimpleActionGroup *actions;
  GtkWidget *menubar;

  gboolean show_menubar;
  GMenu *app_menu_section;
  GMenu *menubar_section;

  guint            id;

  GtkShortcutsWindow *help_overlay;
};

static void
gtk_application_window_update_menubar (GtkApplicationWindow *window)
{
  gboolean should_have_menubar;
  gboolean have_menubar;

  have_menubar = window->priv->menubar != NULL;

  should_have_menubar = window->priv->show_menubar &&
                        (g_menu_model_get_n_items (G_MENU_MODEL (window->priv->app_menu_section)) ||
                         g_menu_model_get_n_items (G_MENU_MODEL (window->priv->menubar_section)));

  if (have_menubar && !should_have_menubar)
    {
      gtk_widget_unparent (window->priv->menubar);
      window->priv->menubar = NULL;

      gtk_widget_queue_resize (GTK_WIDGET (window));
    }

  if (!have_menubar && should_have_menubar)
    {
      GMenu *combined;

      combined = g_menu_new ();
      g_menu_append_section (combined, NULL, G_MENU_MODEL (window->priv->app_menu_section));
      g_menu_append_section (combined, NULL, G_MENU_MODEL (window->priv->menubar_section));

      window->priv->menubar = gtk_menu_bar_new_from_model (G_MENU_MODEL (combined));
      gtk_widget_set_parent (window->priv->menubar, GTK_WIDGET (window));
      gtk_widget_show_all (window->priv->menubar);
      g_object_unref (combined);

      gtk_widget_queue_resize (GTK_WIDGET (window));
    }
}

static gchar *
gtk_application_window_get_app_desktop_name (void)
{
  gchar *retval = NULL;

#if defined(HAVE_GIO_UNIX) && !defined(__APPLE__)
  GDesktopAppInfo *app_info;
  const gchar *app_name = NULL;
  gchar *desktop_file;

  desktop_file = g_strconcat (g_get_prgname (), ".desktop", NULL);
  app_info = g_desktop_app_info_new (desktop_file);
  g_free (desktop_file);

  if (app_info != NULL)
    app_name = g_app_info_get_name (G_APP_INFO (app_info));

  if (app_name != NULL)
    retval = g_strdup (app_name);

  g_clear_object (&app_info);
#endif /* HAVE_GIO_UNIX */

  return retval;
}

static void
gtk_application_window_update_shell_shows_app_menu (GtkApplicationWindow *window,
                                                    GtkSettings          *settings)
{
  gboolean shown_by_shell;
  gboolean shown_by_titlebar;

  g_object_get (settings, "gtk-shell-shows-app-menu", &shown_by_shell, NULL);

  shown_by_titlebar = _gtk_window_titlebar_shows_app_menu (GTK_WINDOW (window));

  if (shown_by_shell || shown_by_titlebar)
    {
      /* the shell shows it, so don't show it locally */
      if (g_menu_model_get_n_items (G_MENU_MODEL (window->priv->app_menu_section)) != 0)
        g_menu_remove (window->priv->app_menu_section, 0);
    }
  else
    {
      /* the shell does not show it, so make sure we show it */
      if (g_menu_model_get_n_items (G_MENU_MODEL (window->priv->app_menu_section)) == 0)
        {
          GMenuModel *app_menu = NULL;

          if (gtk_window_get_application (GTK_WINDOW (window)) != NULL)
            app_menu = gtk_application_get_app_menu (gtk_window_get_application (GTK_WINDOW (window)));

          if (app_menu != NULL)
            {
              const gchar *app_name;
              gchar *name;

              app_name = g_get_application_name ();
              if (app_name != g_get_prgname ())
                {
                  /* the app has set its application name, use it */
                  name = g_strdup (app_name);
                }
              else
                {
                  /* get the name from .desktop file */
                  name = gtk_application_window_get_app_desktop_name ();
                  if (name == NULL)
                    name = g_strdup (_("Application"));
                }

              g_menu_append_submenu (window->priv->app_menu_section, name, app_menu);
              g_free (name);
            }
        }
    }
}

static void
gtk_application_window_update_shell_shows_menubar (GtkApplicationWindow *window,
                                                   GtkSettings          *settings)
{
  gboolean shown_by_shell;

  g_object_get (settings, "gtk-shell-shows-menubar", &shown_by_shell, NULL);

  if (shown_by_shell)
    {
      /* the shell shows it, so don't show it locally */
      if (g_menu_model_get_n_items (G_MENU_MODEL (window->priv->menubar_section)) != 0)
        g_menu_remove (window->priv->menubar_section, 0);
    }
  else
    {
      /* the shell does not show it, so make sure we show it */
      if (g_menu_model_get_n_items (G_MENU_MODEL (window->priv->menubar_section)) == 0)
        {
          GMenuModel *menubar = NULL;

          if (gtk_window_get_application (GTK_WINDOW (window)) != NULL)
            menubar = gtk_application_get_menubar (gtk_window_get_application (GTK_WINDOW (window)));

          if (menubar != NULL)
            g_menu_append_section (window->priv->menubar_section, NULL, menubar);
        }
    }
}

static void
gtk_application_window_shell_shows_app_menu_changed (GObject    *object,
                                                     GParamSpec *pspec,
                                                     gpointer    user_data)
{
  GtkApplicationWindow *window = user_data;

  gtk_application_window_update_shell_shows_app_menu (window, GTK_SETTINGS (object));
  gtk_application_window_update_menubar (window);
}

static void
gtk_application_window_shell_shows_menubar_changed (GObject    *object,
                                                    GParamSpec *pspec,
                                                    gpointer    user_data)
{
  GtkApplicationWindow *window = user_data;

  gtk_application_window_update_shell_shows_menubar (window, GTK_SETTINGS (object));
  gtk_application_window_update_menubar (window);
}

static gchar **
gtk_application_window_list_actions (GActionGroup *group)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);

  /* may be NULL after dispose has run */
  if (!window->priv->actions)
    return g_new0 (char *, 0 + 1);

  return g_action_group_list_actions (G_ACTION_GROUP (window->priv->actions));
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

  if (!window->priv->actions)
    return FALSE;

  return g_action_group_query_action (G_ACTION_GROUP (window->priv->actions),
                                      action_name, enabled, parameter_type, state_type, state_hint, state);
}

static void
gtk_application_window_activate_action (GActionGroup *group,
                                        const gchar  *action_name,
                                        GVariant     *parameter)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);

  if (!window->priv->actions)
    return;

  g_action_group_activate_action (G_ACTION_GROUP (window->priv->actions), action_name, parameter);
}

static void
gtk_application_window_change_action_state (GActionGroup *group,
                                            const gchar  *action_name,
                                            GVariant     *state)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);

  if (!window->priv->actions)
    return;

  g_action_group_change_action_state (G_ACTION_GROUP (window->priv->actions), action_name, state);
}

static GAction *
gtk_application_window_lookup_action (GActionMap  *action_map,
                                      const gchar *action_name)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);

  if (!window->priv->actions)
    return NULL;

  return g_action_map_lookup_action (G_ACTION_MAP (window->priv->actions), action_name);
}

static void
gtk_application_window_add_action (GActionMap *action_map,
                                   GAction    *action)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);

  if (!window->priv->actions)
    return;

  g_action_map_add_action (G_ACTION_MAP (window->priv->actions), action);
}

static void
gtk_application_window_remove_action (GActionMap  *action_map,
                                      const gchar *action_name)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);

  if (!window->priv->actions)
    return;

  g_action_map_remove_action (G_ACTION_MAP (window->priv->actions), action_name);
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

G_DEFINE_TYPE_WITH_CODE (GtkApplicationWindow, gtk_application_window, GTK_TYPE_WINDOW,
                         G_ADD_PRIVATE (GtkApplicationWindow)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gtk_application_window_group_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_MAP, gtk_application_window_map_iface_init))

enum {
  PROP_0,
  PROP_SHOW_MENUBAR,
  N_PROPS
};
static GParamSpec *gtk_application_window_properties[N_PROPS];

static void
gtk_application_window_real_get_preferred_height (GtkWidget *widget,
                                                  gint      *minimum_height,
                                                  gint      *natural_height)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)
    ->get_preferred_height (widget, minimum_height, natural_height);

  if (window->priv->menubar != NULL)
    {
      gint menubar_min_height, menubar_nat_height;

      gtk_widget_get_preferred_height (window->priv->menubar, &menubar_min_height, &menubar_nat_height);
      *minimum_height += menubar_min_height;
      *natural_height += menubar_nat_height;
    }
}

static void
gtk_application_window_real_get_preferred_height_for_width (GtkWidget *widget,
                                                            gint       width,
                                                            gint      *minimum_height,
                                                            gint      *natural_height)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)
    ->get_preferred_height_for_width (widget, width, minimum_height, natural_height);

  if (window->priv->menubar != NULL)
    {
      gint menubar_min_height, menubar_nat_height;

      gtk_widget_get_preferred_height_for_width (window->priv->menubar, width, &menubar_min_height, &menubar_nat_height);
      *minimum_height += menubar_min_height;
      *natural_height += menubar_nat_height;
    }
}

static void
gtk_application_window_real_get_preferred_width (GtkWidget *widget,
                                                 gint      *minimum_width,
                                                 gint      *natural_width)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)
    ->get_preferred_width (widget, minimum_width, natural_width);

  if (window->priv->menubar != NULL)
    {
      gint menubar_min_width, menubar_nat_width;
      gint border_width;
      GtkBorder border = { 0 };

      gtk_widget_get_preferred_width (window->priv->menubar, &menubar_min_width, &menubar_nat_width);

      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
      _gtk_window_get_shadow_width (GTK_WINDOW (widget), &border);

      menubar_min_width += 2 * border_width + border.left + border.right;
      menubar_nat_width += 2 * border_width + border.left + border.right;

      *minimum_width = MAX (*minimum_width, menubar_min_width);
      *natural_width = MAX (*natural_width, menubar_nat_width);
    }
}

static void
gtk_application_window_real_get_preferred_width_for_height (GtkWidget *widget,
                                                            gint       height,
                                                            gint      *minimum_width,
                                                            gint      *natural_width)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);
  gint menubar_height;

  if (window->priv->menubar != NULL)
    gtk_widget_get_preferred_height (window->priv->menubar, &menubar_height, NULL);
  else
    menubar_height = 0;

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)
    ->get_preferred_width_for_height (widget, height - menubar_height, minimum_width, natural_width);

  if (window->priv->menubar != NULL)
    {
      gint menubar_min_width, menubar_nat_width;
      gint border_width;
      GtkBorder border = { 0 };

      gtk_widget_get_preferred_width_for_height (window->priv->menubar, menubar_height, &menubar_min_width, &menubar_nat_width);

      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
      _gtk_window_get_shadow_width (GTK_WINDOW (widget), &border);

      menubar_min_width += 2 * border_width + border.left + border.right;
      menubar_nat_width += 2 * border_width + border.left + border.right;

      *minimum_width = MAX (*minimum_width, menubar_min_width);
      *natural_width = MAX (*natural_width, menubar_nat_width);
    }
}

static void
gtk_application_window_real_size_allocate (GtkWidget     *widget,
                                           GtkAllocation *allocation)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);

  if (window->priv->menubar != NULL)
    {
      GtkAllocation menubar_allocation;
      GtkAllocation child_allocation;
      gint menubar_height;
      GtkWidget *child;

      _gtk_window_set_allocation (GTK_WINDOW (widget), allocation, &child_allocation);
      menubar_allocation = child_allocation;

      gtk_widget_get_preferred_height_for_width (window->priv->menubar,
                                                 menubar_allocation.width,
                                                 &menubar_height, NULL);

      menubar_allocation.height = menubar_height;
      gtk_widget_size_allocate (window->priv->menubar, &menubar_allocation);

      child_allocation.y += menubar_height;
      child_allocation.height -= menubar_height;
      child = gtk_bin_get_child (GTK_BIN (window));
      if (child != NULL && gtk_widget_get_visible (child))
        gtk_widget_size_allocate (child, &child_allocation);
    }
  else
    GTK_WIDGET_CLASS (gtk_application_window_parent_class)
      ->size_allocate (widget, allocation);
}

static void
gtk_application_window_real_realize (GtkWidget *widget)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);
  GtkSettings *settings;

  settings = gtk_widget_get_settings (widget);

  g_signal_connect (settings, "notify::gtk-shell-shows-app-menu",
                    G_CALLBACK (gtk_application_window_shell_shows_app_menu_changed), window);
  g_signal_connect (settings, "notify::gtk-shell-shows-menubar",
                    G_CALLBACK (gtk_application_window_shell_shows_menubar_changed), window);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)->realize (widget);

  gtk_application_window_update_shell_shows_app_menu (window, settings);
  gtk_application_window_update_shell_shows_menubar (window, settings);
  gtk_application_window_update_menubar (window);
}

static void
gtk_application_window_real_unrealize (GtkWidget *widget)
{
  GtkSettings *settings;

  settings = gtk_widget_get_settings (widget);

  g_signal_handlers_disconnect_by_func (settings, gtk_application_window_shell_shows_app_menu_changed, widget);
  g_signal_handlers_disconnect_by_func (settings, gtk_application_window_shell_shows_menubar_changed, widget);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)->unrealize (widget);
}

GActionGroup *
gtk_application_window_get_action_group (GtkApplicationWindow *window)
{
  return G_ACTION_GROUP (window->priv->actions);
}

static void
gtk_application_window_real_map (GtkWidget *widget)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);

  /* XXX could eliminate this by tweaking gtk_window_map */
  if (window->priv->menubar)
    gtk_widget_map (window->priv->menubar);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)->map (widget);
}

static void
gtk_application_window_real_unmap (GtkWidget *widget)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);

  /* XXX could eliminate this by tweaking gtk_window_unmap */
  if (window->priv->menubar)
    gtk_widget_unmap (window->priv->menubar);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)->unmap (widget);
}

static void
gtk_application_window_real_forall_internal (GtkContainer *container,
                                             gboolean      include_internal,
                                             GtkCallback   callback,
                                             gpointer      user_data)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (container);

  if (window->priv->menubar)
    callback (window->priv->menubar, user_data);

  GTK_CONTAINER_CLASS (gtk_application_window_parent_class)
    ->forall (container, include_internal, callback, user_data);
}

static void
gtk_application_window_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SHOW_MENUBAR:
      g_value_set_boolean (value, window->priv->show_menubar);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_application_window_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SHOW_MENUBAR:
      gtk_application_window_set_show_menubar (window, g_value_get_boolean (value));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_application_window_dispose (GObject *object)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (object);

  if (window->priv->menubar)
    {
      gtk_widget_unparent (window->priv->menubar);
      window->priv->menubar = NULL;
    }

  g_clear_object (&window->priv->app_menu_section);
  g_clear_object (&window->priv->menubar_section);

  if (window->priv->help_overlay)
    {
      gtk_widget_destroy (GTK_WIDGET (window->priv->help_overlay));
      g_clear_object (&window->priv->help_overlay);
    }

  G_OBJECT_CLASS (gtk_application_window_parent_class)->dispose (object);

  /* We do this below the chain-up above to give us a chance to be
   * removed from the GtkApplication (which is done in the dispose
   * handler of GtkWindow).
   *
   * That reduces our chances of being watched as a GActionGroup from a
   * muxer constructed by GtkApplication.
   */
  g_clear_object (&window->priv->actions);
}

static void
gtk_application_window_init (GtkApplicationWindow *window)
{
  window->priv = gtk_application_window_get_instance_private (window);

  window->priv->actions = gtk_application_window_actions_new (window);
  window->priv->app_menu_section = g_menu_new ();
  window->priv->menubar_section = g_menu_new ();

  gtk_widget_insert_action_group (GTK_WIDGET (window), "win", G_ACTION_GROUP (window->priv->actions));

  /* window->priv->actions is the one and only ref on the group, so when
   * we dispose, the action group will die, disconnecting all signals.
   */
  g_signal_connect_swapped (window->priv->actions, "action-added",
                            G_CALLBACK (g_action_group_action_added), window);
  g_signal_connect_swapped (window->priv->actions, "action-enabled-changed",
                            G_CALLBACK (g_action_group_action_enabled_changed), window);
  g_signal_connect_swapped (window->priv->actions, "action-state-changed",
                            G_CALLBACK (g_action_group_action_state_changed), window);
  g_signal_connect_swapped (window->priv->actions, "action-removed",
                            G_CALLBACK (g_action_group_action_removed), window);
}

static void
gtk_application_window_class_init (GtkApplicationWindowClass *class)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  container_class->forall = gtk_application_window_real_forall_internal;
  widget_class->get_preferred_height = gtk_application_window_real_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_application_window_real_get_preferred_height_for_width;
  widget_class->get_preferred_width = gtk_application_window_real_get_preferred_width;
  widget_class->get_preferred_width_for_height = gtk_application_window_real_get_preferred_width_for_height;
  widget_class->size_allocate = gtk_application_window_real_size_allocate;
  widget_class->realize = gtk_application_window_real_realize;
  widget_class->unrealize = gtk_application_window_real_unrealize;
  widget_class->map = gtk_application_window_real_map;
  widget_class->unmap = gtk_application_window_real_unmap;
  object_class->get_property = gtk_application_window_get_property;
  object_class->set_property = gtk_application_window_set_property;
  object_class->dispose = gtk_application_window_dispose;

  /**
   * GtkApplicationWindow:show-menubar:
   *
   * If this property is %TRUE, the window will display a menubar
   * that includes the app menu and menubar, unless these are
   * shown by the desktop shell. See gtk_application_set_app_menu()
   * and gtk_application_set_menubar().
   *
   * If %FALSE, the window will not display a menubar, regardless
   * of whether the desktop shell is showing the menus or not.
   */
  gtk_application_window_properties[PROP_SHOW_MENUBAR] =
    g_param_spec_boolean ("show-menubar",
                          P_("Show a menubar"),
                          P_("TRUE if the window should show a "
                             "menubar at the top of the window"),
                          TRUE, G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_properties (object_class, N_PROPS, gtk_application_window_properties);
}

/**
 * gtk_application_window_new:
 * @application: a #GtkApplication
 *
 * Creates a new #GtkApplicationWindow.
 *
 * Returns: a newly created #GtkApplicationWindow
 *
 * Since: 3.4
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
 * gtk_application_window_get_show_menubar:
 * @window: a #GtkApplicationWindow
 *
 * Returns whether the window will display a menubar for the app menu
 * and menubar as needed.
 *
 * Returns: %TRUE if @window will display a menubar when needed
 *
 * Since: 3.4
 */
gboolean
gtk_application_window_get_show_menubar (GtkApplicationWindow *window)
{
  return window->priv->show_menubar;
}

/**
 * gtk_application_window_set_show_menubar:
 * @window: a #GtkApplicationWindow
 * @show_menubar: whether to show a menubar when needed
 *
 * Sets whether the window will display a menubar for the app menu
 * and menubar as needed.
 *
 * Since: 3.4
 */
void
gtk_application_window_set_show_menubar (GtkApplicationWindow *window,
                                         gboolean              show_menubar)
{
  g_return_if_fail (GTK_IS_APPLICATION_WINDOW (window));

  show_menubar = !!show_menubar;

  if (window->priv->show_menubar != show_menubar)
    {
      window->priv->show_menubar = show_menubar;

      gtk_application_window_update_menubar (window);

      g_object_notify_by_pspec (G_OBJECT (window), gtk_application_window_properties[PROP_SHOW_MENUBAR]);
    }
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
 *
 * Since: 3.6
 */
guint
gtk_application_window_get_id (GtkApplicationWindow *window)
{
  g_return_val_if_fail (GTK_IS_APPLICATION_WINDOW (window), 0);

  return window->priv->id;
}

void
gtk_application_window_set_id (GtkApplicationWindow *window,
                               guint                 id)
{
  g_return_if_fail (GTK_IS_APPLICATION_WINDOW (window));
  window->priv->id = id;
}

static void
show_help_overlay (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  GtkApplicationWindow *window = user_data;

  if (window->priv->help_overlay)
    gtk_widget_show (GTK_WIDGET (window->priv->help_overlay));
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
 *
 * Since: 3.20
 */
void
gtk_application_window_set_help_overlay (GtkApplicationWindow *window,
                                         GtkShortcutsWindow   *help_overlay)
{
  g_return_if_fail (GTK_IS_APPLICATION_WINDOW (window));
  g_return_if_fail (help_overlay == NULL || GTK_IS_SHORTCUTS_WINDOW (help_overlay));

  if (window->priv->help_overlay)
    gtk_widget_destroy (GTK_WIDGET (window->priv->help_overlay));
  g_set_object (&window->priv->help_overlay, help_overlay);

  if (!window->priv->help_overlay)
    return;

  gtk_window_set_modal (GTK_WINDOW (help_overlay), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (help_overlay), GTK_WINDOW (window));
  gtk_shortcuts_window_set_window (help_overlay, GTK_WINDOW (window));

  g_signal_connect (help_overlay, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  if (!g_action_map_lookup_action (G_ACTION_MAP (window->priv->actions), "show-help-overlay"))
    {
      GSimpleAction *action;

      action = g_simple_action_new ("show-help-overlay", NULL);
      g_signal_connect (action, "activate", G_CALLBACK (show_help_overlay), window);

      g_action_map_add_action (G_ACTION_MAP (window->priv->actions), G_ACTION (action));
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
 *
 * Since: 3.20
 */
GtkShortcutsWindow *
gtk_application_window_get_help_overlay (GtkApplicationWindow *window)
{
  g_return_val_if_fail (GTK_IS_APPLICATION_WINDOW (window), NULL);

  return window->priv->help_overlay;
}
