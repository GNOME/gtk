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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcssproviderprivate.h"

#include "gtkbitmaskprivate.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcsskeyframesprivate.h"
#include "gtkcssparserprivate.h"
#include "gtkcssselectorprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtksettingsprivate.h"
#include "gtkstyleprovider.h"
#include "gtkstylecontextprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtkwidgetpath.h"
#include "gtkbindings.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkversion.h"

#include <string.h>
#include <stdlib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>

/**
 * SECTION:gtkcssprovider
 * @Short_description: CSS-like styling for widgets
 * @Title: GtkCssProvider
 * @See_also: #GtkStyleContext, #GtkStyleProvider
 *
 * GtkCssProvider is an object implementing the #GtkStyleProvider interface.
 * It is able to parse [CSS-like][css-overview] input in order to style widgets.
 *
 * An application can make GTK+ parse a specific CSS style sheet by calling
 * gtk_css_provider_load_from_file() or gtk_css_provider_load_from_resource()
 * and adding the provider with gtk_style_context_add_provider() or
 * gtk_style_context_add_provider_for_display().

 * In addition, certain files will be read when GTK+ is initialized. First, the
 * file `$XDG_CONFIG_HOME/gtk-4.0/gtk.css` is loaded if it exists. Then, GTK+
 * loads the first existing file among
 * `XDG_DATA_HOME/themes/THEME/gtk-VERSION/gtk-VARIANT.css`,
 * `$HOME/.themes/THEME/gtk-VERSION/gtk-VARIANT.css`,
 * `$XDG_DATA_DIRS/themes/THEME/gtk-VERSION/gtk-VARIANT.css` and
 * `DATADIR/share/themes/THEME/gtk-VERSION/gtk-VARIANT.css`, where `THEME` is the name of
 * the current theme (see the #GtkSettings:gtk-theme-name setting),
 * VARIANT is the variant to load (see the #GtkSettings:gtk-application-prefer-dark-theme setting), `DATADIR`
 * is the prefix configured when GTK+ was compiled (unless overridden by the
 * `GTK_DATA_PREFIX` environment variable), and `VERSION` is the GTK+ version number.
 * If no file is found for the current version, GTK+ tries older versions all the
 * way back to 4.0.
 */

struct _GtkCssProviderClass
{
  GObjectClass parent_class;

  void (* parsing_error)                        (GtkCssProvider  *provider,
                                                 GtkCssSection   *section,
                                                 const GError *   error);
};

typedef struct GtkCssRuleset GtkCssRuleset;
typedef struct _GtkCssScanner GtkCssScanner;
typedef struct _PropertyValue PropertyValue;
typedef enum ParserScope ParserScope;
typedef enum ParserSymbol ParserSymbol;

struct _PropertyValue {
  GtkCssStyleProperty *property;
  GtkCssValue         *value;
  GtkCssSection       *section;
};

struct GtkCssRuleset
{
  GtkCssSelector *selector;
  GtkCssSelectorTree *selector_match;
  PropertyValue *styles;
  GtkBitmask *set_styles;
  guint n_styles;
  guint owns_styles : 1;
};

struct _GtkCssScanner
{
  GtkCssProvider *provider;
  GtkCssParser *parser;
  GtkCssScanner *parent;
};

struct _GtkCssProviderPrivate
{
  GScanner *scanner;

  GHashTable *symbolic_colors;
  GHashTable *keyframes;

  GArray *rulesets;
  GtkCssSelectorTree *tree;
  GResource *resource;
  gchar *path;
};

enum {
  PARSING_ERROR,
  LAST_SIGNAL
};

static gboolean gtk_keep_css_sections = FALSE;

static guint css_provider_signals[LAST_SIGNAL] = { 0 };

static void gtk_css_provider_finalize (GObject *object);
static void gtk_css_style_provider_iface_init (GtkStyleProviderInterface *iface);
static void gtk_css_style_provider_emit_error (GtkStyleProvider *provider,
                                               GtkCssSection    *section,
                                               const GError     *error);

static void
gtk_css_provider_load_internal (GtkCssProvider *css_provider,
                                GtkCssScanner  *scanner,
                                GFile          *file,
                                GBytes         *bytes);

G_DEFINE_TYPE_EXTENDED (GtkCssProvider, gtk_css_provider, G_TYPE_OBJECT, 0,
                        G_ADD_PRIVATE (GtkCssProvider)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_css_style_provider_iface_init));

static void
gtk_css_provider_parsing_error (GtkCssProvider  *provider,
                                GtkCssSection   *section,
                                const GError    *error)
{
  /* Only emit a warning when we have no error handlers. This is our
   * default handlers. And in this case erroneous CSS files are a bug
   * and should be fixed.
   * Note that these warnings can also be triggered by a broken theme
   * that people installed from some weird location on the internets.
   */
  if (!g_signal_has_handler_pending (provider,
                                     css_provider_signals[PARSING_ERROR],
                                     0,
                                     TRUE))
    {
      char *s = gtk_css_section_to_string (section);

      g_warning ("Theme parsing error: %s: %s",
                 s,
                 error->message);

      g_free (s);
    }
}

/* This is exported privately for use in GtkInspector.
 * It is the callers responsibility to reparse the current theme.
 */
void
gtk_css_provider_set_keep_css_sections (void)
{
  gtk_keep_css_sections = TRUE;
}

