/*
 * gtkopenwithonline.h: an extension point for online integration
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

#include <config.h>

#include "gtkopenwithonline.h"

#include "gtkopenwithonlinedummy.h"
#include "gtkopenwithmodule.h"

#include <gio/gio.h>

G_DEFINE_INTERFACE (GtkOpenWithOnline, gtk_open_with_online, G_TYPE_OBJECT);

static void
gtk_open_with_online_default_init (GtkOpenWithOnlineInterface *iface)
{
  /* do nothing */
}

GtkOpenWithOnline *
gtk_open_with_online_get_default (void)
{
  GIOExtensionPoint *ep;
  GIOExtension *extension;
  GList *extensions;
  GtkOpenWithOnline *retval;

  _gtk_open_with_module_ensure ();

  ep = g_io_extension_point_lookup ("gtkopenwith-online");
  extensions = g_io_extension_point_get_extensions (ep);

  if (extensions != NULL)
    {
      /* pick the first */
      extension = extensions->data;
      retval = g_object_new (g_io_extension_get_type (extension),
			     NULL);
    }
  else
    {
      retval = g_object_new (GTK_TYPE_OPEN_WITH_ONLINE_DUMMY,
			     NULL);
    }

  return retval;
}

void
gtk_open_with_online_search_for_mimetype_async (GtkOpenWithOnline *self,
						const gchar *content_type,
						GtkWindow *parent,
						GAsyncReadyCallback callback,
						gpointer user_data)
{
  GtkOpenWithOnlineInterface *iface;

  g_return_if_fail (GTK_IS_OPEN_WITH_ONLINE (self));

  iface = GTK_OPEN_WITH_ONLINE_GET_IFACE (self);

  (* iface->search_for_mimetype_async) (self, content_type, parent, callback, user_data);
}

gboolean
gtk_open_with_online_search_for_mimetype_finish (GtkOpenWithOnline *self,
						 GAsyncResult *res,
						 GError **error)
{
  GtkOpenWithOnlineInterface *iface;

  g_return_val_if_fail (GTK_IS_OPEN_WITH_ONLINE (self), FALSE);

  iface = GTK_OPEN_WITH_ONLINE_GET_IFACE (self);

  return ((* iface->search_for_mimetype_finish) (self, res, error));
}
