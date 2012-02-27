/*
 * gtkappchooseronline.h: an extension point for online integration
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

#include "config.h"

#include "gtkappchooseronline.h"

#include "gtkappchoosermodule.h"
#include "gtkintl.h"

#include <gio/gio.h>

G_DEFINE_INTERFACE_WITH_CODE (GtkAppChooserOnline, _gtk_app_chooser_online, G_TYPE_OBJECT,
                              g_type_interface_add_prerequisite (g_define_type_id, G_TYPE_ASYNC_INITABLE);)

static void
_gtk_app_chooser_online_default_init (GtkAppChooserOnlineInterface *iface)
{
  /* do nothing */
}

GtkAppChooserOnline *
_gtk_app_chooser_online_get_default_finish (GObject      *source,
                                            GAsyncResult *result)
{
  GtkAppChooserOnline *retval;

  retval = GTK_APP_CHOOSER_ONLINE (g_async_initable_new_finish (G_ASYNC_INITABLE (source),
                                                                result, NULL));

  return retval;
}

void
_gtk_app_chooser_online_get_default_async (GAsyncReadyCallback callback,
                                           gpointer            user_data)
{
  GIOExtensionPoint *ep;
  GIOExtension *extension;
  GList *extensions;

  _gtk_app_chooser_module_ensure ();

  ep = g_io_extension_point_lookup ("gtkappchooser-online");
  extensions = g_io_extension_point_get_extensions (ep);

  if (extensions != NULL)
    {
      /* pick the first */
      extension = extensions->data;
      g_async_initable_new_async (g_io_extension_get_type (extension), G_PRIORITY_DEFAULT,
                                  NULL, callback, user_data, NULL);
    }
}

void
_gtk_app_chooser_online_search_for_mimetype_async (GtkAppChooserOnline *self,
                                                   const gchar         *content_type,
                                                   GtkWindow           *parent,
                                                   GCancellable        *cancellable,
                                                   GAsyncReadyCallback  callback,
                                                   gpointer             user_data)
{
  GtkAppChooserOnlineInterface *iface;

  g_return_if_fail (GTK_IS_APP_CHOOSER_ONLINE (self));

  iface = GTK_APP_CHOOSER_ONLINE_GET_IFACE (self);

  (* iface->search_for_mimetype_async) (self, content_type, parent, cancellable, callback, user_data);
}

gboolean
_gtk_app_chooser_online_search_for_mimetype_finish (GtkAppChooserOnline  *self,
                                                    GAsyncResult         *res,
                                                    GError              **error)
{
  GtkAppChooserOnlineInterface *iface;

  g_return_val_if_fail (GTK_IS_APP_CHOOSER_ONLINE (self), FALSE);

  iface = GTK_APP_CHOOSER_ONLINE_GET_IFACE (self);

  return ((* iface->search_for_mimetype_finish) (self, res, error));
}
