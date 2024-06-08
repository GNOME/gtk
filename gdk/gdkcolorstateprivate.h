#pragma once

#include "gdkcolorstate.h"
#include "gdkmemoryformatprivate.h"

typedef enum
{
  GDK_COLOR_STATE_ID_SRGB,
  GDK_COLOR_STATE_ID_SRGB_LINEAR,
} GdkColorStateId;

typedef struct _GdkColorStateClass GdkColorStateClass;

struct _GdkColorState
{
  GdkColorStateClass *klass;
  int ref_count;
};

struct _GdkColorStateClass
{
  void                  (* free)                (GdkColorState  *self);
  gboolean              (* equal)               (GdkColorState  *self,
                                                 GdkColorState  *other);
  const char *          (* get_name)            (GdkColorState  *self);
};

extern GdkColorState gdk_default_color_states[];

#define GDK_COLOR_STATE_SRGB        (&gdk_default_color_states[GDK_COLOR_STATE_ID_SRGB])
#define GDK_COLOR_STATE_SRGB_LINEAR (&gdk_default_color_states[GDK_COLOR_STATE_ID_SRGB_LINEAR])

#define GDK_IS_DEFAULT_COLOR_STATE(c) (GDK_COLOR_STATE_SRGB <= (c) && (c) <= GDK_COLOR_STATE_SRGB_LINEAR)
#define GDK_DEFAULT_COLOR_STATE_ID(c) ((GdkColorStateId) (c - gdk_default_color_states))

const char *    gdk_color_state_get_name                (GdkColorState          *color_state);
const char *    gdk_color_state_get_name_from_id        (GdkColorStateId         id);
void            gdk_color_state_print                   (GdkColorState          *color_state,
                                                         GString                *string);

GdkMemoryDepth  gdk_color_state_get_min_depth           (GdkColorState          *color_state);

#define gdk_color_state_ref(self) _gdk_color_state_ref (self)
static inline GdkColorState *
_gdk_color_state_ref (GdkColorState *self)
{
  if (GDK_IS_DEFAULT_COLOR_STATE (self))
    return self;

  self->ref_count++;

  return self;
}

#define gdk_color_state_unref(self) _gdk_color_state_unref (self)
static inline void
_gdk_color_state_unref (GdkColorState *self)
{
  if (GDK_IS_DEFAULT_COLOR_STATE (self))
    return;

  self->ref_count--;

  if (self->ref_count == 0)
    self->klass->free (self);
}

#define gdk_color_state_equal(a,b) _gdk_color_state_equal ((a), (b))
static inline gboolean
_gdk_color_state_equal (GdkColorState *self,
                        GdkColorState *other)
{
  if (self == other)
    return TRUE;

  if (GDK_IS_DEFAULT_COLOR_STATE (self) || GDK_IS_DEFAULT_COLOR_STATE (other))
    return FALSE;

  if (self->klass != other->klass)
    return FALSE;

  return self->klass->equal (self, other);
}

