#pragma once

#include "gdkcolorstate.h"
#include "gdkmemoryformatprivate.h"

typedef enum
{
  GDK_COLOR_STATE_ID_SRGB,
  GDK_COLOR_STATE_ID_SRGB_LINEAR,

  GDK_COLOR_STATE_N_IDS
} GdkColorStateId;

typedef struct _GdkColorStateClass GdkColorStateClass;

struct _GdkColorState
{
  const GdkColorStateClass *klass;
  gatomicrefcount ref_count;

  GdkMemoryDepth depth;
};

struct _GdkColorStateClass
{
  void                  (* free)                (GdkColorState  *self);
  gboolean              (* equal)               (GdkColorState  *self,
                                                 GdkColorState  *other);
  const char *          (* get_name)            (GdkColorState  *self);
  gboolean              (* has_srgb_tf)         (GdkColorState  *self,
                                                 GdkColorState **out_no_srgb);
};

typedef struct _GdkDefaultColorState GdkDefaultColorState;

struct _GdkDefaultColorState
{
  GdkColorState parent;

  const char *name;
  GdkColorState *no_srgb;
};

extern GdkDefaultColorState gdk_default_color_states[GDK_COLOR_STATE_N_IDS];

#define GDK_COLOR_STATE_SRGB        ((GdkColorState *) &gdk_default_color_states[GDK_COLOR_STATE_ID_SRGB])
#define GDK_COLOR_STATE_SRGB_LINEAR ((GdkColorState *) &gdk_default_color_states[GDK_COLOR_STATE_ID_SRGB_LINEAR])

#define GDK_IS_DEFAULT_COLOR_STATE(c) ((GdkDefaultColorState *) (c) >= &gdk_default_color_states[0] && \
                                       (GdkDefaultColorState *) (c) < &gdk_default_color_states[GDK_COLOR_STATE_N_IDS])
#define GDK_DEFAULT_COLOR_STATE_ID(c) ((GdkColorStateId) (((GdkDefaultColorState *) c) - gdk_default_color_states))

const char *    gdk_color_state_get_name                (GdkColorState          *color_state);
gboolean        gdk_color_state_has_srgb_tf             (GdkColorState          *self,
                                                         GdkColorState         **out_no_srgb);

static inline GdkMemoryDepth
gdk_color_state_get_depth (GdkColorState *self)
{
  return self->depth;
}

#define gdk_color_state_ref(self) _gdk_color_state_ref (self)
static inline GdkColorState *
_gdk_color_state_ref (GdkColorState *self)
{
  if (GDK_IS_DEFAULT_COLOR_STATE (self))
    return self;

  g_atomic_ref_count_inc (&self->ref_count);

  return self;
}

#define gdk_color_state_unref(self) _gdk_color_state_unref (self)
static inline void
_gdk_color_state_unref (GdkColorState *self)
{
  if (GDK_IS_DEFAULT_COLOR_STATE (self))
    return;

  if (g_atomic_ref_count_dec (&self->ref_count))
    self->klass->free (self);
}

#define gdk_color_state_equal(a,b) _gdk_color_state_equal ((a), (b))
static inline gboolean
_gdk_color_state_equal (GdkColorState *self,
                        GdkColorState *other)
{
  if (self == other)
    return TRUE;

  if (self->klass != other->klass)
    return FALSE;

  return self->klass->equal (self, other);
}

