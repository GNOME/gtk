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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "gtkapplication.h"
#include "gtkmarshalers.h"
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

G_DEFINE_TYPE (GtkApplication, gtk_application, G_TYPE_APPLICATION)

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

static void
gtk_application_startup (GApplication *application)
{
  gtk_init (0, 0);
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

  g_variant_iter_init (&iter, platform_data);
  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value))
    {
      if (strcmp (key, "desktop-startup-id") == 0)
        {
          GdkDisplay *display;
          const gchar *id;

          display = gdk_display_get_default ();
          id = g_variant_get_string (value, NULL);
          gdk_x11_display_set_startup_notification_id (display, id);
       }
    }
}

static void
gtk_application_after_emit (GApplication *application,
                            GVariant     *platform_data)
{
  gdk_notify_startup_complete ();
}

static void
gtk_application_init (GtkApplication *application)
{
}

static void
gtk_application_class_init (GtkApplicationClass *class)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  application_class->add_platform_data = gtk_application_add_platform_data;
  application_class->before_emit = gtk_application_before_emit;
  application_class->after_emit = gtk_application_after_emit;
  application_class->startup = gtk_application_startup;

  application_class->quit_mainloop = gtk_application_quit_mainloop;
  application_class->run_mainloop = gtk_application_run_mainloop;
}
