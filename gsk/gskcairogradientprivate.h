#pragma once

#include "gskgradientprivate.h"
#include "gdk/gdkcolorstateprivate.h"
#include "gdk/gdkcolorprivate.h"


typedef void (* GskColorStopCallback) (float          offset,
                                       GdkColorState *ccs,
                                       float          values[4],
                                       gpointer       data);
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
                                   gpointer              data);
