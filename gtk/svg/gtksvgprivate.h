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
#include "gtkenums.h"
#include "gtkbitmaskprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtk/svg/gtksvgtypesprivate.h"
#include "gtk/svg/gtksvgenumsprivate.h"

G_BEGIN_DECLS

#define INDEFINITE G_MAXINT64
#define REPEAT_FOREVER INFINITY
#define DEFAULT_FONT_SIZE 13.333

struct _GtkSvg
{
  GObject parent_instance;
  Shape *content;

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
  SHAPE_FILTER,
  SHAPE_SYMBOL,
  SHAPE_SWITCH,
  SHAPE_LINK,
} ShapeType;

typedef enum
{
  SHAPE_ATTR_DISPLAY,
  FIRST_SHAPE_ATTR = SHAPE_ATTR_DISPLAY,
  SHAPE_ATTR_FONT_SIZE, /* must come before lengths */
  SHAPE_ATTR_VISIBILITY,
  SHAPE_ATTR_TRANSFORM,
  SHAPE_ATTR_TRANSFORM_ORIGIN,
  SHAPE_ATTR_TRANSFORM_BOX,
  SHAPE_ATTR_OPACITY,
  SHAPE_ATTR_COLOR,
  SHAPE_ATTR_COLOR_INTERPOLATION,
  SHAPE_ATTR_COLOR_INTERPOLATION_FILTERS,
  SHAPE_ATTR_CLIP_PATH,
  SHAPE_ATTR_CLIP_RULE,
  SHAPE_ATTR_MASK,
  SHAPE_ATTR_FONT_FAMILY,
  SHAPE_ATTR_FONT_STYLE,
  SHAPE_ATTR_FONT_VARIANT,
  SHAPE_ATTR_FONT_WEIGHT,
  SHAPE_ATTR_FONT_STRETCH,
  SHAPE_ATTR_FILTER,
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
  SHAPE_ATTR_MARKER_START,
  SHAPE_ATTR_MARKER_MID,
  SHAPE_ATTR_MARKER_END,
  SHAPE_ATTR_PAINT_ORDER,
  SHAPE_ATTR_SHAPE_RENDERING,
  SHAPE_ATTR_TEXT_RENDERING,
  SHAPE_ATTR_IMAGE_RENDERING,
  SHAPE_ATTR_BLEND_MODE,
  SHAPE_ATTR_ISOLATION,
  SHAPE_ATTR_PATH_LENGTH,
  SHAPE_ATTR_HREF,
  SHAPE_ATTR_OVERFLOW,
  SHAPE_ATTR_VECTOR_EFFECT,
  SHAPE_ATTR_CONTENT_UNITS,
  SHAPE_ATTR_BOUND_UNITS,
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
  SHAPE_ATTR_FX,
  SHAPE_ATTR_FY,
  SHAPE_ATTR_FR,
  SHAPE_ATTR_MASK_TYPE,
  SHAPE_ATTR_VIEW_BOX,
  SHAPE_ATTR_CONTENT_FIT,
  SHAPE_ATTR_REF_X,
  SHAPE_ATTR_REF_Y,
  SHAPE_ATTR_MARKER_UNITS,
  SHAPE_ATTR_MARKER_ORIENT,
  SHAPE_ATTR_LANG,
  SHAPE_ATTR_TEXT_ANCHOR,
  SHAPE_ATTR_DX,
  SHAPE_ATTR_DY,
  SHAPE_ATTR_UNICODE_BIDI,
  SHAPE_ATTR_DIRECTION,
  SHAPE_ATTR_WRITING_MODE,
  SHAPE_ATTR_LETTER_SPACING,
  SHAPE_ATTR_TEXT_DECORATION,
  SHAPE_ATTR_REQUIRED_EXTENSIONS,
  SHAPE_ATTR_SYSTEM_LANGUAGE,
  SHAPE_ATTR_STROKE_MINWIDTH,
  SHAPE_ATTR_STROKE_MAXWIDTH,
  LAST_SHAPE_ATTR = SHAPE_ATTR_STROKE_MAXWIDTH,
  SHAPE_ATTR_STOP_OFFSET,
  FIRST_STOP_ATTR = SHAPE_ATTR_STOP_OFFSET,
  SHAPE_ATTR_STOP_COLOR,
  SHAPE_ATTR_STOP_OPACITY,
  LAST_STOP_ATTR = SHAPE_ATTR_STOP_OPACITY,
  SHAPE_ATTR_FE_X,
  FIRST_FILTER_ATTR = SHAPE_ATTR_FE_X,
  SHAPE_ATTR_FE_Y,
  SHAPE_ATTR_FE_WIDTH,
  SHAPE_ATTR_FE_HEIGHT,
  SHAPE_ATTR_FE_RESULT,
  SHAPE_ATTR_FE_COLOR,
  SHAPE_ATTR_FE_OPACITY,
  SHAPE_ATTR_FE_IN,
  SHAPE_ATTR_FE_IN2,
  SHAPE_ATTR_FE_STD_DEV,
  SHAPE_ATTR_FE_DX,
  SHAPE_ATTR_FE_DY,
  SHAPE_ATTR_FE_BLUR_EDGE_MODE,
  SHAPE_ATTR_FE_BLEND_MODE,
  SHAPE_ATTR_FE_BLEND_COMPOSITE,
  SHAPE_ATTR_FE_COLOR_MATRIX_TYPE,
  SHAPE_ATTR_FE_COLOR_MATRIX_VALUES,
  SHAPE_ATTR_FE_COMPOSITE_OPERATOR,
  SHAPE_ATTR_FE_COMPOSITE_K1,
  SHAPE_ATTR_FE_COMPOSITE_K2,
  SHAPE_ATTR_FE_COMPOSITE_K3,
  SHAPE_ATTR_FE_COMPOSITE_K4,
  SHAPE_ATTR_FE_DISPLACEMENT_SCALE,
  SHAPE_ATTR_FE_DISPLACEMENT_X,
  SHAPE_ATTR_FE_DISPLACEMENT_Y,
  SHAPE_ATTR_FE_IMAGE_HREF,
  SHAPE_ATTR_FE_IMAGE_CONTENT_FIT,
  SHAPE_ATTR_FE_FUNC_TYPE,
  SHAPE_ATTR_FE_FUNC_VALUES,
  SHAPE_ATTR_FE_FUNC_SLOPE,
  SHAPE_ATTR_FE_FUNC_INTERCEPT,
  SHAPE_ATTR_FE_FUNC_AMPLITUDE,
  SHAPE_ATTR_FE_FUNC_EXPONENT,
  SHAPE_ATTR_FE_FUNC_OFFSET,
  LAST_FILTER_ATTR = SHAPE_ATTR_FE_FUNC_OFFSET,
} ShapeAttr;

