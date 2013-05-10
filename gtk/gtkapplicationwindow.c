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
#include "gtkaccelgroup.h"
#include "gtkaccelmap.h"
#include "gtkmenubar.h"
#include "gtkintl.h"
#include "gtksettings.h"

#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#ifdef HAVE_GIO_UNIX
#include <gio/gdesktopappinfo.h>
#endif

/**
 * SECTION:gtkapplicationwindow
 * @title: GtkApplicationWindow
 * @short_description: GtkWindow subclass with GtkApplication support
 *
 * GtkApplicationWindow is a #GtkWindow subclass that offers some
 * extra functionality for better integration with #GtkApplication
 * features.  Notably, it can handle both the application menu as well
 * as the menubar. See gtk_application_set_app_menu() and
 * gtk_application_set_menubar().
 *
 * This class implements the #GActionGroup and #GActionMap interfaces,
 * to let you add window-specific actions that will be exported by the
 * associated #GtkApplication, together with its application-wide
 * actions.  Window-specific actions are prefixed with the "win."
 * prefix and application-wide actions are prefixed with the "app."
 * prefix.  Actions must be addressed with the prefixed name when
 * referring to them from a #GMenuModel.
 *
 * Note that widgets that are placed inside a GtkApplicationWindow
 * can also activate these actions, if they implement the
 * GtkActionable interface.
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
 * (see the #GtkApplication docs for some screenshots of how this
 * looks on different platforms).
 * This behaviour can be overridden with the #GtkApplicationWindow:show-menubar
 * property. If the desktop environment does not display the application
 * menu, then it will automatically be included in the menubar.
 *
 * <example><title>A GtkApplicationWindow with a menubar</title>
 * <programlisting><![CDATA[
 * app = gtk_application_new ();
 *
 * builder = gtk_builder_new ();
 * gtk_builder_add_from_string (builder,
 *     "<interface>"
 *     "  <menu id='menubar'>"
 *     "    <submenu label='_Edit'>"
 *     "      <item label='_Copy' action='win.copy'/>"
 *     "      <item label='_Paste' action='win.paste'/>"
 *     "    </submenu>"
 *     "  </menu>"
 *     "</interface>");
 * gtk_application_set_menubar (G_APPLICATION (app),
 *                              G_MENU_MODEL (gtk_builder_get_object (builder, "menubar")));
 * g_object_unref (builder);
 *
 * ...
 *
 * window = gtk_application_window_new (app);
 * ]]>
 * </programlisting>
 * </example>
 *
 * <example><title>Handling fallback yourself</title>
 * <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../examples/sunny.c">
 *  <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 * </programlisting>
 * </example>
 *
 * The XML format understood by #GtkBuilder for #GMenuModel consists
 * of a toplevel <tag class="starttag">menu</tag> element, which contains
 * one or more <tag class="starttag">item</tag> elements. Each
 * <tag class="starttag">item</tag> element contains
 * <tag class="starttag">attribute</tag> and <tag class="starttag">link</tag>
 * elements with a mandatory name attribute.
 * <tag class="starttag">link</tag> elements have the same content
 * model as <tag class="starttag">menu</tag>.
 *
 * Attribute values can be translated using gettext, like other #GtkBuilder
 * content. <tag class="starttag">attribute</tag> elements can be marked for
 * translation with a <literal>translatable="yes"</literal> attribute.
 * It is also possible to specify message context and translator comments,
 * using the context and comments attributes. To make use of this, the
 * #GtkBuilder must have been given the gettext domain to use.
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
  GtkAccelGroup *accels;
  GSList *accel_closures;
  guint accel_map_changed_id;

  GMenu *app_menu_section;
  GMenu *menubar_section;
  gboolean show_menubar;

  GDBusConnection *session;
  gchar           *object_path;
  guint            export_id;

  guint            id;
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

#ifdef HAVE_GIO_UNIX
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

  g_object_get (settings, "gtk-shell-shows-app-menu", &shown_by_shell, NULL);

  if (shown_by_shell)
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
          GMenuModel *app_menu;

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
          GMenuModel *menubar;

          menubar = gtk_application_get_menubar (gtk_window_get_application (GTK_WINDOW (window)));

          if (menubar != NULL)
            g_menu_append_section (window->priv->menubar_section, NULL, menubar);
        }
    }
}

typedef struct {
  GClosure closure;
  gchar *action_name;
  GVariant *parameter;
} AccelClosure;

static void
accel_activate (GClosure     *closure,
                GValue       *return_value,
                guint         n_param_values,
                const GValue *param_values,
                gpointer      invocation_hint,
                gpointer      marshal_data)
{
  AccelClosure *aclosure = (AccelClosure*)closure;
  GActionGroup *actions;

  actions = G_ACTION_GROUP (closure->data);
  if (g_action_group_get_action_enabled (actions, aclosure->action_name))
    {
       g_action_group_activate_action (actions, aclosure->action_name, aclosure->parameter);

      /* we handled the accelerator */
      g_value_set_boolean (return_value, TRUE);
    }
}

