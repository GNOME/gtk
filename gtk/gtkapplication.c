/*
 * Copyright © 2010 Codethink Limited
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkapplication.h"

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>

#include "gtkapplicationprivate.h"
#include "gtkmarshalers.h"
#include "gtkmain.h"
#include "gtkaccelmapprivate.h"
#include "gactionmuxer.h"

#ifdef GDK_WINDOWING_QUARTZ
#include "gtkquartz-menu.h"
#import <Cocoa/Cocoa.h>
#endif

#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

/**
 * SECTION:gtkapplication
 * @title: GtkApplication
 * @short_description: Application class
 *
 * #GtkApplication is a class that handles many important aspects
 * of a GTK+ application in a convenient fashion, without enforcing
 * a one-size-fits-all application model.
 *
 * Currently, GtkApplication handles GTK+ initialization, application
 * uniqueness, provides some basic scriptability and desktop shell integration
 * by exporting actions and menus and manages a list of toplevel windows whose
 * life-cycle is automatically tied to the life-cycle of your application.
 *
 * While GtkApplication works fine with plain #GtkWindows, it is recommended
 * to use it together with #GtkApplicationWindow.
 *
 * When GDK threads are enabled, GtkApplication will acquire the GDK
 * lock when invoking actions that arrive from other processes.  The GDK
 * lock is not touched for local action invocations.  In order to have
 * actions invoked in a predictable context it is therefore recommended
 * that the GDK lock be held while invoking actions locally with
 * g_action_group_activate_action().  The same applies to actions
 * associated with #GtkApplicationWindow and to the 'activate' and
 * 'open' #GApplication methods.
 *
 * To set an application menu on a GtkApplication, use
 * g_application_set_app_menu(). The #GMenuModel that this function
 * expects is usually constructed using #GtkBuilder, as seen in the
 * following example. To set a menubar that will be automatically picked
 * up by #GApplicationWindows, use g_application_set_menubar(). GTK+
 * makes these menus appear as expected, depending on the platform
 * the application is running on.
 *
 * <figure label="Menu integration in OS X">
 * <graphic fileref="bloatpad-osx.png" format="PNG"/>
 * </figure>
 *
 * <figure label="Menu integration in GNOME">
 * <graphic fileref="bloatpad-gnome.png" format="PNG"/>
 * </figure>
 *
 * <figure label="Menu integration in Xfce">
 * <graphic fileref="bloatpad-xfce.png" format="PNG"/>
 * </figure>
 *
 * <example id="gtkapplication"><title>A simple application</title>
 * <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../examples/bloatpad.c">
 *  <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 * </programlisting>
 * </example>
 */

enum {
  WINDOW_ADDED,
  WINDOW_REMOVED,
  LAST_SIGNAL
};

static guint gtk_application_signals[LAST_SIGNAL];

G_DEFINE_TYPE (GtkApplication, gtk_application, G_TYPE_APPLICATION)

struct _GtkApplicationPrivate
{
  GList *windows;

#ifdef GDK_WINDOWING_X11
  GDBusConnection *session_bus;
  gchar *window_prefix;
  guint next_id;
#endif

#ifdef GDK_WINDOWING_QUARTZ
  GActionMuxer *muxer;
  GMenu *combined;
#endif
};

#ifdef GDK_WINDOWING_X11
static void
gtk_application_window_added_x11 (GtkApplication *application,
                                  GtkWindow      *window)
{
  if (application->priv->session_bus == NULL)
    return;

  if (GTK_IS_APPLICATION_WINDOW (window))
    {
      GtkApplicationWindow *app_window = GTK_APPLICATION_WINDOW (window);
      gboolean success;

      /* GtkApplicationWindow associates with us when it is first created,
       * so surely it's not realized yet...
       */
      g_assert (!gtk_widget_get_realized (GTK_WIDGET (window)));

      do
        {
          gchar *window_path;
          guint window_id;

          window_id = application->priv->next_id++;
          window_path = g_strdup_printf ("%s%d", application->priv->window_prefix, window_id);
          success = gtk_application_window_publish (app_window, application->priv->session_bus, window_path);
          g_free (window_path);
        }
      while (!success);
    }
}

