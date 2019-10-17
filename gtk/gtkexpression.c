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
  GType value_type;

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
                                                 GtkExpression          *source);
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
 * @expression_class: class structure for this expression
 * @value_type: the type of the value returned by this expression
 *
 * Returns: (transfer full): the newly created #GtkExpression
 */
static gpointer
gtk_expression_alloc (const GtkExpressionClass *expression_class,
                      GType                     value_type)
{
  GtkExpression *self;

  g_return_val_if_fail (expression_class != NULL, NULL);

  self = g_atomic_rc_box_alloc0 (expression_class->struct_size);

  self->expression_class = expression_class;
  self->value_type = value_type;

  return self;
}

static void
gtk_expression_notify (GtkExpression *self)
{
  if (self->owner == NULL)
    return;

  self->owner->expression_class->notify (self->owner, self);
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
  gtk_expression_notify (expr);
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
  GtkObjectExpression *result;

  result = gtk_expression_alloc (&GTK_OBJECT_EXPRESSION_CLASS, G_OBJECT_TYPE (object));
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
  GParamSpec *pspec;

  GClosure *closure;
};

static void
gtk_property_expression_finalize (GtkExpression *expr)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;

  gtk_expression_unref (self->expr);
  if (self->closure)
    {
      g_closure_invalidate (self->closure);
      g_closure_unref (self->closure);
    }
}

static void
gtk_property_expression_print (GtkExpression *expr,
                              GString       *string)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;

  gtk_expression_print (self->expr, string);
  g_string_append (string, ".");
  g_string_append (string, self->pspec->name);
}

static void
gtk_property_expression_notify_cb (GObject       *object,
                                   GParamSpec    *pspec,
                                   GtkExpression *expr)
{
  gtk_expression_notify (expr);
}

static void
gtk_property_expression_connect (GtkPropertyExpression *self)
{
  GValue value = G_VALUE_INIT;
  GObject *object;

  if (gtk_expression_evaluate (self->expr, &value))
    {
      object = g_value_get_object (&value);
      if (object)
        {
          self->closure = g_cclosure_new (G_CALLBACK (gtk_property_expression_notify_cb), self, NULL),
          g_signal_connect_closure_by_id (object,
                                          g_signal_lookup ("notify", G_OBJECT_TYPE (object)),
                                          g_quark_from_string (self->pspec->name),
                                          g_closure_ref (self->closure),
                                          FALSE);
        }
      g_value_unset (&value);
    }
  else
    object = NULL;
}

static void
gtk_property_expression_notify (GtkExpression *expr,
                                GtkExpression *source)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;

  if (self->closure)
    {
      g_closure_invalidate (self->closure);
      g_clear_pointer (&self->closure, g_closure_unref);
    }

  gtk_property_expression_connect (self);

  gtk_expression_notify (expr);
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

  g_object_get_property (object, self->pspec->name, value);
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
                             GParamSpec    *pspec)
{
  GtkPropertyExpression *result;

  g_assert (g_type_is_a (gtk_expression_get_value_type (expr), pspec->owner_type));

  result = gtk_expression_alloc (&GTK_PROPERTY_EXPRESSION_CLASS, pspec->value_type);

  gtk_expression_set_owner (expr, (GtkExpression *) result);
  result->expr = expr;
  result->pspec = pspec;

  gtk_property_expression_connect (result);

  return (GtkExpression *) result;
}

/*** CAST ***/

typedef struct _GtkCastExpression GtkCastExpression;

typedef gboolean (* GtkCastFunc) (GValue       *to,
                                  const GValue *from);

struct _GtkCastExpression
{
  GtkExpression parent;

  GtkExpression *expr;
  GtkCastFunc cast_func;
};

static gboolean
gtk_cast_copy (GValue       *to,
               const GValue *from)
{
  g_value_copy (from, to);
  return TRUE;
}

static gboolean
gtk_cast_transform (GValue       *to,
                    const GValue *from)
{
  g_value_transform (from, to);
  return TRUE;
}

static gboolean
gtk_cast_upcast (GValue       *to,
                 const GValue *from)
{
  GObject *o = g_value_get_object (from);

  if (o && !g_type_is_a (G_OBJECT_TYPE (o), G_VALUE_TYPE (to)))
    {
      g_value_unset (to);
      return FALSE;
    }

  g_value_set_object (to, o);
  return TRUE;
}

static gboolean
gtk_cast_impossible (GValue       *to,
                     const GValue *from)
{
  g_value_unset (to);
  return FALSE;
}

