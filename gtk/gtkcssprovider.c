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
typedef enum CombinatorType CombinatorType;
typedef enum ParserScope ParserScope;
typedef enum ParserSymbol ParserSymbol;

enum SelectorElementType {
  SELECTOR_TYPE_NAME,
  SELECTOR_NAME,
  SELECTOR_GTYPE,
  SELECTOR_REGION,
  SELECTOR_GLOB
};

enum CombinatorType {
  COMBINATOR_DESCENDANT, /* No direct relation needed */
  COMBINATOR_CHILD       /* Direct child */
};

struct SelectorElement
{
  SelectorElementType elem_type;
  CombinatorType combinator;

  union
  {
    GQuark name;
    GType type;

    struct
    {
      GQuark name;
      GtkChildClassFlags flags;
    } region;
  };
};

struct SelectorPath
{
  GSList *elements;
  GtkStateType state;
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
  gchar *filename;

  GHashTable *symbolic_colors;

  GPtrArray *selectors_info;

  /* Current parser state */
  GSList *state;
  GSList *cur_selectors;
  GHashTable *cur_properties;
};

enum ParserScope {
  SCOPE_SELECTOR,
  SCOPE_PSEUDO_CLASS,
  SCOPE_NTH_CHILD,
  SCOPE_DECLARATION,
  SCOPE_VALUE
};

/* Extend GtkStateType, since these
 * values are also used as symbols
 */
enum ParserSymbol {
  /* Scope: pseudo-class */
  SYMBOL_NTH_CHILD = GTK_STATE_LAST,
  SYMBOL_FIRST_CHILD,
  SYMBOL_LAST_CHILD,

  /* Scope: nth-child */
  SYMBOL_NTH_CHILD_EVEN,
  SYMBOL_NTH_CHILD_ODD,
  SYMBOL_NTH_CHILD_FIRST,
  SYMBOL_NTH_CHILD_LAST
};

static void gtk_css_provider_finalize (GObject *object);
static void gtk_css_style_provider_iface_init (GtkStyleProviderIface *iface);

static void css_provider_apply_scope (GtkCssProvider *css_provider,
                                      ParserScope     scope);
static gboolean css_provider_parse_value (const gchar *value_str,
                                          GValue      *value);

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
  path->state = GTK_STATE_NORMAL;
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
      g_slice_free (SelectorElement, path->elements->data);
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
  elem->combinator = COMBINATOR_DESCENDANT;
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

static void
selector_path_prepend_glob (SelectorPath *path)
{
  SelectorElement *elem;

  elem = g_slice_new (SelectorElement);
  elem->elem_type = SELECTOR_GLOB;
  elem->combinator = COMBINATOR_DESCENDANT;

  path->elements = g_slist_prepend (path->elements, elem);
}

static void
selector_path_prepend_region (SelectorPath       *path,
                              const gchar        *name,
                              GtkChildClassFlags  flags)
{
  SelectorElement *elem;

  elem = g_slice_new (SelectorElement);
  elem->combinator = COMBINATOR_DESCENDANT;
  elem->elem_type = SELECTOR_REGION;

  elem->region.name = g_quark_from_string (name);
  elem->region.flags = flags;

  path->elements = g_slist_prepend (path->elements, elem);
}

static void
selector_path_prepend_combinator (SelectorPath   *path,
                                  CombinatorType  combinator)
{
  SelectorElement *elem;

  g_assert (path->elements != NULL);

  /* It is actually stored in the last element */
  elem = path->elements->data;
  elem->combinator = combinator;
}

