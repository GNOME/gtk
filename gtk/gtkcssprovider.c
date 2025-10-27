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

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtk/css/gtkcssvariablevalueprivate.h"
#include "gtkbitmaskprivate.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcsscustompropertypoolprivate.h"
#include "gtkcsskeyframesprivate.h"
#include "gtkcssmediaqueryprivate.h"
#include "gtkcssreferencevalueprivate.h"
#include "gtkcssselectorprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtksettingsprivate.h"
#include "gtkstyleprovider.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkversion.h"
#include "gtktypebuiltins.h"

#include <string.h>
#include <stdlib.h>

#include "gdk/gdkprofilerprivate.h"
#include <cairo-gobject.h>

#define GDK_ARRAY_NAME gtk_css_selectors
#define GDK_ARRAY_TYPE_NAME GtkCssSelectors
#define GDK_ARRAY_ELEMENT_TYPE GtkCssSelector *
#define GDK_ARRAY_PREALLOC 64
#include "gdk/gdkarrayimpl.c"

/* For lack of a better place, assert here that these two definitions match */
G_STATIC_ASSERT (GTK_DEBUG_CSS == GTK_CSS_PARSER_DEBUG_CSS);

/**
 * GtkCssProvider:
 *
 * A style provider for CSS.
 *
 * It is able to parse CSS-like input in order to style widgets.
 *
 * An application can make GTK parse a specific CSS style sheet by calling
 * [method@Gtk.CssProvider.load_from_file] or
 * [method@Gtk.CssProvider.load_from_resource]
 * and adding the provider with [method@Gtk.StyleContext.add_provider] or
 * [func@Gtk.StyleContext.add_provider_for_display].

 * In addition, certain files will be read when GTK is initialized.
 * First, the file `$XDG_CONFIG_HOME/gtk-4.0/gtk.css` is loaded if it
 * exists. Then, GTK loads the first existing file among
 * `XDG_DATA_HOME/themes/THEME/gtk-VERSION/gtk-VARIANT.css`,
 * `$HOME/.themes/THEME/gtk-VERSION/gtk-VARIANT.css`,
 * `$XDG_DATA_DIRS/themes/THEME/gtk-VERSION/gtk-VARIANT.css` and
 * `DATADIR/share/themes/THEME/gtk-VERSION/gtk-VARIANT.css`,
 * where `THEME` is the name of the current theme (see the
 * [property@Gtk.Settings:gtk-theme-name] setting), `VARIANT` is the
 * variant to load (see the
 * [property@Gtk.Settings:gtk-application-prefer-dark-theme] setting),
 * `DATADIR` is the prefix configured when GTK was compiled (unless
 * overridden by the `GTK_DATA_PREFIX` environment variable), and
 * `VERSION` is the GTK version number. If no file is found for the
 * current version, GTK tries older versions all the way back to 4.0.
 *
 * To track errors while loading CSS, connect to the
 * [signal@Gtk.CssProvider::parsing-error] signal.
 */

#define MAX_SELECTOR_LIST_LENGTH 64

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
  guint n_styles;
  guint owns_styles : 1;
  GHashTable *custom_properties;
};

struct _GtkCssScanner
{
  GtkCssProvider *provider;
  GtkCssParser *parser;
  GtkCssScanner *parent;
  GArray *media_features;
  guint skip_count;
};

struct _GtkCssProviderPrivate
{
  GScanner *scanner;

  GtkInterfaceColorScheme prefers_color_scheme;
  GtkInterfaceContrast prefers_contrast;
  GtkReducedMotion prefers_reduced_motion;

  GHashTable *symbolic_colors;
  GHashTable *keyframes;

  GArray *rulesets;
  GtkCssSelectorTree *tree;

  GBytes *source;
  GFile *source_file;
  gboolean needs_rerender;

  GResource *resource;
  char *path;
  GBytes *bytes; /* *no* reference */
};

enum {
  PARSING_ERROR,
  LAST_SIGNAL
};

enum {
   PROP_PREFERS_COLOR_SCHEME = 1,
   PROP_PREFERS_CONTRAST,
   PROP_PREFERS_REDUCED_MOTION,
   NUM_PROPERTIES
};

static GParamSpec *pspecs[NUM_PROPERTIES] = { NULL, };

static gboolean gtk_keep_css_sections = FALSE;

static guint css_provider_signals[LAST_SIGNAL] = { 0 };

static void gtk_css_provider_finalize         (GObject *object);
static void gtk_css_provider_get_property     (GObject               *object,
                                               guint                  property_id,
                                               GValue                *value,
                                               GParamSpec            *pspec);
static void gtk_css_provider_set_property     (GObject               *object,
                                               guint                  property_id,
                                               const GValue          *value,
                                               GParamSpec            *pspec);
static void gtk_css_provider_notify           (GObject               *object,
                                               GParamSpec            *pspec);
static void gtk_css_style_provider_iface_init (GtkStyleProviderInterface *iface);
static void gtk_css_style_provider_emit_error (GtkStyleProvider *provider,
                                               GtkCssSection    *section,
                                               const GError     *error);
static void gtk_css_provider_reset            (GtkCssProvider        *css_provider);


static void gtk_css_provider_load_internal (GtkCssProvider *css_provider,
                                            GtkCssScanner  *scanner,
                                            GFile          *file,
                                            GBytes         *bytes);