static void
gtk_application_window_removed_x11 (GtkApplication *application,
                                    GtkWindow      *window)
{
  if (application->priv->session_bus == NULL)
    return;

  if (GTK_IS_APPLICATION_WINDOW (window))
    gtk_application_window_unpublish (GTK_APPLICATION_WINDOW (window));
}

static gchar *
window_prefix_from_appid (const gchar *appid)
{
  gchar *appid_path, *iter;

  appid_path = g_strconcat ("/", appid, "/windows/", NULL);
  for (iter = appid_path; *iter; iter++)
    {
      if (*iter == '.')
        *iter = '/';

      if (*iter == '-')
        *iter = '_';
    }

  return appid_path;
}

static void
gtk_application_startup_x11 (GtkApplication *application)
{
  const gchar *application_id;

  application_id = g_application_get_application_id (G_APPLICATION (application));
  application->priv->session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  application->priv->window_prefix = window_prefix_from_appid (application_id);
}

static void
gtk_application_shutdown_x11 (GtkApplication *application)
{
  g_free (application->priv->window_prefix);
  application->priv->window_prefix = NULL;
  g_clear_object (&application->priv->session_bus);
}
#endif

#ifdef GDK_WINDOWING_QUARTZ
static void
gtk_application_menu_changed_quartz (GObject    *object,
                                     GParamSpec *pspec,
                                     gpointer    user_data)
{
  GtkApplication *application = GTK_APPLICATION (object);
  GMenu *combined;

  combined = g_menu_new ();
  g_menu_append_submenu (combined, "Application", g_application_get_app_menu (application));
  g_menu_append_section (combined, NULL, gtk_application_get_menubar (application));

  gtk_quartz_set_main_menu (G_MENU_MODEL (combined), G_ACTION_OBSERVABLE (application->priv->muxer));
}

static void
gtk_application_startup_quartz (GtkApplication *application)
{
  [NSApp finishLaunching];

  application->priv->muxer = g_action_muxer_new ();
  g_action_muxer_insert (application->priv->muxer, "app", G_ACTION_GROUP (application));

  g_signal_connect (application, "notify::app-menu", G_CALLBACK (gtk_application_menu_changed_quartz), NULL);
  g_signal_connect (application, "notify::menubar", G_CALLBACK (gtk_application_menu_changed_quartz), NULL);
  gtk_application_menu_changed_quartz (G_OBJECT (application), NULL, NULL);
}

static void
gtk_application_shutdown_quartz (GtkApplication *application)
{
  g_signal_handlers_disconnect_by_func (application, gtk_application_menu_changed_quartz, NULL);

  g_object_unref (application->priv->muxer);
  application->priv->muxer = NULL;
}

static void
gtk_application_focus_changed (GtkApplication *application,
                               GtkWindow      *window)
{
  if (G_IS_ACTION_GROUP (window))
    g_action_muxer_insert (application->priv->muxer, "win", G_ACTION_GROUP (window));
  else
    g_action_muxer_remove (application->priv->muxer, "win");
}
#endif

static gboolean
gtk_application_focus_in_event_cb (GtkWindow      *window,
                                   GdkEventFocus  *event,
                                   GtkApplication *application)
{
  GtkApplicationPrivate *priv = application->priv;
  GList *link;

  /* Keep the window list sorted by most-recently-focused. */
  link = g_list_find (priv->windows, window);
  if (link != NULL && link != priv->windows)
    {
      priv->windows = g_list_remove_link (priv->windows, link);
      priv->windows = g_list_concat (link, priv->windows);
    }

#ifdef GDK_WINDOWING_QUARTZ
  gtk_application_focus_changed (application, window);
#endif

  return FALSE;
}