static void
gtk_css_provider_class_init (GtkCssProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  if (g_getenv ("GTK_CSS_DEBUG"))
    gtk_css_provider_set_keep_css_sections ();

  /**
   * GtkCssProvider::parsing-error:
   * @provider: the provider that had a parsing error
   * @section: section the error happened in
   * @error: The parsing error
   *
   * Signals that a parsing error occurred. the @path, @line and @position
   * describe the actual location of the error as accurately as possible.
   *
   * Parsing errors are never fatal, so the parsing will resume after
   * the error. Errors may however cause parts of the given
   * data or even all of it to not be parsed at all. So it is a useful idea
   * to check that the parsing succeeds by connecting to this signal.
   *
   * Note that this signal may be emitted at any time as the css provider
   * may opt to defer parsing parts or all of the input to a later time
   * than when a loading function was called.
   */
  css_provider_signals[PARSING_ERROR] =
    g_signal_new (I_("parsing-error"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkCssProviderClass, parsing_error),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOXED_BOXED,
                  G_TYPE_NONE, 2, GTK_TYPE_CSS_SECTION, G_TYPE_ERROR);

  object_class->finalize = gtk_css_provider_finalize;

  klass->parsing_error = gtk_css_provider_parsing_error;
}

static void
gtk_css_ruleset_init_copy (GtkCssRuleset       *new,
                           GtkCssRuleset       *ruleset,
                           GtkCssSelector      *selector)
{
  memcpy (new, ruleset, sizeof (GtkCssRuleset));

  new->selector = selector;
  /* First copy takes over ownership */
  if (ruleset->owns_styles)
    ruleset->owns_styles = FALSE;
  if (new->set_styles)
    new->set_styles = _gtk_bitmask_copy (new->set_styles);
}

static void
gtk_css_ruleset_clear (GtkCssRuleset *ruleset)
{
  if (ruleset->owns_styles)
    {
      guint i;

      for (i = 0; i < ruleset->n_styles; i++)
        {
          _gtk_css_value_unref (ruleset->styles[i].value);
	  ruleset->styles[i].value = NULL;
	  if (ruleset->styles[i].section)
	    gtk_css_section_unref (ruleset->styles[i].section);
        }
      g_free (ruleset->styles);
    }
  if (ruleset->set_styles)
    _gtk_bitmask_free (ruleset->set_styles);
  if (ruleset->selector)
    _gtk_css_selector_free (ruleset->selector);

  memset (ruleset, 0, sizeof (GtkCssRuleset));
}

static void
gtk_css_ruleset_add (GtkCssRuleset       *ruleset,
                     GtkCssStyleProperty *property,
                     GtkCssValue         *value,
                     GtkCssSection       *section)
{
  guint i;

  g_return_if_fail (ruleset->owns_styles || ruleset->n_styles == 0);

  if (ruleset->set_styles == NULL)
    ruleset->set_styles = _gtk_bitmask_new ();

  ruleset->set_styles = _gtk_bitmask_set (ruleset->set_styles,
                                          _gtk_css_style_property_get_id (property),
                                          TRUE);

  ruleset->owns_styles = TRUE;

  for (i = 0; i < ruleset->n_styles; i++)
    {
      if (ruleset->styles[i].property == property)
        {
          _gtk_css_value_unref (ruleset->styles[i].value);
	  ruleset->styles[i].value = NULL;
	  if (ruleset->styles[i].section)
	    gtk_css_section_unref (ruleset->styles[i].section);
          break;
        }
    }
  if (i == ruleset->n_styles)
    {
      ruleset->n_styles++;
      ruleset->styles = g_realloc (ruleset->styles, ruleset->n_styles * sizeof (PropertyValue));
      ruleset->styles[i].value = NULL;
      ruleset->styles[i].property = property;
    }

  ruleset->styles[i].value = value;
  if (gtk_keep_css_sections)
    ruleset->styles[i].section = gtk_css_section_ref (section);
  else
    ruleset->styles[i].section = NULL;
}

static void
gtk_css_scanner_destroy (GtkCssScanner *scanner)
{
  g_object_unref (scanner->provider);
  gtk_css_parser_unref (scanner->parser);

  g_slice_free (GtkCssScanner, scanner);
}

static void
gtk_css_style_provider_emit_error (GtkStyleProvider *provider,
                                   GtkCssSection    *section,
                                   const GError     *error)
{
  g_signal_emit (provider, css_provider_signals[PARSING_ERROR], 0, section, error);
}

static void
gtk_css_scanner_parser_error (GtkCssParser         *parser,
                              const GtkCssLocation *start,
                              const GtkCssLocation *end,
                              const GError         *error,
                              gpointer              user_data)
{
  GtkCssScanner *scanner = user_data;
  GtkCssSection *section;

  section = gtk_css_section_new (gtk_css_parser_get_file (parser),
                                 start,
                                 end);

  gtk_css_style_provider_emit_error (GTK_STYLE_PROVIDER (scanner->provider), section, error);

  gtk_css_section_unref (section);
}

static GtkCssScanner *
gtk_css_scanner_new (GtkCssProvider *provider,
                     GtkCssScanner  *parent,
                     GFile          *file,
                     GBytes         *bytes)
{
  GtkCssScanner *scanner;

  scanner = g_slice_new0 (GtkCssScanner);

  g_object_ref (provider);
  scanner->provider = provider;
  scanner->parent = parent;

  scanner->parser = gtk_css_parser_new_for_bytes (bytes,
                                                  file,
                                                  NULL,
                                                  gtk_css_scanner_parser_error,
                                                  scanner,
                                                  NULL);

  return scanner;
}

