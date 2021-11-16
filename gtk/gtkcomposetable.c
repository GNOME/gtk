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


#define GTK_COMPOSE_TABLE_MAGIC "GtkComposeTable"
#define GTK_COMPOSE_TABLE_VERSION (2)

/* Maximum length of sequences we parse */

#define MAX_COMPOSE_LEN 20

typedef struct {
  gunichar *sequence;
  char *value;
} GtkComposeData;


static void
gtk_compose_data_free (GtkComposeData *compose_data)
{
  g_free (compose_data->sequence);
  g_free (compose_data->value);
  g_slice_free (GtkComposeData, compose_data);
}

static void
gtk_compose_list_element_free (GtkComposeData *compose_data, gpointer data)
{
  gtk_compose_data_free (compose_data);
}

static gboolean
is_codepoint (const char *str)
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
                     const char     *val,
                     const char     *line)
{
  const char *p;
  GString *value;
  gunichar ch;
  char *endp;

  if (val[0] != '"')
    {
      g_warning ("Only strings supported after ':': %s: %s", val, line);
      goto fail;
    }

  value = g_string_new ("");

  p = val + 1;
  while (*p)
    {
      if (*p == '\0')
        {
          g_warning ("Missing closing '\"': %s: %s", val, line);
          goto fail;
        }
      else if (*p == '\"')
        {
          break;
        }
      else if (*p == '\\')
        {
          if (p[1] == '"')
            {
              g_string_append_c (value, '"');
              p += 2;
            }
          else if (p[1] == '\\')
            {
              g_string_append_c (value, '\\');
              p += 2;
            }
          else if (p[1] >= '0' && p[1] < '8')
            {
              ch = g_ascii_strtoll (p + 1, &endp, 8);
              if (ch == 0)
                {
                  g_warning ("Invalid escape sequence: %s: %s", val, line);
                  goto fail;
                }
              g_string_append_unichar (value, ch);
              p = endp;
            }
          else if (p[1] == 'x' || p[1] == 'X')
            {
              ch = g_ascii_strtoll (p + 2, &endp, 16);
              if (ch == 0)
                {
                  g_warning ("Invalid escape sequence: %s: %s", val, line);
                  goto fail;
                }
              g_string_append_unichar (value, ch);
              p = endp;
            }
          else
            {
              g_warning ("Invalid escape sequence: %s: %s", val, line);
              goto fail;
            }
        }
      else
        {
          ch = g_utf8_get_char (p);
          g_string_append_unichar (value, ch);
          p = g_utf8_next_char (p);
        }
    }

  compose_data->value = g_string_free (value, FALSE);

  return TRUE;

fail:
  return FALSE;
}