static GtkCastFunc
gtk_cast_expression_find_cast_func (GType from,
                                    GType to)
{
  if (g_value_type_compatible (from, to))
    return gtk_cast_copy;
  else if (g_type_is_a (from, G_TYPE_OBJECT) && g_type_is_a (to, G_TYPE_OBJECT))
    return gtk_cast_upcast;
  else if (g_value_type_transformable (from, to))
    return gtk_cast_transform;
  else
    return gtk_cast_impossible;
}

static void
gtk_cast_expression_finalize (GtkExpression *expr)
{
  GtkCastExpression *self = (GtkCastExpression *) expr;

  gtk_expression_unref (self->expr);
}

static void
gtk_cast_expression_print (GtkExpression *expr,
                           GString       *string)
{
  GtkCastExpression *self = (GtkCastExpression *) expr;

  gtk_expression_print (self->expr, string);
  g_string_append (string, ":");
  g_string_append (string, g_type_name (gtk_expression_get_value_type (expr)));
}

static void
gtk_cast_expression_notify (GtkExpression *expr,
                            GtkExpression *source)
{
  gtk_expression_notify (expr);
}

static gboolean
gtk_cast_expression_evaluate (GtkExpression *expr,
                              GValue        *value)
{
  GtkCastExpression *self = (GtkCastExpression *) expr;
  GValue expr_value = G_VALUE_INIT;
  gboolean result;

  if (!gtk_expression_evaluate (self->expr, &expr_value))
    return FALSE;

  g_value_init (value, gtk_expression_get_value_type (expr));
  result = self->cast_func (value, &expr_value);
  g_value_unset (&expr_value);

  return result;
}

static const GtkExpressionClass GTK_CAST_EXPRESSION_CLASS =
{
  sizeof (GtkCastExpression),
  "GtkCastExpression",
  gtk_cast_expression_finalize,
  gtk_cast_expression_print,
  gtk_cast_expression_notify,
  gtk_cast_expression_evaluate,
};

