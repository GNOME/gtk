/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtkexpressionprivate.h"

#include <gtk/css/gtkcss.h>
#include <gtk/css/gtkcssparserprivate.h>
#include "gtk/gtkbuildable.h"

typedef struct _GtkExpressionClass GtkExpressionClass;

struct _GtkExpression
{
  const GtkExpressionClass *expression_class;

  GtkExpression *owner;
};

struct _GtkExpressionClass
{
  gsize struct_size;
  const char *type_name;

  void                  (* finalize)            (GtkExpression          *expr);
  void                  (* print)               (GtkExpression          *expr,
                                                 GString                *string);
  void                  (* notify)              (GtkExpression          *expr,
                                                 GtkExpression          *source,
                                                 gboolean                destroyed);
  gboolean              (* evaluate)            (GtkExpression          *expr,
                                                 GValue                 *value);
};

/**
 * GtkExpression: (ref-func gtk_expression_ref) (unref-func gtk_expression_unref)
 *
 * The `GtkExpression` structure contains only private data.
 */

/*< private >
 * gtk_expression_alloc:
 * @expression_class: class structure for this self
 *
 * Returns: (transfer full): the newly created #GtkExpression
 */
static gpointer
gtk_expression_alloc (const GtkExpressionClass *expression_class)
{
  GtkExpression *self;

  g_return_val_if_fail (expression_class != NULL, NULL);

  self = g_atomic_rc_box_alloc0 (expression_class->struct_size);

  self->expression_class = expression_class;

  return self;
}

static void
gtk_expression_notify (GtkExpression *self,
                       gboolean       destroyed)
{
  if (self->owner == NULL)
    return;

  self->owner->expression_class->notify (self->owner, self, destroyed);
}

static void
gtk_expression_set_owner (GtkExpression *self,
                          GtkExpression *owner)
{
  g_assert (self->owner == NULL);
  self->owner = owner;
}

/*** LITERAL ***/

typedef struct _GtkLiteralExpression GtkLiteralExpression;

struct _GtkLiteralExpression
{
  GtkExpression parent;

  GValue value;
};

static void
gtk_literal_expression_finalize (GtkExpression *expr)
{
  GtkLiteralExpression *self = (GtkLiteralExpression *) expr;

  g_value_unset (&self->value);
}

static void
gtk_literal_expression_print (GtkExpression *expr,
                              GString       *string)
{
  GtkLiteralExpression *self = (GtkLiteralExpression *) expr;
  char *s;

  s = g_strdup_value_contents (&self->value);
  g_string_append (string, s);
  g_free (s);
}

static gboolean
gtk_literal_expression_evaluate (GtkExpression *expr,
                                 GValue        *value)
{
  GtkLiteralExpression *self = (GtkLiteralExpression *) expr;

  g_value_init (value, G_VALUE_TYPE (&self->value));
  g_value_copy (&self->value, value);
  return TRUE;
}

static const GtkExpressionClass GTK_LITERAL_EXPRESSION_CLASS =
{
  sizeof (GtkLiteralExpression),
  "GtkLiteralExpression",
  gtk_literal_expression_finalize,
  gtk_literal_expression_print,
  NULL,
  gtk_literal_expression_evaluate,
};

/*** OBJECT ***/

typedef struct _GtkObjectExpression GtkObjectExpression;

struct _GtkObjectExpression
{
  GtkExpression parent;

  GObject *object;
};

static void
gtk_object_expression_weak_cb (gpointer expr,
                               GObject *unused)
{
  GtkObjectExpression *self = (GtkObjectExpression *) expr;

  self->object = NULL;
  gtk_expression_notify (expr, TRUE);
}

static void
gtk_object_expression_finalize (GtkExpression *expr)
{
  GtkObjectExpression *self = (GtkObjectExpression *) expr;

  if (self->object)
    g_object_weak_unref (self->object, gtk_object_expression_weak_cb, self);
}

static void
print_object (GObject *object,
              GString *string)
{
  if (GTK_IS_BUILDABLE (object) && gtk_buildable_get_name (GTK_BUILDABLE (object)))
    {
      g_string_append (string, gtk_buildable_get_name (GTK_BUILDABLE (object)));
    }
  else if (object == NULL)
    {
      g_string_append (string, "NULL");
    }
  else
    {
      g_string_append_printf (string, "0x%p:%s", object, G_OBJECT_TYPE_NAME (object));
    }
}