static gboolean
gtk_css_scanner_would_recurse (GtkCssScanner *scanner,
                               GFile         *file)
{
  while (scanner)
    {
      GFile *parser_file = gtk_css_parser_get_file (scanner->parser);
      if (parser_file && g_file_equal (parser_file, file))
        return TRUE;

      scanner = scanner->parent;
    }

  return FALSE;
}

static void
gtk_css_provider_init (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);

  priv->rulesets = g_array_new (FALSE, FALSE, sizeof (GtkCssRuleset));

  priv->symbolic_colors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 (GDestroyNotify) g_free,
                                                 (GDestroyNotify) _gtk_css_value_unref);
  priv->keyframes = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           (GDestroyNotify) g_free,
                                           (GDestroyNotify) _gtk_css_keyframes_unref);
}

static void
verify_tree_match_results (GtkCssProvider *provider,
			   const GtkCssMatcher *matcher,
			   GPtrArray *tree_rules)
{
#ifdef VERIFY_TREE
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (provider);
  GtkCssRuleset *ruleset;
  gboolean should_match;
  int i, j;

  for (i = 0; i < priv->rulesets->len; i++)
    {
      gboolean found = FALSE;

      ruleset = &g_array_index (priv->rulesets, GtkCssRuleset, i);

      for (j = 0; j < tree_rules->len; j++)
	{
	  if (ruleset == tree_rules->pdata[j])
	    {
	      found = TRUE;
	      break;
	    }
	}
      should_match = _gtk_css_selector_matches (ruleset->selector, matcher);
      if (found != !!should_match)
	{
	  g_error ("expected rule '%s' to %s, but it %s",
		   _gtk_css_selector_to_string (ruleset->selector),
		   should_match ? "match" : "not match",
		   found ? "matched" : "didn't match");
	}
    }
#endif
}

static void
verify_tree_get_change_results (GtkCssProvider *provider,
				const GtkCssMatcher *matcher,
				GtkCssChange change)
{
#ifdef VERIFY_TREE
  {
    GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (provider);
    GtkCssChange verify_change = 0;
    GPtrArray *tree_rules;
    int i;

    tree_rules = _gtk_css_selector_tree_match_all (priv->tree, matcher);
    if (tree_rules)
      {
        verify_tree_match_results (provider, matcher, tree_rules);

        for (i = tree_rules->len - 1; i >= 0; i--)
          {
	    GtkCssRuleset *ruleset;

            ruleset = tree_rules->pdata[i];

            verify_change |= _gtk_css_selector_get_change (ruleset->selector);
          }

        g_ptr_array_free (tree_rules, TRUE);
      }

    if (change != verify_change)
      {
	GString *s;

	s = g_string_new ("");
	g_string_append (s, "expected change ");
        gtk_css_change_print (verify_change, s);
        g_string_append (s, ", but it was ");
        gtk_css_change_print (change, s);
	if ((change & ~verify_change) != 0)
          {
	    g_string_append (s, ", unexpectedly set: ");
            gtk_css_change_print (change & ~verify_change, s);
          }
	if ((~change & verify_change) != 0)
          {
	    g_string_append_printf (s, ", unexpectedly not set: ");
            gtk_css_change_print (~change & verify_change, s);
          }
	g_warning (s->str);
	g_string_free (s, TRUE);
      }
  }
#endif
}


static GtkCssValue *
gtk_css_style_provider_get_color (GtkStyleProvider *provider,
                                  const char       *name)
{
  GtkCssProvider *css_provider = GTK_CSS_PROVIDER (provider);
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);

  return g_hash_table_lookup (priv->symbolic_colors, name);
}

static GtkCssKeyframes *
gtk_css_style_provider_get_keyframes (GtkStyleProvider *provider,
                                      const char       *name)
{
  GtkCssProvider *css_provider = GTK_CSS_PROVIDER (provider);
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);

  return g_hash_table_lookup (priv->keyframes, name);
}

static void
gtk_css_style_provider_lookup (GtkStyleProvider    *provider,
                               const GtkCssMatcher *matcher,
                               GtkCssLookup        *lookup,
                               GtkCssChange        *change)
{
  GtkCssProvider *css_provider = GTK_CSS_PROVIDER (provider);
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);
  GtkCssRuleset *ruleset;
  guint j;
  int i;
  GPtrArray *tree_rules;

  if (_gtk_css_selector_tree_is_empty (priv->tree))
    return;

  tree_rules = _gtk_css_selector_tree_match_all (priv->tree, matcher);
  if (tree_rules)
    {
      verify_tree_match_results (css_provider, matcher, tree_rules);

      for (i = tree_rules->len - 1; i >= 0; i--)
        {
          ruleset = tree_rules->pdata[i];

          if (ruleset->styles == NULL)
            continue;

          for (j = 0; j < ruleset->n_styles; j++)
            {
              GtkCssStyleProperty *prop = ruleset->styles[j].property;
              guint id = _gtk_css_style_property_get_id (prop);

              if (!_gtk_css_lookup_is_missing (lookup, id))
                continue;

              _gtk_css_lookup_set (lookup,
                                   id,
                                   ruleset->styles[j].section,
                                   ruleset->styles[j].value);
            }

          if (_gtk_css_lookup_all_set (lookup))
            break;
        }

      g_ptr_array_free (tree_rules, TRUE);
    }

  if (change)
    {
      GtkCssMatcher change_matcher;

      _gtk_css_matcher_superset_init (&change_matcher, matcher, GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_CLASS);

      *change = _gtk_css_selector_tree_get_change_all (priv->tree, &change_matcher);
      verify_tree_get_change_results (css_provider, &change_matcher, *change);
    }
}

