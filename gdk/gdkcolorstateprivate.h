#pragma once

#include "gdkcolorstate.h"
#include "gdkmemoryformatprivate.h"
#include <lcms2.h>

const char *   gdk_color_state_get_name      (GdkColorState *color_state);
GdkMemoryDepth gdk_color_state_get_min_depth (GdkColorState *color_state);
int            gdk_color_state_get_hue_coord (GdkColorState *color_state);

typedef void (* StepFunc) (float  s0, float  s1, float  s2,
                           float *d0, float *d1, float *d2);

typedef struct _GdkColorStateTransform GdkColorStateTransform;

struct _GdkColorStateTransform
{
  gpointer transform;
  StepFunc step1;
  StepFunc step2;
  gboolean cms_first;
  gboolean copy_alpha;
};

void gdk_color_state_transform_init (GdkColorStateTransform *tf,
                                     GdkColorState           *from,
                                     GdkColorState           *to,
                                     gboolean                 copy_alpha);

void gdk_color_state_transform_finish (GdkColorStateTransform *tf);

void gdk_color_state_transform (GdkColorStateTransform *tf,
                                const float            *src,
                                float                  *dst,
                                int                     width);

GdkColorState *gdk_color_state_new_from_lcms_profile (cmsHPROFILE profile);

const char *gdk_color_state_get_name_from_id (GdkColorStateId id);
