/* gtkbuilderparser.c
 * Copyright (C) 2019 Red Hat,
 *                         Alexander Larsson <alexander.larsson@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gio/gio.h>
#include "gtkbuilderprivate.h"
#include "gtkbuilder.h"
#include "gtkbuildableprivate.h"

/*****************************************  Record a GMarkup parser call ***************************/

typedef enum
{
 RECORD_TYPE_ELEMENT,
 RECORD_TYPE_END_ELEMENT,
 RECORD_TYPE_TEXT,
} RecordTreeType;

typedef struct {
  char *string;
  int count;
  int offset;
} RecordDataString;

typedef struct RecordDataTree RecordDataTree;

/* All strings are owned by the string table */
struct RecordDataTree {
  RecordDataTree *parent;
  RecordTreeType type;
  int n_attributes;
  RecordDataString *data;
  RecordDataString **attributes;
  RecordDataString **values;
  GList *children;
};

static RecordDataTree *
record_data_tree_new (RecordDataTree   *parent,
                      RecordTreeType    type,
                      RecordDataString *data)
{
  RecordDataTree *tree = g_slice_new0 (RecordDataTree);

  tree->parent = parent;
  tree->type = type;
  tree->data = data;

  if (parent)
    parent->children = g_list_prepend  (parent->children, tree);

  return tree;
}

static void
record_data_tree_free (RecordDataTree *tree)
{
  g_list_free_full (tree->children, (GDestroyNotify)record_data_tree_free);
  g_free (tree->attributes);
  g_free (tree->values);
  g_slice_free (RecordDataTree, tree);
}

static void
record_data_string_free (RecordDataString *s)
{
  g_free (s->string);
  g_slice_free (RecordDataString, s);
}

static RecordDataString *
record_data_string_lookup (GHashTable *strings,
                           const char *str,
                           gssize      len)
{
  char *copy = NULL;
  RecordDataString *s;

  if (len >= 0)
    {
      /* Ensure str is zero terminated */
      copy = g_strndup (str, len);
      str = copy;
    }

  s = g_hash_table_lookup (strings, str);
  if (s)
    {
      g_free (copy);
      s->count++;
      return s;
    }

  s = g_slice_new (RecordDataString);
  s->string = copy ? copy : g_strdup (str);
  s->count = 1;

  g_hash_table_insert (strings, s->string, s);
  return s;
}

typedef struct {
  GHashTable *strings;
  RecordDataTree *root;
  RecordDataTree *current;
} RecordData;

static void
record_start_element (GMarkupParseContext  *context,
                      const char           *element_name,
                      const char          **names,
                      const char          **values,
                      gpointer              user_data,
                      GError              **error)
{
  gsize n_attrs = g_strv_length ((char **)names);
  RecordData *data = user_data;
  RecordDataTree *child;
  int i;

  child = record_data_tree_new (data->current, RECORD_TYPE_ELEMENT,
                                record_data_string_lookup (data->strings, element_name, -1));
  data->current = child;

  child->n_attributes = n_attrs;
  child->attributes = g_new (RecordDataString *, n_attrs);
  child->values = g_new (RecordDataString *, n_attrs);

  for (i = 0; i < n_attrs; i++)
    {
      child->attributes[i] = record_data_string_lookup (data->strings, names[i], -1);
      child->values[i] = record_data_string_lookup (data->strings, values[i], -1);
    }
}

static void
record_end_element (GMarkupParseContext  *context,
                    const char           *element_name,
                    gpointer              user_data,
                    GError              **error)
{
  RecordData *data = user_data;

  data->current = data->current->parent;
}

static void
record_text (GMarkupParseContext  *context,
             const char           *text,
             gsize                 text_len,
             gpointer              user_data,
             GError              **error)
{
  RecordData *data = user_data;

  record_data_tree_new (data->current, RECORD_TYPE_TEXT,
                        record_data_string_lookup (data->strings, text, text_len));
}

static const GMarkupParser record_parser =
{
  record_start_element,
  record_end_element,
  record_text,
  NULL, // passthrough, not stored
  NULL, // error, fails immediately
};

static int
compare_string (gconstpointer _a,
                gconstpointer _b)
{
  const RecordDataString *a = _a;
  const RecordDataString *b = _b;

  return b->count - a->count;
}

