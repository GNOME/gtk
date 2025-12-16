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

#include "gtksvg.h"
#include "gtkbitmaskprivate.h"
#include "gtkenums.h"

G_BEGIN_DECLS

#define INDEFINITE G_MAXINT64
#define REPEAT_FOREVER INFINITY

typedef struct _SvgValue SvgValue;
typedef struct _Shape Shape;
typedef struct _Timeline Timeline;

typedef enum
{
  ALIGN_MIN,
  ALIGN_MID,
  ALIGN_MAX,
} Align;

typedef enum
{
  MEET,
  SLICE,
} MeetOrSlice;

typedef enum
{
  GTK_SVG_RUN_MODE_STOPPED,
  GTK_SVG_RUN_MODE_DISCRETE,
  GTK_SVG_RUN_MODE_CONTINUOUS,
} GtkSvgRunMode;

struct _GtkSvg
{
  GObject parent_instance;
  Shape *content;

  double width, height;
  graphene_rect_t bounds;
  graphene_rect_t viewport;

  double weight;
  unsigned int state;
  unsigned int max_state;
  int64_t state_change_delay;

  int64_t load_time;
  int64_t current_time;

  gboolean playing;
  GtkSvgRunMode run_mode;
  GdkFrameClock *clock;
  unsigned long clock_update_id;
  unsigned int periodic_update_id;

  int64_t next_update;
  unsigned int pending_invalidate;
  gboolean advance_after_snapshot;

  unsigned int gpa_version;
  char *gpa_keywords;

  Timeline *timeline;

  GHashTable *images;
};

typedef enum
{
  SHAPE_LINE,
  SHAPE_POLYLINE,
  SHAPE_POLYGON,
  SHAPE_RECT,
  SHAPE_CIRCLE,
  SHAPE_ELLIPSE,
  SHAPE_PATH,
  SHAPE_GROUP,
  SHAPE_CLIP_PATH,
  SHAPE_MASK,
  SHAPE_DEFS,
  SHAPE_USE,
  SHAPE_LINEAR_GRADIENT,
  SHAPE_RADIAL_GRADIENT,
  SHAPE_PATTERN,
  SHAPE_MARKER,
  SHAPE_TEXT,
  SHAPE_TSPAN,
  SHAPE_SVG,
  SHAPE_IMAGE,
} ShapeType;

