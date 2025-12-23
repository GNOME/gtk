/*
 * Copyright © 2020 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */


#pragma once

#include "gskpath.h"

#include "gskpathopprivate.h"

G_BEGIN_DECLS

typedef enum
{
  /* path has only lines */
  GSK_PATH_FLAT,
  /* all points of the path are identical */
  GSK_PATH_ZERO_LENGTH,
  /* all contours are closed */
  GSK_PATH_CLOSED
} GskPathFlags;

typedef struct _GskContour GskContour;
typedef struct _GskRealPathPoint GskRealPathPoint;

/* Same as Skia, so looks like a good value. ¯\_(ツ)_/¯ */
#define GSK_PATH_TOLERANCE_DEFAULT (0.5)

GskPath *               gsk_path_new_from_contours              (const GSList           *contours);

gsize                   gsk_path_get_n_contours                 (const GskPath          *self);
const GskContour *      gsk_path_get_contour                    (const GskPath          *self,
                                                                 gsize                   i);

GskPathFlags            gsk_path_get_flags                      (const GskPath          *self);

gboolean                gsk_path_foreach_with_tolerance         (GskPath                *self,
                                                                 GskPathForeachFlags     flags,
                                                                 double                  tolerance,
                                                                 GskPathForeachFunc      func,
                                                                 gpointer                user_data);


void                    gsk_path_builder_add_contour            (GskPathBuilder         *builder,
                                                                 GskContour             *contour);


/* implemented in gskstrokenode.c */
void                    gsk_cairo_stroke_path                   (cairo_t                *cr,
                                                                 GskPath                *path,
                                                                 GskStroke              *stroke);

static inline void
gsk_cairo_set_fill_rule (cairo_t     *cr,
                         GskFillRule  fill_rule)
{
  switch (fill_rule)
    {
      case GSK_FILL_RULE_WINDING:
        cairo_set_fill_rule (cr, CAIRO_FILL_RULE_WINDING);
        break;
      case GSK_FILL_RULE_EVEN_ODD:
        cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
        break;
      default:
        g_assert_not_reached ();
        break;
    }
}

/* Callbacks for gsk_path_parse_full
 * add_rect, add_circle and add_rounded_rect
 * are optional - the parser will just emit
 * equivalent ops if they are not provided
 */
typedef struct
{
  gboolean (* add_op)        (GskPathOperation        op,
                              const graphene_point_t *pts,
                              gsize                   n_pts,
                              float                   weight,
                              gpointer                user_data);
  gboolean (* add_arc)       (float                   rx,
                              float                   ry,
                              float                   x_axis_rotation,
                              gboolean                large_arc,
                              gboolean                positive_sweep,
                              float                   x,
                              float                   y,
                              gpointer                user_data);
  gboolean (* add_rect)      (const graphene_rect_t  *rect,
                              gpointer                user_data);
  gboolean (* add_circle)      (const graphene_point_t *center,
                                float                    radius,
                                gpointer                 user_data);
  gboolean (* add_rounded_rect) (const GskRoundedRect    *rect,
                                 gpointer                 user_data);
} GskPathParser;

gboolean gsk_path_parse_full (const char    *string,
                              GskPathParser *callbacks,
                              gpointer       data);

G_END_DECLS
