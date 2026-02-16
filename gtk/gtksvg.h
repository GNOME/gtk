/*
 * Copyright © 2025 Red Hat, Inc
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gsk/gsk.h>
#include <gtk/gtksnapshot.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

#define GTK_TYPE_SVG (gtk_svg_get_type ())

GDK_AVAILABLE_IN_4_22
G_DECLARE_FINAL_TYPE (GtkSvg, gtk_svg, GTK, SVG, GObject)

GDK_AVAILABLE_IN_4_22
GtkSvg *         gtk_svg_new               (void);

GDK_AVAILABLE_IN_4_22
GtkSvg *         gtk_svg_new_from_bytes    (GBytes        *bytes);

GDK_AVAILABLE_IN_4_22
GtkSvg *         gtk_svg_new_from_resource (const char    *path);

GDK_AVAILABLE_IN_4_22
void             gtk_svg_load_from_bytes   (GtkSvg        *self,
                                            GBytes        *bytes);

GDK_AVAILABLE_IN_4_22
void             gtk_svg_load_from_resource (GtkSvg        *self,
                                             const char    *path);

GDK_AVAILABLE_IN_4_22
GBytes *         gtk_svg_serialize         (GtkSvg        *self);

GDK_AVAILABLE_IN_4_22
gboolean         gtk_svg_write_to_file     (GtkSvg        *self,
                                            const char    *filename,
                                            GError       **error);

GDK_AVAILABLE_IN_4_22
void             gtk_svg_set_weight        (GtkSvg        *self,
                                            double         weight);
GDK_AVAILABLE_IN_4_22
double           gtk_svg_get_weight        (GtkSvg        *self);

#define GTK_SVG_STATE_EMPTY ((unsigned int) -1)

GDK_AVAILABLE_IN_4_22
void             gtk_svg_set_state         (GtkSvg        *self,
                                            unsigned int   state);
GDK_AVAILABLE_IN_4_22
unsigned int     gtk_svg_get_state         (GtkSvg        *self);

GDK_AVAILABLE_IN_4_22
unsigned int     gtk_svg_get_n_states      (GtkSvg        *self);

GDK_AVAILABLE_IN_4_22
void             gtk_svg_set_frame_clock   (GtkSvg        *self,
                                            GdkFrameClock *clock);

GDK_AVAILABLE_IN_4_22
void             gtk_svg_play              (GtkSvg        *self);

GDK_AVAILABLE_IN_4_22
void             gtk_svg_pause             (GtkSvg        *self);

/**
 * GtkSvgFeatures:
 * @GTK_SVG_ANIMATIONS: Whether to run animations. If disabled,
 *   state changes are applied without transitions
 * @GTK_SVG_SYSTEM_RESOURCES: Whether to use system resources,
 *   such as fonts. If disabled, only embedded fonts are used
 * @GTK_SVG_EXTERNAL_RESOURCES: Whether to load external
 *   resources, such as images. If disabled, only embedded
 *   images are loaded
 * @GTK_SVG_EXTENSIONS: Whether to allow gpa extensions, such
 *   as states and transitions
 * @GTK_SVG_TRADITIONAL_SYMBOLIC: This feature is meant for
 *   compatibility with old symbolic icons. If this is enabled,
 *   fill and stroke attributes are ignored. The used colors
 *   are derived from symbolic style classes if present, and
 *   the default fill color is the symbolic foreground color.
 *
 * Features of the SVG renderer that can be enabled or disabled.
 *
 * By default, all features except `GTK_SVG_TRADITIONAL_SYMBOLIC`
 * are enabled.
 *
 * New values may be added in the future.
 *
 * Since: 4.22
 */
typedef enum
{
  GTK_SVG_ANIMATIONS           = 1 << 0,
  GTK_SVG_SYSTEM_RESOURCES     = 1 << 1,
  GTK_SVG_EXTERNAL_RESOURCES   = 1 << 2,
  GTK_SVG_EXTENSIONS           = 1 << 3,
  GTK_SVG_TRADITIONAL_SYMBOLIC = 1 << 4,
} GtkSvgFeatures;

/**
 * GTK_SVG_DEFAULT_FEATURES:
 *
 * The `GtkSvgFeatures` that are enabled by default.
 *
 * Since: 4.22
 */
#define GTK_SVG_DEFAULT_FEATURES \
  (GTK_SVG_ANIMATIONS | \
   GTK_SVG_SYSTEM_RESOURCES | \
   GTK_SVG_EXTERNAL_RESOURCES | \
   GTK_SVG_EXTENSIONS)

GDK_AVAILABLE_IN_4_22
void             gtk_svg_set_features      (GtkSvg         *self,
                                            GtkSvgFeatures  features);

GDK_AVAILABLE_IN_4_22
GtkSvgFeatures   gtk_svg_get_features      (GtkSvg         *self);

typedef enum
{
  GTK_SVG_ERROR_INVALID_SYNTAX,
  GTK_SVG_ERROR_INVALID_ELEMENT,
  GTK_SVG_ERROR_INVALID_ATTRIBUTE,
  GTK_SVG_ERROR_MISSING_ATTRIBUTE,
  GTK_SVG_ERROR_INVALID_REFERENCE,
  GTK_SVG_ERROR_FAILED_UPDATE,
  GTK_SVG_ERROR_FAILED_RENDERING,
} GtkSvgError;

typedef struct
{
  size_t bytes;
  size_t lines;
  size_t line_chars;
} GtkSvgLocation;

#define GTK_SVG_ERROR (gtk_svg_error_quark ())

GDK_AVAILABLE_IN_4_22
GQuark       gtk_svg_error_quark               (void);
GDK_AVAILABLE_IN_4_22
const char * gtk_svg_error_get_element     (const GError *error);
GDK_AVAILABLE_IN_4_22
const char * gtk_svg_error_get_attribute   (const GError *error);
GDK_AVAILABLE_IN_4_22
const GtkSvgLocation *
              gtk_svg_error_get_start      (const GError *error);
GDK_AVAILABLE_IN_4_22
const GtkSvgLocation *
              gtk_svg_error_get_end        (const GError *error);

G_END_DECLS
