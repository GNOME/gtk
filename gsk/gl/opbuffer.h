#ifndef __OP_BUFFER_H__
#define __OP_BUFFER_H__

#include <gdk/gdk.h>
#include <gsk/gsk.h>
#include <graphene.h>

#include "gskgldriverprivate.h"

typedef struct _Program Program;

typedef enum
{
  OP_NONE                              =  0,
  OP_CHANGE_OPACITY                    =  1,
  OP_CHANGE_COLOR                      =  2,
  OP_CHANGE_PROJECTION                 =  3,
  OP_CHANGE_MODELVIEW                  =  4,
  OP_CHANGE_PROGRAM                    =  5,
  OP_CHANGE_RENDER_TARGET              =  6,
  OP_CHANGE_CLIP                       =  7,
  OP_CHANGE_VIEWPORT                   =  8,
  OP_CHANGE_SOURCE_TEXTURE             =  9,
  OP_CHANGE_REPEAT                     = 10,
  OP_CHANGE_LINEAR_GRADIENT            = 11,
  OP_CHANGE_RADIAL_GRADIENT            = 12,
  OP_CHANGE_COLOR_MATRIX               = 13,
  OP_CHANGE_BLUR                       = 14,
  OP_CHANGE_INSET_SHADOW               = 15,
  OP_CHANGE_OUTSET_SHADOW              = 16,
  OP_CHANGE_BORDER                     = 17,
  OP_CHANGE_BORDER_COLOR               = 18,
  OP_CHANGE_BORDER_WIDTH               = 19,
  OP_CHANGE_CROSS_FADE                 = 20,
  OP_CHANGE_UNBLURRED_OUTSET_SHADOW    = 21,
  OP_CLEAR                             = 22,
  OP_DRAW                              = 23,
  OP_DUMP_FRAMEBUFFER                  = 24,
  OP_PUSH_DEBUG_GROUP                  = 25,
  OP_POP_DEBUG_GROUP                   = 26,
  OP_CHANGE_BLEND                      = 27,
  OP_CHANGE_GL_SHADER_ARGS             = 28,
  OP_CHANGE_EXTRA_SOURCE_TEXTURE       = 29,
  OP_CHANGE_CONIC_GRADIENT             = 30,
  OP_LAST
} OpKind;


typedef struct { int value; guint send: 1; }    IntUniformValue;
typedef struct { float value; guint send: 1; }    FloatUniformValue;
typedef struct { float value[2]; guint send: 1; } Float2UniformValue;
typedef struct { GskRoundedRect value; guint send: 1; guint send_corners: 1; } RRUniformValue;
typedef struct { const GdkRGBA *value; guint send: 1; } RGBAUniformValue;
typedef struct { const graphene_vec4_t *value; guint send: 1; } Vec4UniformValue;
typedef struct { const GskColorStop *value; guint send: 1; } ColorStopUniformValue;
typedef struct { const graphene_matrix_t *value; guint send: 1; } MatrixUniformValue;

/* OpNode are allocated within OpBuffer.pos, but we keep
 * a secondary index into the locations of that buffer
 * from OpBuffer.index. This allows peeking at the kind
 * and quickly replacing existing entries when necessary.
 */
typedef struct
{
  RRUniformValue outline;
  FloatUniformValue spread;
  Float2UniformValue offset;
  RGBAUniformValue color;
} OpShadow;

typedef struct
{
  RRUniformValue outline;
} OpOutsetShadow;

typedef struct
{
  guint  pos;
  OpKind kind;
} OpBufferEntry;

typedef struct
{
  guint8  *buf;
  gsize    buflen;
  gsize    bufpos;
  GArray  *index;
} OpBuffer;

typedef struct
{
  float opacity;
} OpOpacity;

typedef struct
{
  graphene_matrix_t matrix;
} OpMatrix;

typedef struct
{
  const Program *program;
} OpProgram;

typedef struct
{
  const GdkRGBA *rgba;
} OpColor;

typedef struct
{
  int render_target_id;
} OpRenderTarget;