static void
gtk_css_style_provider_iface_init (GtkStyleProviderInterface *iface)
{
  iface->get_color = gtk_css_style_provider_get_color;
  iface->get_keyframes = gtk_css_style_provider_get_keyframes;
  iface->lookup = gtk_css_style_provider_lookup;
  iface->emit_error = gtk_css_style_provider_emit_error;
}

static void
gtk_css_provider_finalize (GObject *object)
{
  GtkCssProvider *css_provider = GTK_CSS_PROVIDER (object);
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);
  guint i;

  for (i = 0; i < priv->rulesets->len; i++)
    gtk_css_ruleset_clear (&g_array_index (priv->rulesets, GtkCssRuleset, i));

  g_array_free (priv->rulesets, TRUE);
  _gtk_css_selector_tree_free (priv->tree);

  g_hash_table_destroy (priv->symbolic_colors);
  g_hash_table_destroy (priv->keyframes);

  if (priv->resource)
    {
      g_resources_unregister (priv->resource);
      g_resource_unref (priv->resource);
      priv->resource = NULL;
    }

  g_free (priv->path);

  G_OBJECT_CLASS (gtk_css_provider_parent_class)->finalize (object);
}

/**
 * gtk_css_provider_new:
 *
 * Returns a newly created #GtkCssProvider.
 *
 * Returns: A new #GtkCssProvider
 **/
GtkCssProvider *
gtk_css_provider_new (void)
{
  return g_object_new (GTK_TYPE_CSS_PROVIDER, NULL);
}

static void
css_provider_commit (GtkCssProvider *css_provider,
                     GSList         *selectors,
                     GtkCssRuleset  *ruleset)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);
  GSList *l;

  for (l = selectors; l; l = l->next)
    {
      GtkCssRuleset new;

      gtk_css_ruleset_init_copy (&new, ruleset, l->data);

      g_array_append_val (priv->rulesets, new);
    }

  g_slist_free (selectors);
}

static void
gtk_css_provider_reset (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);
  guint i;

  if (priv->resource)
    {
      g_resources_unregister (priv->resource);
      g_resource_unref (priv->resource);
      priv->resource = NULL;
    }

  if (priv->path)
    {
      g_free (priv->path);
      priv->path = NULL;
    }

  g_hash_table_remove_all (priv->symbolic_colors);
  g_hash_table_remove_all (priv->keyframes);

  for (i = 0; i < priv->rulesets->len; i++)
    gtk_css_ruleset_clear (&g_array_index (priv->rulesets, GtkCssRuleset, i));
  g_array_set_size (priv->rulesets, 0);
  _gtk_css_selector_tree_free (priv->tree);
  priv->tree = NULL;
}

static gboolean
parse_import (GtkCssScanner *scanner)
{
  GFile *file;

  if (!gtk_css_parser_try_at_keyword (scanner->parser, "import"))
    return FALSE;

  if (gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_STRING))
    {
      char *url;

      url = gtk_css_parser_consume_string (scanner->parser);
      if (url)
        {
          file = gtk_css_parser_resolve_url (scanner->parser, url);
          if (file == NULL)
            {
              gtk_css_parser_error_import (scanner->parser,
                                           "Could not resolve \"%s\" to a valid URL",
                                           url);
            }
          g_free (url);
        }
      else
        file = NULL;
    }
  else
    {
      char *url = gtk_css_parser_consume_url (scanner->parser);
      if (url)
        {
          file = gtk_css_parser_resolve_url (scanner->parser, url);
          g_free (url);
        }
      else
        file = NULL;
    }

  if (file == NULL)
    {
      /* nothing to do */
    }
  else if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_error_syntax (scanner->parser, "Expected ';'");
    }
  else if (gtk_css_scanner_would_recurse (scanner, file))
    {
       char *path = g_file_get_path (file);
       gtk_css_parser_error (scanner->parser,
                             GTK_CSS_PARSER_ERROR_IMPORT,
                             gtk_css_parser_get_block_location (scanner->parser),
                             gtk_css_parser_get_end_location (scanner->parser),
                             "Loading '%s' would recurse",
                             path);
       g_free (path);
    }
  else
    {
      gtk_css_provider_load_internal (scanner->provider,
                                      scanner,
                                      file,
                                      NULL);
    }

  g_clear_object (&file);

  return TRUE;
}

static gboolean
parse_color_definition (GtkCssScanner *scanner)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (scanner->provider);
  GtkCssValue *color;
  char *name;

  if (!gtk_css_parser_try_at_keyword (scanner->parser, "define-color"))
    return FALSE;

  name = gtk_css_parser_consume_ident (scanner->parser);
  if (name == NULL)
    return TRUE;

  color = _gtk_css_color_value_parse (scanner->parser);
  if (color == NULL)
    {
      g_free (name);
      return TRUE;
    }

  if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      g_free (name);
      _gtk_css_value_unref (color);
      gtk_css_parser_error_syntax (scanner->parser,
                                   "Missing semicolon at end of color definition");
      return TRUE;
    }

  g_hash_table_insert (priv->symbolic_colors, name, color);

  return TRUE;
}

