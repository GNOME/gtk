/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gtkstyleprovider.h>

#include "gtkcssprovider.h"

#include "gtkalias.h"

typedef struct GtkCssProviderPrivate GtkCssProviderPrivate;
typedef struct SelectorElement SelectorElement;
typedef struct SelectorPath SelectorPath;
typedef struct SelectorStyleInfo SelectorStyleInfo;
typedef enum SelectorElementType SelectorElementType;
typedef enum ParserScope ParserScope;

enum SelectorElementType {
  SELECTOR_TYPE_NAME,
  SELECTOR_NAME,
  SELECTOR_GTYPE
};

struct SelectorElement
{
  SelectorElementType elem_type;
  GtkStateType state;

  union
  {
    GQuark name;
    GType type;
  };
};

struct SelectorPath
{
  GSList *elements;
  guint ref_count;
};

struct SelectorStyleInfo
{
  SelectorPath *path;
  GHashTable *style;
};

struct GtkCssProviderPrivate
{
  GScanner *scanner;
  GPtrArray *selectors_info;

  /* Current parser state */
  GSList *state;
  GSList *cur_selectors;
  GHashTable *cur_properties;
};

enum ParserScope {
  SCOPE_SELECTOR,
  SCOPE_DECLARATION,
  SCOPE_VALUE
};

#define GTK_CSS_PROVIDER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_CSS_PROVIDER, GtkCssProviderPrivate))

static void gtk_css_provider_finalize (GObject *object);
static void gtk_css_style_provider_iface_init (GtkStyleProviderIface *iface);

static void css_provider_apply_scope (GtkCssProvider *css_provider,
                                      ParserScope     scope);

G_DEFINE_TYPE_EXTENDED (GtkCssProvider, gtk_css_provider, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_css_style_provider_iface_init));

static void
gtk_css_provider_class_init (GtkCssProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_provider_finalize;

  g_type_class_add_private (object_class, sizeof (GtkCssProviderPrivate));
}

static SelectorPath *
selector_path_new (void)
{
  SelectorPath *path;

  path = g_slice_new0 (SelectorPath);
  path->ref_count = 1;

  return path;
}

static SelectorPath *
selector_path_ref (SelectorPath *path)
{
  path->ref_count++;
  return path;
}

static void
selector_path_unref (SelectorPath *path)
{
  path->ref_count--;

  if (path->ref_count > 0)
    return;

  while (path->elements)
    {
      g_slice_free (SelectorPath, path->elements->data);
      path->elements = g_slist_delete_link (path->elements, path->elements);
    }

  g_slice_free (SelectorPath, path);
}

static void
selector_path_prepend_type (SelectorPath *path,
                            const gchar  *type_name)
{
  SelectorElement *elem;
  GType type;

  elem = g_slice_new (SelectorElement);
  type = g_type_from_name (type_name);

  if (type == G_TYPE_INVALID)
    {
      elem->elem_type = SELECTOR_TYPE_NAME;
      elem->name = g_quark_from_string (type_name);
    }
  else
    {
      elem->elem_type = SELECTOR_GTYPE;
      elem->type = type;
    }

  path->elements = g_slist_prepend (path->elements, elem);
}

static SelectorStyleInfo *
selector_style_info_new (SelectorPath *path)
{
  SelectorStyleInfo *info;

  info = g_slice_new0 (SelectorStyleInfo);
  info->path = selector_path_ref (path);

  return info;
}

static void
selector_style_info_free (SelectorStyleInfo *info)
{
  if (info->style)
    g_hash_table_unref (info->style);

  if (info->path)
    selector_path_unref (info->path);
}

static void
selector_style_info_set_style (SelectorStyleInfo *info,
                               GHashTable        *style)
{
  if (info->style)
    g_hash_table_unref (info->style);

  if (style)
    info->style = g_hash_table_ref (style);
  else
    info->style = NULL;
}

