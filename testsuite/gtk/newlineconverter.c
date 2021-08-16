/*
 * Copyright © 2021 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include <locale.h>

#include <gdk/gnewlineconverter.h>

#define N 100

#define MAX_SIZE 20

static const char *words[] = { "", "", "lorem", "ipsum", "dolor", "sit", "amet",
  "consectetur", "adipisci", "elit", "sed", "eiusmod", "tempor", "incidunt",
  "labore", "et", "dolore", "magna", "aliqua", "ut", "enim", "ad", "minim",
  "veniam", "quis", "nostrud", "exercitation", "ullamco", "laboris", "nisi",
  "ut", "aliquid", "ex", "ea", "commodi", "consequat" };

static const char *breaks[] = { "", "\r", "\n", "\r\n" };

static GBytes *
generate_random_text (gboolean fuzz)
{
  GByteArray *array;
  guint i, n;

  array = g_byte_array_new ();

  n = g_test_rand_int_range (0, 100);

  for (i = 0; i < n; i++)
    {
      const char *word;

      word = words[g_test_rand_int_range (0, G_N_ELEMENTS (words))];
      g_byte_array_append (array, (const guchar *) word, strlen (word));

      word = breaks[g_test_rand_int_range (0, G_N_ELEMENTS (breaks))];
      g_byte_array_append (array, (const guchar *) word, strlen (word));
    }

  if (fuzz && array->len > 0)
    {
      for (i = 0; i < 100; i++)
        {
          array->data[g_test_rand_int_range (0, array->len)] = g_test_rand_int_range (0, 255);
        }
    }

  return g_byte_array_free_to_bytes (array);
}

static GBytes *
convert (GBytes                 *input,
         GDataStreamNewlineType  to_newline,
         GDataStreamNewlineType  from_newline)
{
  GConverter *converter;
  GByteArray *output;
  const guchar *inbuf, *inbuf_end;
  gsize inbuf_size;

  output = g_byte_array_new ();
  converter = G_CONVERTER (g_newline_converter_new (to_newline, from_newline));
  inbuf = g_bytes_get_data (input, &inbuf_size);
  inbuf_end = inbuf + inbuf_size;

  while (inbuf < inbuf_end)
    {
      gsize in_size, out_size, bytes_read, bytes_written;
      guchar outbuf[MAX_SIZE];
      GConverterResult res;
      GError *error = NULL;

      in_size = g_test_rand_int_range (1, MAX_SIZE);
      in_size = MIN (in_size, inbuf_end - inbuf);
      out_size = g_test_rand_int_range (1, MAX_SIZE);

      res = g_converter_convert (converter,
                                 inbuf,
                                 in_size,
                                 outbuf,
                                 out_size,
                                 inbuf + in_size == inbuf_end ? G_CONVERTER_INPUT_AT_END : 0,
                                 &bytes_read,
                                 &bytes_written,
                                 &error);
      switch (res)
        {
          case G_CONVERTER_ERROR:
            g_assert_nonnull (error);
            if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PARTIAL_INPUT))
              {
                g_assert (bytes_read == 0);
                g_assert (bytes_written == 0);
                g_clear_error (&error);
                continue;
              }
            /* There should never be any other error, but this check is more informative
             * than assert_not_reached () */
            g_assert_no_error (error);
            break;
          case G_CONVERTER_CONVERTED:
          case G_CONVERTER_FINISHED:
            g_assert_no_error (error);
            g_assert (bytes_read > 0);
            g_assert (bytes_written > 0);
            g_assert (bytes_read <= in_size);
            g_assert (bytes_written <= out_size);
            inbuf += bytes_read;
            g_byte_array_append (output, outbuf, bytes_written);
            if (res == G_CONVERTER_FINISHED)
              g_assert (inbuf == inbuf_end);
            else
              g_assert (inbuf < inbuf_end);
            break;
          case G_CONVERTER_FLUSHED:
            /* we don't pass FLUSH, so it should never happen */
            g_assert_not_reached ();
            break;
          default:
            g_assert_not_reached ();
            break;
        }
    }

  return g_byte_array_free_to_bytes (output);
}

