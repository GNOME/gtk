#include "gskgradientprivate.h"

GskGradient *
gsk_gradient_new (void)
{
  GskGradient *gradient = g_new0 (GskGradient, 1);

  gradient_stops_init (&gradient->stops);

  gradient->interpolation = GDK_COLOR_STATE_SRGB;
  gradient->hue_interpolation = GSK_HUE_INTERPOLATION_SHORTER;
  gradient->premultiplied = TRUE;
  gradient->repeat = GSK_REPEAT_PAD;
  gradient->opaque = TRUE;

  return gradient;
}

GskGradient *
gsk_gradient_copy (const GskGradient *gradient)
{
  return gsk_gradient_init_copy (gsk_gradient_new (), gradient);
}

GskGradient *
gsk_gradient_init_copy (GskGradient       *gradient,
                        const GskGradient *orig)
{
  gradient_stops_splice (&gradient->stops,
                         0, 0, FALSE,
                         gradient_stops_get_data (&orig->stops),
                         gradient_stops_get_size (&orig->stops));

  for (unsigned int i = 0; i < gradient_stops_get_size (&gradient->stops); i++)
    {
      GskGradientStop *s = gradient_stops_index (&gradient->stops, i);
      gdk_color_state_ref (s->color.color_state);
    }

  gradient->interpolation = gdk_color_state_ref (orig->interpolation);
  gradient->hue_interpolation = orig->hue_interpolation;
  gradient->premultiplied = orig->premultiplied;
  gradient->repeat = orig->repeat;
  gradient->opaque = orig->opaque;

  return gradient;
}

void
gsk_gradient_clear (GskGradient *gradient)
{
  gradient_stops_clear (&gradient->stops);
  gdk_color_state_unref (gradient->interpolation);
  g_free (gradient->rgba_stops);
}

void
gsk_gradient_free (GskGradient *gradient)
{
  gsk_gradient_clear (gradient);
  g_free (gradient);
}

gboolean
gsk_gradient_equal (const GskGradient *gradient0,
                    const GskGradient *gradient1)
{
  if (gradient0->repeat != gradient1->repeat ||
      gradient0->hue_interpolation != gradient1->hue_interpolation ||
      gradient0->premultiplied != gradient1->premultiplied ||
      gradient_stops_get_size (&gradient0->stops) != gradient_stops_get_size (&gradient1->stops) ||
      !gdk_color_state_equal (gradient0->interpolation, gradient1->interpolation))
    return FALSE;

  for (gsize i = 0; i < gradient_stops_get_size (&gradient0->stops); i++)
    {
      GskGradientStop *stop0 = gradient_stops_index (&gradient0->stops, i);
      GskGradientStop *stop1 = gradient_stops_index (&gradient1->stops, i);

      if (stop0->offset != stop1->offset ||
          stop0->transition_hint != stop1->transition_hint ||
          !gdk_color_equal (&stop0->color, &stop1->color))
        return FALSE;
    }

  return TRUE;
}

void
gsk_gradient_add_stop (GskGradient    *gradient,
                       float           offset,
                       float           transition_hint,
                       const GdkColor *color)
{
  GskGradientStop stop;

  g_return_if_fail (0 <= offset && offset <= 1);

  if (gradient_stops_get_size (&gradient->stops) > 0)
    g_return_if_fail (offset >= gradient_stops_index (&gradient->stops, gradient_stops_get_size (&gradient->stops) - 1)->offset);

  stop.offset = offset;
  stop.transition_hint = transition_hint;
  gdk_color_init_copy (&stop.color, color);
  gradient_stops_append (&gradient->stops, &stop);

  gradient->opaque = gradient->opaque & gdk_color_is_opaque (color);
}

void
gsk_gradient_add_color_stops (GskGradient        *gradient,
                              const GskColorStop *stops,
                              gsize               n_stops)
{
  for (gsize i = 0; i < n_stops; i++)
    {
      GskGradientStop stop;

      stop.offset = stops[i].offset;
      stop.transition_hint = 0.5;
      gdk_color_init_from_rgba (&stop.color, &stops[i].color);
      gradient_stops_append (&gradient->stops, &stop);
      gradient->opaque = gradient->opaque & gdk_color_is_opaque (&stop.color);
    }
}

void
gsk_gradient_set_repeat (GskGradient *gradient,
                         GskRepeat    repeat)
{
  gradient->repeat = repeat;
}

void
gsk_gradient_set_interpolation (GskGradient   *gradient,
                                GdkColorState *interpolation)
{
  gdk_color_state_unref (gradient->interpolation);
  gradient->interpolation = gdk_color_state_ref (interpolation);
}

void
gsk_gradient_set_hue_interpolation (GskGradient         *gradient,
                                    GskHueInterpolation  hue_interpolation)
{
  gradient->hue_interpolation = hue_interpolation;
}

void
gsk_gradient_set_premultiplied (GskGradient *gradient,
                                gboolean     premultiplied)
{
  gradient->premultiplied = premultiplied;
}

gsize
gsk_gradient_get_n_stops (const GskGradient *gradient)
{
  return gradient_stops_get_size (&gradient->stops);
}

const GskGradientStop *
gsk_gradient_get_stops (const GskGradient *gradient)
{
  return (const GskGradientStop *) gradient_stops_get_data (&gradient->stops);
}

const GdkColor *
gsk_gradient_get_stop_color (const GskGradient *gradient,
                             gsize              idx)
{
  return &gradient_stops_index (&gradient->stops, idx)->color;
}