typedef struct
{
  GskRoundedRect clip;
  guint send_corners: 1;
} OpClip;

typedef struct
{
  graphene_rect_t viewport;
} OpViewport;

typedef struct
{
  int texture_id;
} OpTexture;

typedef struct
{
  int texture_id;
  int idx;
} OpExtraTexture;

typedef struct
{
  gsize vao_offset;
  gsize vao_size;
} OpDraw;

typedef struct
{
  ColorStopUniformValue color_stops;
  IntUniformValue n_color_stops;
  float start_point[2];
  float end_point[2];
  gboolean repeat;
} OpLinearGradient;

typedef struct
{
  ColorStopUniformValue color_stops;
  IntUniformValue n_color_stops;
  float start;
  float end;
  float radius[2];
  float center[2];
  gboolean repeat;
} OpRadialGradient;

typedef struct
{
  ColorStopUniformValue color_stops;
  IntUniformValue n_color_stops;
  float center[2];
  float angle;
} OpConicGradient;

typedef struct
{
  MatrixUniformValue matrix;
  Vec4UniformValue offset;
} OpColorMatrix;

typedef struct
{
  float radius;
  graphene_size_t size;
  float dir[2];
} OpBlur;

typedef struct
{
  float widths[4];
  const GdkRGBA *color;
  GskRoundedRect outline;
} OpBorder;

typedef struct
{
  float progress;
  int source2;
} OpCrossFade;

typedef struct
{
  char *filename;
  int width;
  int height;
} OpDumpFrameBuffer;

typedef struct
{
  char text[64];
} OpDebugGroup;

typedef struct
{
  int source2;
  int mode;
} OpBlend;

typedef struct
{
  float child_bounds[4];
  float texture_rect[4];
} OpRepeat;

typedef struct
{
  float size[2];
  GskGLShader *shader;
  const guchar *uniform_data;
} OpGLShader;

void     op_buffer_init            (OpBuffer *buffer);
void     op_buffer_destroy         (OpBuffer *buffer);
void     op_buffer_clear           (OpBuffer *buffer);
gpointer op_buffer_add             (OpBuffer *buffer,
                                    OpKind    kind);

typedef struct
{
  GArray   *index;
  OpBuffer *buffer;
  guint     pos;
} OpBufferIter;

static inline void
op_buffer_iter_init (OpBufferIter *iter,
                     OpBuffer     *buffer)
{
  iter->index = buffer->index;
  iter->buffer = buffer;
  iter->pos = 1; /* Skip first OP_NONE */
}

static inline gpointer
op_buffer_iter_next (OpBufferIter *iter,
                     OpKind       *kind)
{
  const OpBufferEntry *entry;

  if (iter->pos == iter->index->len)
    return NULL;

  entry = &g_array_index (iter->index, OpBufferEntry, iter->pos);

  iter->pos++;

  *kind = entry->kind;
  return &iter->buffer->buf[entry->pos];
}

static inline void
op_buffer_pop_tail (OpBuffer *buffer)
{
  /* Never truncate the first OP_NONE */
  if G_LIKELY (buffer->index->len > 0)
    buffer->index->len--;
}

static inline gpointer
op_buffer_peek_tail (OpBuffer *buffer,
                     OpKind   *kind)
{
  const OpBufferEntry *entry;

  entry = &g_array_index (buffer->index, OpBufferEntry, buffer->index->len - 1);
  *kind = entry->kind;
  return &buffer->buf[entry->pos];
}

static inline gpointer
op_buffer_peek_tail_checked (OpBuffer *buffer,
                             OpKind    kind)
{
  const OpBufferEntry *entry;

  entry = &g_array_index (buffer->index, OpBufferEntry, buffer->index->len - 1);

  if (entry->kind == kind)
    return &buffer->buf[entry->pos];

  return NULL;
}

static inline guint
op_buffer_n_ops (OpBuffer *buffer)
{
  return buffer->index->len - 1;
}

#endif /* __OP_BUFFER_H__ */