static void parse_statement                (GtkCssScanner  *scanner);

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

      if (GTK_DEBUG_CHECK (CSS) ||
          !g_error_matches (error, GTK_CSS_PARSER_WARNING, GTK_CSS_PARSER_WARNING_DEPRECATED))
        g_warning ("Theme parser %s: %s: %s",
                   error->domain == GTK_CSS_PARSER_WARNING ? "warning" : "error",
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

  object_class->finalize = gtk_css_provider_finalize;
  object_class->get_property = gtk_css_provider_get_property;
  object_class->set_property = gtk_css_provider_set_property;
  object_class->notify = gtk_css_provider_notify;

  klass->parsing_error = gtk_css_provider_parsing_error;

  /**
   * GtkCssProvider::parsing-error:
   * @provider: the provider that had a parsing error
   * @section: section the error happened in
   * @error: The parsing error
   *
   * Signals that a parsing error occurred.
   *
   * The expected error values are in the [error@Gtk.CssParserError]
   * and [enum@Gtk.CssParserWarning] enumerations.
   *
   * The @path, @line and @position describe the actual location of
   * the error as accurately as possible.
   *
   * Parsing errors are never fatal, so the parsing will resume after
   * the error. Errors may however cause parts of the given data or
   * even all of it to not be parsed at all. So it is a useful idea
   * to check that the parsing succeeds by connecting to this signal.
   *
   * Errors in the [enum@Gtk.CssParserWarning] enumeration should not
   * be treated as fatal errors.
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
  g_signal_set_va_marshaller (css_provider_signals[PARSING_ERROR],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_VOID__BOXED_BOXEDv);

  /**
   * GtkCssProvider:prefers-color-scheme:
   *
   * Define the color scheme used for rendering the user interface.
   *
   * The UI can be set to either [enum@Gtk.InterfaceColorScheme.LIGHT],
   * or [enum@Gtk.InterfaceColorScheme.DARK] mode. Other values will
   * be interpreted the same as [enum@Gtk.InterfaceColorScheme.LIGHT].
   *
   * This setting is be available for media queries in CSS:
   *
   * ```css
   * @media (prefers-color-scheme: dark) {
   *   // some dark mode styling
   * }
   * ```
   *
   * Changing this setting will reload the style sheet.
   *
   * Since: 4.20
   */
  pspecs[PROP_PREFERS_COLOR_SCHEME] = g_param_spec_enum ("prefers-color-scheme", NULL, NULL,
                                                         GTK_TYPE_INTERFACE_COLOR_SCHEME,
                                                         GTK_INTERFACE_COLOR_SCHEME_DEFAULT,
                                                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkCssProvider:prefers-contrast:
   *
   * Define the contrast mode to use for the user interface.
   *
   * When set to [enum@Gtk.InterfaceContrast.MORE] or
   * [enum@Gtk.InterfaceContrast.LESS], the UI is rendered in
   * high or low contrast.
   *
   * When set to [enum@Gtk.InterfaceContrast.NO_PREFERENCE] (the default),
   * the user interface will be rendered in default mode.
   *
   * This setting is be available for media queries in CSS:
   *
   * ```css
   * @media (prefers-contrast: more) {
   *   // some style with high contrast
   * }
   * ```
   *
   * Changing this setting will reload the style sheet.
   *
   * Since: 4.20
   */
  pspecs[PROP_PREFERS_CONTRAST] = g_param_spec_enum ("prefers-contrast", NULL, NULL,
                                                     GTK_TYPE_INTERFACE_CONTRAST,
                                                     GTK_INTERFACE_CONTRAST_NO_PREFERENCE,
                                                     GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkCssProvider:prefers-reduced-motion:
   *
   * Define the type of reduced motion to use for the user interface.
   *
   * When set to [enum@Gtk.ReducedMotion.REDUCE] the UI is rendered in
   * with reduced motion animations.
   *
   * When set to [enum@Gtk.ReducedMotion.NO_PREFERENCE] (the default),
   * the user interface will be rendered in default mode.
   *
   * This setting is be available for media queries in CSS:
   *
   * ```css
   * @media (prefers-reduced-motion: reduce) {
   *   // some style with reduced motion
   * }
   * ```
   *
   * Changing this setting will reload the style sheet.
   *
   * Since: 4.22
   */
  pspecs[PROP_PREFERS_REDUCED_MOTION] = g_param_spec_enum ("prefers-reduced-motion", NULL, NULL,
                                                           GTK_TYPE_REDUCED_MOTION,
                                                           GTK_REDUCED_MOTION_NO_PREFERENCE,
                                                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, pspecs);
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
}

static void
gtk_css_ruleset_clear (GtkCssRuleset *ruleset)
{
  if (ruleset->owns_styles)
    {
      guint i;

      for (i = 0; i < ruleset->n_styles; i++)
        {
          gtk_css_value_unref (ruleset->styles[i].value);
	  ruleset->styles[i].value = NULL;
	  if (ruleset->styles[i].section)
	    gtk_css_section_unref (ruleset->styles[i].section);
        }
      g_free (ruleset->styles);
      if (ruleset->custom_properties)
        g_hash_table_unref (ruleset->custom_properties);
    }
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

  g_return_if_fail (ruleset->owns_styles || (ruleset->n_styles == 0 && ruleset->custom_properties == NULL));

  ruleset->owns_styles = TRUE;

  for (i = 0; i < ruleset->n_styles; i++)
    {
      if (ruleset->styles[i].property == property)
        {
          gtk_css_value_unref (ruleset->styles[i].value);
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
unref_custom_property_name (gpointer pointer)
{
  GtkCssCustomPropertyPool *pool = gtk_css_custom_property_pool_get ();

  gtk_css_custom_property_pool_unref (pool, GPOINTER_TO_INT (pointer));
}

static void
gtk_css_ruleset_add_custom (GtkCssRuleset       *ruleset,
                            const char          *name,
                            GtkCssVariableValue *value)
{
  GtkCssCustomPropertyPool *pool;
  int id;

  g_return_if_fail (ruleset->owns_styles || (ruleset->n_styles == 0 && ruleset->custom_properties == NULL));

  ruleset->owns_styles = TRUE;

  if (ruleset->custom_properties == NULL)
    {
      ruleset->custom_properties = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                          unref_custom_property_name,
                                                          (GDestroyNotify) gtk_css_variable_value_unref);
    }

  pool = gtk_css_custom_property_pool_get ();
  id = gtk_css_custom_property_pool_add (pool, name);

  g_hash_table_replace (ruleset->custom_properties, GINT_TO_POINTER (id), value);
}

static void
gtk_css_scanner_destroy (GtkCssScanner *scanner)
{
  g_object_unref (scanner->provider);
  gtk_css_parser_unref (scanner->parser);

  /* Discrete media features are all using static strings. */
  g_array_unref (scanner->media_features);

  g_free (scanner);
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

  section = gtk_css_section_new_with_bytes (gtk_css_parser_get_file (parser),
                                            gtk_css_parser_get_bytes (parser),
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
  GtkCssDiscreteMediaFeature feature;

  scanner = g_new0 (GtkCssScanner, 1);

  g_object_ref (provider);
  scanner->provider = provider;
  scanner->parent = parent;

  scanner->parser = gtk_css_parser_new_for_bytes (bytes,
                                                  file,
                                                  gtk_css_scanner_parser_error,
                                                  scanner,
                                                  NULL);

  if (parent == NULL)
    {
      GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (provider);

      scanner->media_features = g_array_sized_new (FALSE, FALSE, sizeof (GtkCssDiscreteMediaFeature), 2);

      feature.name = "prefers-color-scheme";

      switch (priv->prefers_color_scheme)
        {
        case GTK_INTERFACE_COLOR_SCHEME_DEFAULT:
        case GTK_INTERFACE_COLOR_SCHEME_LIGHT:
        case GTK_INTERFACE_COLOR_SCHEME_UNSUPPORTED:
          feature.value = "light";
          break;

        case GTK_INTERFACE_COLOR_SCHEME_DARK:
          feature.value = "dark";
          break;

        default:
          g_assert_not_reached ();
        }

      g_array_append_vals (scanner->media_features, &feature, 1);

      feature.name = "prefers-contrast";

      switch (priv->prefers_contrast)
        {
        case GTK_INTERFACE_CONTRAST_NO_PREFERENCE:
        case GTK_INTERFACE_CONTRAST_UNSUPPORTED:
          feature.value = "no-preference";
          break;

        case GTK_INTERFACE_CONTRAST_MORE:
          feature.value = "more";
          break;

        case GTK_INTERFACE_CONTRAST_LESS:
          feature.value = "less";
          break;

        default:
          g_assert_not_reached ();
        }

      g_array_append_vals (scanner->media_features, &feature, 1);

      feature.name = "prefers-reduced-motion";

      switch (priv->prefers_reduced_motion)
        {
        case GTK_REDUCED_MOTION_NO_PREFERENCE:
          feature.value = "no-preference";
          break;

        case GTK_REDUCED_MOTION_REDUCE:
          feature.value = "reduce";
          break;

        default:
          g_assert_not_reached ();
        }

      g_array_append_vals (scanner->media_features, &feature, 1);
    }
  else
    scanner->media_features = g_array_ref (parent->media_features);

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

static gboolean
gtk_css_scanner_should_commit (GtkCssScanner *scanner)
{
  gboolean commit = (scanner->skip_count == 0);

  if (scanner->parent)
    commit &= gtk_css_scanner_should_commit (scanner->parent);

  return commit;
}

static void
gtk_css_provider_init (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);

  priv->prefers_color_scheme = GTK_INTERFACE_COLOR_SCHEME_DEFAULT;
  priv->prefers_contrast = GTK_INTERFACE_CONTRAST_NO_PREFERENCE;
  priv->needs_rerender = FALSE;

  priv->rulesets = g_array_new (FALSE, FALSE, sizeof (GtkCssRuleset));

  priv->symbolic_colors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 (GDestroyNotify) g_free,
                                                 (GDestroyNotify) gtk_css_value_unref);
  priv->keyframes = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           (GDestroyNotify) g_free,
                                           (GDestroyNotify) _gtk_css_keyframes_unref);
}

static void
verify_tree_match_results (GtkCssProvider        *provider,
                           GtkCssNode            *node,
                           GtkCssSelectorMatches *tree_rules)
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

      for (j = 0; j < gtk_css_selector_matches_get_size (tree_rules); j++)
	{
	  if (ruleset == gtk_css_selector_matches_get (tree_rules, j))
	    {
	      found = TRUE;
	      break;
	    }
	}
      should_match = gtk_css_selector_matches (ruleset->selector, node);
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
gtk_css_style_provider_lookup (GtkStyleProvider             *provider,
                               const GtkCountingBloomFilter *filter,
                               GtkCssNode                   *node,
                               GtkCssLookup                 *lookup,
                               GtkCssChange                 *change)
{
  GtkCssProvider *css_provider = GTK_CSS_PROVIDER (provider);
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);
  GtkCssRuleset *ruleset;
  guint j;
  int i;
  GtkCssSelectorMatches tree_rules;

  if (_gtk_css_selector_tree_is_empty (priv->tree))
    return;

  gtk_css_selector_matches_init (&tree_rules);
  _gtk_css_selector_tree_match_all (priv->tree, filter, node, &tree_rules);

  if (!gtk_css_selector_matches_is_empty (&tree_rules))
    {
      verify_tree_match_results (css_provider, node, &tree_rules);

      for (i = gtk_css_selector_matches_get_size (&tree_rules) - 1; i >= 0; i--)
        {
          ruleset = gtk_css_selector_matches_get (&tree_rules, i);

          if (ruleset->styles == NULL && ruleset->custom_properties == NULL)
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

          if (ruleset->custom_properties)
            {
              GHashTableIter iter;
              gpointer id;
              GtkCssVariableValue *value;

              g_hash_table_iter_init (&iter, ruleset->custom_properties);

              while (g_hash_table_iter_next (&iter, &id, (gpointer) &value))
                _gtk_css_lookup_set_custom (lookup, GPOINTER_TO_INT (id), value);
            }
        }
    }
  gtk_css_selector_matches_clear (&tree_rules);

  if (change)
    *change = gtk_css_selector_tree_get_change_all (priv->tree, filter, node);
}

static gboolean
gtk_css_style_provider_has_section (GtkStyleProvider *provider,
                                    GtkCssSection    *section)
{
  GtkCssProvider *self = GTK_CSS_PROVIDER (provider);
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (self);

  return priv->bytes == gtk_css_section_get_bytes (section);
}

static void
gtk_css_style_provider_iface_init (GtkStyleProviderInterface *iface)
{
  iface->get_color = gtk_css_style_provider_get_color;
  iface->get_keyframes = gtk_css_style_provider_get_keyframes;
  iface->lookup = gtk_css_style_provider_lookup;
  iface->emit_error = gtk_css_style_provider_emit_error;
  iface->has_section = gtk_css_style_provider_has_section;
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

  g_clear_pointer (&priv->source, g_bytes_unref);
  g_clear_object (&priv->source_file);

  if (priv->resource)
    {
      g_resources_unregister (priv->resource);
      g_resource_unref (priv->resource);
      priv->resource = NULL;
    }

  g_free (priv->path);

  G_OBJECT_CLASS (gtk_css_provider_parent_class)->finalize (object);
}

static void
gtk_css_provider_get_property (GObject         *object,
                               guint            prop_id,
                               GValue          *value,
                               GParamSpec      *pspec)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (GTK_CSS_PROVIDER (object));

  switch (prop_id)
    {
    case PROP_PREFERS_COLOR_SCHEME:
      g_value_set_enum (value, priv->prefers_color_scheme);
      break;
    case PROP_PREFERS_CONTRAST:
      g_value_set_enum (value, priv->prefers_contrast);
      break;
    case PROP_PREFERS_REDUCED_MOTION:
      g_value_set_enum (value, priv->prefers_reduced_motion);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_css_provider_set_property (GObject         *object,
                               guint            prop_id,
                               const GValue    *value,
                               GParamSpec      *pspec)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (GTK_CSS_PROVIDER (object));
  int enum_value;

  switch (prop_id)
    {
    case PROP_PREFERS_COLOR_SCHEME:
      enum_value = g_value_get_enum (value);
      if (priv->prefers_color_scheme != enum_value)
        {
          priv->prefers_color_scheme = enum_value;
          priv->needs_rerender = TRUE;
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_PREFERS_CONTRAST:
      enum_value = g_value_get_enum (value);
      if (priv->prefers_contrast != enum_value)
        {
          priv->prefers_contrast = enum_value;
          priv->needs_rerender = TRUE;
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_PREFERS_REDUCED_MOTION:
      enum_value = g_value_get_enum (value);
      if (priv->prefers_reduced_motion != enum_value)
        {
          priv->prefers_reduced_motion = enum_value;
          priv->needs_rerender = TRUE;
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
maybe_rerender_style_sheet (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);

  if (priv->needs_rerender && priv->source != NULL)
    {
      GBytes *source = g_bytes_ref (priv->source);
      GFile *source_file = NULL;

      if (priv->source_file != NULL)
        source_file = g_object_ref (priv->source_file);

      gtk_css_provider_reset (css_provider);
      gtk_css_provider_load_internal (css_provider, NULL, source_file, source);

      priv->source = source;
      priv->source_file = source_file;

      gtk_style_provider_changed (GTK_STYLE_PROVIDER (css_provider));
    }

  priv->needs_rerender = FALSE;
}

static void
gtk_css_provider_notify (GObject    *object,
                         GParamSpec *pspec)
{
  GtkCssProvider *css_provider = GTK_CSS_PROVIDER (object);

  switch (pspec->param_id)
    {
    case PROP_PREFERS_COLOR_SCHEME:
    case PROP_PREFERS_CONTRAST:
      maybe_rerender_style_sheet (css_provider);
      break;
    default:
      break;
    }
}

/**
 * gtk_css_provider_new:
 *
 * Returns a newly created `GtkCssProvider`.
 *
 * Returns: A new `GtkCssProvider`
 */
GtkCssProvider *
gtk_css_provider_new (void)
{
  return g_object_new (GTK_TYPE_CSS_PROVIDER, NULL);
}

static void
css_provider_commit (GtkCssProvider  *css_provider,
                     GtkCssSelectors *selectors,
                     GtkCssRuleset   *ruleset)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);
  guint i;

  if (ruleset->styles == NULL && ruleset->custom_properties == NULL)
    {
      for (i = 0; i < gtk_css_selectors_get_size (selectors); i++)
        _gtk_css_selector_free (gtk_css_selectors_get (selectors, i));
      return;
    }

  for (i = 0; i < gtk_css_selectors_get_size (selectors); i++)
    {
      GtkCssRuleset *new;

      g_array_set_size (priv->rulesets, priv->rulesets->len + 1);

      new = &g_array_index (priv->rulesets, GtkCssRuleset, priv->rulesets->len - 1);
      gtk_css_ruleset_init_copy (new, ruleset, gtk_css_selectors_get (selectors, i));
    }
}

static void
gtk_css_provider_reset (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);
  guint i;

  g_clear_pointer (&priv->source, g_bytes_unref);
  g_clear_object (&priv->source_file);

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
  else if (!gtk_css_scanner_should_commit (scanner))
    {
      /* nothing to do */
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
      GError *load_error = NULL;

      GBytes *bytes = g_file_load_bytes (file, NULL, NULL, &load_error);

      if (bytes == NULL)
        {
          gtk_css_parser_error (scanner->parser,
                                GTK_CSS_PARSER_ERROR_IMPORT,
                                gtk_css_parser_get_block_location (scanner->parser),
                                gtk_css_parser_get_end_location (scanner->parser),
                                "Failed to import: %s",
                                load_error->message);
          g_error_free (load_error);
        }
      else
        {
          gtk_css_provider_load_internal (scanner->provider,
                                          scanner,
                                          file,
                                          bytes);
          g_bytes_unref (bytes);
        }
    }

  g_clear_object (&file);

  return TRUE;
}

static gboolean
parse_media_block (GtkCssScanner *scanner)
{
  gboolean is_match = TRUE;

  if (!gtk_css_parser_try_at_keyword (scanner->parser, "media"))
    return FALSE;

  if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY))
    {
      is_match = gtk_css_media_query_parse (scanner->parser, scanner->media_features);
    }

  if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY))
    {
      gtk_css_parser_error_syntax (scanner->parser, "Expected '{' after @media query");
      return FALSE;
    }

  gtk_css_parser_start_block (scanner->parser);

  if (!is_match)
    scanner->skip_count += 1;

  while (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_CLOSE_CURLY) &&
         !gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    parse_statement (scanner);

  if (!is_match)
    scanner->skip_count -= 1;

  gtk_css_parser_end_block (scanner->parser);

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

  color = gtk_css_color_value_parse (scanner->parser);
  if (color == NULL)
    {
      g_free (name);
      return TRUE;
    }

  if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      g_free (name);
      gtk_css_value_unref (color);
      gtk_css_parser_error_syntax (scanner->parser,
                                   "Missing semicolon at end of color definition");
      return TRUE;
    }

  if (gtk_css_scanner_should_commit (scanner))
    g_hash_table_insert (priv->symbolic_colors, name, color);
  else
    {
      gtk_css_value_unref (color);
      g_free (name);
    }

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
    {
      if (gtk_css_scanner_should_commit (scanner))
        g_hash_table_insert (priv->keyframes, name, keyframes);
      else
        _gtk_css_keyframes_unref (keyframes);
    }

  if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    gtk_css_parser_error_syntax (scanner->parser, "Expected '}' after declarations");

  return TRUE;
}