static void
gtk_application_startup (GApplication *application)
{
  G_APPLICATION_CLASS (gtk_application_parent_class)
    ->startup (application);

  gtk_init (0, 0);

#ifdef GDK_WINDOWING_X11
  gtk_application_startup_x11 (GTK_APPLICATION (application));
#endif

#ifdef GDK_WINDOWING_QUARTZ
  gtk_application_startup_quartz (GTK_APPLICATION (application));
#endif
}

static void
gtk_application_shutdown (GApplication *application)
{
#ifdef GDK_WINDOWING_X11
  gtk_application_shutdown_x11 (GTK_APPLICATION (application));
#endif

#ifdef GDK_WINDOWING_QUARTZ
  gtk_application_shutdown_quartz (GTK_APPLICATION (application));
#endif

  G_APPLICATION_CLASS (gtk_application_parent_class)
    ->shutdown (application);
}

static void
gtk_application_add_platform_data (GApplication    *application,
                                   GVariantBuilder *builder)
{
  const gchar *startup_id;

  startup_id = getenv ("DESKTOP_STARTUP_ID");
  
  if (startup_id && g_utf8_validate (startup_id, -1, NULL))
    g_variant_builder_add (builder, "{sv}", "desktop-startup-id",
                           g_variant_new_string (startup_id));
}

static void
gtk_application_before_emit (GApplication *application,
                             GVariant     *platform_data)
{
  GVariantIter iter;
  const gchar *key;
  GVariant *value;

  gdk_threads_enter ();

  g_variant_iter_init (&iter, platform_data);
  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value))
    {
#ifdef GDK_WINDOWING_X11
      if (strcmp (key, "desktop-startup-id") == 0)
        {
          GdkDisplay *display;
          const gchar *id;

          display = gdk_display_get_default ();
          id = g_variant_get_string (value, NULL);
          if (GDK_IS_X11_DISPLAY (display))
            gdk_x11_display_set_startup_notification_id (display, id);
       }
#endif
    }
}

static void
gtk_application_after_emit (GApplication *application,
                            GVariant     *platform_data)
{
  gdk_notify_startup_complete ();

  gdk_threads_leave ();
}

static void
gtk_application_init (GtkApplication *application)
{
  application->priv = G_TYPE_INSTANCE_GET_PRIVATE (application,
                                                   GTK_TYPE_APPLICATION,
                                                   GtkApplicationPrivate);
}

static void
gtk_application_window_added (GtkApplication *application,
                              GtkWindow      *window)
{
  GtkApplicationPrivate *priv = application->priv;

  priv->windows = g_list_prepend (priv->windows, window);
  gtk_window_set_application (window, application);
  g_application_hold (G_APPLICATION (application));

  g_signal_connect (window, "focus-in-event",
                    G_CALLBACK (gtk_application_focus_in_event_cb),
                    application);

#ifdef GDK_WINDOWING_X11
  gtk_application_window_added_x11 (application, window);
#endif
}

static void
gtk_application_window_removed (GtkApplication *application,
                                GtkWindow      *window)
{
  GtkApplicationPrivate *priv = application->priv;

#ifdef GDK_WINDOWING_X11
  gtk_application_window_removed_x11 (application, window);
#endif

  g_signal_handlers_disconnect_by_func (window,
                                        gtk_application_focus_in_event_cb,
                                        application);

  g_application_release (G_APPLICATION (application));
  priv->windows = g_list_remove (priv->windows, window);
  gtk_window_set_application (window, NULL);
}

