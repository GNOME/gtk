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

#include "gtkcsskeyframesruleprivate.h"

#include "gtkcsskeyframeruleprivate.h"
#include "gtkcssstylesheetprivate.h"

typedef struct _GtkCssKeyframesRulePrivate GtkCssKeyframesRulePrivate;
struct _GtkCssKeyframesRulePrivate {
  char *name;
  GtkCssRuleList *rules;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssKeyframesRule, gtk_css_keyframes_rule, GTK_TYPE_CSS_RULE)

typedef struct _GtkCssTokenSourceKeyframe GtkCssTokenSourceKeyframe;
struct _GtkCssTokenSourceKeyframe {
  GtkCssTokenSource parent;
  GtkCssTokenSource *source;
  GSList *blocks;
  gboolean done;
};

static void
gtk_css_token_source_keyframe_finalize (GtkCssTokenSource *source)
{
  GtkCssTokenSourceKeyframe *keyframe = (GtkCssTokenSourceKeyframe *) source;

  g_slist_free (keyframe->blocks);

  gtk_css_token_source_unref (keyframe->source);
}

static void
gtk_css_token_source_keyframe_consume_token (GtkCssTokenSource *source,
                                             GObject           *consumer)
{
  GtkCssTokenSourceKeyframe *keyframe = (GtkCssTokenSourceKeyframe *) source;
  const GtkCssToken *token;

  if (keyframe->done)
    return;

  token = gtk_css_token_source_peek_token (keyframe->source);
  switch (token->type)
    {
    case GTK_CSS_TOKEN_FUNCTION:
    case GTK_CSS_TOKEN_OPEN_PARENS:
      keyframe->blocks = g_slist_prepend (keyframe->blocks, GUINT_TO_POINTER (GTK_CSS_TOKEN_CLOSE_PARENS));
      break;
    case GTK_CSS_TOKEN_OPEN_SQUARE:
      keyframe->blocks = g_slist_prepend (keyframe->blocks, GUINT_TO_POINTER (GTK_CSS_TOKEN_CLOSE_SQUARE));
      break;
    case GTK_CSS_TOKEN_OPEN_CURLY:
      keyframe->blocks = g_slist_prepend (keyframe->blocks, GUINT_TO_POINTER (GTK_CSS_TOKEN_CLOSE_CURLY));
      break;
    case GTK_CSS_TOKEN_CLOSE_PARENS:
    case GTK_CSS_TOKEN_CLOSE_SQUARE:
    case GTK_CSS_TOKEN_CLOSE_CURLY:
      if (keyframe->blocks && GPOINTER_TO_UINT (keyframe->blocks->data) == token->type)
        keyframe->blocks = g_slist_remove (keyframe->blocks, keyframe->blocks->data);
      if (token->type == GTK_CSS_TOKEN_CLOSE_CURLY && keyframe->blocks == NULL)
        keyframe->done = TRUE;
      break;
    default:
      break;
    }

  gtk_css_token_source_consume_token_as (keyframe->source, consumer);
}

const GtkCssToken *
gtk_css_token_source_keyframe_peek_token (GtkCssTokenSource *source)
{
  GtkCssTokenSourceKeyframe *keyframe = (GtkCssTokenSourceKeyframe *) source;
  static GtkCssToken eof_token = { GTK_CSS_TOKEN_EOF };

  if (keyframe->done)
    return &eof_token;

  return gtk_css_token_source_peek_token (keyframe->source);
}

static void
gtk_css_token_source_keyframe_error (GtkCssTokenSource *source,
                                     const GError      *error)
{
  GtkCssTokenSourceKeyframe *keyframe = (GtkCssTokenSourceKeyframe *) source;

  gtk_css_token_source_emit_error (keyframe->source, error);
}

static GFile *
gtk_css_token_source_keyframe_get_location (GtkCssTokenSource *source)
{
  GtkCssTokenSourceKeyframe *keyframe = (GtkCssTokenSourceKeyframe *) source;

  return gtk_css_token_source_get_location (keyframe->source);
}

static const GtkCssTokenSourceClass GTK_CSS_TOKEN_SOURCE_KEYFRAME = {
  gtk_css_token_source_keyframe_finalize,
  gtk_css_token_source_keyframe_consume_token,
  gtk_css_token_source_keyframe_peek_token,
  gtk_css_token_source_keyframe_error,
  gtk_css_token_source_keyframe_get_location,
};

static GtkCssTokenSource *
gtk_css_token_source_new_keyframe (GtkCssTokenSource *source)
{
  GtkCssTokenSourceKeyframe *keyframe = gtk_css_token_source_new (GtkCssTokenSourceKeyframe, &GTK_CSS_TOKEN_SOURCE_KEYFRAME);

  keyframe->source = gtk_css_token_source_ref (source);
  gtk_css_token_source_set_consumer (&keyframe->parent,
                                     gtk_css_token_source_get_consumer (source));

  return &keyframe->parent;
}

