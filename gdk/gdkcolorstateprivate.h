#pragma once

#include "gdkcolorstate.h"
#include "gdkmemoryformatprivate.h"

struct _GdkColorState
{
  GObject parent_instance;
};

struct _GdkColorStateClass
{
  GObjectClass parent_class;

  GBytes *              (* save_to_icc_profile) (GdkColorState        *self,
                                                 GError              **error);

  gboolean              (* equal)               (GdkColorState        *self,
                                                 GdkColorState        *other);

  const char *          (* get_name)            (GdkColorState        *self);
  GdkMemoryDepth        (* get_min_depth)       (GdkColorState        *self);
};

const char *   gdk_color_state_get_name      (GdkColorState *color_state);
GdkMemoryDepth gdk_color_state_get_min_depth (GdkColorState *color_state);
