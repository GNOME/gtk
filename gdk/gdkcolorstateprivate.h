#pragma once

#include "gdkcolorstate.h"

#include "gdkcicpparamsprivate.h"
#include "gdkdebugprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkrgba.h"

typedef enum
{
  GDK_COLOR_STATE_ID_SRGB,
  GDK_COLOR_STATE_ID_SRGB_LINEAR,
  GDK_COLOR_STATE_ID_REC2100_PQ,
  GDK_COLOR_STATE_ID_REC2100_LINEAR,
  GDK_COLOR_STATE_ID_OKLAB,
  GDK_COLOR_STATE_ID_OKLCH,

  GDK_COLOR_STATE_N_IDS
} GdkColorStateId;

typedef struct _GdkColorStateClass GdkColorStateClass;

struct _GdkColorState
{
  const GdkColorStateClass *klass;
  gatomicrefcount ref_count;

  GdkMemoryDepth depth;
  GdkColorState *rendering_color_state;
  GdkColorState *rendering_color_state_linear;
};

/* Note: self may be the source or the target colorstate */
typedef void            (* GdkFloatColorConvert)(GdkColorState  *self,
                                                 float         (*values)[4],
                                                 gsize           n_values);

struct _GdkColorStateClass
{
  void                  (* free)                (GdkColorState  *self);
  gboolean              (* equal)               (GdkColorState  *self,
                                                 GdkColorState  *other);
  const char *          (* get_name)            (GdkColorState  *self);
  GdkColorState *       (* get_no_srgb_tf)      (GdkColorState  *self);
  GdkFloatColorConvert  (* get_convert_to)      (GdkColorState  *self,
                                                 GdkColorState  *target);
  GdkFloatColorConvert  (* get_convert_from)    (GdkColorState  *self,
                                                 GdkColorState  *source);
  const GdkCicp *       (* get_cicp)            (GdkColorState  *self);
  void                  (* clamp)               (GdkColorState  *self,
                                                 const float     src[4],
                                                 float           dest[4]);
};

typedef struct _GdkDefaultColorState GdkDefaultColorState;

struct _GdkDefaultColorState
{
  GdkColorState parent;

  const char *name;
  GdkColorState *no_srgb;
  GdkFloatColorConvert convert_to[GDK_COLOR_STATE_N_IDS];
  void (* clamp) (GdkColorState  *self,
                  const float     src[4],
                  float           dest[4]);

  GdkCicp cicp;
};

extern GdkDefaultColorState gdk_default_color_states[GDK_COLOR_STATE_N_IDS];

#define GDK_COLOR_STATE_SRGB           ((GdkColorState *) &gdk_default_color_states[GDK_COLOR_STATE_ID_SRGB])
#define GDK_COLOR_STATE_SRGB_LINEAR    ((GdkColorState *) &gdk_default_color_states[GDK_COLOR_STATE_ID_SRGB_LINEAR])
#define GDK_COLOR_STATE_REC2100_PQ     ((GdkColorState *) &gdk_default_color_states[GDK_COLOR_STATE_ID_REC2100_PQ])
#define GDK_COLOR_STATE_REC2100_LINEAR ((GdkColorState *) &gdk_default_color_states[GDK_COLOR_STATE_ID_REC2100_LINEAR])
#define GDK_COLOR_STATE_OKLAB          ((GdkColorState *) &gdk_default_color_states[GDK_COLOR_STATE_ID_OKLAB])
#define GDK_COLOR_STATE_OKLCH          ((GdkColorState *) &gdk_default_color_states[GDK_COLOR_STATE_ID_OKLCH])

#define GDK_IS_DEFAULT_COLOR_STATE(c) ((GdkDefaultColorState *) (c) >= &gdk_default_color_states[0] && \
                                       (GdkDefaultColorState *) (c) < &gdk_default_color_states[GDK_COLOR_STATE_N_IDS])
#define GDK_DEFAULT_COLOR_STATE_ID(c) ((GdkColorStateId) (((GdkDefaultColorState *) c) - gdk_default_color_states))

const char *    gdk_color_state_get_name                (GdkColorState          *self);
GdkColorState * gdk_color_state_get_no_srgb_tf          (GdkColorState          *self);

GdkColorState * gdk_color_state_new_for_cicp            (const GdkCicp          *cicp,
                                                         GError                **error);

void            gdk_color_state_clamp                   (GdkColorState          *self,
                                                         const float             src[4],
                                                         float                   dest[4]);

static inline GdkColorState *
gdk_color_state_get_rendering_color_state (GdkColorState *self)
{
  if (GDK_DEBUG_CHECK (HDR))
    self = GDK_COLOR_STATE_REC2100_PQ;

  if (!GDK_DEBUG_CHECK (LINEAR))
    return self->rendering_color_state;

  return self->rendering_color_state_linear;
}

static inline GdkMemoryDepth
gdk_color_state_get_depth (GdkColorState *self)
{
  if (!GDK_DEBUG_CHECK (LINEAR) && self->depth == GDK_MEMORY_U8_SRGB)
    return GDK_MEMORY_U8;

  return self->depth;
}

static inline GdkColorState *
gdk_color_state_get_by_id (GdkColorStateId id)
{
  return (GdkColorState *) &gdk_default_color_states[id];
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

/* Note: the functions returned from this expect the source
 * color state to be passed as self
 */
static inline GdkFloatColorConvert
gdk_color_state_get_convert_to (GdkColorState *self,
                                GdkColorState *target)
{
  return self->klass->get_convert_to (self, target);
}

/* Note: the functions returned from this expect the target
 * color state to be passed as self
 */
static inline GdkFloatColorConvert
gdk_color_state_get_convert_from (GdkColorState *self,
                                  GdkColorState *source)
{
  return self->klass->get_convert_from (self, source);
}

static inline const GdkCicp *
gdk_color_state_get_cicp (GdkColorState *self)
{
  return self->klass->get_cicp (self);
}

static inline void
gdk_color_state_convert_color (GdkColorState *src_cs,
                               const float    src[4],
                               GdkColorState *dest_cs,
                               float          dest[4])
{
  GdkFloatColorConvert convert = NULL;
  GdkFloatColorConvert convert2 = NULL;

  dest[0] = src[0];
  dest[1] = src[1];
  dest[2] = src[2];
  dest[3] = src[3];

  if (gdk_color_state_equal (src_cs, dest_cs))
    return;

  convert = gdk_color_state_get_convert_to (src_cs, dest_cs);

  if (!convert)
    convert2 = gdk_color_state_get_convert_from (dest_cs, src_cs);

  if (!convert && !convert2)
    {
      GdkColorState *connection = GDK_COLOR_STATE_REC2100_LINEAR;
      convert = gdk_color_state_get_convert_to (src_cs, connection);
      convert2 = gdk_color_state_get_convert_from (dest_cs, connection);
    }

  if (convert)
    convert (src_cs,  (float(*)[4]) dest, 1);

  if (convert2)
    convert2 (dest_cs, (float(*)[4]) dest, 1);
}

static inline void
gdk_color_state_from_rgba (GdkColorState *self,
                           const GdkRGBA *rgba,
                           float          out_color[4])
{
  gdk_color_state_convert_color (GDK_COLOR_STATE_SRGB,
                                 (const float *) rgba,
                                 self,
                                 out_color);
}
