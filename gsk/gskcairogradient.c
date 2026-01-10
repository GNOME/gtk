#include "gskcairogradientprivate.h"

#include "gskrectprivate.h"

#include "gdk/gdkcairoprivate.h"

static float
adjust_hue (GskHueInterpolation interp,
            float               h1,
            float               h2)
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

#define lerp(t,a,b) ((a) + (t) * ((b) - (a)))

void
gsk_cairo_interpolate_color_stops (GdkColorState        *ccs,
                                   GdkColorState        *interpolation,
                                   GskHueInterpolation   hue_interpolation,
                                   float                 offset1,
                                   const GdkColor       *color1,
                                   float                 offset2,
                                   const GdkColor       *color2,
                                   float                 transition_hint,
                                   GskColorStopCallback  callback,
                                   gpointer              data)
{
  float values1[4];
  float values2[4];
  int n;
  float exp;

  gdk_color_to_float (color1, interpolation, values1);
  gdk_color_to_float (color2, interpolation, values2);

  if (gdk_color_state_equal (interpolation, GDK_COLOR_STATE_OKLCH))
    {
      values2[2] = adjust_hue (hue_interpolation, values1[2], values2[2]);
      /* don't make hue steps larger than 30° */
      n = ceilf (fabsf (values2[2] - values1[2]) / 30);
    }
  else
    {
      /* just some steps */
      n = 7;
    }

  if (transition_hint <= 0)
    exp = 0;
  else if (transition_hint >= 1)
    exp = INFINITY;
  else if (transition_hint == 0.5)
    exp = 1;
  else
    exp = - M_LN2 / logf (transition_hint);

  for (int k = 1; k < n; k++)
    {
      float f = k / (float) n;
      float values[4];
      float offset;
      float C;
      GdkColor c;

      if (transition_hint <= 0)
        C = 1;
      else if (transition_hint >= 1)
        C = 0;
      else if (transition_hint == 0.5)
        C = f;
      else
        C = powf (f, exp);

      values[0] = lerp (C, values1[0], values2[0]);
      values[1] = lerp (C, values1[1], values2[1]);
      values[2] = lerp (C, values1[2], values2[2]);
      values[3] = lerp (C, values1[3], values2[3]);
      offset = lerp (f, offset1, offset2);

      gdk_color_init (&c, interpolation, values);
      gdk_color_to_float (&c, ccs, values);

      callback (offset, ccs, values, data);

      gdk_color_finish (&c);
    }
}
