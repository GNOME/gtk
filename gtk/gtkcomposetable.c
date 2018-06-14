/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Takao Fujiwara <takao.fujiwara1@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gdk/gdk.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "gtkcomposetable.h"
#include "gtkimcontextsimple.h"

#include "gtkimcontextsimpleprivate.h"


#define GTK_COMPOSE_TABLE_MAGIC "GtkComposeTable"
#define GTK_COMPOSE_TABLE_VERSION (1)

typedef struct {
  gunichar     *sequence;
  gunichar      value[2];
  gchar        *comment;
} GtkComposeData;


static void
gtk_compose_data_free (GtkComposeData *compose_data)
{
  g_free (compose_data->sequence);
  g_free (compose_data->comment);
  g_slice_free (GtkComposeData, compose_data);
}

static void
gtk_compose_list_element_free (GtkComposeData *compose_data, gpointer data)
{
  gtk_compose_data_free (compose_data);
}

static gboolean
is_codepoint (const gchar *str)
{
  int i;

  /* 'U' is not code point but 'U00C0' is code point */
  if (str[0] == '\0' || str[0] != 'U' || str[1] == '\0')
    return FALSE;

  for (i = 1; str[i] != '\0'; i++)
    {
      if (!g_ascii_isxdigit (str[i]))
        return FALSE;
    }

  return TRUE;
}

static gboolean
parse_compose_value (GtkComposeData *compose_data,
                     const gchar    *val,
                     const gchar    *line)
{
  gchar **words = g_strsplit (val, "\"", 3);
  gunichar uch;

  if (g_strv_length (words) < 3)
    {
      g_warning ("Need to double-quote the value: %s: %s", val, line);
      goto fail;
    }

  uch = g_utf8_get_char (words[1]);

  if (uch == 0)
    {
      g_warning ("Invalid value: %s: %s", val, line);
      goto fail;
    }
  else if (uch == '\\')
    {
      uch = words[1][1];

      /* The escaped string "\"" is separated with '\\' and '"'. */
      if (uch == '\0' && words[2][0] == '"')
        uch = '"';
      /* The escaped octal */
      else if (uch >= '0' && uch <= '8')
        uch = g_ascii_strtoll(words[1] + 1, NULL, 8);
      /* If we need to handle other escape sequences. */
      else if (uch != '\\')
        {
          g_warning ("Invalid escape sequence: %s: %s", val, line);
        }
    }

  if (g_utf8_get_char (g_utf8_next_char (words[1])) > 0)
    {
      g_warning ("GTK+ supports to output one char only: %s: %s", val, line);
      goto fail;
    }

  compose_data->value[1] = uch;

  if (uch == '"')
    compose_data->comment = g_strdup (g_strstrip (words[2] + 1));
  else
    compose_data->comment = g_strdup (g_strstrip (words[2]));

  g_strfreev (words);

  return TRUE;

fail:
  g_strfreev (words);
  return FALSE;
}

static gboolean
parse_compose_sequence (GtkComposeData *compose_data,
                        const gchar    *seq,
                        const gchar    *line)
{
  gchar **words = g_strsplit (seq, "<", -1);
  int i;
  int n = 0;

  if (g_strv_length (words) < 2)
    {
      g_warning ("key sequence format is <a> <b>...: %s", line);
      goto fail;
    }

  for (i = 1; words[i] != NULL; i++)
    {
      gchar *start = words[i];
      gchar *end = strchr (words[i], '>');
      gchar *match;
      gunichar codepoint;

      if (words[i][0] == '\0')
             continue;

      if (start == NULL || end == NULL || end <= start)
        {
          g_warning ("key sequence format is <a> <b>...: %s", line);
          goto fail;
        }

      match = g_strndup (start, end - start);

      if (compose_data->sequence == NULL)
        compose_data->sequence = g_malloc (sizeof (gunichar) * 2);
      else
        compose_data->sequence = g_realloc (compose_data->sequence, sizeof (gunichar) * (n + 2));

      if (is_codepoint (match))
        {
          codepoint = (gunichar) g_ascii_strtoll (match + 1, NULL, 16);
          compose_data->sequence[n] = codepoint;
          compose_data->sequence[n + 1] = 0;
        }
      else
        {
          codepoint = (gunichar) gdk_keyval_from_name (match);
          compose_data->sequence[n] = codepoint;
          compose_data->sequence[n + 1] = 0;
        }

      if (codepoint == GDK_KEY_VoidSymbol)
        g_warning ("Could not get code point of keysym %s", match);
      g_free (match);
      n++;
    }

  g_strfreev (words);
  if (0 == n || n >= GTK_MAX_COMPOSE_LEN)
    {
      g_warning ("The max number of sequences is %d: %s",
                 GTK_MAX_COMPOSE_LEN, line);
      return FALSE;
    }

  return TRUE;

fail:
  g_strfreev (words);
  return FALSE;
}