static gboolean
parse_keyframes (GtkCssScanner *scanner)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (scanner->provider);
  GtkCssKeyframes *keyframes;
  char *name;

  if (!gtk_css_parser_try_at_keyword (scanner->parser, "keyframes"))
    return FALSE;

  name = gtk_css_parser_consume_ident (scanner->parser);
  if (name == NULL)
    return FALSE;

  if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_error_syntax (scanner->parser, "Expected '{' for keyframes");
      return FALSE;
    }

  gtk_css_parser_end_block_prelude (scanner->parser);

  keyframes = _gtk_css_keyframes_parse (scanner->parser);
  if (keyframes != NULL)
    g_hash_table_insert (priv->keyframes, name, keyframes);

  if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    gtk_css_parser_error_syntax (scanner->parser, "Expected '}' after declarations");

  return TRUE;
}

static void
parse_at_keyword (GtkCssScanner *scanner)
{
  gtk_css_parser_start_semicolon_block (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY);

  if (!parse_import (scanner) &&
      !parse_color_definition (scanner) &&
      !parse_keyframes (scanner))
    {
      gtk_css_parser_error_syntax (scanner->parser, "Unknown @ rule");
    }

  gtk_css_parser_end_block (scanner->parser);
}

static GSList *
parse_selector_list (GtkCssScanner *scanner)
{
  GSList *selectors = NULL;

  do {
      GtkCssSelector *select = _gtk_css_selector_parse (scanner->parser);

      if (select == NULL)
        return NULL;

      selectors = g_slist_prepend (selectors, select);
    }
  while (gtk_css_parser_try_token (scanner->parser, GTK_CSS_TOKEN_COMMA));

  return selectors;
}

static void
parse_declaration (GtkCssScanner *scanner,
                   GtkCssRuleset *ruleset)
{
  GtkStyleProperty *property;
  char *name;

  /* advance the location over whitespace */
  gtk_css_parser_get_token (scanner->parser);
  gtk_css_parser_start_semicolon_block (scanner->parser, GTK_CSS_TOKEN_EOF);

  if (gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_warn_syntax (scanner->parser, "Empty declaration");
      gtk_css_parser_end_block (scanner->parser);
      return;
    }

  name = gtk_css_parser_consume_ident (scanner->parser);
  if (name == NULL)
    {
      gtk_css_parser_end_block (scanner->parser);
      return;
    }

  property = _gtk_style_property_lookup (name);

  if (property)
    {
      GtkCssSection *section;
      GtkCssValue *value;

      if (!gtk_css_parser_try_token (scanner->parser, GTK_CSS_TOKEN_COLON))
        {
          gtk_css_parser_error_syntax (scanner->parser, "Expected ':'");
          g_free (name);
          gtk_css_parser_end_block (scanner->parser);
          return;
        }

      value = _gtk_style_property_parse_value (property,
                                               scanner->parser);

      if (value == NULL)
        {
          gtk_css_parser_end_block (scanner->parser);
          return;
        }

      if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
        {
          gtk_css_parser_error_syntax (scanner->parser, "Junk at end of value for %s", property->name);
          gtk_css_parser_end_block (scanner->parser);
          return;
        }

      if (gtk_keep_css_sections)
        {
          section = gtk_css_section_new (gtk_css_parser_get_file (scanner->parser),
                                         gtk_css_parser_get_block_location (scanner->parser),
                                         gtk_css_parser_get_end_location (scanner->parser));
        }
      else
        section = NULL;

      if (GTK_IS_CSS_SHORTHAND_PROPERTY (property))
        {
          GtkCssShorthandProperty *shorthand = GTK_CSS_SHORTHAND_PROPERTY (property);
          guint i;

          for (i = 0; i < _gtk_css_shorthand_property_get_n_subproperties (shorthand); i++)
            {
              GtkCssStyleProperty *child = _gtk_css_shorthand_property_get_subproperty (shorthand, i);
              GtkCssValue *sub = _gtk_css_array_value_get_nth (value, i);
              
              gtk_css_ruleset_add (ruleset, child, _gtk_css_value_ref (sub), section);
            }
          
            _gtk_css_value_unref (value);
        }
      else if (GTK_IS_CSS_STYLE_PROPERTY (property))
        {

          gtk_css_ruleset_add (ruleset, GTK_CSS_STYLE_PROPERTY (property), value, section);
        }
      else
        {
          g_assert_not_reached ();
          _gtk_css_value_unref (value);
        }

      g_clear_pointer (&section, gtk_css_section_unref);
    }
  else
    {
      gtk_css_parser_error_value (scanner->parser, "No property named \"%s\"", name);
    }

  g_free (name);

  gtk_css_parser_end_block (scanner->parser);
}

static void
parse_declarations (GtkCssScanner *scanner,
                    GtkCssRuleset *ruleset)
{
  while (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      parse_declaration (scanner, ruleset);
    }
}

static void
parse_ruleset (GtkCssScanner *scanner)
{
  GSList *selectors;
  GtkCssRuleset ruleset = { 0, };

  selectors = parse_selector_list (scanner);
  if (selectors == NULL)
    {
      gtk_css_parser_skip_until (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY);
      gtk_css_parser_skip (scanner->parser);
      return;
    }

  if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY))
    {
      gtk_css_parser_error_syntax (scanner->parser, "Expected '{' after selectors");
      g_slist_free_full (selectors, (GDestroyNotify) _gtk_css_selector_free);
      gtk_css_parser_skip_until (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY);
      gtk_css_parser_skip (scanner->parser);
      return;
    }

  gtk_css_parser_start_block (scanner->parser);

  parse_declarations (scanner, &ruleset);

  gtk_css_parser_end_block (scanner->parser);

  css_provider_commit (scanner->provider, selectors, &ruleset);
  gtk_css_ruleset_clear (&ruleset);
}

