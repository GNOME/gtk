#include "config.h"

#include "broadway-buffer.h"

#include <string.h>

/* This code is based on some code from weston with this license:
 *
 * Copyright © 2012 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

struct entry {
  int count;
  int matches;
  guint32 hash;
  int x, y;
  int index;
};

struct _BroadwayBuffer {
  guint8 *data;
  struct entry *table;
  int width, height, stride;
  int encoded;
  int block_stride, length, block_count, shift;
  int stats[5];
  int clashes;
};

static const guint32 prime = 0x1f821e2d;
static const guint32 end_prime = 0xf907ec81;	/* prime^block_size */
#if 0
static const guint32 vprime = 0x0137b89d;
static const guint32 end_vprime = 0xaea9a281;	/* vprime^block_size */
#else
static const guint32 vprime = 0xf907ec81;
static const guint32 end_vprime = 0xcdb99001;	/* vprime^block_size */
#endif
static const guint32 step = 0x0ac93019;
static const int block_size = 32, block_mask = 31;

static gboolean
verify_block_match (BroadwayBuffer *buffer, int x, int y,
                    BroadwayBuffer *prev, struct entry *entry)
{
  int i;
  void *old, *match;
  int w1, w2, h1, h2;

  w1 = block_size;
  if (x + block_size > buffer->width)
    w1 = buffer->width - x;

  h1 = block_size;
  if (y + block_size > buffer->height)
    h1 = buffer->height - y;

  w2 = block_size;
  if (entry->x + block_size > prev->width)
    w2 = prev->width - entry->x;

  h2 = block_size;
  if (entry->y + block_size > prev->height)
    h2 = prev->height - entry->y;

  if (w1 != w2 || h1 != h2)
    return FALSE;

  for (i = 0; i < h1; i++)
    {
      match = buffer->data + (y + i) * buffer->stride + x * 4;
      old = prev->data + (entry->y + i) * prev->stride + entry->x * 4;
      if (memcmp (match, old, w1 * 4) != 0)
        {
          buffer->clashes++;
          return FALSE;
        }
    }

  return TRUE;
}

static void
insert_block (BroadwayBuffer *buffer, guint32 h, int x, int y)
{
  struct entry *entry;
  int i;
  guint32 collision = 0;

  entry = &buffer->table[h >> buffer->shift];
  for (i = step; entry->count > 0 && entry->hash != h; i += step)
    {
      entry = &buffer->table[(h + i) >> buffer->shift];
      collision++;
    }

  entry->hash = h;
  entry->count++;
  entry->x = x;
  entry->y = y;
  entry->index = (buffer->block_stride * y + x) / block_size;

  if (collision > G_N_ELEMENTS (buffer->stats) - 1)
    collision = G_N_ELEMENTS (buffer->stats) - 1;
  buffer->stats[collision]++;
}

static struct entry *
lookup_block (BroadwayBuffer *prev, guint32 h)
{
  guint32 i;
  struct entry *entry;
  int shift = prev->shift;

  for (i = h;
       entry = &prev->table[i >> shift], entry->count > 0;
       i += step)
    {
      if (entry->hash == h)
        return entry;
    }

  return NULL;
}

struct encoder {
  guint32 color;
  guint32 color_run;
  guint32 delta;
  guint32 delta_run;
  GString *dest;
  int bytes;
};

/* Encoding:
 *
 *  - all 1 pixel colors are encoded literally
 *
 *  - We don’t need to support colors with alpha 0 and non-zero
 *    color components, as they mean the same on the canvas anyway.
 *    So we use these as special codes:
 *
 *     - 0x00 00 00 00 : one alpha 0 pixel
 *     - 0xaa rr gg bb : one color pixel, alpha > 0
 *     - 0x00 1x xx xx : delta 0 run, x is length, (20 bits)
 *     - 0x00 2x xx xx 0x xxxx yyyy: block ref, block number x (20 bits) at x, y
 *     - 0x00 3x xx xx 0xaarrggbb : solid color run, length x
 *     - 0x00 4x xx xx 0xaarrggbb : delta run, length x
 *
 */

static void
emit (struct encoder *encoder, guint32 symbol)
{
  g_string_append_len (encoder->dest, (char *)&symbol, sizeof (guint32));
  encoder->bytes += sizeof (guint32);
}

static void
encode_run (struct encoder *encoder)
{
  if (encoder->color_run == 0 && encoder->delta_run == 0)
    return;

  if (encoder->color_run >= encoder->delta_run)
    {
      if (encoder->color_run == 1)
        emit (encoder, encoder->color);
      else
        {
          emit (encoder, 0x00300000 | encoder->color_run);
          emit (encoder, encoder->color);
        }
    }
  else
    {
      if (encoder->delta == 0)
        emit(encoder, 0x00100000 | encoder->delta_run);
      else
        {
          emit(encoder, 0x00400000 | encoder->delta_run);
          emit(encoder, encoder->delta);
        }
    }
}

