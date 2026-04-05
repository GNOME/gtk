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

#include "gtksvgelementprivate.h"

G_BEGIN_DECLS

typedef struct {
  GBytes *content;
  GtkSvgLocation location;
} StyleElt;

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
      SvgElement *shape;
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

typedef struct
{
  SvgProperty attr;
  SvgValue *value;
} PropertyValue;

void property_value_clear (PropertyValue *pv);

struct _SvgElement
{
  SvgElementType type;
  SvgElement *parent;
  GtkBitmask *attrs;
  char *id;
  char *style;
  char **classes;
  size_t line;
  GtkSvgLocation style_loc;

  /* For style matching */
  GtkCssNode *css_node;

  /* Dependency order for computing updates */
  SvgElement *first;
  SvgElement *next;

  gboolean computed_for_use;
  gboolean valid_bounds;

  GArray *specified;
  SvgValue *base[N_SVG_PROPERTIES];
  SvgValue *current[N_SVG_PROPERTIES];

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
      SvgElement *shape;
      double pos;
    } attach;
  } gpa;
};

G_END_DECLS