static void
free_accel_closures (GtkApplicationWindow *window)
{
  GSList *l;

  for (l = window->priv->accel_closures; l; l = l->next)
    {
       AccelClosure *closure = l->data;

       gtk_accel_group_disconnect (window->priv->accels, &closure->closure);

       g_object_unref (closure->closure.data);
       if (closure->parameter)
         g_variant_unref (closure->parameter);
       g_free (closure->action_name);
       g_closure_invalidate (&closure->closure);
       g_closure_unref (&closure->closure);
    }
  g_slist_free (window->priv->accel_closures);
  window->priv->accel_closures = NULL;
}

/* Hack. We iterate over the accel map instead of the actions,
 * in order to pull the parameters out of accel map entries
 */
static void
add_accel_closure (gpointer         data,
                   const gchar     *accel_path,
                   guint            accel_key,
                   GdkModifierType  accel_mods,
                   gboolean         changed)
{
  GtkApplicationWindow *window = data;
  GActionGroup *actions;
  const gchar *path;
  const gchar *p;
  gchar *action_name;
  GVariant *parameter;
  AccelClosure *closure;

  if (accel_key == 0)
    return;

  if (!g_str_has_prefix (accel_path, "<GAction>/"))
    return;

  path = accel_path + strlen ("<GAction>/");
  p = strchr (path, '/');
  if (p)
    {
      action_name = g_strndup (path, p - path);
      parameter = g_variant_parse (NULL, p + 1, NULL, NULL, NULL);
      if (!parameter)
        g_warning ("Failed to parse parameter from '%s'\n", accel_path);
    }
  else
    {
      action_name = g_strdup (path);
      parameter = NULL;
    }

  actions = G_ACTION_GROUP (_gtk_widget_get_action_muxer (GTK_WIDGET (window)));
  if (g_action_group_has_action (actions, action_name))
    {
      closure = (AccelClosure*) g_closure_new_object (sizeof (AccelClosure), g_object_ref (actions));
      g_closure_set_marshal (&closure->closure, accel_activate);

      closure->action_name = g_strdup (action_name);
      closure->parameter = parameter ? g_variant_ref_sink (parameter) : NULL;

      window->priv->accel_closures = g_slist_prepend (window->priv->accel_closures, g_closure_ref (&closure->closure));
      g_closure_sink (&closure->closure);

      gtk_accel_group_connect_by_path (window->priv->accels, accel_path, &closure->closure);
    }
  else if (parameter)
    {
      g_variant_unref (parameter);
    }

  g_free (action_name);
}

static void
gtk_application_window_update_accels (GtkApplicationWindow *window)
{
  free_accel_closures (window);

  gtk_accel_map_foreach (window, add_accel_closure);
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

  return g_action_group_query_action (G_ACTION_GROUP (window->priv->actions),
                                      action_name, enabled, parameter_type, state_type, state_hint, state);
}

static void
gtk_application_window_activate_action (GActionGroup *group,
                                        const gchar  *action_name,
                                        GVariant     *parameter)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);

  return g_action_group_activate_action (G_ACTION_GROUP (window->priv->actions), action_name, parameter);
}

