/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#include "gtkstyleproviderprivate.h"
#include "gtkstyleprovider.h"

G_DEFINE_INTERFACE (GtkStyleProviderPrivate, _gtk_style_provider_private, GTK_TYPE_STYLE_PROVIDER)

static void
_gtk_style_provider_private_default_init (GtkStyleProviderPrivateInterface *iface)
{
}

GtkSymbolicColor *
_gtk_style_provider_private_get_color (GtkStyleProviderPrivate *provider,
                                       const char              *name)
{
  GtkStyleProviderPrivateInterface *iface;

  g_return_val_if_fail (GTK_IS_STYLE_PROVIDER_PRIVATE (provider), NULL);

  iface = GTK_STYLE_PROVIDER_PRIVATE_GET_INTERFACE (provider);

  if (!iface->get_color)
    return NULL;

  return iface->get_color (provider, name);
}

void
_gtk_style_provider_private_lookup (GtkStyleProviderPrivate *provider,
                                    GtkWidgetPath           *path,
                                    GtkStateFlags            state,
                                    GtkCssLookup            *lookup)
{
  GtkStyleProviderPrivateInterface *iface;

  g_return_if_fail (GTK_IS_STYLE_PROVIDER_PRIVATE (provider));
  g_return_if_fail (path != NULL);
  g_return_if_fail (lookup != NULL);

  iface = GTK_STYLE_PROVIDER_PRIVATE_GET_INTERFACE (provider);

  if (!iface->lookup)
    return;

  iface->lookup (provider, path, state, lookup);
}
