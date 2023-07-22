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

#include "config.h"

#include "gskcontourprivate.h"
#include "gskcurveprivate.h"
#include "gskpathprivate.h"
#include "gskstrokeprivate.h"

typedef struct
{
  float offset;                 /* how much of the current dash we've spent */
  gsize dash_index;             /* goes from 0 to n_dash * 2, so we don't have to care about on/off
                                 * for uneven dashes
                                 */
  gboolean on;                  /* If we're currently dashing or not */
  gboolean may_close;           /* TRUE if we haven't turned the dash off in this contour */
  gboolean needs_move_to;       /* If we have emitted the initial move_to() yet */
  enum {
    NORMAL, /* no special behavior required */
    SKIP,   /* skip the next dash */
    ONLY,   /* only do the first dash */
    DONE    /* done with the first dash */
  } first_dash_behavior;        /* How to handle the first dash in the loop. We loop closed contours
                                 * twice to make sure the first dash and the last dash can get joined
                                 */
  float collect_start;          /* We're collecting multiple line segments when decomposing. */

  /* from the stroke */
  float *dash;
  gsize n_dash;
  float dash_length;
  float dash_offset;

  GskPathForeachFunc func;
  gpointer user_data;
} GskPathDash;

static void
gsk_path_dash_setup (GskPathDash *self)
{
  self->offset = fmodf (self->dash_offset, 2 * self->dash_length);

  self->dash_index = 0;
  self->on = TRUE;
  self->may_close = TRUE;
  while (self->offset > self->dash[self->dash_index % self->n_dash])
    {
      self->offset -= self->dash[self->dash_index % self->n_dash];
      self->dash_index++;
      self->on = !self->on;
    }
  if (self->first_dash_behavior != ONLY)
    self->needs_move_to = TRUE;
}

static gboolean
gsk_path_dash_ensure_move_to (GskPathDash            *self,
                              const graphene_point_t *pt)
{
  if (!self->needs_move_to)
    return TRUE;

  if (!self->func (GSK_PATH_MOVE, pt, 1, 0.f, self->user_data))
    return FALSE;

  self->needs_move_to = FALSE;
  return TRUE;
}

static gboolean
gsk_path_dash_add_curve (const GskCurve *curve,
                         GskPathDash    *self)
{
  float remaining, t_start, t_end;
  float used;

  remaining = gsk_curve_get_length (curve);
  used = 0;
  t_start = 0;

  while (remaining > 0)
    {
      float piece;

      if (self->offset + remaining <= self->dash[self->dash_index % self->n_dash])
        piece = remaining;
      else
        piece = self->dash[self->dash_index % self->n_dash] - self->offset;

      t_end = gsk_curve_at_length (curve, used + piece, 0.001);

      if (self->on)
        {
          if (self->first_dash_behavior != SKIP)
            {
              GskCurve segment;

              if (piece > 0)
                {
                  gsk_curve_segment (curve, t_start, t_end, &segment);
                  if (!gsk_path_dash_ensure_move_to (self, gsk_curve_get_start_point (&segment)))
                    return FALSE;

                  if (!gsk_pathop_foreach (gsk_curve_pathop (&segment), self->func, self->user_data))
                    return FALSE;
                }
              else
                {
                  graphene_point_t p;

                  gsk_curve_get_point (curve, t_start, &p);
                  if (!gsk_path_dash_ensure_move_to (self, &p))
                    return FALSE;
                }
            }
        }
      else
        {
          self->may_close = FALSE;
          if (self->first_dash_behavior == ONLY)
            {
              self->first_dash_behavior = DONE;
              return FALSE;
            }
          self->first_dash_behavior = NORMAL;
        }

      if (self->offset + remaining <= self->dash[self->dash_index % self->n_dash])
        {
          self->offset += remaining;
          remaining = 0;
        }
      else
        {
          t_start = t_end;
          remaining -= piece;
          used += piece;
          self->offset = 0;
          self->dash_index++;
          self->dash_index %= 2 * self->n_dash;
          self->on = !self->on;
          self->needs_move_to = TRUE;
        }
    }

  return TRUE;
}

static gboolean
gsk_path_dash_foreach (GskPathOperation        op,
                       const graphene_point_t *pts,
                       gsize                   n_pts,
                       float                   weight,
                       gpointer                user_data)
{
  GskPathDash *self = user_data;
  GskCurve curve;

  switch (op)
  {
    case GSK_PATH_MOVE:
      gsk_path_dash_setup (self);
      break;

    case GSK_PATH_CLOSE:
      if (self->may_close)
        {
          if (graphene_point_equal (&pts[0], &pts[1]))
            return self->func (GSK_PATH_CLOSE, pts, 2, weight, self->user_data);
        }
      else
        op = GSK_PATH_LINE;
      G_GNUC_FALLTHROUGH;

    case GSK_PATH_LINE:
    case GSK_PATH_QUAD:
    case GSK_PATH_CUBIC:
    case GSK_PATH_CONIC:
      gsk_curve_init_foreach (&curve, op, pts, n_pts, weight);
      if (!gsk_path_dash_add_curve (&curve, self))
        return FALSE;
      break;

    default:
      g_assert_not_reached ();
      break;
  }

  return TRUE;
}

gboolean
gsk_contour_dash (const GskContour   *contour,
                  GskStroke          *stroke,
                  GskPathForeachFunc  func,
                  gpointer            user_data)
{
  GskPathDash self = {
    .offset = 0,
    .dash = stroke->dash,
    .n_dash = stroke->n_dash,
    .dash_length = stroke->dash_length,
    .dash_offset = stroke->dash_offset,
    .func = func,
    .user_data = user_data
  };
  gboolean is_closed = gsk_contour_get_flags (contour) & GSK_PATH_CLOSED ? TRUE : FALSE;

  self.first_dash_behavior = is_closed ? SKIP : NORMAL;
  if (!gsk_contour_foreach (contour, gsk_path_dash_foreach, &self))
    return FALSE;

  if (is_closed)
    {
      if (self.first_dash_behavior == NORMAL)
        self.first_dash_behavior = ONLY;
      else
        self.first_dash_behavior = NORMAL;
      self.needs_move_to = !self.on;
      if (!gsk_contour_foreach (contour, gsk_path_dash_foreach, &self) &&
          self.first_dash_behavior != DONE)
        return FALSE;
    }

  return TRUE;
}

/**
 * gsk_path_dash:
 * @self: the `GskPath` to dash
 * @stroke: the stroke containing the dash parameters
 * @func: (scope call) (closure user_data): the function to call for operations
 * @user_data: (nullable): user data passed to @func
 *
 * Calls @func for every operation of the path that is the result
 * of dashing @self with the dash pattern from @stroke.
 *
 * Returns: `FALSE` if @func returned FALSE`, `TRUE` otherwise.
 *
 * Since: 4.14
 */
gboolean
gsk_path_dash (GskPath            *self,
               GskStroke          *stroke,
               GskPathForeachFunc  func,
               gpointer            user_data)
{
  gsize i;

  /* Dashing disabled, no need to do any work */
  if (stroke->dash_length <= 0)
    return gsk_path_foreach (self, -1, func, user_data);

  for (i = 0; i < gsk_path_get_n_contours (self); i++)
    {
      if (!gsk_contour_dash (gsk_path_get_contour (self, i), stroke, func, user_data))
        return FALSE;
    }

  return TRUE;
}