static void
extract_accel_from_menu_item (GMenuModel     *model,
                              gint            item,
                              GtkApplication *app)
{
  GMenuAttributeIter *iter;
  const gchar *key;
  GVariant *value;
  const gchar *accel = NULL;
  const gchar *action = NULL;
  GVariant *target = NULL;

  iter = g_menu_model_iterate_item_attributes (model, item);
  while (g_menu_attribute_iter_get_next (iter, &key, &value))
    {
      if (g_str_equal (key, "action") && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        action = g_variant_get_string (value, NULL);
      else if (g_str_equal (key, "accel") && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        accel = g_variant_get_string (value, NULL);
      else if (g_str_equal (key, "target"))
        target = g_variant_ref (value);
      g_variant_unref (value);
    }
  g_object_unref (iter);

  if (accel && action)
    gtk_application_add_accelerator (app, accel, action, target);

  if (target)
    g_variant_unref (target);
}

static void
extract_accels_from_menu (GMenuModel     *model,
                          GtkApplication *app)
{
  gint i;
  GMenuLinkIter *iter;
  const gchar *key;
  GMenuModel *m;

  for (i = 0; i < g_menu_model_get_n_items (model); i++)
    {
      extract_accel_from_menu_item (model, i, app);

      iter = g_menu_model_iterate_item_links (model, i);
      while (g_menu_link_iter_get_next (iter, &key, &m))
        {
          extract_accels_from_menu (m, app);
          g_object_unref (m);
        }
      g_object_unref (iter);
    }
}

static void
gtk_application_notify (GObject    *object,
                        GParamSpec *pspec)
{
  if (strcmp (pspec->name, "app-menu") == 0 ||
      strcmp (pspec->name, "menubar") == 0)
    {
      GMenuModel *model;
      g_object_get (object, pspec->name, &model, NULL);
      if (model)
        {
          extract_accels_from_menu (model, GTK_APPLICATION (object));
          g_object_unref (model);
        }
    }

  if (G_OBJECT_CLASS (gtk_application_parent_class)->notify)
    G_OBJECT_CLASS (gtk_application_parent_class)->notify (object, pspec);
}

static void
gtk_application_class_init (GtkApplicationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  object_class->notify = gtk_application_notify;

  application_class->add_platform_data = gtk_application_add_platform_data;
  application_class->before_emit = gtk_application_before_emit;
  application_class->after_emit = gtk_application_after_emit;
  application_class->startup = gtk_application_startup;
  application_class->shutdown = gtk_application_shutdown;

  class->window_added = gtk_application_window_added;
  class->window_removed = gtk_application_window_removed;

  g_type_class_add_private (class, sizeof (GtkApplicationPrivate));

  /**
   * GtkApplication::window-added:
   * @application: the #GtkApplication which emitted the signal
   * @window: the newly-added #GtkWindow
   *
   * Emitted when a #GtkWindow is added to @application through
   * gtk_application_add_window().
   *
   * Since: 3.2
   */
  gtk_application_signals[WINDOW_ADDED] =
    g_signal_new ("window-added", GTK_TYPE_APPLICATION, G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkApplicationClass, window_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_WINDOW);

  /**
   * GtkApplication::window-removed:
   * @application: the #GtkApplication which emitted the signal
   * @window: the #GtkWindow that is being removed
   *
   * Emitted when a #GtkWindow is removed from @application,
   * either as a side-effect of being destroyed or explicitly
   * through gtk_application_remove_window().
   *
   * Since: 3.2
   */
  gtk_application_signals[WINDOW_REMOVED] =
    g_signal_new ("window-removed", GTK_TYPE_APPLICATION, G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkApplicationClass, window_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_WINDOW);
}

/**
 * gtk_application_new:
 * @application_id: the application id
 * @flags: the application flags
 *
 * Creates a new #GtkApplication instance.
 *
 * This function calls g_type_init() for you. gtk_init() is called
 * as soon as the application gets registered as the primary instance.
 *
 * Note that commandline arguments are not passed to gtk_init().
 * All GTK+ functionality that is available via commandline arguments
 * can also be achieved by setting suitable environment variables
 * such as <envvar>G_DEBUG</envvar>, so this should not be a big
 * problem. If you absolutely must support GTK+ commandline arguments,
 * you can explicitly call gtk_init() before creating the application
 * instance.
 *
 * The application id must be valid. See g_application_id_is_valid().
 *
 * Returns: a new #GtkApplication instance
 */
GtkApplication *
gtk_application_new (const gchar       *application_id,
                     GApplicationFlags  flags)
{
  g_return_val_if_fail (g_application_id_is_valid (application_id), NULL);

  g_type_init ();

  return g_object_new (GTK_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       NULL);
}

/**
 * gtk_application_add_window:
 * @application: a #GtkApplication
 * @window: a #GtkWindow
 *
 * Adds a window from @application.
 *
 * This call is equivalent to setting the #GtkWindow:application
 * property of @window to @application.
 *
 * Normally, the connection between the application and the window
 * will remain until the window is destroyed, but you can explicitly
 * remove it with gtk_application_remove_window().
 *
 * GTK+ will keep the application running as long as it has
 * any windows.
 *
 * Since: 3.0
 **/
void
gtk_application_add_window (GtkApplication *application,
                            GtkWindow      *window)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));

  if (!g_list_find (application->priv->windows, window))
    g_signal_emit (application,
                   gtk_application_signals[WINDOW_ADDED], 0, window);
}