static void
encode_pixel (struct encoder *encoder, guint32 color, guint32 prev_color)
{
  guint32 delta = 0;
  guint32 a, r, g, b;

  if (color == prev_color)
    delta = 0;
  else if (prev_color == 0)
    delta = color;
  else
    {
      a = ((color & 0xff000000) - (prev_color & 0xff000000)) & 0xff000000;
      r = ((color & 0x00ff0000) - (prev_color & 0x00ff0000)) & 0x00ff0000;
      g = ((color & 0x0000ff00) - (prev_color & 0x0000ff00)) & 0x0000ff00;
      b = ((color & 0x000000ff) - (prev_color & 0x000000ff)) & 0x000000ff;

      delta = a | r | g | b;
    }

  if ((encoder->color != color &&
       encoder->color_run > encoder->delta_run) ||

      (encoder->delta != delta &&
       encoder->delta_run > encoder->color_run) ||

      (encoder->delta != delta && encoder->color != color) ||

      (encoder->delta_run == 0xFFFFF || encoder->color_run == 0xFFFFF))
    {
      encode_run (encoder);

      encoder->color_run = 1;
      encoder->color = color;
      encoder->delta_run = 1;
      encoder->delta = delta;
      return;
    }

  if (encoder->color == color)
    encoder->color_run++;
  else
    {
      encoder->color_run = 1;
      encoder->color = color;
    }

  if (encoder->delta == delta)
    encoder->delta_run++;
  else
    {
      encoder->delta_run = 1;
      encoder->delta = delta;
    }
}

static void
encoder_flush (struct encoder *encoder)
{
  encode_run (encoder);
}


static void
encode_block (struct encoder *encoder, struct entry *entry, int x, int y)
{
  /* 0x00 2x xx xx 0x xxxx yyyy:
   *	block ref, block number x (20 bits) at x, y */

  /* FIXME: Maybe don't encode pixels under blocks and just emit
   * blocks at their position within the stream. */

  emit (encoder, 0x00200000 | entry->index);
  emit (encoder, (x << 16) | y);
}

void
broadway_buffer_destroy (BroadwayBuffer *buffer)
{
  g_free (buffer->data);
  g_free (buffer->table);
  g_free (buffer);
}

int
broadway_buffer_get_width (BroadwayBuffer *buffer)
{
  return buffer->width;
}

int
broadway_buffer_get_height (BroadwayBuffer *buffer)
{
  return buffer->height;
}