static void
gtk_object_expression_print (GtkExpression *expr,
                             GString       *string)
{
  GtkObjectExpression *self = (GtkObjectExpression *) expr;

  print_object (self->object, string);
}

static gboolean
gtk_object_expression_evaluate (GtkExpression *expr,
                                GValue        *value)
{
  GtkObjectExpression *self = (GtkObjectExpression *) expr;

  if (self->object == NULL)
    return FALSE;

  g_value_init (value, G_OBJECT_TYPE (self->object));
  g_value_set_object (value, self->object);

  return TRUE;
}

static const GtkExpressionClass GTK_OBJECT_EXPRESSION_CLASS =
{
  sizeof (GtkObjectExpression),
  "GtkObjectExpression",
  gtk_object_expression_finalize,
  gtk_object_expression_print,
  NULL,
  gtk_object_expression_evaluate,
};

static GtkExpression *
gtk_object_expression_new (GObject *object)
{
  GtkObjectExpression *result = gtk_expression_alloc (&GTK_OBJECT_EXPRESSION_CLASS);

  result->object = object;
  g_object_weak_ref (result->object, gtk_object_expression_weak_cb, result);

  return (GtkExpression *) result;
}

/*** PROPERTY ***/

typedef struct _GtkPropertyExpression GtkPropertyExpression;

struct _GtkPropertyExpression
{
  GtkExpression parent;

  GtkExpression *expr;
  char *property;

  GObject *current;
  guint signal_id;
};

static void
gtk_property_expression_finalize (GtkExpression *expr)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;

  gtk_expression_unref (self->expr);
  g_free (self->property);
}

static void
gtk_property_expression_print (GtkExpression *expr,
                              GString       *string)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;

  gtk_expression_print (self->expr, string);
  g_string_append (string, ".");
  g_string_append (string, self->property);
}

static void
gtk_property_expression_notify_cb (GObject       *object,
                                   GParamSpec    *pspec,
                                   GtkExpression *expr)
{
  gtk_expression_notify (expr, FALSE);
}

static void
gtk_property_expression_connect (GtkPropertyExpression *self)
{
  GValue value = G_VALUE_INIT;

  gtk_expression_evaluate (self->expr, &value);
  self->current = g_value_get_object (&value);
  g_value_unset (&value);
  if (self->current == NULL)
    {
      self->signal_id = 0;
      return;
    }
  self->signal_id = g_signal_connect_closure_by_id (self->current,
                                                    g_signal_lookup ("notify", G_OBJECT_TYPE (self->current)),
                                                    g_quark_from_string (self->property),
                                                    g_cclosure_new (G_CALLBACK (gtk_property_expression_notify_cb), self, NULL),
                                                    FALSE);
}

static void
gtk_property_expression_notify (GtkExpression *expr,
                                GtkExpression *source,
                                gboolean       destroyed)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;

  if (!destroyed)
    g_signal_handler_disconnect (self->current, self->signal_id);

  gtk_property_expression_connect (self);

  gtk_expression_notify (expr, FALSE);
}

static gboolean
gtk_property_expression_evaluate (GtkExpression *expr,
                                  GValue        *value)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;
  GValue object_value = G_VALUE_INIT;
  GObject *object;

  if (!gtk_expression_evaluate (self->expr, &object_value))
    return FALSE;

  object = g_value_get_object (&object_value);
  if (object == NULL)
    {
      g_value_unset (&object_value);
      return FALSE;
    }

  g_object_get_property (object, self->property, value);
  g_value_unset (&object_value);
  return TRUE;
}

static const GtkExpressionClass GTK_PROPERTY_EXPRESSION_CLASS =
{
  sizeof (GtkPropertyExpression),
  "GtkPropertyExpression",
  gtk_property_expression_finalize,
  gtk_property_expression_print,
  gtk_property_expression_notify,
  gtk_property_expression_evaluate,
};

static GtkExpression *
gtk_expression_new_property (GtkExpression *expr,
                             const char    *property)
{
  GtkPropertyExpression *result = gtk_expression_alloc (&GTK_PROPERTY_EXPRESSION_CLASS);

  gtk_expression_set_owner (expr, (GtkExpression *) result);
  result->expr = expr;
  result->property = g_strdup (property);

  gtk_property_expression_connect (result);

  return (GtkExpression *) result;
}

/*** ASSIGN ***/

