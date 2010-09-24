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
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gtkanimationdescription.h"
#include "gtk9slice.h"
#include "gtkcssprovider.h"

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
  SELECTOR_CLASS,
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
      GtkRegionFlags flags;
    } region;
  };
};

struct SelectorPath
{
  GSList *elements;
  GtkStateFlags state;
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
static gboolean css_provider_parse_value (GtkCssProvider *css_provider,
                                          const gchar    *value_str,
                                          GValue         *value);

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
selector_path_prepend_region (SelectorPath   *path,
                              const gchar    *name,
                              GtkRegionFlags  flags)
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
selector_path_prepend_name (SelectorPath *path,
                            const gchar  *name)
{
  SelectorElement *elem;

  elem = g_slice_new (SelectorElement);
  elem->combinator = COMBINATOR_DESCENDANT;
  elem->elem_type = SELECTOR_NAME;

  elem->name = g_quark_from_string (name);

  path->elements = g_slist_prepend (path->elements, elem);
}

static void
selector_path_prepend_class (SelectorPath *path,
                             const gchar  *name)
{
  SelectorElement *elem;

  elem = g_slice_new (SelectorElement);
  elem->combinator = COMBINATOR_DESCENDANT;
  elem->elem_type = SELECTOR_CLASS;

  elem->name = g_quark_from_string (name);

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

      type = gtk_widget_path_iter_get_widget_type (path, index);

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
      GtkRegionFlags flags;

      if (!gtk_widget_path_iter_has_qregion (path, index,
                                             elem->region.name,
                                             &flags))
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
  else if (elem->elem_type == SELECTOR_NAME)
    {
      if (!gtk_widget_path_iter_has_qname (path, index, elem->name))
        return FALSE;

      *score = 0xF;
      return TRUE;
    }
  else if (elem->elem_type == SELECTOR_CLASS)
    {
      if (!gtk_widget_path_iter_has_qclass (path, index, elem->name))
        return FALSE;

      *score = 0xF;
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
  GtkStateFlags state;
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

          /* Properties starting with '-' may be both widget style properties
           * or custom properties from the theming engine, so check whether
           * the type is registered or not.
           */
          if (prop[0] == '-' &&
              !gtk_style_set_lookup_property (prop, NULL, NULL))
            continue;

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

          css_provider_parse_value (GTK_CSS_PROVIDER (provider), val_str, value);
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
      priv->scanner->config->cset_identifier_nth = G_CSET_a_2_z "@#-_ 0123456789(),.\n" G_CSET_A_2_Z;
      priv->scanner->config->scan_identifier_1char = TRUE;
    }
  else if (scope == SCOPE_SELECTOR)
    {
      priv->scanner->config->cset_identifier_first = G_CSET_a_2_z G_CSET_A_2_Z "*@";
      priv->scanner->config->cset_identifier_nth = G_CSET_a_2_z "-_#" G_CSET_A_2_Z;
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
  priv->scanner->config->cpair_comment_single = NULL;
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
parse_nth_child (GtkCssProvider *css_provider,
                 GScanner       *scanner,
                 GtkRegionFlags *flags)
{
  ParserSymbol symbol;

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
          *flags = GTK_REGION_EVEN;
          break;
        case SYMBOL_NTH_CHILD_ODD:
          *flags = GTK_REGION_ODD;
          break;
        case SYMBOL_NTH_CHILD_FIRST:
          *flags = GTK_REGION_FIRST;
          break;
        case SYMBOL_NTH_CHILD_LAST:
          *flags = GTK_REGION_LAST;
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
    *flags = GTK_REGION_FIRST;
  else if (symbol == SYMBOL_LAST_CHILD)
    *flags = GTK_REGION_LAST;
  else
    {
      *flags = 0;
      return G_TOKEN_SYMBOL;
    }

  return G_TOKEN_NONE;
}

static GTokenType
parse_pseudo_class (GtkCssProvider *css_provider,
                    GScanner       *scanner,
                    SelectorPath   *selector)
{
  GtkStateType state;

  g_scanner_get_next_token (scanner);

  if (scanner->token != G_TOKEN_SYMBOL)
    return G_TOKEN_SYMBOL;

  state = GPOINTER_TO_INT (scanner->value.v_symbol);

  switch (state)
    {
    case GTK_STATE_ACTIVE:
      selector->state |= GTK_STATE_FLAG_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      selector->state |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      selector->state |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      selector->state |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    case GTK_STATE_INCONSISTENT:
      selector->state |= GTK_STATE_FLAG_INCONSISTENT;
      break;
    case GTK_STATE_FOCUSED:
      selector->state |= GTK_STATE_FLAG_FOCUSED;
      break;
    default:
      return G_TOKEN_SYMBOL;
    }

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
      scanner->token != '#' &&
      scanner->token != '.' &&
      scanner->token != G_TOKEN_IDENTIFIER)
    return G_TOKEN_IDENTIFIER;

  while (scanner->token == '#' ||
         scanner->token == '.' ||
         scanner->token == G_TOKEN_IDENTIFIER)
    {
      if (scanner->token == '#' ||
          scanner->token == '.')
        {
          gboolean is_class;

          is_class = (scanner->token == '.');

          g_scanner_get_next_token (scanner);

          if (scanner->token != G_TOKEN_IDENTIFIER)
            return G_TOKEN_IDENTIFIER;

          selector_path_prepend_glob (path);
          selector_path_prepend_combinator (path, COMBINATOR_CHILD);

          if (is_class)
            selector_path_prepend_class (path, scanner->value.v_identifier);
          else
            selector_path_prepend_name (path, scanner->value.v_identifier);
        }
      else if (g_ascii_isupper (scanner->value.v_identifier[0]))
        {
          gchar *pos;

          if ((pos = strchr (scanner->value.v_identifier, '#')) != NULL ||
              (pos = strchr (scanner->value.v_identifier, '.')) != NULL)
            {
              gchar *type_name, *name;
              gboolean is_class;

              is_class = (*pos == '.');

              /* Widget type and name/class put together */
              name = pos + 1;
              *pos = '\0';
              type_name = scanner->value.v_identifier;

              selector_path_prepend_type (path, type_name);

              /* This is only so there is a direct relationship
               * between widget type and its name.
               */
              selector_path_prepend_combinator (path, COMBINATOR_CHILD);

              if (is_class)
                selector_path_prepend_class (path, name);
              else
                selector_path_prepend_name (path, name);
            }
          else
            selector_path_prepend_type (path, scanner->value.v_identifier);
        }
      else if (g_ascii_islower (scanner->value.v_identifier[0]))
        {
          GtkRegionFlags flags = 0;
          gchar *region_name;

          region_name = g_strdup (scanner->value.v_identifier);

          if (g_scanner_peek_next_token (scanner) == ':')
            {
              ParserSymbol symbol;

              g_scanner_get_next_token (scanner);
              css_provider_push_scope (css_provider, SCOPE_PSEUDO_CLASS);

              /* Check for the next token being nth-child, parse in that
               * case, and fallback into common state parsing if not.
               */
              if (g_scanner_peek_next_token (scanner) != G_TOKEN_SYMBOL)
                return G_TOKEN_SYMBOL;

              symbol = GPOINTER_TO_INT (scanner->next_value.v_symbol);

              if (symbol == SYMBOL_FIRST_CHILD ||
                  symbol == SYMBOL_LAST_CHILD ||
                  symbol == SYMBOL_NTH_CHILD)
                {
                  GTokenType token;

                  if ((token = parse_nth_child (css_provider, scanner, &flags)) != G_TOKEN_NONE)
                    return token;

                  css_provider_pop_scope (css_provider);
                }
              else
                {
                  css_provider_pop_scope (css_provider);
                  selector_path_prepend_region (path, region_name, 0);
                  g_free (region_name);
                  break;
                }
            }

          selector_path_prepend_region (path, region_name, flags);
          g_free (region_name);
        }
      else if (scanner->value.v_identifier[0] == '*')
        selector_path_prepend_glob (path);
      else
        return G_TOKEN_IDENTIFIER;

      g_scanner_get_next_token (scanner);

      if (scanner->token == '>')
        {
          selector_path_prepend_combinator (path, COMBINATOR_CHILD);
          g_scanner_get_next_token (scanner);
        }
    }

  if (scanner->token == ':')
    {
      /* Add glob selector if path is empty */
      if (selector_path_depth (path) == 0)
        selector_path_prepend_glob (path);

      css_provider_push_scope (css_provider, SCOPE_PSEUDO_CLASS);

      while (scanner->token == ':')
        {
          GTokenType token;

          if ((token = parse_pseudo_class (css_provider, scanner, path)) != G_TOKEN_NONE)
            return token;

          g_scanner_get_next_token (scanner);
        }

      css_provider_pop_scope (css_provider);
    }

  return G_TOKEN_NONE;
}

#define SKIP_SPACES(s) while (s[0] == ' ' || s[0] == '\t' || s[0] == '\n') s++;

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

static GtkGradient *
gradient_parse_str (const gchar  *str,
                    gchar       **end_ptr)
{
  GtkGradient *gradient = NULL;

  if (g_str_has_prefix (str, "-gtk-gradient"))
    {
      cairo_pattern_type_t type;

      str += strlen ("-gtk-gradient");
      SKIP_SPACES (str);

      if (*str != '(')
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);

      /* Parse gradient type */
      if (g_str_has_prefix (str, "linear"))
        {
          type = CAIRO_PATTERN_TYPE_LINEAR;
          str += strlen ("linear");
        }
      else if (g_str_has_prefix (str, "radial"))
        {
          type = CAIRO_PATTERN_TYPE_RADIAL;
          str += strlen ("radial");
        }
      else
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      SKIP_SPACES (str);

      if (type == CAIRO_PATTERN_TYPE_LINEAR)
        {
          gdouble coords[4];
          gchar *end;
          guint i;

          /* Parse start/stop position parameters */
          for (i = 0; i < 2; i++)
            {
              if (*str != ',')
                {
                  *end_ptr = (gchar *) str;
                  return NULL;
                }

              str++;
              SKIP_SPACES (str);

              if (g_str_has_prefix (str, "left"))
                {
                  coords[i * 2] = 0;
                  str += strlen ("left");
                }
              else if (g_str_has_prefix (str, "right"))
                {
                  coords[i * 2] = 1;
                  str += strlen ("right");
                }
              else
                {
                  coords[i * 2] = g_ascii_strtod (str, &end);
                  str = end;
                }

              SKIP_SPACES (str);

              if (g_str_has_prefix (str, "top"))
                {
                  coords[(i * 2) + 1] = 0;
                  str += strlen ("top");
                }
              else if (g_str_has_prefix (str, "bottom"))
                {
                  coords[(i * 2) + 1] = 1;
                  str += strlen ("bottom");
                }
              else
                {
                  coords[(i * 2) + 1] = g_ascii_strtod (str, &end);
                  str = end;
                }

              SKIP_SPACES (str);
            }

          gradient = gtk_gradient_new_linear (coords[0], coords[1], coords[2], coords[3]);

          while (*str == ',')
            {
              GtkSymbolicColor *color;
              gdouble position;

              if (*str != ',')
                {
                  *end_ptr = (gchar *) str;
                  return gradient;
                }

              str++;
              SKIP_SPACES (str);

              if (g_str_has_prefix (str, "from"))
                {
                  position = 0;
                  str += strlen ("from");
                  SKIP_SPACES (str);

                  if (*str != '(')
                    {
                      *end_ptr = (gchar *) str;
                      return gradient;
                    }
                }
              else if (g_str_has_prefix (str, "to"))
                {
                  position = 1;
                  str += strlen ("to");
                  SKIP_SPACES (str);

                  if (*str != '(')
                    {
                      *end_ptr = (gchar *) str;
                      return gradient;
                    }
                }
              else if (g_str_has_prefix (str, "color-stop"))
                {
                  str += strlen ("color-stop");
                  SKIP_SPACES (str);

                  if (*str != '(')
                    {
                      *end_ptr = (gchar *) str;
                      return gradient;
                    }

                  str++;
                  SKIP_SPACES (str);

                  position = g_strtod (str, &end);

                  str = end;
                  SKIP_SPACES (str);

                  if (*str != ',')
                    {
                      *end_ptr = (gchar *) str;
                      return gradient;
                    }
                }
              else
                {
                  *end_ptr = (gchar *) str;
                  return gradient;
                }

              str++;
              SKIP_SPACES (str);

              color = symbolic_color_parse_str (str, &end);

              str = end;
              SKIP_SPACES (str);

              if (*str != ')')
                {
                  *end_ptr = (gchar *) str;
                  return gradient;
                }

              str++;
              SKIP_SPACES (str);

              if (color)
                {
                  gtk_gradient_add_color_stop (gradient, position, color);
                  gtk_symbolic_color_unref (color);
                }
            }

          if (*str != ')')
            {
              *end_ptr = (gchar *) str;
              return gradient;
            }

          str++;
        }
    }

  *end_ptr = (gchar *) str;

  return gradient;
}

static GtkGradient *
gradient_parse (const gchar *str)
{
  GtkGradient *gradient;
  gchar *end;

  gradient = gradient_parse_str (str, &end);

  if (*end != '\0')
    {
      g_warning ("Error parsing pattern \"%s\", stopped at char %ld : '%c'",
                 str, end - str, *end);

      if (gradient)
        {
          gtk_gradient_unref (gradient);
          gradient = NULL;
        }
    }

  return gradient;
}

static gchar *
url_parse_str (const gchar  *str,
               gchar       **end_ptr)
{
  gchar *path, *chr;

  if (!g_str_has_prefix (str, "url"))
    {
      *end_ptr = (gchar *) str;
      return NULL;
    }

  str += strlen ("url");
  SKIP_SPACES (str);

  if (*str != '(')
    {
      *end_ptr = (gchar *) str;
      return NULL;
    }

  chr = strchr (str, ')');

  if (!chr)
    {
      *end_ptr = (gchar *) str;
      return NULL;
    }

  str++;
  SKIP_SPACES (str);

  path = g_strndup (str, chr - str);
  g_strstrip (path);

  *end_ptr = chr + 1;

  return path;
}

static Gtk9Slice *
slice_parse_str (GtkCssProvider  *css_provider,
                 const gchar     *str,
                 gchar          **end_ptr)
{
  gdouble distance_top, distance_bottom;
  gdouble distance_left, distance_right;
  GtkSliceSideModifier mods[2];
  GError *error = NULL;
  GdkPixbuf *pixbuf;
  gchar *path;
  gint i = 0;

  SKIP_SPACES (str);

  /* Parse image url */
  path = url_parse_str (str, end_ptr);

  if (!path)
      return NULL;

  if (!g_path_is_absolute (path))
    {
      GtkCssProviderPrivate *priv;
      gchar *dirname, *full_path;

      priv = css_provider->priv;

      /* Use relative path to the current CSS file path, if any */
      dirname = g_path_get_dirname (priv->filename);

      full_path = g_build_filename (dirname, path, NULL);
      g_free (path);
      path = full_path;
    }

  str = *end_ptr;
  SKIP_SPACES (str);

  /* Parse top/left/bottom/right distances */
  distance_top = g_strtod (str, end_ptr);

  str = *end_ptr;
  SKIP_SPACES (str);

  distance_right = g_strtod (str, end_ptr);

  str = *end_ptr;
  SKIP_SPACES (str);

  distance_bottom = g_strtod (str, end_ptr);

  str = *end_ptr;
  SKIP_SPACES (str);

  distance_left = g_strtod (str, end_ptr);

  str = *end_ptr;
  SKIP_SPACES (str);

  while (*str && i < 2)
    {
      if (g_str_has_prefix (str, "stretch"))
        {
          str += strlen ("stretch");
          mods[i] = GTK_SLICE_STRETCH;
        }
      else if (g_str_has_prefix (str, "repeat"))
        {
          str += strlen ("repeat");
          mods[i] = GTK_SLICE_REPEAT;
        }
      else
        {
          g_free (path);
          *end_ptr = (gchar *) str;
          return NULL;
        }

      SKIP_SPACES (str);
      i++;
    }

  *end_ptr = (gchar *) str;

  if (*str != '\0')
    {
      g_free (path);
      return NULL;
    }

  if (i != 2)
    {
      /* Fill in second modifier, same as the first */
      mods[1] = mods[1];
    }

  pixbuf = gdk_pixbuf_new_from_file (path, &error);
  g_free (path);

  if (error)
    {
      g_warning ("Pixbuf could not be loaded: %s\n", error->message);
      g_error_free (error);
      *end_ptr = (gchar *) str;
      return NULL;
    }

  return gtk_9slice_new (pixbuf,
                         distance_top, distance_bottom,
                         distance_left, distance_right,
                         mods[0], mods[1]);
}

static Gtk9Slice *
slice_parse (GtkCssProvider *css_provider,
             const gchar    *str)
{
  Gtk9Slice *slice;
  gchar *end;

  slice = slice_parse_str (css_provider, str, &end);

  if (*end != '\0')
    {
      g_warning ("Error parsing sliced image \"%s\", stopped at char %ld : '%c'",
                 str, end - str, *end);

      if (slice)
        {
          gtk_9slice_unref (slice);
          slice = NULL;
        }
    }

  return slice;
}

static gboolean
css_provider_parse_value (GtkCssProvider *css_provider,
                          const gchar    *value_str,
                          GValue         *value)
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
  else if (type == GTK_TYPE_ANIMATION_DESCRIPTION)
    {
      GtkAnimationDescription *desc;

      desc = gtk_animation_description_from_string (value_str);

      if (desc)
        g_value_take_boxed (value, desc);
      else
        parsed = FALSE;
    }
  else if (type == GTK_TYPE_BORDER)
    {
      guint first, second, third, fourth;
      GtkBorder border;

      /* FIXME: no unit support */
      if (sscanf (value_str, "%d %d %d %d",
                  &first, &second, &third, &fourth) == 4)
        {
          border.top = first;
          border.right = second;
          border.bottom = third;
          border.left = fourth;
        }
      else if (sscanf (value_str, "%d %d %d",
                       &first, &second, &third) == 3)
        {
          border.top = first;
          border.left = border.right = second;
          border.bottom = third;
        }
      else if (sscanf (value_str, "%d %d", &first, &second) == 2)
        {
          border.top = border.bottom = first;
          border.left = border.right = second;
        }
      else if (sscanf (value_str, "%d", &first) == 1)
        {
          border.top = border.bottom = first;
          border.left = border.right = first;
        }
      else
        parsed = FALSE;

      if (parsed)
        g_value_set_boxed (value, &border);
    }
  else if (type == GDK_TYPE_CAIRO_PATTERN)
    {
      GtkGradient *gradient;

      gradient = gradient_parse (value_str);

      if (gradient)
        {
          g_value_unset (value);
          g_value_init (value, GTK_TYPE_GRADIENT);
          g_value_take_boxed (value, gradient);
        }
      else
        parsed = FALSE;
    }
  else if (type == GTK_TYPE_9SLICE)
    {
      Gtk9Slice *slice;

      slice = slice_parse (css_provider, value_str);

      if (slice)
        g_value_take_boxed (value, slice);
      else
        parsed = FALSE;
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
      gchar *color_name, *color_str;

      /* Rule is a color mapping */
      color_name = g_strdup (&scanner->value.v_identifier[1]);
      g_scanner_get_next_token (scanner);

      if (scanner->token != ':')
        return ':';

      css_provider_push_scope (css_provider, SCOPE_VALUE);
      g_scanner_get_next_token (scanner);

      if (scanner->token != G_TOKEN_IDENTIFIER)
        return G_TOKEN_IDENTIFIER;

      color_str = g_strstrip (scanner->value.v_identifier);
      color = symbolic_color_parse (color_str);

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
      GtkStylePropertyParser parse_func = NULL;
      GType prop_type;
      GError *error = NULL;
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

      if (gtk_style_set_lookup_property (prop, &prop_type, &parse_func))
        {
          GValue *val;

          val = g_slice_new0 (GValue);
          g_value_init (val, prop_type);

          if (prop_type == G_TYPE_STRING)
            {
              g_value_set_string (val, value_str);
              g_hash_table_insert (priv->cur_properties, prop, val);
            }
          else if ((parse_func && (parse_func) (value_str, val, &error)) ||
                   (!parse_func && css_provider_parse_value (css_provider, value_str, val)))
            g_hash_table_insert (priv->cur_properties, prop, val);
          else
            {
              if (error)
                {
                  g_warning ("Error parsing property value: %s\n", error->message);
                  g_error_free (error);
                }

              g_value_unset (val);
              g_slice_free (GValue, val);
              g_free (prop);

              return G_TOKEN_IDENTIFIER;
            }
        }
      else if (prop[0] == '-' &&
               g_ascii_isupper (prop[1]))
        {
          GValue *val;

          val = g_slice_new0 (GValue);
          g_value_init (val, G_TYPE_STRING);
          g_value_set_string (val, value_str);

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

  g_free (priv->filename);
  priv->filename = NULL;

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

gboolean
gtk_css_provider_load_from_path (GtkCssProvider  *css_provider,
                                 const gchar     *path,
                                 GError         **error)
{
  GtkCssProviderPrivate *priv;
  GError *internal_error = NULL;
  gchar *data;
  gsize length;

  g_return_val_if_fail (GTK_IS_CSS_PROVIDER (css_provider), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  priv = css_provider->priv;

  if (g_file_get_contents (path, &data, &length,
                           &internal_error))
    {
      g_propagate_error (error, internal_error);
      return FALSE;
    }

  if (priv->selectors_info->len > 0)
    g_ptr_array_remove_range (priv->selectors_info, 0, priv->selectors_info->len);

  g_free (priv->filename);
  priv->filename = g_strdup (path);

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
        "*,\n"
        "GtkTreeView > GtkButton {\n"
        "  background-color: @bg_color;\n"
        "  foreground-color: @fg_color;\n"
        "  text-color: @text_color; \n"
        "  base-color: @base_color; \n"
        "  padding: 2 2; \n"
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
        "GtkTreeView > row:nth-child(odd) { \n"
        "  background-color: shade(#ffffff, 0.93); \n"
        "}\n"
        "\n"
        ".tooltip {\n"
        "  background-color: @tooltip_bg_color; \n"
        "  foreground-color: @tooltip_fg_color; \n"
        "}\n"
        "\n"
        "GtkToggleButton:prelight {\n"
        "  text-color: #000; \n"
        "}\n"
        "\n"
        ".button:active {\n"
        "  background-color: #c4c2bd;\n"
        "  foreground-color: #000000;\n"
        "  text-color: #c4c2bd; \n"
        "  base-color: #9c9a94; \n"
        "}\n"
        "\n";

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_data (provider, str, -1, NULL);
    }

  return provider;
}

static gchar *
css_provider_get_theme_dir (void)
{
  const gchar *var;
  gchar *path;

  var = g_getenv ("GTK_DATA_PREFIX");

  if (var)
    path = g_build_filename (var, "share", "themes", NULL);
  else
    path = g_build_filename (GTK_DATA_PREFIX, "share", "themes", NULL);

  return path;
}

GtkCssProvider *
gtk_css_provider_get_named (const gchar *name,
                            const gchar *variant)
{
  static GHashTable *themes = NULL;
  GtkCssProvider *provider;

  if (G_UNLIKELY (!themes))
    themes = g_hash_table_new (g_str_hash, g_str_equal);

  provider = g_hash_table_lookup (themes, name);

  if (!provider)
    {
      const gchar *home_dir;
      gchar *subpath, *path = NULL;

      if (variant)
        subpath = g_strdup_printf ("gtk-%d.0" G_DIR_SEPARATOR_S "gtk-%s.css", GTK_MAJOR_VERSION, variant);
      else
        subpath = g_strdup_printf ("gtk-%d.0" G_DIR_SEPARATOR_S "gtk.css", GTK_MAJOR_VERSION);

      /* First look in the users home directory
       */
      home_dir = g_get_home_dir ();
      if (home_dir)
        {
          path = g_build_filename (home_dir, ".themes", name, subpath, NULL);

          if (!g_file_test (path, G_FILE_TEST_EXISTS))
            {
              g_free (path);
              path = NULL;
            }
        }

      if (!path)
        {
          gchar *theme_dir = css_provider_get_theme_dir ();
          path = g_build_filename (theme_dir, name, subpath, NULL);
          g_free (theme_dir);

          if (!g_file_test (path, G_FILE_TEST_EXISTS))
            {
              g_free (path);
              path = NULL;
            }
        }

      if (path)
        {
          GtkCssProvider *provider;
          GError *error = NULL;

          provider = gtk_css_provider_new ();
          gtk_css_provider_load_from_path (provider, path, &error);

          if (error)
            {
              g_warning ("Could not load named theme \"%s\": %s", name, error->message);
              g_error_free (error);

              g_object_unref (provider);
              provider = NULL;
            }
          else
            g_hash_table_insert (themes, g_strdup (name), provider);
        }
    }

  return provider;
}