static void
parse_at_keyword (GtkCssScanner *scanner)
{
  if (parse_media_block (scanner))
    return;

  gtk_css_parser_start_semicolon_block (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY);

  if (!parse_import (scanner) &&
      !parse_color_definition (scanner) &&
      !parse_keyframes (scanner))
    {
      gtk_css_parser_error_syntax (scanner->parser, "Unknown @ rule");
    }

  gtk_css_parser_end_block (scanner->parser);
}

static void
parse_selector_list (GtkCssScanner   *scanner,
                     GtkCssSelectors *selectors)
{
  do {
      GtkCssSelector *select = _gtk_css_selector_parse (scanner->parser);

      if (select == NULL)
        {
          for (int i = 0; i < gtk_css_selectors_get_size (selectors); i++)
            _gtk_css_selector_free (gtk_css_selectors_get (selectors, i));
          gtk_css_selectors_clear (selectors);
          return;
        }

      gtk_css_selectors_append (selectors, select);
    }
  while (gtk_css_parser_try_token (scanner->parser, GTK_CSS_TOKEN_COMMA));
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
    goto out;

  /* This is a custom property */
  if (name[0] == '-' && name[1] == '-')
    {
      GtkCssVariableValue *value;
      GtkCssLocation start_location;
      GtkCssSection *section;

      if (!gtk_css_parser_try_token (scanner->parser, GTK_CSS_TOKEN_COLON))
        {
          gtk_css_parser_error_syntax (scanner->parser, "Expected ':'");
          goto out;
        }

      gtk_css_parser_skip_whitespace (scanner->parser);

      if (gtk_keep_css_sections)
        start_location = *gtk_css_parser_get_start_location (scanner->parser);

      value = gtk_css_parser_parse_value_into_token_stream (scanner->parser);
      if (value == NULL)
        goto out;

      if (gtk_keep_css_sections)
        {
          section = gtk_css_section_new_with_bytes (gtk_css_parser_get_file (scanner->parser),
                                                    gtk_css_parser_get_bytes (scanner->parser),
                                                    &start_location,
                                                    gtk_css_parser_get_start_location (scanner->parser));
        }
      else
        section = NULL;

      if (section != NULL)
        {
          gtk_css_variable_value_set_section (value, section);
          gtk_css_section_unref (section);
        }

      gtk_css_ruleset_add_custom (ruleset, name, value);

      goto out;
    }

  property = _gtk_style_property_lookup (name);

  if (property)
    {
      GtkCssSection *section;
      GtkCssValue *value;

      if (!gtk_css_parser_try_token (scanner->parser, GTK_CSS_TOKEN_COLON))
        {
          gtk_css_parser_error_syntax (scanner->parser, "Expected ':'");
          goto out;
        }

      if (gtk_css_parser_has_references (scanner->parser))
        {
          GtkCssLocation start_location;
          GtkCssVariableValue *var_value;

          gtk_css_parser_skip_whitespace (scanner->parser);

          if (gtk_keep_css_sections)
            start_location = *gtk_css_parser_get_start_location (scanner->parser);

          var_value = gtk_css_parser_parse_value_into_token_stream (scanner->parser);
          if (var_value == NULL)
            goto out;

          if (gtk_keep_css_sections)
            section = gtk_css_section_new_with_bytes (gtk_css_parser_get_file (scanner->parser),
                                                      gtk_css_parser_get_bytes (scanner->parser),
                                                      &start_location,
                                                      gtk_css_parser_get_start_location (scanner->parser));
          else
            section = NULL;

          if (section != NULL)
            {
              gtk_css_variable_value_set_section (var_value, section);
              gtk_css_section_unref (section);
            }

          if (GTK_IS_CSS_SHORTHAND_PROPERTY (property))
            {
              GtkCssShorthandProperty *shorthand = GTK_CSS_SHORTHAND_PROPERTY (property);
              guint i, n;
              GtkCssValue **values;

              n = _gtk_css_shorthand_property_get_n_subproperties (shorthand);

              values = g_new (GtkCssValue *, n);

              for (i = 0; i < n; i++)
                {
                  GtkCssValue *child =
                    _gtk_css_reference_value_new (property,
                                                  var_value,
                                                  gtk_css_parser_get_file (scanner->parser));
                  _gtk_css_reference_value_set_subproperty (child, i);

                  values[i] = _gtk_css_array_value_get_nth (child, i);
                }

              value = _gtk_css_array_value_new_from_array (values, n);
              g_free (values);
            }
          else
            {
              value = _gtk_css_reference_value_new (property,
                                                    var_value,
                                                    gtk_css_parser_get_file (scanner->parser));
            }

          gtk_css_variable_value_unref (var_value);
        }
      else
        {
          value = _gtk_style_property_parse_value (property, scanner->parser);

          if (value == NULL)
            goto out;

          if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
            {
              gtk_css_parser_error_syntax (scanner->parser, "Junk at end of value for %s", property->name);
              gtk_css_value_unref (value);
              goto out;
            }
        }

      if (gtk_keep_css_sections)
        {
          section = gtk_css_section_new_with_bytes (gtk_css_parser_get_file (scanner->parser),
                                                    gtk_css_parser_get_bytes (scanner->parser),
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

              gtk_css_ruleset_add (ruleset, child, gtk_css_value_ref (sub), section);
            }

            gtk_css_value_unref (value);
        }
      else if (GTK_IS_CSS_STYLE_PROPERTY (property))
        {

          gtk_css_ruleset_add (ruleset, GTK_CSS_STYLE_PROPERTY (property), value, section);
        }
      else
        {
          g_assert_not_reached ();
          gtk_css_value_unref (value);
        }

      g_clear_pointer (&section, gtk_css_section_unref);
    }
  else
    {
      gtk_css_parser_error_value (scanner->parser, "No property named \"%s\"", name);
    }

out:
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
  GtkCssSelectors selectors;
  GtkCssRuleset ruleset = { 0, };

  gtk_css_selectors_init (&selectors);

  parse_selector_list (scanner, &selectors);
  if (gtk_css_selectors_get_size (&selectors) == 0)
    {
      gtk_css_parser_skip_until (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY);
      gtk_css_parser_skip (scanner->parser);
      goto out;
    }

  if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY))
    {
      guint i;
      gtk_css_parser_error_syntax (scanner->parser, "Expected '{' after selectors");
      for (i = 0; i < gtk_css_selectors_get_size (&selectors); i++)
        _gtk_css_selector_free (gtk_css_selectors_get (&selectors, i));
      gtk_css_parser_skip_until (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY);
      gtk_css_parser_skip (scanner->parser);
      goto out;
    }

  gtk_css_parser_start_block (scanner->parser);

  parse_declarations (scanner, &ruleset);

  gtk_css_parser_end_block (scanner->parser);

  if (gtk_css_scanner_should_commit (scanner))
    css_provider_commit (scanner->provider, &selectors, &ruleset);
  else
    {
      guint i;

      for (i = 0; i < gtk_css_selectors_get_size (&selectors); i++)
        _gtk_css_selector_free (gtk_css_selectors_get (&selectors, i));
    }

  gtk_css_ruleset_clear (&ruleset);