typedef struct _GtkAssignExpression GtkAssignExpression;

struct _GtkAssignExpression
{
  GtkExpression parent;

  GObject *object;
  char *property;
  GtkExpression *expr;
};

static void
gtk_assign_expression_weak_cb (gpointer expr,
                               GObject *unused)
{
  GtkObjectExpression *self = (GtkObjectExpression *) expr;

  self->object = NULL;
  gtk_expression_unref (expr);
}

static void
gtk_assign_expression_finalize (GtkExpression *expr)
{
  GtkAssignExpression *self = (GtkAssignExpression *) expr;

  g_assert (self->object == NULL);
  gtk_expression_unref (self->expr);
  g_free (self->property);
}

static void
gtk_assign_expression_print (GtkExpression *expr,
                             GString       *string)
{
  GtkAssignExpression *self = (GtkAssignExpression *) expr;

  print_object (self->object, string);
  g_string_append (string, ".");
  g_string_append (string, self->property);
  g_string_append (string, " = ");
  gtk_expression_print (self->expr, string);
}

static void
gtk_assign_expression_notify (GtkExpression *expr,
                              GtkExpression *source,
                              gboolean       destroyed)
{
  GtkAssignExpression *self = (GtkAssignExpression *) expr;
  GValue value = G_VALUE_INIT;

  if (self->object)
    {
      if (!gtk_expression_evaluate (self->expr, &value))
        {
          g_warn_if_reached ();
          /* XXX: init default value here or decide to not do anything */
        }
      g_object_set_property (self->object, self->property, &value);
      g_value_unset (&value);
    }

  gtk_expression_notify (expr, destroyed);
}

static gboolean
gtk_assign_expression_evaluate (GtkExpression *expr,
                                GValue        *value)
{
  GtkAssignExpression *self = (GtkAssignExpression *) expr;

  return gtk_expression_evaluate (self->expr, value);
}

static const GtkExpressionClass GTK_ASSIGN_EXPRESSION_CLASS =
{
  sizeof (GtkAssignExpression),
  "GtkAssignExpression",
  gtk_assign_expression_finalize,
  gtk_assign_expression_print,
  gtk_assign_expression_notify,
  gtk_assign_expression_evaluate,
};

GtkExpression *
gtk_expression_new_assign (GObject       *object,
                           const char    *property,
                           GtkExpression *expr)
{
  GtkAssignExpression *result = gtk_expression_alloc (&GTK_ASSIGN_EXPRESSION_CLASS);

  gtk_expression_set_owner (expr, (GtkExpression *) result);
  result->object = object;
  result->expr = expr;
  result->property = g_strdup (property);
  if (result->object)
    {
      g_object_weak_ref (result->object, gtk_assign_expression_weak_cb, result);
      gtk_expression_ref ((GtkExpression *) result);
    }

  return (GtkExpression *) result;
}

/*** SUM ***/

typedef struct _GtkSumExpression GtkSumExpression;

struct _GtkSumExpression
{
  GtkExpression parent;

  GtkExpression *left;
  GtkExpression *right;
};

static void
gtk_sum_expression_finalize (GtkExpression *expr)
{
  GtkSumExpression *self = (GtkSumExpression *) expr;

  gtk_expression_unref (self->left);
  gtk_expression_unref (self->right);
}

static void
gtk_sum_expression_print (GtkExpression *expr,
                          GString       *string)
{
  GtkSumExpression *self = (GtkSumExpression *) expr;

  gtk_expression_print (self->left, string);
  g_string_append (string, " + ");
  gtk_expression_print (self->right, string);
}

static void
gtk_sum_expression_notify (GtkExpression *expr,
                           GtkExpression *source,
                           gboolean       destroyed)
{
  gtk_expression_notify (expr, FALSE);
}

static char *
value_to_string (const GValue *value)
{
  if (G_VALUE_TYPE (value) == G_TYPE_STRING)
    {
      return g_value_dup_string (value);
    }
  else
    {
      return g_strdup_value_contents (value);
    }
}