typedef enum
{
  SHAPE_ATTR_LANG,
  FIRST_SHAPE_ATTR = SHAPE_ATTR_LANG,
  SHAPE_ATTR_VISIBILITY,
  SHAPE_ATTR_TRANSFORM,
  SHAPE_ATTR_OPACITY,
  SHAPE_ATTR_OVERFLOW,
  SHAPE_ATTR_FILTER,
  SHAPE_ATTR_CLIP_PATH,
  SHAPE_ATTR_CLIP_RULE,
  SHAPE_ATTR_MASK,
  SHAPE_ATTR_MASK_TYPE,
  SHAPE_ATTR_FILL,
  SHAPE_ATTR_FILL_OPACITY,
  SHAPE_ATTR_FILL_RULE,
  SHAPE_ATTR_STROKE,
  SHAPE_ATTR_STROKE_OPACITY,
  SHAPE_ATTR_STROKE_WIDTH,
  SHAPE_ATTR_STROKE_LINECAP,
  SHAPE_ATTR_STROKE_LINEJOIN,
  SHAPE_ATTR_STROKE_MITERLIMIT,
  SHAPE_ATTR_STROKE_DASHARRAY,
  SHAPE_ATTR_STROKE_DASHOFFSET,
  SHAPE_ATTR_PAINT_ORDER,
  SHAPE_ATTR_BLEND_MODE,
  SHAPE_ATTR_ISOLATION,
  SHAPE_ATTR_HREF,
  SHAPE_ATTR_PATH_LENGTH,
  SHAPE_ATTR_PATH,
  SHAPE_ATTR_CX,
  SHAPE_ATTR_CY,
  SHAPE_ATTR_R,
  SHAPE_ATTR_X,
  SHAPE_ATTR_Y,
  SHAPE_ATTR_WIDTH,
  SHAPE_ATTR_HEIGHT,
  SHAPE_ATTR_RX,
  SHAPE_ATTR_RY,
  SHAPE_ATTR_X1,
  SHAPE_ATTR_Y1,
  SHAPE_ATTR_X2,
  SHAPE_ATTR_Y2,
  SHAPE_ATTR_POINTS,
  SHAPE_ATTR_SPREAD_METHOD,
  SHAPE_ATTR_CONTENT_UNITS,
  SHAPE_ATTR_BOUND_UNITS,
  SHAPE_ATTR_FX,
  SHAPE_ATTR_FY,
  SHAPE_ATTR_FR,
  SHAPE_ATTR_VIEW_BOX,
  SHAPE_ATTR_CONTENT_FIT,
  SHAPE_ATTR_REF_X,
  SHAPE_ATTR_REF_Y,
  SHAPE_ATTR_MARKER_UNITS,
  SHAPE_ATTR_MARKER_ORIENT,
  SHAPE_ATTR_MARKER_START,
  SHAPE_ATTR_MARKER_MID,
  SHAPE_ATTR_MARKER_END,
  SHAPE_ATTR_TEXT_ANCHOR,
  SHAPE_ATTR_DX,
  SHAPE_ATTR_DY,
  SHAPE_ATTR_UNICODE_BIDI,
  SHAPE_ATTR_DIRECTION,
  SHAPE_ATTR_WRITING_MODE,
  SHAPE_ATTR_FONT_FAMILY,
  SHAPE_ATTR_FONT_STYLE,
  SHAPE_ATTR_FONT_VARIANT,
  SHAPE_ATTR_FONT_WEIGHT,
  SHAPE_ATTR_FONT_STRECH, // Deprecated & not part of SVG2!
  SHAPE_ATTR_FONT_SIZE,
  SHAPE_ATTR_LETTER_SPACING,
  SHAPE_ATTR_TEXT_DECORATION,
  SHAPE_ATTR_STROKE_MINWIDTH,
  SHAPE_ATTR_STROKE_MAXWIDTH,
  LAST_SHAPE_ATTR = SHAPE_ATTR_STROKE_MAXWIDTH,
  SHAPE_ATTR_STOP_OFFSET,
  FIRST_STOP_ATTR = SHAPE_ATTR_STOP_OFFSET,
  SHAPE_ATTR_STOP_COLOR,
  SHAPE_ATTR_STOP_OPACITY,
  LAST_STOP_ATTR = SHAPE_ATTR_STOP_OPACITY,
} ShapeAttr;

#define N_SHAPE_ATTRS (LAST_SHAPE_ATTR + 1 - FIRST_SHAPE_ATTR)
#define N_STOP_ATTRS (LAST_STOP_ATTR + 1 - FIRST_STOP_ATTR)

typedef enum
{
  GPA_TRANSITION_NONE,
  GPA_TRANSITION_ANIMATE,
  GPA_TRANSITION_MORPH,
  GPA_TRANSITION_FADE,
} GpaTransition;

typedef enum
{
  GPA_ANIMATION_NONE,
  GPA_ANIMATION_NORMAL,
  GPA_ANIMATION_ALTERNATE,
  GPA_ANIMATION_REVERSE,
  GPA_ANIMATION_REVERSE_ALTERNATE,
  GPA_ANIMATION_IN_OUT,
  GPA_ANIMATION_IN_OUT_ALTERNATE,
  GPA_ANIMATION_IN_OUT_REVERSE,
  GPA_ANIMATION_SEGMENT,
  GPA_ANIMATION_SEGMENT_ALTERNATE,
} GpaAnimation;

typedef enum
{
  GPA_EASING_LINEAR,
  GPA_EASING_EASE_IN_OUT,
  GPA_EASING_EASE_IN,
  GPA_EASING_EASE_OUT,
  GPA_EASING_EASE,
} GpaEasing;

typedef enum
{
  TEXT_NODE_SHAPE,
  TEXT_NODE_CHARACTERS
} TextNodeType;