static void
gtk_css_provider_init (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv;
  GScanner *scanner;

  priv = GTK_CSS_PROVIDER_GET_PRIVATE (css_provider);
  priv->selectors_info = g_ptr_array_new_with_free_func ((GDestroyNotify) selector_style_info_free);

  scanner = g_scanner_new (NULL);
  /* scanner->input_name = path; */

  priv->scanner = scanner;
  css_provider_apply_scope (css_provider, SCOPE_SELECTOR);
}

typedef struct ComparePathData ComparePathData;

struct ComparePathData
{
  guint64 score;
  SelectorPath *path;
  GSList *iter;
};

static gboolean
compare_path_foreach (GType        type,
                      const gchar *name,
                      gpointer     user_data)
{
  ComparePathData *data;
  SelectorElement *elem;

  data = user_data;
  elem = data->iter->data;

  if (elem->elem_type == SELECTOR_TYPE_NAME)
    {
      const gchar *type_name;
      GType resolved_type;

      /* Resolve the type name */
      type_name = g_quark_to_string (elem->name);
      resolved_type = g_type_from_name (type_name);

      if (resolved_type == G_TYPE_INVALID)
        {
          /* Type couldn't be resolved, so the selector
           * clearly doesn't affect the given widget path
           */
          data->score = 0;
          return TRUE;
        }

      elem->elem_type = SELECTOR_GTYPE;
      elem->type = resolved_type;
    }

  if (elem->elem_type == SELECTOR_GTYPE)
    {
      if (!g_type_is_a (type, elem->type))
        {
          /* Selector definitely doesn't match */
          data->score = 0;
          return TRUE;
        }
      else if (type == elem->type)
        data->score |= 0xF;
      else
        {
          GType parent = type;
          guint8 score = 0xE;

          while ((parent = g_type_parent (parent)) != G_TYPE_INVALID)
            {
              if (parent == elem->type)
                break;

              score--;

              if (score == 1)
                {
                  g_warning ("Hierarchy is higher than expected.");
                  break;
                }
            }

          data->score |= score;
        }
    }

  data->iter = data->iter->next;

  if (data->iter)
    {
      data->score <<= 4;
      return FALSE;
    }

  return TRUE;
}

static guint64
compare_selector (GtkWidgetPath *path,
                  SelectorPath  *selector)
{
  ComparePathData data;

  data.score = 0;
  data.path = selector;
  data.iter = selector->elements;

  gtk_widget_path_foreach (path,
                           compare_path_foreach,
                           &data);

  if (data.iter)
    {
      /* There is remaining data to compare,
       * so don't take it as a match.
       */
      data.score = 0;
    }

  return data.score;
}

typedef struct StylePriorityInfo StylePriorityInfo;

struct StylePriorityInfo
{
  guint64 score;
  GHashTable *style;
};

static GtkStyleSet *
gtk_style_get_style (GtkStyleProvider *provider,
                     GtkWidgetPath    *path)
{
  GtkCssProviderPrivate *priv;
  GtkStyleSet *set;
  GArray *priority_info;
  guint i, j;

  priv = GTK_CSS_PROVIDER_GET_PRIVATE (provider);
  set = gtk_style_set_new ();
  priority_info = g_array_new (FALSE, FALSE, sizeof (StylePriorityInfo));

  for (i = 0; i < priv->selectors_info->len; i++)
    {
      SelectorStyleInfo *info;
      StylePriorityInfo new;
      gboolean added = FALSE;
      guint64 score;

      info = g_ptr_array_index (priv->selectors_info, i);
      score = compare_selector (path, info->path);

      if (score <= 0)
        continue;

      new.score = score;
      new.style = info->style;

      for (j = 0; j < priority_info->len; j++)
        {
          StylePriorityInfo *cur;

          cur = &g_array_index (priority_info, StylePriorityInfo, j);

          if (cur->score > new.score)
            {
              g_array_insert_val (priority_info, j, new);
              added = TRUE;
              break;
            }
        }

      if (!added)
        g_array_append_val (priority_info, new);
    }

  for (i = 0; i < priority_info->len; i++)
    {
      StylePriorityInfo *info;
      GHashTableIter iter;
      gpointer key, value;

      info = &g_array_index (priority_info, StylePriorityInfo, i);
      g_hash_table_iter_init (&iter, info->style);

      while (g_hash_table_iter_next (&iter, &key, &value))
        gtk_style_set_set_property (set, key, GTK_STATE_NORMAL, value);
    }

  g_array_free (priority_info, TRUE);

  return set;
}