static void
parse_statement (GtkCssScanner *scanner)
{
  if (gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_AT_KEYWORD))
    parse_at_keyword (scanner);
  else
    parse_ruleset (scanner);
}

static void
parse_stylesheet (GtkCssScanner *scanner)
{
  while (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      if (gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_CDO) ||
          gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_CDC))
        {
          gtk_css_parser_consume_token (scanner->parser);
          continue;
        }

      parse_statement (scanner);
    }
}

static int
gtk_css_provider_compare_rule (gconstpointer a_,
                               gconstpointer b_)
{
  const GtkCssRuleset *a = (const GtkCssRuleset *) a_;
  const GtkCssRuleset *b = (const GtkCssRuleset *) b_;
  int compare;

  compare = _gtk_css_selector_compare (a->selector, b->selector);
  if (compare != 0)
    return compare;

  return 0;
}

static void
gtk_css_provider_postprocess (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);
  GtkCssSelectorTreeBuilder *builder;
  guint i;

  g_array_sort (priv->rulesets, gtk_css_provider_compare_rule);

  builder = _gtk_css_selector_tree_builder_new ();
  for (i = 0; i < priv->rulesets->len; i++)
    {
      GtkCssRuleset *ruleset;

      ruleset = &g_array_index (priv->rulesets, GtkCssRuleset, i);

      _gtk_css_selector_tree_builder_add (builder,
					  ruleset->selector,
					  &ruleset->selector_match,
					  ruleset);
    }

  priv->tree = _gtk_css_selector_tree_builder_build (builder);
  _gtk_css_selector_tree_builder_free (builder);

#ifndef VERIFY_TREE
  for (i = 0; i < priv->rulesets->len; i++)
    {
      GtkCssRuleset *ruleset;

      ruleset = &g_array_index (priv->rulesets, GtkCssRuleset, i);

      _gtk_css_selector_free (ruleset->selector);
      ruleset->selector = NULL;
    }
#endif
}

static void
gtk_css_provider_load_internal (GtkCssProvider *self,
                                GtkCssScanner  *parent,
                                GFile          *file,
                                GBytes         *bytes)
{
  if (bytes == NULL)
    {
      GError *load_error = NULL;

      bytes = g_file_load_bytes (file, NULL, NULL, &load_error);

      if (bytes == NULL)
        {
          if (parent == NULL)
            {
              GtkCssLocation empty = { 0, };
              GtkCssSection *section = gtk_css_section_new (file, &empty, &empty);

              gtk_css_style_provider_emit_error (GTK_STYLE_PROVIDER (self), section, load_error);
              gtk_css_section_unref (section);
            }
          else
            {
              gtk_css_parser_error (parent->parser, 
                                    GTK_CSS_PARSER_ERROR_IMPORT,
                                    gtk_css_parser_get_block_location (parent->parser),
                                    gtk_css_parser_get_end_location (parent->parser),
                                    "Failed to import: %s",
                                    load_error->message);
            }
        }
    }

  if (bytes)
    {
      GtkCssScanner *scanner;

      scanner = gtk_css_scanner_new (self,
                                     parent,
                                     file,
                                     bytes);

      parse_stylesheet (scanner);

      gtk_css_scanner_destroy (scanner);

      if (parent == NULL)
        gtk_css_provider_postprocess (self);

      g_bytes_unref (bytes);
    }
}

/**
 * gtk_css_provider_load_from_data:
 * @css_provider: a #GtkCssProvider
 * @data: (array length=length) (element-type guint8): CSS data loaded in memory
 * @length: the length of @data in bytes, or -1 for NUL terminated strings. If
 *   @length is not -1, the code will assume it is not NUL terminated and will
 *   potentially do a copy.
 *
 * Loads @data into @css_provider, and by doing so clears any previously loaded
 * information.
 **/
void
gtk_css_provider_load_from_data (GtkCssProvider  *css_provider,
                                 const gchar     *data,
                                 gssize           length)
{
  GBytes *bytes;

  g_return_if_fail (GTK_IS_CSS_PROVIDER (css_provider));
  g_return_if_fail (data != NULL);

  if (length < 0)
    length = strlen (data);

  bytes = g_bytes_new_static (data, length);

  gtk_css_provider_reset (css_provider);

  g_bytes_ref (bytes);
  gtk_css_provider_load_internal (css_provider, NULL, NULL, bytes);
  g_bytes_unref (bytes);

  gtk_style_provider_changed (GTK_STYLE_PROVIDER (css_provider));
}

/**
 * gtk_css_provider_load_from_file:
 * @css_provider: a #GtkCssProvider
 * @file: #GFile pointing to a file to load
 *
 * Loads the data contained in @file into @css_provider, making it
 * clear any previously loaded information.
 **/
void
gtk_css_provider_load_from_file (GtkCssProvider  *css_provider,
                                 GFile           *file)
{
  g_return_if_fail (GTK_IS_CSS_PROVIDER (css_provider));
  g_return_if_fail (G_IS_FILE (file));

  gtk_css_provider_reset (css_provider);

  gtk_css_provider_load_internal (css_provider, NULL, file, NULL);

  gtk_style_provider_changed (GTK_STYLE_PROVIDER (css_provider));
}