/**
 * gtk_application_remove_window:
 * @application: a #GtkApplication
 * @window: a #GtkWindow
 *
 * Remove a window from @application.
 *
 * If @window belongs to @application then this call is equivalent to
 * setting the #GtkWindow:application property of @window to
 * %NULL.
 *
 * The application may stop running as a result of a call to this
 * function.
 *
 * Since: 3.0
 **/
void
gtk_application_remove_window (GtkApplication *application,
                               GtkWindow      *window)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));

  if (g_list_find (application->priv->windows, window))
    g_signal_emit (application,
                   gtk_application_signals[WINDOW_REMOVED], 0, window);
}

/**
 * gtk_application_get_windows:
 * @application: a #GtkApplication
 *
 * Gets a list of the #GtkWindows associated with @application.
 *
 * The list is sorted by most recently focused window, such that the first
 * element is the currently focused window. (Useful for choosing a parent
 * for a transient window.)
 *
 * The list that is returned should not be modified in any way. It will
 * only remain valid until the next focus change or window creation or
 * deletion.
 *
 * Returns: (element-type GtkWindow) (transfer none): a #GList of #GtkWindow
 *
 * Since: 3.0
 **/
GList *
gtk_application_get_windows (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return application->priv->windows;
}

/**
 * gtk_application_add_accelerator:
 * @application: a #GtkApplication
 * @accelerator: accelerator string
 * @action_name: the name of the action to activate
 * @parameter: (allow-none): parameter to pass when activating the action,
 *   or %NULL if the action does not accept an activation parameter
 *
 * Installs an accelerator that will cause the named action
 * to be activated when the key combination specificed by @accelerator
 * is pressed.
 *
 * @accelerator must be a string that can be parsed by
 * gtk_accelerator_parse(), e.g. "<Primary>q" or "<Control><Alt>p".
 *
 * @action_name must be the name of an action as it would be used
 * in the app menu, i.e. actions that have been added to the application
 * are referred to with an "app." prefix, and window-specific actions
 * with a "win." prefix.
 *
 * GtkApplication also extracts accelerators out of 'accel' attributes
 * in the #GMenuModels passed to g_application_set_app_menu() and
 * g_application_set_menubar(), which is usually more convenient
 * than calling this function for each accelerator.
 *
 * Since: 3.4
 */
void
gtk_application_add_accelerator (GtkApplication *application,
                                 const gchar    *accelerator,
                                 const gchar    *action_name,
                                 GVariant       *parameter)
{
  gchar *accel_path;
  guint accel_key;
  GdkModifierType accel_mods;

  g_return_if_fail (GTK_IS_APPLICATION (application));

  /* Call this here, since gtk_init() is only getting called in startup() */
  _gtk_accel_map_init ();

  gtk_accelerator_parse (accelerator, &accel_key, &accel_mods);

  if (accel_key == 0)
    {
      g_warning ("Failed to parse accelerator: '%s'\n", accelerator);
      return;
    }

  accel_path = _gtk_accel_path_for_action (action_name, parameter);

  if (gtk_accel_map_lookup_entry (accel_path, NULL))
    gtk_accel_map_change_entry (accel_path, accel_key, accel_mods, TRUE);
  else
    gtk_accel_map_add_entry (accel_path, accel_key, accel_mods);

  g_free (accel_path);
}

