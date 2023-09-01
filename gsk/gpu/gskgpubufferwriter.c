#include "config.h"

#include "gskgpubufferwriterprivate.h"

#include "gskrectprivate.h"

#include <string.h>

gsize
gsk_gpu_buffer_writer_commit (GskGpuBufferWriter *self)
{
  return self->finish (self, TRUE);
}

void
gsk_gpu_buffer_writer_abort (GskGpuBufferWriter *self)
{
  self->finish (self, FALSE);
}

gsize
gsk_gpu_buffer_writer_get_size (GskGpuBufferWriter *self)
{
  return self->size;
}

void
gsk_gpu_buffer_writer_rewind (GskGpuBufferWriter *self,
                              gsize               size)
{
  g_assert (size <= self->size);

  self->size = size;
}

void
gsk_gpu_buffer_writer_ensure_size (GskGpuBufferWriter *self,
                                   gsize               size)
{
  if (size <= self->allocated)
    return;

  self->ensure_size (self, size);
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

void
gsk_gpu_buffer_writer_append (GskGpuBufferWriter *self,
                              gsize               align,
                              const guchar       *data,
                              gsize               size)
{
  gsize aligned_size = round_up (self->size, align);
  gsk_gpu_buffer_writer_ensure_size (self, aligned_size + size);

  memcpy (self->data + aligned_size, data, size);
  self->size = aligned_size + size;
}

void
gsk_gpu_buffer_writer_append_float (GskGpuBufferWriter *self,
                                    float               f)
{
  gsk_gpu_buffer_writer_append (self, G_ALIGNOF (float), (guchar *) &f, sizeof (float));
}

void
gsk_gpu_buffer_writer_append_int (GskGpuBufferWriter *self,
                                  gint32              i)
{
  gsk_gpu_buffer_writer_append (self, G_ALIGNOF (gint32), (guchar *) &i, sizeof (gint32));
}

void
gsk_gpu_buffer_writer_append_uint (GskGpuBufferWriter *self,
                                   guint32             u)
{
  gsk_gpu_buffer_writer_append (self, G_ALIGNOF (guint32), (guchar *) &u, sizeof (guint32));
}

void
gsk_gpu_buffer_writer_append_rect (GskGpuBufferWriter     *self,
                                   const graphene_rect_t  *rect,
                                   const graphene_point_t *offset)
{
  float f[4];

  gsk_gpu_rect_to_float (rect, offset, f);

  gsk_gpu_buffer_writer_append (self, G_ALIGNOF (float), (guchar *) f, sizeof (f));
}
