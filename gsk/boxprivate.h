#pragma once

#include <graphene.h>
#include <math.h>
#include "gsktypesprivate.h"
#include "scaleprivate.h"
#include "pointprivate.h"

#ifndef USE_SIMD

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

#else  /* USE_SIMD */

struct _Box
{
  GRAPHENE_ALIGNED_DECL (graphene_simd4f_t v, 16);
};

static inline float
box_x0 (const Box box)
{
  return graphene_simd4f_get_x (box.v);
}

static inline float
box_y0 (const Box box)
{
  return graphene_simd4f_get_y (box.v);
}

static inline float
box_x1 (const Box box)
{
  return graphene_simd4f_get_z (box.v);
}

static inline float
box_y1 (const Box box)
{
  return graphene_simd4f_get_w (box.v);
}

static inline float
box_width (const Box box)
{
  return box_x1 (box) - box_x0 (box);
}

static inline float
box_height (const Box box)
{
  return box_y1 (box) - box_y0 (box);
}

static inline Box
box (float x0,
     float y0,
     float x1,
     float y1)
{
  return (Box) { .v = graphene_simd4f_init (x0, y0, x1, y1) };
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

/* { a[0], a[1], b[0], b[1] }  */
#  define graphene_simd4f_splat_xyxy(a,b) \
  (__extension__ ({ \
    (graphene_simd4f_t) _mm_shuffle_ps ((a), (b), _MM_SHUFFLE (1, 0, 1, 0)); \
  }))

static inline Box
box_from_points (Point p0,
                 Point p1)
{
  return (Box) { .v = graphene_simd4f_splat_xyxy (p0.v, p1.v) };
}

static inline Point
box_origin (const Box box)
{
  return (Point) { .v = graphene_simd4f_zero_zw (box.v) };
}

static inline Point
box_opposite (const Box box)
{
  return (Point) { .v = graphene_simd4f_zero_zw (graphene_simd4f_shuffle_zwxy (box.v)) };
}

static inline void
box_to_float (const Box box,
              float     v[4])
{
  graphene_simd4f_dup_4f (box.v, v);
  v[2] -= v[0];
  v[3] -= v[1];
}

static inline Box
box_inset (const Box box,
           float     dx,
           float     dy)
{
  return (Box) { .v = graphene_simd4f_add (box.v, graphene_simd4f_init (dx, dy, -dx, -dy)) };
}

/* return  a[0] < b[0] && a[1] < b[1] */
#ifndef graphene_simd4f_cmple_xy
#  define graphene_simd4f_cmple_xy(a,b) \
  (__extension__ ({ \
    __m128i __res = (__m128i) _mm_cmple_ps ((a), (b)); \
    (bool) ((_mm_movemask_epi8 (__res) & 0xff) == 0xff); \
  }))
#endif

static inline gboolean
box_intersect (const Box  box1,
               const Box  box2,
               Box       *box)
{
  graphene_simd4f_t s, t, t1;

  s = graphene_simd4f_max (box1.v, box2.v);
  t = graphene_simd4f_min (box1.v, box2.v);
  t1 = graphene_simd4f_shuffle_zwxy (t);

  if (graphene_simd4f_cmple_xy (s, t1))
    {
      if (box)
        box->v = graphene_simd4f_splat_xyxy (s, t);
      return TRUE;
    }

  return FALSE;
}

static inline gboolean
box_equal (const Box box1,
           const Box box2)
{
  return (gboolean) !!graphene_simd4f_cmp_eq (box1.v, box2.v);
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
  /* FIXME simd */
  return box_x0 (box) == box_x1 (box) || box_y0 (box) == box_y1 (box);
}

/* a splat variation */
#ifndef graphene_simd4f_shuffle_xyxy
#  define graphene_simd4f_shuffle_xyxy(v) \
  (__extension__ ({ \
    (graphene_simd4f_t) _mm_shuffle_ps ((v), (v), _MM_SHUFFLE (1, 0, 1, 0)); \
  }))
#endif

static inline Box
box_add (const Box   box,
         const Point offset)
{
  return (Box) { .v = graphene_simd4f_add (box.v, graphene_simd4f_shuffle_xyxy (offset.v)) };
}

static inline Box
box_sub (const Box   box,
         const Point offset)
{
  return (Box) { .v = graphene_simd4f_sub (box.v, graphene_simd4f_shuffle_xyxy (offset.v)) };
}

static inline Box
box_mul (const Box   box,
         const Scale scale)
{
  Box b =  (Box) { .v = graphene_simd4f_mul (box.v, graphene_simd4f_shuffle_xyxy (scale.v)) };

  if (G_UNLIKELY (!graphene_simd4f_cmple_xy (graphene_simd4f_init (0, 0, 0, 0), scale.v)))
    {
      graphene_simd4f_t v = graphene_simd4f_shuffle_zwxy (b.v);
      graphene_simd4f_t s = graphene_simd4f_min (b.v, v);
      graphene_simd4f_t t = graphene_simd4f_max (b.v, v);

      return (Box) { .v = graphene_simd4f_splat_xyxy (s, t) };
    }

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

#ifdef __SSE4_1__

static inline Box
box_round_larger (const Box box)
{
  return { (Box) .v = graphene_simd4f_splat_xyxy (graphene_simd4f_floor (b.v), graphene_simd4f_ceil (b.v)) };
}

#else

static inline Box
box_round_larger (const Box b)
{
  return box (floorf (box_x0 (b)),
              floorf (box_y0 (b)),
              ceilf (box_x1 (b)),
              ceilf (box_y1 (b)));
}

#endif

static inline Box
box_round_to_pixels (const Box   box,
                     const Scale scale,
                     const Point offset)
{
  return box_sub (box_div (box_round_larger (box_mul (box_add (box, offset), scale)), scale), offset);
}

#endif