/**
 * gtk_application_remove_accelerator:
 * @application: a #GtkApplication
 * @action_name: the name of the action to activate
 * @parameter: (allow-none): parameter to pass when activating the action,
 *   or %NULL if the action does not accept an activation parameter
 *
 * Removes an accelerator that has been previously added
 * with gtk_application_add_accelerator().
 *
 * Since: 3.4
 */
void
gtk_application_remove_accelerator (GtkApplication *application,
                                    const gchar    *action_name,
                                    GVariant       *parameter)
{
  gchar *accel_path;

  g_return_if_fail (GTK_IS_APPLICATION (application));

  accel_path = _gtk_accel_path_for_action (action_name, parameter);

  if (!gtk_accel_map_lookup_entry (accel_path, NULL))
    {
      g_warning ("No accelerator found for '%s'\n", accel_path);
      g_free (accel_path);
      return;
    }

  gtk_accel_map_change_entry (accel_path, 0, 0, FALSE);
  g_free (accel_path);
}

/**
 * gtk_application_set_app_menu:
 * @application: a #GtkApplication
 * @model: (allow-none): a #GMenuModel, or %NULL
 *
 * Sets or unsets the application menu for @application.
 *
 * The application menu is a single menu containing items that typically
 * impact the application as a whole, rather than acting on a specific
 * window or document.  For example, you would expect to see
 * "Preferences" or "Quit" in an application menu, but not "Save" or
 * "Print".
 *
 * If supported, the application menu will be rendered by the desktop
 * environment.
 *
 * Since: 3.4
 */
void
gtk_application_set_app_menu (GtkApplication *application,
                              GMenuModel     *model)
{
  g_object_set (application, "app-menu", model, NULL);
}

/**
 * gtk_application_get_app_menu:
 * @application: a #GtkApplication
 *
 * Returns the menu model that has been set with
 * g_application_set_app_menu().
 *
 * Returns: (transfer none): the application menu of @application
 *
 * Since: 3.4
 */
GMenuModel *
gtk_application_get_app_menu (GtkApplication *application)
{
  GMenuModel *app_menu = NULL;

  g_object_get (application, "app-menu", &app_menu, NULL);

  if (app_menu)
    g_object_unref (app_menu);

  return app_menu;
}

/**
 * gtk_application_set_menubar:
 * @application: a #GtkApplication
 * @model: (allow-none): a #GMenuModel, or %NULL
 *
 * Sets or unsets the menubar for windows of @application.
 *
 * This is a menubar in the traditional sense.
 *
 * Depending on the desktop environment, this may appear at the top of
 * each window, or at the top of the screen.  In some environments, if
 * both the application menu and the menubar are set, the application
 * menu will be presented as if it were the first item of the menubar.
 * Other environments treat the two as completely separate -- for
 * example, the application menu may be rendered by the desktop shell
 * while the menubar (if set) remains in each individual window.
 *
 * Since: 3.4
 */
void
gtk_application_set_menubar (GtkApplication *application,
                             GMenuModel     *model)
{
  g_object_set (application, "menubar", model, NULL);
}

/**
 * gtk_application_get_menubar:
 * @application: a #GtkApplication
 *
 * Returns the menu model that has been set with
 * g_application_set_menubar().
 *
 * Returns: (transfer none): the menubar for windows of @application
 *
 * Since: 3.4
 */
GMenuModel *
gtk_application_get_menubar (GtkApplication *application)
{
  GMenuModel *menubar = NULL;

  g_object_get (application, "menubar", &menubar, NULL);

  if (menubar)
    g_object_unref (menubar);

  return menubar;
}
