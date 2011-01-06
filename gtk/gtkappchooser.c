/*
 * gtkappchooser.c: app-chooser interface
 *
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Cosimo Cecchi <ccecchi@redhat.com>
 */

/**
 * SECTION:gtkappchooser
 * @Title: GtkAppChooser
 * @Short_description: Interface implemented by widgets allowing to chooser applications
 *
 * #GtkAppChooser is an interface that can be implemented by widgets which
 * allow the user to choose an application (typically for the purpose of
 * opening a file). The main objects that implement this interface are
 * #GtkAppChooserWidget, #GtkAppChooserDialog and #GtkAppChooserButton.
 */

#include "config.h"

#include "gtkappchooser.h"

#include "gtkintl.h"
#include "gtkappchooserprivate.h"
#include "gtkwidget.h"

#include <glib.h>

G_DEFINE_INTERFACE (GtkAppChooser, gtk_app_chooser, GTK_TYPE_WIDGET);

static void
gtk_app_chooser_default_init (GtkAppChooserIface *iface)
{
  GParamSpec *pspec;

  /**
   * GtkAppChooser:content-type:
   *
   * The content type of the #GtkAppChooser object.
   */
  pspec = g_param_spec_string ("content-type",
                               P_("Content type"),
                               P_("The content type used by the open with object"),
                               NULL,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                               G_PARAM_STATIC_STRINGS);
  g_object_interface_install_property (iface, pspec);
}


/**
 * gtk_app_chooser_get_content_type:
 * @self: a #GtkAppChooser
 *
 * Returns the current value of the #GtkAppChooser:content-type property.
 *
 * Returns: the content type of @self. Free with g_free()
 *
 * Since: 3.0
 */
gchar *
gtk_app_chooser_get_content_type (GtkAppChooser *self)
{
  gchar *retval = NULL;

  g_return_val_if_fail (GTK_IS_APP_CHOOSER (self), NULL);

  g_object_get (self,
                "content-type", &retval,
                NULL);

  return retval;
}

/**
 * gtk_app_chooser_get_app_info:
 * @self: a #GtkAppChooser
 *
 * Returns the currently selected application.
 *
 * Returns: (transfer full): a #GAppInfo for the currently selected
 *     application, or %NULL if none is selected. Free with g_object_unref()
 *
 * Since: 3.0
 */
GAppInfo *
gtk_app_chooser_get_app_info (GtkAppChooser *self)
{
  return GTK_APP_CHOOSER_GET_IFACE (self)->get_app_info (self);
}

/**
 * gtk_app_chooser_refresh:
 * @self: a #GtkAppChooser
 *
 * Reloads the list of applications.
 *
 * Since: 3.0
 */
void
gtk_app_chooser_refresh (GtkAppChooser *self)
{
  GTK_APP_CHOOSER_GET_IFACE (self)->refresh (self);
}