out:
  gtk_css_selectors_clear (&selectors);
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
  gint64 before G_GNUC_UNUSED;

  before = GDK_PROFILER_CURRENT_TIME;

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

  gdk_profiler_end_mark (before, "Create CSS selector tree", NULL);
}

static void
gtk_css_provider_load_internal (GtkCssProvider *self,
                                GtkCssScanner  *parent,
                                GFile          *file,
                                GBytes         *bytes)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (self);
  gint64 before G_GNUC_UNUSED;
  GtkCssScanner *scanner;

  before = GDK_PROFILER_CURRENT_TIME;

  priv->bytes = bytes;

  scanner = gtk_css_scanner_new (self,
                                  parent,
                                  file,
                                  bytes);

  parse_stylesheet (scanner);

  gtk_css_scanner_destroy (scanner);

  if (parent == NULL)
    gtk_css_provider_postprocess (self);

  if (GDK_PROFILER_IS_RUNNING)
    {
      const char *uri G_GNUC_UNUSED;
      uri = file ? g_file_peek_path (file) : NULL;
      gdk_profiler_end_mark (before, "CSS theme load", uri);
    }
}

/**
 * gtk_css_provider_load_from_data:
 * @css_provider: a `GtkCssProvider`
 * @data: CSS data to be parsed
 * @length: the length of @data in bytes, or -1 for NUL terminated strings
 *
 * Loads @data into @css_provider.
 *
 * This clears any previously loaded information.
 *
 * Deprecated: 4.12: Use [method@Gtk.CssProvider.load_from_string]
 *   or [method@Gtk.CssProvider.load_from_bytes] instead
 */