static gboolean
gtk_sum_expression_evaluate (GtkExpression *expr,
                             GValue        *value)
{
  GtkSumExpression *self = (GtkSumExpression *) expr;
  GValue lvalue = G_VALUE_INIT;
  GValue rvalue = G_VALUE_INIT;
  gboolean success = FALSE;

  if (!gtk_expression_evaluate (self->left, &lvalue))
    return FALSE;
  if (!gtk_expression_evaluate (self->right, &rvalue))
    goto fail;

  if (G_VALUE_TYPE (&lvalue) == G_TYPE_STRING || G_VALUE_TYPE (&rvalue) == G_TYPE_STRING)
    {
      /* do string concatenation */
      GString *string = g_string_new (NULL);
      char *s;

      s = value_to_string (&lvalue);
      g_string_append (string, s);
      g_free (s);
      s = value_to_string (&rvalue);
      g_string_append (string, s);
      g_free (s);
      g_value_init (value, G_TYPE_STRING);
      g_value_take_string (value, g_string_free (string, FALSE));
      success = TRUE;
    }
  else
    {
      /* implement! */
    }

  g_value_unset (&rvalue);
fail:
  g_value_unset (&lvalue);
  return success;
}

static const GtkExpressionClass GTK_SUM_EXPRESSION_CLASS =
{
  sizeof (GtkSumExpression),
  "GtkSumExpression",
  gtk_sum_expression_finalize,
  gtk_sum_expression_print,
  gtk_sum_expression_notify,
  gtk_sum_expression_evaluate,
};

static GtkExpression *
gtk_expression_new_sum (GtkExpression *left,
                        GtkExpression *right)
{
  GtkSumExpression *result = gtk_expression_alloc (&GTK_SUM_EXPRESSION_CLASS);

  gtk_expression_set_owner (left, (GtkExpression *) result);
  gtk_expression_set_owner (right, (GtkExpression *) result);
  result->left = left;
  result->right = right;

  return (GtkExpression *) result;
}

/*** PUBLIC API ***/

static void
gtk_expression_finalize (GtkExpression *self)
{
  self->expression_class->finalize (self);
}

/**
 * gtk_expression_ref:
 * @self: (allow-none): a #GtkExpression
 *
 * Acquires a reference on the given #GtkExpression.
 *
 * Returns: (transfer none): the #GtkExpression with an additional reference
 */
GtkExpression *
gtk_expression_ref (GtkExpression *self)
{
  return g_atomic_rc_box_acquire (self);
}

/**
 * gtk_expression_unref:
 * @self: (allow-none): a #GtkExpression
 *
 * Releases a reference on the given #GtkExpression.
 *
 * If the reference was the last, the resources associated to the @self are
 * freed.
 */
void
gtk_expression_unref (GtkExpression *self)
{
  g_atomic_rc_box_release_full (self, (GDestroyNotify) gtk_expression_finalize);
}

/**
 * gtk_expression_print:
 * @self: a #GtkExpression
 * @string: The string to print into
 *
 * Converts @self into a human-readable string representation suitable
 * for printing that can be parsed with gtk_expression_parse().
 **/
void
gtk_expression_print (GtkExpression *self,
                      GString       *string)
{
  g_return_if_fail (GTK_IS_EXPRESSION (self));
  g_return_if_fail (string != NULL);

  self->expression_class->print (self, string);
}

/**
 * gtk_expression_to_string:
 * @self: a #GtkExpression
 *
 * Converts an expression into a string that is suitable for
 * printing and can later be parsed with gtk_expression_parse().
 *
 * This is a wrapper around gtk_expression_print(), see that function
 * for details.
 *
 * Returns: A new string for @self
 **/
char *
gtk_expression_to_string (GtkExpression *self)
{
  GString *string;

  string = g_string_new ("");

  gtk_expression_print (self, string);

  return g_string_free (string, FALSE);
}