typedef struct
{
  TextNodeType type;
  union {
    struct {
      Shape *shape;
      gboolean has_bounds; // FALSE for text nodes without any character children
      graphene_rect_t bounds;
    } shape;
    struct {
      char *text;
      PangoLayout *layout;
      double x, y, r;
    } characters;
  };
} TextNode;

struct _Shape
{
  ShapeType type;
  gboolean display;
  Shape *parent;
  GtkBitmask *attrs;
  char *id;

  /* Dependency order for computing updates */
  Shape *first;
  Shape *next;

  gboolean computed_for_use;

  SvgValue *base[N_SHAPE_ATTRS];
  SvgValue *current[N_SHAPE_ATTRS];

  GPtrArray *shapes;
  GPtrArray *animations;
  GPtrArray *color_stops;
  GPtrArray *deps;

  GskPath *path;
  GskPathMeasure *measure;
  union {
    struct {
      double cx, cy, r;
    } circle;
    struct {
      double cx, cy, rx, ry;
    } ellipse;
    struct {
      double x, y, w, h, rx, ry;
    } rect;
    struct {
      double x1, y1, x2, y2;
    } line;
    struct {
      SvgValue *points;
    } polyline;
  } path_for;

  // GArray<TextNode>
  GArray *text;

  struct {
    uint64_t states;
    GpaTransition transition;
    GpaEasing transition_easing;
    int64_t transition_duration;
    int64_t transition_delay;
    GpaAnimation animation;
    GpaEasing animation_easing;
    int64_t animation_duration;
    double animation_repeat;
    double animation_segment;
    double origin;
    struct {
      char *ref;
      Shape *shape;
      double pos;
    } attach;
  } gpa;
};

typedef enum
{
  SVG_DIMENSION_NUMBER,
  SVG_DIMENSION_PERCENTAGE,
  SVG_DIMENSION_LENGTH,
} SvgDimension;

typedef enum
{
  PAINT_NONE,
  PAINT_CONTEXT_FILL,
  PAINT_CONTEXT_STROKE,
  PAINT_COLOR,
  PAINT_SYMBOLIC,
  PAINT_SERVER,
} PaintKind;

typedef enum
{
  PAINT_ORDER_FILL_STROKE_MARKERS,
  PAINT_ORDER_FILL_MARKERS_STROKE,
  PAINT_ORDER_STROKE_FILL_MARKERS,
  PAINT_ORDER_STROKE_MARKERS_FILL,
  PAINT_ORDER_MARKERS_FILL_STROKE,
  PAINT_ORDER_MARKERS_STROKE_FILL,
} PaintOrder;

typedef enum
{
  CLIP_NONE,
  CLIP_PATH,
  CLIP_REF,
} ClipKind;

typedef enum
{
  TRANSFORM_NONE,
  TRANSFORM_TRANSLATE,
  TRANSFORM_SCALE,
  TRANSFORM_ROTATE,
  TRANSFORM_SKEW_X,
  TRANSFORM_SKEW_Y,
  TRANSFORM_MATRIX,
} TransformType;

double       svg_shape_attr_get_number    (Shape                 *shape,
                                           ShapeAttr              attr,
                                           const graphene_rect_t *viewport);
GskPath *    svg_shape_attr_get_path      (Shape                 *shape,
                                           ShapeAttr              attr);
unsigned int svg_shape_attr_get_enum      (Shape                 *shape,
                                           ShapeAttr              attr);
PaintKind    svg_shape_attr_get_paint     (Shape                 *shape,
                                           ShapeAttr              attr,
                                           GtkSymbolicColor      *symbolic,
                                           GdkRGBA               *color);
double *     svg_shape_attr_get_points    (Shape                 *shape,
                                           ShapeAttr              attr,
                                           unsigned int          *n_params);
ClipKind     svg_shape_attr_get_clip      (Shape                 *shape,
                                           ShapeAttr              attr,
                                           GskPath              **path);
char *       svg_shape_attr_get_transform (Shape                 *shape,
                                           ShapeAttr              attr);