void
gtk_css_provider_load_from_data (GtkCssProvider  *css_provider,
                                 const char      *data,
                                 gssize           length)
{
  GBytes *bytes;

  g_return_if_fail (GTK_IS_CSS_PROVIDER (css_provider));
  g_return_if_fail (data != NULL);

  if (length < 0)
    length = strlen (data);

  bytes = g_bytes_new (data, length);

  gtk_css_provider_load_from_bytes (css_provider, bytes);

  g_bytes_unref (bytes);
}

/**
 * gtk_css_provider_load_from_string:
 * @css_provider: a `GtkCssProvider`
 * @string: the CSS to load
 *
 * Loads @string into @css_provider.
 *
 * This clears any previously loaded information.
 *
 * Since: 4.12
 */
void
gtk_css_provider_load_from_string (GtkCssProvider *css_provider,
                                   const char     *string)
{
  GBytes *bytes;

  g_return_if_fail (GTK_IS_CSS_PROVIDER (css_provider));
  g_return_if_fail (string != NULL);

  bytes = g_bytes_new (string, strlen (string));

  gtk_css_provider_load_from_bytes (css_provider, bytes);

  g_bytes_unref (bytes);
}

/**
 * gtk_css_provider_load_from_bytes:
 * @css_provider: a `GtkCssProvider`
 * @data: `GBytes` containing the data to load
 *
 * Loads @data into @css_provider.
 *
 * This clears any previously loaded information.
 *
 * Since: 4.12
 */