static void
gtk_css_style_provider_iface_init (GtkStyleProviderIface *iface)
{
  iface->get_style = gtk_style_get_style;
}

static void
gtk_css_provider_finalize (GObject *object)
{
  GtkCssProviderPrivate *priv;

  priv = GTK_CSS_PROVIDER_GET_PRIVATE (object);
  g_scanner_destroy (priv->scanner);

  g_ptr_array_free (priv->selectors_info, TRUE);

  g_slist_foreach (priv->cur_selectors, (GFunc) selector_path_unref, NULL);
  g_slist_free (priv->cur_selectors);

  g_hash_table_unref (priv->cur_properties);

  G_OBJECT_CLASS (gtk_css_provider_parent_class)->finalize (object);
}

GtkCssProvider *
gtk_css_provider_new (void)
{
  return g_object_new (GTK_TYPE_CSS_PROVIDER, NULL);
}

static void
property_value_free (GValue *value)
{
  if (G_IS_VALUE (value))
    g_value_unset (value);

  g_slice_free (GValue, value);
}

static void
css_provider_apply_scope (GtkCssProvider *css_provider,
                          ParserScope     scope)
{
  GtkCssProviderPrivate *priv;

  priv = GTK_CSS_PROVIDER_GET_PRIVATE (css_provider);

  g_scanner_set_scope (priv->scanner, scope);

  if (scope == SCOPE_VALUE)
    {
      priv->scanner->config->cset_identifier_first = G_CSET_a_2_z "#-_0123456789" G_CSET_A_2_Z;
      priv->scanner->config->cset_identifier_nth = G_CSET_a_2_z "#-_ 0123456789" G_CSET_A_2_Z;
    }
  else
    {
      priv->scanner->config->cset_identifier_first = G_CSET_a_2_z G_CSET_A_2_Z;
      priv->scanner->config->cset_identifier_nth = G_CSET_a_2_z "-" G_CSET_A_2_Z;
    }

  priv->scanner->config->scan_float = FALSE;
}

static void
css_provider_push_scope (GtkCssProvider *css_provider,
                         ParserScope     scope)
{
  GtkCssProviderPrivate *priv;

  priv = GTK_CSS_PROVIDER_GET_PRIVATE (css_provider);
  priv->state = g_slist_prepend (priv->state, GUINT_TO_POINTER (scope));

  css_provider_apply_scope (css_provider, scope);
}

static ParserScope
css_provider_pop_scope (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv;
  ParserScope scope = SCOPE_SELECTOR;

  priv = GTK_CSS_PROVIDER_GET_PRIVATE (css_provider);

  if (!priv->state)
    {
      g_warning ("Push/pop calls to parser scope aren't paired");
      css_provider_apply_scope (css_provider, SCOPE_SELECTOR);
      return SCOPE_SELECTOR;
    }

  priv->state = g_slist_delete_link (priv->state, priv->state);

  /* Fetch new scope */
  if (priv->state)
    scope = GPOINTER_TO_INT (priv->state->data);

  css_provider_apply_scope (css_provider, scope);

  return scope;
}

static void
css_provider_reset_parser (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv;

  priv = GTK_CSS_PROVIDER_GET_PRIVATE (css_provider);

  g_slist_free (priv->state);
  priv->state = NULL;

  css_provider_apply_scope (css_provider, SCOPE_SELECTOR);

  g_slist_foreach (priv->cur_selectors, (GFunc) selector_path_unref, NULL);
  g_slist_free (priv->cur_selectors);
  priv->cur_selectors = NULL;

  if (priv->cur_properties)
    g_hash_table_unref (priv->cur_properties);

  priv->cur_properties = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                (GDestroyNotify) g_free,
                                                (GDestroyNotify) property_value_free);
}

