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


#ifndef __GSK_PATHOP_PRIVATE_H__
#define __GSK_PATHOP_PRIVATE_H__

#include <gskpath.h>

G_BEGIN_DECLS

typedef gpointer gskpathop;

static inline
gskpathop               gsk_pathop_encode                       (GskPathOperation        op,
                                                                 const graphene_point_t *pts);
static inline
const graphene_point_t *gsk_pathop_points                       (gskpathop               pop);
static inline
GskPathOperation        gsk_pathop_op                           (gskpathop               pop);

static inline
gboolean                gsk_pathop_foreach                      (gskpathop               pop,
                                                                 GskPathForeachFunc      func,
                                                                 gpointer                user_data);

/* IMPLEMENTATION */

#define GSK_PATHOP_OPERATION_MASK (0x7)

static inline gskpathop
gsk_pathop_encode (GskPathOperation        op,
                   const graphene_point_t *pts)
{
  /* g_assert (op & GSK_PATHOP_OPERATION_MASK == op); */

  return GSIZE_TO_POINTER (GPOINTER_TO_SIZE (pts) | op);
}

static inline const graphene_point_t *
gsk_pathop_points (gskpathop pop)
{
  return GSIZE_TO_POINTER (GPOINTER_TO_SIZE (pop) & ~GSK_PATHOP_OPERATION_MASK);
}

static inline
GskPathOperation gsk_pathop_op (gskpathop pop)
{
  return GPOINTER_TO_SIZE (pop) & GSK_PATHOP_OPERATION_MASK;
}

static inline gboolean
gsk_pathop_foreach (gskpathop          pop,
                    GskPathForeachFunc func,
                    gpointer           user_data)
{
  switch (gsk_pathop_op (pop))
  {
    case GSK_PATH_MOVE:
      return func (gsk_pathop_op (pop), gsk_pathop_points (pop), 1, 0, user_data);

    case GSK_PATH_CLOSE:
    case GSK_PATH_LINE:
      return func (gsk_pathop_op (pop), gsk_pathop_points (pop), 2, 0, user_data);

    case GSK_PATH_CURVE:
      return func (gsk_pathop_op (pop), gsk_pathop_points (pop), 4, 0, user_data);

    case GSK_PATH_CONIC:
      {
        const graphene_point_t *pts = gsk_pathop_points (pop);
        return func (gsk_pathop_op (pop), (graphene_point_t[3]) { pts[0], pts[1], pts[3] }, 3, pts[2].x, user_data);
      }

    default:
      g_assert_not_reached ();
      return TRUE;
    }
}


G_END_DECLS

#endif /* __GSK_PATHOP_PRIVATE_H__ */