/**
 * gtk_css_provider_load_from_path:
 * @css_provider: a #GtkCssProvider
 * @path: the path of a filename to load, in the GLib filename encoding
 *
 * Loads the data contained in @path into @css_provider, making it clear
 * any previously loaded information.
 **/
void
gtk_css_provider_load_from_path (GtkCssProvider  *css_provider,
                                 const gchar     *path)
{
  GFile *file;

  g_return_if_fail (GTK_IS_CSS_PROVIDER (css_provider));
  g_return_if_fail (path != NULL);

  file = g_file_new_for_path (path);
  
  gtk_css_provider_load_from_file (css_provider, file);

  g_object_unref (file);
}

/**
 * gtk_css_provider_load_from_resource:
 * @css_provider: a #GtkCssProvider
 * @resource_path: a #GResource resource path
 *
 * Loads the data contained in the resource at @resource_path into
 * the #GtkCssProvider, clearing any previously loaded information.
 *
 * To track errors while loading CSS, connect to the
 * #GtkCssProvider::parsing-error signal.
 */
void
gtk_css_provider_load_from_resource (GtkCssProvider *css_provider,
			             const gchar    *resource_path)
{
  GFile *file;
  gchar *uri, *escaped;

  g_return_if_fail (GTK_IS_CSS_PROVIDER (css_provider));
  g_return_if_fail (resource_path != NULL);

  escaped = g_uri_escape_string (resource_path,
				 G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
  uri = g_strconcat ("resource://", escaped, NULL);
  g_free (escaped);

  file = g_file_new_for_uri (uri);
  g_free (uri);

  gtk_css_provider_load_from_file (css_provider, file);

  g_object_unref (file);
}

gchar *
_gtk_get_theme_dir (void)
{
  const gchar *var;

  var = g_getenv ("GTK_DATA_PREFIX");
  if (var == NULL)
    var = _gtk_get_data_prefix ();
  return g_build_filename (var, "share", "themes", NULL);
}

/* Return the path that this providers gtk.css was loaded from,
 * if it is part of a theme, otherwise NULL.
 */
const gchar *
_gtk_css_provider_get_theme_dir (GtkCssProvider *provider)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (provider);

  return priv->path;
}

#if (GTK_MINOR_VERSION % 2)
#define MINOR (GTK_MINOR_VERSION + 1)
#else
#define MINOR GTK_MINOR_VERSION
#endif

/*
 * Look for
 * $dir/$subdir/gtk-4.16/gtk-$variant.css
 * $dir/$subdir/gtk-4.14/gtk-$variant.css
 *  ...
 * $dir/$subdir/gtk-4.0/gtk-$variant.css
 * and return the first found file.
 */
static gchar *
_gtk_css_find_theme_dir (const gchar *dir,
                         const gchar *subdir,
                         const gchar *name,
                         const gchar *variant)
{
  gchar *file;
  gchar *base;
  gchar *subsubdir;
  gint i;
  gchar *path;

  if (variant)
    file = g_strconcat ("gtk-", variant, ".css", NULL);
  else
    file = g_strdup ("gtk.css");

  if (subdir)
    base = g_build_filename (dir, subdir, name, NULL);
  else
    base = g_build_filename (dir, name, NULL);

  for (i = MINOR; i >= 0; i = i - 2)
    {
      subsubdir = g_strdup_printf ("gtk-4.%d", i);
      path = g_build_filename (base, subsubdir, file, NULL);
      g_free (subsubdir);

      if (g_file_test (path, G_FILE_TEST_EXISTS))
        break;

      g_free (path);
      path = NULL;
    }

  g_free (file);
  g_free (base);

  return path;
}

#undef MINOR

static gchar *
_gtk_css_find_theme (const gchar *name,
                     const gchar *variant)
{
  gchar *path;
  const char *const *dirs;
  int i;
  char *dir;

  /* First look in the user's data directory */
  path = _gtk_css_find_theme_dir (g_get_user_data_dir (), "themes", name, variant);
  if (path)
    return path;

  /* Next look in the user's home directory */
  path = _gtk_css_find_theme_dir (g_get_home_dir (), ".themes", name, variant);
  if (path)
    return path;

  /* Look in system data directories */
  dirs = g_get_system_data_dirs ();
  for (i = 0; dirs[i]; i++)
    {
      path = _gtk_css_find_theme_dir (dirs[i], "themes", name, variant);
      if (path)
        return path;
    }

  /* Finally, try in the default theme directory */
  dir = _gtk_get_theme_dir ();
  path = _gtk_css_find_theme_dir (dir, NULL, name, variant);
  g_free (dir);

  return path;
}

/**
 * gtk_css_provider_load_named:
 * @provider: a #GtkCssProvider
 * @name: A theme name
 * @variant: (allow-none): variant to load, for example, "dark", or
 *     %NULL for the default
 *
 * Loads a theme from the usual theme paths. The actual process of
 * finding the theme might change between releases, but it is
 * guaranteed that this function uses the same mechanism to load the
 * theme that GTK uses for loading its own theme.
 **/
