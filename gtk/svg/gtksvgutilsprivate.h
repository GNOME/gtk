#pragma once

#include <tgmath.h>
#include <graphene.h>
#include "gdk/gdkcolorprivate.h"
#include "gtk/svg/gtksvgtypesprivate.h"
#include "gtk/svg/gtksvgenumsprivate.h"

#define DEFAULT_FONT_SIZE 13.333

static inline void
_sincos (double x, double *_sin, double *_cos)
{
#ifdef HAVE_SINCOS
  sincos (x, _sin, _cos);
#else
  *_sin = sin (x);
  *_cos = cos (x);
#endif
}

static inline unsigned int
gcd (unsigned int a,
     unsigned int b)
{
  if (b == 0)
    return a;

  return gcd (b, a % b);
}

static inline unsigned int
lcm (unsigned int a,
     unsigned int b)
{
  return (a * b) / gcd (a, b);
}

static inline double
normalized_diagonal (const graphene_rect_t *viewport)
{
  return hypot (viewport->size.width, viewport->size.height) / M_SQRT2;
}

static inline double
lerp (double t, double a, double b)
{
  return a + (b - a) * t;
}

static inline gboolean
lerp_bool (double t, gboolean b0, gboolean b1)
{
  return (t * b0 + (1 - t) * b1) != 0;
}

static inline void
lerp_point (double                  t,
            const graphene_point_t *p0,
            const graphene_point_t *p1,
            graphene_point_t       *p)
{
  p->x = lerp (t, p0->x, p1->x);
  p->y = lerp (t, p0->y, p1->y);
}

static inline void
lerp_color (double          t,
            const GdkColor *c0,
            const GdkColor *c1,
            GdkColorState  *interpolation,
            GdkColor       *c)
{
  GdkColor cc0, cc1;
  float values[4];

  gdk_color_convert (&cc0, interpolation, c0);
  gdk_color_convert (&cc1, interpolation, c1);

  for (unsigned int i = 0; i < 4; i++)
    values[i] = lerp (t, cc0.values[i], cc1.values[i]);

  gdk_color_finish (&cc0);
  gdk_color_finish (&cc1);

  gdk_color_init (c, interpolation, values);
}

static inline double
accumulate (double a, double b, int n)
{
  return a + b * n;
}

static inline void
accumulate_color (const GdkColor *c0,
                  const GdkColor *c1,
                  int             n,
                  GdkColorState  *interpolation,
                  GdkColor       *c)
{
  GdkColor cc0, cc1;
  float values[4];

  gdk_color_convert (&cc0, interpolation, c0);
  gdk_color_convert (&cc1, interpolation, c1);

  for (unsigned int i = 0; i < 4; i++)
    values[0] = accumulate (cc0.values[i], cc1.values[i], n);

  gdk_color_finish (&cc0);
  gdk_color_finish (&cc1);

  gdk_color_init (c, interpolation, values);
}

static inline double
color_distance (const GdkColor *c0,
                const GdkColor *c1)
{
  g_assert (gdk_color_state_equal (c0->color_state, c1->color_state));

  return sqrt ((c0->red - c1->red)     * (c0->red - c1->red) +
               (c0->green - c1->green) * (c0->green - c1->green) +
               (c0->blue - c1->blue)   * (c0->blue - c1->blue) +
               (c0->alpha - c1->alpha) * (c0->alpha - c1->alpha));
}

char ** strsplit_set (const char *str,
                      const char *sep);

GArray * parse_numbers2 (const char *value,
                         const char *sep,
                         double      min,
                         double      max);

gboolean parse_numbers (const char   *value,
                        const char   *sep,
                        double        min,
                        double        max,
                        double       *values,
                        unsigned int  length,
                        unsigned int *n_values);

gboolean parse_number (const char *value,
                       double      min,
                       double      max,
                       double     *f);

gboolean match_str_len (const char *value,
                        const char *str,
                        size_t      len);

#define match_str(value, str) match_str_len (value, str, strlen (str))

gboolean parse_enum (const char    *value,
                     const char   **values,
                     size_t         n_values,
                     unsigned int  *result);
