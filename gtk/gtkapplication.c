/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 *
 * Author: Colin Walters <walters@verbum.org>
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <stdlib.h>
#include <unistd.h>

#include "gtkapplication.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"
#include "gtkprivate.h"

#include "gtkalias.h"

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
 * Currently, GtkApplication handles application uniqueness, provides
 * some basic scriptability by exporting 'actions', implements some
 * standard actions itself (such as 'Quit') and provides a main window
 * whose life-cycle is automatically tied to the life-cycle of your
 * application.
 *
 * <example id="gtkapplication"><title>A simple application</title>
 * <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../gtk/tests/gtk-example-application.c">
 *  <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 * </programlisting>
 * </example>
 */
enum
{
  PROP_0,
  PROP_WINDOW
};

enum
{
  ACTIVATED,
  QUIT,
  ACTION,

  LAST_SIGNAL
};

static guint gtk_application_signals[LAST_SIGNAL] = { 0 };

struct _GtkApplicationPrivate
{
  GtkActionGroup *main_actions;

  GtkWindow *default_window;
  GSList *windows;
};

G_DEFINE_TYPE (GtkApplication, gtk_application, G_TYPE_APPLICATION)

static void
process_timestamp_from_platform_data (GVariant *platform_data)
{
  /* TODO - extract timestamp from here, update GDK time */
}

static gboolean
gtk_application_default_quit (GtkApplication *application)
{
  gtk_main_quit ();
  return TRUE;
}

static gboolean
gtk_application_default_quit_with_data (GApplication *application,
					GVariant     *platform_data)
{
  gboolean result;

  process_timestamp_from_platform_data (platform_data);
  
  g_signal_emit (application, gtk_application_signals[QUIT], 0, &result);

  return result;
}

static void
gtk_application_default_run (GApplication *application)
{
  gtk_main ();
}