static void
marshal_uint32 (GString *str,
                guint32  v)
{
  /*
    We encode in a variable length format similar to
    utf8:

    v size      byte 1    byte 2    byte 3   byte 4  byte 5
    7 bit:    0xxxxxxx
    14 bit:   10xxxxxx  xxxxxxxx
    21 bit:   110xxxxx  xxxxxxxx  xxxxxxxx
    28 bit:   1110xxxx  xxxxxxxx  xxxxxxxx xxxxxxxx
    32 bit:   11110000  xxxxxxxx  xxxxxxxx xxxxxxxx xxxxxxx
  */

  if (v < 128)
    {
      g_string_append_c (str, (guchar)v);
    }
  else if (v < (1<<14))
    {
      g_string_append_c (str, (guchar)(v >> 8) | 0x80);
      g_string_append_c (str, (guchar)(v & 0xff));
    }
  else if (v < (1<<21))
    {
      g_string_append_c (str, (guchar)(v >> 16) | 0xc0);
      g_string_append_c (str, (guchar)((v >> 8) & 0xff));
      g_string_append_c (str, (guchar)(v & 0xff));
    }
  else if (v < (1<<28))
    {
      g_string_append_c (str, (guchar)(v >> 24) | 0xe0);
      g_string_append_c (str, (guchar)((v >> 16) & 0xff));
      g_string_append_c (str, (guchar)((v >> 8) & 0xff));
      g_string_append_c (str, (guchar)(v & 0xff));
    }
  else
    {
      g_string_append_c (str, 0xf0);
      g_string_append_c (str, (guchar)((v >> 24) & 0xff));
      g_string_append_c (str, (guchar)((v >> 16) & 0xff));
      g_string_append_c (str, (guchar)((v >> 8) & 0xff));
      g_string_append_c (str, (guchar)(v & 0xff));
    }
}

static void
marshal_tree (GString        *marshaled,
              RecordDataTree *tree)
{
  GList *l;
  int i;

  /* Special case the root */
  if (tree->parent == NULL)
    {
      for (l = g_list_last (tree->children); l != NULL; l = l->prev)
        marshal_tree (marshaled, l->data);
      return;
    }

  switch (tree->type)
    {
    case RECORD_TYPE_ELEMENT:
      marshal_uint32 (marshaled, RECORD_TYPE_ELEMENT);
      marshal_uint32 (marshaled, tree->data->offset);
      marshal_uint32 (marshaled, tree->n_attributes);
      for (i = 0; i < tree->n_attributes; i++)
        {
          marshal_uint32 (marshaled, tree->attributes[i]->offset);
          marshal_uint32 (marshaled, tree->values[i]->offset);
        }
      for (l = g_list_last (tree->children); l != NULL; l = l->prev)
        marshal_tree (marshaled, l->data);

      marshal_uint32 (marshaled, RECORD_TYPE_END_ELEMENT);
      break;
    case RECORD_TYPE_TEXT:
      marshal_uint32 (marshaled, RECORD_TYPE_TEXT);
      marshal_uint32 (marshaled, tree->data->offset);
      break;
    case RECORD_TYPE_END_ELEMENT:
    default:
      g_assert_not_reached ();
    }
}

/**
 * _gtk_buildable_parser_precompile:
 * @text: chunk of text to parse
 * @text_len: length of @text in bytes
 *
 * Converts the xml format typically used by GtkBuilder to a
 * binary form that is more efficient to parse. This is a custom
 * format that is only supported by GtkBuilder.
 *
 * returns: A `GBytes` with the precompiled data
 **/
GBytes *
_gtk_buildable_parser_precompile (const char  *text,
                                  gssize       text_len,
                                  GError     **error)
{
  GMarkupParseContext *ctx;
  RecordData data = { 0 };
  GList *string_table, *l;
  GString *marshaled;
  int offset;

  data.strings = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify)record_data_string_free);
  data.root = record_data_tree_new (NULL, RECORD_TYPE_ELEMENT, NULL);
  data.current = data.root;

  ctx = g_markup_parse_context_new (&record_parser, G_MARKUP_TREAT_CDATA_AS_TEXT, &data, NULL);

  if (!g_markup_parse_context_parse (ctx, text, text_len, error) ||
      !g_markup_parse_context_end_parse (ctx, error))
    {
      record_data_tree_free (data.root);
      g_hash_table_destroy (data.strings);
      g_markup_parse_context_free (ctx);
      return NULL;
    }

  g_markup_parse_context_free (ctx);

  string_table = g_hash_table_get_values (data.strings);

  string_table = g_list_sort (string_table, compare_string);

  offset = 0;
  for (l = string_table; l != NULL; l = l->next)
    {
      RecordDataString *s = l->data;
      s->offset = offset;
      offset += strlen (s->string) + 1;
    }

  marshaled = g_string_new ("");
  /* Magic marker */
  g_string_append_len (marshaled, "GBU\0", 4);
  marshal_uint32 (marshaled, offset);

  for (l = string_table; l != NULL; l = l->next)
    {
      RecordDataString *s = l->data;
      g_string_append_len (marshaled, s->string, strlen (s->string) + 1);
    }

  g_list_free (string_table);

  marshal_tree (marshaled, data.root);

  record_data_tree_free (data.root);
  g_hash_table_destroy (data.strings);

  return g_string_free_to_bytes (marshaled);
}