static gint
selector_path_depth (SelectorPath *path)
{
  return g_slist_length (path->elements);
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

  priv = css_provider->priv = G_TYPE_INSTANCE_GET_PRIVATE (css_provider,
                                                           GTK_TYPE_CSS_PROVIDER,
                                                           GtkCssProviderPrivate);

  priv->selectors_info = g_ptr_array_new_with_free_func ((GDestroyNotify) selector_style_info_free);

  scanner = g_scanner_new (NULL);

  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "active", GUINT_TO_POINTER (GTK_STATE_ACTIVE));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "prelight", GUINT_TO_POINTER (GTK_STATE_PRELIGHT));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "hover", GUINT_TO_POINTER (GTK_STATE_PRELIGHT));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "selected", GUINT_TO_POINTER (GTK_STATE_SELECTED));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "insensitive", GUINT_TO_POINTER (GTK_STATE_INSENSITIVE));

  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "nth-child", GUINT_TO_POINTER (SYMBOL_NTH_CHILD));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "first-child", GUINT_TO_POINTER (SYMBOL_FIRST_CHILD));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "last-child", GUINT_TO_POINTER (SYMBOL_LAST_CHILD));

  g_scanner_scope_add_symbol (scanner, SCOPE_NTH_CHILD, "even", GUINT_TO_POINTER (SYMBOL_NTH_CHILD_EVEN));
  g_scanner_scope_add_symbol (scanner, SCOPE_NTH_CHILD, "odd", GUINT_TO_POINTER (SYMBOL_NTH_CHILD_ODD));
  g_scanner_scope_add_symbol (scanner, SCOPE_NTH_CHILD, "first", GUINT_TO_POINTER (SYMBOL_NTH_CHILD_FIRST));
  g_scanner_scope_add_symbol (scanner, SCOPE_NTH_CHILD, "last", GUINT_TO_POINTER (SYMBOL_NTH_CHILD_LAST));

  priv->scanner = scanner;
  css_provider_apply_scope (css_provider, SCOPE_SELECTOR);

  priv->symbolic_colors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 (GDestroyNotify) g_free,
                                                 (GDestroyNotify) gtk_symbolic_color_unref);
}

typedef struct ComparePathData ComparePathData;

struct ComparePathData
{
  guint64 score;
  SelectorPath *path;
  GSList *iter;
};

static gboolean
compare_selector_element (GtkWidgetPath   *path,
                          guint            index,
                          SelectorElement *elem,
                          guint8          *score)
{
  *score = 0;

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
          return FALSE;
        }

      elem->elem_type = SELECTOR_GTYPE;
      elem->type = resolved_type;
    }

  if (elem->elem_type == SELECTOR_GTYPE)
    {
      GType type;

      type = gtk_widget_path_get_element_type (path, index);

      if (!g_type_is_a (type, elem->type))
        return FALSE;

      if (type == elem->type)
        *score |= 0xF;
      else
        {
          GType parent = type;

          *score = 0xE;

          while ((parent = g_type_parent (parent)) != G_TYPE_INVALID)
            {
              if (parent == elem->type)
                break;

              *score -= 1;

              if (*score == 1)
                {
                  g_warning ("Hierarchy is higher than expected.");
                  break;
                }
            }
        }

      return TRUE;
    }
  else if (elem->elem_type == SELECTOR_REGION)
    {
      const gchar *region_name;
      GtkChildClassFlags flags;

      /* FIXME: Need GQuark API here */
      region_name = g_quark_to_string (elem->region.name);

      if (!gtk_widget_path_iter_has_region (path, index, region_name, &flags))
        return FALSE;

      if (elem->region.flags != 0 &&
          (flags & elem->region.flags) == 0)
        return FALSE;

      *score = 0xF;
      return TRUE;
    }
  else if (elem->elem_type == SELECTOR_GLOB)
    {
      /* Treat as lowest matching type */
      *score = 1;
      return TRUE;
    }

  return FALSE;
}

