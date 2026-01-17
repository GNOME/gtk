#pragma once

#include "gskrendernode.h"
#include <cairo.h>

#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/gdkcolorprivate.h"

G_BEGIN_DECLS

/*< private >
 * GskGradientStop:
 * @offset: the offset of the color stop, as a value between 0 and 1
 * @transition_hint: where to place the midpoint between the previous stop
 *   and this one, as a value between 0 and 1. If this is != 0.5, the
 *   interpolation is non-linear
 * @color: the color at the given offset
 *
 * A color stop in a gradient node.
 */
typedef struct _GskGradientStop GskGradientStop;
struct _GskGradientStop
{
  float offset;
  float transition_hint;
  GdkColor color;
};

typedef enum
{
  GSK_HUE_INTERPOLATION_SHORTER,
  GSK_HUE_INTERPOLATION_LONGER,
  GSK_HUE_INTERPOLATION_INCREASING,
  GSK_HUE_INTERPOLATION_DECREASING,
} GskHueInterpolation;

typedef enum
{
  GSK_REPEAT_NONE,
  GSK_REPEAT_PAD,
  GSK_REPEAT_REPEAT,
  GSK_REPEAT_REFLECT,
} GskRepeat;

typedef struct _GskGradient GskGradient;

static inline void
clear_stop (GskGradientStop *s)
{
  gdk_color_finish (&s->color);
}

#define GDK_ARRAY_NAME gradient_stops
#define GDK_ARRAY_TYPE_NAME GradientStops
#define GDK_ARRAY_ELEMENT_TYPE GskGradientStop
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 4
#define GDK_ARRAY_NO_MEMSET 1
#define GDK_ARRAY_FREE_FUNC clear_stop
#include "gdk/gdkarrayimpl.c"

struct _GskGradient
{
  GradientStops stops;
  GdkColorState *interpolation;
  GskHueInterpolation hue_interpolation;
  gboolean premultiplied;
  GskRepeat repeat;
  gboolean opaque;
  GskColorStop *rgba_stops;
};

GskGradient *   gsk_gradient_new                        (void);

GskGradient *   gsk_gradient_copy                       (const GskGradient       *gradient);
void            gsk_gradient_free                       (GskGradient             *gradient);

void            gsk_gradient_clear                      (GskGradient             *gradient);
GskGradient *   gsk_gradient_init_copy                  (GskGradient             *gradient,
                                                         const GskGradient       *orig);

gboolean        gsk_gradient_equal                      (const GskGradient       *gradient0,
                                                         const GskGradient       *gradient1);

void            gsk_gradient_add_stop                   (GskGradient             *gradient,
                                                         float                    offset,
                                                         float                    transition_hint,
                                                         const GdkColor          *color);
void            gsk_gradient_add_color_stops            (GskGradient             *gradient,
                                                         const GskColorStop      *stops,
                                                         gsize                    n_stops);
void            gsk_gradient_set_repeat                 (GskGradient             *gradient,
                                                         GskRepeat                start);
void            gsk_gradient_set_interpolation          (GskGradient             *gradient,
                                                         GdkColorState           *interpolation);
void            gsk_gradient_set_hue_interpolation      (GskGradient             *gradient,
                                                         GskHueInterpolation      hue_interpolation);
void            gsk_gradient_set_premultiplied          (GskGradient             *gradient,
                                                         gboolean                 premultiplied);

gsize           gsk_gradient_get_n_stops                (const GskGradient       *gradient);
const GskGradientStop *
                gsk_gradient_get_stops                  (const GskGradient       *gradient);
const GdkColor *gsk_gradient_get_stop_color             (const GskGradient       *gradient,
                                                         gsize                    idx);
float           gsk_gradient_get_stop_offset            (const GskGradient       *gradient,
                                                         gsize                    idx);
float           gsk_gradient_get_stop_transition_hint   (const GskGradient       *gradient,
                                                         gsize                    idx);

GdkColorState * gsk_gradient_get_interpolation          (const GskGradient       *gradient);
GskHueInterpolation
                gsk_gradient_get_hue_interpolation      (const GskGradient       *gradient);
gboolean        gsk_gradient_get_premultiplied          (const GskGradient       *gradient);
GskRepeat       gsk_gradient_get_repeat                 (const GskGradient       *gradient);
gboolean        gsk_gradient_is_opaque                  (const GskGradient       *gradient);
const GdkColor *gsk_gradient_check_single_color         (const GskGradient       *gradient);
void            gsk_gradient_get_average_color          (const GskGradient       *self,
                                                         GdkColor                *out_color);
const GskColorStop *
                gsk_gradient_get_color_stops            (GskGradient             *gradient,
                                                         gsize                   *n_stops);

float           gsk_hue_interpolation_fixup             (GskHueInterpolation      interp,
                                                         float                    h1,
                                                         float                    h2);
cairo_extend_t  gsk_repeat_to_cairo (GskRepeat repeat);

G_END_DECLS