static void
gtk_application_window_change_action_state (GActionGroup *group,
                                            const gchar  *action_name,
                                            GVariant     *state)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);

  return g_action_group_change_action_state (G_ACTION_GROUP (window->priv->actions), action_name, state);
}

static GAction *
gtk_application_window_lookup_action (GActionMap  *action_map,
                                      const gchar *action_name)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);

  return g_action_map_lookup_action (G_ACTION_MAP (window->priv->actions), action_name);
}

static void
gtk_application_window_add_action (GActionMap *action_map,
                                   GAction    *action)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);

  g_action_map_add_action (G_ACTION_MAP (window->priv->actions), action);
}

static void
gtk_application_window_remove_action (GActionMap  *action_map,
                                      const gchar *action_name)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);

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

      gtk_widget_get_preferred_width (window->priv->menubar, &menubar_min_width, &menubar_nat_width);
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

      gtk_widget_get_preferred_width_for_height (window->priv->menubar, menubar_height, &menubar_min_width, &menubar_nat_width);
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
      GtkAllocation menubar_allocation = *allocation;
      gint menubar_height;
      GtkWidget *child;

      _gtk_window_set_allocation (GTK_WINDOW (widget), allocation);

      gtk_widget_get_preferred_height_for_width (window->priv->menubar, allocation->width, &menubar_height, NULL);

      menubar_allocation.height = menubar_height;
      gtk_widget_size_allocate (window->priv->menubar, &menubar_allocation);

      child = gtk_bin_get_child (GTK_BIN (window));
      if (child != NULL && gtk_widget_get_visible (child))
        {
          GtkAllocation child_allocation = *allocation;
          gint border_width;

          border_width = gtk_container_get_border_width (GTK_CONTAINER (window));
          child_allocation.x += border_width;
          child_allocation.y += border_width + menubar_height;
          child_allocation.width = MAX (1, child_allocation.width - border_width * 2);
          child_allocation.height = MAX (1, child_allocation.height - border_width * 2 - menubar_height);

          gtk_widget_size_allocate (child, &child_allocation);
        }
    }
  else
    GTK_WIDGET_CLASS (gtk_application_window_parent_class)
      ->size_allocate (widget, allocation);
}

static void
gtk_application_window_real_realize (GtkWidget *widget)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);
  GtkApplication *application;
  GtkSettings *settings;

  application = gtk_window_get_application (GTK_WINDOW (window));
  settings = gtk_widget_get_settings (widget);

  g_signal_connect (settings, "notify::gtk-shell-shows-app-menu",
                    G_CALLBACK (gtk_application_window_shell_shows_app_menu_changed), window);
  g_signal_connect (settings, "notify::gtk-shell-shows-menubar",
                    G_CALLBACK (gtk_application_window_shell_shows_menubar_changed), window);

  gtk_application_window_update_shell_shows_app_menu (window, settings);
  gtk_application_window_update_shell_shows_menubar (window, settings);
  gtk_application_window_update_menubar (window);

  /* Update the accelerators, and ensure we do again
   * if the accel map changes */
  gtk_application_window_update_accels (window);
  window->priv->accel_map_changed_id = g_signal_connect_swapped (gtk_accel_map_get (), "changed",
								 G_CALLBACK (gtk_application_window_update_accels), window);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)
    ->realize (widget);

#ifdef GDK_WINDOWING_X11
  {
    GdkWindow *gdkwindow;

    gdkwindow = gtk_widget_get_window (GTK_WIDGET (window));

    if (GDK_IS_X11_WINDOW (gdkwindow) && window->priv->session)
      {
        gdk_x11_window_set_utf8_property (gdkwindow, "_GTK_APPLICATION_ID",
                                          g_application_get_application_id (G_APPLICATION (application)));

        gdk_x11_window_set_utf8_property (gdkwindow, "_GTK_UNIQUE_BUS_NAME",
                                          g_dbus_connection_get_unique_name (window->priv->session));

        gdk_x11_window_set_utf8_property (gdkwindow, "_GTK_APPLICATION_OBJECT_PATH",
                                          g_application_get_dbus_object_path (G_APPLICATION (application)));

        gdk_x11_window_set_utf8_property (gdkwindow, "_GTK_WINDOW_OBJECT_PATH",
                                          window->priv->object_path);

        gdk_x11_window_set_utf8_property (gdkwindow, "_GTK_APP_MENU_OBJECT_PATH",
                                          gtk_application_get_app_menu_object_path (application));

        gdk_x11_window_set_utf8_property (gdkwindow, "_GTK_MENUBAR_OBJECT_PATH",
                                          gtk_application_get_menubar_object_path (application));
      }
  }