static guint64
compare_selector (GtkWidgetPath *path,
                  SelectorPath  *selector)
{
  GSList *elements = selector->elements;
  gboolean match = TRUE;
  guint64 score = 0;
  guint len;
  guint i = 0;

  len = gtk_widget_path_length (path);

  while (elements && match && i < len)
    {
      SelectorElement *elem;
      guint8 elem_score;

      elem = elements->data;

      match = compare_selector_element (path, i, elem, &elem_score);

      /* Only move on to the next index if there is no match
       * with the current element (whether to continue or not
       * handled right after in the combinator check), or a
       * GType or glob has just been matched.
       *
       * Region and widget names do not trigger this because
       * the next element in the selector path could also be
       * related to the same index.
       */
      if (!match ||
          (elem->elem_type == SELECTOR_GTYPE ||
           elem->elem_type == SELECTOR_GLOB))
        i++;

      if (!match &&
          elem->elem_type != SELECTOR_TYPE_NAME &&
          elem->combinator == COMBINATOR_DESCENDANT)
        {
          /* With descendant combinators there may
           * be intermediate chidren in the hierarchy
           */
          match = TRUE;
        }
      else if (match)
        elements = elements->next;

      if (match)
        {
          /* Only 4 bits are actually used */
          score <<= 4;
          score |= elem_score;
        }
    }

  /* If there are pending selector
   * elements to compare, it's not
   * a match.
   */
  if (elements)
    match = FALSE;

  if (!match)
    score = 0;

  return score;
}

typedef struct StylePriorityInfo StylePriorityInfo;

struct StylePriorityInfo
{
  guint64 score;
  GHashTable *style;
  GtkStateType state;
};

static GArray *
css_provider_get_selectors (GtkCssProvider *css_provider,
                            GtkWidgetPath  *path)
{
  GtkCssProviderPrivate *priv;
  GArray *priority_info;
  guint i, j;

  priv = css_provider->priv;
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
      new.state = info->path->state;

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

  return priority_info;
}

static void
css_provider_dump_symbolic_colors (GtkCssProvider *css_provider,
                                   GtkStyleSet    *set)
{
  GtkCssProviderPrivate *priv;
  GHashTableIter iter;
  gpointer key, value;

  priv = css_provider->priv;
  g_hash_table_iter_init (&iter, priv->symbolic_colors);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const gchar *name;
      GtkSymbolicColor *color;

      name = key;
      color = value;

      gtk_style_set_map_color (set, name, color);
    }
}

static GtkStyleSet *
gtk_css_provider_get_style (GtkStyleProvider *provider,
                            GtkWidgetPath    *path)
{
  GtkCssProvider *css_provider;
  GtkCssProviderPrivate *priv;
  GtkStyleSet *set;
  GArray *priority_info;
  guint i;

  css_provider = GTK_CSS_PROVIDER (provider);
  priv = css_provider->priv;
  set = gtk_style_set_new ();

  css_provider_dump_symbolic_colors (css_provider, set);
  priority_info = css_provider_get_selectors (css_provider, path);

  for (i = 0; i < priority_info->len; i++)
    {
      StylePriorityInfo *info;
      GHashTableIter iter;
      gpointer key, value;

      info = &g_array_index (priority_info, StylePriorityInfo, i);
      g_hash_table_iter_init (&iter, info->style);

      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          gchar *prop = key;

          if (info->state == GTK_STATE_NORMAL)
            gtk_style_set_set_default (set, key, value);
          else
            gtk_style_set_set_property (set, key, info->state, value);
        }
    }

  g_array_free (priority_info, TRUE);

  return set;
}

static gboolean
gtk_css_provider_get_style_property (GtkStyleProvider *provider,
                                     GtkWidgetPath    *path,
                                     const gchar      *property_name,
                                     GValue           *value)
{
  GArray *priority_info;
  gboolean found = FALSE;
  gchar *prop_name;
  GType path_type;
  gint i;

  path_type = gtk_widget_path_get_widget_type (path);

  prop_name = g_strdup_printf ("-%s-%s",
                               g_type_name (path_type),
                               property_name);

  priority_info = css_provider_get_selectors (GTK_CSS_PROVIDER (provider), path);

  for (i = priority_info->len - 1; i >= 0; i--)
    {
      StylePriorityInfo *info;
      GValue *val;

      info = &g_array_index (priority_info, StylePriorityInfo, i);
      val = g_hash_table_lookup (info->style, prop_name);

      if (val)
        {
          const gchar *val_str;

          val_str = g_value_get_string (val);
          found = TRUE;

          css_provider_parse_value (val_str, value);
          break;
        }
    }

  g_array_free (priority_info, TRUE);
  g_free (prop_name);

  return found;
}

