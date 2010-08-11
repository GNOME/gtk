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
#include "gtkwindow.h"
#include "gtkmain.h"

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
  PROP_NONE,
  PROP_ACTION_GROUP,
  PROP_DEFAULT_WINDOW
};

struct _GtkApplicationPrivate
{
  GtkActionGroup *action_group;
  GtkWindow *default_window;
};

G_DEFINE_TYPE (GtkApplication, gtk_application, G_TYPE_APPLICATION)

static void
gtk_application_set_property (GObject *object, guint prop_id,
                              const GValue *value, GParamSpec *pspec)
{
  GtkApplication *application = GTK_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_ACTION_GROUP:
      gtk_application_set_action_group (application,
                                        g_value_get_object (value));
      break;

    case PROP_DEFAULT_WINDOW:
      gtk_application_set_default_window (application,
                                          g_value_get_object (value));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_application_get_property (GObject *object, guint prop_id,
                              GValue *value, GParamSpec *pspec)
{
  GtkApplication *application = GTK_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_ACTION_GROUP:
      g_value_set_object (value,
                          gtk_application_get_action_group (application));
      break;

    case PROP_DEFAULT_WINDOW:
      g_value_set_object (value,
                          gtk_application_get_default_window (application));
      break;

    default:
      g_assert_not_reached ();
    }
}

static GtkWindow *
gtk_application_real_create_window (GtkApplication *application)
{
  return GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
}

static void
gtk_application_activate (GApplication *gapplication)
{
  GtkApplication *application = GTK_APPLICATION (gapplication);

  if (application->priv->default_window == NULL)
    {
      GtkWindow *window;

      window = GTK_APPLICATION_GET_CLASS (application)
        ->create_window (application);

      if (window != NULL)
        gtk_application_set_default_window (application, window);
    }

  if (application->priv->default_window != NULL)
    gtk_window_present (application->priv->default_window);
}

static void
gtk_application_action (GApplication *gapplication,
                        const gchar  *action_name,
                        GVariant     *parameters)
{
  GtkApplication *application = GTK_APPLICATION (gapplication);

  if (parameters == NULL && application->priv->action_group != NULL)
    {
      GtkAction *action;

      action = gtk_action_group_get_action (application->priv->action_group,
                                            action_name);

      if (action != NULL)
        {
          gtk_action_activate (action);
          return;
        }
    }
}

GtkApplication *
gtk_application_new (const gchar       *application_id,
                     GApplicationFlags  flags,
                     GtkActionGroup    *action_group)
{
  g_return_val_if_fail (g_application_id_is_valid (application_id), NULL);

  gtk_init (NULL, NULL);

  return g_object_new (GTK_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       "action-group", action_group,
                       NULL);
}

GtkActionGroup *
gtk_application_get_action_group (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return application->priv->action_group;
}

void
gtk_application_set_action_group (GtkApplication *application,
                                  GtkActionGroup *action_group)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (action_group == NULL || GTK_IS_ACTION_GROUP (action_group));

  if (application->priv->action_group != action_group)
    {
      if (application->priv->action_group)
        g_object_unref (application->priv->action_group);

      application->priv->action_group = action_group;

      if (application->priv->action_group)
        g_object_ref (application->priv->action_group);

      /* XXX synch the list... */

      g_object_notify (G_OBJECT (application), "action-group");
    }
}

GtkWindow *
gtk_application_get_default_window (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return application->priv->default_window;
}

void
gtk_application_set_default_window (GtkApplication *application,
                                    GtkWindow      *window)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (window == NULL || GTK_IS_WINDOW (window));

  if (application->priv->default_window != window)
    {
      if (window)
        gtk_window_set_application (window, application);

      application->priv->default_window = window;

      g_object_notify (G_OBJECT (application), "default-window");
    }
}

static void
gtk_application_quit_mainloop (GApplication *application)
{
  gtk_main_quit ();
}

static void
gtk_application_run_mainloop (GApplication *application)
{
  gtk_main ();
}

static void
gtk_application_finalize (GObject *object)
{
  GtkApplication *application = GTK_APPLICATION (object);

  if (application->priv->action_group)
    g_object_unref (application->priv->action_group);

  G_OBJECT_CLASS (gtk_application_parent_class)
    ->finalize (object);
}

static void
gtk_application_init (GtkApplication *application)
{
  application->priv = G_TYPE_INSTANCE_GET_PRIVATE (application,
                                                   GTK_TYPE_APPLICATION,
                                                   GtkApplicationPrivate);
}

static void
gtk_application_class_init (GtkApplicationClass *class)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->set_property = gtk_application_set_property;
  object_class->get_property = gtk_application_get_property;

  class->create_window = gtk_application_real_create_window;

  application_class->quit_mainloop = gtk_application_quit_mainloop;
  application_class->run_mainloop = gtk_application_run_mainloop;
  application_class->activate = gtk_application_activate;
  application_class->action = gtk_application_action;

  object_class->finalize = gtk_application_finalize;

  g_object_class_install_property (object_class, PROP_ACTION_GROUP,
    g_param_spec_object ("action-group", "action group",
                         "the GtkActionGroup for the application",
                         GTK_TYPE_ACTION_GROUP, G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_DEFAULT_WINDOW,
    g_param_spec_object ("default-window", "default window",
                         "the default window for the application",
                         GTK_TYPE_WINDOW,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (object_class, sizeof (GtkApplicationPrivate));
}
