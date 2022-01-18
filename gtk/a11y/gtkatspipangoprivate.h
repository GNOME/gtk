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

#include <pango2/pangocairo.h>
#include "gtkatspiprivate.h"

G_BEGIN_DECLS

const char *pango_wrap_mode_to_string (Pango2WrapMode mode);
const char *pango_line_style_to_string (Pango2LineStyle underline);
const char *pango_stretch_to_string (Pango2Stretch stretch);
const char *pango_style_to_string (Pango2Style style);
const char *pango_variant_to_string (Pango2Variant variant);

void gtk_pango_get_font_attributes (Pango2FontDescription *font,
                                    GVariantBuilder      *builder);
void gtk_pango_get_default_attributes (Pango2Layout     *layout,
                                       GVariantBuilder *builder);
void gtk_pango_get_run_attributes     (Pango2Layout     *layout,
                                       GVariantBuilder *builder,
                                       int              offset,
                                       int             *start_offset,
                                       int             *end_offset);

char *gtk_pango_get_text_before (Pango2Layout           *layout,
                                 int                    offset,
                                 AtspiTextBoundaryType  boundary_type,
                                 int                   *start_offset,
                                 int                   *end_offset);
char *gtk_pango_get_text_at     (Pango2Layout           *layout,
                                 int                    offset,
                                 AtspiTextBoundaryType  boundary_type,
                                 int                   *start_offset,
                                 int                   *end_offset);
char *gtk_pango_get_text_after  (Pango2Layout           *layout,
                                 int                    offset,
                                 AtspiTextBoundaryType  boundary_type,
                                 int                   *start_offset,
                                 int                   *end_offset);
char *gtk_pango_get_string_at   (Pango2Layout           *layout,
                                 int                    offset,
                                 AtspiTextGranularity   granularity,
                                 int                   *start_offset,
                                 int                   *end_offset);

G_END_DECLS
