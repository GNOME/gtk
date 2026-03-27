/*
 * Copyright © 2025 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtksvgerrorprivate.h"

typedef struct
{
  char *element;
  char *attribute;
  GtkSvgLocation start;
  GtkSvgLocation end;
} GtkSvgErrorPrivate;

static void
gtk_svg_error_private_init (GtkSvgErrorPrivate *priv)
{
}

static void
gtk_svg_error_private_copy (const GtkSvgErrorPrivate *src,
                            GtkSvgErrorPrivate       *dest)
{
  g_assert (dest != NULL);
  g_assert (src != NULL);
  dest->element = g_strdup (src->element);
  dest->attribute = g_strdup (src->attribute);
  dest->start = src->start;
  dest->end = src->end;
}

static void
gtk_svg_error_private_clear (GtkSvgErrorPrivate *priv)
{
  g_assert (priv != NULL);
  g_free (priv->element);
  g_free (priv->attribute);
}

G_DEFINE_EXTENDED_ERROR (GtkSvgError, gtk_svg_error);

void
gtk_svg_error_set_element (GError     *error,
                           const char *element)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);
  g_assert (error->domain == GTK_SVG_ERROR);
  priv->element = g_strdup (element);
}

void
gtk_svg_error_set_attribute (GError     *error,
                             const char *attribute)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);
  g_assert (error->domain == GTK_SVG_ERROR);
  priv->attribute = g_strdup (attribute);
}

void
gtk_svg_error_set_location (GError               *error,
                            const GtkSvgLocation *start,
                            const GtkSvgLocation *end)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);
  g_assert (error->domain == GTK_SVG_ERROR);
  if (start)
    priv->start = *start;
  if (end)
    priv->end = *end;
}

/**
 * GtkSvgError:
 * @GTK_SVG_ERROR_INVALID_SYNTAX: The XML syntax is broken
 *   in some way
 * @GTK_SVG_ERROR_INVALID_ELEMENT: An XML element is invalid
 *   (either because it is not part of SVG, or because it is
 *   in the wrong place, or because it not implemented in GTK)
 * @GTK_SVG_ERROR_INVALID_ATTRIBUTE: An attribute is invalid
 *   (either because it is not part of SVG, or because it is
 *   not implemented in GTK, or its value is problematic)
 * @GTK_SVG_ERROR_MISSING_ATTRIBUTE: A required attribute is missing
 * @GTK_SVG_ERROR_INVALID_REFERENCE: A reference does not point to
 *   a suitable element
 * @GTK_SVG_ERROR_FAILED_UPDATE: An animation could not be updated
 * @GTK_SVG_ERROR_FAILED_RENDERING: Rendering is not according to
 *   expecations
 * @GTK_SVG_ERROR_IGNORED_ELEMENT: An XML element is ignored,
 *   but it should not affect rendering (this error code is used
 *   for metadata and exension elements)
 * @GTK_SVG_ERROR_LIMITS_EXCEEDED: An implementation limit has
 *   been hit, such as the number of loaded shapes.
 * @GTK_SVG_ERROR_NOT_IMPLEMENTED: The SVG uses features that
 *   are not supported by `GtkSvg`. It may be advisable to use
 *   a different SVG renderer.
 *
 * Error codes in the `GTK_SVG_ERROR` domain for errors
 * that happen during parsing or rendering of SVG.
 *
 * Since: 4.22
 */

/* GTK_SVG_ERROR_FEATURE_DISABLED:
 *
 * The SVG uses features that have been disabled with `gtk_svg_set_features`.
 *
 * Since: 4.24
 */

/**
 * gtk_svg_error_get_element:
 * @error: an error in the [error@Gtk.SvgError] domain
 *
 * Returns context information about what XML element
 * the parsing error occurred in.
 *
 * Returns: (nullable): the element name
 *
 * Since: 4.22
 */
const char *
gtk_svg_error_get_element (const GError *error)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);

  if (priv)
    return priv->element;
  else
    return NULL;
}

/**
 * gtk_svg_error_get_attribute:
 * @error: an error in the [error@Gtk.SvgError] domain
 *
 * Returns context information about what XML attribute
 * the parsing error occurred in.
 *
 * Returns: (nullable): the attribute name
 *
 * Since: 4.22
 */
const char *
gtk_svg_error_get_attribute (const GError *error)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);

  if (priv)
    return priv->attribute;
  else
    return NULL;
}

/**
 * gtk_svg_error_get_start:
 * @error: an error in the [error@Gtk.SvgError] domain
 *
 * Returns context information about the start position
 * in the document where the parsing error occurred.
 *
 * Returns: (nullable): the [struct@Gtk.SvgLocation]
 *
 * Since: 4.22
 */
const GtkSvgLocation *
gtk_svg_error_get_start (const GError *error)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);

  if (priv)
    return &priv->start;
  else
    return NULL;
}

/**
 * gtk_svg_error_get_end:
 * @error: an error in the [error@Gtk.SvgError] domain
 *
 * Returns context information about the end position
 * in the document where the parsing error occurred.
 *
 * Returns: (nullable): the [struct@Gtk.SvgLocation]
 *
 * Since: 4.22
 */
const GtkSvgLocation *
gtk_svg_error_get_end (const GError *error)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);

  if (priv)
    return &priv->end;
  else
    return NULL;
}