static void
gtk_css_style_provider_iface_init (GtkStyleProviderIface *iface)
{
  iface->get_style = gtk_css_provider_get_style;
  iface->get_style_property = gtk_css_provider_get_style_property;
}

static void
gtk_css_provider_finalize (GObject *object)
{
  GtkCssProvider *css_provider;
  GtkCssProviderPrivate *priv;

  css_provider = GTK_CSS_PROVIDER (object);
  priv = css_provider->priv;

  g_scanner_destroy (priv->scanner);
  g_free (priv->filename);

  g_ptr_array_free (priv->selectors_info, TRUE);

  g_slist_foreach (priv->cur_selectors, (GFunc) selector_path_unref, NULL);
  g_slist_free (priv->cur_selectors);

  g_hash_table_unref (priv->cur_properties);
  g_hash_table_destroy (priv->symbolic_colors);

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

  priv = css_provider->priv;

  g_scanner_set_scope (priv->scanner, scope);

  if (scope == SCOPE_VALUE)
    {
      priv->scanner->config->cset_identifier_first = G_CSET_a_2_z "@#-_0123456789" G_CSET_A_2_Z;
      priv->scanner->config->cset_identifier_nth = G_CSET_a_2_z "@#-_ 0123456789(),." G_CSET_A_2_Z;
      priv->scanner->config->scan_identifier_1char = TRUE;
    }
  else if (scope == SCOPE_SELECTOR)
    {
      priv->scanner->config->cset_identifier_first = G_CSET_a_2_z G_CSET_A_2_Z "*@";
      priv->scanner->config->cset_identifier_nth = G_CSET_a_2_z "-_" G_CSET_A_2_Z;
      priv->scanner->config->scan_identifier_1char = TRUE;
    }
  else if (scope == SCOPE_PSEUDO_CLASS ||
           scope == SCOPE_NTH_CHILD ||
           scope == SCOPE_DECLARATION)
    {
      priv->scanner->config->cset_identifier_first = G_CSET_a_2_z "-" G_CSET_A_2_Z;
      priv->scanner->config->cset_identifier_nth = G_CSET_a_2_z "-" G_CSET_A_2_Z;
      priv->scanner->config->scan_identifier_1char = FALSE;
    }
  else
    g_assert_not_reached ();

  priv->scanner->config->scan_float = FALSE;
}

static void
css_provider_push_scope (GtkCssProvider *css_provider,
                         ParserScope     scope)
{
  GtkCssProviderPrivate *priv;

  priv = css_provider->priv;
  priv->state = g_slist_prepend (priv->state, GUINT_TO_POINTER (scope));

  css_provider_apply_scope (css_provider, scope);
}

