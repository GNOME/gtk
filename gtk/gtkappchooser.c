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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Cosimo Cecchi <ccecchi@redhat.com>
 */

/**
 * SECTION:gtkappchooser
 * @Title: GtkAppChooser
 * @Short_description: Interface implemented by widgets for choosing an application
 * @See_also: #GAppInfo
 *
 * #GtkAppChooser is an interface that can be implemented by widgets which
 * allow the user to choose an application (typically for the purpose of
 * opening a file). The main objects that implement this interface are
 * #GtkAppChooserWidget, #GtkAppChooserDialog and #GtkAppChooserButton.
 *
 * Applications are represented by GIO #GAppInfo objects here.
 * GIO has a concept of recommended and fallback applications for a
 * given content type. Recommended applications are those that claim
 * to handle the content type itself, while fallback also includes
 * applications that handle a more generic content type. GIO also
 * knows the default and last-used application for a given content
 * type. The #GtkAppChooserWidget provides detailed control over
 * whether the shown list of applications should include default,
 * recommended or fallback applications.
 *
 * To obtain the application that has been selected in a #GtkAppChooser,
 * use gtk_app_chooser_get_app_info().
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
   *
   * See [GContentType][gio-GContentType]
   * for more information about content types.
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
 * Returns: (nullable) (transfer full): a #GAppInfo for the currently selected
 *     application, or %NULL if none is selected. Free with g_object_unref()
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
 */
void
gtk_app_chooser_refresh (GtkAppChooser *self)
{
  GTK_APP_CHOOSER_GET_IFACE (self)->refresh (self);
}