static void
gtk_application_default_prepare_activation (GApplication *application,
					    GVariant     *arguments,
					    GVariant     *platform_data)
{
  GVariantIter iter;
  gchar *key;
  GVariant *value;

  g_variant_iter_init (&iter, platform_data);
  while (g_variant_iter_next (&iter, "{&sv}", &key, &value))
    {
      if (strcmp (key, "startup-notification-id") == 0 &&
          g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        gdk_notify_startup_complete_with_id (g_variant_get_string (value, NULL));
      g_variant_unref (value);
    }
  
  g_signal_emit (G_OBJECT (application), gtk_application_signals[ACTIVATED], 0, arguments);
}

static void
gtk_application_default_activated (GtkApplication *application,
                                   GVariant     *arguments)
{
  GtkApplication *app = GTK_APPLICATION (application);

  /* TODO: should we raise the last focused window instead ? */
  if (app->priv->default_window != NULL)
    gtk_window_present (app->priv->default_window);
}

static void
gtk_application_default_action (GtkApplication *application,
				const gchar    *action_name)
{
  GtkAction *action;

  action = gtk_action_group_get_action (application->priv->main_actions, action_name);
  if (action)
    {
      gtk_action_activate (action);
    }
}

static GtkWindow *
gtk_application_default_create_window (GtkApplication *application)
{
  return GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
}

static void
gtk_application_default_action_with_data (GApplication *application,
					  const gchar  *action_name,
					  GVariant     *platform_data)
{
  process_timestamp_from_platform_data (platform_data);

  g_signal_emit (application, gtk_application_signals[ACTION], g_quark_from_string (action_name));
}

static GVariant *
gtk_application_format_activation_data (void)
{
  const gchar *startup_id = NULL;
  GdkDisplay *display = gdk_display_get_default ();
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  /* try and get the startup notification id from GDK, the environment
   * or, if everything else failed, fake one.
   */
#ifdef GDK_WINDOWING_X11
  startup_id = gdk_x11_display_get_startup_notification_id (display);
#endif /* GDK_WINDOWING_X11 */

  if (startup_id)
    g_variant_builder_add (&builder, "{sv}", "startup-notification-id",
                           g_variant_new ("s", startup_id));
  return g_variant_builder_end (&builder);
}

static GVariant *
variant_from_argv (int    argc,
		   char **argv)
{
  GVariantBuilder builder;
  int i;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aay"));

  for (i = 1; i < argc; i++)
    {
      guint8 *argv_bytes;

      argv_bytes = (guint8*) argv[i];
      g_variant_builder_add_value (&builder,
				   g_variant_new_byte_array (argv_bytes, -1));
    }
  
  return g_variant_builder_end (&builder);
}

/**
 * gtk_application_new:
 * @appid: System-dependent application identifier
 * @argc: (allow-none) (inout): System argument count
 * @argv: (allow-none) (inout): System argument vector
 *
 * Create a new #GtkApplication, or if one has already been initialized
 * in this process, return the existing instance. This function will as
 * a side effect initialize the display system; see gtk_init().
 *
 * For the behavior if this application is running in another process,
 * see g_application_new().
 *
 * Returns: (transfer full): A newly-referenced #GtkApplication
 *
 * Since: 3.0
 */
GtkApplication*
gtk_application_new (const gchar   *appid,
		     gint          *argc,
                     gchar       ***argv)
{
  GtkApplication *app;
  gint argc_for_app;
  gchar **argv_for_app;
  GVariant *argv_variant;
  GError *error = NULL;

  gtk_init (argc, argv);

  if (argc)
    argc_for_app = *argc;
  else
    argc_for_app = 0;
  if (argv)
    argv_for_app = *argv;
  else
    argv_for_app = NULL;

  argv_variant = variant_from_argv (argc_for_app, argv_for_app);

  app = g_initable_new (GTK_TYPE_APPLICATION, 
			NULL,
			&error,
			"application-id", appid, 
			"argv", argv_variant, 
			NULL);
  if (!app)
    {
      g_error ("%s", error->message);
      g_clear_error (&error);
      return NULL;
    }

  return app;
}

static void
on_action_sensitive (GtkAction      *action,
                     GParamSpec     *pspec,
                     GtkApplication *app)
{
  g_application_set_action_enabled (G_APPLICATION (app),
                                    gtk_action_get_name (action),
                                    gtk_action_get_sensitive (action));
}

/**
 * gtk_application_set_action_group:
 * @app: A #GtkApplication
 * @group: A #GtkActionGroup
 *
 * Set @group as this application's global action group.
 * This will ensure the operating system interface uses
 * these actions as follows:
 *
 * <itemizedlist>
 *   <listitem>In GNOME 2 this exposes the actions for scripting.</listitem>
 *   <listitem>In GNOME 3, this function populates the application menu.</listitem>
 *   <listitem>In Windows prior to version 7, this function does nothing.</listitem>
 *   <listitem>In Windows 7, this function adds "Tasks" to the Jump List.</listitem>
 *   <listitem>In Mac OS X, this function extends the Dock menu.</listitem>
 * </itemizedlist>
 *
 * It is an error to call this function more than once.
 *
 * Since: 3.0
 */
void
gtk_application_set_action_group (GtkApplication *app,
                                  GtkActionGroup *group)
{
  GList *actions, *iter;

  g_return_if_fail (GTK_IS_APPLICATION (app));
  g_return_if_fail (app->priv->main_actions == NULL);

  app->priv->main_actions = g_object_ref (group);
  actions = gtk_action_group_list_actions (group);
  for (iter = actions; iter; iter = iter->next)
    {
      GtkAction *action = iter->data;
      g_application_add_action (G_APPLICATION (app),
                                gtk_action_get_name (action),
                                gtk_action_get_tooltip (action));
      g_signal_connect (action, "notify::sensitive",
                        G_CALLBACK (on_action_sensitive), app);
    }
  g_list_free (actions);
}

static gboolean
gtk_application_on_window_destroy (GtkWidget *window,
                                   gpointer   user_data)
{
  GtkApplication *app = GTK_APPLICATION (user_data);

  app->priv->windows = g_slist_remove (app->priv->windows, window);

  if (app->priv->windows == NULL)
    gtk_application_quit (app);

  return FALSE;
}

static gchar *default_title;

/**
 * gtk_application_add_window:
 * @app: a #GtkApplication
 * @window: a toplevel window to add to @app
 *
 * Adds a window to the #GtkApplication.
 *
 * If the user closes all of the windows added to @app, the default
 * behaviour is to call gtk_application_quit().
 *
 * If your application uses only a single toplevel window, you can
 * use gtk_application_get_window(). If you are using a sub-class
 * of #GtkApplication you should call gtk_application_create_window()
 * to let the #GtkApplication instance create a #GtkWindow and add
 * it to the list of toplevels of the application. You should call
 * this function only to add #GtkWindow<!-- -->s that you created
 * directly using gtk_window_new().
 *
 * Since: 3.0
 */
void
gtk_application_add_window (GtkApplication *app,
                            GtkWindow      *window)
{
  GtkApplicationPrivate *priv;

  g_return_if_fail (GTK_IS_APPLICATION (app));
  g_return_if_fail (GTK_IS_WINDOW (window));

  priv = app->priv;

  if (g_slist_find (priv->windows, window) != NULL)
    return;

  priv->windows = g_slist_prepend (priv->windows, window);

  if (priv->default_window == NULL)
    priv->default_window = window;

  if (gtk_window_get_title (window) == NULL && default_title != NULL)
    gtk_window_set_title (window, default_title);

  g_signal_connect (window, "destroy",
                    G_CALLBACK (gtk_application_on_window_destroy),
                    app);
}

/**
 * gtk_application_get_window:
 * @app: a #GtkApplication
 *
 * A simple #GtkApplication has a "default window". This window should
 * act as the primary user interaction point with your application.
 * The window returned by this function is of type #GTK_WINDOW_TYPE_TOPLEVEL
 * and its properties such as "title" and "icon-name" will be initialized
 * as appropriate for the platform.
 *
 * If the user closes this window, and your application hasn't created
 * any other windows, the default action will be to call gtk_application_quit().
 *
 * If your application has more than one toplevel window (e.g. an
 * single-document-interface application with multiple open documents),
 * or if you are constructing your toplevel windows yourself (e.g. using
 * #GtkBuilder), use gtk_application_create_window() or
 * gtk_application_add_window() instead.
 *
 * Returns: (transfer none): The default #GtkWindow for this application
 *
 * Since: 3.0
 */
GtkWindow *
gtk_application_get_window (GtkApplication *app)
{
  GtkApplicationPrivate *priv;
  GtkWindow *window;

  g_return_val_if_fail (GTK_IS_APPLICATION (app), NULL);

  priv = app->priv;

  if (priv->default_window != NULL)
    return priv->default_window;

  return gtk_application_create_window (app);
}

/**
 * gtk_application_create_window:
 * @app: a #GtkApplication
 *
 * Creates a new #GtkWindow for the application.
 *
 * This function calls the #GtkApplication::create_window() virtual function,
 * which can be overridden by sub-classes, for instance to use #GtkBuilder to
 * create the user interface. After creating a new #GtkWindow instance, it will
 * be added to the list of toplevels associated to the application.
 *
 * Return value: (transfer none): the newly created application #GtkWindow
 *
 * Since: 3.0
 */
GtkWindow *
gtk_application_create_window (GtkApplication *app)
{
  GtkWindow *window;

  g_return_val_if_fail (GTK_IS_APPLICATION (app), NULL);

  window = GTK_APPLICATION_GET_CLASS (app)->create_window (app);
  gtk_application_add_window (app, window);

  return window;
}

/**
 * gtk_application_run:
 * @app: a #GtkApplication
 *
 * Runs the main loop; see g_application_run().
 * The default implementation for #GtkApplication uses gtk_main().
 *
 * Since: 3.0
 */
void
gtk_application_run (GtkApplication *app)
{
  g_application_run (G_APPLICATION (app));
}

/**
 * gtk_application_quit:
 * @app: a #GtkApplication
 *
 * Request the application exit.  This function invokes
 * g_application_quit_with_data(), which normally will
 * in turn cause @app to emit #GtkApplication::quit.
 *
 * To control an application's quit behavior (for example, to ask for
 * files to be saved), connect to the #GtkApplication::quit signal
 * handler.
 *
 * Since: 3.0
 */
void
gtk_application_quit (GtkApplication *app)
{
  GVariantBuilder builder;
  GVariant *platform_data;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&builder, "{sv}",
			 "timestamp",
			 g_variant_new ("u",
					gtk_get_current_event_time ()));
  platform_data = g_variant_builder_end (&builder);
    
  g_application_quit_with_data (G_APPLICATION (app), platform_data);

  g_variant_unref (platform_data);
}