static void
parse_compose_line (GList       **compose_list,
                    const gchar  *line)
{
  gchar **components = NULL;
  GtkComposeData *compose_data = NULL;

  if (line[0] == '\0' || line[0] == '#')
    return;

  if (g_str_has_prefix (line, "include "))
    return;

  components = g_strsplit (line, ":", 2);

  if (components[1] == NULL)
    {
      g_warning ("No delimiter ':': %s", line);
      goto fail;
    }

  compose_data = g_slice_new0 (GtkComposeData);

  if (!parse_compose_sequence (compose_data, g_strstrip (components[0]), line))
    goto fail;

  if (!parse_compose_value (compose_data, g_strstrip (components[1]), line))
    goto fail;

  g_strfreev (components);

  *compose_list = g_list_append (*compose_list, compose_data);

  return;

fail:
  g_strfreev (components);
  if (compose_data)
    gtk_compose_data_free (compose_data);
}

static GList *
gtk_compose_list_parse_file (const gchar *compose_file)
{
  gchar *contents = NULL;
  gchar **lines = NULL;
  gsize length = 0;
  GError *error = NULL;
  GList *compose_list = NULL;
  int i;

  if (!g_file_get_contents (compose_file, &contents, &length, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return NULL;
    }

  lines = g_strsplit (contents, "\n", -1);
  g_free (contents);
  for (i = 0; lines[i] != NULL; i++)
    parse_compose_line (&compose_list, lines[i]);
  g_strfreev (lines);

  return compose_list;
}

static GList *
gtk_compose_list_check_duplicated (GList *compose_list)
{
  GList *list;
  GList *removed_list = NULL;
  GtkComposeData *compose_data;

  for (list = compose_list; list != NULL; list = list->next)
    {
      static guint16 keysyms[GTK_MAX_COMPOSE_LEN + 1];
      int i;
      int n_compose = 0;
      gboolean compose_finish;
      gunichar output_char;

      compose_data = list->data;

      for (i = 0; i < GTK_MAX_COMPOSE_LEN + 1; i++)
        keysyms[i] = 0;

      for (i = 0; i < GTK_MAX_COMPOSE_LEN + 1; i++)
        {
          gunichar codepoint = compose_data->sequence[i];
          keysyms[i] = (guint16) codepoint;

          if (codepoint == 0)
            break;

          n_compose++;
        }

      if (gtk_check_compact_table (&gtk_compose_table_compact,
                                   keysyms,
                                   n_compose,
                                   &compose_finish,
                                   NULL,
                                   &output_char) &&
          compose_finish)
        {
          if (compose_data->value[1] == output_char)
            removed_list = g_list_prepend (removed_list, compose_data);
        }
      else if (gtk_check_algorithmically (keysyms, n_compose, &output_char))
        {
          if (compose_data->value[1] == output_char)
            removed_list = g_list_prepend (removed_list, compose_data);
        }
    }

  for (list = removed_list; list != NULL; list = list->next)
    {
      compose_data = list->data;
      compose_list = g_list_remove (compose_list, compose_data);
      gtk_compose_data_free (compose_data);
    }

  g_list_free (removed_list);

  return compose_list;
}

static GList *
gtk_compose_list_check_uint16 (GList *compose_list)
{
  GList *list;
  GList *removed_list = NULL;
  GtkComposeData *compose_data;

  for (list = compose_list; list != NULL; list = list->next)
    {
      int i;

      compose_data = list->data;
      for (i = 0; i < GTK_MAX_COMPOSE_LEN; i++)
        {
          gunichar codepoint = compose_data->sequence[i];

          if (codepoint == 0)
            break;

          if (codepoint > 0xffff)
            {
              removed_list = g_list_prepend (removed_list, compose_data);
              break;
            }
        }
    }

  for (list = removed_list; list != NULL; list = list->next)
    {
      compose_data = list->data;
      compose_list = g_list_remove (compose_list, compose_data);
      gtk_compose_data_free (compose_data);
    }

  g_list_free (removed_list);

  return compose_list;
}

static GList *
gtk_compose_list_format_for_gtk (GList *compose_list,
                                 int   *p_max_compose_len,
                                 int   *p_n_index_stride)
{
  GList *list;
  GtkComposeData *compose_data;
  int max_compose_len = 0;
  int i;
  gunichar codepoint;

  for (list = compose_list; list != NULL; list = list->next)
    {
      compose_data = list->data;
      for (i = 0; i < GTK_MAX_COMPOSE_LEN + 1; i++)
        {
          codepoint = compose_data->sequence[i];
          if (codepoint == 0)
            {
              if (max_compose_len < i)
                max_compose_len = i;
              break;
            }
        }
    }

  if (p_max_compose_len)
    *p_max_compose_len = max_compose_len;
  if (p_n_index_stride)
    *p_n_index_stride = max_compose_len + 2;

  for (list = compose_list; list != NULL; list = list->next)
    {
      compose_data = list->data;
      codepoint = compose_data->value[1];
      if (codepoint > 0xffff)
        {
          compose_data->value[0] = codepoint / 0x10000;
          compose_data->value[1] = codepoint - codepoint / 0x10000 * 0x10000;
        }
    }

  return compose_list;
}

static gint
gtk_compose_data_compare (gpointer a,
                          gpointer b,
                          gpointer data)
{
  GtkComposeData *compose_data_a = a;
  GtkComposeData *compose_data_b = b;
  int max_compose_len = GPOINTER_TO_INT (data);
  int i;
  for (i = 0; i < max_compose_len; i++)
    {
      gunichar code_a = compose_data_a->sequence[i];
      gunichar code_b = compose_data_b->sequence[i];

      if (code_a != code_b)
        return code_a - code_b;
    }

  return 0;
}

static void
gtk_compose_list_print (GList *compose_list,
                        int    max_compose_len,
                        int    n_index_stride)
{
  GList *list;
  int i, j;
  GtkComposeData *compose_data;
  int total_size = 0;
  gunichar upper;
  gunichar lower;
  const gchar *comment;
  const gchar *keyval;

  for (list = compose_list; list != NULL; list = list->next)
    {
      compose_data = list->data;
      g_printf ("  ");

      for (i = 0; i < max_compose_len; i++)
        {
          if (compose_data->sequence[i] == 0)
            {
              for (j = i; j < max_compose_len; j++)
                {
                    if (j == max_compose_len - 1)
                      g_printf ("0,\n");
                    else
                      g_printf ("0, ");
                }
              break;
            }

          keyval = gdk_keyval_name (compose_data->sequence[i]);
          if (i == max_compose_len - 1)
            g_printf ("%s,\n", keyval ? keyval : "(null)");
          else
            g_printf ("%s, ", keyval ? keyval : "(null)");
        }
      upper = compose_data->value[0];
      lower = compose_data->value[1];
      comment = compose_data->comment;

      if (list == g_list_last (compose_list))
        g_printf ("    %#06X, %#06X  /* %s */\n", upper, lower, comment);
      else
        g_printf ("    %#06X, %#06X, /* %s */\n", upper, lower, comment);

      total_size += n_index_stride;
    }

  g_printerr ("TOTAL_SIZE: %d\nMAX_COMPOSE_LEN: %d\nN_INDEX_STRIDE: %d\n",
              total_size, max_compose_len, n_index_stride);
}

/* Implemented from g_str_hash() */
static guint32
gtk_compose_table_data_hash (gconstpointer v, int length)
{
  const guint16 *p, *head;
  unsigned char c;
  guint32 h = 5381;

  for (p = v, head = v; (p - head) < length; p++)
    {
      c = 0x00ff & (*p >> 8);
      h = (h << 5) + h + c;
      c = 0x00ff & *p;
      h = (h << 5) + h + c;
    }

  return h;
}

static gchar *
gtk_compose_hash_get_cache_path (guint32 hash)
{
  gchar *basename = NULL;
  gchar *dir = NULL;
  gchar *path = NULL;

  basename = g_strdup_printf ("%08x.cache", hash);

  dir = g_build_filename (g_get_user_cache_dir (), "gtk-4.0", "compose", NULL);
  path = g_build_filename (dir, basename, NULL);
  if (g_mkdir_with_parents (dir, 0755) != 0)
    {
      g_warning ("Failed to mkdir %s", dir);
      g_free (path);
      path = NULL;
    }

  g_free (dir);
  g_free (basename);

  return path;
}

static gchar *
gtk_compose_table_serialize (GtkComposeTable *compose_table,
                             gsize           *count)
{
  gchar *p, *contents;
  gsize length, total_length;
  guint16 bytes;
  const gchar *header = GTK_COMPOSE_TABLE_MAGIC;
  const guint16 version = GTK_COMPOSE_TABLE_VERSION;
  guint16 max_seq_len = compose_table->max_seq_len;
  guint16 index_stride = max_seq_len + 2;
  guint16 n_seqs = compose_table->n_seqs;
  guint32 i;

  g_return_val_if_fail (compose_table != NULL, NULL);
  g_return_val_if_fail (max_seq_len > 0, NULL);
  g_return_val_if_fail (index_stride > 0, NULL);

  length = strlen (header);
  total_length = length + sizeof (guint16) * (3 +  index_stride * n_seqs);
  if (count)
    *count = total_length;

  p = contents = g_slice_alloc (total_length);

  memcpy (p, header, length);
  p += length;

  /* Copy by byte for endian */
#define BYTE_COPY_FROM_BUF(element)                                           \
  bytes = GUINT16_TO_BE ((element));                                          \
  memcpy (p, &bytes, length);                                                 \
  p += length;                                                                \
  if (p - contents > total_length)                                            \
    {                                                                         \
      g_warning ("data size %lld is bigger than %" G_GSIZE_FORMAT,            \
                 (long long) (p - contents), total_length);                   \
      g_free (contents);                                                      \
      if (count)                                                              \
        {                                                                     \
          *count = 0;                                                         \
        }                                                                     \
      return NULL;                                                            \
    }

  length = sizeof (guint16);

  BYTE_COPY_FROM_BUF (version);
  BYTE_COPY_FROM_BUF (max_seq_len);
  BYTE_COPY_FROM_BUF (n_seqs);

  for (i = 0; i < (guint32) index_stride * n_seqs; i++)
    {
      BYTE_COPY_FROM_BUF (compose_table->data[i]);
    }

#undef BYTE_COPY_FROM_BUF

  return contents;
}

static gint
gtk_compose_table_find (gconstpointer data1,
                        gconstpointer data2)
{
  const GtkComposeTable *compose_table = (const GtkComposeTable *) data1;
  guint32 hash = (guint32) GPOINTER_TO_INT (data2);
  return compose_table->id != hash;
}

static GtkComposeTable *
gtk_compose_table_load_cache (const gchar *compose_file)
{
  guint32 hash;
  gchar *path = NULL;
  gchar *contents = NULL;
  gchar *p;
  GStatBuf original_buf;
  GStatBuf cache_buf;
  gsize total_length;
  gsize length;
  GError *error = NULL;
  guint16 bytes;
  guint16 version;
  guint16 max_seq_len;
  guint16 index_stride;
  guint16 n_seqs;
  guint32 i;
  guint16 *gtk_compose_seqs = NULL;
  GtkComposeTable *retval;

  hash = g_str_hash (compose_file);
  if ((path = gtk_compose_hash_get_cache_path (hash)) == NULL)
    return NULL;
  if (!g_file_test (path, G_FILE_TEST_EXISTS))
    goto out_load_cache;

  g_stat (compose_file, &original_buf);
  g_stat (path, &cache_buf);
  if (original_buf.st_mtime > cache_buf.st_mtime)
    goto out_load_cache;
  if (!g_file_get_contents (path, &contents, &total_length, &error))
    {
      g_warning ("Failed to get cache content %s: %s", path, error->message);
      g_error_free (error);
      goto out_load_cache;
    }

  /* Copy by byte for endian */
#define BYTE_COPY_TO_BUF(element)                                       \
  memcpy (&bytes, p, length);                                           \
  element = GUINT16_FROM_BE (bytes);                                    \
  p += length;                                                          \
  if (p - contents > total_length)                                      \
    {                                                                   \
      g_warning ("Broken cache content %s in %s", path, #element);      \
      goto out_load_cache;                                              \
    }

  p = contents;
  if (g_ascii_strncasecmp (p, GTK_COMPOSE_TABLE_MAGIC,
                           strlen (GTK_COMPOSE_TABLE_MAGIC)) != 0)
    {
      g_warning ("The file is not a GtkComposeTable cache file %s", path);
      goto out_load_cache;
    }
  p += strlen (GTK_COMPOSE_TABLE_MAGIC);
  if (p - contents > total_length)
    {
      g_warning ("Broken cache content %s at head", path);
      goto out_load_cache;
    }

  length = sizeof (guint16);

  BYTE_COPY_TO_BUF (version);
  if (version != GTK_COMPOSE_TABLE_VERSION)
    {
      g_warning ("cache version is different %u != %u",
                 version, GTK_COMPOSE_TABLE_VERSION);
      goto out_load_cache;
    }

  BYTE_COPY_TO_BUF (max_seq_len);
  BYTE_COPY_TO_BUF (n_seqs);

  if (max_seq_len == 0 || n_seqs == 0)
    {
      g_warning ("cache size is not correct %d %d", max_seq_len, n_seqs);
      goto out_load_cache;
    }

  index_stride = max_seq_len + 2;
  gtk_compose_seqs = g_new0 (guint16, n_seqs * index_stride);

  for (i = 0; i < (guint32) index_stride * n_seqs; i++)
    {
      BYTE_COPY_TO_BUF (gtk_compose_seqs[i]);
    }

  retval = g_new0 (GtkComposeTable, 1);
  retval->data = gtk_compose_seqs;
  retval->max_seq_len = max_seq_len;
  retval->n_seqs = n_seqs;
  retval->id = hash;

  g_free (contents);
  g_free (path);

  return retval;

#undef BYTE_COPY_TO_BUF

out_load_cache:
  g_free (gtk_compose_seqs);
  g_free (contents);
  g_free (path);
  return NULL;
}

static void
gtk_compose_table_save_cache (GtkComposeTable *compose_table)
{
  gchar *path = NULL;
  gchar *contents = NULL;
  GError *error = NULL;
  gsize length = 0;

  if ((path = gtk_compose_hash_get_cache_path (compose_table->id)) == NULL)
    return;

  contents = gtk_compose_table_serialize (compose_table, &length);
  if (contents == NULL)
    {
      g_warning ("Failed to serialize compose table %s", path);
      goto out_save_cache;
    }
  if (!g_file_set_contents (path, contents, length, &error))
    {
      g_warning ("Failed to save compose table %s: %s", path, error->message);
      g_error_free (error);
      goto out_save_cache;
    }

out_save_cache:
  g_slice_free1 (length, contents);
  g_free (path);
}

static GtkComposeTable *
gtk_compose_table_new_with_list (GList   *compose_list,
                                 int      max_compose_len,
                                 int      n_index_stride,
                                 guint32  hash)
{
  guint length;
  guint n = 0;
  int i, j;
  guint16 *gtk_compose_seqs = NULL;
  GList *list;
  GtkComposeData *compose_data;
  GtkComposeTable *retval = NULL;

  g_return_val_if_fail (compose_list != NULL, NULL);

  length = g_list_length (compose_list);

  gtk_compose_seqs = g_new0 (guint16, length * n_index_stride);

  for (list = compose_list; list != NULL; list = list->next)
    {
      compose_data = list->data;
      for (i = 0; i < max_compose_len; i++)
        {
          if (compose_data->sequence[i] == 0)
            {
              for (j = i; j < max_compose_len; j++)
                gtk_compose_seqs[n++] = 0;
              break;
            }
          gtk_compose_seqs[n++] = (guint16) compose_data->sequence[i];
        }
      gtk_compose_seqs[n++] = (guint16) compose_data->value[0];
      gtk_compose_seqs[n++] = (guint16) compose_data->value[1];
    }

  retval = g_new0 (GtkComposeTable, 1);
  retval->data = gtk_compose_seqs;
  retval->max_seq_len = max_compose_len;
  retval->n_seqs = length;
  retval->id = hash;

  return retval;
}

GtkComposeTable *
gtk_compose_table_new_with_file (const gchar *compose_file)
{
  GList *compose_list = NULL;
  GtkComposeTable *compose_table;
  int max_compose_len = 0;
  int n_index_stride = 0;

  g_assert (compose_file != NULL);

  compose_list = gtk_compose_list_parse_file (compose_file);
  if (compose_list == NULL)
    return NULL;
  compose_list = gtk_compose_list_check_duplicated (compose_list);
  compose_list = gtk_compose_list_check_uint16 (compose_list);
  compose_list = gtk_compose_list_format_for_gtk (compose_list,
                                                  &max_compose_len,
                                                  &n_index_stride);
  compose_list = g_list_sort_with_data (compose_list,
                                        (GCompareDataFunc) gtk_compose_data_compare,
                                        GINT_TO_POINTER (max_compose_len));
  if (compose_list == NULL)
    {
      g_warning ("compose file %s does not include any keys besides keys in en-us compose file", compose_file);
      return NULL;
    }

  if (g_getenv ("GTK_COMPOSE_TABLE_PRINT") != NULL)
    gtk_compose_list_print (compose_list, max_compose_len, n_index_stride);

  compose_table = gtk_compose_table_new_with_list (compose_list,
                                                   max_compose_len,
                                                   n_index_stride,
                                                   g_str_hash (compose_file));
  g_list_free_full (compose_list, (GDestroyNotify) gtk_compose_list_element_free);
  return compose_table;
}

GSList *
gtk_compose_table_list_add_array (GSList        *compose_tables,
                                  const guint16 *data,
                                  gint           max_seq_len,
                                  gint           n_seqs)
{
  guint32 hash;
  GtkComposeTable *compose_table;
  int n_index_stride = max_seq_len + 2;
  int length = n_index_stride * n_seqs;
  int i;
  guint16 *gtk_compose_seqs = NULL;

  g_return_val_if_fail (data != NULL, compose_tables);
  g_return_val_if_fail (max_seq_len <= GTK_MAX_COMPOSE_LEN, compose_tables);

  hash = gtk_compose_table_data_hash (data, length);

  if (g_slist_find_custom (compose_tables, GINT_TO_POINTER (hash), gtk_compose_table_find) != NULL)
    return compose_tables;

  gtk_compose_seqs = g_new0 (guint16, length);
  for (i = 0; i < length; i++)
    gtk_compose_seqs[i] = data[i];

  compose_table = g_new (GtkComposeTable, 1);
  compose_table->data = gtk_compose_seqs;
  compose_table->max_seq_len = max_seq_len;
  compose_table->n_seqs = n_seqs;
  compose_table->id = hash;

  return g_slist_prepend (compose_tables, compose_table);
}

GSList *
gtk_compose_table_list_add_file (GSList      *compose_tables,
                                 const gchar *compose_file)
{
  guint32 hash;
  GtkComposeTable *compose_table;

  g_return_val_if_fail (compose_file != NULL, compose_tables);

  hash = g_str_hash (compose_file);
  if (g_slist_find_custom (compose_tables, GINT_TO_POINTER (hash), gtk_compose_table_find) != NULL)
    return compose_tables;

  compose_table = gtk_compose_table_load_cache (compose_file);
  if (compose_table != NULL)
    return g_slist_prepend (compose_tables, compose_table);

  if ((compose_table = gtk_compose_table_new_with_file (compose_file)) == NULL)
    return compose_tables;

  gtk_compose_table_save_cache (compose_table);
  return g_slist_prepend (compose_tables, compose_table);
}
