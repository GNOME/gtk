/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkstyleprovider.h"

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkwidgetpath.h"

/**
 * SECTION:gtkstyleprovider
 * @Short_description: Interface to provide style information to GtkStyleContext
 * @Title: GtkStyleProvider
 * @See_also: #GtkStyleContext, #GtkCssProvider
 *
 * GtkStyleProvider is an interface used to provide style information to a #GtkStyleContext.
 * See gtk_style_context_add_provider() and gtk_style_context_add_provider_for_screen().
 */

static void gtk_style_provider_iface_init (gpointer g_iface);

GType
gtk_style_provider_get_type (void)
{
  static GType style_provider_type = 0;

  if (!style_provider_type)
    style_provider_type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                                         I_("GtkStyleProvider"),
                                                         sizeof (GtkStyleProviderIface),
                                                         (GClassInitFunc) gtk_style_provider_iface_init,
                                                         0, NULL, 0);
  return style_provider_type;
}

static void
gtk_style_provider_iface_init (gpointer g_iface)
{
}