static void
gtk_application_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkApplication *app = GTK_APPLICATION (object);

  switch (prop_id)
    {
      case PROP_WINDOW:
        g_value_set_object (value, gtk_application_get_window (app));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_application_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkApplication *app = GTK_APPLICATION (object);

  g_assert (app != NULL);

  switch (prop_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
setup_default_window_decorations (void)
{
  const gchar *pid;
  const gchar *filename;
  GKeyFile *keyfile;
  gchar *title;
  gchar *icon_name;

  pid = g_getenv ("GIO_LAUNCHED_DESKTOP_FILE_PID");
  filename = g_getenv ("GIO_LAUNCHED_DESKTOP_FILE");

  keyfile = g_key_file_new ();

  if (pid != NULL && filename != NULL && atoi (pid) == getpid () &&
      g_key_file_load_from_file (keyfile, filename, 0, NULL))
    {
      title = g_key_file_get_locale_string (keyfile, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME, NULL, NULL);
      icon_name = g_key_file_get_string (keyfile, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, NULL);

      if (default_title == NULL)
        default_title = title;

      if (gtk_window_get_default_icon_name () == NULL)
        gtk_window_set_default_icon_name (icon_name);

      g_free (icon_name);
    }

  g_key_file_free (keyfile);
}

static void
gtk_application_init (GtkApplication *application)
{
  application->priv = G_TYPE_INSTANCE_GET_PRIVATE (application, GTK_TYPE_APPLICATION, GtkApplicationPrivate);

  g_object_set (application, "platform-data", gtk_application_format_activation_data (), NULL);

  setup_default_window_decorations ();
}


static GObject*
gtk_application_constructor (GType                  type,
                             guint                  n_construct_properties,
                             GObjectConstructParam *construct_params)
{
  GObject *object;

  /* Last ditch effort here */
  gtk_init (0, NULL);

  object = (* G_OBJECT_CLASS (gtk_application_parent_class)->constructor) (type,
                                                                           n_construct_properties,
                                                                           construct_params);

  return object;
}

static void
gtk_application_class_init (GtkApplicationClass *klass)
{
  GObjectClass *gobject_class;
  GApplicationClass *application_class;

  gobject_class = G_OBJECT_CLASS (klass);
  application_class = G_APPLICATION_CLASS (klass);

  gobject_class->constructor = gtk_application_constructor;
  gobject_class->get_property = gtk_application_get_property;
  gobject_class->set_property = gtk_application_set_property;

  application_class->run = gtk_application_default_run;
  application_class->quit_with_data = gtk_application_default_quit_with_data;
  application_class->action_with_data = gtk_application_default_action_with_data;
  application_class->prepare_activation = gtk_application_default_prepare_activation;

  klass->quit = gtk_application_default_quit;
  klass->action = gtk_application_default_action;
  klass->create_window = gtk_application_default_create_window;

  klass->activated = gtk_application_default_activated;

  /**
   * GtkApplication::activated:
   * @arguments: A #GVariant with the signature "aay"
   *
   * This signal is emitted when a non-primary process for a given
   * application is invoked while your application is running; for
   * example, when a file browser launches your program to open a
   * file.  The raw operating system arguments are passed in the
   * variant @arguments.
   */

  gtk_application_signals[ACTIVATED] =
    g_signal_new (g_intern_static_string ("activated"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkApplicationClass, activated),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VARIANT,
                  G_TYPE_NONE, 1,
                  G_TYPE_VARIANT);

  /**
   * GtkApplication::quit:
   * @application: the object on which the signal is emitted
   *
   * This signal is emitted when a quit is initiated.  See also
   * the #GApplication::quit-with-data signal which may in
   * turn trigger this signal.
   *
   * The default handler for this signal exits the mainloop of the
   * application.
   *
   * Returns: %TRUE if the signal has been handled, %FALSE to continue
   *   signal emission
   */
  gtk_application_signals[QUIT] =
    g_signal_new (g_intern_static_string ("quit"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkApplicationClass, quit),
                  g_signal_accumulator_true_handled, NULL,
                  _gtk_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /**
   * GtkApplication::action:
   * @application: the object on which the signal is emitted
   * @name: The name of the activated action
   *
   * This signal is emitted when an action is activated. The action name
   * is passed as the first argument, but also as signal detail, so it
   * is possible to connect to this signal for individual actions.
   *
   * See also the #GApplication::action-with-data signal which may in
   * turn trigger this signal.
   *
   * The signal is never emitted for disabled actions.
   */
  gtk_application_signals[ACTION] =
    g_signal_new (g_intern_static_string ("action"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (GtkApplicationClass, action),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);
                 
  g_type_class_add_private (gobject_class, sizeof (GtkApplicationPrivate));
}

#define __GTK_APPLICATION_C__
#include "gtkaliasdef.c"