static GtkExpression *
gtk_expression_new_cast (GtkExpression *expr,
                         GType          type)
{
  GtkCastExpression *result;

  result = gtk_expression_alloc (&GTK_CAST_EXPRESSION_CLASS, type);

  gtk_expression_set_owner (expr, (GtkExpression *) result);
  result->expr = expr;
  result->cast_func = gtk_cast_expression_find_cast_func (gtk_expression_get_value_type (expr),
                                                          type);

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
                              GtkExpression *source)
{
  GtkAssignExpression *self = (GtkAssignExpression *) expr;
  GValue value = G_VALUE_INIT;

  if (self->object)
    {
      if (gtk_expression_evaluate (self->expr, &value))
        {
          g_object_set_property (self->object, self->property, &value);
          g_value_unset (&value);
        }
      else
        {
          /* XXX: init default value here or just not do anything? */
          /* g_warn_if_reached (); */
        }
    }

  gtk_expression_notify (expr);
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
  GtkAssignExpression *result;

  result = gtk_expression_alloc (&GTK_ASSIGN_EXPRESSION_CLASS,
                                 gtk_expression_get_value_type (expr));

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

/*** NEGATE ***/

typedef struct _GtkNegateExpression GtkNegateExpression;

struct _GtkNegateExpression
{
  GtkExpression parent;

  GtkExpression *expr;
};

static void
gtk_negate_expression_finalize (GtkExpression *expr)
{
  GtkNegateExpression *self = (GtkNegateExpression *) expr;

  gtk_expression_unref (self->expr);
}

static void
gtk_negate_expression_print (GtkExpression *expr,
                          GString       *string)
{
  GtkNegateExpression *self = (GtkNegateExpression *) expr;

  g_string_append (string, "!");
  gtk_expression_print (self->expr, string);
}

static void
gtk_negate_expression_notify (GtkExpression *expr,
                              GtkExpression *source)
{
  gtk_expression_notify (expr);
}

static gboolean
gtk_negate_expression_evaluate (GtkExpression *expr,
                                GValue        *value)
{
  GtkNegateExpression *self = (GtkNegateExpression *) expr;
  GValue expr_value = G_VALUE_INIT;

  if (!gtk_expression_evaluate (self->expr, &expr_value))
    return FALSE;

  g_value_init (value, G_TYPE_BOOLEAN);
  g_value_set_boolean (value, !gtk_expression_value_to_boolean (&expr_value));
  g_value_unset (&expr_value);
  return TRUE;
}

static const GtkExpressionClass GTK_NEGATE_EXPRESSION_CLASS =
{
  sizeof (GtkNegateExpression),
  "GtkNegateExpression",
  gtk_negate_expression_finalize,
  gtk_negate_expression_print,
  gtk_negate_expression_notify,
  gtk_negate_expression_evaluate,
};

static GtkExpression *
gtk_expression_new_negate (GtkExpression *expr)
{
  GtkNegateExpression *result;

  result = gtk_expression_alloc (&GTK_NEGATE_EXPRESSION_CLASS,
                                 G_TYPE_BOOLEAN);

  gtk_expression_set_owner (expr, (GtkExpression *) result);
  result->expr = expr;

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
                           GtkExpression *source)
{
  gtk_expression_notify (expr);
}

static gboolean
gtk_sum_expression_evaluate (GtkExpression *expr,
                             GValue        *value)
{
  GtkSumExpression *self = (GtkSumExpression *) expr;
  GValue lvalue = G_VALUE_INIT;
  GValue rvalue = G_VALUE_INIT;
  char *lstr, *rstr;

  if (!gtk_expression_evaluate (self->left, &lvalue))
    return FALSE;
  if (!gtk_expression_evaluate (self->right, &rvalue))
    {
      g_value_unset (&lvalue);
      return FALSE;
    }

  lstr = gtk_expression_value_to_string (&lvalue);
  rstr = gtk_expression_value_to_string (&rvalue);
  g_value_init (value, G_TYPE_STRING);
  g_value_take_string (value, g_strconcat (lstr, rstr, NULL));
  g_free (lstr);
  g_free (rstr);

  g_value_unset (&rvalue);
  g_value_unset (&lvalue);
  return TRUE;
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
  GtkSumExpression *result;

  result = gtk_expression_alloc (&GTK_SUM_EXPRESSION_CLASS,
                                 G_TYPE_STRING);

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

/**
 * gtk_expression_get_value_type:
 * @self: a #GtkExpression
 *
 * Gets the #GType that this expression evaluates to. This type
 * is constant and will not change over the lifetime of this expression.
 *
 * Returns: The type returned from gtk_expression_evaluate()
 **/
GType
gtk_expression_get_value_type (GtkExpression *self)
{
  g_return_val_if_fail (GTK_IS_EXPRESSION (self), G_TYPE_INVALID);

  return self->value_type;
}

/**
 * gtk_expression_evaluate:
 * @self: a #GtkExpression
 * @value: an empty #GValue
 *
 * Evaluates the given expression and on success stores the result
 * in @value. The #GType of @value will be the type given by
 * gtk_expression_get_value_type().
 *
 * It is possible that expressions cannot be evaluated - for example
 * when the expression references objects that have been destroyed or
 * set to %NULL. In that case @value will remain empty and %FALSE
 * will be returned.
 *
 * Returns: %TRUE if the expression could be evaluated
 **/
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
      GtkLiteralExpression *literal = gtk_expression_alloc (&GTK_LITERAL_EXPRESSION_CLASS, G_TYPE_INT);
      int i;

      g_value_init (&literal->value, G_TYPE_INT);
      gtk_css_parser_consume_integer (parser, &i);
      g_value_set_int (&literal->value, i);

      return (GtkExpression *) literal;
    }
  else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNLESS_INTEGER))
    {
      GtkLiteralExpression *literal = gtk_expression_alloc (&GTK_LITERAL_EXPRESSION_CLASS, G_TYPE_UINT);
      int i;

      g_value_init (&literal->value, G_TYPE_UINT);
      gtk_css_parser_consume_integer (parser, &i);
      g_value_set_uint (&literal->value, i);

      return (GtkExpression *) literal;
    }
  else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
           gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNLESS_NUMBER))
    {
      GtkLiteralExpression *literal = gtk_expression_alloc (&GTK_LITERAL_EXPRESSION_CLASS, G_TYPE_DOUBLE);
      double d;

      g_value_init (&literal->value, G_TYPE_DOUBLE);
      gtk_css_parser_consume_number (parser, &d);
      g_value_set_double (&literal->value, d);

      return (GtkExpression *) literal;
    }
  else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_STRING))
    {
      GtkLiteralExpression *literal = gtk_expression_alloc (&GTK_LITERAL_EXPRESSION_CLASS, G_TYPE_STRING);

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
          GType type = gtk_expression_get_value_type (expr);
          GParamSpec *pspec;

          gtk_css_parser_consume_token (parser);
          token = gtk_css_parser_peek_token (parser);
          if (!gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
            {
              gtk_css_parser_error_syntax (parser, "Expected field member after '.'");
              gtk_expression_unref (expr);
              return NULL;
            }
          if (g_type_is_a (type, G_TYPE_OBJECT))
            {
              pspec = g_object_class_find_property (g_type_class_peek (type), token->string.string);
            }
          else if (g_type_is_a (type, G_TYPE_INTERFACE))
            {
              pspec = g_object_interface_find_property (g_type_default_interface_peek (type),
                                                        token->string.string);
            }
          else
            {
              gtk_css_parser_error_value (parser, "Values of type \"%s\" cannot have members",
                                          g_type_name (type));
              gtk_expression_unref (expr);
              return NULL;
            }
          if (pspec == NULL)
            {
              gtk_css_parser_error_value (parser, "\"%s\" has no property named \"%s\"",
                                          g_type_name (type), token->string.string);
              gtk_expression_unref (expr);
              return NULL;
            }

          expr = gtk_expression_new_property (expr, pspec);
          gtk_css_parser_consume_token (parser);
        }
      else if (gtk_css_token_is (token, GTK_CSS_TOKEN_COLON))
        {
          GType type;

          gtk_css_parser_consume_token (parser);
          token = gtk_css_parser_peek_token (parser);
          if (!gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
            {
              gtk_css_parser_error_syntax (parser, "Expected type name after ':'");
              gtk_expression_unref (expr);
              return NULL;
            }
          type = gtk_builder_get_type_from_name (scope, token->string.string);
          if (type == G_TYPE_INVALID)
            {
              gtk_css_parser_error_value (parser, "Cannot cast to unknown type \"%s\"", token->string.string);
              gtk_expression_unref (expr);
              return NULL;
            }
          expr = gtk_expression_new_cast (expr, type);
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
gtk_expression_parser_parse_unary (GtkBuilder   *scope,
                                   GtkCssParser *parser)
{
  if (gtk_css_parser_try_delim (parser, '!'))
    {
      GtkExpression *expr;

      expr = gtk_expression_parser_parse_postfix (scope, parser);
      if (expr == NULL)
        return NULL;

      return gtk_expression_new_negate (expr);
    }
  else
    {
      return gtk_expression_parser_parse_postfix (scope, parser);
    }
}

static GtkExpression *
gtk_expression_parser_parse_additive (GtkBuilder   *scope,
                                      GtkCssParser *parser)
{
  GtkExpression *expr;

  expr = gtk_expression_parser_parse_unary (scope, parser);
  if (expr == NULL)
    return NULL;

  while (TRUE)
    {
      if (gtk_css_parser_try_delim (parser, '+'))
        {
          GtkExpression *right;

          right = gtk_expression_parser_parse_unary (scope, parser);
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

static void
gtk_expression_builder_error_forward (GtkCssParser         *parser,
                                      const GtkCssLocation *start,
                                      const GtkCssLocation *end,
                                      const GError         *error,
                                      gpointer              user_data)
{
  GError **forward_error = (GError **) user_data;

  if (forward_error && *forward_error == NULL)
    *forward_error = g_error_copy (error);
  g_print ("error: %s\n", error->message);
}

/**
 * gtk_expression_parse:
 * @scope: a #GtkBuilder object to lookup variables in
 * @string: the string to parse
 * @error: Return location to store error or %NULL to ignore
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
gtk_expression_parse (GtkBuilder  *scope,
                      const char  *string,
                      GError     **error)
{
  GtkCssParser *parser;
  GBytes *bytes;
  GtkExpression *result;

  g_return_val_if_fail (GTK_IS_BUILDER (scope), NULL);
  g_return_val_if_fail (string != NULL, NULL);

  bytes = g_bytes_new_static (string, strlen (string));
  parser = gtk_css_parser_new_for_bytes (bytes,
                                         NULL,
                                         NULL,
                                         gtk_expression_builder_error_forward,
                                         error,
                                         NULL);

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

/**
 * gtk_expression_value_to_string:
 * @value: a #GValue
 *
 * Converts a given #GValue to its string representation.
 * This operation never fails, but the returned strings may
 * not be useful.
 *
 * Returns: a new string, free with g_free().
 **/
char *
gtk_expression_value_to_string (const GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE (value), NULL);

  switch (G_TYPE_FUNDAMENTAL (G_VALUE_TYPE (value)))
    {
    case G_TYPE_INVALID:
      return g_strdup ("[invalid]");
    case G_TYPE_NONE:
      return g_strdup ("[none]");
    case G_TYPE_INTERFACE:
    case G_TYPE_CHAR:
    case G_TYPE_UCHAR:
      g_return_val_if_reached (g_strdup ("FIXME"));
    case G_TYPE_BOOLEAN:
      return g_strdup (g_value_get_boolean (value) ? "true" : "false");
    case G_TYPE_INT:
      return g_strdup_printf ("%d", g_value_get_int (value));
    case G_TYPE_UINT:
      return g_strdup_printf ("%u", g_value_get_uint (value));
    case G_TYPE_LONG:
      return g_strdup_printf ("%ld", g_value_get_long (value));
    case G_TYPE_ULONG:
      return g_strdup_printf ("%lu", g_value_get_ulong (value));
    case G_TYPE_INT64:
      return g_strdup_printf ("%" G_GINT64_FORMAT , g_value_get_int64 (value));
    case G_TYPE_UINT64:
      return g_strdup_printf ("%" G_GUINT64_FORMAT , g_value_get_uint64 (value));
    case G_TYPE_ENUM:
    case G_TYPE_FLAGS:
      g_return_val_if_reached (g_strdup ("FIXME"));
    case G_TYPE_FLOAT:
      return g_ascii_dtostr (g_malloc (G_ASCII_DTOSTR_BUF_SIZE),
                             G_ASCII_DTOSTR_BUF_SIZE,
                             g_value_get_float (value));
    case G_TYPE_DOUBLE:
      return g_ascii_dtostr (g_malloc (G_ASCII_DTOSTR_BUF_SIZE),
                             G_ASCII_DTOSTR_BUF_SIZE,
                             g_value_get_double (value));
    case G_TYPE_STRING:
      return g_value_dup_string (value);
    case G_TYPE_POINTER:
      return g_strdup_printf ("[0x%p]", g_value_get_pointer (value));
    case G_TYPE_BOXED:
    case G_TYPE_PARAM:
    case G_TYPE_OBJECT:
      return g_strdup_printf ("[%s]", G_VALUE_TYPE_NAME (value));
    case G_TYPE_VARIANT:
      return g_variant_print (g_value_get_variant (value), TRUE);
    default:
      return g_strconcat ("[", g_type_name (G_VALUE_TYPE (value)), "]", NULL);
    }
}

/**
 * gtk_expression_value_to_boolean:
 * @value: a #GValue
 *
 * Converts a given #GValue to its boolean value.
 * Every value has a boolean representation.
 *
 * Number types are %TRUE when their value is different
 * from 0, pointer types are %TRUE when their value
 * is different from %NULL and unknown and invalid
 * types are always %FALSE.
 *
 * In particular, this means that the empty string "" is %TRUE.
 *
 * Returns: the boolean representation of @value.
 **/
gboolean
gtk_expression_value_to_boolean (const GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE (value), FALSE);

  switch (G_TYPE_FUNDAMENTAL (G_VALUE_TYPE (value)))
    {
    case G_TYPE_INVALID:
    case G_TYPE_NONE:
      return FALSE;
    case G_TYPE_INTERFACE:
      g_return_val_if_reached (FALSE);
    case G_TYPE_CHAR:
      return g_value_get_char (value) != 0;
    case G_TYPE_UCHAR:
      return g_value_get_uchar (value) != 0;
    case G_TYPE_BOOLEAN:
      return g_value_get_boolean (value);
    case G_TYPE_INT:
      return g_value_get_int (value) != 0;
    case G_TYPE_UINT:
      return g_value_get_uint (value) != 0;
    case G_TYPE_LONG:
      return g_value_get_long (value) != 0;
    case G_TYPE_ULONG:
      return g_value_get_ulong (value) != 0;
    case G_TYPE_INT64:
      return g_value_get_int64 (value) != 0;
    case G_TYPE_UINT64:
      return g_value_get_uint64 (value) != 0;
    case G_TYPE_ENUM:
      return g_value_get_enum (value) != 0;
    case G_TYPE_FLAGS:
      return g_value_get_flags (value) != 0;
    case G_TYPE_FLOAT:
      return g_value_get_float (value) != 0;
    case G_TYPE_DOUBLE:
      return g_value_get_double (value) != 0;
    case G_TYPE_STRING:
      return g_value_get_string (value) != NULL;
    case G_TYPE_POINTER:
      return g_value_get_pointer (value) != NULL;
    case G_TYPE_BOXED:
      return g_value_get_boxed (value) != NULL;
    case G_TYPE_PARAM:
      return g_value_get_param (value) != NULL;
    case G_TYPE_OBJECT:
      return g_value_get_object (value) != NULL;
    case G_TYPE_VARIANT:
      return g_value_get_variant (value) != NULL;
    default:
      return FALSE;
    }
}

