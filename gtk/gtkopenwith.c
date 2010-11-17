/*
 * gtkopenwith.c: open-with interface
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

#include "gtkopenwith.h"

#include "gtkintl.h"
#include "gtkopenwithprivate.h"
#include "gtkwidget.h"

#include <glib.h>

G_DEFINE_INTERFACE (GtkOpenWith, gtk_open_with, GTK_TYPE_WIDGET);

static void
gtk_open_with_default_init (GtkOpenWithIface *iface)
{
  GParamSpec *pspec;

  pspec = g_param_spec_string ("content-type",
			       P_("Content type"),
			       P_("The content type used by the open with object"),
			       NULL,
			       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
			       G_PARAM_STATIC_STRINGS);
  g_object_interface_install_property (iface, pspec);
}

gchar *
gtk_open_with_get_content_type (GtkOpenWith *self)
{
  gchar *retval = NULL;

  g_return_val_if_fail (GTK_IS_OPEN_WITH (self), NULL);

  g_object_get (self,
		"content-type", &retval,
		NULL);

  return retval;
}

GAppInfo *
gtk_open_with_get_app_info (GtkOpenWith *self)
{
  return GTK_OPEN_WITH_GET_IFACE (self)->get_app_info (self);
}