void
gtk_css_provider_load_from_bytes (GtkCssProvider *css_provider,
                                  GBytes         *data)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);
  g_return_if_fail (GTK_IS_CSS_PROVIDER (css_provider));
  g_return_if_fail (data != NULL);

  gtk_css_provider_reset (css_provider);

  gtk_css_provider_load_internal (css_provider, NULL, NULL, data);

  priv->source = g_bytes_ref (data);
  priv->source_file = NULL;

  gtk_style_provider_changed (GTK_STYLE_PROVIDER (css_provider));
}

/**
 * gtk_css_provider_load_from_file:
 * @css_provider: a `GtkCssProvider`
 * @file: `GFile` pointing to a file to load
 *
 * Loads the data contained in @file into @css_provider.
 *
 * This clears any previously loaded information.
 */
void
gtk_css_provider_load_from_file (GtkCssProvider  *css_provider,
                                 GFile           *file)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (css_provider);
  GBytes *bytes;

  g_return_if_fail (GTK_IS_CSS_PROVIDER (css_provider));
  g_return_if_fail (G_IS_FILE (file));

  gtk_css_provider_reset (css_provider);

  GError *load_error = NULL;

  bytes = g_file_load_bytes (file, NULL, NULL, &load_error);

  if (load_error != NULL)
    {
      GtkCssLocation empty = { 0, };
      GtkCssSection *section = gtk_css_section_new (file, &empty, &empty);

      gtk_css_style_provider_emit_error (GTK_STYLE_PROVIDER (css_provider), section, load_error);
      gtk_css_section_unref (section);

      g_clear_pointer (&priv->bytes, g_bytes_unref);
      g_error_free (load_error);
    }
  else
    {
      gtk_css_provider_load_internal (css_provider, NULL, file, bytes);

      priv->source = bytes;
      priv->source_file = g_object_ref (file);
    }

  gtk_style_provider_changed (GTK_STYLE_PROVIDER (css_provider));
}