/*****************************************  Replay GMarkup parser callbacks ***************************/

static guint32
demarshal_uint32 (const char **tree)
{
  const guchar *p = (const guchar *)*tree;
  guchar c = *p;
  /* see marshal_uint32 for format */

  if (c < 128) /* 7 bit */
    {
      *tree += 1;
      return c;
    }
  else if ((c & 0xc0) == 0x80) /* 14 bit */
    {
      *tree += 2;
      return (c & 0x3f) << 8 | p[1];
    }
  else if ((c & 0xe0) == 0xc0) /* 21 bit */
    {
      *tree += 3;
      return (c & 0x1f) << 16 | p[1] << 8 | p[2];
    }
  else if ((c & 0xf0) == 0xe0) /* 28 bit */
    {
      *tree += 4;
      return (c & 0xf) << 24 | p[1] << 16 | p[2] << 8 | p[3];
    }
  else
    {
      *tree += 5;
      return p[1] << 24 | p[2] << 16 | p[3] << 8 | p[4];
    }
}

static const char *
demarshal_string (const char **tree,
                  const char  *strings)
{
  guint32 offset = demarshal_uint32 (tree);

  return strings + offset;
}

static void
propagate_error (GtkBuildableParseContext *context,
                 GError                  **dest,
                 GError                   *src)
{
  (*context->internal_callbacks->error) (NULL, src, context);
  g_propagate_error (dest, src);
}

static gboolean
replay_start_element (GtkBuildableParseContext  *context,
                      const char               **tree,
                      const char                *strings,
                      GError                   **error)
{
  const char *element_name;
  guint32 i, n_attrs;
  const char **attr_names;
  const char **attr_values;
  GError *tmp_error = NULL;

  element_name = demarshal_string (tree, strings);
  n_attrs = demarshal_uint32 (tree);

  attr_names = g_newa (const char *, n_attrs + 1);
  attr_values = g_newa (const char *, n_attrs + 1);
  for (i = 0; i < n_attrs; i++)
    {
      attr_names[i] = demarshal_string (tree, strings);
      attr_values[i] = demarshal_string (tree, strings);
    }
  attr_names[i] = NULL;
  attr_values[i] = NULL;

  (* context->internal_callbacks->start_element) (NULL,
                                                  element_name,
                                                  attr_names,
                                                  attr_values,
                                                  context,
                                                  &tmp_error);

  if (tmp_error)
    {
      propagate_error (context, error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

static gboolean
replay_end_element (GtkBuildableParseContext  *context,
                    const char               **tree,
                    const char                *strings,
                    GError                   **error)
{
  GError *tmp_error = NULL;

  (* context->internal_callbacks->end_element) (NULL,
                                                gtk_buildable_parse_context_get_element (context),
                                                context,
                                                &tmp_error);
  if (tmp_error)
    {
      propagate_error (context, error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

static gboolean
replay_text (GtkBuildableParseContext  *context,
             const char               **tree,
             const char                *strings,
             GError                   **error)
{
  const char *text;
  GError *tmp_error = NULL;

  text = demarshal_string (tree, strings);

  (*context->internal_callbacks->text) (NULL,
                                        text,
                                        strlen (text),
                                        context,
                                        &tmp_error);

  if (tmp_error)
    {
      propagate_error (context, error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

gboolean
_gtk_buildable_parser_is_precompiled (const char *data,
                                      gssize      data_len)
{
  return
    data_len > 4 &&
    data[0] == 'G' &&
    data[1] == 'B' &&
    data[2] == 'U' &&
    data[3] == 0;
}

gboolean
_gtk_buildable_parser_replay_precompiled (GtkBuildableParseContext  *context,
                                          const char                *data,
                                          gssize                     data_len,
                                          GError                   **error)
{
  const char *data_end = data + data_len;
  guint32 type, len;
  const char *strings;
  const char *tree;

  data = data + 4; /* Skip header */

  len = demarshal_uint32 (&data);

  strings = data;
  data = data + len;
  tree = data;

  while (tree < data_end)
    {
      gboolean res;
      type = demarshal_uint32 (&tree);

      switch (type)
        {
        case RECORD_TYPE_ELEMENT:
          res = replay_start_element (context, &tree, strings, error);
          break;
        case RECORD_TYPE_END_ELEMENT:
          res = replay_end_element (context, &tree, strings, error);
          break;
        case RECORD_TYPE_TEXT:
          res = replay_text (context, &tree, strings, error);
          break;
        default:
          g_assert_not_reached ();
        }

      if (!res)
        return FALSE;
    }

  return TRUE;
}
