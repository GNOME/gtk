/*
 * gtkappchoosermodule.c: an extension point for online integration
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

#include "gtkappchoosermodule.h"

#include <gio/gio.h>

#include "gtkappchooseronline.h"

#ifdef ENABLE_PACKAGEKIT
#include "gtkappchooseronlinepk.h"
#endif

G_LOCK_DEFINE_STATIC (registered_ep);

void
_gtk_app_chooser_module_ensure (void)
{
  static gboolean registered_ep = FALSE;
  GIOExtensionPoint *ep;

  G_LOCK (registered_ep);

  if (!registered_ep)
    {
      registered_ep = TRUE;

      ep = g_io_extension_point_register ("gtkappchooser-online");
      g_io_extension_point_set_required_type (ep, GTK_TYPE_APP_CHOOSER_ONLINE);

#ifdef ENABLE_PACKAGEKIT
      _gtk_app_chooser_online_pk_get_type ();
#endif
    }

  G_UNLOCK (registered_ep);
}
