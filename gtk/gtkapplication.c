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
#include "gtkintl.h"

#ifdef GDK_WINDOWING_QUARTZ
#include "gtkquartz-menu.h"
#import <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>
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
 * uniqueness, session management, provides some basic scriptability and
 * desktop shell integration by exporting actions and menus and manages a
 * list of toplevel windows whose life-cycle is automatically tied to the
 * life-cycle of your application.
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
 *
 * GtkApplication optionally registers with a session manager
 * of the users session (if you set the #GtkApplication::register-session
 * property) and offers various functionality related to the session
 * life-cycle.
 *
 * An application can be informed when the session is about to end
 * by connecting to the #GtkApplication::quit-requested signal.
 *
 * An application can request the session to be ended by calling
 * gtk_application_end_session().
 *
 * An application can block various ways to end the session with
 * the gtk_application_inhibit() function. Typical use cases for
 * this kind of inhibiting are long-running, uninterruptible operations,
 * such as burning a CD or performing a disk backup. The session
 * manager may not honor the inhibitor, but it can be expected to
 * inform the user about the negative consequences of ending the
 * session while inhibitors are present.
 */

enum {
  WINDOW_ADDED,
  WINDOW_REMOVED,
  QUIT_REQUESTED,
  QUIT_CANCELLED,
  QUIT,
  LAST_SIGNAL
};

static guint gtk_application_signals[LAST_SIGNAL];

enum {
  PROP_ZERO,
  PROP_REGISTER_SESSION
};

G_DEFINE_TYPE (GtkApplication, gtk_application, G_TYPE_APPLICATION)

struct _GtkApplicationPrivate
{
  GList *windows;

  gboolean register_session;
  gboolean quit_requested;

#ifdef GDK_WINDOWING_X11
  GDBusConnection *session_bus;
  gchar *window_prefix;
  guint next_id;

  GDBusProxy *sm_proxy;
  GDBusProxy *client_proxy;
  gchar *app_id;
  gchar *client_path;
#endif

#ifdef GDK_WINDOWING_QUARTZ
  GActionMuxer *muxer;
  GMenu *combined;