static ParserScope
css_provider_pop_scope (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv;
  ParserScope scope = SCOPE_SELECTOR;

  priv = css_provider->priv;

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

  priv = css_provider->priv;

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

  priv = css_provider->priv;
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
parse_pseudo_class (GtkCssProvider     *css_provider,
                    GScanner           *scanner,
                    SelectorPath       *selector,
                    GtkChildClassFlags *flags)
{
  ParserSymbol symbol;

  css_provider_push_scope (css_provider, SCOPE_PSEUDO_CLASS);
  g_scanner_get_next_token (scanner);

  if (scanner->token != G_TOKEN_SYMBOL)
    return G_TOKEN_SYMBOL;

  symbol = GPOINTER_TO_INT (scanner->value.v_symbol);

  if (symbol == SYMBOL_NTH_CHILD)
    {
      g_scanner_get_next_token (scanner);

      if (scanner->token != G_TOKEN_LEFT_PAREN)
        return G_TOKEN_LEFT_PAREN;

      css_provider_push_scope (css_provider, SCOPE_NTH_CHILD);
      g_scanner_get_next_token (scanner);

      if (scanner->token != G_TOKEN_SYMBOL)
        return G_TOKEN_SYMBOL;

      symbol = GPOINTER_TO_INT (scanner->value.v_symbol);

      switch (symbol)
        {
        case SYMBOL_NTH_CHILD_EVEN:
          *flags = GTK_CHILD_CLASS_EVEN;
          break;
        case SYMBOL_NTH_CHILD_ODD:
          *flags = GTK_CHILD_CLASS_ODD;
          break;
        case SYMBOL_NTH_CHILD_FIRST:
          *flags = GTK_CHILD_CLASS_FIRST;
          break;
        case SYMBOL_NTH_CHILD_LAST:
          *flags = GTK_CHILD_CLASS_LAST;
          break;
        default:
          break;
        }

      g_scanner_get_next_token (scanner);

      if (scanner->token != G_TOKEN_RIGHT_PAREN)
        return G_TOKEN_RIGHT_PAREN;

      css_provider_pop_scope (css_provider);
    }
  else if (symbol == SYMBOL_FIRST_CHILD)
    *flags = GTK_CHILD_CLASS_FIRST;
  else if (symbol == SYMBOL_LAST_CHILD)
    *flags = GTK_CHILD_CLASS_LAST;
  else
    {
      GtkStateType state;

      state = GPOINTER_TO_INT (scanner->value.v_symbol);
      selector->state = state;
    }

  css_provider_pop_scope (css_provider);
  return G_TOKEN_NONE;
}

static GTokenType
parse_selector (GtkCssProvider  *css_provider,
                GScanner        *scanner,
                SelectorPath   **selector_out)
{
  SelectorPath *path;

  path = selector_path_new ();
  *selector_out = path;

  if (scanner->token != ':' &&
      scanner->token != G_TOKEN_IDENTIFIER)
    return G_TOKEN_IDENTIFIER;

  while (scanner->token == G_TOKEN_IDENTIFIER)
    {
      if (g_ascii_isupper (scanner->value.v_identifier[0]))
        selector_path_prepend_type (path, scanner->value.v_identifier);
      else if (g_ascii_islower (scanner->value.v_identifier[0]))
        {
          GtkChildClassFlags flags = 0;
          gchar *region_name;

          region_name = g_strdup (scanner->value.v_identifier);

          /* Parse nth-child type pseudo-class, and
           * possibly a state pseudo-class after it.
           */
          while (path->state == GTK_STATE_NORMAL &&
                 g_scanner_peek_next_token (scanner) == ':')
            {
              GTokenType token;

              g_scanner_get_next_token (scanner);

              if ((token = parse_pseudo_class (css_provider, scanner, path, &flags)) != G_TOKEN_NONE)
                return token;
            }

          selector_path_prepend_region (path, region_name, flags);
          g_free (region_name);
        }
      else if (scanner->value.v_identifier[0] == '*')
        selector_path_prepend_glob (path);
      else
        return G_TOKEN_IDENTIFIER;

      g_scanner_get_next_token (scanner);

      /* State is the last element in the selector */
      if (path->state != GTK_STATE_NORMAL)
        break;

      if (scanner->token == '>')
        {
          selector_path_prepend_combinator (path, COMBINATOR_CHILD);
          g_scanner_get_next_token (scanner);
        }
    }

  if (scanner->token == ':' &&
      path->state == GTK_STATE_NORMAL)
    {
      GtkChildClassFlags flags = 0;
      GTokenType token;

      /* Add glob selector if path is empty */
      if (selector_path_depth (path) == 0)
        selector_path_prepend_glob (path);

      if ((token = parse_pseudo_class (css_provider, scanner, path, &flags)) != G_TOKEN_NONE)
        return token;

      if (flags != 0)
        {
          /* This means either a standalone :nth-child
           * selector, or on a invalid element type.
           */
          return G_TOKEN_SYMBOL;
        }

      g_scanner_get_next_token (scanner);
    }
  else if (scanner->token == G_TOKEN_SYMBOL)
    {
      /* A new pseudo-class was starting, but the state was already
       * parsed, so nothing is supposed to go after that.
       */
      return G_TOKEN_LEFT_CURLY;
    }

  return G_TOKEN_NONE;
}

#define SKIP_SPACES(s) while (s[0] == ' ') s++;

static GtkSymbolicColor *
symbolic_color_parse_str (const gchar  *string,
			  gchar       **end_ptr)
{
  GtkSymbolicColor *symbolic_color = NULL;
  gchar *str;

  str = (gchar *) string;

  if (str[0] == '#')
    {
      GdkColor color;
      gchar *color_str;
      const gchar *end;

      end = str + 1;

      while (g_ascii_isxdigit (*end))
        end++;

      color_str = g_strndup (str, end - str);
      *end_ptr = (gchar *) end;

      if (!gdk_color_parse (color_str, &color))
        {
          g_free (color_str);
          return NULL;
        }

      symbolic_color = gtk_symbolic_color_new_literal (&color);
      g_free (color_str);
    }
  else if (str[0] == '@')
    {
      const gchar *end;
      gchar *name;

      str++;
      end = str;

      while (*end == '-' || *end == '_' ||
             g_ascii_isalpha (*end))
        end++;

      name = g_strndup (str, end - str);
      symbolic_color = gtk_symbolic_color_new_name (name);
      g_free (name);

      *end_ptr = (gchar *) end;
    }
  else if (g_str_has_prefix (str, "shade"))
    {
      GtkSymbolicColor *param_color;
      gdouble factor;

      str += strlen ("shade");

      SKIP_SPACES (str);

      if (*str != '(')
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      param_color = symbolic_color_parse_str (str, end_ptr);

      if (!param_color)
        return NULL;

      str = *end_ptr;
      SKIP_SPACES (str);

      if (str[0] != ',')
        {
          gtk_symbolic_color_unref (param_color);
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      factor = g_ascii_strtod (str, end_ptr);

      str = *end_ptr;
      SKIP_SPACES (str);
      *end_ptr = (gchar *) str;

      if (str[0] != ')')
        {
          gtk_symbolic_color_unref (param_color);
          return NULL;
        }

      symbolic_color = gtk_symbolic_color_new_shade (param_color, factor);
      (*end_ptr)++;
    }
  else if (g_str_has_prefix (str, "mix"))
    {
      GtkSymbolicColor *color1, *color2;
      gdouble factor;

      str += strlen ("mix");
      SKIP_SPACES (str);

      if (*str != '(')
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      color1 = symbolic_color_parse_str (str, end_ptr);

      if (!color1)
        return NULL;

      str = *end_ptr;
      SKIP_SPACES (str);

      if (str[0] != ',')
        {
          gtk_symbolic_color_unref (color1);
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      color2 = symbolic_color_parse_str (str, end_ptr);

      if (!color2 || *end_ptr[0] != ',')
        {
          gtk_symbolic_color_unref (color1);
          return NULL;
        }

      str = *end_ptr;
      SKIP_SPACES (str);

      if (str[0] != ',')
        {
          gtk_symbolic_color_unref (color1);
          gtk_symbolic_color_unref (color2);
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      factor = g_ascii_strtod (str, end_ptr);

      str = *end_ptr;
      SKIP_SPACES (str);
      *end_ptr = (gchar *) str;

      if (str[0] != ')')
	{
          gtk_symbolic_color_unref (color1);
          gtk_symbolic_color_unref (color2);
          return NULL;
        }

      symbolic_color = gtk_symbolic_color_new_mix (color1, color2, factor);
      (*end_ptr)++;
    }

  return symbolic_color;
}

#undef SKIP_SPACES

static GtkSymbolicColor *
symbolic_color_parse (const gchar *str)
{
  GtkSymbolicColor *color;
  gchar *end;

  color = symbolic_color_parse_str (str, &end);

  if (*end != '\0')
    {
      g_warning ("Error parsing symbolic color \"%s\", stopped at char %ld : '%c'",
                 str, end - str, *end);

      if (color)
        {
          gtk_symbolic_color_unref (color);
          color = NULL;
        }
    }

  return color;
}

static gboolean
css_provider_parse_value (const gchar *value_str,
                          GValue      *value)
{
  GType type;
  gboolean parsed = TRUE;

  type = G_VALUE_TYPE (value);

  if (type == GDK_TYPE_COLOR)
    {
      GdkColor color;

      if (gdk_color_parse (value_str, &color) == TRUE)
        g_value_set_boxed (value, &color);
      else
        {
          GtkSymbolicColor *symbolic_color;

          symbolic_color = symbolic_color_parse (value_str);

          if (!symbolic_color)
            return FALSE;

          g_value_unset (value);
          g_value_init (value, GTK_TYPE_SYMBOLIC_COLOR);
          g_value_take_boxed (value, symbolic_color);
        }
    }
  else if (type == PANGO_TYPE_FONT_DESCRIPTION)
    {
      PangoFontDescription *font_desc;

      font_desc = pango_font_description_from_string (value_str);
      g_value_take_boxed (value, font_desc);
    }
  else if (type == G_TYPE_BOOLEAN)
    {
      if (value_str[0] == '1' ||
          g_ascii_strcasecmp (value_str, "true") == 0)
        g_value_set_boolean (value, TRUE);
      else
        g_value_set_boolean (value, FALSE);
    }
  else if (type == G_TYPE_INT)
    g_value_set_int (value, atoi (value_str));
  else if (type == G_TYPE_DOUBLE)
    g_value_set_double (value, g_ascii_strtod (value_str, NULL));
  else if (type == GTK_TYPE_THEMING_ENGINE)
    {
      GtkThemingEngine *engine;

      engine = gtk_theming_engine_load (value_str);
      g_value_set_object (value, engine);
    }
  else
    {
      g_warning ("Cannot parse string '%s' for type %s", value_str, g_type_name (type));
      parsed = FALSE;
    }

  return parsed;
}

static GTokenType
parse_rule (GtkCssProvider *css_provider,
            GScanner       *scanner)
{
  GtkCssProviderPrivate *priv;
  GTokenType expected_token;
  SelectorPath *selector;

  priv = css_provider->priv;

  css_provider_push_scope (css_provider, SCOPE_SELECTOR);

  if (scanner->token == G_TOKEN_IDENTIFIER &&
      scanner->value.v_identifier[0] == '@')
    {
      GtkSymbolicColor *color;
      gchar *color_name;

      /* Rule is a color mapping */
      color_name = g_strdup (&scanner->value.v_identifier[1]);
      g_scanner_get_next_token (scanner);

      if (scanner->token != ':')
        return ':';

      css_provider_push_scope (css_provider, SCOPE_VALUE);
      g_scanner_get_next_token (scanner);

      if (scanner->token != G_TOKEN_IDENTIFIER)
        return G_TOKEN_IDENTIFIER;

      color = symbolic_color_parse (scanner->value.v_identifier);

      if (!color)
        return G_TOKEN_IDENTIFIER;

      g_hash_table_insert (priv->symbolic_colors, color_name, color);

      css_provider_pop_scope (css_provider);
      g_scanner_get_next_token (scanner);

      if (scanner->token != ';')
        return ';';

      return G_TOKEN_NONE;
    }

  expected_token = parse_selector (css_provider, scanner, &selector);

  if (expected_token != G_TOKEN_NONE)
    {
      selector_path_unref (selector);
      return expected_token;
    }

  priv->cur_selectors = g_slist_prepend (priv->cur_selectors, selector);

  while (scanner->token == ',')
    {
      g_scanner_get_next_token (scanner);

      expected_token = parse_selector (css_provider, scanner, &selector);

      if (expected_token != G_TOKEN_NONE)
        {
          selector_path_unref (selector);
          return expected_token;
        }

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

      if (prop[0] == '-' &&
          g_ascii_isupper (prop[1]))
        {
          GValue *val;

          val = g_slice_new0 (GValue);
          g_value_init (val, G_TYPE_STRING);
          g_value_set_string (val, value_str);

          g_hash_table_insert (priv->cur_properties, prop, val);
        }
      else if (gtk_style_set_lookup_property (prop, &prop_type))
        {
          GValue *val;

          val = g_slice_new0 (GValue);
          g_value_init (val, prop_type);

          if (prop_type == G_TYPE_STRING)
            {
              g_value_set_string (val, value_str);
              g_hash_table_insert (priv->cur_properties, prop, val);
            }
          else if (css_provider_parse_value (value_str, val))
            g_hash_table_insert (priv->cur_properties, prop, val);
          else
            {
              g_value_unset (val);
              g_slice_free (GValue, val);
              g_free (prop);

              return G_TOKEN_IDENTIFIER;
            }
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

  priv = css_provider->priv;
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

  priv = css_provider->priv;

  if (length < 0)
    length = strlen (data);

  if (priv->selectors_info->len > 0)
    g_ptr_array_remove_range (priv->selectors_info, 0, priv->selectors_info->len);

  css_provider_reset_parser (css_provider);
  priv->scanner->input_name = "-";
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

  priv = css_provider->priv;

  if (!g_file_load_contents (file, NULL,
                             &data, &length,
                             NULL, &internal_error))
    {
      g_propagate_error (error, internal_error);
      return FALSE;
    }

  if (priv->selectors_info->len > 0)
    g_ptr_array_remove_range (priv->selectors_info, 0, priv->selectors_info->len);

  g_free (priv->filename);
  priv->filename = g_file_get_path (file);

  css_provider_reset_parser (css_provider);
  priv->scanner->input_name = priv->filename;
  g_scanner_input_text (priv->scanner, data, (guint) length);

  parse_stylesheet (css_provider);

  g_free (data);

  return TRUE;
}

GtkCssProvider *
gtk_css_provider_get_default (void)
{
  static GtkCssProvider *provider;

  if (G_UNLIKELY (!provider))
    {
      const gchar *str =
        "@fg_color: #000; \n"
        "@bg_color: #dcdad5; \n"
        "@text_color: #000; \n"
        "@base_color: #fff; \n"
        "@selected_bg_color: #4b6983; \n"
        "@selected_fg_color: #fff; \n"
        "@tooltip_bg_color: #eee1b3; \n"
        "@tooltip_fg_color: #000; \n"
        "\n"
        "*, GtkTreeView GtkButton {\n"
        "  background-color: @bg_color;\n"
        "  foreground-color: @fg_color;\n"
        "  text-color: @text_color; \n"
        "  base-color: @base_color; \n"
        "}\n"
        "\n"
        "*:active {\n"
        "  background-color: #c4c2bd;\n"
        "  foreground-color: #000000;\n"
        "  text-color: #c4c2bd; \n"
        "  base-color: #9c9a94; \n"
        "}\n"
        "\n"
        "*:prelight {\n"
        "  background-color: #eeebe7;\n"
        "  foreground-color: #000000;\n"
        "  text-color: #eeebe7; \n"
        "  base-color: #ffffff; \n"
        "}\n"
        "\n"
        "*:selected {\n"
        "  background-color: #4b6983;\n"
        "  foreground-color: #ffffff;\n"
        "  base-color: #4b6983;\n"
        "  text-color: #ffffff;\n"
        "}\n"
        "\n"
        "*:insensitive {\n"
        "  background-color: #dcdad5;\n"
        "  foreground-color: #757575;\n"
        "}\n"
        "\n"
        "GtkTreeView, GtkIconView {\n"
        "  background-color: #ffffff; \n"
        "}\n"
        "\n"
        "row:nth-child(odd) { \n"
        "  background-color: shade(#ffffff, 0.93); \n"
        "}\n"
        "\n"
        "tooltip {\n"
        "  background-color: @tooltip_bg_color; \n"
        "  foreground-color: @tooltip_fg_color; \n"
        "}\n"
        "\n"
        "GtkToggleButton:prelight {\n"
        "  text-color: #000; \n"
        "}\n"
        "\n";

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_data (provider, str, -1, NULL);
    }

  return provider;
}

#define __GTK_CSS_PROVIDER_C__
#include "gtkaliasdef.c"