char *       svg_shape_attr_get_filter    (Shape                 *shape,
                                           ShapeAttr              attr);
GskPath *    svg_shape_get_path           (Shape                 *shape,
                                           const graphene_rect_t *viewport);
void         svg_shape_attr_set           (Shape                 *shape,
                                           ShapeAttr              attr,
                                           SvgValue              *value);

SvgValue *   svg_value_ref          (SvgValue         *value);
void         svg_value_unref        (SvgValue         *value);
GType        svg_value_get_type     (void) G_GNUC_CONST;
gboolean     svg_value_equal        (const SvgValue   *self,
                                     const SvgValue   *other);
char *       svg_value_to_string    (const SvgValue   *self);

SvgValue *   svg_number_new         (double            value);
SvgValue *   svg_linecap_new        (GskLineCap        value);
SvgValue *   svg_linejoin_new       (GskLineJoin       value);
SvgValue *   svg_fill_rule_new      (GskFillRule       rule);
SvgValue *   svg_paint_order_new    (PaintOrder        order);
SvgValue *   svg_paint_new_none     (void);
SvgValue *   svg_paint_new_symbolic (GtkSymbolicColor  symbolic);
SvgValue *   svg_paint_new_rgba     (const GdkRGBA    *rgba);
SvgValue *   svg_numbers_new        (double           *values,
                                     unsigned int      n_values);
SvgValue *   svg_path_new           (GskPath          *path);
SvgValue *   svg_clip_new_none      (void);
SvgValue *   svg_clip_new_path      (GskPath          *path);
SvgValue *   svg_transform_parse    (const char       *value);
unsigned int svg_transform_get_n_transforms (const SvgValue *value);
SvgValue *   svg_transform_get_transform    (const SvgValue *value,
                                             unsigned int    pos);
TransformType svg_transform_get_primitive (const SvgValue *value,
                                           unsigned int    pos,
                                           double          params[6]);
SvgValue *   svg_transform_new_none (void);
SvgValue *   svg_transform_new_translate (double x,
                                          double y);
SvgValue *   svg_transform_new_scale     (double x,
                                          double y);
SvgValue *   svg_transform_new_rotate    (double angle,
                                          double x,
                                          double y);
SvgValue *   svg_transform_new_skew_x    (double angle);
SvgValue *   svg_transform_new_skew_y    (double angle);
SvgValue *   svg_transform_new_matrix    (double params[6]);
SvgValue *   svg_filter_parse       (const char       *value);

Shape *      svg_shape_add          (Shape            *parent,
                                     ShapeType         type);
void         svg_shape_delete       (Shape            *shape);

/* --- */

GtkSvg *       gtk_svg_copy            (GtkSvg                *orig);

gboolean       gtk_svg_equal           (GtkSvg                *svg1,
                                        GtkSvg                *svg2);

void           gtk_svg_set_load_time   (GtkSvg                *self,
                                        int64_t                load_time);

void           gtk_svg_set_playing     (GtkSvg                *self,
                                        gboolean               playing);

void           gtk_svg_advance         (GtkSvg                *self,
                                        int64_t                current_time);

GtkSvgRunMode  gtk_svg_get_run_mode    (GtkSvg                *self);

int64_t        gtk_svg_get_next_update (GtkSvg                *self);

typedef enum
{
  GTK_SVG_SERIALIZE_DEFAULT            = 0,
  GTK_SVG_SERIALIZE_AT_CURRENT_TIME    = 1 << 0,
  GTK_SVG_SERIALIZE_EXCLUDE_ANIMATION  = 1 << 1,
  GTK_SVG_SERIALIZE_INCLUDE_STATE      = 1 << 2,
  GTK_SVG_SERIALIZE_EXPAND_GPA_ATTRS   = 1 << 3,
} GtkSvgSerializeFlags;

GBytes *       gtk_svg_serialize_full  (GtkSvg                *self,
                                        const GdkRGBA         *colors,
                                        size_t                 n_colors,
                                        GtkSvgSerializeFlags   flags);

G_END_DECLS