static void
gtk_css_keyframes_rule_finalize (GObject *object)
{
  GtkCssKeyframesRule *keyframes_rule = GTK_CSS_KEYFRAMES_RULE (object);
  GtkCssKeyframesRulePrivate *priv = gtk_css_keyframes_rule_get_instance_private (keyframes_rule);

  g_free (priv->name);
  g_object_unref (priv->rules);

  G_OBJECT_CLASS (gtk_css_keyframes_rule_parent_class)->finalize (object);
}

static void
gtk_css_keyframes_rule_class_init (GtkCssKeyframesRuleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_keyframes_rule_finalize;
}

static void
gtk_css_keyframes_rule_init (GtkCssKeyframesRule *keyframes_rule)
{
  GtkCssKeyframesRulePrivate *priv = gtk_css_keyframes_rule_get_instance_private (keyframes_rule);

  priv->rules = gtk_css_rule_list_new ();
}

static void
gtk_css_keyframes_rule_set_name (GtkCssKeyframesRule *keyframes,
                                 const char          *name)
{
  GtkCssKeyframesRulePrivate *priv;
  
  priv = gtk_css_keyframes_rule_get_instance_private (keyframes);

  g_free (priv->name);
  priv->name = g_strdup (name);
}

static void
gtk_css_keyframes_rule_append (GtkCssKeyframesRule *keyframes,
                               GtkCssRule          *rule)
{
  GtkCssKeyframesRulePrivate *priv;
  
  priv = gtk_css_keyframes_rule_get_instance_private (keyframes);

  gtk_css_rule_list_append (priv->rules, rule);
}

static GtkCssRule *
gtk_css_keyframes_rule_new (GtkCssRule       *parent_rule,
                            GtkCssStyleSheet *parent_style_sheet)
{
  return g_object_new (GTK_TYPE_CSS_KEYFRAMES_RULE,
                       "parent-rule", parent_rule,
                       "parent-stylesheet", parent_style_sheet,
                       NULL);
}

GtkCssRule *
gtk_css_keyframes_rule_new_parse (GtkCssTokenSource *source,
                                  GtkCssRule        *parent_rule,
                                  GtkCssStyleSheet  *parent_style_sheet)
{
  const GtkCssToken *token;
  GtkCssRule *rule;

  g_return_val_if_fail (source != NULL, NULL);
  g_return_val_if_fail (parent_rule == NULL || GTK_IS_CSS_RULE (parent_rule), NULL);
  g_return_val_if_fail (GTK_IS_CSS_STYLE_SHEET (parent_style_sheet), NULL);

  rule = gtk_css_keyframes_rule_new (parent_rule, parent_style_sheet);
  gtk_css_token_source_set_consumer (source, G_OBJECT (rule));

  token = gtk_css_token_source_get_token (source);
  if (token->type != GTK_CSS_TOKEN_AT_KEYWORD ||
      g_ascii_strcasecmp (token->string.string, "keyframes") != 0)
    {
      gtk_css_token_source_error (source, "Expected '@keyframes'");
      gtk_css_token_source_consume_all (source);
      g_object_unref (rule);
      return NULL;
    }
  gtk_css_token_source_consume_token (source);

  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    {
      gtk_css_token_source_error (source, "Expected name of keyframes");
      gtk_css_token_source_consume_all (source);
      return NULL;
    }
  gtk_css_keyframes_rule_set_name (GTK_CSS_KEYFRAMES_RULE (rule), token->string.string);
  gtk_css_token_source_consume_token (source);

  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_OPEN_CURLY))
    {
      gtk_css_token_source_error (source, "Expected '{'");
      gtk_css_token_source_consume_all (source);
      return NULL;
    }
  gtk_css_token_source_consume_token (source);

  for (token = gtk_css_token_source_get_token (source);
       !gtk_css_token_is (token, GTK_CSS_TOKEN_EOF) && !gtk_css_token_is (token, GTK_CSS_TOKEN_CLOSE_CURLY);
       token = gtk_css_token_source_get_token (source))
    {
      GtkCssTokenSource *keyframe_source = gtk_css_token_source_new_keyframe (source);
      GtkCssRule *keyframe;

      keyframe = gtk_css_keyframe_rule_new_parse (keyframe_source, rule, parent_style_sheet);
      if (keyframe)
        gtk_css_keyframes_rule_append (GTK_CSS_KEYFRAMES_RULE (rule), keyframe);
      
      gtk_css_token_source_unref (keyframe_source);
    }

  gtk_css_token_source_consume_token (source);

  return rule;
}

const char *
gtk_css_keyframes_rule_get_name (GtkCssKeyframesRule *rule)
{
  GtkCssKeyframesRulePrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_KEYFRAMES_RULE (rule), NULL);

  priv = gtk_css_keyframes_rule_get_instance_private (rule);

  return priv->name;
}

GtkCssRuleList *
gtk_css_keyframes_rule_get_css_rules (GtkCssKeyframesRule *rule)
{
  GtkCssKeyframesRulePrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_KEYFRAMES_RULE (rule), NULL);

  priv = gtk_css_keyframes_rule_get_instance_private (rule);

  return priv->rules;
}