float
gsk_gradient_get_stop_offset (const GskGradient *gradient,
                              gsize              idx)
{
  return gradient_stops_index (&gradient->stops, idx)->offset;
}

float
gsk_gradient_get_stop_transition_hint (const GskGradient *gradient,
                                       gsize              idx)
{
  return gradient_stops_index (&gradient->stops, idx)->transition_hint;
}

GdkColorState *
gsk_gradient_get_interpolation (const GskGradient *gradient)
{
  return gradient->interpolation;
}

GskHueInterpolation
gsk_gradient_get_hue_interpolation (const GskGradient *gradient)
{
  return gradient->hue_interpolation;
}

gboolean
gsk_gradient_get_premultiplied (const GskGradient *gradient)
{
  return gradient->premultiplied;
}

GskRepeat
gsk_gradient_get_repeat (const GskGradient *gradient)
{
  return gradient->repeat;
}

/*< private >
 * gsk_gradient_get_color_stops:
 * @gradient: a gradient
 * @n_stops: (out): Return location for the number of stops
 *
 * Returns an array of GskColorStop structs representing
 * the color stops of the gradient.
 *
 * This is used to implement the deprecated render node
 * APIs for color stops.
 */
const GskColorStop *
gsk_gradient_get_color_stops (GskGradient *gradient,
                              gsize       *n_stops)
{
  const GskColorStop *stops;

  if (n_stops != NULL)
    *n_stops = gradient_stops_get_size (&gradient->stops);

  if (gradient->rgba_stops == NULL)
    {
      gradient->rgba_stops = g_new (GskColorStop, gradient_stops_get_size (&gradient->stops));
      for (gsize i = 0; i < gradient_stops_get_size (&gradient->stops); i++)
        {
          const GskGradientStop *stop = gradient_stops_index (&gradient->stops, i);
          gradient->rgba_stops[i].offset = stop->offset;
          gdk_color_to_float (&stop->color, GDK_COLOR_STATE_SRGB, (float *) &gradient->rgba_stops[i].color);
        }
    }

  stops = gradient->rgba_stops;

  return stops;
}

/*< private >
 * gsk_gradient_is_opaque:
 * @gradient: a gradient
 *
 * Returns whether @gradient completely covers
 * the plane with non-translucent color.
 *
 * Returns: true if @gradient is opaque
 */
gboolean
gsk_gradient_is_opaque (const GskGradient *gradient)
{
  return gradient->opaque &&
         gradient_stops_get_size (&gradient->stops) > 0 &&
         gradient->repeat != GSK_REPEAT_NONE;
}

/*< private >
 * gsk_gradient_check_single_color:
 * @gradient: a gradient
 *
 * Checks whether the gradient fills the entire plane
 * with a single color.
 *
 * This API is used to optimize away gradient nodes
 * that can be replaced by color nodes.
 *
 * Returns: (transfer none) (nullable): the single color
 *   that the gradient will fill the plane with, or `NULL`
 *   if the gradient will not fill the plane with a single
 *   color
 */
const GdkColor *
gsk_gradient_check_single_color (const GskGradient *gradient)
{
  GskGradientStop *first = gradient_stops_index (&gradient->stops, 0);

  if (gradient->hue_interpolation == GSK_HUE_INTERPOLATION_LONGER)
    return NULL;

  if (gradient->repeat == GSK_REPEAT_NONE)
    return NULL;

  for (unsigned int i = 1; i < gradient_stops_get_size (&gradient->stops); i++)
    {
      GskGradientStop *next = gradient_stops_index (&gradient->stops, i);
      if (!gdk_color_equal (&first->color, &next->color))
        return NULL;
    }

  return &first->color;
}

float
gsk_hue_interpolation_fixup (GskHueInterpolation  interp,
                             float                h1,
                             float                h2)
{
  float d;

  d = h2 - h1;

  while (d > 360)
    {
      h2 -= 360;
      d = h2 - h1;
    }
  while (d < -360)
    {
      h2 += 360;
      d = h2 - h1;
    }

  g_assert (fabsf (d) <= 360);

  switch (interp)
    {
    case GSK_HUE_INTERPOLATION_SHORTER:
      {
        if (d > 180)
          h2 -= 360;
        else if (d < -180)
          h2 += 360;
      }
      g_assert (fabsf (h2 - h1) <= 180);
      break;

   case GSK_HUE_INTERPOLATION_LONGER:
      {
        if (0 < d && d < 180)
          h2 -= 360;
        else if (-180 < d && d <= 0)
          h2 += 360;
      g_assert (fabsf (h2 - h1) >= 180);
      }
      break;

    case GSK_HUE_INTERPOLATION_INCREASING:
      if (h2 < h1)
        h2 += 360;
      d = h2 - h1;
      g_assert (h1 <= h2);
      break;

    case GSK_HUE_INTERPOLATION_DECREASING:
      if (h1 < h2)
        h2 -= 360;
      d = h2 - h1;
      g_assert (h1 >= h2);
      break;

    default:
      g_assert_not_reached ();
    }

  return h2;
}

cairo_extend_t
gsk_repeat_to_cairo (GskRepeat repeat)
{
  switch (repeat)
    {
    case GSK_REPEAT_NONE: return CAIRO_EXTEND_NONE;
    case GSK_REPEAT_REPEAT: return CAIRO_EXTEND_REPEAT;
    case GSK_REPEAT_REFLECT: return CAIRO_EXTEND_REFLECT;
    case GSK_REPEAT_PAD: return CAIRO_EXTEND_PAD;
    default: g_assert_not_reached ();
    }
}
