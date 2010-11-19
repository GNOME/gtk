/*
 * gtkopenwithonlinedummy.c: an extension point for online integration
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

#include "gtkopenwithonlinedummy.h"

#include "gtkintl.h"
#include "gtkopenwithonline.h"

#include <gio/gio.h>

#define gtk_open_with_online_dummy_get_type _gtk_open_with_online_dummy_get_type
static void open_with_online_iface_init (GtkOpenWithOnlineInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkOpenWithOnlineDummy, gtk_open_with_online_dummy,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_OPEN_WITH_ONLINE,
						open_with_online_iface_init)
			 g_io_extension_point_implement ("gtkopenwith-online",
							 g_define_type_id,
							 "dummy", 0));

static void
gtk_open_with_online_dummy_class_init (GtkOpenWithOnlineDummyClass *klass)
{
  /* do nothing */
}

static void
gtk_open_with_online_dummy_init (GtkOpenWithOnlineDummy *self)
{
  /* do nothing */
}

static gboolean
dummy_search_mime_finish (GtkOpenWithOnline *obj,
			  GAsyncResult *res,
			  GError **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);

  return !g_simple_async_result_propagate_error (simple, error);
}

static void
dummy_search_mime_async (GtkOpenWithOnline *obj,
			 const gchar *content_type,
			 GtkWindow *parent,
			 GAsyncReadyCallback callback,
			 gpointer user_data)
{
  g_simple_async_report_error_in_idle (G_OBJECT (obj),
				       callback, user_data,
				       G_IO_ERROR,
				       G_IO_ERROR_FAILED,
				       "%s",
				       _("Operation not supported"));
}

static void
open_with_online_iface_init (GtkOpenWithOnlineInterface *iface)
{
  iface->search_for_mimetype_async = dummy_search_mime_async;
  iface->search_for_mimetype_finish = dummy_search_mime_finish;
}
