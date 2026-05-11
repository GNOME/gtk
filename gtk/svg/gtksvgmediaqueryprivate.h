/*
 * Copyright © 2026 Red Hat, Inc
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

#pragma once

#include <glib.h>
#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvg.h"

G_BEGIN_DECLS

typedef union _SvgCssMediaCondition SvgCssMediaCondition;
typedef struct _SvgCssMediaBlock SvgCssMediaBlock;

typedef enum
{
  SVG_CSS_MEDIA_NOT,
  SVG_CSS_MEDIA_AND,
  SVG_CSS_MEDIA_OR,
  SVG_CSS_MEDIA_FEATURE,
} SvgCssMediaType;

union _SvgCssMediaCondition
{
  SvgCssMediaType type;
  struct {
    SvgCssMediaType type;
    SvgCssMediaCondition *condition;
  } not;
  struct {
    SvgCssMediaType type;
    unsigned int length;
    SvgCssMediaCondition **condition;
  } and_or;
  struct {
    SvgCssMediaType type;
    const char *name;
    const char *value;
  } feature;
};

void     svg_css_media_condition_free     (SvgCssMediaCondition *condition);
gboolean svg_css_media_condition_evaluate (SvgCssMediaCondition *condition,
                                           GArray               *features);

struct _SvgCssMediaBlock
{
  SvgCssMediaBlock *next;
  SvgCssMediaCondition *condition;
  gboolean value;
};

void     svg_css_media_block_free      (SvgCssMediaBlock *media);
void     svg_css_media_block_evaluate  (SvgCssMediaBlock *media,
                                        GArray           *features);

SvgCssMediaCondition * parse_media_query (GtkCssParser *parser);

void                   gtk_svg_update_media (GtkSvg *self);

G_END_DECLS