  AppleEvent quit_event, quit_reply;
  gboolean quitting;
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

static void gtk_application_startup_session_dbus (GtkApplication *app);

static void
gtk_application_startup_x11 (GtkApplication *application)
{
  const gchar *application_id;

  application_id = g_application_get_application_id (G_APPLICATION (application));
  application->priv->session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  application->priv->window_prefix = window_prefix_from_appid (application_id);

  gtk_application_startup_session_dbus (GTK_APPLICATION (application));
}

static void
gtk_application_shutdown_x11 (GtkApplication *application)
{
  g_free (application->priv->window_prefix);
  application->priv->window_prefix = NULL;
  g_clear_object (&application->priv->session_bus);

  g_clear_object (&application->priv->sm_proxy);
  g_clear_object (&application->priv->client_proxy);
  g_free (application->priv->app_id);
  g_free (application->priv->client_path);
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

static void gtk_application_startup_session_quartz (GtkApplication *app);

static void
gtk_application_startup_quartz (GtkApplication *application)
{
  [NSApp finishLaunching];

  application->priv->muxer = g_action_muxer_new ();
  g_action_muxer_insert (application->priv->muxer, "app", G_ACTION_GROUP (application));

  g_signal_connect (application, "notify::app-menu", G_CALLBACK (gtk_application_menu_changed_quartz), NULL);
  g_signal_connect (application, "notify::menubar", G_CALLBACK (gtk_application_menu_changed_quartz), NULL);
  gtk_application_menu_changed_quartz (G_OBJECT (application), NULL, NULL);

  gtk_application_startup_session_quartz (application);
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
gtk_application_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkApplication *application = GTK_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_REGISTER_SESSION:
      g_value_set_boolean (value, application->priv->register_session);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_application_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkApplication *application = GTK_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_REGISTER_SESSION:
      application->priv->register_session = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_application_class_init (GtkApplicationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  object_class->get_property = gtk_application_get_property;
  object_class->set_property = gtk_application_set_property;
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

  /**
   * GtkApplication::quit-requested:
   * @application: the #GtkApplication
   *
   * Emitted when the session manager requests that the application
   * exit (generally because the user is logging out). The application
   * should decide whether or not it is willing to quit and then call
   * g_application_quit_response(), passing %TRUE or %FALSE to give its
   * answer to the session manager. It does not need to give an answer
   * before returning from the signal handler; the answer can be given
   * later on, but <emphasis>the application must not attempt to perform
   * any actions or interact with the user</emphasis> in response to
   * this signal. Any actions required for a clean shutdown should take
   * place in response to the #GtkApplication::quit signal.
   *
   * The application should limit its operations until either the
   * #GApplication::quit or #GtkApplication::quit-cancelled signals is
   * emitted.
   *
   * To receive this signal, you need to set the
   * #GtkApplication::register-session property
   * when creating the application object.
   *
   * Since: 3.4
   */
  gtk_application_signals[QUIT_REQUESTED] =
    g_signal_new ("quit-requested", GTK_TYPE_APPLICATION, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkApplicationClass, quit_requested),
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GtkApplication::quit-cancelled:
   * @application: the #GtkApplication
   *
   * Emitted when the session manager decides to cancel a logout after
   * the application has already agreed to quit. After receiving this
   * signal, the application can go back to what it was doing before
   * receiving the #GtkApplication::quit-requested signal.
   *
   * To receive this signal, you need to set the
   * #GtkApplication::register-session property
   * when creating the application object.
   *
   * Since: 3.4
   */
  gtk_application_signals[QUIT_CANCELLED] =
    g_signal_new ("quit-cancelled", GTK_TYPE_APPLICATION, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkApplicationClass, quit_cancelled),
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GtkApplication::quit:
   * @application: the #GtkApplication
   *
   * Emitted when the session manager wants the application to quit
   * (generally because the user is logging out). The application
   * should exit as soon as possible after receiving this signal; if
   * it does not, the session manager may choose to forcibly kill it.
   *
   * Normally, an application would only be sent a ::quit if it
   * agreed to quit in response to a #GtkApplication::quit-requested
   * signal. However, this is not guaranteed; in some situations the
   * session manager may decide to end the session without giving
   * applications a chance to object.
   *
   * To receive this signal, you need to set the
   * #GtkApplication::register-session property
   * when creating the application object.
   *
   * Since: 3.4
   */
  gtk_application_signals[QUIT] =
    g_signal_new ("quit", GTK_TYPE_APPLICATION, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkApplicationClass, quit),
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  g_object_class_install_property (object_class, PROP_REGISTER_SESSION,
    g_param_spec_boolean ("register-session",
                          P_("Register session"),
                          P_("Register with the session manager"),
                          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
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

#if defined(GDK_WINDOWING_X11)

/* D-Bus Session Management
 *
 * The protocol and the D-Bus API are described here:
 * http://live.gnome.org/SessionManagement/GnomeSession
 * http://people.gnome.org/~mccann/gnome-session/docs/gnome-session.html
 */

static void
unregister_client (GtkApplication *app)
{
  GError *error = NULL;

  g_debug ("Unregistering client");

  g_dbus_proxy_call_sync (app->priv->sm_proxy,
                          "UnregisterClient",
                          g_variant_new ("(o)", app->priv->client_path),
                          G_DBUS_CALL_FLAGS_NONE,
                          G_MAXINT,
                          NULL,
                          &error);

  if (error)
    {
      g_warning ("Failed to unregister client: %s", error->message);
      g_error_free (error);
    }

  g_clear_object (&app->priv->client_proxy);

  g_free (app->priv->client_path);
  app->priv->client_path = NULL;
}

static void
client_proxy_signal (GDBusProxy     *proxy,
                     const gchar    *sender_name,
                     const gchar    *signal_name,
                     GVariant       *parameters,
                     GtkApplication *app)
{
  if (strcmp (signal_name, "QueryEndSession") == 0)
    {
      g_debug ("Received QueryEndSession");
      app->priv->quit_requested = TRUE;
      g_signal_emit (app, gtk_application_signals[QUIT_REQUESTED], 0);
    }
  else if (strcmp (signal_name, "EndSession") == 0)
    {
      g_debug ("Received EndSession");
      gtk_application_quit_response (app, TRUE, NULL);
      unregister_client (app);
      g_signal_emit (app, gtk_application_signals[QUIT], 0);
    }
  else if (strcmp (signal_name, "CancelEndSession") == 0)
    {
      g_debug ("Received CancelEndSession");
      g_signal_emit (app, gtk_application_signals[QUIT_CANCELLED], 0);
    }
  else if (strcmp (signal_name, "Stop") == 0)
    {
      g_debug ("Received Stop");
      unregister_client (app);
      g_signal_emit (app, gtk_application_signals[QUIT], 0);
    }
}

static void
gtk_application_startup_session_dbus (GtkApplication *app)
{
  static gchar *client_id;
  GError *error = NULL;
  GVariant *res;

  if (app->priv->session_bus == NULL)
    return;

  if (client_id == NULL)
    {
      const gchar *desktop_autostart_id;

      desktop_autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");
      /* Unset DESKTOP_AUTOSTART_ID in order to avoid child processes to
       * use the same client id.
       */
      g_unsetenv ("DESKTOP_AUTOSTART_ID");
      client_id = g_strdup (desktop_autostart_id ? desktop_autostart_id : "");
    }

  g_debug ("Connecting to session manager");

  app->priv->sm_proxy = g_dbus_proxy_new_sync (app->priv->session_bus,
                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                               G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                               NULL,
                                               "org.gnome.SessionManager",
                                               "/org/gnome/SessionManager",
                                               "org.gnome.SessionManager",
                                               NULL,
                                               &error);
  if (error)
    {
      g_warning ("Failed to get a session proxy: %s", error->message);
      g_error_free (error);
      return;
    }

  /* FIXME: should we reuse the D-Bus application id here ? */
  app->priv->app_id = g_strdup (g_get_prgname ());

  if (!app->priv->register_session)
    return;

  g_debug ("Registering client '%s' '%s'", app->priv->app_id, client_id);

  res = g_dbus_proxy_call_sync (app->priv->sm_proxy,
                                "RegisterClient",
                                g_variant_new ("(ss)", app->priv->app_id, client_id),
                                G_DBUS_CALL_FLAGS_NONE,
                                G_MAXINT,
                                NULL,
                                &error);

  if (error)
    {
      g_warning ("Failed to register client: %s", error->message);
      g_error_free (error);
      g_clear_object (&app->priv->sm_proxy);
      return;
    }

  g_variant_get (res, "(o)", &app->priv->client_path);
  g_variant_unref (res);

  g_debug ("Registered client at '%s'", app->priv->client_path);

  app->priv->client_proxy = g_dbus_proxy_new_sync (app->priv->session_bus, 0,
                                                   NULL,
                                                   "org.gnome.SessionManager",
                                                   app->priv->client_path,
                                                   "org.gnome.SessionManager.ClientPrivate",
                                                   NULL,
                                                   &error);
  if (error)
    {
      g_warning ("Failed to get client proxy: %s", error->message);
      g_error_free (error);
      g_clear_object (&app->priv->sm_proxy);
      g_free (app->priv->client_path);
      app->priv->client_path = NULL;
      return;
    }

  g_signal_connect (app->priv->client_proxy, "g-signal", G_CALLBACK (client_proxy_signal), app);
}

/**
 * gtk_application_quit_response:
 * @application: the #GtkApplication
 * @will_quit: whether the application agrees to quit
 * @reason: (allow-none): a short human-readable string that explains
 *     why quitting is not possible
 *
 * This function <emphasis>must</emphasis> be called in response to the
 * #GtkApplication::quit-requested signal, to indicate whether or
 * not the application is willing to quit. The application may call
 * it either directly from the signal handler, or at some later point.
 *
 * It should be stressed that <emphasis>applications should not assume
 * that they have the ability to block logout or shutdown</emphasis>,
 * even when %FALSE is passed for @will_quit.
 *
 * After calling this method, the application should wait to receive
 * either #GtkApplication::quit-cancelled or #GtkApplication::quit.
 *
 * If the application does not connect to #GtkApplication::quit-requested,
 * #GtkApplication will call this method on its behalf (passing %TRUE
 * for @will_quit).
 *
 * Since: 3.4
 */
void
gtk_application_quit_response (GtkApplication *application,
                               gboolean        will_quit,
                               const gchar    *reason)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (!g_application_get_is_remote (G_APPLICATION (application)));
  g_return_if_fail (application->priv->client_proxy != NULL);
  g_return_if_fail (application->priv->quit_requested);

  application->priv->quit_requested = FALSE;

  g_debug ("Calling EndSessionResponse %d '%s'", will_quit, reason);

  g_dbus_proxy_call (application->priv->client_proxy,
                     "EndSessionResponse",
                     g_variant_new ("(bs)", will_quit, reason ? reason : ""),
                     G_DBUS_CALL_FLAGS_NONE,
                     G_MAXINT,
                     NULL, NULL, NULL);
}

/**
 * GtkApplicationInhibitFlags:
 * @GTK_APPLICATION_INHIBIT_LOGOUT: Inhibit logging out (including shutdown
 *     of the computer)
 * @GTK_APPLICATION_INHIBIT_SWITCH: Inhibit user switching
 * @GTK_APPLICATION_INHIBIT_SUSPEND: Inhibit suspending the
 *     session or computer
 * @GTK_APPLICATION_INHIBIT_IDLE: Inhibit the session being
 *     marked as idle (and possibly locked)
 *
 * Types of user actions that may be blocked by gtk_application_inhibit().
 *
 * Since: 3.4
 */

/**
 * gtk_application_inhibit:
 * @application: the #GApplication
 * @window: (allow-none): a #GtkWindow, or %NULL
 * @flags: what types of actions should be inhibited
 * @reason: (allow-none): a short, human-readable string that explains
 *     why these operations are inhibited
 *
 * Inform the session manager that certain types of actions should be
 * inhibited. This is not guaranteed to work on all platforms and for
 * all types of actions.
 *
 * Applications should invoke this method when they begin an operation
 * that should not be interrupted, such as creating a CD or DVD. The
 * types of actions that may be blocked are specified by the @flags
 * parameter. When the application completes the operation it should
 * call g_application_uninhibit() to remove the inhibitor.
 * Inhibitors are also cleared when the application exits.
 *
 * Applications should not expect that they will always be able to block
 * the action. In most cases, users will be given the option to force
 * the action to take place.
 *
 * Reasons should be short and to the point.
 *
 * If a window is passed, the session manager may point the user to
 * this window to find out more about why the action is inhibited.
 *
 * Returns: A non-zero cookie that is used to uniquely identify this
 *     request. It should be used as an argument to g_application_uninhibit()
 *     in order to remove the request. If the platform does not support
 *     inhibiting or the request failed for some reason, 0 is returned.
 *
 * Since: 3.4
 */
guint
gtk_application_inhibit (GtkApplication             *application,
                         GtkWindow                  *window,
                         GtkApplicationInhibitFlags  flags,
                         const gchar                *reason)
{
  GVariant *res;
  GError *error = NULL;
  guint cookie;
  guint xid;

  g_return_val_if_fail (GTK_IS_APPLICATION (application), 0);
  g_return_val_if_fail (!g_application_get_is_remote (G_APPLICATION (application)), 0);
  g_return_val_if_fail (application->priv->sm_proxy != NULL, 0);

  if (window != NULL)
    xid = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (window)));
  else
    xid = 0;

  res = g_dbus_proxy_call_sync (application->priv->sm_proxy,
                                "Inhibit",
                                g_variant_new ("(susu)",
                                               application->priv->app_id,
                                               xid,
                                               reason,
                                               flags),
                                G_DBUS_CALL_FLAGS_NONE,
                                G_MAXINT,
                                NULL,
                                &error);
 if (error)
    {
      g_warning ("Calling Inhibit failed: %s\n", error->message);
      g_error_free (error);
      return 0;
    }

  g_variant_get (res, "(u)", &cookie);
  g_variant_unref (res);

  return cookie;
}

/**
 * gtk_application_uninhibit:
 * @application: the #GApplication
 * @cookie: a cookie that was returned by g_application_inhibit()
 *
 * Removes an inhibitor that has been established with g_application_inhibit().
 * Inhibitors are also cleared when the application exits.
 *
 * Since: 3.4
 */
void
gtk_application_uninhibit (GtkApplication *application,
                           guint           cookie)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (!g_application_get_is_remote (G_APPLICATION (application)));
  g_return_if_fail (application->priv->sm_proxy != NULL);

  g_dbus_proxy_call (application->priv->sm_proxy,
                     "Uninhibit",
                     g_variant_new ("(u)", cookie),
                     G_DBUS_CALL_FLAGS_NONE,
                     G_MAXINT,
                     NULL, NULL, NULL);
}

/**
 * gtk_application_is_inhibited:
 * @application: the #GApplication
 * @flags: what types of actions should be queried
 *
 * Determines if any of the actions specified in @flags are
 * currently inhibited (possibly by another application).
 *
 * Returns: %TRUE if any of the actions specified in @flags are inhibited
 *
 * Since: 3.4
 */
gboolean
gtk_application_is_inhibited (GtkApplication             *application,
                              GtkApplicationInhibitFlags  flags)
{
  GVariant *res;
  GError *error = NULL;
  gboolean inhibited;

  g_return_val_if_fail (GTK_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (!g_application_get_is_remote (G_APPLICATION (application)), FALSE);
  g_return_val_if_fail (application->priv->sm_proxy != NULL, FALSE);

  res = g_dbus_proxy_call_sync (application->priv->sm_proxy,
                                "IsInhibited",
                                g_variant_new ("(u)", flags),
                                G_DBUS_CALL_FLAGS_NONE,
                                G_MAXINT,
                                NULL,
                                &error);
  if (error)
    {
      g_warning ("Calling IsInhibited failed: %s\n", error->message);
      g_error_free (error);
      return FALSE;
    }

  g_variant_get (res, "(b)", &inhibited);
  g_variant_unref (res);

  return inhibited;
}

/**
 * GtkApplicationEndStyle:
 * @GTK_APPLICATION_LOGOUT: End the session by logging out.
 * @GTK_APPLICATION_REBOOT: Restart the computer.
 * @GTK_APPLICATION_SHUTDOWN: Shut the computer down.
 *
 * Different ways to end a user session, for use with
 * gtk_application_end_session().
 */

/**
 * gtk_application_end_session:
 * @application: the #GtkApplication
 * @style: the desired kind of session end
 * @request_confirmation: whether or not the user should get a chance
 *     to confirm the action
 *
 * Requests that the session manager end the current session.
 * @style indicates how the session should be ended, and
 * @request_confirmation indicates whether or not the user should be
 * given a chance to confirm the action. Both of these flags are merely
 * hints though; the session manager may choose to ignore them.
 *
 * Return value: %TRUE if the request was sent; %FALSE if it could not
 *     be sent (eg, because it could not connect to the session manager)
 *
 * Since: 3.4
 */
gboolean
gtk_application_end_session (GtkApplication         *application,
                             GtkApplicationEndStyle  style,
                             gboolean                request_confirmation)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (!g_application_get_is_remote (G_APPLICATION (application)), FALSE);
  g_return_val_if_fail (application->priv->sm_proxy != NULL, FALSE);

  switch (style)
    {
    case GTK_APPLICATION_LOGOUT:
      g_dbus_proxy_call (application->priv->sm_proxy,
                         "Logout",
                         g_variant_new ("(u)", request_confirmation),
                         G_DBUS_CALL_FLAGS_NONE,
                         G_MAXINT,
                         NULL, NULL, NULL);
      break;
    case GTK_APPLICATION_REBOOT:
    case GTK_APPLICATION_SHUTDOWN:
      g_dbus_proxy_call (application->priv->sm_proxy,
                         "Shutdown",
                         NULL,
                         G_DBUS_CALL_FLAGS_NONE,
                         G_MAXINT,
                         NULL, NULL, NULL);
      break;
    }

  return TRUE;
}

#elif defined(GDK_WINDOWING_QUARTZ)

/* OS X implementation copied from EggSMClient */

static gboolean
idle_quit_requested (gpointer client)
{
  g_signal_emit (client, gtk_application_signals[QUIT_REQUESTED], 0);

  return FALSE;
}

static pascal OSErr
quit_requested (const AppleEvent *aevt,
                AppleEvent       *reply,
                long              refcon)
{
  GtkApplication *app = GSIZE_TO_POINTER ((gsize)refcon);

  g_return_val_if_fail (!app->priv->quit_requested, userCanceledErr);

  /* FIXME AEInteractWithUser? */
  osx->quit_requested = TRUE;
  AEDuplicateDesc (aevt, &app->priv->quit_event);
  AEDuplicateDesc (reply, &app->priv->quit_reply);
  AESuspendTheCurrentEvent (aevt);

  /* Don't emit the "quit_requested" signal immediately, since we're
   * called from a weird point in the guts of gdkeventloop-quartz.c
   */
  g_idle_add (idle_quit_requested, app);

  return noErr;
}

static void
gtk_application_startup_session_quartz (GtkApplication *app)
{
  if (app->priv->register_session)
    AEInstallEventHandler (kCoreEventClass, kAEQuitApplication,
                           NewAEEventHandlerUPP (quit_requested),
                           (long)GPOINTER_TO_SIZE (app), false);
}

static pascal OSErr
quit_requested_resumed (const AppleEvent *aevt,
                        AppleEvent       *reply,
                        long              refcon)
{
  GtkApplication *app = GSIZE_TO_POINTER ((gsize)refcon);

  app->priv->quit_requested = FALSE;

  return app->priv->quitting ? noErr : userCanceledErr;
}

static gboolean
idle_will_quit (gpointer data)
{
  GtkApplication *app = data;

  /* Resume the event with a new handler that will return
   * a value to the system
   */
  AEResumeTheCurrentEvent (&app->priv->quit_event, &app->priv->quit_reply,
                           NewAEEventHandlerUPP (quit_requested_resumed),
                           (long)GPOINTER_TO_SIZE (app));

  AEDisposeDesc (&app->quit->quit_event);
  AEDisposeDesc (&app->quit->quit_reply);

  if (app->priv->quitting)
    g_signal_emit (app, gtk_application_signals[QUIT], 0);

  return FALSE;
}

void
gtk_application_quit_response (GtkApplication *application,
                               gboolean        will_quit,
                               const gchar    *reason)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (!g_application_get_is_remote (G_APPLICATION (application)));
  g_return_if_fail (application->priv->quit_requested);

  application->priv->quitting = will_quit;

  /* Finish in an idle handler since the caller might have called
   * gtk_application_quit_response() from inside the ::quit-requested
   * signal handler, but may not expect the ::quit signal to arrive
   * during the gtk_application_quit_response() call.
   */
  g_idle_add (idle_will_quit, application);
}

guint
gtk_application_inhibit (GtkApplication             *application,
                         GtkWindow                  *window,
                         GtkApplicationInhibitFlags  flags,
                         const gchar                *reason)
{
  return 0;
}

void
gtk_application_uninhibit (GtkApplication *application,
                           guint           cookie)
{
}

gboolean
gtk_application_is_inhibited (GtkApplication             *application,
                              GtkApplicationInhibitFlags  flags)
{
  return FALSE;
}

gboolean
gtk_application_end_session (GtkApplication         *application,
                             GtkApplicationEndStyle *style,
                             gboolean                request_confirmation)
{
  static const ProcessSerialNumber loginwindow_psn = { 0, kSystemProcess };
  AppleEvent event = { typeNull, NULL };
  AppleEvent reply = { typeNull, NULL };
  AEAddressDesc target;
  AEEventID id;
  OSErr err;

  switch (style)
    {
    case GTK_APPLICATION_LOGOUT:
      id = request_confirmation ? kAELogOut : kAEReallyLogOut;
      break;
    case GTK_APPLICATION_REBOOT:
      id = request_confirmation ? kAEShowRestartDialog : kAERestart;
      break;
    case GTK_APPLICATION_SHUTDOWN:
      id = request_confirmation ? kAEShowShutdownDialog : kAEShutDown;
      break;
    }

  err = AECreateDesc (typeProcessSerialNumber, &loginwindow_psn,
                      sizeof (loginwindow_psn), &target);
  if (err != noErr)
    {
      g_warning ("Could not create descriptor for loginwindow: %d", err);
      return FALSE;
    }

  err = AECreateAppleEvent (kCoreEventClass, id, &target,
                            kAutoGenerateReturnID, kAnyTransactionID,
                            &event);
  AEDisposeDesc (&target);
  if (err != noErr)
    {
      g_warning ("Could not create logout AppleEvent: %d", err);
      return FALSE;
    }

  err = AESend (&event, &reply, kAENoReply, kAENormalPriority,
                kAEDefaultTimeout, NULL, NULL);
  AEDisposeDesc (&event);
 if (err == noErr)
    AEDisposeDesc (&reply);

  return err == noErr;
}

#else

/* Trivial implementation.
 *
 * For the inhibit API on Windows, see
 * http://msdn.microsoft.com/en-us/library/ms700677%28VS.85%29.aspx
 */

void
gtk_application_quit_response (GtkApplication *application,
                               gboolean        will_quit,
                               const gchar    *reason)
{
}

guint
gtk_application_inhibit (GtkApplication             *application,
                         GtkWindow                  *window,
                         GtkApplicationInhibitFlags  flags,
                         const gchar                *reason)
{
  return 0;
}

void
gtk_application_uninhibit (GtkApplication *application,
                           guint           cookie)
{
}

gboolean
gtk_application_is_inhibited (GtkApplication             *application,
                              GtkApplicationInhibitFlags  flags)
{
  return FALSE;
}

gboolean
gtk_application_end_session (GtkApplication         *application,
                             GtkApplicationEndStyle *style,
                             gboolean                request_confirmation)
{
  return FALSE;
}

#endif