#define assert_bytes_equal(one, two) G_STMT_START{\
  const char *one_data, *two_data; \
  gsize one_size, two_size; \
  one_data = g_bytes_get_data (one, &one_size); \
  two_data = g_bytes_get_data (two, &two_size); \
  g_assert_cmpmem (one_data, one_size, two_data, two_size); \
} G_STMT_END

#define assert_bytes_equal_text(one, two) G_STMT_START{\
  const char *one_data, *two_data; \
  char **a, **b; \
  gsize ai, bi; \
  one_data = g_bytes_get_data (one, NULL); \
  two_data = g_bytes_get_data (two, NULL); \
  a = g_strsplit_set (one_data ? one_data : "", "\r\n", -1); \
  b = g_strsplit_set (two_data ? two_data : "", "\r\n", -1); \
  for (ai = bi = 0; a[ai] && b[bi]; ai++) \
    { \
      if (*a[ai] == 0) \
        continue; \
      for (; b[bi]; bi++) \
        { \
          if (*b[bi] == 0) \
            continue; \
        } \
      if (!b[bi]) \
        break; \
      g_assert_cmpstr (a[ai], ==, b[bi]); \
    }\
  g_strfreev(b);\
  g_strfreev(a);\
} G_STMT_END

static void
test_intermediate (void)
{
  GBytes *input, *output1, *output2, *tmp;
  gsize i;
  GDataStreamNewlineType target, intermediate;

  for (i = 0; i < N; i++)
    {
      target = g_test_rand_int_range (0, 3); /* not including any here */
      intermediate = g_test_rand_int_range (0, 4); /* can include any */

      input = generate_random_text (TRUE);

      output1 = convert (input, target, G_DATA_STREAM_NEWLINE_TYPE_ANY);
      tmp = convert (input, intermediate, G_DATA_STREAM_NEWLINE_TYPE_ANY);
      output2 = convert (tmp, target, intermediate);
      
      assert_bytes_equal (output1, output2);

      g_bytes_unref (tmp);
      g_bytes_unref (output2);
      g_bytes_unref (output1);
      g_bytes_unref (input);
    }
}

static void
test_conversion_and_back (void)
{
  GBytes *input, *output1, *output2, *output3, *tmp;
  gsize i;
  GDataStreamNewlineType start, target;

  for (i = 0; i < N; i++)
    {
      start = g_test_rand_bit () ? G_DATA_STREAM_NEWLINE_TYPE_CR : G_DATA_STREAM_NEWLINE_TYPE_LF;
      target = g_test_rand_int_range (0, 3); /* not including any here */

      tmp = generate_random_text (g_test_rand_bit ());
      /* convert either all CR => LF or all LF => CR */
      input = convert (tmp, start, start == G_DATA_STREAM_NEWLINE_TYPE_LF ? G_DATA_STREAM_NEWLINE_TYPE_CR : G_DATA_STREAM_NEWLINE_TYPE_LF);
      g_bytes_unref (tmp);

      output1 = convert (input, target, start);
      output2 = convert (output1, start, target);
      output3 = convert (input, target, G_DATA_STREAM_NEWLINE_TYPE_ANY);
      
      assert_bytes_equal (output1, output3);
      assert_bytes_equal (input, output2);

      g_bytes_unref (output3);
      g_bytes_unref (output2);
      g_bytes_unref (output1);
      g_bytes_unref (input);
    }
}

static void
test_simple (void)
{
  GBytes *input, *output;
  gsize i;

  for (i = 0; i < N; i++)
    {
      input = generate_random_text (FALSE);
      output = convert (input, g_test_rand_int_range (0, 4), g_test_rand_int_range (0, 4));
  
      assert_bytes_equal_text (input, output);

      g_bytes_unref (output);
      g_bytes_unref (input);
    }
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/newlineconverter/simple", test_simple);
  g_test_add_func ("/newlineconverter/intermediate", test_intermediate);
  g_test_add_func ("/newlineconverter/conversion_and_back", test_conversion_and_back);

  return g_test_run ();
}
