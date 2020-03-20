/*
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "bitarray.h"

#define BIT_ARRAY_MASK(onoff) (onoff ? (gpointer)(~(guintptr)0) : NULL)

static void
bit_array_expand_from_static (BitArray *ba,
                              guint     n_bytes)
{
  guint8 *old = ba->data;
  gpointer default_value;

  ba->data = g_malloc (n_bytes);
  memcpy (ba->data, &old, GLIB_SIZEOF_VOID_P);

  default_value = BIT_ARRAY_MASK (ba->default_value);
  for (guint i = ba->n_bytes; i < n_bytes; i++)
    ba->data[i] = GPOINTER_TO_UINT (default_value) & 0xFF;

  ba->n_bytes = n_bytes;
}

static void
bit_array_expand_realloc (BitArray *ba,
                          guint     n_bytes)
{
  gpointer default_value;

  ba->data = g_realloc (ba->data, n_bytes);

  default_value = BIT_ARRAY_MASK (ba->default_value);
  for (guint i = ba->n_bytes; i < n_bytes; i++)
    ba->data[i] = GPOINTER_TO_UINT (default_value) & 0xFF;

  ba->n_bytes = n_bytes;
}

void
bit_array_init (BitArray *ba,
                gboolean  default_value)
{
  ba->default_value = !!default_value;
  ba->n_bytes = GLIB_SIZEOF_VOID_P;
  ba->data = BIT_ARRAY_MASK (default_value);
}

void
bit_array_clear (BitArray *ba)
{
  if (ba->n_bytes != GLIB_SIZEOF_VOID_P)
    g_free (ba->data);
}

void
bit_array_set (BitArray *ba,
               guint     position,
               gboolean  value)
{
  guint8 *buf;
  guint target = position >> 3;

  if (target >= ba->n_bytes)
    {
      if (ba->n_bytes == GLIB_SIZEOF_VOID_P)
        bit_array_expand_from_static (ba, target + 1);
      else
        bit_array_expand_realloc (ba, target + 1);
    }

  if (ba->n_bytes == GLIB_SIZEOF_VOID_P)
    buf = (gpointer)&ba->data;
  else
    buf = ba->data;

  if (value)
    buf[target] |= (1U << (position & 0x7));
  else
    buf[target] &= ~(1U << (position & 0x7));
}

gboolean
bit_array_get (BitArray *ba,
               guint     position)
{
  guint8 *buf;
  guint target = position >> 3;

  if (target >= ba->n_bytes)
    return ba->default_value;

  if (ba->n_bytes == GLIB_SIZEOF_VOID_P)
    buf = (gpointer)&ba->data;
  else
    buf = ba->data;

  return (buf[target] >> (position & 0x7)) & 0x1;
}

#if 0
gint
main (gint argc,
      gchar *argv[])
{
  BitArray ba;

  bit_array_init (&ba, FALSE);
  g_assert_true (ba.data == NULL);
  for (guint i = 0; i < 100; i++)
    g_assert_cmpint (bit_array_get (&ba, i), ==, FALSE);
  g_assert_true (ba.data == NULL);
  bit_array_clear (&ba);

  bit_array_init (&ba, TRUE);
  g_assert_true (ba.data == BIT_ARRAY_MASK(TRUE));
  for (guint i = 0; i < 100; i++)
    g_assert_cmpint (bit_array_get (&ba, i), ==, TRUE);
  g_assert_true (ba.data == BIT_ARRAY_MASK(TRUE));
  bit_array_clear (&ba);

  bit_array_init (&ba, TRUE);
  g_assert_true (ba.data == BIT_ARRAY_MASK(TRUE));
  for (guint i = 0; i < 1024; i++)
    {
      g_assert_cmpint (bit_array_get (&ba, i), ==, TRUE);
      bit_array_set (&ba, i, FALSE);
      g_assert_cmpint (bit_array_get (&ba, i), ==, FALSE);
    }
  for (guint i = 0; i < 1024; i++)
    g_assert_cmpint (bit_array_get (&ba, i), ==, FALSE);
  for (guint i = 0; i < (1024/8); i++)
    g_assert_false (ba.data[i]);
  bit_array_clear (&ba);
}
#endif