gboolean
gtk_expression_evaluate (GtkExpression *self,
                         GValue        *value)
{
  g_return_val_if_fail (GTK_IS_EXPRESSION (self), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  return self->expression_class->evaluate (self, value);
}

static GtkExpression *
gtk_expression_parser_parse_primary (GtkBuilder   *scope,
                                     GtkCssParser *parser)
{
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_INTEGER))
    {
      GtkLiteralExpression *literal = gtk_expression_alloc (&GTK_LITERAL_EXPRESSION_CLASS);
      int i;

      g_value_init (&literal->value, G_TYPE_INT);
      gtk_css_parser_consume_integer (parser, &i);
      g_value_set_int (&literal->value, i);

      return (GtkExpression *) literal;
    }
  else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNLESS_INTEGER))
    {
      GtkLiteralExpression *literal = gtk_expression_alloc (&GTK_LITERAL_EXPRESSION_CLASS);
      int i;

      g_value_init (&literal->value, G_TYPE_UINT);
      gtk_css_parser_consume_integer (parser, &i);
      g_value_set_uint (&literal->value, i);

      return (GtkExpression *) literal;
    }
  else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
           gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNLESS_NUMBER))
    {
      GtkLiteralExpression *literal = gtk_expression_alloc (&GTK_LITERAL_EXPRESSION_CLASS);
      double d;

      g_value_init (&literal->value, G_TYPE_DOUBLE);
      gtk_css_parser_consume_number (parser, &d);
      g_value_set_double (&literal->value, d);

      return (GtkExpression *) literal;
    }
  else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_STRING))
    {
      GtkLiteralExpression *literal = gtk_expression_alloc (&GTK_LITERAL_EXPRESSION_CLASS);

      g_value_init (&literal->value, G_TYPE_STRING);
      g_value_take_string (&literal->value, gtk_css_parser_consume_string (parser));

      return (GtkExpression *) literal;
    }
  else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    {
      char *ident = gtk_css_parser_consume_ident (parser);
      GObject *object = gtk_builder_get_object (scope, ident);

      if (object == NULL)
        {
          gtk_css_parser_error_value (parser, "No variable named \"%s\"", ident);
          g_free (ident);
          return NULL;
        }

      g_free (ident);
      return gtk_object_expression_new (object);
    }
  else
    {
      gtk_css_parser_error_syntax (parser, "Unexpected syntax");
      return NULL;
    }
}

static GtkExpression *
gtk_expression_parser_parse_postfix (GtkBuilder   *scope,
                                     GtkCssParser *parser)
{
  const GtkCssToken *token;
  GtkExpression *expr;
  
  expr = gtk_expression_parser_parse_primary (scope, parser);
  if (expr == NULL)
    return NULL;

  while (TRUE)
    {
      token = gtk_css_parser_peek_token (parser);
      if (gtk_css_token_is_delim (token, '.'))
        {
          gtk_css_parser_consume_token (parser);
          token = gtk_css_parser_peek_token (parser);
          if (!gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
            {
              gtk_css_parser_error_syntax (parser, "Expected field member after '.'");
              gtk_expression_unref (expr);
              return NULL;
            }

          expr = gtk_expression_new_property (expr, token->string.string);
          gtk_css_parser_consume_token (parser);
        }
      else
        {
          break;
        }
    }
  return expr;
}

static GtkExpression *
gtk_expression_parser_parse_additive (GtkBuilder   *scope,
                                      GtkCssParser *parser)
{
  GtkExpression *expr;

  expr = gtk_expression_parser_parse_postfix (scope, parser);
  if (expr == NULL)
    return NULL;

  while (TRUE)
    {
      if (gtk_css_parser_try_delim (parser, '+'))
        {
          GtkExpression *right;

          right = gtk_expression_parser_parse_postfix (scope, parser);
          if (right == NULL)
            {
              gtk_expression_unref (expr);
              return NULL;
            }
          expr = gtk_expression_new_sum (expr, right);
        }
      else
        {
          return expr;
        }
    }
}

static GtkExpression *
gtk_expression_parser_parse (GtkBuilder   *scope,
                             GtkCssParser *parser)
{
  return gtk_expression_parser_parse_additive (scope, parser);
}

/**
 * gtk_expression_parse:
 * @scope: a #GtkBuilder object to lookup variables in
 * @string: the string to parse
 *
 * Parses the given @string into an expression and returns it.
 * Strings printed via gtk_expression_to_string()
 * can be read in again successfully using this function.
 *
 * If @string does not describe a valid expression, %NULL is
 * returned.
 *
 * Returns: A new expression
 **/
GtkExpression *
gtk_expression_parse (GtkBuilder *scope,
                      const char *string)
{
  GtkCssParser *parser;
  GBytes *bytes;
  GtkExpression *result;

  g_return_val_if_fail (GTK_IS_BUILDER (scope), NULL);
  g_return_val_if_fail (string != NULL, NULL);

  bytes = g_bytes_new_static (string, strlen (string));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL, NULL);

  result = gtk_expression_parser_parse (scope, parser);
  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_error_syntax (parser, "Unexpected junk at end of value");
      g_clear_pointer (&result, gtk_expression_unref);
    }

  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  return result; 
}