#define N_SHAPE_ATTRS (LAST_SHAPE_ATTR + 1 - FIRST_SHAPE_ATTR)
#define N_STOP_ATTRS (LAST_STOP_ATTR + 1 - FIRST_STOP_ATTR)
#define N_FILTER_ATTRS (LAST_FILTER_ATTR + 1 - FIRST_FILTER_ATTR)

#define N_ATTRS (LAST_FILTER_ATTR + 1 - FIRST_SHAPE_ATTR)

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
  Shape *parent;
  GtkBitmask *attrs;
  char *id;
  char *style;
  char **classes;
  size_t line;
  GtkSvgLocation style_loc;

  /* For style matching */
  GtkCssNode *css_node;

  /* Dependency order for computing updates */
  Shape *first;
  Shape *next;

  gboolean computed_for_use;
  gboolean valid_bounds;

  SvgValue *base[N_ATTRS];
  SvgValue *current[N_ATTRS];

  GPtrArray *shapes;
  GPtrArray *animations;
  GPtrArray *color_stops;
  GPtrArray *filters;
  GPtrArray *deps;
  GPtrArray *styles;

  GskPath *path;
  GskPathMeasure *measure;
  graphene_rect_t bounds;
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
    SvgValue *fill;
    SvgValue *stroke;
    SvgValue *width;
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
                                           GskPath              **path,
                                           const char           **ref);
const char * svg_shape_attr_get_mask      (Shape                 *shape,
                                           ShapeAttr              attr);
char *       svg_shape_attr_get_transform (Shape                 *shape,
                                           ShapeAttr              attr);
char *       svg_shape_attr_get_filter    (Shape                 *shape,
                                           ShapeAttr              attr);
GskPath *    svg_shape_get_path           (Shape                 *shape,
                                           const graphene_rect_t *viewport);
gboolean     svg_shape_attr_get_viewbox   (Shape                 *shape,
                                           ShapeAttr              attr,
                                           graphene_rect_t       *rect);

void         svg_shape_attr_set           (Shape                 *shape,
                                           ShapeAttr              attr,
                                           SvgValue              *value);

SvgValue *   svg_view_box_new       (const graphene_rect_t *box);
SvgValue *   svg_path_new           (GskPath *path);
SvgValue *   svg_clip_new_none      (void);
SvgValue *   svg_clip_new_path      (const char *string,
                                     unsigned int fill_rule);
SvgValue *   svg_clip_new_ref       (const char *string);
SvgValue *   svg_mask_new_none      (void);
SvgValue *   svg_mask_new_ref       (const char *string);
SvgValue *   svg_filter_parse       (const char       *value);

Shape *      svg_shape_add          (Shape            *parent,
                                     ShapeType         type);
void         svg_shape_delete       (Shape            *shape);

const char * svg_shape_get_name     (ShapeType  type);
gboolean     svg_shape_has_attr     (Shape     *shape,
                                     ShapeAttr  attr);

gboolean     gtk_svg_set_state_names (GtkSvg      *svg,
                                      const char **names);

typedef void (* ShapeCallback) (Shape    *shape,
                                gpointer  user_data);

void         svg_foreach_shape (Shape         *shape,
                                ShapeCallback  callback,
                                gpointer       user_data);

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
