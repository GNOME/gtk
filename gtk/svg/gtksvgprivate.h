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

#pragma once

#include "gtksvg.h"
#include "gtkenums.h"
#include "gtkbitmaskprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtk/svg/gtksvgtypesprivate.h"
#include "gtk/svg/gtksvgenumsprivate.h"
#include "gtk/svg/gtksvgelementtypeprivate.h"
#include "gtk/svg/gtksvgfiltertypeprivate.h"
#include "gtk/svg/gtksvgpropertyprivate.h"

G_BEGIN_DECLS

#define DEFAULT_FONT_SIZE 13.333

struct _GtkSvg
{
  GObject parent_instance;
  SvgElement *content;

  double current_width, current_height; /* Last snapshot size */

  double width, height; /* Intrinsic size */

  double weight;
  unsigned int initial_state;
  unsigned int state;
  unsigned int max_state;
  unsigned int n_state_names;
  char **state_names;
  int64_t state_change_delay;
  gboolean has_animations;
  GtkSvgFeatures features;
  GtkSvgUses used;

  char *resource;

  int64_t load_time;
  int64_t current_time;
  int64_t pause_time;

  GtkOverflow overflow;
  gboolean playing;
  GtkSvgRunMode run_mode;
  GdkFrameClock *clock;
  unsigned long clock_update_id;

  int64_t next_update;
  unsigned int pending_advance;
  gboolean advance_after_snapshot;

  unsigned int gpa_version;
  char *author;
  char *license;
  char *description;
  char *keywords;

  Timeline *timeline;

  GHashTable *images;

  PangoFontMap *fontmap;
  GPtrArray *font_files;

  GskRenderNode *node;

  struct {
    double width, height;
    GdkRGBA colors[5];
    size_t n_colors;
    double weight;
    int64_t time;
    unsigned int state;
  } node_for;
};

double       svg_shape_attr_get_number    (SvgElement                 *shape,
                                           SvgProperty            attr,
                                           const graphene_rect_t *viewport);
GskPath *    svg_shape_attr_get_path      (SvgElement                 *shape,
                                           SvgProperty            attr);
unsigned int svg_shape_attr_get_enum      (SvgElement                 *shape,
                                           SvgProperty            attr);
PaintKind    svg_shape_attr_get_paint     (SvgElement                 *shape,
                                           SvgProperty            attr,
                                           GtkSymbolicColor      *symbolic,
                                           GdkRGBA               *color);
double *     svg_shape_attr_get_points    (SvgElement                 *shape,
                                           SvgProperty            attr,
                                           unsigned int          *n_params);
ClipKind     svg_shape_attr_get_clip      (SvgElement                 *shape,
                                           SvgProperty            attr,
                                           GskPath              **path,
                                           const char           **ref);
const char * svg_shape_attr_get_mask      (SvgElement                 *shape,
                                           SvgProperty            attr);
char *       svg_shape_attr_get_transform (SvgElement                 *shape,
                                           SvgProperty            attr);
char *       svg_shape_attr_get_filter    (SvgElement                 *shape,
                                           SvgProperty            attr);
GskPath *    svg_shape_get_path           (SvgElement                 *shape,
                                           const graphene_rect_t *viewport);
gboolean     svg_shape_attr_get_viewbox   (SvgElement                 *shape,
                                           SvgProperty            attr,
                                           graphene_rect_t       *rect);

void         svg_shape_attr_set           (SvgElement                 *shape,
                                           SvgProperty            attr,
                                           SvgValue              *value);

SvgElement *      svg_shape_add          (SvgElement            *parent,
                                     SvgElementType    type);
void         svg_shape_delete       (SvgElement            *shape);

const char * svg_shape_get_name     (SvgElementType  type);
gboolean     svg_shape_has_attr     (SvgElement          *shape,
                                     SvgProperty     attr);

gboolean     gtk_svg_set_state_names (GtkSvg      *svg,
                                      const char **names);

/* --- */

GtkSvg *       gtk_svg_copy            (GtkSvg                *orig);

gboolean       gtk_svg_equal           (GtkSvg                *svg1,
                                        GtkSvg                *svg2);

void           gtk_svg_set_load_time   (GtkSvg                *self,
                                        int64_t                load_time);

void           gtk_svg_set_playing     (GtkSvg                *self,
                                        gboolean               playing);

void           gtk_svg_clear_content   (GtkSvg                *self);

void           gtk_svg_advance         (GtkSvg                *self,
                                        int64_t                current_time);

GtkSvgRunMode  gtk_svg_get_run_mode    (GtkSvg                *self);

int64_t        gtk_svg_get_next_update (GtkSvg                *self);

/*< private >
 * GtkSvgSerializeFlags:
 * @GTK_SVG_SERIALIZE_DEFAULT: Default behavior. Serialize
 *   the DOM, with gpa attributes, and with compatibility
 *   tweaks
 * @GTK_SVG_SERIALIZE_AT_CURRENT_TIME: Serialize the current
 *   values of a running animation, as opposed to the DOM
 *   values that the parser produced
 * @GTK_SVG_SERIALIZE_INCLUDE_STATE: Include custom attributes
 *   with various information about the state of the renderer,
 *   such as the current time, or the status of running animations
 * @GTK_SVG_SERIALIZE_EXPAND_GPA_ATTRS: Instead of gpa attributes,
 *   include the animations that were generated from them
 * @GTK_SVG_SERIALIZE_NO_COMPAT: Don't include things that
 *   improve the rendering of the serialized result in renderers
 *   which don't support extensions, but stick to the pristine
 *   DOM
 */
typedef enum
{
  GTK_SVG_SERIALIZE_DEFAULT            = 0,
  GTK_SVG_SERIALIZE_AT_CURRENT_TIME    = 1 << 0,
  GTK_SVG_SERIALIZE_EXCLUDE_ANIMATION  = 1 << 1,
  GTK_SVG_SERIALIZE_INCLUDE_STATE      = 1 << 2,
  GTK_SVG_SERIALIZE_EXPAND_GPA_ATTRS   = 1 << 3,
  GTK_SVG_SERIALIZE_NO_COMPAT          = 1 << 4,
} GtkSvgSerializeFlags;

GBytes *       gtk_svg_serialize_full  (GtkSvg                *self,
                                        const GdkRGBA         *colors,
                                        size_t                 n_colors,
                                        GtkSvgSerializeFlags   flags);

GskRenderNode *gtk_svg_apply_filter    (GtkSvg                *svg,
                                        const char            *filter,
                                        const graphene_rect_t *bounds,
                                        GskRenderNode         *node);

G_END_DECLS
