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

#include "gtkcssruleprivate.h"

#include "gtkcssstylesheetprivate.h"

typedef struct _GtkCssRulePrivate GtkCssRulePrivate;
struct _GtkCssRulePrivate {
  GtkCssRule *parent_rule;
  GtkCssStyleSheet *parent_style_sheet;
};

typedef struct _GtkCssTokenSourceAt GtkCssTokenSourceAt;
struct _GtkCssTokenSourceAt {
  GtkCssTokenSource parent;
  GtkCssTokenSource *source;
  guint inside_curly_block :1;
  guint done :1;
};

static void
gtk_css_token_source_at_finalize (GtkCssTokenSource *source)
{
  GtkCssTokenSourceAt *at = (GtkCssTokenSourceAt *) source;

  gtk_css_token_source_unref (at->source);
}

static void
gtk_css_token_source_at_consume_token (GtkCssTokenSource *source,
                                       GObject           *consumer)
{
  GtkCssTokenSourceAt *at = (GtkCssTokenSourceAt *) source;
  const GtkCssToken *token;

  if (at->done)
    return;

  if (gtk_css_token_get_pending_block (source))
    {
      gtk_css_token_source_consume_token_as (at->source, consumer);
      return;
    }

  token = gtk_css_token_source_peek_token (at->source);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SEMICOLON))
    at->done = TRUE;
  else if (at->inside_curly_block && gtk_css_token_is (token, GTK_CSS_TOKEN_CLOSE_CURLY))
    at->done = TRUE;
  else if (gtk_css_token_is (token, GTK_CSS_TOKEN_OPEN_CURLY))
    at->inside_curly_block = TRUE;

  gtk_css_token_source_consume_token_as (at->source, consumer);
}

const GtkCssToken *
gtk_css_token_source_at_peek_token (GtkCssTokenSource *source)
{
  GtkCssTokenSourceAt *at = (GtkCssTokenSourceAt *) source;
  static GtkCssToken eof_token = { GTK_CSS_TOKEN_EOF };

  if (at->done)
    return &eof_token;

  return gtk_css_token_source_peek_token (at->source);
}

static void
gtk_css_token_source_at_error (GtkCssTokenSource *source,
                               const GError      *error)
{
  GtkCssTokenSourceAt *at = (GtkCssTokenSourceAt *) source;

  gtk_css_token_source_emit_error (at->source, error);
}

static GFile *
gtk_css_token_source_at_get_location (GtkCssTokenSource *source)
{
  GtkCssTokenSourceAt *at = (GtkCssTokenSourceAt *) source;

  return gtk_css_token_source_get_location (at->source);
}

static const GtkCssTokenSourceClass GTK_CSS_TOKEN_SOURCE_AT = {
  gtk_css_token_source_at_finalize,
  gtk_css_token_source_at_consume_token,
  gtk_css_token_source_at_peek_token,
  gtk_css_token_source_at_error,
  gtk_css_token_source_at_get_location,
};

static GtkCssTokenSource *
gtk_css_token_source_new_at (GtkCssTokenSource *source)
{
  GtkCssTokenSourceAt *at = gtk_css_token_source_new (GtkCssTokenSourceAt, &GTK_CSS_TOKEN_SOURCE_AT);

  at->source = gtk_css_token_source_ref (source);
  gtk_css_token_source_set_consumer (&at->parent,
                                     gtk_css_token_source_get_consumer (source));

  return &at->parent;
}

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkCssRule, gtk_css_rule, G_TYPE_OBJECT)

static void
gtk_css_rule_class_init (GtkCssRuleClass *klass)
{
}

static void
gtk_css_rule_init (GtkCssRule *rule)
{
}

GtkCssRule *
gtk_css_rule_new_from_at_rule (GtkCssTokenSource *source,
                               GtkCssRule        *parent_rule,
                               GtkCssStyleSheet  *parent_style_sheet)
{
  GtkCssTokenSource *at_source;
  const GtkCssToken *token;

  g_return_val_if_fail (source != NULL, NULL);
  g_return_val_if_fail (parent_rule == NULL || GTK_IS_CSS_RULE (parent_rule), NULL);
  g_return_val_if_fail (GTK_IS_CSS_STYLE_SHEET (parent_style_sheet), NULL);

  at_source = gtk_css_token_source_new_at (source);

  token = gtk_css_token_source_get_token (at_source);
  if (token->type != GTK_CSS_TOKEN_AT_KEYWORD)
    {
      gtk_css_token_source_error (at_source, "Expected an '@'");
      gtk_css_token_source_unref (at_source);
      return NULL;
    }
  else
    {
      const char *name = token->string.string;

      if (g_ascii_strcasecmp (name, "import") == 0)
        {
          gtk_css_token_source_error (source, "Add code to parse @import here");
        }
      else
        {
          gtk_css_token_source_unknown (source, "Unknown rule @%s", name);
        }
    }

  gtk_css_token_source_consume_all (at_source);
  gtk_css_token_source_unref (at_source);

  return NULL;
}

void
gtk_css_rule_print_css_text (GtkCssRule *rule,
                             GString    *string)
{
  GtkCssRuleClass *klass;

  g_return_if_fail (GTK_IS_CSS_RULE (rule));
  g_return_if_fail (string != NULL);

  klass = GTK_CSS_RULE_GET_CLASS (rule);

  klass->get_css_text (rule, string);
}

char *
gtk_css_rule_get_css_text (GtkCssRule *rule)
{
  GString *string;

  g_return_val_if_fail (GTK_IS_CSS_RULE (rule), NULL);

  string = g_string_new (NULL);

  gtk_css_rule_print_css_text (rule, string);

  return g_string_free (string, FALSE);
}

GtkCssRule *
gtk_css_rule_get_parent_rule (GtkCssRule *rule)
{
  GtkCssRulePrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_RULE (rule), NULL);

  priv = gtk_css_rule_get_instance_private (rule);

  return priv->parent_rule;
}

GtkCssStyleSheet *
gtk_css_rule_get_parent_style_sheet (GtkCssRule *rule)
{
  GtkCssRulePrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_RULE (rule), NULL);

  priv = gtk_css_rule_get_instance_private (rule);

  return priv->parent_style_sheet;
}