#endif
}

static void
gtk_application_window_real_unrealize (GtkWidget *widget)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);
  GtkSettings *settings;

  settings = gtk_widget_get_settings (widget);

  g_signal_handlers_disconnect_by_func (settings, gtk_application_window_shell_shows_app_menu_changed, widget);
  g_signal_handlers_disconnect_by_func (settings, gtk_application_window_shell_shows_menubar_changed, widget);

  g_signal_handler_disconnect (gtk_accel_map_get (), window->priv->accel_map_changed_id);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)
    ->unrealize (widget);
}

gboolean
gtk_application_window_publish (GtkApplicationWindow *window,
                                GDBusConnection      *session,
                                const gchar          *object_path,
                                guint                 object_id)
{
  g_assert (window->priv->session == NULL);
  g_assert (window->priv->export_id == 0);
  g_assert (window->priv->object_path == NULL);
  g_assert (window->priv->id == 0);

  window->priv->export_id = g_dbus_connection_export_action_group (session, object_path,
                                                                   G_ACTION_GROUP (window->priv->actions),
                                                                   NULL);

  if (window->priv->export_id == 0)
    return FALSE;

  window->priv->session = session;
  window->priv->object_path = g_strdup (object_path);
  window->priv->id = object_id;

  return TRUE;
}

void
gtk_application_window_unpublish (GtkApplicationWindow *window)
{
  g_assert (window->priv->session != NULL);
  g_assert (window->priv->export_id != 0);
  g_assert (window->priv->object_path != NULL);
  g_assert (window->priv->id != 0);

  g_dbus_connection_unexport_action_group (window->priv->session, window->priv->export_id);
  window->priv->session = NULL;
  window->priv->export_id = 0;
  window->priv->id = 0;

  g_free (window->priv->object_path);
  window->priv->object_path = NULL;
}

static void
gtk_application_window_real_map (GtkWidget *widget)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);

  /* XXX could eliminate this by tweaking gtk_window_map */
  if (window->priv->menubar)
    gtk_widget_map (window->priv->menubar);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)
    ->map (widget);
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

  free_accel_closures (window);

  g_clear_object (&window->priv->app_menu_section);
  g_clear_object (&window->priv->menubar_section);
  g_clear_object (&window->priv->actions);
  g_clear_object (&window->priv->accels);

  G_OBJECT_CLASS (gtk_application_window_parent_class)
    ->dispose (object);
}

static void
gtk_application_window_init (GtkApplicationWindow *window)
{
  window->priv = G_TYPE_INSTANCE_GET_PRIVATE (window, GTK_TYPE_APPLICATION_WINDOW, GtkApplicationWindowPrivate);

  window->priv->actions = gtk_application_window_actions_new (window);
  window->priv->app_menu_section = g_menu_new ();
  window->priv->menubar_section = g_menu_new ();
  window->priv->accels = gtk_accel_group_new ();
  gtk_window_add_accel_group (GTK_WINDOW (window), window->priv->accels);

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
                          TRUE, G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);
  g_object_class_install_properties (object_class, N_PROPS, gtk_application_window_properties);
  g_type_class_add_private (class, sizeof (GtkApplicationWindowPrivate));
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

GtkAccelGroup *
gtk_application_window_get_accel_group (GtkApplicationWindow *window)
{
  return window->priv->accels;
}

/**
 * gtk_application_window_get_id:
 * @window: a #GtkApplicationWindow
 *
 * Returns the unique ID of the window. If the window has not yet been added to
 * a #GtkApplication, returns <literal>0</literal>.
 *
 * Returns: the unique ID for @window, or <literal>0</literal> if the window
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