/**
 * gtk_css_provider_load_from_path:
 * @css_provider: a `GtkCssProvider`
 * @path: (type filename): the path of a filename to load, in the GLib filename encoding
 *
 * Loads the data contained in @path into @css_provider.
 *
 * This clears any previously loaded information.
 */
void
gtk_css_provider_load_from_path (GtkCssProvider  *css_provider,
                                 const char      *path)
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
 * @css_provider: a `GtkCssProvider`
 * @resource_path: a `GResource` resource path
 *
 * Loads the data contained in the resource at @resource_path into
 * the @css_provider.
 *
 * This clears any previously loaded information.
 */
void
gtk_css_provider_load_from_resource (GtkCssProvider *css_provider,
			             const char     *resource_path)
{
  GFile *file;
  char *uri, *escaped;

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

char *
_gtk_get_theme_dir (void)
{
  const char *var;

  var = g_getenv ("GTK_DATA_PREFIX");
  if (var == NULL)
    var = _gtk_get_data_prefix ();
  return g_build_filename (var, "share", "themes", NULL);
}

/* Return the path that this providers gtk.css was loaded from,
 * if it is part of a theme, otherwise NULL.
 */
const char *
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
 * $dir/$subdir/gtk-4.16/$file
 * $dir/$subdir/gtk-4.14/$file
 *  ...
 * $dir/$subdir/gtk-4.0/$file
 * and return the first found file.
 */
static char *
_gtk_css_find_theme_dir (const char *dir,
                         const char *subdir,
                         const char *name,
                         const char *file)
{
  char *path;
  char *base;

  if (subdir)
    base = g_build_filename (dir, subdir, name, NULL);
  else
    base = g_build_filename (dir, name, NULL);

  if (!g_file_test (base, G_FILE_TEST_IS_DIR))
    {
      g_free (base);
      return NULL;
    }

  for (int i = MINOR; i >= 0; i = i - 2)
    {
      char subsubdir[64];

      g_snprintf (subsubdir, sizeof (subsubdir), "gtk-4.%d", i);
      path = g_build_filename (base, subsubdir, file, NULL);

      if (g_file_test (path, G_FILE_TEST_EXISTS))
        break;

      g_free (path);
      path = NULL;
    }

  g_free (base);

  return path;
}

#undef MINOR

static char *
_gtk_css_find_theme (const char *name,
                     const char *variant)
{
  char file[256];
  char *path;
  const char *const *dirs;
  int i;
  char *dir;

  if (variant && *variant)
    g_snprintf (file, sizeof (file), "gtk-%s.css", variant);
  else
    strcpy (file, "gtk.css");

  /* First look in the user's data directory */
  path = _gtk_css_find_theme_dir (g_get_user_data_dir (), "themes", name, file);
  if (path)
    return path;

  /* Next look in the user's home directory */
  path = _gtk_css_find_theme_dir (g_get_home_dir (), ".themes", name, file);
  if (path)
    {
      GTK_DEBUG (CSS, "Loading custom CSS from $HOME/.themes/ is deprecated");
      return path;
    }

  /* Look in system data directories */
  dirs = g_get_system_data_dirs ();
  for (i = 0; dirs[i]; i++)
    {
      path = _gtk_css_find_theme_dir (dirs[i], "themes", name, file);
      if (path)
        return path;
    }

  /* Finally, try in the default theme directory */
  dir = _gtk_get_theme_dir ();
  path = _gtk_css_find_theme_dir (dir, NULL, name, file);
  g_free (dir);

  return path;
}

/**
 * gtk_css_provider_load_named:
 * @provider: a `GtkCssProvider`
 * @name: A theme name
 * @variant: (nullable): variant to load, for example, "dark", or
 *   %NULL for the default
 *
 * Loads a theme from the usual theme paths.
 *
 * The actual process of finding the theme might change between
 * releases, but it is guaranteed that this function uses the same
 * mechanism to load the theme that GTK uses for loading its own theme.
 *
 * Deprecated: 4.20: Using any of the other theme loaders, combine with media queries.
 */
void
gtk_css_provider_load_named (GtkCssProvider *provider,
                             const char     *name,
                             const char     *variant)
{
  GtkCssProviderPrivate *priv = gtk_css_provider_get_instance_private (provider);
  char *path;
  char *resource_path;

  g_return_if_fail (GTK_IS_CSS_PROVIDER (provider));
  g_return_if_fail (name != NULL);

  gtk_css_provider_reset (provider);

  if (variant != NULL)
    {
      if (strstr (variant, "dark") != NULL)
        priv->prefers_color_scheme = GTK_INTERFACE_COLOR_SCHEME_DARK;
      else
        priv->prefers_color_scheme = GTK_INTERFACE_COLOR_SCHEME_LIGHT;

      if (strstr (variant, "hc") != NULL)
        priv->prefers_contrast = GTK_INTERFACE_CONTRAST_MORE;
      else
        priv->prefers_contrast = GTK_INTERFACE_CONTRAST_NO_PREFERENCE;
    }

  /* try loading the resource for the theme. This is mostly meant for built-in
   * themes.
   */
  if (variant && *variant)
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
      /* Things failed! Fall back! Fall back!
       *
       * We accept the names HighContrast, HighContrastInverse,
       * Adwaita and Adwaita-dark as aliases for the variants
       * of the Default theme.
       */
      if (strcmp (name, "HighContrast") == 0)
        {
          if (g_strcmp0 (variant, "dark") == 0)
            gtk_css_provider_load_named (provider, DEFAULT_THEME_NAME, "hc-dark");
          else
            gtk_css_provider_load_named (provider, DEFAULT_THEME_NAME, "hc");
        }
      else if (strcmp (name, "HighContrastInverse") == 0)
        gtk_css_provider_load_named (provider, DEFAULT_THEME_NAME, "hc-dark");
      else if (strcmp (name, "Adwaita-dark") == 0)
        gtk_css_provider_load_named (provider, DEFAULT_THEME_NAME, "dark");
      else if (strcmp (name, DEFAULT_THEME_NAME) != 0)
        gtk_css_provider_load_named (provider, DEFAULT_THEME_NAME, variant);
      else
        {
          g_return_if_fail (variant != NULL);
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

/* This is looking into a GPtrArray where each "pointer" is actually
 * GINT_TO_POINTER (id), so a and b are pointers to pointer-sized quantities */
static int
compare_custom_properties (gconstpointer a, gconstpointer b, gpointer user_data)
{
  GtkCssCustomPropertyPool *pool = user_data;
  const void * const *ap = a;
  const void * const *bp = b;
  const char *name1, *name2;

  name1 = gtk_css_custom_property_pool_get_name (pool, GPOINTER_TO_INT (*ap));
  name2 = gtk_css_custom_property_pool_get_name (pool, GPOINTER_TO_INT (*bp));

  return strcmp (name1, name2);
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
      g_sort_array (sorted, ruleset->n_styles, sizeof (guint), compare_properties, ruleset->styles);

      for (i = 0; i < ruleset->n_styles; i++)
        {
          PropertyValue *prop = &ruleset->styles[sorted[i]];
          g_string_append (str, "  ");
          g_string_append (str, _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop->property)));
          g_string_append (str, ": ");
          gtk_css_value_print (prop->value, str);
          g_string_append (str, ";\n");
        }

      g_free (sorted);
    }

  if (ruleset->custom_properties)
    {
      GtkCssCustomPropertyPool *pool = gtk_css_custom_property_pool_get ();
      GPtrArray *keys;

      keys = g_hash_table_get_keys_as_ptr_array (ruleset->custom_properties);
      g_ptr_array_sort_with_data (keys, compare_custom_properties, pool);

      for (i = 0; i < keys->len; i++)
        {
          int id = GPOINTER_TO_INT (g_ptr_array_index (keys, i));
          const char *name = gtk_css_custom_property_pool_get_name (pool, id);
          GtkCssVariableValue *value = g_hash_table_lookup (ruleset->custom_properties,
                                                            GINT_TO_POINTER (id));

          g_string_append (str, "  ");
          g_string_append (str, name);
          g_string_append (str, ": ");
          gtk_css_variable_value_print (value, str);
          g_string_append (str, ";\n");
        }

      g_ptr_array_unref (keys);
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
      gtk_css_value_print (color, str);
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
 * Using [method@Gtk.CssProvider.load_from_string] with the return
 * value from this function on a new provider created with
 * [ctor@Gtk.CssProvider.new] will basically create a duplicate
 * of this @provider.
 *
 * Returns: a new string representing the @provider.
 */
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