static void
css_provider_commit (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv;
  GSList *l;

  priv = GTK_CSS_PROVIDER_GET_PRIVATE (css_provider);
  l = priv->cur_selectors;

  while (l)
    {
      SelectorPath *path = l->data;
      SelectorStyleInfo *info;

      info = selector_style_info_new (path);
      selector_style_info_set_style (info, priv->cur_properties);

      g_ptr_array_add (priv->selectors_info, info);
      l = l->next;
    }
}

static GTokenType
parse_selector (GScanner      *scanner,
                SelectorPath **selector_out)
{
  SelectorPath *path;

  *selector_out = NULL;

  if (scanner->token != G_TOKEN_IDENTIFIER)
    return G_TOKEN_IDENTIFIER;

  path = selector_path_new ();

  if (g_ascii_isupper (scanner->value.v_identifier[0]))
    {
      while (scanner->token == G_TOKEN_IDENTIFIER &&
             g_ascii_isupper (scanner->value.v_identifier[0]))
        {
          selector_path_prepend_type (path, scanner->value.v_identifier);
          g_scanner_get_next_token (scanner);
        }

      if (scanner->token == '.')
        {
          /* style class scanning */
          g_scanner_get_next_token (scanner);

          if (scanner->token != G_TOKEN_IDENTIFIER)
            {
              selector_path_unref (path);
              return G_TOKEN_IDENTIFIER;
            }

          g_print ("Style class: %s\n", scanner->value.v_identifier);
          g_scanner_get_next_token (scanner);
        }
    }
  else
    {
      selector_path_unref (path);
      return G_TOKEN_IDENTIFIER;
    }

  *selector_out = path;

  return G_TOKEN_NONE;
}

static gboolean
parse_value (GType        type,
             const gchar *value_str,
             GValue      *value)
{
  g_value_init (value, type);

  if (type == GDK_TYPE_COLOR)
    {
      GdkColor color;

      if (gdk_color_parse (value_str, &color) == FALSE)
        return FALSE;

      g_value_set_boxed (value, &color);
      return TRUE;
    }
  else if (type == PANGO_TYPE_FONT_DESCRIPTION)
    {
      PangoFontDescription *font_desc;

      font_desc = pango_font_description_from_string (value_str);
      g_value_take_boxed (value, font_desc);

      return TRUE;
    }
  else if (type == G_TYPE_INT)
    {
      g_value_set_int (value, atoi (value_str));
      return TRUE;
    }
  else if (type == G_TYPE_DOUBLE)
    {
      g_value_set_double (value, g_ascii_strtod (value_str, NULL));
      return TRUE;
    }
  else
    {
      g_warning ("Cannot parse string '%s' for type %s", value_str, g_type_name (type));
    }

  return FALSE;
}

