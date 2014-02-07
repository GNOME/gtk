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

/**
 * gtk_style_provider_get_style:
 * @provider: a #GtkStyleProvider
 * @path: #GtkWidgetPath to query
 *
 * Returns the style settings affecting a widget defined by @path, or %NULL if
 * @provider doesn’t contemplate styling @path.
 *
 * Returns: (transfer full): a #GtkStyleProperties containing the
 * style settings affecting @path
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: Will always return %NULL for all GTK-provided style providers
 *     as the interface cannot correctly work the way CSS is specified.
 **/
GtkStyleProperties *
gtk_style_provider_get_style (GtkStyleProvider *provider,
                              GtkWidgetPath    *path)
{
  GtkStyleProviderIface *iface;

  g_return_val_if_fail (GTK_IS_STYLE_PROVIDER (provider), NULL);

  iface = GTK_STYLE_PROVIDER_GET_IFACE (provider);

  if (!iface->get_style)
    return NULL;

  return iface->get_style (provider, path);
}

/**
 * gtk_style_provider_get_style_property:
 * @provider: a #GtkStyleProvider
 * @path: #GtkWidgetPath to query
 * @state: state to query the style property for
 * @pspec: The #GParamSpec to query
 * @value: (out): return location for the property value
 *
 * Looks up a widget style property as defined by @provider for
 * the widget represented by @path.
 *
 * Returns: %TRUE if the property was found and has a value, %FALSE otherwise
 *
 * Since: 3.0
 **/
gboolean
gtk_style_provider_get_style_property (GtkStyleProvider *provider,
                                       GtkWidgetPath    *path,
                                       GtkStateFlags     state,
                                       GParamSpec       *pspec,
                                       GValue           *value)
{
  GtkStyleProviderIface *iface;

  g_return_val_if_fail (GTK_IS_STYLE_PROVIDER (provider), FALSE);
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (g_type_is_a (gtk_widget_path_get_object_type (path), pspec->owner_type), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  iface = GTK_STYLE_PROVIDER_GET_IFACE (provider);

  if (!iface->get_style_property)
    return FALSE;

  return iface->get_style_property (provider, path, state, pspec, value);
}

/**
 * gtk_style_provider_get_icon_factory:
 * @provider: a #GtkStyleProvider
 * @path: #GtkWidgetPath to query
 *
 * Returns the #GtkIconFactory defined to be in use for @path, or %NULL if none
 * is defined.
 *
 * Returns: (transfer none): The icon factory to use for @path, or %NULL
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: Will always return %NULL for all GTK-provided style providers.
 **/
GtkIconFactory *
gtk_style_provider_get_icon_factory (GtkStyleProvider *provider,
				     GtkWidgetPath    *path)
{
  GtkStyleProviderIface *iface;

  g_return_val_if_fail (GTK_IS_STYLE_PROVIDER (provider), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  iface = GTK_STYLE_PROVIDER_GET_IFACE (provider);

  if (!iface->get_icon_factory)
    return NULL;

  return iface->get_icon_factory (provider, path);
}