void
gtk_css_provider_load_named (GtkCssProvider *provider,
                             const gchar    *name,
                             const gchar    *variant)
{
  gchar *path;
  gchar *resource_path;

  g_return_if_fail (GTK_IS_CSS_PROVIDER (provider));
  g_return_if_fail (name != NULL);

  gtk_css_provider_reset (provider);

  /* try loading the resource for the theme. This is mostly meant for built-in
   * themes.
   */
  if (variant)
    resource_path = g_strdup_printf ("/org/gtk/libgtk/theme/%s/gtk-%s.css", name, variant);
  else
    resource_path = g_strdup_printf ("/org/gtk/libgtk/theme/%s/gtk.css", name);

  if (g_resources_get_info (resource_path, 0, NULL, NULL, NULL))
    {
      gtk_css_provider_load_from_resource (provider, resource_path);
      g_free (resource_path);
      return;
    }
  g_free (resource_path);

  /* Next try looking for files in the various theme directories. */
  path = _gtk_css_find_theme (name, variant);
  if (path)
    {
      GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (provider);
      char *dir, *resource_file;
      GResource *resource;

      dir = g_path_get_dirname (path);
      resource_file = g_build_filename (dir, "gtk.gresource", NULL);
      resource = g_resource_load (resource_file, NULL);
      g_free (resource_file);

      if (resource != NULL)
        g_resources_register (resource);

      gtk_css_provider_load_from_path (provider, path);

      /* Only set this after load, as load_from_path will clear it */
      priv->resource = resource;
      priv->path = dir;

      g_free (path);
    }
  else
    {
      /* Things failed! Fall back! Fall back! */

      if (variant)
        {
          /* If there was a variant, try without */
          gtk_css_provider_load_named (provider, name, NULL);
        }
      else
        {
          /* Worst case, fall back to the default */
          g_return_if_fail (!g_str_equal (name, DEFAULT_THEME_NAME)); /* infloop protection */
          gtk_css_provider_load_named (provider, DEFAULT_THEME_NAME, NULL);
        }
    }
}

static int
compare_properties (gconstpointer a, gconstpointer b, gpointer style)
{
  const guint *ua = a;
  const guint *ub = b;
  PropertyValue *styles = style;

  return strcmp (_gtk_style_property_get_name (GTK_STYLE_PROPERTY (styles[*ua].property)),
                 _gtk_style_property_get_name (GTK_STYLE_PROPERTY (styles[*ub].property)));
}

static void
gtk_css_ruleset_print (const GtkCssRuleset *ruleset,
                       GString             *str)
{
  guint i;

  _gtk_css_selector_tree_match_print (ruleset->selector_match, str);

  g_string_append (str, " {\n");

  if (ruleset->styles)
    {
      guint *sorted = g_new (guint, ruleset->n_styles);

      for (i = 0; i < ruleset->n_styles; i++)
        sorted[i] = i;

      /* so the output is identical for identical selector styles */
      g_qsort_with_data (sorted, ruleset->n_styles, sizeof (guint), compare_properties, ruleset->styles);

      for (i = 0; i < ruleset->n_styles; i++)
        {
          PropertyValue *prop = &ruleset->styles[sorted[i]];
          g_string_append (str, "  ");
          g_string_append (str, _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop->property)));
          g_string_append (str, ": ");
          _gtk_css_value_print (prop->value, str);
          g_string_append (str, ";\n");
        }

      g_free (sorted);
    }

  g_string_append (str, "}\n");
}

static void
gtk_css_provider_print_colors (GHashTable *colors,
                               GString    *str)
{
  GList *keys, *walk;

  keys = g_hash_table_get_keys (colors);
  /* so the output is identical for identical styles */
  keys = g_list_sort (keys, (GCompareFunc) strcmp);

  for (walk = keys; walk; walk = walk->next)
    {
      const char *name = walk->data;
      GtkCssValue *color = g_hash_table_lookup (colors, (gpointer) name);

      g_string_append (str, "@define-color ");
      g_string_append (str, name);
      g_string_append (str, " ");
      _gtk_css_value_print (color, str);
      g_string_append (str, ";\n");
    }

  g_list_free (keys);
}

static void
gtk_css_provider_print_keyframes (GHashTable *keyframes,
                                  GString    *str)
{
  GList *keys, *walk;

  keys = g_hash_table_get_keys (keyframes);
  /* so the output is identical for identical styles */
  keys = g_list_sort (keys, (GCompareFunc) strcmp);

  for (walk = keys; walk; walk = walk->next)
    {
      const char *name = walk->data;
      GtkCssKeyframes *keyframe = g_hash_table_lookup (keyframes, (gpointer) name);

      if (str->len > 0)
        g_string_append (str, "\n");
      g_string_append (str, "@keyframes ");
      g_string_append (str, name);
      g_string_append (str, " {\n");
      _gtk_css_keyframes_print (keyframe, str);
      g_string_append (str, "}\n");
    }

  g_list_free (keys);
}

/**
 * gtk_css_provider_to_string:
 * @provider: the provider to write to a string
 *
 * Converts the @provider into a string representation in CSS
 * format.
 *
 * Using gtk_css_provider_load_from_data() with the return value
 * from this function on a new provider created with
 * gtk_css_provider_new() will basically create a duplicate of
 * this @provider.
 *
 * Returns: a new string representing the @provider.
 **/
char *
gtk_css_provider_to_string (GtkCssProvider *provider)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (provider);
  GString *str;
  guint i;

  g_return_val_if_fail (GTK_IS_CSS_PROVIDER (provider), NULL);

  str = g_string_new ("");

  gtk_css_provider_print_colors (priv->symbolic_colors, str);
  gtk_css_provider_print_keyframes (priv->keyframes, str);

  for (i = 0; i < priv->rulesets->len; i++)
    {
      if (str->len != 0)
        g_string_append (str, "\n");
      gtk_css_ruleset_print (&g_array_index (priv->rulesets, GtkCssRuleset, i), str);
    }

  return g_string_free (str, FALSE);
}

