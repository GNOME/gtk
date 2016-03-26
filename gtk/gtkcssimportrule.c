/*
 * Copyright © 2016 Red Hat Inc.
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

#include "config.h"

#include "gtkcssimportruleprivate.h"

#include "gtkcssstylesheetprivate.h"

typedef struct _GtkCssImportRulePrivate GtkCssImportRulePrivate;
struct _GtkCssImportRulePrivate {
  GFile *file;
  GtkCssStyleSheet *style_sheet;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssImportRule, gtk_css_import_rule, GTK_TYPE_CSS_RULE)

static void
gtk_css_import_rule_finalize (GObject *object)
{
  GtkCssImportRule *import_rule = GTK_CSS_IMPORT_RULE (object);
  GtkCssImportRulePrivate *priv = gtk_css_import_rule_get_instance_private (import_rule);

  if (priv->file)
    g_object_unref (priv->file);
  if (priv->style_sheet)
    g_object_unref (priv->style_sheet);

  G_OBJECT_CLASS (gtk_css_import_rule_parent_class)->finalize (object);
}

static void
gtk_css_import_rule_class_init (GtkCssImportRuleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_import_rule_finalize;
}

static void
gtk_css_import_rule_init (GtkCssImportRule *import_rule)
{
}

static GtkCssRule *
gtk_css_import_rule_new (GtkCssRule       *parent_rule,
                         GtkCssStyleSheet *parent_style_sheet)
{
  return g_object_new (GTK_TYPE_CSS_IMPORT_RULE,
                       "parent-rule", parent_rule,
                       "parent-stylesheet", parent_style_sheet,
                       NULL);
}

static gboolean
gtk_css_import_rule_would_recurse (GtkCssImportRule *rule)
{
  GtkCssImportRulePrivate *priv = gtk_css_import_rule_get_instance_private (GTK_CSS_IMPORT_RULE (rule));
  GtkCssStyleSheet *sheet;

  for (sheet = gtk_css_rule_get_parent_style_sheet (GTK_CSS_RULE (rule));
       sheet != NULL;
       sheet = gtk_css_style_sheet_get_parent_style_sheet (sheet))
    {
      GFile *sheet_file = gtk_css_style_sheet_get_file (sheet);

      if (sheet_file && g_file_equal (sheet_file, priv->file))
        return TRUE;
    }

  return FALSE;
}

GtkCssRule *
gtk_css_import_rule_new_parse (GtkCssTokenSource *source,
                               GtkCssRule        *parent_rule,
                               GtkCssStyleSheet  *parent_style_sheet)
{
  GtkCssImportRulePrivate *priv;
  const GtkCssToken *token;
  GtkCssRule *result;
  GtkCssTokenizer *tokenizer;
  GtkCssTokenSource *import_source;
  GError *load_error = NULL;
  GBytes *bytes;
  char *data;
  gsize size;

  g_return_val_if_fail (source != NULL, NULL);
  g_return_val_if_fail (parent_rule == NULL || GTK_IS_CSS_RULE (parent_rule), NULL);
  g_return_val_if_fail (GTK_IS_CSS_STYLE_SHEET (parent_style_sheet), NULL);

  result = gtk_css_import_rule_new (parent_rule, parent_style_sheet);
  gtk_css_token_source_set_consumer (source, G_OBJECT (result));
  priv = gtk_css_import_rule_get_instance_private (GTK_CSS_IMPORT_RULE (result));

  token = gtk_css_token_source_get_token (source);
  if (token->type != GTK_CSS_TOKEN_AT_KEYWORD ||
      g_ascii_strcasecmp (token->string.string, "import") != 0)
    {
      gtk_css_token_source_error (source, "Expected '@import'");
      gtk_css_token_source_consume_all (source);
      return NULL;
    }
  gtk_css_token_source_consume_token (source);

  token = gtk_css_token_source_get_token (source);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_STRING))
    {
      priv->file = gtk_css_token_source_resolve_url (source, token->string.string);
      gtk_css_token_source_consume_token (source);
    }
  else
    {
      priv->file = gtk_css_token_source_consume_url (source);
      if (priv->file == NULL)
        {
          g_object_unref (result);
          return NULL;
        }
    }

  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_SEMICOLON))
    {
      gtk_css_token_source_error (source, "Expected ';' at end of @import");
      gtk_css_token_source_consume_all (source);
      g_object_unref (result);
      return NULL;
    }

  if (gtk_css_import_rule_would_recurse (GTK_CSS_IMPORT_RULE (result)))
    {
      char *path = g_file_get_path (priv->file);
      gtk_css_token_source_error (source,
                                  "Loading '%s' would recurse",
                                  path);
      g_object_unref (result);
      g_free (path);
      bytes = g_bytes_new (NULL, 0);
    }
  else if (g_file_load_contents (priv->file, NULL,
                                 &data, &size,
                                 NULL, &load_error))
    {
      bytes = g_bytes_new_take (data, size);
    }
  else
    {
      gtk_css_token_source_emit_error (source, load_error);
      bytes = g_bytes_new (NULL, 0);
      g_error_free (load_error);
    }
  tokenizer = gtk_css_tokenizer_new (bytes, NULL, NULL, NULL);
  import_source = gtk_css_token_source_new_for_tokenizer (tokenizer, priv->file);
  priv->style_sheet = gtk_css_style_sheet_new_import (import_source, result);
  gtk_css_token_source_unref (import_source);
  gtk_css_tokenizer_unref (tokenizer);

  gtk_css_token_source_consume_token (source);

  return result;
}

GtkCssStyleSheet *
gtk_css_import_rule_get_style_sheet (GtkCssImportRule *rule)
{
  GtkCssImportRulePrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_IMPORT_RULE (rule), NULL);

  priv = gtk_css_import_rule_get_instance_private (rule);

  return priv->style_sheet;
}