static void
unpremultiply_line (void *destp, void *srcp, int width)
{
  guint32 *src = srcp;
  guint32 *dest = destp;
  guint32 *end = src + width;
  while (src < end)
    {
      guint32 pixel;
      guint8 alpha, r, g, b;

      pixel = *src++;

      alpha = (pixel & 0xff000000) >> 24;

      if (alpha == 0xff)
        *dest++ = pixel;
      else if (alpha == 0)
        *dest++ = 0;
      else
        {
          r = (((pixel & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
          g = (((pixel & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
          b = (((pixel & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
          *dest++ = (guint32)alpha << 24 | (guint32)r << 16 | (guint32)g << 8 | (guint32)b;
        }
    }
}

BroadwayBuffer *
broadway_buffer_create (int width, int height, guint8 *data, int stride)
{
  BroadwayBuffer *buffer;
  int y, bits_required;

  buffer = g_new0 (BroadwayBuffer, 1);
  buffer->width = width;
  buffer->stride = width * 4;
  buffer->height = height;

  buffer->block_stride = (width + block_size - 1) / block_size;
  buffer->block_count =
    buffer->block_stride * ((height + block_size - 1) / block_size);
  bits_required = g_bit_storage (buffer->block_count * 4);
  buffer->shift = 32 - bits_required;
  buffer->length = 1 << bits_required;

  buffer->table = g_malloc0 (buffer->length * sizeof buffer->table[0]);

  memset (buffer->stats, 0, sizeof buffer->stats);
  buffer->clashes = 0;

  buffer->data = g_malloc (buffer->stride * height);

  for (y = 0; y < height; y++)
    unpremultiply_line (buffer->data + y * buffer->stride, data + y * stride, width);

  return buffer;
}

void
broadway_buffer_encode (BroadwayBuffer *buffer, BroadwayBuffer *prev, GString *dest)
{
  struct entry *entry;
  int i, j, k;
  int x0, x1, y0, y1;
  guint32 *block_hashes;
  guint32 hash, bottom_hash, h, *line, *bottom, *prev_line;
  int width, height;
  struct encoder encoder = { 0 };
  int *skyline, skyline_pixels;
  int matches;

  width = buffer->width;
  height = buffer->height;
  x0 = 0;
  x1 = width;
  y0 = 0;
  y1 = height;

  skyline = g_malloc0 ((width + block_size) * sizeof skyline[0]);

  block_hashes = g_malloc0 (width * sizeof block_hashes[0]);

  matches = 0;
  encoder.dest = dest;

  // Calculate the block hashes for the first row
  for (i = y0; i < MIN(y1, y0 + block_size); i++)
    {
      line = (guint32 *)(buffer->data + i * buffer->stride);
      hash = 0;
      for (j = x0; j < MIN(x1, x0 + block_size); j++)
        hash = hash * prime + line[j];
      for (; j < x0 + block_size; j++)
        hash = hash * prime;

      for (j = x0; j < x1; j++)
        {
          block_hashes[j] = block_hashes[j] * vprime + hash;

          hash = hash * prime - line[j] * end_prime;
          if (j + block_size < width)
            hash += line[j + block_size];
        }
    }
  // Do the last rows if height < block_size
  for (; i < y0 + block_size; i++)
    {
      for (j = x0; j < x1; j++)
        block_hashes[j] = block_hashes[j] * vprime;
    }

  for (i = y0; i < y1; i++)
    {
      line = (guint32 *) (buffer->data + i * buffer->stride);
      bottom = (guint32 *) (buffer->data + (i + block_size) * buffer->stride);
      bottom_hash = 0;
      hash = 0;
      skyline_pixels = 0;

      if (prev && i < prev->height)
        prev_line = (guint32 *) (prev->data + i * prev->stride);
      else
        prev_line = NULL;

      for (j = x0; j < x0 + block_size; j++)
        {
          hash = hash * prime;
          if (j < width)
            hash += line[j];
          if (i + block_size < height)
            {
              bottom_hash = bottom_hash * prime;
              if (j < width)
                bottom_hash += bottom[j];
            }
          if (i < skyline[j])
            skyline_pixels = 0;
          else
            skyline_pixels++;
        }

      for (j = x0; j < x1; j++)
        {
          if (i < skyline[j])
            encode_pixel (&encoder, line[j], line[j]);
          else if (prev)
            {
              /* FIXME: Add back overlap exception
               * for consecutive blocks */

              h = block_hashes[j];
              entry = lookup_block (prev, h);
              if (entry && entry->count < 2 &&
                  skyline_pixels >= block_size &&
                  verify_block_match (buffer, j, i, prev, entry) &&
                  (entry->x != j || entry->y != i))
                {
                  matches++;
                  encode_block (&encoder, entry, j, i);

                  for (k = 0; k < block_size; k++)
                    skyline[j + k] = i + block_size;

                  encode_pixel (&encoder, line[j], line[j]);
                }
              else
                {
                  if (prev_line && j < prev->width)
                    encode_pixel (&encoder, line[j],
                                  prev_line[j]);
                  else
                    encode_pixel (&encoder, line[j], 0);
                }
            }
          else
            encode_pixel (&encoder, line[j], 0);

          if (i < skyline[j + block_size])
            skyline_pixels = 0;
          else
            skyline_pixels++;

          /* Insert block in hash table if we're on a
           * grid point. */
          if (((i | j) & block_mask) == 0 && !buffer->encoded)
            insert_block (buffer, block_hashes[j], j, i);

          /* Update sliding block hash */
          block_hashes[j] =
            block_hashes[j] * vprime + bottom_hash -
            hash * end_vprime;

          if (i + block_size < height)
            {
              bottom_hash = bottom_hash * prime - bottom[j] * end_prime;
              if (j + block_size < width)
                bottom_hash += bottom[j + block_size];
            }
          hash = hash * prime - line[j] * end_prime;
          if  (j + block_size < width)
            hash += line[j + block_size] ;
        }
    }

  encoder_flush (&encoder);

#if 0
  fprintf(stderr, "collision stats:");
  for (i = 0; i < (int) G_N_ELEMENTS(buffer->stats); i++)
    fprintf(stderr, "%c%d", i == 0 ? ' ' : '/', buffer->stats[i]);
  fprintf(stderr, "\n");

  fprintf(stderr, "%d / %d blocks (%d%%) matched, %d clashes\n",
          matches, buffer->block_count,
          100 * matches / buffer->block_count, buffer->clashes);

  fprintf(stderr, "output stream %d bytes, raw buffer %d bytes (%d%%)\n",
          encoder.bytes, height * buffer->stride,
          100 * encoder.bytes / (height * buffer->stride));
#endif

  g_free (skyline);
  g_free (block_hashes);

  buffer->encoded = TRUE;
}
