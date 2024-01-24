#pragma once

#include <graphene.h>
#include <math.h>
#include "gsktypesprivate.h"
#include "scaleprivate.h"
#include "pointprivate.h"

struct _Box
{
  float x0, y0, x1, y1;
};

static inline float
box_x0 (const Box box)
{
  return box.x0;
}

static inline float
box_y0 (const Box box)
{
  return box.y0;
}

static inline float
box_x1 (const Box box)
{
  return box.x1;
}

static inline float
box_y1 (const Box box)
{
  return box.y1;
}

static inline float
box_width (const Box box)
{
  return box.x1 - box.x0;
}

static inline float
box_height (const Box box)
{
  return box.y1 - box.y0;
}

/* Assumes x0 <= x1 && y0 <= y1 */
static inline Box
box (float x0,
     float y0,
     float x1,
     float y1)
{
  return (Box) { .x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1 };
}

static inline Box
box_from_rect (float x,
               float y,
               float w,
               float h)
{
  return box (x, y, x + w, y + h);
}

static inline Box
box_from_graphene (const graphene_rect_t *rect)
{
  return box_from_rect (rect->origin.x,
                        rect->origin.y,
                        rect->size.width,
                        rect->size.height);
}

/* Assumes p0.x <= p1.x && p0.y <= p1.y */
static inline Box
box_from_points (Point p0,
                 Point p1)
{
  return box (p0.x, p0.y, p1.x, p1.y);
}

static inline Point
box_origin (const Box box)
{
  return point (box.x0, box.y0);
}

static inline Point
box_opposite (const Box box)
{
  return point (box.x1, box.y1);
}

static inline void
box_to_float (const Box box,
              float     v[4])
{
  v[0] = box.x0;
  v[1] = box.y0;
  v[2] = box.x1 - box.x0;
  v[3] = box.y1 - box.y0;
}

static inline Box
box_inset (const Box box,
           float     dx,
           float     dy)
{
  return (Box) { .x0 = box.x0 + dx, .y0 = box.y0 + dy,
                 .x1 = box.x1 - dx, .y1 = box.y1 - dy };
}

static inline gboolean
box_intersect (const Box  box1,
               const Box  box2,
               Box       *box)
{
  Box b;

  b.x0 = MAX (box1.x0, box2.x0);
  b.y0 = MAX (box1.y0, box2.y0);
  b.x1 = MIN (box1.x1, box2.x1);
  b.y1 = MIN (box1.y1, box2.y1);

  if (b.x0 <= b.x1 && b.y0 <= b.x1)
    {
      if (box)
        *box = b;

      return TRUE;
    }

  return FALSE;
}

static inline gboolean
box_equal (const Box box1,
           const Box box2)
{
  return memcmp (&box1, &box2, sizeof (Box)) == 0;
}

static inline gboolean
box_contains (const Box box1,
              const Box box2)
{
  Box box;

  if (box_intersect (box1, box2, &box))
    return box_equal (box, box2);

  return FALSE;
}

static inline gboolean
box_empty (const Box box)
{
  return box.x0 == box.x1 || box.y0 == box.y1;
}

static inline Box
box_add (const Box   box,
         const Point offset)
{
  return (Box) { .x0 = box.x0 + offset.x, .y0 = box.y0 + offset.y,
                 .x1 = box.x1 + offset.x, .y1 = box.y1 + offset.y };
}

static inline Box
box_sub (const Box   box,
         const Point offset)
{
  return (Box) { .x0 = box.x0 - offset.x, .y0 = box.y0 - offset.y,
                 .x1 = box.x1 - offset.x, .y1 = box.y1 - offset.y };
}

static inline Box
box_mul (const Box   box,
         const Scale scale)
{
  Box b = (Box) { .x0 = box.x0 * scale.x, .y0 = box.y0 * scale.y,
                  .x1 = box.x1 * scale.x, .y1 = box.y1 * scale.y };

  if (G_UNLIKELY (scale.x < 0 || scale.y < 0))
    return (Box) { .x0 = MIN (b.x0, b.x1), .y0 = MIN (b.y0, b.y1),
                   .x1 = MAX (b.x0, b.x1), .y1 = MAX (b.y0, b.y1) };

  return b;
}

static inline Box
box_div (const Box   box,
         const Scale scale)
{
  return box_mul (box, scale_inv (scale));
}

static inline void
box_offset_to_float (const Box   box,
                     const Point offset,
                     float       v[4])
{
  box_to_float (box_add (box, offset), v);
}

static inline Box
box_round_larger (const Box box)
{
  return (Box) { .x0 = floorf (box.x0), .y0 = floorf (box.y0),
                 .x1 = ceilf (box.x1), .y1 = ceilf (box.y1) };
}

static inline Box
box_round_to_pixels (const Box   box,
                     const Scale scale,
                     const Point offset)
{
  return box_sub (box_div (box_round_larger (box_mul (box_add (box, offset), scale)), scale), offset);
}