static gboolean
parse_compose_sequence (GtkComposeData *compose_data,
                        const char     *seq,
                        const char     *line)
{
  char **words = g_strsplit (seq, "<", -1);
  int i;
  int n = 0;

  if (g_strv_length (words) < 2)
    {
      g_warning ("key sequence format is <a> <b>...: %s", line);
      goto fail;
    }

  for (i = 1; words[i] != NULL; i++)
    {
      char *start = words[i];
      char *end = strchr (words[i], '>');
      char *match;
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
  if (0 == n || n > MAX_COMPOSE_LEN)
    {
      g_warning ("Suspicious compose sequence length (%d). Are you sure this is right?: %s",
                 n, line);
      return FALSE;
    }

  return TRUE;

fail:
  g_strfreev (words);
  return FALSE;
}

static void
parse_compose_line (GList       **compose_list,
                    const char   *line)
{
  char **components = NULL;
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

extern const GtkComposeTableCompact gtk_compose_table_compact;

static GList *
gtk_compose_list_parse_file (const char *compose_file)
{
  char *contents = NULL;
  char **lines = NULL;
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
      static guint16 keysyms[MAX_COMPOSE_LEN + 1];
      int i;
      int n_compose = 0;
      gboolean compose_finish;
      gunichar output_char;
      char buf[8] = { 0, };

      compose_data = list->data;

      for (i = 0; i < MAX_COMPOSE_LEN + 1; i++)
        keysyms[i] = 0;

      for (i = 0; i < MAX_COMPOSE_LEN + 1; i++)
        {
          gunichar codepoint = compose_data->sequence[i];
          keysyms[i] = (guint16) codepoint;

          if (codepoint == 0)
            break;

          n_compose++;
        }

      if (gtk_compose_table_compact_check (&gtk_compose_table_compact,
                                           keysyms, n_compose,
                                           &compose_finish,
                                           NULL,
                                           &output_char) &&
          compose_finish)
        {
          g_unichar_to_utf8 (output_char, buf);
          if (strcmp (compose_data->value, buf) == 0)
            removed_list = g_list_prepend (removed_list, compose_data);
        }
      else if (gtk_check_algorithmically (keysyms, n_compose, &output_char))
        {
          g_unichar_to_utf8 (output_char, buf);
          if (strcmp (compose_data->value, buf) == 0)
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
      for (i = 0; i < MAX_COMPOSE_LEN; i++)
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
      for (i = 0; i < MAX_COMPOSE_LEN + 1; i++)
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

  return compose_list;
}

static int
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

static char *
gtk_compose_hash_get_cache_path (guint32 hash)
{
  char *basename = NULL;
  char *dir = NULL;
  char *path = NULL;

  basename = g_strdup_printf ("%08x.cache", hash);

  dir = g_build_filename (g_get_user_cache_dir (), "gtk-3.0", "compose", NULL);
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

static char *
gtk_compose_table_serialize (GtkComposeTable *compose_table,
                             gsize           *count)
{
  char *p, *contents;
  gsize length, total_length;
  guint16 bytes;
  const char *header = GTK_COMPOSE_TABLE_MAGIC;
  const guint16 version = GTK_COMPOSE_TABLE_VERSION;
  guint16 max_seq_len = compose_table->max_seq_len;
  guint16 index_stride = max_seq_len + 2;
  guint16 n_seqs = compose_table->n_seqs;
  guint16 n_chars = compose_table->n_chars;
  guint32 i;

  g_return_val_if_fail (compose_table != NULL, NULL);
  g_return_val_if_fail (max_seq_len > 0, NULL);
  g_return_val_if_fail (index_stride > 0, NULL);

  length = strlen (header);
  total_length = length + sizeof (guint16) * (4 + index_stride * n_seqs) + n_chars;
  if (count)
    *count = total_length;

  p = contents = g_malloc (total_length);

  memcpy (p, header, length);
  p += length;

#define APPEND_GUINT16(elt) \
  bytes = GUINT16_TO_BE (elt); \
  memcpy (p, &bytes, sizeof (guint16)); \
  p += sizeof (guint16);

  APPEND_GUINT16 (version);
  APPEND_GUINT16 (max_seq_len);
  APPEND_GUINT16 (n_seqs);
  APPEND_GUINT16 (n_chars);

  for (i = 0; i < (guint32) index_stride * n_seqs; i++)
    {
      APPEND_GUINT16 (compose_table->data[i]);
    }

  if (compose_table->n_chars > 0)
    memcpy (p, compose_table->char_data, compose_table->n_chars);

#undef APPEND_GUINT16

  return contents;
}

static int
gtk_compose_table_find (gconstpointer data1,
                        gconstpointer data2)
{
  const GtkComposeTable *compose_table = (const GtkComposeTable *) data1;
  guint32 hash = (guint32) GPOINTER_TO_INT (data2);
  return compose_table->id != hash;
}

static GtkComposeTable *
gtk_compose_table_load_cache (const char *compose_file)
{
  guint32 hash;
  char *path = NULL;
  char *contents = NULL;
  char *p;
  GStatBuf original_buf;
  GStatBuf cache_buf;
  gsize total_length;
  GError *error = NULL;
  guint16 bytes;
  guint16 version;
  guint16 max_seq_len;
  guint16 index_stride;
  guint16 n_seqs;
  guint16 n_chars;
  guint32 i;
  guint16 *gtk_compose_seqs = NULL;
  GtkComposeTable *retval;
  char *char_data = NULL;

  hash = g_str_hash (compose_file);
  if ((path = gtk_compose_hash_get_cache_path (hash)) == NULL)
    return NULL;
  if (!g_file_test (path, G_FILE_TEST_EXISTS))
    goto out_load_cache;

  g_stat (path, &cache_buf);
  g_lstat (compose_file, &original_buf);
  if (original_buf.st_mtime > cache_buf.st_mtime)
    goto out_load_cache;
  g_stat (compose_file, &original_buf);
  if (original_buf.st_mtime > cache_buf.st_mtime)
    goto out_load_cache;
  if (!g_file_get_contents (path, &contents, &total_length, &error))
    {
      g_warning ("Failed to get cache content %s: %s", path, error->message);
      g_error_free (error);
      goto out_load_cache;
    }

#define GET_GUINT16(elt) \
  memcpy (&bytes, p, sizeof (guint16)); \
  elt = GUINT16_FROM_BE (bytes); \
  p += sizeof (guint16);

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

  GET_GUINT16 (version);
  if (version != GTK_COMPOSE_TABLE_VERSION)
    {
      g_warning ("cache version is different %u != %u",
                 version, GTK_COMPOSE_TABLE_VERSION);
      goto out_load_cache;
    }

  GET_GUINT16 (max_seq_len);
  GET_GUINT16 (n_seqs);
  GET_GUINT16 (n_chars);

  if (max_seq_len == 0 || n_seqs == 0)
    {
      g_warning ("cache size is not correct %d %d", max_seq_len, n_seqs);
      goto out_load_cache;
    }

  index_stride = max_seq_len + 2;
  gtk_compose_seqs = g_new0 (guint16, n_seqs * index_stride);

  for (i = 0; i < (guint32) index_stride * n_seqs; i++)
    {
      GET_GUINT16 (gtk_compose_seqs[i]);
    }

  if (n_chars > 0)
    {
      char_data = g_new (char, n_chars + 1);
      memcpy (char_data, p, n_chars);
      char_data[n_chars] = '\0';
    }

  retval = g_new0 (GtkComposeTable, 1);
  retval->data = gtk_compose_seqs;
  retval->max_seq_len = max_seq_len;
  retval->n_seqs = n_seqs;
  retval->char_data = char_data;
  retval->n_chars = n_chars;
  retval->id = hash;

  g_free (contents);
  g_free (path);

  return retval;

#undef GET_GUINT16

out_load_cache:
  g_free (gtk_compose_seqs);
  g_free (char_data);
  g_free (contents);
  g_free (path);
  return NULL;
}

static void
gtk_compose_table_save_cache (GtkComposeTable *compose_table)
{
  char *path = NULL;
  char *contents = NULL;
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
  g_free (contents);
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
  gunichar codepoint;
  GString *char_data;

  g_return_val_if_fail (compose_list != NULL, NULL);

  length = g_list_length (compose_list);

  gtk_compose_seqs = g_new0 (guint16, length * n_index_stride);

  char_data = g_string_new ("");

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

      if (g_utf8_strlen (compose_data->value, -1) > 1)
        {
          if (char_data->len > 0)
            g_string_append_c (char_data, 0);

          codepoint = char_data->len | (1 << 31);

          g_string_append (char_data, compose_data->value);
        }
      else
        {
          codepoint = g_utf8_get_char (compose_data->value);
          g_assert ((codepoint & (1 << 31)) == 0);
        }

      gtk_compose_seqs[n++] = (codepoint & 0xffff0000) >> 16;
      gtk_compose_seqs[n++] = codepoint & 0xffff;
    }

  retval = g_new0 (GtkComposeTable, 1);
  retval->data = gtk_compose_seqs;
  retval->max_seq_len = max_compose_len;
  retval->n_seqs = length;
  retval->id = hash;
  retval->n_chars = char_data->len;
  retval->char_data = g_string_free (char_data, FALSE);

  return retval;
}

GtkComposeTable *
gtk_compose_table_new_with_file (const char *compose_file)
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
                                  int            max_seq_len,
                                  int            n_seqs)
{
  guint32 hash;
  GtkComposeTable *compose_table;
  gsize n_index_stride;
  gsize length;
  int i;
  guint16 *gtk_compose_seqs = NULL;

  g_return_val_if_fail (data != NULL, compose_tables);
  g_return_val_if_fail (max_seq_len >= 0, compose_tables);
  g_return_val_if_fail (n_seqs >= 0, compose_tables);

  n_index_stride = max_seq_len + 2;
  if (!g_size_checked_mul (&length, n_index_stride, n_seqs))
    {
      g_critical ("Overflow in the compose sequences");
      return compose_tables;
    }

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
  compose_table->char_data = NULL;
  compose_table->n_chars = 0;

  return g_slist_prepend (compose_tables, compose_table);
}

GSList *
gtk_compose_table_list_add_file (GSList     *compose_tables,
                                 const char *compose_file)
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

static int
compare_seq (const void *key, const void *value)
{
  int i = 0;
  const guint16 *keysyms = key;
  const guint16 *seq = value;

  while (keysyms[i])
    {
      if (keysyms[i] < seq[i])
        return -1;
      else if (keysyms[i] > seq[i])
        return 1;

      i++;
    }

  return 0;
}

/*
 * gtk_compose_table_check:
 * @table: the table to check
 * @compose_buffer: the key vals to match
 * @n_compose: number of non-zero key vals in @compose_buffer
 * @compose_finish: (out): return location for whether there may be longer matches
 * @compose_match: (out): return location for whether there is a match
 * @output: (out) (caller-allocates): return location for the match values
 *
 * Looks for matches for a key sequence in @table.
 *
 * Returns: %TRUE if there were any matches, %FALSE otherwise
 */
gboolean
gtk_compose_table_check (const GtkComposeTable *table,
                         const guint16         *compose_buffer,
                         int                    n_compose,
                         gboolean              *compose_finish,
                         gboolean              *compose_match,
                         GString               *output)
{
  int row_stride = table->max_seq_len + 2;
  guint16 *seq;

  *compose_finish = FALSE;
  *compose_match = FALSE;

  g_string_set_size (output, 0);

  /* Will never match, if the sequence in the compose buffer is longer
   * than the sequences in the table.  Further, compare_seq (key, val)
   * will overrun val if key is longer than val.
   */
  if (n_compose > table->max_seq_len)
    return FALSE;

  seq = bsearch (compose_buffer,
                 table->data, table->n_seqs,
                 sizeof (guint16) * row_stride,
                 compare_seq);

  if (seq)
    {
      guint16 *prev_seq;

      /* Back up to the first sequence that matches to make sure
       * we find the exact match if there is one.
       */
      while (seq > table->data)
        {
          prev_seq = seq - row_stride;
          if (compare_seq (compose_buffer, prev_seq) != 0)
            break;
          seq = prev_seq;
        }

      if (n_compose == table->max_seq_len ||
          seq[n_compose] == 0) /* complete sequence */
        {
          guint16 *next_seq;
          gunichar value;

          value = (seq[table->max_seq_len] << 16) | seq[table->max_seq_len + 1];
          if ((value & (1 << 31)) != 0)
            g_string_append (output, &table->char_data[value & ~(1 << 31)]);
          else
            g_string_append_unichar (output, value);

          *compose_match = TRUE;

          /* We found a tentative match. See if there are any longer
           * sequences containing this subsequence
           */
          next_seq = seq + row_stride;
          if (next_seq < table->data + row_stride * table->n_seqs)
            {
              if (compare_seq (compose_buffer, next_seq) == 0)
                return TRUE;
            }

          *compose_finish = TRUE;
          return TRUE;
        }

      return TRUE;
    }

  return FALSE;
}

static int
compare_seq_index (const void *key, const void *value)
{
  const guint16 *keysyms = key;
  const guint16 *seq = value;

  if (keysyms[0] < seq[0])
    return -1;
  else if (keysyms[0] > seq[0])
    return 1;

  return 0;
}

gboolean
gtk_compose_table_compact_check (const GtkComposeTableCompact  *table,
                                 const guint16                 *compose_buffer,
                                 int                            n_compose,
                                 gboolean                      *compose_finish,
                                 gboolean                      *compose_match,
                                 gunichar                      *output_char)
{
  int row_stride;
  guint16 *seq_index;
  guint16 *seq;
  int i;
  gboolean match;
  gunichar value;

  if (compose_finish)
    *compose_finish = FALSE;
  if (compose_match)
    *compose_match = FALSE;
  if (output_char)
    *output_char = 0;

  /* Will never match, if the sequence in the compose buffer is longer
   * than the sequences in the table.  Further, compare_seq (key, val)
   * will overrun val if key is longer than val.
   */
  if (n_compose > table->max_seq_len)
    return FALSE;

  seq_index = bsearch (compose_buffer,
                       table->data,
                       table->n_index_size,
                       sizeof (guint16) * table->n_index_stride,
                       compare_seq_index);

  if (!seq_index)
    return FALSE;

  if (n_compose == 1)
    return TRUE;

  seq = NULL;
  match = FALSE;
  value = 0;

  for (i = n_compose - 1; i < table->max_seq_len; i++)
    {
      row_stride = i + 1;

      if (seq_index[i + 1] - seq_index[i] > 0)
        {
          seq = bsearch (compose_buffer + 1,
                         table->data + seq_index[i],
                         (seq_index[i + 1] - seq_index[i]) / row_stride,
                         sizeof (guint16) *  row_stride,
                         compare_seq);

          if (seq)
            {
              if (i == n_compose - 1)
                {
                  value = seq[row_stride - 1];
                  match = TRUE;
                }
              else
                {
                  if (output_char)
                    *output_char = value;
                  if (match)
                    {
                      if (compose_match)
                        *compose_match = TRUE;
                    }

                  return TRUE;
                }
            }
        }
    }

  if (match)
    {
      if (compose_match)
        *compose_match = TRUE;
      if (compose_finish)
        *compose_finish = TRUE;
      if (output_char)
        *output_char = value;

      return TRUE;
    }

  return FALSE;
}

/* Checks if a keysym is a dead key.
 * Dead key keysym values are defined in ../gdk/gdkkeysyms.h and the
 * first is GDK_KEY_dead_grave. As X.Org is updated, more dead keys
 * are added and we need to update the upper limit.
 */
#define IS_DEAD_KEY(k) \
    ((k) >= GDK_KEY_dead_grave && (k) <= GDK_KEY_dead_greek)

/* This function receives a sequence of Unicode characters and tries to
 * normalize it (NFC). We check for the case where the resulting string
 * has length 1 (single character).
 * NFC normalisation normally rearranges diacritic marks, unless these
 * belong to the same Canonical Combining Class.
 * If they belong to the same canonical combining class, we produce all
 * permutations of the diacritic marks, then attempt to normalize.
 */
static gboolean
check_normalize_nfc (gunichar *combination_buffer,
                     int       n_compose)
{
  gunichar *combination_buffer_temp;
  char *combination_utf8_temp = NULL;
  char *nfc_temp = NULL;
  int n_combinations;
  gunichar temp_swap;
  int i;

  combination_buffer_temp = g_alloca (n_compose * sizeof (gunichar));

  n_combinations = 1;

  for (i = 1; i < n_compose; i++)
     n_combinations *= i;

  /* Xorg reuses dead_tilde for the perispomeni diacritic mark.
   * We check if base character belongs to Greek Unicode block,
   * and if so, we replace tilde with perispomeni.
   */
  if (combination_buffer[0] >= 0x390 && combination_buffer[0] <= 0x3FF)
    {
      for (i = 1; i < n_compose; i++ )
        if (combination_buffer[i] == 0x303)
          combination_buffer[i] = 0x342;
    }

  memcpy (combination_buffer_temp, combination_buffer, n_compose * sizeof (gunichar) );

  for (i = 0; i < n_combinations; i++)
    {
      g_unicode_canonical_ordering (combination_buffer_temp, n_compose);
      combination_utf8_temp = g_ucs4_to_utf8 (combination_buffer_temp, n_compose, NULL, NULL, NULL);
      nfc_temp = g_utf8_normalize (combination_utf8_temp, -1, G_NORMALIZE_NFC);

      if (g_utf8_strlen (nfc_temp, -1) == 1)
        {
          memcpy (combination_buffer, combination_buffer_temp, n_compose * sizeof (gunichar) );

          g_free (combination_utf8_temp);
          g_free (nfc_temp);

          return TRUE;
        }

      g_free (combination_utf8_temp);
      g_free (nfc_temp);

      if (n_compose > 2)
        {
          temp_swap = combination_buffer_temp[i % (n_compose - 1) + 1];
          combination_buffer_temp[i % (n_compose - 1) + 1] = combination_buffer_temp[(i+1) % (n_compose - 1) + 1];
          combination_buffer_temp[(i+1) % (n_compose - 1) + 1] = temp_swap;
        }
      else
        break;
    }

  return FALSE;
}

gboolean
gtk_check_algorithmically (const guint16 *compose_buffer,
                           int            n_compose,
                           gunichar      *output_char)

{
  int i;
  gunichar *combination_buffer;
  char *combination_utf8, *nfc;

  combination_buffer = alloca (sizeof (gunichar) * (n_compose + 1));

  if (output_char)
    *output_char = 0;

  for (i = 0; i < n_compose && IS_DEAD_KEY (compose_buffer[i]); i++)
    ;

  /* Allow at most 2 dead keys */
  if (i > 2)
    return FALSE;

  /* Can't combine if there's no base character */
  if (i == n_compose)
    return TRUE;

  if (i > 0 && i == n_compose - 1)
    {
      combination_buffer[0] = gdk_keyval_to_unicode (compose_buffer[i]);
      combination_buffer[n_compose] = 0;
      i--;
      while (i >= 0)
        {
          switch (compose_buffer[i])
            {
#define CASE(keysym, unicode) \
            case GDK_KEY_dead_##keysym: combination_buffer[i+1] = unicode; break

            CASE (grave, 0x0300);
            CASE (acute, 0x0301);
            CASE (circumflex, 0x0302);
            CASE (tilde, 0x0303);       /* Also used with perispomeni, 0x342. */
            CASE (macron, 0x0304);
            CASE (breve, 0x0306);
            CASE (abovedot, 0x0307);
            CASE (diaeresis, 0x0308);
            CASE (abovering, 0x30A);
            CASE (hook, 0x0309);
            CASE (doubleacute, 0x030B);
            CASE (caron, 0x030C);
            CASE (cedilla, 0x0327);
            CASE (ogonek, 0x0328);      /* Legacy use for dasia, 0x314.*/
            CASE (iota, 0x0345);
            CASE (voiced_sound, 0x3099);        /* Per Markus Kuhn keysyms.txt file. */
            CASE (semivoiced_sound, 0x309A);    /* Per Markus Kuhn keysyms.txt file. */
            CASE (belowdot, 0x0323);
            CASE (horn, 0x031B);        /* Legacy use for psili, 0x313 (or 0x343). */
            CASE (stroke, 0x335);
            CASE (abovecomma, 0x0313);  /* Equivalent to psili */
            CASE (abovereversedcomma, 0x0314); /* Equivalent to dasia */
            CASE (doublegrave, 0x30F);
            CASE (belowring, 0x325);
            CASE (belowmacron, 0x331);
            CASE (belowcircumflex, 0x32D);
            CASE (belowtilde, 0x330);
            CASE (belowbreve, 0x32e);
            CASE (belowdiaeresis, 0x324);
            CASE (invertedbreve, 0x32f);
            CASE (belowcomma, 0x326);
            CASE (lowline, 0x332);
            CASE (aboveverticalline, 0x30D);
            CASE (belowverticalline, 0x329);
            CASE (longsolidusoverlay, 0x338);
            CASE (a, 0x363);
            CASE (A, 0x363);
            CASE (e, 0x364);
            CASE (E, 0x364);
            CASE (i, 0x365);
            CASE (I, 0x365);
            CASE (o, 0x366);
            CASE (O, 0x366);
            CASE (u, 0x367);
            CASE (U, 0x367);
            CASE (small_schwa, 0x1DEA);
            CASE (capital_schwa, 0x1DEA);
#undef CASE
            default:
              combination_buffer[i+1] = gdk_keyval_to_unicode (compose_buffer[i]);
            }
          i--;
        }

      /* If the buffer normalizes to a single character, then modify the order
       * of combination_buffer accordingly, if necessary, and return TRUE.
       */
      if (check_normalize_nfc (combination_buffer, n_compose))
        {
          combination_utf8 = g_ucs4_to_utf8 (combination_buffer, -1, NULL, NULL, NULL);
          nfc = g_utf8_normalize (combination_utf8, -1, G_NORMALIZE_NFC);

          if (output_char)
            *output_char = g_utf8_get_char (nfc);

          g_free (combination_utf8);
          g_free (nfc);

          return TRUE;
        }
    }

  return FALSE;
}