static GTokenType
parse_rule (GtkCssProvider *css_provider,
            GScanner       *scanner)
{
  GtkCssProviderPrivate *priv;
  GTokenType expected_token;
  SelectorPath *selector;

  priv = GTK_CSS_PROVIDER_GET_PRIVATE (css_provider);

  css_provider_push_scope (css_provider, SCOPE_SELECTOR);
  expected_token = parse_selector (scanner, &selector);

  if (expected_token != G_TOKEN_NONE)
    return expected_token;

  priv->cur_selectors = g_slist_prepend (priv->cur_selectors, selector);

  while (scanner->token == ',')
    {
      g_scanner_get_next_token (scanner);

      expected_token = parse_selector (scanner, &selector);

      if (expected_token != G_TOKEN_NONE)
        return expected_token;

      priv->cur_selectors = g_slist_prepend (priv->cur_selectors, selector);
    }

  css_provider_pop_scope (css_provider);

  if (scanner->token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  /* Declarations parsing */
  css_provider_push_scope (css_provider, SCOPE_DECLARATION);
  g_scanner_get_next_token (scanner);

  while (scanner->token == G_TOKEN_IDENTIFIER)
    {
      const gchar *value_str = NULL;
      GValue value = { 0 };
      GType prop_type;
      gchar *prop;

      prop = g_strdup (scanner->value.v_identifier);
      g_scanner_get_next_token (scanner);

      if (scanner->token != ':')
        {
          g_free (prop);
          return ':';
        }

      css_provider_push_scope (css_provider, SCOPE_VALUE);
      g_scanner_get_next_token (scanner);

      if (scanner->token != G_TOKEN_IDENTIFIER)
        {
          g_free (prop);
          return G_TOKEN_IDENTIFIER;
        }

      value_str = g_strstrip (scanner->value.v_identifier);

      if (gtk_style_set_lookup_property (prop, &prop_type, NULL) &&
          parse_value (prop_type, value_str, &value))
        {
          GValue *val;

          val = g_slice_new0 (GValue);
          g_value_init (val, prop_type);
          g_value_copy (&value, val);

          g_hash_table_insert (priv->cur_properties, prop, val);
        }
      else
        g_free (prop);

      css_provider_pop_scope (css_provider);
      g_scanner_get_next_token (scanner);

      if (scanner->token != ';')
        return ';';

      g_scanner_get_next_token (scanner);
    }

  if (scanner->token != G_TOKEN_RIGHT_CURLY)
    return G_TOKEN_RIGHT_CURLY;

  css_provider_pop_scope (css_provider);

  return G_TOKEN_NONE;
}

static gboolean
parse_stylesheet (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv;

  priv = GTK_CSS_PROVIDER_GET_PRIVATE (css_provider);
  g_scanner_get_next_token (priv->scanner);

  while (!g_scanner_eof (priv->scanner))
    {
      GTokenType expected_token;

      css_provider_reset_parser (css_provider);
      expected_token = parse_rule (css_provider, priv->scanner);

      if (expected_token != G_TOKEN_NONE)
        {
          g_scanner_unexp_token (priv->scanner, expected_token,
                                 NULL, NULL, NULL,
                                 "Error parsing style resource", FALSE);

          while (!g_scanner_eof (priv->scanner) &&
                 priv->scanner->token != G_TOKEN_RIGHT_CURLY)
            g_scanner_get_next_token (priv->scanner);
        }
      else
        css_provider_commit (css_provider);

      g_scanner_get_next_token (priv->scanner);
    }

  return TRUE;
}

gboolean
gtk_css_provider_load_from_data (GtkCssProvider *css_provider,
                                 const gchar    *data,
                                 gssize          length,
                                 GError         *error)
{
  GtkCssProviderPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_PROVIDER (css_provider), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  priv = GTK_CSS_PROVIDER_GET_PRIVATE (css_provider);

  if (length < 0)
    length = strlen (data);

  if (priv->selectors_info->len > 0)
    g_ptr_array_remove_range (priv->selectors_info, 0, priv->selectors_info->len);

  css_provider_reset_parser (css_provider);
  g_scanner_input_text (priv->scanner, data, (guint) length);

  parse_stylesheet (css_provider);

  return TRUE;
}

gboolean
gtk_css_provider_load_from_file (GtkCssProvider  *css_provider,
                                 GFile           *file,
                                 GError         **error)
{
  GtkCssProviderPrivate *priv;
  GError *internal_error = NULL;
  gchar *data;
  gsize length;

  g_return_val_if_fail (GTK_IS_CSS_PROVIDER (css_provider), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  priv = GTK_CSS_PROVIDER_GET_PRIVATE (css_provider);

  if (!g_file_load_contents (file, NULL,
                             &data, &length,
                             NULL, &internal_error))
    {
      g_propagate_error (error, internal_error);
      return FALSE;
    }

  if (priv->selectors_info->len > 0)
    g_ptr_array_remove_range (priv->selectors_info, 0, priv->selectors_info->len);

  css_provider_reset_parser (css_provider);
  g_scanner_input_text (priv->scanner, data, (guint) length);

  parse_stylesheet (css_provider);

  g_free (data);

  return TRUE;
}

#define __GTK_CSS_PROVIDER_C__
#include "gtkaliasdef.c"
