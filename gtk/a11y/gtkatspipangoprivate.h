/* gtkatspipangoprivate.h: Utilities for pango and AT-SPI
 * Copyright 2020 Red Hat, Inc
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

#pragma once

#include <pango/pangocairo.h>
#include "gtkatspiprivate.h"

G_BEGIN_DECLS

const char *pango_wrap_mode_to_string (PangoWrapMode mode);
const char *pango_underline_to_string (PangoUnderline underline);
const char *pango_stretch_to_string (PangoStretch stretch);
const char *pango_style_to_string (PangoStyle style);
const char *pango_variant_to_string (PangoVariant variant);

void gtk_pango_get_font_attributes (PangoFontDescription *font,
                                    GVariantBuilder      *builder);
void gtk_pango_get_default_attributes (PangoLayout     *layout,
                                       GVariantBuilder *builder);
void gtk_pango_get_run_attributes     (PangoLayout     *layout,
                                       GVariantBuilder *builder,
                                       int              offset,
                                       int             *start_offset,
                                       int             *end_offset);

char *gtk_pango_get_text_before (PangoLayout           *layout,
                                 int                    offset,
                                 AtspiTextBoundaryType  boundary_type,
                                 int                   *start_offset,
                                 int                   *end_offset);
char *gtk_pango_get_text_at     (PangoLayout           *layout,
                                 int                    offset,
                                 AtspiTextBoundaryType  boundary_type,
                                 int                   *start_offset,
                                 int                   *end_offset);
char *gtk_pango_get_text_after  (PangoLayout           *layout,
                                 int                    offset,
                                 AtspiTextBoundaryType  boundary_type,
                                 int                   *start_offset,
                                 int                   *end_offset);
char *gtk_pango_get_string_at   (PangoLayout           *layout,
                                 int                    offset,
                                 AtspiTextGranularity   granularity,
                                 int                   *start_offset,
                                 int                   *end_offset);

G_END_DECLS
